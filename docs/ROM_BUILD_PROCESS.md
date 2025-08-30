# How ROM Build Works (8AD Format)

★ Insight ─────────────────────────────────────
This script performs a sophisticated ROM assembly process that combines your compiled code with King
Gizzard's music tracks using the GBFS (Game Boy File System). It's essentially creating a self-contained
music player cartridge by embedding all the audio data directly into the ROM file, just like commercial
GBA games did. Now using efficient 8AD format for better performance.
─────────────────────────────────────────────────

## Step-by-Step Process:

### 1. Audio File Preparation (Lines 6-10)

```bash
mkdir -p gbfs_content/side_a_8ad
cp album_processed/side_a_8ad/*.ad gbfs_content/side_a_8ad/
```
- Creates temporary folder for Side A music files
- Copies 4 8AD audio files (compressed King Gizzard tracks):
  - 01_crumbling_castle.ad (5.6MB)
  - 02_polygon.ad (1.8MB)
  - 03_castle_in_air.ad (1.5MB)
  - 04_deserted_dunes.ad (1.9MB)

### 2. GBFS Filesystem Creation (Line 13)

```bash
../tools/gbfs64/gbfs side_a_8ad.gbfs side_a_8ad/*.ad
```
- Packages all 4 music files into single side_a_8ad.gbfs archive (11MB total)
- Creates file directory table so your code can find tracks by name
- Similar to a ZIP file but optimized for GBA hardware access

### 3. Code Compilation (Lines 22-23)

```bash
make clean && make
```
- Compiles your C source code (main.c, 8ad_player.c, etc.) into polygon.gba
- Includes latest features:
  - Spectrum bar visualizer
  - Waveform visualizer with audio reactivity
  - Debug pixels for amplitude testing
  - Mode switching between visualizations
- Results in ~53KB base ROM with your visualization system

### 4. ROM Alignment (Lines 25-38)

```python
# Pad the ROM to 256-byte alignment (required for GBFS)
```
- Adds padding bytes to make ROM size divisible by 256
- Required because GBFS needs aligned memory access on GBA hardware
- Ensures the music data starts at correct memory boundary

### 5. Final Assembly (Line 42)

```bash
cat polygon.gba gbfs_content/side_a_8ad.gbfs > polygondwanaland_side_a_8ad.gba
```
- Concatenates your compiled code + music archive into single file
- Creates the final playable ROM: polygondwanaland_side_a_8ad.gba
- Results in 11MB complete cartridge image

### 6. ROM Header Fix (Line 46)

```bash
gbafix polygondwanaland_side_a_8ad.gba
```
- Updates ROM header with correct size information
- Calculates proper checksums for GBA hardware compatibility
- Makes ROM bootable on real hardware or emulators

## Final Result:

```
polygondwanaland_side_a_8ad.gba (11MB) =
├── Your compiled code (~53KB)
│   ├── Spectrum bar visualizer
│   ├── Waveform visualizer with debug pixels
│   ├── Mode switching (UP/DOWN arrows)
│   ├── Track switching (LEFT/RIGHT arrows)
│   ├── 8AD audio decoder
│   └── GBFS file system reader
└── Music archive (11MB)
    ├── Crumbling Castle (5.6MB)
    ├── Polygondwanaland (1.8MB)
    ├── The Castle In The Air (1.5MB)
    └── Deserted Dunes Welcome Weary Feet (1.9MB)
```

## Why This Works:
When you run the ROM, your code uses the GBFS functions (`find_first_gbfs_file()`, `gbfs_get_obj()`) to locate and stream the music files directly from the ROM's embedded filesystem - just like a real commercial game! 

## Audio Format Benefits (8AD vs GSM):
- **CPU Usage**: ~6% (vs 70% for GSM) - leaves plenty of power for visualizations
- **Compression**: ~4:1 (vs 10:1 for GSM) - larger files but much better performance
- **Quality**: Near-CD quality with ADPCM encoding

## Current Features:
- **Dual Visualizers**: Spectrum bars and waveform display
- **Audio Reactivity**: Waveform amplitude changes with music volume
- **Debug System**: Red pixels show current amplitude for testing
- **Smart Mode Detection**: Automatically detects Side A (4 tracks) vs Side B (6 tracks)

## Build Commands:
```bash
# Build Side A ROM
./build_side_a_8ad_rom.sh

# Build Side B ROM  
./build_side_b_8ad_rom.sh
```

🎵 #polygon