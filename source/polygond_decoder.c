#include "polygond_decoder.h"
#include <string.h>

/**
 * Polygondwanaland Audio Decoder Implementation
 * 
 * High-performance delta decompression for GBA ARM7TDMI processor
 * Optimized for memory efficiency and real-time audio playback
 */

// Helper function to read little-endian uint32_t
static uint32_t read_uint32_le(const uint8_t* data) {
    return (uint32_t)data[0] | 
           ((uint32_t)data[1] << 8) | 
           ((uint32_t)data[2] << 16) | 
           ((uint32_t)data[3] << 24);
}

// Validate magic bytes
static bool validate_magic(const uint8_t* data) {
    const char expected_magic[] = PGDA_MAGIC_BYTES;
    return (data[0] == expected_magic[0] && 
            data[1] == expected_magic[1] && 
            data[2] == expected_magic[2] && 
            data[3] == expected_magic[3]);
}

pgda_error_t pgda_validate_header(const uint8_t* data, uint32_t data_size) {
    // Check minimum size for header
    if (data_size < PGDA_HEADER_SIZE) {
        return PGDA_ERROR_TOO_LARGE;
    }
    
    // Validate magic bytes
    if (!validate_magic(data)) {
        return PGDA_ERROR_INVALID_MAGIC;
    }
    
    // Check sample rate
    uint32_t sample_rate = read_uint32_le(data + 4);
    if (sample_rate < MIN_SAMPLE_RATE || sample_rate > MAX_SAMPLE_RATE) {
        return PGDA_ERROR_INVALID_SAMPLE_RATE;
    }
    
    // Check total size
    uint32_t num_deltas = read_uint32_le(data + 8);
    uint32_t expected_size = PGDA_HEADER_SIZE + num_deltas;
    if (expected_size > data_size || num_deltas > MAX_AUDIO_SIZE) {
        return PGDA_ERROR_TOO_LARGE;
    }
    
    return PGDA_SUCCESS;
}

pgda_error_t pgda_init_decoder(pgda_decoder_t* decoder, const uint8_t* data, uint32_t data_size) {
    if (!decoder || !data) {
        return PGDA_ERROR_NULL_POINTER;
    }
    
    // Validate header first
    pgda_error_t result = pgda_validate_header(data, data_size);
    if (result != PGDA_SUCCESS) {
        return result;
    }
    
    // Parse header
    memcpy(decoder->header.magic, data, 4);
    decoder->header.sample_rate = read_uint32_le(data + 4);
    decoder->header.num_deltas = read_uint32_le(data + 8);
    decoder->header.first_sample = (int8_t)data[12];
    
    // Set up decoder state
    decoder->delta_data = (const int8_t*)(data + PGDA_HEADER_SIZE);
    decoder->current_position = 0;
    decoder->last_sample = decoder->header.first_sample;  // Start with 8-bit value
    decoder->is_initialized = true;
    
    return PGDA_SUCCESS;
}

uint32_t pgda_decode_samples(pgda_decoder_t* decoder, audio_buffer_t* output, uint32_t samples_requested) {
    if (!decoder || !output || !decoder->is_initialized) {
        return 0;
    }
    
    // Limit to buffer size and remaining samples
    uint32_t samples_to_decode = samples_requested;
    if (samples_to_decode > AUDIO_BUFFER_SIZE) {
        samples_to_decode = AUDIO_BUFFER_SIZE;
    }
    
    uint32_t remaining_samples = decoder->header.num_deltas - decoder->current_position;
    if (samples_to_decode > remaining_samples) {
        samples_to_decode = remaining_samples;
    }
    
    if (samples_to_decode == 0) {
        output->length = 0;
        return 0;
    }
    
    // Delta decompression for 8-bit GBA compression
    int8_t current_sample = (int8_t)decoder->last_sample;
    const int8_t* delta_ptr = decoder->delta_data + decoder->current_position;
    
    for (uint32_t i = 0; i < samples_to_decode; i++) {
        // Apply delta in 8-bit space with clamping
        int16_t next_sample = (int16_t)current_sample + (int16_t)delta_ptr[i];
        
        // Clamp to 8-bit range (matches encoder behavior)
        if (next_sample > 127) {
            next_sample = 127;
        } else if (next_sample < -128) {
            next_sample = -128;
        }
        
        current_sample = (int8_t)next_sample;
        
        // Store 8-bit sample directly 
        output->samples[i] = (int16_t)current_sample;
    }
    
    // Update decoder state - CRITICAL: advance position
    decoder->current_position += samples_to_decode;
    decoder->last_sample = current_sample;
    
    // Set output buffer metadata
    output->length = samples_to_decode;
    output->sample_rate = decoder->header.sample_rate;
    
    return samples_to_decode;
}

