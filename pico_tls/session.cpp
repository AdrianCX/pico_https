#include "pico_tls_common.h"
#include "session.h"

#include <string.h>
#include <time.h>

#include "hardware/structs/rosc.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"
#include "lwip/altcp_tcp.h"
#include "lwip/altcp_tls.h"
#include "lwip/dns.h"

#if defined(MBEDTLS_DEBUG_C)
#include "mbedtls/debug.h"
#endif

#include "mbedtls/ssl_ticket.h"
#include "mbedtls_wrapper.h"

Session::Session(void *arg)
    : m_pcb((struct altcp_pcb *)arg)
    , m_closing(false)
{
    log("Session::Session: this=%p, pcb=%p\n", this, m_pcb);
    
    altcp_setprio(m_pcb, TCP_PRIO_MIN);

    altcp_arg(m_pcb, this);
    altcp_recv(m_pcb, http_recv);
    altcp_err(m_pcb, http_err);
    altcp_sent(m_pcb, http_sent);

    altcp_nagle_disable(m_pcb);
}

Session::~Session()
{
    log("Session::~Session: this:%p, m_pcb=%p, m_closing=%d\n", this, m_pcb, m_closing);
}

u16_t Session::send_buffer_size()
{
    return (m_pcb != NULL) ? altcp_sndbuf(m_pcb) : 0;
}

err_t Session::send(u8_t *data, size_t len)
{
    log("Session::send: this=%p, m_pcb=%p, data=%p, len=%d\n", this, m_pcb, data, len);
    if (data == NULL)
    {
        return ERR_VAL;
    }

    if (m_pcb == NULL)
    {
        return ERR_CLSD;
    }
    
    return altcp_write(m_pcb, data, len, TCP_WRITE_FLAG_COPY);
}

err_t Session::http_sent(void *arg, struct altcp_pcb *pcb, u16_t len)
{
    log("Session::http_sent: this=%p, m_pcb=%p, len=%d\n", arg, pcb, len);
    if (arg == NULL)
    {
        log("Session::http_sent: ERROR: Invalid http_sent with NULL arg\n");
        return ERR_VAL;
    }

    ((Session *)arg)->on_sent(len);
    return ERR_OK;
}


err_t Session::http_recv(void *arg, struct altcp_pcb *pcb, struct pbuf *p, err_t err)
{
    Session *self = (Session *)arg;
    log("Session::http_recv: this=%p, pcb=%p, m_pcb=%p, pbuf=%p, data=%p, len=%d, err=%s\n", arg, pcb, (self != NULL ? self->m_pcb : NULL), p, (p != NULL ? p->payload : NULL), (p != NULL ? p->len : 0), lwip_strerr(err));

    if (self == NULL)
    {
        log("Session::http_recv: ERROR: Invalid http_recv with NULL arg\n");

        // Try and fix up any pcb and memory, something is terribly bad if we reached this.
        if (p != NULL)
        {
            pbuf_free(p);
        }

        // Mark it as closed and return status for that.
        altcp_close(self->m_pcb);
        return ERR_CLSD;
    }
    
    // RX side is closed for the connection
    if (p == NULL)
    {
        log("Session::http_recv: connection is closed\n");
        self->close();
        return ERR_OK;
    }
    
    if (err != ERR_OK)
    {
        // Weirdly enough, errors on recv don't close the pcb. (we don't even get that use case for mbedtls)
        // So signal the error upstream and close the connection anyway.
        self->on_error(err, lwip_strerr(err));
        self->close();
        return ERR_OK;
    }

    // Confirm we've processed the data
    altcp_recved(pcb, p->tot_len);

    self->on_recv((u8_t *)p->payload, p->len);
    
    pbuf_free(p);
    
    return ERR_OK;
}



void Session::http_err(void *arg, err_t err)
{
    log("Session::http_err: this=%p, err=%s\n", arg, lwip_strerr(err));
    
    Session *self = (Session *)arg;
    if (self == NULL)
    {
        log("Session::http_err: ERROR: Invalid http_err with NULL arg\n");
        return;
    }
    
    self->m_pcb = NULL;
    self->on_error(err, lwip_strerr(err));
    self->on_closed();
}

void Session::close()
{
    log("Session::close: this=%p, m_pcb=%p, m_closing=%d\n", this, (void *)m_pcb, m_closing);

    if (m_closing)
    {
        return;
    }
    m_closing = true;

    if (m_pcb == NULL)
    {
        on_closed();
        return;
    }

    err_t err = altcp_close(m_pcb);
    if (err == ERR_OK)
    {
        m_pcb = NULL;
        on_closed();
        return;
    }

    log("Session::close: this=%p, altcp_close pcb=%p, error=%s, scheduling poll\n", this, (void *)m_pcb, lwip_strerr(err));
    altcp_poll(m_pcb, http_poll, 4);
}

err_t Session::http_poll(void *arg, struct altcp_pcb *pcb) {
    Session *self = (Session *)arg;
    log("Session::http_poll: this=%p, pcb=%p, m_pcb, m_closing: %d\n", self, (void *)pcb, (self != NULL ? self->m_pcb : NULL), (self != NULL ? self->m_closing : 0));
    
    if (self == NULL)
    {
        log("Session::http_poll: ERROR: Invalid http_poll with NULL arg\n");

        // This will just loop around and print error above on next poll/
        return ERR_OK;
    }

    if (self->m_pcb != pcb)
    {
        log("Session::http_poll: ERROR: invalid software assumption, this=%p, this->m_pcb(%p) != pcb argument(%p)\n", self, self->m_pcb, pcb);

        // This will just loop around and print error above on next poll/
        return ERR_OK;
    }
    
    if (self->m_closing)
    {
        err_t err = altcp_close(self->m_pcb);
        if (err == ERR_OK)
        {
            self->m_pcb = NULL;
            self->on_closed();

            // Close automatically removes poll callback and deletes pcb, so we won't get called back anymore
            return ERR_OK;
        }

        // We'll get called back at regular intervals until close succeeds
        log("Session::http_poll: this=%p, altcp_close pcb=%p, error=%s, rescheduling poll\n", self, (void *)self->m_pcb, lwip_strerr(err));
    }

    return ERR_OK;    
}
