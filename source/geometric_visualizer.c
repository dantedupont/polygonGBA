#include "geometric_visualizer.h"
#include "8ad_player.h"
#include "font.h"
#include <string.h>

// External functions from main.c and 8ad_player.c
extern const char* get_full_track_name(int track_index);
extern const char* get_visualization_name(int viz_index);
extern int get_current_track_8ad(void);
extern int get_current_visualization(void);

// Forward declarations for helper functions
static int cos_approx(int angle);
static int sin_approx(int angle);
static void draw_track_info_tiles(void);
static void draw_text_bg0(int tile_x, int tile_y, const char* text);

// State variables
static int is_initialized = 0;
static int current_rings = MIN_HEXAGON_RINGS;
static int target_rings = MIN_HEXAGON_RINGS;
static int current_palette = 0; // 0-7 for color cycling
static int color_cycle_counter = 0;
static int active_sprites = 0;

// Audio-reactive parameters
static long total_amplitude = 0;
static long max_amplitude = 0;
static int adaptive_scale = 1000;
static long previous_total = 0;

void init_geometric_visualizer(void) {
    if (is_initialized) return;
    
    // CRITICAL: Clear Mode 3 framebuffer data before switching to Mode 1
    // This prevents old pixel data from being interpreted as corrupt tile data
    u16* framebuffer = (u16*)0x6000000;
    for (int i = 0; i < 240 * 160; i++) {
        framebuffer[i] = 0; // Clear all framebuffer data
    }
    
    // Switch to Mode 1 - focus on getting BG0 text working first
    // Enable ONLY BG0 for now to debug text display
    SetMode(MODE_1 | BG0_ENABLE | OBJ_ENABLE | OBJ_1D_MAP);
    
    // Initialize font system FIRST before setting up BG0
    init_font_tiles();
    
    // DEBUG: Create test tiles at high indices to avoid font conflicts
    u16* char_mem = (u16*)0x6000000; // Character base 0
    u16* test_tile = &char_mem[100 * 16]; // Tile 100: well past font tiles (1-96)
    
    // Create a very simple solid yellow square for testing
    for (int i = 0; i < 16; i++) {
        test_tile[i] = 0x1111; // All pixels use palette color 1 (yellow)
    }
    
    // Also try a different tile pattern at tile 101 for extra testing
    u16* test_tile2 = &char_mem[101 * 16]; // Tile 101
    for (int i = 0; i < 16; i++) {
        test_tile2[i] = 0x2222; // All pixels use palette color 2 (magenta)
    }
    
    // Configure BG0 for text with highest priority (0 = highest)
    // Try character base 0 now since we moved our test tiles there
    REG_BG0CNT = BG_SIZE_0 | BG_16_COLOR | BG_PRIORITY(0) | CHAR_BASE(0) | SCREEN_BASE(29);
    
    // DEBUG: Also set scroll position to make sure BG0 is visible
    REG_BG0HOFS = 0;
    REG_BG0VOFS = 0;
    
    // Clear BG0 tilemap explicitly
    u16* textMap = (u16*)SCREEN_BASE_BLOCK(29); // BG0 uses screen base 29
    for (int i = 0; i < 1024; i++) {
        textMap[i] = 0; // Clear all tilemap entries
    }
    
    // Palette already set up above - don't duplicate
    
    // Configure BG2 for hardware scaling (rotation/scaling background)  
    // In Mode 1, BG2 is affine (rotation/scaling) - needs different setup
    REG_BG2CNT = BG_SIZE_0 | BG_256_COLOR | CHAR_BASE(1) | SCREEN_BASE(30);
    
    // Create a simple test tile in character block 1
    u8* tile_mem = (u8*)(0x6004000); // Character base 1
    
    // Clear ALL character blocks to ensure clean slate (64KB total)
    for (int i = 0; i < 65536; i++) { // Clear all 64KB of character memory
        tile_mem[i] = 0;
    }
    
    // TODO(human): BG2 test tile creation temporarily removed for text debugging
    // Will re-add BG2 affine transformation system once text rendering works
    
    // Clear ALL screen base memory (used for tilemaps)
    u16* screen_mem = (u16*)(0x6006000); // Start of screen base memory
    for (int i = 0; i < 16384; i++) { // Clear all 32KB of screen base memory
        screen_mem[i] = 0;
    }
    
    // Set up BG palette for text rendering
    BG_PALETTE[0] = RGB5(0, 0, 0);      // Transparent/black background  
    BG_PALETTE[1] = RGB5(31, 31, 0);    // Yellow text color
    BG_PALETTE[2] = RGB5(31, 0, 31);    // Magenta for test tiles
    BG_PALETTE[3] = RGB5(0, 0, 15);     // Blue for text area background
    
    // TODO(human): BG2 transformation setup temporarily removed for text debugging
    
    // Enhanced sprite palette for animated hexagon
    // Palette 0: Orange/Red theme
    SPRITE_PALETTE[0] = RGB5(0, 0, 0);      // Transparent
    SPRITE_PALETTE[1] = RGB5(31, 0, 0);     // Bright red
    SPRITE_PALETTE[2] = RGB5(31, 15, 0);    // Orange  
    SPRITE_PALETTE[3] = RGB5(31, 31, 0);    // Yellow
    
    // Palette 1: Blue/Cyan theme  
    SPRITE_PALETTE[16] = RGB5(0, 0, 0);     // Transparent
    SPRITE_PALETTE[17] = RGB5(0, 0, 31);    // Bright blue
    SPRITE_PALETTE[18] = RGB5(0, 15, 31);   // Light blue
    SPRITE_PALETTE[19] = RGB5(0, 31, 31);   // Cyan
    
    // Palette 2: Green theme
    SPRITE_PALETTE[32] = RGB5(0, 0, 0);     // Transparent  
    SPRITE_PALETTE[33] = RGB5(0, 31, 0);    // Bright green
    SPRITE_PALETTE[34] = RGB5(15, 31, 15);  // Light green
    SPRITE_PALETTE[35] = RGB5(31, 31, 15);  // Yellow-green
    
    // Palette 3: Purple/Magenta theme
    SPRITE_PALETTE[48] = RGB5(0, 0, 0);     // Transparent
    SPRITE_PALETTE[49] = RGB5(31, 0, 31);   // Magenta
    SPRITE_PALETTE[50] = RGB5(20, 0, 31);   // Purple
    SPRITE_PALETTE[51] = RGB5(31, 15, 31);  // Pink
    
    // Create geometric sprite tiles at safe location
    u32* spriteGfx = (u32*)(0x6014000); // Safe sprite memory
    
    // Tile 0: Solid circle pattern
    u32 circle_pattern[16] = {
        0x00000000, // ........
        0x00111000, // ..***...
        0x01222100, // .*##*...
        0x12222210, // *####*..
        0x12222210, // *####*..
        0x12222210, // *####*..
        0x01222100, // .*##*...
        0x00111000, // ..***...
        0x00000000, // ........
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000
    };
    for (int i = 0; i < 16; i++) {
        spriteGfx[i] = circle_pattern[i];
    }
    
    // Tile 1: Ring/outline pattern
    u32 ring_pattern[16] = {
        0x00000000, // ........
        0x00111000, // ..***...
        0x01000100, // .*...*..
        0x10000010, // *....**.
        0x10000010, // *....**.
        0x10000010, // *....**.
        0x01000100, // .*...*..
        0x00111000, // ..***...
        0x00000000, // ........
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000
    };
    for (int i = 0; i < 16; i++) {
        spriteGfx[16 + i] = ring_pattern[i];
    }
    
    // Tile 2: Cross pattern  
    u32 cross_pattern[16] = {
        0x00010000, // ...*....
        0x00010000, // ...*....
        0x00010000, // ...*....
        0x11111110, // ******.
        0x11111110, // ******.
        0x00010000, // ...*....
        0x00010000, // ...*....
        0x00010000, // ...*....
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000
    };
    for (int i = 0; i < 16; i++) {
        spriteGfx[32 + i] = cross_pattern[i];
    }
    
    // Tile 3: Diamond pattern
    u32 diamond_pattern[16] = {
        0x00010000, // ...*....
        0x00121000, // ..*#*...
        0x01232100, // .*##*...
        0x12332210, // *###*...
        0x12332210, // *###*...
        0x01232100, // .*##*...
        0x00121000, // ..*#*...
        0x00010000, // ...*....
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000
    };
    for (int i = 0; i < 16; i++) {
        spriteGfx[48 + i] = diamond_pattern[i];
    }
    
    // Set up initial geometric pattern - 3 sprites forming basic shape
    // Center sprite
    OAM[0].attr0 = ATTR0_NORMAL | ATTR0_COLOR_16 | ATTR0_SQUARE | (75 & 0xFF);
    OAM[0].attr1 = ATTR1_SIZE_8 | (115 & 0x01FF);
    OAM[0].attr2 = ATTR2_PALETTE(0) | 512; // Center tile
    
    // Ring sprites around center (basic geometric pattern)
    OAM[1].attr0 = ATTR0_NORMAL | ATTR0_COLOR_16 | ATTR0_SQUARE | (67 & 0xFF);
    OAM[1].attr1 = ATTR1_SIZE_8 | (107 & 0x01FF);  
    OAM[1].attr2 = ATTR2_PALETTE(0) | 513; // Ring tile
    
    OAM[2].attr0 = ATTR0_NORMAL | ATTR0_COLOR_16 | ATTR0_SQUARE | (67 & 0xFF);
    OAM[2].attr1 = ATTR1_SIZE_8 | (123 & 0x01FF);
    OAM[2].attr2 = ATTR2_PALETTE(0) | 513; // Ring tile
    
    is_initialized = 1;
}

