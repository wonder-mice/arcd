cmake_minimum_required(VERSION 3.2)

include(CMakeParseArguments)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -Wall -Wextra -Werror -pedantic-errors")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Wextra -Werror -pedantic-errors")

add_executable(codec_tests codec_tests.cpp)
target_link_libraries(codec_tests arcd)
add_test(NAME codec_tests COMMAND codec_tests)
