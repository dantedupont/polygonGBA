#!/bin/bash
set -e

echo "=== Building Polygondwanaland Side A ROM with 8AD Audio ==="

# Create GBFS for Side A (8AD format)
echo "Creating Side A 8AD GBFS..."
mkdir -p gbfs_content/side_a_8ad
cp album_processed/side_a_8ad/*.ad gbfs_content/side_a_8ad/

# Build GBFS filesystem  
cd gbfs_content
../tools/gbfs64/gbfs side_a_8ad.gbfs side_a_8ad/*.ad
cd ..

echo "GBFS Contents:"
ls -lh gbfs_content/side_a_8ad/
echo "GBFS Size: $(ls -lh gbfs_content/side_a_8ad.gbfs | awk '{print $5}')"

# Build the base ROM with 8AD support
echo "Building base ROM with 8AD support..."
make clean
make

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
echo "Combining ROM with Side A 8AD audio..."
cat polygon.gba gbfs_content/side_a_8ad.gbfs > polygondwanaland_side_a_8ad.gba

# Apply ROM fix to the combined file
echo "Applying ROM fix..."
gbafix polygondwanaland_side_a_8ad.gba

echo "=== Side A 8AD ROM Build Complete ==="
echo "Output: polygondwanaland_side_a_8ad.gba"
echo "Size: $(ls -lh polygondwanaland_side_a_8ad.gba | awk '{print $5}')"
echo ""
echo "Side A Track List (8AD Format):"
echo "1. Crumbling Castle"
echo "2. Polygondwanaland"
echo "3. The Castle In The Air"
echo "4. Deserted Dunes Welcome Weary Feet"
echo ""
echo "Codec: 8AD (Pin Eight ADPCM)"
echo "CPU Usage: ~6% (vs 70% for GSM)"
echo "Compression: ~4:1 (vs 10:1 for GSM)"