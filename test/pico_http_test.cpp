#include <gtest/gtest.h>
#include <stdarg.h>
#include "pico_http/http_header.h"

extern "C" void trace(const char *parameters, ...)
{
    va_list args;
    va_start (args, parameters);
    vprintf(parameters, args);
    va_end (args);
}

extern "C" const char *safestr(const char *value)
{
    return value != NULL ? value : "null";
}

// Demonstrate some basic assertions.
TEST(HTTPHeader, BasicParse) {

    HTTPHeader header;
    
    const char *request = "GET /assets/chunk-node_modules_scroll-anchoring_dist_scroll-anchoring_esm_js-app_components_notifications_notif-fce33d-ecb3639d56ad.js HTTP/1.1\r\n"
        "Host: github.githubassets.com\r\n"
        "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:121.0) Gecko/20100101 Firefox/121.0\r\n"
        "Accept: */*\r\n"
        "Accept-Language: en-US,en;q=0.5\r\n"
        "Accept-Encoding: gzip, deflate, br\r\n"
        "Referer: https://github.com/\r\n"
        "Origin: https://github.com\r\n"
        "Connection: keep-alive\r\n"
        "Sec-Fetch-Dest: script\r\n"
        "Sec-Fetch-Mode: cors\r\n"
        "Sec-Fetch-Site: cross-site\r\n"
        "\r\n";

    int buffer_size=strlen(request);
    char *buffer = new char[buffer_size];

    memcpy(buffer, request, buffer_size);

    EXPECT_EQ(true, header.parse(buffer, buffer_size));
    EXPECT_STREQ(header.getCommand(), "GET");
    EXPECT_STREQ(header.getPath(), "/assets/chunk-node_modules_scroll-anchoring_dist_scroll-anchoring_esm_js-app_components_notifications_notif-fce33d-ecb3639d56ad.js");
    EXPECT_STREQ(header.getHeaderValue("Host"), "github.githubassets.com");
    EXPECT_STREQ(header.getHeaderValue("user-agent"), "Mozilla/5.0 (X11; Linux x86_64; rv:121.0) Gecko/20100101 Firefox/121.0");
    EXPECT_STREQ(header.getHeaderValue("Accept"), "*/*");
    EXPECT_STREQ(header.getHeaderValue("Accept-Language"), "en-US,en;q=0.5");
    EXPECT_STREQ(header.getHeaderValue("Accept-Encoding"), "gzip, deflate, br");
    EXPECT_STREQ(header.getHeaderValue("Referer"), "https://github.com/");
    EXPECT_STREQ(header.getHeaderValue("Origin"), "https://github.com");
    EXPECT_STREQ(header.getHeaderValue("Connection"), "keep-alive");
    EXPECT_STREQ(header.getHeaderValue("Sec-Fetch-Dest"), "script");
    EXPECT_STREQ(header.getHeaderValue("Sec-Fetch-Mode"), "cors");
    EXPECT_STREQ(header.getHeaderValue("Sec-Fetch-Site"), "cross-site");

    delete[] buffer;
}

TEST(HTTPHeader, IncompleteHeader) {

    HTTPHeader header;
    
    const char *request = "GET /assets/chunk-node_modules_scroll-anchoring_dist_scroll-anchoring_esm_js-app_components_notifications_notif-fce33d-ecb3639d56ad.js HTTP/1.1\r\n"
        "Host: github.githubassets.com\r\n"
        "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:121.0) Gecko/20100101 Firefox/121.0\r\n"
        "Accept: */*\r\n"
        "Accept-Language: en-US,en;q=0.5\r\n"
        "Accept-Encoding: gzip, deflate, br\r\n"
        "Referer: https://github.com/\r\n"
        "Origin: https://github.com\r\n"
        "Connection: keep-alive\r\n"
        "Sec-Fetch-Dest: script\r\n"
        "Sec-Fetch-Mode: cors\r\n"
        "Sec-Fetch-Site: cross-site\r\n";

    int buffer_size=strlen(request);
    char *buffer = new char[buffer_size];

    memcpy(buffer, request, buffer_size);

    EXPECT_EQ(false, header.parse(buffer, buffer_size));
    delete[] buffer;
}

TEST(HTTPHeader, FirstLineIncomplete) {

    HTTPHeader header;
    
    const char *request = "GET /assets/chunk-node_modules_scroll-anchoring_dist_scroll-anchoring_esm_js-app_components_notifications_notif-fce33d-ecb3639d56ad.js HTTP/1.1";
    int buffer_size=strlen(request);
    char *buffer = new char[buffer_size];

    memcpy(buffer, request, buffer_size);

    EXPECT_EQ(false, header.parse(buffer, buffer_size));
    delete[] buffer;
}

TEST(HTTPHeader, SimpleRequest) {

    HTTPHeader header;
    
    const char *request = "GET / HTTP/1.1\r\n\r\n";
    int buffer_size=strlen(request);
    char *buffer = new char[buffer_size];

    memcpy(buffer, request, buffer_size);

    EXPECT_EQ(true, header.parse(buffer, buffer_size));

    EXPECT_STREQ(header.getCommand(), "GET");
    EXPECT_STREQ(header.getPath(), "/");
    EXPECT_EQ(0, header.getNumHeaders());
    
    delete[] buffer;
}

TEST(HTTPHeader, SimpleRequestWithExtraData) {

    HTTPHeader header;
    
    const char *request = "GET / HTTP/1.1\r\n\r\nextradata";
    int buffer_size=strlen(request)+1;
    char *buffer = new char[buffer_size];

    memcpy(buffer, request, buffer_size);

    EXPECT_EQ(true, header.parse(buffer, buffer_size));

    EXPECT_STREQ(header.getCommand(), "GET");
    EXPECT_STREQ(header.getPath(), "/");
    EXPECT_EQ(0, header.getNumHeaders());
    
    EXPECT_STREQ(buffer + header.getHeaderSize(), "extradata");
    
    delete[] buffer;
}

TEST(HTTPHeader, SimplePostRequest) {

    HTTPHeader header;
    
    const char *request = "POST / HTTP/1.1\r\nContent-Length: 10\r\n\r\n0123456789";
    int buffer_size=strlen(request)+1;
    char *buffer = new char[buffer_size];

    memcpy(buffer, request, buffer_size);

    EXPECT_EQ(true, header.parse(buffer, buffer_size));

    EXPECT_STREQ(header.getCommand(), "POST");
    EXPECT_STREQ(header.getPath(), "/");
    EXPECT_EQ(1, header.getNumHeaders());
    EXPECT_STREQ("10", header.getHeaderValue("Content-Length"));
    
    EXPECT_STREQ(buffer + header.getHeaderSize(), "0123456789");
    
    delete[] buffer;
}
