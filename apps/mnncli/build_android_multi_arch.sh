#!/bin/bash

# MNNCLI Multi-Architecture Android Build Script
# Supports arm64-v8a, armeabi-v7a, x86, x86_64

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Default values
ANDROID_ABI="arm64-v8a"
BUILD_TYPE="Release"
CLEAN_BUILD=false

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --abi)
            ANDROID_ABI="$2"
            shift 2
            ;;
        --type)
            BUILD_TYPE="$2"
            shift 2
            ;;
        --clean)
            CLEAN_BUILD=true
            shift
            ;;
        --help)
            echo "Usage: $0 [OPTIONS]"
            echo "Options:"
            echo "  --abi <ABI>        Target ABI (arm64-v8a, armeabi-v7a, x86, x86_64)"
            echo "  --type <TYPE>      Build type (Debug, Release, RelWithDebInfo)"
            echo "  --clean            Clean build directory before building"
            echo "  --help             Show this help message"
            echo ""
            echo "Examples:"
            echo "  $0 --abi arm64-v8a --type Release"
            echo "  $0 --abi armeabi-v7a --clean"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

echo -e "${GREEN}Building MNNCLI for Android (${ANDROID_ABI})...${NC}"

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

# Validate ABI
VALID_ABIS=("arm64-v8a" "armeabi-v7a" "x86" "x86_64")
if [[ ! " ${VALID_ABIS[@]} " =~ " ${ANDROID_ABI} " ]]; then
    echo -e "${RED}Error: Invalid ABI '${ANDROID_ABI}'${NC}"
    echo -e "${YELLOW}Valid ABIs: ${VALID_ABIS[*]}${NC}"
    exit 1
fi

# Create build directory
BUILD_DIR="$PROJECT_ROOT/build_mnncli_android_${ANDROID_ABI}"
echo -e "${YELLOW}Build directory: $BUILD_DIR${NC}"
echo -e "${YELLOW}Target ABI: $ANDROID_ABI${NC}"
echo -e "${YELLOW}Build type: $BUILD_TYPE${NC}"

# Clean build directory if requested or if it exists
if [ "$CLEAN_BUILD" = true ] || [ -d "$BUILD_DIR" ]; then
    echo -e "${YELLOW}Cleaning build directory...${NC}"
    rm -rf "$BUILD_DIR"
fi

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure CMake for Android
echo -e "${YELLOW}Configuring CMake for Android (${ANDROID_ABI})...${NC}"
cmake \
    -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK/build/cmake/android.toolchain.cmake" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DANDROID_ABI="$ANDROID_ABI" \
    -DANDROID_STL=c++_static \
    -DANDROID_NATIVE_API_LEVEL=android-21 \
    -DMNN_BUILD_FOR_ANDROID_COMMAND=true \
    -DMNN_USE_LOGCAT=false \
    -DMNN_BUILD_LLM=ON \
    -DMNN_BUILD_SHARED_LIBS=ON \
    -DMNN_LOW_MEMORY=ON \
    -DMNN_CPU_WEIGHT_DEQUANT_GEMM=ON \
    -DMNN_SUPPORT_TRANSFORMER_FUSE=ON \
    -DLLM_SUPPORT_VISION=ON \
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
    
    echo -e "${BLUE}Android mnncli executable built successfully for ${ANDROID_ABI}!${NC}"
    echo -e "${YELLOW}You can now copy this executable to your Android device and run it.${NC}"
    echo -e "${YELLOW}Note: Make sure to set proper permissions: chmod +x mnncli${NC}"
    
    # Show deployment commands
    echo -e "${BLUE}Deployment commands:${NC}"
    echo -e "${YELLOW}adb push $BUILD_DIR/apps/mnncli/mnncli /data/local/tmp/${NC}"
    echo -e "${YELLOW}adb shell chmod +x /data/local/tmp/mnncli${NC}"
    echo -e "${YELLOW}adb shell /data/local/tmp/mnncli --help${NC}"
else
    echo -e "${RED}Build failed! Executable not found.${NC}"
    exit 1
fi

echo -e "${GREEN}Android build for ${ANDROID_ABI} completed successfully!${NC}"
