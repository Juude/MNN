#!/bin/bash

# MNNCLI Android Build Script
# This script builds mnncli as an Android command line executable

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${GREEN}Building MNNCLI for Android...${NC}"

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Check if ANDROID_NDK is set
if [ -z "$ANDROID_NDK" ]; then
    echo -e "${RED}Error: ANDROID_NDK environment variable is not set${NC}"
    echo -e "${YELLOW}Please set ANDROID_NDK to your Android NDK path${NC}"
    echo -e "${YELLOW}Example: export ANDROID_NDK=/path/to/android-ndk${NC}"
    exit 1
fi

# Check if ANDROID_NDK exists
if [ ! -d "$ANDROID_NDK" ]; then
    echo -e "${RED}Error: ANDROID_NDK directory does not exist: $ANDROID_NDK${NC}"
    exit 1
fi

# Create build directory
BUILD_DIR="$PROJECT_ROOT/build_mnncli_android"
echo -e "${YELLOW}Build directory: $BUILD_DIR${NC}"

# Clean build directory if it exists
if [ -d "$BUILD_DIR" ]; then
    echo -e "${YELLOW}Cleaning existing build directory...${NC}"
    rm -rf "$BUILD_DIR"
fi

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure CMake for Android
echo -e "${YELLOW}Configuring CMake for Android...${NC}"
cmake \
    -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK/build/cmake/android.toolchain.cmake" \
    -DCMAKE_BUILD_TYPE=Release \
    -DANDROID_ABI="arm64-v8a" \
    -DANDROID_STL=c++_static \
    -DANDROID_NATIVE_API_LEVEL=android-21 \
    -DMNN_BUILD_FOR_ANDROID_COMMAND=true \
    -DMNN_USE_LOGCAT=false \
    -DMNN_BUILD_LLM=OFF \
    -DMNN_BUILD_SHARED_LIBS=ON \
    -DMNN_LOW_MEMORY=ON \
    -DMNN_CPU_WEIGHT_DEQUANT_GEMM=ON \
    -DMNN_SUPPORT_TRANSFORMER_FUSE=ON \
    -DLLM_SUPPORT_VISION=OFF \
    -DMNN_BUILD_OPENCV=ON \
    -DMNN_IMGCODECS=ON \
    -DLLM_SUPPORT_AUDIO=ON \
    -DMNN_BUILD_AUDIO=ON \
    -DMNN_BUILD_DIFFUSION=ON \
    -DMNN_SEP_BUILD=OFF \
    -DBUILD_MNNCLI=ON \
    -DNATIVE_LIBRARY_OUTPUT=. \
    -DNATIVE_INCLUDE_OUTPUT=. \
    "$PROJECT_ROOT"

# Build
echo -e "${YELLOW}Building...${NC}"
if command -v nproc &> /dev/null; then
    make -j$(nproc)
else
    make -j$(sysctl -n hw.ncpu 2>/dev/null || echo 4)
fi

# Check if build was successful
if [ -f "apps/mnncli/mnncli" ]; then
    echo -e "${GREEN}Build successful!${NC}"
    echo -e "${GREEN}Executable location: $BUILD_DIR/apps/mnncli/mnncli${NC}"
    
    # Show file size
    ls -lh "apps/mnncli/mnncli"
    
    # Show file type
    file "apps/mnncli/mnncli"
    
    echo -e "${BLUE}Android mnncli executable built successfully!${NC}"
    echo -e "${YELLOW}You can now copy this executable to your Android device and run it.${NC}"
    echo -e "${YELLOW}Note: Make sure to set proper permissions: chmod +x mnncli${NC}"
else
    echo -e "${RED}Build failed! Executable not found.${NC}"
    exit 1
fi

echo -e "${GREEN}Android build completed successfully!${NC}"
