#!/usr/bin/env python3
"""
FFmpeg to PGDA Converter
Creates PGDA files from FFmpeg Opus compression using existing magic bytes
"""

import numpy as np
import soundfile as sf
import subprocess
import tempfile
import os
from pathlib import Path

def ffmpeg_to_pgda(input_file: str, output_file: str):
    """Convert audio to PGDA using FFmpeg Opus compression."""
    
    print(f"=== FFMPEG TO PGDA CONVERSION ===")
    
    # Target sample rate
    target_sr = 11025
    
    # Create temporary files
    temp_dir = tempfile.mkdtemp()
    temp_mono = os.path.join(temp_dir, "mono.wav")
    temp_resampled = os.path.join(temp_dir, "resampled.wav") 
    temp_compressed = os.path.join(temp_dir, "compressed.opus")
    temp_decoded = os.path.join(temp_dir, "decoded.wav")
    
    try:
        # Step 1: Convert to mono
        mono_cmd = ['ffmpeg', '-i', input_file, '-ac', '1', '-y', temp_mono]
        subprocess.run(mono_cmd, capture_output=True, check=True)
        
        # Step 2: Resample to 11.025kHz
        resample_cmd = [
            'ffmpeg', '-i', temp_mono,
            '-ar', str(target_sr),
            '-acodec', 'pcm_s16le',
            '-y', temp_resampled
        ]
        subprocess.run(resample_cmd, capture_output=True, check=True)
        
        # Step 3: Compress with Opus at 64kbps
        compress_cmd = [
            'ffmpeg', '-i', temp_resampled,
            '-c:a', 'libopus',
            '-b:a', '64k',
            '-vbr', 'on',
            '-compression_level', '10',
            '-y', temp_compressed
        ]
        subprocess.run(compress_cmd, capture_output=True, check=True)
        
        # Step 4: Decode back to WAV
        decode_cmd = [
            'ffmpeg', '-i', temp_compressed,
            '-ar', str(target_sr),
            '-ac', '1',
            '-acodec', 'pcm_s16le',
            '-y', temp_decoded
        ]
        subprocess.run(decode_cmd, capture_output=True, check=True)
        
        # Step 5: Convert to PGDA format with FQWT magic (compatible with existing decoder)
        audio_data, sr = sf.read(temp_decoded)
        print(f"Loaded decoded audio: {len(audio_data)} samples @ {sr}Hz")
        
        # Convert to 8-bit signed integers
        audio_8bit = np.round(audio_data * 127).astype(np.int8)
        
        # Create delta compression
        deltas = np.diff(audio_8bit)
        first_sample = audio_8bit[0]
        
        # Write PGDA file with FQWT magic (existing decoder compatible)
        with open(output_file, 'wb') as f:
            f.write(b'FQWT')  # Use existing magic bytes
            f.write(np.array([target_sr], dtype=np.uint32).tobytes())
            f.write(np.array([len(deltas)], dtype=np.uint32).tobytes())
            f.write(np.array([first_sample], dtype=np.int8).tobytes())
            f.write(deltas.astype(np.int8).tobytes())
        
        # Calculate compression metrics
        original_size = Path(input_file).stat().st_size
        compressed_size = Path(output_file).stat().st_size
        compression_ratio = original_size / compressed_size
        
        print(f"Original size: {original_size:,} bytes")
        print(f"Compressed size: {compressed_size:,} bytes") 
        print(f"Compression ratio: {compression_ratio:.2f}x")
        print(f"Sample rate: {target_sr}Hz")
        print(f"Duration: {len(audio_data) / target_sr:.1f}s")
        
        return {
            'compression_ratio': compression_ratio,
            'sample_rate': target_sr,
            'duration': len(audio_data) / target_sr
        }
        
    finally:
        # Cleanup
        for temp_file in [temp_mono, temp_resampled, temp_compressed, temp_decoded]:
            if os.path.exists(temp_file):
                os.remove(temp_file)
        os.rmdir(temp_dir)

def main():
    import sys
    if len(sys.argv) != 3:
        print("Usage: python ffmpeg_to_pgda.py <input.wav> <output.pgda>")
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_file = sys.argv[2]
    
    try:
        result = ffmpeg_to_pgda(input_file, output_file)
        print(f"\n=== CONVERSION COMPLETE ===")
        print(f"Output: {output_file}")
        print(f"Compression: {result['compression_ratio']:.2f}x")
        
        # Project to full album  
        album_size_mb = 526
        projected_size = album_size_mb / result['compression_ratio']
        print(f"Full album projection: {projected_size:.1f} MB")
        
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()