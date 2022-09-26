#ifndef MBEDTLS_TLS_LISTENER_H
#define MBEDTLS_TLS_LISTENER_H

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "session.h"

struct altcp_tls_config;

typedef Session *(session_factory_t)(void *arg);

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