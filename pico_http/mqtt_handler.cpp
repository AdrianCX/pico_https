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

#include "pico/stdio.h"
#include "pico/time.h"
#include "mqtt_handler.h"

extern "C" void trace(const char *parameters, ...);
extern "C" const char *safestr(const char *value);

bool MQTTSocketHandler::on_recv(u8_t *data, size_t len)
{
    trace("MQTTSocketHandler::on_recv: data[%p], len[%d].", data, len);
    return decode_data(data, len);
}

bool MQTTSocketHandler::on_sent(u16_t len)
{
    return m_upstream->on_sent(len);
}

void MQTTSocketHandler::on_closed()
{
    m_upstream->on_closed();
}

void MQTTSocketHandler::on_connected()
{
    m_upstream->on_connected();
}


bool MQTTSocketHandler::decode_data(uint8_t* data, size_t len)
{
    while (len > 0)
    {
        if (m_state == SocketState::WAIT_PACKET)
        {
            uint16_t bytesToCopy = std::min(len, (size_t)(MQTT_BUFFER_SIZE - m_recvBufferIdx));
            memcpy(&m_recvBuffer[m_recvBufferIdx], data, bytesToCopy);
            
            m_recvBufferIdx += bytesToCopy;
            if (m_recvBufferIdx < 2)
            {
                return true;
            }

            uint32_t pos = 1;
            uint32_t length = 0;

            // determine message size
            for (;pos<5;++pos)
            {
                if (pos>=m_recvBufferIdx)
                {
                    return true;
                }

                if (m_recvBuffer[pos] == 5)
                {
                    trace("MQTTSocketHandler::decodeData: received disconnect message.");
                    return false;
                }

                length += (m_recvBuffer[pos] & 0x7F) << ((pos-1)*7);

                if ((m_recvBuffer[pos] & 0x80) == 0)
                {
                    ++pos;
                    break;
                }
            }

            uint32_t msg_size = length + pos;
            uint8_t message_type = m_recvBuffer[0] & 0xf0;
            //trace("MQTTSocketHandler::decode_data: message type[0x%x], size[%d] bufferIndex[%d] pos[%d].", message_type, msg_size, m_recvBufferIdx, pos);

            // Potentially large messages that we handle in chunks
            if (message_type == MQTTPUBLISH)
            {
                if (pos + 2 > m_recvBufferIdx)
                {
                    return true;
                }
    
                uint16_t topic_length = (m_recvBuffer[pos] << 8) + m_recvBuffer[pos+1];
                pos+=2;

                if (pos + topic_length >= MQTT_BUFFER_SIZE)
                {
                    trace("MQTTSocketHandler::decodeData: received message with header larger then local buffer, received header[%d], max header[%d]", pos + topic_length, MQTT_BUFFER_SIZE);
                    return false;
                }
                
                if (pos + topic_length > m_recvBufferIdx)
                {
                    return true;
                }
    
                uint8_t *topic = &m_recvBuffer[pos];
                pos += topic_length;
                
                if ((m_recvBuffer[0] & (MQTTQOS1 | MQTTQOS2)) && (pos + 2 > m_recvBufferIdx))
                {
                    return true;
                }
                
                uint16_t message_id = (m_recvBuffer[0] & (MQTTQOS1 | MQTTQOS2)) ? (m_recvBuffer[pos] << 8) + m_recvBuffer[pos+1] : 0;
                pos += (m_recvBuffer[0] & (MQTTQOS1 | MQTTQOS2)) ? 2 : 0;

                uint32_t message_length = msg_size - pos;

                m_upstream->on_publish_header(topic, topic_length, message_id, message_length);

                m_pendingDataLen = message_length;
                m_state = SocketState::WAIT_DATA;
                
                m_recvBufferIdx = 0;
                data += pos;
                len -= pos;
            }
            else
            {
                // Smaller messages should fit in the defined buffer.
                if (msg_size >= MQTT_BUFFER_SIZE)
                {
                    trace("MQTTSocketHandler::decodeData: received message with header larger then local buffer, msg_size[%d], max_header[%d], msg_type[%d]", msg_size, MQTT_BUFFER_SIZE, message_type);
                    return false;
                }

                if (msg_size > m_recvBufferIdx)
                {
                    return true;
                }

                if (message_type == MQTTCONNACK)
                {
                    if ((pos + 2 > m_recvBufferIdx))
                    {
                        return true;
                    }

                    uint8_t conn_ack_flags = m_recvBuffer[pos];
                    conn_ack_code_t code = (conn_ack_code_t)m_recvBuffer[pos+1];
                    pos += 2;
                    
                    m_upstream->on_conn_ack(code, conn_ack_flags);
                }
                else if ((message_type == MQTTSUBACK) || (message_type == MQTTPUBACK))
                {
                    if ((pos + 2 > m_recvBufferIdx))
                    {
                        return true;
                    }

                    uint16_t message_id = (m_recvBuffer[pos] << 8) + m_recvBuffer[pos+1];
                    pos += 2;
                    
                    if (message_type == MQTTPUBACK)
                    {
                        m_upstream->on_pub_ack(message_id);
                    }
                    else
                    {
                        m_upstream->on_sub_ack(message_id);
                    }
                }
                else if (message_type == MQTTPINGRESP)
                {
                    m_upstream->on_ping_resp();
                }
                else
                {
                    trace("mqtt::decode_message: Unknown message: %d, size: %d", m_recvBuffer[0], msg_size);
                }

                m_recvBufferIdx = 0;
                data += msg_size;
                len -= msg_size;
            }
        }
        else if (m_state == SocketState::WAIT_DATA)
        {
            //trace("MQTTSocketHandler::decode_data: received data this[%p] len[%d] m_pendingDataLen[%d].", this, len, m_pendingDataLen);
            
            uint32_t bytesReceived = len < m_pendingDataLen ? len : m_pendingDataLen;
            
            m_upstream->on_publish_data(data, bytesReceived, (len == m_pendingDataLen));

            len -= bytesReceived;
            m_pendingDataLen -= bytesReceived;

            if (m_pendingDataLen == 0)
            {
                m_state = SocketState::WAIT_PACKET;
            }
        }
    }

    return true;
}

