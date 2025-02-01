add_library(pico_logger INTERFACE)
target_sources(pico_logger INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/pico_logger.c
  ${CMAKE_CURRENT_SOURCE_DIR}/pico_logger.h
  ${CMAKE_CURRENT_SOURCE_DIR}/pico_nologger.h
  ${CMAKE_CURRENT_SOURCE_DIR}/cm_backtrace/fault_handler/gcc/cmb_fault.S
  ${CMAKE_CURRENT_SOURCE_DIR}/cm_backtrace/cm_backtrace.c
)

target_link_libraries(pico_logger INTERFACE pico_lwip)

target_include_directories(pico_logger INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(pico_logger INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/cm_backtrace)
