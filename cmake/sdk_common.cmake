# Common things for sdk and sdk unittests.

# Set this relative to this file to allow including form other places in dir tree.
set(SDK_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")

# Set correct compiler for cross compiling for BeagleBoneBlack
if (BEAGLEBONE)
    set (CMAKE_CXX_COMPILER "arm-linux-gnueabihf-g++")
    # Everything is statically linked for BeagleBoneBlack target
    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s -static-libstdc++ -static -pthread -std=c++14 -Wl,--whole-archive -lpthread -Wl,--no-whole-archive")
endif()

# Find all platform dependent source files in the specified SDK directory.
# include directory list is placed in INCLUDES_VAR
# Source file list is placed in SOURCES_VAR
# Header file list is placed in HEADERS_VAR
function(Find_platform_sources SDK_DIR INCLUDES_VAR SOURCES_VAR HEADERS_VAR)
    
    if (ANDROID)
        set(PLAT_NAME "android")
    else()
        set(PLAT_NAME ${CMAKE_SYSTEM_NAME})
    endif()

    string(TOLOWER ${PLAT_NAME} PLAT_NAME)
    
    if (PLAT_NAME MATCHES "windows")
        set(PLAT_NAME "win")
    elseif (PLAT_NAME MATCHES "darwin")
        set(PLAT_NAME "mac")
    endif()
    
    file(GLOB DIRS RELATIVE "${SDK_DIR}/src/platform" "${SDK_DIR}/src/platform/*")
    set(PLAT_SOURCES, "")
    foreach(DIR ${DIRS})
        string(FIND ${DIR} ${PLAT_NAME} has_match)
        if (NOT has_match EQUAL -1)
            file(GLOB _sources "${SDK_DIR}/src/platform/${DIR}/*.cpp")
            list(APPEND PLAT_SOURCES ${_sources})
            file(GLOB _sources "${SDK_DIR}/src/platform/${DIR}/*.c")
            list(APPEND PLAT_SOURCES ${_sources})
        endif()
    endforeach()

    file(GLOB DIRS RELATIVE "${SDK_DIR}/include/platform" "${SDK_DIR}/include/platform/*")
    set(PLAT_HEADERS, "")
    set(PLAT_INCLUDES, "")
    foreach(DIR ${DIRS})
        string(FIND ${DIR} ${PLAT_NAME} has_match)
        if (NOT has_match EQUAL -1)
            file(GLOB _headers "${SDK_DIR}/include/platform/${DIR}/*.h")
            list(APPEND PLAT_HEADERS ${_headers})
            list(APPEND PLAT_INCLUDES "${SDK_DIR}/include/platform/${DIR}")
        endif()
    endforeach()

    if(PLAT_NAME MATCHES "win")
        # MinGW does not have bfd.h in its standard includes, so hack it a bit.
        find_path(BFD_INCLUDE "bfd.h" PATH_SUFFIXES "..//include")
        if (BFD_INCLUDE)
            list(APPEND PLAT_INCLUDES ${BFD_INCLUDE})
        endif()
    endif()

    set(${SOURCES_VAR} ${PLAT_SOURCES} PARENT_SCOPE)
    set(${INCLUDES_VAR} ${PLAT_INCLUDES} PARENT_SCOPE)
    set(${HEADERS_VAR} ${PLAT_HEADERS} PARENT_SCOPE)
endfunction()

# Function for generating DLL import libraries from .def files in the specified
# directory. Global variable DLL_IMPORT_LIBS contains all the import libraries
# generated, DLL_IMPORT_LIB_NAMES - short names for generated libraries
function(Process_dll_defs DIR_PATH)
    if (CMAKE_SYSTEM_NAME MATCHES "Windows" AND ENABLE_DLL_IMPORT)
        file(GLOB DEFS "${DIR_PATH}/*.def")
        foreach(DEF ${DEFS})
            get_filename_component(BASENAME ${DEF} NAME_WE)
            set(LIBNAME ${CMAKE_BINARY_DIR}/lib${BASENAME}.a)
            add_custom_command(OUTPUT
                ${LIBNAME}
                COMMAND dlltool -k -d ${DEF} -l ${LIBNAME}
                DEPENDS ${DEF})
            set(DLL_IMPORT_LIBS ${DLL_IMPORT_LIBS} ${LIBNAME} PARENT_SCOPE)
            set(LIB_NAMES ${LIB_NAMES} ${LIBNAME})
            set(DLL_IMPORT_LIB_NAMES ${DLL_IMPORT_LIB_NAMES} ${BASENAME} PARENT_SCOPE)
        endforeach()
        set(VSM_PLAT_LIBS ${VSM_PLAT_LIBS} ${LIB_NAMES} PARENT_SCOPE)
    endif()
endfunction()

# Setup MavLink support code compilation. Should be used in SDK only.
# Set some variables in parent scope:
# MAVLINK_INCLUDES_VAR - directory with automatically generated headers.
# MAVLINK_SOURCES_VAR - list of automatically generated source files.
# MAVLINK_HEADERS_VAR - list of automatically generated header files.
# MAVLINK_LUA_VAR - generated wireshark dissector.
function(Build_mavlink SDK_SRC_DIR MAVLINK_INCLUDES_VAR MAVLINK_SOURCES_VAR MAVLINK_HEADERS_VAR MAVLINK_LUA_VAR)
    # Automatic generation of MAVLink definitions
    set(MAVGEN ${SDK_SRC_DIR}/tools/mavgen/mavgen.py)
    set(MAV_DEF_DIR ${SDK_SRC_DIR}/resources/mavlink)
    set(MAV_XML ${MAV_DEF_DIR}/common.xml
                ${MAV_DEF_DIR}/ardupilotmega.xml
                ${MAV_DEF_DIR}/sphengineering.xml
                ${MAV_DEF_DIR}/sensyn.xml)
    set(MAV_XSD ${SDK_SRC_DIR}/resources/mavlink/mavschema.xsd)

    set(MAVLINK_INCLUDES ${CMAKE_BINARY_DIR}/mavlink/include)

    set(MAVLINK_HEADERS 
        ${MAVLINK_INCLUDES}/ugcs/vsm/auto_mavlink_enums.h
        ${MAVLINK_INCLUDES}/ugcs/vsm/auto_mavlink_messages.h)
    
    set(MAVLINK_SOURCES ${CMAKE_BINARY_DIR}/mavlink/auto_mavlink_messages.cpp)
    set(MAVLINK_LUA ${CMAKE_BINARY_DIR}/mavlink/auto_mavlink_dissector.lua)

    set(${MAVLINK_SOURCES_VAR} ${MAVLINK_SOURCES} PARENT_SCOPE)
    set(${MAVLINK_HEADERS_VAR} ${MAVLINK_HEADERS} PARENT_SCOPE)
    set(${MAVLINK_INCLUDES_VAR} ${MAVLINK_INCLUDES} PARENT_SCOPE)
    set(${MAVLINK_LUA_VAR} ${MAVLINK_LUA} PARENT_SCOPE)
    
    set(XML_PARAM "")
    foreach(XML ${MAV_XML})
        set(XML_PARAM ${XML_PARAM} --xml-def=${XML})
    endforeach()

    add_custom_command(
        OUTPUT ${MAVLINK_HEADERS} ${MAVLINK_SOURCES}
        COMMAND python 
            -B ${MAVGEN} 
            --output-dir=${CMAKE_BINARY_DIR}/mavlink
            ${XML_PARAM} 
            --schema=${MAV_XSD}
        DEPENDS ${MAV_XML} ${MAV_XSD} ${MAVGEN})

    # Generate mavlink dissector for wireshark
    add_custom_command(
        OUTPUT ${MAVLINK_LUA}
        COMMAND python 
            -B ${MAVGEN} 
            --output-dir=${CMAKE_BINARY_DIR}/mavlink
            --lang=Lua
            --xml-def=${MAV_DEF_DIR}/common.xml
            --xml-def=${MAV_DEF_DIR}/sphengineering.xml
            --xml-def=${MAV_DEF_DIR}/ardupilotmega.xml
            --schema=${MAV_XSD}
            --merge-extensions
        DEPENDS ${MAV_XML} ${MAV_XSD} ${MAVGEN})

    add_custom_target(mavlink_lua ALL
        DEPENDS ${MAVLINK_LUA})

endfunction()

# Generate Android static library form given sources and adds it to the package.
# Script uses Application.mk.in and Android_sdk.mk.in templates.
# ANDROID_ABI - Semicolon separated list of target ABIs (Eg. "armeabi-v7a;x86" )
# ANDROID_PLATFORM - target platform, Eg. "android-21"
# SOURCES - Semicolon separated list of source files. Absolute paths required.
# INCLUDE_DIRS - Semicolon separated list of include directories. Absolute paths required.
function (Create_android_build ANDROID_ABI ANDROID_PLATFORM SOURCES INCLUDE_DIRS)
    set(ANDROID_APPLICATION_MK "${ANDROID_BINARY_DIR}/jni/Application.mk")
    set(ANDROID_ANDROID_MK "${ANDROID_BINARY_DIR}/jni/Android.mk")
    
    set(ANDROID_CFLAGS "-DSDK_VERSION_MAJOR=${SDK_VERSION_MAJOR} -DSDK_VERSION_MINOR=${SDK_VERSION_MINOR}")
    set(ANDROID_CFLAGS "${ANDROID_CFLAGS} -DSDK_VERSION_BUILD=\\\"${SDK_VERSION_BUILD}\\\"")
    set(NDK_BUILD_PARAMS "")
    if(CMAKE_BUILD_TYPE MATCHES "RELEASE")
        set(ANDROID_CFLAGS "${ANDROID_CFLAGS} -O2")
    else()
        set(ANDROID_CFLAGS "${ANDROID_CFLAGS} -O0 -g -DDEBUG")
        set(NDK_BUILD_PARAMS "${NDK_BUILD_PARAMS} NDK_DEBUG=1")
    endif()

    # Variables substituted in Android makefiles templates.
    List_to_string("${SOURCES}" ANDROID_SOURCES)
    List_to_string("${INCLUDE_DIRS}" ANDROID_INCLUDE_DIRS)
    List_to_string("${ANDROID_ABI}" ANDROID_ABI_LIST)

    foreach (ITEM ${ANDROID_ABI})
        install(FILES ${ANDROID_BINARY_DIR}/obj/local/${ITEM}/libvsm-sdk.a
                DESTINATION "${UGCS_INSTALL_DIR}/android/lib/${ITEM}")
    endforeach()

    configure_file(android/Application.mk.in ${ANDROID_APPLICATION_MK} @ONLY)
    configure_file(android/Android_sdk.mk.in ${ANDROID_ANDROID_MK} @ONLY)

    add_custom_target(
        android
        COMMAND ${ANDROID_NDK}/ndk-build ${NDK_BUILD_PARAMS} -C ${ANDROID_BINARY_DIR} -j4
        DEPENDS ${ANDROID_ANDROID_MK} ${ANDROID_APPLICATION_MK} ${SOURCES})

    install(FILES android/Application.mk.in android/Android_app.mk.in
            DESTINATION "${UGCS_INSTALL_DIR}/android")
    
    message ("Added target android with abi=${ANDROID_ABI}, platform=${ANDROID_PLATFORM}")
endfunction()

function(Add_cppcheck_target)
    if (CMAKE_SYSTEM_NAME MATCHES "Linux")
        set (PARAMS
            "--quiet"
            "--template=gcc"
            "--enable=all"
            "--std=c++11"
            "-UANDROID"
            "--suppress=noExplicitConstructor"
            "--suppress=missingIncludeSystem"
            "--suppress=unusedFunction")
        get_directory_property (PLIST COMPILE_DEFINITIONS)
        foreach (PARAM  ${PLIST})
            set (PARAMS ${PARAMS} -D${PARAM})
        endforeach()
        get_directory_property (PLIST INCLUDE_DIRECTORIES)
        foreach (PARAM  ${PLIST})
            set (PARAMS ${PARAMS} -I${PARAM})
        endforeach()
        set (PARAMS ${PARAMS} ${CMAKE_SOURCE_DIR}/src)
        add_custom_target (cppcheck
            COMMAND cppcheck ${PARAMS}
            VERBATIM)
        # This creates cppcheck-result.xml file in the build directory.
        # Used for buildserver integration.
        add_custom_target(cppcheckxml
            COMMAND cppcheck --xml-version=2 ${PARAMS} ${ARGV} 2> cppcheck-result.xml
            VERBATIM)
    endif()
endfunction()

function(Add_cpplint_target SOURCE_LIST)
    if (CMAKE_SYSTEM_NAME MATCHES "Linux")
        add_custom_target (cpplint
            COMMAND python ${CMAKE_SOURCE_DIR}/tools/cpplint.py
            --linelength=120
            # 1) Do not care about converting constructors for now.
            # 2) We are fine with std headers included in class header.
            #    No need for redundant include in cpp.
            # 3) suppress 'redundant "virtual"' warning
            # 4) suppress 'Is this a non-const reference' warning. All uses are legit.
            --filter=-runtime/explicit,-build/include_what_you_use,-readability/inheritance,-runtime/references
            --verbose=0
            ${ARGV}
            VERBATIM)
    endif()
endfunction()

Find_platform_sources("${SDK_SOURCE_ROOT}" PLATFORM_INCLUDES PLATFORM_SOURCES PLATFORM_HEADERS)

Compile_protobuf_definitions(
    "ucs_vsm.proto;ucs_vsm_defs.proto"
    "${SDK_SOURCE_ROOT}/resources/protobuf"
    "ucs_vsm_proto.h")

# Setup MavLink compilation
Build_mavlink(${SDK_SOURCE_ROOT} MAVLINK_INCLUDES MAVLINK_SOURCES MAVLINK_HEADERS MAVLINK_LUA)

# Add all SDK source files
file(GLOB SDK_SOURCES "${SDK_SOURCE_ROOT}/src/*.cpp")
file(GLOB SDK_HEADERS "${SDK_SOURCE_ROOT}/src/include/vsm/*.h")

set (SDK_INCLUDES "${SDK_SOURCE_ROOT}/include" ${PLATFORM_INCLUDES} ${MAVLINK_INCLUDES} ${CMAKE_BINARY_DIR}/protobuf_generated)
if (NOT ANDROID)
    set(SDK_INCLUDES ${SDK_INCLUDES} ${PROTOBUF_INCLUDE_DIR})
endif()

set (SDK_SOURCES ${SDK_SOURCES} ${PLATFORM_SOURCES} ${MAVLINK_SOURCES} ${PROTOBUF_AUTO_SOURCES})

set (SDK_HEADERS ${SDK_HEADERS} ${PLATFORM_HEADERS} ${MAVLINK_HEADERS} ${PROTOBUF_AUTO_HEADERS})

# Process DLL module definitions on Windows
Process_dll_defs("${SDK_SOURCE_ROOT}/src/platform/win")

include_directories(${SDK_INCLUDES})
