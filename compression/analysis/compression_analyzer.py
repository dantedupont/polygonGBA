#!/usr/bin/env python3
"""
Compression Opportunity Assessment Tool
Analyzes delta compression potential, bit allocation, and overall compression strategy.
Combines frequency analysis and pattern detection results for optimal compression planning.
"""

import os
import sys
import json
import numpy as np
import librosa
import matplotlib.pyplot as plt
from pathlib import Path
from typing import Dict, List, Tuple, Optional

class CompressionAnalyzer:
    def __init__(self, sample_rate: int = 32000):
        self.sample_rate = sample_rate
        self.target_size_mb = 12
        self.target_compression_ratio = 44  # Need 44x compression for 526MB -> 12MB
        
    def load_analysis_results(self, analysis_file: str, pattern_file: str) -> Tuple[Dict, Dict]:
        """Load frequency analysis and pattern detection results."""
        
        frequency_results = None
        if Path(analysis_file).exists():
            with open(analysis_file, 'r') as f:
                frequency_results = json.load(f)
        
        pattern_results = None
        if Path(pattern_file).exists():
            with open(pattern_file, 'r') as f:
                pattern_results = json.load(f)
        
        return frequency_results, pattern_results
    
    def analyze_delta_compression(self, audio: np.ndarray) -> Dict:
        """Analyze potential for delta (differential) compression."""
        
        print("Analyzing delta compression potential...")
        
        # Calculate deltas between consecutive samples
        deltas = np.diff(audio)
        
        # Analyze delta distribution
        delta_stats = {
            'mean_delta': float(np.mean(np.abs(deltas))),
            'max_delta': float(np.max(np.abs(deltas))),
            'delta_variance': float(np.var(deltas)),
            'zero_deltas': int(np.sum(deltas == 0)),
            'small_deltas': int(np.sum(np.abs(deltas) < 0.01)),  # Very small changes
            'medium_deltas': int(np.sum((np.abs(deltas) >= 0.01) & (np.abs(deltas) < 0.1))),
            'large_deltas': int(np.sum(np.abs(deltas) >= 0.1))
        }
        
        # Calculate optimal bit allocation for delta encoding
        total_deltas = len(deltas)
        small_ratio = delta_stats['small_deltas'] / total_deltas
        medium_ratio = delta_stats['medium_deltas'] / total_deltas
        large_ratio = delta_stats['large_deltas'] / total_deltas
        
        # Estimate compression potential
        # Small deltas: 4 bits, Medium: 8 bits, Large: 12 bits
        avg_bits_per_delta = (small_ratio * 4 + medium_ratio * 8 + large_ratio * 12)
        delta_compression_ratio = 16 / avg_bits_per_delta  # vs original 16-bit samples
        
        delta_analysis = {
            'stats': delta_stats,
            'compression_potential': {
                'avg_bits_per_delta': avg_bits_per_delta,
                'compression_ratio': delta_compression_ratio,
                'space_savings_percent': (1 - 1/delta_compression_ratio) * 100
            },
            'bit_allocation': {
                'small_deltas_4bit': small_ratio,
                'medium_deltas_8bit': medium_ratio,  
                'large_deltas_12bit': large_ratio
            }
        }
        
        return delta_analysis
    
    def calculate_multi_stage_compression(self, frequency_results: Dict, 
                                        pattern_results: Dict, 
                                        delta_results: Dict) -> Dict:
        """Calculate overall compression combining all techniques."""
        
        print("Calculating multi-stage compression potential...")
        
        # Extract compression ratios from each technique
        freq_compression = 1.0
        if frequency_results and 'recommendations' in frequency_results:
            freq_strategy = frequency_results['recommendations']['frequency_strategy']
            # Calculate weighted average compression based on frequency importance
            total_compression = 0
            total_weight = 0
            
            for band, strategy in freq_strategy.items():
                importance = strategy['importance']
                compression = strategy['compression_factor']
                total_compression += importance * compression
                total_weight += importance
            
            freq_compression = total_compression / total_weight if total_weight > 0 else 1.0
        
        # Pattern compression
        pattern_compression = 1.0
        if pattern_results and 'album_summary' in pattern_results:
            pattern_compression = pattern_results['album_summary']['pattern_compression_ratio']
        
        # Delta compression
        delta_compression = delta_results['compression_potential']['compression_ratio']
        
        # Combined compression (multiplicative - each stage builds on the last)
        combined_compression = freq_compression * pattern_compression * delta_compression
        
        # Calculate final estimates
        if frequency_results and 'recommendations' in frequency_results:
            original_duration = frequency_results['recommendations']['album_stats']['total_duration']
            original_size_mb = frequency_results['recommendations']['album_stats']['current_size_estimate_mb']
        else:
            original_duration = 600  # Estimate
            original_size_mb = 40    # Estimate
        
        final_size_mb = original_size_mb / combined_compression
        
        multi_stage_analysis = {
            'compression_stages': {
                'frequency_domain': freq_compression,
                'pattern_based': pattern_compression,
                'delta_encoding': delta_compression,
                'combined': combined_compression
            },
            'size_estimates': {
                'original_mb': original_size_mb,
                'after_frequency_mb': original_size_mb / freq_compression,
                'after_pattern_mb': original_size_mb / (freq_compression * pattern_compression),
                'final_size_mb': final_size_mb,
                'target_size_mb': self.target_size_mb
            },
            'target_achievement': {
                'meets_target': final_size_mb <= self.target_size_mb,
                'compression_ratio_needed': self.target_compression_ratio,
                'compression_ratio_achieved': combined_compression,
                'gap_ratio': self.target_compression_ratio / combined_compression if combined_compression > 0 else float('inf')
            }
        }
        
        return multi_stage_analysis
    
    def recommend_optimization_strategy(self, multi_stage_results: Dict) -> List[str]:
        """Recommend optimization strategies based on analysis."""
        
        recommendations = []
        
        target_met = multi_stage_results['target_achievement']['meets_target']
        gap_ratio = multi_stage_results['target_achievement']['gap_ratio']
        
        if target_met:
            recommendations.append("✓ Target compression ratio achievable with current strategy!")
        else:
            recommendations.append(f"⚠ Additional {gap_ratio:.1f}x compression needed")
            
            # Suggest improvements based on gap size
            if gap_ratio > 5:
                recommendations.append("• Consider more aggressive frequency quantization")
                recommendations.append("• Implement advanced psychoacoustic modeling")
                recommendations.append("• Add temporal redundancy reduction")
            elif gap_ratio > 2:
                recommendations.append("• Optimize pattern detection for smaller segments")
                recommendations.append("• Implement variable bit-rate delta encoding")
                recommendations.append("• Consider spectral compression techniques")
            else:
                recommendations.append("• Fine-tune existing parameters")
                recommendations.append("• Optimize encoder efficiency")
        
        # Stage-specific recommendations
        stages = multi_stage_results['compression_stages']
        if stages['pattern_based'] < 1.5:
            recommendations.append("• Pattern compression underperforming - check similarity thresholds")
        
        if stages['delta_encoding'] < 2.0:
            recommendations.append("• Delta compression could be improved - analyze sample correlation")
        
        if stages['frequency_domain'] < 3.0:
            recommendations.append("• Frequency compression conservative - consider more aggressive quantization")
        
        return recommendations
    
    def generate_compression_report(self, output_dir: str, analysis_file: str, 
                                  pattern_file: str, audio_file: str) -> Dict:
        """Generate comprehensive compression analysis report."""
        
        output_dir = Path(output_dir)
        output_dir.mkdir(parents=True, exist_ok=True)
        
        print(f"=== COMPRESSION ANALYSIS REPORT ===")
        print(f"Input files:")
        print(f"  Frequency analysis: {analysis_file}")
        print(f"  Pattern analysis: {pattern_file}")
        print(f"  Audio file: {audio_file}")
        
        # Load previous analysis results
        frequency_results, pattern_results = self.load_analysis_results(analysis_file, pattern_file)
        
        if not frequency_results:
            print("Error: Frequency analysis results not found!")
            return None
        
        # Load audio for delta analysis
        audio = None
        if Path(audio_file).exists():
            try:
                audio, _ = librosa.load(audio_file, sr=self.sample_rate, mono=True)
                print(f"Loaded audio: {len(audio)} samples, {len(audio)/self.sample_rate:.1f}s")
            except Exception as e:
                print(f"Error loading audio: {e}")
        
        # Perform delta compression analysis
        delta_results = None
        if audio is not None:
            delta_results = self.analyze_delta_compression(audio)
        else:
            # Use synthetic delta analysis if audio not available
            print("Using estimated delta compression values...")
            delta_results = {
                'compression_potential': {'compression_ratio': 2.5},
                'stats': {},
                'bit_allocation': {}
            }
        
        # Calculate multi-stage compression
        multi_stage_results = self.calculate_multi_stage_compression(
            frequency_results, pattern_results, delta_results
        )
        
        # Generate recommendations
        recommendations = self.recommend_optimization_strategy(multi_stage_results)
        
        # Compile final report
        report = {
            'analysis_summary': {
                'frequency_analysis': frequency_results is not None,
                'pattern_analysis': pattern_results is not None,
                'delta_analysis': delta_results is not None,
                'audio_analyzed': audio is not None
            },
            'compression_breakdown': multi_stage_results,
            'delta_analysis': delta_results,
            'optimization_recommendations': recommendations,
            'target_metrics': {
                'target_size_mb': self.target_size_mb,
                'target_compression_ratio': self.target_compression_ratio,
                'album_size_estimate': '526MB (full Polygondwanaland)'
            }
        }
        
        # Save report
        report_path = output_dir / 'compression_analysis_report.json'
        with open(report_path, 'w') as f:
            json.dump(report, f, indent=2)
        
        # Print summary
        print(f"\n=== COMPRESSION SUMMARY ===")
        stages = multi_stage_results['compression_stages']
        print(f"Frequency compression: {stages['frequency_domain']:.1f}x")
        print(f"Pattern compression: {stages['pattern_based']:.1f}x") 
        print(f"Delta compression: {stages['delta_encoding']:.1f}x")
        print(f"Combined compression: {stages['combined']:.1f}x")
        
        sizes = multi_stage_results['size_estimates']
        print(f"\nSize progression:")
        print(f"Original: {sizes['original_mb']:.1f} MB")
        print(f"Final: {sizes['final_size_mb']:.1f} MB")
        print(f"Target: {sizes['target_size_mb']:.1f} MB")
        
        target = multi_stage_results['target_achievement']
        if target['meets_target']:
            print("✓ TARGET ACHIEVED!")
        else:
            print(f"⚠ Need {target['gap_ratio']:.1f}x more compression")
        
        print(f"\nFull report saved: {report_path}")
        
        return report

def main():
    if len(sys.argv) < 4:
        print("Usage: python compression_analyzer.py <frequency_analysis.json> <pattern_analysis.json> <audio_file> [output_dir]")
        print("Example: python compression_analyzer.py ../test_data/analysis_output/polygondwanaland_analysis.json ../test_data/pattern_output/pattern_analysis_results.json ../test_data/original/1\\ Crumbling\\ Castle.wav")
        sys.exit(1)
    
    analysis_file = sys.argv[1]
    pattern_file = sys.argv[2] 
    audio_file = sys.argv[3]
    output_dir = sys.argv[4] if len(sys.argv) > 4 else './compression_report'
    
    analyzer = CompressionAnalyzer(sample_rate=32000)
    report = analyzer.generate_compression_report(output_dir, analysis_file, pattern_file, audio_file)
    
    if report:
        recommendations = report['optimization_recommendations']
        print(f"\n=== RECOMMENDATIONS ===")
        for rec in recommendations:
            print(rec)

if __name__ == "__main__":
    main()