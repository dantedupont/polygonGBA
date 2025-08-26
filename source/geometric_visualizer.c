#include "geometric_visualizer.h"
#include "8ad_player.h"
#include "visualization_manager.h"

// Geometric visualizer state
static int rotation_angle = 0;
static bool is_initialized = 0;

// Animation state
static int pulse_counter = 0;
static int color_cycle = 0;

void init_geometric_visualizer(void) {
    if (is_initialized) return;
    
    // MODE_1: No mode switching needed - everything uses unified MODE_1
    // Font system already initialized by main.c
    
    // MODE_1: BG layers already configured by main.c
    // BG1: Text layer, BG2: Background layer
    // No need to reconfigure - just use the existing setup
    
    // MODE_1: No memory clearing needed - handled by unified architecture
    
    // Set up BG palette for text rendering
    BG_PALETTE[0] = RGB5(0, 0, 0);      // Transparent/black background  
    BG_PALETTE[1] = RGB5(31, 31, 0);    // Yellow text color
    BG_PALETTE[2] = RGB5(31, 0, 31);    // Magenta for test tiles
    BG_PALETTE[3] = RGB5(0, 0, 15);     // Blue for text area background
    
    // Enhanced sprite palette for animated hexagon (use colors 9+ to avoid spectrum's 0-8)
    SPRITE_PALETTE[0] = RGB5(0, 0, 0);      // Transparent (always 0)
    SPRITE_PALETTE[9] = RGB5(31, 31, 0);    // Bright yellow for geometric mode
    SPRITE_PALETTE[10] = RGB5(31, 15, 0);   // Orange  
    SPRITE_PALETTE[11] = RGB5(31, 0, 0);    // Bright red
    
    // Palette 1: Blue/Cyan theme  
    SPRITE_PALETTE[16] = RGB5(0, 0, 0);     // Transparent
    SPRITE_PALETTE[17] = RGB5(0, 0, 31);    // Bright blue
    SPRITE_PALETTE[18] = RGB5(0, 15, 31);   // Light blue
    SPRITE_PALETTE[19] = RGB5(0, 31, 31);   // Cyan
    
    // Palette 2: Green theme
    SPRITE_PALETTE[32] = RGB5(0, 0, 0);     // Transparent  
    SPRITE_PALETTE[33] = RGB5(0, 31, 0);    // Bright green
    SPRITE_PALETTE[34] = RGB5(15, 31, 15);  // Light green
    SPRITE_PALETTE[35] = RGB5(31, 31, 15);  // Yellow-green
    
    // Palette 3: Purple/Magenta theme
    SPRITE_PALETTE[48] = RGB5(0, 0, 0);     // Transparent
    SPRITE_PALETTE[49] = RGB5(31, 0, 31);   // Magenta
    SPRITE_PALETTE[50] = RGB5(20, 0, 31);   // Purple
    SPRITE_PALETTE[51] = RGB5(31, 15, 31);  // Pink
    
    // DEBUG: Store information in sprite palette slots for debugging
    SPRITE_PALETTE[27] = 0x4747; // 'GG' for Geometric mode active
    SPRITE_PALETTE[26] = 120; // Geometric uses tiles starting at 120 (was 600+)
    
    // Geometric tile creation moved to render function to avoid unused variables
    // This ensures tiles are created only when actually needed
    
    // Clear all sprites initially - the hexagon will be set up in render function
    for (int i = 0; i < 128; i++) {
        OAM[i].attr0 = ATTR0_DISABLED;
        OAM[i].attr1 = 0;
        OAM[i].attr2 = 0;
    }
    
    is_initialized = 1;
}

