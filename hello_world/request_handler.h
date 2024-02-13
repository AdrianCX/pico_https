#ifndef REQUEST_HANDLER_H
#define REQUEST_HANDLER_H

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "http_session.h"

class RequestHandler : public HTTPSession {
public:
    static Session *create(void *arg);

    virtual bool onRequestReceived(HTTPHeader& header) override;

    bool onHttpData(u8_t *data, size_t len) override;
    bool onWebSocketData(u8_t *data, size_t len) override;

protected:
    RequestHandler(void *arg);
    virtual ~RequestHandler() {};
};


#endif