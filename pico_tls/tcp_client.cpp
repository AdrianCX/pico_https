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

#include "tcp_client.h"

#include "pico/cyw43_arch.h"

#include "lwip/pbuf.h"
#include "lwip/altcp_tcp.h"
#include "lwip/altcp_tls.h"
#include "lwip/dns.h"

int TCPClient::NUM_CLIENTS = 0;

TCPClient::TCPClient()
{
    trace("TCPClient::TCPClient: this=%p\n", this);
    ++NUM_CLIENTS;
}

TCPClient::~TCPClient()
{
    trace("TCPClient::~TCPClient: this=%p\n", this);
    
    --NUM_CLIENTS;
}

bool TCPClient::connect(const char *host, u16_t port)
{
    trace("TCPClient::connect: this=%p host=%s, port=%d\n", this, safestr(host), port);

    m_port = port;
    
    ip_addr_t target_ip;
    err_t err = dns_gethostbyname(host, &target_ip, dns_callback, this);

    if (err == ERR_OK)
    {
        return connect(host, &target_ip, port);
    }
    else if (err != ERR_INPROGRESS)
    {
        trace("TCPClient::connect: this=%p host=%s, port=%d, err=%d\n", this, safestr(host), port, err);
        return false;
    }

    return true;
}

void TCPClient::dns_callback(const char* host, const ip_addr_t *ipaddr, void *arg)
{
    trace("TCPClient::dns_callback: this=%p host=%s, ip=%p\n", arg, safestr(host), ipaddr);
    
    TCPClient *self = (TCPClient *)arg;
    
    if (!ipaddr)
    {
        self->on_closed();
        return;
    }

    self->connect(host, ipaddr, self->m_port);
}

bool TCPClient::connect(const char *host, const ip_addr_t *ipaddr, u16_t port)
{
    trace("TCPClient::connect: this=%p host=%s, ip=%p, port=%d\n", this, safestr(host), ipaddr, port);
    
    struct altcp_pcb *pcb = (struct altcp_pcb *)altcp_new_ip_type(&tcp_allocator, IPADDR_TYPE_ANY);
    altcp_setprio(pcb, TCP_PRIO_MIN);
    
    m_session = Session::create(pcb);
    m_session->set_callback(this);
    
    err_t err = m_session->connect(ipaddr, port);

    if (err != ERR_OK)
    {
        trace("TCPClient::connect: this=%p host=%s, ip=%p, port=%d, err=%d, err_str=%s\n", this, safestr(host), ipaddr, port, err, lwip_strerr(err));
        m_session->close();
        return false;
    }

    return true;
}

void TCPClient::on_sent(u16_t len)
{
    trace("TCPClient::on_sent: this=%p len=%d", this, len);
}

void TCPClient::on_recv(u8_t *data, size_t len)
{
    trace("TCPClient::on_recv: this=%p data=%p, len=%d", this, data, len);
}

void TCPClient::on_closed()
{
    trace("TCPClient::on_closed: this=%p", this);
    m_session.reset();
}

void TCPClient::on_connected()
{
    trace("TCPClient::on_connected: this=%p", this);
}
