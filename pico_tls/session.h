#ifndef MBEDTLS_TLS_SESSION_H
#define MBEDTLS_TLS_SESSION_H

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

// Can be either TLS or regular TCP, abstracted away by altcp.
class Session {
public:
    Session(void *arg);
    //
    // Send the requested parameters, get called back as bytes are sent in on_sent.
    //
    // @returns - ERR_MEM if the length of the data exceeds the current send buffer size or if the length of the queue of outgoing segment is larger than the upper limit defined in lwipopts.h.
    //            can call send_buffer_size() to obtain max buffer size available.
    //          - ERR_CLSD if trying to send data on already closed connection.
    //          - ERR_VAL if data is NULL
    //
    err_t send(u8_t *data, size_t len);
    virtual void on_sent(u16_t len) {}

    err_t flush();
    u16_t send_buffer_size();

    //
    // Called when data is received.
    //
    virtual void on_recv(u8_t *data, size_t len) {}

    //
    // Call when you want to close the connection, wait until on_closed is called to confirm/free memory.
    //
    void close();
    virtual void on_closed() {}

    //
    // Called when error ocurred. Connection is closed at this point. Session object still exists until user decides to delete.
    //
    virtual void on_error(err_t err, const char *err_str) {}


protected:
    virtual ~Session();
    
private:
    static err_t http_recv(void *arg, struct altcp_pcb *pcb, struct pbuf *p, err_t err);
    static err_t http_sent(void *arg, struct altcp_pcb *pcb, u16_t len);
    static err_t http_close_or_abort_conn(struct altcp_pcb *pcb, Session *hs, u8_t abort_conn);
    static err_t http_close_conn(struct altcp_pcb *pcb, Session *hs);
    static void http_err(void *arg, err_t err);

    struct altcp_pcb *m_pcb;
    bool m_closing;
    
    static err_t http_poll(void *arg, struct altcp_pcb *pcb);
};

#endif