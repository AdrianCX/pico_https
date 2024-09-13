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
#include "pico_nologger.h"
#include "session.h"

#include <string.h>
#include <time.h>

#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"
#include "lwip/altcp_tcp.h"
#include "lwip/altcp_tls.h"
#include "lwip/dns.h"

int Session::NUM_SESSIONS = 0;

Session::Session(void *arg)
    : m_pcb((struct altcp_pcb *)arg)
    , m_closing(false)
{
    trace("Session::Session: this=%p, pcb=%p\n", this, m_pcb);
    
    altcp_setprio(m_pcb, TCP_PRIO_MIN);

    altcp_arg(m_pcb, this);
    altcp_recv(m_pcb, http_recv);
    altcp_err(m_pcb, http_err);
    altcp_sent(m_pcb, http_sent);

    altcp_nagle_disable(m_pcb);

    ++NUM_SESSIONS;
}

Session::~Session()
{
    trace("Session::~Session: this:%p, m_pcb=%p, m_closing=%d\n", this, m_pcb, m_closing);

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
    
    return altcp_write(m_pcb, data, len, TCP_WRITE_FLAG_COPY);
}

err_t Session::flush()
{
    trace("Session::flush: this=%p, m_pcb=%p\n", this, m_pcb);
    if (m_pcb == NULL)
    {
        return ERR_OK;
    }
    
    return altcp_output(m_pcb);
}


err_t Session::http_sent(void *arg, struct altcp_pcb *pcb, u16_t len)
{
    trace("Session::http_sent: this=%p, m_pcb=%p, len=%d\n", arg, pcb, len);
    if (arg == NULL)
    {
        trace("Session::http_sent: ERROR: Invalid http_sent with NULL arg\n");
        return ERR_VAL;
    }

    ((Session *)arg)->on_sent(len);
    return ERR_OK;
}


err_t Session::http_recv(void *arg, struct altcp_pcb *pcb, struct pbuf *p, err_t err)
{
    Session *self = (Session *)arg;
    trace("Session::http_recv: this=%p, pcb=%p, m_pcb=%p, pbuf=%p, data=%p, len=%d, err=%s\n", arg, pcb, (self != NULL ? self->m_pcb : NULL), p, (p != NULL ? p->payload : NULL), (p != NULL ? p->len : 0), lwip_strerr(err));

    if (self == NULL)
    {
        trace("Session::http_recv: ERROR: Invalid http_recv with NULL arg\n");

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
        trace("Session::http_recv: connection is closed by remote party.\n");
        return self->close();
    }
    
    if (err != ERR_OK)
    {
        // Weirdly enough, errors on recv don't close the pcb. (we don't even get that use case for mbedtls)
        // So signal the error upstream and close the connection anyway.
        trace("Session::http_recv: err[%d] [%s]\n", err, lwip_strerr(err));
        self->on_error(err, lwip_strerr(err));
        return self->close();
    }

    // Confirm we've processed the data
    altcp_recved(pcb, p->tot_len);

    if (!self->on_recv((u8_t *)p->payload, p->len))
    {
        pbuf_free(p);
        return self->close();
    }

    pbuf_free(p);
    return ERR_OK;
}



void Session::http_err(void *arg, err_t err)
{
    trace("Session::http_err: this=%p, err=%s\n", arg, lwip_strerr(err));
    
    Session *self = (Session *)arg;
    if (self == NULL)
    {
        trace("Session::http_err: ERROR: Invalid http_err with NULL arg\n");
        return;
    }
    
    self->m_pcb = NULL;
    self->on_error(err, lwip_strerr(err));
    self->on_closed();
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
        on_closed();
        return ERR_OK;
    }

    altcp_arg(m_pcb, NULL);
    altcp_recv(m_pcb, NULL);
    altcp_err(m_pcb, NULL);
    altcp_poll(m_pcb, NULL, 0);
    altcp_sent(m_pcb, NULL);

    err_t err = altcp_close(m_pcb);
    if (err != ERR_OK) {
        altcp_abort(m_pcb);
        m_pcb = NULL;

        err = ERR_ABRT;
    }

    on_closed();
    return err;
}
