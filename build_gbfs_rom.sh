#!/bin/bash

# PolygonGBA GBFS ROM Builder
# Creates final ROM with embedded GBFS filesystem

echo "Building PolygonGBA with GBFS..."

# Build the base ROM
echo "1. Building base ROM..."
make clean && make
if [ $? -ne 0 ]; then
    echo "❌ ROM build failed!"
    exit 1
fi

# Create GBFS archive if content exists
if [ -d "gbfs_content" ] && [ "$(ls -A gbfs_content/*.pgda 2>/dev/null)" ]; then
    echo "2. Creating GBFS archive..."
    cd gbfs_content
    rm -f polygondwanaland.gbfs  # Remove old archive
    ../tools/gbfs64/gbfs polygondwanaland.gbfs *.pgda
    cd ..
    
    echo "3. Combining ROM with GBFS..."
    cat polygon.gba gbfs_content/polygondwanaland.gbfs > polygon_gbfs.gba
    
    echo "✅ GBFS ROM created: polygon_gbfs.gba"
    echo "📂 GBFS contains: $(ls gbfs_content/*.pgda | wc -l) track(s)"
    echo "📊 Final ROM size: $(du -h polygon_gbfs.gba | cut -f1)"
else
    echo "⚠️  No GBFS content found, using embedded data only"
    echo "✅ Standard ROM created: polygon.gba"
fi