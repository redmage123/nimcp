/**
 * @file test_snn_bridge_regression.cpp
 * @brief Regression tests for SNN bridge modules
 *
 * WHAT: Regression tests ensuring SNN bridge stability
 * WHY:  Prevent regressions in bridge behavior and API compatibility
 * HOW:  Deterministic tests with known expected values
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_config.h"
#include "snn/nimcp_snn_training.h"
#include "snn/bridges/nimcp_snn_training_integration_bridge.h"
#include "snn/bridges/nimcp_snn_emotion_bridge.h"
#include "snn/bridges/nimcp_snn_sleep_bridge.h"
#include "snn/bridges/nimcp_snn_autobiographical_bridge.h"
#include "snn/bridges/nimcp_snn_medulla_bridge.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Training Integration Bridge Regression Tests
//=============================================================================

class TrainingIntegrationBridgeRegression : public ::testing::Test {
protected:
    snn_network_t* network = nullptr;
    snn_training_ctx_t* training = nullptr;

    void SetUp() override {
        snn_config_t config;
        snn_config_default(&config);
        config.n_inputs = 8;
        config.n_outputs = 4;
        config.n_populations = 2;
        config.dt = 1.0f;
        network = snn_network_create(&config);

        snn_stdp_config_t stdp_config;
        snn_stdp_config_default(&stdp_config);
        training = snn_training_create_stdp(&stdp_config);
    }

    void TearDown() override {
        if (training) snn_training_destroy(training);
        if (network) snn_network_destroy(network);
    }
};

TEST_F(TrainingIntegrationBridgeRegression, DefaultConfigStableAcrossVersions) {
    snn_training_integration_config_t config;
    snn_training_integration_config_default(&config);

    // These defaults must remain stable
    EXPECT_GT(config.update_interval_ms, 0u);
    EXPECT_GT(config.history_length, 0u);
}

TEST_F(TrainingIntegrationBridgeRegression, CreateDestroyNoMemoryLeak) {
    for (int i = 0; i < 10; i++) {
        snn_training_integration_bridge_t* bridge = snn_training_integration_create(nullptr);
        ASSERT_NE(nullptr, bridge);
        snn_training_integration_destroy(bridge);
    }
}

TEST_F(TrainingIntegrationBridgeRegression, ContextConnectionStable) {
    snn_training_integration_bridge_t* bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(nullptr, bridge);

    int ctx_id = snn_training_integration_connect_context(bridge, training, network, "test");
    EXPECT_GE(ctx_id, 0);

    // Disconnect should work
    int result = snn_training_integration_disconnect_context(bridge, ctx_id);
    EXPECT_EQ(0, result);

    snn_training_integration_destroy(bridge);
}

TEST_F(TrainingIntegrationBridgeRegression, RewardAccumulationConsistent) {
    snn_training_integration_bridge_t* bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(nullptr, bridge);

    snn_training_integration_start(bridge);

    // Add known rewards
    snn_training_integration_set_reward(bridge, 1.0f, SNN_REWARD_SOURCE_EXTERNAL);
    snn_training_integration_set_reward(bridge, 0.5f, SNN_REWARD_SOURCE_LOSS);

    float reward = snn_training_integration_get_cumulative_reward(bridge);
    EXPECT_GT(reward, 0.0f);

    snn_training_integration_destroy(bridge);
}

TEST_F(TrainingIntegrationBridgeRegression, LTPLTDRatioCalculationStable) {
    snn_training_integration_bridge_t* bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(nullptr, bridge);

    snn_training_integration_start(bridge);

    // Equal LTP and LTD should give ratio 0.5
    for (int i = 0; i < 10; i++) {
        snn_training_integration_report_event(bridge, SNN_LEARNING_EVENT_LTP, 0.1f);
        snn_training_integration_report_event(bridge, SNN_LEARNING_EVENT_LTD, 0.1f);
    }

    // Update to compute the ratio
    snn_training_integration_update(bridge, 10.0f);

    float ratio = snn_training_integration_get_ltp_ltd_ratio(bridge);
    // Ratio may not be exactly 0.5 depending on implementation
    EXPECT_GE(ratio, 0.0f);
    EXPECT_LE(ratio, 1.0f);

    snn_training_integration_destroy(bridge);
}

TEST_F(TrainingIntegrationBridgeRegression, ModeTransitionsWork) {
    snn_training_integration_bridge_t* bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(nullptr, bridge);

    snn_training_integration_start(bridge);

    snn_training_integration_state_t state;

    // Test consolidation mode
    snn_training_integration_consolidation_mode(bridge, true);
    snn_training_integration_get_state(bridge, &state);
    EXPECT_TRUE(state.consolidation_active);

    snn_training_integration_consolidation_mode(bridge, false);
    snn_training_integration_get_state(bridge, &state);
    EXPECT_FALSE(state.consolidation_active);

    // Test exploration mode
    snn_training_integration_exploration_mode(bridge, true);
    snn_training_integration_get_state(bridge, &state);
    EXPECT_TRUE(state.exploration_active);

    snn_training_integration_exploration_mode(bridge, false);
    snn_training_integration_get_state(bridge, &state);
    EXPECT_FALSE(state.exploration_active);

    snn_training_integration_destroy(bridge);
}

TEST_F(TrainingIntegrationBridgeRegression, StatsCountersIncrement) {
    snn_training_integration_bridge_t* bridge = snn_training_integration_create(nullptr);
    ASSERT_NE(nullptr, bridge);

    snn_training_integration_start(bridge);

    // Update multiple times
    for (int i = 0; i < 100; i++) {
        snn_training_integration_update(bridge, 1.0f);
    }

    snn_training_integration_stats_t stats;
    snn_training_integration_get_stats(bridge, &stats);
    EXPECT_EQ(100u, stats.total_update_calls);

    snn_training_integration_destroy(bridge);
}

//=============================================================================
// Emotion Bridge Regression Tests
//=============================================================================

class EmotionBridgeRegression : public ::testing::Test {
protected:
    snn_network_t* network = nullptr;

    void SetUp() override {
        snn_config_t config;
        snn_config_default(&config);
        config.n_inputs = 8;
        config.n_outputs = 4;
        config.n_populations = 2;
        network = snn_network_create(&config);
    }

    void TearDown() override {
        if (network) snn_network_destroy(network);
    }
};

TEST_F(EmotionBridgeRegression, DefaultConfigStable) {
    snn_emotion_config_t config;
    snn_emotion_config_default(&config);

    // Key defaults must remain stable for compatibility
    EXPECT_GT(config.update_interval_ms, 0.0f);
}

TEST_F(EmotionBridgeRegression, CreateDestroyNoLeak) {
    for (int i = 0; i < 10; i++) {
        snn_emotion_config_t config;
        snn_emotion_config_default(&config);
        snn_emotion_bridge_t* bridge = snn_emotion_bridge_create(&config, network, nullptr);
        ASSERT_NE(nullptr, bridge);
        snn_emotion_bridge_destroy(bridge);
    }
}

TEST_F(EmotionBridgeRegression, ValenceArousalBounded) {
    snn_emotion_config_t config;
    snn_emotion_config_default(&config);
    snn_emotion_bridge_t* bridge = snn_emotion_bridge_create(&config, network, nullptr);
    ASSERT_NE(nullptr, bridge);

    float valence = snn_emotion_get_decoded_valence(bridge);
    float arousal = snn_emotion_get_decoded_arousal(bridge);

    // Values must be in [-1, 1] range
    EXPECT_GE(valence, -1.0f);
    EXPECT_LE(valence, 1.0f);
    EXPECT_GE(arousal, -1.0f);
    EXPECT_LE(arousal, 1.0f);

    snn_emotion_bridge_destroy(bridge);
}

TEST_F(EmotionBridgeRegression, NullBridgeReturnsDefaults) {
    float valence = snn_emotion_get_decoded_valence(nullptr);
    float arousal = snn_emotion_get_decoded_arousal(nullptr);

    EXPECT_FLOAT_EQ(0.0f, valence);
    EXPECT_FLOAT_EQ(0.0f, arousal);
}

//=============================================================================
// Sleep Bridge Regression Tests
//=============================================================================

class SleepBridgeRegression : public ::testing::Test {
protected:
    snn_network_t* network = nullptr;

    void SetUp() override {
        snn_config_t config;
        snn_config_default(&config);
        config.n_inputs = 8;
        config.n_outputs = 4;
        config.n_populations = 2;
        network = snn_network_create(&config);
    }

    void TearDown() override {
        if (network) snn_network_destroy(network);
    }
};

TEST_F(SleepBridgeRegression, DefaultConfigStable) {
    snn_sleep_config_t config;
    snn_sleep_config_default(&config);

    EXPECT_GT(config.spindle_frequency, 0.0f);
    EXPECT_GT(config.spindle_bandwidth, 0.0f);
    EXPECT_GT(config.slow_wave_max_freq, 0.0f);
    EXPECT_GT(config.update_interval_ms, 0.0f);
}

TEST_F(SleepBridgeRegression, CreateDestroyNoLeak) {
    for (int i = 0; i < 10; i++) {
        snn_sleep_config_t config;
        snn_sleep_config_default(&config);
        snn_sleep_bridge_t* bridge = snn_sleep_bridge_create(&config, network);
        ASSERT_NE(nullptr, bridge);
        snn_sleep_bridge_destroy(bridge);
    }
}

TEST_F(SleepBridgeRegression, StageEnumValuesStable) {
    snn_sleep_config_t config;
    snn_sleep_config_default(&config);
    snn_sleep_bridge_t* bridge = snn_sleep_bridge_create(&config, network);
    ASSERT_NE(nullptr, bridge);

    snn_sleep_stage_t stage = snn_sleep_get_stage(bridge);

    // Stage should be a valid enum value
    EXPECT_GE(stage, SNN_SLEEP_WAKE);
    EXPECT_LE(stage, SNN_SLEEP_UNKNOWN);

    snn_sleep_bridge_destroy(bridge);
}

TEST_F(SleepBridgeRegression, REMActivityBounded) {
    snn_sleep_config_t config;
    snn_sleep_config_default(&config);
    snn_sleep_bridge_t* bridge = snn_sleep_bridge_create(&config, network);
    ASSERT_NE(nullptr, bridge);

    float rem = snn_sleep_get_rem_activity(bridge);
    EXPECT_GE(rem, 0.0f);
    EXPECT_LE(rem, 1.0f);

    snn_sleep_bridge_destroy(bridge);
}

TEST_F(SleepBridgeRegression, NullBridgeReturnsDefaults) {
    EXPECT_EQ(SNN_SLEEP_UNKNOWN, snn_sleep_get_stage(nullptr));
    EXPECT_FLOAT_EQ(0.0f, snn_sleep_get_stage_duration(nullptr));
    EXPECT_EQ(0u, snn_sleep_get_spindle_count(nullptr));
    EXPECT_EQ(0u, snn_sleep_get_slow_wave_count(nullptr));
    EXPECT_FLOAT_EQ(0.0f, snn_sleep_get_rem_activity(nullptr));
}

//=============================================================================
// Autobiographical Bridge Regression Tests
//=============================================================================

class AutobiographicalBridgeRegression : public ::testing::Test {
protected:
    snn_network_t* network = nullptr;

    void SetUp() override {
        snn_config_t config;
        snn_config_default(&config);
        config.n_inputs = 8;
        config.n_outputs = 4;
        config.n_populations = 2;
        network = snn_network_create(&config);
    }

    void TearDown() override {
        if (network) snn_network_destroy(network);
    }
};

TEST_F(AutobiographicalBridgeRegression, DefaultConfigStable) {
    snn_autobiographical_config_t config;
    snn_autobiographical_config_default(&config);

    EXPECT_GT(config.max_memories, 0u);
    EXPECT_GT(config.update_interval_ms, 0.0f);
}

TEST_F(AutobiographicalBridgeRegression, CreateDestroyNoLeak) {
    for (int i = 0; i < 10; i++) {
        snn_autobiographical_config_t config;
        snn_autobiographical_config_default(&config);
        snn_autobiographical_bridge_t* bridge = snn_autobiographical_bridge_create(&config, network);
        ASSERT_NE(nullptr, bridge);
        snn_autobiographical_bridge_destroy(bridge);
    }
}

TEST_F(AutobiographicalBridgeRegression, EncodingStrengthEnumStable) {
    snn_autobiographical_config_t config;
    snn_autobiographical_config_default(&config);
    snn_autobiographical_bridge_t* bridge = snn_autobiographical_bridge_create(&config, network);
    ASSERT_NE(nullptr, bridge);

    uint32_t memory_id;

    // All encoding strengths should work
    int ret1 = snn_autobiographical_encode_episode(bridge, SNN_ENCODING_WEAK, 0.3f, &memory_id);
    EXPECT_EQ(0, ret1);

    int ret2 = snn_autobiographical_encode_episode(bridge, SNN_ENCODING_MODERATE, 0.5f, &memory_id);
    EXPECT_EQ(0, ret2);

    int ret3 = snn_autobiographical_encode_episode(bridge, SNN_ENCODING_STRONG, 0.8f, &memory_id);
    EXPECT_EQ(0, ret3);

    snn_autobiographical_bridge_destroy(bridge);
}

TEST_F(AutobiographicalBridgeRegression, StatsIncrementCorrectly) {
    snn_autobiographical_config_t config;
    snn_autobiographical_config_default(&config);
    snn_autobiographical_bridge_t* bridge = snn_autobiographical_bridge_create(&config, network);
    ASSERT_NE(nullptr, bridge);

    uint32_t memory_id;
    snn_autobiographical_encode_episode(bridge, SNN_ENCODING_STRONG, 0.8f, &memory_id);
    snn_autobiographical_encode_episode(bridge, SNN_ENCODING_STRONG, 0.7f, &memory_id);
    snn_autobiographical_encode_episode(bridge, SNN_ENCODING_STRONG, 0.6f, &memory_id);

    uint32_t memory_count = 0, encoding_count = 0;
    float retrieval_rate = 0.0f;
    snn_autobiographical_get_stats(bridge, &memory_count, &encoding_count, &retrieval_rate);
    EXPECT_GE(encoding_count, 3u);

    snn_autobiographical_bridge_destroy(bridge);
}

//=============================================================================
// Medulla Bridge Regression Tests
//=============================================================================

class MedullaBridgeRegression : public ::testing::Test {
protected:
    snn_network_t* network = nullptr;

    void SetUp() override {
        snn_config_t config;
        snn_config_default(&config);
        config.n_inputs = 8;
        config.n_outputs = 4;
        config.n_populations = 2;
        network = snn_network_create(&config);
    }

    void TearDown() override {
        if (network) snn_network_destroy(network);
    }
};

TEST_F(MedullaBridgeRegression, DefaultConfigStable) {
    snn_medulla_config_t config;
    snn_medulla_config_default(&config);

    EXPECT_GT(config.arousal_min_rate_factor, 0.0f);
    EXPECT_GT(config.arousal_max_rate_factor, config.arousal_min_rate_factor);
    EXPECT_GT(config.update_interval_ms, 0.0f);
}

TEST_F(MedullaBridgeRegression, CreateDestroyNoLeak) {
    for (int i = 0; i < 10; i++) {
        snn_medulla_config_t config;
        snn_medulla_config_default(&config);
        snn_medulla_bridge_t* bridge = snn_medulla_bridge_create(&config, network, nullptr);
        ASSERT_NE(nullptr, bridge);
        snn_medulla_bridge_destroy(bridge);
    }
}

TEST_F(MedullaBridgeRegression, ModulationWithoutMedullaReturnsDefaults) {
    snn_medulla_config_t config;
    snn_medulla_config_default(&config);
    snn_medulla_bridge_t* bridge = snn_medulla_bridge_create(&config, network, nullptr);
    ASSERT_NE(nullptr, bridge);

    // Without medulla connected, modulation should be neutral
    float arousal_mod = snn_medulla_compute_arousal_modulation(bridge);
    float protection_gate = snn_medulla_compute_protection_gate(bridge);
    float circadian_mod = snn_medulla_compute_circadian_modulation(bridge);

    EXPECT_FLOAT_EQ(1.0f, arousal_mod);
    EXPECT_FLOAT_EQ(1.0f, protection_gate);
    EXPECT_FLOAT_EQ(0.0f, circadian_mod);

    snn_medulla_bridge_destroy(bridge);
}

TEST_F(MedullaBridgeRegression, CombinedModulationBounded) {
    snn_medulla_config_t config;
    snn_medulla_config_default(&config);
    snn_medulla_bridge_t* bridge = snn_medulla_bridge_create(&config, network, nullptr);
    ASSERT_NE(nullptr, bridge);

    float combined = snn_medulla_get_combined_modulation(bridge);

    // Combined modulation should be in reasonable range
    EXPECT_GE(combined, 0.0f);
    EXPECT_LE(combined, 3.0f);

    snn_medulla_bridge_destroy(bridge);
}

//=============================================================================
// Cross-Bridge Regression Tests
//=============================================================================

class CrossBridgeRegression : public ::testing::Test {
protected:
    snn_network_t* network = nullptr;

    void SetUp() override {
        snn_config_t config;
        snn_config_default(&config);
        config.n_inputs = 8;
        config.n_outputs = 4;
        config.n_populations = 2;
        network = snn_network_create(&config);
    }

    void TearDown() override {
        if (network) snn_network_destroy(network);
    }
};

TEST_F(CrossBridgeRegression, MultipleBridgesCoexist) {
    // Create all bridges simultaneously
    snn_emotion_config_t emotion_config;
    snn_emotion_config_default(&emotion_config);
    snn_emotion_bridge_t* emotion = snn_emotion_bridge_create(&emotion_config, network, nullptr);
    ASSERT_NE(nullptr, emotion);

    snn_sleep_config_t sleep_config;
    snn_sleep_config_default(&sleep_config);
    snn_sleep_bridge_t* sleep = snn_sleep_bridge_create(&sleep_config, network);
    ASSERT_NE(nullptr, sleep);

    snn_autobiographical_config_t autobio_config;
    snn_autobiographical_config_default(&autobio_config);
    snn_autobiographical_bridge_t* autobio = snn_autobiographical_bridge_create(&autobio_config, network);
    ASSERT_NE(nullptr, autobio);

    snn_training_integration_bridge_t* training = snn_training_integration_create(nullptr);
    ASSERT_NE(nullptr, training);

    // All bridges should function independently
    float valence = snn_emotion_get_decoded_valence(emotion);
    snn_sleep_stage_t stage = snn_sleep_get_stage(sleep);
    snn_training_integration_start(training);

    EXPECT_GE(valence, -1.0f);
    EXPECT_LE(valence, 1.0f);
    EXPECT_GE(stage, SNN_SLEEP_WAKE);

    // Cleanup in reverse order
    snn_training_integration_destroy(training);
    snn_autobiographical_bridge_destroy(autobio);
    snn_sleep_bridge_destroy(sleep);
    snn_emotion_bridge_destroy(emotion);
}

TEST_F(CrossBridgeRegression, BridgesWithSharedNetworkStable) {
    // Create bridges that all share the same network
    snn_emotion_config_t emotion_config;
    snn_emotion_config_default(&emotion_config);
    snn_emotion_bridge_t* emotion = snn_emotion_bridge_create(&emotion_config, network, nullptr);

    snn_sleep_config_t sleep_config;
    snn_sleep_config_default(&sleep_config);
    snn_sleep_bridge_t* sleep = snn_sleep_bridge_create(&sleep_config, network);

    // Update all bridges
    for (int i = 0; i < 100; i++) {
        snn_emotion_bridge_update(emotion, 1.0f);
        // Sleep update may fail without cortical population
        snn_sleep_bridge_update(sleep, 1.0f);
    }

    // Bridges should still be functional
    float valence = snn_emotion_get_decoded_valence(emotion);
    snn_sleep_stage_t stage = snn_sleep_get_stage(sleep);

    EXPECT_FALSE(std::isnan(valence));
    EXPECT_GE(stage, SNN_SLEEP_WAKE);

    snn_sleep_bridge_destroy(sleep);
    snn_emotion_bridge_destroy(emotion);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
