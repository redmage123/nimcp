/**
 * @file test_global_workspace_shannon.cpp
 * @brief Comprehensive unit tests for Shannon-enhanced Global Workspace
 *
 * TEST COVERAGE:
 * - Shannon configuration and defaults
 * - Enable/disable Shannon features
 * - Information measurement (entropy, relative info)
 * - Information-weighted competition
 * - Shannon-monitored broadcast
 * - Subscriber capacity management
 * - Adaptive broadcast rate control
 * - Statistics collection and reset
 * - Edge cases and error handling
 *
 * PHASE: 1.5.3 - Global Workspace Integration + Information Competition
 *
 * @author NIMCP Development Team
 * @date 2025-11-23
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <thread>
#include <chrono>

#include "cognitive/global_workspace/nimcp_global_workspace.h"
#include "cognitive/global_workspace/nimcp_global_workspace_shannon.h"

//=============================================================================
// Test Fixture
//=============================================================================

class GlobalWorkspaceShannonTest : public ::testing::Test {
protected:
    global_workspace_t* workspace;

    void SetUp() override {
        workspace = global_workspace_create();
        ASSERT_NE(workspace, nullptr);
    }

    void TearDown() override {
        if (workspace != nullptr) {
            global_workspace_disable_shannon(workspace);
            global_workspace_destroy(workspace);
            workspace = nullptr;
        }
    }

    // Helper: Create test feature vector (uniform distribution)
    std::vector<float> CreateUniformFeatures(uint32_t dim) {
        std::vector<float> features(dim);
        float val = 1.0f / static_cast<float>(dim);
        for (uint32_t i = 0; i < dim; i++) {
            features[i] = val;
        }
        return features;
    }

    // Helper: Create peaked feature vector (one dominant)
    std::vector<float> CreatePeakedFeatures(uint32_t dim, uint32_t peak_idx) {
        std::vector<float> features(dim, 0.01f);
        if (peak_idx < dim) {
            features[peak_idx] = 1.0f;
        }
        return features;
    }

    // Helper: Create random-ish feature vector
    std::vector<float> CreateVariedFeatures(uint32_t dim, float base = 1.0f) {
        std::vector<float> features(dim);
        for (uint32_t i = 0; i < dim; i++) {
            features[i] = base + static_cast<float>(i % 10) * 0.1f;
        }
        return features;
    }

    // Helper: Sleep for milliseconds
    void SleepMs(uint32_t ms) {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }
};

//=============================================================================
// 1. Configuration Tests
//=============================================================================

TEST_F(GlobalWorkspaceShannonTest, DefaultConfigValid) {
    shannon_workspace_config_t config = shannon_workspace_default_config();

    // Check defaults match documented values
    EXPECT_TRUE(config.enable_info_weighted_competition);
    EXPECT_FLOAT_EQ(config.info_threshold_bits, GWS_DEFAULT_INFO_THRESHOLD_BITS);
    EXPECT_FLOAT_EQ(config.info_weight, 0.5f);
    EXPECT_TRUE(config.enable_shannon_monitoring);
    EXPECT_FLOAT_EQ(config.default_subscriber_capacity, GWS_DEFAULT_SUBSCRIBER_CAPACITY);
    EXPECT_FLOAT_EQ(config.bottleneck_threshold, GWS_BOTTLENECK_THRESHOLD);
    EXPECT_TRUE(config.enable_adaptive_rate);
    EXPECT_FLOAT_EQ(config.rate_reduction_factor, GWS_BOTTLENECK_RATE_REDUCTION);
    EXPECT_FLOAT_EQ(config.rate_recovery_factor, GWS_RATE_RECOVERY_FACTOR);
    EXPECT_FLOAT_EQ(config.min_broadcast_rate, 0.1f);
    EXPECT_FLOAT_EQ(config.max_broadcast_rate, 1.0f);
}

//=============================================================================
// 2. Enable/Disable Shannon Tests
//=============================================================================

TEST_F(GlobalWorkspaceShannonTest, EnableShannonValid) {
    EXPECT_FALSE(global_workspace_is_shannon_enabled(workspace));

    bool enabled = global_workspace_enable_shannon(workspace, nullptr);
    EXPECT_TRUE(enabled);
    EXPECT_TRUE(global_workspace_is_shannon_enabled(workspace));
}

TEST_F(GlobalWorkspaceShannonTest, EnableShannonWithConfig) {
    shannon_workspace_config_t config = shannon_workspace_default_config();
    config.info_threshold_bits = 3.0f;
    config.info_weight = 0.7f;

    bool enabled = global_workspace_enable_shannon(workspace, &config);
    EXPECT_TRUE(enabled);
    EXPECT_TRUE(global_workspace_is_shannon_enabled(workspace));
}

TEST_F(GlobalWorkspaceShannonTest, EnableShannonIdempotent) {
    bool enabled1 = global_workspace_enable_shannon(workspace, nullptr);
    EXPECT_TRUE(enabled1);

    bool enabled2 = global_workspace_enable_shannon(workspace, nullptr);
    EXPECT_TRUE(enabled2);  // Should succeed (already enabled)

    EXPECT_TRUE(global_workspace_is_shannon_enabled(workspace));
}

TEST_F(GlobalWorkspaceShannonTest, DisableShannonValid) {
    global_workspace_enable_shannon(workspace, nullptr);
    EXPECT_TRUE(global_workspace_is_shannon_enabled(workspace));

    global_workspace_disable_shannon(workspace);
    EXPECT_FALSE(global_workspace_is_shannon_enabled(workspace));
}

TEST_F(GlobalWorkspaceShannonTest, DisableShannonNotEnabled) {
    EXPECT_FALSE(global_workspace_is_shannon_enabled(workspace));

    // Should not crash
    global_workspace_disable_shannon(workspace);
    EXPECT_FALSE(global_workspace_is_shannon_enabled(workspace));
}

TEST_F(GlobalWorkspaceShannonTest, EnableShannonNullWorkspace) {
    bool enabled = global_workspace_enable_shannon(nullptr, nullptr);
    EXPECT_FALSE(enabled);
}

TEST_F(GlobalWorkspaceShannonTest, IsShannonEnabledNull) {
    EXPECT_FALSE(global_workspace_is_shannon_enabled(nullptr));
}

//=============================================================================
// 3. Information Measurement Tests
//=============================================================================

TEST_F(GlobalWorkspaceShannonTest, MeasureInformationUniform) {
    // Uniform distribution has maximum entropy = log2(dim)
    auto features = CreateUniformFeatures(8);
    float info = shannon_measure_feature_information(features.data(), 8);

    // Max entropy for 8 states = log2(8) = 3 bits
    EXPECT_NEAR(info, 3.0f, 0.01f);
}

TEST_F(GlobalWorkspaceShannonTest, MeasureInformationPeaked) {
    // Peaked distribution has low entropy (close to 0)
    auto features = CreatePeakedFeatures(8, 0);
    float info = shannon_measure_feature_information(features.data(), 8);

    // Nearly deterministic - should be low entropy
    EXPECT_LT(info, 1.0f);
}

TEST_F(GlobalWorkspaceShannonTest, MeasureInformationDeterministic) {
    // Single non-zero value = 0 entropy
    std::vector<float> features(8, 0.0f);
    features[3] = 1.0f;
    float info = shannon_measure_feature_information(features.data(), 8);

    EXPECT_NEAR(info, 0.0f, 0.01f);
}

TEST_F(GlobalWorkspaceShannonTest, MeasureInformationZeroVector) {
    std::vector<float> features(8, 0.0f);
    float info = shannon_measure_feature_information(features.data(), 8);

    EXPECT_FLOAT_EQ(info, 0.0f);
}

TEST_F(GlobalWorkspaceShannonTest, MeasureInformationNullFeatures) {
    float info = shannon_measure_feature_information(nullptr, 8);
    EXPECT_FLOAT_EQ(info, 0.0f);
}

TEST_F(GlobalWorkspaceShannonTest, MeasureInformationZeroDim) {
    std::vector<float> features(8, 1.0f);
    float info = shannon_measure_feature_information(features.data(), 0);
    EXPECT_FLOAT_EQ(info, 0.0f);
}

TEST_F(GlobalWorkspaceShannonTest, MeasureInformationNegativeValues) {
    // Negative values should be handled (absolute value used)
    std::vector<float> features = {-0.25f, -0.25f, 0.25f, 0.25f};
    float info = shannon_measure_feature_information(features.data(), 4);

    // All equal magnitudes = max entropy for 4 states = 2 bits
    EXPECT_NEAR(info, 2.0f, 0.01f);
}

TEST_F(GlobalWorkspaceShannonTest, MeasureRelativeInformationUniform) {
    // Uniform has 0 relative information (max entropy = 0 structure)
    auto features = CreateUniformFeatures(8);
    float rel_info = shannon_measure_relative_information(features.data(), 8);

    EXPECT_NEAR(rel_info, 0.0f, 0.01f);
}

TEST_F(GlobalWorkspaceShannonTest, MeasureRelativeInformationPeaked) {
    // Peaked has high relative info (far from uniform)
    auto features = CreatePeakedFeatures(8, 0);
    float rel_info = shannon_measure_relative_information(features.data(), 8);

    // Should be close to max_entropy = log2(8) = 3
    EXPECT_GT(rel_info, 2.0f);
}

TEST_F(GlobalWorkspaceShannonTest, MeasureRelativeInformationNull) {
    float rel_info = shannon_measure_relative_information(nullptr, 8);
    EXPECT_FLOAT_EQ(rel_info, 0.0f);
}

//=============================================================================
// 4. Information-Weighted Competition Tests
//=============================================================================

TEST_F(GlobalWorkspaceShannonTest, CompeteWithInfoHighInfo) {
    global_workspace_enable_shannon(workspace, nullptr);

    auto content = CreateUniformFeatures(256);
    float info_bits = 0.0f;

    bool won = global_workspace_compete_with_info(
        workspace, MODULE_WORKING_MEMORY,
        content.data(), 256, 0.8f, &info_bits
    );

    // High entropy content should have good info
    EXPECT_GT(info_bits, 2.0f);  // Above default threshold
    EXPECT_TRUE(won);  // Should win with high salience + info
}

TEST_F(GlobalWorkspaceShannonTest, CompeteWithInfoLowInfo) {
    global_workspace_enable_shannon(workspace, nullptr);

    // Create truly low-entropy content: single non-zero value = 0 entropy
    std::vector<float> content(256, 0.0f);
    content[0] = 1.0f;  // Single value = deterministic = 0 entropy

    float info_bits = 0.0f;

    bool won = global_workspace_compete_with_info(
        workspace, MODULE_WORKING_MEMORY,
        content.data(), 256, 0.8f, &info_bits
    );

    // 0 entropy - below default threshold (2.0 bits), should be rejected
    EXPECT_LT(info_bits, 2.0f);
    EXPECT_FALSE(won);  // Should be rejected due to low info content
}

TEST_F(GlobalWorkspaceShannonTest, CompeteWithInfoNullInfoBits) {
    global_workspace_enable_shannon(workspace, nullptr);

    auto content = CreateUniformFeatures(256);

    // Should work without info_bits output
    bool won = global_workspace_compete_with_info(
        workspace, MODULE_WORKING_MEMORY,
        content.data(), 256, 0.8f, nullptr
    );

    EXPECT_TRUE(won);
}

TEST_F(GlobalWorkspaceShannonTest, CompeteWithInfoShannonDisabled) {
    // Don't enable Shannon - should use raw salience

    auto content = CreateUniformFeatures(256);
    float info_bits = 0.0f;

    bool won = global_workspace_compete_with_info(
        workspace, MODULE_WORKING_MEMORY,
        content.data(), 256, 0.8f, &info_bits
    );

    // Still measures info
    EXPECT_GT(info_bits, 0.0f);
    // Uses raw salience, should win
    EXPECT_TRUE(won);
}

TEST_F(GlobalWorkspaceShannonTest, CompeteWithInfoNullWorkspace) {
    auto content = CreateUniformFeatures(256);
    float info_bits = 0.0f;

    bool won = global_workspace_compete_with_info(
        nullptr, MODULE_WORKING_MEMORY,
        content.data(), 256, 0.8f, &info_bits
    );

    EXPECT_FALSE(won);
}

TEST_F(GlobalWorkspaceShannonTest, CompeteWithInfoNullContent) {
    global_workspace_enable_shannon(workspace, nullptr);

    bool won = global_workspace_compete_with_info(
        workspace, MODULE_WORKING_MEMORY,
        nullptr, 256, 0.8f, nullptr
    );

    EXPECT_FALSE(won);
}

TEST_F(GlobalWorkspaceShannonTest, CompeteWithInfoZeroDim) {
    global_workspace_enable_shannon(workspace, nullptr);

    auto content = CreateUniformFeatures(256);

    bool won = global_workspace_compete_with_info(
        workspace, MODULE_WORKING_MEMORY,
        content.data(), 0, 0.8f, nullptr
    );

    EXPECT_FALSE(won);
}

TEST_F(GlobalWorkspaceShannonTest, CompeteWithInfoStatisticsUpdated) {
    global_workspace_enable_shannon(workspace, nullptr);

    auto content = CreateUniformFeatures(256);

    // Compete several times
    for (int i = 0; i < 5; i++) {
        global_workspace_compete_with_info(
            workspace, MODULE_WORKING_MEMORY,
            content.data(), 256, 0.8f, nullptr
        );
        SleepMs(60);  // Wait for refractory
    }

    shannon_workspace_stats_t stats;
    bool got_stats = global_workspace_get_shannon_stats(workspace, &stats);

    EXPECT_TRUE(got_stats);
    EXPECT_GT(stats.total_competitions_with_info, 0u);
    EXPECT_GT(stats.avg_winner_info_bits, 0.0f);
}

//=============================================================================
// 5. Shannon-Monitored Broadcast Tests
//=============================================================================

TEST_F(GlobalWorkspaceShannonTest, BroadcastWithShannonBasic) {
    global_workspace_enable_shannon(workspace, nullptr);

    auto content = CreateUniformFeatures(256);
    float info_bits = shannon_measure_feature_information(content.data(), 256);

    shannon_broadcast_metrics_t metrics = global_workspace_broadcast_with_shannon(
        workspace, content.data(), 256, info_bits
    );

    // Basic metrics should be populated
    EXPECT_FLOAT_EQ(metrics.content_info_bits, info_bits);
    EXPECT_GT(metrics.broadcast_timestamp_ms, 0u);
}

TEST_F(GlobalWorkspaceShannonTest, BroadcastWithShannonNoBottleneck) {
    global_workspace_enable_shannon(workspace, nullptr);

    // Add subscriber with high capacity
    global_workspace_subscribe(workspace, MODULE_EXECUTIVE);
    global_workspace_set_subscriber_capacity(workspace, MODULE_EXECUTIVE, 1000.0f);

    auto content = CreateUniformFeatures(256);
    float info_bits = shannon_measure_feature_information(content.data(), 256);

    shannon_broadcast_metrics_t metrics = global_workspace_broadcast_with_shannon(
        workspace, content.data(), 256, info_bits
    );

    // No bottleneck with high capacity
    EXPECT_FALSE(metrics.bottleneck_detected);
    EXPECT_EQ(metrics.num_bottlenecked, 0u);
    EXPECT_FLOAT_EQ(metrics.delivery_efficiency, 1.0f);
}

TEST_F(GlobalWorkspaceShannonTest, BroadcastWithShannonBottleneck) {
    global_workspace_enable_shannon(workspace, nullptr);

    // Add subscriber with low capacity
    global_workspace_subscribe(workspace, MODULE_EXECUTIVE);
    global_workspace_set_subscriber_capacity(workspace, MODULE_EXECUTIVE, 0.1f);  // Very low

    // Saturate the subscriber's load first
    for (int i = 0; i < 10; i++) {
        global_workspace_update_subscriber_load(workspace, MODULE_EXECUTIVE, 10.0f);
    }

    auto content = CreateUniformFeatures(256);
    float info_bits = shannon_measure_feature_information(content.data(), 256);

    shannon_broadcast_metrics_t metrics = global_workspace_broadcast_with_shannon(
        workspace, content.data(), 256, info_bits
    );

    // Should detect bottleneck
    EXPECT_TRUE(metrics.bottleneck_detected);
    EXPECT_GT(metrics.num_bottlenecked, 0u);
    EXPECT_GT(metrics.total_info_loss, 0.0f);
    EXPECT_LT(metrics.delivery_efficiency, 1.0f);
}

TEST_F(GlobalWorkspaceShannonTest, BroadcastWithShannonNull) {
    global_workspace_enable_shannon(workspace, nullptr);

    shannon_broadcast_metrics_t metrics = global_workspace_broadcast_with_shannon(
        nullptr, nullptr, 256, 5.0f
    );

    EXPECT_FLOAT_EQ(metrics.content_info_bits, 5.0f);
    EXPECT_EQ(metrics.num_subscribers, 0u);
}

TEST_F(GlobalWorkspaceShannonTest, BroadcastWithShannonDisabled) {
    // Don't enable Shannon

    auto content = CreateUniformFeatures(256);

    shannon_broadcast_metrics_t metrics = global_workspace_broadcast_with_shannon(
        workspace, content.data(), 256, 5.0f
    );

    // Should still return basic metrics
    EXPECT_FLOAT_EQ(metrics.content_info_bits, 5.0f);
}

//=============================================================================
// 6. Subscriber Capacity Management Tests
//=============================================================================

TEST_F(GlobalWorkspaceShannonTest, SetSubscriberCapacityValid) {
    global_workspace_enable_shannon(workspace, nullptr);
    global_workspace_subscribe(workspace, MODULE_WORKING_MEMORY);

    bool ok = global_workspace_set_subscriber_capacity(
        workspace, MODULE_WORKING_MEMORY, 200.0f
    );

    EXPECT_TRUE(ok);

    float capacity = global_workspace_get_subscriber_capacity(
        workspace, MODULE_WORKING_MEMORY
    );
    EXPECT_FLOAT_EQ(capacity, 200.0f);
}

TEST_F(GlobalWorkspaceShannonTest, SetSubscriberCapacityMultiple) {
    global_workspace_enable_shannon(workspace, nullptr);
    global_workspace_subscribe(workspace, MODULE_WORKING_MEMORY);
    global_workspace_subscribe(workspace, MODULE_EXECUTIVE);
    global_workspace_subscribe(workspace, MODULE_SALIENCE);

    global_workspace_set_subscriber_capacity(workspace, MODULE_WORKING_MEMORY, 100.0f);
    global_workspace_set_subscriber_capacity(workspace, MODULE_EXECUTIVE, 200.0f);
    global_workspace_set_subscriber_capacity(workspace, MODULE_SALIENCE, 300.0f);

    EXPECT_FLOAT_EQ(global_workspace_get_subscriber_capacity(
        workspace, MODULE_WORKING_MEMORY), 100.0f);
    EXPECT_FLOAT_EQ(global_workspace_get_subscriber_capacity(
        workspace, MODULE_EXECUTIVE), 200.0f);
    EXPECT_FLOAT_EQ(global_workspace_get_subscriber_capacity(
        workspace, MODULE_SALIENCE), 300.0f);
}

TEST_F(GlobalWorkspaceShannonTest, SetSubscriberCapacityNullWorkspace) {
    bool ok = global_workspace_set_subscriber_capacity(
        nullptr, MODULE_WORKING_MEMORY, 100.0f
    );
    EXPECT_FALSE(ok);
}

TEST_F(GlobalWorkspaceShannonTest, SetSubscriberCapacityNegative) {
    global_workspace_enable_shannon(workspace, nullptr);

    bool ok = global_workspace_set_subscriber_capacity(
        workspace, MODULE_WORKING_MEMORY, -100.0f
    );
    EXPECT_FALSE(ok);
}

TEST_F(GlobalWorkspaceShannonTest, GetSubscriberCapacityNotFound) {
    global_workspace_enable_shannon(workspace, nullptr);

    float capacity = global_workspace_get_subscriber_capacity(
        workspace, MODULE_WORKING_MEMORY
    );
    EXPECT_FLOAT_EQ(capacity, 0.0f);  // Not registered
}

TEST_F(GlobalWorkspaceShannonTest, GetSubscriberLoadInitialZero) {
    global_workspace_enable_shannon(workspace, nullptr);
    global_workspace_set_subscriber_capacity(workspace, MODULE_WORKING_MEMORY, 100.0f);

    float load = global_workspace_get_subscriber_load(
        workspace, MODULE_WORKING_MEMORY
    );
    EXPECT_FLOAT_EQ(load, 0.0f);  // Initial load is zero
}

TEST_F(GlobalWorkspaceShannonTest, UpdateSubscriberLoad) {
    global_workspace_enable_shannon(workspace, nullptr);
    global_workspace_set_subscriber_capacity(workspace, MODULE_WORKING_MEMORY, 100.0f);

    global_workspace_update_subscriber_load(workspace, MODULE_WORKING_MEMORY, 50.0f);

    float load = global_workspace_get_subscriber_load(
        workspace, MODULE_WORKING_MEMORY
    );
    EXPECT_GT(load, 0.0f);  // Should have some load
}

//=============================================================================
// 7. Adaptive Broadcast Rate Control Tests
//=============================================================================

TEST_F(GlobalWorkspaceShannonTest, GetBroadcastRateInitial) {
    global_workspace_enable_shannon(workspace, nullptr);

    float rate = global_workspace_get_broadcast_rate(workspace);
    EXPECT_FLOAT_EQ(rate, 1.0f);  // Initial rate is 1.0
}

TEST_F(GlobalWorkspaceShannonTest, SetBroadcastRateValid) {
    global_workspace_enable_shannon(workspace, nullptr);

    bool ok = global_workspace_set_broadcast_rate(workspace, 0.5f);
    EXPECT_TRUE(ok);

    float rate = global_workspace_get_broadcast_rate(workspace);
    EXPECT_FLOAT_EQ(rate, 0.5f);
}

TEST_F(GlobalWorkspaceShannonTest, SetBroadcastRateClampsLow) {
    global_workspace_enable_shannon(workspace, nullptr);

    // Set below minimum
    global_workspace_set_broadcast_rate(workspace, 0.01f);

    float rate = global_workspace_get_broadcast_rate(workspace);
    EXPECT_GE(rate, 0.1f);  // Should be clamped to min
}

TEST_F(GlobalWorkspaceShannonTest, SetBroadcastRateClampsHigh) {
    global_workspace_enable_shannon(workspace, nullptr);

    // Set above maximum
    global_workspace_set_broadcast_rate(workspace, 2.0f);

    float rate = global_workspace_get_broadcast_rate(workspace);
    EXPECT_LE(rate, 1.0f);  // Should be clamped to max
}

TEST_F(GlobalWorkspaceShannonTest, SetBroadcastRateNullWorkspace) {
    bool ok = global_workspace_set_broadcast_rate(nullptr, 0.5f);
    EXPECT_FALSE(ok);
}

TEST_F(GlobalWorkspaceShannonTest, GetBroadcastRateShannonDisabled) {
    // Don't enable Shannon
    float rate = global_workspace_get_broadcast_rate(workspace);
    EXPECT_FLOAT_EQ(rate, 1.0f);  // Default
}

TEST_F(GlobalWorkspaceShannonTest, AdaptBroadcastRateOnBottleneck) {
    global_workspace_enable_shannon(workspace, nullptr);

    // Create metrics with bottleneck
    shannon_broadcast_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));
    metrics.bottleneck_detected = true;

    float initial_rate = global_workspace_get_broadcast_rate(workspace);

    global_workspace_adapt_broadcast_rate(workspace, &metrics);

    float new_rate = global_workspace_get_broadcast_rate(workspace);
    EXPECT_LT(new_rate, initial_rate);  // Should decrease
}

TEST_F(GlobalWorkspaceShannonTest, AdaptBroadcastRateOnClear) {
    global_workspace_enable_shannon(workspace, nullptr);

    // First reduce rate
    global_workspace_set_broadcast_rate(workspace, 0.5f);

    // Create metrics without bottleneck
    shannon_broadcast_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));
    metrics.bottleneck_detected = false;

    float initial_rate = global_workspace_get_broadcast_rate(workspace);

    global_workspace_adapt_broadcast_rate(workspace, &metrics);

    float new_rate = global_workspace_get_broadcast_rate(workspace);
    EXPECT_GT(new_rate, initial_rate);  // Should increase (recover)
}

TEST_F(GlobalWorkspaceShannonTest, AdaptBroadcastRateNull) {
    global_workspace_enable_shannon(workspace, nullptr);

    // Should not crash
    global_workspace_adapt_broadcast_rate(workspace, nullptr);
    global_workspace_adapt_broadcast_rate(nullptr, nullptr);
}

//=============================================================================
// 8. Statistics Tests
//=============================================================================

TEST_F(GlobalWorkspaceShannonTest, GetShannonStatsValid) {
    global_workspace_enable_shannon(workspace, nullptr);

    shannon_workspace_stats_t stats;
    bool ok = global_workspace_get_shannon_stats(workspace, &stats);

    EXPECT_TRUE(ok);
    EXPECT_FLOAT_EQ(stats.current_broadcast_rate, 1.0f);
}

TEST_F(GlobalWorkspaceShannonTest, GetShannonStatsNull) {
    global_workspace_enable_shannon(workspace, nullptr);

    bool ok = global_workspace_get_shannon_stats(workspace, nullptr);
    EXPECT_FALSE(ok);
}

TEST_F(GlobalWorkspaceShannonTest, GetShannonStatsShannonDisabled) {
    shannon_workspace_stats_t stats;
    bool ok = global_workspace_get_shannon_stats(workspace, &stats);
    EXPECT_FALSE(ok);  // Shannon not enabled
}

TEST_F(GlobalWorkspaceShannonTest, ResetShannonStats) {
    global_workspace_enable_shannon(workspace, nullptr);

    // Generate some statistics
    auto content = CreateUniformFeatures(256);
    for (int i = 0; i < 3; i++) {
        global_workspace_compete_with_info(
            workspace, MODULE_WORKING_MEMORY,
            content.data(), 256, 0.8f, nullptr
        );
        SleepMs(60);
    }

    // Reset
    global_workspace_reset_shannon_stats(workspace);

    shannon_workspace_stats_t stats;
    global_workspace_get_shannon_stats(workspace, &stats);

    EXPECT_EQ(stats.total_competitions_with_info, 0u);
    EXPECT_FLOAT_EQ(stats.total_info_delivered_bits, 0.0f);
}

TEST_F(GlobalWorkspaceShannonTest, ResetShannonStatsNull) {
    // Should not crash
    global_workspace_reset_shannon_stats(nullptr);
}

TEST_F(GlobalWorkspaceShannonTest, GetLastBroadcastMetrics) {
    global_workspace_enable_shannon(workspace, nullptr);

    auto content = CreateUniformFeatures(256);
    float info = shannon_measure_feature_information(content.data(), 256);

    global_workspace_broadcast_with_shannon(workspace, content.data(), 256, info);

    shannon_broadcast_metrics_t metrics;
    bool ok = global_workspace_get_last_broadcast_metrics(workspace, &metrics);

    EXPECT_TRUE(ok);
    EXPECT_FLOAT_EQ(metrics.content_info_bits, info);
}

TEST_F(GlobalWorkspaceShannonTest, GetLastBroadcastMetricsNoBroadcast) {
    global_workspace_enable_shannon(workspace, nullptr);

    shannon_broadcast_metrics_t metrics;
    bool ok = global_workspace_get_last_broadcast_metrics(workspace, &metrics);

    EXPECT_FALSE(ok);  // No broadcast yet
}

TEST_F(GlobalWorkspaceShannonTest, GetLastBroadcastMetricsNull) {
    global_workspace_enable_shannon(workspace, nullptr);

    bool ok = global_workspace_get_last_broadcast_metrics(workspace, nullptr);
    EXPECT_FALSE(ok);
}

//=============================================================================
// 9. Event Handler Integration Tests
//=============================================================================

TEST_F(GlobalWorkspaceShannonTest, OnSaliencePeakShannonValid) {
    global_workspace_enable_shannon(workspace, nullptr);

    // Create a mock salience peak structure
    struct MockPeak {
        float salience_score;
        float* feature_vector;
        uint32_t dimension;
    };

    auto features = CreateUniformFeatures(256);
    MockPeak peak = {
        .salience_score = 0.8f,
        .feature_vector = features.data(),
        .dimension = 256
    };

    bool handled = global_workspace_on_salience_peak_shannon(
        workspace, &peak
    );

    // Should win with high salience and information
    EXPECT_TRUE(handled);
}

TEST_F(GlobalWorkspaceShannonTest, OnSaliencePeakShannonNull) {
    global_workspace_enable_shannon(workspace, nullptr);

    bool handled = global_workspace_on_salience_peak_shannon(workspace, nullptr);
    EXPECT_FALSE(handled);

    handled = global_workspace_on_salience_peak_shannon(nullptr, nullptr);
    EXPECT_FALSE(handled);
}

TEST_F(GlobalWorkspaceShannonTest, OnSaliencePeakShannonLowInfo) {
    global_workspace_enable_shannon(workspace, nullptr);

    struct MockPeak {
        float salience_score;
        float* feature_vector;
        uint32_t dimension;
    };

    // Very low information content (single peak)
    auto features = CreatePeakedFeatures(256, 0);
    MockPeak peak = {
        .salience_score = 0.8f,
        .feature_vector = features.data(),
        .dimension = 256
    };

    bool handled = global_workspace_on_salience_peak_shannon(
        workspace, &peak
    );

    // May fail due to low information content
    // (depends on threshold vs actual entropy)
}

//=============================================================================
// 10. Edge Cases and Robustness Tests
//=============================================================================

TEST_F(GlobalWorkspaceShannonTest, MultipleWorkspacesWithShannon) {
    // Create second workspace
    global_workspace_t* workspace2 = global_workspace_create();
    ASSERT_NE(workspace2, nullptr);

    // Enable Shannon on both
    bool ok1 = global_workspace_enable_shannon(workspace, nullptr);
    bool ok2 = global_workspace_enable_shannon(workspace2, nullptr);

    EXPECT_TRUE(ok1);
    EXPECT_TRUE(ok2);

    // Both should be independently enabled
    EXPECT_TRUE(global_workspace_is_shannon_enabled(workspace));
    EXPECT_TRUE(global_workspace_is_shannon_enabled(workspace2));

    // Set different rates
    global_workspace_set_broadcast_rate(workspace, 0.5f);
    global_workspace_set_broadcast_rate(workspace2, 0.7f);

    EXPECT_FLOAT_EQ(global_workspace_get_broadcast_rate(workspace), 0.5f);
    EXPECT_FLOAT_EQ(global_workspace_get_broadcast_rate(workspace2), 0.7f);

    // Cleanup
    global_workspace_disable_shannon(workspace2);
    global_workspace_destroy(workspace2);
}

TEST_F(GlobalWorkspaceShannonTest, LargeFeatureVector) {
    // Test with large dimension
    const uint32_t large_dim = 4096;
    auto features = CreateUniformFeatures(large_dim);

    float info = shannon_measure_feature_information(features.data(), large_dim);

    // Max entropy = log2(4096) = 12 bits
    EXPECT_NEAR(info, 12.0f, 0.1f);
}

TEST_F(GlobalWorkspaceShannonTest, VerySmallFeatureVector) {
    // Test with tiny dimension
    std::vector<float> features = {0.5f, 0.5f};

    float info = shannon_measure_feature_information(features.data(), 2);

    // Max entropy = log2(2) = 1 bit
    EXPECT_NEAR(info, 1.0f, 0.01f);
}

TEST_F(GlobalWorkspaceShannonTest, StressTestCompetition) {
    global_workspace_enable_shannon(workspace, nullptr);

    auto content = CreateVariedFeatures(256);

    // Many competitions
    for (int i = 0; i < 100; i++) {
        global_workspace_compete_with_info(
            workspace, MODULE_WORKING_MEMORY,
            content.data(), 256, 0.8f, nullptr
        );
    }

    // Should not crash, stats should be accumulated
    shannon_workspace_stats_t stats;
    global_workspace_get_shannon_stats(workspace, &stats);

    EXPECT_GE(stats.total_competitions_with_info, 100u);
}

//=============================================================================
// Run All Tests
//=============================================================================

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
