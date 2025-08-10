/* MIT License

Copyright (c) 2024 Adrian Cruceru - https://github.com/AdrianCX/pico_https_example

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

#include "session.h"
#include "pico_logger.h"
#include "general_config.h"
#include "http_request.h"
#include <cstdlib>

HTTPRequest::HTTPRequest(const char *host, uint16_t port, bool tls, const char *command, const char *path)
{
    trace("HTTPRequest::HTTPRequest: this=%p, host=%s, port=%d, tls=%d, command=%s, path=%s", this, safestr(host), port, tls, safestr(command), safestr(path));
    
    m_connection.set_callback(this);
    m_connection.set_tls(tls);

    if ((host == NULL) || (command == NULL) || (path == NULL) || (port == 0))
    {
        trace("HTTPRequest::HTTPRequest: this=%p, host=%s, port=%d, tls=%d, command=%s, path=%s, error: bad parameters", this, safestr(host), port, tls, safestr(command), safestr(path));

        m_state = FAIL;
        return;
    }

    // first part of the buffer is the host
    m_headerIndex = snprintf(m_header, MAX_HEADER_SIZE, "%s", host);
    if ((m_headerIndex >= MAX_HEADER_SIZE-2) || (m_headerIndex < 0))
    {
        trace("HTTPRequest::HTTPRequest: this=%p, host=%s, port=%d, tls=%d, command=%s, path=%s, error: host is larger then buffer", this, host, port, tls, command, path);
        m_state = FAIL;
        return;
    }

    ++m_headerIndex;

    m_headerStart = m_headerIndex;
    m_port = port;
    
    int written = snprintf(&m_header[m_headerIndex], MAX_HEADER_SIZE-m_headerIndex, "%s %s HTTP/1.1\r\nHost: %s:%d\r\n", command, path, host, port);

    if ((m_headerIndex + written >= MAX_HEADER_SIZE-2) || (written < 0))
    {
        trace("HTTPRequest::HTTPRequest: this=%p, host=%s, port=%d, tls=%d, command=%s, path=%s, failure writing to buffer: %d", this, host, port, tls, command, path, written);

        m_headerIndex = MAX_HEADER_SIZE;
        m_state = FAIL;
        return;
    }

    m_headerIndex += written;
}

HTTPRequest::~HTTPRequest()
{
    trace("HTTPRequest::~HTTPRequest: this=%p", this);
    
    if (m_body != NULL)
    {
        free(m_body);
        m_body = NULL;
    }

    if (m_callback != NULL)
    {
        m_callback->onRequestDestroyed();
        m_callback = NULL;
    }
}

bool HTTPRequest::addHeader(const char *header, const char* value)
{
    if (m_state == FAIL)
    {
        trace("HTTPRequest::addHeader: this=%p, request already failed.", this);
        return false;
    }

    if (m_headerIndex >= MAX_HEADER_SIZE)
    {
        trace("HTTPRequest::addHeader: this=%p, not enough room, increase MAX_HEADER_SIZE", this);
        return false;
    }

    int written = snprintf(&m_header[m_headerIndex], MAX_HEADER_SIZE-m_headerIndex, "%s: %s\r\n", header, value);
    if ((m_headerIndex + written >= MAX_HEADER_SIZE) || (written < 0))
    {
        trace("HTTPRequest::addHeader: this=%p, header=%s, value=%s failure writing to buffer: %d", this, header, value, written);
        m_headerIndex = MAX_HEADER_SIZE;

        return false;
    }

    m_headerIndex += written;
    return true;
}

bool HTTPRequest::setBody(u8_t *data, size_t len)
{
    if (m_state == FAIL)
    {
        trace("HTTPRequest::addHeader: this=%p, request already failed.", this);
        return false;
    }

    if (m_headerIndex >= MAX_HEADER_SIZE)
    {
        trace("HTTPRequest::setBody: this=%p, not enough room, increase MAX_HEADER_SIZE", this);
        return false;
    }

    if (m_body != NULL)
    {
        trace("HTTPRequest::setBody: this=%p, body was already set.", this);
        return false;
    }

    m_bodyLen = len;
    m_body = (u8_t *)malloc(len);
    memcpy(m_body, data, len);
    
    int written = snprintf(&m_header[m_headerIndex], MAX_HEADER_SIZE-m_headerIndex, "Content-Length: %d\r\n", len);

    if ((m_headerIndex + written >= MAX_HEADER_SIZE) || (written < 0))
    {
        trace("HTTPRequest::setBody: this=%p, failure writing content-length to buffer: %d", this, written);
        m_headerIndex = MAX_HEADER_SIZE;
        return false;
    }

    m_headerIndex += written;
    return true;
}

void HTTPRequest::setDebug(bool debug)
{
    m_connection.set_debug(debug);
}

bool HTTPRequest::send()
{
    trace("HTTPRequest::send: this=%p, m_host=%s, m_port=%d", this, m_header, m_port);

    if (m_state == FAIL)
    {
        trace("HTTPRequest::send: this=%p, request already failed.", this);
        return false;
    }
    
    if (m_port == 0)
    {
        trace("HTTPRequest::send: this=%p, bad parameters.", this);
        return false;
    }


    if (m_headerIndex >= MAX_HEADER_SIZE-3)
    {
        trace("HTTPRequest::send: this=%p, not enough room for header, increase MAX_HEADER_SIZE", this);
        return false;
    }

    m_header[m_headerIndex++]='\r';
    m_header[m_headerIndex++]='\n';
    m_header[m_headerIndex++]=0;

    err_t err = m_connection.connect(m_header, m_port);

    if (err != ERR_OK)
    {
        trace("HTTPRequest::send: this=%p, m_host=%s, m_port=%d, error=%d", this, m_header, m_port, err);
        return false;
    }

    return true;
}

void HTTPRequest::on_connected()
{
    trace("HTTPRequest::on_connected: this=%p, host=%s, header=%s, body=[%.*s] bodyLen=%d", this, m_header, &m_header[m_headerStart], m_bodyLen, m_body, m_bodyLen);
    
    m_connection.send((const u8_t*)&m_header[m_headerStart], m_headerIndex-m_headerStart-1);

    if (m_body != NULL && m_bodyLen > 0)
    {
        m_connection.send(m_body, m_bodyLen);
    }
}

bool HTTPRequest::on_recv(u8_t *data, size_t len)
{
    switch (m_state)
    {
        case INIT:
        {
            HTTPHeader header;
            if (!header.parse((char *)data, len))
            {
                return false;
            }
            
            m_state = HEADER_RECEIVED;
            if (m_callback != NULL && !m_callback->onHeaderReceived(header))
            {
                return false;
            }

            data += header.getHeaderSize();
            len -= header.getHeaderSize();
            
            if ((len > 0) && (m_callback != NULL) && (!m_callback->onHttpData(data,len)))
            {
                return false;
            }

            return true;
        }
        case HEADER_RECEIVED:
        {
            return ((m_callback == NULL) || (m_callback->onHttpData(data, len)));
        }
        case WEBSOCKET_ESTABLISHED:
        {
            // future functionality
            return true;
        }
        case FAIL:
        {
            return false;
        }
    }
    return true;
}

void HTTPRequest::on_closed()
{
    trace("HTTPRequest::on_closed: this=%p", this);
    delete this;
}
