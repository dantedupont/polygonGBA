#include "visualization_manager.h"
#include "spectrum_visualizer.h"
// TODO: Add other visualization includes as we create them

// Current visualization state
static VisualizationMode current_mode = VIZ_SPECTRUM_BARS;
static VisualizationMode initialized_mode = VIZ_SPECTRUM_BARS; // Track what's currently loaded
static bool manager_initialized = false;

// Visualization names for display
static const char* viz_names[NUM_VISUALIZATIONS] = {
    "Spectrum Bars",
    "Waveform", 
    "Particles",
    "Oscilloscope"
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
            // TODO: cleanup_waveform_visualizer();
            break;
        case VIZ_PARTICLES:
            // TODO: cleanup_particle_visualizer();
            break;
        case VIZ_OSCILLOSCOPE:
            // TODO: cleanup_oscilloscope_visualizer();
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
            // TODO: init_waveform_visualizer();
            break;
        case VIZ_PARTICLES:
            // TODO: init_particle_visualizer();
            break;
        case VIZ_OSCILLOSCOPE:
            // TODO: init_oscilloscope_visualizer();
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
            // TODO: update_waveform_visualizer();
            break;
        case VIZ_PARTICLES:
            // TODO: update_particle_visualizer();
            break;
        case VIZ_OSCILLOSCOPE:
            // TODO: update_oscilloscope_visualizer();
            break;
        default:
            break;
    }
}

void render_current_visualization(void) {
    if (!manager_initialized) return;
    
    switch(current_mode) {
        case VIZ_SPECTRUM_BARS:
            render_spectrum_bars();
            break;
        case VIZ_WAVEFORM:
            // TODO: render_waveform();
            break;
        case VIZ_PARTICLES:
            // TODO: render_particles();
            break;
        case VIZ_OSCILLOSCOPE:
            // TODO: render_oscilloscope();
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