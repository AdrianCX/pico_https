cmake_minimum_required(VERSION 3.12)

project(pico_unittests C CXX ASM)

include(FetchContent)

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)

FetchContent_MakeAvailable(googletest)

enable_testing()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)

add_executable(
  pico_http_test
  pico_http_test.cpp
  pico_websocket_test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../pico_http/http_header.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../pico_http/websocket_handler.cpp
)

set(CMAKE_CXX_FLAGS  "-g")

target_link_libraries(
  pico_http_test
  GTest::gtest_main
  GTest::gmock_main
)

include(GoogleTest)
gtest_discover_tests(pico_http_test)
