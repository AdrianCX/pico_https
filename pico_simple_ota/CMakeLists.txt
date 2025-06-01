add_library(pico_simple_ota INTERFACE)
target_sources(pico_simple_ota INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/upgrade_helper.cpp
)

target_link_libraries(pico_simple_ota INTERFACE pico_tls pico_logger)

target_include_directories(pico_simple_ota INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
