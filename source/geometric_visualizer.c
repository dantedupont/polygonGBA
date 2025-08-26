#include "geometric_visualizer.h"
#include "8ad_player.h"
#include "font.h"
#include <string.h>
#include <stdlib.h>

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

// Audio-reactive parameters  
static long total_amplitude = 0;
static int hexagon_scale = 256;      // Current scale (256 = 1.0x)
static int target_scale = 256;       // Target scale for smooth transitions
static int hexagon_rotation = 0;     // Current rotation angle
static int color_cycle_counter = 0;  // Counter for color changes
static int current_palette = 0;      // Current color palette index

void init_geometric_visualizer(void) {
    if (is_initialized) return;
    
    // CRITICAL: DO NOT clear framebuffer - it destroys ALL sprite tile data!
    // The framebuffer at 0x6000000 overlaps with sprite memory, clearing it 
    // destroys all sprite tiles for the entire system
    /*
    u16* framebuffer = (u16*)0x6000000;
    for (int i = 0; i < 240 * 160; i++) {
        framebuffer[i] = 0; // This was destroying ALL sprite tiles!
    }
    */
    
    // MODE_1: No mode switching needed - everything uses unified MODE_1
    // Font system already initialized by main.c
    
    // CRITICAL: DO NOT write to 0x6000000 - it corrupts sprite tile memory in MODE_3!
    // The geometric mode switching between MODE_1 and MODE_3 was corrupting sprite tiles
    // Comment out tile creation to prevent sprite memory corruption
    
    /*
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
    */
    
    // MODE_1: BG layers already configured by main.c
    // BG1: Text layer, BG2: Background layer
    // No need to reconfigure - just use the existing setup
    
    // Create a simple test tile in character block 1
    u8* tile_mem = (u8*)(0x6004000); // Character base 1
    
    // CRITICAL: Don't clear character/screen memory! This corrupts sprite system
    // The memory clearing was interfering with sprite tile organization
    /*
    for (int i = 0; i < 65536; i++) { // Clear all 64KB of character memory
        tile_mem[i] = 0; // This corrupts sprite memory!
    }
    
    u16* screen_mem = (u16*)(0x6006000); // Start of screen base memory
    for (int i = 0; i < 16384; i++) { // Clear all 32KB of screen base memory
        screen_mem[i] = 0; // This corrupts sprite system!
    }
    */
    
    // Set up BG palette for text rendering
    BG_PALETTE[0] = RGB5(0, 0, 0);      // Transparent/black background  
    BG_PALETTE[1] = RGB5(31, 31, 0);    // Yellow text color
    BG_PALETTE[2] = RGB5(31, 0, 31);    // Magenta for test tiles
    BG_PALETTE[3] = RGB5(0, 0, 15);     // Blue for text area background
    
    // TODO(human): BG2 transformation setup temporarily removed for text debugging
    
    // Enhanced sprite palette for animated hexagon (use colors 9+ to avoid spectrum's 0-8)
    SPRITE_PALETTE[0] = RGB5(0, 0, 0);      // Transparent (always 0)
    SPRITE_PALETTE[9] = RGB5(31, 31, 0);    // Bright yellow for geometric mode
    SPRITE_PALETTE[10] = RGB5(31, 15, 0);   // Orange  
    SPRITE_PALETTE[11] = RGB5(31, 0, 0);    // Bright red
    
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
    
    // Create a large 32x32 hexagon using 16 tiles (4x4 arrangement)
    // 32x32 sprites use tiles in this pattern: [0 ][1 ][2 ][3 ]
    //                                         [4 ][5 ][6 ][7 ]
    //                                         [8 ][9 ][10][11]
    //                                         [12][13][14][15]
    
    // Clear all 16 tiles first
    for (int tile = 0; tile < 16; tile++) {
        for (int i = 0; i < 16; i++) {
            spriteGfx[tile * 16 + i] = 0x00000000;
        }
    }
    
    // Define proper hexagon pattern for 32x32 sprite
    // Each u32 contains 8 pixels (4 bits each): 0=transparent, 1=color1 (edge), 3=color3 (fill)
    // Design a true hexagon with angled edges, not diagonal stripes
    
    // Top row tiles (0, 1, 2, 3) - top point of hexagon
    // Each tile is 8x8 pixels, needs exactly 8 u32 values (one per row)
    u32 tile0[8] = { // Top-left corner (mostly empty)
        0x00000000, // Row 0: all transparent
        0x00000000, // Row 1: all transparent  
        0x00000000, // Row 2: all transparent
        0x00000000, // Row 3: all transparent
        0x00000000, // Row 4: all transparent
        0x00000000, // Row 5: all transparent
        0x00000013, // Row 6: tiny bit of hexagon edge
        0x00000133  // Row 7: more hexagon edge
    };
    
    u32 tile1[8] = { // Top-center-left 
        0x00000000, // Row 0: transparent
        0x00000000, // Row 1: transparent
        0x00000000, // Row 2: transparent
        0x00000110, // Row 3: start of hexagon top
        0x00001330, // Row 4: hexagon expanding
        0x00013330, // Row 5: hexagon expanding more
        0x30133330, // Row 6: wider hexagon section
        0x31333330  // Row 7: full width section
    };
    
    u32 tile2[8] = { // Top-center-right
        0x00000000, // Row 0: transparent
        0x00000000, // Row 1: transparent
        0x00000000, // Row 2: transparent
        0x01100000, // Row 3: start of hexagon top (mirrored)
        0x03331000, // Row 4: hexagon expanding
        0x03333100, // Row 5: hexagon expanding more
        0x03333310, // Row 6: wider hexagon section
        0x03333313  // Row 7: full width section
    };
    
    u32 tile3[8] = { // Top-right corner (mostly empty)
        0x00000000, // Row 0: transparent
        0x00000000, // Row 1: transparent  
        0x00000000, // Row 2: transparent
        0x00000000, // Row 3: transparent
        0x00000000, // Row 4: transparent
        0x00000000, // Row 5: transparent
        0x31000000, // Row 6: tiny bit of hexagon edge
        0x33100000  // Row 7: more hexagon edge
    };
    
    // Create a simpler, more visible hexagon - middle tiles are mostly solid
    u32 tile4[8] = { // Upper-left side
        0x00013333, 0x00133333, 0x01333333, 0x13333333,
        0x13333333, 0x13333333, 0x13333333, 0x13333333
    };
    
    u32 tile5[8] = { // Upper-center-left (full)
        0x33333333, 0x33333333, 0x33333333, 0x33333333,
        0x33333333, 0x33333333, 0x33333333, 0x33333333
    };
    
    u32 tile6[8] = { // Upper-center-right (full)
        0x33333333, 0x33333333, 0x33333333, 0x33333333,
        0x33333333, 0x33333333, 0x33333333, 0x33333333
    };
    
    u32 tile7[8] = { // Upper-right side
        0x33331000, 0x33331100, 0x33333110, 0x33333311,
        0x33333331, 0x33333331, 0x33333331, 0x33333331
    };
    
    // Lower middle tiles - mirror upper middle
    u32 tile8[8] = { // Lower-left side
        0x13333333, 0x13333333, 0x13333333, 0x13333333,
        0x13333333, 0x01333333, 0x00133333, 0x00013333
    };
    
    u32 tile9[8] = { // Lower-center-left (full)
        0x33333333, 0x33333333, 0x33333333, 0x33333333,
        0x33333333, 0x33333333, 0x33333333, 0x33333333
    };
    
    u32 tile10[8] = { // Lower-center-right (full)
        0x33333333, 0x33333333, 0x33333333, 0x33333333,
        0x33333333, 0x33333333, 0x33333333, 0x33333333
    };
    
    u32 tile11[8] = { // Lower-right side
        0x33333331, 0x33333331, 0x33333331, 0x33333331,
        0x33333331, 0x33333110, 0x33331100, 0x33331000
    };
    
    // Bottom row tiles - mirror top row
    u32 tile12[8] = { // Bottom-left corner
        0x00000133, 0x00000013, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000
    };
    
    u32 tile13[8] = { // Bottom-center-left
        0x31333330, 0x30133330, 0x00013330, 0x00001330,
        0x00000110, 0x00000000, 0x00000000, 0x00000000
    };
    
    u32 tile14[8] = { // Bottom-center-right
        0x03333313, 0x03333310, 0x03333100, 0x03331000,
        0x01100000, 0x00000000, 0x00000000, 0x00000000
    };
    
    u32 tile15[8] = { // Bottom-right corner
        0x33100000, 0x31000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000
    };
    
    // DON'T write sprite data in MODE_1 - it might not work!
    // Move sprite creation to render function where we're in correct mode
    // Just set up palette and other non-sprite state here
    
    // Clear all sprites initially - the hexagon will be set up in render function
    for (int i = 0; i < 128; i++) {
        OAM[i].attr0 = ATTR0_DISABLED;
        OAM[i].attr1 = 0;
        OAM[i].attr2 = 0;
    }
    
    is_initialized = 1;
}

