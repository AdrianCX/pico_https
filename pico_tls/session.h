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
#ifndef PICO_SESSION_H
#define PICO_SESSION_H

#include "pico/cyw43_arch.h"
#include "lwip/altcp_tcp.h"

// Values for lwip err_t 
//
//  0,             /* ERR_OK          0      No error, everything OK. */
//  ENOMEM,        /* ERR_MEM        -1      Out of memory error.     */
//  ENOBUFS,       /* ERR_BUF        -2      Buffer error.            */
//  EWOULDBLOCK,   /* ERR_TIMEOUT    -3      Timeout                  */
//  EHOSTUNREACH,  /* ERR_RTE        -4      Routing problem.         */
//  EINPROGRESS,   /* ERR_INPROGRESS -5      Operation in progress    */
//  EINVAL,        /* ERR_VAL        -6      Illegal value.           */
//  EWOULDBLOCK,   /* ERR_WOULDBLOCK -7      Operation would block.   */
//  EADDRINUSE,    /* ERR_USE        -8      Address in use.          */
//  EALREADY,      /* ERR_ALREADY    -9      Already connecting.      */
//  EISCONN,       /* ERR_ISCONN     -10     Conn already established.*/
//  ENOTCONN,      /* ERR_CONN       -11     Not connected.           */
//  -1,            /* ERR_IF         -12     Low-level netif error    */
//  ECONNABORTED,  /* ERR_ABRT       -13     Connection aborted.      */
//  ECONNRESET,    /* ERR_RST        -14     Connection reset.        */
//  ENOTCONN,      /* ERR_CLSD       -15     Connection closed.       */
//  EIO            /* ERR_ARG        -16     Illegal argument.        */

extern altcp_allocator_t tcp_allocator;

typedef void (session_factory_t)(void *arg, bool tls);

class ISessionCallback
{
public:
    virtual ~ISessionCallback() {};
    
    virtual bool on_sent(u16_t len) { return true; }
    virtual bool on_recv(u8_t *data, size_t len) { return true; }
    virtual void on_closed() {};
    virtual void on_connected() {};
};

class ISessionSender
{
public:
    virtual ~ISessionSender() {};
    
    virtual err_t connect(const char *host, u16_t port) = 0;
    virtual err_t connect(const char *host, const ip_addr_t *ipaddr, u16_t port) = 0;
    virtual err_t send(const u8_t *data, size_t len) = 0;
    virtual err_t flush() = 0;
    virtual err_t close() = 0;
    virtual u16_t send_buffer_size() = 0;
    virtual bool is_connected() = 0;
};

// Can be either TLS or regular TCP, abstracted away by altcp.
class Session
    : public ISessionSender
{
public:
    static void create_client_tls_config(const uint8_t *cert, size_t cert_len);

    Session(void *arg = NULL, bool tls = false);
    virtual ~Session();

    void set_tls(bool tls) { m_tls = tls; }
    
    // ISender
    virtual err_t connect(const char *host, u16_t port) override;
    virtual err_t connect(const char *host, const ip_addr_t *ipaddr, u16_t port) override;
    virtual err_t send(const u8_t *data, size_t len) override;
    virtual err_t close() override;
    virtual err_t flush() override;
    virtual u16_t send_buffer_size() override;
    virtual bool is_connected() override;

    void set_callback(ISessionCallback *callback) { m_callback = callback; }

    static int get_num_sessions() { return NUM_SESSIONS; }

    void *get_pcb() { return m_pcb; }
private:
    static err_t lwip_connected(void *arg, struct altcp_pcb *pcb, err_t err);
    static err_t lwip_recv(void *arg, struct altcp_pcb *pcb, struct pbuf *p, err_t err);
    static err_t lwip_sent(void *arg, struct altcp_pcb *pcb, u16_t len);
    static err_t lwip_close_or_abort_conn(struct altcp_pcb *pcb, Session *hs, u8_t abort_conn);
    static err_t lwip_close_conn(struct altcp_pcb *pcb, Session *hs);
    static void lwip_err(void *arg, err_t err);
    static void dns_callback(const char* hostname, const ip_addr_t *ipaddr, void *arg);

    void init_pcb();
    err_t check_send_failure(err_t err);

    bool m_connected;
    bool m_closing;
    bool m_sending;
    bool m_tls;
    u16_t m_sentBytes;
    u16_t m_port;
    
    ISessionCallback *m_callback;
    struct altcp_pcb *m_pcb;
    
    static int NUM_SESSIONS;
};

#endif
