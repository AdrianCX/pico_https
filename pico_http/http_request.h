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
#pragma once

#include "session.h"

///
/// Basic example HTTP(S) request handler, future functionality pending.
/// Right now it's fire and forget for some telegram notifications.
///
class HTTPRequest : public ISessionCallback
{
    static const int MAX_HEADER_SIZE=1000;
    
public:
    HTTPRequest(const char *host,
                uint16_t port,
                bool tls,
                const char *command,
                const char *path);

    bool addHeader(const char *header, const char* value);
    bool setBody(u8_t *data, size_t len);
    void setDebug(bool debug);

    bool send();
    
protected:
    virtual bool on_sent(u16_t len) override { return true; };
    virtual bool on_recv(u8_t *data, size_t len) override;
    virtual void on_closed() override;
    virtual void on_connected() override;

private:
    virtual ~HTTPRequest();
    
    char *m_host = NULL;
    uint16_t m_port = 443;

    uint16_t m_headerIndex = 0;
    char m_header[MAX_HEADER_SIZE+2];

    u8_t  *m_body = NULL;
    size_t m_bodyLen = 0;
    
    Session m_connection;
};
