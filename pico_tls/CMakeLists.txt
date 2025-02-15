add_library(pico_tls INTERFACE)
target_sources(pico_tls INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/mbedtls_wrapper.c
  ${CMAKE_CURRENT_SOURCE_DIR}/tls_listener.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/listener.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/session.cpp
  )

target_link_libraries(pico_tls INTERFACE pico_lwip_mbedtls pico_mbedtls pico_logger)

target_include_directories(pico_tls INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
