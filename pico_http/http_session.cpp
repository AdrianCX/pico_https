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
#include <algorithm>
#include <string.h>
#include <time.h>

#include "pico/cyw43_arch.h"

#include "http_session.h"
#include "mbedtls_wrapper.h"
#include "pico_logger.h"

void HTTPSession::create(void *arg, bool tls) {
    new HTTPSession(arg, tls);
}

HTTPSession::HTTPSession(void *arg, bool tls)
    : m_state(INIT)
    , m_session(new Session(arg, tls))
{
    trace("HTTPSession::HTTPSession: this=%p, arg=%p, tls=%d\n", this, arg, tls);

    m_session->set_callback(this);
}

HTTPSession::~HTTPSession()
{
    trace("HTTPSession::~HTTPSession: this=%p, session=%p, arg=%p\n", this, m_session, m_session != NULL ? m_session->get_pcb() : NULL);
    if (m_session != NULL)
    {
        delete m_session;
    }
}

bool HTTPSession::on_recv(u8_t *data, size_t len)
{
    switch (m_state)
    {
        case INIT:
        {
            if (!m_header.parse((char *)data, len))
            {
                return false;
            }
            
            trace("HTTPSession::on_recv: this=%p, header:\n", this);
            m_header.print();
            
            m_state = HEADER_RECEIVED;
            if (!onRequestReceived(m_header))
            {
                return false;
            }

            data += m_header.getHeaderSize();
            len -= m_header.getHeaderSize();
            
            if ((len > 0) && (!onHttpData(data,len)))
            {
                return false;
            }

            return true;
        }
        case HEADER_RECEIVED:
        {
            return onHttpData(data,len);
        }
        case WEBSOCKET_ESTABLISHED:
        {
            return m_websocketHandler.decodeData(data, len, this);
        }
        case FAIL:
        {
            return false;
        }
    }
    return true;
}

bool HTTPSession::onWebsocketEncodedData(const uint8_t *data, size_t len)
{
    err_t err = m_session->send((u8_t*)data, len);
    if (err != ERR_OK) {
        trace("HTTPSession::onWebsocketEncodedData: this=%p, failed sending websocket data error[%d]\n", this, err);
        return false;
    }

    return true;
}

bool HTTPSession::sendWebSocketData(const uint8_t *body, int body_len)
{
    if (!m_websocketHandler.encodeData(body, body_len, this))
    {
        return false;
    }

    return true;
}


bool HTTPSession::sendHttpReply(const char *extra_headers, const char *body, int body_len)
{
    const int BUFFER_SIZE = 128;
    
    char buffer[BUFFER_SIZE];
    int n = snprintf(buffer, BUFFER_SIZE, "HTTP/1.0 200 OK\r\n%sContent-Length: %d\r\n\r\n", extra_headers, body_len);

    if (n >= BUFFER_SIZE)
    {
        trace("HTTPSession::sendReply: this=%p, buffer to small, expected[%d] got[%d]:\n", this, n, BUFFER_SIZE);
        return false;
    }

    trace("HTTPSession::sendReply: this=%p, replyHeader:\n%s\n", this, buffer);

    err_t err = m_session->send((u8_t*)buffer, n);
    if (err != ERR_OK) {
        trace("HTTPSession::sendReply: this=%p, failed sending header error[%d]\n", this, err);
        return false;
    }

    err = m_session->send((u8_t*)body, body_len);
    if (err != ERR_OK) {
        trace("HTTPSession::sendReply: this=%p, failed sending body[%p], error[%d] body_len[%d]\n", this, body, err, body_len);
        return false;
    }

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
    
    err = m_session->send((u8_t*)reply, n);
    if (err != ERR_OK)
    {
        printf("HTTPSession::acceptWebSocket: failed sending reply, error[%d]", err);
        return false;
    }

    m_state = WEBSOCKET_ESTABLISHED;
    trace("HTTPSession::acceptWebSocket: this=%p, websocket accepted, reply:\n%s\n", this, reply);
    return true;    
}   


bool HTTPSession::on_sent(u16_t len) {
    return true;
}

void HTTPSession::close()
{
    m_state = FAIL;
    
    if (m_session)
    {
        m_session->close();
    }
}

u16_t HTTPSession::send_buffer_size()
{
    return m_session ? m_session->send_buffer_size() : 0;
}

void HTTPSession::on_closed() {
    trace("HTTPSession::on_closed: this=%p\n", this);
    delete this;
}

