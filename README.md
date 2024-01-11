# Simple HTTPS/TLS server on raspberry pi pico.


Goal here is to have a simple build environment and libraries to create your own TLS server.
A simple 'hello world' is provided as example.


### How to compile:

1. Create certificates or insert your own.
```
cd pico_tls/certificate
./create_cert.sh
```
This generates a self signed certificate in key.h/cert.h.
Alternative is to create a 'certificate' folder with same file format anywhere else in include path.


2. Configure wifi in example (otherwise it won't connect):
```
hello_world/config/wifi.h
```

3. Fetch submodules

```
git submodule update --init --recursive
```

4. Build - do one of the following based on preference

Option 1: Use a docker build image
```
./build_via_docker.sh
```

Option 2: build on a local ubuntu

Install pico dependencies
```
sudo apt install cmake gcc-arm-none-eabi libnewlib-arm-none-eabi xxd python3 g++ bash
```

Build:
```
./build.sh
```

### Logs

1. Simple one line to always connect when pico starts up:
```
while true; do minicom -b 115200 -o -D /dev/ttyACM0; done
```

2. To enable logs

2.1. If you want full verbose mbedtls logs with packets, add the following
```
hello_world/config/mbedtls_config.h
```
```
#define MBEDTLS_DEBUG_C
```

2.2. If you want pico-tls/altcp mbedtls logs
```
hello_world/config/lwipopts.h
```
```
#define LWIP_DEBUG 1
#define ALTCP_MBEDTLS_DEBUG  LWIP_DBG_ON
#define PICO_TLS_DEBUG LWIP_DBG_ON
```

2.3. If you want TCP stack logs

Change any of the LWIP_DBG_OFF to LWIP_DBG_ON that you are interested in file below.
Use a build that doesn't have NDEBUG defined.

```
hello_world/config/lwipopts_common.h
```