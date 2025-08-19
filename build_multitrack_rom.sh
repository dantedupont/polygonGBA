#!/bin/bash
set -e

echo "=== Building Multi-Track ROM ==="

# Step 1: Build base ROM
echo "Step 1: Building base ROM..."
make clean && make

# Step 2: Create GBFS structure with multiple tracks
echo "Step 2: Creating GBFS structure..."
mkdir -p multitrack_dir
cp test_tracks/*.gsm multitrack_dir/
cd multitrack_dir
../tools/gbfs64/gbfs ../multitrack.gbfs *.gsm
cd ..

# Step 3: Pad ROM to 256-byte boundary
echo "Step 3: Padding ROM to 256-byte boundary..."
python3 -c "
import os
size = os.path.getsize('polygon.gba')
remainder = size % 256
if remainder != 0:
    with open('polygon.gba', 'ab') as f:
        f.write(b'\0' * (256 - remainder))
    print(f'Padded ROM by {256 - remainder} bytes')
else:
    print('ROM already aligned to 256-byte boundary')
"

# Step 4: Combine ROM + GBFS
echo "Step 4: Combining ROM + GBFS..."
cat polygon.gba multitrack.gbfs > polygon_multitrack.gba

# Step 5: Apply final gbafix
echo "Step 5: Applying final ROM fix..."
gbafix polygon_multitrack.gba

echo "=== Build Complete ==="
echo "Output: polygon_multitrack.gba"
echo "Size: $(ls -lh polygon_multitrack.gba | awk '{print $5}')"
echo ""
echo "Multi-Track ROM Details:"
echo "- Tracks: $(ls -1 test_tracks/*.gsm | wc -l)"
echo "- Total audio size: $(du -sh test_tracks | awk '{print $1}')"
echo "- Sample Rate: 18.157 kHz"
echo "- Format: GSM RPE-LTP"
echo ""
echo "Controls:"
echo "- A/B/Start: Play/Pause"
echo "- Left/Right: Previous/Next track"
echo "- L/R: Seek back/forward"
echo "- Select: Toggle lock"