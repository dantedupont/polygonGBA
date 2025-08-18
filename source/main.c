#include <gba.h>
#include "polygond_decoder.h"
#include "crumbling_castle_final.h"
#include "gbfs.h"

// Using final optimized GBA compressed Crumbling Castle music data
extern const unsigned char crumbling_castle_final_data[];
extern const unsigned int crumbling_castle_final_data_size;

// Real-time streaming audio system
static pgda_decoder_t audio_decoder;
static audio_buffer_t decode_buffer;

// GSM-style audio buffers - optimized for 11.025kHz playback
#undef AUDIO_BUFFER_SIZE
#define AUDIO_BUFFER_SIZE 2048  // Larger buffer for smoother playback
static s8 __attribute__((aligned(4))) audio_buffer_a[AUDIO_BUFFER_SIZE];
static s8 __attribute__((aligned(4))) audio_buffer_b[AUDIO_BUFFER_SIZE];
static volatile bool need_refill = false;
static volatile u32 current_buffer = 0;  // 0 = buffer_a, 1 = buffer_b

// Audio timing and control
static volatile u32 vblank_counter = 0;
static u32 buffer_refill_period = 6;  // Refill every 6 VBlanks for 11.025kHz

// Audio tracking
static u32 total_decoded = 0;

// GBFS filesystem support
static const GBFS_FILE *track_filesystem = NULL;
static const unsigned char *current_track_data = NULL;
static u32 current_track_size = 0;

// Load track from GBFS or fallback to embedded data
bool load_track(const char *track_name) {
    // Try to load from GBFS first
    if(track_filesystem) {
        current_track_data = gbfs_get_obj(track_filesystem, track_name, &current_track_size);
        if(current_track_data) {
            return true;
        }
    }
    
    // Fallback to embedded data
    current_track_data = crumbling_castle_final_data;
    current_track_size = crumbling_castle_final_data_size;
    return true;
}

// GSM-style DMA buffer switching - critical for clean audio
static void pgda_switch_buffers(const void *src) {
    // Disable DMA first
    REG_DMA1CNT = 0;
    
    // Critical timing - let DMA registers catch up
    asm volatile("eor r0, r0; eor r0, r0" ::: "r0");
    
    // Set new source address and re-enable
    REG_DMA1SAD = (u32)src;
    REG_DMA1DAD = (u32)&REG_FIFO_A;
    REG_DMA1CNT = DMA_DST_FIXED | DMA_SRC_INC | DMA_REPEAT | DMA32 | 
                  DMA_SPECIAL | DMA_ENABLE | 1;
}
// GSM-style VBlank interrupt handler for audio timing
void audio_vblank_handler() {
    static u32 refill_counter = 0;
    
    vblank_counter++;
    refill_counter++;
    
    // Check if it's time to refill buffer
    if(refill_counter >= buffer_refill_period) {
        need_refill = true;
        refill_counter = 0;
    }
}

void fill_audio_buffer() {
    // Select which buffer to fill (the one not currently playing)
    s8* target_buffer = (current_buffer == 0) ? audio_buffer_b : audio_buffer_a;
    u32 buffer_pos = 0;
    
    while(buffer_pos < AUDIO_BUFFER_SIZE) {
        // Calculate how many samples we need for this chunk
        u32 chunk_size = AUDIO_BUFFER_SIZE - buffer_pos;
        if(chunk_size > 1024) {  // Decoder buffer limit
            chunk_size = 1024;
        }
        
        // Decode next chunk from PGDA
        u32 decoded = pgda_decode_samples(&audio_decoder, &decode_buffer, chunk_size);
        
        if(decoded == 0) {
            // End of audio - loop back to beginning
            pgda_reset_decoder(&audio_decoder);
            decoded = pgda_decode_samples(&audio_decoder, &decode_buffer, chunk_size);
            
            if(decoded == 0) {
                // Still no samples - fill remaining with silence
                for(u32 i = buffer_pos; i < AUDIO_BUFFER_SIZE; i++) {
                    target_buffer[i] = 0;
                }
                break;
            }
        }
        
        // Convert decoded samples for GBA Direct Sound
        for(u32 i = 0; i < decoded && buffer_pos < AUDIO_BUFFER_SIZE; i++) {
            // Use 8-bit samples directly
            target_buffer[buffer_pos] = (s8)decode_buffer.samples[i];
            buffer_pos++;
        }
    }
    
    total_decoded += AUDIO_BUFFER_SIZE;  // Track total for debugging
}

