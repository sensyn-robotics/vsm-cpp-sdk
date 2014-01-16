cmake_minimum_required (VERSION 2.6)

project(vsm_sdk)

if(NOT DEFINED VSM_BUILD_DOC)
    SET(VSM_BUILD_DOC YES)
endif()

include("make/common.cmake")

# Set install prefix
set(CMAKE_INSTALL_PREFIX ${VSM_SDK_DIR})

# Shared includes
include_directories(include)

# Add all source files
file(GLOB SOURCES "src/*.cpp")
file(GLOB FILES "src/include/vsm/*.h")
set(SOURCES ${SOURCES} ${FILES})
Find_platform_sources("src/platform" PLATFORM_SOURCES)
set(SOURCES ${SOURCES} ${PLATFORM_SOURCES})

# Process DLL module definitions on Windows
Process_dll_defs("src/platform/win")

# Setup MavLink compilation
Build_mavlink(${CMAKE_SOURCE_DIR})
set(SOURCES ${SOURCES} ${MAV_AUTO_SRCS})
set(SOURCES ${SOURCES} ${MAV_AUTO_LUA})

# add (static) library
add_library(${CMAKE_PROJECT_NAME} STATIC ${SOURCES} ${DLL_IMPORT_LIBS})

# Installed files
install(TARGETS ${CMAKE_PROJECT_NAME}
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)
install(DIRECTORY include/vsm  DESTINATION include)
install(DIRECTORY ${MAV_AUTO_INCLUDE_DIR}/vsm  DESTINATION include)
install(FILES resources/configuration/vsm.conf DESTINATION share)
install(FILES ${MAV_AUTO_LUA} DESTINATION share)

install(DIRECTORY tools  DESTINATION share)

if(VSM_BUILD_DOC)
    file(GLOB EXAMPLES
         doc/examples/*.cpp doc/examples/*.h doc/examples/CMakeLists.txt)
    install(FILES ${EXAMPLES} DESTINATION share/examples)
    install(DIRECTORY ${CMAKE_BINARY_DIR}/doc-default/html/ DESTINATION share/doc)
endif()

install(DIRECTORY make DESTINATION lib)
install(FILES ${DLL_IMPORT_LIBS} DESTINATION lib)
