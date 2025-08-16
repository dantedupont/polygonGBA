# GBA Audio Implementation: Critical Fixes and Technical Analysis

## Executive Summary

We successfully fixed a broken GBA audio implementation by adopting Pokemon Emerald's proven audio architecture. The key breakthrough was understanding that GBA Direct Sound requires active buffer management, proper hardware register configuration, and precise timing - not just "fire and forget" DMA transfers.

## Critical Differences: Our Broken vs Pokemon's Working Implementation

### 1. **Buffer Architecture**
```c
// ❌ Our Original (Broken)
static s16 stream_buffer_a[32];  // 16-bit samples, tiny buffer
static s16 stream_buffer_b[32];  // No alignment specified

// ✅ Pokemon's Working Approach  
#define PCM_DMA_BUF_SIZE 1584
static s8 __attribute__((aligned(4))) pcm_buffer[PCM_DMA_BUF_SIZE * 2];  // 8-bit, double buffer, aligned
```

**Why This Mattered:**
- **Sample Format**: GBA hardware expects 8-bit signed samples for Direct Sound, not 16-bit
- **Alignment**: DMA transfers require 4-byte alignment for optimal performance  
- **Buffer Size**: 32 samples = ~2.4ms at 13379Hz. Way too small, causing constant underruns

### 2. **Sound Bias Configuration**
```c
// ❌ Our Original (Missing)
// No sound bias configuration

// ✅ Pokemon's Approach
REG_SOUNDBIAS = (REG_SOUNDBIAS & 0x3F) | 0x4000;  // Set bias to 0x40
```

**What This Does:**
Sound bias eliminates DC offset and improves audio quality by setting the DAC's center point. Without it, audio sounds distorted and "crushed."

### 3. **Timer Calculation Method**
```c
// ❌ Our Original (Incorrect)
u16 timer0_reload = 65536 - (16777216 / sample_rate);  // CPU frequency approach

// ✅ Pokemon's Approach  
REG_TM0CNT_L = -(280896 / 224);  // LCD refresh cycle approach
```

**Why Pokemon's Method Works:**
- **280896**: Exact cycles per VBlank period (59.7275 Hz)
- **224**: Samples per VBlank for 13379 Hz (280896 / 13379 * 59.7275)
- This synchronizes audio with display refresh, preventing artifacts

### 4. **DMA Configuration and Management**
```c
// ❌ Our Original (Fire and Forget)
REG_DMA1CNT = DMA_ENABLE | DMA_SPECIAL | DMA16 | DMA_REPEAT | 32;
// Set once, never managed

// ✅ Pokemon's Approach (Active Management)
// Initial setup
REG_DMA1CNT = DMA_ENABLE | DMA_SPECIAL | DMA32 | DMA_DST_FIXED | DMA_REPEAT;

// VBlank management  
if (REG_DMA1CNT & DMA_REPEAT) {
    REG_DMA1CNT = DMA_ENABLE | DMA_IMMEDIATE | DMA32 | DMA_SRC_INC | DMA_DST_FIXED | 4;
}
REG_DMA1CNT = DMA32;  // Disable
// Refill buffer
REG_DMA1CNT = DMA_ENABLE | DMA_SPECIAL | DMA32 | DMA_DST_FIXED | DMA_REPEAT;  // Re-enable
```

**Critical Elements:**
- **DMA_SPECIAL**: Triggers on timer overflow (not DMA_IMMEDIATE)
- **DMA32**: 32-bit transfers for FIFO efficiency
- **Active Management**: DMA disabled during buffer refills to prevent corruption

### 5. **Buffer Management Strategy**
```c
// ❌ Our Original (Infrequent, Static)
if(frame_counter >= 60) {  // Once per second
    fill_stream_buffer(stream_buffer_a, 32);
}

// ✅ Pokemon's Approach (Frequent, Continuous)
if(frame_counter >= 4) {  // Every 4 frames (~15Hz)
    // DMA cleanup sequence
    // Refill entire double buffer with continuous data
    fill_pcm_buffer();  // Fills 3168 samples seamlessly
}
```

**Why Frequency Matters:**
- **Buffer Duration**: 1584 samples at 13379 Hz = ~118ms
- **Refill Rate**: Every 4 frames = ~67ms  
- **Safety Margin**: Refill before buffer exhaustion prevents audio dropouts

