#include "waveform_visualizer.h"
#include "8ad_player.h"
#include "visualization_manager.h"

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
static int debug_active_bands = 0; // For debug display
static bool debug_processed_this_frame = false; // Did waveform process data this frame?
static long debug_avg_energy = 0; // Show the actual energy value
static long debug_scaled_energy = 0; // Show the scaled energy value
static int smoothed_amplitude = 10; // Smoothed amplitude to reduce jitter

void init_waveform_visualizer(void) {
    if (is_initialized) return;
    
    // MODE_1: No mode switching needed - everything uses MODE_1 now
    // SetMode already handled by main.c or visualization_manager
    
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
    
    // Set up palette 8 for waveform (avoid conflict with spectrum palettes 0-7)
    SPRITE_PALETTE[8 * 16 + 0] = RGB5(0, 0, 0);      // Transparent
    SPRITE_PALETTE[8 * 16 + 1] = RGB5(0, 31, 0);     // BRIGHT GREEN for waveform circles
    SPRITE_PALETTE[8 * 16 + 2] = RGB5(31, 31, 31);   // BRIGHT WHITE  
    SPRITE_PALETTE[8 * 16 + 3] = RGB5(31, 0, 31);    // BRIGHT MAGENTA
    SPRITE_PALETTE[8 * 16 + 4] = RGB5(31, 31, 0);    // BRIGHT YELLOW
    
    // MODE_1: Create waveform sprite tile at index 120 (after spectrum tiles 100-115)
    u32* spriteGfx = (u32*)0x6010000; // MODE_1 standard sprite memory
    int waveform_tile_index = 120;
    u32 pixel_data = 0x11111111; // All pixels use palette color 1 (bright green)
    
    // Create 8x8 solid tile for waveform dots
    for(int row = 0; row < 8; row++) {
        spriteGfx[(waveform_tile_index * 8) + row] = pixel_data;
    }
    
    // Sprite creation moved to render function - no initialization needed here
    
    // Clear only waveform's OAM entries (0-119, up to 120 sprites)
    for (int i = 0; i < 120; i++) {
        OAM[i].attr0 = ATTR0_DISABLED;
        OAM[i].attr1 = 0;
        OAM[i].attr2 = 0;
    }
    
    is_initialized = true;
}

void cleanup_waveform_visualizer(void) {
    if (!is_initialized) return;
    
    // Clear only waveform's OAM entries (0-119, up to 120 sprites)
    for (int i = 0; i < 120; i++) {
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
    
    debug_processed_this_frame = false; // Reset debug flag
    
    // Sample the audio more frequently for better responsiveness
    sample_counter++;
    if (sample_counter >= 4) { // Sample every 4 frames for more responsive waveform
        sample_counter = 0;
        debug_processed_this_frame = true; // Mark that we processed data
        
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
        
        // Update wave amplitude based on fresh spectrum data (moved from render)
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
            
            // Fix integer division - multiply first, then divide
            long scaled_energy = (avg_energy * 25) / 100;  // Much more aggressive scaling
            current_amplitude = 5 + scaled_energy;
            if (current_amplitude > 30) current_amplitude = 30;
            if (current_amplitude < 5) current_amplitude = 5;
            
            // Save scaled energy for debug
            debug_scaled_energy = scaled_energy;
        } else {
            // DEBUG: If no active bands, set amplitude to something obvious
            current_amplitude = 15; // Should show 15 white pixels if this path is taken
            debug_scaled_energy = 0;
        }
        
        // Save for debug display
        debug_active_bands = active_bands;
        debug_avg_energy = (active_bands > 0) ? (total_audio_energy / active_bands) : 0;
    }
}

// Unused helper function removed

