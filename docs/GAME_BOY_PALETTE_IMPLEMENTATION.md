# Game Boy Palette Transformation Implementation

## Overview

This document details the implementation of a Game Boy color palette transformation for the Polygondwanaland GBA music player, including the complex debugging journey that led to the final working solution.

## Objective

Transform the entire music player to use original Game Boy palette colors:
- **Game Boy Green (#9bbc0f)**: Bright green background for all modes
- **Special "Fourth Colour" Mode**: White background with rainbow visualizations for the final track
- **Album Cover Integration**: Maintain proper album artwork display with correct background colors

## The Problem

Initial attempts to implement Game Boy colors resulted in:
1. **Black background** instead of Game Boy green in all modes
2. **Screen flicker** when switching to album cover mode  
3. **Album cover pixels adopting background colors** (green artifacts in artwork)

## Root Cause Analysis

Through systematic debugging, we discovered **three fundamental issues**:

### Issue 1: Video Mode Incompatibility
**Problem**: Using MODE_1 with regular tiled background setup
- MODE_1: BG2 is **affine transformation** background (for rotation/scaling)
- Our code: Configured BG2 as **regular tiled** background
- **Result**: BG2 layer not rendering at all (appeared black)

**Solution**: Switch to MODE_0
- MODE_0: All backgrounds (BG0, BG1, BG2) are regular tiled
- Allows proper BG2 tiled background rendering

### Issue 2: Palette Index Conflicts
**Problem**: Multiple systems competing for same palette indices
- **Background tiles**: Used palette index 1 for Game Boy green
- **Album cover**: Also tried to use palette index 1 for black artwork
- **Album palette loading**: Overwrote carefully set background colors

**Solutions**:
1. **Protected palette ranges**: Modified album palette loading to skip indices 0-1
2. **Remapped album artwork**: Moved album pixels from index 1 to index 254
3. **Runtime enforcement**: Force BG_PALETTE[1] to Game Boy green every frame

### Issue 3: Layer Transparency and Positioning
**Problem**: Album cover covering entire screen instead of showing background
- Original tilemap loading filled entire 32x32 screen
- No transparency in non-artwork areas
- Incorrect interpretation of GRIT-generated tilemap structure

**Solution**: Selective tilemap loading with proper transparency
- Load only top-left 16x16 tiles from GRIT tilemap (where artwork resides)
- Clear tile 0 to ensure true transparency
- Use hardware scroll registers for precise positioning

## Technical Implementation

### Background System (BG2)
```c
// MODE_0 for regular tiled backgrounds
SetMode(MODE_0 | BG0_ENABLE | BG1_ENABLE | BG2_ENABLE | OBJ_ENABLE | OBJ_1D_MAP);

// BG2 configuration with explicit priority
REG_BG2CNT = BG_SIZE_0 | BG_256_COLOR | CHAR_BASE(1) | SCREEN_BASE(29) | BG_PRIORITY(2);

// Create background tile using palette index 1
for (int i = 0; i < 64; i++) {
    tileMem[64 + i] = 1; // Tile 1 uses palette index 1
}

// Set Game Boy green color
BG_PALETTE[1] = RGB5(19, 23, 1);  // Game Boy bright green
```

### Album Cover Integration
```c
// BG0 with higher priority than background
REG_BG0CNT = SCREEN_BASE(28) | CHAR_BASE(2) | BG_PRIORITY(1) | BG_256_COLOR | BG_SIZE_0;

// Hardware scroll positioning
REG_BG0HOFS = 0;   // Horizontal positioning
REG_BG0VOFS = 12;  // Vertical positioning

// Selective tilemap loading (16x16 artwork area only)
for (int y = 0; y < 16; y++) {
    for (int x = 0; x < 16; x++) {
        int screen_pos = (2 + y) * 32 + (7 + x);  // Center on screen
        int tilemap_pos = y * 32 + x;             // Top-left of GRIT tilemap
        screenEntries[screen_pos] = polygondwanaland_128Map[tilemap_pos];
    }
}

// Ensure transparency in unused areas
for (int i = 0; i < 64; i++) {
    albumTileData[i] = 0;  // Tile 0 = transparent
}
```

### Palette Management
```c
// Protected palette loading (skip indices used by background)
for (int i = 2; i < 256; i++) {  // Start from 2 to preserve BG_PALETTE[0] and [1]
    // Load album palette colors
}

// Album artwork uses high palette index to avoid conflicts
BG_PALETTE[254] = RGB5(0, 0, 0);  // Black for album pixels (remapped from index 0)

// Runtime protection against palette corruption
BG_PALETTE[1] = RGB5(19, 23, 1);  // Force Game Boy green every frame
```

## Layer Priority Structure

Final working layer configuration:
- **BG1 (Text)**: Priority 0 (highest) - Track titles and UI text
- **BG0 (Album)**: Priority 1 - Album artwork display
- **BG2 (Background)**: Priority 2 (lowest) - Game Boy green background

This allows:
- Text displays over everything
- Album artwork displays over background
- Background shows through in transparent album areas

## Key Insights

### 1. Hardware Compatibility
GBA video modes have specific capabilities. MODE_1's BG2 is affine-only, incompatible with regular tiled backgrounds.

### 2. Resource Conflicts
Multiple subsystems using the same palette indices create conflicts. Spatial separation (using different indices) works better than temporal coordination.

### 3. Transparency Implementation
True transparency requires both tilemap (tile 0) and character data (palette index 0) coordination. Hardware treats palette index 0 as transparent only if properly configured.

### 4. Tool Output Understanding
GRIT generates standardized data structures. A 128x128 image creates a 32x32 tilemap with artwork positioned in a specific region, not necessarily centered.

### 5. Debugging Complex Systems
When multiple systems interact, isolate each component:
- Test single layers in isolation
- Use extreme test colors (bright magenta) for visibility
- Verify fundamental assumptions (mode compatibility, memory addresses)

## Debugging Methodology

### Phase 1: System Architecture Analysis
- Map all video layers and their configurations
- Trace palette management across all subsystems
- Document initialization sequence and timing

### Phase 2: Isolation Testing
- Disable all layers except BG2
- Use bright test colors instead of subtle green
- Test with simple patterns before complex artwork

### Phase 3: Root Cause Identification
- Discovered MODE_1/BG2 incompatibility through systematic testing
- Found palette conflicts through comprehensive search
- Identified transparency issues through layer priority experiments

## Final Result

✅ **Game Boy green background** in all visualization modes  
✅ **Proper album cover display** with correct positioning  
✅ **"Fourth Colour" transformation** (white background, rainbow effects)  
✅ **No screen flicker** during mode transitions  
✅ **No color artifacts** in album artwork  

## Performance Notes

- Runtime palette enforcement adds minimal CPU overhead
- Selective tilemap loading reduces memory usage
- Hardware scroll positioning provides pixel-perfect control
- MODE_0 supports all required layer types efficiently

## Lessons Learned

1. **Verify hardware capabilities** before implementing algorithms
2. **Plan resource allocation** to prevent conflicts between subsystems  
3. **Use hardware features** (scroll registers) instead of complex software solutions
4. **Test fundamental assumptions** when debugging complex interactions
5. **Document the journey** - complex bugs often have simple root causes that are hard to find

This implementation demonstrates how embedded systems debugging requires understanding both high-level system interactions and low-level hardware constraints.