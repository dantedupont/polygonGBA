#include "album_cover.h"
#include "polygondwanaland_128.h"
#include <gba_dma.h>
#include <gba_systemcalls.h>

// Album cover display state
static bool is_initialized = 0;

void init_album_cover(void) {
    if (is_initialized) return;
    
    // Set up BG0 for album cover (regular tiled background) 
    // Use different screen base to avoid conflict with BG2 (main background)
    // BG1 uses SCREEN_BASE(30), BG2 uses SCREEN_BASE(29), so use SCREEN_BASE(28)
    REG_BG0CNT = SCREEN_BASE(28) | CHAR_BASE(2) | BG_PRIORITY(1) | BG_256_COLOR | BG_SIZE_0;
    
    // Position the 128x128 artwork correctly on the 240x160 screen
    // User feedback: horizontal = 0, vertical = 12 for proper centering
    REG_BG0HOFS = 0;   // Horizontal positioning
    REG_BG0VOFS = 12;  // Vertical positioning
    
    // Load the properly converted Polygondwanaland artwork
    // 1. Load palette with RGB channel fix
    while (REG_DMA3CNT & DMA_ENABLE) VBlankIntrWait();
    
    // Load the complete original album palette first
    for (int i = 0; i < 256; i++) {
        u16 originalColor = polygondwanaland_128Pal[i];
        u16 r = (originalColor & 0x1F);           // Extract R
        u16 g = (originalColor >> 5) & 0x1F;     // Extract G  
        u16 b = (originalColor >> 10) & 0x1F;    // Extract B
        
        // Swap R and B channels to fix the color mapping
        BG_PALETTE[i] = b | (g << 5) | (r << 10);  // B, G, R instead of R, G, B
    }
    
    // Dynamic background and text colors based on current track
    // Import track detection function
    extern int is_final_track_8ad(void);
    
    if (is_final_track_8ad()) {
        // "The Fourth Color" - white background, black text
        BG_PALETTE[0] = RGB5(31, 31, 31);  // White background (using index 255)
        BG_PALETTE[16] = RGB5(0, 0, 0);    // Color 0 of palette 1: transparent/black  
        BG_PALETTE[17] = RGB5(0, 0, 0);    // Color 1 of palette 1: black text
    } else {
        // All other tracks - Game Boy colors
        BG_PALETTE[0] = RGB5(19, 23, 1);   // Game Boy bright green background (using index 255)
        BG_PALETTE[16] = RGB5(0, 0, 0);    // Color 0 of palette 1: transparent/black
        BG_PALETTE[17] = RGB5(1, 7, 1);    // Color 1 of palette 1: darkest Game Boy green text
    }
    
    // 2. Load tile data to character base 2 FIRST
    while (REG_DMA3CNT & DMA_ENABLE) VBlankIntrWait();
    dmaCopy(polygondwanaland_128Tiles, PATRAM8(2, 0), polygondwanaland_128TilesLen);
    
    // 3. POST-PROCESS: Remap any pixels using palette index 0 to use index 254
    // This prevents album artwork from using the background color and avoids conflict with BG_PALETTE[1]
    u8* tileData = (u8*)PATRAM8(2, 0);
    for (int i = 0; i < polygondwanaland_128TilesLen; i++) {
        if (tileData[i] == 0) {
            tileData[i] = 254; // Remap palette 0 to palette 254 (safe high index)
        }
    }
    
    // Make sure palette index 254 has the proper black color for album artwork
    BG_PALETTE[254] = RGB5(0, 0, 0); // Pure black for album artwork pixels
    
    // CRITICAL: Apply Game Boy color corrections after loading album palette
    // This ensures the background and text colors are set correctly
    update_album_cover_colors();
    
    // No pixel manipulation needed - grit should handle palette reservation
    
    // 3. Set up screen entries at our allocated screen base
    u16* screenEntries = SCREEN_BASE_BLOCK(28);
    
    // Clear screen first - use tile index that doesn't exist to ensure transparency
    for (int i = 0; i < 32 * 32; i++) {
        screenEntries[i] = 0;  // Tile 0 should be transparent
    }
    
    // CRITICAL: Ensure tile 0 in album character base is completely transparent
    u8* albumTileData = (u8*)CHAR_BASE_ADR(2);  // Album uses character base 2
    for (int i = 0; i < 64; i++) {  // Clear first 64 bytes (tile 0)
        albumTileData[i] = 0;  // All pixels use palette index 0 (transparent)
    }
    
    // Load the central 16x16 tiles from the 32x32 GRIT tilemap
    // GRIT generates 32x32 tilemaps even for smaller images, with artwork in center
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            // Screen position: center the 16x16 area on 30x20 screen
            int screen_x = 7 + x;  // Center horizontally: (30-16)/2 = 7
            int screen_y = 2 + y;  // Center vertically: (20-16)/2 = 2
            int screen_pos = screen_y * 32 + screen_x;
            
            // Tilemap position: try top-left corner of 32x32 GRIT tilemap
            int tilemap_x = x;  // Start from top-left
            int tilemap_y = y;  // Start from top-left 
            int tilemap_pos = tilemap_y * 32 + tilemap_x;
            
            screenEntries[screen_pos] = polygondwanaland_128Map[tilemap_pos];
        }
    }
    
    is_initialized = 1;
}

void cleanup_album_cover(void) {
    if (!is_initialized) return;
    
    // Completely disable BG0 by clearing its enable bit in the main mode
    // and reset scrolling offsets
    REG_BG0HOFS = 0;
    REG_BG0VOFS = 0;
    
    // Clear the tilemap to remove any displayed tiles
    u16* screenEntries = SCREEN_BASE_BLOCK(28);
    for (int i = 0; i < 32 * 32; i++) {
        screenEntries[i] = 0;
    }
    
    // Set BG0 to lowest priority as backup
    REG_BG0CNT = SCREEN_BASE(28) | CHAR_BASE(2) | BG_PRIORITY(3) | BG_256_COLOR | BG_SIZE_0;
    
    is_initialized = 0;
}

void update_album_cover_colors(void) {
    // Import track detection function
    extern int is_final_track_8ad(void);
    
    if (is_final_track_8ad()) {
        // "The Fourth Color" - white background, black text
        BG_PALETTE[0] = RGB5(31, 31, 31);  // White background (using index 255)
        BG_PALETTE[16] = RGB5(0, 0, 0);    // Color 0 of palette 1: transparent/black  
        BG_PALETTE[17] = RGB5(0, 0, 0);    // Color 1 of palette 1: black text
    } else {
        // All other tracks - Game Boy colors
        BG_PALETTE[0] = RGB5(19, 23, 1);   // Game Boy bright green background (using index 255)
        BG_PALETTE[16] = RGB5(0, 0, 0);    // Color 0 of palette 1: transparent/black
        BG_PALETTE[17] = RGB5(1, 7, 1);    // Color 1 of palette 1: darkest Game Boy green text
    }
}

// No update or render functions needed - the background displays automatically once initialized