int main() {
    // Initialize interrupts and video
    irqInit();
    irqSet(IRQ_VBLANK, audio_vblank_handler);  // Set our VBlank handler
    irqEnable(IRQ_VBLANK);
    
    SetMode(MODE_3 | BG2_ENABLE);
    u16* videoBuffer = (u16*)0x6000000;
    
    // Initialize GBFS filesystem (if available)
    track_filesystem = find_first_gbfs_file(find_first_gbfs_file);
    
    // Load first track (GBFS or embedded fallback)
    load_track("crumbling_castle.pgda");
    
    // Initialize audio decoder with loaded track data
    pgda_error_t init_result = pgda_init_decoder(&audio_decoder, current_track_data, current_track_size);
    
    // Check basic decoder state
    u32 total_samples = pgda_get_total_samples(&audio_decoder);
    u32 num_deltas = audio_decoder.header.num_deltas;
    u32 sample_rate = audio_decoder.header.sample_rate;
    
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
    
    // GSM-style audio initialization
    // Reset DMA channels first
    REG_DMA1CNT = 0;
    
    // Enable master sound circuit (GSM approach)
    REG_SOUNDCNT_X = 0x80;  // Master enable
    
    // Configure Direct Sound for mono playback on FIFO A
    REG_SOUNDCNT_H = 0x0B0E;  // FIFO reset, timer 0, full volume, both speakers
    REG_SOUNDCNT_L = 0x0077;  // Max volume for both sides
    
    // Set sound bias for clean output (critical for quality)
    REG_SOUNDBIAS = (REG_SOUNDBIAS & 0x3F) | 0x4000;  // Set bias to 0x40
    
    // Configure timer for 11.025kHz sample rate (GSM-style)
    u32 target_sample_rate = 11025;  // Our target sample rate
    
    // GSM-style timer calculation - much cleaner than Pokemon approach
    // Timer counts down from reload value at 16.78MHz
    // Reload = 0x10000 - (16777216 / sample_rate)
    u32 timer_reload_val = 0x10000 - (16777216 / target_sample_rate);
    REG_TM0CNT_L = timer_reload_val;
    REG_TM0CNT_H = TIMER_START;  // Enable timer with 1:1 prescaler
    
    // Calculate buffer refill period for 11.025kHz
    // At 60 VBlanks/sec, we need ~184 samples per VBlank
    u32 samples_per_vblank = target_sample_rate / 60;
    buffer_refill_period = AUDIO_BUFFER_SIZE / samples_per_vblank;
    if(buffer_refill_period == 0) buffer_refill_period = 1;  // Safety
    
    // Fill initial audio buffer
    fill_audio_buffer();
    
    // Start DMA with first buffer (GSM-style)
    pgda_switch_buffers(audio_buffer_a);
    current_buffer = 0;
    
    while(1) {
        // GSM-style buffer management
        if(need_refill) {
            need_refill = false;
            
            // Fill the background buffer
            fill_audio_buffer();
            
            // Switch to the newly filled buffer
            if(current_buffer == 0) {
                pgda_switch_buffers(audio_buffer_b);
                current_buffer = 1;
            } else {
                pgda_switch_buffers(audio_buffer_a);
                current_buffer = 0;
            }
        }
        
        // Audio activity indicator using current buffer
        static u32 display_counter = 0;
        display_counter++;
        if(display_counter >= 60) {  // Update display once per second
            display_counter = 0;
            
            // Check current playing buffer for activity
            s8* active_buffer = (current_buffer == 0) ? audio_buffer_a : audio_buffer_b;
            s8 current_sample = active_buffer[0];
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