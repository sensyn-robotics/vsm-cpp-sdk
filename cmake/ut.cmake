# Include this script to create unit tests build scenario description file which
# uses sources from VSM C++ SDK, current directory and additional files
# specified via the following variables:
# VSM_SDK_DIR - path to VSM C++ SDK.
# UTPP_DIR - path to unittest++ framework.
# ADDITIONAL_SRC - additional sources (optional).
# Each unit test suite must be created in one separate .cpp file. Its name
# should start with "ut_" prefix followed by suite name and terminated by ".cpp"
# suffix. In this case the suite is automatically detected and added to the
# tests list to build and execute.
# Run unit tests by "ctest" command. Optional "--output-on-failure" option can
# be specified.

include("sdk_common")

enable_testing()

include("unittestpp")

set(CTEST_OUTPUT_ON_FAILURE ON)

# Use Valgrind if available on build platform. (Linux only)
set(VALGRIND valgrind --error-exitcode=255 --leak-check=full)
if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(EXT_TOOL ${VALGRIND})
else()
    set(EXT_TOOL "")
endif()

Find_platform_sources("${VSM_SDK_DIR}" PLATFORM_INCLUDES PLATFORM_SOURCES)

include_directories(${VSM_SDK_DIR}/include)
include_directories(${PLATFORM_INCLUDES})

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

# The best brutal hack agreed by VSM SDK developers to allow unit tests access
# protected and private members. Never ever copy this to SDK production code!
add_definitions(-Dprivate=public -Dprotected=public)

# Add all SDK source files
file(GLOB SDK_SRCS "${VSM_SDK_DIR}/src/*.cpp")
file(GLOB HEADERS "${VSM_SDK_DIR}/src/include/vsm/*.h")
set(SDK_SRCS ${SDK_SRCS} ${HEADERS} ${PLATFORM_SOURCES})
Build_mavlink(${VSM_SDK_DIR})
set(SDK_SRCS ${SDK_SRCS} ${MAV_AUTO_SRCS})
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

add_dependencies(ut_vsm_sdk unittestpp)

# Add additional sources as a separate library
if (DEFINED ADDITIONAL_SRC)
    add_library(ut_additional_sources STATIC ${ADDITIONAL_SRC})
    add_dependencies(ut_additional_sources ut_vsm_sdk)
    set(ADDITIONAL_SRC_LIB ut_additional_sources)
endif()

file(GLOB TEST_SRCS ut_*.cpp)
file(STRINGS no_valgrind.txt NO_VALGRIND)
set(TESTS, "")
foreach (TEST_SRC ${TEST_SRCS})
    string(REGEX REPLACE ".*/ut_(.+)\\.cpp" \\1 TEST_SRC ${TEST_SRC})
    list(APPEND TESTS ${TEST_SRC})
endforeach()

foreach (TEST ${TESTS})
    set (TEST_BIN ut_${TEST})
    add_executable(${TEST_BIN} ut_${TEST}.cpp main.cpp ${DLL_IMPORT_LIBS})
    add_dependencies(${TEST_BIN} initial_config)
    target_link_libraries(${TEST_BIN} unittestpp ${ADDITIONAL_SRC_LIB} ut_vsm_sdk ${VSM_PLAT_LIBS})
    add_test(${TEST} ${EXT_TOOL} ./${TEST_BIN})
    list(FIND NO_VALGRIND ${TEST} RES)
    if (RES GREATER -1)
        add_test(${TEST}_no_valgrind ./${TEST_BIN})
    endif()
endforeach()
