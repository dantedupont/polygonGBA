#ifndef POLYGOND_DECODER_H
#define POLYGOND_DECODER_H

#include <stdint.h>
#include <stdbool.h>

/**
 * Polygondwanaland Audio Decoder for Game Boy Advance
 * 
 * Decodes .pgda files compressed with frequency-weighted delta compression
 * Optimized for ARM7TDMI processor and GBA memory constraints
 */

// File format constants
#define PGDA_MAGIC_SIZE 4
#define PGDA_MAGIC_BYTES {'F', 'Q', 'W', 'T'}
#define PGDA_HEADER_SIZE 13  // 4 (magic) + 4 (sample_rate) + 4 (num_deltas) + 1 (first_sample)

// Audio constraints
#define MAX_SAMPLE_RATE 8000     // Maximum supported sample rate
#define MIN_SAMPLE_RATE 4000     // Minimum supported sample rate  
#define AUDIO_BUFFER_SIZE 1024   // GBA-friendly buffer size
#define MAX_AUDIO_SIZE 5000000   // ~5MB max compressed audio

// Error codes
typedef enum {
    PGDA_SUCCESS = 0,
    PGDA_ERROR_INVALID_MAGIC = -1,
    PGDA_ERROR_INVALID_SAMPLE_RATE = -2,
    PGDA_ERROR_TOO_LARGE = -3,
    PGDA_ERROR_BUFFER_OVERFLOW = -4,
    PGDA_ERROR_NULL_POINTER = -5
} pgda_error_t;

// Compressed audio file header
typedef struct {
    char magic[4];              // "FQWT" magic bytes
    uint32_t sample_rate;       // Sample rate (6750 Hz for our codec)
    uint32_t num_deltas;        // Number of delta values
    int8_t first_sample;        // Initial sample value
} __attribute__((packed)) pgda_header_t;

// Audio decoder state
typedef struct {
    pgda_header_t header;       // File header information
    const int8_t* delta_data;   // Pointer to delta data in ROM
    uint32_t current_position;  // Current decode position
    int8_t last_sample;         // Last decoded sample (for delta reconstruction)
    bool is_initialized;        // Decoder initialization flag
} pgda_decoder_t;

// Audio output buffer for GBA
typedef struct {
    int16_t samples[AUDIO_BUFFER_SIZE];  // 16-bit samples for GBA audio
    uint32_t length;                     // Number of valid samples
    uint32_t sample_rate;                // Sample rate for this buffer
} audio_buffer_t;

// Function declarations

/**
 * Initialize decoder with compressed audio data
 * @param decoder Decoder state structure
 * @param data Pointer to compressed .pgda data in ROM
 * @param data_size Size of compressed data
 * @return PGDA_SUCCESS or error code
 */
pgda_error_t pgda_init_decoder(pgda_decoder_t* decoder, const uint8_t* data, uint32_t data_size);

/**
 * Decode next chunk of audio samples
 * @param decoder Decoder state
 * @param output Output buffer for decoded samples
 * @param samples_requested Number of samples to decode
 * @return Number of samples actually decoded (0 if end of stream)
 */
uint32_t pgda_decode_samples(pgda_decoder_t* decoder, audio_buffer_t* output, uint32_t samples_requested);

/**
 * Reset decoder to beginning of audio stream
 * @param decoder Decoder state
 */
void pgda_reset_decoder(pgda_decoder_t* decoder);

/**
 * Get total duration in samples
 * @param decoder Decoder state
 * @return Total number of samples in audio
 */
uint32_t pgda_get_total_samples(const pgda_decoder_t* decoder);

/**
 * Get current playback position
 * @param decoder Decoder state
 * @return Current sample position
 */
uint32_t pgda_get_position(const pgda_decoder_t* decoder);

/**
 * Check if decoder has reached end of audio
 * @param decoder Decoder state
 * @return true if at end of stream
 */
bool pgda_is_end_of_stream(const pgda_decoder_t* decoder);

/**
 * Validate .pgda file header
 * @param data Pointer to file data
 * @param data_size Size of data
 * @return PGDA_SUCCESS or error code
 */
pgda_error_t pgda_validate_header(const uint8_t* data, uint32_t data_size);

// GBA-specific audio integration functions (to be implemented with GBA audio driver)

/**
 * Initialize GBA audio system for decoded audio playback
 * @param sample_rate Target sample rate
 * @return true if initialization successful
 */
bool gba_audio_init(uint32_t sample_rate);

/**
 * Queue audio buffer for GBA playback
 * @param buffer Audio buffer to play
 * @return true if buffer queued successfully
 */
bool gba_audio_queue_buffer(const audio_buffer_t* buffer);

/**
 * Get number of free audio buffers available
 * @return Number of buffers that can be queued
 */
uint32_t gba_audio_get_free_buffers(void);

#endif // POLYGOND_DECODER_H