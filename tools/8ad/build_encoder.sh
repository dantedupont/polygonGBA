#!/bin/bash
# Build the 8AD encoder tool

echo "=== Building 8AD Encoder ==="

# Build the encoder
gcc -o myima myima.c readwav.c -lm

if [ $? -eq 0 ]; then
    echo "✓ 8AD encoder built successfully"
    echo "Usage: ./myima input.wav output.ad"
else 
    echo "✗ Failed to build 8AD encoder"
    exit 1
fi