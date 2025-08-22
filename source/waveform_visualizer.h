#ifndef WAVEFORM_VISUALIZER_H
#define WAVEFORM_VISUALIZER_H

#include <gba.h>

#define WAVEFORM_WIDTH 220    // Width of waveform display in pixels
#define WAVEFORM_HEIGHT 120   // Height of waveform display (full screen above text)
#define WAVEFORM_SAMPLES 110  // Number of sample points to track (more for wider display)

// Initialize the waveform visualizer
void init_waveform_visualizer(void);

// Cleanup the waveform visualizer
void cleanup_waveform_visualizer(void);

// Update waveform with new audio data
void update_waveform_visualizer(void);

// Render waveform to screen
void render_waveform(void);

#endif // WAVEFORM_VISUALIZER_H