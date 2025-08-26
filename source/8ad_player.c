
#include <gba.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "gbfs.h"
#include "8ad_decoder.h"
#include "spectrum_visualizer.h" // For reset function

// 8AD system constants (from Pin Eight's implementation)
#define MIXBUF_SIZE 304
#define AUDIO_FRAME_BYTES (MIXBUF_SIZE >> 1)  // 152 bytes per frame

// Audio buffers
static int cur_mixbuf = 0;
static signed char mixbuf[2][MIXBUF_SIZE];

// 8AD decoder state
static ADGlobals ad;

// GBFS filesystem
extern const GBFS_FILE *fs;

// Track management
static int current_track = 0;
static int playing = 0;
static int paused = 0; // Pause state - audio loaded but not playing
static const unsigned char *track_data_start;
static const unsigned char *track_data_end;
static int auto_advanced = 0; // Flag to prevent multiple auto-advances per track

// Real frequency analysis for visualizer  
long spectrum_accumulators_8ad[8] = {0};
int spectrum_sample_count_8ad = 0;

// Simple frequency filters for real frequency analysis
int bass_filter_state = 0;
int mid_filter_state = 0;
int treble_filter_state = 0;

// Forward declaration for auto-advance
void next_track_8ad(void);

// Audio system (simplified from Pin Eight)
static void dsound_switch_buffers(const signed char *src)
{
  REG_DMA1CNT = 0;
  
  // no-op to let DMA registers catch up
  asm volatile ("eor r0, r0; eor r0, r0" ::: "r0");
  
  REG_DMA1SAD = (intptr_t)src;
  REG_DMA1DAD = (intptr_t)0x040000a0; // write to FIFO A address
  REG_DMA1CNT = DMA_DST_FIXED | DMA_SRC_INC | DMA_REPEAT | DMA32 |
                DMA_SPECIAL | DMA_ENABLE | 1;
}

void init_8ad_sound(void)
{
  REG_TM0CNT_H = 0;
  SETSNDRES(1);
  SNDSTAT = SNDSTAT_ENABLE;
  DSOUNDCTRL = 0x0b0e;
  REG_TM0CNT_L = 0x10000 - 924;  // Timer for ~18.157 kHz (Pin Eight's rate)
  REG_TM0CNT_H = 0x80;  // Timer enable
}

void start_8ad_track(int track_num)
{
  if (!fs) return;
  
  u32 track_len;
  const unsigned char *track_data = gbfs_get_nth_obj(fs, track_num, NULL, &track_len);
  
  if (track_data) {
    ad.data = track_data;
    ad.last_sample = 0;
    ad.last_index = 0;
    
    track_data_start = track_data;
    track_data_end = track_data + track_len;
    current_track = track_num;
    playing = 1;
    paused = 0; // Reset pause state for new track
    auto_advanced = 0; // Reset auto-advance flag for new track
    
    // CRITICAL: Clear spectrum accumulators during track change to prevent bar sticking
    for(int i = 0; i < 7; i++) {
      spectrum_accumulators_8ad[i] = 0;
    }
    spectrum_sample_count_8ad = 0;
    
    // CRITICAL: Also reset spectrum visualizer state to clear Bar[7] and others immediately
    reset_spectrum_visualizer_state();
    
    // Debug: Log first few bytes of track data to verify 8AD format
    // We'll add unit tests to validate this data
  }
}

