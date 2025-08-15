#!/usr/bin/env python3
"""
Delta Compression Encoder
Implements variable-length delta encoding for audio samples.
First stage of the multi-stage compression pipeline.
"""

import numpy as np
import struct
import json
from pathlib import Path
from typing import Dict, List, Tuple, Optional
import soundfile as sf

class DeltaEncoder:
    def __init__(self):
        # Bit allocation thresholds (based on delta magnitude)
        self.small_delta_threshold = 0.01    # 4 bits
        self.medium_delta_threshold = 0.1    # 8 bits  
        # Large deltas use 12 bits
        
        # Quantization levels for each bit depth
        self.quantization_levels = {
            4: 16,    # 2^4 = 16 levels
            8: 256,   # 2^8 = 256 levels
            12: 4096  # 2^12 = 4096 levels
        }
        
    def quantize_delta(self, delta: float, bits: int) -> int:
        """Quantize delta value to specified bit depth."""
        max_val = self.quantization_levels[bits] // 2 - 1  # Half range for signed
        min_val = -max_val - 1
        
        # Scale delta to quantization range
        scaled = int(delta * max_val / 1.0)  # Assuming delta range [-1, 1]
        
        # Clamp to valid range
        return max(min_val, min(max_val, scaled))
    
    def compress_audio_delta(self, audio: np.ndarray) -> Dict:
        """Compress audio using variable-length delta encoding."""
        
        print(f"Delta encoding {len(audio)} samples...")
        
        # Calculate deltas
        deltas = np.diff(audio)
        
        # Classify deltas by magnitude
        abs_deltas = np.abs(deltas)
        
        small_mask = abs_deltas < self.small_delta_threshold
        medium_mask = (abs_deltas >= self.small_delta_threshold) & (abs_deltas < self.medium_delta_threshold)
        large_mask = abs_deltas >= self.medium_delta_threshold
        
        small_deltas = deltas[small_mask]
        medium_deltas = deltas[medium_mask]  
        large_deltas = deltas[large_mask]
        
        print(f"  Small deltas (4-bit): {len(small_deltas)} ({len(small_deltas)/len(deltas)*100:.1f}%)")
        print(f"  Medium deltas (8-bit): {len(medium_deltas)} ({len(medium_deltas)/len(deltas)*100:.1f}%)")
        print(f"  Large deltas (12-bit): {len(large_deltas)} ({len(large_deltas)/len(deltas)*100:.1f}%)")
        
        # Encode deltas with variable bit allocation
        encoded_data = []
        
        # Store first sample as-is (16-bit reference)
        first_sample = int(audio[0] * 32767)  # Convert to 16-bit int
        encoded_data.append(('reference', 16, first_sample))
        
        # Encode deltas
        for i, delta in enumerate(deltas):
            abs_delta = abs(delta)
            
            if abs_delta < self.small_delta_threshold:
                quantized = self.quantize_delta(delta, 4)
                encoded_data.append(('small', 4, quantized))
            elif abs_delta < self.medium_delta_threshold:
                quantized = self.quantize_delta(delta, 8)
                encoded_data.append(('medium', 8, quantized))
            else:
                quantized = self.quantize_delta(delta, 12)
                encoded_data.append(('large', 12, quantized))
        
        # Calculate statistics
        total_bits = sum(bits for _, bits, _ in encoded_data)
        original_bits = len(audio) * 16  # Original 16-bit samples
        compression_ratio = original_bits / total_bits
        
        compression_info = {
            'original_samples': len(audio),
            'delta_samples': len(deltas),
            'original_bits': original_bits,
            'compressed_bits': total_bits,
            'compression_ratio': compression_ratio,
            'distribution': {
                'small_deltas': len(small_deltas),
                'medium_deltas': len(medium_deltas),
                'large_deltas': len(large_deltas)
            }
        }
        
        return {
            'encoded_data': encoded_data,
            'compression_info': compression_info,
            'decoder_params': {
                'first_sample': first_sample,
                'small_threshold': self.small_delta_threshold,
                'medium_threshold': self.medium_delta_threshold,
                'quantization_levels': self.quantization_levels
            }
        }
    
    def pack_encoded_data(self, encoded_data: List[Tuple], output_file: str):
        """Pack encoded data into binary format."""
        
        print(f"Packing encoded data to {output_file}...")
        
        with open(output_file, 'wb') as f:
            # Write header
            f.write(b'PGDD')  # Polygond Delta Data magic number
            f.write(struct.pack('<I', len(encoded_data)))  # Number of entries
            
            # Pack data efficiently
            bit_buffer = 0
            bits_in_buffer = 0
            
            for entry_type, bits, value in encoded_data:
                # Add value to bit buffer
                bit_buffer |= (value & ((1 << bits) - 1)) << bits_in_buffer
                bits_in_buffer += bits
                
                # Flush complete bytes
                while bits_in_buffer >= 8:
                    f.write(struct.pack('B', bit_buffer & 0xFF))
                    bit_buffer >>= 8
                    bits_in_buffer -= 8
            
            # Flush remaining bits
            if bits_in_buffer > 0:
                f.write(struct.pack('B', bit_buffer & 0xFF))
        
        file_size = Path(output_file).stat().st_size
        print(f"  Packed data size: {file_size:,} bytes ({file_size/1024/1024:.2f} MB)")
        
        return file_size
    
    def decode_delta_data(self, encoded_result: Dict) -> np.ndarray:
        """Decode delta-compressed data back to audio."""
        
        encoded_data = encoded_result['encoded_data']
        decoder_params = encoded_result['decoder_params']
        
        print("Decoding delta-compressed data...")
        
        # Start with first sample
        reconstructed = [decoder_params['first_sample'] / 32767.0]  # Convert back to float
        
        # Decode deltas
        for entry_type, bits, quantized_delta in encoded_data[1:]:  # Skip reference
            # Dequantize delta
            max_val = self.quantization_levels[bits] // 2 - 1
            delta = quantized_delta / max_val  # Convert back to [-1, 1] range
            
            # Reconstruct sample
            next_sample = reconstructed[-1] + delta
            reconstructed.append(next_sample)
        
        return np.array(reconstructed)

