# MNNCLI Windows Build Script
# This PowerShell script builds the mnncli executable on Windows

param(
    [Switch]$Clean,
    [Switch]$Help,
    [String]$Configuration = "Debug",
    [String]$Architecture = "x64",
    [String]$Backends = "",
    [Switch]$Verbose
)

if ($Help) {
    Write-Host "MNNCLI Windows Build Script" -ForegroundColor Green
    Write-Host ""
    Write-Host "Usage: .\build.ps1 [options]"
    Write-Host ""
    Write-Host "Options:"
    Write-Host "  -Clean              Clean build directories before building"
    Write-Host "  -Configuration      Build configuration (Debug|Release) [Default: Debug]"
    Write-Host "  -Architecture       Target architecture (x64|x86) [Default: x64]"
    Write-Host "  -Backends           Comma-separated list of backends (opencl,vulkan,cuda)"
    Write-Host "  -Verbose            Enable verbose output"
    Write-Host "  -Help               Show this help message"
    Write-Host ""
    Write-Host "Examples:"
    Write-Host "  .\build.ps1                                    # Default build"
    Write-Host "  .\build.ps1 -Clean -Configuration Release      # Clean Release build"
    Write-Host "  .\build.ps1 -Backends opencl,vulkan            # Build with GPU backends"
    Write-Host "  .\build.ps1 -Architecture x86                  # Build for x86"
    exit 0
}

# Set error action preference
$ErrorActionPreference = "Stop"

# Colors for output
$Red = "Red"
$Green = "Green"
$Yellow = "Yellow"
$Cyan = "Cyan"

function Write-ColorOutput {
    param([String]$Message, [String]$Color = "White")
    Write-Host $Message -ForegroundColor $Color
}

Write-ColorOutput "Building MNNCLI on Windows..." $Green

# Get the directory where this script is located
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent (Split-Path -Parent $ScriptDir)

Write-ColorOutput "Script directory: $ScriptDir" $Cyan
Write-ColorOutput "Project root: $ProjectRoot" $Cyan

# Stage 1: Build MNN static library
$MnnBuildDir = Join-Path $ProjectRoot "build_mnn_static"
Write-ColorOutput "Stage 1: Building MNN static library..." $Yellow
Write-ColorOutput "MNN build directory: $MnnBuildDir" $Cyan

# Clean MNN build directory if requested
if ($Clean) {
    if (Test-Path $MnnBuildDir) {
        Write-ColorOutput "Cleaning MNN build directory..." $Yellow
        Remove-Item -Path $MnnBuildDir -Recurse -Force
    }
}

# Create MNN build directory
New-Item -Path $MnnBuildDir -ItemType Directory -Force | Out-Null

# Configure MNN build
Write-ColorOutput "Configuring MNN..." $Yellow

$MnnCmakeArgs = @(
    "-B", $MnnBuildDir,
    "-S", $ProjectRoot,
    "-G", "Ninja",
    "-DMNN_BUILD_LLM=ON",
    "-DMNN_BUILD_SHARED_LIBS=OFF",
    "-DCMAKE_BUILD_TYPE=$Configuration",
    "-DMNN_LOW_MEMORY=ON",
    "-DMNN_CPU_WEIGHT_DEQUANT_GEMM=ON",
    "-DMNN_SUPPORT_TRANSFORMER_FUSE=ON",
    "-DLLM_SUPPORT_VISION=ON",
    "-DMNN_BUILD_OPENCV=ON",
    "-DMNN_IMGCODECS=ON",
    "-DLLM_SUPPORT_AUDIO=ON",
    "-DMNN_BUILD_AUDIO=ON",
    "-DMNN_BUILD_DIFFUSION=ON",
    "-DMNN_SEP_BUILD=OFF",
    "-DMNN_USE_OPENCV=ON",
    "-DMNN_WIN_RUNTIME_MT=ON"
)

# Add backend-specific options
if ($Backends) {
    $BackendList = $Backends.Split(",")
    foreach ($Backend in $BackendList) {
        $Backend = $Backend.Trim()
        switch ($Backend.ToLower()) {
            "opencl" { $MnnCmakeArgs += "-DMNN_OPENCL=ON" }
            "vulkan" { $MnnCmakeArgs += "-DMNN_VULKAN=ON" }
            "cuda" { $MnnCmakeArgs += "-DMNN_CUDA=ON" }
        }
    }
}

# Add architecture-specific options
if ($Architecture -eq "x86") {
    $MnnCmakeArgs += "-A", "Win32"
} else {
    $MnnCmakeArgs += "-A", "x64"
}

if ($Verbose) {
    Write-ColorOutput "CMake command: cmake $($MnnCmakeArgs -join ' ')" $Cyan
}

& cmake @MnnCmakeArgs
if ($LASTEXITCODE -ne 0) {
    Write-ColorOutput "Failed to configure MNN!" $Red
    exit 1
}

