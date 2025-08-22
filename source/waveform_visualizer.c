#include "waveform_visualizer.h"
#include "8ad_player.h"

// Waveform visualizer state
static bool is_initialized = false;
static int waveform_samples[WAVEFORM_SAMPLES];
static int sample_index = 0;
static int sample_counter = 0;
static int dynamic_scale = 100; // Dynamic scaling factor for auto-adjustment

// Performance optimization state
static int render_frame_counter = 0;
static bool need_clear = true;

// Wave animation state
static int wave_phase = 0; // For animating the wave
static int current_amplitude = 10; // Current wave amplitude based on audio level

void init_waveform_visualizer(void) {
    if (is_initialized) return;
    
    // Clear waveform sample buffer
    for (int i = 0; i < WAVEFORM_SAMPLES; i++) {
        waveform_samples[i] = 0;
    }
    sample_index = 0;
    sample_counter = 0;
    render_frame_counter = 0;
    need_clear = true; // Always clear when switching to waveform mode
    wave_phase = 0;
    current_amplitude = 10;
    
    // Don't clear the framebuffer in init - let each mode handle its own background
    
    // Set up palette for waveform - use BRIGHT colors to ensure visibility
    SPRITE_PALETTE[0] = RGB5(0, 0, 0);      // Transparent
    SPRITE_PALETTE[1] = RGB5(31, 31, 31);   // BRIGHT WHITE for maximum visibility
    SPRITE_PALETTE[2] = RGB5(31, 0, 31);    // BRIGHT MAGENTA
    SPRITE_PALETTE[3] = RGB5(0, 31, 0);     // BRIGHT GREEN
    SPRITE_PALETTE[4] = RGB5(31, 31, 0);    // BRIGHT YELLOW
    
    // Create waveform sprite tiles at the very beginning of sprite memory
    u32* spriteGfx = (u32*)(0x6014000); // Start of sprite graphics memory
    
    // Create a bright solid tile pattern for the waveform
    u32 waveform_pattern = 0x11111111; // All pixels use color 1 (bright white from palette)
    
    // Create tile 0 as our waveform tile
    for (int row = 0; row < 8; row++) {
        spriteGfx[row] = waveform_pattern;
    }
    
    // Clear OAM
    for (int i = 0; i < 128; i++) {
        OAM[i].attr0 = ATTR0_DISABLED;
        OAM[i].attr1 = 0;
        OAM[i].attr2 = 0;
    }
    
    is_initialized = true;
}

void cleanup_waveform_visualizer(void) {
    if (!is_initialized) return;
    
    // Clear all OAM entries
    for (int i = 0; i < 128; i++) {
        OAM[i].attr0 = ATTR0_DISABLED;
        OAM[i].attr1 = 0;
        OAM[i].attr2 = 0;
    }
    
    // No debug cleanup needed anymore
    
    // Reset state
    for (int i = 0; i < WAVEFORM_SAMPLES; i++) {
        waveform_samples[i] = 0;
    }
    sample_index = 0;
    sample_counter = 0;
    render_frame_counter = 0;
    need_clear = true;
    
    is_initialized = false;
}

void update_waveform_visualizer(void) {
    if (!is_initialized) return;
    
    // Sample the audio more frequently for better responsiveness
    sample_counter++;
    if (sample_counter >= 4) { // Sample every 4 frames for more responsive waveform
        sample_counter = 0;
        
        // Get a representative audio sample from the current spectrum data
        // Use the mid-range frequencies for a good waveform representation
        long sample_sum = 0;
        int valid_bands = 0;
        
        // Use more frequency bands for richer waveform representation
        for (int i = 1; i < 7; i++) { // Use broader range of frequencies
            if (spectrum_accumulators_8ad[i] > 0) {
                // Weight the bands differently for more musical representation
                long weighted_amplitude = spectrum_accumulators_8ad[i];
                if (i == 2 || i == 3) weighted_amplitude *= 2; // Boost important mid frequencies
                if (i == 1) weighted_amplitude *= 3; // Boost bass for punch
                
                sample_sum += weighted_amplitude;
                valid_bands++;
            }
        }
        
        int new_sample = 0;
        if (valid_bands > 0 && spectrum_sample_count_8ad > 0) {
            // Get raw amplitude
            long avg_amplitude = sample_sum / valid_bands;
            
            // MUCH more aggressive scaling for dramatic waveforms!
            // Use dynamic scaling that adapts to audio levels
            new_sample = (avg_amplitude * 60 * 20) / (spectrum_sample_count_8ad * dynamic_scale);
            
            // Dynamic scaling adjustment - make it more responsive over time
            if (avg_amplitude > 0) {
                long signal_strength = avg_amplitude / spectrum_sample_count_8ad;
                if (signal_strength > dynamic_scale) {
                    dynamic_scale = signal_strength / 2; // Scale down for louder signals
                } else if (signal_strength < dynamic_scale / 8) {
                    dynamic_scale = signal_strength * 4; // Scale up for quieter signals
                }
                
                // Keep scale in reasonable range
                if (dynamic_scale < 50) dynamic_scale = 50;
                if (dynamic_scale > 2000) dynamic_scale = 2000;
            }
            
            // Clamp to full range but allow dramatic swings
            if (new_sample > 60) new_sample = 60;
            if (new_sample < -60) new_sample = -60;
        }
        
        // Add new sample to circular buffer
        waveform_samples[sample_index] = new_sample;
        sample_index = (sample_index + 1) % WAVEFORM_SAMPLES;
    }
}