void cleanup_geometric_visualizer(void) {
    if (!is_initialized) return;
    
    // Clean up all sprites
    for (int i = 0; i < 128; i++) {
        OAM[i].attr0 = ATTR0_DISABLED;
    }
    
    // RESTORE MODE 3 for other visualizers
    SetMode(MODE_3 | BG2_ENABLE | OBJ_ENABLE | OBJ_1D_MAP);
    
    // Clear screen to black for Mode 3
    u16* framebuffer = (u16*)0x6000000;
    for (int i = 0; i < 240 * 160; i++) {
        framebuffer[i] = RGB5(0, 0, 0);
    }
    
    is_initialized = 0;
}

void update_geometric_visualizer(void) {
    if (!is_initialized) return;
    
    // All audio processing moved to render function to avoid timing conflicts
    // The spectrum visualizer processes data after this update, so we read fresh data in render
}

void render_geometric_hexagon(void) {
    if (!is_initialized) return;
    
    // AUDIO-REACTIVE HEXAGON VISUALIZER
    
    // Calculate audio reactivity
    total_amplitude = 0;
    for (int i = 0; i < 8; i++) {
        total_amplitude += spectrum_accumulators_8ad[i];
    }
    
    // Calculate scaling and color based on audio
    int base_radius = 20; // Base hexagon radius
    int audio_scale = 0;
    int color_intensity = 0;
    
    if (spectrum_sample_count_8ad > 0) {
        long avg_amplitude = total_amplitude / 8;
        // Scale radius from base to base+24 pixels based on audio
        audio_scale = (avg_amplitude * 24) / (spectrum_sample_count_8ad * 50);
        if (audio_scale > 24) audio_scale = 24;
        if (audio_scale < 0) audio_scale = 0;
        
        // Color intensity based on audio (0-7 for palette cycling)
        color_intensity = (avg_amplitude * 7) / (spectrum_sample_count_8ad * 100);
        if (color_intensity > 7) color_intensity = 7;
        if (color_intensity < 0) color_intensity = 0;
    }
    
    int current_radius = base_radius + audio_scale;
    
    // Center position
    int center_x = 120;
    int center_y = 80;
    
    // Rotation animation - increment rotation angle each frame
    static int rotation_angle = 0;
    rotation_angle = (rotation_angle + 2) % 360; // Rotate 2 degrees per frame
    
    // Create hexagon using 6 sprites positioned at hexagon vertices
    // Hexagon vertices are at 60-degree intervals: 0°, 60°, 120°, 180°, 240°, 300°
    for (int i = 0; i < 6; i++) {
        int vertex_angle = (i * 60 + rotation_angle) % 360;
        
        // Calculate vertex position using our trigonometric functions
        int vertex_x = center_x + (cos_approx(vertex_angle) * current_radius) / 256;
        int vertex_y = center_y + (sin_approx(vertex_angle) * current_radius) / 256;
        
        // Choose sprite tile and palette based on audio intensity and vertex
        int tile_index = 512 + (color_intensity % 4); // Cycle through our 4 tile types (circle, ring, cross, diamond)
        int palette_offset = (color_intensity + i) % 4; // Create color variation per vertex
        
        // Position sprite at vertex
        OAM[i].attr0 = ATTR0_NORMAL | ATTR0_COLOR_16 | ATTR0_SQUARE | (vertex_y & 0xFF);
        OAM[i].attr1 = ATTR1_SIZE_8 | (vertex_x & 0x01FF);
        OAM[i].attr2 = ATTR2_PALETTE(palette_offset) | tile_index;
    }
    
    // Add center sprite for visual anchor
    OAM[6].attr0 = ATTR0_NORMAL | ATTR0_COLOR_16 | ATTR0_SQUARE | (center_y & 0xFF);
    OAM[6].attr1 = ATTR1_SIZE_8 | (center_x & 0x01FF);
    OAM[6].attr2 = ATTR2_PALETTE(color_intensity % 4) | 512; // Center uses base tile with audio color
    
    // Disable remaining sprites
    for (int i = 7; i < 128; i++) {
        OAM[i].attr0 = ATTR0_DISABLED;
    }
    
    // Draw track information using tile-based text
    draw_track_info_tiles();
}

