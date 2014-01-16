cmake_minimum_required(VERSION 2.6)

set(VSM_SDK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../..)
if (NOT DEFINED UTPP_DIR)
    set(UTPP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../unittest++)
endif()

include("${VSM_SDK_DIR}/make/ut.cmake")
