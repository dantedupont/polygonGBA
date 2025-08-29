# GBA 8x8 Bitmap Font System

## Overview

This project uses a custom 8x8 bitmap font system for rendering text on the Game Boy Advance. The font system is designed to be simple, efficient, and suitable for the GBA's limited resources.

## Architecture

The font system consists of two main files:

- **`font.h`** - Header file with function declarations and external variable declarations
- **`font.c`** - Implementation file containing the font data and rendering function

## Font Data Structure

### Character Encoding
- Supports printable ASCII characters (32-126)
- Total of 95 characters stored in the `font_data` array
- Each character is indexed by `(ascii_value - 32)`

### Bitmap Format
Each character is represented as an 8x8 pixel bitmap:
```c
const u8 font_data[95][8] = {
    // Each character is 8 bytes
    {0x3C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00}, // Example: Letter 'O'
    //  ^^    ^^    ^^    ^^    ^^    ^^    ^^    ^^
    // Row0  Row1  Row2  Row3  Row4  Row5  Row6  Row7
};
```

### Bit Layout
Each byte represents one horizontal row of the character:
- **Bit 7 (0x80)**: Leftmost pixel
- **Bit 6 (0x40)**: Second pixel from left
- **Bit 5 (0x20)**: Third pixel from left
- **Bit 4 (0x10)**: Fourth pixel from left
- **Bit 3 (0x08)**: Fifth pixel from left
- **Bit 2 (0x04)**: Sixth pixel from left
- **Bit 1 (0x02)**: Seventh pixel from left
- **Bit 0 (0x01)**: Rightmost pixel

### Example Character Breakdown
Letter 'A' (ASCII 65, index 33):
```
Binary:     Hex:   Visual:
00011000 -> 0x18 -> ...##...
00111100 -> 0x3C -> ..####..
01100110 -> 0x66 -> .##..##.
01100110 -> 0x66 -> .##..##.
01111110 -> 0x7E -> .######.
01100110 -> 0x66 -> .##..##.
01100110 -> 0x66 -> .##..##.
00000000 -> 0x00 -> ........
```

## Text Rendering Function

### Function Signature
```c
void draw_text(u16* buffer, int x, int y, const char* text, u16 color);
```

### Parameters
- **`buffer`**: Pointer to GBA video buffer (240x160 pixels, 16-bit RGB555 format)
- **`x, y`**: Starting position for text (top-left corner of first character)
- **`text`**: Null-terminated string to render
- **`color`**: 16-bit color value in RGB555 format (use `RGB5(r,g,b)` macro)

### Rendering Process
1. **Character Iteration**: Loop through each character in the input string
2. **ASCII Validation**: Only render characters in range 32-126
3. **Index Calculation**: Convert ASCII to array index using `(ascii - 32)`
4. **Pixel Rendering**: For each 8x8 character:
   - Loop through 8 rows (y-axis)
   - Loop through 8 columns (x-axis)
   - Test each bit using `(row_data & (0x80 >> column))`
   - Set pixel color if bit is set
5. **Advancement**: Move to next character position (8 pixels right)

### Boundary Checking
- **Screen Width**: Stops rendering when `char_x >= 240 - 8`
- **Screen Height**: Clips pixels when `y + py >= 160`
- **Character Width**: Clips pixels when `char_x + px >= 240`

## Usage Examples

### Basic Text Rendering
```c
#include \"font.h\"

// In your main function
u16* videoBuffer = (u16*)0x6000000;

// White text at position (10, 50)
draw_text(videoBuffer, 10, 50, \"Hello World!\", RGB5(31, 31, 31));

// Yellow text at position (10, 70)
draw_text(videoBuffer, 10, 70, \"Track Info\", RGB5(31, 31, 0));

// Red text at position (10, 90)
draw_text(videoBuffer, 10, 90, \"Error!\", RGB5(31, 0, 0));
```

### Color Format (RGB555)
The GBA uses 15-bit color in RGB555 format:
- **Red**: 5 bits (0-31)
- **Green**: 5 bits (0-31)  
- **Blue**: 5 bits (0-31)

Use the `RGB5(r, g, b)` macro to create colors:
```c
RGB5(31, 31, 31)  // White
RGB5(31, 0, 0)    // Red
RGB5(0, 31, 0)    // Green
RGB5(0, 0, 31)    // Blue
RGB5(31, 31, 0)   // Yellow
RGB5(0, 0, 0)     // Black
```

## Performance Considerations

### Memory Usage
- **Font Data**: 95 characters × 8 bytes = 760 bytes total
- **No Dynamic Allocation**: All data is compile-time constant
- **ROM Storage**: Font data stored in ROM, not RAM

### Rendering Speed
- **Direct Pixel Access**: Writes directly to video buffer
- **Bit Operations**: Uses efficient bit masking for pixel testing
- **No Buffering**: Immediate rendering to screen
- **Character Spacing**: Fixed 8-pixel advance (no kerning)

### Optimization Tips
- **Limit Text Updates**: Only redraw text when necessary
- **Clear Background**: Clear text areas before redrawing to avoid artifacts
- **Batch Operations**: Group multiple text draws together
- **Screen Boundaries**: Pre-calculate text positions to avoid clipping

## Character Set Coverage

### Supported Characters
- **Digits**: 0-9
- **Uppercase Letters**: A-Z
- **Lowercase Letters**: a-z
- **Punctuation**: ! \" # $ % & ' ( ) * + , - . / : ; < = > ? @ [ \\ ] ^ _ ` { | } ~
- **Space**: ASCII 32

### Special Characters
Some characters have simplified designs due to 8x8 pixel constraints:
- Complex punctuation may be simplified
- Lowercase letters use proper descenders where possible
- Numbers are designed for clarity at small size

## Integration Notes

### Build Requirements
- Include both `font.h` and `font.c` in your project
- Link `font.c` during compilation
- Ensure `gba_types.h` is available for type definitions

### Header Dependencies
```c
// In your main source file
#include \"font.h\"
```

### Video Mode Requirements
- Works with any GBA video mode that provides direct framebuffer access
- Tested with Mode 3 (240x160, 16-bit per pixel)
- Buffer format must be 16-bit RGB555

## Troubleshooting

### Common Issues
1. **Garbled Text**: Verify character data and bit order
2. **Missing Characters**: Check ASCII range (32-126 only)
3. **Wrong Colors**: Ensure RGB555 format and proper color macros
4. **Text Cutoff**: Check screen boundary calculations
5. **Compilation Errors**: Verify font.c is included in build

### Debug Tips
- Test with simple characters first (letters, numbers)
- Use distinct colors to verify rendering
- Check video buffer pointer validity
- Verify video mode setup before text rendering

This font system provides a solid foundation for text display in GBA homebrew projects while maintaining simplicity and efficiency.