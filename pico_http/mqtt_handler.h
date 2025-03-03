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
#pragma once

#ifdef __TARGET_CPU_CORTEX_M0PLUS
#include "pico/stdlib.h"
#else
#include "stdlib.h"
#include <cstdint>
#endif

#include "session.h"

#define MQTTQOS0 0x00
#define MQTTQOS1 0x02
#define MQTTQOS2 0x04
#define MQTTRETAIN 0x01

#define MQTT_MAX_HEADER_SIZE 5
#define MQTT_BUFFER_SIZE 256

enum class SocketState
{
    WAIT_PACKET,
    WAIT_DATA,
};

enum conn_ack_code_t
{
    CONNECTION_ACCEPTED = 0x00,
    UNACCEPTABLE_PROTOCOL = 0x01,
    IDENTIFIER_REJECTED = 0x02,
    SERVER_UNAVAILABLE = 0x03,
    BAD_USER_OR_PASS = 0x04,
    NOT_AUTHORIZED = 0x05,
};

enum mqtt_message_type_t
{
    MQTTCONNECT = 0x10,
    MQTTCONNACK = 0x20,
    MQTTPUBLISH = 0x30,
    MQTTPUBACK = 0x40,
    MQTTPUBREC = 0x50,
    MQTTPUBREL = 0x60,
    MQTTPUBCOMP = 0x70,
    MQTTSUBSCRIBE = 0x80,
    MQTTSUBACK = 0x90, 
    MQTTUNSUBSCRIBE = 0xA0,
    MQTTUNSUBACK = 0xB0,
    MQTTPINGREQ = 0xC0,
    MQTTPINGRESP = 0xD0,
    MQTTDISCONNECT = 0xE0, 
    MQTTRESERVED = 0xF0,
};

class MQTTSocketInterface
{
public:
    // Return true if handling successful, return false if you want processing to fail and lead to disconnect.
    virtual bool on_pub_ack(uint16_t message_id) { return true; }
    virtual bool on_sub_ack(uint16_t message_id) { return true; }
    virtual bool on_conn_ack(conn_ack_code_t code, uint8_t flags) { return true; }
    
    virtual bool on_ping_resp() { return true; }

    virtual bool on_publish_header(uint8_t *topic, uint16_t topic_length, uint16_t message_id, uint32_t message_length) { return true; }
    virtual bool on_publish_data(uint8_t *data, uint32_t len, bool last_chunk) { return true; }

    virtual bool on_sent(u16_t len) { return true; }
    virtual void on_closed() {};
    virtual void on_connected() {};
};

class MQTTSocketHandler
    : public ISessionCallback
{
public:
    MQTTSocketHandler() {};

    bool send_connect(bool clean_session, uint8_t keepalive_seconds, const char *id = NULL, const char *will_topic = NULL, const char *will_message = NULL, const char *user = NULL, const char *pass = NULL);
    bool send_subscribe(const char *topic);
    bool send_publish_header(const char *topic, uint32_t message_length);
    bool send_publish_data(uint8_t *data, size_t len);

    bool send_ping();

    void set_upstream(MQTTSocketInterface *upstream) { m_upstream = upstream; }
    void set_downstream(ISessionSender *downstream) { m_downstream = downstream; }

    virtual bool on_sent(u16_t len) override;
    virtual bool on_recv(u8_t *data, size_t len) override;
    virtual void on_closed() override;
    virtual void on_connected() override;

private:
    bool decode_data(uint8_t* data, size_t len);

    static uint16_t write_string(const char* msg, uint16_t len, uint8_t* buffer, uint16_t pos);
    static uint32_t write_header(uint8_t message_type, uint32_t message_size, uint8_t *buffer, size_t buffer_size, size_t laterBytes=0);
    
    uint8_t m_keepaliveSeconds = 0;
    uint16_t m_messageId = 1;
    uint16_t m_recvBufferIdx = 0;
    uint8_t m_recvBuffer[MQTT_BUFFER_SIZE];

    SocketState m_state = SocketState::WAIT_PACKET;
    uint64_t m_lastKeepaliveUs = 0;
    uint32_t m_pendingDataLen = 0;
    uint32_t m_pendingSendDataLen = 0;

    MQTTSocketInterface *m_upstream = NULL;
    ISessionSender *m_downstream = NULL;
};

