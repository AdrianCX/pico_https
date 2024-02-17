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
#ifndef MBEDTLS_TLS_SESSION_H
#define MBEDTLS_TLS_SESSION_H

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

// Can be either TLS or regular TCP, abstracted away by altcp.
class Session {
public:
    Session(void *arg);
    //
    // Send the requested parameters, get called back as bytes are sent in on_sent.
    //
    // @returns - ERR_MEM if the length of the data exceeds the current send buffer size or if the length of the queue of outgoing segment is larger than the upper limit defined in lwipopts.h.
    //            can call send_buffer_size() to obtain max buffer size available.
    //          - ERR_CLSD if trying to send data on already closed connection.
    //          - ERR_VAL if data is NULL
    //
    err_t send(const u8_t *data, size_t len);
    virtual void on_sent(u16_t len) {}

    err_t flush();
    u16_t send_buffer_size();

    //
    // Called when data is received.
    //
    virtual bool on_recv(u8_t *data, size_t len) { return true; }

    //
    // Call when you want to close the connection, wait until on_closed is called to confirm/free memory.
    //
    err_t close();
    virtual void on_closed() {}

    //
    // Called when error ocurred. Connection is closed at this point. Session object still exists until user decides to delete.
    //
    virtual void on_error(err_t err, const char *err_str) {}


protected:
    virtual ~Session();
    
private:
    static err_t http_recv(void *arg, struct altcp_pcb *pcb, struct pbuf *p, err_t err);
    static err_t http_sent(void *arg, struct altcp_pcb *pcb, u16_t len);
    static err_t http_close_or_abort_conn(struct altcp_pcb *pcb, Session *hs, u8_t abort_conn);
    static err_t http_close_conn(struct altcp_pcb *pcb, Session *hs);
    static void http_err(void *arg, err_t err);

    struct altcp_pcb *m_pcb;
    bool m_closing;
    
    static err_t http_poll(void *arg, struct altcp_pcb *pcb);
};

#endif