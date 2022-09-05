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

include("ugcs/common")
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

if (DEFINED VSM_MEMORY_SANITIZER OR DEFINED ENV{VSM_MEMORY_SANITIZER})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
    # Valgrind cannot work together with address sanitizer
    set(EXT_TOOL "")
endif()

# Does not work now
if (DEFINED ENV{VSM_THREAD_SANITIZER})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread -shared -fPIC")
endif()

#suppress warnings about throw in destructors
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-terminate")

add_definitions(-DDEBUG -DUNITTEST)

# Copy initial configuration and resources
add_custom_target(initial_config COMMENT "Copying initial configuration")
set(INITIAL_CONFIG_SRC ${SDK_SOURCE_ROOT}/resources/configuration/vsm.conf)
set(INITIAL_CONFIG_DST ${CMAKE_BINARY_DIR}/vsm.conf)
add_custom_command(TARGET initial_config COMMAND 
    ${CMAKE_COMMAND} -E copy ${INITIAL_CONFIG_SRC} ${INITIAL_CONFIG_DST})

add_library(ut_vsm_sdk STATIC ${SDK_SOURCES} ${SDK_HEADERS})

add_dependencies(ut_vsm_sdk unittestpp initial_config)

set(EXT_LIB ut_vsm_sdk ${VSM_PLAT_LIBS} ${PROTOBUF_LIBRARIES})

# Make sure the SDK example can be built
add_executable(hello_world_vsm ${CMAKE_SOURCE_DIR}/../../doc/examples/Hello_world_VSM/hello_world_vsm.cpp ${DLL_IMPORT_LIBS})
target_link_libraries(hello_world_vsm ${EXT_LIB})

include(ugcs/ut)