void pgda_reset_decoder(pgda_decoder_t* decoder) {
    if (!decoder || !decoder->is_initialized) {
        return;
    }
    
    decoder->current_position = 0;
    decoder->last_sample = decoder->header.first_sample;  // Reset to 8-bit value
}

uint32_t pgda_get_total_samples(const pgda_decoder_t* decoder) {
    if (!decoder || !decoder->is_initialized) {
        return 0;
    }
    
    return decoder->header.num_deltas;
}

uint32_t pgda_get_position(const pgda_decoder_t* decoder) {
    if (!decoder || !decoder->is_initialized) {
        return 0;
    }
    
    return decoder->current_position;
}

bool pgda_is_end_of_stream(const pgda_decoder_t* decoder) {
    if (!decoder || !decoder->is_initialized) {
        return true;
    }
    
    return decoder->current_position >= decoder->header.num_deltas;
}

// GBA Direct Sound audio implementation
#include <gba.h>

// Audio buffers for DMA double buffering
static int16_t audio_buffer_a[AUDIO_BUFFER_SIZE] __attribute__((aligned(4)));
static int16_t audio_buffer_b[AUDIO_BUFFER_SIZE] __attribute__((aligned(4)));
static volatile bool buffer_a_playing = true;
static volatile uint32_t current_buffer_pos = 0;
static volatile uint32_t current_buffer_size = 0;

// Audio interrupt handler
void audio_interrupt_handler(void) {
    // Switch buffers when current one finishes
    buffer_a_playing = !buffer_a_playing;
    current_buffer_pos = 0;
}

bool gba_audio_init(uint32_t sample_rate) {
    // Simple, proven GBA audio setup
    
    // Turn off sound while we set it up
    REG_SOUNDCNT_X = 0;
    
    // Clear all sound control registers first
    REG_SOUNDCNT_L = 0;
    REG_SOUNDCNT_H = 0;
    
    // Enable master sound
    REG_SOUNDCNT_X = 0x80;  // Enable sound
    
    // Set up Direct Sound A
    // Bit layout: 0x0B0F
    // - Bits 0-1: Master volume (11 = 100%)
    // - Bit 2: DSound A to left (1 = enable) 
    // - Bit 3: DSound A to right (1 = enable)
    // - Bits 8-9: DSound A volume (11 = 100%)
    // - Bit 10: DSound A use Timer 0 (1 = enable)
    // - Bit 11: DSound A reset FIFO (1 = reset)
    REG_SOUNDCNT_H = 0x0B0F;
    
    // Set up Timer 0 for sample rate
    uint16_t timer_reload = 65536 - (16777216 / sample_rate);
    REG_TM0CNT_L = timer_reload;
    REG_TM0CNT_H = 0x0080; // Enable timer, no prescaler
    
    // Set up DMA1 to feed FIFO_A
    REG_DMA1SAD = (uint32_t)audio_buffer_a;
    REG_DMA1DAD = 0x040000A0; // FIFO_A address
    REG_DMA1CNT = 0xB600; // Enable, repeat, 32-bit, sound mode
    
    return true;
}

bool gba_audio_queue_buffer(const audio_buffer_t* buffer) {
    if (!buffer || buffer->length == 0) {
        return false;
    }
    
    // Simple approach: just copy to buffer A and let DMA loop it
    uint32_t samples_to_copy = buffer->length;
    if (samples_to_copy > AUDIO_BUFFER_SIZE) {
        samples_to_copy = AUDIO_BUFFER_SIZE;
    }
    
    // Convert 16-bit samples to 8-bit for FIFO (GBA Direct Sound uses 8-bit)
    for (uint32_t i = 0; i < samples_to_copy; i++) {
        // Scale down 16-bit sample to 8-bit signed
        int8_t sample_8bit = (int8_t)(buffer->samples[i] >> 8);
        ((int8_t*)audio_buffer_a)[i] = sample_8bit;
    }
    
    // Set DMA length to match our data
    REG_DMA1CNT = 0; // Stop DMA
    REG_DMA1CNT = 0xB600 | samples_to_copy; // Restart with correct length
    
    return true;
}

uint32_t gba_audio_get_free_buffers(void) {
    // Simple implementation - always return 1 available buffer
    return 1;
}