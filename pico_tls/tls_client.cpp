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

#include "tls_client.h"

#include "pico/cyw43_arch.h"

#include "lwip/pbuf.h"
#include "lwip/altcp_tcp.h"
#include "lwip/altcp_tls.h"
#include "lwip/dns.h"

int TLSClient::NUM_CLIENTS = 0;

// Can't affort to have more then one tls config considering current memory.
static struct altcp_tls_config *TLS_CONFIG = NULL;

TLSClient::TLSClient()
{
    trace("TLSClient::TLSClient: this=%p\n", this);
    ++NUM_CLIENTS;
}

TLSClient::~TLSClient()
{
    trace("TLSClient::~TLSClient: this=%p\n", this);
    
    --NUM_CLIENTS;
}

void TLSClient::create_tls_config(const uint8_t *cert, size_t cert_len)
{
    if (TLS_CONFIG == NULL)
    {
        TLS_CONFIG = altcp_tls_create_config_client(cert, cert_len);

#if defined(MBEDTLS_DEBUG_C)
        // useful for debugging TLS problems, prints out whole packets and all logic in mbedtls if define is present.
        mbedtls_ssl_conf_dbg( (mbedtls_ssl_config *)TLS_CONFIG, mbedtls_debug_print, NULL );
        mbedtls_debug_set_threshold(5);
#endif
    }
}

bool TLSClient::connect(const char *host, u16_t port)
{
    trace("TLSClient::connect: this=%p host=%s, port=%d\n", this, safestr(host), port);
    m_connected = false;

    m_session.reset();

    m_port = port;
    
    ip_addr_t target_ip;
    err_t err = dns_gethostbyname(host, &target_ip, dns_callback, this);

    if (err == ERR_OK)
    {
        return connect(host, &target_ip, port);
    }
    else if (err != ERR_INPROGRESS)
    {
        trace("TLSClient::connect: this=%p host=%s, port=%d, err=%d\n", this, safestr(host), port, err);
        return false;
    }

    return true;
}

void TLSClient::dns_callback(const char* host, const ip_addr_t *ipaddr, void *arg)
{
    trace("TLSClient::dns_callback: this=%p host=%s, ip=%p\n", arg, safestr(host), ipaddr);
    
    TLSClient *self = (TLSClient *)arg;
    
    if (!ipaddr)
    {
        self->on_closed();
        return;
    }

    self->connect(host, ipaddr, self->m_port);
}

bool TLSClient::connect(const char *host, const ip_addr_t *ipaddr, u16_t port)
{
    if (TLS_CONFIG == NULL)
    {
        trace("TLSClient::connect: this=%p host=%s, ip=%p, port=%d, Error=TLS CA cert was not configured.\n", this, safestr(host), ipaddr, port, TLS_CONFIG);
        return false;
    }
    m_connected = false;
    m_session.reset();

    trace("TLSClient::connect: this=%p host=%s, ip=%p, port=%d, TLS_CONFIG=%p\n", this, safestr(host), ipaddr, port, TLS_CONFIG);
    
    struct altcp_pcb *pcb = (struct altcp_pcb *)altcp_tls_new(TLS_CONFIG, IPADDR_TYPE_ANY);

    mbedtls_ssl_set_hostname((mbedtls_ssl_context *)altcp_tls_context(pcb), host);
    
    altcp_setprio(pcb, TCP_PRIO_MIN);
    
    m_session = Session::create(pcb);
    m_session->set_callback(this);
    
    err_t err = m_session->connect(ipaddr, port);

    if (err != ERR_OK)
    {
        trace("TLSClient::connect: this=%p host=%s, ip=%p, port=%d, err=%d, err_str=%s\n", this, safestr(host), ipaddr, port, err, lwip_strerr(err));
        m_session->close();
        return false;
    }

    return true;
}

void TLSClient::on_sent(u16_t len)
{
    trace("TLSClient::on_sent: this=%p len=%d", this, len);
}

void TLSClient::on_recv(u8_t *data, size_t len)
{
    trace("TLSClient::on_recv: this=%p, len=%d, data=%p", this, len, data);
}

void TLSClient::on_closed()
{
    trace("TLSClient::on_closed: this=%p", this);
    m_session.reset();
    m_connected = false;
}

void TLSClient::on_connected()
{
    trace("TLSClient::on_connected: this=%p", this);

    m_connected = true;
}

err_t TLSClient::send(const u8_t *data, size_t len)
{
    trace("TLSClient::send: this=%p, m_session=%p, len=%d, data=[%.*s]", this, m_session.get(), len, len, data);

    if (m_session)
    {
        return m_session->send(data, len);
    }

    return ERR_CLSD;
}

err_t TLSClient::close()
{
    trace("TLSClient::close: this=%p, m_session=%p", this, m_session.get());

    if (m_session)
    {
        return m_session->close();
    }

    return ERR_CLSD;
}
