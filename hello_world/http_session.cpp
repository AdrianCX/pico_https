#include <string.h>
#include <time.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "tls_listener.h"
#include "http_session.h"
#include "wifi.h"

#include "pico_logger.h"
#include "rc/index.html.hex.h"

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

    char buffer[128];
    
    snprintf(buffer, 128, "HTTP/1.0 200 OK\r\nContent-Length: %d\r\n\r\n", _source_hello_world_rc_index_html_len); 

    m_bytes_pending = strlen(buffer) + _source_hello_world_rc_index_html_len;
    trace("HTTPSession::send: this=%p, buffer:\n%s\n", this, buffer);

    err_t err = send((u8_t*)buffer, strlen(buffer));
    if (err != ERR_OK) {
        printf("Failed writing data, error: %d", err);
    }

    err = send((u8_t*)_source_hello_world_rc_index_html, _source_hello_world_rc_index_html_len);
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
