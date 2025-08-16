#include <gba.h>
#include "polygond_decoder.h"
#include "crumbling_castle_final.h"

// Using final optimized GBA compressed Crumbling Castle music data
extern const unsigned char crumbling_castle_final_data[];
extern const unsigned int crumbling_castle_final_data_size;

// Real-time streaming audio system
static pgda_decoder_t audio_decoder;
static audio_buffer_t decode_buffer;

// Pokemon-style PCM buffers - 4-byte aligned and properly sized
#define PCM_DMA_BUF_SIZE 1584
static s8 __attribute__((aligned(4))) pcm_buffer[PCM_DMA_BUF_SIZE * 2];  // Double buffer like Pokemon
static volatile bool need_refill = false;

// Pokemon-style VBlank counter for audio timing
static volatile u32 vblank_counter = 0;
static u32 pcm_dma_counter = 0;
static u32 pcm_dma_period = 4;  // Refill every 4 VBlanks (back to working frequency)

// Fill a tiny buffer with real-time decompression
static u32 total_decoded = 0;
static u32 last_decoded_count = 0;
static s16 first_sample = 0;
static u32 decoder_position_before = 0;
static u32 decoder_position_after = 0;
// Pokemon-style VBlank interrupt handler for audio timing
void audio_vblank_handler() {
    vblank_counter++;
    
    // Decrement DMA counter (Pokemon's approach)
    if(pcm_dma_counter > 0) {
        pcm_dma_counter--;
    }
    
    // When counter reaches 0, signal buffer refill needed
    if(pcm_dma_counter == 0) {
        need_refill = true;
        pcm_dma_counter = pcm_dma_period;  // Reset counter
    }
}

void fill_pcm_buffer() {
    // Decode compressed audio using PGDA decoder
    u32 samples_needed = PCM_DMA_BUF_SIZE * 2;  // Fill entire double buffer
    u32 buffer_pos = 0;
    
    while(buffer_pos < samples_needed) {
        // Calculate how many samples we need for this chunk
        u32 chunk_size = samples_needed - buffer_pos;
        if(chunk_size > AUDIO_BUFFER_SIZE) {
            chunk_size = AUDIO_BUFFER_SIZE;  // Respect decoder buffer limits
        }
        
        // Decode next chunk from PGDA
        u32 decoded = pgda_decode_samples(&audio_decoder, &decode_buffer, chunk_size);
        
        if(decoded == 0) {
            // End of audio - loop back to beginning
            pgda_reset_decoder(&audio_decoder);
            decoded = pgda_decode_samples(&audio_decoder, &decode_buffer, chunk_size);
            
            if(decoded == 0) {
                // Still no samples - fill remaining with silence
                for(u32 i = buffer_pos; i < samples_needed; i++) {
                    pcm_buffer[i] = 0;
                }
                break;
            }
        }
        
        // Convert decoded 8-bit samples for GBA Direct Sound
        for(u32 i = 0; i < decoded && buffer_pos < samples_needed; i++) {
            // The decoder outputs true 8-bit samples, use directly
            s8 sample_8bit = (s8)decode_buffer.samples[i];
            pcm_buffer[buffer_pos] = sample_8bit;
            buffer_pos++;
        }
    }
    
    total_decoded += PCM_DMA_BUF_SIZE;  // Track total for debugging
}

