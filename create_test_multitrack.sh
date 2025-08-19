#!/bin/bash
set -e

echo "=== Creating Multi-Track Test ROM ==="

# Create output directory
mkdir -p test_tracks

# Function to convert WAV to GSM using our proven pipeline
convert_to_gsm() {
    local input_file="$1"
    local output_file="$2"
    local track_name="$3"
    
    echo "Processing: $track_name"
    echo "  Input: $input_file"
    echo "  Output: $output_file"
    
    # Use our proven 18.157 kHz "lying to Sox" technique
    sox "$input_file" -r 18157 -t s16 -c 1 - | sox -t s16 -r 8000 -c 1 - "$output_file"
    
    # Show file size
    echo "  Size: $(ls -lh "$output_file" | awk '{print $5}')"
    echo "  ✓ Complete"
    echo
}

echo "Converting multiple tracks for testing..."

# Convert first 3 tracks from Side A for testing
convert_to_gsm "album_source/side_a/1 Crumbling Castle.wav" "test_tracks/01_crumbling_castle.gsm" "Crumbling Castle"
convert_to_gsm "album_source/side_a/2 Polygondwanaland.wav" "test_tracks/02_polygon.gsm" "Polygondwanaland"  
convert_to_gsm "album_source/side_a/3 The Castle In The Air.wav" "test_tracks/03_castle_in_air.gsm" "The Castle In The Air"

echo "=== Conversion Complete ==="
echo "Total tracks: $(ls -1 test_tracks/*.gsm | wc -l)"
echo "Total size: $(du -sh test_tracks | awk '{print $1}')"
echo ""
echo "Track listing:"
ls -lh test_tracks/

echo ""
echo "Next step: Build ROM with multi-track support!"