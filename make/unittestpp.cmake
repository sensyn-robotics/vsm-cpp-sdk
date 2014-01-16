# Makefile for Unittest++ library building.
# UTPP_DIR should point to Unittest++ source folder.

# Get the main sources
file(GLOB UTPP_SRCS ${UTPP_DIR}/src/*.cpp ${UTPP_DIR}/src/*.h)

IF(CMAKE_COMPILER_IS_GNUCXX)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
ENDIF(CMAKE_COMPILER_IS_GNUCXX)

# Get platform specific sources
if (WIN32)
    set(UTPP_PLAT_DIR Win32)
else()
    set(UTPP_PLAT_DIR Posix)
endif(WIN32)

file(GLOB UTPP_PLAT_SRCS ${UTPP_DIR}/src/${UTPP_PLAT_DIR}/*.cpp ${UTPP_DIR}/src/${UTPP_PLAT_DIR}/*.h)

include_directories(${UTPP_DIR}/src)

# Create the library
add_library(unittestpp STATIC ${UTPP_SRCS} ${UTPP_PLAT_SRCS})
set_target_properties(unittestpp PROPERTIES OUTPUT_NAME unittest++)
