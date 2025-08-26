# Video Architecture Documentation

## Overview

This GBA music visualizer uses a hybrid video architecture approach, leveraging different GBA video modes to optimize each visualization type. Each mode uses the most appropriate rendering method for its specific requirements.

## Video Mode Summary

| Visualization | Video Mode | Rendering Method | Text System | Primary Graphics |
|---------------|------------|------------------|-------------|------------------|
| Geometric     | Mode 1     | Tile-based + Sprites | BG0 Tiles | Audio-reactive sprites |
| Spectrum      | Mode 3     | Framebuffer + Sprites | Framebuffer | 32x8 spectrum bar sprites |
| Waveform      | Mode 3     | Framebuffer + Sprites | Framebuffer | 8x8 waveform sprites |

## Detailed Implementation

### Mode 1: Geometric Visualization

**Video Setup:**
```c
SetMode(MODE_1 | BG0_ENABLE | BG2_ENABLE | OBJ_ENABLE | OBJ_1D_MAP);
REG_BG0CNT = BG_SIZE_0 | BG_16_COLOR | CHAR_BASE(0) | SCREEN_BASE(31) | BG_PRIORITY(0);
REG_BG2CNT = BG_SIZE_0 | BG_256_COLOR | CHAR_BASE(1) | SCREEN_BASE(30) | BG_PRIORITY(1);
```

**Memory Layout:**
- **Character Base 0**: Font tiles (tiles 0-95)
- **Character Base 1**: Graphics tiles (tiles 100+) + sprite data
- **Screen Base 30**: BG2 tilemap (graphics background)
- **Screen Base 31**: BG0 tilemap (text overlay)

**Text Rendering:**
- Uses tile-based system with BG0 overlay
- Custom font tiles in character base 0
- Text positioned at bottom of screen (tile rows 17-18)
- Yellow text on transparent background

**Graphics Rendering:**
- Audio-reactive sprites (7 sprites forming geometric patterns)
- Dynamic sprite positioning based on audio amplitude
- Orange/yellow color scheme with audio-driven scaling

**Benefits of Mode 1:**
1. **Hardware Acceleration**: Tile-based backgrounds and sprite transformations are hardware-accelerated
2. **Memory Efficiency**: Tile reuse reduces VRAM usage compared to framebuffer
3. **Layering Control**: Separate priority levels for text (BG0) and graphics (BG2)
4. **Scalability**: Easy to add more complex tile-based effects
5. **Performance**: Sprites and tiles are rendered by PPU, reducing CPU load

### Mode 3: Spectrum and Waveform Visualizations

**Video Setup:**
```c
SetMode(MODE_3 | BG2_ENABLE | OBJ_ENABLE | OBJ_1D_MAP);
```

**Memory Layout:**
- **Framebuffer**: 0x6000000-0x6012C00 (240x160x16bpp)
- **Sprite Memory**: 0x6014000+ (after framebuffer)
- **Text Area**: Framebuffer region y=130-155

**Text Rendering:**
- Direct framebuffer pixel writing
- Blue background (RGB5(0,0,15)) with yellow/green text
- Positioned at bottom of framebuffer (y=130-155)
- Uses existing font system with direct pixel access

**Spectrum Graphics:**
- **Method**: Large 32x8 sprites for spectrum bars
- **Sprites**: Up to 96 sprites (8 bars × 12 height segments each)
- **Colors**: Frequency-based color mapping (blue→green→yellow→red)
- **Animation**: Height-based on audio frequency analysis

**Waveform Graphics:**
- **Method**: Small 8x8 sprites forming continuous wave
- **Sprites**: Up to 120 sprites across screen width
- **Colors**: Bright white/magenta for visibility
- **Animation**: Sine wave with amplitude driven by audio energy

## Text System Architecture

### Unified Track Title Display

All three visualizations display consistent track information:
- **Track Name**: Current playing track (e.g., "Crumbling Castle")
- **Visualization Mode**: Current mode name (e.g., "Geometric Hexagon")

### Mode-Specific Text Implementation

**Mode 1 (Geometric):**
```c
// Tile-based text using BG0
static void draw_text_bg0(int tile_x, int tile_y, const char* text) {
    u16* textMap = (u16*)SCREEN_BASE_BLOCK(31);
    // Map characters to tile indices for hardware rendering
}
```

