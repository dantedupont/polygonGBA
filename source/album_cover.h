#ifndef ALBUM_COVER_H
#define ALBUM_COVER_H

#include <gba.h>

// Album cover display configuration - displays Polygondwanaland artwork
#define ALBUM_COVER_CENTER_X 120        // Center X position for album art
#define ALBUM_COVER_CENTER_Y 80         // Center Y position for album art
#define ALBUM_COVER_SIZE 128            // Album cover display size (128x128)
#define SHARED_CHAR_BASE 1              // Character base for album art tiles

// Initialize the album cover display (loads artwork and sets up background)
void init_album_cover(void);

// Cleanup the album cover display (hides background when switching modes)
void cleanup_album_cover(void);

#endif // ALBUM_COVER_H