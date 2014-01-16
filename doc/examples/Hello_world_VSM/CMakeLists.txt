# Template makefile for VSM applications.

cmake_minimum_required (VERSION 2.6)

# Name of VSM application
project(hello-world-vsm)

# Deal with path to SDK
if (NOT DEFINED VSM_SDK_DIR)
    if (DEFINED ENV{VSM_SDK_DIR})
        set(VSM_SDK_DIR $ENV{VSM_SDK_DIR})
    else()
        # Default SDK path
        if(CMAKE_SYSTEM_NAME MATCHES "Windows")
            set(VSM_SDK_DIR "C:/Program Files/vsm_sdk")
        else()
            set(VSM_SDK_DIR "/opt/vsm_sdk")
        endif()
    endif()
endif()

# Common for all VSMs
include("${VSM_SDK_DIR}/lib/make/vsm.cmake")

# Default VSM config file
# Optional arguments are source and destination file locations.
# For example Build_vsm_config(/path/to/your/vsm.conf) to copy vsm.conf from
# your location to default binary folder location or
# Build_vsm_config(/path/to/your/vsm.conf ${CMAKE_BINARY_DIR}/your.name) to
# copy and rename the destination file also.
Build_vsm_config()

# VSM source and header files
file(GLOB SOURCES "*.cpp" "*.h")

# VSM executable
add_executable(${CMAKE_PROJECT_NAME} ${SOURCES})

# Link to VSM SDK library
target_link_libraries(${CMAKE_PROJECT_NAME} ${VSM_LIBS})