// Pin Eight's mixer approach - simple and direct
void mixer_8ad(void)
{
  // Handle pause state first
  if (paused) {
    // Fill with silence when paused
    memset(mixbuf[cur_mixbuf], 0, MIXBUF_SIZE);
    return;
  }
  
  // Check if track has ended and auto-advance to next track
  if (playing && ad.data && ad.data >= track_data_end && !auto_advanced) {
    // DEBUG: Store auto-advance trigger info in palette for debugging
    SPRITE_PALETTE[30] = 0xABCD; // Auto-advance triggered marker
    SPRITE_PALETTE[31] = current_track; // Which track was ending
    
    auto_advanced = 1; // Prevent multiple auto-advances
    next_track_8ad(); // Automatically advance to next track
    return; // Let the next mixer call handle the new track
  }
  
  if (!playing || !ad.data) {
    // Fill with silence if not playing or no data
    memset(mixbuf[cur_mixbuf], 0, MIXBUF_SIZE);
    return;
  }
  
  // Check if we have enough data left for a full frame
  if (ad.data + AUDIO_FRAME_BYTES <= track_data_end) {
    // Decode exactly MIXBUF_SIZE samples from AUDIO_FRAME_BYTES bytes
    decode_ad(&ad, mixbuf[cur_mixbuf], ad.data, MIXBUF_SIZE);
    ad.data += AUDIO_FRAME_BYTES;
    
    // DEBUG: Track progress info for debugging auto-advance
    u32 track_length = track_data_end - track_data_start;
    u32 current_position = ad.data - track_data_start;
    SPRITE_PALETTE[28] = (track_length >> 16) & 0xFFFF; // Track length high
    SPRITE_PALETTE[29] = (current_position >> 12) & 0xFFFF; // Position (scaled down)
    
    // FAST balanced frequency analysis - optimized for GBA performance
    static int low_pass = 0; // Single low-pass for bass
    static int prev_sample = 0;
    
    for (int i = 0; i < MIXBUF_SIZE; i++) {
      int sample = mixbuf[cur_mixbuf][i];
      int abs_sample = (sample < 0) ? -sample : sample;
      
      // Simple but effective filtering
      low_pass += (sample - low_pass) >> 3; // Bass filter
      int bass_content = (low_pass < 0) ? -low_pass : low_pass;
      int treble_content = (sample - prev_sample);
      if (treble_content < 0) treble_content = -treble_content;
      
      prev_sample = sample;
      
      // FAST balanced distribution - keep the energy spread but simpler
      
      // Bass bands get bass content + baseline boost
      spectrum_accumulators_8ad[0] += bass_content + (bass_content >> 1) + (abs_sample >> 6); // Sub-bass
      spectrum_accumulators_8ad[1] += bass_content + (abs_sample >> 5); // Bass
      
      // Bar[2]: Enhanced bass-mid for symmetrical spectrum response
      spectrum_accumulators_8ad[2] += abs_sample + (bass_content >> 1) + (treble_content >> 2); // Full raw + bass + treble mix
      
      // Bar[3] and Bar[4]: AGGRESSIVE guitar/vocal frequency targeting
      spectrum_accumulators_8ad[3] += abs_sample + (treble_content >> 1); // Low-mid: DOUBLE treble content
      spectrum_accumulators_8ad[4] += abs_sample + treble_content; // Mid: FULL treble content for maximum guitar response
      
      // Bar[5] and Bar[6]: MAXIMUM high-frequency sensitivity for cymbals/hi-hats  
      spectrum_accumulators_8ad[5] += abs_sample + treble_content + (treble_content >> 1); // High-mid: raw + 150% treble
      spectrum_accumulators_8ad[6] += abs_sample + (abs_sample >> 3); // Treble: gentle processing like Bar[0] but with actual sample data
      
      // Simple baseline activity for 7 bands
      int baseline = abs_sample >> 7;
      for(int i = 0; i < 7; i++) {
        spectrum_accumulators_8ad[i] += baseline;
      }
    }
    spectrum_sample_count_8ad += MIXBUF_SIZE;
  } else {
    // End of track reached - trigger auto-advance if not already done
    if (!auto_advanced) {
      // DEBUG: Store end-of-track auto-advance trigger
      SPRITE_PALETTE[30] = 0xBEEF; // End-of-track auto-advance marker
      SPRITE_PALETTE[31] = current_track; // Which track was ending
      
      auto_advanced = 1;
      next_track_8ad(); // Automatically advance to next track
      return; // Let the next mixer call handle the new track
    }
    
    // Fill with silence if auto-advance already happened or failed
    memset(mixbuf[cur_mixbuf], 0, MIXBUF_SIZE);
  }
}

void audio_vblank_8ad(void)
{
  dsound_switch_buffers(mixbuf[cur_mixbuf]);
  cur_mixbuf = !cur_mixbuf;
}

// Simple control functions
void next_track_8ad(void)
{
  if (fs) {
    int total_tracks = gbfs_count_objs(fs);
    current_track = (current_track + 1) % total_tracks;
    start_8ad_track(current_track);
  }
}

void prev_track_8ad(void)
{
  if (fs) {
    int total_tracks = gbfs_count_objs(fs);
    current_track = (current_track - 1 + total_tracks) % total_tracks;
    start_8ad_track(current_track);
  }
}

