#include <string.h>
#include <time.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "tls_listener.h"
#include "http_session.h"
#include "wifi.h"

#include "pico_logger.h"

Session *HTTPSession::create(void *arg) {
    return new HTTPSession(arg);
}

HTTPSession::HTTPSession(void *arg)
    : Session(arg)
    , m_bytes_pending(0)
{
    trace("HTTPSession::HTTPSession: this=%p, arg=%p\n", this, arg);
}

void HTTPSession::on_recv(u8_t *data, size_t len) {
    trace("HTTPSession::on_recv: this=%p, data=%p, len=%d\n", this, data, len);

    char buffer[128] = {0};
    char body[128] = {0};
    snprintf(body, 128, "<html><body>hello world, uptime: %lld</html></body>", get_absolute_time());
    snprintf(buffer, 128, "HTTP/1.0 200 OK\r\nContent-Length: %d\r\n\r\n", strlen(body)); 

    m_bytes_pending = strlen(buffer) + strlen(body);
    trace("HTTPSession::send: this=%p, buffer:\n%s%s\n", this, buffer, body);

    err_t err = send((u8_t*)buffer, strlen(buffer));
    if (err != ERR_OK) {
        printf("Failed writing data, error: %d", err);
    }

    err = send((u8_t*)body, strlen(body));
    if (err != ERR_OK) {
        printf("Failed writing data, error: %d", err);
    }

    trace("HTTPSession::send: this=%p, buffer:\n%s\n", this, buffer);
}

void HTTPSession::on_sent(u16_t len) {
    trace("HTTPSession::on_sent: this=%p, len=%d, m_bytes_pending=%d\n", this, len, m_bytes_pending);
    m_bytes_pending -= len;
}

void HTTPSession::on_closed() {
    delete this;
}
