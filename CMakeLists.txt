cmake_minimum_required (VERSION 2.6)

project(vsm-sdk)

if(NOT DEFINED VSM_BUILD_DOC)
    SET(VSM_BUILD_DOC NO)
endif()

set(UGCS_PACKAGE_GROUP "vsm-sdk")
set(UGCS_PACKAGE_SUMMARY "UgCS VSM SDK used to develop vehicle specific modules for UgCS")

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# Compile protobuf compiler before including sdk_common.
# This ensures that protobuf_compiler is built for host platform.
add_subdirectory(third-party/protobuf/src ${CMAKE_BINARY_DIR}/protobuf)

include("sdk_common")

# Add all source files
file(GLOB SOURCES "src/*.cpp" "src/*.c")
file(GLOB HEADERS "include/ugcs/vsm/*.h")

# Setup platform specific sources
Find_platform_sources(${CMAKE_SOURCE_DIR}
                      PLATFORM_INCLUDES PLATFORM_SOURCES PLATFORM_HEADERS)

# Setup MavLink compilation
Build_mavlink(${CMAKE_SOURCE_DIR} MAVLINK_INCLUDES MAVLINK_SOURCES MAVLINK_HEADERS MAVLINK_LUA)

Compile_protobuf_definitions(
    "ucs_vsm.proto;ucs_vsm_defs.proto"
    "${CMAKE_SOURCE_DIR}/resources/protobuf"
    "ucs_vsm_proto.h")

# Platform includes and shared includes
set (SDK_INCLUDES "${CMAKE_SOURCE_DIR}/include" ${PLATFORM_INCLUDES} ${MAVLINK_INCLUDES})

set (SDK_SOURCES ${SOURCES} ${PLATFORM_SOURCES} ${MAVLINK_SOURCES} ${PROTOBUF_AUTO_SOURCES})

set (SDK_HEADERS ${HEADERS} ${PLATFORM_HEADERS} ${MAVLINK_HEADERS} ${PROTOBUF_AUTO_HEADERS})

# Process DLL module definitions on Windows
Process_dll_defs("src/platform/win")

include_directories(${SDK_INCLUDES})

if (ANDROID)
    # Add protobuf include directories.
    set (SDK_INCLUDES
        ${SDK_INCLUDES}
        "${CMAKE_BINARY_DIR}/protobuf_generated"
        "${CMAKE_SOURCE_DIR}/third-party/protobuf/src")

    # Add protobuf sources to vsm-sdk library
    set(PROTO_ROOT "${CMAKE_SOURCE_DIR}/third-party/protobuf/src/google/protobuf")    
    set(SDK_SOURCES ${SDK_SOURCES} ${SDK_HEADERS}
        ${PROTO_ROOT}/stubs/atomicops_internals_x86_gcc.cc         
        ${PROTO_ROOT}/stubs/atomicops_internals_x86_msvc.cc        
        ${PROTO_ROOT}/stubs/common.cc                              
        ${PROTO_ROOT}/stubs/once.cc                                
        ${PROTO_ROOT}/stubs/hash.h                                 
        ${PROTO_ROOT}/stubs/map-util.h                             
        ${PROTO_ROOT}/stubs/stl_util.h                             
        ${PROTO_ROOT}/stubs/stringprintf.cc                        
        ${PROTO_ROOT}/stubs/stringprintf.h                         
        ${PROTO_ROOT}/extension_set.cc                             
        ${PROTO_ROOT}/generated_message_util.cc                    
        ${PROTO_ROOT}/message_lite.cc                              
        ${PROTO_ROOT}/repeated_field.cc                            
        ${PROTO_ROOT}/wire_format_lite.cc                          
        ${PROTO_ROOT}/io/coded_stream.cc                           
        ${PROTO_ROOT}/io/coded_stream_inl.h                        
        ${PROTO_ROOT}/io/zero_copy_stream.cc                       
        ${PROTO_ROOT}/io/zero_copy_stream_impl_lite.cc
        ${PROTO_ROOT}/stubs/strutil.cc                             
        ${PROTO_ROOT}/stubs/strutil.h                              
        ${PROTO_ROOT}/stubs/substitute.cc                          
        ${PROTO_ROOT}/stubs/substitute.h                           
        ${PROTO_ROOT}/stubs/structurally_valid.cc                  
        ${PROTO_ROOT}/descriptor.cc                                
        ${PROTO_ROOT}/descriptor.pb.cc                             
        ${PROTO_ROOT}/descriptor_database.cc                       
        ${PROTO_ROOT}/dynamic_message.cc                           
        ${PROTO_ROOT}/extension_set_heavy.cc                       
        ${PROTO_ROOT}/generated_message_reflection.cc              
        ${PROTO_ROOT}/message.cc                                   
        ${PROTO_ROOT}/reflection_ops.cc                            
        ${PROTO_ROOT}/service.cc                                   
        ${PROTO_ROOT}/text_format.cc                               
        ${PROTO_ROOT}/unknown_field_set.cc                         
        ${PROTO_ROOT}/wire_format.cc                               
        ${PROTO_ROOT}/io/gzip_stream.cc                            
        ${PROTO_ROOT}/io/printer.cc                                
        ${PROTO_ROOT}/io/tokenizer.cc                              
        ${PROTO_ROOT}/io/zero_copy_stream_impl.cc                  
        ${PROTO_ROOT}/compiler/importer.cc                         
        ${PROTO_ROOT}/compiler/parser.cc
    )

    # Add target "android"
    Create_android_build("${ANDROID_ABI_LIST}" "${ANDROID_PLATFORM}" "${SDK_SOURCES}" "${SDK_INCLUDES}")
    add_custom_target(${CMAKE_PROJECT_NAME} ALL)
    add_dependencies(${CMAKE_PROJECT_NAME} android)
else()
    # Build the vsm-sdk static library
    if (BEAGLEBONE)
        # Create protobuf lib for BB.
        add_subdirectory(third-party/protobuf/src ${CMAKE_BINARY_DIR}/protobuf_bb)
        add_library(${CMAKE_PROJECT_NAME} STATIC
            ${SDK_SOURCES} ${SDK_HEADERS} ${DLL_IMPORT_LIBS}
            $<TARGET_OBJECTS:protobuf_objlib_bb>
            )
        set (LIBRARY_INSTALL_DIR "${UGCS_INSTALL_DIR}/beaglebone/lib")
    else()

        add_library(${CMAKE_PROJECT_NAME} STATIC
            ${SDK_SOURCES} ${SDK_HEADERS} ${DLL_IMPORT_LIBS}
            $<TARGET_OBJECTS:protobuf_objlib>
            )
        set (LIBRARY_INSTALL_DIR "${UGCS_INSTALL_DIR}/lib")
    endif()

    # Install vsm-sdk library
    install(TARGETS ${CMAKE_PROJECT_NAME}
            RUNTIME DESTINATION "${UGCS_INSTALL_DIR}/bin"
            LIBRARY DESTINATION "${LIBRARY_INSTALL_DIR}"
            ARCHIVE DESTINATION "${LIBRARY_INSTALL_DIR}")
endif()


add_dependencies(${CMAKE_PROJECT_NAME} mavlink_lua)

add_dependencies (${CMAKE_PROJECT_NAME} protobuf_compiler)

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

# Install google headers
install (DIRECTORY third-party/protobuf/src/google
        DESTINATION "${UGCS_INSTALL_DIR}/include"
        FILES_MATCHING PATTERN "*.h")

# Install import dlls
install (FILES ${DLL_IMPORT_LIBS} 
        DESTINATION "${UGCS_INSTALL_DIR}/lib")

#Add package target.
include (CPack)
