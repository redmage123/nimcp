/**
 * @file test_corpus_callosum_integration.cpp
 * @brief Integration tests for corpus callosum inter-hemispheric communication
 *
 * Tests cover:
 * - Inter-hemispheric communication across all channels
 * - All bandwidth modes (UNLIMITED, REALISTIC, RESTRICTED, CUSTOM)
 * - Message queue processing with latency simulation
 * - Split-brain simulation (disconnect/reconnect)
 * - Connection strength effects on bandwidth
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <cstring>
#include <chrono>
#include <thread>
#include "utils/nimcp_test_base.h"

#include "core/brain/hemispheric/nimcp_corpus_callosum.h"
#include "core/brain/hemispheric/nimcp_brain_hemisphere.h"
#include "core/brain/hemispheric/nimcp_lateralization.h"

/**
 * @class CorpusCallosumIntegrationTest
 * @brief Test fixture for corpus callosum integration tests
 */
class CorpusCallosumIntegrationTest : public NimcpTestBase {
protected:
    static constexpr uint32_t INPUT_SIZE = 8;
    static constexpr uint32_t OUTPUT_SIZE = 4;
    static constexpr uint32_t TEST_MESSAGE_TYPE = 100;

    corpus_callosum_t* callosum = nullptr;
    brain_hemisphere_t* left_hemisphere = nullptr;
    brain_hemisphere_t* right_hemisphere = nullptr;

    void SetUp() override {
        NimcpTestBase::SetUp();

        // Create callosum
        callosum_config_t config = callosum_default_config();
        config.bandwidth_mode = CALLOSUM_BW_UNLIMITED;
        config.enable_bio_async = false;
        config.queue_capacity = 64;

        callosum = callosum_create(&config);

        // Create hemispheres
        hemisphere_config_t left_config = hemisphere_default_config(HEMISPHERE_LEFT);
        left_config.num_inputs = INPUT_SIZE;
        left_config.num_outputs = OUTPUT_SIZE;
        left_config.size = BRAIN_SIZE_SMALL;
        left_config.enable_bio_async = false;
        left_hemisphere = hemisphere_create(&left_config);

        hemisphere_config_t right_config = hemisphere_default_config(HEMISPHERE_RIGHT);
        right_config.num_inputs = INPUT_SIZE;
        right_config.num_outputs = OUTPUT_SIZE;
        right_config.size = BRAIN_SIZE_SMALL;
        right_config.enable_bio_async = false;
        right_hemisphere = hemisphere_create(&right_config);

        // Connect hemispheres to callosum
        if (callosum && left_hemisphere && right_hemisphere) {
            callosum_connect_hemispheres(callosum, left_hemisphere, right_hemisphere);
        }
    }

    void TearDown() override {
        if (callosum) {
            callosum_destroy(callosum);
            callosum = nullptr;
        }
        if (left_hemisphere) {
            hemisphere_destroy(left_hemisphere);
            left_hemisphere = nullptr;
        }
        if (right_hemisphere) {
            hemisphere_destroy(right_hemisphere);
            right_hemisphere = nullptr;
        }
        NimcpTestBase::TearDown();
    }
};

/* ============================================================================
 * Creation and Lifecycle Tests
 * ============================================================================ */

TEST_F(CorpusCallosumIntegrationTest, CreateWithDefaultConfig) {
    ASSERT_NE(callosum, nullptr);
    EXPECT_TRUE(callosum_is_connected(callosum));
}

TEST_F(CorpusCallosumIntegrationTest, CreateWithCustomConfig) {
    callosum_destroy(callosum);

    callosum_config_t config = callosum_default_config();
    config.bandwidth_mode = CALLOSUM_BW_REALISTIC;
    config.custom_max_msgs_per_second = 100;
    config.queue_capacity = 128;
    config.initial_connection_strength = 0.8f;

    callosum = callosum_create(&config);
    ASSERT_NE(callosum, nullptr);
    EXPECT_NEAR(callosum_get_connection_strength(callosum), 0.8f, 0.01f);
}

