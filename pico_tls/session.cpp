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

altcp_allocator_t tcp_allocator {altcp_tcp_alloc, nullptr};

int Session::NUM_SESSIONS = 0;

Session::Session(void *arg)
    : m_pcb((struct altcp_pcb *)arg)
    , m_callback(NULL)
    , m_closing(false)
    , m_sending(false)
    , m_sentBytes(0)
{
    trace("Session::Session: this=%p, pcb=%p\n", this, m_pcb);
    
    altcp_setprio(m_pcb, TCP_PRIO_MIN);

    altcp_arg(m_pcb, this);
    altcp_recv(m_pcb, lwip_recv);
    altcp_err(m_pcb, lwip_err);
    altcp_sent(m_pcb, lwip_sent);

    altcp_nagle_disable(m_pcb);

    ++NUM_SESSIONS;
}

Session::~Session()
{
    trace("Session::~Session: this:%p, m_pcb=%p, m_closing=%d\n", this, m_pcb, m_closing);

    if (!m_closing)
    {
        close();
    }
    
    --NUM_SESSIONS;
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

    // We might get failures that can delete this class while processing, guard against that and handle errors inline.
    auto guard = shared_from_this();
    
    // prevent partial write notifications back to caller
    m_sending = true;

    // if len exceeds MBEDTLS_SSL_OUT_CONTENT_LEN then it will quietly fail in "altcp_tls_mbedtls.c" / "altcp_mbedtls_write"
    while (len > 0)
    {
        int bytesToSend = (len > MBEDTLS_SSL_OUT_CONTENT_LEN) ? MBEDTLS_SSL_OUT_CONTENT_LEN : len;
        trace("Session::send: this=%p, m_pcb=%p, data=%p, len=%d bytesToSend=%d\n", this, m_pcb, data, len, bytesToSend);

        err_t err = altcp_write(m_pcb, data, bytesToSend, TCP_WRITE_FLAG_COPY);

        if (err != ERR_OK)
        {
            m_sending = false;
            return err;
        }

        if (m_closing || m_pcb == NULL)
        {
            m_sending = false;
            return ERR_ABRT;
        }
        
        data += bytesToSend;
        len -= bytesToSend;
    }

    m_sending = false;
    
    if (m_sentBytes > 0)
    {
        lwip_sent(this, m_pcb, 0);
    }

    return flush();
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
            self->m_callback->on_sent(len);
        }
    }
    return ERR_OK;
}

err_t Session::lwip_recv(void *arg, struct altcp_pcb *pcb, struct pbuf *p, err_t err)
{
    Session *self = (Session *)arg;
    trace("Session::lwip_recv: this=%p, pcb=%p, m_pcb=%p, pbuf=%p, data=%p, len=%d, err=%s\n", arg, pcb, (self != NULL ? self->m_pcb : NULL), p, (p != NULL ? p->payload : NULL), (p != NULL ? p->len : 0), lwip_strerr(err));

    if (self == NULL)
    {
        trace("Session::lwip_recv: ERROR: Invalid lwip_recv with NULL arg\n");

        // Try and fix up any pcb and memory, something is terribly bad if we reached this.
        if (p != NULL)
        {
            // Confirm we've processed the data
            altcp_recved(pcb, p->tot_len);
            pbuf_free(p);
        }

        // Mark it as closed and return status for that.
        altcp_close(self->m_pcb);
        return ERR_OK;
    }
    
    // RX side is closed for the connection
    if (p == NULL)
    {
        trace("Session::lwip_recv: connection is closed by remote party.\n");
        return self->close();
    }
    
    if (err != ERR_OK)
    {
        // Weirdly enough, errors on recv don't close the pcb. (we don't even get that use case for mbedtls)
        // So signal the error upstream and close the connection anyway.
        trace("Session::lwip_recv: err[%d] [%s]\n", err, lwip_strerr(err));
        return self->close();
    }

    // Confirm we've processed the data
    altcp_recved(pcb, p->tot_len);

    if (self->m_callback)
    {
        self->m_callback->on_recv((u8_t *)p->payload, p->len);
    }

    pbuf_free(p);
    return ERR_OK;
}

void Session::lwip_err(void *arg, err_t err)
{
    trace("Session::lwip_err: this=%p, err=%s\n", arg, lwip_strerr(err));
    
    Session *self = (Session *)arg;
    if (self == NULL)
    {
        trace("Session::lwip_err: ERROR: Invalid lwip_err with NULL arg\n");
        return;
    }

    // from lwip tcp_err:
    // * @note The corresponding pcb is already freed when this callback is called!
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
    
    m_closing = true;

    if (m_pcb == NULL)
    {
        if (m_callback)
        {
            m_callback->on_closed();
        }
        return ERR_OK;
    }

    altcp_arg(m_pcb, NULL);
    altcp_recv(m_pcb, NULL);
    altcp_err(m_pcb, NULL);
    altcp_poll(m_pcb, NULL, 0);
    altcp_sent(m_pcb, NULL);

    err_t err = altcp_close(m_pcb);
    if (err != ERR_OK && m_pcb != NULL) {
        altcp_abort(m_pcb);
        m_pcb = NULL;

        err = ERR_ABRT;
    }

    if (m_callback)
    {
        m_callback->on_closed();
    }
    return err;
}

err_t Session::connect(const ip_addr_t *ipaddr, u16_t port)
{
    trace("Session::connect: this=%p, m_pcb=%p, ipaddr=%x, port=%d\n", this, m_pcb, *ipaddr, port);
    
    return altcp_connect(m_pcb, ipaddr, port, lwip_connected);
}

err_t Session::lwip_connected(void *arg, struct altcp_pcb *pcb, err_t err)
{
    trace("Session::lwip_connected: this=%p, m_pcb=%p, ipaddr=%x, err=%d\n", arg, pcb, err);
    
    Session *self = (Session *)arg;
    if (err != ERR_OK)
    {
        return self->close();
    }

    if (self->m_callback)
    {
        self->m_callback->on_connected();
    }
    
    return ERR_OK;
}
