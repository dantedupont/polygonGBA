#!/usr/bin/env python3
"""
FFmpeg Comparison Codec
Uses FFmpeg's proven audio compression to establish baseline quality 
for comparison against our custom PGDA codec.
"""

import numpy as np
import soundfile as sf
from pathlib import Path
import subprocess
import tempfile
import os

def ffmpeg_compression(input_file: str, output_base: str):
    """FFmpeg-based compression for quality comparison."""
    
    print(f"=== FFMPEG COMPARISON COMPRESSION ===")
    
    # Load original audio for analysis
    audio, sr = sf.read(input_file)
    if len(audio.shape) > 1:
        audio = np.mean(audio, axis=1)  # Convert to mono
    
    print(f"Original: {len(audio)} samples @ {sr}Hz")
    
    # Target sample rate to match our custom codec
    target_sr = 11025
    
    # Create temporary files
    temp_dir = tempfile.mkdtemp()
    temp_mono = os.path.join(temp_dir, "mono_input.wav")
    temp_resampled = os.path.join(temp_dir, "resampled.wav")
    temp_compressed = os.path.join(temp_dir, "compressed.opus")
    temp_decoded = os.path.join(temp_dir, "decoded.wav")
    
    try:
        # Step 1: Convert to mono if needed
        sf.write(temp_mono, audio, sr)
        print(f"Mono conversion: {temp_mono}")
        
        # Step 2: Resample to target rate using FFmpeg's high-quality resampler
        resample_cmd = [
            'ffmpeg', '-i', temp_mono,
            '-ar', str(target_sr),
            '-ac', '1',  # Mono
            '-acodec', 'pcm_s16le',
            '-y', temp_resampled
        ]
        
        print("Running FFmpeg resample...")
        result = subprocess.run(resample_cmd, capture_output=True, text=True)
        if result.returncode != 0:
            print(f"FFmpeg resample error: {result.stderr}")
            return None
        
        # Load resampled audio for size calculation
        resampled_audio, _ = sf.read(temp_resampled)
        print(f"Resampled {sr}Hz → {target_sr}Hz: {len(resampled_audio)} samples")
        
        # Step 3: Compress using Opus codec at target bitrate
        # Calculate target bitrate to match our compression ratio (~24x)
        original_size = Path(input_file).stat().st_size
        target_compressed_size = original_size / 24.0  # Match our 24x ratio
        duration_seconds = len(resampled_audio) / target_sr
        target_bitrate_kbps = int((target_compressed_size * 8) / (duration_seconds * 1000))
        
        # Clamp bitrate to reasonable Opus range (6-64 kbps)
        target_bitrate_kbps = max(6, min(64, target_bitrate_kbps))
        
        compress_cmd = [
            'ffmpeg', '-i', temp_resampled,
            '-c:a', 'libopus',
            '-b:a', f'{target_bitrate_kbps}k',
            '-vbr', 'on',  # Variable bitrate for better quality
            '-compression_level', '10',  # Maximum compression efficiency
            '-y', temp_compressed
        ]
        
        print(f"Compressing with Opus at {target_bitrate_kbps}kbps...")
        result = subprocess.run(compress_cmd, capture_output=True, text=True)
        if result.returncode != 0:
            print(f"FFmpeg compression error: {result.stderr}")
            return None
        
        # Step 4: Decode back to WAV for comparison
        decode_cmd = [
            'ffmpeg', '-i', temp_compressed,
            '-ar', str(target_sr),
            '-ac', '1',
            '-acodec', 'pcm_s16le',
            '-y', temp_decoded
        ]
        
        print("Decoding for comparison...")
        result = subprocess.run(decode_cmd, capture_output=True, text=True)
        if result.returncode != 0:
            print(f"FFmpeg decode error: {result.stderr}")
            return None
        
        # Copy results to output location
        compressed_file = output_base + "_ffmpeg_compressed.opus"
        decoded_file = output_base + "_ffmpeg_decoded.wav"
        
        subprocess.run(['cp', temp_compressed, compressed_file])
        subprocess.run(['cp', temp_decoded, decoded_file])
        
        # Calculate compression metrics
        compressed_size = Path(compressed_file).stat().st_size
        compression_ratio = original_size / compressed_size
        
        print(f"Original size: {original_size:,} bytes")
        print(f"Compressed size: {compressed_size:,} bytes")
        print(f"Compression ratio: {compression_ratio:.2f}x")
        print(f"Target bitrate used: {target_bitrate_kbps}kbps")
        
        # Load decoded audio for analysis
        decoded_audio, decoded_sr = sf.read(decoded_file)
        print(f"Decoded audio: {len(decoded_audio)} samples @ {decoded_sr}Hz")
        
        return {
            'original_file': input_file,
            'compressed_file': compressed_file,
            'decoded_file': decoded_file,
            'compression_ratio': compression_ratio,
            'sample_rate': decoded_sr,
            'duration': len(decoded_audio) / decoded_sr,
            'bitrate_kbps': target_bitrate_kbps
        }
        
    finally:
        # Cleanup temporary files
        for temp_file in [temp_mono, temp_resampled, temp_compressed, temp_decoded]:
            if os.path.exists(temp_file):
                os.remove(temp_file)
        os.rmdir(temp_dir)