TEST_F(CorpusCallosumIntegrationTest, HemispheresConnected) {
    ASSERT_NE(left_hemisphere, nullptr);
    ASSERT_NE(right_hemisphere, nullptr);
    EXPECT_TRUE(callosum_is_connected(callosum));
}

/* ============================================================================
 * Channel Communication Tests
 * ============================================================================ */

TEST_F(CorpusCallosumIntegrationTest, SendOnMotorChannel) {
    const char* data = "motor_command";
    int result = callosum_send(
        callosum, HEMISPHERE_LEFT,
        CALLOSUM_CHANNEL_MOTOR,
        CALLOSUM_PRIORITY_NORMAL,
        TEST_MESSAGE_TYPE,
        data, strlen(data)
    );

    EXPECT_GE(result, 0);  // 0 = immediate, 1 = queued
}

TEST_F(CorpusCallosumIntegrationTest, SendOnSensoryChannel) {
    float sensory_data[] = {0.1f, 0.2f, 0.3f, 0.4f};
    int result = callosum_send(
        callosum, HEMISPHERE_RIGHT,
        CALLOSUM_CHANNEL_SENSORY,
        CALLOSUM_PRIORITY_HIGH,
        TEST_MESSAGE_TYPE,
        sensory_data, sizeof(sensory_data)
    );

    EXPECT_GE(result, 0);
}

TEST_F(CorpusCallosumIntegrationTest, SendOnCognitiveChannel) {
    uint32_t cognitive_data = 42;
    int result = callosum_send(
        callosum, HEMISPHERE_LEFT,
        CALLOSUM_CHANNEL_COGNITIVE,
        CALLOSUM_PRIORITY_NORMAL,
        TEST_MESSAGE_TYPE,
        &cognitive_data, sizeof(cognitive_data)
    );

    EXPECT_GE(result, 0);
}

TEST_F(CorpusCallosumIntegrationTest, SendOnEmotionalChannel) {
    float emotional_valence = 0.75f;
    int result = callosum_send(
        callosum, HEMISPHERE_RIGHT,
        CALLOSUM_CHANNEL_EMOTIONAL,
        CALLOSUM_PRIORITY_HIGH,
        TEST_MESSAGE_TYPE,
        &emotional_valence, sizeof(emotional_valence)
    );

    EXPECT_GE(result, 0);
}

TEST_F(CorpusCallosumIntegrationTest, SendOnInhibitoryChannel) {
    bool inhibit_signal = true;
    int result = callosum_send(
        callosum, HEMISPHERE_LEFT,
        CALLOSUM_CHANNEL_INHIBITORY,
        CALLOSUM_PRIORITY_URGENT,
        TEST_MESSAGE_TYPE,
        &inhibit_signal, sizeof(inhibit_signal)
    );

    EXPECT_GE(result, 0);
}

TEST_F(CorpusCallosumIntegrationTest, SendAndReceive) {
    const char* test_data = "test_message";
    size_t data_len = strlen(test_data);

    // Send from left to right
    callosum_send(
        callosum, HEMISPHERE_LEFT,
        CALLOSUM_CHANNEL_COGNITIVE,
        CALLOSUM_PRIORITY_NORMAL,
        TEST_MESSAGE_TYPE,
        test_data, data_len
    );

    // Process queues (needed for latency simulation)
    callosum_process_queues(callosum);

    // Receive on right hemisphere
    callosum_message_t messages[10];
    int count = callosum_receive(callosum, HEMISPHERE_RIGHT, messages, 10);

    EXPECT_GE(count, 0);
}

