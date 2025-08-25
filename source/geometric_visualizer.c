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
    
    // PHASE 1: MINIMAL MODE 1 TEST - Switch to Mode 1 and create one simple scaling tile
    // Enable BG0 for text, BG2 for our graphics, and sprites
    SetMode(MODE_1 | BG0_ENABLE | BG2_ENABLE | OBJ_ENABLE | OBJ_1D_MAP);
    
    // Configure BG0 for text compatibility (ensure track title still shows)
    // Use character base 1 to match where init_font_tiles() puts the font data
    REG_BG0CNT = BG_SIZE_0 | BG_16_COLOR | CHAR_BASE(1) | SCREEN_BASE(29);
    
    // Initialize font system for tile-based text rendering in Mode 1
    init_font_tiles();
    
    // Set up text palette on BG palette
    BG_PALETTE[0] = RGB5(0, 0, 15);     // Blue background for text area
    BG_PALETTE[1] = RGB5(31, 31, 0);    // Yellow text color
    BG_PALETTE[16] = RGB5(0, 0, 15);    // Blue background for text area (palette 1)
    BG_PALETTE[17] = RGB5(31, 31, 0);   // Yellow text color (palette 1)
    
    // Configure BG2 for hardware scaling (rotation/scaling background)  
    // In Mode 1, BG2 is affine (rotation/scaling) - needs different setup
    REG_BG2CNT = BG_SIZE_0 | BG_256_COLOR | CHAR_BASE(1) | SCREEN_BASE(30);
    
    // Create a simple test tile in character block 1
    u8* tile_mem = (u8*)(0x6004000); // Character base 1
    
    // Clear character block first
    for (int i = 0; i < 16384; i++) { // 16KB character block
        tile_mem[i] = 0;
    }
    
    // Create tile 100: Simple test pattern (8x8 pixels, 256-color = 64 bytes)
    // Use tile 100+ to avoid conflict with font tiles (0-95)
    u8* tile1 = tile_mem + (100 * 64); // Tile 100 offset
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            if (x == 0 || x == 7 || y == 0 || y == 7) {
                tile1[y * 8 + x] = 1; // Border - palette color 1
            } else {
                tile1[y * 8 + x] = 2; // Fill - palette color 2  
            }
        }
    }
    
    // Set up BG2 tilemap in screen base 30
    u16* bg2_map = (u16*)(0x6007800); // Screen base 30
    
    // Clear tilemap
    for (int i = 0; i < 1024; i++) {
        bg2_map[i] = 0; // All tiles start as transparent
    }
    
    // Place one test tile in center of 32x32 map
    int center = 16 * 32 + 16;
    bg2_map[center] = 100; // Use tile 100
    
    // Set up palette
    BG_PALETTE[0] = RGB5(0, 0, 0);      // Transparent black
    BG_PALETTE[1] = RGB5(31, 0, 0);     // Red border
    BG_PALETTE[2] = RGB5(31, 31, 0);    // Yellow fill
    
    // Initialize BG2 transformation to normal scale
    REG_BG2PA = 256; REG_BG2PB = 0;    // No rotation, 1.0x scale
    REG_BG2PC = 0;   REG_BG2PD = 256;
    REG_BG2X = 0;    REG_BG2Y = 0;     // No offset
    
    // PHASE 1: Since sprites work perfectly, implement geometric visualizer with sprites
    // Create sprite palette for geometric shapes
    SPRITE_PALETTE[0] = RGB5(0, 0, 0);      // Transparent
    SPRITE_PALETTE[1] = RGB5(31, 15, 0);    // Orange center
    SPRITE_PALETTE[2] = RGB5(31, 31, 0);    // Yellow middle ring
    SPRITE_PALETTE[3] = RGB5(15, 31, 15);   // Green outer ring
    
    // Create geometric sprite tiles at safe location
    u32* spriteGfx = (u32*)(0x6014000); // Safe sprite memory
    
    // Tile 0: Solid center (small square)
    u32 center_pattern = 0x11111111; // Orange center
    for (int i = 0; i < 16; i++) {
        spriteGfx[i] = center_pattern;
    }
    
    // Tile 1: Middle ring pattern
    u32 ring_pattern = 0x22222222; // Yellow ring
    for (int i = 0; i < 16; i++) {
        spriteGfx[16 + i] = ring_pattern;
    }
    
    // Tile 2: Outer ring pattern  
    u32 outer_pattern = 0x33333333; // Green outer
    for (int i = 0; i < 16; i++) {
        spriteGfx[32 + i] = outer_pattern;
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
    
    // PHASE 1: SPRITE-BASED GEOMETRIC VISUALIZER - Audio reactive sprite positioning
    
    // Calculate audio reactivity
    total_amplitude = 0;
    for (int i = 0; i < 8; i++) {
        total_amplitude += spectrum_accumulators_8ad[i];
    }
    
    // Calculate expansion based on audio
    int expansion = 0; // Default - no expansion
    if (spectrum_sample_count_8ad > 0) {
        long avg_amplitude = total_amplitude / 8;
        // Scale expansion from 0 to 16 pixels based on audio
        expansion = (avg_amplitude * 16) / (spectrum_sample_count_8ad * 50);
        if (expansion > 16) expansion = 16; // Cap expansion
        if (expansion < 0) expansion = 0;   // Floor expansion
    }
    
    // Center sprite position (fixed)
    int center_x = 115;
    int center_y = 75;
    
    // Update center sprite (always visible)
    OAM[0].attr0 = ATTR0_NORMAL | ATTR0_COLOR_16 | ATTR0_SQUARE | (center_y & 0xFF);
    OAM[0].attr1 = ATTR1_SIZE_8 | (center_x & 0x01FF);
    OAM[0].attr2 = ATTR2_PALETTE(0) | 512; // Orange center
    
    // Update ring sprites - expand outward based on audio
    int ring_distance = 8 + expansion; // Base distance + audio expansion
    
    // Ring sprite positions (4 sprites around center forming basic geometric pattern)
    OAM[1].attr0 = ATTR0_NORMAL | ATTR0_COLOR_16 | ATTR0_SQUARE | ((center_y - ring_distance) & 0xFF);
    OAM[1].attr1 = ATTR1_SIZE_8 | ((center_x - ring_distance) & 0x01FF);
    OAM[1].attr2 = ATTR2_PALETTE(0) | 513; // Yellow ring
    
    OAM[2].attr0 = ATTR0_NORMAL | ATTR0_COLOR_16 | ATTR0_SQUARE | ((center_y - ring_distance) & 0xFF);
    OAM[2].attr1 = ATTR1_SIZE_8 | ((center_x + ring_distance) & 0x01FF);
    OAM[2].attr2 = ATTR2_PALETTE(0) | 513; // Yellow ring
    
    // Add more ring sprites for fuller geometric effect
    OAM[3].attr0 = ATTR0_NORMAL | ATTR0_COLOR_16 | ATTR0_SQUARE | ((center_y + ring_distance) & 0xFF);
    OAM[3].attr1 = ATTR1_SIZE_8 | ((center_x - ring_distance) & 0x01FF);
    OAM[3].attr2 = ATTR2_PALETTE(0) | 513; // Yellow ring
    
    OAM[4].attr0 = ATTR0_NORMAL | ATTR0_COLOR_16 | ATTR0_SQUARE | ((center_y + ring_distance) & 0xFF);
    OAM[4].attr1 = ATTR1_SIZE_8 | ((center_x + ring_distance) & 0x01FF);
    OAM[4].attr2 = ATTR2_PALETTE(0) | 513; // Yellow ring
    
    // Outer ring with larger expansion
    int outer_distance = 16 + (expansion * 2); // Larger expansion multiplier
    
    OAM[5].attr0 = ATTR0_NORMAL | ATTR0_COLOR_16 | ATTR0_SQUARE | ((center_y - outer_distance) & 0xFF);
    OAM[5].attr1 = ATTR1_SIZE_8 | (center_x & 0x01FF);
    OAM[5].attr2 = ATTR2_PALETTE(0) | 514; // Green outer
    
    OAM[6].attr0 = ATTR0_NORMAL | ATTR0_COLOR_16 | ATTR0_SQUARE | ((center_y + outer_distance) & 0xFF);
    OAM[6].attr1 = ATTR1_SIZE_8 | (center_x & 0x01FF);
    OAM[6].attr2 = ATTR2_PALETTE(0) | 514; // Green outer
    
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
    
    // Create a blue background tile in tile 96 (right after font tiles 0-95)
    u16* bgTile = (u16*)CHAR_BASE_ADR(1) + (96 * 16); // Character base 1, tile 96
    for (int i = 0; i < 16; i++) {
        bgTile[i] = 0x0000; // All pixels use palette color 0 (blue background)
    }
    
    // Fill bottom 3 rows with blue background
    for (int ty = 17; ty < 20; ty++) {
        for (int tx = 0; tx < 30; tx++) {
            textMap[ty * 32 + tx] = 96; // Use blue background tile (tile 96)
        }
    }
    
    // Draw track name on row 17 (tile_y = 17)
    draw_text_bg0(1, 17, track_name);
    
    // Draw visualization name on row 18 (tile_y = 18) 
    draw_text_bg0(1, 18, viz_name);
}

// Custom text drawing function for BG0 (screen base 29)
static void draw_text_bg0(int tile_x, int tile_y, const char* text) {
    u16* textMap = (u16*)SCREEN_BASE_BLOCK(29); // BG0 uses screen base 29
    const char* ptr = text;
    int x = tile_x;
    
    // Draw each character as a tile
    while (*ptr && x < 30) {
        char c = *ptr++;
        
        if (c >= 32 && c <= 126) {
            int tile_index = (c - 32) + 1; // +1 because tile 0 is reserved for space
            textMap[tile_y * 32 + x] = tile_index | (1 << 12); // Use palette 1 (yellow text)
        }
        x++;
    }
}