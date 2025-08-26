#ifndef EIGHTAD_PLAYER_H
#define EIGHTAD_PLAYER_H

// Initialize 8AD audio system
void init_8ad_sound(void);

// Track control
void start_8ad_track(int track_num);
void next_track_8ad(void);
void prev_track_8ad(void);
void toggle_pause_8ad(void); // Pause/resume playback

// Audio processing
void mixer_8ad(void);
void audio_vblank_8ad(void);

// Status
int get_current_track_8ad(void);
int is_playing_8ad(void);

// Real frequency analysis for visualizer
extern long spectrum_accumulators_8ad[8];
extern int spectrum_sample_count_8ad;

// Simple frequency filters for real frequency analysis
extern int bass_filter_state;
extern int mid_filter_state;
extern int treble_filter_state;

// Debug unit tests
int test_gbfs_access(char* result_buffer);
int test_8ad_decoder(char* result_buffer);
int test_track_data_format(char* result_buffer);
int test_real_decode(char* result_buffer);

#endif // EIGHTAD_PLAYER_H