#ifndef HTTP_SESSION_H
#define HTTP_SESSION_H

#ifdef __TARGET_CPU_CORTEX_M0PLUS
#include "pico/stdlib.h"
#else
#include "stdlib.h"
#endif

#include "session.h"
#include "http_header.h"

enum HTTPSessionState
{
    INIT,
    HEADER_RECEIVED,
    WEBSOCKET_WAIT_PACKET,
    WEBSOCKET_WAIT_DATA,
    FAIL,
};

class HTTPSession : public Session {
public:
    static Session *create(void *arg);

    virtual bool onRequestReceived(HTTPHeader& header) { return false; };
    virtual bool onHttpData(u8_t *data, size_t len) { return false; }
    virtual bool onWebSocketData(u8_t *data, size_t len) { return false; }
    
    bool acceptWebSocket(HTTPHeader& header);
    bool sendWebSocketData(const char *body, int body_len);
    
    bool sendHttpReply(const char *body, int body_len);

    virtual bool on_recv(u8_t *data, size_t len) override;
    virtual void on_sent(u16_t len) override;
    virtual void on_closed() override;

protected:
    HTTPSession(void *arg);
    virtual ~HTTPSession() {}
    
private:
    bool decodeWebSocketData(u8_t *data, size_t len);

    HTTPSessionState m_state;
    HTTPHeader m_header;
};

#endif
