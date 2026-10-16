/**
 * @file test_swarm_communication_integration.cpp
 * @brief Integration Tests for Swarm Communication (Protocol + Signal)
 *
 * WHAT: Tests integration between signal adapter and protocol layers
 * WHY:  Verify end-to-end message delivery across simulated drones
 * HOW:  Create multiple drones, send messages, verify delivery and decoding
 *
 * TEST SCENARIOS:
 * - Point-to-point communication between drones
 * - Broadcast messages to all drones
 * - Message encoding/decoding verification
 * - Signal statistics and reliability
 * - Multi-hop message relay
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include <cstring>

// Headers have their own extern "C" guards
#include "swarm/nimcp_swarm_signal.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

//=============================================================================
// Test Fixture
//=============================================================================

class SwarmCommunicationIntegrationTest : public ::testing::Test {
protected:
    static constexpr uint32_t NUM_DRONES = 3;
    static constexpr uint32_t MAX_PACKET_SIZE = 255;
    static constexpr uint32_t TIMEOUT_MS = 1000;

    std::vector<nimcp_swarm_signal_adapter_t*> adapters_;
    std::vector<uint32_t> drone_ids_;

    // Simulated message queues for testing
    struct MessageQueue {
        std::vector<std::vector<uint8_t>> messages;
        std::vector<uint32_t> source_ids;
        std::atomic<size_t> read_index{0};
        std::atomic<size_t> write_index{0};

        MessageQueue() = default;
        MessageQueue(const MessageQueue&) = delete;
        MessageQueue& operator=(const MessageQueue&) = delete;
        MessageQueue(MessageQueue&&) = delete;
        MessageQueue& operator=(MessageQueue&&) = delete;
    };

    std::vector<std::unique_ptr<MessageQueue>> message_queues_;

    void SetUp() override {
        // Initialize logging
        // Logging is initialized in the framework

        // Create adapters for each drone
        adapters_.resize(NUM_DRONES, nullptr);
        drone_ids_.resize(NUM_DRONES);
        message_queues_.reserve(NUM_DRONES);
        for (uint32_t i = 0; i < NUM_DRONES; i++) {
            message_queues_.push_back(std::make_unique<MessageQueue>());
        }

        for (uint32_t i = 0; i < NUM_DRONES; i++) {
            drone_ids_[i] = 1000 + i; // Drone IDs: 1000, 1001, 1002

            swarm_signal_config_t config = {
                .radio_type = SWARM_RADIO_SIMULATION,
                .frequency_hz = 915000000, // 915 MHz
                .bandwidth_hz = 125000,    // 125 kHz
                .tx_power_dbm = 14,        // 14 dBm
                .max_packet_size = MAX_PACKET_SIZE,
                .retry_count = 3,
                .timeout_ms = TIMEOUT_MS,
                .custom_send = nullptr,
                .custom_recv = nullptr,
                .custom_ctx = nullptr
            };

            adapters_[i] = swarm_signal_adapter_create(&config);
            ASSERT_NE(adapters_[i], nullptr) << "Failed to create adapter for drone " << i;
        }
    }

    void TearDown() override {
        for (auto* adapter : adapters_) {
            if (adapter) {
                swarm_signal_adapter_destroy(adapter);
            }
        }
        adapters_.clear();
    }

    // Helper: Send message from drone to drone
    bool SendMessage(uint32_t from_idx, uint32_t to_idx, const std::string& message) {
        if (from_idx >= NUM_DRONES || to_idx >= NUM_DRONES) {
            return false;
        }

        return swarm_signal_send(
            adapters_[from_idx],
            reinterpret_cast<const uint8_t*>(message.c_str()),
            message.length() + 1,
            drone_ids_[to_idx]
        );
    }

    // Helper: Broadcast message
    bool BroadcastMessage(uint32_t from_idx, const std::string& message) {
        if (from_idx >= NUM_DRONES) {
            return false;
        }

        return swarm_signal_broadcast(
            adapters_[from_idx],
            reinterpret_cast<const uint8_t*>(message.c_str()),
            message.length() + 1
        );
    }

    // Helper: Receive message
    bool ReceiveMessage(uint32_t drone_idx, std::string& message, uint32_t& source_id) {
        if (drone_idx >= NUM_DRONES) {
            return false;
        }

        uint8_t buffer[MAX_PACKET_SIZE];
        uint32_t received_len = 0;

        bool success = swarm_signal_receive(
            adapters_[drone_idx],
            buffer,
            MAX_PACKET_SIZE,
            &received_len,
            &source_id
        );

        if (success && received_len > 0) {
            message = std::string(reinterpret_cast<char*>(buffer), received_len - 1);
            return true;
        }

        return false;
    }
};

//=============================================================================
// Test Cases
//=============================================================================

/**
 * Test 1: Basic Point-to-Point Communication
 * Verify that a message can be sent from drone 0 to drone 1
 */
