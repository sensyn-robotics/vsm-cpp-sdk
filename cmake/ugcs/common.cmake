# Common things for sdk and all projects depending on this sdk.
# (vsms, unittests, ...)

# Enable C++11 standard, extended warnings, multithreading.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fmessage-length=0 -std=c++11 -Werror -Wall -Wextra -Wold-style-cast -pthread")

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

# Debug build options
if(NOT CMAKE_BUILD_TYPE MATCHES "RELEASE")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -gdwarf-3 -fno-omit-frame-pointer")
    add_definitions(-DDEBUG)
endif()

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(VSM_PLAT_LIBS rt)
elseif (CMAKE_SYSTEM_NAME MATCHES "Windows")
    set(VSM_PLAT_LIBS ws2_32 Userenv bfd iberty dbghelp z)
endif()

# Enable packaging script automatically if vsm is compiled from ugcs source tree.
if (NOT DEFINED UGCS_PACKAGING_ENABLED)
    # Search for UgCS specific script for creating debian packages.
    find_file(PACKAGING_SCRIPT
        "configure_packaging.cmake"
        PATHS "../../../build-scripts/cmake"
        "../..build-scripts/cmake" 
        "../build-scripts/cmake"
        NO_DEFAULT_PATH)
    
    if (PACKAGING_SCRIPT)
        get_filename_component(PACKAGING_SCRIPT_PATH "${PACKAGING_SCRIPT}" PATH)
        set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PACKAGING_SCRIPT_PATH}")
        include("configure_packaging")
        set(UGCS_PACKAGING_ENABLED YES)
    else()
        set(UGCS_PACKAGING_ENABLED NO)
    endif()
endif()

if (NOT UGCS_PACKAGING_ENABLED)
    # Set up reasonable defaults for install and package targets.
    # Users wanting to maintain their own packages should change these accordingly.
    if (UNIX)
        set(UGCS_INSTALL_DIR "opt/${CMAKE_PROJECT_NAME}")
        set(UGCS_INSTALL_BIN_DIR "${UGCS_INSTALL_DIR}/bin")
        set(UGCS_INSTALL_LIB_DIR "${UGCS_INSTALL_DIR}/lib")
        set(UGCS_INSTALL_CFG_DIR "etc/${UGCS_INSTALL_DIR}")
        set(UGCS_INSTALL_VAR_DIR "var/${UGCS_INSTALL_DIR}")
        set(UGCS_INSTALL_LOG_DIR "var/${UGCS_INSTALL_DIR}/log")
    elseif(WIN32)
        set(UGCS_INSTALL_DIR "${CMAKE_PROJECT_NAME}")
        set(UGCS_INSTALL_BIN_DIR "${UGCS_INSTALL_DIR}/bin")
        set(UGCS_INSTALL_CFG_DIR "${UGCS_INSTALL_DIR}/config")
        set(UGCS_INSTALL_VAR_DIR "${UGCS_INSTALL_DIR}/var")
        set(UGCS_INSTALL_LOG_DIR "${UGCS_INSTALL_DIR}/log")
        # lib dir must be the same as bin dir on windows so that our dlls
        # are found correctly.
        set(UGCS_INSTALL_LIB_DIR "${UGCS_INSTALL_BIN_DIR}")
    endif()
    set (UGCS_INSTALLED_LOG_FILE_PATH "${CMAKE_INSTALL_PREFIX}/${UGCS_INSTALL_LOG_DIR}/${CMAKE_PROJECT_NAME}.log")
    set (UGCS_INSTALLED_LOG_DIR "${CMAKE_INSTALL_PREFIX}/${UGCS_INSTALL_LOG_DIR}")
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
    
    add_custom_target(pdf env UGCS_PACKAGE_VERSION=${UGCS_PACKAGE_VERSION}
                      UGCS_VSM_USER_GUIDE_LATEX_TITLE=${UGCS_VSM_USER_GUIDE_LATEX_TITLE}
                      make -C doc-${DOC_CONFIG_NAME}/latex pdf
                      DEPENDS doc)
endif()

if (PDF_DOC_NAME_OVERRIDE)
  set(PDF_DOC_NAME ${PDF_DOC_NAME_OVERRIDE})
else()
  set(PDF_DOC_NAME manual-${CMAKE_PROJECT_NAME}.pdf)
endif()

add_custom_command(TARGET pdf POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy ${DOC_DIR}/latex/refman.pdf ${PDF_DOC_NAME})