def create_ffmpeg_pgda_file(opus_file: str, output_file: str, sample_rate: int):
    """Convert Opus file to PGDA format for GBA integration."""
    
    print(f"Converting Opus to PGDA format...")
    
    # Create temporary WAV for processing
    temp_dir = tempfile.mkdtemp()
    temp_wav = os.path.join(temp_dir, "temp.wav")
    
    try:
        # Decode Opus to WAV with specific format
        decode_cmd = [
            'ffmpeg', '-i', opus_file,
            '-ar', str(sample_rate),
            '-ac', '1',
            '-acodec', 'pcm_s16le',
            '-y', temp_wav
        ]
        
        result = subprocess.run(decode_cmd, capture_output=True, text=True)
        if result.returncode != 0:
            print(f"Opus decode error: {result.stderr}")
            return False
        
        # Load decoded audio
        audio, sr = sf.read(temp_wav)
        print(f"Loaded decoded audio: {len(audio)} samples @ {sr}Hz")
        
        # Convert to 8-bit samples like our custom codec
        audio_8bit = np.round(audio * 127).astype(np.int8)
        
        # Create delta compression
        deltas = np.diff(audio_8bit)
        first_sample = audio_8bit[0]
        
        # Write PGDA file
        with open(output_file, 'wb') as f:
            f.write(b'FFMP')  # Magic for FFmpeg-compressed
            f.write(np.array([sample_rate], dtype=np.uint32).tobytes())
            f.write(np.array([len(deltas)], dtype=np.uint32).tobytes())
            f.write(np.array([first_sample], dtype=np.int8).tobytes())
            f.write(deltas.astype(np.int8).tobytes())
        
        print(f"PGDA file created: {output_file}")
        print(f"Delta range: {np.min(deltas)} to {np.max(deltas)}")
        
        return True
        
    finally:
        if os.path.exists(temp_wav):
            os.remove(temp_wav)
        os.rmdir(temp_dir)

def main():
    import sys
    if len(sys.argv) < 2:
        print("Usage: python ffmpeg_comparison_codec.py <input.wav>")
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_base = str(Path(input_file).stem) + "_ffmpeg"
    
    try:
        # Compress with FFmpeg
        result = ffmpeg_compression(input_file, output_base)
        
        if result is None:
            print("FFmpeg compression failed!")
            sys.exit(1)
        
        print(f"\n=== FFMPEG COMPRESSION COMPLETE ===")
        print(f"Compression: {result['compression_ratio']:.2f}x")
        print(f"Bitrate: {result['bitrate_kbps']}kbps")
        print(f"Play decoded: afplay {result['decoded_file']}")
        
        # Create PGDA version for GBA integration
        pgda_file = output_base + "_compressed.pgda"
        if create_ffmpeg_pgda_file(result['compressed_file'], pgda_file, result['sample_rate']):
            print(f"PGDA file for GBA: {pgda_file}")
        
        # Project to full album
        album_size_mb = 526
        projected_size = album_size_mb / result['compression_ratio']
        print(f"Full album projection: {projected_size:.1f} MB")
        
        if projected_size <= 24:
            print("✓ 24MB TARGET ACHIEVED!")
            cartridge_per_side = projected_size / 2
            print(f"Per cartridge: {cartridge_per_side:.1f} MB (leaves {16-cartridge_per_side:.1f} MB for visuals/code)")
        else:
            print(f"⚠ Need {projected_size/24:.1f}x more compression for 24MB target")
        
    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()

if __name__ == "__main__":
    main()