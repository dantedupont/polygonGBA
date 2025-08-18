#!/bin/bash
set -e

echo "=== Building PolygonGBA with GSM Audio ==="

# Build the base ROM
echo "Building base ROM..."
make clean && make

# Pad the ROM to 256-byte alignment (required for GBFS)
echo "Padding ROM for GBFS alignment..."
if ! command -v padbin &> /dev/null; then
    # Create a simple padding function if padbin is not available
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
else
    padbin 256 polygon.gba
fi

# Create the final ROM with GBFS audio
echo "Combining ROM with GSM audio..."
cat polygon.gba gbfs_content/gsm_audio.gbfs > polygon_gsm.gba

# Apply ROM fix to the combined file
echo "Applying ROM fix..."
gbafix polygon_gsm.gba

echo "=== Build Complete ==="
echo "Output: polygon_gsm.gba"
echo "Size: $(ls -lh polygon_gsm.gba | awk '{print $5}')"
echo ""
echo "GSM Audio Details:"
echo "- Sample Rate: 18.157 kHz"
echo "- Format: GSM RPE-LTP codec"
echo "- File: crumbling_castle_18157.gsm"
echo "- Size: $(ls -lh gsms/crumbling_castle_18157.gsm | awk '{print $5}')"