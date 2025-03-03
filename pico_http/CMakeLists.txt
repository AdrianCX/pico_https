add_library(pico_http INTERFACE)
target_sources(pico_http INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/http_session.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/http_header.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/websocket_handler.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/mqtt_handler.cpp
  )

target_include_directories(pico_http INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
