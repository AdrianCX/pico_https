/* MIT License

Copyright (c) 2024 Adrian Cruceru - https://github.com/AdrianCX/pico_https

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#pragma once

#if defined(__x86_64__) || defined(_M_X64)
#include "stdlib.h"
#include <cstdint>
typedef uint32_t ip_addr_t;
#else
#include "pico/stdlib.h"
#include "lwip/altcp_tcp.h"
#endif

class ISessionCallback
{
public:
    virtual ~ISessionCallback() {};
    
    virtual bool on_sent(uint16_t len) { return true; }
    virtual bool on_recv(uint8_t *data, size_t len) { return true; }
    virtual void on_closed() {};
    virtual void on_connected() {};
};

class ISessionSender
{
public:
    virtual ~ISessionSender() {};
    
    virtual int8_t connect(const char *host, uint16_t port) = 0;
    virtual int8_t connect(const char *host, const ip_addr_t *ipaddr, uint16_t port) = 0;
    virtual int8_t send(const uint8_t *data, size_t len) = 0;
    virtual int8_t flush() = 0;
    virtual int8_t close() = 0;
    virtual uint16_t send_buffer_size() = 0;
    virtual bool is_connected() = 0;
};
