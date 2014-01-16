# Common things for all components.

include(CPack)


# Verbose output so eclipse can parse it 
set(CMAKE_VERBOSE_MAKEFILE ON)


# Deal with path to SDK
if (NOT DEFINED VSM_SDK_DIR)
    if (DEFINED ENV{VSM_SDK_DIR})
        set(VSM_SDK_DIR $ENV{VSM_SDK_DIR})
    else()
        # Default SDK path
        if(CMAKE_SYSTEM_NAME MATCHES "Windows")
            set(VSM_SDK_DIR "C:/Program Files/vsm_sdk")
        else()
            set(VSM_SDK_DIR "/opt/vsm_sdk")
        endif()
    endif()
endif()
message("VSM SDK path set to ${VSM_SDK_DIR}")


# Enable C++11 standard, extended warnings, multithreading.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fmessage-length=0 -std=c++11 -Werror -Wall -Wextra -Wold-style-cast -pthread")


# Debug build options
if(NOT CMAKE_BUILD_TYPE MATCHES "RELEASE")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -gdwarf-3 -fno-omit-frame-pointer")
    add_definitions(-DDEBUG)
endif()


# Support DLL import on Windows 6.x
if (CMAKE_SYSTEM MATCHES "Windows-6\\.[0-9]+")
    set(OBSOLETE_WINDOWS OFF)
    set(ENABLE_DLL_IMPORT ON)
elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")
    set(OBSOLETE_WINDOWS ON)
    message("Obsolete windows version detected, some features will be disabled")
    set(ENABLE_DLL_IMPORT OFF)
    add_definitions(-DVSM_DISABLE_HID)
endif()

# Find all platform dependent source files in the specified directory. All found
# file names are placed in variable which name is in VAR_NAME argument.
function(Find_platform_sources PLATFORM_DIR VAR_NAME)
    if (CMAKE_SYSTEM_NAME MATCHES "Linux")
        set(SUBDIR "linux")
    elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")
        set(SUBDIR "win")
        # MinGW does not have bfd.h in its standard includes, so hack it a bit.
        find_path(BFD_INCLUDE "bfd.h" PATH_SUFFIXES "..//include")
        include_directories(${BFD_INCLUDE})
    elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
        set(SUBDIR "mac")
    endif()

    file(GLOB SOURCES "${PLATFORM_DIR}/${SUBDIR}/*.cpp")
    file(GLOB HEADERS "${PLATFORM_DIR}/${SUBDIR}/*.h")
    set(${VAR_NAME} ${SOURCES} ${HEADERS} PARENT_SCOPE)
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
# MAV_AUTO_INCLUDE_DIR - directory with automatically generated headers.
# MAV_AUTO_SRCS - list of automatically generated source files.
function(Build_mavlink SDK_SRC_DIR)
    # Automatic generation of MAVLink definitions
    set(MAVGEN python -B ${SDK_SRC_DIR}/tools/mavgen/mavgen.py)
    set(MAV_DEF_DIR ${SDK_SRC_DIR}/resources/mavlink/message_definitions/v1.0)
    set(MAV_XML ${MAV_DEF_DIR}/common.xml
                ${MAV_DEF_DIR}/ardupilotmega.xml
                ${MAV_DEF_DIR}/ugcs.xml)
    set(MAV_XSD ${SDK_SRC_DIR}/resources/mavlink/mavschema.xsd)
    set(MAV_AUTO_INCLUDE_DIR ${CMAKE_BINARY_DIR}/mavlink/include)
    set(MAV_AUTO_INCLUDE_DIR ${CMAKE_BINARY_DIR}/mavlink/include PARENT_SCOPE)
    set(MAV_AUTO_ENUMS ${MAV_AUTO_INCLUDE_DIR}/vsm/auto_mavlink_enums.h)
    set(MAV_AUTO_MSGS_HDR ${MAV_AUTO_INCLUDE_DIR}/vsm/auto_mavlink_messages.h)
    set(MAV_AUTO_MSGS_IMPL ${CMAKE_BINARY_DIR}/mavlink/auto_mavlink_messages.cpp)
    set(MAV_AUTO_SRCS ${MAV_AUTO_ENUMS} ${MAV_AUTO_MSGS_HDR} ${MAV_AUTO_MSGS_IMPL})
    set(MAV_AUTO_SRCS ${MAV_AUTO_ENUMS} ${MAV_AUTO_MSGS_HDR} ${MAV_AUTO_MSGS_IMPL} PARENT_SCOPE)
    
    set(MAV_AUTO_LUA ${CMAKE_BINARY_DIR}/mavlink/auto_mavlink_dissector.lua)
    set(MAV_AUTO_LUA ${CMAKE_BINARY_DIR}/mavlink/auto_mavlink_dissector.lua PARENT_SCOPE)
    
    set(XML_PARAM "")
    foreach(XML ${MAV_XML})
        set(XML_PARAM ${XML_PARAM} --xml-def=${XML})
    endforeach()
    
    add_custom_command(OUTPUT
        ${MAV_AUTO_SRCS}
        COMMAND ${MAVGEN} --output-dir=${CMAKE_BINARY_DIR}/mavlink
        ${XML_PARAM} --schema=${MAV_XSD}
        DEPENDS ${MAV_XML} ${MAV_XSD})
    # Generate mavlink dissector for wireshark
    add_custom_command(OUTPUT
        ${MAV_AUTO_LUA}
        COMMAND ${MAVGEN} --output-dir=${CMAKE_BINARY_DIR}/mavlink
        --lang=Lua
        ${XML_PARAM}
        --schema=${MAV_XSD}
        --merge-extensions
        DEPENDS ${MAV_XML} ${MAV_XSD})
    include_directories(${CMAKE_BINARY_DIR}/mavlink/include)
