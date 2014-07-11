# Build settings for VSM binaries

set(PLANTUML java -jar ${VSM_SDK_DIR}/share/tools/plantuml.jar -failonerror)

set(VSM_EXECUTABLE_NAME ${CMAKE_PROJECT_NAME})

include("ugcs/common")

# this removes the -rdynamic link flag which bloats the executable.
string(REPLACE "-rdynamic" "" CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "${CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS}")

if(CMAKE_BUILD_TYPE MATCHES "RELEASE")
    # strip the executable for release build
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s")
endif()

include_directories("${VSM_SDK_DIR}/include")

link_directories(${VSM_SDK_DIR}/lib)

# Add DLL import libraries from SDK
if (CMAKE_SYSTEM_NAME MATCHES "Windows" AND ENABLE_DLL_IMPORT)
    set(DLL_IMPORT_LIB_NAMES ${DLL_IMPORT_LIB_NAMES} vsm_hid)
endif()

# Special handling for adding libstdc++ to installation.
# what we do here is reference the stdcpp library in our distribution to support
# older ubuntu releases (<13.10)
# Assume vsms will install under .../bin which is at the same level as .../lib.
if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(CMAKE_INSTALL_RPATH "\$ORIGIN/../lib")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "ugcs-stdcpp-runtime, libc6 (>= 2.15), libgcc1 (>= 1:4.1.1)")
elseif (CMAKE_SYSTEM_NAME MATCHES "Darwin")
    set(CMAKE_MACOSX_RPATH NO)
endif()

if (UGCS_PACKAGING_ENABLED)
    Install_common_log_file()
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

set(VSM_LIBS vsm-sdk ${VSM_PLAT_LIBS} ${DLL_IMPORT_LIB_NAMES})

# Function for adding standard install target for VSM and and corresponding log directory
function(Add_install_target)
    if (UGCS_PACKAGING_ENABLED)
        Patch_mac_binary("${VSM_EXECUTABLE_NAME}" "${VSM_EXECUTABLE_NAME}" "libstdc++.6.dylib" "libgcc_s.1.dylib")
        Install_vsm_as_upstart_job()
        # propagate variable to caller script.
        set (CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
                ${CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA}
                PARENT_SCOPE)
    endif()
    install(TARGETS ${VSM_EXECUTABLE_NAME} 
            RUNTIME DESTINATION "${UGCS_INSTALL_BIN_DIR}")
endfunction()
