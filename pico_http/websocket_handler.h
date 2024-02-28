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
#ifndef WEBSOCKET_HANDLER_H
#define WEBSOCKET_HANDLER_H

#ifdef __TARGET_CPU_CORTEX_M0PLUS
#include "pico/stdlib.h"
#else
#include "stdlib.h"
#include <cstdint>
#endif

#define MAX_WEBSOCKET_HEADER 16

enum WebSocketState
{
    WAIT_PACKET,
    WAIT_DATA,
};

enum WebSocketOperation
{
    CONTINUATION_FRAME = 0x00,
    TEXT_FRAME         = 0x01,
    BINARY_FRAME       = 0x02,
    CONNECTION_CLOSE   = 0x08,
    PING               = 0x09,
    PONG               = 0x0A,
};

class WebSocketInterface
{
public:
    virtual bool onWebSocketData(uint8_t *data, size_t len) = 0;
    virtual bool onWebsocketEncodedData(const uint8_t *data, size_t len) = 0;
};

class WebSocketHandler
{
public:
    WebSocketHandler() {};

    ///
    /// This will decode messages in-place in provided data and trigger callback with them.
    /// It will cache any remaining bytes that are part of a next message header.
    ///
    /// @returns - true - if all data consumed succesfully.
    ///          - false - on failure, connection must be closed.
    ///
    bool decodeData(uint8_t* data, size_t len, WebSocketInterface *callback);

    ///
    /// This will encode data in a message and call back with bytes to send.
    ///
    /// @returns - true - if all data processed succesfully.
    ///          - false - on failure, connection must be closed.
    ///
    bool encodeData(const uint8_t* data, size_t len, WebSocketInterface *callback);

private:
    bool m_fin = false;
    bool m_mask = false;
    WebSocketOperation m_webSocketOperation;
    uint64_t m_webSocketDataLen = 0;
    uint64_t m_webSocketDataIndex = 0;
    uint8_t m_maskingKey[4];

    WebSocketState m_state = WebSocketState::WAIT_PACKET;
    uint16_t m_bufferIndex = 0;
    uint8_t m_buffer[MAX_WEBSOCKET_HEADER];
};

#endif