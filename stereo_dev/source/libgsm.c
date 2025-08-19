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
#include <string.h> // for memset

#include "gsm.h"
#include "private.h" /* for sizeof(struct gsm_state) */
#include "gbfs.h"
#include "libgsm.h"

// gsmplay.c ////////////////////////////////////////////////////////

static void dsound_switch_pokemon_stereo_buffers(const void *pcm_base_src)
{
  // Stop both DMA channels
  REG_DMA1CNT = 0;
  REG_DMA2CNT = 0;

  /* no-op to let DMA registers catch up */
  asm volatile("eor r0, r0; eor r0, r0" ::: "r0");

  // Pokémon approach: Single buffer, offset for second channel
  // DMA1: FIFO A (Right channel) - base buffer
  REG_DMA1SAD = (intptr_t)pcm_base_src;
  REG_DMA1DAD = (intptr_t)0x040000a0; /* FIFO A address */
  REG_DMA1CNT = DMA_DST_FIXED | DMA_SRC_INC | DMA_REPEAT | DMA32 |
                DMA_SPECIAL | DMA_ENABLE | 1;

  // DMA2: FIFO B (Left channel) - buffer + offset
  REG_DMA2SAD = (intptr_t)pcm_base_src + PCM_DMA_BUF_SIZE;
  REG_DMA2DAD = (intptr_t)0x040000a4; /* FIFO B address */
  REG_DMA2CNT = DMA_DST_FIXED | DMA_SRC_INC | DMA_REPEAT | DMA32 |
                DMA_SPECIAL | DMA_ENABLE | 1;
}

#define TIMER_16MHZ 0

void init_sound(void)
{
  // TM0CNT_L is count; TM0CNT_H is control
  REG_TM0CNT_H = 0;
  // turn on sound circuit
  SETSNDRES(1);
  SNDSTAT = SNDSTAT_ENABLE;
  
  // Use Pokémon Emerald's stereo configuration
  // FIFO A -> Right channel, FIFO B -> Left channel
  // Pokémon Emerald stereo config: A->Right, B->Left, both Timer0, full mix
  REG_SOUNDCNT_H = 0x0100 | 0x2000 | 0x0000 | 0x0000 | 0x0008;  // SOUND_A_RIGHT_OUTPUT | SOUND_B_LEFT_OUTPUT | SOUND_A_TIMER_0 | SOUND_B_TIMER_0 | SOUND_ALL_MIX_FULL
  
  REG_TM0CNT_L = 0x10000 - (924 / 2);  // Timer for 18.157 kHz: 16MHz / 18157 / 2 ≈ 462
  REG_TM0CNT_H = TIMER_16MHZ | TIMER_START;
}

/* gsm_init() **************
   This is to gsm_create() as placement new is to new.
*/
void gsm_init(gsm r)
{
  memset((char *)r, 0, sizeof(*r));
  r->nrp = 40;
}

struct GsmPlaybackInputMapping DEFAULT_PLAYBACK_INPUT_MAPPING = {
    .TOGGLE_PLAY_PAUSE = KEY_A | KEY_B | KEY_START,
    .PREV_TRACK = KEY_LEFT,
    .NEXT_TRACK = KEY_RIGHT,
    .SEEK_BACK = KEY_L,
    .SEEK_FORWARD = KEY_R,
    .TOGGLE_LOCK = KEY_SELECT,
};

struct gsm_state decoder;
struct gsm_state decoder_L;  // Left channel decoder for stereo
struct gsm_state decoder_R;  // Right channel decoder for stereo
const GBFS_FILE *fs;
const unsigned char *src;
uint32_t src_len;

int initPlayback(GsmPlaybackTracker *playback)
{
  fs = find_first_gbfs_file(find_first_gbfs_file);
  if (!fs)
  {
    return 1;
  }
  init_sound();
  playback->src_pos = NULL;
  playback->src_end = NULL;
  playback->decode_pos = 160;
  playback->cur_buffer = 0;
  playback->last_joy = 0x3ff;
  playback->cur_song = (unsigned int)(-1);
  playback->curr_song_name_len = 0;
  playback->last_sample = 0;
  playback->playing = 1;
  playback->locked = 0;
  return 0;
}