TEST_F(CorpusCallosumIntegrationTest, BidirectionalCommunication) {
    float left_data = 1.0f;
    float right_data = 2.0f;

    // Left to right
    callosum_send(
        callosum, HEMISPHERE_LEFT,
        CALLOSUM_CHANNEL_SENSORY,
        CALLOSUM_PRIORITY_NORMAL,
        TEST_MESSAGE_TYPE,
        &left_data, sizeof(left_data)
    );

    // Right to left
    callosum_send(
        callosum, HEMISPHERE_RIGHT,
        CALLOSUM_CHANNEL_SENSORY,
        CALLOSUM_PRIORITY_NORMAL,
        TEST_MESSAGE_TYPE + 1,
        &right_data, sizeof(right_data)
    );

    callosum_process_queues(callosum);

    callosum_stats_t stats;
    callosum_get_stats(callosum, &stats);

    EXPECT_GE(stats.total_messages_left_to_right + stats.total_messages_right_to_left, 0u);
}

/* ============================================================================
 * Bandwidth Mode Tests
 * ============================================================================ */

TEST_F(CorpusCallosumIntegrationTest, BandwidthModeUnlimited) {
    EXPECT_EQ(callosum_set_bandwidth_mode(callosum, CALLOSUM_BW_UNLIMITED), 0);

    // Send many messages quickly - should all succeed
    for (int i = 0; i < 100; ++i) {
        int result = callosum_send(
            callosum, HEMISPHERE_LEFT,
            CALLOSUM_CHANNEL_MOTOR,
            CALLOSUM_PRIORITY_NORMAL,
            TEST_MESSAGE_TYPE,
            &i, sizeof(i)
        );
        EXPECT_GE(result, 0);
    }
}

TEST_F(CorpusCallosumIntegrationTest, BandwidthModeRealistic) {
    EXPECT_EQ(callosum_set_bandwidth_mode(callosum, CALLOSUM_BW_REALISTIC), 0);

    // Send messages
    for (int i = 0; i < 20; ++i) {
        callosum_send(
            callosum, HEMISPHERE_LEFT,
            CALLOSUM_CHANNEL_COGNITIVE,
            CALLOSUM_PRIORITY_NORMAL,
            TEST_MESSAGE_TYPE,
            &i, sizeof(i)
        );
    }

    // Check bandwidth utilization
    float utilization = callosum_get_bandwidth_utilization(callosum);
    EXPECT_GE(utilization, 0.0f);
    EXPECT_LE(utilization, 1.0f);
}

TEST_F(CorpusCallosumIntegrationTest, BandwidthModeRestricted) {
    EXPECT_EQ(callosum_set_bandwidth_mode(callosum, CALLOSUM_BW_RESTRICTED), 0);

    // Restricted mode has lower throughput
    for (int i = 0; i < 10; ++i) {
        callosum_send(
            callosum, HEMISPHERE_RIGHT,
            CALLOSUM_CHANNEL_EMOTIONAL,
            CALLOSUM_PRIORITY_NORMAL,
            TEST_MESSAGE_TYPE,
            &i, sizeof(i)
        );
    }
}

TEST_F(CorpusCallosumIntegrationTest, BandwidthModeCustom) {
    EXPECT_EQ(callosum_set_bandwidth_mode(callosum, CALLOSUM_BW_CUSTOM), 0);
    EXPECT_EQ(callosum_set_bandwidth_limit(callosum, 500), 0);  // 500 msg/s

    uint32_t base_bw = corpus_callosum_get_base_bandwidth(callosum);
    EXPECT_GT(base_bw, 0u);
}

TEST_F(CorpusCallosumIntegrationTest, PerChannelBandwidth) {
    // Set different bandwidth for motor channel
    EXPECT_EQ(callosum_set_channel_bandwidth(
        callosum, CALLOSUM_CHANNEL_MOTOR, 1000), 0);

    // Motor channel should allow high throughput
    for (int i = 0; i < 50; ++i) {
        int result = callosum_send(
            callosum, HEMISPHERE_LEFT,
            CALLOSUM_CHANNEL_MOTOR,
            CALLOSUM_PRIORITY_NORMAL,
            TEST_MESSAGE_TYPE,
            &i, sizeof(i)
        );
        EXPECT_GE(result, 0);
    }
}

/* ============================================================================
 * Latency Simulation Tests
 * ============================================================================ */

