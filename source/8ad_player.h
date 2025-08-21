#ifndef EIGHTAD_PLAYER_H
#define EIGHTAD_PLAYER_H

#if defined(USE_8AD) || defined(CLEAN_8AD)

// Initialize 8AD audio system
void init_8ad_sound(void);

// Track control
void start_8ad_track(int track_num);
void next_track_8ad(void);
void prev_track_8ad(void);

// Audio processing
void mixer_8ad(void);
void audio_vblank_8ad(void);

// Status
int get_current_track_8ad(void);
int is_playing_8ad(void);

// Debug unit tests
int test_gbfs_access(char* result_buffer);
int test_8ad_decoder(char* result_buffer);
int test_track_data_format(char* result_buffer);
int test_real_decode(char* result_buffer);

#endif // USE_8AD || CLEAN_8AD

#endif // EIGHTAD_PLAYER_H