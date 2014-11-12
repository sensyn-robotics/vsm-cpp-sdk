# Common things for sdk and sdk unittests.

include("ugcs/common")

# Find all platform dependent source files in the specified SDK directory.
# include directory list is placed in INCLUDES_VAR
# Source file list is placed in SOURCES_VAR
function(Find_platform_sources SDK_DIR INCLUDES_VAR SOURCES_VAR)
    if (CMAKE_SYSTEM_NAME MATCHES "Linux")
        file(GLOB PLAT_SOURCES
                "${SDK_DIR}/src/platform/linux/*.h"
                "${SDK_DIR}/src/platform/linux/*.cpp"
                "${SDK_DIR}/src/platform/unix/*.h"
                "${SDK_DIR}/src/platform/unix/*.cpp"
                )
        set (PLAT_INCLUDES
                "${SDK_DIR}/include/platform/linux"
                "${SDK_DIR}/include/platform/unix"
                )
    elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
        file(GLOB PLAT_SOURCES
                "${SDK_DIR}/src/platform/mac/*.h"
                "${SDK_DIR}/src/platform/mac/*.cpp"
                "${SDK_DIR}/src/platform/unix/*.h"
                "${SDK_DIR}/src/platform/unix/*.cpp"
                )
        set (PLAT_INCLUDES
                "${SDK_DIR}/include/platform/mac"
                "${SDK_DIR}/include/platform/unix"
                )
    elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")
        file(GLOB PLAT_SOURCES
                "${SDK_DIR}/src/platform/win/*.h"
                "${SDK_DIR}/src/platform/win/*.cpp"
                )
        set (PLAT_INCLUDES
                "${SDK_DIR}/include/platform/win"
                )
        # MinGW does not have bfd.h in its standard includes, so hack it a bit.
        find_path(BFD_INCLUDE "bfd.h" PATH_SUFFIXES "..//include")
        set (PLAT_INCLUDES ${PLAT_INCLUDES} ${BFD_INCLUDE})
    endif()

    set(${SOURCES_VAR} ${PLAT_SOURCES} PARENT_SCOPE)
    set(${INCLUDES_VAR} ${PLAT_INCLUDES} PARENT_SCOPE)
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
    set(MAV_AUTO_ENUMS ${MAV_AUTO_INCLUDE_DIR}/ugcs/vsm/auto_mavlink_enums.h)
    set(MAV_AUTO_MSGS_HDR ${MAV_AUTO_INCLUDE_DIR}/ugcs/vsm/auto_mavlink_messages.h)
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

# Compile protobuf declarations into C++ code.
# PROTO_INPUT_FILES - should be paths relative to absolute path PROTO_ROOT
# Set some variables in parent scope:
# PROTOBUF_AUTO_SRCS - list of automatically generated source files which needs
# to be compiled.
function(Compile_protobuf_definitions PROTO_INPUT_FILES PROTO_ROOT
    PROTO_OUTPUT_DIR PROTO_COMMON_INCLUDE_NAME)
    
    # Binary generated during build process
    set(PROTOBUF_PROTOC ${CMAKE_BINARY_DIR}/protobuf/protoc)
    
    # Include prefix "ugcs/vsm" is hardcoded for auto-generated include files.
    # It works for SDK and does not harm others.
    set(ALL_DEFS_INCLUDE_FILE
        ${CMAKE_BINARY_DIR}/_all_protobuf_includes_generated_for_${PROTO_COMMON_INCLUDE_NAME})
    file(WRITE ${ALL_DEFS_INCLUDE_FILE} "// DO NOT EDIT! Generated automatically\n\n")
    
    # A rule for each proto file
    # Auto include is generated during configure phase, not build time.
    # Consider it "enough" for now.
    foreach(DEF ${PROTO_INPUT_FILES})
        string(REPLACE .proto .pb.h OUT_H ${DEF})
        set(OUT_H_FULL ${PROTO_OUTPUT_DIR}/${OUT_H})
        string(REPLACE .proto .pb.cc OUT_CC ${DEF})
        set(OUT_CC_FULL ${PROTO_OUTPUT_DIR}/${OUT_CC})
        set(PROTOBUF_AUTO_SRCS ${PROTOBUF_AUTO_SRCS} ${OUT_CC_FULL} ${OUT_H_FULL})
        file(APPEND ${ALL_DEFS_INCLUDE_FILE} "#include \"../../../${OUT_H}\"\n")
        add_custom_command(OUTPUT ${OUT_CC_FULL} ${OUT_H_FULL}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${PROTO_OUTPUT_DIR}
            COMMAND ${PROTOBUF_PROTOC} --cpp_out=${PROTO_OUTPUT_DIR} ${DEF}
            DEPENDS ${PROTO_ROOT}/${DEF}
            WORKING_DIRECTORY ${PROTO_ROOT}
            COMMENT "Protobuf: ${DEF}")
    endforeach()

    # A rule for building (copying) auto generated include file
    add_custom_command(OUTPUT ${PROTO_OUTPUT_DIR}/include/ugcs/vsm/${PROTO_COMMON_INCLUDE_NAME}
        COMMAND ${CMAKE_COMMAND}
            -E make_directory ${PROTO_OUTPUT_DIR}/include/ugcs/vsm
        COMMAND ${CMAKE_COMMAND} -E copy ${ALL_DEFS_INCLUDE_FILE} 
            ${PROTO_OUTPUT_DIR}/include/ugcs/vsm/${PROTO_COMMON_INCLUDE_NAME}
        DEPENDS ${ALL_DEFS_INCLUDE_FILE}
        COMMENT "Protobuf common include: ${PROTO_COMMON_INCLUDE_NAME}")
    
    set(PROTOBUF_AUTO_SRCS ${PROTOBUF_AUTO_SRCS}
        ${PROTO_OUTPUT_DIR}/include/ugcs/vsm/${PROTO_COMMON_INCLUDE_NAME})
    
    set(PROTOBUF_AUTO_SRCS ${PROTOBUF_AUTO_SRCS} PARENT_SCOPE)
    # Includes used by the generated headers itself
    include_directories(${CMAKE_SOURCE_DIR}/third-party/protobuf/src)
    # Includes for the generated headers
    include_directories(${PROTO_OUTPUT_DIR}/include) 
endfunction()
