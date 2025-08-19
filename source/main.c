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

// Forward declarations

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


void draw_track_info_mode3(u16* buffer, GsmPlaybackTracker* playback) {
    // Clear text area (bottom portion of screen)
    for (int y = 140; y < 160; y++) {
        for (int x = 0; x < 240; x++) {
            buffer[y * 240 + x] = RGB5(0, 31, 0); // Green background
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
    
    // Draw scrolling track name using the beautiful original Mode 3 rendering
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
    
    // Use Mode 3 (your favorite!) with careful sprite placement
    SetMode(MODE_3 | BG2_ENABLE | OBJ_ENABLE | OBJ_1D_MAP);
    u16* videoBuffer = (u16*)0x6000000;
    
    // Fill screen with green background
    for(int i = 0; i < 240*160; i++) {
        videoBuffer[i] = RGB5(0, 31, 0);
    }
    
    // Initialize hardware sprite spectrum analyzer
    #define NUM_BARS 8  // Only use 8 bars for better performance
    
    // Set up sprite palette (16-color mode) - use palettes 0-3
    SPRITE_PALETTE[0] = RGB5(0, 0, 0);      // Transparent
    SPRITE_PALETTE[1] = RGB5(0, 31, 0);     // Green (low)
    SPRITE_PALETTE[2] = RGB5(31, 31, 0);    // Yellow (medium)
    SPRITE_PALETTE[3] = RGB5(31, 0, 0);     // Red (high)
    
    // Set up second palette for variation
    SPRITE_PALETTE[16] = RGB5(0, 0, 0);     // Transparent
    SPRITE_PALETTE[17] = RGB5(0, 31, 31);   // Cyan
    SPRITE_PALETTE[18] = RGB5(31, 0, 31);   // Magenta
    SPRITE_PALETTE[19] = RGB5(31, 31, 31);  // White
    
    // Create solid bar tile (8x8 pixels, 4-bit per pixel)
    // Each pixel uses 4 bits, so 2 pixels per byte, 4 bytes per row
    u32* spriteGfx = (u32*)(0x6010000);  // Sprite GFX starts here
    
    // Fill tile 0 with solid color (pixel value 1)
    for(int i = 0; i < 8; i++) {
        spriteGfx[i] = 0x11111111;  // All pixels set to color index 1
    }
    
    // Initialize ALL sprites as disabled first
    for(int i = 0; i < 128; i++) {  // Clear all 128 possible sprites
        OAM[i].attr0 = ATTR0_DISABLED;
        OAM[i].attr1 = 0;
        OAM[i].attr2 = 0;
    }
    
    // Initialize GBFS filesystem
    track_filesystem = find_first_gbfs_file(find_first_gbfs_file);
    
    // Initialize GSM playback system
    if (initPlayback(&playback) == 0) {
        // Success - green background already set via BG_PALETTE[0]
    } else {
        // Failed - change background to red
        BG_PALETTE[0] = RGB5(31, 0, 0);  // Red background
        while(1) VBlankIntrWait();
    }
    
    // Main loop with proper GSM timing
    while(1) {
        // Use original GSMPlayer-GBA timing: 
        // advancePlayback() BEFORE VBlankIntrWait(), writeFromPlaybackBuffer() AFTER
        advancePlayback(&playback, &DEFAULT_PLAYBACK_INPUT_MAPPING);
        VBlankIntrWait();
        writeFromPlaybackBuffer(&playback);
        
        // Ultra-minimal sprite visualization - preserve audio quality above all
        static u32 viz_frame_counter = 0;
        viz_frame_counter++;
        if (viz_frame_counter >= 20) { // Update every 20 frames (3fps) for better visibility
            viz_frame_counter = 0;
            int sample_intensity = playback.last_sample < 0 ? -playback.last_sample : playback.last_sample;
            sample_intensity = (sample_intensity >> 6) & 0x3FF; // Scale to 0-1023
            
            // Ensure minimum activity for testing
            if(sample_intensity < 64) sample_intensity = 64 + (viz_frame_counter & 31); // Always show activity
            
            // Update all 8 sprites with clear positioning
            for(int i = 0; i < NUM_BARS; i++) {
                int x = 20 + i * 25;  // Clear spacing: 20, 45, 70, 95, 120, 145, 170, 195
                
                // Create distinct bar heights per channel
                int bar_height = (sample_intensity >> 4) + (i * 2) + 8; // Base height + variation
                if(i < 3) bar_height += 4;      // Bass boost
                else if(i > 5) bar_height -= 2; // Treble reduce
                
                // Add frame-based animation for testing
                bar_height += (viz_frame_counter + i) & 7; // Animated variation
                
                if(bar_height > 40) bar_height = 40; // Limit height
                if(bar_height < 8) bar_height = 8;   // Minimum height
                
                int y = 80 - bar_height;  // Position from top of screen
                if(y < 20) y = 20; // Don't go too high
                if(y > 100) y = 100; // Don't go too low
                
                // Use different palettes for variety
                int palette = (i & 1) ? 1 : 0;  // Alternate between palette 0 and 1
                
                // Configure sprite with clear attributes
                OAM[i].attr0 = ATTR0_NORMAL | ATTR0_COLOR_16 | ATTR0_SQUARE | (y & 0xFF);
                OAM[i].attr1 = ATTR1_SIZE_8 | (x & 0x1FF);
                OAM[i].attr2 = ATTR2_PALETTE(palette) | 0; // Use tile 0
            }
            
            // Ensure unused sprites stay hidden
            for(int i = NUM_BARS; i < 32; i++) {
                OAM[i].attr0 = ATTR0_DISABLED;
            }
        }
        
        // Show playback status and track info (less frequently to reduce CPU load)
        static u32 display_counter = 0;
        static u32 last_song = 0xFFFFFFFF; // Track when song changes
        display_counter++;
        
        // Update display every second OR when track changes
        if(display_counter >= 60 || last_song != playback.cur_song) {
            display_counter = 0;
            last_song = playback.cur_song;
            
            // Draw track information using beautiful Mode 3 bitmap text
            draw_track_info_mode3(videoBuffer, &playback);
            
            // Draw status indicators using Mode 3 bitmap text
            draw_text(videoBuffer, 220, 145, playback.playing ? ">" : "||", RGB5(31, 31, 0)); // Yellow play/pause indicator
            if (playback.locked) {
                draw_text(videoBuffer, 230, 145, "L", RGB5(31, 0, 0)); // Red lock indicator
            }
        }
    }
    
    return 0;
}