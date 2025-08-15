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
    decoder->last_sample = decoder->header.first_sample;
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
    
    // Delta decompression loop (optimized for ARM7TDMI)
    int8_t current_sample = decoder->last_sample;
    const int8_t* delta_ptr = decoder->delta_data + decoder->current_position;
    
    for (uint32_t i = 0; i < samples_to_decode; i++) {
        // Apply delta (with overflow protection)
        int16_t next_sample = (int16_t)current_sample + (int16_t)delta_ptr[i];
        
        // Clamp to 8-bit range
        if (next_sample > 127) {
            next_sample = 127;
        } else if (next_sample < -128) {
            next_sample = -128;
        }
        
        current_sample = (int8_t)next_sample;
        
        // Convert to 16-bit for GBA audio (scale up from 8-bit)
        // Multiply by 256 to use full 16-bit range (efficient left shift)
        output->samples[i] = (int16_t)current_sample << 8;
    }
    
    // Update decoder state
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
    decoder->last_sample = decoder->header.first_sample;
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

// GBA audio integration stubs (to be implemented with actual GBA audio driver)
bool gba_audio_init(uint32_t sample_rate) {
    // TODO: Initialize GBA Direct Sound audio system
    // Set up DMA channels for audio playback
    // Configure audio registers for target sample rate
    return true;  // Stub implementation
}

bool gba_audio_queue_buffer(const audio_buffer_t* buffer) {
    // TODO: Queue buffer to GBA audio DMA system
    // Handle double buffering for smooth playback
    return true;  // Stub implementation
}

uint32_t gba_audio_get_free_buffers(void) {
    // TODO: Return number of available audio buffers
    return 2;  // Stub implementation (typical double buffer)
}