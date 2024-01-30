add_library(pico_logger INTERFACE)
target_sources(pico_logger INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/pico_logger.c
  ${CMAKE_CURRENT_SOURCE_DIR}/pico_logger.h
  ${CMAKE_CURRENT_SOURCE_DIR}/m0FaultDispatch/m0FaultDispatch.c
)

target_link_libraries(pico_logger INTERFACE pico_lwip)

target_include_directories(pico_logger INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