## How Each Component Creates Sound

### 1. **Timer 0: The Audio Clock**
```c
REG_TM0CNT_L = -(280896 / 224);  // ~-1254 
REG_TM0CNT_H = 0x0080;           // Enable, 1:1 prescaler
```

**Function**: Generates precise 13379 Hz timing pulses. Each timer overflow triggers DMA to move one sample from buffer to FIFO.

### 2. **DMA Channel 1: The Data Pipeline**
```c
REG_DMA1SAD = (u32)pcm_buffer;           // Source: our sample buffer
REG_DMA1DAD = (u32)&REG_FIFO_A;         // Destination: hardware FIFO
REG_DMA1CNT = DMA_ENABLE | DMA_SPECIAL | DMA32 | DMA_DST_FIXED | DMA_REPEAT;
```

**Function**: 
- **DMA_SPECIAL**: Wait for timer overflow signal
- **DMA_DST_FIXED**: FIFO address never changes
- **DMA_REPEAT**: Automatically advance source pointer for next sample

### 3. **Sound Control Registers: The Audio Pipeline**
```c
REG_SOUNDCNT_X = 0x80;           // Master enable
REG_SOUNDCNT_H = 0x0B0E;         // Direct Sound A: timer 0, both speakers, FIFO reset
REG_SOUNDCNT_L = 0x0077;         // Max volume left/right
REG_SOUNDBIAS = 0x4000;          // DC bias for clean output
```

**Audio Flow**:
1. Timer 0 overflow → DMA transfers sample to REG_FIFO_A
2. FIFO feeds sample to DAC at timer rate
3. DAC converts to analog, applies bias
4. Output to speakers at controlled volume

### 4. **PCM Buffer: The Continuous Data Source**
```c
void fill_pcm_buffer() {
    for(u32 i = 0; i < PCM_DMA_BUF_SIZE * 2; i++) {  // Fill both halves
        u32 phase = (total_decoded + i) % 64;         // Continuous sine wave
        // Generate sample...
        pcm_buffer[i] = sample >> 1;                  // 8-bit signed output
    }
    total_decoded += PCM_DMA_BUF_SIZE;               // Advance phase
}
```

**Seamless Playback**: Phase counter ensures sine wave continues smoothly across buffer boundaries.

## The Complete Audio System Architecture

```
[Timer 0] → [DMA 1] → [FIFO A] → [DAC] → [Speakers]
    ↑           ↑         ↑        ↑        ↑
13379 Hz    PCM Buffer  Hardware  Analog   Audio
timing      Management   Queue    Output   Output

VBlank Interrupt ↓
Buffer Refill (67ms intervals)
```

## Why Our Original Implementation Failed

1. **Wrong Sample Format**: 16-bit samples to 8-bit hardware
2. **No Sound Bias**: Caused "crushed" audio quality  
3. **Tiny Buffers**: Constant underruns caused tearing
4. **Incorrect Timer Math**: Audio/timer desynchronization artifacts
5. **No Buffer Management**: Fire-and-forget approach failed
6. **Wrong DMA Flags**: DMA16 instead of DMA32, wrong transfer modes

## Key Takeaways

**GBA Audio Requires**:
- **Active Management**: Not fire-and-forget, but continuous buffer monitoring
- **Precise Timing**: Hardware-synchronized sample rates using LCD timing  
- **Proper Register Configuration**: Every register matters for clean output
- **Sufficient Buffering**: Large enough buffers to handle refill latencies

The success demonstrates that **copying proven working implementations** (like Pokemon Emerald) is often more effective than theoretical approaches when dealing with complex hardware timing requirements.

## Implementation Files

- `source/main.c:176-220` - Pokemon-style audio initialization sequence
- `source/main.c:27-48` - PCM buffer management and continuous sine wave generation  
- `source/main.c:197-220` - VBlank-synchronized buffer refill logic

## References

- Pokemon Emerald decompilation project (pokeemerald repository)
- GBA Hardware Manual - Direct Sound specifications
- DevkitARM libgba documentation - DMA and timer configuration

---

*This documentation was created during the debugging process that successfully implemented working GBA audio by analyzing and adopting Pokemon Emerald's proven audio architecture.*