void cleanup_geometric_visualizer(void) {
    if (!is_initialized) return;
    
    // MODE_1: No mode switching needed - everything stays in unified MODE_1
    
    // Clear all OAM entries to ensure clean transition
    for (int i = 0; i < 128; i++) {
        OAM[i].attr0 = ATTR0_DISABLED;
        OAM[i].attr1 = 0;
        OAM[i].attr2 = 0;
    }
    
    // Reset geometric state
    rotation_angle = 0;
    pulse_counter = 0;
    color_cycle = 0;
    
    // Restore spectrum/waveform palette entries that geometric might have modified
    // Make sure we don't interfere with spectrum palettes 0-7 or waveform palette 8
    SPRITE_PALETTE[0] = RGB5(0, 0, 0);      // Transparent
    SPRITE_PALETTE[1] = RGB5(31, 31, 31);   // BRIGHT WHITE (waveform needs this)
    SPRITE_PALETTE[2] = RGB5(31, 0, 31);    // BRIGHT MAGENTA (waveform needs this)
    SPRITE_PALETTE[3] = RGB5(0, 31, 0);     // BRIGHT GREEN (waveform needs this)
    SPRITE_PALETTE[4] = RGB5(31, 31, 0);    // BRIGHT YELLOW (waveform needs this)
    
    is_initialized = 0;
}

void update_geometric_visualizer(void) {
    if (!is_initialized) return;
    
    // All audio processing moved to render function to avoid timing conflicts
    // The spectrum visualizer processes data after this update, so we read fresh data in render
}

void render_geometric_hexagon(void) {
    if (!is_initialized) return;
    
    // IN MODE_1: Sprite tiles can use the full range 0-1023 at 0x06010000
    // No need to worry about framebuffer overlap like in MODE_3
    
    // Animate rotation and pulsing based on audio data
    rotation_angle += 2; // Steady rotation
    if (rotation_angle >= 360) rotation_angle = 0;
    
    pulse_counter++;
    color_cycle++;
    
    // Process audio for responsive geometric animation
    long total_audio_energy = 0;
    int active_bands = 0;
    
    for (int i = 1; i < 7; i++) { // Use frequency bands 1-6 for geometric response
        if (spectrum_accumulators_8ad[i] > 0 && spectrum_sample_count_8ad > 0) {
            total_audio_energy += spectrum_accumulators_8ad[i] / spectrum_sample_count_8ad;
            active_bands++;
        }
    }
    
    // Scale based on audio energy for pulsing effect
    int scale = 16; // Base size
    if (active_bands > 0) {
        long avg_energy = total_audio_energy / active_bands;
        scale = 12 + (avg_energy / 100); // Pulse from 12 to ~20+ pixels
        if (scale > 24) scale = 24; // Max size
        if (scale < 8) scale = 8;   // Min size
    }
    
    // Simple geometric shape - single 16x16 sprite in center of screen
    int center_x = 120; // Screen center X (240/2)
    int center_y = 80;  // Screen center Y (160/2)
    
    // Create a simple square sprite for now
    OAM[127].attr0 = ATTR0_NORMAL | ATTR0_COLOR_16 | ATTR0_SQUARE | (center_y & 0xFF);
    OAM[127].attr1 = ATTR1_SIZE_16 | (center_x & 0x01FF);
    OAM[127].attr2 = ATTR2_PALETTE(0) | 600; // Use high tile index to avoid conflicts
    
    // Create simple tile data dynamically
    u32* spriteGfx = (u32*)0x6010000;
    u32 color_data = 0x11111111; // Simple pattern
    
    // Cycle through colors based on audio
    if (active_bands > 0) {
        int color_index = (color_cycle / 10) % 4;
        switch(color_index) {
            case 0: color_data = 0x11111111; break; // Color 1
            case 1: color_data = 0x22222222; break; // Color 2  
            case 2: color_data = 0x33333333; break; // Color 3
            case 3: color_data = 0x12121212; break; // Mixed pattern
        }
    }
    
    // Write tile data for 16x16 sprite (4 8x8 tiles)
    for (int tile = 0; tile < 4; tile++) {
        for (int row = 0; row < 8; row++) {
            spriteGfx[(600 + tile) * 8 + row] = color_data;
        }
    }
}