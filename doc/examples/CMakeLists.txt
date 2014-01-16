# Examples building scenario.

cmake_minimum_required(VERSION 2.6)

# Deal with path to SDK
if (NOT DEFINED VSM_SDK_DIR)
    if (DEFINED ENV{VSM_SDK_DIR})
        set(VSM_SDK_DIR $ENV{VSM_SDK_DIR})
    else()
        # Default SDK path
        set(VSM_SDK_DIR "/opt/vsm_sdk")
    endif()
endif()
message("VSM SDK path set to ${VSM_SDK_DIR}")

include("${VSM_SDK_DIR}/lib/make/vsm.cmake")

# Copy initial configuration
add_custom_target(initial_config COMMENT "Copying initial configuration")
set(INITIAL_CONFIG_SRC ${VSM_SDK_DIR}/share/vsm.conf)
set(INITIAL_CONFIG_DST ${CMAKE_BINARY_DIR}/vsm.conf)
add_custom_command(TARGET initial_config COMMAND 
    ${CMAKE_COMMAND} -E copy ${INITIAL_CONFIG_SRC} ${INITIAL_CONFIG_DST})

# Gather all examples
file(GLOB EXAMPLES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cpp)

foreach (EXAMPLE ${EXAMPLES})
    string(REGEX REPLACE "(.*)\\.cpp" "\\1" EXAMPLE ${EXAMPLE})
    add_executable(${EXAMPLE} ${EXAMPLE}.cpp)
    add_dependencies(${EXAMPLE} initial_config)
    target_link_libraries(${EXAMPLE} ${VSM_LIBS})
endforeach()
