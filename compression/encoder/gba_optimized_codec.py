#!/usr/bin/env python3
"""
GBA-Optimized Audio Codec
8-bit delta compression specifically designed for Game Boy Advance Direct Sound.
"""

import numpy as np
import soundfile as sf
from pathlib import Path
import librosa

def gba_optimized_compression(input_file: str, output_base: str):
    """8-bit codec optimized for GBA Direct Sound constraints."""
    
    print(f"=== GBA-OPTIMIZED 8-BIT COMPRESSION ===")
    
    # Load audio
    audio, sr = sf.read(input_file)
    if len(audio.shape) > 1:
        audio = np.mean(audio, axis=1)  # Convert to mono
    
    print(f"Original: {len(audio)} samples @ {sr}Hz")
    
    # Step 1: Revert to proven working sample rate
    target_sr = 6750  # Back to working rate while we debug timer calculation
    audio_resampled = librosa.resample(audio, orig_sr=sr, target_sr=target_sr)
    print(f"Resampled {sr}Hz → {target_sr}Hz: {len(audio_resampled)} samples")
    
    # Step 2: Improved 8-bit quantization designed for GBA
    # Optimize dynamic range for better quality within 8-bit constraints
    
    # Calculate optimal scaling to use full 8-bit range
    audio_peak = np.max(np.abs(audio_resampled))
    if audio_peak > 0:
        # Scale to use 95% of range (leave headroom to prevent clipping)
        optimal_scale = 121.0 / audio_peak  # 121 = 95% of 127
        audio_scaled = audio_resampled * optimal_scale
    else:
        audio_scaled = audio_resampled * 127.0
    
    # Apply quantization with dithering for better quality
    # Simple triangular dithering reduces quantization noise
    dither = np.random.triangular(-0.5, 0, 0.5, size=len(audio_scaled))
    audio_dithered = audio_scaled + dither
    
    # Quantize to 8-bit
    audio_8bit = np.clip(np.round(audio_dithered), -128, 127).astype(np.int8)
    
    print(f"Optimized 8-bit quantization: range {np.min(audio_8bit)} to {np.max(audio_8bit)}")
    print(f"Dynamic range utilization: {(np.max(audio_8bit) - np.min(audio_8bit))/255.0*100:.1f}%")
    
    # Step 3: Delta compression with 8-bit deltas
    deltas = np.diff(audio_8bit.astype(np.int16))  # Use int16 for diff to prevent overflow
    first_sample = audio_8bit[0]
    
    # Clamp deltas to 8-bit range
    deltas = np.clip(deltas, -128, 127).astype(np.int8)
    
    print(f"Delta compression: {len(deltas)} deltas")
    print(f"Delta range: {np.min(deltas)} to {np.max(deltas)}")
    
    # Save compressed data with same format as working version
    compressed_file = output_base + "_gba_8bit.pgda"
    with open(compressed_file, 'wb') as f:
        f.write(b'FQWT')  # Magic bytes
        f.write(np.array([target_sr], dtype=np.uint32).tobytes())
        f.write(np.array([len(deltas)], dtype=np.uint32).tobytes())
        f.write(np.array([first_sample], dtype=np.int8).tobytes())
        f.write(deltas.tobytes())
    
    # Calculate compression
    original_size = Path(input_file).stat().st_size
    compressed_size = Path(compressed_file).stat().st_size
    compression_ratio = original_size / compressed_size
    
    print(f"Original size: {original_size:,} bytes")
    print(f"Compressed size: {compressed_size:,} bytes")
    print(f"Compression ratio: {compression_ratio:.2f}x")
    
    # Test decode to verify quality
    decoded_file = output_base + "_gba_8bit_decoded.wav"
    
    with open(compressed_file, 'rb') as f:
        magic = f.read(4)
        if magic != b'FQWT':
            raise ValueError("Bad magic")
        
        sample_rate = np.frombuffer(f.read(4), dtype=np.uint32)[0]
        num_deltas = np.frombuffer(f.read(4), dtype=np.uint32)[0]
        first_sample = np.frombuffer(f.read(1), dtype=np.int8)[0]
        deltas = np.frombuffer(f.read(), dtype=np.int8)
    
    # Reconstruct audio using 8-bit accumulation (matching our GBA decoder)
    current_sample = first_sample
    reconstructed = [current_sample]
    
    for delta in deltas:
        # 8-bit accumulation with clamping (matches GBA decoder behavior)
        next_sample = current_sample + delta
        next_sample = max(-128, min(127, next_sample))
        current_sample = next_sample
        reconstructed.append(current_sample)
    
    # Convert to float and save
    reconstructed_audio = np.array(reconstructed[:-1], dtype=np.float32) / 127.0
    sf.write(decoded_file, reconstructed_audio, sample_rate)
    
    print(f"Decoded audio saved: {decoded_file}")
    print(f"Duration: {len(reconstructed_audio) / sample_rate:.1f}s")
    
    return {
        'original_file': input_file,
        'compressed_file': compressed_file,
        'decoded_file': decoded_file,
        'compression_ratio': compression_ratio,
        'sample_rate': sample_rate,
        'duration': len(reconstructed_audio) / sample_rate
    }

def main():
    import sys
    if len(sys.argv) < 2:
        print("Usage: python gba_optimized_codec.py <input.wav>")
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_base = str(Path(input_file).stem)
    
    try:
        result = gba_optimized_compression(input_file, output_base)
        print(f"\n=== GBA-OPTIMIZED COMPRESSION COMPLETE ===")
        print(f"Compression: {result['compression_ratio']:.2f}x")
        print(f"Play decoded: afplay {result['decoded_file']}")
        
    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()

if __name__ == "__main__":
    main()