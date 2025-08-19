#ifndef VISUALIZER_H
#define VISUALIZER_H

#include <gba.h>

// Spectrum analyzer configuration
#define SPECTRUM_BANDS 16
#define SPECTRUM_MAX_HEIGHT 120

typedef struct {
    int current_heights[SPECTRUM_BANDS];
    int target_heights[SPECTRUM_BANDS];
    int decay_rates[SPECTRUM_BANDS];
} SpectrumAnalyzer;

// Initialize the visualization system
void init_visualizer(void);

// Update visualizations based on audio sample
void update_visualizer_from_audio(int audio_sample);

// Animate and update visualization state
void animate_visualizer(void);

// Draw the current visualization to screen buffer
void draw_visualizer(u16* buffer);

#endif