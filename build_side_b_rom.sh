#!/bin/bash
set -e

echo "=== Building Polygondwanaland Side B ROM ==="

# Create GBFS for Side B
echo "Creating Side B GBFS..."
mkdir -p gbfs_content/side_b
cp album_processed/side_b_gsm/*.gsm gbfs_content/side_b/

# Build GBFS filesystem
cd gbfs_content
../tools/gbfs64/gbfs side_b.gbfs side_b/*.gsm
cd ..

echo "GBFS Contents:"
ls -lh gbfs_content/side_b/
echo "GBFS Size: $(ls -lh gbfs_content/side_b.gbfs | awk '{print $5}')"

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
echo "Combining ROM with Side B audio..."
cat polygon.gba gbfs_content/side_b.gbfs > polygondwanaland_side_b.gba

# Apply ROM fix to the combined file
echo "Applying ROM fix..."
gbafix polygondwanaland_side_b.gba

echo "=== Side B ROM Build Complete ==="
echo "Output: polygondwanaland_side_b.gba"
echo "Size: $(ls -lh polygondwanaland_side_b.gba | awk '{print $5}')"
echo ""
echo "Side B Track List:"
echo "5. Inner Cell"
echo "6. Loyalty"
echo "7. Horology" 
echo "8. Tetrachromacy"
echo "9. Searching..."
echo "10. The Fourth Colour"
echo ""
echo "Available space for features: ~$(echo "16 - $(du -m polygondwanaland_side_b.gba | awk '{print $1}')" | bc)MB"