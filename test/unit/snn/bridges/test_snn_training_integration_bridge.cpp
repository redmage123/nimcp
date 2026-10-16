/**
 * @file test_snn_training_integration_bridge.cpp
 * @brief Unit tests for SNN-Training Integration bridge
 *
 * Tests the bidirectional bridge between SNN training subsystem
 * and the NIMCP training pipeline.
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "snn/bridges/nimcp_snn_training_integration_bridge.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_config.h"
#include "snn/nimcp_snn_training.h"

class SNNTrainingIntegrationBridgeTest : public ::testing::Test {
protected:
    snn_network_t* snn;
    snn_training_ctx_t* training_ctx;
    snn_training_integration_bridge_t* bridge;

    void SetUp() override {
        snn_config_t snn_config;
        snn_config_default(&snn_config);
        snn_config.n_inputs = 10;
        snn_config.n_outputs = 10;
        snn_config.n_populations = 3;
        snn_config.dt = 0.1f;
        snn = snn_network_create(&snn_config);
        ASSERT_NE(snn, nullptr);

        snn_stdp_config_t stdp_config;
        snn_stdp_config_default(&stdp_config);
        training_ctx = snn_training_create_stdp(&stdp_config);
        ASSERT_NE(training_ctx, nullptr);

        bridge = nullptr;
    }

    void TearDown() override {
        if (bridge) snn_training_integration_destroy(bridge);
        if (training_ctx) snn_training_destroy(training_ctx);
        if (snn) snn_network_destroy(snn);
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(SNNTrainingIntegrationBridgeTest, DefaultConfigInitialization) {
    snn_training_integration_config_t config;
    snn_training_integration_config_default(&config);

    EXPECT_EQ(config.op_mode, SNN_TRAINING_INTEGRATION_OP_AUTOMATIC);
    EXPECT_EQ(config.learning_mode, SNN_TRAINING_INTEGRATION_MODE_STDP);
    EXPECT_GT(config.update_interval_ms, 0u);
    EXPECT_GT(config.stdp_modulation_scale, 0.0f);
    EXPECT_TRUE(config.enable_loss_reward);
    EXPECT_TRUE(config.enable_emergency_brake);
    EXPECT_TRUE(config.enable_bio_async);
}

TEST_F(SNNTrainingIntegrationBridgeTest, DefaultConfigNullSafe) {
    // Should not crash on null
    snn_training_integration_config_default(nullptr);
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(SNNTrainingIntegrationBridgeTest, BridgeCreation) {
    snn_training_integration_config_t config;
    snn_training_integration_config_default(&config);

    bridge = snn_training_integration_create(&config);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(SNNTrainingIntegrationBridgeTest, BridgeCreationWithNullConfig) {
    // Should use defaults
    bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(SNNTrainingIntegrationBridgeTest, BridgeDestruction) {
    snn_training_integration_config_t config;
    snn_training_integration_config_default(&config);

    bridge = snn_training_integration_create(&config);
    ASSERT_NE(bridge, nullptr);

    snn_training_integration_destroy(bridge);
    bridge = nullptr;  // Prevent double-free
}

TEST_F(SNNTrainingIntegrationBridgeTest, DestroyNullSafe) {
    snn_training_integration_destroy(nullptr);  // Should not crash
}

TEST_F(SNNTrainingIntegrationBridgeTest, StartStop) {
    bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(snn_training_integration_start(bridge), 0);
    EXPECT_EQ(snn_training_integration_stop(bridge), 0);
}

TEST_F(SNNTrainingIntegrationBridgeTest, StartIdempotent) {
    bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(snn_training_integration_start(bridge), 0);
    EXPECT_EQ(snn_training_integration_start(bridge), 0);  // Second call OK
}

TEST_F(SNNTrainingIntegrationBridgeTest, StopIdempotent) {
    bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(snn_training_integration_stop(bridge), 0);  // Not started
    snn_training_integration_start(bridge);
    EXPECT_EQ(snn_training_integration_stop(bridge), 0);
    EXPECT_EQ(snn_training_integration_stop(bridge), 0);  // Second call OK
}

//=============================================================================
// Context Connection Tests
//=============================================================================

TEST_F(SNNTrainingIntegrationBridgeTest, ConnectContext) {
    bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    int context_id = snn_training_integration_connect_context(
        bridge, training_ctx, snn, "test_context");
    EXPECT_GE(context_id, 0);
}

TEST_F(SNNTrainingIntegrationBridgeTest, ConnectMultipleContexts) {
    bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    int id1 = snn_training_integration_connect_context(bridge, training_ctx, snn, "ctx1");
    EXPECT_GE(id1, 0);

    // Create another context
    snn_stdp_config_t stdp_config;
    snn_stdp_config_default(&stdp_config);
    snn_training_ctx_t* ctx2 = snn_training_create_stdp(&stdp_config);
    ASSERT_NE(ctx2, nullptr);

    int id2 = snn_training_integration_connect_context(bridge, ctx2, snn, "ctx2");
    EXPECT_GE(id2, 0);
    EXPECT_NE(id1, id2);

    snn_training_destroy(ctx2);
}

TEST_F(SNNTrainingIntegrationBridgeTest, DisconnectContext) {
    bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    int context_id = snn_training_integration_connect_context(
        bridge, training_ctx, snn, "test_context");
    EXPECT_GE(context_id, 0);

    EXPECT_EQ(snn_training_integration_disconnect_context(bridge, context_id), 0);
}

TEST_F(SNNTrainingIntegrationBridgeTest, DisconnectInvalidContext) {
    bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_LT(snn_training_integration_disconnect_context(bridge, 999), 0);
    EXPECT_LT(snn_training_integration_disconnect_context(bridge, -1), 0);
}

TEST_F(SNNTrainingIntegrationBridgeTest, ConnectContextNullParams) {
    bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_LT(snn_training_integration_connect_context(nullptr, training_ctx, snn, "x"), 0);
    EXPECT_LT(snn_training_integration_connect_context(bridge, nullptr, snn, "x"), 0);
}

//=============================================================================
// Pipeline Connection Tests
//=============================================================================

TEST_F(SNNTrainingIntegrationBridgeTest, ConnectBrainTraining) {
    bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    // Connect with NULL (disconnect)
    EXPECT_EQ(snn_training_integration_connect_brain_training(bridge, nullptr), 0);
}

TEST_F(SNNTrainingIntegrationBridgeTest, ConnectCognitiveTraining) {
    bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(snn_training_integration_connect_cognitive_training(bridge, nullptr), 0);
}

TEST_F(SNNTrainingIntegrationBridgeTest, ConnectTrainingImmune) {
    bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(snn_training_integration_connect_training_immune(bridge, nullptr), 0);
}

TEST_F(SNNTrainingIntegrationBridgeTest, ConnectTrainingPlasticity) {
    bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(snn_training_integration_connect_training_plasticity(bridge, nullptr), 0);
}

//=============================================================================
// Metrics API Tests
//=============================================================================

TEST_F(SNNTrainingIntegrationBridgeTest, GetMetrics) {
    bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(bridge, nullptr);
    snn_training_integration_start(bridge);

    snn_training_metrics_t metrics;
    EXPECT_EQ(snn_training_integration_get_metrics(bridge, &metrics), 0);
    EXPECT_EQ(metrics.ltp_count, 0u);
    EXPECT_EQ(metrics.ltd_count, 0u);
}

TEST_F(SNNTrainingIntegrationBridgeTest, GetMetricsNullParams) {
    bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_training_metrics_t metrics;
    EXPECT_LT(snn_training_integration_get_metrics(nullptr, &metrics), 0);
    EXPECT_LT(snn_training_integration_get_metrics(bridge, nullptr), 0);
}

TEST_F(SNNTrainingIntegrationBridgeTest, GetLtpLtdRatio) {
    bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    float ratio = snn_training_integration_get_ltp_ltd_ratio(bridge);
    EXPECT_GE(ratio, 0.0f);
    EXPECT_LE(ratio, 1.0f);
}

TEST_F(SNNTrainingIntegrationBridgeTest, GetLearningStability) {
    bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    float stability = snn_training_integration_get_learning_stability(bridge);
    EXPECT_GE(stability, 0.0f);
    EXPECT_LE(stability, 1.0f);
}

TEST_F(SNNTrainingIntegrationBridgeTest, GetCumulativeReward) {
    bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    float reward = snn_training_integration_get_cumulative_reward(bridge);
    EXPECT_EQ(reward, 0.0f);  // Initially zero
}

TEST_F(SNNTrainingIntegrationBridgeTest, GetEligibilityMean) {
    bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    float eligibility = snn_training_integration_get_eligibility_mean(bridge);
    EXPECT_GE(eligibility, 0.0f);
    EXPECT_LE(eligibility, 1.0f);
}

TEST_F(SNNTrainingIntegrationBridgeTest, NullBridgeMetricsFunctions) {
    EXPECT_FLOAT_EQ(snn_training_integration_get_ltp_ltd_ratio(nullptr), 0.5f);
    EXPECT_FLOAT_EQ(snn_training_integration_get_learning_stability(nullptr), 0.0f);
    EXPECT_FLOAT_EQ(snn_training_integration_get_cumulative_reward(nullptr), 0.0f);
    EXPECT_FLOAT_EQ(snn_training_integration_get_eligibility_mean(nullptr), 0.0f);
}

//=============================================================================
// Parameter API Tests
//=============================================================================

TEST_F(SNNTrainingIntegrationBridgeTest, SetParams) {
    bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_pipeline_params_t params = {};
    params.lr_factor = 1.5f;
    params.stdp_amplitude_scale = 1.2f;
    params.reward_scale = 2.0f;
    params.valid = true;

    EXPECT_EQ(snn_training_integration_set_params(bridge, &params), 0);
}

TEST_F(SNNTrainingIntegrationBridgeTest, SetParamsNullParams) {
    bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_pipeline_params_t params = {};
    EXPECT_LT(snn_training_integration_set_params(nullptr, &params), 0);
    EXPECT_LT(snn_training_integration_set_params(bridge, nullptr), 0);
}

TEST_F(SNNTrainingIntegrationBridgeTest, ApplyLrModulation) {
    bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(snn_training_integration_apply_lr_modulation(bridge, 1.5f), 0);
    EXPECT_EQ(snn_training_integration_apply_lr_modulation(bridge, 0.5f), 0);
}

TEST_F(SNNTrainingIntegrationBridgeTest, ApplyLrModulationClamping) {
    bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    // Values should be clamped to [0.1, 2.0]
    EXPECT_EQ(snn_training_integration_apply_lr_modulation(bridge, 0.01f), 0);
    EXPECT_EQ(snn_training_integration_apply_lr_modulation(bridge, 10.0f), 0);
}

TEST_F(SNNTrainingIntegrationBridgeTest, SetReward) {
    bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(snn_training_integration_set_reward(bridge, 1.0f, SNN_REWARD_SOURCE_EXTERNAL), 0);
    EXPECT_EQ(snn_training_integration_set_reward(bridge, 0.5f, SNN_REWARD_SOURCE_LOSS), 0);
    EXPECT_EQ(snn_training_integration_set_reward(bridge, 0.3f, SNN_REWARD_SOURCE_CURIOSITY), 0);

    float cumulative = snn_training_integration_get_cumulative_reward(bridge);
    EXPECT_GT(cumulative, 0.0f);
}

TEST_F(SNNTrainingIntegrationBridgeTest, SetRewardInvalidSource) {
    bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_LT(snn_training_integration_set_reward(bridge, 1.0f, (snn_reward_source_t)-1), 0);
    EXPECT_LT(snn_training_integration_set_reward(bridge, 1.0f, SNN_REWARD_SOURCE_COUNT), 0);
}

TEST_F(SNNTrainingIntegrationBridgeTest, PauseLearning) {
    bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(snn_training_integration_pause_learning(bridge, true), 0);
    EXPECT_EQ(snn_training_integration_pause_learning(bridge, false), 0);
}

TEST_F(SNNTrainingIntegrationBridgeTest, ConsolidationMode) {
    bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(snn_training_integration_consolidation_mode(bridge, true), 0);

    snn_training_integration_state_t state;
    snn_training_integration_get_state(bridge, &state);
    EXPECT_TRUE(state.consolidation_active);

    EXPECT_EQ(snn_training_integration_consolidation_mode(bridge, false), 0);
}

TEST_F(SNNTrainingIntegrationBridgeTest, ExplorationMode) {
    bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(snn_training_integration_exploration_mode(bridge, true), 0);

    snn_training_integration_state_t state;
    snn_training_integration_get_state(bridge, &state);
    EXPECT_TRUE(state.exploration_active);

    EXPECT_EQ(snn_training_integration_exploration_mode(bridge, false), 0);
}

//=============================================================================
// Update Cycle Tests
//=============================================================================

TEST_F(SNNTrainingIntegrationBridgeTest, Update) {
    bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(bridge, nullptr);
    snn_training_integration_start(bridge);

    EXPECT_EQ(snn_training_integration_update(bridge, 10.0f), 0);
    EXPECT_EQ(snn_training_integration_update(bridge, 10.0f), 0);
    EXPECT_EQ(snn_training_integration_update(bridge, 10.0f), 0);
}

TEST_F(SNNTrainingIntegrationBridgeTest, UpdateNotStarted) {
    bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    // Should return success but do nothing
    EXPECT_EQ(snn_training_integration_update(bridge, 10.0f), 0);
}

TEST_F(SNNTrainingIntegrationBridgeTest, ReportEvent) {
    bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(snn_training_integration_report_event(bridge, SNN_LEARNING_EVENT_LTP, 0.1f), 0);
    EXPECT_EQ(snn_training_integration_report_event(bridge, SNN_LEARNING_EVENT_LTD, 0.05f), 0);
    EXPECT_EQ(snn_training_integration_report_event(bridge, SNN_LEARNING_EVENT_REWARD, 1.0f), 0);
}

TEST_F(SNNTrainingIntegrationBridgeTest, ReportEventInvalid) {
    bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_LT(snn_training_integration_report_event(bridge, (snn_learning_event_t)-1, 0.1f), 0);
    EXPECT_LT(snn_training_integration_report_event(bridge, SNN_LEARNING_EVENT_COUNT, 0.1f), 0);
}

TEST_F(SNNTrainingIntegrationBridgeTest, EpochComplete) {
    bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(bridge, nullptr);
    snn_training_integration_start(bridge);

    // Report some events
    snn_training_integration_report_event(bridge, SNN_LEARNING_EVENT_LTP, 0.1f);
    snn_training_integration_report_event(bridge, SNN_LEARNING_EVENT_LTD, 0.05f);
    snn_training_integration_set_reward(bridge, 1.0f, SNN_REWARD_SOURCE_EXTERNAL);

    EXPECT_EQ(snn_training_integration_epoch_complete(bridge, 1), 0);

    // Cumulative reward should reset after epoch
    float reward = snn_training_integration_get_cumulative_reward(bridge);
    EXPECT_EQ(reward, 0.0f);
}

//=============================================================================
// Bio-Async Tests
//=============================================================================

TEST_F(SNNTrainingIntegrationBridgeTest, BioAsyncConnectionStatus) {
    bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_FALSE(snn_training_integration_is_bio_async_connected(bridge));
}

TEST_F(SNNTrainingIntegrationBridgeTest, BioAsyncConnect) {
    bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    // Bio-async may or may not be available
    int ret = snn_training_integration_connect_bio_async(bridge);
    EXPECT_TRUE(ret <= 0);  // 0 = success, negative = router not available
}

TEST_F(SNNTrainingIntegrationBridgeTest, BioAsyncDisconnect) {
    bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(snn_training_integration_disconnect_bio_async(bridge), 0);
}

TEST_F(SNNTrainingIntegrationBridgeTest, BioAsyncNullBridge) {
    EXPECT_FALSE(snn_training_integration_is_bio_async_connected(nullptr));
}

//=============================================================================
// State and Statistics Tests
//=============================================================================

TEST_F(SNNTrainingIntegrationBridgeTest, GetState) {
    bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_training_integration_state_t state;
    EXPECT_EQ(snn_training_integration_get_state(bridge, &state), 0);
    EXPECT_EQ(state.op_mode, SNN_TRAINING_INTEGRATION_OP_AUTOMATIC);
    EXPECT_EQ(state.contexts_connected, 0u);
    EXPECT_FALSE(state.learning_paused);
}

TEST_F(SNNTrainingIntegrationBridgeTest, GetStateNullParams) {
    bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_training_integration_state_t state;
    EXPECT_LT(snn_training_integration_get_state(nullptr, &state), 0);
    EXPECT_LT(snn_training_integration_get_state(bridge, nullptr), 0);
}

TEST_F(SNNTrainingIntegrationBridgeTest, GetStats) {
    bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(bridge, nullptr);
    snn_training_integration_start(bridge);

    // Do some updates
    snn_training_integration_update(bridge, 10.0f);
    snn_training_integration_update(bridge, 10.0f);

    snn_training_integration_stats_t stats;
    EXPECT_EQ(snn_training_integration_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.total_update_calls, 2u);
}

TEST_F(SNNTrainingIntegrationBridgeTest, GetStatsNullParams) {
    bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_training_integration_stats_t stats;
    EXPECT_LT(snn_training_integration_get_stats(nullptr, &stats), 0);
    EXPECT_LT(snn_training_integration_get_stats(bridge, nullptr), 0);
}

TEST_F(SNNTrainingIntegrationBridgeTest, ResetStats) {
    bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(bridge, nullptr);
    snn_training_integration_start(bridge);

    // Do some updates
    snn_training_integration_update(bridge, 10.0f);
    snn_training_integration_report_event(bridge, SNN_LEARNING_EVENT_LTP, 0.1f);

    snn_training_integration_reset_stats(bridge);

    snn_training_integration_stats_t stats;
    snn_training_integration_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_update_calls, 0u);
    EXPECT_EQ(stats.total_ltp_events, 0u);
}

TEST_F(SNNTrainingIntegrationBridgeTest, ResetStatsNullSafe) {
    snn_training_integration_reset_stats(nullptr);  // Should not crash
}

//=============================================================================
// Utility Tests
//=============================================================================

TEST_F(SNNTrainingIntegrationBridgeTest, ModeToString) {
    EXPECT_STREQ(snn_training_integration_mode_to_string(SNN_TRAINING_INTEGRATION_MODE_STDP), "STDP");
    EXPECT_STREQ(snn_training_integration_mode_to_string(SNN_TRAINING_INTEGRATION_MODE_RSTDP), "R-STDP");
    EXPECT_STREQ(snn_training_integration_mode_to_string(SNN_TRAINING_INTEGRATION_MODE_EPROP), "eProp");
    EXPECT_STREQ(snn_training_integration_mode_to_string(SNN_TRAINING_INTEGRATION_MODE_SURROGATE), "Surrogate");
    EXPECT_STREQ(snn_training_integration_mode_to_string(SNN_TRAINING_INTEGRATION_MODE_HOMEOSTATIC), "Homeostatic");
    EXPECT_STREQ(snn_training_integration_mode_to_string(SNN_TRAINING_INTEGRATION_MODE_HYBRID), "Hybrid");
    EXPECT_STREQ(snn_training_integration_mode_to_string((snn_training_integration_mode_t)99), "Unknown");
}

TEST_F(SNNTrainingIntegrationBridgeTest, RewardSourceToString) {
    EXPECT_STREQ(snn_training_integration_reward_source_to_string(SNN_REWARD_SOURCE_EXTERNAL), "External");
    EXPECT_STREQ(snn_training_integration_reward_source_to_string(SNN_REWARD_SOURCE_LOSS), "Loss");
    EXPECT_STREQ(snn_training_integration_reward_source_to_string(SNN_REWARD_SOURCE_CURIOSITY), "Curiosity");
    EXPECT_STREQ(snn_training_integration_reward_source_to_string(SNN_REWARD_SOURCE_EMOTION), "Emotion");
    EXPECT_STREQ(snn_training_integration_reward_source_to_string(SNN_REWARD_SOURCE_COGNITIVE), "Cognitive");
    EXPECT_STREQ(snn_training_integration_reward_source_to_string(SNN_REWARD_SOURCE_NOVELTY), "Novelty");
    EXPECT_STREQ(snn_training_integration_reward_source_to_string(SNN_REWARD_SOURCE_PREDICTION), "Prediction");
    EXPECT_STREQ(snn_training_integration_reward_source_to_string((snn_reward_source_t)99), "Unknown");
}

TEST_F(SNNTrainingIntegrationBridgeTest, EventToString) {
    EXPECT_STREQ(snn_training_integration_event_to_string(SNN_LEARNING_EVENT_LTP), "LTP");
    EXPECT_STREQ(snn_training_integration_event_to_string(SNN_LEARNING_EVENT_LTD), "LTD");
    EXPECT_STREQ(snn_training_integration_event_to_string(SNN_LEARNING_EVENT_ELIGIBILITY), "Eligibility");
    EXPECT_STREQ(snn_training_integration_event_to_string(SNN_LEARNING_EVENT_REWARD), "Reward");
    EXPECT_STREQ(snn_training_integration_event_to_string(SNN_LEARNING_EVENT_HOMEOSTATIC), "Homeostatic");
    EXPECT_STREQ(snn_training_integration_event_to_string(SNN_LEARNING_EVENT_SATURATION), "Saturation");
    EXPECT_STREQ(snn_training_integration_event_to_string(SNN_LEARNING_EVENT_INSTABILITY), "Instability");
    EXPECT_STREQ(snn_training_integration_event_to_string((snn_learning_event_t)99), "Unknown");
}

//=============================================================================
// Integration Scenario Tests
//=============================================================================

TEST_F(SNNTrainingIntegrationBridgeTest, FullTrainingCycle) {
    bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    // Connect context
    int ctx_id = snn_training_integration_connect_context(bridge, training_ctx, snn, "main");
    EXPECT_GE(ctx_id, 0);

    // Start
    EXPECT_EQ(snn_training_integration_start(bridge), 0);

    // Simulate training epoch
    for (int step = 0; step < 100; step++) {
        // Report some learning events
        if (step % 5 == 0) {
            snn_training_integration_report_event(bridge, SNN_LEARNING_EVENT_LTP, 0.01f);
        }
        if (step % 7 == 0) {
            snn_training_integration_report_event(bridge, SNN_LEARNING_EVENT_LTD, 0.005f);
        }

        // Occasional reward
        if (step % 20 == 0) {
            snn_training_integration_set_reward(bridge, 0.1f, SNN_REWARD_SOURCE_LOSS);
        }

        // Update
        snn_training_integration_update(bridge, 1.0f);
    }

    // End epoch
    EXPECT_EQ(snn_training_integration_epoch_complete(bridge, 1), 0);

    // Check stats
    snn_training_integration_stats_t stats;
    snn_training_integration_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_update_calls, 100u);
    EXPECT_GT(stats.total_ltp_events, 0u);
    EXPECT_GT(stats.total_ltd_events, 0u);
    EXPECT_GT(stats.total_reward_events, 0u);

    // Stop
    EXPECT_EQ(snn_training_integration_stop(bridge), 0);
}

TEST_F(SNNTrainingIntegrationBridgeTest, LrModulationAffectsState) {
    bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_training_integration_apply_lr_modulation(bridge, 1.8f);

    snn_training_integration_state_t state;
    snn_training_integration_get_state(bridge, &state);
    EXPECT_GT(state.current_lr_factor, 1.0f);
}

TEST_F(SNNTrainingIntegrationBridgeTest, EmergencyBrakeTriggering) {
    snn_training_integration_config_t config;
    snn_training_integration_config_default(&config);
    config.enable_emergency_brake = true;
    config.emergency_instability_threshold = 0.5f;

    bridge = snn_training_integration_create(&config);
    ASSERT_NE(bridge, nullptr);
    snn_training_integration_start(bridge);

    // Simulate many instability events
    for (int i = 0; i < 20; i++) {
        snn_training_integration_report_event(bridge, SNN_LEARNING_EVENT_INSTABILITY, 1.0f);
        snn_training_integration_update(bridge, 10.0f);
    }

    // Check stats for emergency brake
    snn_training_integration_stats_t stats;
    snn_training_integration_get_stats(bridge, &stats);
    EXPECT_GT(stats.instability_events, 0u);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