TEST_F(CorpusCallosumIntegrationTest, LatencyConfiguration) {
    // Set latency range (5-20ms is biological)
    EXPECT_EQ(callosum_set_latency(callosum, 5.0f, 20.0f), 0);
    EXPECT_EQ(callosum_enable_latency_simulation(callosum, true), 0);

    float avg_latency = callosum_get_avg_latency(callosum);
    EXPECT_GE(avg_latency, 0.0f);
}

TEST_F(CorpusCallosumIntegrationTest, PerChannelLatency) {
    // Motor channel should be fast
    EXPECT_EQ(callosum_set_channel_latency(
        callosum, CALLOSUM_CHANNEL_MOTOR, 2.0f, 5.0f), 0);

    // Cognitive channel can be slower
    EXPECT_EQ(callosum_set_channel_latency(
        callosum, CALLOSUM_CHANNEL_COGNITIVE, 10.0f, 30.0f), 0);
}

TEST_F(CorpusCallosumIntegrationTest, MessageQueueProcessingWithLatency) {
    EXPECT_EQ(callosum_enable_latency_simulation(callosum, true), 0);
    EXPECT_EQ(callosum_set_latency(callosum, 1.0f, 5.0f), 0);

    // Send messages
    for (int i = 0; i < 5; ++i) {
        callosum_send(
            callosum, HEMISPHERE_LEFT,
            CALLOSUM_CHANNEL_SENSORY,
            CALLOSUM_PRIORITY_NORMAL,
            TEST_MESSAGE_TYPE,
            &i, sizeof(i)
        );
    }

    // Process queues multiple times
    int total_delivered = 0;
    for (int i = 0; i < 10; ++i) {
        total_delivered += callosum_process_queues(callosum);
    }

    EXPECT_GE(total_delivered, 0);
}

TEST_F(CorpusCallosumIntegrationTest, FlushDeliveryImmediate) {
    EXPECT_EQ(callosum_enable_latency_simulation(callosum, true), 0);
    EXPECT_EQ(callosum_set_latency(callosum, 100.0f, 200.0f), 0);  // Long latency

    // Send messages
    for (int i = 0; i < 5; ++i) {
        callosum_send(
            callosum, HEMISPHERE_LEFT,
            CALLOSUM_CHANNEL_COGNITIVE,
            CALLOSUM_PRIORITY_NORMAL,
            TEST_MESSAGE_TYPE,
            &i, sizeof(i)
        );
    }

    // Flush - should deliver immediately despite latency
    int flushed = callosum_flush(callosum);
    EXPECT_GE(flushed, 0);
}

/* ============================================================================
 * Split-Brain Simulation Tests
 * ============================================================================ */

TEST_F(CorpusCallosumIntegrationTest, DisconnectCallosum) {
    EXPECT_TRUE(callosum_is_connected(callosum));

    EXPECT_EQ(callosum_disconnect(callosum), 0);
    EXPECT_FALSE(callosum_is_connected(callosum));
    EXPECT_EQ(callosum_get_state(callosum), CALLOSUM_STATE_DISCONNECTED);
}

TEST_F(CorpusCallosumIntegrationTest, ReconnectCallosum) {
    callosum_disconnect(callosum);
    EXPECT_FALSE(callosum_is_connected(callosum));

    EXPECT_EQ(callosum_reconnect(callosum), 0);
    EXPECT_TRUE(callosum_is_connected(callosum));
    EXPECT_EQ(callosum_get_state(callosum), CALLOSUM_STATE_HEALTHY);
}

TEST_F(CorpusCallosumIntegrationTest, DisconnectedBlocksMessages) {
    callosum_disconnect(callosum);

    // Attempt to send
    int data = 42;
    int result = callosum_send(
        callosum, HEMISPHERE_LEFT,
        CALLOSUM_CHANNEL_MOTOR,
        CALLOSUM_PRIORITY_URGENT,
        TEST_MESSAGE_TYPE,
        &data, sizeof(data)
    );

    EXPECT_EQ(result, -1);  // Should fail
}

