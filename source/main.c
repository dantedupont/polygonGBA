#include <gba.h>
#include <stdio.h>
#include <string.h>
#include "gbfs.h"
// 8AD audio system
#include "8ad_player.h"
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
    
    // Visualization mode (0=spectrum, 1=waveform)
    int visualization_mode = 0;
    int last_viz_mode = -1;
    
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
        
        // Handle visualization switching
        if (pressed & KEY_UP) {
            visualization_mode = (visualization_mode + 1) % 2; // 0=spectrum, 1=waveform
        }
        
        if (pressed & KEY_DOWN) {
            visualization_mode = (visualization_mode - 1 + 2) % 2;
        }
        
        // Handle visualization mode switching
        if (visualization_mode != last_viz_mode) {
            // Clean up previous visualization
            if (last_viz_mode == 0) {
                cleanup_spectrum_visualizer();
            } else if (last_viz_mode == 1) {
                cleanup_waveform_visualizer();
            }
            
            // Initialize new visualization
            if (visualization_mode == 0) {
                init_spectrum_visualizer();
            } else if (visualization_mode == 1) {
                init_waveform_visualizer();
            }
            
            last_viz_mode = visualization_mode;
        }
        
        // Update and render current visualization
        if (visualization_mode == 0) {
            update_spectrum_visualizer();
            render_spectrum_bars();
        } else if (visualization_mode == 1) {
            update_waveform_visualizer();
            render_waveform();
        }
        
        // Display track title and visualization info
        static int last_displayed_track = -1;
        static int last_displayed_viz = -1;
        int current_track_num = get_current_track_8ad();
        
        if (current_track_num != last_displayed_track || visualization_mode != last_displayed_viz) {
            last_displayed_track = current_track_num;
            last_displayed_viz = visualization_mode;
            
            // Clear and draw blue background for info area
            u16* framebuffer = (u16*)0x6000000;
            for (int y = 130; y < 155; y++) {
                for (int x = 0; x < 240; x++) {
                    framebuffer[y * 240 + x] = RGB5(0, 0, 15); // Blue background
                }
            }
            
            // Draw track title
            const char* track_name = get_full_track_name(current_track_num);
            draw_text(framebuffer, 10, 135, track_name, RGB5(31, 31, 0)); // Yellow text
            
            // Draw visualization name
            const char* viz_name = (visualization_mode == 0) ? "Spectrum Bars" : "Waveform";
            draw_text(framebuffer, 10, 145, viz_name, RGB5(15, 31, 15)); // Light green text
        }
    }
    
    return 0;
}


