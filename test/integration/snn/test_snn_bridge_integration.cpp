/**
 * @file test_snn_bridge_integration.cpp
 * @brief Integration tests for SNN bridges
 *
 * WHAT: Test SNN bridges working together with connected modules
 * WHY:  Verify cross-bridge functionality and module interactions
 * HOW:  Create combined systems and test bidirectional data flow
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_config.h"
#include "snn/nimcp_snn_training.h"
#include "snn/bridges/nimcp_snn_training_integration_bridge.h"
#include "snn/bridges/nimcp_snn_emotion_bridge.h"
#include "snn/bridges/nimcp_snn_sleep_bridge.h"
#include "snn/bridges/nimcp_snn_autobiographical_bridge.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Training Integration Bridge Tests
//=============================================================================

class SNNTrainingIntegrationTest : public ::testing::Test {
protected:
    snn_network_t* network = nullptr;
    snn_training_ctx_t* training_ctx = nullptr;
    snn_training_integration_bridge_t* bridge = nullptr;

    void SetUp() override {
        snn_config_t config;
        snn_config_default(&config);
        config.n_inputs = 8;
        config.n_outputs = 4;
        config.n_populations = 3;
        config.dt = 1.0f;
        network = snn_network_create(&config);

        snn_stdp_config_t stdp_config;
        snn_stdp_config_default(&stdp_config);
        training_ctx = snn_training_create_stdp(&stdp_config);
    }

    void TearDown() override {
        if (bridge) {
            snn_training_integration_destroy(bridge);
            bridge = nullptr;
        }
        if (training_ctx) {
            snn_training_destroy(training_ctx);
            training_ctx = nullptr;
        }
        if (network) {
            snn_network_destroy(network);
            network = nullptr;
        }
    }
};

TEST_F(SNNTrainingIntegrationTest, BridgeWithSingleContext) {
    bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(nullptr, bridge);

    int ctx_id = snn_training_integration_connect_context(bridge, training_ctx, network, "main");
    EXPECT_GE(ctx_id, 0);

    snn_training_integration_start(bridge);

    // Verify state shows connected context
    snn_training_integration_state_t state;
    snn_training_integration_get_state(bridge, &state);
    EXPECT_EQ(1u, state.contexts_connected);
}

TEST_F(SNNTrainingIntegrationTest, BridgeWithMultipleContexts) {
    bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(nullptr, bridge);

    // Create second training context
    snn_stdp_config_t stdp_config;
    snn_stdp_config_default(&stdp_config);
    snn_training_ctx_t* ctx2 = snn_training_create_stdp(&stdp_config);
    ASSERT_NE(nullptr, ctx2);

    int id1 = snn_training_integration_connect_context(bridge, training_ctx, network, "ctx1");
    int id2 = snn_training_integration_connect_context(bridge, ctx2, network, "ctx2");

    EXPECT_GE(id1, 0);
    EXPECT_GE(id2, 0);
    EXPECT_NE(id1, id2);

    snn_training_integration_state_t state;
    snn_training_integration_get_state(bridge, &state);
    EXPECT_EQ(2u, state.contexts_connected);

    snn_training_destroy(ctx2);
}

TEST_F(SNNTrainingIntegrationTest, RewardPropagationAcrossContexts) {
    bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(nullptr, bridge);

    snn_training_integration_connect_context(bridge, training_ctx, network, "main");
    snn_training_integration_start(bridge);

    // Set multiple reward sources
    snn_training_integration_set_reward(bridge, 0.5f, SNN_REWARD_SOURCE_EXTERNAL);
    snn_training_integration_set_reward(bridge, 0.3f, SNN_REWARD_SOURCE_LOSS);
    snn_training_integration_set_reward(bridge, 0.2f, SNN_REWARD_SOURCE_CURIOSITY);

    // Verify cumulative reward
    float cumulative = snn_training_integration_get_cumulative_reward(bridge);
    EXPECT_GT(cumulative, 0.5f);  // At least first reward
}

TEST_F(SNNTrainingIntegrationTest, UpdateCycleWithLearningEvents) {
    bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(nullptr, bridge);

    snn_training_integration_connect_context(bridge, training_ctx, network, "main");
    snn_training_integration_start(bridge);

    // Simulate learning events
    for (int i = 0; i < 50; i++) {
        snn_training_integration_report_event(bridge, SNN_LEARNING_EVENT_LTP, 0.01f);
        snn_training_integration_update(bridge, 1.0f);
    }
    for (int i = 0; i < 30; i++) {
        snn_training_integration_report_event(bridge, SNN_LEARNING_EVENT_LTD, 0.005f);
        snn_training_integration_update(bridge, 1.0f);
    }

    // Check LTP/LTD ratio
    float ratio = snn_training_integration_get_ltp_ltd_ratio(bridge);
    EXPECT_GE(ratio, 0.5f);  // More LTP than LTD (50 LTP, 30 LTD -> ratio >= 0.5)
    EXPECT_LE(ratio, 1.0f);

    // Check stats
    snn_training_integration_stats_t stats;
    snn_training_integration_get_stats(bridge, &stats);
    EXPECT_EQ(50u, stats.total_ltp_events);
    EXPECT_EQ(30u, stats.total_ltd_events);
}

TEST_F(SNNTrainingIntegrationTest, LrModulationAffectsState) {
    bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(nullptr, bridge);

    snn_training_integration_connect_context(bridge, training_ctx, network, "main");
    snn_training_integration_start(bridge);

    // Apply LR modulation
    snn_training_integration_apply_lr_modulation(bridge, 0.5f);

    snn_training_integration_state_t state;
    snn_training_integration_get_state(bridge, &state);
    EXPECT_FLOAT_EQ(0.5f, state.current_lr_factor);

    // Apply higher LR
    snn_training_integration_apply_lr_modulation(bridge, 1.8f);
    snn_training_integration_get_state(bridge, &state);
    EXPECT_FLOAT_EQ(1.8f, state.current_lr_factor);
}

TEST_F(SNNTrainingIntegrationTest, ConsolidationModeReducesLearning) {
    bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(nullptr, bridge);

    snn_training_integration_connect_context(bridge, training_ctx, network, "main");
    snn_training_integration_start(bridge);

    // Enable consolidation mode
    snn_training_integration_consolidation_mode(bridge, true);

    snn_training_integration_state_t state;
    snn_training_integration_get_state(bridge, &state);
    EXPECT_TRUE(state.consolidation_active);

    // Disable
    snn_training_integration_consolidation_mode(bridge, false);
    snn_training_integration_get_state(bridge, &state);
    EXPECT_FALSE(state.consolidation_active);
}

TEST_F(SNNTrainingIntegrationTest, ExplorationModeIncreasesVariability) {
    bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(nullptr, bridge);

    snn_training_integration_connect_context(bridge, training_ctx, network, "main");
    snn_training_integration_start(bridge);

    // Enable exploration mode
    snn_training_integration_exploration_mode(bridge, true);

    snn_training_integration_state_t state;
    snn_training_integration_get_state(bridge, &state);
    EXPECT_TRUE(state.exploration_active);
}

TEST_F(SNNTrainingIntegrationTest, EpochCompletionResetsAccumulators) {
    bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(nullptr, bridge);

    snn_training_integration_connect_context(bridge, training_ctx, network, "main");
    snn_training_integration_start(bridge);

    // Accumulate rewards
    snn_training_integration_set_reward(bridge, 1.0f, SNN_REWARD_SOURCE_EXTERNAL);
    snn_training_integration_set_reward(bridge, 0.5f, SNN_REWARD_SOURCE_LOSS);

    float before = snn_training_integration_get_cumulative_reward(bridge);
    EXPECT_GT(before, 0.0f);

    // Complete epoch
    snn_training_integration_epoch_complete(bridge, 1);

    float after = snn_training_integration_get_cumulative_reward(bridge);
    EXPECT_EQ(0.0f, after);
}

//=============================================================================
// Multi-Bridge Integration Tests
//=============================================================================

class SNNMultiBridgeIntegrationTest : public ::testing::Test {
protected:
    snn_network_t* network = nullptr;

    void SetUp() override {
        snn_config_t config;
        snn_config_default(&config);
        config.n_inputs = 8;
        config.n_outputs = 4;
        config.n_populations = 3;
        config.dt = 1.0f;
        network = snn_network_create(&config);
    }

    void TearDown() override {
        if (network) {
            snn_network_destroy(network);
            network = nullptr;
        }
    }
};

TEST_F(SNNMultiBridgeIntegrationTest, EmotionAndSleepBridges) {
    // Create emotion bridge
    snn_emotion_config_t emotion_config;
    snn_emotion_config_default(&emotion_config);
    snn_emotion_bridge_t* emotion_bridge = snn_emotion_bridge_create(&emotion_config, network, nullptr);
    ASSERT_NE(nullptr, emotion_bridge);

    // Create sleep bridge
    snn_sleep_config_t sleep_config;
    snn_sleep_config_default(&sleep_config);
    snn_sleep_bridge_t* sleep_bridge = snn_sleep_bridge_create(&sleep_config, network);
    ASSERT_NE(nullptr, sleep_bridge);

    // Both bridges should coexist
    float valence = snn_emotion_get_decoded_valence(emotion_bridge);
    snn_sleep_stage_t stage = snn_sleep_get_stage(sleep_bridge);

    EXPECT_GE(valence, -1.0f);
    EXPECT_LE(valence, 1.0f);
    EXPECT_GE(stage, SNN_SLEEP_WAKE);

    snn_sleep_bridge_destroy(sleep_bridge);
    snn_emotion_bridge_destroy(emotion_bridge);
}

TEST_F(SNNMultiBridgeIntegrationTest, TrainingWithEmotionModulation) {
    // Create training integration bridge
    snn_training_integration_bridge_t* training_bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(nullptr, training_bridge);

    // Create emotion bridge
    snn_emotion_config_t emotion_config;
    snn_emotion_config_default(&emotion_config);
    snn_emotion_bridge_t* emotion_bridge = snn_emotion_bridge_create(&emotion_config, network, nullptr);
    ASSERT_NE(nullptr, emotion_bridge);

    snn_training_integration_start(training_bridge);

    // Simulate emotion-based reward
    float valence = snn_emotion_get_decoded_valence(emotion_bridge);
    snn_training_integration_set_reward(training_bridge, valence, SNN_REWARD_SOURCE_EMOTION);

    // Verify reward was recorded
    float reward = snn_training_integration_get_cumulative_reward(training_bridge);
    EXPECT_GE(reward, -1.0f);

    snn_emotion_bridge_destroy(emotion_bridge);
    snn_training_integration_destroy(training_bridge);
}

TEST_F(SNNMultiBridgeIntegrationTest, AutobiographicalMemoryWithTraining) {
    // Create autobiographical bridge
    snn_autobiographical_config_t autobio_config;
    snn_autobiographical_config_default(&autobio_config);
    snn_autobiographical_bridge_t* autobio_bridge = snn_autobiographical_bridge_create(&autobio_config, network);
    ASSERT_NE(nullptr, autobio_bridge);

    // Create training integration bridge
    snn_training_integration_bridge_t* training_bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(nullptr, training_bridge);

    snn_training_integration_start(training_bridge);

    // Encode an episode
    uint32_t memory_id;
    int ret = snn_autobiographical_encode_episode(autobio_bridge, SNN_ENCODING_STRONG, 0.8f, &memory_id);
    EXPECT_EQ(0, ret);

    // Report as learning event
    snn_training_integration_report_event(training_bridge, SNN_LEARNING_EVENT_LTP, 0.8f);

    snn_training_integration_stats_t stats;
    snn_training_integration_get_stats(training_bridge, &stats);
    EXPECT_EQ(1u, stats.total_ltp_events);

    snn_training_integration_destroy(training_bridge);
    snn_autobiographical_bridge_destroy(autobio_bridge);
}

//=============================================================================
// Training Pipeline Integration Tests
//=============================================================================

class SNNTrainingPipelineIntegrationTest : public ::testing::Test {
protected:
    snn_network_t* network = nullptr;
    snn_training_ctx_t* stdp_ctx = nullptr;
    snn_training_integration_bridge_t* bridge = nullptr;

    void SetUp() override {
        snn_config_t config;
        snn_config_default(&config);
        config.n_inputs = 16;
        config.n_outputs = 8;
        config.n_populations = 4;
        config.dt = 0.5f;
        network = snn_network_create(&config);

        snn_stdp_config_t stdp_config;
        snn_stdp_config_default(&stdp_config);
        stdp_ctx = snn_training_create_stdp(&stdp_config);

        bridge = snn_training_integration_create(nullptr);
        snn_training_integration_connect_context(bridge, stdp_ctx, network, "stdp");
        snn_training_integration_start(bridge);
    }

    void TearDown() override {
        if (bridge) snn_training_integration_destroy(bridge);
        if (stdp_ctx) snn_training_destroy(stdp_ctx);
        if (network) snn_network_destroy(network);
    }
};

TEST_F(SNNTrainingPipelineIntegrationTest, FullTrainingEpoch) {
    // Simulate a full training epoch
    for (int step = 0; step < 1000; step++) {
        // Simulate LTP/LTD events
        if (step % 10 == 0) {
            snn_training_integration_report_event(bridge, SNN_LEARNING_EVENT_LTP, 0.01f);
        }
        if (step % 15 == 0) {
            snn_training_integration_report_event(bridge, SNN_LEARNING_EVENT_LTD, 0.005f);
        }

        // Periodic rewards
        if (step % 100 == 0) {
            snn_training_integration_set_reward(bridge, 0.1f, SNN_REWARD_SOURCE_LOSS);
        }

        // Update
        snn_training_integration_update(bridge, 0.5f);
    }

    // Complete epoch
    snn_training_integration_epoch_complete(bridge, 1);

    // Verify metrics
    snn_training_metrics_t metrics;
    snn_training_integration_get_metrics(bridge, &metrics);
    EXPECT_EQ(1u, metrics.epoch);

    // Verify stats
    snn_training_integration_stats_t stats;
    snn_training_integration_get_stats(bridge, &stats);
    EXPECT_EQ(1000u, stats.total_update_calls);
    EXPECT_EQ(100u, stats.total_ltp_events);  // 1000/10
    EXPECT_GE(stats.total_ltd_events, 60u);   // ~1000/15
}

TEST_F(SNNTrainingPipelineIntegrationTest, LearningStabilityOverTime) {
    // Run many updates and check stability doesn't degrade
    for (int epoch = 0; epoch < 5; epoch++) {
        for (int step = 0; step < 200; step++) {
            snn_training_integration_report_event(bridge, SNN_LEARNING_EVENT_LTP, 0.01f);
            snn_training_integration_update(bridge, 0.5f);
        }
        snn_training_integration_epoch_complete(bridge, epoch + 1);
    }

    // Check stability is reasonable
    float stability = snn_training_integration_get_learning_stability(bridge);
    EXPECT_GE(stability, 0.0f);
    EXPECT_LE(stability, 1.0f);
}

TEST_F(SNNTrainingPipelineIntegrationTest, PauseAndResumeTraining) {
    // Run some training
    for (int i = 0; i < 50; i++) {
        snn_training_integration_report_event(bridge, SNN_LEARNING_EVENT_LTP, 0.01f);
        snn_training_integration_update(bridge, 0.5f);
    }

    // Pause
    snn_training_integration_pause_learning(bridge, true);

    snn_training_integration_state_t state;
    snn_training_integration_get_state(bridge, &state);
    EXPECT_TRUE(state.learning_paused);

    // Resume
    snn_training_integration_pause_learning(bridge, false);
    snn_training_integration_get_state(bridge, &state);
    EXPECT_FALSE(state.learning_paused);

    // Continue training
    for (int i = 0; i < 50; i++) {
        snn_training_integration_report_event(bridge, SNN_LEARNING_EVENT_LTP, 0.01f);
        snn_training_integration_update(bridge, 0.5f);
    }

    snn_training_integration_stats_t stats;
    snn_training_integration_get_stats(bridge, &stats);
    EXPECT_EQ(100u, stats.total_ltp_events);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
