add_library(analog_microphone INTERFACE)
target_sources(analog_microphone INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/analog_microphone_handler.cpp
)

target_link_libraries(analog_microphone INTERFACE pico_logger)

target_include_directories(analog_microphone INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
