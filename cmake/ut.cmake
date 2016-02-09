# Include this script to create unit tests build scenario description file which
# uses sources from VSM C++ SDK, current directory and additional files
# specified via the following variables:
# VSM_SDK_DIR - path to VSM C++ SDK.
# UTPP_DIR - path to unittest++ framework.
# Each unit test suite must be created in one separate .cpp file. Its name
# should start with "ut_" prefix followed by suite name and terminated by ".cpp"
# suffix. In this case the suite is automatically detected and added to the
# tests list to build and execute.
# Run unit tests by "ctest" command. Optional "--output-on-failure" option can
# be specified.

include("sdk_common")

enable_testing()

set(CTEST_OUTPUT_ON_FAILURE ON)

# Use Valgrind if available on build platform. (Linux only)
set(VALGRIND valgrind --error-exitcode=255 --leak-check=full)
if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(EXT_TOOL ${VALGRIND})
else()
    set(EXT_TOOL "")
endif()

Find_platform_sources("${VSM_SDK_DIR}"
                      PLATFORM_INCLUDES PLATFORM_SOURCES PLATFORM_HEADERS)
Build_mavlink(${VSM_SDK_DIR} MAVLINK_INCLUDES MAVLINK_SOURCES MAVLINK_HEADERS MAVLINK_LUA)

include_directories(${VSM_SDK_DIR}/include)
include_directories(${PLATFORM_INCLUDES} ${MAVLINK_INCLUDES})
include_directories(${VSM_SDK_DIR}/third-party/protobuf/src)

if (DEFINED VSM_MEMORY_SANITIZER OR DEFINED ENV{VSM_MEMORY_SANITIZER})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
    # Valgrind cannot work together with address sanitizer
    set(EXT_TOOL "")
endif()

# Does not work now
if (DEFINED ENV{VSM_THREAD_SANITIZER})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread -shared -fPIC")
endif()

add_definitions(-DDEBUG -DUNITTEST)

# Add all SDK source files
file(GLOB SDK_SRCS "${VSM_SDK_DIR}/src/*.cpp")
file(GLOB HEADERS "${VSM_SDK_DIR}/src/include/vsm/*.h")
set(SDK_SRCS ${SDK_SRCS} ${HEADERS} ${PLATFORM_SOURCES} ${PLATFORM_HEADERS})
set(SDK_SRCS ${SDK_SRCS} ${MAVLINK_SOURCES} ${MAVLINK_HEADERS})
# Process DLL module definitions on Windows
Process_dll_defs("${VSM_SDK_DIR}/src/platform/win")

# Copy initial configuration and resources
add_custom_target(initial_config COMMENT "Copying initial configuration")
set(INITIAL_CONFIG_SRC ${VSM_SDK_DIR}/resources/configuration/vsm.conf)
set(INITIAL_CONFIG_DST ${CMAKE_BINARY_DIR}/vsm.conf)
add_custom_command(TARGET initial_config COMMAND 
    ${CMAKE_COMMAND} -E copy ${INITIAL_CONFIG_SRC} ${INITIAL_CONFIG_DST})
add_custom_command(TARGET initial_config COMMAND 
    ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/resources
    ${CMAKE_BINARY_DIR}/resources)

add_library(ut_vsm_sdk STATIC ${SDK_SRCS})

add_dependencies(ut_vsm_sdk unittestpp initial_config)

set(EXT_LIB ut_vsm_sdk)
include(ugcs/ut)
