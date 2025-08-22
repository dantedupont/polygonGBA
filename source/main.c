#include <gba.h>
#include <stdio.h>
#include <string.h>
#include "gbfs.h"

// 8AD flag is now working properly!

// Conditional audio system includes
#ifdef CLEAN_8AD
#include "8ad_player.h"
#elif defined(USE_8AD)
#include "8ad_player.h"
#else
#include "libgsm.h"
#endif

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
    
    
    if (total_tracks == 4 || total_tracks == 3) {
        // Side A ROM (4 tracks for GSM, 3 tracks for 8AD test)
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

// Scrolling text state (only for GSM builds)
#if !defined(CLEAN_8AD) && !defined(USE_8AD)
static char full_track_name[64] = {0};
static int scroll_pixel_offset = 0;
static int scroll_delay = 0;
static int pause_counter = 0;
static u32 last_track_for_scroll = 0xFFFFFFFF;
#endif

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


#if !defined(CLEAN_8AD) && !defined(USE_8AD)
void draw_track_info_mode3(u16* buffer, GsmPlaybackTracker* playback) {
    // Clear text area (bottom portion of screen)
    for (int y = 140; y < 160; y++) {
        for (int x = 0; x < 240; x++) {
            buffer[y * 240 + x] = RGB5(0, 0, 15); // Green background
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
#endif // !CLEAN_8AD && !USE_8AD

// CPU-optimized static text version - no scrolling
#if !defined(CLEAN_8AD) && !defined(USE_8AD)
void draw_track_info_mode3_static(u16* buffer, GsmPlaybackTracker* playback) {
    // Only update when track changes - MAJOR CPU savings!
    static u32 last_displayed_track = 0xFFFFFFFF;
    
    if (playback->cur_song != last_displayed_track) {
        last_displayed_track = playback->cur_song;
        
        // Clear text area once
        for (int y = 140; y < 160; y++) {
            for (int x = 0; x < 240; x++) {
                buffer[y * 240 + x] = RGB5(0, 0, 15); // Green background
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
#endif // !CLEAN_8AD && !USE_8AD

// Audio playback tracker (conditional)
#if !defined(CLEAN_8AD) && !defined(USE_8AD)
static GsmPlaybackTracker playback;
#endif

// GBFS filesystem support
static const GBFS_FILE *track_filesystem = NULL;

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
    #define NUM_BARS 8
    #define TILES_PER_BAR 12  // Each bar can be up to 12 tiles tall (96 pixels) - MASSIVE bars!
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
    
    // Clear ALL sprite tile memory to prevent garbage artifacts
    // Sprite tiles start at 0x6010000 and go to 0x6018000 (32KB)
    u32* spriteMem = (u32*)0x6010000;
    for(int i = 0; i < (0x8000 / 4); i++) {
        spriteMem[i] = 0; // Clear all sprite tile memory
    }
    
    // Set up sprite palette for spectrum bars
    SPRITE_PALETTE[0] = RGB5(0, 0, 0);      // Transparent
    SPRITE_PALETTE[1] = RGB5(0, 15, 31);    // Blue (low frequencies)
    SPRITE_PALETTE[2] = RGB5(0, 31, 15);    // Green-blue 
    SPRITE_PALETTE[3] = RGB5(0, 31, 0);     // Green (mid frequencies)
    SPRITE_PALETTE[4] = RGB5(15, 31, 0);    // Yellow-green
    SPRITE_PALETTE[5] = RGB5(31, 31, 0);    // Yellow (high frequencies)
    SPRITE_PALETTE[6] = RGB5(31, 15, 0);    // Orange
    SPRITE_PALETTE[7] = RGB5(31, 0, 0);     // Red (very high frequencies)
    SPRITE_PALETTE[8] = RGB5(31, 0, 31);    // Bright magenta for bar 8
    
    // In Mode 3, framebuffer is 0x6000000-0x6012C00, so sprites must be after that
    u32* spriteGfx = (u32*)(0x6014000); // Safe location after Mode 3 framebuffer
    
    // Create 32x8 tiles for ULTRA WIDE, MASSIVE height bars with no color bleeding
    // Each bar gets completely separate tile memory space
    for(int bar = 0; bar < NUM_BARS; bar++) {
        u32 base_color = bar + 1; // Use different colors for each bar
        
        // Create 32x8 sprite data: each 32x8 sprite uses 4 consecutive 8x8 tiles
        u32 pixel_data = (base_color << 0) | (base_color << 4) | (base_color << 8) | (base_color << 12) |
                        (base_color << 16) | (base_color << 20) | (base_color << 24) | (base_color << 28);
        
        // Each bar gets 12 tile quads (48 tiles total per bar) - no overlap!
        for(int tile_quad = 0; tile_quad < TILES_PER_BAR; tile_quad++) {
            int base_tile = (bar * TILES_PER_BAR + tile_quad) * 4; // Each 32x8 uses 4 8x8 tiles
            
            // Create all 4 tiles for this 32x8 sprite
            for(int subtile = 0; subtile < 4; subtile++) {
                for(int row = 0; row < 8; row++) {
                    spriteGfx[(base_tile + subtile) * 8 + row] = pixel_data;
                }
            }
        }
    }
    
    // Clear OAM
    for(int i = 0; i < 128; i++) {
        OAM[i].attr0 = ATTR0_DISABLED;
        OAM[i].attr1 = 0;
        OAM[i].attr2 = 0;
    }
    
    // Initialize GBFS filesystem
    track_filesystem = find_first_gbfs_file(find_first_gbfs_file);
    
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
        
        // Skip spectrum analysis for now - just get basic display working
        
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
        
        // 8AD Spectrum visualization - REAL AUDIO RESPONSE!
        static int reset_counter = 0;
        static int bar_current_heights[NUM_BARS] = {8,8,8,8,8,8,8,8};
        static int bar_target_heights[NUM_BARS] = {8,8,8,8,8,8,8,8};
        static int bar_velocities[NUM_BARS] = {0,0,0,0,0,0,0,0};
        static long previous_amplitudes[NUM_BARS] = {0};
        static int adaptive_scale = 1000; // Dynamic scaling factor
        
        reset_counter++;
        if(reset_counter >= 4) {
            reset_counter = 0;
            
            // Enhanced dynamic scaling for MAXIMUM visual drama
            long total_amplitude = 0;
            long max_amplitude = 0;
            for(int i = 0; i < NUM_BARS; i++) {
                total_amplitude += spectrum_accumulators_8ad[i];
                if(spectrum_accumulators_8ad[i] > max_amplitude) {
                    max_amplitude = spectrum_accumulators_8ad[i];
                }
            }
            
            // More aggressive dynamic scaling for dramatic visuals
            if(total_amplitude > 0 && spectrum_sample_count_8ad > 0) {
                long avg_total = total_amplitude / NUM_BARS;
                
                // Much more sensitive scaling - responds to smaller changes
                if(avg_total > adaptive_scale) {
                    adaptive_scale = avg_total / 2; // More aggressive scale reduction
                } else if(avg_total < adaptive_scale / 4) {
                    adaptive_scale = avg_total * 3; // More aggressive scale increase
                }
                
                // Wider clamp range for more dramatic visuals
                if(adaptive_scale < 200) adaptive_scale = 200;   // More sensitive to quiet parts
                if(adaptive_scale > 8000) adaptive_scale = 8000; // Handle very loud parts
            }
            
            for(int i = 0; i < NUM_BARS; i++) {
                long accumulated_amplitude = spectrum_accumulators_8ad[i];
                int target_height = 8; // Minimum height
                
                if(spectrum_sample_count_8ad > 0 && accumulated_amplitude > 0) {
                    // Fast responsiveness calculation - optimized for GBA
                    long amplitude_change = accumulated_amplitude - previous_amplitudes[i];
                    long base_amplitude = accumulated_amplitude >> 3; // Fast divide by 8 instead of complex division
                    
                    // Simplified dramatic response using bit shifts
                    int relative_boost = 0;
                    if(amplitude_change > 0) {
                        relative_boost = (amplitude_change << 1) / adaptive_scale; // 2x change sensitivity
                    }
                    int base_response = (base_amplitude << 1) / adaptive_scale; // 2x base sensitivity (reduced from 3x)
                    
                    // Fast peak detection using bit operations
                    int peak_boost = 0;
                    if(accumulated_amplitude > (max_amplitude >> 1) + (max_amplitude >> 2)) { // Fast 75% check
                        peak_boost = 8; // Extra height for peak frequencies
                    }
                    
                    target_height = 8 + base_response + relative_boost + peak_boost;
                    
                    // BALANCED frequency-specific behavior - ensure all bars are lively
                    if(i == 0) {
                        // Sub-bass: Massive boost to make it always visible
                        target_height += target_height; // +100% boost (was +50%)
                        target_height += 8; // Larger baseline for visibility
                    } else if(i == 1) {
                        // Bass: Very strong response
                        target_height += (target_height >> 1) + (target_height >> 2); // +75% boost
                        target_height += 5; // Baseline visibility
                        if(amplitude_change > (previous_amplitudes[i] >> 1)) {
                            target_height += 12; // Stronger bass attacks
                        }
                    } else if(i == 2) {
                        // Bass-mid: Reduce dominance but keep visible
                        target_height += (target_height >> 3); // +12.5% boost (was +25%)
                        target_height += 4; // Baseline for warmth
                    } else if(i == 3 || i == 4) {
                        // Mids: Reduce dominance - these were too active
                        target_height += (target_height >> 4); // +6.25% boost (was +12.5%)
                        if(amplitude_change > 0) {
                            target_height += (amplitude_change >> 8); // Less vocal sensitivity
                        }
                    } else if(i == 5) {
                        // High-mid: Increase visibility
                        target_height += (target_height >> 1); // +50% boost (was +25%)
                        target_height += 3; // Baseline visibility
                    } else if(i == 6) {
                        // Treble: Strong boost for visibility
                        target_height += target_height; // +100% boost (was +50%)
                        target_height += 6; // Baseline for treble presence
                        if(amplitude_change > previous_amplitudes[i]) {
                            target_height += 10; // Stronger treble attacks
                        }
                    } else { // i == 7
                        // High treble: Maximum boost for sparkle
                        target_height += target_height + (target_height >> 1); // +150% boost
                        target_height += 4; // Baseline for sparkle
                        if(amplitude_change > (previous_amplitudes[i] >> 2)) {
                            target_height += 15; // Maximum transient response
                        }
                    }
                    
                    // Fast rhythm emphasis using simple comparison
                    if(amplitude_change > (previous_amplitudes[i] << 1)) { // Fast 2x check
                        target_height += 10; // Boost for sudden attacks
                    }
                }
                
                // Expanded height range for more dramatic visuals
                if(target_height < 8) target_height = 8;
                if(target_height > 120) target_height = 120; // Taller bars for more drama
                
                bar_target_heights[i] = target_height;
                previous_amplitudes[i] = accumulated_amplitude;
                spectrum_accumulators_8ad[i] = 0; // Reset accumulator
            }
            spectrum_sample_count_8ad = 0;
        }
        
        // Enhanced bouncy physics simulation for MAXIMUM liveliness
        for(int i = 0; i < NUM_BARS; i++) {
            int current = bar_current_heights[i];
            int target = bar_target_heights[i];
            
            if(target > current) {
                // Audio got louder - MORE DRAMATIC snap up with overshoot
                int jump = target - current;
                
                // Frequency-specific overshoot for musical realism
                if(jump > 15) {
                    // Different overshoot behavior per frequency range
                    int overshoot;
                    if(i <= 1) {
                        // Bass: moderate overshoot (bass instruments don't spike as much)
                        overshoot = jump >> 3; // 12.5% overshoot
                    } else if(i >= 6) {
                        // Treble: large overshoot (cymbals, attacks spike dramatically)
                        overshoot = jump >> 2; // 25% overshoot
                    } else {
                        // Mids: balanced overshoot
                        overshoot = jump >> 3; // 12.5% overshoot
                    }
                    
                    bar_current_heights[i] = target + overshoot;
                    bar_velocities[i] = -(overshoot >> 1); // Bounce back proportionally
                } else {
                    // Small jumps with frequency-specific behavior
                    if(i <= 1) {
                        // Bass: solid, no overshoot for small changes
                        bar_current_heights[i] = target;
                        bar_velocities[i] = 0;
                    } else if(i >= 6) {
                        // Treble: always slight overshoot for sparkle
                        bar_current_heights[i] = target + 3;
                        bar_velocities[i] = -1;
                    } else {
                        // Mids: slight overshoot
                        bar_current_heights[i] = target + 1;
                        bar_velocities[i] = -1;
                    }
                }
            } else {
                // Audio got quieter - MUSICALLY REALISTIC fall physics
                
                // Each frequency band has unique physical characteristics
                int gravity;
                if(i == 0) {
                    gravity = 1; // Sub-bass: very slow decay (like a kick drum resonance)
                } else if(i == 1) {
                    gravity = 1; // Bass: slow decay (bass instruments sustain)
                } else if(i == 2) {
                    gravity = 2; // Bass-mid: medium-slow decay
                } else if(i == 3 || i == 4) {
                    gravity = 2; // Mids: medium decay (vocal/instrument sustain)
                } else if(i == 5) {
                    gravity = 3; // High-mid: faster decay
                } else if(i == 6) {
                    gravity = 4; // Treble: fast decay (strings, guitars)
                } else { // i == 7
                    gravity = 6; // High treble: very fast decay (cymbals, hi-hats)
                }
                
                bar_velocities[i] += gravity;
                bar_current_heights[i] -= bar_velocities[i];
                
                // Bounce effect when hitting target
                if(bar_current_heights[i] < target) {
                    bar_current_heights[i] = target;
                    
                    // Add subtle bounce for liveliness
                    if(bar_velocities[i] > 3) {
                        bar_velocities[i] = -(bar_velocities[i] / 4); // Bounce back with 25% energy
                    } else {
                        bar_velocities[i] = 0; // Stop small bounces
                    }
                }
                
                // Enhanced minimum clamp with overshoot correction
                if(bar_current_heights[i] < 8) {
                    bar_current_heights[i] = 8;
                    bar_velocities[i] = 0;
                }
                
                // Handle overshoot correction (when bars go too high)
                if(bar_current_heights[i] > 120) {
                    bar_current_heights[i] = 120;
                    bar_velocities[i] = 1; // Small downward velocity to settle
                }
            }
        }
        
        // Render perfectly centered, multi-tile bars
        int sprite_index = 0;
        int screen_center_x = 120; // GBA screen is 240 pixels wide
        int bar_width = 32; // Each sprite is 32px wide - ULTRA WIDE bars!
        int bar_gap = 8; // Smaller gap for 32px bars
        int total_content = (NUM_BARS * bar_width) + ((NUM_BARS - 1) * bar_gap); // 8*32 + 7*8 = 312
        int start_x = screen_center_x - (total_content / 2); // Perfect centering
        
        for(int i = 0; i < NUM_BARS; i++) {
            int bar_height = bar_current_heights[i];
            int x = start_x + (i * (bar_width + bar_gap)); // Perfectly centered bars
            int base_y = 100; // Bottom of bars - positioned above track title bar
            
            // Calculate how many tiles this bar needs (each tile is 8 pixels tall)
            int tiles_needed = (bar_height + 7) / 8; // Round up division
            if(tiles_needed > TILES_PER_BAR) tiles_needed = TILES_PER_BAR;
            
            // Render 32x8 tiles from bottom to top - NO COLOR BLEEDING!
            for(int tile = 0; tile < tiles_needed && sprite_index < 128; tile++) {
                int tile_y = base_y - ((tile + 1) * 8); // Each tile 8 pixels up
                int tile_index = ((i * TILES_PER_BAR) + tile) * 4; // Each 32x8 uses 4 8x8 tiles
                
                OAM[sprite_index].attr0 = ATTR0_NORMAL | ATTR0_COLOR_16 | ATTR0_SQUARE | (tile_y & 0xFF);
                OAM[sprite_index].attr1 = ATTR1_SIZE_32 | (x & 0x01FF); // 32x8 sprites - ULTRA WIDE!
                OAM[sprite_index].attr2 = ATTR2_PALETTE(0) | (512 + tile_index); // Points to first tile
                sprite_index++;
            }
        }
        
        // Disable unused sprites
        while(sprite_index < 128) {
            OAM[sprite_index].attr0 = ATTR0_DISABLED;
            sprite_index++;
        }
        
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

#else
// GSM main function
int main() {
    // Initialize interrupts and video
    irqInit();
    irqEnable(IRQ_VBLANK);
    
    // Use Mode 3 for beautiful bitmap text + sprites
    SetMode(MODE_3 | BG2_ENABLE | OBJ_ENABLE | OBJ_1D_MAP);
    
    // Enhanced spectrum analyzer visualization - 8 multi-tile vertical bars
    #define NUM_BARS 8
    #define TILES_PER_BAR 12  // Each bar can be up to 12 tiles tall (96 pixels) - MASSIVE bars!
    
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
    
    // Create 32x8 tiles for ULTRA WIDE, MASSIVE height bars with no color bleeding
    // Each bar gets completely separate tile memory space
    for(int bar = 0; bar < NUM_BARS; bar++) {
        u32 base_color = bar + 1; // Use different colors for each bar
        
        // Create 32x8 sprite data: each 32x8 sprite uses 4 consecutive 8x8 tiles
        u32 pixel_data = (base_color << 0) | (base_color << 4) | (base_color << 8) | (base_color << 12) |
                        (base_color << 16) | (base_color << 20) | (base_color << 24) | (base_color << 28);
        
        // Each bar gets 12 tile quads (48 tiles total per bar) - no overlap!
        for(int tile_quad = 0; tile_quad < TILES_PER_BAR; tile_quad++) {
            int base_tile = (bar * TILES_PER_BAR + tile_quad) * 4; // Each 32x8 uses 4 8x8 tiles
            
            // Create all 4 tiles for this 32x8 sprite
            for(int subtile = 0; subtile < 4; subtile++) {
                for(int row = 0; row < 8; row++) {
                    spriteGfx[(base_tile + subtile) * 8 + row] = pixel_data;
                }
            }
        }
    }
    
    // Clear OAM
    for(int i = 0; i < 128; i++) {
        OAM[i].attr0 = ATTR0_DISABLED;
        OAM[i].attr1 = 0;
        OAM[i].attr2 = 0;
    }
    
    // Initial OAM setup will be handled dynamically in main loop
    // No need for static sprite initialization since we're using multi-tile bars
    
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
        // ENHANCED spectrum analyzer - bouncy, centered, multi-tile bars!
        
        // Enhanced responsive frequency analysis with dynamic scaling
        static int reset_counter = 0;
        static long previous_amplitudes[NUM_BARS] = {0};
        static int adaptive_scale = 1000; // Dynamic scaling factor
        
        reset_counter++;
        if(reset_counter >= 4) { // Faster updates for better responsiveness
            reset_counter = 0;
            
            // Calculate average amplitude across all bars for auto-scaling
            long total_amplitude = 0;
            for(int i = 0; i < NUM_BARS; i++) {
                total_amplitude += playback.spectrum_accumulators[i];
            }
            
            // Dynamic scaling: when all frequencies are active, increase sensitivity
            if(total_amplitude > 0 && playback.spectrum_sample_count > 0) {
                long avg_total = total_amplitude / NUM_BARS;
                if(avg_total > adaptive_scale * 2) {
                    adaptive_scale = avg_total / 3; // Reduce scale when loud
                } else if(avg_total < adaptive_scale / 3) {
                    adaptive_scale = avg_total * 2; // Increase scale when quiet
                }
                // Clamp scaling factor
                if(adaptive_scale < 500) adaptive_scale = 500;
                if(adaptive_scale > 4000) adaptive_scale = 4000;
            }
            
            for(int i = 0; i < NUM_BARS; i++) {
                long accumulated_amplitude = playback.spectrum_accumulators[i];
                int target_height = 8; // Minimum height
                
                if(playback.spectrum_sample_count > 0 && accumulated_amplitude > 0) {
                    // Use relative amplitude compared to previous frame
                    long amplitude_change = accumulated_amplitude - previous_amplitudes[i];
                    long base_amplitude = accumulated_amplitude / (playback.spectrum_sample_count / 8);
                    
                    // Dynamic response: emphasize changes and relative differences
                    int relative_boost = (amplitude_change > 0) ? (amplitude_change / adaptive_scale) : 0;
                    int base_response = base_amplitude / adaptive_scale;
                    
                    target_height = 8 + base_response + relative_boost;
                    
                    // Add frequency-specific sensitivity
                    if(i == 0) target_height = target_height * 12 / 10; // Boost bass slightly
                    if(i >= 6) target_height = target_height * 14 / 10; // Boost treble more
                }
                
                // Clamp target height - MASSIVE bars for symmetric screen usage!
                if(target_height < 8) target_height = 8;
                if(target_height > 100) target_height = 100; // Symmetric: 20px margins top/bottom
                
                playback.bar_target_heights[i] = target_height;
                previous_amplitudes[i] = accumulated_amplitude;
                playback.spectrum_accumulators[i] = 0; // Reset accumulator
            }
            playback.spectrum_sample_count = 0;
        }
        
        // Bouncy physics simulation for each bar
        for(int i = 0; i < NUM_BARS; i++) {
            int current = playback.bar_current_heights[i];
            int target = playback.bar_target_heights[i];
            
            if(target > current) {
                // Audio got louder - snap up quickly
                playback.bar_current_heights[i] = target;
                playback.bar_velocities[i] = 0;
            } else {
                // Audio got quieter - fall down smoothly with physics
                playback.bar_velocities[i] += 2; // Gravity acceleration
                playback.bar_current_heights[i] -= playback.bar_velocities[i];
                
                // Don't fall below target or minimum
                if(playback.bar_current_heights[i] < target) {
                    playback.bar_current_heights[i] = target;
                    playback.bar_velocities[i] = 0;
                }
                if(playback.bar_current_heights[i] < 8) {
                    playback.bar_current_heights[i] = 8;
                    playback.bar_velocities[i] = 0;
                }
            }
        }
        
        // Render perfectly centered, multi-tile bars
        int sprite_index = 0;
        int screen_center_x = 120; // GBA screen is 240 pixels wide
        int bar_width = 32; // Each sprite is 32px wide - ULTRA WIDE bars!
        int bar_gap = 8; // Smaller gap for 32px bars
        int total_content = (NUM_BARS * bar_width) + ((NUM_BARS - 1) * bar_gap); // 8*32 + 7*8 = 312
        int start_x = screen_center_x - (total_content / 2); // Perfect centering
        
        for(int i = 0; i < NUM_BARS; i++) {
            int bar_height = playback.bar_current_heights[i];
            int x = start_x + (i * (bar_width + bar_gap)); // Perfectly centered bars
            int base_y = 100; // Bottom of bars - positioned above track title bar
            
            // Calculate how many tiles this bar needs (each tile is 8 pixels tall)
            int tiles_needed = (bar_height + 7) / 8; // Round up division
            if(tiles_needed > TILES_PER_BAR) tiles_needed = TILES_PER_BAR;
            
            // Render 32x8 tiles from bottom to top - NO COLOR BLEEDING!
            for(int tile = 0; tile < tiles_needed && sprite_index < 128; tile++) {
                int tile_y = base_y - ((tile + 1) * 8); // Each tile 8 pixels up
                int tile_index = ((i * TILES_PER_BAR) + tile) * 4; // Each 32x8 uses 4 8x8 tiles
                
                OAM[sprite_index].attr0 = ATTR0_NORMAL | ATTR0_COLOR_16 | ATTR0_SQUARE | (tile_y & 0xFF);
                OAM[sprite_index].attr1 = ATTR1_SIZE_32 | (x & 0x01FF); // 32x8 sprites - ULTRA WIDE!
                OAM[sprite_index].attr2 = ATTR2_PALETTE(0) | (512 + tile_index); // Points to first tile
                sprite_index++;
            }
        }
        
        // Disable unused sprites
        while(sprite_index < 128) {
            OAM[sprite_index].attr0 = ATTR0_DISABLED;
            sprite_index++;
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

#endif // CLEAN_8AD