void cleanup_geometric_visualizer(void) {
    if (!is_initialized) return;
    
    // MODE_1: No mode switching needed - everything stays in unified MODE_1
    
    // Clear all OAM entries to ensure clean transition
    for (int i = 0; i < 128; i++) {
        OAM[i].attr0 = ATTR0_DISABLED;
        OAM[i].attr1 = 0;
        OAM[i].attr2 = 0;
    }
    
    // DON'T clear sprite tiles - we might switch back to this mode
    // Only clear OAM to hide sprites, but leave tile data intact
    // Sprite tile data should persist across mode switches
    
    // CRITICAL: Restore sprite palette for waveform/spectrum modes
    // Our geometric mode overwrites the sprite palette, breaking other visualizers
    SPRITE_PALETTE[0] = RGB5(0, 0, 0);      // Transparent
    SPRITE_PALETTE[1] = RGB5(31, 31, 31);   // BRIGHT WHITE (waveform needs this)
    SPRITE_PALETTE[2] = RGB5(31, 0, 31);    // BRIGHT MAGENTA (waveform needs this)
    SPRITE_PALETTE[3] = RGB5(0, 31, 0);     // BRIGHT GREEN (waveform needs this)
    SPRITE_PALETTE[4] = RGB5(31, 31, 0);    // BRIGHT YELLOW (waveform needs this)
    
    // CRITICAL: DO NOT clear framebuffer - it destroys ALL sprite tile data!
    // The framebuffer at 0x6000000 overlaps with sprite memory in MODE_3
    // This was the root cause of sprite disappearance!
    /*
    u16* framebuffer = (u16*)0x6000000;
    for (int i = 0; i < 240 * 160; i++) {
        framebuffer[i] = RGB5(0, 0, 0); // This destroys sprite tiles!
    }
    */
    
    is_initialized = 0;
}

