#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <stdarg.h>

#include "pico_simple_mqtt/mqtt_handler.h"

using ::testing::_;

class MockMQTTSocketInterface : public MQTTSocketInterface
{
public:
    MOCK_METHOD(bool, on_pub_ack, (uint16_t message_id), (override));
    MOCK_METHOD(bool, on_sub_ack, (uint16_t message_id), (override));
    MOCK_METHOD(bool, on_conn_ack, (conn_ack_code_t code, uint8_t flags), (override));
    
    MOCK_METHOD(bool, on_ping_req, (), (override));
    MOCK_METHOD(bool, on_ping_resp, (), (override));

    MOCK_METHOD(bool, on_publish_header, (uint8_t *topic, uint16_t topic_length, uint16_t message_id, uint32_t message_length), (override));
    MOCK_METHOD(bool, on_publish_data, (uint8_t *data, uint32_t len, bool last_chunk), (override));

    MOCK_METHOD(bool, on_sent, (uint16_t len), (override));
    MOCK_METHOD(void, on_closed, (), (override));
    MOCK_METHOD(void, on_connected, (), (override));
};

TEST(MQTTSocketHandler, ConnAck) {

    unsigned char conn_ack[] = {0x20, 0x2, 0x0, 0x0};

    MockMQTTSocketInterface mockListener;
    
    EXPECT_CALL(mockListener, on_conn_ack(_,_))
        .WillOnce(testing::WithArgs<0, 1>([](conn_ack_code_t code, uint8_t flags) {
            EXPECT_EQ(flags, 0);
            EXPECT_EQ(code, 0);
            return true;
        }));
            

    MQTTSocketHandler handler;
    handler.set_upstream(&mockListener);
    
    handler.on_recv(conn_ack, sizeof(conn_ack));
}

TEST(MQTTSocketHandler, SimplePublish) {

    unsigned char bytes[] = {0x31, 0x1e, 0x0, 0x17, 0x67, 0x61, 0x72, 0x64, 0x65, 0x6e, 0x2f, 0x64, 0x6f, 0x77, 0x6e, 0x73, 0x74, 0x72, 0x65, 0x61, 0x6d, 0x2f, 0x74, 0x65, 0x73, 0x74, 0x33, 0x74, 0x65, 0x73, 0x74, 0x33};

    MockMQTTSocketInterface mockListener;
    
    EXPECT_CALL(mockListener, on_publish_header(_,_,_,_))
        .WillOnce(testing::WithArgs<0, 1, 2, 3>([](uint8_t *topic, uint16_t topic_length, uint16_t message_id, uint32_t message_length) {
            unsigned char topic_message[] = "garden/downstream/test3";

            EXPECT_EQ(topic_length, sizeof(topic_message)-1);
            EXPECT_EQ(memcmp(topic_message, topic, topic_length), 0);
            EXPECT_EQ(message_length, 5);
            return true;
        }));
            
    EXPECT_CALL(mockListener, on_publish_data(_,_,_))
        .WillOnce(testing::WithArgs<0, 1, 2>([](uint8_t *data, uint32_t len, bool last_chunk) {
            unsigned char expected[] = "test3";
            
            EXPECT_EQ(last_chunk, 1);
            EXPECT_EQ(memcmp(data, expected, len), 0);
            return true;
        }));

    MQTTSocketHandler handler;
    handler.set_upstream(&mockListener);
    
    handler.on_recv(bytes, sizeof(bytes));
}

TEST(MQTTSocketHandler, UpgradeTest) {

    // Full test with a firmware upgrade
    unsigned char header[] = {0x31, 0x9b, 0xf8, 0x34, 0x0, 0x19, 0x67, 0x61, 0x72, 0x64, 0x65, 0x6e, 0x2f, 0x64, 0x6f, 0x77, 0x6e, 0x73, 0x74, 0x72, 0x65, 0x61, 0x6d, 0x2f, 0x75, 0x70, 0x67, 0x72, 0x61, 0x64, 0x65};
    uint64_t header_size = sizeof(header);
    uint32_t message_len = 867359;

    uint8_t *buffer = new uint8_t[message_len];

    memcpy(buffer, header, header_size);
    for (int i=header_size;i<message_len;++i)
    {
        buffer[i] = static_cast<uint8_t>(i);
    }
    
    MockMQTTSocketInterface mockListener;
    
    EXPECT_CALL(mockListener, on_publish_header(_,_,_,_))
        .WillOnce(testing::WithArgs<0, 1, 2, 3>([header_size, message_len](uint8_t *topic, uint16_t topic_length, uint16_t message_id, uint32_t message_length) {
            unsigned char topic_message[] = "garden/downstream/upgrade";

            EXPECT_EQ(topic_length, sizeof(topic_message)-1);
            EXPECT_EQ(memcmp(topic_message, topic, topic_length), 0);

            // message_length is the body length
            EXPECT_EQ(message_length, message_len - header_size);
            return true;
        }));

    uint64_t position = header_size;
    EXPECT_CALL(mockListener, on_publish_data(_,_,_))
        .WillRepeatedly(testing::WithArgs<0, 1, 2>([&position, buffer, message_len](uint8_t *data, uint32_t len, bool last_chunk) {
            EXPECT_EQ(memcmp(data, &buffer[position], len), 0);
            position+=len;

            bool expect_last_chunk = (position == message_len);
            EXPECT_EQ(last_chunk, expect_last_chunk);
            
            return true;
        }));

    MQTTSocketHandler handler;
    handler.set_upstream(&mockListener);
    
    handler.on_recv(buffer, message_len);

    delete[] buffer;
}



