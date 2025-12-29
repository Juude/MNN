cmake_minimum_required(VERSION 3.10)
project(mnn-node)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include node-addon-api
execute_process(COMMAND node -p "require('node-addon-api').include"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE NODE_ADDON_API_DIR_RAW
        OUTPUT_STRIP_TRAILING_WHITESPACE)

string(REPLACE "\"" "" NODE_ADDON_API_DIR ${NODE_ADDON_API_DIR_RAW})
include_directories(${NODE_ADDON_API_DIR})
include_directories(${CMAKE_JS_INC})

# MNN setup
set(MNN_ROOT ${CMAKE_SOURCE_DIR}/..)

# Include directories
include_directories(${MNN_ROOT}/include)

# Find MNN library
# We look for MNN library in common build directories or use find_library
# For now, we search in ../build
set(MNN_SEARCH_PATH ${MNN_ROOT}/build)
if(NOT EXISTS ${MNN_SEARCH_PATH})
    set(MNN_SEARCH_PATH ${MNN_ROOT})
endif()

find_library(MNN_LIB NAMES MNN PATHS ${MNN_SEARCH_PATH} PATH_SUFFIXES lib NO_DEFAULT_PATH)

if (NOT MNN_LIB)
    message(WARNING "MNN library not found in ${MNN_SEARCH_PATH}. Please ensure MNN is built.")
    # Fallback to system search
    find_library(MNN_LIB NAMES MNN)
endif()

if (MNN_LIB)
    message(STATUS "Found MNN library: ${MNN_LIB}")
else()
    message(FATAL_ERROR "MNN library not found.")
endif()

add_library(mnn-node SHARED src/MNN.cc)
set_target_properties(mnn-node PROPERTIES PREFIX "" SUFFIX ".node")

target_link_libraries(mnn-node ${MNN_LIB})

# Set RPATH so it finds the library at runtime
if(UNIX)
    get_filename_component(MNN_LIB_DIR ${MNN_LIB} DIRECTORY)
    set_target_properties(mnn-node PROPERTIES LINK_FLAGS "-Wl,-rpath,${MNN_LIB_DIR}")
endif()
