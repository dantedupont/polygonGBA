#ifndef SPECTRUM_VISUALIZER_H
#define SPECTRUM_VISUALIZER_H

#include <gba.h>

#define NUM_BARS 8
#define TILES_PER_BAR 12  // Each bar can be up to 12 tiles tall (96 pixels)

// Initialize the spectrum visualizer
void init_spectrum_visualizer(void);

// Cleanup the spectrum visualizer (for switching)
void cleanup_spectrum_visualizer(void);

// Update spectrum analysis and bar physics (call every 4 frames)
void update_spectrum_visualizer(void);

// Render all spectrum bars to screen (call every frame)
void render_spectrum_bars(void);

#endif // SPECTRUM_VISUALIZER_H