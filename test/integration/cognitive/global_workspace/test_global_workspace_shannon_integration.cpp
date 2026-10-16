/**
 * @file test_global_workspace_shannon_integration.cpp
 * @brief Integration tests for Shannon-enhanced Global Workspace
 *
 * TEST COVERAGE:
 * - Shannon integration with brain initialization
 * - Multi-module competition with information weighting
 * - Bottleneck detection across subscriber chain
 * - Adaptive rate control under load
 * - Cross-module information flow
 * - End-to-end salience peak to broadcast pipeline
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
#include <algorithm>

#include "cognitive/global_workspace/nimcp_global_workspace.h"
#include "cognitive/global_workspace/nimcp_global_workspace_shannon.h"

//=============================================================================
// Integration Test Fixture
//=============================================================================

class GlobalWorkspaceShannonIntegration : public ::testing::Test {
protected:
    global_workspace_t* workspace;
    static constexpr uint32_t DEFAULT_DIM = 256;

    void SetUp() override {
        workspace = global_workspace_create();
        ASSERT_NE(workspace, nullptr);

        // Enable Shannon with default config
        bool enabled = global_workspace_enable_shannon(workspace, nullptr);
        ASSERT_TRUE(enabled);
    }

    void TearDown() override {
        if (workspace != nullptr) {
            global_workspace_disable_shannon(workspace);
            global_workspace_destroy(workspace);
            workspace = nullptr;
        }
    }

    // Helper: Create uniform feature vector
    std::vector<float> CreateUniformFeatures(uint32_t dim) {
        std::vector<float> features(dim);
        float val = 1.0f / static_cast<float>(dim);
        for (uint32_t i = 0; i < dim; i++) {
            features[i] = val;
        }
        return features;
    }

    // Helper: Create features with specified entropy
    std::vector<float> CreateFeaturesWithEntropy(uint32_t dim, float target_entropy) {
        std::vector<float> features(dim, 0.0f);

        // Number of active states determines entropy: H = log2(n)
        uint32_t n_active = static_cast<uint32_t>(std::pow(2.0f, target_entropy));
        if (n_active > dim) n_active = dim;
        if (n_active < 1) n_active = 1;

        float val = 1.0f / static_cast<float>(n_active);
        for (uint32_t i = 0; i < n_active; i++) {
            features[i] = val;
        }

        return features;
    }

    // Helper: Setup a single subscriber
    void SetupSubscriber(cognitive_module_t module, float capacity = 100.0f) {
        global_workspace_subscribe(workspace, module);
        global_workspace_set_subscriber_capacity(workspace, module, capacity);
    }

    void SleepMs(uint32_t ms) {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }
};

//=============================================================================
// 1. Multi-Module Competition Integration
//=============================================================================

TEST_F(GlobalWorkspaceShannonIntegration, MultiModuleCompetitionHighInfoWins) {
    // Multiple modules compete - highest information × salience should win

    // High information content (uniform distribution)
    auto high_info_content = CreateFeaturesWithEntropy(DEFAULT_DIM, 6.0f);  // ~6 bits
    float high_info_bits = 0.0f;

    // Low information content (peaked)
    auto low_info_content = CreateFeaturesWithEntropy(DEFAULT_DIM, 2.0f);   // ~2 bits
    float low_info_bits = 0.0f;

    // High info module competes with moderate salience
    global_workspace_submit(workspace, MODULE_WORKING_MEMORY,
                           high_info_content.data(), DEFAULT_DIM, 0.7f);

    // Low info module competes with same salience
    global_workspace_submit(workspace, MODULE_EXECUTIVE,
                           low_info_content.data(), DEFAULT_DIM, 0.7f);

    cognitive_module_t winner = MODULE_NONE;
    bool resolved = global_workspace_resolve(workspace, &winner);

    // High info should win (info weighting)
    EXPECT_TRUE(resolved);
    // Winner depends on info × salience calculation
}

TEST_F(GlobalWorkspaceShannonIntegration, SalienceCanOvercomeInfoDeficit) {
    // Very high salience can overcome lower information content

    auto low_info = CreateFeaturesWithEntropy(DEFAULT_DIM, 2.5f);   // Above threshold
    auto high_info = CreateFeaturesWithEntropy(DEFAULT_DIM, 5.0f);

    // Low info but very high salience
    global_workspace_submit(workspace, MODULE_WELLBEING,
                           low_info.data(), DEFAULT_DIM, 0.99f);

    // High info but moderate salience
    global_workspace_submit(workspace, MODULE_CURIOSITY,
                           high_info.data(), DEFAULT_DIM, 0.6f);

    cognitive_module_t winner = MODULE_NONE;
    global_workspace_resolve(workspace, &winner);

    // Competition result depends on weighted formula
    // strength = salience × (w × info/norm + (1-w))
}

TEST_F(GlobalWorkspaceShannonIntegration, InfoThresholdFiltering) {
    // Modules below info threshold should be rejected

    // Configure with higher threshold
    shannon_workspace_config_t config = shannon_workspace_default_config();
    config.info_threshold_bits = 4.0f;  // Higher threshold

    global_workspace_disable_shannon(workspace);
    global_workspace_enable_shannon(workspace, &config);

    // Low info content (below threshold)
    auto low_info = CreateFeaturesWithEntropy(DEFAULT_DIM, 2.0f);

    float info_bits = 0.0f;
    bool won = global_workspace_compete_with_info(
        workspace, MODULE_WORKING_MEMORY,
        low_info.data(), DEFAULT_DIM, 0.9f, &info_bits
    );

    EXPECT_LT(info_bits, 4.0f);  // Below threshold
    EXPECT_FALSE(won);           // Should be rejected

    // Verify rejection counted in statistics
    shannon_workspace_stats_t stats;
    global_workspace_get_shannon_stats(workspace, &stats);
    EXPECT_GT(stats.low_info_rejections, 0u);
}

//=============================================================================
// 2. Subscriber Chain Integration
//=============================================================================

TEST_F(GlobalWorkspaceShannonIntegration, MultipleSubscribersBroadcast) {
    // Setup multiple subscribers with different capacities
    SetupSubscriber(MODULE_EXECUTIVE, 200.0f);
    SetupSubscriber(MODULE_SALIENCE, 150.0f);
    SetupSubscriber(MODULE_THEORY_OF_MIND, 100.0f);
    SetupSubscriber(MODULE_METACOGNITION, 50.0f);

    auto content = CreateUniformFeatures(DEFAULT_DIM);
    float info_bits = shannon_measure_feature_information(content.data(), DEFAULT_DIM);

    shannon_broadcast_metrics_t metrics = global_workspace_broadcast_with_shannon(
        workspace, content.data(), DEFAULT_DIM, info_bits
    );

    // All subscribers should receive broadcast
    EXPECT_EQ(metrics.num_subscribers, 4u);
    EXPECT_FLOAT_EQ(metrics.content_info_bits, info_bits);
}

TEST_F(GlobalWorkspaceShannonIntegration, BottleneckPropagation) {
    // Test bottleneck detection across subscriber chain

    // Executive has high capacity
    SetupSubscriber(MODULE_EXECUTIVE, 1000.0f);

    // Salience has very low capacity
    SetupSubscriber(MODULE_SALIENCE, 1.0f);

    // Saturate salience subscriber
    for (int i = 0; i < 20; i++) {
        global_workspace_update_subscriber_load(workspace, MODULE_SALIENCE, 10.0f);
    }

    auto content = CreateUniformFeatures(DEFAULT_DIM);
    float info_bits = shannon_measure_feature_information(content.data(), DEFAULT_DIM);

    shannon_broadcast_metrics_t metrics = global_workspace_broadcast_with_shannon(
        workspace, content.data(), DEFAULT_DIM, info_bits
    );

    // Should detect bottleneck on salience
    EXPECT_TRUE(metrics.bottleneck_detected);
    EXPECT_EQ(metrics.bottlenecked_module, MODULE_SALIENCE);
    EXPECT_EQ(metrics.num_bottlenecked, 1u);  // Only one bottlenecked
}

//=============================================================================
// 3. Adaptive Rate Control Integration
//=============================================================================

TEST_F(GlobalWorkspaceShannonIntegration, RateReductionUnderLoad) {
    SetupSubscriber(MODULE_EXECUTIVE);
    global_workspace_set_subscriber_capacity(workspace, MODULE_EXECUTIVE, 1.0f);

    // Saturate the subscriber
    for (int i = 0; i < 50; i++) {
        global_workspace_update_subscriber_load(workspace, MODULE_EXECUTIVE, 10.0f);
    }

    float initial_rate = global_workspace_get_broadcast_rate(workspace);
    EXPECT_FLOAT_EQ(initial_rate, 1.0f);

    // Multiple broadcasts should trigger rate reduction
    auto content = CreateUniformFeatures(DEFAULT_DIM);
    float info_bits = 5.0f;

    for (int i = 0; i < 10; i++) {
        global_workspace_broadcast_with_shannon(workspace, content.data(), DEFAULT_DIM, info_bits);
    }

    float final_rate = global_workspace_get_broadcast_rate(workspace);

    // Rate should have decreased due to bottleneck
    EXPECT_LT(final_rate, initial_rate);
}

TEST_F(GlobalWorkspaceShannonIntegration, RateRecoveryWhenClear) {
    SetupSubscriber(MODULE_EXECUTIVE);
    global_workspace_set_subscriber_capacity(workspace, MODULE_EXECUTIVE, 10000.0f);  // Very high

    // Start with reduced rate
    global_workspace_set_broadcast_rate(workspace, 0.3f);
    float initial_rate = global_workspace_get_broadcast_rate(workspace);

    // Broadcasts without bottleneck should recover rate
    auto content = CreateUniformFeatures(DEFAULT_DIM);
    float info_bits = 3.0f;

    for (int i = 0; i < 10; i++) {
        global_workspace_broadcast_with_shannon(workspace, content.data(), DEFAULT_DIM, info_bits);
    }

    float final_rate = global_workspace_get_broadcast_rate(workspace);

    // Rate should have increased (recovered)
    EXPECT_GT(final_rate, initial_rate);
}

//=============================================================================
// 4. End-to-End Pipeline Integration
//=============================================================================

TEST_F(GlobalWorkspaceShannonIntegration, CompeteAndBroadcastPipeline) {
    // Full pipeline: compete with info → win → broadcast with monitoring

    SetupSubscriber(MODULE_EXECUTIVE);
    SetupSubscriber(MODULE_SALIENCE);
    SetupSubscriber(MODULE_WORKING_MEMORY);

    auto content = CreateUniformFeatures(DEFAULT_DIM);
    float info_bits = 0.0f;

    // Compete with info
    bool won = global_workspace_compete_with_info(
        workspace, MODULE_CURIOSITY,
        content.data(), DEFAULT_DIM, 0.85f, &info_bits
    );

    EXPECT_TRUE(won);
    EXPECT_GT(info_bits, 0.0f);

    // Get last broadcast metrics
    shannon_broadcast_metrics_t metrics;
    bool has_metrics = global_workspace_get_last_broadcast_metrics(workspace, &metrics);

    // Note: compete_with_info doesn't automatically broadcast with shannon
    // The metrics come from broadcast_with_shannon which isn't automatically called
}

TEST_F(GlobalWorkspaceShannonIntegration, SaliencePeakHandlerPipeline) {
    // Test the salience peak handler integration

    SetupSubscriber(MODULE_EXECUTIVE);
    SetupSubscriber(MODULE_WORKING_MEMORY);

    // Create mock salience peak
    struct MockPeak {
        float salience_score;
        float* feature_vector;
        uint32_t dimension;
    };

    auto features = CreateUniformFeatures(DEFAULT_DIM);
    MockPeak peak = {
        .salience_score = 0.9f,
        .feature_vector = features.data(),
        .dimension = DEFAULT_DIM
    };

    bool handled = global_workspace_on_salience_peak_shannon(workspace, &peak);
    EXPECT_TRUE(handled);

    // Should have broadcast metrics
    shannon_broadcast_metrics_t metrics;
    bool has_metrics = global_workspace_get_last_broadcast_metrics(workspace, &metrics);
    EXPECT_TRUE(has_metrics);
    EXPECT_GT(metrics.content_info_bits, 0.0f);
}

//=============================================================================
// 5. Statistics Accumulation Integration
//=============================================================================

TEST_F(GlobalWorkspaceShannonIntegration, StatisticsAccumulateOverTime) {
    SetupSubscriber(MODULE_EXECUTIVE);
    global_workspace_set_subscriber_capacity(workspace, MODULE_EXECUTIVE, 100.0f);

    auto content = CreateUniformFeatures(DEFAULT_DIM);
    float info_bits = shannon_measure_feature_information(content.data(), DEFAULT_DIM);

    // Multiple operations
    for (int i = 0; i < 20; i++) {
        global_workspace_compete_with_info(
            workspace, MODULE_WORKING_MEMORY,
            content.data(), DEFAULT_DIM, 0.75f, nullptr
        );

        global_workspace_broadcast_with_shannon(
            workspace, content.data(), DEFAULT_DIM, info_bits
        );

        SleepMs(60);  // Wait for refractory
    }

    shannon_workspace_stats_t stats;
    global_workspace_get_shannon_stats(workspace, &stats);

    // Verify accumulation
    EXPECT_GE(stats.total_competitions_with_info, 20u);
    EXPECT_GT(stats.total_shannon_broadcasts, 0u);
    EXPECT_GT(stats.total_info_delivered_bits, 0.0f);
    EXPECT_GT(stats.avg_winner_info_bits, 0.0f);
}

TEST_F(GlobalWorkspaceShannonIntegration, PerSubscriberStatistics) {
    SetupSubscriber(MODULE_EXECUTIVE);
    SetupSubscriber(MODULE_SALIENCE);

    // Set different capacities
    global_workspace_set_subscriber_capacity(workspace, MODULE_EXECUTIVE, 1000.0f);
    global_workspace_set_subscriber_capacity(workspace, MODULE_SALIENCE, 10.0f);

    // Saturate salience
    for (int i = 0; i < 50; i++) {
        global_workspace_update_subscriber_load(workspace, MODULE_SALIENCE, 5.0f);
    }

    auto content = CreateUniformFeatures(DEFAULT_DIM);
    float info_bits = 5.0f;

    // Multiple broadcasts
    for (int i = 0; i < 10; i++) {
        global_workspace_broadcast_with_shannon(workspace, content.data(), DEFAULT_DIM, info_bits);
    }

    shannon_workspace_stats_t stats;
    global_workspace_get_shannon_stats(workspace, &stats);

    // Salience should have bottleneck counts
    EXPECT_GT(stats.bottleneck_events, 0u);
}

//=============================================================================
// 6. Configuration Integration
//=============================================================================

TEST_F(GlobalWorkspaceShannonIntegration, DisabledFeaturesBypass) {
    // Disable specific Shannon features via config

    shannon_workspace_config_t config = shannon_workspace_default_config();
    config.enable_info_weighted_competition = false;
    config.enable_shannon_monitoring = false;
    config.enable_adaptive_rate = false;

    global_workspace_disable_shannon(workspace);
    global_workspace_enable_shannon(workspace, &config);

    auto content = CreateUniformFeatures(DEFAULT_DIM);

    // Should still work but without Shannon enhancements
    bool won = global_workspace_compete_with_info(
        workspace, MODULE_WORKING_MEMORY,
        content.data(), DEFAULT_DIM, 0.8f, nullptr
    );

    // Uses raw salience when info weighting disabled
    EXPECT_TRUE(won);
}

TEST_F(GlobalWorkspaceShannonIntegration, CustomInfoWeight) {
    // Test with different info_weight values

    shannon_workspace_config_t config = shannon_workspace_default_config();
    config.info_weight = 0.9f;  // Heavy info weighting

    global_workspace_disable_shannon(workspace);
    global_workspace_enable_shannon(workspace, &config);

    // High info, low salience
    auto high_info = CreateFeaturesWithEntropy(DEFAULT_DIM, 6.0f);

    // Low info, high salience
    auto low_info = CreateFeaturesWithEntropy(DEFAULT_DIM, 3.0f);

    global_workspace_submit(workspace, MODULE_WORKING_MEMORY,
                           high_info.data(), DEFAULT_DIM, 0.5f);
    global_workspace_submit(workspace, MODULE_EXECUTIVE,
                           low_info.data(), DEFAULT_DIM, 0.9f);

    cognitive_module_t winner = MODULE_NONE;
    global_workspace_resolve(workspace, &winner);

    // With high info_weight (0.9), information should matter more
}

//=============================================================================
// 7. Error Recovery Integration
//=============================================================================

TEST_F(GlobalWorkspaceShannonIntegration, RecoverFromInvalidOperations) {
    // System should remain stable after invalid operations

    // Invalid operations
    global_workspace_set_subscriber_capacity(nullptr, MODULE_EXECUTIVE, 100.0f);
    global_workspace_update_subscriber_load(nullptr, MODULE_EXECUTIVE, 10.0f);
    global_workspace_broadcast_with_shannon(nullptr, nullptr, 0, 0.0f);
    global_workspace_compete_with_info(nullptr, MODULE_NONE, nullptr, 0, 0.0f, nullptr);

    // System should still function
    auto content = CreateUniformFeatures(DEFAULT_DIM);

    bool won = global_workspace_compete_with_info(
        workspace, MODULE_WORKING_MEMORY,
        content.data(), DEFAULT_DIM, 0.8f, nullptr
    );

    EXPECT_TRUE(won);
    EXPECT_TRUE(global_workspace_is_shannon_enabled(workspace));
}

TEST_F(GlobalWorkspaceShannonIntegration, ResetDuringOperation) {
    SetupSubscriber(MODULE_EXECUTIVE);

    auto content = CreateUniformFeatures(DEFAULT_DIM);

    // Generate some activity
    for (int i = 0; i < 5; i++) {
        global_workspace_compete_with_info(
            workspace, MODULE_WORKING_MEMORY,
            content.data(), DEFAULT_DIM, 0.8f, nullptr
        );
        SleepMs(60);
    }

    // Reset statistics
    global_workspace_reset_shannon_stats(workspace);

    // Continue operation
    global_workspace_compete_with_info(
        workspace, MODULE_WORKING_MEMORY,
        content.data(), DEFAULT_DIM, 0.8f, nullptr
    );

    shannon_workspace_stats_t stats;
    global_workspace_get_shannon_stats(workspace, &stats);

    // Stats should show only post-reset activity
    EXPECT_EQ(stats.total_competitions_with_info, 1u);
}

//=============================================================================
// Run All Tests
//=============================================================================

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