**Mode 3 (Spectrum/Waveform):**
```c
// Framebuffer text using direct pixel access
static void draw_track_info_framebuffer(void) {
    u16* framebuffer = (u16*)0x6000000;
    // Direct pixel writing to framebuffer
    draw_text(framebuffer, x, y, text, color);
}
```

### Mode Transition Handling

**Challenge**: When switching from Mode 1 back to Mode 3, track titles disappeared because:
1. Mode 1 initialization may clear/modify the framebuffer
2. Mode 3 text systems use static variables to avoid unnecessary redraws
3. State tracking becomes inconsistent across mode switches

**Solution**: Force redraw on initialization
```c
void init_spectrum_visualizer(void) {
    // ... setup code ...
    force_spectrum_text_redraw(); // Ensure text appears after mode switch
}
```

## Technical Benefits Analysis

### Why Mode 1 for Geometric Visualization?

1. **Hardware Sprite Scaling**: Mode 1 supports affine transformations for BG2, enabling hardware-accelerated sprite effects
2. **Tile Efficiency**: Geometric patterns can be stored as reusable tiles rather than recalculated pixels
3. **Layer Separation**: Clean separation between text overlay (BG0) and graphics (BG2 + sprites)
4. **CPU Efficiency**: PPU handles most rendering, freeing CPU for audio processing

### Why Mode 3 for Spectrum/Waveform?

1. **Direct Pixel Control**: Spectrum bars and waveforms benefit from direct framebuffer access
2. **Smooth Animations**: Framebuffer allows for smooth, non-tile-aligned animations
3. **Complex Shapes**: Waveform curves are easier to render with direct pixel access
4. **Existing Codebase**: Leverages proven framebuffer rendering techniques

### Memory Optimization Strategies

**Character Base Separation:**
- Mode 1: Font tiles (CB0) separate from graphics tiles (CB1)
- Mode 3: No tile conflicts since framebuffer-based

**Screen Base Management:**
- BG0 (text): Screen base 31
- BG2 (graphics): Screen base 30
- No overlap or conflicts between layers

**Sprite Memory Layout:**
- Mode 1: Sprite tiles start after reserved tile ranges
- Mode 3: Sprite memory positioned after framebuffer (0x6014000+)

## Performance Characteristics

### CPU Usage Comparison

| Mode | Text Rendering | Graphics Rendering | Estimated CPU % |
|------|---------------|-------------------|-----------------|
| Mode 1 | Tile mapping (low) | Sprite positioning (low) | ~15% |
| Mode 3 | Pixel writing (medium) | Sprite management (medium) | ~25% |

### Memory Usage

| Component | Mode 1 | Mode 3 |
|-----------|--------|--------|
| Font Data | 6KB (tiles) | 6KB (bitmap) |
| Graphics | 8KB (tiles) | 76KB (framebuffer) |
| Text Buffer | 2KB (tilemap) | Framebuffer region |

## Implementation Lessons

### Key Insights Discovered

1. **Character Base Conflicts**: Different video modes require careful VRAM layout planning
2. **State Management**: Mode transitions need explicit state reset for text systems
3. **Timing Issues**: Initialization order affects visualization mode detection
4. **Memory Layout**: Proper separation prevents corruption between systems

### Best Practices Established

1. **Separate Memory Regions**: Each system gets dedicated VRAM space
2. **Force Redraw on Init**: Ensure visual consistency across mode switches
3. **Static Variable Management**: Reset tracking variables when needed
4. **Hardcoded Fallbacks**: Use known values when dynamic queries may be stale

## Future Considerations

### Potential Enhancements

1. **Mode 1 Spectrum**: Convert spectrum bars to tile-based for consistency
2. **Advanced Effects**: Leverage Mode 1's affine capabilities for rotation/scaling
3. **Memory Optimization**: Further VRAM layout improvements
4. **Performance Profiling**: Detailed CPU usage analysis per mode

### Scalability Notes

The hybrid approach provides excellent scalability:
- New tile-based visualizations can use Mode 1 template
- New framebuffer visualizations can use Mode 3 template  
- Text system works consistently across both approaches
- Memory layout supports additional graphics modes

## Conclusion

The hybrid video architecture successfully balances performance, memory efficiency, and visual quality. Each visualization uses the most appropriate GBA video mode for its requirements, while maintaining a consistent user interface through unified track title display.

This architecture demonstrates that different rendering approaches can coexist effectively, providing both the pixel-level control needed for smooth animations and the hardware acceleration benefits of tile-based graphics.