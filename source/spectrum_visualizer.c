#include "spectrum_visualizer.h"
#include "8ad_player.h"
#include "visualization_manager.h"
#include "album_cover.h"

// Spectrum visualizer state
static int reset_counter = 0;
static int bar_current_heights[NUM_BARS] = {8,8,8,8,8,8,8};
static int bar_target_heights[NUM_BARS] = {8,8,8,8,8,8,8};
static int bar_velocities[NUM_BARS] = {0,0,0,0,0,0,0};
static long previous_amplitudes[NUM_BARS] = {0};
static int adaptive_scale = 1000; // Dynamic scaling factor
static int last_track = -1; // Track last known track for palette switching
static bool is_initialized = false;
// Removed height tracking - always update sprites for consistent rendering

// Update spectrum palette based on current track
static void update_spectrum_palette(void) {
    if (is_final_track_8ad()) {
        // "The Fourth Color" - use full rainbow spectrum
        u16 rainbow_colors[7] = {
            RGB5(31, 0, 0),   // Bar 0: Red
            RGB5(31, 15, 0),  // Bar 1: Orange  
            RGB5(31, 31, 0),  // Bar 2: Yellow
            RGB5(0, 31, 0),   // Bar 3: Green
            RGB5(0, 0, 31),   // Bar 4: Blue
            RGB5(15, 0, 31),  // Bar 5: Indigo
            RGB5(31, 0, 31)   // Bar 6: Violet
        };
        
        for(int pal = 0; pal < 7; pal++) {
            SPRITE_PALETTE[pal * 16 + 0] = RGB5(0, 0, 0);  // Transparent
            SPRITE_PALETTE[pal * 16 + 1] = rainbow_colors[pal];
        }
        
        // CRITICAL: Bar[1] needs individual handling even in rainbow mode
        SPRITE_PALETTE[1 * 16 + 1] = RGB5(31, 15, 0); // Force orange for bar[1] in rainbow mode
    } else {
        // All other tracks - Game Boy green
        for(int pal = 0; pal < 7; pal++) {
            SPRITE_PALETTE[pal * 16 + 0] = RGB5(0, 0, 0);  // Transparent
            SPRITE_PALETTE[pal * 16 + 1] = RGB5(6, 12, 6); // Game Boy dark green for all bars
        }
        
        // CRITICAL: Bar[1] needs individual handling - force Game Boy green
        SPRITE_PALETTE[1 * 16 + 1] = RGB5(6, 12, 6); // Game Boy dark green specifically for bar[1]
    }
}

// Update background and text colors based on current track
static void update_background_colors(void) {
    if (is_final_track_8ad()) {
        // "The Fourth Color" - white background, black text
        BG_PALETTE[0] = RGB5(31, 31, 31);  // White background (using index 255)
        BG_PALETTE[17] = RGB5(0, 0, 0);    // Black text
    } else {
        // All other tracks - Game Boy colors
        BG_PALETTE[0] = RGB5(19, 23, 1);   // Game Boy bright green background (using index 255)
        BG_PALETTE[17] = RGB5(1, 7, 1);    // Game Boy darkest green text
    }
}

void init_spectrum_visualizer(void) {
    if (is_initialized) return;
    
    // MODE_1: No mode switching needed - everything uses MODE_1 now
    // SetMode already handled by main.c or visualization_manager
    
    // Initialize palette - will be updated based on current track
    update_spectrum_palette();
    
    // MODE_1: Standard sprite memory at 0x6010000, tiles 100+ (after font tiles 0-95)
    u32* spriteGfx = (u32*)0x6010000; // Standard sprite tile memory for MODE_1
    
    // Create 7 pairs of 8x8 tiles for 16x8 spectrum bars (tiles 100-113)
    for(int bar = 0; bar < 7; bar++) {
        int left_tile = 100 + (bar * 2);     // Left half: 100, 102, 104, 106, 108, 110, 112, 114
        int right_tile = 100 + (bar * 2) + 1; // Right half: 101, 103, 105, 107, 109, 111, 113, 115
        u32 pixel_data = 0x11111111; // All pixels use palette color 1
        
        // Create left 8x8 tile
        for(int row = 0; row < 8; row++) {
            spriteGfx[(left_tile * 8) + row] = pixel_data;
        }
        
        // Create right 8x8 tile (identical to left for solid bars)
        for(int row = 0; row < 8; row++) {
            spriteGfx[(right_tile * 8) + row] = pixel_data;
        }
    }
    
    // Clear all OAM entries (spectrum uses many sprites so it can clear everything)
    for(int i = 0; i < 128; i++) {
        OAM[i].attr0 = ATTR0_DISABLED;
        OAM[i].attr1 = 0;
        OAM[i].attr2 = 0;
    }
    
    is_initialized = true;
}

