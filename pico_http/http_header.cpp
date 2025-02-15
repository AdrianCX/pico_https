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
#include "string.h"

#ifdef __TARGET_CPU_CORTEX_M0PLUS
#include "pico/stdlib.h"
#else
#include "stdlib.h"
#endif

#include "http_header.h"

extern "C" void trace(const char *parameters, ...);
extern "C" const char *safestr(const char *value);

HTTPHeader::HTTPHeader()
{
}

void HTTPHeader::print()
{
    if (isRequest())
    {
        trace("HTTPHeader::print: this=%p, command[%s] path[%s]\n", this, safestr(m_first), safestr(m_second));
    }
    else
    {
        trace("HTTPHeader::print: this=%p, response[%s]\n", this, safestr(m_second));
    }
    for (int i=0;i<m_numHeaders;++i)
    {
        trace("HTTPHeader::print: this=%p, name[%s] value[%s]\n", this, safestr(m_headerName[i]), safestr(m_headerValue[i]));
    }
}

const char *HTTPHeader::getHeaderValue(const char *headerName)
{
    for (int i=0;i<m_numHeaders;++i)
    {
        if (strcasecmp(m_headerName[i], headerName) == 0)
        {
            trace("HTTPHeader::getHeaderValue: this=%p, found header[%s] value[%s]\n", this, safestr(headerName), safestr(m_headerValue[i]));
            return m_headerValue[i];
        }
    }
    trace("HTTPHeader::getHeaderValue: this=%p, could not find[%s]\n", this, safestr(headerName));
    return NULL;
}

bool HTTPHeader::parse(char *data, int len)
{
    m_numHeaders = 0;
    m_responseCode = 0;
    m_headerSize = 0;

    int i=0;
    m_first = &data[i];

    // GET/POST or HTTP/1.1 for responses
    for (;i<len && data[i]!=' ';++i) {};
    
    if (i>=len)
        return false;

    data[i++] = 0;

    // PATH or response code
    m_second = &data[i];
    for (;i<len && data[i]!=' ';++i)
    {
        if (isResponse())
        {
            if ((data[i] < '0') || (data[i] > '9'))
            {
                return false;
            }
            
            m_responseCode = m_responseCode * 10 + (m_second[0] - '0');
        }
    }
    
    if (i>=len)
        return false;

    data[i++]=0;

    // HTTP/1.[01] or response text
    for (;i<len && data[i]!='\n';++i) {}

    ++i;
    if (i+1>=len)
        return false;

    if (data[i] == '\r' && data[i+1] == '\n')
    {
        m_headerSize = i+2;
        return true;
    }

    while (m_numHeaders<MAX_HEADERS)
    {
        m_headerName[m_numHeaders]=&data[i];
    
        for (;i<len && data[i]!=':';++i) {};

        if (i>=len)
            return false;

        data[i++]=0;

        // skip space too.
        ++i;

        m_headerValue[m_numHeaders]=&data[i];
        for (;i<len && data[i]!='\r';++i) {};
        
        if (i>=len)
            return false;

        data[i++]=0;

        if ((i>=len) || (data[i] != '\n'))
        {
            return false;
        }
        ++i;

        if (i+1>=len)
            return false;

        ++m_numHeaders;
        
        if (data[i] == '\r' && data[i+1] == '\n')
        {
            m_headerSize = i+2;
            return true;
        }
    }

    for (;i+4<=len;++i)
    {
        if (data[i]=='\r' && data[i+1] == '\n' && data[i+2] == '\r' && data[i+3] == '\n')
        {
            m_headerSize = i+4;
            return true;
        }
    }

    return false;
}

