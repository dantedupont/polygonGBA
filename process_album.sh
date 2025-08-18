#!/bin/bash
set -e

echo "=== Processing Polygondwanaland Album to GSM ==="

# Create output directories
mkdir -p album_processed/side_a_gsm
mkdir -p album_processed/side_b_gsm

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

echo "=== Processing Side A ==="
convert_to_gsm "album_source/side_a/1 Crumbling Castle.wav" "album_processed/side_a_gsm/01_crumbling_castle.gsm" "Crumbling Castle"
convert_to_gsm "album_source/side_a/2 Polygondwanaland.wav" "album_processed/side_a_gsm/02_polygon.gsm" "Polygondwanaland"  
convert_to_gsm "album_source/side_a/3 The Castle In The Air.wav" "album_processed/side_a_gsm/03_castle_in_air.gsm" "The Castle In The Air"
convert_to_gsm "album_source/side_a/4 Deserted Dunes Welcome Weary Feet.wav" "album_processed/side_a_gsm/04_deserted_dunes.gsm" "Deserted Dunes Welcome Weary Feet"

echo "=== Processing Side B ==="
convert_to_gsm "album_source/side_b/5 Inner Cell.wav" "album_processed/side_b_gsm/05_inner_cell.gsm" "Inner Cell"
convert_to_gsm "album_source/side_b/6 Loyalty.wav" "album_processed/side_b_gsm/06_loyalty.gsm" "Loyalty"
convert_to_gsm "album_source/side_b/7 Horology.wav" "album_processed/side_b_gsm/07_horology.gsm" "Horology"
convert_to_gsm "album_source/side_b/8 Tetrachromacy.wav" "album_processed/side_b_gsm/08_tetrachromacy.gsm" "Tetrachromacy"
convert_to_gsm "album_source/side_b/9 Searching....wav" "album_processed/side_b_gsm/09_searching.gsm" "Searching..."
convert_to_gsm "album_source/side_b/10 The Fourth Colour.wav" "album_processed/side_b_gsm/10_fourth_colour.gsm" "The Fourth Colour"

echo "=== Processing Complete ==="
echo "Side A total size: $(du -sh album_processed/side_a_gsm | awk '{print $1}')"
echo "Side B total size: $(du -sh album_processed/side_b_gsm | awk '{print $1}')"
echo
echo "Next steps:"
echo "1. Run ./build_side_a_rom.sh to create Side A ROM"
echo "2. Run ./build_side_b_rom.sh to create Side B ROM"