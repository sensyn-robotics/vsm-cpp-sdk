# Module tests build scenario description file.
# Each module test should be placed in a separate directory which name has "mt_"
# prefix. Any number of arbitrary named .cpp files can be inside - they all are
# found and compiled automatically.

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

# Gather all tests
file(GLOB TESTS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} mt_*)

foreach (TEST ${TESTS})
    file(GLOB SRCS ${TEST}/*.cpp)
    add_executable(${TEST} ${SRCS})
    target_link_libraries(${TEST} ${VSM_LIBS})
endforeach()
