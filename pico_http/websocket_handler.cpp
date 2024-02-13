#include <algorithm>
#include <string.h>
#include <time.h>

#include "websocket_handler.h"
#include "pico_logger.h"

bool WebSocketReceiver::decodeData(uint8_t* data, size_t len, WebSocketInterface *callback)
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
                trace("WebSocketReceiver::decodeData: this=%p, incomplete websocket header[%d] exected[%d]\n", this, m_bufferIndex, websocketHeaderSize);
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

                if (!callback->onWebSocketData(data, len))
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
