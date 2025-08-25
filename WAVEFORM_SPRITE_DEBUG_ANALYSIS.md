# Waveform Sprite Tile Memory Issue - Technical Analysis

## The Problem: Invisible Sprites
The waveform visualizer sprites were completely invisible despite correct sprite attribute setup, smooth audio processing, and proper initialization. The spectrum visualizer worked perfectly, but switching to waveform mode showed nothing.

## Root Cause: Sprite Tile Memory Conflicts

### How GBA Sprite Graphics Work
GBA sprites require two components:
1. **Sprite Attributes (OAM)**: Position, size, palette - stored in Object Attribute Memory
2. **Sprite Tile Data**: The actual graphics pixels - stored in sprite graphics memory at 0x6014000

```c
// This works (attributes)
OAM[0].attr0 = ATTR0_NORMAL | ATTR0_COLOR_16 | ATTR0_SQUARE | (50 & 0xFF);
OAM[0].attr1 = ATTR1_SIZE_32 | (100 & 0x01FF); 
OAM[0].attr2 = ATTR2_PALETTE(0) | tile_number; // ← This must point to valid graphics!
```

### The Memory Conflict
**Spectrum Visualizer**: Uses tiles 512-896
- 8 bars × 12 tiles per bar × 4 (for 32x8 sprites) = 384 tiles
- Memory range: 512 to (512 + 384) = 896

**Waveform Visualizer (broken attempts)**:
1. **Tile 500**: Too close to spectrum's 512 - possible overlap during initialization
2. **Tile 0**: Possible conflict with system/other graphics  
3. **Tile 900**: Beyond GBA sprite tile memory limits

### Failed Solutions Attempted
```c
// FAILED - Tile reference mismatch
OAM[sprite].attr2 = ATTR2_PALETTE(0) | (512 + waveform_tile); // Wrong reference
// Sprite pointed to tile 1012, but graphics data was at tile 500

// FAILED - Memory conflicts  
int waveform_tile = 500; // Overlapped with spectrum initialization
int waveform_tile = 0;   // System conflicts
int waveform_tile = 900; // Beyond hardware limits
```

## The Solution: Dynamic Tile Overwriting

Instead of finding "safe" memory, we dynamically claim the spectrum's working memory space:

```c
void render_waveform(void) {
    // Overwrite spectrum's first tiles with our green pattern EVERY FRAME
    u32* spriteGfx = (u32*)(0x6014000);
    u32 waveform_pattern = 0x33333333; // Green pattern
    int waveform_tile = 512; // Use spectrum's first tile
    
    // Overwrite just one tile for 8x8 sprites
    for (int row = 0; row < 8; row++) {
        spriteGfx[waveform_tile * 8 + row] = waveform_pattern;
    }
    
    // Create sprites using the freshly overwritten tile
    OAM[sprite].attr2 = ATTR2_PALETTE(0) | waveform_tile;
}
```

### Why This Works
1. **Guaranteed Working Memory**: We use tile 512, which spectrum proves works perfectly
2. **No Conflicts**: We only overwrite when in waveform mode  
3. **Dynamic Graphics**: Fresh tile data every frame ensures correct visuals
4. **Mode Isolation**: Spectrum mode recreates its own tiles, waveform mode creates its own

## Performance Optimizations Made

### 1. Smaller Sprites
**Before**: 32x8 sprites (thick, tall rectangles)
```c
OAM[sprite].attr1 = ATTR1_SIZE_32 | (wave_x & 0x01FF); // Thick wave
for (int x = 0; x < WAVEFORM_WIDTH; x += 16) // Sparse placement
```

**After**: 8x8 sprites (thin, precise dots)  
```c
OAM[sprite].attr1 = ATTR1_SIZE_8 | (wave_x & 0x01FF); // Thin wave
for (int x = 0; x < WAVEFORM_WIDTH; x += 8) // Denser placement
```

### 2. Optimized Tile Creation
**Before**: Created 4 tiles for 32x8 sprite (overkill)
```c
for (int subtile = 0; subtile < 4; subtile++) { // 4 tiles = 32x8
    for (int row = 0; row < 8; row++) {
        spriteGfx[(waveform_tile + subtile) * 8 + row] = waveform_pattern;
    }
}
```

**After**: Single tile for 8x8 sprite (efficient)
```c
for (int row = 0; row < 8; row++) { // 1 tile = 8x8
    spriteGfx[waveform_tile * 8 + row] = waveform_pattern;
}
```

## Key Insights

`★ Insight ─────────────────────────────────────`
**Memory Management**: GBA sprite development requires careful memory management. Instead of fighting for "safe" memory ranges, sometimes the best solution is to dynamically share memory between mutually exclusive modes.

**Hardware Limits**: The GBA sprite system has specific tile memory limits. High tile numbers (900+) may be beyond hardware capabilities.

**Debugging Strategy**: When sprites are invisible, test with known-working tile references first, then isolate whether the issue is in attributes or graphics data.
`─────────────────────────────────────────────────`

## Final Result
- **Visual**: Thin, smooth green waveform made of 8x8 sprites
- **Performance**: Efficient single-tile approach with dynamic overwriting
- **Compatibility**: No conflicts between spectrum and waveform modes
- **Responsive**: Amplitude reacts smoothly to music volume

The solution demonstrates that sometimes the best approach in embedded systems is dynamic resource sharing rather than static allocation.