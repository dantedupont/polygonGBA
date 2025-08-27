const fs = require('fs');
const path = require('path');

// Simple quantization for GBA (adapted from gsmplayer-gba)
const TILE_WIDTH = 8;
const TILE_SIZE = TILE_WIDTH * TILE_WIDTH;
const PALETTE_SIZE = 256;
const RESERVED_PALETTE_SIZE = 16;
const MAX_COLORS = PALETTE_SIZE - RESERVED_PALETTE_SIZE;

const R_SHIFT = 0;
const G_SHIFT = 5;
const B_SHIFT = 10;

function quantizeSimple(imageData) {
    const colorMap = new Map();
    const paletteMap = new Map([[0, 0]]);
    let paletteColorIndex = RESERVED_PALETTE_SIZE;
    
    const quantizedData = Array(imageData.length / 4);
    
    for (let i = 0; i < imageData.length / 4; i++) {
        const dataIndex = i * 4;
        const r = imageData[dataIndex + 0] >> 3; // 8-bit to 5-bit
        const g = imageData[dataIndex + 1] >> 3; // 8-bit to 5-bit  
        const b = imageData[dataIndex + 2] >> 3; // 8-bit to 5-bit
        
        let paletteColor = 0;
        paletteColor |= (r << R_SHIFT) & 0b11111;
        paletteColor |= (g << G_SHIFT) & 0b11111_00000;
        paletteColor |= (b << B_SHIFT) & 0b11111_00000_00000;
        
        let pIndex = paletteMap.get(paletteColor);
        if (typeof pIndex !== 'number' && paletteColorIndex < PALETTE_SIZE) {
            pIndex = paletteColorIndex++;
            paletteMap.set(paletteColor, pIndex);
        } else if (typeof pIndex !== 'number') {
            // Find closest existing color if we're out of palette space
            pIndex = RESERVED_PALETTE_SIZE;
        }
        
        quantizedData[i] = pIndex;
    }
    
    const palette = Array(PALETTE_SIZE).fill(0);
    for (const [color, index] of paletteMap) {
        palette[index] = color;
    }
    
    return { data: quantizedData, palette };
}

function bitmapToTiledFormat(data, width, height) {
    const tilesPerRow = width / TILE_WIDTH;
    const tilesPerCol = height / TILE_WIDTH;
    const tiledData = Array(data.length);
    
    let outputIndex = 0;
    for (let tileRow = 0; tileRow < tilesPerCol; tileRow++) {
        for (let tileCol = 0; tileCol < tilesPerRow; tileCol++) {
            for (let pixelRow = 0; pixelRow < TILE_WIDTH; pixelRow++) {
                for (let pixelCol = 0; pixelCol < TILE_WIDTH; pixelCol++) {
                    const srcY = tileRow * TILE_WIDTH + pixelRow;
                    const srcX = tileCol * TILE_WIDTH + pixelCol;
                    const srcIndex = srcY * width + srcX;
                    tiledData[outputIndex++] = data[srcIndex];
                }
            }
        }
    }
    
    return tiledData;
}

function generateGBAData(imageData, width, height) {
    if (width % TILE_WIDTH !== 0 || height % TILE_WIDTH !== 0) {
        throw new Error('Image dimensions must be multiples of 8');
    }
    
    const { palette, data: quantizedData } = quantizeSimple(imageData);
    const tiledData = bitmapToTiledFormat(quantizedData, width, height);
    
    return { palette, bitmap: tiledData };
}

function generateCArrays(palette, bitmap, name) {
    const paletteStr = palette.map(color => `0x${color.toString(16).padStart(4, '0')}`).join(', ');
    const bitmapStr = bitmap.map(pixel => `0x${pixel.toString(16).padStart(2, '0')}`).join(', ');
    
    return `// Generated album artwork data for ${name}
#ifndef ${name.toUpperCase()}_H
#define ${name.toUpperCase()}_H

extern const unsigned short ${name}_palette[${palette.length}];
extern const unsigned char ${name}_bitmap[${bitmap.length}];

// Palette data
const unsigned short ${name}_palette[${palette.length}] = {
    ${paletteStr}
};

// Bitmap data (8-bit indexed)
const unsigned char ${name}_bitmap[${bitmap.length}] = {
    ${bitmapStr}
};

#endif // ${name.toUpperCase()}_H
`;
}

module.exports = {
    generateGBAData,
    generateCArrays
};