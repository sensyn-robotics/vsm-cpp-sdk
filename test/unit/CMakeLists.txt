cmake_minimum_required(VERSION 2.6)

project(sdk-unittest)

if (NOT DEFINED UTPP_DIR)
    set(UTPP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../unittestpp)
endif()

set(UGCS_PACKAGE_SUMMARY "Unittest package")

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/../../cmake")

include("ut")
