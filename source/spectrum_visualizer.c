#include "spectrum_visualizer.h"
#include "8ad_player.h"

// Spectrum visualizer state
static int reset_counter = 0;
static int bar_current_heights[NUM_BARS] = {8,8,8,8,8,8,8,8};
static int bar_target_heights[NUM_BARS] = {8,8,8,8,8,8,8,8};
static int bar_velocities[NUM_BARS] = {0,0,0,0,0,0,0,0};
static long previous_amplitudes[NUM_BARS] = {0};
static int adaptive_scale = 1000; // Dynamic scaling factor

void init_spectrum_visualizer(void) {
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
}