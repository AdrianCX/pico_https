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
#ifndef PICO_NONE_CLIENT_H
#define PICO_NONE_CLIENT_H

#include "pico/cyw43_arch.h"

#include "session.h"

class TCPClient : public ISessionCallback
{
public:
    TCPClient();
    virtual ~TCPClient();

    // 
    // Start up a regular Listener that calls 'factory' on each new accepted client.
    //
    bool connect(const char *host, u16_t port);
    bool connect(const char *host, const ip_addr_t *ipaddr, u16_t port);

    virtual void on_sent(u16_t len) override;
    virtual void on_recv(u8_t *data, size_t len) override;
    virtual void on_closed() override;
    virtual void on_connected();

    static int get_num_clients() { return NUM_CLIENTS; }
    
private:
    static void dns_callback(const char* hostname, const ip_addr_t *ipaddr, void *arg);

    static int NUM_CLIENTS;

    u16_t m_port;
    std::shared_ptr<Session> m_session;
};

#endif