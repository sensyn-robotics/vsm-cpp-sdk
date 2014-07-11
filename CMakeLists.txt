cmake_minimum_required (VERSION 2.6)

project(vsm-sdk)

if(NOT DEFINED VSM_BUILD_DOC)
    SET(VSM_BUILD_DOC NO)
endif()

set(UGCS_PACKAGE_GROUP "vsm-sdk")
set(UGCS_PACKAGE_SUMMARY "UgCS VSM SDK used to develop vehicle specific modules for UgCS")

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# XXX Think about better way!!!
# Enable packaging script automatically if SDK is compiled from ugcs source tree.
if (NOT DEFINED UGCS_PACKAGING_ENABLED)
    # This is UgCS specific script for creating debian packages.
    if (EXISTS "${CMAKE_SOURCE_DIR}/../build-scripts/cmake/configure_packaging.cmake")
        set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/../build-scripts/cmake")
        include("configure_packaging")
        set(UGCS_PACKAGING_ENABLED YES)
    else()
        set(UGCS_PACKAGING_ENABLED NO)
    endif()
endif()

include("sdk_common")

Find_platform_sources("." PLATFORM_INCLUDES PLATFORM_SOURCES)

# Shared includes
include_directories(include)

# Platform includes
include_directories(${PLATFORM_INCLUDES})

# Add all source files
file(GLOB SOURCES "src/*.cpp")
file(GLOB HEADERS "src/include/vsm/*.h")
set(SOURCES ${SOURCES} ${HEADERS} ${PLATFORM_SOURCES})

# Process DLL module definitions on Windows
Process_dll_defs("src/platform/win")

# Setup MavLink compilation
Build_mavlink(${CMAKE_SOURCE_DIR})
set(SOURCES ${SOURCES} ${MAV_AUTO_SRCS})
set(SOURCES ${SOURCES} ${MAV_AUTO_LUA})

# add (static) library
add_library(${CMAKE_PROJECT_NAME} STATIC ${SOURCES} ${DLL_IMPORT_LIBS})
        
install(TARGETS ${CMAKE_PROJECT_NAME}
        RUNTIME DESTINATION "${UGCS_INSTALL_DIR}/bin"
        LIBRARY DESTINATION "${UGCS_INSTALL_DIR}/lib"
        ARCHIVE DESTINATION "${UGCS_INSTALL_DIR}/lib")

install(DIRECTORY include/ugcs  
        DESTINATION "${UGCS_INSTALL_DIR}/include")
install(DIRECTORY ${MAV_AUTO_INCLUDE_DIR}/ugcs  
        DESTINATION "${UGCS_INSTALL_DIR}/include")

# Install platform specific header for sockets.
if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(PLATFORM_SOCKETS_H "include/platform/linux/ugcs/vsm/platform_sockets.h")
elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    set(PLATFORM_SOCKETS_H "include/platform/mac/ugcs/vsm/platform_sockets.h")
elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")
    set(PLATFORM_SOCKETS_H "include/platform/win/ugcs/vsm/platform_sockets.h")
endif()

install(FILES ${PLATFORM_SOCKETS_H}
        DESTINATION "${UGCS_INSTALL_DIR}/include/ugcs/vsm")

install(FILES resources/configuration/vsm.conf 
        DESTINATION "${UGCS_INSTALL_DIR}/share")
install(FILES ${MAV_AUTO_LUA}       
        DESTINATION "${UGCS_INSTALL_DIR}/share")

install(DIRECTORY tools  
        DESTINATION "${UGCS_INSTALL_DIR}/share")

if(VSM_BUILD_DOC)
    file(GLOB EXAMPLES
         doc/examples/*.cpp doc/examples/*.h doc/examples/CMakeLists.txt)
    install(FILES ${EXAMPLES} 
            DESTINATION "${UGCS_INSTALL_DIR}/share/examples")
    install(DIRECTORY ${CMAKE_BINARY_DIR}/doc-default/html/ 
            DESTINATION "${UGCS_INSTALL_DIR}/share/doc")
endif()

install(FILES   "cmake/ugcs/vsm.cmake"
                "cmake/ugcs/common.cmake"
        DESTINATION "${UGCS_INSTALL_DIR}/share/cmake/ugcs")

install(FILES ${DLL_IMPORT_LIBS} 
        DESTINATION "${UGCS_INSTALL_DIR}/lib")

#Add package target.
include(CPack)
