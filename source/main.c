#include <gba.h>
#include <gba_dma.h>
#include <stdio.h>
#include <string.h>
#include "gbfs.h"
// 8AD audio system
#include "8ad_player.h"
#include "visualization_manager.h"
#include "spectrum_visualizer.h"
#include "waveform_visualizer.h"
#include "polygondwanaland_128.h"
#include "album_cover.h"

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
    
    // MODE_0: Use regular tiled backgrounds - BG2 confirmed working
    // Enable all layers now that we know BG2 renders correctly
    SetMode(MODE_0 | BG0_ENABLE | BG1_ENABLE | BG2_ENABLE | OBJ_ENABLE | OBJ_1D_MAP);
    
    // Wait for any pending DMA operations like album cover does
    while (REG_DMA3CNT & DMA_ENABLE) VBlankIntrWait();
    
    // Clear character base 0 completely to remove any garbage data
    u16* char_base_0 = (u16*)CHAR_BASE_ADR(0);
    for (int i = 0; i < 2048; i++) { // Clear 4KB of character data (96 tiles * 32 bytes / 2)
        char_base_0[i] = 0x0000;
    }
    
    // Configure BG1 for text overlay (uses existing font system)
    REG_BG1CNT = BG_SIZE_0 | BG_16_COLOR | BG_PRIORITY(0) | CHAR_BASE(0) | SCREEN_BASE(30);
    REG_BG1HOFS = 0;
    REG_BG1VOFS = 0;
    
    // Configure BG2 for solid background
    REG_BG2CNT = BG_SIZE_0 | BG_256_COLOR | CHAR_BASE(1) | SCREEN_BASE(29) | BG_PRIORITY(2);
    
    // Clear BG1 tilemap (text layer) BEFORE enabling it
    u16* textMap = (u16*)SCREEN_BASE_BLOCK(30);
    for (int i = 0; i < 1024; i++) {
        textMap[i] = 0;
    }
    
    // Now that we know BG2 works, use simple solid background
    u16* bgMap = (u16*)SCREEN_BASE_BLOCK(29);
    for (int i = 0; i < 1024; i++) {
        bgMap[i] = 1; // Use tile 1 for solid background
    }
    
    // Create a simple background tile at index 1 in character base 1
    u8* tileMem = (u8*)0x6004000; // Character base 1
    
    // Clear the entire tile memory first
    for (int i = 0; i < 128; i++) { // Clear tiles 0 and 1
        tileMem[i] = 0;
    }
    
    // DEBUG: Create multiple test tiles with different colors
    // Tile 1: All palette index 1 (magenta)
    for (int i = 0; i < 64; i++) {
        tileMem[64 + i] = 1; // Tile 1 = palette index 1
    }
    // Tile 2: All palette index 2 (yellow)  
    for (int i = 0; i < 64; i++) {
        tileMem[128 + i] = 2; // Tile 2 = palette index 2
    }
    // Tile 3: All palette index 3 (cyan)
    for (int i = 0; i < 64; i++) {
        tileMem[192 + i] = 3; // Tile 3 = palette index 3
    }
    
    // Set up Game Boy green background color palette
    // Game Boy green (#9bbc0f = 155,188,15 RGB) converted to RGB5
    BG_PALETTE[0] = RGB5(0, 0, 0);    // Black (unused)
    BG_PALETTE[1] = RGB5(19, 23, 1);  // Game Boy bright green for tile 1
    
    // Also ensure the font text color is set correctly
    BG_PALETTE[17] = RGB5(1, 7, 1);    // Game Boy darkest green text
    
    // EXPERIMENT: Load the complete album palette to see if that activates font system
    // This replicates exactly what album cover does for palette initialization
    for (int i = 2; i < 256; i++) { // Start from 2 to preserve BG_PALETTE[0] and BG_PALETTE[1]
        u16 originalColor = polygondwanaland_128Pal[i];
        u16 r = (originalColor & 0x1F);           // Extract R
        u16 g = (originalColor >> 5) & 0x1F;     // Extract G  
        u16 b = (originalColor >> 10) & 0x1F;    // Extract B
        
        // Swap R and B channels to fix the color mapping (same as album cover)
        BG_PALETTE[i] = b | (g << 5) | (r << 10);  // B, G, R instead of R, G, B
    }
    
    // CRITICAL: Font system needs palette 1 (entries 16-17) for text rendering
    // Set up font palette after loading album colors - Game Boy colors
    BG_PALETTE[16] = RGB5(0, 0, 0);      // Color 0 of palette 1: transparent/black
    BG_PALETTE[17] = RGB5(1, 7, 1);      // Color 1 of palette 1: darkest Game Boy green text
    
    // CRITICAL: Initialize ALL OAM entries to disabled state first
    // This prevents sprite artifacts from previous sessions or undefined memory
    for (int i = 0; i < 128; i++) {
        OAM[i].attr0 = ATTR0_DISABLED;
        OAM[i].attr1 = 0;
        OAM[i].attr2 = 0;
    }
    
    // Initialize visualization manager
    init_visualization_manager();
    
    // CRITICAL: Initialize font system AFTER visualization system
    // This ensures font tiles don't get corrupted by other graphics initialization
    init_font_tiles();
    
    // EXPERIMENT: Test if album cover initialization sequence enables font system
    // Initialize album cover briefly, then clean it up - testing if the init sequence
    // itself (not the ongoing display) is what permanently fixes font rendering
    init_album_cover();
    cleanup_album_cover();
    
    // CRITICAL: Reinitialize the visualization manager to correct mode after album cover test
    // The direct init_album_cover() call may have corrupted the manager's state
    init_spectrum_visualizer();
    
    
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
    
    // CRITICAL: Now that audio system is ready, set proper Game Boy colors
    // This ensures track detection works correctly and colors are set properly
    update_album_cover_colors();
    
    // FINAL OVERRIDE: Force the background tile color after ALL initialization
    BG_PALETTE[1] = RGB5(19, 23, 1);  // Game Boy bright green for tile 1
    
    // Font system already initialized above
    
    unsigned short last_keys = 0;
    
    // Visualization mode variables removed - handled by visualization_manager
    
    // Main loop with spectrum analyzer
    while(1) {
        VBlankIntrWait();
        
        // CRITICAL: Update sprites immediately after VBlank to prevent tearing
        render_current_visualization();
        
        // 8AD audio processing
        audio_vblank_8ad();
        mixer_8ad();
        
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
        
        if (pressed & KEY_A) {
            toggle_pause_8ad();
        }
        
        // Handle visualization switching (UP/DOWN)
        handle_visualization_controls(pressed);
        
        // CRITICAL: Set background color based on current track
        if (is_final_track_8ad()) {
            BG_PALETTE[1] = RGB5(31, 31, 31);  // White background for "The Fourth Colour"
        } else {
            BG_PALETTE[1] = RGB5(19, 23, 1);   // Game Boy bright green for all other tracks
        }
        
        // Update visualization data for next frame
        update_current_visualization();
        
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
            
            // EXPERIMENT: Now that floating zeros are fixed, try enabling text in all modes
            // to see if album cover's init sequence was the missing piece
            draw_text_tiles(1, 17, track_name);  // Row 17 (near bottom)
            draw_text_tiles(1, 18, viz_name);    // Row 18
            
            // Clean interface - no debug text needed
        }
    }
    
    return 0;
}



