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
#ifndef MBEDTLS_TLS_LISTENER_H
#define MBEDTLS_TLS_LISTENER_H

#include "pico/cyw43_arch.h"

#include "session.h"

struct altcp_tls_config;

class TLSListener {
public:
    TLSListener();

    // 
    // Start up a TLS Listener that calls 'factory' on each new accepted client.
    // Certificate is provided via 'certificate/key.h' and 'certificate/cert.h' in include path.
    //
    int listen(u16_t port, session_factory_t *factory);
    
private:
    // Cleanup not implemented yet for listener, expecting it to live for the whole runtime of the pico
    ~TLSListener();

    static err_t http_accept(void *arg, struct altcp_pcb *pcb, err_t err);

    session_factory_t           *m_session_factory;

    altcp_tls_config           *m_conf;
    altcp_pcb                  *m_bind_pcb;
    altcp_pcb                  *m_listen_pcb;
    
    static const char *m_alpn_strings[];
};

#endif