endfunction()

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(VSM_PLAT_LIBS rt)
elseif (CMAKE_SYSTEM_NAME MATCHES "Windows")
    set(VSM_PLAT_LIBS ws2_32 Userenv bfd iberty dbghelp z)
endif()


# XXX check other platforms
set(DOXYGEN doxygen)
if (NOT DEFINED PLANTUML)
    set(PLANTUML java -jar ${CMAKE_SOURCE_DIR}/tools/plantuml.jar -failonerror)
endif()


# Documentation

# Configuration name
if (NOT DEFINED DOC_CONFIG_NAME)
    if (DEFINED ENV{DOC_CONFIG_NAME})
        set(DOC_CONFIG_NAME $ENV{DOC_CONFIG_NAME})
    else()
        # Default configuration
        set(DOC_CONFIG_NAME default)
    endif()
endif()


# Documentation output directory
set(DOC_DIR ${CMAKE_BINARY_DIR}/doc-${DOC_CONFIG_NAME})

add_custom_target(doc_dir ${CMAKE_COMMAND} -E make_directory ${DOC_DIR})
add_custom_target(doc_diagrams_dir ${CMAKE_COMMAND} -E make_directory ${DOC_DIR}/images/diagrams
                  DEPENDS doc_dir)
add_custom_target(doc_dirs DEPENDS doc_dir doc_diagrams_dir)

add_custom_target(doc_diagrams ${PLANTUML} -o ${DOC_DIR}/images/diagrams diagrams/*.uml
                  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/doc
                  DEPENDS doc_diagrams_dir)
    
if(CMAKE_SYSTEM_NAME MATCHES "Windows")
    if(VSM_BUILD_DOC)
        add_custom_target(doc ALL set VSM_SDK_DOC_OUTPUT_DIR=${DOC_DIR} && ${DOXYGEN} ${DOC_CONFIG_NAME}.conf
                          WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/doc
                          DEPENDS doc_dirs doc_diagrams)
    else()
        add_custom_target(doc set VSM_SDK_DOC_OUTPUT_DIR=${DOC_DIR} && ${DOXYGEN} ${DOC_CONFIG_NAME}.conf
                          WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/doc
                          DEPENDS doc_dirs doc_diagrams)
    endif()
    
else()
    if(VSM_BUILD_DOC)
        add_custom_target(doc ALL env VSM_SDK_DOC_OUTPUT_DIR=${DOC_DIR} ${DOXYGEN} ${DOC_CONFIG_NAME}.conf
                          WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/doc
                          DEPENDS doc_dirs doc_diagrams)
    else()
        add_custom_target(doc env VSM_SDK_DOC_OUTPUT_DIR=${DOC_DIR} ${DOXYGEN} ${DOC_CONFIG_NAME}.conf
                          WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/doc
                          DEPENDS doc_dirs doc_diagrams)
    endif()
endif()