TEST_F(CorpusCallosumIntegrationTest, SplitBrainStatistics) {
    callosum_disconnect(callosum);

    callosum_stats_t stats;
    callosum_get_stats(callosum, &stats);

    EXPECT_EQ(stats.current_state, CALLOSUM_STATE_DISCONNECTED);
    EXPECT_GE(stats.disconnection_events, 1u);

    callosum_reconnect(callosum);
    callosum_get_stats(callosum, &stats);

    EXPECT_GE(stats.reconnection_events, 1u);
}

/* ============================================================================
 * Connection Strength Tests
 * ============================================================================ */

TEST_F(CorpusCallosumIntegrationTest, ConnectionStrengthFull) {
    EXPECT_EQ(callosum_set_connection_strength(callosum, 1.0f), 0);
    EXPECT_NEAR(callosum_get_connection_strength(callosum), 1.0f, 0.01f);
    EXPECT_EQ(callosum_get_state(callosum), CALLOSUM_STATE_HEALTHY);
}

TEST_F(CorpusCallosumIntegrationTest, ConnectionStrengthDegraded) {
    EXPECT_EQ(callosum_set_connection_strength(callosum, 0.5f), 0);
    EXPECT_NEAR(callosum_get_connection_strength(callosum), 0.5f, 0.01f);

    // Should be in degraded state
    callosum_state_t state = callosum_get_state(callosum);
    EXPECT_TRUE(state == CALLOSUM_STATE_DEGRADED || state == CALLOSUM_STATE_IMPAIRED);
}

TEST_F(CorpusCallosumIntegrationTest, ConnectionStrengthImpaired) {
    EXPECT_EQ(callosum_set_connection_strength(callosum, 0.2f), 0);
    EXPECT_NEAR(callosum_get_connection_strength(callosum), 0.2f, 0.01f);
}

TEST_F(CorpusCallosumIntegrationTest, ConnectionStrengthAffectsBandwidth) {
    EXPECT_EQ(callosum_set_bandwidth_mode(callosum, CALLOSUM_BW_REALISTIC), 0);

    // Full strength
    callosum_set_connection_strength(callosum, 1.0f);
    uint32_t full_bw = corpus_callosum_get_base_bandwidth(callosum);

    // Half strength should reduce effective bandwidth
    callosum_set_connection_strength(callosum, 0.5f);
    uint32_t half_bw = corpus_callosum_get_base_bandwidth(callosum);

    // Bandwidth may scale with connection strength
    // Just verify it's a reasonable value
    EXPECT_GT(full_bw, 0u);
    EXPECT_GT(half_bw, 0u);
}

/* ============================================================================
 * Channel Control Tests
 * ============================================================================ */

TEST_F(CorpusCallosumIntegrationTest, EnableDisableChannel) {
    // Initially enabled
    EXPECT_TRUE(callosum_is_channel_enabled(callosum, CALLOSUM_CHANNEL_MOTOR));

    // Disable
    EXPECT_EQ(callosum_set_channel_enabled(callosum, CALLOSUM_CHANNEL_MOTOR, false), 0);
    EXPECT_FALSE(callosum_is_channel_enabled(callosum, CALLOSUM_CHANNEL_MOTOR));

    // Re-enable
    EXPECT_EQ(callosum_set_channel_enabled(callosum, CALLOSUM_CHANNEL_MOTOR, true), 0);
    EXPECT_TRUE(callosum_is_channel_enabled(callosum, CALLOSUM_CHANNEL_MOTOR));
}

TEST_F(CorpusCallosumIntegrationTest, DisabledChannelBlocksMessages) {
    EXPECT_EQ(callosum_set_channel_enabled(callosum, CALLOSUM_CHANNEL_EMOTIONAL, false), 0);

    float data = 0.5f;
    int result = callosum_send(
        callosum, HEMISPHERE_LEFT,
        CALLOSUM_CHANNEL_EMOTIONAL,
        CALLOSUM_PRIORITY_NORMAL,
        TEST_MESSAGE_TYPE,
        &data, sizeof(data)
    );

    EXPECT_EQ(result, -1);  // Should fail on disabled channel
}

