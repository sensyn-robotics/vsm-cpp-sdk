cmake_minimum_required (VERSION 3.8.2)

project(sdk-unittest)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
add_definitions(-Wno-deprecated)

if (NOT DEFINED UTPP_DIR)
    set(UTPP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../unittestpp)
endif()

set(UGCS_PACKAGE_SUMMARY "Unittest package")

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/../../cmake")

include("ut")