void toggle_pause_8ad(void)
{
  if (playing) {
    paused = !paused; // Toggle pause state
    
    // Clear spectrum data when pausing to stop visualizations immediately
    if (paused) {
      for(int i = 0; i < 7; i++) {
        spectrum_accumulators_8ad[i] = 0;
      }
      spectrum_sample_count_8ad = 0;
      
      // Also reset spectrum visualizer state for immediate visual feedback
      reset_spectrum_visualizer_state();
    }
  }
}


int get_current_track_8ad(void)
{
  return current_track;
}

int is_playing_8ad(void)
{
  return playing;
}

// ========== DEBUG UNIT TESTS ==========

// Test 1: GBFS file system integrity
int test_gbfs_access(char* result_buffer)
{
  if (!fs) {
    sprintf(result_buffer, "FAIL: No GBFS");
    return 0;
  }
  
  int total = gbfs_count_objs(fs);
  u32 track_len;
  const unsigned char *track_data = gbfs_get_nth_obj(fs, 0, NULL, &track_len);
  
  if (!track_data || track_len == 0) {
    sprintf(result_buffer, "FAIL: No track data");
    return 0;
  }
  
  sprintf(result_buffer, "PASS: %d tracks, T0=%d bytes", total, track_len);
  return 1;
}

// Test 2: 8AD decoder with synthetic data - now with detailed debugging
int test_8ad_decoder(char* result_buffer)
{
  // Create simple test pattern: single byte 0x34 (nibbles 4 and 3)
  unsigned char test_input[1] = {0x34};
  signed char test_output[2]; // 2 samples from 1 byte
  
  // Create test decoder state
  ADGlobals test_decoder;
  test_decoder.last_sample = 0;
  test_decoder.last_index = 40; // Start with mid-range index
  
  // Clear output buffer
  test_output[0] = 99; // Sentinel values to detect if decode actually writes
  test_output[1] = 99;
  
  // Decode just 2 samples to trace exactly what happens
  decode_ad(&test_decoder, test_output, test_input, 2);
  
  // Show what actually happened
  sprintf(result_buffer, "IN:34 OUT:%d,%d IDX:%d", 
          test_output[0], test_output[1], test_decoder.last_index);
  
  return (test_output[0] != 99 || test_output[1] != 99) ? 1 : 0;
}

// Test 3: Real track data format validation
int test_track_data_format(char* result_buffer)
{
  if (!fs) {
    sprintf(result_buffer, "FAIL: No GBFS");
    return 0;
  }
  
  u32 track_len;
  const unsigned char *track_data = gbfs_get_nth_obj(fs, 0, NULL, &track_len);
  
  if (!track_data) {
    sprintf(result_buffer, "FAIL: No track");
    return 0;
  }
  
  // Sample first 8 bytes and show their hex values
  sprintf(result_buffer, "DATA: %02X%02X%02X%02X %02X%02X%02X%02X", 
          track_data[0], track_data[1], track_data[2], track_data[3],
          track_data[4], track_data[5], track_data[6], track_data[7]);
  return 1;
}

// Test 4: Decoder with real track data (small sample) - FIXED
int test_real_decode(char* result_buffer)
{
  if (!fs) {
    sprintf(result_buffer, "FAIL: No GBFS");
    return 0;
  }
  
  u32 track_len;
  const unsigned char *track_data = gbfs_get_nth_obj(fs, 0, NULL, &track_len);
  
  if (!track_data || track_len < 8) {
    sprintf(result_buffer, "FAIL: No data");
    return 0;
  }
  
  // Decode first 4 samples from real track (2 bytes input)
  signed char real_output[4];
  ADGlobals test_decoder;
  test_decoder.last_sample = 0;
  test_decoder.last_index = 40; // Same as synthetic test
  
  // Clear output to detect if decode runs
  real_output[0] = real_output[1] = real_output[2] = real_output[3] = 88;
  
  decode_ad(&test_decoder, real_output, track_data, 4);
  
  // Show results and debug info
  sprintf(result_buffer, "%d,%d,%d,%d I:%d", 
          real_output[0], real_output[1], real_output[2], real_output[3], 
          test_decoder.last_index);
  return 1;
}