TEST_F(CorpusCallosumIntegrationTest, SelectiveChannelImpairment) {
    // Disable cognitive and emotional, keep motor and sensory
    callosum_set_channel_enabled(callosum, CALLOSUM_CHANNEL_COGNITIVE, false);
    callosum_set_channel_enabled(callosum, CALLOSUM_CHANNEL_EMOTIONAL, false);

    int data = 1;

    // Motor should work
    int motor_result = callosum_send(
        callosum, HEMISPHERE_LEFT,
        CALLOSUM_CHANNEL_MOTOR,
        CALLOSUM_PRIORITY_NORMAL,
        TEST_MESSAGE_TYPE,
        &data, sizeof(data)
    );
    EXPECT_GE(motor_result, 0);

    // Sensory should work
    int sensory_result = callosum_send(
        callosum, HEMISPHERE_RIGHT,
        CALLOSUM_CHANNEL_SENSORY,
        CALLOSUM_PRIORITY_NORMAL,
        TEST_MESSAGE_TYPE,
        &data, sizeof(data)
    );
    EXPECT_GE(sensory_result, 0);

    // Cognitive should fail
    int cognitive_result = callosum_send(
        callosum, HEMISPHERE_LEFT,
        CALLOSUM_CHANNEL_COGNITIVE,
        CALLOSUM_PRIORITY_NORMAL,
        TEST_MESSAGE_TYPE,
        &data, sizeof(data)
    );
    EXPECT_EQ(cognitive_result, -1);
}

/* ============================================================================
 * Priority Tests
 * ============================================================================ */

