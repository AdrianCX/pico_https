#ifndef WEBSOCKET_HANDLER_H
#define WEBSOCKET_HANDLER_H

#ifdef __TARGET_CPU_CORTEX_M0PLUS
#include "pico/stdlib.h"
#else
#include "stdlib.h"
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
};

class WebSocketReceiver
{
public:
    WebSocketReceiver() {};

    ///
    /// This will decode messages in-place in provided data and trigger callback with them.
    /// It will cache any remaining bytes that are part of a next message header.
    ///
    /// @returns - true - if all data consumed succesfully.
    ///          - false - on failure, connection must be closed.
    ///
    bool decodeData(uint8_t* data, size_t len, WebSocketInterface *callback);

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