int main() {
    // Initialize interrupts and video
    irqInit();
    irqSet(IRQ_VBLANK, audio_vblank_handler);  // Set our VBlank handler
    irqEnable(IRQ_VBLANK);
    
    SetMode(MODE_3 | BG2_ENABLE);
    u16* videoBuffer = (u16*)0x6000000;
    
    // Initialize audio decoder with final optimized Crumbling Castle music
    pgda_error_t init_result = pgda_init_decoder(&audio_decoder, crumbling_castle_final_data, crumbling_castle_final_data_size);
    
    // Test basic decoder state regardless of init result
    u32 total_samples = pgda_get_total_samples(&audio_decoder);
    u32 current_pos = pgda_get_position(&audio_decoder);
    
    // Debug: Check decoder header values AND raw data bytes
    u32 num_deltas = audio_decoder.header.num_deltas;
    u32 sample_rate = audio_decoder.header.sample_rate;
    s8 first_sample_val = audio_decoder.header.first_sample;
    
    // Header should parse correctly now - no manual fix needed
    // (keeping this comment as a reminder of the previous debugging)
    
    // Show decoder state on screen
    for(int i = 0; i < 240*160; i++) {
        if(init_result == PGDA_SUCCESS) {
            videoBuffer[i] = RGB5(0, 31, 0);  // Green = success
        } else {
            videoBuffer[i] = RGB5(31, 0, init_result & 0x1F);  // Red with error code
        }
    }
    
    // Show total samples vs manual fix comparison
    for(int i = 0; i < 10; i++) {
        for(int j = 0; j < 120; j++) {
            // Left: total_samples from function call
            videoBuffer[i * 240 + j] = RGB5((total_samples >> 16) & 0x1F, (total_samples >> 11) & 0x1F, (total_samples >> 6) & 0x1F);
        }
        for(int j = 120; j < 240; j++) {
            // Right: decoder's num_deltas (after manual fix)
            videoBuffer[i * 240 + j] = RGB5((num_deltas >> 16) & 0x1F, (num_deltas >> 11) & 0x1F, (num_deltas >> 6) & 0x1F);
        }
    }
    
    // Check raw bytes at positions 8-11 (num_deltas location)
    u8 byte8 = crumbling_castle_final_data[8];
    u8 byte9 = crumbling_castle_final_data[9];
    u8 byte10 = crumbling_castle_final_data[10];
    u8 byte11 = crumbling_castle_final_data[11];
    
    // Manual calculation of what num_deltas should be
    u32 manual_calc = (u32)byte8 | ((u32)byte9 << 8) | ((u32)byte10 << 16) | ((u32)byte11 << 24);
    
    // Show num_deltas from decoder (should be 0) vs manual calc (should be ~4M)
    for(int i = 10; i < 20; i++) {
        for(int j = 0; j < 120; j++) {
            // Left half: decoder's num_deltas (should be black)
            videoBuffer[i * 240 + j] = RGB5((num_deltas >> 16) & 0x1F, (num_deltas >> 11) & 0x1F, (num_deltas >> 6) & 0x1F);
        }
        for(int j = 120; j < 240; j++) {
            // Right half: manual calculation (should be colorful)
            videoBuffer[i * 240 + j] = RGB5((manual_calc >> 16) & 0x1F, (manual_calc >> 11) & 0x1F, (manual_calc >> 6) & 0x1F);
        }
    }
    
    // Show sample rate in third strip - this will help us debug!
    for(int i = 20; i < 30; i++) {
        for(int j = 0; j < 240; j++) {
            // Visualize sample rate - if it shows up as very bright, the rate might be wrong
            videoBuffer[i * 240 + j] = RGB5((sample_rate >> 8) & 0x1F, (sample_rate >> 4) & 0x1F, sample_rate & 0x1F);
        }
    }
    
    // CRITICAL DEBUG: Show actual decoded sample rate in fourth strip
    u32 actual_rate = audio_decoder.header.sample_rate;
    for(int i = 30; i < 40; i++) {
        for(int j = 0; j < 240; j++) {
            videoBuffer[i * 240 + j] = RGB5((actual_rate >> 8) & 0x1F, (actual_rate >> 4) & 0x1F, actual_rate & 0x1F);
        }
    }
    
    // Show raw bytes in fourth strip to debug data reading
    for(int i = 30; i < 40; i++) {
        for(int j = 0; j < 60; j++) {
            videoBuffer[i * 240 + j] = RGB5(byte8 >> 3, 15, 15);  // Byte 8
        }
        for(int j = 60; j < 120; j++) {
            videoBuffer[i * 240 + j] = RGB5(15, byte9 >> 3, 15);  // Byte 9
        }
        for(int j = 120; j < 180; j++) {
            videoBuffer[i * 240 + j] = RGB5(15, 15, byte10 >> 3);  // Byte 10
        }
        for(int j = 180; j < 240; j++) {
            videoBuffer[i * 240 + j] = RGB5(byte11 >> 3, byte11 >> 3, byte11 >> 3);  // Byte 11
        }
    }
    
    if(init_result != PGDA_SUCCESS) {
        while(1) VBlankIntrWait();
    }
    
    // Don't use VBlank interrupts - use manual polling instead
    
    // Clear screen to green (streaming mode indicator)
    for(int i = 0; i < 240*160; i++) {
        videoBuffer[i] = RGB5(0, 31, 0);
    }
    
    // Pokemon-style audio initialization sequence
    
    // Clear any existing DMA operations (Pokemon's approach)
    if (REG_DMA1CNT & DMA_REPEAT)
        REG_DMA1CNT = DMA_ENABLE | DMA_IMMEDIATE | DMA32 | DMA_SRC_INC | DMA_DST_FIXED | 4;
    if (REG_DMA2CNT & DMA_REPEAT)
        REG_DMA2CNT = DMA_ENABLE | DMA_IMMEDIATE | DMA32 | DMA_SRC_INC | DMA_DST_FIXED | 4;

    // Reset DMA channels  
    REG_DMA1CNT = DMA32;
    REG_DMA2CNT = DMA32;
    
    // Enable master sound (Pokemon's exact sequence)
    REG_SOUNDCNT_X = 0x80;  // Master enable
    
    // Configure Direct Sound with FIFO reset and timer association (Pokemon style)
    REG_SOUNDCNT_H = 0x0B0E;  // FIFO reset, timer 0, full volume, both speakers
    REG_SOUNDCNT_L = 0x0077;  // Max volume for both sides
    
    // Set sound bias for better quality (Pokemon's critical setting)
    REG_SOUNDBIAS = (REG_SOUNDBIAS & 0x3F) | 0x4000;  // Set bias to 0x40
    
    // Use ONLY the header sample rate - no override
    u32 target_sample_rate = 6750;  // Working fallback rate
    if(audio_decoder.is_initialized) {
        target_sample_rate = audio_decoder.header.sample_rate;  // Use the actual encoded rate
    }
    
    // Pokemon's exact timer calculation 
    // 280896 = cycles per VBlank, calculate samples per VBlank for our rate
    u32 pcm_samples_per_vblank = (target_sample_rate * 280896) / 16777216;
    if(pcm_samples_per_vblank == 0) pcm_samples_per_vblank = 1;  // Safety check
    
    // Pokemon's timer formula: -(280896 / pcmSamplesPerVBlank)
    s16 timer_reload = -(280896 / pcm_samples_per_vblank);
    REG_TM0CNT_L = timer_reload;
    REG_TM0CNT_H = 0x0080;  // Enable timer with 1:1 clock ratio
    
    // Update our refill period to match Pokemon's approach
    pcm_dma_period = PCM_DMA_BUF_SIZE / pcm_samples_per_vblank;
    if(pcm_dma_period == 0) pcm_dma_period = 1;  // Safety check
    
    // Initialize Pokemon-style audio counter
    pcm_dma_counter = pcm_dma_period;
    
    // Fill initial PCM buffer
    fill_pcm_buffer();
    
    // Set up DMA addresses (Pokemon's exact approach)
    REG_DMA1SAD = (u32)pcm_buffer;                     // FIFO_A source (right channel)
    REG_DMA1DAD = (u32)&REG_FIFO_A;                   // FIFO_A destination
    REG_DMA2SAD = (u32)pcm_buffer + PCM_DMA_BUF_SIZE; // FIFO_B source (left channel) 
    REG_DMA2DAD = (u32)&REG_FIFO_B;                   // FIFO_B destination
    
    // Enable DMA for FIFO mode (Pokemon's exact flags)
    REG_DMA1CNT = DMA_ENABLE | DMA_SPECIAL | DMA32 | DMA_DST_FIXED | DMA_REPEAT;
    REG_DMA2CNT = DMA_ENABLE | DMA_SPECIAL | DMA32 | DMA_DST_FIXED | DMA_REPEAT;
    
    while(1) {
        // Restore working Pokemon-style buffer refill
        if(need_refill) {
            need_refill = false;
            
            // Check if DMA is still active (Pokemon's approach)
            if (REG_DMA1CNT & DMA_REPEAT) {
                // DMA is active, do immediate transfer to clear any pending data
                REG_DMA1CNT = DMA_ENABLE | DMA_IMMEDIATE | DMA32 | DMA_SRC_INC | DMA_DST_FIXED | 4;
            }
            if (REG_DMA2CNT & DMA_REPEAT) {
                REG_DMA2CNT = DMA_ENABLE | DMA_IMMEDIATE | DMA32 | DMA_SRC_INC | DMA_DST_FIXED | 4;
            }
            
            // Temporarily turn off DMA
            REG_DMA1CNT = DMA32;
            REG_DMA2CNT = DMA32;
            
            // Refill PCM buffer with fresh data
            fill_pcm_buffer();
            
            // Re-enable DMA in FIFO mode
            REG_DMA1CNT = DMA_ENABLE | DMA_SPECIAL | DMA32 | DMA_DST_FIXED | DMA_REPEAT;
            REG_DMA2CNT = DMA_ENABLE | DMA_SPECIAL | DMA32 | DMA_DST_FIXED | DMA_REPEAT;
        }
        
        // Simple audio activity indicator (not tied to buffer refill timing)
        static u32 display_counter = 0;
        display_counter++;
        if(display_counter >= 60) {  // Update display once per second
            display_counter = 0;
            
            // Simple audio activity indicator in top-left corner
            s8 current_sample = pcm_buffer[0];
            u16 activity_color = current_sample != 0 ? RGB5(0, 31, 0) : RGB5(31, 0, 0);
            
            // Small 10x10 indicator to show audio is working
            for(int i = 0; i < 10; i++) {
                for(int j = 0; j < 10; j++) {
                    videoBuffer[i * 240 + j] = activity_color;
                }
            }
        }
        
        VBlankIntrWait();  // Maintain 60fps timing like Pokemon
    }
    return 0;
}