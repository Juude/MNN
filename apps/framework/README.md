# MyFramework C++ Multiplatform Project

This project is a template for creating a C++ library that can be used across multiple platforms: Android (JNI for AAR), iOS (Framework), and Desktop (static/shared library).

## Project Structure

- `src/common/`: C++ code shared across all platforms (e.g., `calculator.cpp`).
- `src/android/`: Android JNI wrapper code (e.g., `jni_bridge.cpp`).
- `src/ios/`: iOS Objective-C++ wrapper code (e.g., `ios_bridge.mm`).
- `src/desktop/`: Desktop-specific C++ code (if any).
- `include/`: Public header files for the common library (e.g., `calculator.h`).
- `tests/`: Test code for the common library (e.g., `test_calculator.cpp`).
- `build/`: Directory for build-related scripts or output (e.g., CMake toolchain files if needed).
- `CMakeLists.txt`: Main CMake build script for the project.

## Building the Project

This project uses CMake to manage the build process.

### Prerequisites

- CMake (version 3.10 or higher)
- A C++ compiler (e.g., GCC, Clang, MSVC)
- **For Android:**
    - Android NDK
    - Android SDK & Gradle (for AAR packaging)
- **For iOS:**
    - Xcode and command-line tools

### General Build Steps (Desktop Library & Tests)

1.  Create a build directory:
    ```bash
    mkdir build_desktop
    cd build_desktop
    ```
2.  Configure CMake:
    ```bash
    cmake ../
    ```
    (Adjust path to `CMakeLists.txt` if your build directory is elsewhere, e.g., `cmake ../apps/framework`)
3.  Build:
    ```bash
    cmake --build .
    ```
4.  Run tests (if configured):
    ```bash
    ctest
    ```
    Or run the test executable directly: `tests/calculator_tests`

### Building for Android (Conceptual)

The `CMakeLists.txt` defines a `android_jni_lib` target.
Typically, you would integrate this with a Gradle project:
1.  Configure your Android project's `build.gradle` to use CMake and point to this `CMakeLists.txt`.
2.  Specify the `android_jni_lib` as the target.
3.  Gradle will invoke CMake with the appropriate Android NDK toolchain to build the `.so` files, which are then packaged into an AAR.

Example `build.gradle` snippet:
```gradle
android {
    // ...
    defaultConfig {
        // ...
        externalNativeBuild {
            cmake {
                cppFlags ""
                // Arguments for CMake
                // arguments "-DANDROID_STL=c++_shared"
            }
        }
    }
    externalNativeBuild {
        cmake {
            path "src/main/cpp/apps/framework/CMakeLists.txt" // Adjust path
            version "3.18.1" // Specify your CMake version
        }
    }
}
```

### Building for iOS (Conceptual)

The `CMakeLists.txt` defines an `ios_bridge_lib` target configured as a Framework.
1.  Use CMake to generate an Xcode project:
    ```bash
    mkdir build_ios
    cd build_ios
    cmake -G Xcode ../ -DCMAKE_SYSTEM_NAME=iOS           -DCMAKE_OSX_SYSROOT=iphonesimulator           # Add other iOS specific CMake flags (e.g., architecture, deployment target)
    ```
    (Adjust path to `CMakeLists.txt` if needed)
2.  Open the generated `.xcodeproj` in Xcode.
3.  Build the `ios_bridge_lib` target (or your framework target) from Xcode. This will produce a `.framework` bundle.

### Using the Library

-   **Desktop:** Link against the compiled static (`.a` or `.lib`) or shared (`.so`, `.dylib`, or `.dll`) library.
-   **Android:** Include the generated AAR in your Android app project. Call the JNI functions from your Java/Kotlin code.
-   **iOS:** Embed the generated `.framework` in your iOS app project. Call the bridged functions from Swift/Objective-C.

## Running Tests

After building (e.g., for desktop), you can run tests using CTest:
```bash
cd build_desktop # Or your build directory
ctest
```
Or run the executable directly:
```bash
./tests/calculator_tests # Adjust path if necessary
```

This README provides a starting point. Detailed build instructions for each platform can be quite extensive and depend on the specific versions of tools and NDK/SDKs used.