def process_audio_file(input_file: str, output_dir: str) -> Dict:
    """Process an audio file through delta compression."""
    
    input_path = Path(input_file)
    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    
    if not input_path.exists():
        print(f"Error: Input file {input_file} not found!")
        return None
    
    print(f"=== DELTA COMPRESSION: {input_path.name} ===")
    
    # Load audio
    try:
        audio, sr = sf.read(input_file, always_2d=False)
        if len(audio.shape) > 1:  # Convert stereo to mono
            audio = np.mean(audio, axis=1)
        print(f"Loaded: {len(audio)} samples, {len(audio)/sr:.1f}s @ {sr}Hz")
    except Exception as e:
        print(f"Error loading audio: {e}")
        return None
    
    # Initialize encoder
    encoder = DeltaEncoder()
    
    # Compress
    compressed_result = encoder.compress_audio_delta(audio)
    
    # Save compressed data
    track_name = input_path.stem
    compressed_file = output_dir / f"{track_name}.pgdd"
    file_size = encoder.pack_encoded_data(compressed_result['encoded_data'], str(compressed_file))
    
    # Test decode
    print("Testing decode quality...")
    decoded_audio = encoder.decode_delta_data(compressed_result)
    
    # Quality metrics
    if len(decoded_audio) == len(audio):
        mse = np.mean((audio - decoded_audio) ** 2)
        snr = 10 * np.log10(np.mean(audio ** 2) / mse) if mse > 0 else float('inf')
        max_error = np.max(np.abs(audio - decoded_audio))
        
        quality_metrics = {
            'mse': float(mse),
            'snr_db': float(snr),
            'max_error': float(max_error)
        }
    else:
        quality_metrics = {'error': 'Length mismatch'}
    
    # Save decoded version for comparison
    decoded_file = output_dir / f"{track_name}_decoded.wav"
    sf.write(decoded_file, decoded_audio, sr)
    
    # Calculate original file size for comparison
    original_size = input_path.stat().st_size
    actual_compression_ratio = original_size / file_size
    
    # Compile results
    results = {
        'input_file': str(input_path),
        'compressed_file': str(compressed_file),
        'decoded_file': str(decoded_file),
        'original_size_bytes': original_size,
        'compressed_size_bytes': file_size,
        'actual_compression_ratio': actual_compression_ratio,
        'theoretical_compression_ratio': compressed_result['compression_info']['compression_ratio'],
        'quality_metrics': quality_metrics,
        'compression_info': compressed_result['compression_info']
    }
    
    # Save results
    results_file = output_dir / f"{track_name}_delta_results.json"
    with open(results_file, 'w') as f:
        json.dump(results, f, indent=2)
    
    # Print summary
    print(f"\n=== DELTA COMPRESSION RESULTS ===")
    print(f"Original size: {original_size:,} bytes ({original_size/1024/1024:.2f} MB)")
    print(f"Compressed size: {file_size:,} bytes ({file_size/1024/1024:.2f} MB)")
    print(f"Compression ratio: {actual_compression_ratio:.2f}x")
    print(f"Space saved: {(1 - 1/actual_compression_ratio)*100:.1f}%")
    
    if 'snr_db' in quality_metrics:
        print(f"SNR: {quality_metrics['snr_db']:.1f} dB")
        print(f"Max error: {quality_metrics['max_error']:.6f}")
    
    print(f"Results saved: {results_file}")
    
    return results

def main():
    import sys
    if len(sys.argv) < 2:
        print("Usage: python delta_encoder.py <audio_file> [output_dir]")
        print("Example: python delta_encoder.py '../test_data/original/1 Crumbling Castle.wav' '../test_data/compressed'")
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_dir = sys.argv[2] if len(sys.argv) > 2 else '../test_data/compressed'
    
    results = process_audio_file(input_file, output_dir)
    
    if results:
        print(f"\nDelta compression complete! Check {output_dir} for results.")

if __name__ == "__main__":
    main()