# Build MNN
Write-ColorOutput "Building MNN..." $Yellow
$CpuCount = (Get-WmiObject -Class Win32_Processor).NumberOfCores
$BuildArgs = @("--build", $MnnBuildDir, "--", "-j$CpuCount")

if ($Verbose) {
    Write-ColorOutput "Build command: cmake $($BuildArgs -join ' ')" $Cyan
}

& cmake @BuildArgs
if ($LASTEXITCODE -ne 0) {
    Write-ColorOutput "Failed to build MNN!" $Red
    exit 1
}

# Verify MNN library was built
$MnnLibPath = Join-Path $MnnBuildDir "MNN.lib"
if (-not (Test-Path $MnnLibPath)) {
    Write-ColorOutput "Failed to build MNN static library!" $Red
    Write-ColorOutput "Expected library at: $MnnLibPath" $Red
    exit 1
}

Write-ColorOutput "MNN static library built successfully!" $Green
$LibInfo = Get-Item $MnnLibPath
Write-ColorOutput "Library size: $([math]::Round($LibInfo.Length / 1MB, 2)) MB" $Cyan

# Stage 2: Build mnncli executable
$MnncliBuildDir = Join-Path $ScriptDir "build_mnncli"
Write-ColorOutput "Stage 2: Building mnncli executable..." $Yellow
Write-ColorOutput "mnncli build directory: $MnncliBuildDir" $Cyan

# Clean mnncli build directory
if (Test-Path $MnncliBuildDir) {
    Write-ColorOutput "Cleaning mnncli build directory..." $Yellow
    Remove-Item -Path $MnncliBuildDir -Recurse -Force
}

New-Item -Path $MnncliBuildDir -ItemType Directory -Force | Out-Null

# Configure mnncli
Write-ColorOutput "Configuring mnncli..." $Yellow

$MnncliCmakeArgs = @(
    "-B", $MnncliBuildDir,
    "-S", $ScriptDir,
    "-G", "Ninja",
    "-DMNN_BUILD_DIR=$MnnBuildDir",
    "-DMNN_SOURCE_DIR=$ProjectRoot",
    "-DCMAKE_BUILD_TYPE=$Configuration"
)

# Add architecture-specific options
if ($Architecture -eq "x86") {
    $MnncliCmakeArgs += "-A", "Win32"
} else {
    $MnncliCmakeArgs += "-A", "x64"
}

if ($Verbose) {
    Write-ColorOutput "CMake command: cmake $($MnncliCmakeArgs -join ' ')" $Cyan
}

& cmake @MnncliCmakeArgs
if ($LASTEXITCODE -ne 0) {
    Write-ColorOutput "Failed to configure mnncli!" $Red
    exit 1
}

# Build mnncli
Write-ColorOutput "Building mnncli..." $Yellow
$MnncliBuildArgs = @("--build", $MnncliBuildDir, "--", "-j$CpuCount")

if ($Verbose) {
    Write-ColorOutput "Build command: cmake $($MnncliBuildArgs -join ' ')" $Cyan
}

& cmake @MnncliBuildArgs
if ($LASTEXITCODE -ne 0) {
    Write-ColorOutput "Failed to build mnncli!" $Red
    exit 1
}

# Check if build was successful
$MnncliExe = Join-Path $MnncliBuildDir "mnncli.exe"
if (Test-Path $MnncliExe) {
    Write-ColorOutput "Build successful!" $Green
    Write-ColorOutput "Executable location: $MnncliExe" $Green
    
    # Show file size
    $ExeInfo = Get-Item $MnncliExe
    Write-ColorOutput "Executable size: $([math]::Round($ExeInfo.Length / 1MB, 2)) MB" $Cyan
    
    # Test basic functionality
    Write-ColorOutput "Testing basic functionality..." $Yellow
    try {
        & $MnncliExe --help | Out-Null
        if ($LASTEXITCODE -eq 0) {
            Write-ColorOutput "Basic functionality test passed!" $Green
        } else {
            Write-ColorOutput "Basic functionality test failed!" $Red
            exit 1
        }
    } catch {
        Write-ColorOutput "Basic functionality test failed with error: $($_.Exception.Message)" $Red
        exit 1
    }
} else {
    Write-ColorOutput "Build failed! Executable not found." $Red
    Write-ColorOutput "Expected executable at: $MnncliExe" $Red
    exit 1
}

Write-ColorOutput "Build completed successfully!" $Green
Write-ColorOutput ""
Write-ColorOutput "Usage examples:" $Cyan
Write-ColorOutput "  $MnncliExe --help" $Cyan
Write-ColorOutput "  $MnncliExe list" $Cyan
Write-ColorOutput "  $MnncliExe search qwen" $Cyan
Write-ColorOutput "  $MnncliExe download Qwen/Qwen-7B-Chat" $Cyan
Write-ColorOutput "  $MnncliExe run Qwen-7B-Chat" $Cyan