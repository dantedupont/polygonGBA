#!/bin/bash
set -e

echo "=== Building Polygondwanaland Side B ROM with 8AD Audio ==="

# Create GBFS for Side B (8AD format)
echo "Creating Side B 8AD GBFS..."
mkdir -p gbfs_content/side_b_8ad
cp album_processed/side_b_8ad/*.ad gbfs_content/side_b_8ad/

# Build GBFS filesystem  
cd gbfs_content
../tools/gbfs64/gbfs side_b_8ad.gbfs side_b_8ad/*.ad
cd ..

echo "GBFS Contents:"
ls -lh gbfs_content/side_b_8ad/
echo "GBFS Size: $(ls -lh gbfs_content/side_b_8ad.gbfs | awk '{print $5}')"

# Build the base ROM with 8AD support
echo "Building base ROM with 8AD support..."
make clean
make EXTRA_CFLAGS="-DUSE_8AD" SOURCES_EXCLUDE="audio_playback.c"

# Pad the ROM to 256-byte alignment (required for GBFS)
echo "Padding ROM for GBFS alignment..."
python3 -c "
import sys
import os
size = os.path.getsize('polygon.gba')
pad_size = 256
remainder = size % pad_size
if remainder != 0:
    padding = pad_size - remainder
    with open('polygon.gba', 'ab') as f:
        f.write(b'\\0' * padding)
    print(f'Padded ROM by {padding} bytes to {size + padding} total')
"

# Create the final ROM with GBFS audio
echo "Combining ROM with Side B 8AD audio..."
cat polygon.gba gbfs_content/side_b_8ad.gbfs > polygondwanaland_side_b_8ad.gba

# Apply ROM fix to the combined file
echo "Applying ROM fix..."
gbafix polygondwanaland_side_b_8ad.gba

echo "=== Side B 8AD ROM Build Complete ==="
echo "Output: polygondwanaland_side_b_8ad.gba"
echo "Size: $(ls -lh polygondwanaland_side_b_8ad.gba | awk '{print $5}')"
echo ""
echo "Side B Track List (8AD Format):"
echo "5. Inner Cell"
echo "6. Loyalty"
echo "7. Horology"
echo "8. Tetrachromacy"
echo "9. Searching..."
echo "10. The Fourth Colour"
echo ""
echo "Codec: 8AD (Pin Eight ADPCM)"
echo "CPU Usage: ~6% (vs 70% for GSM)"
echo "Compression: ~4:1 (vs 10:1 for GSM)"