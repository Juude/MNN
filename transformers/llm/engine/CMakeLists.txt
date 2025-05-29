option(LLM_SUPPORT_VISION "Llm model support vision input." OFF)
option(LLM_SUPPORT_AUDIO "Llm model support audio input." OFF)
option(BUILD_MLS "Build PC Commandline." OFF)

set(LLM_DEPS ${MNN_DEPS})
if (LLM_SUPPORT_VISION AND MNN_BUILD_OPENCV)
    add_definitions(-DLLM_SUPPORT_VISION)
    list(APPEND LLM_DEPS MNNOpenCV)
    include_directories(${CMAKE_CURRENT_LIST_DIR}/../../../tools/cv/include/)
endif()

if (LLM_SUPPORT_AUDIO AND MNN_BUILD_AUDIO)
    list(APPEND LLM_DEPS MNNAudio)
    include_directories(${CMAKE_CURRENT_LIST_DIR}/../../../tools/audio/include/)
endif()

# include dir
include_directories(${CMAKE_CURRENT_LIST_DIR}/include/)

# source files
FILE(GLOB SRCS ${CMAKE_CURRENT_LIST_DIR}/src/*.cpp)

if (MNN_SEP_BUILD)
    if (MNN_BUILD_SHARED_LIBS)
        # compile dynamic so, support Linux/Mac
        add_library(llm SHARED ${SRCS})
        set_target_properties(llm PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
        target_link_libraries(llm ${LLM_DEPS})
    else()
        add_library(llm STATIC ${SRCS})
        # For static lib, OpenCV_LIBS should be PUBLIC so that executables linking llm also link to OpenCV
        target_link_libraries(llm PUBLIC ${OpenCV_LIBS})
    endif()
    list(APPEND LLM_DEPS llm)
else()
    add_library(llm OBJECT ${SRCS})
endif()

find_package(OpenCV REQUIRED)

if (LLM_SUPPORT_VISION AND MNN_BUILD_OPENCV)
    target_compile_definitions(llm PRIVATE LLM_SUPPORT_VISION)
endif()
if (LLM_SUPPORT_AUDIO AND MNN_BUILD_AUDIO)
    target_compile_definitions(llm PRIVATE LLM_SUPPORT_AUDIO)
endif()

add_executable(llm_demo ${CMAKE_CURRENT_LIST_DIR}/demo/llm_demo.cpp)
add_executable(embedding_demo ${CMAKE_CURRENT_LIST_DIR}/demo/embedding_demo.cpp)
add_executable(rollback_demo ${CMAKE_CURRENT_LIST_DIR}/demo/rollback_demo.cpp)
add_executable(llm_bench ${CMAKE_CURRENT_LIST_DIR}/demo/llm_bench.cpp)
target_link_libraries(llm_demo ${LLM_DEPS} ${OpenCV_LIBS})
target_link_libraries(embedding_demo ${LLM_DEPS} ${OpenCV_LIBS})
target_link_libraries(rollback_demo ${LLM_DEPS} ${OpenCV_LIBS})
target_link_libraries(llm_bench ${LLM_DEPS} ${OpenCV_LIBS})

if (BUILD_MLS)
set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum macOS version" FORCE)

add_executable(mls
        ${CMAKE_CURRENT_LIST_DIR}/app/mls.cpp
        ${CMAKE_CURRENT_LIST_DIR}/app/remote_model_downloader.cpp
        ${CMAKE_CURRENT_LIST_DIR}/app/file_utils.cpp
        ${CMAKE_CURRENT_LIST_DIR}/app/llm_benchmark.cpp
        ${CMAKE_CURRENT_LIST_DIR}/app/hf_api_client.cpp
        ${CMAKE_CURRENT_LIST_DIR}/app/mls_server.cpp
)
# set(OPENSSL_ROOT_DIR "C:/Program Files/OpenSSL-Win64")
# set(OPENSSL_INCLUDE_DIR "C:/Program Files/OpenSSL-Win64/include")
# set(OPENSSL_CRYPTO_LIBRARY "C:/Program Files/OpenSSL-Win64/lib/VC/x64/MD/libcrypto.lib")
# set(OPENSSL_LIBRARIES "C:/Program Files/OpenSSL-Win64/lib//VC/x64/MD/libssl.lib;C:/Program Files/OpenSSL-Win64/lib/VC/x64/MD/libcrypto.lib")
# Set OpenSSL_FOUND based on the presence of include directory and libraries
set(OPENSSL_USE_STATIC_LIBS TRUE)
message(STATUS "OPENSSL_INCLUDE_DIR: ${OPENSSL_INCLUDE_DIR} OPENSSL_LIBRARIES:${OPENSSL_LIBRARIES}")
if(OPENSSL_LIBRARIES AND OPENSSL_INCLUDE_DIR)
    set(OpenSSL_FOUND TRUE)
else()
    set(OpenSSL_FOUND FALSE)
endif()
# Check if OPENSSL_LIBRARIES is not set to find OpenSSL
if(NOT OpenSSL_FOUND)
    find_package(OpenSSL REQUIRED)
endif()

# Check if OpenSSL is found
if(OpenSSL_FOUND)
    message(STATUS "OpenSSL found: ${OPENSSL_INCLUDE_DIR} ${OPENSSL_LIBRARIES}")

    # Use the OpenSSL include directory
    target_include_directories(mls PRIVATE ${OPENSSL_INCLUDE_DIR})

    # Link to OpenSSL libraries
    target_link_libraries(mls PRIVATE ${OPENSSL_LIBRARIES})

    # If on Apple platform, link with Security framework
    if(APPLE)
        find_library(SECURITY_FRAMEWORK Security)
        if(SECURITY_FRAMEWORK)
            target_link_libraries(mls PRIVATE ${SECURITY_FRAMEWORK})
        else()
            message(FATAL_ERROR "Could not find Security framework")
        endif()
    endif()
else()
    message(FATAL_ERROR "OpenSSL not found. Please install OpenSSL and set paths.")
endif()

target_include_directories(mls PRIVATE ${MNN_INCLUDES} ${CMAKE_CURRENT_LIST_DIR}/src/)
set_property(TARGET mls PROPERTY CXX_STANDARD 17)
set_property(TARGET mls PROPERTY CXX_STANDARD_REQUIRED ON)
# target_compile_options(mls PRIVATE -std=c++17)
target_link_libraries(mls PRIVATE ${LLM_DEPS} ${OpenCV_LIBS})
target_compile_definitions(mls PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
endif()

# Unit Tests
# GTest should ideally be found by a higher-level CMakeLists.txt
# If not, uncomment and adjust the following:
# find_package(GTest REQUIRED)

if(TARGET GTest::gtest AND TARGET GTest::gtest_main)
    # Add definition for LLM_SUPPORT_VIDEO for the tests if it's globally ON
    # This ensures test code matching #ifdef LLM_SUPPORT_VIDEO compiles as expected.
    # Alternatively, could add this directly to video_test if it's the only one needing it.
    if(LLM_SUPPORT_VIDEO)
        add_definitions(-DLLM_SUPPORT_VIDEO)
    endif()

    # Video processing test
    add_executable(video_test ${CMAKE_CURRENT_LIST_DIR}/test/video_test.cpp)
    target_link_libraries(video_test PRIVATE
        llm # The library built in this project
        ${LLM_DEPS} # Dependencies of llm, including MNN
        ${OpenCV_LIBS} # OpenCV
        GTest::gtest
        GTest::gtest_main
    )
    # Ensure the test can find its data files by setting a working directory if run via CTest
    # For direct execution, users need to run from a path where test_data/sample_video.mp4 is accessible.
    # Example for CTest:
    # set_property(TEST video_test PROPERTY WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/test)
    # add_test(NAME VideoTest COMMAND video_test)
else()
    message(WARNING "GTest not found, video_test will not be built.")
endif()
