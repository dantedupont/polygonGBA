#include "visualization_manager.h"
#include "spectrum_visualizer.h"
#include "waveform_visualizer.h"
#include "geometric_visualizer.h"
#include <gba.h>

// Current visualization state
static VisualizationMode current_mode = VIZ_SPECTRUM_BARS;
static VisualizationMode initialized_mode = VIZ_SPECTRUM_BARS; // Track what's currently loaded
static bool manager_initialized = false;

// Visualization names for display
static const char* viz_names[NUM_VISUALIZATIONS] = {
    "Spectrum Bars",
    "Waveform",
    "Geometric"
};

void init_visualization_manager(void) {
    if (manager_initialized) return;
    
    // Initialize with spectrum bars (our current working visualization)
    current_mode = VIZ_SPECTRUM_BARS;
    initialized_mode = VIZ_SPECTRUM_BARS;
    init_spectrum_visualizer();
    
    manager_initialized = true;
}

static void cleanup_visualization(VisualizationMode mode) {
    switch(mode) {
        case VIZ_SPECTRUM_BARS:
            cleanup_spectrum_visualizer();
            break;
        case VIZ_WAVEFORM:
            cleanup_waveform_visualizer();
            break;
        case VIZ_GEOMETRIC:
            cleanup_geometric_visualizer();
            break;
        default:
            break;
    }
}

static void init_visualization(VisualizationMode mode) {
    switch(mode) {
        case VIZ_SPECTRUM_BARS:
            init_spectrum_visualizer();
            break;
        case VIZ_WAVEFORM:
            init_waveform_visualizer();
            break;
        case VIZ_GEOMETRIC:
            init_geometric_visualizer();
            break;
        default:
            break;
    }
}

void switch_visualization(VisualizationMode new_mode) {
    if (!manager_initialized) {
        init_visualization_manager();
    }
    
    if (new_mode == current_mode) {
        return; // Already using this visualization
    }
    
    // Cleanup current visualization
    cleanup_visualization(initialized_mode);
    
    // MODE_1: No mode switching needed - everything uses MODE_1
    // This eliminates all corruption artifacts from mode switching
    
    // Initialize new visualization
    init_visualization(new_mode);
    
    // Update state
    current_mode = new_mode;
    initialized_mode = new_mode;
}

VisualizationMode get_current_visualization(void) {
    return current_mode;
}

const char* get_visualization_name(VisualizationMode mode) {
    if (mode >= 0 && mode < NUM_VISUALIZATIONS) {
        return viz_names[mode];
    }
    return "Unknown";
}

void update_current_visualization(void) {
    if (!manager_initialized) return;
    
    switch(current_mode) {
        case VIZ_SPECTRUM_BARS:
            update_spectrum_visualizer();
            break;
        case VIZ_WAVEFORM:
            // For waveform mode: update waveform FIRST, then spectrum processing
            update_waveform_visualizer();  // Read spectrum data before it's reset
            update_spectrum_visualizer();  // Process and reset spectrum data
            break;
        case VIZ_GEOMETRIC:
            // For geometric mode: update geometric FIRST (reads spectrum data), 
            // then spectrum processing (resets data but doesn't affect our sprites since we render after)
            update_geometric_visualizer();
            update_spectrum_visualizer(); // Process and reset spectrum data for next frame
            break;
        default:
            break;
    }
}

void render_current_visualization(void) {
    if (!manager_initialized) return;
    
    // DEBUG: Store current mode in palette for debugging
    SPRITE_PALETTE[31] = current_mode; // 0=spectrum, 1=waveform, 2=geometric
    
    switch(current_mode) {
        case VIZ_SPECTRUM_BARS:
            render_spectrum_bars();
            break;
        case VIZ_WAVEFORM:
            render_waveform();
            break;
        case VIZ_GEOMETRIC:
            render_geometric_hexagon();
            break;
        default:
            break;
    }
}

void handle_visualization_controls(unsigned short pressed_keys) {
    if (!manager_initialized) return;
    
    if (pressed_keys & KEY_UP) {
        // Cycle to next visualization
        VisualizationMode next_mode = (current_mode + 1) % NUM_VISUALIZATIONS;
        switch_visualization(next_mode);
    }
    
    if (pressed_keys & KEY_DOWN) {
        // Cycle to previous visualization  
        VisualizationMode prev_mode = (current_mode - 1 + NUM_VISUALIZATIONS) % NUM_VISUALIZATIONS;
        switch_visualization(prev_mode);
    }
}