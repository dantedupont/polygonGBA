#!/bin/bash
set -e

echo "=== Building Polygondwanaland Side A ROM ==="

# Create GBFS for Side A
echo "Creating Side A GBFS..."
mkdir -p gbfs_content/side_a
cp album_processed/side_a_gsm/*.gsm gbfs_content/side_a/

# Build GBFS filesystem
cd gbfs_content
../tools/gbfs64/gbfs side_a.gbfs side_a/*.gsm
cd ..

echo "GBFS Contents:"
ls -lh gbfs_content/side_a/
echo "GBFS Size: $(ls -lh gbfs_content/side_a.gbfs | awk '{print $5}')"

# Build the base ROM
echo "Building base ROM..."
make clean && make

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
        f.write(b'\0' * padding)
    print(f'Padded ROM by {padding} bytes to {size + padding} total')
"

# Create the final ROM with GBFS audio
echo "Combining ROM with Side A audio..."
cat polygon.gba gbfs_content/side_a.gbfs > polygondwanaland_side_a.gba

# Apply ROM fix to the combined file
echo "Applying ROM fix..."
gbafix polygondwanaland_side_a.gba

echo "=== Side A ROM Build Complete ==="
echo "Output: polygondwanaland_side_a.gba"
echo "Size: $(ls -lh polygondwanaland_side_a.gba | awk '{print $5}')"
echo ""
echo "Side A Track List:"
echo "1. Crumbling Castle"
echo "2. Polygondwanaland"
echo "3. The Castle In The Air"
echo "4. Deserted Dunes Welcome Weary Feet"
echo ""
echo "Available space for features: ~$(echo "16 - $(du -m polygondwanaland_side_a.gba | awk '{print $1}')" | bc)MB"