void render_waveform(void) {
    if (!is_initialized) return;
    
    // Calculate amplitude every frame for immediate reactivity
    long total_audio_energy = 0;
    int active_bands = 0;
    
    for (int i = 1; i < 7; i++) { // Use main frequency bands
        if (spectrum_accumulators_8ad[i] > 0 && spectrum_sample_count_8ad > 0) {
            total_audio_energy += spectrum_accumulators_8ad[i] / spectrum_sample_count_8ad;
            active_bands++;
        }
    }
    
    // Update wave amplitude based on audio level with smoothing and drama
    int target_amplitude = 2; // Nearly flat when quiet
    
    if (active_bands > 0) {
        long avg_energy = total_audio_energy / active_bands;
        // Much more dramatic scaling - from 2 pixels (flat line) to 60 pixels (big waves)
        long scaled_energy = (avg_energy * 58) / 50;  // More aggressive scaling
        target_amplitude = 2 + scaled_energy;
        if (target_amplitude > 60) target_amplitude = 60; // Allow much bigger waves
        if (target_amplitude < 2) target_amplitude = 2;   // Nearly flat minimum
        debug_scaled_energy = scaled_energy;
    } else {
        debug_scaled_energy = 0;
    }
    
    // Smooth the amplitude changes to reduce jitter (interpolation)
    int amplitude_diff = target_amplitude - smoothed_amplitude;
    if (amplitude_diff > 0) {
        // Growing: fast response for attacks
        smoothed_amplitude += (amplitude_diff / 3) + 1;
    } else if (amplitude_diff < 0) {
        // Shrinking: slower decay for musical feel
        smoothed_amplitude += amplitude_diff / 8;
    }
    
    // Clamp smoothed value
    if (smoothed_amplitude > 60) smoothed_amplitude = 60;
    if (smoothed_amplitude < 2) smoothed_amplitude = 2;
    
    current_amplitude = smoothed_amplitude;
    
    debug_active_bands = active_bands;
    debug_avg_energy = (active_bands > 0) ? (total_audio_energy / active_bands) : 0;
    
    // Clean waveform display - no debug lines needed
    
    // MODE_1: Use pre-created tile from init function  
    int waveform_tile = 120; // Our waveform tile from init (moved to avoid spectrum conflict)
    
    // DEBUG: Store tile info for debugging
    SPRITE_PALETTE[24] = waveform_tile; // Store which tile we're using (120)
    u32* spriteGfx = (u32*)0x6010000; // MODE_1 sprite memory
    SPRITE_PALETTE[25] = (spriteGfx[waveform_tile * 8] & 0xFFFF); // Store tile data check
    
    // DEBUG: Store current visualization mode for debugging
    SPRITE_PALETTE[26] = get_current_visualization(); // Should be 1 (VIZ_WAVEFORM) when in waveform
    
    // Animate wave phase for flowing effect
    wave_phase += 3; // Speed of wave animation
    
    int sprite_count = 0;
    int attempted_sprites = 0; // DEBUG: Count how many sprites we try to create
    int failed_bounds = 0;     // DEBUG: Count how many fail bounds check
    
    // Create flowing sine wave using smaller 8x8 sprites
    for (int x = 0; x < WAVEFORM_WIDTH; x += 8) { // Every 8 pixels for thinner wave
        attempted_sprites++; // DEBUG: Count every attempt
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
        int center_y = 60; // More centered vertically (screen is 160px, info area is bottom 25px)
        
        int wave_x = start_x + x;
        int wave_y = center_y + sine_value;
        
        // Create smaller 8x8 sprite
        if (wave_x >= 0 && wave_x < 240 && wave_y >= 0 && wave_y < 160 && sprite_count < 120) {
            OAM[sprite_count].attr0 = ATTR0_NORMAL | ATTR0_COLOR_16 | ATTR0_SQUARE | (wave_y & 0xFF);
            OAM[sprite_count].attr1 = ATTR1_SIZE_8 | (wave_x & 0x01FF); // 8x8 sprites instead of 32x8
            OAM[sprite_count].attr2 = ATTR2_PALETTE(8) | waveform_tile; // Use palette 8 with green color
            sprite_count++;
        } else {
            failed_bounds++; // DEBUG: Count failed boundary checks
        }
    }
    
    // DEBUG: Store debug values in global area for main.c to display
    // We'll abuse some unused sprite palette entries as debug storage
    SPRITE_PALETTE[16] = sprite_count;        // Number of sprites created
    SPRITE_PALETTE[17] = attempted_sprites;   // Number of attempts 
    SPRITE_PALETTE[18] = failed_bounds;       // Number that failed bounds
    SPRITE_PALETTE[19] = current_amplitude;   // Current amplitude value
    
    // DEBUG: Store first few sprite positions to see what's happening
    if (sprite_count > 0) {
        SPRITE_PALETTE[20] = OAM[0].attr0 & 0xFF;   // First sprite Y position
        SPRITE_PALETTE[21] = OAM[0].attr1 & 0x1FF;  // First sprite X position
    }
    if (sprite_count > 10) {
        SPRITE_PALETTE[22] = OAM[10].attr0 & 0xFF;  // 11th sprite Y position  
        SPRITE_PALETTE[23] = OAM[10].attr1 & 0x1FF; // 11th sprite X position
    }
    
    // Disable only waveform's unused sprites (up to 120, don't touch geometric's OAM[127])
    while (sprite_count < 120) {
        OAM[sprite_count].attr0 = ATTR0_DISABLED;
        sprite_count++;
    }
}