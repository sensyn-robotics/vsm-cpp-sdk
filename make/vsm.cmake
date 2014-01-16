# Build settings for VSM binaries

set(PLANTUML java -jar ${VSM_SDK_DIR}/share/tools/plantuml.jar -failonerror)

include("${VSM_SDK_DIR}/lib/make/common.cmake")

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
    set(VSM_CONFIG_FILE_DEST ${CMAKE_BINARY_DIR}/vsm.conf)
  else ()
    set(VSM_CONFIG_FILE_DEST ${ARGV1})
  endif ()
  
  add_custom_target(VSM_CONFIG_FILE_TARGET ALL DEPENDS ${VSM_CONFIG_FILE_DEST}
  COMMENT "VSM config file: ${VSM_CONFIG_FILE_SOURCE}")

  add_custom_command(OUTPUT ${VSM_CONFIG_FILE_DEST}
  COMMAND ${CMAKE_COMMAND} -E copy ${VSM_CONFIG_FILE_SOURCE} ${VSM_CONFIG_FILE_DEST}
  DEPENDS ${VSM_CONFIG_FILE_SOURCE})
  
  install(FILES ${VSM_CONFIG_FILE_DEST} DESTINATION ".")
endfunction()

set(VSM_LIBS vsm_sdk ${VSM_PLAT_LIBS} ${DLL_IMPORT_LIB_NAMES})

# Function for adding standard install target for VSM
function(Add_install_target)
  install(TARGETS ${CMAKE_PROJECT_NAME} RUNTIME DESTINATION ".")
endfunction()