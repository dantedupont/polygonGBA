#include <gba.h>
#include <stdio.h>
#include <string.h>
#include "gbfs.h"
#include "libgsm.h"
#include "font.h"


// Full track name mapping
const char* get_full_track_name(int track_index) {
    // Map track indices to full names
    static const char* track_names[] = {
        "Crumbling Castle",
        "Polygondwanaland", 
        "The Castle In The Air",
        "Deserted Dunes Welcome Weary Feet",
        "Inner Cell",
        "Loyalty",
        "Horology", 
        "Tetrachromacy",
        "Searching...",
        "The Fourth Colour"
    };
    
    if (track_index >= 0 && track_index < 10) {
        return track_names[track_index];
    }
    return "Unknown Track";
}

// Scrolling text state
static char full_track_name[64] = {0};
static int scroll_offset = 0;
static int scroll_delay = 0;
static u32 last_track_for_scroll = 0xFFFFFFFF;

void draw_scrolling_text(u16* buffer, int x, int y, const char* text, u16 color, int offset) {
    int text_len = strlen(text);
    int screen_chars = (240 - x) / 8; // How many chars fit on screen from x position
    
    if (text_len <= screen_chars) {
        // Text fits, no scrolling needed
        draw_text(buffer, x, y, text, color);
        return;
    }
    
    // Create display window with scrolling
    char display_text[32] = {0};
    int start_pos = offset;
    
    // Wrap around when we reach the end
    if (start_pos >= text_len) {
        start_pos = 0;
    }
    
    // Copy characters for display with wrapping
    for (int i = 0; i < screen_chars - 1 && i < 30; i++) {
        int src_pos = (start_pos + i) % text_len;
        display_text[i] = text[src_pos];
    }
    display_text[screen_chars - 1] = '\0';
    
    draw_text(buffer, x, y, display_text, color);
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
        scroll_offset = 0;
        scroll_delay = 0;
        
        // Get full track name from mapping
        const char* full_name = get_full_track_name(playback->cur_song);
        strncpy(full_track_name, full_name, 63);
        full_track_name[63] = '\0';
    }
    
    // Update scrolling animation
    scroll_delay++;
    if (scroll_delay >= 30) { // Scroll every 30 frames (0.5 seconds at 60fps)
        scroll_delay = 0;
        scroll_offset++;
        
        // Reset scroll when we've shown the full text
        int text_len = strlen(full_track_name);
        int screen_chars = (240 - 10) / 8;
        if (scroll_offset >= text_len + screen_chars) {
            scroll_offset = 0;
        }
    }
    
    // Draw scrolling track name
    draw_scrolling_text(buffer, 10, 145, full_track_name, RGB5(31, 31, 0), scroll_offset);
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