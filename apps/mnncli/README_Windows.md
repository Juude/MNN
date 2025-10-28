# MNNCLI Windows Build Guide

This guide explains how to build and use MNNCLI on Windows systems.

## Prerequisites

### Required Software

1. **Visual Studio 2019 or later** (with C++ build tools)
   - Download from: https://visualstudio.microsoft.com/downloads/
   - Make sure to install "Desktop development with C++" workload
   - Or install "Build Tools for Visual Studio" for a minimal installation

2. **CMake 3.16 or later**
   - Download from: https://cmake.org/download/
   - Make sure to add CMake to your PATH during installation

3. **Ninja Build System**
   - Download from: https://ninja-build.org/
   - Extract ninja.exe to a directory in your PATH (e.g., C:\Windows\System32)

4. **OpenSSL (Static Libraries)**
   - Download from: https://slproweb.com/products/Win32OpenSSL.html
   - Choose "Win64 OpenSSL v3.x.x" (or Win32 for 32-bit builds)
   - Install to default location (usually C:\Program Files\OpenSSL-Win64)
   - Make sure to install the "Visual C++ 2019 Redistributables" if prompted

5. **Git** (for cloning the repository)
   - Download from: https://git-scm.com/download/win

### Optional Software

- **OpenCV** (if you want vision processing support)
  - Download from: https://opencv.org/releases/
  - Extract to a directory like C:\opencv
  - Set environment variable `OpenCV_DIR` to point to the build directory

## Building MNNCLI

### Quick Start

1. **Clone the repository:**
   ```cmd
   git clone https://github.com/alibaba/MNN.git
   cd MNN\apps\mnncli
   ```

2. **Run the build script:**
   ```cmd
   build.bat
   ```

   Or for more control:
   ```cmd
   build.bat -Clean -Configuration Release
   ```

### Advanced Build Options

#### Using PowerShell Script Directly

The `build.ps1` script provides more options:

```powershell
# Basic build
.\build.ps1

# Clean build with Release configuration
.\build.ps1 -Clean -Configuration Release

# Build with GPU backends (OpenCL, Vulkan, CUDA)
.\build.ps1 -Backends "opencl,vulkan"

# Build for x86 architecture
.\build.ps1 -Architecture x86

# Verbose output
.\build.ps1 -Verbose

# Show help
.\build.ps1 -Help
```

#### Manual Build Process

If you prefer to build manually:

1. **Build MNN static library:**
   ```cmd
   cd ..\..
   mkdir build_mnn_static
   cd build_mnn_static
   cmake -G Ninja -DMNN_BUILD_LLM=ON -DMNN_BUILD_SHARED_LIBS=OFF -DCMAKE_BUILD_TYPE=Debug -DMNN_WIN_RUNTIME_MT=ON ..
   ninja
   ```

2. **Build mnncli:**
   ```cmd
   cd ..\apps\mnncli
   mkdir build_mnncli
   cd build_mnncli
   cmake -G Ninja -DMNN_BUILD_DIR=..\..\build_mnn_static -DMNN_SOURCE_DIR=..\.. -DCMAKE_BUILD_TYPE=Debug ..
   ninja
   ```

## Build Output

After a successful build, you'll find:
- **Executable:** `build_mnncli\mnncli.exe`
- **MNN Library:** `..\..\build_mnn_static\MNN.lib`

## Usage

### Basic Commands

```cmd
# Show help
build_mnncli\mnncli.exe --help

# List local models
build_mnncli\mnncli.exe list

# Search for models
build_mnncli\mnncli.exe search qwen

# Download a model
build_mnncli\mnncli.exe download Qwen/Qwen-7B-Chat

# Run model interactively
build_mnncli\mnncli.exe run Qwen-7B-Chat

# Run with a prompt
build_mnncli\mnncli.exe run -p "Hello, how are you?"

# Start API server
build_mnncli\mnncli.exe serve Qwen-7B-Chat --port 8000

# Run benchmarks
build_mnncli\mnncli.exe benchmark Qwen-7B-Chat
```

### Configuration

```cmd
# Show current configuration
build_mnncli\mnncli.exe config show

# Set default model
build_mnncli\mnncli.exe config set default_model Qwen-7B-Chat

# Set download provider
build_mnncli\mnncli.exe config set download_provider modelscope
```

## Troubleshooting

### Common Issues

1. **"CMake not found" error:**
   - Make sure CMake is installed and added to your PATH
   - Restart your command prompt after installing CMake

2. **"Ninja not found" error:**
   - Download ninja.exe and add it to your PATH
   - Or install via package manager: `choco install ninja`

3. **"OpenSSL not found" error:**
   - Make sure OpenSSL is installed in the default location
   - Check that the OpenSSL installation includes the development files
   - Try setting `OPENSSL_ROOT_DIR` environment variable

4. **"Visual Studio not found" error:**
   - Make sure Visual Studio Build Tools are installed
   - Run the build from a "Developer Command Prompt for VS"
   - Or set up the environment: `vcvarsall.bat x64`

5. **Build fails with linking errors:**
   - Make sure you're using the same architecture (x64/x86) for both MNN and mnncli
   - Try a clean build: `build.bat -Clean`

6. **PowerShell execution policy error:**
   ```powershell
   Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
   ```

### Environment Variables

You can set these environment variables to customize the build:

- `OPENSSL_ROOT_DIR`: Path to OpenSSL installation
- `OpenCV_DIR`: Path to OpenCV installation
- `CMAKE_PREFIX_PATH`: Additional search paths for CMake

### Build Logs

For detailed build information, use the `-Verbose` flag:
```cmd
build.bat -Verbose
```

## Performance Notes

- **Release builds** are significantly faster than Debug builds
- **Static linking** (default) creates larger executables but better portability
- **GPU backends** (OpenCL/Vulkan) can significantly speed up inference
- **Multi-threading** is enabled by default for better performance

## Platform Support

- **Windows 10/11** (x64 and x86)
- **Visual Studio 2019/2022** (MSVC compiler)
- **MinGW-w64** (GCC compiler) - experimental support

## API Server

When running the server with `mnncli serve`, the following endpoints are available:

- `GET /` - Web interface
- `POST /chat/completions` - OpenAI-compatible chat API
- `POST /v1/chat/completions` - OpenAI-compatible chat API (v1)
- `POST /reset` - Reset conversation
- `GET /v1/models` - List available models

Access the web interface at: http://localhost:8000

## Contributing

If you encounter issues or want to contribute:

1. Check the [main README](../README.md) for general information
2. Report Windows-specific issues in the GitHub repository
3. Include build logs and system information when reporting issues

## License

This project is licensed under the Apache License 2.0. See the [LICENSE](../../LICENSE.txt) file for details.