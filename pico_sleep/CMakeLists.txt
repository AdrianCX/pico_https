add_library(pico_sleep INTERFACE)
target_sources(pico_sleep INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/pico_sleep.c
  ${CMAKE_CURRENT_SOURCE_DIR}/pico_sleep.h
)

target_include_directories(pico_logger INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