// Reset spectrum visualizer state during track changes
void reset_spectrum_visualizer_state(void) {
    if (!is_initialized) return;
    
    // Reset all bars to baseline immediately
    for(int i = 0; i < NUM_BARS; i++) {
        bar_current_heights[i] = 8;
        bar_target_heights[i] = 8;
        bar_velocities[i] = 0;
        previous_amplitudes[i] = 0; // CRITICAL: Clear residual amplitude memory
    }
    
    // Reset adaptive scaling
    adaptive_scale = 1000;
    reset_counter = 0;
}

void cleanup_spectrum_visualizer(void) {
    if (!is_initialized) return;
    
    // Clear all OAM entries used by spectrum visualizer
    for(int i = 0; i < 128; i++) {
        OAM[i].attr0 = ATTR0_DISABLED;
        OAM[i].attr1 = 0;
        OAM[i].attr2 = 0;
    }
    
    // Reset state variables
    reset_counter = 0;
    for(int i = 0; i < NUM_BARS; i++) {
        bar_current_heights[i] = 8;
        bar_target_heights[i] = 8;
        bar_velocities[i] = 0;
        previous_amplitudes[i] = 0;
    }
    adaptive_scale = 1000;
    
    // MODE_1: No framebuffer clearing needed - background handled by BG layers
    
    is_initialized = false;
}

