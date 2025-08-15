#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "polygond_decoder.h"

/**
 * Test program for Polygondwanaland Audio Decoder
 * Validates C decoder against compressed Crumbling Castle data
 */

// Helper function to load binary file
uint8_t* load_file(const char* filename, uint32_t* file_size) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        printf("Error: Could not open file %s\n", filename);
        return NULL;
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    *file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    // Allocate and read data
    uint8_t* data = malloc(*file_size);
    if (!data) {
        printf("Error: Could not allocate memory for file\n");
        fclose(file);
        return NULL;
    }
    
    size_t bytes_read = fread(data, 1, *file_size, file);
    if (bytes_read != *file_size) {
        printf("Error: Could not read complete file\n");
        free(data);
        fclose(file);
        return NULL;
    }
    
    fclose(file);
    return data;
}

// Test header validation
void test_header_validation(const uint8_t* data, uint32_t data_size) {
    printf("=== Header Validation Test ===\n");
    
    pgda_error_t result = pgda_validate_header(data, data_size);
    switch (result) {
        case PGDA_SUCCESS:
            printf("✓ Header validation passed\n");
            break;
        case PGDA_ERROR_INVALID_MAGIC:
            printf("✗ Invalid magic bytes\n");
            break;
        case PGDA_ERROR_INVALID_SAMPLE_RATE:
            printf("✗ Invalid sample rate\n");
            break;
        case PGDA_ERROR_TOO_LARGE:
            printf("✗ File too large or corrupted\n");
            break;
        default:
            printf("✗ Unknown error: %d\n", result);
            break;
    }
}

// Test decoder initialization
bool test_decoder_init(pgda_decoder_t* decoder, const uint8_t* data, uint32_t data_size) {
    printf("\n=== Decoder Initialization Test ===\n");
    
    pgda_error_t result = pgda_init_decoder(decoder, data, data_size);
    if (result != PGDA_SUCCESS) {
        printf("✗ Decoder initialization failed: %d\n", result);
        return false;
    }
    
    printf("✓ Decoder initialized successfully\n");
    printf("  Sample rate: %u Hz\n", decoder->header.sample_rate);
    printf("  Total samples: %u\n", decoder->header.num_deltas);
    printf("  First sample: %d\n", decoder->header.first_sample);
    printf("  Duration: %.1f seconds\n", 
           (float)decoder->header.num_deltas / decoder->header.sample_rate);
    
    return true;
}

// Test sample decoding
void test_sample_decoding(pgda_decoder_t* decoder) {
    printf("\n=== Sample Decoding Test ===\n");
    
    audio_buffer_t buffer;
    uint32_t total_decoded = 0;
    uint32_t chunks = 0;
    
    // Decode first few chunks to test
    const uint32_t test_chunks = 5;
    const uint32_t samples_per_chunk = 512;
    
    for (uint32_t chunk = 0; chunk < test_chunks; chunk++) {
        uint32_t decoded = pgda_decode_samples(decoder, &buffer, samples_per_chunk);
        
        if (decoded == 0) {
            printf("  End of stream reached at chunk %u\n", chunk);
            break;
        }
        
        total_decoded += decoded;
        chunks++;
        
        // Analyze first and last samples of chunk
        if (decoded > 0) {
            printf("  Chunk %u: decoded %u samples, range [%d, %d]\n", 
                   chunk, decoded, buffer.samples[0], buffer.samples[decoded-1]);
        }
    }
    
    printf("✓ Decoded %u samples in %u chunks\n", total_decoded, chunks);
    printf("  Current position: %u / %u\n", 
           pgda_get_position(decoder), pgda_get_total_samples(decoder));
}

// Test decoder reset
void test_decoder_reset(pgda_decoder_t* decoder) {
    printf("\n=== Decoder Reset Test ===\n");
    
    uint32_t pos_before = pgda_get_position(decoder);
    pgda_reset_decoder(decoder);
    uint32_t pos_after = pgda_get_position(decoder);
    
    printf("  Position before reset: %u\n", pos_before);
    printf("  Position after reset: %u\n", pos_after);
    
    if (pos_after == 0) {
        printf("✓ Decoder reset successfully\n");
    } else {
        printf("✗ Decoder reset failed\n");
    }
}

// Performance test
void test_performance(pgda_decoder_t* decoder) {
    printf("\n=== Performance Test ===\n");
    
    pgda_reset_decoder(decoder);
    
    audio_buffer_t buffer;
    uint32_t total_samples = 0;
    uint32_t iterations = 0;
    
    // Decode entire audio stream
    while (!pgda_is_end_of_stream(decoder)) {
        uint32_t decoded = pgda_decode_samples(decoder, &buffer, AUDIO_BUFFER_SIZE);
        if (decoded == 0) break;
        
        total_samples += decoded;
        iterations++;
    }
    
    printf("✓ Performance test completed\n");
    printf("  Total samples decoded: %u\n", total_samples);
    printf("  Decode iterations: %u\n", iterations);
    printf("  Average samples per iteration: %.1f\n", 
           (float)total_samples / iterations);
}

int main(int argc, char* argv[]) {
    const char* default_file = "../test_data/compressed/crumbling_castle_final.pgda";
    const char* filename = (argc > 1) ? argv[1] : default_file;
    
    printf("Polygondwanaland Audio Decoder Test\n");
    printf("Testing file: %s\n\n", filename);
    
    // Load compressed audio file
    uint32_t file_size;
    uint8_t* data = load_file(filename, &file_size);
    if (!data) {
        return 1;
    }
    
    printf("Loaded file: %u bytes\n", file_size);
    
    // Run tests
    test_header_validation(data, file_size);
    
    pgda_decoder_t decoder;
    if (test_decoder_init(&decoder, data, file_size)) {
        test_sample_decoding(&decoder);
        test_decoder_reset(&decoder);
        test_performance(&decoder);
    }
    
    // Cleanup
    free(data);
    
    printf("\n=== Test Complete ===\n");
    return 0;
}