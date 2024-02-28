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
#include <algorithm>
#include <string.h>
#include <time.h>

#include "websocket_handler.h"

extern "C" void trace(const char *parameters, ...);
extern "C" const char *safestr(const char *value);

bool WebSocketHandler::decodeData(uint8_t* data, size_t len, WebSocketInterface *callback)
{
    while (len > 0)
    {
        if (m_state == WebSocketState::WAIT_PACKET)
        {
            uint16_t bytesToCopy = std::min(len, (size_t)(MAX_WEBSOCKET_HEADER - m_bufferIndex));
            memcpy(&m_buffer[m_bufferIndex], data, bytesToCopy);
            
            m_bufferIndex += bytesToCopy;
            if (m_bufferIndex < 2)
            {
                return true;
            }
            
            m_fin = m_buffer[0] & 0x80;
            m_webSocketOperation = (WebSocketOperation) (m_buffer[0] & 0x0f);
            m_mask = m_buffer[1] & 0x80;
            uint16_t payload_len = (m_buffer[1] & 0x7f);

            uint16_t websocketHeaderSize = 2 + (payload_len == 126 ? 2 : 0) + (payload_len == 127 ? 8 : 0) + (m_mask ? 4 : 0);
            if (m_bufferIndex < websocketHeaderSize)
            {
                trace("WebSocketHandler::decodeData: this=%p, incomplete websocket header[%d] exected[%d]\n", this, m_bufferIndex, websocketHeaderSize);
                return true; 
            }

            int extraBytes = bytesToCopy - (m_bufferIndex - websocketHeaderSize);
            data += extraBytes;
            len -= extraBytes;

            int i = 0;
            if (payload_len < 126)
            {
                m_webSocketDataLen = payload_len;
                i = 2;
            }
            else if (payload_len == 126)
            {
                m_webSocketDataLen = (((uint64_t) m_buffer[2]) << 8) | ((uint64_t) m_buffer[3]);
                i = 4;
            }
            else if (payload_len == 127)
            {
                m_webSocketDataLen = (((uint64_t) m_buffer[2]) << 56) | (((uint64_t) m_buffer[3]) << 48) | (((uint64_t) m_buffer[4]) << 40) | (((uint64_t) m_buffer[5]) << 32) | (((uint64_t) m_buffer[6]) << 24) | (((uint64_t) m_buffer[7]) << 16) | (((uint64_t) m_buffer[8]) << 8) | ((uint64_t) m_buffer[9]);
                i = 10;
            }
        
            if (m_mask)
            {
                m_maskingKey[0] = ((uint8_t) m_buffer[i+0]) << 0;
                m_maskingKey[1] = ((uint8_t) m_buffer[i+1]) << 0;
                m_maskingKey[2] = ((uint8_t) m_buffer[i+2]) << 0;
                m_maskingKey[3] = ((uint8_t) m_buffer[i+3]) << 0;
            }
            else
            {
                m_maskingKey[0] = 0;
                m_maskingKey[1] = 0;
                m_maskingKey[2] = 0;
                m_maskingKey[3] = 0;
            }

            trace("HTTPSession::decodeWebSocketData: this=%p, headerSize[%d] len[%d] extraBytesCached[%d] m_webSocketDataLen[%d]", this,
                  websocketHeaderSize, len, (bytesToCopy - (m_bufferIndex - websocketHeaderSize)), m_webSocketDataLen);

            m_webSocketDataIndex = 0;
            m_state = WebSocketState::WAIT_DATA;
            m_bufferIndex = 0;
        }
        else if (m_state == WebSocketState::WAIT_DATA)
        {
            if ((m_webSocketOperation == WebSocketOperation::TEXT_FRAME) || (m_webSocketOperation == WebSocketOperation::BINARY_FRAME) || (m_webSocketOperation == WebSocketOperation::CONTINUATION_FRAME))
            {
                uint32_t i=0;
                for (; (i<len) && (m_webSocketDataIndex < m_webSocketDataLen);++i,++m_webSocketDataIndex)
                {
                    if (m_mask)
                    {
                        data[i] ^= m_maskingKey[m_webSocketDataIndex&0x3];
                    }
                }

                if (!callback->onWebSocketData(data, i))
                {
                    return false;
                }

                len-=i;
                data+=i;

                if (m_webSocketDataIndex == m_webSocketDataLen)
                {
                    m_state = WebSocketState::WAIT_PACKET;
                }
            }
            else
            {
                return false;
            }
        }
        else
        {
            return false;
        }
    }

    return true;

}


bool WebSocketHandler::encodeData(const uint8_t* data, size_t len, WebSocketInterface *callback)
{
    uint8_t buffer[MAX_WEBSOCKET_HEADER] = {0};
    uint16_t headerSize = 0;

    // FIN + operation
    buffer[0] = 0x80 | WebSocketOperation::BINARY_FRAME;
    buffer[1] |= 0x80;
    
    if (len < 126)
    {
        buffer[1] |= len;
        headerSize += 2;
    }
    else if (len <= 0xffff)
    {
        buffer[1] |= 126;
        buffer[2] = (len >> 8) & 0xff;
        buffer[3] = len & 0xff;
        headerSize += 4;
    }
    else
    {
        buffer[1] |= 127;
        buffer[2] = 0;
        buffer[3] = 0;
        buffer[4] = 0;
        buffer[5] = 0;
        buffer[6] = (len >> 24) & 0xff;
        buffer[7] = (len >> 16) & 0xff;
        buffer[8] = (len >> 8) & 0xff;
        buffer[9] = (len) & 0xff;
        headerSize += 10;
    }

    // Empty mask for now
    headerSize+=4;
    
    // 4 mask bytes if needed
    if (!callback->onWebsocketEncodedData(&buffer[0], headerSize))
    {
        return false;
    }
    
    return callback->onWebsocketEncodedData(data, len);
}