void update_spectrum_visualizer(void) {
    // Check if track has changed and update palette accordingly
    int current_track = get_current_track_8ad();
    if (current_track != last_track) {
        update_spectrum_palette();
        update_background_colors(); // Update background and text colors too
        update_album_cover_colors(); // Update album cover colors too
        last_track = current_track;
    }
    
    // CRITICAL: Force bar[1] color every frame to prevent brown override
    if (is_final_track_8ad()) {
        SPRITE_PALETTE[1 * 16 + 1] = RGB5(31, 15, 0); // Orange for rainbow mode
    } else {
        SPRITE_PALETTE[1 * 16 + 1] = RGB5(6, 12, 6);  // Game Boy green for retro mode
    }
    
    reset_counter++;
    if(reset_counter >= 3) { // Balanced: update every 3 frames (20Hz) - compromise between reactivity and CPU load
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
            
            // More reactive scaling range for better music response
            if(adaptive_scale < 100) adaptive_scale = 100;   // Very sensitive to quiet parts  
            if(adaptive_scale > 4000) adaptive_scale = 4000; // Handle loud parts without over-dampening
            
            // DEBUG: Store scaling values for analysis (using safe slots 240+)
            SPRITE_PALETTE[240] = (adaptive_scale & 0xFFFF); // Current adaptive scale
            SPRITE_PALETTE[241] = (avg_total & 0xFFFF);      // Average amplitude
        }
        
        // DEBUG: Store sample analysis data (using safe slots 242+)
        SPRITE_PALETTE[242] = (spectrum_sample_count_8ad & 0xFFFF);        // Sample count
        SPRITE_PALETTE[243] = (spectrum_accumulators_8ad[1] & 0xFFFF);     // Bass bar raw data
        SPRITE_PALETTE[244] = (spectrum_accumulators_8ad[4] & 0xFFFF);     // Mid bar raw data
        SPRITE_PALETTE[245] = (spectrum_accumulators_8ad[6] & 0xFFFF);     // Treble bar raw data (Bar[6], not [7])
        
        // DEBUG: Compare Bar[0] and Bar[6] processing
        SPRITE_PALETTE[246] = (spectrum_accumulators_8ad[0] & 0xFFFF);     // Bar[0] raw data for comparison
        SPRITE_PALETTE[247] = bar_current_heights[0];                      // Bar[0] current height
        SPRITE_PALETTE[248] = bar_current_heights[6];                      // Bar[6] current height
        
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
                    // Sub-bass: Reduced for spectrum curve effect (less active edges)
                    target_height += (target_height >> 2); // +25% boost (reduced from +100%)
                    target_height += 3; // Lower baseline to de-emphasize
                } else if(i == 1) {
                    // Bass: Very strong response
                    target_height += (target_height >> 1) + (target_height >> 2); // +75% boost
                    target_height += 5; // Baseline visibility
                    if(amplitude_change > (previous_amplitudes[i] >> 1)) {
                        target_height += 12; // Stronger bass attacks
                    }
                } else if(i == 2) {
                    // Bass-mid: Enhanced for symmetrical spectrum response (match Bar[5])
                    target_height += target_height; // +100% boost (matching Bar[5])
                    target_height += 5; // Higher baseline for presence
                    if(amplitude_change > (previous_amplitudes[i] >> 2)) { // Sensitive bass transient detection
                        target_height += 10; // Strong response to bass attacks
                    }
                } else if(i == 3 || i == 4) {
                    // Mids: Reduce dominance - these were too active
                    target_height += (target_height >> 4); // +6.25% boost (was +12.5%)
                    if(amplitude_change > 0) {
                        target_height += (amplitude_change >> 8); // Less vocal sensitivity
                    }
                } else if(i == 5) {
                    // High-mid: MAXIMUM sensitivity for cymbals and high-freq transients
                    target_height += target_height; // +100% boost (doubled from +50%)
                    target_height += 5; // Higher baseline for presence
                    if(amplitude_change > (previous_amplitudes[i] >> 3)) { // Very sensitive threshold
                        target_height += 12; // Strong transient response for hi-hats
                    }
                } else if(i == 6) {
                    // High treble: IDENTICAL to Bar[0] processing for exact same animation
                    target_height += (target_height >> 2); // +25% boost (same as Bar[0])
                    target_height += 3; // Same baseline as Bar[0]
                    // No special transient response (like Bar[0])
                }
                
                // Fast rhythm emphasis using simple comparison (exclude Bar[6] for gentleness)
                if(i != 6 && amplitude_change > (previous_amplitudes[i] << 1)) { // Fast 2x check
                    target_height += 10; // Boost for sudden attacks (Bar[6] excluded for smooth behavior)
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
        
        // PHASE 1: Calculate lateral energy spreading (cohesive animation)
        // Use two-phase approach to avoid feedback loops
        int spreads[NUM_BARS] = {0,0,0,0,0,0,0}; // Initialize all to 0
        
        for(int i = 1; i < NUM_BARS-1; i++) { // Skip edges (bars 0 and 6)
            // Each bar gets influenced by its immediate neighbors
            int left_influence = (bar_target_heights[i-1] - 8) / 8;  // 12.5% of neighbor's height above baseline
            int right_influence = (bar_target_heights[i+1] - 8) / 8; // 12.5% of neighbor's height above baseline
            
            // Only positive influence (bars don't reduce each other)
            if(left_influence > 0) spreads[i] += left_influence;
            if(right_influence > 0) spreads[i] += right_influence;
            
            // Cap the spreading influence to prevent runaway
            if(spreads[i] > 15) spreads[i] = 15; // Max 15 pixels of spreading
        }
        
        // PHASE 2: Apply all spreads simultaneously (no feedback loops)
        for(int i = 0; i < NUM_BARS; i++) {
            bar_target_heights[i] += spreads[i];
            
            // Re-clamp after spreading
            if(bar_target_heights[i] < 8) bar_target_heights[i] = 8;
            if(bar_target_heights[i] > 120) bar_target_heights[i] = 120;
        }
    }
    
    // Enhanced bouncy physics simulation for MAXIMUM liveliness
    for(int i = 0; i < NUM_BARS; i++) {
        int current = bar_current_heights[i];
        int target = bar_target_heights[i];
        
        // SMOOTH ANIMATION: Eliminate overshoot and bouncing to prevent tearing
        int diff = target - current;
        
        if(diff > 0) {
            // Audio got louder - smooth upward movement
            if(diff >= 8) {
                // Large jumps: move quickly but smoothly
                bar_current_heights[i] = current + (diff >> 1) + 2; // Move halfway + 2px per frame
            } else {
                // Small jumps: direct movement
                bar_current_heights[i] = target;
            }
            bar_velocities[i] = 0; // No bouncing velocities
        } else if(diff < 0) {
            // Audio got quieter - smooth decay based on frequency
            int decay_rate;
            if(i <= 1) {
                decay_rate = 1; // Bass: slow decay
            } else if(i == 6) {
                decay_rate = 1; // Bar[6]: IDENTICAL decay to Bar[0] (gentle)
            } else if(i == 5) {
                decay_rate = 4; // Bar[5]: AGGRESSIVE decay
            } else {
                decay_rate = 2; // Mids: medium decay
            }
            
            bar_current_heights[i] = current - decay_rate;
            if(bar_current_heights[i] < target) {
                bar_current_heights[i] = target; // Don't overshoot downward
            }
            bar_velocities[i] = 0; // No bouncing
        }
        
        // Simple bounds checking
        if(bar_current_heights[i] < 8) bar_current_heights[i] = 8;
        if(bar_current_heights[i] > 120) bar_current_heights[i] = 120;
        
        // AGGRESSIVE reset for Bar[5] only (Bar[6] now uses gentle behavior like Bar[0])
        if(i == 5 && bar_current_heights[i] > bar_target_heights[i] + 20) {
            bar_current_heights[i] = bar_target_heights[i] + 10; // Jump closer to target immediately
        }
    }
}

