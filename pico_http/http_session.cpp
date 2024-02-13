#include <algorithm>
#include <string.h>
#include <time.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "http_session.h"
#include "mbedtls_wrapper.h"
#include "pico_logger.h"

Session *HTTPSession::create(void *arg) {
    return new HTTPSession(arg);
}

HTTPSession::HTTPSession(void *arg)
    : Session(arg)
    , m_state(INIT)
{
    trace("HTTPSession::HTTPSession: this=%p, arg=%p\n", this, arg);
}

bool HTTPSession::on_recv(u8_t *data, size_t len) {
    trace("HTTPSession::on_recv: this=%p, m_state=%d, data=%p, len=%d\n", this, m_state, data, len);

    switch (m_state)
    {
        case INIT:
        {
            if (!m_header.parse((char *)data, len))
            {
                m_state = FAIL;
                return false;
            }
            
            trace("HTTPSession::on_recv: this=%p, header:\n", this);
            m_header.print();
            
            m_state = HEADER_RECEIVED;
            if (!onRequestReceived(m_header))
            {
                m_state = FAIL;
                return false;
            }

            data += m_header.getHeaderSize();
            len -= m_header.getHeaderSize();
            
            if (len > 0)
            {
                return onHttpData(data,len);
            }
            
            return true;
        }
        case HEADER_RECEIVED:
        {
            return onHttpData(data,len);            
        }
        case WEBSOCKET_WAIT_PACKET:
        case WEBSOCKET_WAIT_DATA:
        {
            return decodeWebSocketData(data, len);
        }
        case FAIL:
        {
            return false;
        }
    }
    
    return false;
}

bool HTTPSession::decodeWebSocketData(u8_t *data, size_t len)
{
    // TODO
    trace("HTTPSession::decodeWebSocketData: this=%p, NOT Implemented:\n", this);
    return true;
}

bool HTTPSession::sendWebSocketData(const char *body, int body_len)
{
    // TODO
    trace("HTTPSession::sendWebSocketData: this=%p, NOT Implemented:\n", this);
    return true;
}

bool HTTPSession::sendHttpReply(const char *body, int body_len)
{
    const int BUFFER_SIZE = 128;
    
    char buffer[BUFFER_SIZE];
    int n = snprintf(buffer, BUFFER_SIZE, "HTTP/1.0 200 OK\r\nContent-Length: %d\r\n\r\n", body_len);

    if (n >= BUFFER_SIZE)
    {
        trace("HTTPSession::sendReply: this=%p, buffer to small, expected[%d] got[%d]:\n", this, n, BUFFER_SIZE);
        return false;
    }

    trace("HTTPSession::sendReply: this=%p, replyHeader:\n%s\n", this, buffer);

    err_t err = send((u8_t*)buffer, n);
    if (err != ERR_OK) {
        trace("HTTPSession::sendReply: this=%p, failed sending header error[%d]\n", this, err);
        return false;
    }

    err = send((u8_t*)body, body_len);
    if (err != ERR_OK) {
        trace("HTTPSession::sendReply: this=%p, failed sending body, error[%d]\n", this, err);
        return false;
    }

    flush();
    return true;
}

bool HTTPSession::acceptWebSocket(HTTPHeader& header)
{
    const int BUFFER_SIZE = 128;
    const int REPLY_SIZE = 256;
    const int SHA1_SIZE = 20;

    const char *websocket_key = header.getHeaderValue("Sec-WebSocket-Key");
    if (websocket_key == NULL)
    {
        trace("HTTPSession::acceptWebSocket: this=%p, request missing header 'Sec-Websocket-Key':\n", this);
        header.print();
        return false;
    }

    const char *KEY_BUFFER = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    static const int KEY_BUFFER_SIZE = strlen(KEY_BUFFER);

    int len = strlen(websocket_key);

    if (len+1 >= BUFFER_SIZE - KEY_BUFFER_SIZE)
    {
        trace("HTTPSession::acceptWebSocket: this=%p, request 'Sec-Websocket-Key' too long: len[%d] max[%d]:\n", this, len, (BUFFER_SIZE - KEY_BUFFER_SIZE));
        return false;
    }
    char buffer[BUFFER_SIZE];
    memcpy(&buffer[0], websocket_key, len);
    memcpy(&buffer[len], KEY_BUFFER, KEY_BUFFER_SIZE+1);

    uint8_t sha1sum[SHA1_SIZE];
    int err = sha1((u8_t*)buffer, len+KEY_BUFFER_SIZE, sha1sum );
    if (err != 0)
    {
        trace("HTTPSession::acceptWebSocket: sha1 error[%d]\n", this, err);
        return false;
    }
    
    trace("HTTPSession::acceptWebSocket: sha1[%s] len[%d]", buffer, len+KEY_BUFFER_SIZE);
    err = base64_encode(sha1sum, 20, (u8_t*)buffer, 128);
    if (err != 0)
    {
        trace("HTTPSession::acceptWebSocket: base64 error[%d]\n", this, err);
        return false;
    }

    char reply[REPLY_SIZE];
    int n = snprintf(&reply[0], REPLY_SIZE, "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\n\r\n", buffer);
    if ((n >= REPLY_SIZE) || (n <= 0))
    {
        trace("HTTPSession::acceptWebSocket: reply buffer too small, expected[%d] had[%d]\n", this, n, BUFFER_SIZE);
        return false;
    }
    
    err = send((u8_t*)reply, n);
    if (err != ERR_OK)
    {
        printf("HTTPSession::acceptWebSocket: failed sending reply, error[%d]", err);
        return false;
    }

    flush();
    m_state = WEBSOCKET_WAIT_PACKET;
    trace("HTTPSession::acceptWebSocket: this=%p, websocket accepted, reply:\n%s\n", this, reply);
    return true;    
}   


void HTTPSession::on_sent(u16_t len) {
    trace("HTTPSession::on_sent: this=%p, len=%d\n", this, len);
}

void HTTPSession::on_closed() {
    trace("HTTPSession::on_closed: this=%p\n", this);
    delete this;
}