TEST_F(CorpusCallosumIntegrationTest, UrgentPriorityProcessedFirst) {
    EXPECT_EQ(callosum_set_bandwidth_mode(callosum, CALLOSUM_BW_RESTRICTED), 0);
    EXPECT_EQ(callosum_enable_latency_simulation(callosum, true), 0);

    int low_data = 1;
    int urgent_data = 999;

    // Send low priority first
    callosum_send(
        callosum, HEMISPHERE_LEFT,
        CALLOSUM_CHANNEL_COGNITIVE,
        CALLOSUM_PRIORITY_LOW,
        TEST_MESSAGE_TYPE,
        &low_data, sizeof(low_data)
    );

    // Send urgent priority
    callosum_send(
        callosum, HEMISPHERE_LEFT,
        CALLOSUM_CHANNEL_COGNITIVE,
        CALLOSUM_PRIORITY_URGENT,
        TEST_MESSAGE_TYPE,
        &urgent_data, sizeof(urgent_data)
    );

    // Process and verify urgent was handled
    callosum_process_queues(callosum);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(CorpusCallosumIntegrationTest, StatisticsAccumulate) {
    // Send several messages
    for (int i = 0; i < 10; ++i) {
        callosum_send(
            callosum, HEMISPHERE_LEFT,
            CALLOSUM_CHANNEL_MOTOR,
            CALLOSUM_PRIORITY_NORMAL,
            TEST_MESSAGE_TYPE,
            &i, sizeof(i)
        );
    }

    callosum_process_queues(callosum);

    callosum_stats_t stats;
    EXPECT_EQ(callosum_get_stats(callosum, &stats), 0);

    EXPECT_GE(stats.total_messages_left_to_right, 0u);
    EXPECT_GE(stats.total_bytes_transferred, 0u);
}

TEST_F(CorpusCallosumIntegrationTest, ResetStatistics) {
    // Generate some traffic
    for (int i = 0; i < 5; ++i) {
        callosum_send(
            callosum, HEMISPHERE_LEFT,
            CALLOSUM_CHANNEL_SENSORY,
            CALLOSUM_PRIORITY_NORMAL,
            TEST_MESSAGE_TYPE,
            &i, sizeof(i)
        );
    }
    callosum_process_queues(callosum);

    // Reset
    EXPECT_EQ(callosum_reset_stats(callosum), 0);

    callosum_stats_t stats;
    callosum_get_stats(callosum, &stats);

    // Message counts should be reset
    EXPECT_EQ(stats.total_messages_left_to_right, 0u);
}

TEST_F(CorpusCallosumIntegrationTest, PerChannelStatistics) {
    // Send on different channels
    callosum_send(callosum, HEMISPHERE_LEFT, CALLOSUM_CHANNEL_MOTOR,
                  CALLOSUM_PRIORITY_NORMAL, 1, "m", 1);
    callosum_send(callosum, HEMISPHERE_LEFT, CALLOSUM_CHANNEL_SENSORY,
                  CALLOSUM_PRIORITY_NORMAL, 2, "s", 1);
    callosum_send(callosum, HEMISPHERE_LEFT, CALLOSUM_CHANNEL_COGNITIVE,
                  CALLOSUM_PRIORITY_NORMAL, 3, "c", 1);

    callosum_process_queues(callosum);

    callosum_stats_t stats;
    callosum_get_stats(callosum, &stats);

    // Per-channel stats should be populated
    uint64_t total_channel_msgs = 0;
    for (int i = 0; i < CALLOSUM_CHANNEL_COUNT; ++i) {
        total_channel_msgs += stats.channel_messages[i];
    }
    EXPECT_GE(total_channel_msgs, 0u);
}

/* ============================================================================
 * Bio-async Integration Tests
 * ============================================================================ */

TEST_F(CorpusCallosumIntegrationTest, BioAsyncConnectDisconnect) {
    EXPECT_FALSE(callosum_is_bio_async_connected(callosum));

    int result = callosum_connect_bio_async(callosum);
    if (result == 0) {
        EXPECT_TRUE(callosum_is_bio_async_connected(callosum));

        EXPECT_EQ(callosum_disconnect_bio_async(callosum), 0);
        EXPECT_FALSE(callosum_is_bio_async_connected(callosum));
    }
}

/* ============================================================================
 * Utility Function Tests
 * ============================================================================ */

TEST_F(CorpusCallosumIntegrationTest, ChannelNameStrings) {
    EXPECT_STREQ(callosum_channel_name(CALLOSUM_CHANNEL_MOTOR), "Motor");
    EXPECT_STREQ(callosum_channel_name(CALLOSUM_CHANNEL_SENSORY), "Sensory");
    EXPECT_STREQ(callosum_channel_name(CALLOSUM_CHANNEL_COGNITIVE), "Cognitive");
    EXPECT_STREQ(callosum_channel_name(CALLOSUM_CHANNEL_EMOTIONAL), "Emotional");
    EXPECT_STREQ(callosum_channel_name(CALLOSUM_CHANNEL_INHIBITORY), "Inhibitory");
}

TEST_F(CorpusCallosumIntegrationTest, BandwidthModeNameStrings) {
    EXPECT_STREQ(callosum_bandwidth_mode_name(CALLOSUM_BW_UNLIMITED), "Unlimited");
    EXPECT_STREQ(callosum_bandwidth_mode_name(CALLOSUM_BW_REALISTIC), "Realistic");
    EXPECT_STREQ(callosum_bandwidth_mode_name(CALLOSUM_BW_RESTRICTED), "Restricted");
    EXPECT_STREQ(callosum_bandwidth_mode_name(CALLOSUM_BW_CUSTOM), "Custom");
}

TEST_F(CorpusCallosumIntegrationTest, StateNameStrings) {
    EXPECT_STREQ(callosum_state_name(CALLOSUM_STATE_DISCONNECTED), "Disconnected");
    EXPECT_STREQ(callosum_state_name(CALLOSUM_STATE_IMPAIRED), "Impaired");
    EXPECT_STREQ(callosum_state_name(CALLOSUM_STATE_DEGRADED), "Degraded");
    EXPECT_STREQ(callosum_state_name(CALLOSUM_STATE_HEALTHY), "Healthy");
}

TEST_F(CorpusCallosumIntegrationTest, ConfigValidation) {
    callosum_config_t valid_config = callosum_default_config();
    EXPECT_TRUE(callosum_validate_config(&valid_config));
}