bool MQTTSocketHandler::send_connect(bool clean_session, uint8_t keepalive_seconds, const char *id, const char *will_topic, const char *will_message, const char *user, const char *pass)
{
    if (m_pendingSendDataLen > 0)
    {
        trace("MQTTSocketHandler::send_connect: attempting to send another message while previous was not fully sent, disconnecting.");
        return false;
    }
    
    trace("MQTTSocketHandler::send_connect: clean_session[%d], keepalive_seconds[%d], id[%s] will_topic[%s], will_message[%s], user[%s].", clean_session, keepalive_seconds, safestr(id), safestr(will_topic), safestr(will_message), safestr(user));
    
    uint16_t id_len = (id != NULL) ? strlen(id) : 0;
    uint16_t will_topic_len = (will_topic != NULL) ? strlen(will_topic) : 0;
    uint16_t will_message_len = ((will_topic != NULL) && (will_message != NULL)) ? strlen(will_message) : 0;
    uint16_t user_len = (user != NULL) ? strlen(user) : 0;
    uint16_t pass_len = ((user != NULL) && (pass != NULL)) ? strlen(pass) : 0;
    
    uint32_t message_size = 7 + 1 + 2;

    message_size += (id_len > 0) ? (id_len + 2) : 0;
    message_size += (will_topic_len > 0) ? (2 + will_topic_len) : 0;
    message_size += (will_message_len > 0) ? (2 + will_message_len) : 0;
    message_size += (user_len > 0) ? (2 + user_len) : 0;
    message_size += (pass_len > 0) ? (2 + pass_len) : 0;

    if (message_size + MQTT_MAX_HEADER_SIZE > MQTT_BUFFER_SIZE)
    {
        trace("MQTTSocketHandler::send_connect: attempting to send a connect with message size larger then max local buffer, message size[%d], max header[%d]", message_size + MQTT_MAX_HEADER_SIZE, MQTT_BUFFER_SIZE);
        return false;
    }

    uint8_t sendBuffer[message_size + MQTT_MAX_HEADER_SIZE] = {0};
    
    uint32_t pos = write_header(MQTTCONNECT, message_size, sendBuffer, sizeof(sendBuffer));
    if (pos == 0)
    {
        trace("MQTTSocketHandler::send_connect: failed writing header");
        return false;
    }
    
    sendBuffer[pos++] = 0x00;
    sendBuffer[pos++] = 0x04;
    sendBuffer[pos++] = 'M';
    sendBuffer[pos++] = 'Q';
    sendBuffer[pos++] = 'T';
    sendBuffer[pos++] = 'T';
    sendBuffer[pos++] = 4;
        
    uint8_t v = 0;

    if (will_topic != NULL)
    {
        // will topic + will qos to 1 + will retain
        v=0x04|0x08|0x20;
    }

    if (clean_session)
    {
        v = v|0x02;
    }

    if(user != NULL)
    {
        v = v|0x80;

        if(pass != NULL)
        {
            v = v|(0x40);
        }
    }

    sendBuffer[pos++] = v;
    sendBuffer[pos++] = (keepalive_seconds >> 8);
    sendBuffer[pos++] = (keepalive_seconds & 0xFF);
    
    if (id == NULL)
    {
        trace("MQTTSocketHandler::send_connect: missing id for connect");
        return false;
    }

    pos = write_string(id, id_len, sendBuffer, pos);
    
    if (will_topic && will_message)
    {
        pos = write_string(will_topic, will_topic_len, sendBuffer, pos);
        pos = write_string(will_message, will_message_len, sendBuffer, pos);
    }

    if(user != NULL)
    {
        pos = write_string(user, user_len, sendBuffer, pos);
        if(pass != NULL)
        {
            pos = write_string(pass, pass_len, sendBuffer, pos);
        }
    }

    m_messageId = 1;
    m_lastKeepaliveUs = to_us_since_boot(get_absolute_time());
    m_keepaliveSeconds = keepalive_seconds;
    return m_downstream->send(sendBuffer, pos);
}

