#include <gba.h>
#include <stdio.h>
#include <string.h>
#include "gbfs.h"

// PolygonGBA - 8AD Audio Visualizer

// 8AD audio system
#if defined(CLEAN_8AD) || defined(USE_8AD)
#include "8ad_player.h"
#include "spectrum_visualizer.h"
#endif

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








#ifdef CLEAN_8AD
// Clean 8AD main function - simplified Pin Eight style
int main() {
    // Initialize interrupts and video
    irqInit();
    irqEnable(IRQ_VBLANK);
    
    // Use Mode 3 for simple display
    SetMode(MODE_3 | BG2_ENABLE);
    
    // Clear screen with BRIGHT MAGENTA - VERY OBVIOUS TEST
    u16* framebuffer = (u16*)0x6000000;
    for(int i = 0; i < 240*160; i++) {
        framebuffer[i] = RGB5(31, 0, 31); // Bright magenta background
    }
    
    // Initialize GBFS
    extern const GBFS_FILE *fs;
    fs = find_first_gbfs_file(find_first_gbfs_file);
    
    if (!fs) {
        // Failed - red background
        for(int i = 0; i < 240*160; i++) {
            framebuffer[i] = RGB5(31, 0, 0);
        }
        while(1) VBlankIntrWait();
    }
    
    // Initialize clean 8AD system
    init_8ad_sound();
    
    // Start first track
    start_8ad_track(0);
    
    // Initialize font system and show OBVIOUS test messages
    init_font_tiles();
    draw_text(framebuffer, 10, 10, "*** CLEAN 8AD TEST ***", RGB5(31, 31, 31));
    draw_text(framebuffer, 10, 30, "MAGENTA BACKGROUND!", RGB5(31, 31, 0));
    draw_text(framebuffer, 10, 50, "BASIC VERSION WORKS", RGB5(0, 31, 31));
    
    // Simple control variables
    unsigned short last_keys = 0;
    
    // Main loop - simple Pin Eight style
    while(1) {
        VBlankIntrWait();
        
        // Audio processing
        audio_vblank_8ad();
        mixer_8ad();
        
        // Simple controls
        unsigned short keys = ~REG_KEYINPUT & 0x3ff;
        unsigned short pressed = keys & ~last_keys;
        last_keys = keys;
        
        if (pressed & KEY_RIGHT) {
            next_track_8ad();
            char track_info[64];
            sprintf(track_info, "Track: %s", get_full_track_name(get_current_track_8ad()));
            // Clear old text area
            for(int y = 25; y < 40; y++) {
                for(int x = 0; x < 240; x++) {
                    framebuffer[y * 240 + x] = RGB5(0, 8, 0);
                }
            }
            draw_text(framebuffer, 10, 30, track_info, RGB5(31, 31, 0));
        }
        
        if (pressed & KEY_LEFT) {
            prev_track_8ad();
            char track_info[64];
            sprintf(track_info, "Track: %s", get_full_track_name(get_current_track_8ad()));
            // Clear old text area
            for(int y = 25; y < 40; y++) {
                for(int x = 0; x < 240; x++) {
                    framebuffer[y * 240 + x] = RGB5(0, 8, 0);
                }
            }
            draw_text(framebuffer, 10, 30, track_info, RGB5(31, 31, 0));
        }
    }
    
    return 0;
}

#elif defined(USE_8AD)
// 8AD main function with spectrum visualizer
int main() {
    // Initialize interrupts and video
    irqInit();
    irqEnable(IRQ_VBLANK);
    
    // Use Mode 3 with sprites enabled for testing
    SetMode(MODE_3 | BG2_ENABLE | OBJ_ENABLE | OBJ_1D_MAP);
    
    // Clear screen to black
    u16* framebuffer = (u16*)0x6000000;
    for(int i = 0; i < 240*160; i++) {
        framebuffer[i] = RGB5(0, 0, 0); // Black background
    }
    
    // Initialize spectrum visualizer
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
    
    // Initialize font system for track titles (disabled - Mode 3 uses direct framebuffer)
    // init_font_tiles();
    
    unsigned short last_keys = 0;
    
    // Main loop with spectrum analyzer
    while(1) {
        VBlankIntrWait();
        
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
        
        // Update spectrum analysis and render bars
        update_spectrum_visualizer();
        render_spectrum_bars();
        
        // Display track title
        static int last_displayed_track = -1;
        int current_track_num = get_current_track_8ad();
        
        if (current_track_num != last_displayed_track) {
            last_displayed_track = current_track_num;
            
            // Clear and draw blue background for track title
            u16* framebuffer = (u16*)0x6000000;
            for (int y = 140; y < 160; y++) {
                for (int x = 0; x < 240; x++) {
                    framebuffer[y * 240 + x] = RGB5(0, 0, 15); // Blue background
                }
            }
            
            // Draw track title
            const char* track_name = get_full_track_name(current_track_num);
            draw_text(framebuffer, 10, 145, track_name, RGB5(31, 31, 0)); // Yellow text
        }
    }
    
    return 0;
}


#endif // CLEAN_8AD / USE_8AD