void render_spectrum_bars(void) {
    // Create spectrum bar sprites directly in OAM (same approach as waveform)
    // Only create sprites when we're actually in spectrum mode
    if (get_current_visualization() != VIZ_SPECTRUM_BARS) {
        return;
    }
    
    // DEBUG: Store render call count in palette
    static int render_calls = 0;
    render_calls++;
    SPRITE_PALETTE[29] = render_calls & 0xFFFF; // Track how many times this function is called
    
    // MODE_1: Use pre-created tiles from init function (tiles 100-107)
    // No need to recreate tiles every frame - they're persistent in sprite memory
    int base_tile = 100; // Our tiles start at 100
    
    // DEBUG: Check if sprite tile data exists at MODE_1 address
    u32* spriteGfx = (u32*)0x6010000; // MODE_1 sprite memory
    SPRITE_PALETTE[30] = (spriteGfx[base_tile * 8] != 0) ? 1 : 0; // Check tile 100
    
    // ALWAYS update sprites for consistent rendering - prevents tearing from partial updates
    
    // Clear only spectrum visualizer's OAM entries (0-95) when we need updates
    // Leave geometric visualizer's sprites (96-127) alone
    for(int i = 0; i < 96; i++) {
        OAM[i].attr0 = ATTR0_DISABLED;
        OAM[i].attr1 = 0;
        OAM[i].attr2 = 0;
    }
    
    // Create 7 spectrum bars using stacked 8x8 sprites
    int sprite_count = 0;
    
    // DEBUG: Store bar heights and creation info
    SPRITE_PALETTE[25] = bar_current_heights[0]; // First bar height
    SPRITE_PALETTE[26] = bar_current_heights[1]; // Second bar height  
    SPRITE_PALETTE[27] = bar_current_heights[6]; // Last bar height (7-bar system)
    
    // DEBUG: Store positioning info for first few bars
    SPRITE_PALETTE[20] = 20 + (0 * 25); // Bar 0 X position (should be 20)
    SPRITE_PALETTE[21] = 20 + (1 * 25); // Bar 1 X position (should be 45)
    SPRITE_PALETTE[22] = 20 + (2 * 25); // Bar 2 X position (should be 70)
    
    for (int bar = 0; bar < NUM_BARS; bar++) {
        // Use actual animated bar heights from physics system
        int bar_height = bar_current_heights[bar];
        
        int bar_x = 16 + (bar * 32); // Shifted right 8px for better centering: 16px start + 32px gaps = 208px span, centered on 240px screen
        int num_sprites = (bar_height + 7) / 8; // Convert height to number of 8x8 sprites needed
        
        // Variable height limits based on bar position - strategic sprite budget allocation
        int max_sprites_for_bar;
        if (bar == 0 || bar == 6) {
            max_sprites_for_bar = 4; // Edge bars (Bar[0], Bar[6]): conservative 4 levels (32px)
        } else if (bar == 1 || bar == 5) {
            max_sprites_for_bar = 6; // Near-edge bars: moderate 6 levels (48px)
        } else if (bar == 3) {
            max_sprites_for_bar = 10; // Center bar (Bar[3]): maximum height 10 levels (80px)
        } else {
            max_sprites_for_bar = 8; // Side-middle bars (Bar[2], Bar[4]): tall 8 levels (64px)  
        }
        // Total budget: (4+6+8+10+8+6+4) × 2 sprites = 92 sprites (safe within 96 limit)
        
        if (num_sprites > max_sprites_for_bar) num_sprites = max_sprites_for_bar;
        if (num_sprites < 1) num_sprites = 1;    // Always show at least one sprite per bar
        
        // Stack sprites from bottom to top - calculate exact bottom position first
        int bottom_y = 110; // Bottom edge of the spectrum area
        
        for (int sprite = 0; sprite < num_sprites; sprite++) {
            if (sprite_count >= 94) break; // Limit to 94 sprites (leave room for 96 total)
            
            // Stack each 8x8 sprite directly on top of the previous one
            int sprite_y = bottom_y - (sprite * 8) - 8; // -8 because sprite Y is top-left corner
            int bar_palette = bar % 7; // Use palettes 0-6 for bars 0-6
            int left_tile = base_tile + (bar * 2);     // Left tile: 100, 102, 104, etc.  
            int right_tile = base_tile + (bar * 2) + 1; // Right tile: 101, 103, 105, etc.
            
            // Create left 8x8 sprite
            OAM[sprite_count].attr0 = ATTR0_NORMAL | ATTR0_COLOR_16 | ATTR0_SQUARE | (sprite_y & 0xFF);
            OAM[sprite_count].attr1 = ATTR1_SIZE_8 | (bar_x & 0x01FF);
            OAM[sprite_count].attr2 = ATTR2_PALETTE(bar_palette) | left_tile;
            sprite_count++;
            
            // Create right 8x8 sprite
            OAM[sprite_count].attr0 = ATTR0_NORMAL | ATTR0_COLOR_16 | ATTR0_SQUARE | (sprite_y & 0xFF);
            OAM[sprite_count].attr1 = ATTR1_SIZE_8 | ((bar_x + 8) & 0x01FF); // 8px to the right
            OAM[sprite_count].attr2 = ATTR2_PALETTE(bar_palette) | right_tile;
            sprite_count++;
            
            // DEBUG: Store first sprite pair's position for each bar
            if (sprite == 0) { // First sprite pair of each bar
                if (bar == 0) { SPRITE_PALETTE[240] = bar_x; SPRITE_PALETTE[241] = sprite_y; }
                if (bar == 1) { SPRITE_PALETTE[242] = bar_x; SPRITE_PALETTE[243] = sprite_y; }
            }
        }
    }
    
    // Sprites always updated - no tracking needed
    
    // DEBUG: Store total sprite count and bar count in palette
    SPRITE_PALETTE[28] = sprite_count;
    SPRITE_PALETTE[24] = NUM_BARS; // Should be 7
    
    // No override needed - use the Game Boy green set during initialization
}