#!/bin/bash

set -e

echo "Installing dependencies for Vulkan Tutorial..."

# --------------------------------------------------
# Check for vcpkg
# --------------------------------------------------

if ! command -v vcpkg >/dev/null 2>&1; then
    echo "vcpkg not found."
    echo
    echo "Install it using:"
    echo "  cd ~"
    echo "  git clone https://github.com/microsoft/vcpkg.git"
    echo "  cd vcpkg"
    echo "  ./bootstrap-vcpkg.sh"
    echo
    echo 'Then add it to PATH:'
    echo '  echo '\''export PATH="$HOME/vcpkg:$PATH"'\'' >> ~/.zshrc'
    echo '  source ~/.zshrc'
    exit 1
fi

# --------------------------------------------------
# Determine architecture
# --------------------------------------------------

ARCH=$(uname -m)

if [ "$ARCH" = "arm64" ]; then
    TRIPLET="arm64-osx"
elif [ "$ARCH" = "x86_64" ]; then
    TRIPLET="x64-osx"
else
    echo "Unsupported macOS architecture: $ARCH"
    exit 1
fi

echo "Detected architecture: $ARCH"
echo "Using vcpkg triplet: $TRIPLET"

# --------------------------------------------------
# Enable binary caching
# --------------------------------------------------

echo "Enabling vcpkg binary caching..."

CACHE_DIR="${TMPDIR:-/tmp}/vcpkg-cache"

mkdir -p "$CACHE_DIR"

export VCPKG_BINARY_SOURCES="clear;files,$CACHE_DIR,readwrite"

# --------------------------------------------------
# Install dependencies
# --------------------------------------------------

echo "Installing dependencies..."

vcpkg install \
    glfw3 \
    glm \
    tinyobjloader \
    stb \
    tinygltf \
    nlohmann-json \
    "ktx[vulkan]" \
    openal-soft \
    --triplet="$TRIPLET"

echo
echo "Dependencies installed successfully."
echo
echo "IMPORTANT:"
echo "Install the Vulkan SDK for macOS from:"
echo "https://vulkan.lunarg.com/"
echo
echo "The macOS SDK includes MoltenVK for running Vulkan on Metal."
echo
echo "Example CMake configuration:"
echo
echo 'cmake -B build -S . \'
echo '  -DCMAKE_TOOLCHAIN_FILE="$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake" \'
echo "  -DVCPKG_TARGET_TRIPLET=$TRIPLET"
echo
echo "Then build using:"
echo
echo "cmake --build build"