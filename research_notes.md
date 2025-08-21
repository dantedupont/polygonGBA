# GBA Architecture

## CPU Mode Performance
Note that the GamePak ROM bus is limited to 16bits, thus executing ARM instructions (32bit opcodes) from inside of GamePak ROM would result in a not so good performance. So, it'd be more recommended to use THUMB instruction (16bit opcodes) which'd allow each opcode to be read at once.
(ARM instructions can be used at best performance by copying code from GamePak ROM into internal Work RAM)

## Audio

### DMA (Direct Memory Access)
- Lets hardware move data between memory location without using the CPU. A dedicated "data mover"
- Otherwise CPU would have to move every audio sample and get bogged down
- DMA1 and DMA2 can be used to feed digital sample data to the Sound FIFO's
- DMA transfer requires 4-byte alignment:
    - Memory addresses that are divisible by 4 (like 0x1000, 0x1004, 0x1008)
    - GBA's ARM processor is 32-bit (4 bytes) so it reads memory most efficiently in 4-byte chunks

### Sound DMA (FIFO Timing Mode)
- Upon DMA request: 4 units of 16 bytes are transferred
- DMAs with higher priority (Lower number) should not be operated for longer than 0.25 ms

### FIFO (First In, First Out):
- A hardware buffer with built-in intelligence
- It has 32 sample slots: 32 x 8-bit =  256 bits total
- Read pointer points to next sample to play
- Write pointer points to where new data goes
- Status flags (empty, half-full, full)
- FIFO automatically:
    - 1. Received audio data from DMA
    - 2. Feeds samples to DAC at exactly 18.157 kHz
    - 3. Raises interrupt when half-empty (needs more data)
- Requests more data via DMA

### What is a Buffer?
- A temporary storage for data that is being processed or transferred ( like a water tank)
- Needed for:
    - Speed mismatch: GSM decoder works in chunks, but speakers need continuous streams
    - Audio must never stop but processing takes time
    - Prevents gaps and clicks in audio

### Double Buffering:
- Buffer A play DMA to FIFO while Buffer B fills with new audio data
- On VBlank interrupt (the refresh), the buffers swap so that B plays and A fills
- So every frame, they switch roles, creating continuous audio 
- Each buffer contains exactly one frame's worth of audio
* signed char double_buffers[2][608] __attribute__((aligned(4)));
- Why 608?
    - Sample Rate: 18,157 Hz
    - Frame Rate: 60Fps
    - Samples per frame = sample rate / frame rate ~= 304 samples
    - x 2 bytes per sample = 608 bytes
- Why 2 bytes per sample?
    - Even though out audio is 8-bit, GBA's Direct Sound expects 16-bit samples

### VBlank: 
Vertical Blank Interval - occurs 60 times per second when the GBA's LCD finishes drawing a frame. This allows it to be a timing reference for synchronizing audio and graphics. 
  VBlank 1:
  - Start with GSM frame A (160 samples)
  - Use samples 0-159 from frame A
  - Need 144 more samples → decode frame B
  - Use samples 0-143 from frame B
  - Total: 304 samples (160 + 144)
  VBlank 2:
  - Continue with frame B samples 144-159 (16 remaining)
  - Need 288 more → decode frame C (160 samples)
  - Use samples 0-127 from frame C
  - Total: 304 samples (16 + 160 + 128)

## Video
Modes:
0,1,2 - tile based modes -> good for scrolling background effect
3,4,5 - bitmap modes
"Mode 7" zoom and rotation effects

# polygonAudio Codec Notes

## What is a codec?
- Coder + Decoder
- Software that encodes (compresses) and decodes (decompresses) data

## GSM (Global System for Mobile Communications) 
-> industry standard telecom codec
- Originally for telephony
- It uses the RPE-LTP algorithm (Regular Pule Excitation with Long Term Prediction)
- Combines ltp and rpe to generate 160 audio signals
- Reads 33-byte GSM frames from GBFS
    - The fuel size MUST be a multiple of 33 bytes
    - Each frame is exactly 33 bytes of compressed audio
- gsm_decode() is the main decoding function (160 samples per frame)
- Double buffering: 608-byte buffers allows smooth playback
- GSM encoding is sample-rate agnostic at algorithm level which is why we can feed it a 18157Hz input even though the format expects 8000Hz
- So we "lie" to sox (a audio converting too) for it to feed GSM the higher quality audio
- Result: 10:1 Conversion (~4.5Mb for side A) using 70% CPU
- Math operations for LCP are too complex and consume too much operating power

### Linear Predictive Coding (LCP) 
is the core technique behind GSM's  RPE-LTP compression:
- Predicts future audio samples
- Encodes only the prediction
- Achieves 33-byte frames

### Magic bytes: 
- signatures at the beginning of file that identify the file format
  -> I spent some time debugging this when attempting to use .pgda format, making sure the right magic bytes were being used.
- Since file extensions don't exist in ROM, the system can only identify formats via raw bytes 
- How does GSM not have magic bytes?
    - Since it was designed for telephone streams, there's no time for headers
    - Minimal overhead is important in phone networks

## ADPCM (Adaptive Differential Pulse-Code Modulation)
- Optimized for storing digitized voice data
- "Delta encoding": Stores differences between consecutive audio samples rather than full sample
- This is also simpler than the LCP approach that GSM takes. 
- 8AD: ADPCM codec adapted for gameboy by Pin Eight
- Uses 4-bit codes to represent those differences efficiently
- Result: 6% CPU usage! At 4:1 compression (~10.8Mb for side A)

## Verdict: 
The loss in compression ratio is worth it for the massive savings in CPU usage
Importantly - its a concrete excuse to keep my Side A / Side B division