void update_geometric_visualizer(void) {
    if (!is_initialized) return;
    
    // All audio processing moved to render function to avoid timing conflicts
    // The spectrum visualizer processes data after this update, so we read fresh data in render
}

void render_geometric_hexagon(void) {
    if (!is_initialized) return;
    
    // IN MODE_1: Sprite tiles can use the full range 0-1023 at 0x06010000
    static bool sprite_data_created = false;
    if (!sprite_data_created) {
        u32* spriteGfx = (u32*)(0x6010000); // MODE_1: Full sprite memory available
        int tile_start = 600; // Use tiles 600+ to avoid conflicts with other modes
        
        // Create simple test pattern exactly like spectrum does (this WORKS)
        u32 base_color = 9; // Use color 9 (avoid spectrum's 0-8 range)
        u32 pixel_data = (base_color << 0) | (base_color << 4) | (base_color << 8) | (base_color << 12) |
                         (base_color << 16) | (base_color << 20) | (base_color << 24) | (base_color << 28);
        
        // Fill all 16 tiles with the same simple pattern that spectrum uses
        for (int tile = 0; tile < 16; tile++) {
            for (int row = 0; row < 8; row++) {
                spriteGfx[(tile_start + tile) * 8 + row] = pixel_data;
            }
        }
        sprite_data_created = true;
    }
    
    // --- Audio Reactivity Logic ---
    total_amplitude = 0;
    for (int i = 0; i < 8; i++) {
        total_amplitude += spectrum_accumulators_8ad[i];
    }

    if (spectrum_sample_count_8ad > 0) {
        long avg_amplitude = total_amplitude / 8;
        target_scale = 180 + (avg_amplitude * 280) / (spectrum_sample_count_8ad * 50);
        if (target_scale > 460) target_scale = 460;
        if (target_scale < 180) target_scale = 180;
    }

    // Smooth scale transitions and continuous rotation
    hexagon_scale += (target_scale - hexagon_scale) / 6;
    hexagon_rotation = (hexagon_rotation + 1) % 360;

    // --- Update Affine Matrix for Sprite Rotation & Scaling ---
    int sin_val = sin_approx(hexagon_rotation);
    int cos_val = cos_approx(hexagon_rotation);

    // Scale value needs to be inverted for sprites (e.g., 2.0x scale = 1/2 = 128)
    int inv_scale = (256 * 256) / hexagon_scale;

    // Set up affine transformation matrix (using matrix #0)
    // Access affine parameters directly in OAM memory
    OBJAFFINE* affine = &((OBJAFFINE*)OAM)[0];
    affine->pa = (cos_val * inv_scale) >> 8;
    affine->pb = (-sin_val * inv_scale) >> 8;
    affine->pc = (sin_val * inv_scale) >> 8;
    affine->pd = (cos_val * inv_scale) >> 8;

    // --- Color Cycling ---
    color_cycle_counter++;
    if (color_cycle_counter >= 180) {
        color_cycle_counter = 0;
        current_palette = (current_palette + 1) % 3;

        if (current_palette == 0) {
            SPRITE_PALETTE[1] = RGB5(31, 0, 31); // Magenta
        } else if (current_palette == 1) {
            SPRITE_PALETTE[1] = RGB5(0, 31, 31); // Cyan
        } else {
            SPRITE_PALETTE[1] = RGB5(31, 15, 0); // Orange
        }
    }

    // --- Set up Affine Sprite ---
    // Center position
    int center_x = 104;
    int center_y = 60;

    // Configure 32x32 sprite with double-size affine transformation
    // Use OAM[127] (highest index) to avoid conflict with waveform (uses 0-119)
    int geometric_oam_slot = 127;
    OAM[geometric_oam_slot].attr0 = ATTR0_SQUARE |           // Shape: Square
                                   ATTR0_COLOR_16 |         // Colors: 16
                                   ATTR0_ROTSCALE_DOUBLE |  // Mode: Affine (Double-Size Bounding Box)
                                   (center_y & 0xFF);       // Y-Coordinate

    OAM[geometric_oam_slot].attr1 = ATTR1_SIZE_32 |          // Size: 32x32 pixels
                                   ATTR1_ROTDATA(0) |       // Use affine matrix #0
                                   (center_x & 0x01FF);     // X-Coordinate

    OAM[geometric_oam_slot].attr2 = ATTR2_PRIORITY(0) |      // Priority: 0 (Topmost)
                                   ATTR2_PALETTE(0) |       // Use palette #0
                                   528;                     // Start at tile 528 (our hexagon tiles)

    // Disable all other sprites (except our geometric sprite at slot 127)
    for (int i = 0; i < 127; i++) {
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