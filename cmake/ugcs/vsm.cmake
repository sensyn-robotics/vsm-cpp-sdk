# Build settings for VSM binaries

set(PLANTUML java -jar ${VSM_SDK_DIR}/share/tools/plantuml.jar -failonerror)

set(VSM_EXECUTABLE_NAME ${CMAKE_PROJECT_NAME})

include("ugcs/common")

# Set correct compiler for cross compiling for BeagleBoneBlack
if (BEAGLEBONE)
    # Everything is statically linked for BeagleBoneBlack target for now.
    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s -static-libstdc++ -static -pthread -std=c++11 -Wl,--whole-archive -lpthread -Wl,--no-whole-archive")
    set (CMAKE_CXX_COMPILER "arm-linux-gnueabihf-g++")
endif()

# this removes the -rdynamic link flag which bloats the executable.
string(REPLACE "-rdynamic" "" CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "${CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS}")

if(CMAKE_BUILD_TYPE MATCHES "RELEASE")
    # strip the executable for release build
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s")
endif()

include_directories("${VSM_SDK_DIR}/include")
include_directories("${VSM_SDK_DIR}/include/generated")

if (BEAGLEBONE)
    set (VSM_LIBRARY_DIR "${VSM_SDK_DIR}/beaglebone/lib")
else()
    set (VSM_LIBRARY_DIR "${VSM_SDK_DIR}/lib")
endif()

link_directories(${VSM_LIBRARY_DIR})

set (VSM_LIBRARY_FILE "${VSM_LIBRARY_DIR}/${CMAKE_STATIC_LIBRARY_PREFIX}vsm-sdk${CMAKE_STATIC_LIBRARY_SUFFIX}")

# Add DLL import libraries from SDK
if (CMAKE_SYSTEM_NAME MATCHES "Windows" AND ENABLE_DLL_IMPORT)
    set(DLL_IMPORT_LIB_NAMES ${DLL_IMPORT_LIB_NAMES} vsm_hid)
endif()

if (UGCS_PACKAGING_ENABLED)
    if (CMAKE_SYSTEM_NAME MATCHES "Linux")
        set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.15), libgcc1 (>= 1:4.1.1), libstdc++6 (>= 4.8.1)")
    elseif (CMAKE_SYSTEM_NAME MATCHES "Darwin")
        set(CMAKE_MACOSX_RPATH NO)
    endif()
endif()

# Function for building a VSM configuration file, which currently just copies
# it to build folder if destination file is absent or older that source file.
# ARGV0 - source file (${VSM_SDK_DIR}/share/vsm.conf by default)
# ARGV1 - destination file (${CMAKE_BINARY_DIR}/vsm.conf by default)
function(Build_vsm_config)
  # Default source file
  if (${ARGC} EQUAL 0)
    set(VSM_CONFIG_FILE_SOURCE ${VSM_SDK_DIR}/share/vsm.conf)
  else ()
    set(VSM_CONFIG_FILE_SOURCE ${ARGV0})
  endif ()
  
  # Default destination file
  if (${ARGC} LESS 2)
    set(VSM_CONFIG_FILE_DEST "${CMAKE_BINARY_DIR}/${VSM_EXECUTABLE_NAME}.conf")
  else ()
    set(VSM_CONFIG_FILE_DEST ${ARGV1})
  endif ()
  
  add_custom_target(VSM_CONFIG_FILE_TARGET ALL DEPENDS ${VSM_CONFIG_FILE_DEST}
  COMMENT "VSM config file: ${VSM_CONFIG_FILE_SOURCE}")

  configure_file ("${VSM_CONFIG_FILE_SOURCE}" "${VSM_CONFIG_FILE_DEST}")
  
  if (UGCS_PACKAGING_ENABLED)
    Install_conf_files("${VSM_CONFIG_FILE_DEST}")
    # Install_conf_files() updates this value. Need to propagate it to parent
    set (CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA ${CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA} PARENT_SCOPE)
  endif()
endfunction()

set(VSM_LIBS ${VSM_LIBRARY_FILE} ${VSM_PLAT_LIBS} ${DLL_IMPORT_LIB_NAMES})

# Function for adding standard install target for VSM and and corresponding log directory
function(Add_install_target)
    if (UGCS_PACKAGING_ENABLED)
        Patch_mac_binary("${VSM_EXECUTABLE_NAME}" "${VSM_EXECUTABLE_NAME}" "libstdc++.6.dylib" "libgcc_s.1.dylib")
        Install_vsm_launcher()
        # propagate variable to caller script.
        set (CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
                ${CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA}
                PARENT_SCOPE)
    endif()
    install(TARGETS ${VSM_EXECUTABLE_NAME} 
            RUNTIME DESTINATION "${UGCS_INSTALL_BIN_DIR}")
endfunction()

# Generate Android shared library form given sources.
# Script uses Application.mk.in and Android_app.mk.in templates.
# ANDROID_ABI - Semicolon separated list of target ABIs (Eg. "armeabi-v7a;x86" )
# ANDROID_PLATFORM - target platform, Eg. "android-21"
# SOURCES - Semicolon separated list of source files. Absolute paths required.
# INCLUDE_DIRS - Semicolon separated list of include directories. Absolute paths required.
function (Create_android_build ANDROID_ABI ANDROID_PLATFORM SOURCES INCLUDE_DIRS)
    set (ANDROID_APPLICATION_MK "${ANDROID_BINARY_DIR}/jni/Application.mk")
    set (ANDROID_ANDROID_MK "${ANDROID_BINARY_DIR}/jni/Android.mk")
    
    set(ANDROID_CFLAGS "")
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
    
    configure_file("${VSM_SDK_DIR}/android/Application.mk.in" ${ANDROID_APPLICATION_MK} @ONLY)
    configure_file("${VSM_SDK_DIR}/android/Android_app.mk.in" ${ANDROID_ANDROID_MK} @ONLY)

    add_custom_target(
        android
        COMMAND ${ANDROID_NDK}/ndk-build ${NDK_BUILD_PARAMS} -C ${ANDROID_BINARY_DIR} -j4
        DEPENDS ${ANDROID_ANDROID_MK} ${ANDROID_APPLICATION_MK} ${SOURCES})
    
    message ("Added target android with abi=${ANDROID_ABI}, platform=${ANDROID_PLATFORM}")
endfunction()

function(Build_vsm)
    if (ANDROID)
        get_directory_property(INCLUDE_DIRS INCLUDE_DIRECTORIES)
        Create_android_build("${ANDROID_ABI_LIST}" "${ANDROID_PLATFORM}" "${SOURCES}" "${INCLUDE_DIRS}")
        add_custom_target(${CMAKE_PROJECT_NAME} ALL)
        add_dependencies(${CMAKE_PROJECT_NAME} android)
    else()
        # Add the executable
        add_executable(${CMAKE_PROJECT_NAME} ${SOURCES})
        
        target_link_libraries(${CMAKE_PROJECT_NAME} ${VSM_LIBS})
        
        Add_install_target()
        
        #Add package target.
        include(CPack)
    endif()
endfunction()
