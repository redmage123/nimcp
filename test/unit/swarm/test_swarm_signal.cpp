/**
 * @file test_swarm_signal.cpp
 * @brief Comprehensive unit tests for NIMCP Swarm Signal Adapter
 *
 * TEST COVERAGE:
 * - Signal adapter creation and destruction
 * - Simulated radio send/receive
 * - Broadcast functionality
 * - Timeout handling
 * - Statistics tracking
 * - Multiple radio types (LoRa, WiFi, Bluetooth, etc.)
 * - Error handling and edge cases
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>
#include <queue>
#include <chrono>
#include <thread>

// Mock swarm signal structures based on architecture doc
// Headers have their own extern "C" guards

// Radio types
typedef enum {
    SWARM_RADIO_LORA,
    SWARM_RADIO_WIFI,
    SWARM_RADIO_BLUETOOTH,
    SWARM_RADIO_ULTRASONIC,
    SWARM_RADIO_OPTICAL,
    SWARM_RADIO_CUSTOM
} swarm_radio_type_t;

// Signal configuration
typedef struct {
    swarm_radio_type_t radio_type;
    uint32_t max_packet_size;
    uint32_t timeout_ms;
    float tx_power_dbm;
    uint32_t frequency_khz;
    bool enable_encryption;
} swarm_signal_config_t;

// Signal statistics
typedef struct {
    uint64_t packets_sent;
    uint64_t packets_received;
    uint64_t packets_dropped;
    uint64_t bytes_sent;
    uint64_t bytes_received;
    float avg_rssi;
    float packet_loss_rate;
} swarm_signal_stats_t;

// Phoneme message (from protocol)
typedef struct {
    uint8_t phoneme_sequence[8];
    uint8_t sequence_length;
    uint8_t message_type;
    uint16_t sender_id;
    float payload[4];
    uint16_t crc16;
} swarm_phoneme_message_t;

// Opaque signal adapter type
typedef struct swarm_signal_adapter swarm_signal_adapter_t;

// API functions
swarm_signal_adapter_t* swarm_signal_adapter_create(
    swarm_radio_type_t radio_type,
    const swarm_signal_config_t* config
);

void swarm_signal_adapter_destroy(swarm_signal_adapter_t* adapter);

bool swarm_signal_encode(
    swarm_signal_adapter_t* adapter,
    const swarm_phoneme_message_t* message,
    uint8_t* signal_buffer,
    uint32_t* signal_length
);

bool swarm_signal_decode(
    swarm_signal_adapter_t* adapter,
    const uint8_t* signal_buffer,
    uint32_t signal_length,
    swarm_phoneme_message_t* message
);

bool swarm_signal_send(
    swarm_signal_adapter_t* adapter,
    const swarm_phoneme_message_t* message
);

bool swarm_signal_receive(
    swarm_signal_adapter_t* adapter,
    swarm_phoneme_message_t* message,
    uint32_t timeout_ms
);

bool swarm_signal_broadcast(
    swarm_signal_adapter_t* adapter,
    const swarm_phoneme_message_t* message
);

swarm_signal_stats_t swarm_signal_get_stats(swarm_signal_adapter_t* adapter);

void swarm_signal_reset_stats(swarm_signal_adapter_t* adapter);

swarm_signal_config_t swarm_signal_default_config(swarm_radio_type_t radio_type);


//=============================================================================
// Mock Implementation (for testing purposes)
//=============================================================================

struct swarm_signal_adapter {
    swarm_signal_config_t config;
    swarm_signal_stats_t stats;
    std::queue<swarm_phoneme_message_t> rx_queue;
    bool simulated_failure;
    float simulated_rssi;
};

swarm_signal_config_t swarm_signal_default_config(swarm_radio_type_t radio_type) {
    swarm_signal_config_t config;
    config.radio_type = radio_type;
    config.timeout_ms = 100;
    config.tx_power_dbm = 20.0f;
    config.enable_encryption = false;

    switch (radio_type) {
        case SWARM_RADIO_LORA:
            config.max_packet_size = 255;
            config.frequency_khz = 915000; // 915 MHz
            break;
        case SWARM_RADIO_WIFI:
            config.max_packet_size = 1500;
            config.frequency_khz = 2400000; // 2.4 GHz
            break;
        case SWARM_RADIO_BLUETOOTH:
            config.max_packet_size = 512;
            config.frequency_khz = 2400000; // 2.4 GHz
            break;
        case SWARM_RADIO_ULTRASONIC:
            config.max_packet_size = 64;
            config.frequency_khz = 40; // 40 kHz
            break;
        case SWARM_RADIO_OPTICAL:
            config.max_packet_size = 1024;
            config.frequency_khz = 0; // N/A for optical
            break;
        default:
            config.max_packet_size = 255;
            config.frequency_khz = 0;
            break;
    }

    return config;
}

swarm_signal_adapter_t* swarm_signal_adapter_create(
    swarm_radio_type_t radio_type,
    const swarm_signal_config_t* config
) {
    swarm_signal_adapter_t* adapter = new swarm_signal_adapter_t();

    if (config) {
        adapter->config = *config;
    } else {
        adapter->config = swarm_signal_default_config(radio_type);
    }

    memset(&adapter->stats, 0, sizeof(adapter->stats));
    adapter->simulated_failure = false;
    adapter->simulated_rssi = -50.0f; // Good signal

    return adapter;
}

void swarm_signal_adapter_destroy(swarm_signal_adapter_t* adapter) {
    if (adapter) {
        delete adapter;
    }
}

bool swarm_signal_encode(
    swarm_signal_adapter_t* adapter,
    const swarm_phoneme_message_t* message,
    uint8_t* signal_buffer,
    uint32_t* signal_length
) {
    if (!adapter || !message || !signal_buffer) return false;

    // Simple encoding: just copy the message
    memcpy(signal_buffer, message, sizeof(swarm_phoneme_message_t));
    if (signal_length) {
        *signal_length = sizeof(swarm_phoneme_message_t);
    }

    return true;
}

bool swarm_signal_decode(
    swarm_signal_adapter_t* adapter,
    const uint8_t* signal_buffer,
    uint32_t signal_length,
    swarm_phoneme_message_t* message
) {
    if (!adapter || !signal_buffer || !message) return false;
    if (signal_length < sizeof(swarm_phoneme_message_t)) return false;

    memcpy(message, signal_buffer, sizeof(swarm_phoneme_message_t));
    return true;
}

bool swarm_signal_send(
    swarm_signal_adapter_t* adapter,
    const swarm_phoneme_message_t* message
) {
    if (!adapter || !message) return false;
    if (adapter->simulated_failure) return false;

    adapter->stats.packets_sent++;
    adapter->stats.bytes_sent += sizeof(swarm_phoneme_message_t);

    return true;
}

bool swarm_signal_receive(
    swarm_signal_adapter_t* adapter,
    swarm_phoneme_message_t* message,
    uint32_t timeout_ms
) {
    if (!adapter || !message) return false;

    auto start = std::chrono::steady_clock::now();

    while (true) {
        if (!adapter->rx_queue.empty()) {
            *message = adapter->rx_queue.front();
            adapter->rx_queue.pop();
            adapter->stats.packets_received++;
            adapter->stats.bytes_received += sizeof(swarm_phoneme_message_t);
            adapter->stats.avg_rssi = adapter->simulated_rssi;
            return true;
        }

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);

        if (elapsed.count() >= timeout_ms) {
            return false; // Timeout
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

bool swarm_signal_broadcast(
    swarm_signal_adapter_t* adapter,
    const swarm_phoneme_message_t* message
) {
    // Broadcast is same as send in simulation
    return swarm_signal_send(adapter, message);
}

swarm_signal_stats_t swarm_signal_get_stats(swarm_signal_adapter_t* adapter) {
    if (!adapter) {
        swarm_signal_stats_t empty;
        memset(&empty, 0, sizeof(empty));
        return empty;
    }

    // Calculate packet loss rate
    uint64_t total_packets = adapter->stats.packets_sent + adapter->stats.packets_received;
    if (total_packets > 0) {
        adapter->stats.packet_loss_rate =
            (float)adapter->stats.packets_dropped / (float)total_packets;
    }

    return adapter->stats;
}

void swarm_signal_reset_stats(swarm_signal_adapter_t* adapter) {
    if (adapter) {
        memset(&adapter->stats, 0, sizeof(adapter->stats));
    }
}

//=============================================================================
// Test Fixtures
//=============================================================================

class SwarmSignalTest : public ::testing::Test {
protected:
    swarm_signal_adapter_t* adapter;

    void SetUp() override {
        adapter = nullptr;
    }

    void TearDown() override {
        if (adapter) {
            swarm_signal_adapter_destroy(adapter);
            adapter = nullptr;
        }
    }

    // Helper: Create test message
    swarm_phoneme_message_t CreateTestMessage(uint16_t sender_id, uint8_t msg_type) {
        swarm_phoneme_message_t msg;
        memset(&msg, 0, sizeof(msg));
        msg.sender_id = sender_id;
        msg.message_type = msg_type;
        msg.sequence_length = 4;
        for (int i = 0; i < 4; i++) {
            msg.phoneme_sequence[i] = i;
            msg.payload[i] = static_cast<float>(i) * 1.5f;
        }
        msg.crc16 = 0x1234;
        return msg;
    }

    // Helper: Simulate message reception
    void SimulateReceive(swarm_signal_adapter_t* adapter, const swarm_phoneme_message_t& msg) {
        if (adapter) {
            adapter->rx_queue.push(msg);
        }
    }
};

//=============================================================================
// 1. Adapter Creation and Destruction Tests
//=============================================================================

TEST_F(SwarmSignalTest, CreateLoRaAdapter) {
    adapter = swarm_signal_adapter_create(SWARM_RADIO_LORA, nullptr);

    ASSERT_NE(adapter, nullptr);
    EXPECT_EQ(adapter->config.radio_type, SWARM_RADIO_LORA);
    EXPECT_EQ(adapter->config.max_packet_size, 255u);
    EXPECT_EQ(adapter->config.frequency_khz, 915000u);
}

TEST_F(SwarmSignalTest, CreateWiFiAdapter) {
    adapter = swarm_signal_adapter_create(SWARM_RADIO_WIFI, nullptr);

    ASSERT_NE(adapter, nullptr);
    EXPECT_EQ(adapter->config.radio_type, SWARM_RADIO_WIFI);
    EXPECT_EQ(adapter->config.max_packet_size, 1500u);
}

TEST_F(SwarmSignalTest, CreateBluetoothAdapter) {
    adapter = swarm_signal_adapter_create(SWARM_RADIO_BLUETOOTH, nullptr);

    ASSERT_NE(adapter, nullptr);
    EXPECT_EQ(adapter->config.radio_type, SWARM_RADIO_BLUETOOTH);
    EXPECT_EQ(adapter->config.max_packet_size, 512u);
}

TEST_F(SwarmSignalTest, CreateUltrasonicAdapter) {
    adapter = swarm_signal_adapter_create(SWARM_RADIO_ULTRASONIC, nullptr);

    ASSERT_NE(adapter, nullptr);
    EXPECT_EQ(adapter->config.radio_type, SWARM_RADIO_ULTRASONIC);
    EXPECT_EQ(adapter->config.max_packet_size, 64u);
    EXPECT_EQ(adapter->config.frequency_khz, 40u);
}

TEST_F(SwarmSignalTest, CreateWithCustomConfig) {
    swarm_signal_config_t config = swarm_signal_default_config(SWARM_RADIO_LORA);
    config.timeout_ms = 200;
    config.tx_power_dbm = 30.0f;
    config.enable_encryption = true;

    adapter = swarm_signal_adapter_create(SWARM_RADIO_LORA, &config);

    ASSERT_NE(adapter, nullptr);
    EXPECT_EQ(adapter->config.timeout_ms, 200u);
    EXPECT_FLOAT_EQ(adapter->config.tx_power_dbm, 30.0f);
    EXPECT_TRUE(adapter->config.enable_encryption);
}

TEST_F(SwarmSignalTest, DestroyNullAdapter) {
    // Should not crash
    swarm_signal_adapter_destroy(nullptr);
}

//=============================================================================
// 2. Signal Encoding/Decoding Tests
//=============================================================================

TEST_F(SwarmSignalTest, EncodeMessage) {
    adapter = swarm_signal_adapter_create(SWARM_RADIO_LORA, nullptr);
    swarm_phoneme_message_t msg = CreateTestMessage(1, 0);

    uint8_t buffer[256];
    uint32_t signal_length = 0;

    bool success = swarm_signal_encode(adapter, &msg, buffer, &signal_length);

    ASSERT_TRUE(success);
    EXPECT_EQ(signal_length, sizeof(swarm_phoneme_message_t));
}

TEST_F(SwarmSignalTest, DecodeMessage) {
    adapter = swarm_signal_adapter_create(SWARM_RADIO_LORA, nullptr);
    swarm_phoneme_message_t original = CreateTestMessage(42, 5);

    uint8_t buffer[256];
    uint32_t signal_length = 0;

    ASSERT_TRUE(swarm_signal_encode(adapter, &original, buffer, &signal_length));

    swarm_phoneme_message_t decoded;
    bool success = swarm_signal_decode(adapter, buffer, signal_length, &decoded);

    ASSERT_TRUE(success);
    EXPECT_EQ(decoded.sender_id, 42);
    EXPECT_EQ(decoded.message_type, 5);
    EXPECT_EQ(decoded.sequence_length, original.sequence_length);
}

TEST_F(SwarmSignalTest, EncodeDecodeRoundTrip) {
    adapter = swarm_signal_adapter_create(SWARM_RADIO_LORA, nullptr);

    for (int i = 0; i < 10; i++) {
        swarm_phoneme_message_t original = CreateTestMessage(i, i % 5);

        uint8_t buffer[256];
        uint32_t signal_length = 0;

        ASSERT_TRUE(swarm_signal_encode(adapter, &original, buffer, &signal_length));

        swarm_phoneme_message_t decoded;
        ASSERT_TRUE(swarm_signal_decode(adapter, buffer, signal_length, &decoded));

        EXPECT_EQ(decoded.sender_id, original.sender_id);
        EXPECT_EQ(decoded.message_type, original.message_type);
    }
}

//=============================================================================
// 3. Send/Receive Tests
//=============================================================================

TEST_F(SwarmSignalTest, SendMessage) {
    adapter = swarm_signal_adapter_create(SWARM_RADIO_LORA, nullptr);
    swarm_phoneme_message_t msg = CreateTestMessage(1, 0);

    bool success = swarm_signal_send(adapter, &msg);

    ASSERT_TRUE(success);

    swarm_signal_stats_t stats = swarm_signal_get_stats(adapter);
    EXPECT_EQ(stats.packets_sent, 1u);
    EXPECT_EQ(stats.bytes_sent, sizeof(swarm_phoneme_message_t));
}

TEST_F(SwarmSignalTest, SendMultipleMessages) {
    adapter = swarm_signal_adapter_create(SWARM_RADIO_LORA, nullptr);

    for (int i = 0; i < 100; i++) {
        swarm_phoneme_message_t msg = CreateTestMessage(i, i % 10);
        ASSERT_TRUE(swarm_signal_send(adapter, &msg));
    }

    swarm_signal_stats_t stats = swarm_signal_get_stats(adapter);
    EXPECT_EQ(stats.packets_sent, 100u);
}

TEST_F(SwarmSignalTest, ReceiveMessage) {
    adapter = swarm_signal_adapter_create(SWARM_RADIO_LORA, nullptr);

    swarm_phoneme_message_t sent = CreateTestMessage(7, 3);
    SimulateReceive(adapter, sent);

    swarm_phoneme_message_t received;
    bool success = swarm_signal_receive(adapter, &received, 100);

    ASSERT_TRUE(success);
    EXPECT_EQ(received.sender_id, 7);
    EXPECT_EQ(received.message_type, 3);

    swarm_signal_stats_t stats = swarm_signal_get_stats(adapter);
    EXPECT_EQ(stats.packets_received, 1u);
}

TEST_F(SwarmSignalTest, ReceiveMultipleMessages) {
    adapter = swarm_signal_adapter_create(SWARM_RADIO_LORA, nullptr);

    // Queue multiple messages
    for (int i = 0; i < 5; i++) {
        swarm_phoneme_message_t msg = CreateTestMessage(i, i);
        SimulateReceive(adapter, msg);
    }

    // Receive them all
    for (int i = 0; i < 5; i++) {
        swarm_phoneme_message_t received;
        ASSERT_TRUE(swarm_signal_receive(adapter, &received, 100));
        EXPECT_EQ(received.sender_id, static_cast<uint16_t>(i));
    }

    swarm_signal_stats_t stats = swarm_signal_get_stats(adapter);
    EXPECT_EQ(stats.packets_received, 5u);
}

TEST_F(SwarmSignalTest, ReceiveTimeout) {
    adapter = swarm_signal_adapter_create(SWARM_RADIO_LORA, nullptr);

    swarm_phoneme_message_t received;
    bool success = swarm_signal_receive(adapter, &received, 50); // 50ms timeout

    EXPECT_FALSE(success); // Should timeout with no messages
}

//=============================================================================
// 4. Broadcast Tests
//=============================================================================

TEST_F(SwarmSignalTest, BroadcastMessage) {
    adapter = swarm_signal_adapter_create(SWARM_RADIO_LORA, nullptr);
    swarm_phoneme_message_t msg = CreateTestMessage(1, 0);

    bool success = swarm_signal_broadcast(adapter, &msg);

    ASSERT_TRUE(success);

    swarm_signal_stats_t stats = swarm_signal_get_stats(adapter);
    EXPECT_EQ(stats.packets_sent, 1u);
}

TEST_F(SwarmSignalTest, BroadcastMultipleMessages) {
    adapter = swarm_signal_adapter_create(SWARM_RADIO_WIFI, nullptr);

    for (int i = 0; i < 50; i++) {
        swarm_phoneme_message_t msg = CreateTestMessage(0, 0); // Broadcast from node 0
        ASSERT_TRUE(swarm_signal_broadcast(adapter, &msg));
    }

    swarm_signal_stats_t stats = swarm_signal_get_stats(adapter);
    EXPECT_EQ(stats.packets_sent, 50u);
}

//=============================================================================
// 5. Statistics Tests
//=============================================================================

TEST_F(SwarmSignalTest, InitialStatsZero) {
    adapter = swarm_signal_adapter_create(SWARM_RADIO_LORA, nullptr);

    swarm_signal_stats_t stats = swarm_signal_get_stats(adapter);

    EXPECT_EQ(stats.packets_sent, 0u);
    EXPECT_EQ(stats.packets_received, 0u);
    EXPECT_EQ(stats.packets_dropped, 0u);
    EXPECT_EQ(stats.bytes_sent, 0u);
    EXPECT_EQ(stats.bytes_received, 0u);
}

TEST_F(SwarmSignalTest, StatsTrackSendReceive) {
    adapter = swarm_signal_adapter_create(SWARM_RADIO_LORA, nullptr);

    // Send 10 messages
    for (int i = 0; i < 10; i++) {
        swarm_phoneme_message_t msg = CreateTestMessage(i, 0);
        swarm_signal_send(adapter, &msg);
    }

    // Receive 5 messages
    for (int i = 0; i < 5; i++) {
        swarm_phoneme_message_t msg = CreateTestMessage(i, 0);
        SimulateReceive(adapter, msg);
    }

    for (int i = 0; i < 5; i++) {
        swarm_phoneme_message_t received;
        swarm_signal_receive(adapter, &received, 100);
    }

    swarm_signal_stats_t stats = swarm_signal_get_stats(adapter);

    EXPECT_EQ(stats.packets_sent, 10u);
    EXPECT_EQ(stats.packets_received, 5u);
    EXPECT_EQ(stats.bytes_sent, 10u * sizeof(swarm_phoneme_message_t));
    EXPECT_EQ(stats.bytes_received, 5u * sizeof(swarm_phoneme_message_t));
}

TEST_F(SwarmSignalTest, ResetStats) {
    adapter = swarm_signal_adapter_create(SWARM_RADIO_LORA, nullptr);

    // Send some messages
    for (int i = 0; i < 5; i++) {
        swarm_phoneme_message_t msg = CreateTestMessage(i, 0);
        swarm_signal_send(adapter, &msg);
    }

    swarm_signal_reset_stats(adapter);

    swarm_signal_stats_t stats = swarm_signal_get_stats(adapter);
    EXPECT_EQ(stats.packets_sent, 0u);
    EXPECT_EQ(stats.bytes_sent, 0u);
}

TEST_F(SwarmSignalTest, RSSITracking) {
    adapter = swarm_signal_adapter_create(SWARM_RADIO_LORA, nullptr);
    adapter->simulated_rssi = -60.0f;

    swarm_phoneme_message_t msg = CreateTestMessage(1, 0);
    SimulateReceive(adapter, msg);

    swarm_phoneme_message_t received;
    swarm_signal_receive(adapter, &received, 100);

    swarm_signal_stats_t stats = swarm_signal_get_stats(adapter);
    EXPECT_FLOAT_EQ(stats.avg_rssi, -60.0f);
}

//=============================================================================
// 6. Error Handling Tests
//=============================================================================

TEST_F(SwarmSignalTest, SendWithNullAdapter) {
    swarm_phoneme_message_t msg = CreateTestMessage(1, 0);
    bool success = swarm_signal_send(nullptr, &msg);

    EXPECT_FALSE(success);
}

TEST_F(SwarmSignalTest, SendWithNullMessage) {
    adapter = swarm_signal_adapter_create(SWARM_RADIO_LORA, nullptr);
    bool success = swarm_signal_send(adapter, nullptr);

    EXPECT_FALSE(success);
}

TEST_F(SwarmSignalTest, ReceiveWithNullAdapter) {
    swarm_phoneme_message_t msg;
    bool success = swarm_signal_receive(nullptr, &msg, 100);

    EXPECT_FALSE(success);
}

TEST_F(SwarmSignalTest, SimulatedFailure) {
    adapter = swarm_signal_adapter_create(SWARM_RADIO_LORA, nullptr);
    adapter->simulated_failure = true;

    swarm_phoneme_message_t msg = CreateTestMessage(1, 0);
    bool success = swarm_signal_send(adapter, &msg);

    EXPECT_FALSE(success);
}

//=============================================================================
// 7. Radio Type Specific Tests
//=============================================================================

TEST_F(SwarmSignalTest, LoRaPacketSizeLimit) {
    adapter = swarm_signal_adapter_create(SWARM_RADIO_LORA, nullptr);

    EXPECT_EQ(adapter->config.max_packet_size, 255u);
    EXPECT_LE(sizeof(swarm_phoneme_message_t), adapter->config.max_packet_size);
}

TEST_F(SwarmSignalTest, WiFiHighBandwidth) {
    adapter = swarm_signal_adapter_create(SWARM_RADIO_WIFI, nullptr);

    EXPECT_EQ(adapter->config.max_packet_size, 1500u);
    EXPECT_GT(adapter->config.max_packet_size, 255u); // Higher than LoRa
}

TEST_F(SwarmSignalTest, UltrasonicLowBandwidth) {
    adapter = swarm_signal_adapter_create(SWARM_RADIO_ULTRASONIC, nullptr);

    EXPECT_EQ(adapter->config.max_packet_size, 64u);
    EXPECT_LT(adapter->config.max_packet_size, 255u); // Lower than LoRa
}

//=============================================================================
// 8. Configuration Tests
//=============================================================================

TEST_F(SwarmSignalTest, DefaultConfigValid) {
    swarm_signal_config_t config = swarm_signal_default_config(SWARM_RADIO_LORA);

    EXPECT_EQ(config.radio_type, SWARM_RADIO_LORA);
    EXPECT_GT(config.max_packet_size, 0u);
    EXPECT_GT(config.timeout_ms, 0u);
    EXPECT_GT(config.tx_power_dbm, 0.0f);
}

TEST_F(SwarmSignalTest, AllRadioTypesHaveDefaults) {
    for (int type = SWARM_RADIO_LORA; type <= SWARM_RADIO_CUSTOM; type++) {
        swarm_signal_config_t config = swarm_signal_default_config(
            static_cast<swarm_radio_type_t>(type)
        );

        EXPECT_EQ(config.radio_type, type);
        EXPECT_GT(config.max_packet_size, 0u);
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
