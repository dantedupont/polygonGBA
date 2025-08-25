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
    
    // STAY IN MODE 3 - but implement smart rendering to avoid flicker  
    // Mode 3 is already set by main.c, no need to change
    
    is_initialized = 1;
}

void cleanup_geometric_visualizer(void) {
    if (!is_initialized) return;
    
    // Clear hexagon area only
    u16* framebuffer = (u16*)0x6000000;
    for (int y = 35; y < 125; y++) { // Clear hexagon area 
        for (int x = 75; x < 165; x++) {
            if (y < 130) { // Avoid text area
                framebuffer[y * 240 + x] = RGB5(0, 0, 0);
            }
        }
    }
    
    is_initialized = 0;
}

void update_geometric_visualizer(void) {
    if (!is_initialized) return;
    
    // All audio processing moved to render function to avoid timing conflicts
    // The spectrum visualizer processes data after this update, so we read fresh data in render
}

void render_geometric_hexagon(void) {
    if (!is_initialized) return;
    
    // ULTRA-MINIMAL APPROACH: Use sprites like waveform visualizer (known to work)
    
    // Calculate audio reactivity for scaling
    total_amplitude = 0;
    for (int i = 0; i < 8; i++) {
        total_amplitude += spectrum_accumulators_8ad[i];
    }
    
    // Dynamic tile overwriting - exactly like waveform visualizer
    u32* spriteGfx = (u32*)(0x6014000);
    int geometric_tile = 512; // Same tile as waveform uses
    
    // Audio-reactive color pattern
    u32 color_pattern = 0x77777777; // Default red
    if (spectrum_sample_count_8ad > 0) {
        long avg_amplitude = total_amplitude / 8;
        long intensity = avg_amplitude / (spectrum_sample_count_8ad * 10);
        if (intensity > 7) intensity = 7;
        if (intensity < 1) intensity = 1;
        
        // Simple color cycling based on intensity
        if (intensity <= 2) color_pattern = 0x11111111; // Blue
        else if (intensity <= 4) color_pattern = 0x33333333; // Green  
        else if (intensity <= 6) color_pattern = 0x55555555; // Yellow
        else color_pattern = 0x77777777; // Red
    }
    
    // Overwrite spectrum's tile (only when in geometric mode)
    for (int row = 0; row < 8; row++) {
        spriteGfx[geometric_tile * 8 + row] = color_pattern;
    }
    
    // MINIMAL sprite hexagon - just 7 sprites (center + 6 vertices)
    int active_sprites = 0;
    int center_x = 120, center_y = 80;
    
    // Calculate radius based on audio
    int radius = 20; // Base size
    if (spectrum_sample_count_8ad > 0) {
        long avg_amplitude = total_amplitude / 8;
        radius = 15 + (avg_amplitude / (spectrum_sample_count_8ad * 20));
        if (radius > 35) radius = 35;
        if (radius < 10) radius = 10;
    }
    
    // Center sprite
    if (active_sprites < 128) {
        int x = center_x - 4, y = center_y - 4;
        if (x >= 0 && x <= 232 && y >= 0 && y <= 152) {
            OAM[active_sprites].attr0 = ATTR0_NORMAL | ATTR0_COLOR_16 | ATTR0_SQUARE | (y & 0xFF);
            OAM[active_sprites].attr1 = ATTR1_SIZE_8 | (x & 0x1FF);
            OAM[active_sprites].attr2 = ATTR2_PALETTE(0) | geometric_tile;
            active_sprites++;
        }
    }
    
    // 6 hexagon vertices - minimal CPU usage
    for (int i = 0; i < 6 && active_sprites < 128; i++) {
        int angle = i * 60;
        int x = center_x + (radius * cos_approx(angle)) / 256 - 4;
        int y = center_y + (radius * sin_approx(angle)) / 256 - 4;
        
        if (x >= 0 && x <= 232 && y >= 0 && y <= 152) {
            OAM[active_sprites].attr0 = ATTR0_NORMAL | ATTR0_COLOR_16 | ATTR0_SQUARE | (y & 0xFF);
            OAM[active_sprites].attr1 = ATTR1_SIZE_8 | (x & 0x1FF);
            OAM[active_sprites].attr2 = ATTR2_PALETTE(0) | geometric_tile;
            active_sprites++;
        }
    }
    
    // Disable unused sprites (minimal loop)
    for (int i = active_sprites; i < 20; i++) { // Only clear first 20 slots
        if (i < 128) OAM[i].attr0 = ATTR0_DISABLED;
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