// Helper function to choose appropriate sprite tile based on line slope
static int choose_line_tile(int dy) {
    if (dy == 0) return 0; // Horizontal line
    if (dy > 0) {
        if (dy <= 2) return 1; // Slight upward
        if (dy <= 5) return 2; // Medium upward
        return 3; // Steep upward
    } else {
        if (dy >= -2) return 4; // Slight downward
        if (dy >= -5) return 5; // Medium downward
        return 6; // Steep downward
    }
}

void render_waveform(void) {
    if (!is_initialized) return;
    
    // Calculate current audio level from spectrum data for wave amplitude
    long total_audio_energy = 0;
    int active_bands = 0;
    
    for (int i = 1; i < 7; i++) { // Use main frequency bands
        if (spectrum_accumulators_8ad[i] > 0 && spectrum_sample_count_8ad > 0) {
            total_audio_energy += spectrum_accumulators_8ad[i] / spectrum_sample_count_8ad;
            active_bands++;
        }
    }
    
    // Update wave amplitude based on audio level
    if (active_bands > 0) {
        long avg_energy = total_audio_energy / active_bands;
        // Scale to reasonable wave amplitude (5-30 pixels)
        current_amplitude = 5 + (avg_energy / 2000);
        if (current_amplitude > 30) current_amplitude = 30;
        if (current_amplitude < 5) current_amplitude = 5;
    }
    
    // Create waveform tile data
    u32* spriteGfx = (u32*)(0x6014000);
    u32 waveform_pattern = 0x11111111; // All pixels use color 1 (bright white)
    
    // Create tile 500 as our waveform tile (4 8x8 tiles for 32x8 sprite)
    int waveform_tile = 500;
    for (int subtile = 0; subtile < 4; subtile++) {
        for (int row = 0; row < 8; row++) {
            spriteGfx[(waveform_tile + subtile) * 8 + row] = waveform_pattern;
        }
    }
    
    // Animate wave phase for flowing effect
    wave_phase += 3; // Speed of wave animation
    
    int sprite_count = 0;
    
    // Create flowing sine wave using working sprite method
    for (int x = 0; x < WAVEFORM_WIDTH; x += 16) { // Every 16 pixels for performance
        // Simple sine wave calculation
        int angle = ((x + wave_phase) * 360) / 80; // Wave frequency
        angle = angle % 360;
        
        int sine_value;
        if (angle < 90) {
            sine_value = (angle * current_amplitude) / 90;
        } else if (angle < 180) {
            sine_value = ((180 - angle) * current_amplitude) / 90;
        } else if (angle < 270) {
            sine_value = -((angle - 180) * current_amplitude) / 90;
        } else {
            sine_value = -((360 - angle) * current_amplitude) / 90;
        }
        
        int start_x = (240 - WAVEFORM_WIDTH) / 2;
        int center_y = 80;
        
        int wave_x = start_x + x;
        int wave_y = center_y + sine_value;
        
        // Create sprite using the working method
        if (wave_x >= 0 && wave_x < 240 && wave_y >= 0 && wave_y < 160 && sprite_count < 120) {
            OAM[sprite_count].attr0 = ATTR0_NORMAL | ATTR0_COLOR_16 | ATTR0_SQUARE | (wave_y & 0xFF);
            OAM[sprite_count].attr1 = ATTR1_SIZE_32 | (wave_x & 0x01FF);
            OAM[sprite_count].attr2 = ATTR2_PALETTE(0) | (512 + waveform_tile);
            sprite_count++;
        }
    }
    
    // Disable unused sprites
    while (sprite_count < 128) {
        OAM[sprite_count].attr0 = ATTR0_DISABLED;
        sprite_count++;
    }
}