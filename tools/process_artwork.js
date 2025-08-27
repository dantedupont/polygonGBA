const fs = require('fs');
const path = require('path');
const { execSync } = require('child_process');
const { generateGBAData, generateCArrays } = require('./img2gba.js');

const ARTWORK_DIR = path.join(__dirname, '..', 'artwork');
const SOURCE_DIR = path.join(__dirname, '..', 'source');
const TARGET_SIZE = 128; // 128x128 for GBA album art

async function processArtwork(inputFile, outputName) {
    console.log(`Processing ${inputFile} -> ${outputName}`);
    
    const inputPath = path.join(ARTWORK_DIR, inputFile);
    const tempPath = path.join(ARTWORK_DIR, 'temp_resized.bmp');
    
    // Resize to 128x128 and convert to BMP using ImageMagick (if available)
    try {
        execSync(`magick "${inputPath}" -resize ${TARGET_SIZE}x${TARGET_SIZE}! "${tempPath}"`, { stdio: 'pipe' });
    } catch (error) {
        console.log('ImageMagick not available, trying sips (macOS)...');
        try {
            execSync(`sips -z ${TARGET_SIZE} ${TARGET_SIZE} "${inputPath}" --out "${tempPath}"`, { stdio: 'pipe' });
        } catch (sipsError) {
            throw new Error('No image processing tool available (tried ImageMagick and sips)');
        }
    }
    
    // Read BMP data (simplified - assumes 24-bit BMP)
    const bmpData = fs.readFileSync(tempPath);
    const dataOffset = bmpData.readUInt32LE(10);
    const width = bmpData.readUInt32LE(18);
    const height = bmpData.readUInt32LE(22);
    
    if (width !== TARGET_SIZE || height !== TARGET_SIZE) {
        throw new Error(`Unexpected image dimensions: ${width}x${height}`);
    }
    
    // Extract pixel data (BMP is stored bottom-to-top, so we need to flip it)
    const pixelData = new Uint8Array(width * height * 4);
    let pixelIndex = 0;
    
    for (let y = height - 1; y >= 0; y--) {
        for (let x = 0; x < width; x++) {
            const bmpIndex = dataOffset + (y * width + x) * 3;
            pixelData[pixelIndex++] = bmpData[bmpIndex + 2]; // R
            pixelData[pixelIndex++] = bmpData[bmpIndex + 1]; // G
            pixelData[pixelIndex++] = bmpData[bmpIndex + 0]; // B
            pixelData[pixelIndex++] = 255; // A
        }
    }
    
    // Generate GBA data
    const { palette, bitmap } = generateGBAData(pixelData, width, height);
    const cCode = generateCArrays(palette, bitmap, outputName);
    
    // Write output file
    const outputPath = path.join(SOURCE_DIR, `${outputName}.h`);
    fs.writeFileSync(outputPath, cCode);
    
    // Clean up temp file
    fs.unlinkSync(tempPath);
    
    console.log(`Generated ${outputPath}`);
    return { palette: palette.length, bitmap: bitmap.length };
}

// Process the Polygondwanaland artwork
if (require.main === module) {
    try {
        const stats = processArtwork('polygondwanaland.jpg', 'polygondwanaland_art');
        console.log(`Success! Generated palette with ${stats.palette} colors and bitmap with ${stats.bitmap} bytes`);
    } catch (error) {
        console.error('Error processing artwork:', error.message);
        process.exit(1);
    }
}

module.exports = { processArtwork };