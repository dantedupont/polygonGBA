# PolygonGBA Technical Overview

## Magic Bytes

Magic bytes are signature sequences at the beginning of files that identify the file format. They allow programs to quickly determine file types without relying on file extensions.

**Examples:**
- PNG files start with `89 50 4E 47` (‰PNG)
- ZIP files start with `50 4B 03 04` (PK..)
- Our old PGDA format used `46 51 57 54` (FQWT)
- GBFS files start with `50 69 6E 45` (PinE)

Magic bytes are crucial for the GBA because file extensions don't exist in ROM space - the system needs to identify data formats by examining the raw bytes.

## GSM vs PGDA Comparison

### PGDA (Custom Codec)
- **Format**: Custom audio codec we developed
- **Magic bytes**: `FQWT` (46 51 57 54)
- **Compression**: ~8:1 ratio
- **Quality**: Moderate, designed for GBA constraints
- **Implementation**: Custom decoder requiring significant development
- **Status**: Abandoned due to complexity and quality issues

### GSM (Global System for Mobile Communications)
- **Format**: Industry-standard telecom codec (RPE-LTP algorithm)
- **Magic bytes**: None (raw GSM frames)
- **Compression**: ~13:1 ratio
- **Quality**: Excellent for voice/music at low bitrates
- **Implementation**: Proven decoder from GSMPlayer-GBA project
- **Status**: Current solution - reliable and efficient

**Why GSM Won:**
- Mature, debugged decoder implementation
- Better compression efficiency
- No clicking artifacts (once timing was fixed)
- Established toolchain support

## GSM Decoder Operation

The GSM decoder implements the **RPE-LTP** (Regular Pulse Excitation with Long Term Prediction) algorithm:

### Process Flow:
1. **Frame Reading**: Reads 33-byte GSM frames from GBFS
2. **LTP Analysis**: Extracts long-term prediction parameters
3. **RPE Decoding**: Reconstructs regular pulse excitation signals
4. **Synthesis**: Combines LTP and RPE to generate 160 audio samples
5. **Interpolation**: Upsamples to match GBA's Direct Sound requirements

### Key Components:
- **gsm_decode()**: Main decoding function (160 samples per frame)
- **Double buffering**: 608-byte buffers for smooth playback
- **Sample interpolation**: 2:1 linear interpolation for audio smoothing

## Sox and "Lying to Sox"

**Sox** is a cross-platform audio processing tool that can convert between formats and sample rates.

### The Problem:
GSM format expects 8000 Hz input, but we want 18157 Hz quality. Sox won't directly convert arbitrary sample rates to GSM at non-standard rates.

### The "Lying" Solution:
```bash
# Step 1: Resample to our target rate but tell Sox it's 8000 Hz
sox input.wav -r 18157 -t s16 -c 1 - | sox -t s16 -r 8000 -c 1 - output.gsm
```

We resample to 18157 Hz but label it as 8000 Hz, then Sox encodes it as GSM. This preserves the higher frequency content while fitting into GSM's constraints.

**Why This Works:**
- GSM encoding is sample-rate agnostic at the algorithm level
- We just need to play it back at the correct rate (18157 Hz)
- The "lie" tricks Sox into processing higher-quality audio

## Audio Pipeline

```
Original WAV (CD Quality, Stereo)
         ↓
    FFmpeg Processing
    (Mono conversion, normalization)
         ↓
    Sox Resampling 
    (18157 Hz with "lying" technique)
         ↓
    GSM Encoding
    (33-byte frames, RPE-LTP compression)
         ↓
    GBFS Packaging
    (Filesystem embedding)
         ↓
    ROM Integration
    (Appended to GBA binary)
         ↓
    Runtime Playback
    (GSM decoder → Direct Sound → Speakers)
```

### Key Transformations:
- **525MB → 29MB**: ~95% size reduction
- **44.1kHz stereo → 18.157kHz mono**: Optimized for GBA hardware
- **Linear PCM → GSM RPE-LTP**: Efficient compression for limited ROM space

## ROM Building Process

### 1. Source Compilation
```bash
# Compile GBA ARM code
arm-none-eabi-gcc -c source/*.c
arm-none-eabi-ld -T gba.ld -o polygon.elf *.o
arm-none-eabi-objcopy -O binary polygon.elf polygon.gba
```

### 2. ROM Padding
```bash
# Pad to 256-byte alignment (required for GBFS discovery)
python3 -c "
size = os.path.getsize('polygon.gba')
remainder = size % 256
if remainder != 0:
    padding = 256 - remainder
    with open('polygon.gba', 'ab') as f:
        f.write(b'\0' * padding)
"
```

### 3. GBFS Integration
```bash
# Concatenate ROM + GBFS filesystem
cat polygon.gba gbfs_content/gsm_audio.gbfs > polygon_gsm.gba
```

### 4. ROM Fixing
```bash
# Fix GBA header checksums and validation
gbafix polygon_gsm.gba
```

**Why Padding Matters:**
GBFS uses `find_first_gbfs_file()` which searches memory in 256-byte increments. Without proper alignment, the filesystem won't be discovered.

## VBlank and Audio Buffering

### VBlank (Vertical Blank Interval)
VBlank occurs 60 times per second when the GBA's LCD finishes drawing a frame. This provides a timing reference for audio and graphics synchronization.

### Double Buffering System
```c
signed char double_buffers[2][608] __attribute__((aligned(4)));
```

**How It Works:**
1. **Buffer A** plays via DMA to FIFO while **Buffer B** fills with new audio data
2. On VBlank interrupt: swap buffers
3. **Buffer B** now plays while **Buffer A** fills
4. Repeat for continuous audio

### Timing Sequence:
```c
// GSMPlayer-GBA timing pattern
advancePlayback(&playback, &InputMapping);  // Fill next buffer
VBlankIntrWait();                           // Wait for display sync
writeFromPlaybackBuffer(&playback);        // Swap audio buffers
```

**Buffer Size Calculation:**
- 18157 Hz sample rate
- 60 FPS display = ~302 samples per frame
- 608 bytes = 304 samples (with safety margin)
- Each buffer contains exactly one frame's worth of audio

## Source File Purposes

### Core Application
- **main.c**: Main application loop, GBFS initialization, GSM playback control, visual feedback (green screen)

### GBFS Filesystem
- **libgbfs.c**: GBFS filesystem implementation for loading audio files from ROM
- **gbfs.h**: GBFS header definitions and function prototypes

### GSM Audio System (from GSMPlayer-GBA)
- **libgsm.c**: Main GSM decoder implementation, Direct Sound setup, double-buffered playback
- **libgsm.h**: GSM decoder interface and data structures
- **gsmcode.iwram.c**: GSM decoder optimized for IWRAM (fast internal memory)

### GSM Algorithm Implementation
- **gsm.h**: Core GSM algorithm definitions and constants
- **private.h**: Internal GSM decoder structures and private functions
- **proto.h**: Function prototypes for GSM algorithm components
- **unproto.h**: Alternative prototypes for different compiler configurations
- **config.h**: Build configuration for GSM decoder (platform-specific settings)

### Code Organization Rationale:
- **IWRAM optimization**: Critical audio code runs from fast internal RAM
- **Modular design**: GSM decoder is self-contained and portable
- **Proven stability**: GSMPlayer-GBA codebase has been thoroughly tested

**Memory Layout:**
- IWRAM: Time-critical GSM decoding functions
- ROM: Static data, GBFS filesystem, audio samples
- EWRAM: Buffers, state variables, general data