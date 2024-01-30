#ifndef HTTP_SESSION_H
#define HTTP_SESSION_H

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "session.h"

class HTTPSession : public Session {
public:
    static Session *create(void *arg);
    
    virtual void on_recv(u8_t *data, size_t len) override;
    virtual void on_sent(u16_t len) override;

    // Called when connection is terminated
    virtual void on_closed() override;

protected:
    HTTPSession(void *arg);
    virtual ~HTTPSession() {}
    
private:
    int m_bytes_pending = 0;
};

#endif
