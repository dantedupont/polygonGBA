#ifndef GEOMETRIC_VISUALIZER_H
#define GEOMETRIC_VISUALIZER_H

#include <gba.h>

// Geometric visualizer configuration - BG2 hardware scaling with audio reactivity
#define MIN_HEXAGON_RINGS 1         // Minimum rings (legacy, kept for compatibility)
#define MAX_HEXAGON_RINGS 6         // Maximum rings (legacy, kept for compatibility)
#define MAX_GEOMETRIC_SPRITES 25    // Legacy sprite limit (kept for cleanup function)
#define HEXAGON_CENTER_X 120        // Center X position
#define HEXAGON_CENTER_Y 80         // Center Y position
#define BASE_SCALE 256              // Normal scale for BG2 hardware scaling (1.0x)

// Initialize the geometric visualizer (sprite setup)
void init_geometric_visualizer(void);

// Cleanup the geometric visualizer (hide sprites)
void cleanup_geometric_visualizer(void);

// Update hexagon size and color based on audio data
void update_geometric_visualizer(void);

// Render hexagon using arranged sprites (call every frame)
void render_geometric_hexagon(void);

#endif // GEOMETRIC_VISUALIZER_H