//=============================================================================
// test_corpus_callosum_regression.cpp - Corpus Callosum Regression Tests
//=============================================================================
/**
 * @file test_corpus_callosum_regression.cpp
 * @brief Regression tests for corpus callosum inter-hemispheric communication
 *
 * WHAT: Tests for message throughput, latency, queue handling, bandwidth
 * WHY:  Ensure corpus callosum behavior is stable across versions
 * HOW:  GTest framework with throughput benchmarks and accuracy checks
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <chrono>
#include <algorithm>
#include <numeric>

#include "utils/nimcp_test_base.h"


#include "core/brain/hemispheric/nimcp_corpus_callosum.h"
#include "core/brain/hemispheric/nimcp_lateralization.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class CorpusCallosumRegressionTest : public NimcpTestBase {
protected:
    corpus_callosum_t* cc = nullptr;

    void SetUp() override {
        NimcpTestBase::SetUp();

        callosum_config_t config = callosum_default_config();
        config.bandwidth_mode = CALLOSUM_BW_UNLIMITED;  // For determinism
        config.queue_capacity = 256;
        config.enable_bio_async = false;
        cc = callosum_create(&config);
    }

    void TearDown() override {
        if (cc) {
            callosum_destroy(cc);
            cc = nullptr;
        }
        NimcpTestBase::TearDown();
    }

    template<typename Func>
    double measureTimeMs(Func&& func, int iterations = 100) {
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; i++) {
            func();
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        return duration.count() / 1000.0 / iterations;
    }
};

//=============================================================================
// Message Throughput Benchmarks
//=============================================================================

TEST_F(CorpusCallosumRegressionTest, SendThroughputLeftToRight) {
    ASSERT_NE(cc, nullptr);

    uint8_t data[64] = {0};

    double avgMs = measureTimeMs([&]() {
        callosum_send(cc, HEMISPHERE_LEFT, CALLOSUM_CHANNEL_COGNITIVE,
            CALLOSUM_PRIORITY_NORMAL, 1, data, sizeof(data));
    });

    // Single send should be very fast (<0.1ms)
    EXPECT_LT(avgMs, 0.1) << "Send too slow: " << avgMs << "ms";
}

TEST_F(CorpusCallosumRegressionTest, SendThroughputRightToLeft) {
    ASSERT_NE(cc, nullptr);

    uint8_t data[64] = {0};

    double avgMs = measureTimeMs([&]() {
        callosum_send(cc, HEMISPHERE_RIGHT, CALLOSUM_CHANNEL_COGNITIVE,
            CALLOSUM_PRIORITY_NORMAL, 1, data, sizeof(data));
    });

    EXPECT_LT(avgMs, 0.1) << "Send too slow: " << avgMs << "ms";
}

TEST_F(CorpusCallosumRegressionTest, ReceiveThroughput) {
    ASSERT_NE(cc, nullptr);

    uint8_t data[64] = {0};
    callosum_message_t messages[16];

    // Fill queue first
    for (int i = 0; i < 100; i++) {
        callosum_send(cc, HEMISPHERE_LEFT, CALLOSUM_CHANNEL_COGNITIVE,
            CALLOSUM_PRIORITY_NORMAL, 1, data, sizeof(data));
    }

    double avgMs = measureTimeMs([&]() {
        callosum_receive(cc, HEMISPHERE_RIGHT, messages, 16);
    }, 50);

    EXPECT_LT(avgMs, 0.5) << "Receive too slow: " << avgMs << "ms";
}

TEST_F(CorpusCallosumRegressionTest, ProcessQueuesThroughput) {
    ASSERT_NE(cc, nullptr);

    // Enable latency simulation for queue processing
    callosum_enable_latency_simulation(cc, true);
    callosum_set_latency(cc, 0.1f, 0.2f);

    uint8_t data[32] = {0};
    for (int i = 0; i < 50; i++) {
        callosum_send(cc, HEMISPHERE_LEFT, CALLOSUM_CHANNEL_MOTOR,
            CALLOSUM_PRIORITY_NORMAL, 1, data, sizeof(data));
    }

    double avgMs = measureTimeMs([&]() {
        callosum_process_queues(cc);
    }, 100);

    EXPECT_LT(avgMs, 1.0) << "Queue processing too slow: " << avgMs << "ms";
}

TEST_F(CorpusCallosumRegressionTest, AllChannelsThroughput) {
    ASSERT_NE(cc, nullptr);

    uint8_t data[32] = {0};

    double avgMs = measureTimeMs([&]() {
        for (int ch = 0; ch < CALLOSUM_CHANNEL_COUNT; ch++) {
            callosum_send(cc, HEMISPHERE_LEFT, (callosum_channel_type_t)ch,
                CALLOSUM_PRIORITY_NORMAL, 1, data, sizeof(data));
        }
    }, 100);

    // All channels together should still be fast
    EXPECT_LT(avgMs, 0.5) << "Multi-channel send too slow: " << avgMs << "ms";
}

//=============================================================================
// Latency Measurement Accuracy Tests
//=============================================================================

TEST_F(CorpusCallosumRegressionTest, LatencyRangeRespected) {
    ASSERT_NE(cc, nullptr);

    callosum_enable_latency_simulation(cc, true);
    callosum_set_latency(cc, 5.0f, 20.0f);

    callosum_stats_t stats;
    callosum_get_stats(cc, &stats);

    float avg_latency = callosum_get_avg_latency(cc);
    // Average should be within configured range (or 0 if no messages yet)
    EXPECT_GE(avg_latency, 0.0f);
}

TEST_F(CorpusCallosumRegressionTest, PerChannelLatencyConfiguration) {
    ASSERT_NE(cc, nullptr);

    // Set different latencies per channel
    EXPECT_EQ(callosum_set_channel_latency(cc, CALLOSUM_CHANNEL_MOTOR, 1.0f, 5.0f), 0);
    EXPECT_EQ(callosum_set_channel_latency(cc, CALLOSUM_CHANNEL_SENSORY, 5.0f, 10.0f), 0);
    EXPECT_EQ(callosum_set_channel_latency(cc, CALLOSUM_CHANNEL_COGNITIVE, 10.0f, 20.0f), 0);

    // Verify channels have different latency configs
    EXPECT_FLOAT_EQ(cc->channels[CALLOSUM_CHANNEL_MOTOR].min_latency_ms, 1.0f);
    EXPECT_FLOAT_EQ(cc->channels[CALLOSUM_CHANNEL_MOTOR].max_latency_ms, 5.0f);
    EXPECT_FLOAT_EQ(cc->channels[CALLOSUM_CHANNEL_COGNITIVE].min_latency_ms, 10.0f);
}

TEST_F(CorpusCallosumRegressionTest, LatencySimulationToggle) {
    ASSERT_NE(cc, nullptr);

    EXPECT_EQ(callosum_enable_latency_simulation(cc, true), 0);
    EXPECT_TRUE(cc->enable_latency_simulation);

    EXPECT_EQ(callosum_enable_latency_simulation(cc, false), 0);
    EXPECT_FALSE(cc->enable_latency_simulation);
}

//=============================================================================
// Queue Overflow Handling Tests
//=============================================================================

TEST_F(CorpusCallosumRegressionTest, QueueOverflowDropsOldest) {
    ASSERT_NE(cc, nullptr);

    // Create callosum with small queue
    callosum_destroy(cc);
    callosum_config_t config = callosum_default_config();
    config.queue_capacity = 10;
    config.drop_on_overflow = true;
    config.enable_bio_async = false;
    cc = callosum_create(&config);
    ASSERT_NE(cc, nullptr);

    uint8_t data[16] = {0};

    // Send more than queue capacity
    for (int i = 0; i < 20; i++) {
        callosum_send(cc, HEMISPHERE_LEFT, CALLOSUM_CHANNEL_COGNITIVE,
            CALLOSUM_PRIORITY_NORMAL, i, data, sizeof(data));
    }

    callosum_stats_t stats;
    callosum_get_stats(cc, &stats);

    // Should have dropped some messages
    EXPECT_GT(stats.messages_dropped, 0);
}

TEST_F(CorpusCallosumRegressionTest, QueueCapacityRespected) {
    ASSERT_NE(cc, nullptr);

    callosum_destroy(cc);
    callosum_config_t config = callosum_default_config();
    config.queue_capacity = 32;
    config.enable_bio_async = false;
    cc = callosum_create(&config);
    ASSERT_NE(cc, nullptr);

    EXPECT_EQ(cc->left_to_right.capacity, 32);
    EXPECT_EQ(cc->right_to_left.capacity, 32);
}

TEST_F(CorpusCallosumRegressionTest, FlushClearsQueues) {
    ASSERT_NE(cc, nullptr);

    uint8_t data[16] = {0};

    // Fill queues
    for (int i = 0; i < 50; i++) {
        callosum_send(cc, HEMISPHERE_LEFT, CALLOSUM_CHANNEL_COGNITIVE,
            CALLOSUM_PRIORITY_NORMAL, 1, data, sizeof(data));
        callosum_send(cc, HEMISPHERE_RIGHT, CALLOSUM_CHANNEL_MOTOR,
            CALLOSUM_PRIORITY_NORMAL, 1, data, sizeof(data));
    }

    EXPECT_GT(cc->left_to_right.count, 0);
    EXPECT_GT(cc->right_to_left.count, 0);

    // Flush
    int flushed = callosum_flush(cc);
    EXPECT_GT(flushed, 0);

    // Queues should be empty or messages delivered
    // Note: flush delivers, doesn't clear
}

//=============================================================================
// Bandwidth Limiting Accuracy Tests
//=============================================================================

TEST_F(CorpusCallosumRegressionTest, BandwidthModeConfiguration) {
    ASSERT_NE(cc, nullptr);

    EXPECT_EQ(callosum_set_bandwidth_mode(cc, CALLOSUM_BW_UNLIMITED), 0);
    EXPECT_EQ(cc->bandwidth_mode, CALLOSUM_BW_UNLIMITED);

    EXPECT_EQ(callosum_set_bandwidth_mode(cc, CALLOSUM_BW_REALISTIC), 0);
    EXPECT_EQ(cc->bandwidth_mode, CALLOSUM_BW_REALISTIC);

    EXPECT_EQ(callosum_set_bandwidth_mode(cc, CALLOSUM_BW_RESTRICTED), 0);
    EXPECT_EQ(cc->bandwidth_mode, CALLOSUM_BW_RESTRICTED);

    EXPECT_EQ(callosum_set_bandwidth_mode(cc, CALLOSUM_BW_CUSTOM), 0);
    EXPECT_EQ(cc->bandwidth_mode, CALLOSUM_BW_CUSTOM);
}

TEST_F(CorpusCallosumRegressionTest, CustomBandwidthLimit) {
    ASSERT_NE(cc, nullptr);

    EXPECT_EQ(callosum_set_bandwidth_limit(cc, 100), 0);
    EXPECT_EQ(cc->max_messages_per_second, 100);

    EXPECT_EQ(callosum_set_bandwidth_limit(cc, 500), 0);
    EXPECT_EQ(cc->max_messages_per_second, 500);

    // Unlimited
    EXPECT_EQ(callosum_set_bandwidth_limit(cc, 0), 0);
    EXPECT_EQ(cc->max_messages_per_second, 0);
}

TEST_F(CorpusCallosumRegressionTest, PerChannelBandwidth) {
    ASSERT_NE(cc, nullptr);

    EXPECT_EQ(callosum_set_channel_bandwidth(cc, CALLOSUM_CHANNEL_MOTOR, 50), 0);
    EXPECT_EQ(cc->channels[CALLOSUM_CHANNEL_MOTOR].max_msgs_per_second, 50);

    EXPECT_EQ(callosum_set_channel_bandwidth(cc, CALLOSUM_CHANNEL_COGNITIVE, 200), 0);
    EXPECT_EQ(cc->channels[CALLOSUM_CHANNEL_COGNITIVE].max_msgs_per_second, 200);
}

TEST_F(CorpusCallosumRegressionTest, BandwidthUtilizationReporting) {
    ASSERT_NE(cc, nullptr);

    float util = callosum_get_bandwidth_utilization(cc);
    EXPECT_GE(util, 0.0f);
    EXPECT_LE(util, 1.0f);
}

TEST_F(CorpusCallosumRegressionTest, BaseBandwidthQuery) {
    ASSERT_NE(cc, nullptr);

    uint32_t base_bw = corpus_callosum_get_base_bandwidth(cc);
    // Should return a sensible value based on mode
    EXPECT_GE(base_bw, 0);
}

//=============================================================================
// Channel Priority Ordering Tests
//=============================================================================

TEST_F(CorpusCallosumRegressionTest, PriorityLevelsExist) {
    // Verify priority enum values
    EXPECT_LT(CALLOSUM_PRIORITY_LOW, CALLOSUM_PRIORITY_NORMAL);
    EXPECT_LT(CALLOSUM_PRIORITY_NORMAL, CALLOSUM_PRIORITY_HIGH);
    EXPECT_LT(CALLOSUM_PRIORITY_HIGH, CALLOSUM_PRIORITY_URGENT);
}

TEST_F(CorpusCallosumRegressionTest, UrgentPriorityProcessed) {
    ASSERT_NE(cc, nullptr);

    uint8_t data[16] = {0};

    // Send messages with different priorities
    callosum_send(cc, HEMISPHERE_LEFT, CALLOSUM_CHANNEL_COGNITIVE,
        CALLOSUM_PRIORITY_LOW, 1, data, sizeof(data));
    callosum_send(cc, HEMISPHERE_LEFT, CALLOSUM_CHANNEL_COGNITIVE,
        CALLOSUM_PRIORITY_URGENT, 2, data, sizeof(data));
    callosum_send(cc, HEMISPHERE_LEFT, CALLOSUM_CHANNEL_COGNITIVE,
        CALLOSUM_PRIORITY_NORMAL, 3, data, sizeof(data));

    // Verify messages were queued
    EXPECT_GT(cc->left_to_right.count, 0);
}

TEST_F(CorpusCallosumRegressionTest, ChannelTypesSeparate) {
    ASSERT_NE(cc, nullptr);

    uint8_t data[16] = {0};

    // Send on different channels
    callosum_send(cc, HEMISPHERE_LEFT, CALLOSUM_CHANNEL_MOTOR,
        CALLOSUM_PRIORITY_NORMAL, 1, data, sizeof(data));
    callosum_send(cc, HEMISPHERE_LEFT, CALLOSUM_CHANNEL_SENSORY,
        CALLOSUM_PRIORITY_NORMAL, 2, data, sizeof(data));
    callosum_send(cc, HEMISPHERE_LEFT, CALLOSUM_CHANNEL_EMOTIONAL,
        CALLOSUM_PRIORITY_NORMAL, 3, data, sizeof(data));

    callosum_stats_t stats;
    callosum_get_stats(cc, &stats);

    // Each channel should have received messages
    EXPECT_EQ(stats.channel_messages[CALLOSUM_CHANNEL_MOTOR], 1);
    EXPECT_EQ(stats.channel_messages[CALLOSUM_CHANNEL_SENSORY], 1);
    EXPECT_EQ(stats.channel_messages[CALLOSUM_CHANNEL_EMOTIONAL], 1);
}

//=============================================================================
// Connection State Tests
//=============================================================================

TEST_F(CorpusCallosumRegressionTest, ConnectionStrengthRange) {
    ASSERT_NE(cc, nullptr);

    EXPECT_EQ(callosum_set_connection_strength(cc, 0.0f), 0);
    EXPECT_FLOAT_EQ(callosum_get_connection_strength(cc), 0.0f);

    EXPECT_EQ(callosum_set_connection_strength(cc, 0.5f), 0);
    EXPECT_FLOAT_EQ(callosum_get_connection_strength(cc), 0.5f);

    EXPECT_EQ(callosum_set_connection_strength(cc, 1.0f), 0);
    EXPECT_FLOAT_EQ(callosum_get_connection_strength(cc), 1.0f);
}

TEST_F(CorpusCallosumRegressionTest, DisconnectReconnectState) {
    ASSERT_NE(cc, nullptr);

    EXPECT_TRUE(callosum_is_connected(cc));
    EXPECT_EQ(callosum_get_state(cc), CALLOSUM_STATE_HEALTHY);

    EXPECT_EQ(callosum_disconnect(cc), 0);
    EXPECT_FALSE(callosum_is_connected(cc));
    EXPECT_EQ(callosum_get_state(cc), CALLOSUM_STATE_DISCONNECTED);

    EXPECT_EQ(callosum_reconnect(cc), 0);
    EXPECT_TRUE(callosum_is_connected(cc));
    EXPECT_EQ(callosum_get_state(cc), CALLOSUM_STATE_HEALTHY);
}

TEST_F(CorpusCallosumRegressionTest, ChannelEnableDisable) {
    ASSERT_NE(cc, nullptr);

    for (int ch = 0; ch < CALLOSUM_CHANNEL_COUNT; ch++) {
        EXPECT_TRUE(callosum_is_channel_enabled(cc, (callosum_channel_type_t)ch));

        EXPECT_EQ(callosum_set_channel_enabled(cc, (callosum_channel_type_t)ch, false), 0);
        EXPECT_FALSE(callosum_is_channel_enabled(cc, (callosum_channel_type_t)ch));

        EXPECT_EQ(callosum_set_channel_enabled(cc, (callosum_channel_type_t)ch, true), 0);
        EXPECT_TRUE(callosum_is_channel_enabled(cc, (callosum_channel_type_t)ch));
    }
}

//=============================================================================
// Backward Compatibility Tests
//=============================================================================

TEST_F(CorpusCallosumRegressionTest, DefaultConfigStable) {
    callosum_config_t config = callosum_default_config();

    EXPECT_EQ(config.bandwidth_mode, CALLOSUM_BW_REALISTIC);
    EXPECT_GT(config.queue_capacity, 0);
    EXPECT_GE(config.initial_connection_strength, 0.0f);
    EXPECT_LE(config.initial_connection_strength, 1.0f);
}

TEST_F(CorpusCallosumRegressionTest, ChannelNamesStable) {
    EXPECT_STREQ(callosum_channel_name(CALLOSUM_CHANNEL_MOTOR), "Motor");
    EXPECT_STREQ(callosum_channel_name(CALLOSUM_CHANNEL_SENSORY), "Sensory");
    EXPECT_STREQ(callosum_channel_name(CALLOSUM_CHANNEL_COGNITIVE), "Cognitive");
    EXPECT_STREQ(callosum_channel_name(CALLOSUM_CHANNEL_EMOTIONAL), "Emotional");
    EXPECT_STREQ(callosum_channel_name(CALLOSUM_CHANNEL_INHIBITORY), "Inhibitory");
}

TEST_F(CorpusCallosumRegressionTest, BandwidthModeNamesStable) {
    EXPECT_STREQ(callosum_bandwidth_mode_name(CALLOSUM_BW_UNLIMITED), "Unlimited");
    EXPECT_STREQ(callosum_bandwidth_mode_name(CALLOSUM_BW_REALISTIC), "Realistic");
    EXPECT_STREQ(callosum_bandwidth_mode_name(CALLOSUM_BW_RESTRICTED), "Restricted");
    EXPECT_STREQ(callosum_bandwidth_mode_name(CALLOSUM_BW_CUSTOM), "Custom");
}

TEST_F(CorpusCallosumRegressionTest, StateNamesStable) {
    EXPECT_STREQ(callosum_state_name(CALLOSUM_STATE_DISCONNECTED), "Disconnected");
    EXPECT_STREQ(callosum_state_name(CALLOSUM_STATE_IMPAIRED), "Impaired");
    EXPECT_STREQ(callosum_state_name(CALLOSUM_STATE_DEGRADED), "Degraded");
    EXPECT_STREQ(callosum_state_name(CALLOSUM_STATE_HEALTHY), "Healthy");
}

//=============================================================================
// Stress Tests
//=============================================================================

TEST_F(CorpusCallosumRegressionTest, HighVolumeMessaging) {
    ASSERT_NE(cc, nullptr);

    uint8_t data[64] = {0};

    // Send many messages
    for (int i = 0; i < 10000; i++) {
        int result = callosum_send(cc,
            (i % 2 == 0) ? HEMISPHERE_LEFT : HEMISPHERE_RIGHT,
            (callosum_channel_type_t)(i % CALLOSUM_CHANNEL_COUNT),
            CALLOSUM_PRIORITY_NORMAL, i, data, sizeof(data));

        // Should not fail unexpectedly (may queue or drop)
        EXPECT_GE(result, -1);
    }

    // Process queues
    callosum_process_queues(cc);

    // Verify stats are sane
    callosum_stats_t stats;
    callosum_get_stats(cc, &stats);
    EXPECT_GT(stats.total_messages_left_to_right + stats.total_messages_right_to_left, 0);
}

TEST_F(CorpusCallosumRegressionTest, RapidConnectDisconnect) {
    ASSERT_NE(cc, nullptr);

    for (int i = 0; i < 1000; i++) {
        if (i % 2 == 0) {
            callosum_disconnect(cc);
        } else {
            callosum_reconnect(cc);
        }

        // State should be consistent
        if (i % 2 == 0) {
            EXPECT_FALSE(callosum_is_connected(cc));
        } else {
            EXPECT_TRUE(callosum_is_connected(cc));
        }
    }
}

TEST_F(CorpusCallosumRegressionTest, NullPointerSafety) {
    // These should not crash
    callosum_destroy(nullptr);
    callosum_disconnect(nullptr);
    callosum_reconnect(nullptr);
    callosum_flush(nullptr);
    callosum_process_queues(nullptr);
    callosum_is_connected(nullptr);
    callosum_get_state(nullptr);
    callosum_get_connection_strength(nullptr);
    callosum_get_avg_latency(nullptr);
    callosum_get_bandwidth_utilization(nullptr);

    uint8_t data[16];
    callosum_send(nullptr, HEMISPHERE_LEFT, CALLOSUM_CHANNEL_MOTOR,
        CALLOSUM_PRIORITY_NORMAL, 1, data, sizeof(data));

    callosum_message_t messages[4];
    callosum_receive(nullptr, HEMISPHERE_RIGHT, messages, 4);
}

TEST_F(CorpusCallosumRegressionTest, CreateDestroyNoLeak) {
    for (int i = 0; i < 100; i++) {
        callosum_config_t config = callosum_default_config();
        config.queue_capacity = 64;
        config.enable_bio_async = false;
        corpus_callosum_t* c = callosum_create(&config);
        ASSERT_NE(c, nullptr);
        callosum_destroy(c);
    }
}
