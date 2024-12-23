add_library(pico_arducam_mega INTERFACE)

target_sources(pico_arducam_mega INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/ArducamCamera.c
  ${CMAKE_CURRENT_SOURCE_DIR}/cam_spi_master.c
  )

target_include_directories(pico_arducam_mega INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
