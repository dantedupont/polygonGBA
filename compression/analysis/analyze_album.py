#!/usr/bin/env python3
"""
Polygondwanaland Album Frequency Analysis Tool
Analyzes King Gizzard's Polygondwanaland for optimal compression strategy.
Identifies critical frequency ranges for visual synthesis on GBA.
"""

import os
import sys
import librosa
import numpy as np
import matplotlib.pyplot as plt
from pathlib import Path
from typing import Dict, List, Tuple, Optional
import json

class PolygondAnalyzer:
    def __init__(self, sample_rate: int = 32000):
        """
        Initialize analyzer with GBA-optimized sample rate.
        GBA can handle up to ~32kHz effectively.
        """
        self.sample_rate = sample_rate
        self.frequency_bands = {
            'sub_bass': (20, 60),      # Sub-bass rumble
            'bass': (60, 200),         # Critical for visual sync
            'low_mid': (200, 500),     # Fundamental tones
            'mid': (500, 1000),        # Vocal/instrument clarity
            'high_mid': (1000, 4000),  # Presence/attack
            'high': (4000, 8000),      # Brilliance/air
            'ultra_high': (8000, 16000) # Sparkling highs
        }
        
        self.frequency_importance = {
            'sub_bass': 0.4,      
            'bass': 1.0,         
            'low_mid': 0.8,     
            'mid': 0.9,        
            'high_mid': 0.6,  
            'high': 0.4,      
            'ultra_high': 0.2 
        }
        
        self.analysis_results = {}
    
    def load_audio_file(self, file_path: str) -> Tuple[np.ndarray, int]:
        """Load audio file and resample to target rate."""
        try:
            audio, original_sr = librosa.load(file_path, sr=self.sample_rate, mono=True)
            print(f"Loaded {file_path}: {len(audio)/self.sample_rate:.1f}s @ {self.sample_rate}Hz")
            return audio, original_sr
        except Exception as e:
            print(f"Error loading {file_path}: {e}")
            return None, None
    
    def analyze_frequency_spectrum(self, audio: np.ndarray, track_name: str) -> Dict:
        """Perform FFT analysis and extract frequency band characteristics."""
        
        # Compute STFT for time-frequency analysis
        stft = librosa.stft(audio, hop_length=1024, n_fft=2048)
        magnitude = np.abs(stft)
        
        # Convert to frequency bins
        freqs = librosa.fft_frequencies(sr=self.sample_rate, n_fft=2048)
        
        # Analyze energy in each frequency band
        band_analysis = {}
        
        for band_name, (low_freq, high_freq) in self.frequency_bands.items():
            # Find frequency bin indices for this band
            freq_mask = (freqs >= low_freq) & (freqs <= high_freq)
            
            if np.any(freq_mask):
                # Extract magnitude data for this frequency band
                band_magnitude = magnitude[freq_mask, :]
                
                # Calculate statistics
                mean_energy = np.mean(band_magnitude)
                peak_energy = np.max(band_magnitude)
                energy_variance = np.var(band_magnitude)
                
                # Calculate dynamic range (important for compression)
                dynamic_range = peak_energy / (mean_energy + 1e-10)
                
                band_analysis[band_name] = {
                    'freq_range': (low_freq, high_freq),
                    'mean_energy': float(mean_energy),
                    'peak_energy': float(peak_energy),
                    'energy_variance': float(energy_variance),
                    'dynamic_range': float(dynamic_range),
                    'bin_count': int(np.sum(freq_mask))
                }
        
        # Overall spectral characteristics
        spectral_centroid = librosa.feature.spectral_centroid(y=audio, sr=self.sample_rate)[0]
        spectral_rolloff = librosa.feature.spectral_rolloff(y=audio, sr=self.sample_rate)[0]
        
        analysis = {
            'track_name': track_name,
            'duration': len(audio) / self.sample_rate,
            'frequency_bands': band_analysis,
            'spectral_centroid_mean': float(np.mean(spectral_centroid)),
            'spectral_rolloff_mean': float(np.mean(spectral_rolloff)),
            'overall_energy': float(np.mean(magnitude))
        }
        
        return analysis
    
    def generate_frequency_charts(self, analysis: Dict, output_dir: str):
        """Generate visualization charts for frequency analysis."""
        track_name = analysis['track_name']
        
        # Extract band data for plotting
        bands = list(self.frequency_bands.keys())
        mean_energies = [analysis['frequency_bands'][band]['mean_energy'] for band in bands]
        dynamic_ranges = [analysis['frequency_bands'][band]['dynamic_range'] for band in bands]
        
        # Create subplots
        fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 8))
        
        # Plot 1: Mean energy per frequency band
        ax1.bar(bands, mean_energies, color='skyblue', alpha=0.7)
        ax1.set_title(f'{track_name} - Mean Energy by Frequency Band')
        ax1.set_ylabel('Mean Energy')
        ax1.tick_params(axis='x', rotation=45)
        
        # Plot 2: Dynamic range per frequency band
        ax2.bar(bands, dynamic_ranges, color='lightcoral', alpha=0.7)
        ax2.set_title(f'{track_name} - Dynamic Range by Frequency Band')
        ax2.set_ylabel('Dynamic Range (Peak/Mean)')
        ax2.tick_params(axis='x', rotation=45)
        
        plt.tight_layout()
        
        # Save chart
        chart_path = os.path.join(output_dir, f'{track_name}_frequency_analysis.png')
        plt.savefig(chart_path, dpi=150, bbox_inches='tight')
        plt.close()
        
        print(f"Frequency chart saved: {chart_path}")
    
    def recommend_compression_strategy(self, all_analyses: List[Dict]) -> Dict:
        """Generate compression strategy recommendations based on analysis."""
        
        # Aggregate statistics across all tracks
        total_duration = sum(analysis['duration'] for analysis in all_analyses)
        
        # Calculate average characteristics per band
        band_stats = {}
        for band_name in self.frequency_bands.keys():
            energies = [analysis['frequency_bands'][band_name]['mean_energy'] 
                       for analysis in all_analyses]
            ranges = [analysis['frequency_bands'][band_name]['dynamic_range'] 
                     for analysis in all_analyses]
            
            band_stats[band_name] = {
                'avg_energy': np.mean(energies),
                'avg_dynamic_range': np.mean(ranges),
                'importance_weight': self.frequency_importance.get(band_name, 0.5)
            }
        
        # Generate recommendations
        recommendations = {
            'album_stats': {
                'total_duration': total_duration,
                'track_count': len(all_analyses),
                'target_size_mb': 12,
                'current_size_estimate_mb': total_duration * self.sample_rate * 2 / (1024 * 1024),  # 16-bit stereo
                'compression_ratio_needed': (total_duration * self.sample_rate * 2 / (1024 * 1024)) / 12
            },
            'frequency_strategy': {},
            'recommended_sample_rate': self.sample_rate
        }
        
        # Generate per-band compression recommendations
        for band_name, stats in band_stats.items():
            importance = stats['importance_weight']
            dynamic_range = stats['avg_dynamic_range']
            
            # Higher importance = less compression
            # Higher dynamic range = needs more bits
            if importance > 0.8:
                bit_allocation = 12  # High quality
                compression_factor = 2
            elif importance > 0.6:
                bit_allocation = 8   # Medium quality
                compression_factor = 4
            elif importance > 0.4:
                bit_allocation = 6   # Low quality
                compression_factor = 8
            else:
                bit_allocation = 4   # Very low quality
                compression_factor = 16
            
            recommendations['frequency_strategy'][band_name] = {
                'importance': importance,
                'recommended_bits': bit_allocation,
                'compression_factor': compression_factor,
                'preserve_ratio': importance
            }
        
        return recommendations
    
    def analyze_directory(self, audio_dir: str, output_dir: str) -> Dict:
        """Analyze all audio files in directory."""
        audio_dir = Path(audio_dir)
        output_dir = Path(output_dir)
        output_dir.mkdir(parents=True, exist_ok=True)
        
        supported_formats = ['.wav', '.mp3', '.flac', '.m4a']
        audio_files = []
        
        for fmt in supported_formats:
            audio_files.extend(audio_dir.glob(f'*{fmt}'))
            audio_files.extend(audio_dir.glob(f'*{fmt.upper()}'))
        
        if not audio_files:
            print(f"No audio files found in {audio_dir}")
            print(f"Supported formats: {supported_formats}")
            return None
        
        print(f"Found {len(audio_files)} audio files")
        
        all_analyses = []
        
        for file_path in audio_files:
            print(f"\nAnalyzing: {file_path.name}")
            
            audio, original_sr = self.load_audio_file(str(file_path))
            if audio is None:
                continue
                
            track_name = file_path.stem
            analysis = self.analyze_frequency_spectrum(audio, track_name)
            
            # Generate charts
            self.generate_frequency_charts(analysis, str(output_dir))
            
            all_analyses.append(analysis)
        
        # Generate overall recommendations
        if all_analyses:
            recommendations = self.recommend_compression_strategy(all_analyses)
            
            # Save results
            results = {
                'analyses': all_analyses,
                'recommendations': recommendations,
                'analyzer_config': {
                    'sample_rate': self.sample_rate,
                    'frequency_bands': self.frequency_bands,
                    'frequency_importance': self.frequency_importance
                }
            }
            
            results_path = output_dir / 'polygondwanaland_analysis.json'
            with open(results_path, 'w') as f:
                json.dump(results, f, indent=2)
            
            print(f"\nAnalysis complete! Results saved to: {results_path}")
            return results
        
        return None

def main():
    if len(sys.argv) < 2:
        print("Usage: python analyze_album.py <audio_directory> [output_directory]")
        print("Example: python analyze_album.py ./test_data/original ./analysis_output")
        sys.exit(1)
    
    audio_dir = sys.argv[1]
    output_dir = sys.argv[2] if len(sys.argv) > 2 else './analysis_output'
    
    analyzer = PolygondAnalyzer(sample_rate=32000)
    results = analyzer.analyze_directory(audio_dir, output_dir)
    
    if results:
        print("\n=== ANALYSIS SUMMARY ===")
        recs = results['recommendations']
        print(f"Album duration: {recs['album_stats']['total_duration']:.1f} seconds")
        print(f"Compression needed: {recs['album_stats']['compression_ratio_needed']:.1f}x")
        print(f"Target size: {recs['album_stats']['target_size_mb']} MB")
        
        print("\nFrequency band strategy:")
        for band, strategy in recs['frequency_strategy'].items():
            print(f"  {band:12s}: {strategy['recommended_bits']:2d} bits "
                  f"({strategy['compression_factor']:2d}x compression)")

if __name__ == "__main__":
    main()