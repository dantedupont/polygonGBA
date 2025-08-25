#ifndef GEOMETRIC_VISUALIZER_H
#define GEOMETRIC_VISUALIZER_H

#include <gba.h>

// Geometric visualizer configuration - Mode 4 sprite-based
#define MAX_GEOMETRIC_SPRITES 32    // Maximum sprites for hexagon
#define GEOMETRIC_TILE_START 512    // Use spectrum's working tile (dynamic overwrite)
#define HEXAGON_CENTER_X 120        // Center of GBA screen
#define HEXAGON_CENTER_Y 80         // Center of GBA screen  
#define MIN_HEXAGON_RINGS 1         // Minimum rings for small hexagon
#define MAX_HEXAGON_RINGS 6         // Maximum rings for large hexagon

// Initialize the geometric visualizer (sprite setup)
void init_geometric_visualizer(void);

// Cleanup the geometric visualizer (hide sprites)
void cleanup_geometric_visualizer(void);

// Update hexagon size and color based on audio data
void update_geometric_visualizer(void);

// Render hexagon using arranged sprites (call every frame)
void render_geometric_hexagon(void);

#endif // GEOMETRIC_VISUALIZER_H