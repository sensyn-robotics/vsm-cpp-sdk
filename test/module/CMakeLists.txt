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
        if(CMAKE_SYSTEM_NAME MATCHES "Windows")
            set(VSM_SDK_DIR "C:/UGCS/vsm-sdk")
        else()
            set(VSM_SDK_DIR "/opt/ugcs/vsm-sdk")
        endif()
    endif()
endif()
message("VSM SDK path set to ${VSM_SDK_DIR}")

set(UGCS_PACKAGE_SUMMARY "Module test for serial processor")
set(CMAKE_MODULE_PATH "${VSM_SDK_DIR}/share/cmake")
include("ugcs/vsm")

# Gather all tests
file(GLOB TESTS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} mt_*)

foreach (TEST ${TESTS})
    file(GLOB SRCS ${TEST}/*.cpp)
    add_executable(${TEST} ${SRCS})
    target_link_libraries(${TEST} ${VSM_LIBS})
endforeach()
