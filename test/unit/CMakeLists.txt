cmake_minimum_required(VERSION 2.6)

set(VSM_SDK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../..)
if (NOT DEFINED UTPP_DIR)
    set(UTPP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../unittestpp)
endif()

set(UGCS_PACKAGE_SUMMARY "Unittest package")

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/../../cmake")

add_subdirectory(../../third-party/protobuf/src ${CMAKE_BINARY_DIR}/protobuf)

set(PROTOBUF_UT_ENABLED 0)

include("ut")

add_test(NAME protobuf_test WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/protobuf
    COMMAND protobuf_test)
add_test(NAME protobuf_lite_test WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/protobuf
    COMMAND protobuf_lite_test)
add_test(NAME protobuf_lazy_descriptor_test WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/protobuf
    COMMAND protobuf_lazy_descriptor_test)
