#include <gba.h>
#include <stdio.h>
#include <string.h>
#include "gbfs.h"
#include "libgsm.h"
#include "font.h"


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
    
    if (total_tracks == 4) {
        // Side A ROM
        if (track_index >= 0 && track_index < 4) {
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

// Scrolling text state
static char full_track_name[64] = {0};
static int scroll_pixel_offset = 0;
static int scroll_delay = 0;
static int pause_counter = 0;
static u32 last_track_for_scroll = 0xFFFFFFFF;

void draw_scrolling_text(u16* buffer, int x, int y, const char* text, u16 color, int pixel_offset, int pause_active) {
    int text_len = strlen(text);
    int text_width_pixels = text_len * 8; // Each character is 8 pixels wide
    int screen_width = 240 - x; // Available width from x position
    
    if (text_width_pixels <= screen_width) {
        // Text fits, no scrolling needed
        draw_text(buffer, x, y, text, color);
        return;
    }
    
    // Immediate restart approach: scroll text left by pixel_offset
    int display_x = x - pixel_offset;
    
    // Only draw if text is still visible (any part on screen)
    if (display_x > -text_width_pixels) {
        draw_text(buffer, display_x, y, text, color);
    }
}

void draw_track_info(u16* buffer, GsmPlaybackTracker* playback) {
    // Clear info area (bottom portion of screen)
    for (int y = 140; y < 160; y++) {
        for (int x = 0; x < 240; x++) {
            buffer[y * 240 + x] = RGB5(0, 0, 0); // Black background
        }
    }
    
    // Check if track changed - reset scrolling
    if (playback->cur_song != last_track_for_scroll) {
        last_track_for_scroll = playback->cur_song;
        scroll_pixel_offset = 0;
        scroll_delay = 0;
        pause_counter = 0;
        
        // Get full track name from mapping
        const char* full_name = get_full_track_name(playback->cur_song);
        strncpy(full_track_name, full_name, 63);
        full_track_name[63] = '\0';
    }
    
    // Calculate text properties for scrolling
    int text_len = strlen(full_track_name);
    int text_width = text_len * 8;
    int screen_width = 240 - 10;
    
    // Only scroll if text is longer than screen
    if (text_width > screen_width) {
        // Update scrolling animation
        if (pause_counter > 0) {
            pause_counter--; // Count down pause
            scroll_pixel_offset = 0; // Reset to start position during pause
        } else {
            scroll_delay++;
            if (scroll_delay >= 1) { // Scroll every frame
                scroll_delay = 0;
                scroll_pixel_offset += 8; // Move 8 pixels per frame
                
                // When text has scrolled exactly its width, it's completely gone - restart immediately
                if (scroll_pixel_offset >= text_width) {
                    scroll_pixel_offset = 0; // Restart immediately, no pause
                }
            }
        }
    }
    
    // Draw scrolling track name
    draw_scrolling_text(buffer, 10, 145, full_track_name, RGB5(31, 31, 0), scroll_pixel_offset, pause_counter > 0);
}

// GSM playback tracker
static GsmPlaybackTracker playback;

// GBFS filesystem support
static const GBFS_FILE *track_filesystem = NULL;

int main() {
    // Initialize interrupts and video
    irqInit();
    irqEnable(IRQ_VBLANK);
    
    SetMode(MODE_3 | BG2_ENABLE);
    u16* videoBuffer = (u16*)0x6000000;
    
    // Initialize GBFS filesystem
    track_filesystem = find_first_gbfs_file(find_first_gbfs_file);
    
    // Initialize GSM playback system
    if (initPlayback(&playback) == 0) {
        // Success - show green screen
        for(int i = 0; i < 240*160; i++) {
            videoBuffer[i] = RGB5(0, 31, 0);
        }
    } else {
        // Failed - show red screen
        for(int i = 0; i < 240*160; i++) {
            videoBuffer[i] = RGB5(31, 0, 0);
        }
        while(1) VBlankIntrWait();
    }
    
    // Main loop with proper GSM timing
    while(1) {
        // Use original GSMPlayer-GBA timing: 
        // advancePlayback() BEFORE VBlankIntrWait(), writeFromPlaybackBuffer() AFTER
        advancePlayback(&playback, &DEFAULT_PLAYBACK_INPUT_MAPPING);
        VBlankIntrWait();
        writeFromPlaybackBuffer(&playback);
        
        // Show playback status and track info (less frequently to reduce CPU load)
        static u32 display_counter = 0;
        static u32 last_song = 0xFFFFFFFF; // Track when song changes
        display_counter++;
        
        // Update display every second OR when track changes
        if(display_counter >= 60 || last_song != playback.cur_song) {
            display_counter = 0;
            last_song = playback.cur_song;
            
            // Draw track information
            draw_track_info(videoBuffer, &playback);
            
            // Show playing status indicator (small colored square)
            u16 status_color = playback.playing ? RGB5(0, 31, 0) : RGB5(31, 31, 0);
            for(int i = 140; i < 150; i++) {
                for(int j = 220; j < 230; j++) {
                    videoBuffer[i * 240 + j] = status_color;
                }
            }
            
            // Show controls lock status
            if (playback.locked) {
                u16 lock_color = RGB5(31, 0, 0); // Red for locked
                for(int i = 140; i < 150; i++) {
                    for(int j = 210; j < 220; j++) {
                        videoBuffer[i * 240 + j] = lock_color;
                    }
                }
            }
        }
    }
    
    return 0;
}