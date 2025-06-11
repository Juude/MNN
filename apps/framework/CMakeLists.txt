cmake_minimum_required(VERSION 3.10)
project(MyFramework CXX)

# --- Common Library (core logic) ---
# Add include directory for our library
include_directories(include)

# Add the common library source file
add_library(common_lib src/common/calculator.cpp)
target_include_directories(common_lib PUBLIC include)

# --- Desktop Support ---
# For desktop, common_lib can be built as a static or shared library.
# Example: add_library(MyFramework_desktop STATIC src/common/calculator.cpp)
# Or it can be linked directly by a desktop application's CMakeLists.txt.

# --- Android JNI Library ---
# This will be a shared library that includes the JNI bridge and links common_lib.
# The actual AAR packaging is usually handled by Gradle, which calls CMake.
add_library(android_jni_lib SHARED src/android/jni_bridge.cpp)
target_link_libraries(android_jni_lib common_lib)
# Specific Android properties like ABI, STL type, etc.,
# are often set via CMAKE_TOOLCHAIN_FILE or Gradle CMake arguments.

# --- iOS Framework ---
# CMake can generate an Xcode project which then builds the Framework.
# This will be a shared library that includes the iOS bridge and links common_lib.
add_library(ios_bridge_lib SHARED src/ios/ios_bridge.mm)
target_link_libraries(ios_bridge_lib common_lib)
# Setting framework properties (bundle identifier, version, etc.)
# is done using CMake properties like MACOSX_BUNDLE, FRAMEWORK, etc.
set_target_properties(ios_bridge_lib PROPERTIES
    FRAMEWORK TRUE
    MACOSX_FRAMEWORK_IDENTIFIER com.example.myframework
    # Add other necessary properties for iOS framework
)


# --- Tests ---
# Enable testing
enable_testing()

# Add test executable
add_executable(calculator_tests tests/test_calculator.cpp)
target_link_libraries(calculator_tests common_lib)

# Add a simple test command
add_test(NAME BasicTests COMMAND calculator_tests)

# --- Installation (Optional, for desktop libraries) ---
# install(TARGETS common_lib
#         LIBRARY DESTINATION lib
#         ARCHIVE DESTINATION lib
#         PUBLIC_HEADER DESTINATION include)
# install(FILES include/calculator.h DESTINATION include)

# --- Output ---
# Message to confirm configuration
message(STATUS "MyFramework Project Configured")
message(STATUS "  Common library: common_lib")
message(STATUS "  Android JNI: android_jni_lib (integrate with Gradle)")
message(STATUS "  iOS Bridge: ios_bridge_lib (generate Xcode project, then build Framework)")
message(STATUS "  Tests: calculator_tests")
