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

// CPU-optimized static text version - no scrolling
void draw_track_info_mode3_static(u16* buffer, GsmPlaybackTracker* playback) {
    // Only update when track changes - MAJOR CPU savings!
    static u32 last_displayed_track = 0xFFFFFFFF;
    
    if (playback->cur_song != last_displayed_track) {
        last_displayed_track = playback->cur_song;
        
        // Clear text area once
        for (int y = 140; y < 160; y++) {
            for (int x = 0; x < 240; x++) {
                buffer[y * 240 + x] = RGB5(0, 31, 0); // Green background
            }
        }
        
        // Get track name and truncate if needed (no scrolling!)
        const char* full_name = get_full_track_name(playback->cur_song);
        char display_name[30]; // Fit on screen
        strncpy(display_name, full_name, 29);
        display_name[29] = '\0';
        
        // Draw static text - only when track changes
        draw_text(buffer, 10, 145, display_name, RGB5(31, 31, 0));
    }
}

// GSM playback tracker
static GsmPlaybackTracker playback;

// GBFS filesystem support
static const GBFS_FILE *track_filesystem = NULL;

int main() {
    // Initialize interrupts and video
    irqInit();
    irqEnable(IRQ_VBLANK);
    
    // Use Mode 3 for beautiful bitmap text + sprites
    SetMode(MODE_3 | BG2_ENABLE | OBJ_ENABLE | OBJ_1D_MAP);
    
    // Spectrum analyzer visualization - 8 vertical bars
    #define NUM_BARS 8
    
    // Set up sprite palette for visualization bars
    SPRITE_PALETTE[0] = RGB5(0, 0, 0);      // Transparent
    SPRITE_PALETTE[1] = RGB5(0, 15, 31);    // Blue (low frequencies)
    SPRITE_PALETTE[2] = RGB5(0, 31, 15);    // Green-blue 
    SPRITE_PALETTE[3] = RGB5(0, 31, 0);     // Green (mid frequencies)
    SPRITE_PALETTE[4] = RGB5(15, 31, 0);    // Yellow-green
    SPRITE_PALETTE[5] = RGB5(31, 31, 0);    // Yellow (high frequencies)
    SPRITE_PALETTE[6] = RGB5(31, 15, 0);    // Orange
    SPRITE_PALETTE[7] = RGB5(31, 0, 0);     // Red (very high frequencies)
    
    // In Mode 3, framebuffer is 0x6000000-0x6012C00, so sprites must be after that
    u32* spriteGfx = (u32*)(0x6014000); // Safe location after Mode 3 framebuffer
    
    // Create 8x8 solid tiles for visualization bars (4bpp format)
    for(int tile = 0; tile < NUM_BARS; tile++) {
        u32 color_index = tile + 1; // Use different colors for each bar
        u32 pixel_data = (color_index << 0) | (color_index << 4) | (color_index << 8) | (color_index << 12) |
                        (color_index << 16) | (color_index << 20) | (color_index << 24) | (color_index << 28);
        
        // Fill 8 rows of the tile
        for(int row = 0; row < 8; row++) {
            spriteGfx[tile * 8 + row] = pixel_data;
        }
    }
    
    // Clear OAM
    for(int i = 0; i < 128; i++) {
        OAM[i].attr0 = ATTR0_DISABLED;
        OAM[i].attr1 = 0;
        OAM[i].attr2 = 0;
    }
    
    // Create 8 vertical bars for spectrum visualization
    // Position them in top area to avoid text overlap
    for(int i = 0; i < NUM_BARS; i++) {
        int x = 20 + (i * 25);  // Spread across screen width
        int y = 20;             // Top area
        
        OAM[i].attr0 = ATTR0_NORMAL | ATTR0_COLOR_16 | ATTR0_SQUARE | (y);
        OAM[i].attr1 = ATTR1_SIZE_8 | (x);
        // Tiles start at offset from 0x6014000, calculate proper tile number
        OAM[i].attr2 = ATTR2_PALETTE(0) | (512 + i); // 512 = offset for 0x6014000 location
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
        // WORKING spectrum visualizer - vertical bars with height animation
        static u32 animation_time = 0;
        animation_time++; // Increment every frame
        
        // Create 8 animated spectrum bars with different frequencies
        for(int i = 0; i < NUM_BARS; i++) {
            // Each bar has different animation speed and phase
            int phase = animation_time + (i * 20); // Different phase for each bar
            int frequency_divisor = 2 + (i / 2); // Bars get slower from left to right
            int height_wave = (phase / frequency_divisor) % 80; // 0-79 range
            
            // Convert to bar height (sine-like wave)
            int bar_height;
            if(height_wave < 40) {
                bar_height = height_wave; // Growing 0->39
            } else {
                bar_height = 80 - height_wave; // Shrinking 39->0
            }
            
            bar_height = 8 + bar_height; // 8-47 pixels tall
            
            // Position bars vertically (Y grows downward on GBA)
            int base_y = 110; // Bottom of bars (above text)
            int new_y = base_y - bar_height; // Top of bar
            
            // Make sure Y is within screen bounds
            if(new_y < 10) new_y = 10;
            if(new_y > 140) new_y = 140;
            
            // Update sprite Y position for height effect
            OAM[i].attr0 = (OAM[i].attr0 & ~0xFF) | (new_y & 0xFF);
            
            // Keep X positions fixed (no wobble, clean spectrum display)
            int x = 20 + (i * 25); // Evenly spaced across screen
            OAM[i].attr1 = (OAM[i].attr1 & ~0x01FF) | (x & 0x01FF);
        }
        
        // Use original GSMPlayer-GBA timing: 
        // advancePlayback() BEFORE VBlankIntrWait(), writeFromPlaybackBuffer() AFTER
        advancePlayback(&playback, &DEFAULT_PLAYBACK_INPUT_MAPPING);
        VBlankIntrWait();
        writeFromPlaybackBuffer(&playback);
        
        // Get framebuffer for Mode 3
        u16* framebuffer = (u16*)0x6000000;
        
        // Use CPU-optimized static text (no scrolling to save CPU for sprites)
        draw_track_info_mode3_static(framebuffer, &playback);
    }
    
    return 0;
}