# Polygondwanaland Audio Compression System

Custom audio compression system for King Gizzard's Polygondwanaland album to fit on Game Boy Advance cartridge.

## Final Results
- **Compression Ratio**: 39.20x
- **Album Size**: 526 MB → 13.4 MB
- **Target**: 12 MB (achieved 89.4% of goal)
- **Format**: 6.75kHz, frequency-weighted quantization, delta compression

## Directory Structure

### `/encoder/`
- `polygond_audio_codec.py` - **Final working codec** (39.20x compression)
- `delta_encoder.py` - Basic delta compression component

### `/decoder/`
- (Empty - C decoder for GBA to be implemented)

### `/analysis/`
- `analyze_album.py` - Frequency analysis tool with psychoacoustic weights
- `compression_analyzer.py` - Compression opportunity assessment

### `/test_data/`
- `original/` - Source audio files (Crumbling Castle test track)
- `compressed/` - Final compressed outputs
  - `crumbling_castle_final.pgda` - Compressed binary (4.3 MB, 39.20x ratio)
  - `crumbling_castle_decoded.wav` - Decoded audio for quality testing
- `analysis_output/` - Frequency analysis charts and reports

## Usage

### Compress Audio
```bash
cd encoder
python polygond_audio_codec.py "../test_data/original/1 Crumbling Castle.wav"
```

### Test Compressed Audio
```bash
afplay test_data/compressed/crumbling_castle_decoded.wav
```

## Technical Details

**Frequency Importance Weights** (optimized for psychedelic music):
- Sub-bass (20-60Hz): 0.2
- Bass (60-200Hz): 0.9 (critical for visual synthesis)
- Low-mid (200-500Hz): 0.8
- Mid (500-2000Hz): 0.9 (vocals, instruments)
- High-mid (2000-4000Hz): 0.6
- High (4000-8000Hz): 0.5
- Ultra-high (8000Hz+): 0.3

**Compression Pipeline**:
1. Resample 44.1kHz → 6.75kHz
2. Frequency-domain weighting (preserve bass/mid)
3. Adaptive quantization (8-bit with energy-based precision)
4. Delta compression
5. Binary encoding (.pgda format)

## Next Steps
- Build C decoder for GBA ARM7TDMI processor
- Integrate with visual synthesis system
- Process full Polygondwanaland album