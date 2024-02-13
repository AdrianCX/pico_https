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
    : m_command(NULL)
    , m_path(NULL)
    , m_numHeaders(0)
{
}

void HTTPHeader::print()
{
    trace("HTTPHeader::print: this=%p, command[%s] path[%s]\n", this, safestr(m_command), safestr(m_path));
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

    int i=0;
    m_command = &data[i];

    // GET/POST
    for (;i<len && data[i]!=' ';++i) {};
    
    if (i>=len)
        return false;

    data[i++] = 0;

    // PATH
    m_path = &data[i];
    for (;i<len && data[i]!=' ';++i) {}
    
    if (i>=len)
        return false;

    data[i++]=0;

    // HTTP/1.[01]
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

    for (;i+3<=len;++i)
    {
        if (data[i]=='\r' && data[i+1] == '\n' && data[i+2] == '\r' && data[i+3] == '\n')
        {
            m_headerSize = i+4;
            return true;
        }
    }

    return false;
}

