cmake_minimum_required(VERSION 4.2)
project(sijun C)

set(CMAKE_C_STANDARD 99)

enable_testing()

add_executable(sijun main.c)

add_executable(test_main test_main.c)
add_test(NAME test_main COMMAND test_main $<TARGET_FILE:sijun>)
