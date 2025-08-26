#ifndef VISUALIZATION_MANAGER_H
#define VISUALIZATION_MANAGER_H

#include <gba.h>

// Available visualization types
typedef enum {
    VIZ_SPECTRUM_BARS = 0,
    VIZ_WAVEFORM = 1,
    VIZ_GEOMETRIC = 2,
    NUM_VISUALIZATIONS
} VisualizationMode;

// Initialize the visualization manager
void init_visualization_manager(void);

// Switch to a different visualization (handles cleanup/init automatically)
void switch_visualization(VisualizationMode new_mode);

// Get current visualization mode
VisualizationMode get_current_visualization(void);

// Get name of visualization for display
const char* get_visualization_name(VisualizationMode mode);

// Update and render the current visualization
void update_current_visualization(void);
void render_current_visualization(void);

// Handle visualization cycling with UP/DOWN keys
void handle_visualization_controls(unsigned short pressed_keys);

#endif // VISUALIZATION_MANAGER_H