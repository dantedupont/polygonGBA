#!/bin/bash
set -e

echo "=== Processing True Stereo GSM (L/R Channel Separation) ==="

# Create output directory
mkdir -p stereo_dev/true_stereo_test

# Function to create true stereo GSM from stereo WAV
create_true_stereo_gsm() {
    local input_file="$1"
    local output_file="$2"
    local track_name="$3"
    
    echo "Processing: $track_name (TRUE STEREO)"
    echo "  Input: $input_file"
    echo "  Output: $output_file"
    
    # Create temporary files for L/R channels (macOS compatible)
    local temp_left=$(mktemp)
    local temp_right=$(mktemp)
    local temp_left_gsm=$(mktemp)
    local temp_right_gsm=$(mktemp)
    
    # Add proper extensions
    mv "$temp_left" "${temp_left}.wav"
    mv "$temp_right" "${temp_right}.wav"
    mv "$temp_left_gsm" "${temp_left_gsm}.gsm"
    mv "$temp_right_gsm" "${temp_right_gsm}.gsm"
    
    temp_left="${temp_left}.wav"
    temp_right="${temp_right}.wav"
    temp_left_gsm="${temp_left_gsm}.gsm"
    temp_right_gsm="${temp_right_gsm}.gsm"
    
    echo "  Step 1: Splitting stereo into L/R channels..."
    # Extract left channel (remix 1 = take only channel 1)
    sox "$input_file" "$temp_left" remix 1
    # Extract right channel (remix 2 = take only channel 2) 
    sox "$input_file" "$temp_right" remix 2
    
    echo "  Step 2: Converting L channel to GSM..."
    # Use our proven 18.157 kHz "lying to Sox" technique for LEFT channel
    sox "$temp_left" -r 18157 -t s16 -c 1 - | sox -t s16 -r 8000 -c 1 - "$temp_left_gsm"
    
    echo "  Step 3: Converting R channel to GSM..."
    # Use our proven 18.157 kHz "lying to Sox" technique for RIGHT channel  
    sox "$temp_right" -r 18157 -t s16 -c 1 - | sox -t s16 -r 8000 -c 1 - "$temp_right_gsm"
    
    echo "  Step 4: Interleaving L/R GSM frames..."
    # Create the interleaved stereo GSM file using Python
    python3 -c "
left_gsm_file = '$temp_left_gsm'
right_gsm_file = '$temp_right_gsm'
output_file = '$output_file'

print(f'    Reading L channel: {left_gsm_file}')
print(f'    Reading R channel: {right_gsm_file}') 
print(f'    Writing interleaved: {output_file}')

with open(left_gsm_file, 'rb') as left_f, \
     open(right_gsm_file, 'rb') as right_f, \
     open(output_file, 'wb') as out_f:
    
    frame_count = 0
    while True:
        # Read 33-byte GSM frame from left channel
        left_frame = left_f.read(33)
        if len(left_frame) != 33:
            break
            
        # Read 33-byte GSM frame from right channel  
        right_frame = right_f.read(33)
        if len(right_frame) != 33:
            break
            
        # Write interleaved: L-frame then R-frame
        out_f.write(left_frame)
        out_f.write(right_frame)
        frame_count += 1
    
    print(f'    Interleaved {frame_count} stereo frame pairs (66 bytes each)')
"
    
    # Clean up temporary files
    rm -f "$temp_left" "$temp_right" "$temp_left_gsm" "$temp_right_gsm"
    
    # Show file size
    echo "  Size: $(ls -lh "$output_file" | awk '{print $5}')"
    echo "  ✓ Complete (TRUE STEREO with separate L/R GSM encoding)"
    echo
}

# Process Crumbling Castle with true stereo separation
create_true_stereo_gsm "album_source/side_a/1 Crumbling Castle.wav" "stereo_dev/true_stereo_test/crumbling_castle_true_stereo.gsm" "Crumbling Castle"

echo "=== True Stereo Processing Complete ==="
echo "True stereo file size: $(du -sh stereo_dev/true_stereo_test | awk '{print $1}')"
echo
echo "Ready to build true stereo ROM!"