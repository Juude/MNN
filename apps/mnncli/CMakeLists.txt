option(BUILD_MNNCLI "Build PC Commandline." OFF)

if (BUILD_MNNCLI)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum macOS version" FORCE)

    add_executable(mnncli
            ${CMAKE_CURRENT_LIST_DIR}/src/mnncli.cpp
            ${CMAKE_CURRENT_LIST_DIR}/src/remote_model_downloader.cpp
            ${CMAKE_CURRENT_LIST_DIR}/src/file_utils.cpp
            ${CMAKE_CURRENT_LIST_DIR}/src/llm_benchmark.cpp
            ${CMAKE_CURRENT_LIST_DIR}/src/hf_api_client.cpp
            ${CMAKE_CURRENT_LIST_DIR}/src/mnncli_server.cpp
    )
    
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
        target_include_directories(mnncli PRIVATE ${OPENSSL_INCLUDE_DIR})

        # Link to OpenSSL libraries
        target_link_libraries(mnncli PRIVATE ${OPENSSL_LIBRARIES})

            # If on Apple platform, link with Security framework and Core Foundation
    if(APPLE)
        find_library(SECURITY_FRAMEWORK Security)
        find_library(CORE_FOUNDATION_FRAMEWORK CoreFoundation)
        if(SECURITY_FRAMEWORK)
            target_link_libraries(mnncli PRIVATE ${SECURITY_FRAMEWORK})
        else()
            message(FATAL_ERROR "Could not find Security framework")
        endif()
        if(CORE_FOUNDATION_FRAMEWORK)
            target_link_libraries(mnncli PRIVATE ${CORE_FOUNDATION_FRAMEWORK})
        else()
            message(FATAL_ERROR "Could not find CoreFoundation framework")
        endif()
    endif()
    else()
        message(FATAL_ERROR "OpenSSL not found. Please install OpenSSL and set paths.")
    endif()

    target_include_directories(mnncli PRIVATE ${MNN_INCLUDES} ${CMAKE_CURRENT_LIST_DIR}/include/ ${CMAKE_CURRENT_LIST_DIR}/../../include/ ${CMAKE_CURRENT_LIST_DIR}/../../transformers/llm/engine/src/ ${CMAKE_CURRENT_LIST_DIR}/../../3rd_party/)
    set_property(TARGET mnncli PROPERTY CXX_STANDARD 17)
    set_property(TARGET mnncli PROPERTY CXX_STANDARD_REQUIRED ON)
    
    # Enable exceptions for mnncli (override global -fno-exceptions)
    target_compile_options(mnncli PRIVATE -fexceptions)
    
    # Add library paths for pre-built libraries
    target_link_directories(mnncli PRIVATE 
        ${CMAKE_CURRENT_LIST_DIR}/../../build_mac_metal/lib
        ${CMAKE_CURRENT_LIST_DIR}/../../build_mac_metal
    )
    
    target_link_libraries(mnncli PRIVATE ${MNN_DEPS} MNN llm)
    target_compile_definitions(mnncli PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
endif()
