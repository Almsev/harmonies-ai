#!/bin/bash

# Build script for Harminies WASM

set -e

echo "Building Harminies WASM..."

# Check if emscripten is available
if ! command -v emcc &> /dev/null; then
    echo "Error: emcc (Emscripten) not found!"
    echo "Please install Emscripten: https://emscripten.org/docs/getting_started/downloads.html"
    echo "Or activate emsdk: source /path/to/emsdk/emsdk_env.sh"
    exit 1
fi

# Create build directory
mkdir -p build
cd build

# Configure with emcmake
echo "Configuring with CMake..."
emcmake cmake ..

# Build
echo "Building..."
emmake make

# Copy output files to parent directory for web serving
echo "Copying output files..."
cp harminies.js ../
cp harminies.wasm ../

echo ""
echo "✅ Build complete!"
echo ""
echo "To test, run a local web server:"
echo "  python3 -m http.server 8000"
echo ""
echo "Then open: http://localhost:8000"
