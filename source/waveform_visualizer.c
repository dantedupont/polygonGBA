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
static int last_track_waveform = -1; // Track last known track for palette switching
static bool last_color_mode_waveform = false; // Track last known color mode state
static int color_offset = 0; // Pre-computed color offset for performance
static bool is_rainbow_mode_cached = false; // Cached color mode state for performance

// Update waveform palette based on current track or easter egg
static void update_waveform_palette(void) {
    if (is_color_mode_active()) {
        // "The Fourth Color" - set up rainbow palettes for multicolor waveform
        u16 rainbow_colors[7] = {
            RGB5(31, 0, 0),   // Red
            RGB5(31, 15, 0),  // Orange  
            RGB5(31, 31, 0),  // Yellow
            RGB5(0, 31, 0),   // Green
            RGB5(0, 0, 31),   // Blue
            RGB5(15, 0, 31),  // Indigo
            RGB5(31, 0, 31)   // Violet
        };
        
        // Set up palettes 8-14 for rainbow waveform (7 colors)
        for (int pal = 0; pal < 7; pal++) {
            SPRITE_PALETTE[(8 + pal) * 16 + 0] = RGB5(0, 0, 0);  // Transparent
            SPRITE_PALETTE[(8 + pal) * 16 + 1] = rainbow_colors[pal];
        }
    } else {
        // All other tracks - Game Boy green
        SPRITE_PALETTE[8 * 16 + 0] = RGB5(0, 0, 0);      // Transparent
        SPRITE_PALETTE[8 * 16 + 1] = RGB5(6, 12, 6);     // Game Boy dark green for waveform circles
        SPRITE_PALETTE[8 * 16 + 2] = RGB5(31, 31, 31);   // BRIGHT WHITE  
        SPRITE_PALETTE[8 * 16 + 3] = RGB5(31, 0, 31);    // BRIGHT MAGENTA
        SPRITE_PALETTE[8 * 16 + 4] = RGB5(31, 31, 0);    // BRIGHT YELLOW
    }
}

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
    is_rainbow_mode_cached = is_color_mode_active(); // Initialize cached state
    
    // Don't clear the framebuffer in init - let each mode handle its own background
    
    // Initialize waveform palettes - will be updated based on current track
    update_waveform_palette();
    
    // MODE_1: Create waveform sprite tile at index 120 (after spectrum tiles 100-115)
    u32* spriteGfx = (u32*)0x6010000; // MODE_1 standard sprite memory
    int waveform_tile_index = 120;
    
    // Create 8x8 circular tile for waveform dots
    // Circular pattern: each u32 represents one row of 8 pixels (4 bits each)
    // Creating a nice circular dot pattern
    u32 circle_rows[8] = {
        0x00011000,  // Row 0: ••**••••
        0x00111100,  // Row 1: •****•••
        0x01111110,  // Row 2: ******••
        0x01111110,  // Row 3: ******••
        0x01111110,  // Row 4: ******••
        0x01111110,  // Row 5: ******••
        0x00111100,  // Row 6: •****•••
        0x00011000   // Row 7: ••**••••
    };
    
    for(int row = 0; row < 8; row++) {
        spriteGfx[(waveform_tile_index * 8) + row] = circle_rows[row];
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
    
    // Check if track or color mode has changed and update palette accordingly
    int current_track = get_current_track_8ad();
    bool current_color_mode = is_color_mode_active();
    
    if (current_track != last_track_waveform || current_color_mode != last_color_mode_waveform) {
        update_waveform_palette();
        is_rainbow_mode_cached = current_color_mode; // Cache the color mode state for performance
        last_track_waveform = current_track;
        last_color_mode_waveform = current_color_mode;
    }
    
    // PERFORMANCE: Reduced debug overhead
    
    // Sample the audio more frequently for better responsiveness
    sample_counter++;
    if (sample_counter >= 2) { // Sample every 2 frames for maximum responsiveness
        sample_counter = 0;
        // Processing audio data this frame
        
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
        
        // PERFORMANCE FIX: Streamlined amplitude calculation 
        long total_audio_energy = 0;
        int active_bands = 0;
        
        // Calculate total energy from spectrum data
        for (int i = 1; i < 7; i++) {
            if (spectrum_accumulators_8ad[i] > 0 && spectrum_sample_count_8ad > 0) {
                total_audio_energy += spectrum_accumulators_8ad[i] / spectrum_sample_count_8ad;
                active_bands++;
            }
        }
        
        // Update current amplitude with more dramatic scaling
        int target_amplitude = 3; // Smaller base amplitude for quiet music
        if (active_bands > 0) {
            long avg_energy = total_audio_energy / active_bands;
            // MUCH more aggressive scaling for dramatic waveform response
            long scaled_energy = (avg_energy * 60) / 100;  // Increased from 25 to 60
            target_amplitude = 3 + scaled_energy;
            if (target_amplitude > 50) target_amplitude = 50; // Higher max amplitude
            if (target_amplitude < 3) target_amplitude = 3;
        }
        
        // Less smoothing for more responsive amplitude changes
        current_amplitude = (current_amplitude * 2 + target_amplitude) / 3;
    }
}

// Unused helper function removed