bool MQTTSocketHandler::send_subscribe(const char *topic)
{
    if (m_pendingSendDataLen > 0)
    {
        trace("MQTTSocketHandler::send_subscribe: attempting to send another message while previous was not fully sent, disconnecting.");
        return false;
    }
    
    trace("MQTTSocketHandler::send_subscribe: topic[%s]", safestr(topic));

    const int MQTT_MESSAGE_ID_SIZE = 2;
    const int MQTT_TOPIC_LENGTH_SIZE = 2;

    uint16_t topic_length = strlen(topic);
    
    uint32_t message_size = MQTT_TOPIC_LENGTH_SIZE + topic_length + MQTT_MESSAGE_ID_SIZE + 1;
    if (message_size + MQTT_MAX_HEADER_SIZE > MQTT_BUFFER_SIZE)
    {
        trace("MQTTSocketHandler::send_subscribe: attempting to send a connect with message size larger then local buffer, message size[%d], max header[%d]", message_size + MQTT_MAX_HEADER_SIZE, MQTT_BUFFER_SIZE);
        return false;
    }

    uint8_t sendBuffer[message_size + MQTT_MAX_HEADER_SIZE];
    uint32_t pos = write_header(MQTTSUBSCRIBE|MQTTQOS1, message_size, sendBuffer, sizeof(sendBuffer));
    if (pos == 0)
    {
        trace("MQTTSocketHandler::send_subscribe: failed writing header");
        return false;
    }

    sendBuffer[pos++] = m_messageId >> 8;
    sendBuffer[pos++] = m_messageId & 0xFF;

    pos = write_string(topic, topic_length, sendBuffer, pos);

    const int qos = 1;
    sendBuffer[pos++] = qos;

    
    m_messageId = (m_messageId == 0xFFFF) ? 1 : (m_messageId + 1);
    
    m_lastKeepaliveUs = to_us_since_boot(get_absolute_time());
    
    return m_downstream->send(sendBuffer, pos);
}

