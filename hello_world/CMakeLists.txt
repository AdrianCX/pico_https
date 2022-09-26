add_executable(hello_world_https
  main.cpp
  http_session.cpp
  )

# Stack size is excessively large as this is a simple demo, should be reduced
target_compile_definitions(hello_world_https PRIVATE
  PICO_STDIO_DEFAULT_CRLF=1
  PICO_STDIO_ENABLE_CRLF_SUPPORT=1
  PICO_STACK_SIZE=0x4000 # 16K
  PICO_STDIO_STACK_BUFFER_SIZE=1024
  )

target_include_directories(hello_world_https PRIVATE ${CMAKE_CURRENT_LIST_DIR})
      
target_link_libraries(hello_world_https
        pico_cyw43_arch_lwip_threadsafe_background
        pico_lwip_mbedtls
        pico_mbedtls
        pico_stdlib
        pico_tls
        )
pico_add_extra_outputs(hello_world_https)

pico_enable_stdio_usb(hello_world_https 1)
pico_enable_stdio_uart(hello_world_https 0)

target_include_directories(hello_world_https PUBLIC config)
