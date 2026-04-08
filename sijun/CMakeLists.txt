cmake_minimum_required(VERSION 4.2)
project(sijun C)

set(CMAKE_C_STANDARD 99)

enable_testing()

add_library(
    sijun_core
    executor.c
    parser.c
    parser_level2.c
    parser_level3.c
    parser_level4.c
    metadata.c
    repl.c
)
target_include_directories(sijun_core PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(sijun main.c)
target_link_libraries(sijun PRIVATE sijun_core)
target_include_directories(sijun PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(test_main_unit test_main_unit.c)
target_compile_definitions(test_main_unit PRIVATE SIJUN_TEST_BUILD=1)
target_link_libraries(test_main_unit PRIVATE sijun_core)
target_include_directories(test_main_unit PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(test_main_integration test_main_integration.c)
target_include_directories(test_main_integration PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_test(NAME test_main_unit COMMAND test_main_unit)
set_tests_properties(test_main_unit PROPERTIES LABELS "unit")

add_test(NAME test_main_integration COMMAND test_main_integration $<TARGET_FILE:sijun>)
set_tests_properties(test_main_integration PROPERTIES LABELS "integration")