bool MQTTSocketHandler::send_publish_header(const char *topic, uint32_t message_length)
{
    if (m_pendingSendDataLen > 0)
    {
        trace("MQTTSocketHandler::send_publish_header: attempting to send another message while previous was not fully sent, disconnecting.");
        return false;
    }

    trace("MQTTSocketHandler::send_publish_header: topic[%s], message_length[%d]", safestr(topic), message_length);
    
    const int MQTT_MESSAGE_ID_SIZE = 2;
    const int MQTT_TOPIC_LENGTH_SIZE = 2;

    uint16_t topic_length = strlen(topic);

    uint32_t header_size = MQTT_TOPIC_LENGTH_SIZE + topic_length + MQTT_MESSAGE_ID_SIZE;
    uint32_t message_size = header_size + message_length;

    uint8_t sendBuffer[header_size + MQTT_MAX_HEADER_SIZE] = {0};
    uint32_t pos = write_header(MQTTPUBLISH|MQTTQOS1|MQTTRETAIN, message_size, sendBuffer, sizeof(sendBuffer), message_length);

    if (pos == 0)
    {
        return -1;
    }
    
    sendBuffer[pos++] = (topic_length >> 8);
    sendBuffer[pos++] = (topic_length & 0xFF);

    memcpy(&sendBuffer[pos], topic, topic_length);
    pos+=topic_length;

    sendBuffer[pos++] = m_messageId >> 8;
    sendBuffer[pos++] = m_messageId & 0xFF;

    m_pendingSendDataLen = message_length;

    m_messageId = (m_messageId == 0xFFFF) ? 1 : (m_messageId + 1);
    m_lastKeepaliveUs = to_us_since_boot(get_absolute_time());
    
    return m_downstream->send(sendBuffer, pos);
}

bool MQTTSocketHandler::send_publish_data(uint8_t *data, size_t len)
{
    if (m_pendingSendDataLen < len)
    {
        trace("MQTTSocketHandler::send_publish_data: attempting to send more message data[%d] then remaining[%d], disconnecting.", m_pendingSendDataLen, len);
        return false;
    }
    trace("MQTTSocketHandler::send_publish_data: len[%d]", len);
    
    m_pendingSendDataLen -= len;
    return m_downstream->send(data, len);
}

bool MQTTSocketHandler::send_ping()
{
    if (m_pendingSendDataLen > 0)
    {
        return true;
    }

    uint64_t now_us = to_us_since_boot(get_absolute_time());
    if ((now_us - m_lastKeepaliveUs)/1000000LL < m_keepaliveSeconds)
    {
        return true;
    }
    
    trace("MQTTSocketHandler::send_ping: ");
    
    uint32_t message_size = 0;
    uint8_t sendBuffer[MQTT_MAX_HEADER_SIZE] = {0};

    uint32_t pos = write_header(MQTTPINGREQ, message_size, sendBuffer, MQTT_BUFFER_SIZE);

    m_lastKeepaliveUs = to_us_since_boot(get_absolute_time());
    return m_downstream->send(sendBuffer, pos);
}


uint16_t MQTTSocketHandler::write_string(const char* msg, uint16_t len, uint8_t* buffer, uint16_t pos)
{    
    buffer[pos++] = len >> 8;
    buffer[pos++] = len & 0xff;
    memcpy(&buffer[pos], msg, len);
    pos+=len;
    
    return pos;
}

uint32_t MQTTSocketHandler::write_header(uint8_t message_type, uint32_t message_size, uint8_t *buffer, size_t buffer_size, size_t laterBytes)
{
    if (message_size - laterBytes + 5 > buffer_size)
    {
        trace("mqtt::write_header: not enough space, message_size[%d] buffer_size[%d], message_size, buffer_size", message_size - laterBytes + 5, buffer_size);
        return 0;
    }

    if ((message_size>>28)>0)
    {
        trace("mqtt::write_header: mqtt message_size is too large to encode, message_size[%d].", message_size);
        return 0;
    }

    uint32_t pos = 0;
    buffer[pos++] = message_type;
    
    buffer[pos++] = message_size & 0x7f;
    if ((message_size>>7)>0)
    {
        buffer[pos-1] |= 0x80;
        buffer[pos++] = (message_size>>7) & 0x7f;
    }
    if ((message_size>>14)>0)
    {
        buffer[pos-1] |= 0x80;
        buffer[pos++] = (message_size>>14) & 0x7f;
    }
    if ((message_size>>21)>0)
    {
        buffer[pos-1] |= 0x80;
        buffer[pos++] = (message_size>>21) & 0x7f;
    }

    return pos;
}
