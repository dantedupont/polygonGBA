# Album Cover Display Implementation

## Overview

The album cover display system shows the Polygondwanaland album artwork as a static background when in "Album Cover" visualization mode. This document details the implementation, the major issues encountered, and how they were resolved.

## Final Implementation

### Core Components

1. **Source Files**
   - `source/album_cover.c` - Main implementation
   - `source/album_cover.h` - Header with configuration
   - `source/polygondwanaland_128.h/.s` - Grit-converted artwork data

2. **VRAM Resource Allocation**
   - **BG0**: SCREEN_BASE(28), CHAR_BASE(2), Priority 1
   - **Artwork Size**: 128x128 pixels (16x16 tiles) centered in 256x256 padded tilemap
   - **Colors**: 256-color palette with RGB channel swapping fix

### How It Works

1. **Initialization** (`init_album_cover()`)
   - Configures BG0 for 256-color tiled background
   - Loads 256-color palette with R/B channel swap fix
   - Copies tile data to CHAR_BASE(2)
   - Copies tilemap data to SCREEN_BASE(28)

2. **Display**
   - Uses hardware tiled background rendering (no CPU usage during display)
   - BG0 automatically composites with other backgrounds based on priority
   - 128x128 artwork appears centered due to grit's 256x256 padding

3. **Cleanup** (`cleanup_album_cover()`)
   - Sets BG0 to lowest priority (effectively hiding it)
   - Preserves data for fast re-activation

## Major Issues and Solutions

### Issue 1: Resource Conflicts
**Problem**: BG0 was initially configured to use the same VRAM locations as BG2 (the main background), causing display corruption and conflicts.

**Root Cause**: 
```c
// CONFLICT: Both using same memory
REG_BG2CNT = BG_SIZE_0 | BG_256_COLOR | CHAR_BASE(1) | SCREEN_BASE(29); // main.c
REG_BG0CNT = SCREEN_BASE(29) | CHAR_BASE(1) | ...;                      // album_cover.c
```

**Solution**: Allocated dedicated VRAM resources for BG0
```c
// FIXED: Separate memory allocation
REG_BG0CNT = SCREEN_BASE(28) | CHAR_BASE(2) | BG_PRIORITY(1) | BG_256_COLOR | BG_SIZE_0;
```

**Resource Map**:
- BG0 (Album Cover): SCREEN_BASE(28), CHAR_BASE(2)
- BG1 (Text): SCREEN_BASE(30), CHAR_BASE(0)  
- BG2 (Background): SCREEN_BASE(29), CHAR_BASE(1)

### Issue 2: Background Layer Not Enabled
**Problem**: BG0 was configured but not enabled in the main mode setting, so it never displayed.

**Solution**: Added BG0_ENABLE to the mode configuration
```c
// main.c - Enable BG0 alongside existing backgrounds
SetMode(MODE_1 | BG0_ENABLE | BG1_ENABLE | BG2_ENABLE | OBJ_ENABLE | OBJ_1D_MAP);
```

### Issue 3: Image Size vs Screen Size Mismatch  
**Problem**: Original 256x256 image was larger than GBA screen (240x160), requiring scrolling and causing display issues.

**Solution**: Used 128x128 source image that grit automatically pads to 256x256, creating natural centering without scrolling.

### Issue 4: Color Channel Swapping
**Problem**: Colors appeared incorrect (blue→orange, red→blue) due to RGB vs BGR format differences.

**Solution**: Implemented channel swapping during palette loading
```c
// Swap R and B channels to fix color mapping
u16 r = (originalColor & 0x1F);
u16 g = (originalColor >> 5) & 0x1F;  
u16 b = (originalColor >> 10) & 0x1F;
BG_PALETTE[i] = b | (g << 5) | (r << 10);  // B, G, R instead of R, G, B
```

## Technical Details

### Grit Conversion
The artwork is converted using grit with these settings:
```bash
grit polygondwanaland_128.bmp -gt -gB8 -mRtf -mLs -o polygondwanaland_128
```

- `-gt`: Generate tiles
- `-gB8`: 8-bit color depth (256 colors)
- `-mRtf`: Reduce duplicate tiles and flips
- `-mLs`: Output in screen block format

### Memory Usage
- **Tiles**: 16,448 bytes (257 unique tiles)
- **Tilemap**: 2,048 bytes (32x32 entries)
- **Palette**: 512 bytes (256 colors)
- **Total**: ~19KB

### Performance
- **CPU Usage**: 0% during display (hardware-rendered)
- **Init Time**: <1 frame (DMA transfers)
- **Mode Switch**: Instant priority change

## Integration with Visualization Manager

The album cover integrates with the visualization system through:
- `VIZ_GEOMETRIC` mode enum
- `switch_visualization()` for mode changes
- Priority-based show/hide mechanism
- No update/render functions needed (static display)

## Future Considerations

1. **Multiple Album Covers**: Could extend to load different artwork per track
2. **Animations**: Could implement fade transitions or rotation effects
3. **Size Variants**: Could support different artwork sizes (64x64, 160x128)
4. **Compression**: Could use compressed tile data for memory savings

## Debugging Tips

1. **Test Pattern**: Use simple tile index patterns to verify display system
2. **Resource Viewer**: Check VRAM usage in emulator debugger
3. **Priority Testing**: Verify layer ordering with different priority values
4. **Palette Inspection**: Check color values in emulator palette viewer