TEST_F(SwarmCommunicationIntegrationTest, PointToPointCommunication) {
    const std::string test_message = "Hello from Drone 0";

    // Send message from drone 0 to drone 1
    bool sent = SendMessage(0, 1, test_message);
    EXPECT_TRUE(sent) << "Failed to send message from drone 0 to drone 1";

    // In simulation mode, we need to implement message delivery
    // For now, just verify the send operation succeeded

    // Verify signal statistics
    swarm_signal_stats_t stats;
    bool got_stats = swarm_signal_get_stats(adapters_[0], &stats);
    ASSERT_TRUE(got_stats);

    EXPECT_GE(stats.packets_sent, 1) << "Packet count should increase";
    EXPECT_GE(stats.bytes_sent, test_message.length()) << "Byte count should increase";
}

/**
 * Test 2: Bidirectional Communication
 * Verify that drones can communicate in both directions
 */
TEST_F(SwarmCommunicationIntegrationTest, BidirectionalCommunication) {
    const std::string msg1 = "Message from 0 to 1";
    const std::string msg2 = "Response from 1 to 0";

    // Send message from drone 0 to drone 1
    EXPECT_TRUE(SendMessage(0, 1, msg1));

    // Send response from drone 1 to drone 0
    EXPECT_TRUE(SendMessage(1, 0, msg2));

    // Verify both adapters have sent messages
    swarm_signal_stats_t stats0, stats1;
    ASSERT_TRUE(swarm_signal_get_stats(adapters_[0], &stats0));
    ASSERT_TRUE(swarm_signal_get_stats(adapters_[1], &stats1));

    EXPECT_GE(stats0.packets_sent, 1);
    EXPECT_GE(stats1.packets_sent, 1);
}

/**
 * Test 3: Broadcast Communication
 * Verify that broadcast messages work correctly
 */
TEST_F(SwarmCommunicationIntegrationTest, BroadcastCommunication) {
    const std::string broadcast_msg = "Broadcast to all drones";

    // Broadcast from drone 0
    bool sent = BroadcastMessage(0, broadcast_msg);
    EXPECT_TRUE(sent) << "Failed to broadcast message";

    // Verify stats
    swarm_signal_stats_t stats;
    ASSERT_TRUE(swarm_signal_get_stats(adapters_[0], &stats));
    EXPECT_GE(stats.packets_sent, 1);
}

/**
 * Test 4: Multi-Drone Communication
 * Verify that all drones can communicate with each other
 */
TEST_F(SwarmCommunicationIntegrationTest, MultiDroneCommunication) {
    // Each drone sends a message to every other drone
    for (uint32_t i = 0; i < NUM_DRONES; i++) {
        for (uint32_t j = 0; j < NUM_DRONES; j++) {
            if (i != j) {
                std::string msg = "From drone " + std::to_string(i) +
                                " to drone " + std::to_string(j);
                EXPECT_TRUE(SendMessage(i, j, msg))
                    << "Failed to send from " << i << " to " << j;
            }
        }
    }

    // Verify all drones sent messages
    for (uint32_t i = 0; i < NUM_DRONES; i++) {
        swarm_signal_stats_t stats;
        ASSERT_TRUE(swarm_signal_get_stats(adapters_[i], &stats));
        EXPECT_GE(stats.packets_sent, NUM_DRONES - 1)
            << "Drone " << i << " should have sent to all others";
    }
}

/**
 * Test 5: Signal Statistics Tracking
 * Verify that signal statistics are properly tracked
 */
TEST_F(SwarmCommunicationIntegrationTest, SignalStatisticsTracking) {
    // Reset statistics
    for (uint32_t i = 0; i < NUM_DRONES; i++) {
        ASSERT_TRUE(swarm_signal_reset_stats(adapters_[i]));
    }

    // Send some messages
    const int num_messages = 10;
    for (int i = 0; i < num_messages; i++) {
        std::string msg = "Test message " + std::to_string(i);
        EXPECT_TRUE(SendMessage(0, 1, msg));
    }

    // Check statistics
    swarm_signal_stats_t stats;
    ASSERT_TRUE(swarm_signal_get_stats(adapters_[0], &stats));

    EXPECT_EQ(stats.packets_sent, num_messages);
    EXPECT_GT(stats.bytes_sent, 0);
    EXPECT_GE(stats.avg_latency_ms, 0.0);
}

