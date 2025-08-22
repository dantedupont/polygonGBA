#ifdef USE_8AD

#include <gba_base.h>
#include <gba_input.h>
#include <gba_interrupt.h>
#include <gba_systemcalls.h>
#include <gba_compression.h>
#include <gba_dma.h>
#include <gba_sound.h>
#include <gba_timers.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "audio_codec.h"
#include "gbfs.h"
#include "8ad_decoder.h"
#include "libgsm.h"

// Audio system initialization (same for both codecs)
static void dsound_switch_buffers(const void *src)
{
  REG_DMA1CNT = 0;
  asm volatile("eor r0, r0; eor r0, r0" ::: "r0");
  REG_DMA1SAD = (intptr_t)src;
  REG_DMA1DAD = (intptr_t)0x040000a0;
  REG_DMA1CNT = DMA_DST_FIXED | DMA_SRC_INC | DMA_REPEAT | DMA32 |
                DMA_SPECIAL | DMA_ENABLE | 1;
}

#define TIMER_16MHZ 0

void init_sound(void)
{
  REG_TM0CNT_H = 0;
  SETSNDRES(1);
  SNDSTAT = SNDSTAT_ENABLE;
  DSOUNDCTRL = 0x0b0e;
  REG_TM0CNT_L = 0x10000 - (924 / 2);  // Timer for 18.157 kHz
  REG_TM0CNT_H = TIMER_16MHZ | TIMER_START;
}

// Decoder-specific implementations
#ifdef USE_GSM
static struct gsm_state decoder;
static signed short out_samples[160];

void gsm_init(gsm r)
{
  memset((char *)r, 0, sizeof(*r));
  r->nrp = 40;
}
#endif

#ifdef USE_8AD
static ADGlobals ad_decoder;
static signed short out_samples[304]; // 16-bit samples for consistency with Pin Eight
static signed char temp_8bit_samples[304]; // Temporary 8-bit buffer
#endif

// Universal audio buffers
signed char double_buffers[2][608] __attribute__((aligned(4)));

#define CMD_START_SONG 0x0400

extern const GBFS_FILE *fs;

void writeFromPlaybackBuffer(AudioPlaybackTracker *playback) {
  dsound_switch_buffers(double_buffers[playback->cur_buffer]);
  playback->cur_buffer = !playback->cur_buffer;
}

int initPlayback(void *playback_ptr)
{
#ifdef USE_8AD
  AudioPlaybackTracker *playback = (AudioPlaybackTracker*)playback_ptr;
#else
  GsmPlaybackTracker *playback = (GsmPlaybackTracker*)playback_ptr;
#endif

  if (initGBFS() != 0) {
    return 1;
  }
  init_sound();
  
  playback->cur_song = 0;
  playback->playing = 0; 
  playback->locked = 0;
  playback->src_pos = NULL;
  playback->src_end = NULL;
  playback->decode_pos = AUDIO_SAMPLES_PER_FRAME;
  playback->cur_buffer = 0;
  playback->last_joy = 0x3ff;
  playback->last_sample = 0;
  
  // Initialize spectrum analyzer
  for(int i = 0; i < 8; i++) {
    playback->spectrum_accumulators[i] = 0;
    playback->bar_current_heights[i] = 8;
    playback->bar_target_heights[i] = 8; 
    playback->bar_velocities[i] = 0;
  }
  playback->spectrum_sample_count = 0;
  
#ifdef USE_GSM
  gsm_init(&decoder);
#endif

#ifdef USE_8AD
  init_8ad_decoder(&ad_decoder, NULL);
#endif

  return 0;
}

