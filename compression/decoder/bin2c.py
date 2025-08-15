#!/usr/bin/env python3
"""
Convert binary file to C array for embedding in GBA ROM
"""
import sys
from pathlib import Path

def bin2c(input_file, output_file, array_name):
    """Convert binary file to C array"""
    
    # Read binary data
    with open(input_file, 'rb') as f:
        data = f.read()
    
    # Generate C code
    with open(output_file, 'w') as f:
        f.write(f"// Auto-generated from {Path(input_file).name}\n")
        f.write(f"// File size: {len(data)} bytes\n\n")
        f.write(f"const unsigned char {array_name}[] = {{\n")
        
        # Write data as hex bytes (16 per line)
        for i, byte in enumerate(data):
            if i % 16 == 0:
                f.write("    ")
            f.write(f"0x{byte:02x}")
            if i < len(data) - 1:
                f.write(",")
            if i % 16 == 15:
                f.write("\n")
            elif i < len(data) - 1:
                f.write(" ")
        
        if len(data) % 16 != 0:
            f.write("\n")
        
        f.write("};\n\n")
        f.write(f"const unsigned int {array_name}_size = {len(data)};\n")
    
    print(f"Converted {input_file} to {output_file}")
    print(f"Array name: {array_name}")
    print(f"Size: {len(data)} bytes")

if __name__ == "__main__":
    if len(sys.argv) != 4:
        print("Usage: python bin2c.py <input.bin> <output.c> <array_name>")
        sys.exit(1)
    
    bin2c(sys.argv[1], sys.argv[2], sys.argv[3])