#include <gba.h>

int main() {
    // Initialize interrupts
    irqInit();
    irqEnable(IRQ_VBLANK);
    
    SetMode(MODE_3 | BG2_ENABLE);
    u16* videoBuffer = (u16*)0x6000000;
    u16 color = RGB5(31, 0, 0); // Start with red
    
    while(1) {
        scanKeys(); // This should work now
        u16 keys = keysDown();
        
        // Change colors with buttons
        if(keys & KEY_A) color = RGB5(0, 31, 0);     // Green (X key)
        if(keys & KEY_B) color = RGB5(0, 0, 31);     // Blue (Z key)  
        if(keys & KEY_START) color = RGB5(31, 31, 31); // White (Enter)
        if(keys & KEY_SELECT) color = RGB5(31, 31, 0); // Yellow (Backspace)
        if(keys & KEY_L) color = RGB5(31, 0, 31);     // Magenta
        if(keys & KEY_R) color = RGB5(0, 31, 31);     // Cyan
        
        // Fill screen with current color
        for(int i = 0; i < 240*160; i++) {
            videoBuffer[i] = color;
        }
        
        VBlankIntrWait();
    }
    return 0;
}