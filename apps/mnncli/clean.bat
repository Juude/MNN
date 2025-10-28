@echo off
REM MNNCLI Windows Clean Script
REM This script cleans all build artifacts

echo Cleaning MNNCLI build artifacts...

REM Clean mnncli build directory
if exist "build_mnncli" (
    echo Removing mnncli build directory...
    rmdir /s /q "build_mnncli"
)

REM Clean MNN build directory
if exist "..\..\build_mnn_static" (
    echo Removing MNN build directory...
    rmdir /s /q "..\..\build_mnn_static"
)

REM Clean CMake cache files
if exist "CMakeCache.txt" (
    echo Removing CMake cache...
    del /q "CMakeCache.txt"
)

if exist "CMakeFiles" (
    echo Removing CMake files...
    rmdir /s /q "CMakeFiles"
)

echo Clean completed!
pause