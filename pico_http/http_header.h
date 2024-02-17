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