// Simple trigonometric approximations for integer math (256 = 1.0)
static int cos_approx(int angle) {
    angle = angle % 360;
    if (angle < 0) angle += 360;
    
    if (angle <= 90) return 256 - (angle * 256 / 90);
    else if (angle <= 180) return -((angle - 90) * 256 / 90);
    else if (angle <= 270) return -256 + ((angle - 180) * 256 / 90);
    else return (angle - 270) * 256 / 90;
}

static int sin_approx(int angle) {
    return cos_approx(angle - 90);  // sin(x) = cos(x - 90°)
}

// Note: setup_hexagon_sprites() removed - we use dynamic tile overwriting in render function

// Draw track information using tile-based text rendering for Mode 1
static void draw_track_info_tiles(void) {
    // Get the current track name and visualization name
    int current_track_num = get_current_track_8ad();
    int current_viz = get_current_visualization();
    const char* track_name = get_full_track_name(current_track_num);
    const char* viz_name = get_visualization_name(current_viz);
    
    // Clear the text area first (fill with blue background tiles)
    u16* textMap = (u16*)SCREEN_BASE_BLOCK(29); // BG0 uses screen base 29
    
    // Create a blue background tile for text area ONLY (tile 97)
    u16* bgTile = (u16*)CHAR_BASE_ADR(1) + (97 * 16); // Character base 1, tile 97
    for (int i = 0; i < 16; i++) {
        bgTile[i] = 0x3333; // All pixels use palette color 3 (blue for text background)
    }
    
    // Create a test pattern tile for debugging (tile 98)  
    u16* testTile = (u16*)CHAR_BASE_ADR(1) + (98 * 16);
    for (int i = 0; i < 16; i++) {
        testTile[i] = 0x2222; // All pixels use palette color 2 (should show magenta)
    }
    
    // Fill bottom 3 rows with alternating background and test tiles for debugging
    for (int ty = 17; ty < 20; ty++) {
        for (int tx = 0; tx < 30; tx++) {
            if (tx % 2 == 0) {
                textMap[ty * 32 + tx] = 97; // Blue background tile (color 0)
            } else {
                textMap[ty * 32 + tx] = 98; // Magenta test tile (color 2)  
            }
        }
    }
    
    // Display track and visualization info
    draw_text_bg0(1, 18, track_name);
    draw_text_bg0(1, 19, viz_name);
}

// Custom text drawing function for BG0 (screen base 29)
static void draw_text_bg0(int tile_x, int tile_y, const char* text) {
    u16* textMap = (u16*)SCREEN_BASE_BLOCK(29); // BG0 uses screen base 29
    const char* ptr = text;
    int x = tile_x;
    
    // Clear the text line first
    for (int i = 0; i < 30; i++) {
        textMap[tile_y * 32 + i] = 0; // Use tile 0 (transparent background)
    }
    
    // Draw each character as a tile
    while (*ptr && x < 30) {
        char c = *ptr++;
        
        if (c >= 32 && c <= 126) {
            int tile_index = (c - 32) + 1; // +1 because tile 0 is reserved for space
            textMap[tile_y * 32 + x] = tile_index; // Use palette 0 first to test
        } else {
            textMap[tile_y * 32 + x] = 0; // Use blank tile for non-printable chars
        }
        x++;
    }
}