# Generic UT makefile
# Control variables:
# EXT_TOOL - external tool to be prepended to the UT binary (e.g. valgrind)
# EXT_LIB - external lib to link to every UT binary 

enable_testing()

include(ugcs/unittestpp)

file(GLOB TEST_SRCS ut_*.cpp)
file(STRINGS no_valgrind.txt NO_VALGRIND)
set(TESTS "")
foreach (TEST_SRC ${TEST_SRCS})
    string(REGEX REPLACE ".*/ut_(.+)\\.cpp" \\1 TEST_SRC ${TEST_SRC})
    list(APPEND TESTS ${TEST_SRC})
endforeach()

foreach (TEST ${TESTS})
    set (TEST_BIN ut_${TEST})
    add_executable(${TEST_BIN} ut_${TEST}.cpp main.cpp ${DLL_IMPORT_LIBS})
    target_link_libraries(${TEST_BIN} unittestpp ${EXT_LIB} ${VSM_PLAT_LIBS})
    add_test(${TEST} ${EXT_TOOL} ./${TEST_BIN})
    list(FIND NO_VALGRIND ${TEST} RES)
    if (RES GREATER -1)
        add_test(${TEST}_no_valgrind ./${TEST_BIN})
    endif()
    # run unit tests as post build step
    add_custom_command(TARGET ${TEST_BIN}
        POST_BUILD COMMAND ${TEST_BIN}
        COMMENT "Running unit tests")
endforeach()
