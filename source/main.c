#include <gba.h>
#include <stdio.h>
#include <string.h>
#include "gbfs.h"
// 8AD audio system
#include "8ad_player.h"
#include "visualization_manager.h"
#include "spectrum_visualizer.h"
#include "waveform_visualizer.h"

#include "font.h"

// Global GBFS filesystem
const GBFS_FILE *fs = NULL;


// Full track name mapping
const char* get_full_track_name(int track_index) {
    // Map track indices to full names
    // Side A: tracks 0-3, Side B: tracks 0-5 (but represent album tracks 5-10)
    static const char* side_a_tracks[] = {
        "Crumbling Castle",
        "Polygondwanaland", 
        "The Castle In The Air",
        "Deserted Dunes Welcome Weary Feet"
    };
    
    static const char* side_b_tracks[] = {
        "Inner Cell",
        "Loyalty", 
        "Horology",
        "Tetrachromacy",
        "Searching...",
        "The Fourth Colour"
    };
    
    // Determine which ROM we're in based on available tracks
    extern const GBFS_FILE *fs;
    int total_tracks = gbfs_count_objs(fs);
    
    
    if (total_tracks == 4 || total_tracks == 3) {
        // Side A ROM
        if (track_index >= 0 && track_index < total_tracks) {
            return side_a_tracks[track_index];
        }
    } else if (total_tracks == 6) {
        // Side B ROM  
        if (track_index >= 0 && track_index < 6) {
            return side_b_tracks[track_index];
        }
    }
    
    return "Unknown Track";
}


