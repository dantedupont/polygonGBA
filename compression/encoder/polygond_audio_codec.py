#!/usr/bin/env python3
"""
Frequency-Weighted Codec
Builds on our working simple codec but adds frequency-weighted quantization.
"""

import numpy as np
import soundfile as sf
from pathlib import Path
import librosa
from scipy import signal

def frequency_weighted_compression(input_file: str, output_base: str):
    """Frequency-weighted version of our working codec."""
    
    print(f"=== FREQUENCY WEIGHTED COMPRESSION ===")
    
    # Your frequency importance weights
    frequency_importance = {
        'sub_bass': 0.2,      # 20-60Hz  
        'bass': 0.9,          # 60-200Hz
        'low_mid': 0.8,       # 200-500Hz
        'mid': 0.9,           # 500-2000Hz
        'high_mid': 0.6,      # 2000-4000Hz  
        'high': 0.5,          # 4000-8000Hz
        'ultra_high': 0.3     # 8000Hz+
    }
    
    # Load audio (same as working version)
    audio, sr = sf.read(input_file)
    if len(audio.shape) > 1:
        audio = np.mean(audio, axis=1)  # Convert to mono
    
    print(f"Original: {len(audio)} samples @ {sr}Hz")
    
    # Step 1: Downsample to 6.75kHz for better compression
    target_sr = 6750
    audio_resampled = librosa.resample(audio, orig_sr=sr, target_sr=target_sr)
    print(f"Resampled {sr}Hz → {target_sr}Hz: {len(audio_resampled)} samples")
    
    # Step 2: NEW - Frequency-weighted quantization
    print("Applying frequency-weighted quantization...")
    
    # Convert to frequency domain
    fft_data = np.fft.rfft(audio_resampled)
    freqs = np.fft.rfftfreq(len(audio_resampled), 1/target_sr)
    
    # Create frequency weight map
    weights = np.ones_like(freqs)
    for i, freq in enumerate(freqs):
        if freq <= 60:
            weights[i] = frequency_importance['sub_bass']
        elif freq <= 200:
            weights[i] = frequency_importance['bass'] 
        elif freq <= 500:
            weights[i] = frequency_importance['low_mid']
        elif freq <= 2000:
            weights[i] = frequency_importance['mid']
        elif freq <= 4000:
            weights[i] = frequency_importance['high_mid']
        elif freq <= 8000:
            weights[i] = frequency_importance['high']
        else:
            weights[i] = frequency_importance['ultra_high']
    
    print(f"Frequency weights - Bass: {np.mean(weights[freqs <= 200]):.2f}, Mid: {np.mean(weights[(freqs > 200) & (freqs <= 2000)]):.2f}, High: {np.mean(weights[freqs > 2000]):.2f}")
    
    # Apply frequency weighting to FFT coefficients
    # Higher importance = more precision = larger magnitude preservation
    weighted_fft = fft_data * weights
    
    # Convert back to time domain
    audio_weighted = np.fft.irfft(weighted_fft, n=len(audio_resampled))
    
    # Normalize to prevent clipping
    max_val = np.max(np.abs(audio_weighted))
    if max_val > 1.0:
        audio_weighted = audio_weighted / max_val
        print(f"Normalized by {max_val:.2f} to prevent clipping")
    
    # Step 3: Frequency-aware quantization
    # Use adaptive quantization that preserves high-importance frequency content
    
    # Method: Adaptive quantization based on local signal importance
    # High-amplitude regions (likely bass/mid) get more precision
    # Low-amplitude regions (likely high freq) get less precision
    
    # Calculate local signal energy to guide quantization precision
    window_size = 1024  # Small window for local analysis
    audio_quantized = np.zeros_like(audio_weighted)
    
    for i in range(0, len(audio_weighted), window_size):
        window_end = min(i + window_size, len(audio_weighted))
        window = audio_weighted[i:window_end]
        
        # Calculate local energy (higher energy = more important = more precision)
        local_energy = np.sqrt(np.mean(window**2))
        
        # Adaptive quantization scale based on energy
        # High energy gets full 127 range, low energy gets reduced range
        if local_energy > 0.5:  # High energy (likely bass/mid)
            quant_scale = 127
        elif local_energy > 0.2:  # Medium energy  
            quant_scale = 100
        else:  # Low energy (likely high freq)
            quant_scale = 80
            
        # Apply quantization to window
        audio_quantized[i:window_end] = np.round(window * quant_scale).astype(np.int8)
    
    print(f"Adaptive quantization applied - preserving high-energy (bass/mid) content")
    
    # Step 4: Delta compression (same as working version)
    deltas = np.diff(audio_quantized)
    first_sample = audio_quantized[0]
    
    print(f"Delta compression: {len(deltas)} deltas")
    print(f"Delta range: {np.min(deltas)} to {np.max(deltas)}")
    
    # Save compressed data (same format as working version)
    compressed_file = output_base + "_freq_weighted_compressed.bin"
    with open(compressed_file, 'wb') as f:
        f.write(b'FQWT')  # Magic for frequency-weighted
        f.write(np.array([target_sr], dtype=np.uint32).tobytes())
        f.write(np.array([len(deltas)], dtype=np.uint32).tobytes())
        f.write(np.array([first_sample], dtype=np.int8).tobytes())
        f.write(deltas.astype(np.int8).tobytes())
    
    # Calculate compression
    original_size = Path(input_file).stat().st_size
    compressed_size = Path(compressed_file).stat().st_size
    compression_ratio = original_size / compressed_size
    
    print(f"Original size: {original_size:,} bytes")
    print(f"Compressed size: {compressed_size:,} bytes")
    print(f"Compression ratio: {compression_ratio:.2f}x")
    
    # Decode (same as working version)
    decoded_file = output_base + "_freq_weighted_decoded.wav"
    
    with open(compressed_file, 'rb') as f:
        magic = f.read(4)
        if magic != b'FQWT':
            raise ValueError("Bad magic")
        
        sample_rate = np.frombuffer(f.read(4), dtype=np.uint32)[0]
        num_deltas = np.frombuffer(f.read(4), dtype=np.uint32)[0]
        first_sample = np.frombuffer(f.read(1), dtype=np.int8)[0]
        deltas = np.frombuffer(f.read(), dtype=np.int8)
    
    # Reconstruct audio
    reconstructed = [first_sample]
    for delta in deltas:
        next_sample = reconstructed[-1] + delta
        next_sample = max(-128, min(127, next_sample))
        reconstructed.append(next_sample)
    
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
        print("Usage: python frequency_weighted_codec.py <input.wav>")
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_base = str(Path(input_file).stem) + "_freq_weighted"
    
    try:
        result = frequency_weighted_compression(input_file, output_base)
        print(f"\n=== FREQUENCY WEIGHTED COMPRESSION COMPLETE ===")
        print(f"Compression: {result['compression_ratio']:.2f}x")
        print(f"Play decoded: afplay {result['decoded_file']}")
        
        # Project to full album
        album_size_mb = 526
        projected_size = album_size_mb / result['compression_ratio']
        print(f"Full album projection: {projected_size:.1f} MB")
        
        if projected_size <= 12:
            print("✓ TARGET ACHIEVED!")
        else:
            print(f"⚠ Need {projected_size/12:.1f}x more compression")
        
    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()

if __name__ == "__main__":
    main()