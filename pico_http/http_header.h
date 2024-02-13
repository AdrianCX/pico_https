#ifndef HTTP_HEADER_H
#define HTTP_HEADER_H


///
/// Simple HTTP Header parser that uses provided data in place, no allocations or extra buffers.
/// All pointers live as long as provided data.
///
class HTTPHeader
{
    static const int MAX_HEADERS=20;
public:
    HTTPHeader();

    bool parse(char *data, int len);

    const char *getCommand() { return m_command; }
    const char *getPath() { return m_path; }

    int getNumHeaders() { return m_numHeaders; }
    const char *getHeaderValue(const char *headerName);
    
    int getHeaderSize() { return m_headerSize; }

    void print();
private:
    char *m_command;
    char *m_path;
    
    int m_numHeaders;
    char *m_headerName[MAX_HEADERS];
    char *m_headerValue[MAX_HEADERS];

    int m_headerSize;
};

#endif