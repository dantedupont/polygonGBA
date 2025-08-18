#include <gba.h>
#include "gbfs.h"
#include "libgsm.h"

// GSM playback tracker
static GsmPlaybackTracker playback;

// GBFS filesystem support
static const GBFS_FILE *track_filesystem = NULL;

int main() {
    // Initialize interrupts and video
    irqInit();
    irqEnable(IRQ_VBLANK);
    
    SetMode(MODE_3 | BG2_ENABLE);
    u16* videoBuffer = (u16*)0x6000000;
    
    // Initialize GBFS filesystem
    track_filesystem = find_first_gbfs_file(find_first_gbfs_file);
    
    // Initialize GSM playback system
    if (initPlayback(&playback) == 0) {
        // Success - show green screen
        for(int i = 0; i < 240*160; i++) {
            videoBuffer[i] = RGB5(0, 31, 0);
        }
    } else {
        // Failed - show red screen
        for(int i = 0; i < 240*160; i++) {
            videoBuffer[i] = RGB5(31, 0, 0);
        }
        while(1) VBlankIntrWait();
    }
    
    // Main loop with proper GSM timing
    while(1) {
        // Use original GSMPlayer-GBA timing: 
        // advancePlayback() BEFORE VBlankIntrWait(), writeFromPlaybackBuffer() AFTER
        advancePlayback(&playback, &DEFAULT_PLAYBACK_INPUT_MAPPING);
        VBlankIntrWait();
        writeFromPlaybackBuffer(&playback);
        
        // Show playback status (less frequently to reduce CPU load)
        static u32 display_counter = 0;
        display_counter++;
        if(display_counter >= 60) {
            display_counter = 0;
            
            // Show playing status
            u16 status_color = playback.playing ? RGB5(0, 31, 0) : RGB5(31, 31, 0);
            for(int i = 0; i < 10; i++) {
                for(int j = 0; j < 10; j++) {
                    videoBuffer[i * 240 + j] = status_color;
                }
            }
            
            // Show buffer switch timing debug (small indicator)
            u16 timing_color = RGB5(15, 15, 31);  // Light blue
            for(int i = 0; i < 5; i++) {
                for(int j = 10; j < 15; j++) {
                    videoBuffer[i * 240 + j] = timing_color;
                }
            }
        }
    }
    
    return 0;
}