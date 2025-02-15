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

#ifndef HTTP_SESSION_H
#define HTTP_SESSION_H

#ifdef __TARGET_CPU_CORTEX_M0PLUS
#include "pico/stdlib.h"
#else
#include "stdlib.h"
#endif

#include "session.h"
#include "http_header.h"
#include "websocket_handler.h"

enum HTTPSessionState
{
    INIT,
    HEADER_RECEIVED,
    WEBSOCKET_ESTABLISHED,
    FAIL,
};

class HTTPSession
    : public WebSocketInterface
    , public ISessionCallback
{
public:
    static void create(void *arg, bool tls);

    virtual bool onRequestReceived(HTTPHeader& header) { return false; };
    virtual bool onHttpData(u8_t *data, size_t len) { return false; }

    virtual bool onWebSocketData(u8_t *data, size_t len) override { return false; }
    virtual bool onWebsocketEncodedData(const uint8_t *data, size_t len) override;
    
    bool acceptWebSocket(HTTPHeader& header);
    
    bool sendHttpReply(const char *extra_headers, const char *body, int body_len);
    bool sendWebSocketData(const uint8_t *body, int body_len);

    virtual bool on_recv(u8_t *data, size_t len) override;
    virtual bool on_sent(u16_t len) override;
    virtual void on_closed() override;

    void close();
    u16_t send_buffer_size();

protected:
    HTTPSession(void *arg, bool tls);
    virtual ~HTTPSession();
    
private:
    HTTPSessionState m_state;
    HTTPHeader m_header;
    WebSocketHandler m_websocketHandler;
    Session *m_session;
};

#endif
