@echo off
REM MNNCLI Windows Build Script (Batch Wrapper)
REM This batch file provides an easy way to run the PowerShell build script

setlocal enabledelayedexpansion

REM Check if PowerShell is available
powershell -Command "Get-Host" >nul 2>&1
if %errorlevel% neq 0 (
    echo Error: PowerShell is not available or not in PATH
    echo Please install PowerShell or add it to your PATH
    pause
    exit /b 1
)

REM Check if we're running from the correct directory
if not exist "build.ps1" (
    echo Error: build.ps1 not found in current directory
    echo Please run this script from the mnncli directory
    pause
    exit /b 1
)

REM Check if CMake is available
cmake --version >nul 2>&1
if %errorlevel% neq 0 (
    echo Error: CMake is not available or not in PATH
    echo Please install CMake and add it to your PATH
    echo Download from: https://cmake.org/download/
    pause
    exit /b 1
)

REM Check if Ninja is available
ninja --version >nul 2>&1
if %errorlevel% neq 0 (
    echo Error: Ninja build system is not available or not in PATH
    echo Please install Ninja and add it to your PATH
    echo Download from: https://ninja-build.org/
    pause
    exit /b 1
)

REM Check if Visual Studio Build Tools are available
where cl >nul 2>&1
if %errorlevel% neq 0 (
    echo Warning: Visual Studio Build Tools not found in PATH
    echo You may need to run this from a Visual Studio Developer Command Prompt
    echo or install Visual Studio Build Tools
    echo.
    echo Continuing anyway...
    echo.
)

REM Parse command line arguments and pass them to PowerShell
set "args="
:parse_args
if "%~1"=="" goto :run_powershell
set "args=%args% %~1"
shift
goto :parse_args

:run_powershell
REM Run the PowerShell script with the parsed arguments
echo Running MNNCLI Windows build script...
echo.
powershell -ExecutionPolicy Bypass -File "build.ps1" %args%

REM Check if the build was successful
if %errorlevel% neq 0 (
    echo.
    echo Build failed with error code %errorlevel%
    pause
    exit /b %errorlevel%
)

echo.
echo Build completed successfully!
echo.
echo The mnncli executable is located in: build_mnncli\mnncli.exe
echo.
echo You can now run mnncli with commands like:
echo   build_mnncli\mnncli.exe --help
echo   build_mnncli\mnncli.exe list
echo   build_mnncli\mnncli.exe search qwen
echo.
pause