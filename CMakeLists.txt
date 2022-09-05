cmake_minimum_required (VERSION 3.8.2)

project(vsm-sdk)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
add_definitions(-Wno-deprecated)

if(NOT DEFINED VSM_BUILD_DOC)
    SET(VSM_BUILD_DOC NO)
endif()

set(UGCS_PACKAGE_GROUP "vsm-sdk")
set(UGCS_PACKAGE_SUMMARY "UgCS VSM SDK used to develop vehicle specific modules for UgCS")

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

include("ugcs/common")
include("sdk_common")

if (CROSS_COMPILE)
    # When cross compiling for android or beanglebone,
    # we are building libprotobuf-lite from sources and add it directly into sdk.
    # Therefore PROTOBUF_SOURCE_ROOT pointing to protobuf sources is required.
    # The required version is at least 3.6.1.
    # AND it must be equal to the libprotobuf version used by this SDK.
    if (NOT PROTOBUF_SOURCE_ROOT)
        set (PROTOBUF_SOURCE_ROOT "$ENV{PROTOBUF_SOURCE_ROOT}")
    endif()
    if (NOT PROTOBUF_SOURCE_ROOT)
        message (FATAL_ERROR "Please specify protobuf source directory via PROTOBUF_SOURCE_ROOT")
    endif()
    # Set dummy version so libprotobuf.cmake script does not fail.
    set (protobuf_VERSION "1.1.1")
    # set variable for libprotobuf.cmake script
    set (protobuf_source_dir "${PROTOBUF_SOURCE_ROOT}")
    # Use their cmake file to get the library source list.
    set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${protobuf_source_dir}/cmake")
    # If you get error about "include not found" here please update your protobuf sources to at least 3.6.1.
    include ("libprotobuf-lite")
    # We are not interested in building the lib itself, remove the target form all.
    set_target_properties(libprotobuf-lite PROPERTIES EXCLUDE_FROM_ALL YES)

    # Add protobuf sources to vsm-sdk library
    set (SDK_SOURCES ${SDK_SOURCES} ${libprotobuf_lite_files})
    # Add protobuf include dirs to vsm-sdk library
    set (SDK_INCLUDES ${SDK_INCLUDES} ${protobuf_source_dir}/src)

    # Disable some warnings for proto sources.
    foreach(CC_FILE ${libprotobuf_lite_files})
        set_source_files_properties(${CC_FILE} PROPERTIES 
            COMPILE_FLAGS "-Wno-sign-compare -Wno-unused-parameter -Wno-ignored-qualifiers -Wno-old-style-cast")
    endforeach()
    # Add protobuf include dir
    include_directories(${protobuf_source_dir}/src)
    # Tell the protobuf source we have pthreads.
    add_definitions(-DHAVE_PTHREAD)
endif()

if (ANDROID)
    # Add target "android"
    Create_android_build("${ANDROID_ABI_LIST}" "${ANDROID_PLATFORM}" "${SDK_SOURCES}" "${SDK_INCLUDES}")
    add_custom_target(${CMAKE_PROJECT_NAME} ALL)
    add_dependencies(${CMAKE_PROJECT_NAME} android)
else()
    # Build the vsm-sdk static library
    if (BEAGLEBONE)
        # Create protobuf lib for BB.
        set (LIBRARY_INSTALL_DIR "${UGCS_INSTALL_DIR}/beaglebone/lib")
    else()
        set (LIBRARY_INSTALL_DIR "${UGCS_INSTALL_DIR}/lib")
    endif()

    add_library(${CMAKE_PROJECT_NAME} STATIC ${SDK_SOURCES} ${SDK_HEADERS} ${DLL_IMPORT_LIBS})

    # Install vsm-sdk library
    install(TARGETS ${CMAKE_PROJECT_NAME}
            RUNTIME DESTINATION "${UGCS_INSTALL_DIR}/bin"
            LIBRARY DESTINATION "${LIBRARY_INSTALL_DIR}"
            ARCHIVE DESTINATION "${LIBRARY_INSTALL_DIR}")
endif()

Add_cppcheck_target()

set (LINT_SOURCES ${SOURCES} ${PLATFORM_SOURCES} ${HEADERS} ${PLATFORM_HEADERS})
Add_cpplint_target(${LINT_SOURCES})

# Install common includes
install (DIRECTORY include/ugcs
        DESTINATION "${UGCS_INSTALL_DIR}/include")

# Install mavlink includes
install (DIRECTORY ${MAVLINK_INCLUDES}/ugcs
        DESTINATION "${UGCS_INSTALL_DIR}/include")

# Install protobuf includes
install (DIRECTORY ${CMAKE_BINARY_DIR}/protobuf_generated/
        DESTINATION "${UGCS_INSTALL_DIR}/include/generated"
        FILES_MATCHING PATTERN *.h)

# Install platform specific header for sockets.
#XXX eliminate this
if (ANDROID OR CMAKE_SYSTEM_NAME MATCHES "Linux")
    set (PLATFORM_SOCKETS_H "include/platform/android-linux/ugcs/vsm/platform_sockets.h")
elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    set (PLATFORM_SOCKETS_H "include/platform/mac/ugcs/vsm/platform_sockets.h")
elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")
    set (PLATFORM_SOCKETS_H "include/platform/win/ugcs/vsm/platform_sockets.h")
endif()

install(FILES ${PLATFORM_SOCKETS_H}
        DESTINATION "${UGCS_INSTALL_DIR}/include/ugcs/vsm")

# Install sample vsm.conf
install(FILES resources/configuration/vsm.conf 
        DESTINATION "${UGCS_INSTALL_DIR}/share")

# Install wireshark mavlink dissector plugin
install(FILES ${MAVLINK_LUA}
        DESTINATION "${UGCS_INSTALL_DIR}/share")

# Install tools
install(DIRECTORY tools
        DESTINATION "${UGCS_INSTALL_DIR}/share")

# Install docs
if(VSM_BUILD_DOC)
    file (GLOB EXAMPLES
         doc/examples/*.cpp doc/examples/*.h doc/examples/CMakeLists.txt)
    install (FILES ${EXAMPLES} 
            DESTINATION "${UGCS_INSTALL_DIR}/share/examples")
    install (DIRECTORY ${CMAKE_BINARY_DIR}/doc-default/html/ 
            DESTINATION "${UGCS_INSTALL_DIR}/share/doc")
endif()

# Install cmake helper scripts
install(FILES   "cmake/ugcs/vsm.cmake"
                "cmake/ugcs/common.cmake"
                "cmake/ugcs/ut.cmake"
                "cmake/ugcs/unittestpp.cmake"
        DESTINATION "${UGCS_INSTALL_DIR}/share/cmake/ugcs")

# Install import dlls
install (FILES ${DLL_IMPORT_LIBS} 
        DESTINATION "${UGCS_INSTALL_DIR}/lib")

#Add package target.
include (CPack)