void render_waveform(void) {
    if (!is_initialized) return;
    
    // PERFORMANCE FIX: Remove duplicate audio processing - amplitude is now set in update_waveform_visualizer()
    // This eliminates redundant spectrum calculations every frame
    
    // Clean waveform display - no debug lines needed
    
    // MODE_1: Use pre-created tile from init function  
    int waveform_tile = 120; // Our waveform tile from init (moved to avoid spectrum conflict)
    
    // PERFORMANCE: Remove debug palette writes - they cause memory writes every frame
    
    // PERFORMANCE FIX: Restored better update rate now that per-sprite function calls are eliminated  
    render_frame_counter++;
    if (render_frame_counter < 2) return; // Back to every 2 frames since we fixed the real performance issue
    render_frame_counter = 0;
    
    // Restore smoother wave animation
    wave_phase += 3; // Restored original animation speed
    
    // PERFORMANCE: Use cached color mode state - no function calls in render loop!
    if (is_rainbow_mode_cached) {
        color_offset = (wave_phase >> 5) & 7; // Faster color shift (divide by 32)
    }
    
    int sprite_count = 0;
    
    // PERFORMANCE: Pre-calculate constants and use simpler wave generation
    int start_x = (240 - WAVEFORM_WIDTH) / 2;
    int center_y = 60;
    
    // PERFORMANCE: Pre-calculate all color palettes to eliminate per-sprite calculations
    int gb_palette = 8; // Game Boy mode palette (constant)
    static int rainbow_palettes[30]; // Pre-calculated rainbow palette sequence
    static bool palettes_initialized = false;
    static int last_color_offset = -1; // Track when color offset changes
    
    // PERFORMANCE: Replace expensive modulo with lookup table
    static const int modulo_7_table[64] = {
        0,1,2,3,4,5,6,0,1,2,3,4,5,6,0,1,2,3,4,5,6,0,1,2,3,4,5,6,0,1,2,3,
        4,5,6,0,1,2,3,4,5,6,0,1,2,3,4,5,6,0,1,2,3,4,5,6,0,1,2,3,4,5,6,0
    };
    
    if (is_rainbow_mode_cached) {
        // Check if we need to recalculate palettes - but only every 4 frames to reduce CPU spikes
        int reduced_color_offset = color_offset >> 2; // Update 4x less frequently
        if (!palettes_initialized || reduced_color_offset != last_color_offset) {
            // Update rainbow palette sequence when colors shift
            for (int i = 0; i < 30; i++) {
                int lookup_index = (i + reduced_color_offset) & 63; // Mask instead of modulo
                int color_index = modulo_7_table[lookup_index]; // Use lookup table
                rainbow_palettes[i] = 8 + color_index;
            }
            palettes_initialized = true;
            last_color_offset = reduced_color_offset;
        }
    } else {
        palettes_initialized = false; // Reset for next rainbow mode
        last_color_offset = -1;
    }
    
    // Restore smooth quadratic curves but keep performance optimizations
    for (int x = 0; x < WAVEFORM_WIDTH; x += 8) {
        // Higher frequency flowing sine wave with gentle rounded crests
        int phase = ((x * 2) + wave_phase) & 127; // Double frequency: x*2 makes wave twice as frequent
        
        // Smooth quadratic curve segments for each quarter wave
        int sine_value;
        if (phase < 32) {
            // Rising curve (0-90 degrees): parabolic rise to peak
            // Formula: y = amplitude * (1 - (1-t)^2) where t goes from 0 to 1
            int t = phase; // 0 to 31
            int inverted_t = 32 - t; // 32 to 1
            sine_value = current_amplitude - ((inverted_t * inverted_t * current_amplitude) >> 10); // 1024 = 32^2
        } else if (phase < 64) {
            // Peak curve (90-180 degrees): parabolic fall from peak
            // Formula: y = amplitude * (1 - t^2) where t goes from 0 to 1
            int t = phase - 32; // 0 to 31
            sine_value = current_amplitude - ((t * t * current_amplitude) >> 10);
        } else if (phase < 96) {
            // Falling curve (180-270 degrees): parabolic fall to trough
            // Formula: y = -amplitude * (1 - (1-t)^2) where t goes from 0 to 1
            int t = phase - 64; // 0 to 31
            int inverted_t = 32 - t; // 32 to 1
            sine_value = -(current_amplitude - ((inverted_t * inverted_t * current_amplitude) >> 10));
        } else {
            // Trough curve (270-360 degrees): parabolic rise from trough
            // Formula: y = -amplitude * (1 - t^2) where t goes from 0 to 1
            int t = phase - 96; // 0 to 31
            sine_value = -(current_amplitude - ((t * t * current_amplitude) >> 10));
        }
        
        int wave_x = start_x + x;
        int wave_y = center_y + sine_value;
        
        // Create smaller 8x8 sprite
        if (wave_x >= 0 && wave_x < 240 && wave_y >= 0 && wave_y < 160 && sprite_count < 120) {
            // PERFORMANCE: Use pre-calculated palettes
            int waveform_palette;
            if (is_rainbow_mode_cached) {
                int palette_index = (x >> 3) % 30; // Simple modulo with pre-calculated array
                waveform_palette = rainbow_palettes[palette_index];
            } else {
                waveform_palette = gb_palette; // Constant - no calculations
            }
            
            OAM[sprite_count].attr0 = ATTR0_NORMAL | ATTR0_COLOR_16 | ATTR0_SQUARE | (wave_y & 0xFF);
            OAM[sprite_count].attr1 = ATTR1_SIZE_8 | (wave_x & 0x01FF);
            OAM[sprite_count].attr2 = ATTR2_PALETTE(waveform_palette) | waveform_tile;
            sprite_count++;
        }
    }
    
    // PERFORMANCE FIX: Simplified cleanup - clear all remaining waveform sprites at once
    for (int i = sprite_count; i < 120; i++) {
        OAM[i].attr0 = ATTR0_DISABLED;
    }
    
    // PERFORMANCE: Remove debug writes for clean audio
}