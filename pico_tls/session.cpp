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

#ifdef PICO_TLS_DEBUG
#include "pico_logger.h"
#else
#include "pico_nologger.h"
#endif

#include "session.h"

#include <string.h>
#include <time.h>

#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"
#include "lwip/altcp_tcp.h"
#include "lwip/altcp_tls.h"
#include "lwip/dns.h"

#if defined(MBEDTLS_DEBUG_C)
#include "mbedtls_wrapper.h"
#include "mbedtls/debug.h"
#endif

altcp_allocator_t tcp_allocator {altcp_tcp_alloc, nullptr};

static struct altcp_tls_config *TLS_CLIENT_CONFIG = NULL;

int Session::NUM_SESSIONS = 0;

Session::Session(void *arg, bool tls)
    : m_connected(arg != NULL)
    , m_closing(false)
    , m_sending(false)
    , m_tls(tls)
    , m_sentBytes(0)
    , m_port(0)
    , m_callback(NULL)
    , m_pcb((struct altcp_pcb *)arg)
{
    trace("Session::Session: this=%p, pcb=%p, tls=%d\n", this, m_pcb, m_tls);

    if (m_pcb)
    {
        init_pcb();
    }
    
    ++NUM_SESSIONS;
}

void Session::init_pcb()
{
    altcp_setprio(m_pcb, TCP_PRIO_MIN);

    altcp_arg(m_pcb, this);
    altcp_recv(m_pcb, lwip_recv);
    altcp_err(m_pcb, lwip_err);
    altcp_sent(m_pcb, lwip_sent);

    altcp_nagle_disable(m_pcb);
}

Session::~Session()
{
    trace("Session::~Session: this:%p, m_pcb=%p, m_closing=%d, m_tls=%d\n", this, m_pcb, m_closing, m_tls);

    if (m_pcb != NULL)
    {
        m_callback = NULL;    
        close();
    }
    
    --NUM_SESSIONS;
}

void Session::create_client_tls_config(const uint8_t *cert, size_t cert_len)
{
    if (TLS_CLIENT_CONFIG == NULL)
    {
        TLS_CLIENT_CONFIG = altcp_tls_create_config_client(cert, cert_len);

#if defined(MBEDTLS_DEBUG_C)
        // useful for debugging TLS problems, prints out whole packets and all logic in mbedtls if define is present.
        mbedtls_ssl_conf_dbg( (mbedtls_ssl_config *)TLS_CLIENT_CONFIG, mbedtls_debug_print, NULL );
        mbedtls_debug_set_threshold(5);
#endif
    }
}

u16_t Session::send_buffer_size()
{
    return (m_pcb != NULL) ? altcp_sndbuf(m_pcb) : 0;
}

err_t Session::send(const u8_t *data, size_t len)
{
    trace("Session::send: this=%p, m_pcb=%p, data=%p, len=%d\n", this, m_pcb, data, len);
    if (data == NULL)
    {
        return ERR_VAL;
    }

    if (m_pcb == NULL)
    {
        return ERR_OK;
    }

    // prevent partial write notifications back to caller
    m_sending = true;

    // if len exceeds MBEDTLS_SSL_OUT_CONTENT_LEN then it will quietly fail in "altcp_tls_mbedtls.c" / "altcp_mbedtls_write"
    while (len > 0)
    {
        int bytesToSend = (len > MBEDTLS_SSL_OUT_CONTENT_LEN) ? MBEDTLS_SSL_OUT_CONTENT_LEN : len;
        trace("Session::send: this=%p, m_pcb=%p, data=%p, len=%d bytesToSend=%d\n", this, m_pcb, data, len, bytesToSend);

        err_t err = altcp_write(m_pcb, data, bytesToSend, TCP_WRITE_FLAG_COPY);
        if (check_send_failure(err) != ERR_OK)
        {
            return err;
        }
        
        data += bytesToSend;
        len -= bytesToSend;
    }

    err_t err = flush();
    if (check_send_failure(err) != ERR_OK)
    {
        return err;
    }

    m_sending = false;
    
    if (m_sentBytes > 0)
    {
        return lwip_sent(this, m_pcb, 0);
    }

    return ERR_OK;
}

err_t Session::check_send_failure(err_t err)
{
    if (err != ERR_OK)
    {
        m_sending = false;
        return err;
    }
    
    if (m_closing || m_pcb == NULL)
    {
        m_sending = false;
        if (m_callback)
        {
            m_callback->on_closed();
        }
        return ERR_ABRT;
    }

    return ERR_OK;
}

err_t Session::flush()
{
    trace("Session::flush: this=%p, m_closing=%d, m_pcb=%p\n", this, m_closing, m_pcb);
    if (m_closing || m_pcb == NULL)
    {
        return ERR_OK;
    }
    
    return altcp_output(m_pcb);
}

err_t Session::lwip_sent(void *arg, struct altcp_pcb *pcb, u16_t len)
{
    trace("Session::lwip_sent: this=%p, m_pcb=%p, len=%d\n", arg, pcb, len);
    if (arg == NULL)
    {
        trace("Session::lwip_sent: ERROR: Invalid lwip_sent with NULL arg\n");
        return ERR_VAL;
    }

    Session *self = ((Session *)arg);
    
    if (self->m_sending)
    {
        self->m_sentBytes += len;
    }
    else
    {
        len += self->m_sentBytes;
        self->m_sentBytes = 0;
        
        if (self->m_callback)
        {
            if (!self->m_callback->on_sent(len))
            {
                return self->close();
            }
        }
    }
    return ERR_OK;
}