TEST(MQTTSocketHandler, fragmentedTest) {

    // Full test with a firmware upgrade
    unsigned char header[] = {0x31, 0x9b, 0xf8, 0x34, 0x0, 0x19, 0x67, 0x61, 0x72, 0x64, 0x65, 0x6e, 0x2f, 0x64, 0x6f, 0x77, 0x6e, 0x73, 0x74, 0x72, 0x65, 0x61, 0x6d, 0x2f, 0x75, 0x70, 0x67, 0x72, 0x61, 0x64, 0x65};
    uint64_t header_size = sizeof(header);
    uint32_t message_len = 867359;

    uint8_t *buffer = new uint8_t[message_len];

    memcpy(buffer, header, header_size);
    for (int i=header_size;i<message_len;++i)
    {
        buffer[i] = static_cast<uint8_t>(i);
    }
    
    MockMQTTSocketInterface mockListener;
    
    EXPECT_CALL(mockListener, on_publish_header(_,_,_,_))
        .WillOnce(testing::WithArgs<0, 1, 2, 3>([header_size, message_len](uint8_t *topic, uint16_t topic_length, uint16_t message_id, uint32_t message_length) {
            unsigned char topic_message[] = "garden/downstream/upgrade";

            EXPECT_EQ(topic_length, sizeof(topic_message)-1);
            EXPECT_EQ(memcmp(topic_message, topic, topic_length), 0);

            // message_length is the body length
            EXPECT_EQ(message_length, message_len - header_size);
            return true;
        }));

    uint64_t position = header_size;
    EXPECT_CALL(mockListener, on_publish_data(_,_,_))
        .WillRepeatedly(testing::WithArgs<0, 1, 2>([&position, buffer, message_len](uint8_t *data, uint32_t len, bool last_chunk) {
            EXPECT_EQ(memcmp(data, &buffer[position], len), 0);
            position+=len;

            bool expect_last_chunk = (position == message_len);
            EXPECT_EQ(last_chunk, expect_last_chunk);
            
            return true;
        }));

    MQTTSocketHandler handler;
    handler.set_upstream(&mockListener);

    for (int i=0;i<message_len;++i)
    {
        handler.on_recv(&buffer[i], 1);
    }

    delete[] buffer;
}

TEST(MQTTSocketHandler, fragmentedChunkTest) {

    // Full test with a firmware upgrade
    unsigned char header[] = {0x31, 0x9b, 0xf8, 0x34, 0x0, 0x19, 0x67, 0x61, 0x72, 0x64, 0x65, 0x6e, 0x2f, 0x64, 0x6f, 0x77, 0x6e, 0x73, 0x74, 0x72, 0x65, 0x61, 0x6d, 0x2f, 0x75, 0x70, 0x67, 0x72, 0x61, 0x64, 0x65};
    uint64_t header_size = sizeof(header);
    uint32_t message_len = 867359;

    uint8_t *buffer = new uint8_t[message_len];

    memcpy(buffer, header, header_size);
    for (int i=header_size;i<message_len;++i)
    {
        buffer[i] = static_cast<uint8_t>(i);
    }
    
    MockMQTTSocketInterface mockListener;
    
    EXPECT_CALL(mockListener, on_publish_header(_,_,_,_))
        .WillOnce(testing::WithArgs<0, 1, 2, 3>([header_size, message_len](uint8_t *topic, uint16_t topic_length, uint16_t message_id, uint32_t message_length) {
            unsigned char topic_message[] = "garden/downstream/upgrade";

            EXPECT_EQ(topic_length, sizeof(topic_message)-1);
            EXPECT_EQ(memcmp(topic_message, topic, topic_length), 0);

            // message_length is the body length
            EXPECT_EQ(message_length, message_len - header_size);
            return true;
        }));

    uint64_t position = header_size;
    
    EXPECT_CALL(mockListener, on_publish_data(_,_,_))
        .WillRepeatedly(testing::WithArgs<0, 1, 2>([&position, buffer, message_len](uint8_t *data, uint32_t len, bool last_chunk) {
            EXPECT_EQ(memcmp(data, &buffer[position], len), 0);
            position+=len;

            bool expect_last_chunk = (position == message_len);
            EXPECT_EQ(last_chunk, expect_last_chunk);
            
            return true;
        }));

    MQTTSocketHandler handler;
    handler.set_upstream(&mockListener);

    uint32_t chunkSize = 1500;
    uint8_t *test_buffer = new uint8_t[chunkSize];
    
    for (int i=0;i<message_len;i=i+chunkSize)
    {
        uint32_t copy_size = std::min(message_len - i, chunkSize);
        memcpy(test_buffer, &buffer[i], copy_size);
        handler.on_recv(&buffer[i], copy_size);
    }

    delete[] test_buffer;
    delete[] buffer;
}
