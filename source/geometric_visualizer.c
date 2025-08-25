#include "geometric_visualizer.h"
#include "8ad_player.h"
#include <string.h>

// Forward declarations for helper functions
static int cos_approx(int angle);
static int sin_approx(int angle);

// State variables
static int is_initialized = 0;
static int current_rings = MIN_HEXAGON_RINGS;
static int target_rings = MIN_HEXAGON_RINGS;
static int current_palette = 0; // 0-7 for color cycling
static int color_cycle_counter = 0;
static int active_sprites = 0;

// Audio-reactive parameters
static long total_amplitude = 0;
static long max_amplitude = 0;
static int adaptive_scale = 1000;
static long previous_total = 0;

void init_geometric_visualizer(void) {
    if (is_initialized) return;
    
    // Reset all state variables
    current_rings = MIN_HEXAGON_RINGS;
    target_rings = MIN_HEXAGON_RINGS;
    current_palette = 0;
    color_cycle_counter = 0;
    active_sprites = 0;
    total_amplitude = 0;
    max_amplitude = 0;
    adaptive_scale = 1000;
    previous_total = 0;
    
    // Set up sprite palette for color cycling
    SPRITE_PALETTE[0] = RGB5(0, 0, 0);      // Transparent
    SPRITE_PALETTE[1] = RGB5(0, 15, 31);    // Blue (low intensity)
    SPRITE_PALETTE[2] = RGB5(0, 31, 15);    // Blue-green
    SPRITE_PALETTE[3] = RGB5(0, 31, 0);     // Green (medium intensity)
    SPRITE_PALETTE[4] = RGB5(15, 31, 0);    // Yellow-green
    SPRITE_PALETTE[5] = RGB5(31, 31, 0);    // Yellow (high intensity)
    SPRITE_PALETTE[6] = RGB5(31, 15, 0);    // Orange
    SPRITE_PALETTE[7] = RGB5(31, 0, 0);     // Red (very high intensity)
    
    // Don't create tiles in init - we'll do dynamic overwriting in render like waveform
    // Sprites should work with the existing MODE_3 | BG2_ENABLE | OBJ_ENABLE | OBJ_1D_MAP
    
    is_initialized = 1;
}

void cleanup_geometric_visualizer(void) {
    if (!is_initialized) return;
    
    // Hide all geometric sprites
    for (int i = 0; i < MAX_GEOMETRIC_SPRITES && i < 128; i++) {
        OAM[i].attr0 = ATTR0_DISABLED;
        OAM[i].attr1 = 0;
        OAM[i].attr2 = 0;
    }
    
    active_sprites = 0;
    is_initialized = 0;
}

void update_geometric_visualizer(void) {
    if (!is_initialized) return;
    
    // Move all audio processing to render function like waveform visualizer
    // This ensures we get fresh data and don't conflict with spectrum update timing
}

