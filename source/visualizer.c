#include "visualizer.h"
#include <stdlib.h>

static SpectrumAnalyzer spectrum;

void init_visualizer(void) {
    for (int i = 0; i < SPECTRUM_BANDS; i++) {
        spectrum.current_heights[i] = 0;
        spectrum.target_heights[i] = 0;
        spectrum.decay_rates[i] = 2; // Decay speed
    }
}

void update_visualizer_from_audio(int audio_sample) {
    // Simplified processing for performance
    int abs_sample = audio_sample < 0 ? -audio_sample : audio_sample;
    
    // Quick scaling - reduce precision for speed
    int base_amplitude = abs_sample >> 11; // Divide by 2048 instead of 32767
    if (base_amplitude > SPECTRUM_MAX_HEIGHT) base_amplitude = SPECTRUM_MAX_HEIGHT;
    
    // Simplified frequency band distribution
    for (int i = 0; i < SPECTRUM_BANDS; i++) {
        int band_height;
        
        if (i < 5) {
            // Bass: Simple amplitude scaling
            band_height = (base_amplitude * (5 - i)) >> 2; // Divide by 4
        } else if (i < 11) {
            // Mids: Direct amplitude
            band_height = base_amplitude >> 1; // Divide by 2
        } else {
            // Treble: Reduced amplitude
            band_height = base_amplitude >> 2; // Divide by 4
        }
        
        // Simple variation
        band_height += i & 3; // Add 0-3 based on band index
        
        if (band_height > spectrum.target_heights[i]) {
            spectrum.target_heights[i] = band_height;
        }
    }
}

void animate_visualizer(void) {
    for (int i = 0; i < SPECTRUM_BANDS; i++) {
        // Move current height toward target
        if (spectrum.current_heights[i] < spectrum.target_heights[i]) {
            spectrum.current_heights[i] = spectrum.target_heights[i];
        } else if (spectrum.current_heights[i] > spectrum.target_heights[i]) {
            spectrum.current_heights[i] -= spectrum.decay_rates[i];
            if (spectrum.current_heights[i] < 0) {
                spectrum.current_heights[i] = 0;
            }
        }
        
        // Decay target for natural fall-off
        if (spectrum.target_heights[i] > 0) {
            spectrum.target_heights[i] -= 1;
            if (spectrum.target_heights[i] < 0) {
                spectrum.target_heights[i] = 0;
            }
        }
    }
}

void draw_visualizer(u16* buffer) {
    // Super minimal drawing - only draw tops of bars
    int band_width = 240 / SPECTRUM_BANDS;
    int spectrum_y_start = 140; // Move closer to bottom
    
    for (int band = 0; band < SPECTRUM_BANDS; band += 2) { // Skip every other band for speed
        int x_start = band * band_width;
        int height = spectrum.current_heights[band] >> 2; // Quarter height for speed
        
        // Skip tiny heights
        if (height < 1) continue;
        
        // Simple color based on band position
        u16 color = RGB5(5, 10 + band, 5); // Very dim, varying green
        
        // Draw just a few pixels at the top of each bar
        int y = spectrum_y_start - height;
        if (y >= 20 && y < 140) {
            for (int x = x_start; x < x_start + band_width - 4; x += 2) {
                buffer[y * 240 + x] = color;
            }
        }
    }
}