signed short out_samples[160];
signed short out_samples_L[160];  // Left channel samples for stereo
signed short out_samples_R[160];  // Right channel samples for stereo
// Use Pokémon-style single PCM buffer for both channels (interleaved)
signed char pcm_buffer[2][1216] __attribute__((aligned(4)));  // 608 * 2 for stereo interleaving
#define PCM_DMA_BUF_SIZE 608

#define CMD_START_SONG 0x0400

void writeFromPlaybackBuffer(GsmPlaybackTracker *playback) {
  dsound_switch_pokemon_stereo_buffers(pcm_buffer[playback->cur_buffer]);
  playback->cur_buffer = !playback->cur_buffer;
}

void advancePlayback(GsmPlaybackTracker *playback, GsmPlaybackInputMapping *mapping)
{
  unsigned short j = (REG_KEYINPUT & 0x3ff) ^ 0x3ff;
  unsigned short cmd = j & (~playback->last_joy | mapping->SEEK_BACK | mapping->SEEK_FORWARD);
  // Pokémon-style: Single PCM buffer with Right channel at base, Left at offset
  signed char *dst_pos_R = pcm_buffer[playback->cur_buffer];         // Right channel (FIFO A)
  signed char *dst_pos_L = pcm_buffer[playback->cur_buffer] + PCM_DMA_BUF_SIZE; // Left channel (FIFO B)

  playback->last_joy = j;

  if (cmd & mapping->TOGGLE_LOCK)
  {
    playback->locked = playback->locked ? 0 : 1;
  }

  if (playback->locked)
  {
    cmd = 0;
  }

  if (cmd & mapping->TOGGLE_PLAY_PAUSE)
  {
    playback->playing = playback->playing ? 0 : 1;
  }

  if (cmd & mapping->SEEK_BACK)
  {
    playback->src_pos -= 33 * 50;
    if (playback->src_pos < src)
    {
      cmd |= mapping->PREV_TRACK;
    }
  }

  // R button: Skip forward
  if (cmd & mapping->SEEK_FORWARD)
  {
    playback->src_pos += 33 * 50;
  }

  // At end of track, proceed to the next
  if (playback->src_pos >= playback->src_end)
  {
    cmd |= mapping->NEXT_TRACK;
  }

  if (cmd & mapping->NEXT_TRACK)
  {
    playback->cur_song++;
    if (playback->cur_song >= gbfs_count_objs(fs))
    {
      playback->cur_song = 0;
    }
    cmd |= CMD_START_SONG;
  }

  if (cmd & mapping->PREV_TRACK)
  {
    if (playback->cur_song == 0)
    {
      playback->cur_song = gbfs_count_objs(fs) - 1;
    }
    else
    {
      playback->cur_song--;
    }
    cmd |= CMD_START_SONG;
  }

  if (cmd & CMD_START_SONG)
  {
    gsm_init(&decoder);
    gsm_init(&decoder_L);  // Initialize left channel decoder
    gsm_init(&decoder_R);  // Initialize right channel decoder
    src = gbfs_get_nth_obj(fs, playback->cur_song, playback->curr_song_name, &src_len);
    playback->src_start_pos = src;
    {
      unsigned int song_name_len = 0;
      const unsigned int max_len = sizeof(playback->curr_song_name);
      while (song_name_len < max_len && playback->curr_song_name[song_name_len] != '.')
      {
        song_name_len++;
      }
      playback->curr_song_name_len = song_name_len;
      playback->marquee_offset = 0;
      playback->frames_until_marquee_update = 90;
      playback->reel_rotation_theta = 0;
    }
    // If reached by seek, go near end of the track.
    // Otherwise, go to the start.
    if (cmd & mapping->SEEK_BACK)
    {
      playback->src_pos = src + src_len - 33 * 60;
    }
    else
    {
      playback->src_pos = src;
    }
    playback->src_end = src + src_len;
  }

  if (!playback->playing)
  { /* if paused */
    for (j = 304 / 2; j > 0; j--)
    {
      *dst_pos_L++ = playback->last_sample >> 8;
      *dst_pos_L++ = playback->last_sample >> 8;
      *dst_pos_R++ = playback->last_sample >> 8;
      *dst_pos_R++ = playback->last_sample >> 8;
    }
  }
  else
  {
    for (j = 304 / 4; j > 0; j--)
    {
      int cur_sample;
      if (playback->decode_pos >= 160)
      {
        if (playback->src_pos < playback->src_end)
        {
          // Decode LEFT channel (first 33 bytes) 
          gsm_decode(&decoder_L, playback->src_pos, out_samples_L);
          playback->src_pos += 33;
          
          // Decode RIGHT channel (next 33 bytes) 
          if (playback->src_pos < playback->src_end) {
            gsm_decode(&decoder_R, playback->src_pos, out_samples_R);
            playback->src_pos += 33;
          } else {
            // If no right data, copy left to right
            for (int i = 0; i < 160; i++) {
              out_samples_R[i] = out_samples_L[i];
            }
          }
          
          // Mix L+R into main buffer for compatibility but preserve difference
          for (int i = 0; i < 160; i++) {
            out_samples[i] = (out_samples_L[i] + out_samples_R[i]) / 2;  // Center sum
          }
        }
        playback->decode_pos = 0;
      }

      /* 2:1 linear interpolation with simple stereo separation */
      int cur_sample_mono, cur_sample_L, cur_sample_R, stereo_diff;
      
      // Use stable mono sample for timing
      cur_sample_mono = out_samples[playback->decode_pos];
      cur_sample_L = out_samples_L[playback->decode_pos];
      cur_sample_R = out_samples_R[playback->decode_pos];
      stereo_diff = (cur_sample_L - cur_sample_R) / 4;  // Gentle stereo separation
      playback->decode_pos++;
      
      *dst_pos_L++ = (playback->last_sample + cur_sample_mono + stereo_diff) >> 9;
      *dst_pos_L++ = (cur_sample_mono + stereo_diff) >> 8;
      *dst_pos_R++ = (playback->last_sample + cur_sample_mono - stereo_diff) >> 9;
      *dst_pos_R++ = (cur_sample_mono - stereo_diff) >> 8;
      playback->last_sample = cur_sample_mono;

      cur_sample_mono = out_samples[playback->decode_pos];
      cur_sample_L = out_samples_L[playback->decode_pos];
      cur_sample_R = out_samples_R[playback->decode_pos];
      stereo_diff = (cur_sample_L - cur_sample_R) / 4;
      playback->decode_pos++;
      
      *dst_pos_L++ = (playback->last_sample + cur_sample_mono + stereo_diff) >> 9;
      *dst_pos_L++ = (cur_sample_mono + stereo_diff) >> 8;
      *dst_pos_R++ = (playback->last_sample + cur_sample_mono - stereo_diff) >> 9;
      *dst_pos_R++ = (cur_sample_mono - stereo_diff) >> 8;
      playback->last_sample = cur_sample_mono;

      cur_sample_mono = out_samples[playback->decode_pos];
      cur_sample_L = out_samples_L[playback->decode_pos];
      cur_sample_R = out_samples_R[playback->decode_pos];
      stereo_diff = (cur_sample_L - cur_sample_R) / 4;
      playback->decode_pos++;
      
      *dst_pos_L++ = (playback->last_sample + cur_sample_mono + stereo_diff) >> 9;
      *dst_pos_L++ = (cur_sample_mono + stereo_diff) >> 8;
      *dst_pos_R++ = (playback->last_sample + cur_sample_mono - stereo_diff) >> 9;
      *dst_pos_R++ = (cur_sample_mono - stereo_diff) >> 8;
      playback->last_sample = cur_sample_mono;

      cur_sample_mono = out_samples[playback->decode_pos];
      cur_sample_L = out_samples_L[playback->decode_pos];
      cur_sample_R = out_samples_R[playback->decode_pos];
      stereo_diff = (cur_sample_L - cur_sample_R) / 4;
      playback->decode_pos++;
      
      *dst_pos_L++ = (playback->last_sample + cur_sample_mono + stereo_diff) >> 9;
      *dst_pos_L++ = (cur_sample_mono + stereo_diff) >> 8;
      *dst_pos_R++ = (playback->last_sample + cur_sample_mono - stereo_diff) >> 9;
      *dst_pos_R++ = (cur_sample_mono - stereo_diff) >> 8;
      playback->last_sample = cur_sample_mono;
    }
  }
}
