#!/bin/bash
set -e

echo "=== Processing Polygondwanaland Album to 8AD ==="

# Create output directories
mkdir -p album_processed/side_a_8ad
mkdir -p album_processed/side_b_8ad

# Function to convert WAV to 8AD using Pin Eight's pipeline
convert_to_8ad() {
    local input_file="$1"
    local output_file="$2"
    local track_name="$3"
    
    echo "Processing: $track_name"
    echo "  Input: $input_file"
    echo "  Output: $output_file"
    
    # Use Pin Eight's exact specifications: mono, 18157 Hz, 16-bit
    sox "$input_file" -r 18157 -c 1 -b 16 temp_8ad.wav
    
    # Convert to 8AD format
    ./tools/8ad/myima temp_8ad.wav "$output_file"
    
    # Clean up temp file
    rm -f temp_8ad.wav
    
    # Show file size
    echo "  Size: $(ls -lh "$output_file" | awk '{print $5}')"
    echo "  ✓ Complete"
    echo
}

echo "=== Processing Side A ==="
convert_to_8ad "album_source/side_a/1 Crumbling Castle.wav" "album_processed/side_a_8ad/01_crumbling_castle.ad" "Crumbling Castle"
convert_to_8ad "album_source/side_a/2 Polygondwanaland.wav" "album_processed/side_a_8ad/02_polygon.ad" "Polygondwanaland"  
convert_to_8ad "album_source/side_a/3 The Castle In The Air.wav" "album_processed/side_a_8ad/03_castle_in_air.ad" "The Castle In The Air"
convert_to_8ad "album_source/side_a/4 Deserted Dunes Welcome Weary Feet.wav" "album_processed/side_a_8ad/04_deserted_dunes.ad" "Deserted Dunes Welcome Weary Feet"

echo "=== Processing Side B ==="
convert_to_8ad "album_source/side_b/5 Inner Cell.wav" "album_processed/side_b_8ad/05_inner_cell.ad" "Inner Cell"
convert_to_8ad "album_source/side_b/6 Loyalty.wav" "album_processed/side_b_8ad/06_loyalty.ad" "Loyalty"
convert_to_8ad "album_source/side_b/7 Horology.wav" "album_processed/side_b_8ad/07_horology.ad" "Horology"
convert_to_8ad "album_source/side_b/8 Tetrachromacy.wav" "album_processed/side_b_8ad/08_tetrachromacy.ad" "Tetrachromacy"
convert_to_8ad "album_source/side_b/9 Searching....wav" "album_processed/side_b_8ad/09_searching.ad" "Searching..."
convert_to_8ad "album_source/side_b/10 The Fourth Colour.wav" "album_processed/side_b_8ad/10_fourth_colour.ad" "The Fourth Colour"

echo "=== Processing Complete ==="
echo "Side A total size: $(du -sh album_processed/side_a_8ad | awk '{print $1}')"
echo "Side B total size: $(du -sh album_processed/side_b_8ad | awk '{print $1}')"
echo
echo "Next steps:"
echo "1. Update source code to support 8AD decoding"
echo "2. Run ./build_side_a_8ad_rom.sh to create 8AD ROM"