# Websocket/HTTPS/TLS environment for raspberry pi pico.

This repo provides libraries to set up HTTP(S) servers on pico.
The second part is in repo: https://github.com/AdrianCX/pico_https_example

Goal here is to have a simple build environment and libraries to create your own TLS server.

Libraries here include:
- TLS/TCP LWIP wrappers for both client/server
- basic HTTP/websocket/mqtt parsing with minimal ram footprint.
- Logging over UDP with callstack on crash/hang reported
