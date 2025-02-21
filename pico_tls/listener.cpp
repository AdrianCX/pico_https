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
#include "pico_logger.h"
#include "listener.h"

#include "pico/cyw43_arch.h"

#include "lwip/pbuf.h"
#include "lwip/altcp_tcp.h"
#include "lwip/altcp_tls.h"

int Listener::NUM_LISTENERS = 0;

Listener::Listener()
    : m_session_factory(NULL)
    , m_bind_pcb(NULL)
    , m_listen_pcb(NULL)
{
    trace("Listener::Listener: this=%p\n", this);

    ++NUM_LISTENERS;
}

Listener::~Listener()
{
    trace("Listener::~Listener: this=%p\n", this);
    
    --NUM_LISTENERS;
}

int Listener::listen(u16_t port, session_factory_t factory)
{
    trace("Listener::listen: this=%p, port: %d, factory: %p\n", this, (int)port, factory);
    
    if (m_session_factory != NULL) {
        trace("TLS Listener is already initialized");
        return -1;
    }
    
    m_session_factory = factory;

    // Bind to the given port on any ip address

    m_bind_pcb = (struct altcp_pcb *)altcp_new_ip_type(&tcp_allocator, IPADDR_TYPE_ANY);
    
    altcp_setprio(m_bind_pcb, TCP_PRIO_MIN);
    err_t err = altcp_bind(m_bind_pcb, IP_ANY_TYPE, port);

    if (err != ERR_OK) {
        trace("Bind error: %s\r\n", lwip_strerr(err));
        return err;
    }

    m_listen_pcb = altcp_listen(m_bind_pcb);
    if (m_listen_pcb == NULL) {
        trace("Listen failed\r\n");
        return -1;
    }
    
    altcp_arg(m_listen_pcb, this);
    altcp_accept(m_listen_pcb, http_accept);

    trace("Listener::Listener: this=%p, port=%d, bind_pcb=%p, listen_pcb=%p\n", this, (int)port, m_bind_pcb, m_listen_pcb);
    return 0;
}

err_t Listener::http_accept(void *arg, struct altcp_pcb *pcb, err_t err)
{
    trace("Listener::http_accept: this=%p, pcb=%p, err=%s\n", arg, pcb, lwip_strerr(err));
    if ((err != ERR_OK) || (pcb == NULL)) {
        return ERR_VAL;
    }

    // Right now no shared pointers or session tracking, we might want to do that in the future in which case track returned objects
    ((Listener *)arg)->m_session_factory(pcb, false);
    return ERR_OK;
}
