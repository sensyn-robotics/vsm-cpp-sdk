# Select correct makefile generator and directories
# These directories should be put in SCM ignore list.
if (UNIX)
    set (GENERATOR_NAME "Unix Makefiles")
    set(DEBUG_DIR "build-debug-linux")
    set(RELEASE_DIR "build-release-linux")
elseif(WIN32)
    set (GENERATOR_NAME "MinGW Makefiles")
    set(DEBUG_DIR "build-debug-win")
    set(RELEASE_DIR "build-release-win")
elseif(APPLE)
    set (GENERATOR_NAME "TODO...")
endif(UNIX)

# Remove existing build directories
file(REMOVE_RECURSE ${DEBUG_DIR})
file(REMOVE_RECURSE ${RELEASE_DIR})

# Create new build directories
file(MAKE_DIRECTORY ${DEBUG_DIR})
file(MAKE_DIRECTORY ${RELEASE_DIR})

set(VARS "")

if (DEFINED VSM_BUILD_DOC)
    set(VARS ${VARS} -DVSM_BUILD_DOC:BOOL=${VSM_BUILD_DOC})
endif()

if (DEFINED VSM_SDK_DIR)
    set(VARS ${VARS} -DVSM_SDK_DIR=${VSM_SDK_DIR})
endif()

if (DEFINED UTPP_DIR)
    set(VARS ${VARS} -DUTPP_DIR=${UTPP_DIR})
endif()

if (DEFINED CMAKE_INSTALL_PREFIX)
    set(VARS ${VARS} -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX})
endif()

# generate makefiles
message("Generating Debug ${GENERATOR_NAME} ...")
execute_process(COMMAND ${CMAKE_COMMAND} -E chdir ${DEBUG_DIR}/ ${CMAKE_COMMAND} -G "${GENERATOR_NAME}" ../ ${VARS} -DCMAKE_BUILD_TYPE:STRING=DEBUG)
message("Generating Release ${GENERATOR_NAME} ...")
execute_process(COMMAND ${CMAKE_COMMAND} -E chdir ${RELEASE_DIR}/ ${CMAKE_COMMAND} -G "${GENERATOR_NAME}" ../ ${VARS} -DCMAKE_BUILD_TYPE:STRING=RELEASE)
