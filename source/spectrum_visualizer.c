#include "spectrum_visualizer.h"
#include "8ad_player.h"
#include "visualization_manager.h"

// Spectrum visualizer state
static int reset_counter = 0;
static int bar_current_heights[NUM_BARS] = {8,8,8,8,8,8,8,8};
static int bar_target_heights[NUM_BARS] = {8,8,8,8,8,8,8,8};
static int bar_velocities[NUM_BARS] = {0,0,0,0,0,0,0,0};
static long previous_amplitudes[NUM_BARS] = {0};
static int adaptive_scale = 1000; // Dynamic scaling factor
static bool is_initialized = false;

void init_spectrum_visualizer(void) {
    if (is_initialized) return;
    
    // MODE_1: No mode switching needed - everything uses MODE_1 now
    // SetMode already handled by main.c or visualization_manager
    
    // Set up 8 different sprite palettes for colorful frequency bars
    // Each palette has 16 colors, we use color 1 for the bar color
    for(int pal = 0; pal < 8; pal++) {
        SPRITE_PALETTE[pal * 16 + 0] = RGB5(0, 0, 0);  // Transparent
        
        // Assign different colors per frequency band
        if (pal == 0) SPRITE_PALETTE[pal * 16 + 1] = RGB5(0, 15, 31);    // Blue (bass)
        else if (pal == 1) SPRITE_PALETTE[pal * 16 + 1] = RGB5(0, 20, 25);    // Blue-cyan
        else if (pal == 2) SPRITE_PALETTE[pal * 16 + 1] = RGB5(0, 31, 15);    // Green-blue 
        else if (pal == 3) SPRITE_PALETTE[pal * 16 + 1] = RGB5(0, 31, 0);     // Green (mids)
        else if (pal == 4) SPRITE_PALETTE[pal * 16 + 1] = RGB5(15, 31, 0);    // Yellow-green
        else if (pal == 5) SPRITE_PALETTE[pal * 16 + 1] = RGB5(31, 31, 0);    // Yellow (highs)
        else if (pal == 6) SPRITE_PALETTE[pal * 16 + 1] = RGB5(31, 15, 0);    // Orange
        else if (pal == 7) SPRITE_PALETTE[pal * 16 + 1] = RGB5(31, 0, 0);     // Red (treble)
    }
    
    // MODE_1: Standard sprite memory at 0x6010000, tiles 100+ (after font tiles 0-95)
    u32* spriteGfx = (u32*)0x6010000; // Standard sprite tile memory for MODE_1
    
    // Create 8 solid color tiles for spectrum bars (tiles 100-107)
    for(int tile = 0; tile < 8; tile++) {
        int tile_index = 100 + tile; // Start at tile 100 to avoid font conflicts
        u32 pixel_data = 0x11111111; // All pixels use palette color 1
        
        // Each 8x8 tile needs 8 rows of data (32 bytes = 8 u32s)
        for(int row = 0; row < 8; row++) {
            spriteGfx[(tile_index * 8) + row] = pixel_data;
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
    
    // Clear all OAM entries first to ensure clean slate
    for(int i = 0; i < 128; i++) {
        OAM[i].attr0 = ATTR0_DISABLED;
        OAM[i].attr1 = 0;
        OAM[i].attr2 = 0;
    }
    
    // Create 8 spectrum bars using stacked 8x8 sprites
    int sprite_count = 0;
    
    // DEBUG: Store bar heights and creation info
    SPRITE_PALETTE[25] = bar_current_heights[0]; // First bar height
    SPRITE_PALETTE[26] = bar_current_heights[1]; // Second bar height  
    SPRITE_PALETTE[27] = bar_current_heights[7]; // Last bar height
    
    // DEBUG: Store positioning info for first few bars
    SPRITE_PALETTE[20] = 20 + (0 * 25); // Bar 0 X position (should be 20)
    SPRITE_PALETTE[21] = 20 + (1 * 25); // Bar 1 X position (should be 45)
    SPRITE_PALETTE[22] = 20 + (2 * 25); // Bar 2 X position (should be 70)
    
    for (int bar = 0; bar < NUM_BARS; bar++) {
        // Use actual animated bar heights from physics system
        int bar_height = bar_current_heights[bar];
        
        int bar_x = 20 + (bar * 25); // Space bars evenly across screen (8 bars * 25 pixels = 200, centered)
        int num_sprites = (bar_height + 7) / 8; // Convert height to number of 8x8 sprites needed
        
        // Limit to prevent sprite overflow
        if (num_sprites > 15) num_sprites = 15;
        
        // Stack sprites from bottom to top
        for (int sprite = 0; sprite < num_sprites; sprite++) {
            if (sprite_count >= 128) break; // Prevent OAM overflow
            
            int sprite_y = 140 - (sprite * 8); // Start from bottom (Y=140) and go up
            
            OAM[sprite_count].attr0 = ATTR0_NORMAL | ATTR0_COLOR_16 | ATTR0_SQUARE | (sprite_y & 0xFF);
            OAM[sprite_count].attr1 = ATTR1_SIZE_8 | (bar_x & 0x01FF);
            // MODE_1: Each bar uses its own tile (100+bar) with its own palette
            int bar_palette = bar % 8; // Use palettes 0-7 for bars 0-7  
            int bar_tile = base_tile + bar; // Tiles 100-107 for bars 0-7
            OAM[sprite_count].attr2 = ATTR2_PALETTE(bar_palette) | bar_tile;
            
            // DEBUG: Store first sprite's position for each bar
            if (sprite == 0) { // First sprite of each bar
                if (bar == 0) { SPRITE_PALETTE[16] = bar_x; SPRITE_PALETTE[17] = sprite_y; }
                if (bar == 1) { SPRITE_PALETTE[18] = bar_x; SPRITE_PALETTE[19] = sprite_y; }
            }
            
            sprite_count++;
        }
    }
    
    // DEBUG: Store total sprite count and bar count in palette
    SPRITE_PALETTE[28] = sprite_count;
    SPRITE_PALETTE[24] = NUM_BARS; // Should be 8
}