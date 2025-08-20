#ifndef LIBGSM_H
#define LIBGSM_H

typedef struct GsmPlaybackTracker
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

  // these vars should be initialized together
  char curr_song_name[65];
  unsigned int curr_song_name_len;
  int marquee_offset;
  unsigned int frames_until_marquee_update;
  u16 reel_rotation_theta;
  
  // Spectrum analyzer accumulators (8 frequency bands)
  long spectrum_accumulators[8];
  int spectrum_sample_count;
  
  // Bouncy bar physics (8 bars)
  int bar_current_heights[8];  // Current display heights
  int bar_target_heights[8];   // Target heights from audio data
  int bar_velocities[8];       // Falling velocity for smooth decay
} GsmPlaybackTracker;

typedef struct GsmPlaybackInputMapping
{
  int TOGGLE_PLAY_PAUSE;
  int PREV_TRACK;
  int NEXT_TRACK;
  int SEEK_BACK;
  int SEEK_FORWARD;
  int TOGGLE_LOCK;
} GsmPlaybackInputMapping;

extern struct GsmPlaybackInputMapping DEFAULT_PLAYBACK_INPUT_MAPPING;

int initPlayback(GsmPlaybackTracker *playback);

void advancePlayback(GsmPlaybackTracker *playback, GsmPlaybackInputMapping *mapping);

void writeFromPlaybackBuffer(GsmPlaybackTracker *playback);

#endif
