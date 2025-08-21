#ifndef AUDIO_CODEC_H
#define AUDIO_CODEC_H

// Define which codec to use
// #define USE_8AD  // Uncomment for 8AD codec (6% CPU, larger files)
// #define USE_GSM  // Uncomment for GSM codec (70% CPU, smaller files)

// Default to GSM if nothing specified
#if !defined(USE_8AD) && !defined(USE_GSM)
#define USE_GSM
#endif

#ifdef USE_8AD
#include "8ad_decoder.h"
#define AUDIO_FRAME_SIZE 152  // 8AD processes 152 bytes to produce 304 samples  
#define AUDIO_SAMPLES_PER_FRAME 304  // 8AD produces 304 samples per frame (same as Pin Eight)
#define AUDIO_FILE_EXTENSION ".ad"
#endif

#ifdef USE_GSM
#include "gsm.h"
#define AUDIO_FRAME_SIZE 33  // GSM frame size
#define AUDIO_SAMPLES_PER_FRAME 160  // GSM produces 160 samples per frame
#define AUDIO_FILE_EXTENSION ".gsm"
#endif

// Universal audio playback tracker
typedef struct AudioPlaybackTracker
{
  const unsigned char *src_start_pos;
  const unsigned char *src_pos;
  const unsigned char *src_end;
  unsigned int decode_pos;
  unsigned int cur_buffer;
  unsigned short last_joy;
  unsigned int cur_song;
  int last_sample;
  int playing;
  int locked;

  // Track info
  char curr_song_name[65];
  unsigned int curr_song_name_len;
  int marquee_offset;

  // Spectrum analyzer (universal)
  long spectrum_accumulators[8];
  int spectrum_sample_count;
  int bar_current_heights[8];
  int bar_target_heights[8];
  int bar_velocities[8];

#ifdef USE_8AD
  // 8AD specific state
  int ad_last_sample;
  int ad_last_index;
#endif

#ifdef USE_GSM
  // GSM specific state (can reference existing fields)
#endif

} AudioPlaybackTracker;

// Universal function prototypes
int initAudioPlayback(AudioPlaybackTracker *playback);
void advanceAudioPlayback(AudioPlaybackTracker *playback, void *input_mapping);
void writeFromAudioPlaybackBuffer(AudioPlaybackTracker *playback);

#endif