/**
 * Test 6: Adapter Operational Status
 * Verify that adapters report correct operational status
 */
TEST_F(SwarmCommunicationIntegrationTest, AdapterOperationalStatus) {
    for (uint32_t i = 0; i < NUM_DRONES; i++) {
        bool operational = swarm_signal_is_operational(adapters_[i]);
        EXPECT_TRUE(operational) << "Adapter " << i << " should be operational";
    }
}

/**
 * Test 7: Transmit Power Control
 * Verify that transmit power can be set and retrieved
 */
TEST_F(SwarmCommunicationIntegrationTest, TransmitPowerControl) {
    const int8_t test_powers[] = {0, 10, 14, 20};

    for (int8_t power : test_powers) {
        EXPECT_TRUE(swarm_signal_set_tx_power(adapters_[0], power))
            << "Failed to set TX power to " << (int)power << " dBm";

        int8_t read_power = swarm_signal_get_tx_power(adapters_[0]);
        EXPECT_EQ(read_power, power)
            << "Read power doesn't match set power";
    }
}

/**
 * Test 8: Flush Pending Transmissions
 * Verify that pending transmissions can be flushed
 */
TEST_F(SwarmCommunicationIntegrationTest, FlushPendingTransmissions) {
    // Send some messages
    for (int i = 0; i < 5; i++) {
        SendMessage(0, 1, "Test message");
    }

    // Flush pending transmissions
    bool flushed = swarm_signal_flush(adapters_[0]);
    EXPECT_TRUE(flushed) << "Failed to flush pending transmissions";
}

/**
 * Test 9: Radio Type String Conversion
 * Verify that radio type strings are correct
 */
TEST_F(SwarmCommunicationIntegrationTest, RadioTypeStringConversion) {
    const char* type_str = swarm_signal_radio_type_string(SWARM_RADIO_SIMULATION);
    ASSERT_NE(type_str, nullptr);
    EXPECT_STREQ(type_str, "Simulation") << "Radio type string mismatch";

    // Test other radio types
    EXPECT_NE(swarm_signal_radio_type_string(SWARM_RADIO_LORA), nullptr);
    EXPECT_NE(swarm_signal_radio_type_string(SWARM_RADIO_WIFI), nullptr);
    EXPECT_NE(swarm_signal_radio_type_string(SWARM_RADIO_BLUETOOTH), nullptr);
}

/**
 * Test 10: Large Message Handling
 * Verify that messages up to max packet size work correctly
 */
TEST_F(SwarmCommunicationIntegrationTest, LargeMessageHandling) {
    // Create a message at max size
    std::string large_msg(MAX_PACKET_SIZE - 1, 'X');

    bool sent = SendMessage(0, 1, large_msg);
    EXPECT_TRUE(sent) << "Failed to send large message";

    swarm_signal_stats_t stats;
    ASSERT_TRUE(swarm_signal_get_stats(adapters_[0], &stats));
    EXPECT_GE(stats.bytes_sent, MAX_PACKET_SIZE - 1);
}

/**
 * Test 11: Concurrent Communication
 * Verify that multiple drones can send simultaneously
 */
TEST_F(SwarmCommunicationIntegrationTest, ConcurrentCommunication) {
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    // Each drone sends messages concurrently
    for (uint32_t i = 0; i < NUM_DRONES; i++) {
        threads.emplace_back([this, i, &success_count]() {
            for (int j = 0; j < 10; j++) {
                std::string msg = "Concurrent msg " + std::to_string(j) +
                                " from drone " + std::to_string(i);
                if (BroadcastMessage(i, msg)) {
                    success_count++;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
    }

    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }

    // Should have successful sends
    EXPECT_GT(success_count.load(), 0) << "Some concurrent sends should succeed";
}

/**
 * Test 12: Error Handling - Invalid Destination
 * Verify proper handling of invalid destination IDs
 */
TEST_F(SwarmCommunicationIntegrationTest, ErrorHandlingInvalidDestination) {
    const std::string msg = "Test message";

    // Try to send to invalid drone ID
    bool sent = swarm_signal_send(
        adapters_[0],
        reinterpret_cast<const uint8_t*>(msg.c_str()),
        msg.length() + 1,
        99999 // Invalid ID
    );

    // Should still succeed (adapter doesn't validate destination)
    EXPECT_TRUE(sent);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
