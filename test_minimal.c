#include <gba.h>

int main() {
    // Set video mode
    SetMode(MODE_3 | BG2_ENABLE);
    
    // Get video buffer
    u16* videoBuffer = (u16*)0x6000000;
    
    // Fill screen with bright green to show it's working
    for(int i = 0; i < 240*160; i++) {
        videoBuffer[i] = RGB5(0, 31, 0);  // Bright green
    }
    
    // Infinite loop
    while(1) {
        VBlankIntrWait();
    }
    
    return 0;
}