void advancePlayback(AudioPlaybackTracker *playback, void *input_mapping)
{
  typedef struct {
    unsigned short SEEK_BACK;
    unsigned short SEEK_FORWARD;  
  } DefaultPlaybackInputMapping;
  
  DefaultPlaybackInputMapping *mapping = (DefaultPlaybackInputMapping*)input_mapping;
  
  unsigned short j = (REG_KEYINPUT & 0x3ff) ^ 0x3ff;
  unsigned short cmd = j & (~playback->last_joy | mapping->SEEK_BACK | mapping->SEEK_FORWARD);
  signed char *dst_pos = double_buffers[playback->cur_buffer];

  playback->last_joy = j;

  // Handle track switching
  if (cmd & CMD_START_SONG)
  {
#ifdef USE_GSM
    gsm_init(&decoder);
#endif

#ifdef USE_8AD  
    init_8ad_decoder(&ad_decoder, NULL);
#endif

    u32 src_len;
    const unsigned char *src = gbfs_get_nth_obj(fs, playback->cur_song, playback->curr_song_name, &src_len);
    playback->src_start_pos = src;
    playback->src_pos = src;
    playback->src_end = src + src_len;
    playback->decode_pos = AUDIO_SAMPLES_PER_FRAME;
    playback->playing = 1;
  }

  // Decode audio samples
  if (playback->playing && playback->src_pos < playback->src_end)
  {
    for (int j = 608 / 4; j > 0; j--)
    {
      int cur_sample;
      
      // Check if we need to decode a new frame
      if (playback->decode_pos >= AUDIO_SAMPLES_PER_FRAME)
      {
        if (playback->src_pos < playback->src_end)
        {
#ifdef USE_GSM
          gsm_decode(&decoder, playback->src_pos, out_samples);
          playback->src_pos += AUDIO_FRAME_SIZE;
#endif

#ifdef USE_8AD
          // 8AD: decode 304 samples from 152 bytes, similar to Pin Eight's approach
          if (playback->src_pos + AUDIO_FRAME_SIZE <= playback->src_end) {
            decode_ad(&ad_decoder, temp_8bit_samples, playback->src_pos, AUDIO_SAMPLES_PER_FRAME);
            
            // Convert 8-bit samples to 16-bit for consistency with GSM
            for(int i = 0; i < AUDIO_SAMPLES_PER_FRAME; i++) {
              out_samples[i] = temp_8bit_samples[i] << 8; // Convert 8-bit to 16-bit
            }
            
            playback->src_pos += AUDIO_FRAME_SIZE;
          }
#endif
        }
        playback->decode_pos = 0;
      }

      // 2:1 linear interpolation + spectrum analysis
      cur_sample = out_samples[playback->decode_pos++];
      *dst_pos++ = (playback->last_sample + cur_sample) >> 9;
      *dst_pos++ = cur_sample >> 8;
      playback->last_sample = cur_sample;
      
      // Spectrum analysis (same for both codecs)
      int pos = playback->decode_pos - 1;
      int abs_sample = cur_sample < 0 ? -cur_sample : cur_sample;
      int band;
      
      if(pos < 20) {
        band = 0;
      } else if(pos < 40) {
        band = abs_sample > 8000 ? 1 : 0;
      } else if(pos < 60) {
        band = abs_sample > 12000 ? 2 : (abs_sample > 6000 ? 1 : 0);
      } else if(pos < 80) {
        band = abs_sample > 15000 ? 3 : (abs_sample > 8000 ? 2 : 1);
      } else if(pos < 100) {
        band = abs_sample > 18000 ? 4 : (abs_sample > 10000 ? 3 : 2);
      } else if(pos < 120) {
        band = abs_sample > 20000 ? 5 : (abs_sample > 12000 ? 4 : 3);
      } else if(pos < 140) {
        band = abs_sample > 22000 ? 6 : (abs_sample > 14000 ? 5 : 4);
      } else {
        band = abs_sample > 24000 ? 7 : (abs_sample > 16000 ? 6 : 5);
      }
      
      if(band >= 0 && band < 8) {
        playback->spectrum_accumulators[band] += abs_sample;
      }
      
      // Repeat for additional samples (same pattern as original)
      cur_sample = out_samples[playback->decode_pos++];
      *dst_pos++ = (playback->last_sample + cur_sample) >> 9;
      *dst_pos++ = cur_sample >> 8;
      playback->last_sample = cur_sample;
      
      cur_sample = out_samples[playback->decode_pos++];
      *dst_pos++ = (playback->last_sample + cur_sample) >> 9;
      *dst_pos++ = cur_sample >> 8;
      playback->last_sample = cur_sample;
      
      cur_sample = out_samples[playback->decode_pos++];
      *dst_pos++ = (playback->last_sample + cur_sample) >> 9;
      *dst_pos++ = cur_sample >> 8;
      playback->last_sample = cur_sample;
    }
  }
  
  playback->spectrum_sample_count += 608;
}

#endif // USE_8AD