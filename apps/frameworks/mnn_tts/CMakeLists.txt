cmake_minimum_required(VERSION 3.10)
project(mnn_tts LANGUAGES C CXX)

add_library(mnn_tts SHARED
    src/common.cpp
)

target_include_directories(mnn_tts PUBLIC include)
set_target_properties(mnn_tts PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(ANDROID)
    target_sources(mnn_tts PRIVATE src/android/android_jni.cpp)
elseif(APPLE)
    target_sources(mnn_tts PRIVATE src/ios/ios.mm)
else()
    target_sources(mnn_tts PRIVATE src/desktop/desktop.cpp)
endif()

# Tests
enable_testing()
add_executable(mnn_tts_tests tests/test_main.cpp)
target_link_libraries(mnn_tts_tests mnn_tts)
add_test(NAME mnn_tts_run COMMAND mnn_tts_tests)

if(ANDROID)
    add_custom_target(run_android_tests
        COMMAND adb push $<TARGET_FILE:mnn_tts_tests> /data/local/tmp
        COMMAND adb shell /data/local/tmp/mnn_tts_tests
        DEPENDS mnn_tts_tests
        )
endif()
