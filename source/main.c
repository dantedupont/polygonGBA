#include <gba.h>

int main() {
    // Set video mode 3 (240x160, 16-bit color)
    SetMode(MODE_3 | BG2_ENABLE);
    
    // Fill screen with red color (testing)
    u16* videoBuffer = (u16*)0x6000000;
    for(int i = 0; i < 240*160; i++) {
        videoBuffer[i] = RGB15(31, 0, 0); // Red
    }
    
    // Infinite loop (GBA programs never exit)
    while(1) {
        VBlankIntrWait(); // Wait for screen refresh
    }
    
    return 0;
}