err_t Session::lwip_recv(void *arg, struct altcp_pcb *pcb, struct pbuf *p, err_t err)
{
    Session *self = (Session *)arg;
    trace("Session::lwip_recv: this=%p, pcb=%p, m_pcb=%p, pbuf=%p, data=%p, len=%d, err=%s\n", arg, pcb, (self != NULL ? self->m_pcb : NULL), p, (p != NULL ? p->payload : NULL), (p != NULL ? p->len : 0), lwip_strerr(err));

    if (err == ERR_ABRT)
    {
        self->m_pcb = NULL;
    }
    
    // RX side is closed for the connection
    if (p == NULL)
    {
        trace("Session::lwip_recv: connection is closed by remote party.\n");
        return self->close();
    }
    
    if (err != ERR_OK)
    {
        trace("Session::lwip_recv: err[%d] [%s]\n", err, lwip_strerr(err));
        self->close();
        return ERR_ABRT;
    }

    if (self->m_callback)
    {
        if (!self->m_callback->on_recv((u8_t *)p->payload, p->len))
        {
            altcp_recved(pcb, p->tot_len);
            self->close();
            return ERR_ABRT;
        }
    }

    altcp_recved(pcb, p->tot_len);
    pbuf_free(p);
    return ERR_OK;
}

void Session::lwip_err(void *arg, err_t err)
{
    trace("Session::lwip_err: this=%p, err=%d, err_str=%s\n", arg, err, lwip_strerr(err));

    Session *self = (Session *)arg;
    if (self == NULL)
    {
        trace("Session::lwip_err: ERROR: Invalid lwip_err with NULL arg\n");
        return;
    }

    altcp_arg(self->m_pcb, NULL);
    altcp_recv(self->m_pcb, NULL);
    altcp_err(self->m_pcb, NULL);
    altcp_sent(self->m_pcb, NULL);
    self->m_pcb = NULL;
    
    self->close();
}

err_t Session::close()
{
    trace("Session::close: this=%p, m_pcb=%p, m_closing=%d\n", this, (void *)m_pcb, m_closing);
    
    if (m_closing)
    {
        return ERR_OK;
    }

    m_connected = false;
    m_closing = true;

    if (m_pcb == NULL)
    {
        if (!m_sending && m_callback)
        {
            m_callback->on_closed();
        }
        return ERR_OK;
    }

    altcp_arg(m_pcb, NULL);
    altcp_recv(m_pcb, NULL);
    altcp_err(m_pcb, NULL);
    altcp_sent(m_pcb, NULL);

    altcp_abort(m_pcb);
    
    trace("Session::close: this=%p, m_pcb=%p\n", this, (void *)m_pcb);
    m_pcb = NULL;

    if (!m_sending && m_callback)
    {
        m_callback->on_closed();
    }
    return ERR_ABRT;
}


err_t Session::connect(const char *host, u16_t port)
{
    trace("Session::connect: this=%p host=%s, port=%d\n", this, safestr(host), port);

    m_port = port;
    
    ip_addr_t target_ip;
    err_t err = dns_gethostbyname(host, &target_ip, dns_callback, this);

    if (err == ERR_OK)
    {
        return connect(host, &target_ip, port);
    }
    else if (err != ERR_INPROGRESS)
    {
        trace("Session::connect: this=%p host=%s, port=%d, err=%d\n", this, safestr(host), port, err);
    }

    return err;
}

void Session::dns_callback(const char* host, const ip_addr_t *ipaddr, void *arg)
{
    trace("Session::dns_callback: this=%p host=%s, ip=%p\n", arg, safestr(host), ipaddr);
    
    Session *self = (Session *)arg;
    
    if (!ipaddr)
    {
        if (self->m_callback)
        {
            self->m_callback->on_closed();
        }
        return;
    }

    self->connect(host, ipaddr, self->m_port);
}

err_t Session::connect(const char *host, const ip_addr_t *ipaddr, u16_t port)
{
    trace("Session::connect: this=%p host=%s, ip=%p, port=%d\n", this, safestr(host), ipaddr ? ipaddr->addr : 0, port);

    if (!m_tls)
    {
        m_pcb = (struct altcp_pcb *)altcp_new_ip_type(&tcp_allocator, IPADDR_TYPE_ANY);
    }
    else
    {
        if (TLS_CLIENT_CONFIG == NULL)
        {
            trace("Session::connect: this=%p host=%s, ip=%p, port=%d, tls client config was not created.\n", this, safestr(host), ipaddr ? ipaddr->addr : 0, port);
            return ERR_VAL;
        }
        
        m_pcb = (struct altcp_pcb *)altcp_tls_new(TLS_CLIENT_CONFIG, IPADDR_TYPE_ANY);

        if (host != NULL)
        {
            mbedtls_ssl_set_hostname((mbedtls_ssl_context *)altcp_tls_context(m_pcb), host);
        }
    }

    init_pcb();
    
    err_t err =  altcp_connect(m_pcb, ipaddr, port, lwip_connected);

    if (err != ERR_OK)
    {
        trace("Session::connect: this=%p host=%s, ip=%p, port=%d, err=%d, err_str=%s\n", this, safestr(host), ipaddr ? ipaddr->addr : 0, port, err, lwip_strerr(err));
        return close();
    }

    return ERR_OK;
}

err_t Session::lwip_connected(void *arg, struct altcp_pcb *pcb, err_t err)
{
    trace("Session::lwip_connected: this=%p, m_pcb=%p, ipaddr=%x, err=%d\n", arg, pcb, err);
    
    Session *self = (Session *)arg;
    if (err != ERR_OK)
    {
        return self->close();
    }

    self->m_connected = true;

    if (self->m_callback)
    {
        self->m_callback->on_connected();
    }
    
    return ERR_OK;
}

bool Session::is_connected()
{
    return m_pcb != NULL && !m_closing && m_connected;
}
