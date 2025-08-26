# Auto-Play System: Seamless Track Transitions

## Overview

The Polygon GBA music visualizer implements automatic track advancement that creates a continuous listening experience. When one track ends, the system seamlessly transitions to the next track without user intervention, ultimately looping back to the first track after the last one completes.

## Implementation Architecture

### Core Components

**Track State Management**
```c
// Track management in 8ad_player.c
static int current_track = 0;         // Current track index (0-3)
static int playing = 0;               // Master playback state
static int paused = 0;                // Pause state (separate from playing)
static int auto_advanced = 0;        // One-shot flag preventing duplicate advances
```

**Track Boundary Detection**
```c
// Track data pointers for boundary detection
static const unsigned char *track_data_start;  // Beginning of current track
static const unsigned char *track_data_end;    // End of current track data
```

### Auto-Advance Logic Flow

The auto-play system operates through precise detection of track end conditions within the audio mixer:

```c
void mixer_8ad(void)
{
  // 1. Handle pause state (highest priority)
  if (paused) {
    memset(mixbuf[cur_mixbuf], 0, MIXBUF_SIZE);
    return;
  }
  
  // 2. Check for early end detection and auto-advance
  if (playing && ad.data && ad.data >= track_data_end && !auto_advanced) {
    auto_advanced = 1;
    next_track_8ad(); // Seamlessly advance to next track
    return;
  }
  
  // 3. Normal playback processing
  if (ad.data + AUDIO_FRAME_BYTES <= track_data_end) {
    // Continue normal audio decoding and playback
    decode_ad(&ad, mixbuf[cur_mixbuf], ad.data, MIXBUF_SIZE);
    ad.data += AUDIO_FRAME_BYTES;
  } else {
    // 4. Fallback end-of-track detection
    if (!auto_advanced) {
      auto_advanced = 1;
      next_track_8ad();
      return;
    }
  }
}
```

### Track Transition Process

When auto-advance triggers, the system executes a complete track transition:

**1. Next Track Selection with Wraparound**
```c
void next_track_8ad(void)
{
  if (fs) {
    int total_tracks = gbfs_count_objs(fs);
    current_track = (current_track + 1) % total_tracks;  // Wraparound: 3 → 0
    start_8ad_track(current_track);
  }
}
```

**2. Clean Track Initialization**
```c
void start_8ad_track(int track_num)
{
  // Load new track data
  const unsigned char *track_data = gbfs_get_nth_obj(fs, track_num, NULL, &track_len);
  
  // Reset decoder state for new track
  ad.data = track_data;
  ad.last_sample = 0;
  ad.last_index = 0;
  
  // Update track boundaries
  track_data_start = track_data;
  track_data_end = track_data + track_len;
  current_track = track_num;
  
  // Reset auto-advance and pause states
  playing = 1;
  paused = 0;
  auto_advanced = 0;  // Critical: allows next track to auto-advance
  
  // Clear visualization data to prevent sticking bars
  for(int i = 0; i < 7; i++) {
    spectrum_accumulators_8ad[i] = 0;
  }
  spectrum_sample_count_8ad = 0;
  reset_spectrum_visualizer_state();
}
```

## Key Design Decisions

### Dual End Detection Strategy

The system uses two complementary methods to detect track endings:

**Method 1: Proactive Detection**
- Checks `ad.data >= track_data_end` before audio processing
- Triggers when decoder position reaches or exceeds track boundary
- Primary mechanism for smooth transitions

**Method 2: Fallback Detection**  
- Activates when `ad.data + AUDIO_FRAME_BYTES > track_data_end`
- Catches edge cases where track ends mid-frame
- Ensures no track is ever skipped

### One-Shot Flag Architecture

The `auto_advanced` flag prevents duplicate track advances:
- **Set to 1**: When auto-advance triggers for current track
- **Reset to 0**: When new track starts via `start_8ad_track()`
- **Purpose**: Prevents rapid multiple advances during track end processing

### Pause System Integration

Auto-play respects pause state:
```c
// Auto-advance only occurs when not paused
if (playing && ad.data && ad.data >= track_data_end && !auto_advanced) {
  // Only advance if user hasn't paused playback
}

// Pause handling takes precedence
if (paused) {
  // Fill with silence, no auto-advance processing
  return;
}
```

## Track Loop Sequence

For a 4-track album (Polygondwanaland Side A):

```
Track 0: "Crumbling Castle" → Track 1: "Polygondwanaland" 
                           ↓
Track 3: "Deserted Dunes" ← Track 2: "Castle In The Air"
         ↓ (auto-advance)
Track 0: "Crumbling Castle" (loop continues...)
```

## Visualization System Integration

### Spectrum Data Reset

During track transitions, the system prevents visualization artifacts:

```c
// Clear spectrum accumulators to stop bar sticking
for(int i = 0; i < 7; i++) {
  spectrum_accumulators_8ad[i] = 0;
}
spectrum_sample_count_8ad = 0;

// Reset spectrum visualizer internal state
reset_spectrum_visualizer_state();
```

### Seamless Visual Continuity

- **Bars drop to zero** during track transition (prevents sticking)
- **New track data** immediately drives fresh visualizations
- **No visual glitches** or frozen spectrum displays
- **Continuous waveform** animation flows through transitions

## Performance Characteristics

### CPU Impact
- **Minimal overhead**: Auto-advance adds ~2-3 conditional checks per audio frame
- **No blocking operations**: Track transitions happen within single mixer call
- **Efficient wraparound**: Modulo operation for seamless looping

### Memory Usage
- **Static allocation**: No dynamic memory allocation during transitions
- **Clean state**: Each track starts with fresh decoder and visualization state
- **No memory leaks**: Proper cleanup and reset procedures

## User Experience

### Continuous Listening
- **Uninterrupted playback**: Seamless transitions between all tracks
- **Complete album experience**: All 4 tracks play automatically in sequence
- **Infinite loop**: Returns to first track after last track completes

### Manual Control Preserved
- **Manual track changes**: Left/Right arrows still work during auto-play
- **Pause functionality**: A button pauses current track, auto-advance waits
- **Visualization switching**: Up/Down arrows work throughout auto-play sequence

## Debug and Monitoring

The system includes debug markers for development:

```c
// Debug info stored in sprite palette for monitoring
SPRITE_PALETTE[28] = (track_length >> 16) & 0xFFFF;     // Track length
SPRITE_PALETTE[29] = (current_position >> 12) & 0xFFFF; // Current position
SPRITE_PALETTE[30] = 0xBEEF;  // Auto-advance trigger marker
SPRITE_PALETTE[31] = current_track; // Track that triggered auto-advance
```

## Future Enhancements

### Potential Improvements
- **Cross-fade transitions**: Brief audio overlap between tracks
- **Shuffle mode**: Random track order with repeat prevention
- **Track repeat**: Option to repeat single track instead of advancing
- **Playlist support**: Multiple album support with seamless transitions

### Architecture Extensibility
- **Modular design**: Easy to add new transition modes
- **Clean interfaces**: Well-defined boundaries between components
- **State management**: Robust state tracking supports complex playback modes

## Conclusion

The auto-play system transforms the Polygon GBA visualizer from a single-track player into a complete album experience. By implementing dual end detection, proper state management, and seamless visualization transitions, users can enjoy uninterrupted music with continuously flowing visualizations.

The system's robust architecture ensures reliable operation while maintaining the performance characteristics essential for real-time audio processing and visualization on GBA hardware.