void render_geometric_hexagon(void) {
    if (!is_initialized) return;
    
    // Calculate audio reactivity every frame like waveform visualizer
    total_amplitude = 0;
    max_amplitude = 0;
    for (int i = 0; i < 8; i++) {
        total_amplitude += spectrum_accumulators_8ad[i];
        if (spectrum_accumulators_8ad[i] > max_amplitude) {
            max_amplitude = spectrum_accumulators_8ad[i];
        }
    }
    
    // Dynamic scaling similar to spectrum visualizer
    if (total_amplitude > 0 && spectrum_sample_count_8ad > 0) {
        long avg_total = total_amplitude / 8;
        
        if (avg_total > adaptive_scale) {
            adaptive_scale = avg_total / 2;
        } else if (avg_total < adaptive_scale / 4) {
            adaptive_scale = avg_total * 3;
        }
        
        if (adaptive_scale < 200) adaptive_scale = 200;
        if (adaptive_scale > 8000) adaptive_scale = 8000;
    }
    
    // ENHANCED DRAMATIC approach: much more sensitive scaling and smoother colors
    if (total_amplitude > 0) {
        // MUCH more dramatic scaling - divide by smaller number for bigger changes
        target_rings = MIN_HEXAGON_RINGS + (total_amplitude / 2000); // 2.5x more sensitive!
        if (target_rings > MAX_HEXAGON_RINGS) target_rings = MAX_HEXAGON_RINGS;
        
        // Smoother color transitions with more levels for musical feel
        if (total_amplitude < 8000) {
            current_palette = 1; // Blue for very quiet
        } else if (total_amplitude < 15000) {
            current_palette = 2; // Blue-green for quiet  
        } else if (total_amplitude < 25000) {
            current_palette = 3; // Green for low-medium
        } else if (total_amplitude < 40000) {
            current_palette = 4; // Yellow-green for medium
        } else if (total_amplitude < 60000) {
            current_palette = 5; // Yellow for loud
        } else if (total_amplitude < 85000) {
            current_palette = 6; // Orange for very loud
        } else {
            current_palette = 7; // Red for maximum intensity
        }
        
        previous_total = total_amplitude;
    } else {
        target_rings = MIN_HEXAGON_RINGS; // Just center dot when no audio
        current_palette = 1; // Blue for quiet
    }
    
    // SMOOTHER ring transitions with intermediate steps for fluid animation
    if (target_rings > current_rings) {
        // Fast growth but with occasional steps for dramatic builds
        static int growth_counter = 0;
        growth_counter++;
        if (growth_counter >= 2 || (target_rings - current_rings) > 2) {
            current_rings++; // Step up one ring at a time for smooth expansion
            growth_counter = 0;
        }
        if (current_rings > target_rings) {
            current_rings = target_rings; // Don't overshoot
        }
    } else if (target_rings < current_rings) {
        // Smoother decay - step down gradually for musical feel
        static int decay_counter = 0;
        decay_counter++;
        if (decay_counter >= 6) { // Slower, more musical decay
            current_rings--;
            decay_counter = 0;
        }
        if (current_rings < target_rings) {
            current_rings = target_rings; // Don't undershoot
        }
    }
    
    // Ensure we always show at least minimum rings
    if (current_rings < MIN_HEXAGON_RINGS) {
        current_rings = MIN_HEXAGON_RINGS;
    }
    
    // Color cycling is now handled above in the amplitude logic
    
    // DYNAMIC TILE OVERWRITING - create tiles every frame like waveform
    u32* spriteGfx = (u32*)(0x6014000);
    int geometric_tile = GEOMETRIC_TILE_START; // 512 - known working tile
    
    // Choose color pattern based on current palette (intensity)
    u32 geometric_pattern;
    if (current_palette <= 1) {
        geometric_pattern = 0x11111111; // Color 1 - blue (low intensity)
    } else if (current_palette <= 3) {
        geometric_pattern = 0x33333333; // Color 3 - green (medium intensity) 
    } else if (current_palette <= 5) {
        geometric_pattern = 0x55555555; // Color 5 - yellow (high intensity)
    } else {
        geometric_pattern = 0x77777777; // Color 7 - red (very high intensity)
    }
    
    // Overwrite spectrum's tile with our pattern EVERY FRAME
    for (int row = 0; row < 8; row++) {
        spriteGfx[geometric_tile * 8 + row] = geometric_pattern;
    }
    
    // Render hexagon using concentric rings of sprites
    active_sprites = 0;
    
    // Force at least one ring to be visible
    int rings_to_render = current_rings;
    if (rings_to_render < 1) rings_to_render = 1;
    
    for (int ring = 0; ring < rings_to_render && active_sprites < MAX_GEOMETRIC_SPRITES; ring++) {
        int radius = 8 + (ring * 12); // Each ring 12 pixels further out
        int sprites_in_ring = (ring == 0) ? 1 : (ring * 6); // Center + 6 sprites per ring
        
        if (ring == 0) {
            // Center sprite
            if (active_sprites < 128) {
                OAM[active_sprites].attr0 = ATTR0_NORMAL | ATTR0_COLOR_16 | ATTR0_SQUARE | ((HEXAGON_CENTER_Y - 4) & 0xFF);
                OAM[active_sprites].attr1 = ATTR1_SIZE_8 | ((HEXAGON_CENTER_X - 4) & 0x1FF);
                OAM[active_sprites].attr2 = ATTR2_PALETTE(0) | geometric_tile;
                active_sprites++;
            }
        } else {
            // Ring sprites arranged in hexagonal pattern
            for (int i = 0; i < sprites_in_ring && active_sprites < MAX_GEOMETRIC_SPRITES; i++) {
                if (active_sprites >= 128) break;
                
                int angle = (i * 360) / sprites_in_ring;
                int x = HEXAGON_CENTER_X + (radius * cos_approx(angle)) / 256 - 4;
                int y = HEXAGON_CENTER_Y + (radius * sin_approx(angle)) / 256 - 4;
                
                // Keep sprites on screen
                if (x < 0) x = 0;
                if (x > 232) x = 232;
                if (y < 0) y = 0;
                if (y > 152) y = 152;
                
                OAM[active_sprites].attr0 = ATTR0_NORMAL | ATTR0_COLOR_16 | ATTR0_SQUARE | (y & 0xFF);
                OAM[active_sprites].attr1 = ATTR1_SIZE_8 | (x & 0x1FF);
                OAM[active_sprites].attr2 = ATTR2_PALETTE(0) | geometric_tile;
                active_sprites++;
            }
        }
    }
    
    // Hide unused sprites
    for (int i = active_sprites; i < MAX_GEOMETRIC_SPRITES; i++) {
        if (i >= 128) break;
        OAM[i].attr0 = ATTR0_DISABLED;
    }
}

// Simple trigonometric approximations for integer math (256 = 1.0)
static int cos_approx(int angle) {
    angle = angle % 360;
    if (angle < 0) angle += 360;
    
    if (angle <= 90) return 256 - (angle * 256 / 90);
    else if (angle <= 180) return -((angle - 90) * 256 / 90);
    else if (angle <= 270) return -256 + ((angle - 180) * 256 / 90);
    else return (angle - 270) * 256 / 90;
}

static int sin_approx(int angle) {
    return cos_approx(angle - 90);  // sin(x) = cos(x - 90°)
}

// Note: setup_hexagon_sprites() removed - we use dynamic tile overwriting in render function