// Main function with 8AD audio and spectrum visualizer
int main() {
    // Initialize interrupts and video
    irqInit();
    irqEnable(IRQ_VBLANK);
    
    // MODE_1: Use unified video mode for all visualizations
    // This eliminates mode switching corruption artifacts
    SetMode(MODE_1 | BG0_ENABLE | BG2_ENABLE | OBJ_ENABLE | OBJ_1D_MAP);
    
    // Initialize font system for text rendering
    init_font_tiles();
    
    // Configure BG1 for text overlay (uses existing font system)
    REG_BG1CNT = BG_SIZE_0 | BG_16_COLOR | BG_PRIORITY(0) | CHAR_BASE(0) | SCREEN_BASE(30);
    REG_BG1HOFS = 0;
    REG_BG1VOFS = 0;
    
    // Configure BG2 for solid background
    REG_BG2CNT = BG_SIZE_0 | BG_256_COLOR | CHAR_BASE(1) | SCREEN_BASE(29);
    
    // Enable BG1 instead of BG0
    SetMode(MODE_1 | BG1_ENABLE | BG2_ENABLE | OBJ_ENABLE | OBJ_1D_MAP);
    
    // Clear BG1 tilemap (text layer)
    u16* textMap = (u16*)SCREEN_BASE_BLOCK(30);
    for (int i = 0; i < 1024; i++) {
        textMap[i] = 0;
    }
    
    // Set up solid black background using BG2
    u16* bgMap = (u16*)SCREEN_BASE_BLOCK(29);
    for (int i = 0; i < 1024; i++) {
        bgMap[i] = 1; // Use tile 1 (black tile)
    }
    
    // Create a simple black tile at index 1 in character base 1
    u8* tileMem = (u8*)0x6004000; // Character base 1
    for (int i = 0; i < 64; i++) { // 8x8 tile in 256-color mode = 64 bytes
        tileMem[64 + i] = 0; // Tile 1 (offset 64), all pixels black
    }
    
    // Set BG palette colors for text and background
    BG_PALETTE[0] = RGB5(0, 0, 0);        // Color 0: Black (background/transparent)
    BG_PALETTE[16] = RGB5(0, 0, 0);       // Palette 1, Color 0: Transparent
    BG_PALETTE[17] = RGB5(31, 31, 0);     // Palette 1, Color 1: Yellow (text)
    
    // CRITICAL: Initialize ALL OAM entries to disabled state first
    // This prevents sprite artifacts from previous sessions or undefined memory
    for (int i = 0; i < 128; i++) {
        OAM[i].attr0 = ATTR0_DISABLED;
        OAM[i].attr1 = 0;
        OAM[i].attr2 = 0;
    }
    
    // Initialize visualization manager
    init_visualization_manager();
    
    
    // Initialize GBFS
    extern const GBFS_FILE *fs;
    fs = find_first_gbfs_file(find_first_gbfs_file);
    
    if (!fs) {
        // Failed - red background
        BG_PALETTE[0] = RGB5(31, 0, 0);
        while(1) VBlankIntrWait();
    }
    
    // Initialize clean 8AD system
    init_8ad_sound();
    
    // Start first track
    start_8ad_track(0);
    
    // Font system already initialized above
    
    unsigned short last_keys = 0;
    
    // Visualization mode variables removed - handled by visualization_manager
    
    // Main loop with spectrum analyzer
    while(1) {
        VBlankIntrWait();
        
        // 8AD audio processing
        audio_vblank_8ad();
        mixer_8ad();
        
        // Sprites are now handled directly in each visualizer's render function
        // This ensures proper mode isolation and timing
        
        // Input handling
        unsigned short keys = ~REG_KEYINPUT & 0x3ff;
        unsigned short pressed = keys & ~last_keys;
        last_keys = keys;
        
        if (pressed & KEY_RIGHT) {
            next_track_8ad();
        }
        
        if (pressed & KEY_LEFT) {
            prev_track_8ad();
        }
        
        // Handle visualization switching (UP/DOWN)
        handle_visualization_controls(pressed);
        
        // Update and render current visualization
        update_current_visualization();
        render_current_visualization();
        
        // Display track title and visualization info
        static int last_displayed_track = -1;
        static int last_displayed_viz = 0; // VIZ_SPECTRUM_BARS
        int current_track_num = get_current_track_8ad();
        int current_viz = get_current_visualization();
        
        if (current_track_num != last_displayed_track || current_viz != last_displayed_viz) {
            last_displayed_track = current_track_num;
            last_displayed_viz = current_viz;
            
            // MODE_1: Use existing tile-based text system on BG1
            const char* track_name = get_full_track_name(current_track_num);
            const char* viz_name = get_visualization_name(current_viz);
            
            draw_text_tiles(1, 17, track_name);  // Row 17 (near bottom)
            draw_text_tiles(1, 18, viz_name);    // Row 18
            
            // DEBUG: Compact sprite debug info
            char debug_buffer[80];
            int active_first10 = 0;
            int total_active = 0;
            for (int i = 0; i < 128; i++) {
                if ((OAM[i].attr0 & ATTR0_DISABLED) == 0) {
                    total_active++;
                    if (i < 10) active_first10++;
                }
            }
            sprintf(debug_buffer, "Sprites: %d total, %d(0-9)", total_active, active_first10);
            // Debug text on row 19
            draw_text_tiles(1, 19, debug_buffer);
            
            // DEBUG: Show visualization-specific stats  
            if (current_viz == 0) { // VIZ_SPECTRUM_BARS
                char spectrum_debug[60];
                int render_calls = SPRITE_PALETTE[29];
                int tile_exists = SPRITE_PALETTE[30];
                int sprite_count = SPRITE_PALETTE[28];
                int bar0_height = SPRITE_PALETTE[25];
                int x_pos = SPRITE_PALETTE[26];
                int y_pos = SPRITE_PALETTE[27];
                int bar0_h = SPRITE_PALETTE[25];
                int bar1_h = SPRITE_PALETTE[26];
                int bar7_h = SPRITE_PALETTE[27];
                int bar0_x = SPRITE_PALETTE[16]; 
                int bar1_x = SPRITE_PALETTE[18];
                sprintf(spectrum_debug, "SPEC: s:%d x0:%d x1:%d", sprite_count, bar0_x, bar1_x);
                // Additional debug can be added if needed
            } else if (current_viz == 1) { // VIZ_WAVEFORM
                char debug_buffer2[60];
                int created = SPRITE_PALETTE[16];
                int attempted = SPRITE_PALETTE[17]; 
                int failed = SPRITE_PALETTE[18];
                int amplitude = SPRITE_PALETTE[19];
                sprintf(debug_buffer2, "WF: made:%d/%d fail:%d amp:%d", created, attempted, failed, amplitude);
                // Additional waveform debug can be added if needed
            }
        }
    }
    
    return 0;
}



