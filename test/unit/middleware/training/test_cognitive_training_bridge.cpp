/**
 * @file test_cognitive_training_bridge.cpp
 * @brief Unit tests for Cognitive-Training Bridge
 *
 * TEST COVERAGE:
 * - Lifecycle tests
 * - Configuration tests
 * - Connection tests
 * - Modulation tests
 * - Metrics update tests
 * - Statistics tests
 *
 * @author NIMCP Development Team
 * @date 2025-12-20
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "middleware/training/nimcp_cognitive_training_bridge.h"
#include "utils/error/nimcp_error_codes.h"

//=============================================================================
// Test Fixture
//=============================================================================

class CognitiveTrainingBridgeTest : public ::testing::Test {
protected:
    cognitive_training_bridge_t* bridge;
    cognitive_training_config_t config;

    void SetUp() override {
        cognitive_training_default_config(&config);
        config.enable_bio_async = false;
        config.disable_auto_update = true;
        bridge = cognitive_training_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            cognitive_training_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests (8 tests)
//=============================================================================

TEST_F(CognitiveTrainingBridgeTest, CreateWithDefaults) {
    EXPECT_NE(bridge, nullptr);
}

TEST_F(CognitiveTrainingBridgeTest, CreateWithNullConfig) {
    cognitive_training_bridge_t* test_bridge = cognitive_training_create(nullptr);
    EXPECT_NE(test_bridge, nullptr);
    cognitive_training_destroy(test_bridge);
}

TEST_F(CognitiveTrainingBridgeTest, DestroyNull) {
    cognitive_training_destroy(nullptr);
    SUCCEED();
}

TEST_F(CognitiveTrainingBridgeTest, StartStop) {
    EXPECT_EQ(cognitive_training_start(bridge), 0);
    EXPECT_EQ(cognitive_training_stop(bridge), 0);
}

TEST_F(CognitiveTrainingBridgeTest, DoubleStart) {
    EXPECT_EQ(cognitive_training_start(bridge), 0);
    /* Second start may be idempotent */
    int result = cognitive_training_start(bridge);
    (void)result;
    EXPECT_EQ(cognitive_training_stop(bridge), 0);
}

TEST_F(CognitiveTrainingBridgeTest, DoubleStop) {
    EXPECT_EQ(cognitive_training_start(bridge), 0);
    EXPECT_EQ(cognitive_training_stop(bridge), 0);
    /* Second stop may be idempotent */
    int result = cognitive_training_stop(bridge);
    (void)result;
}

TEST_F(CognitiveTrainingBridgeTest, CreateMultiple) {
    cognitive_training_bridge_t* bridge2 = cognitive_training_create(&config);
    EXPECT_NE(bridge2, nullptr);
    cognitive_training_destroy(bridge2);
}

TEST_F(CognitiveTrainingBridgeTest, CreateDestroyCycle) {
    for (int i = 0; i < 10; i++) {
        cognitive_training_bridge_t* temp = cognitive_training_create(&config);
        ASSERT_NE(temp, nullptr);
        EXPECT_EQ(cognitive_training_start(temp), 0);
        EXPECT_EQ(cognitive_training_stop(temp), 0);
        cognitive_training_destroy(temp);
    }
}

//=============================================================================
// Configuration Tests (8 tests)
//=============================================================================

TEST_F(CognitiveTrainingBridgeTest, DefaultConfig) {
    cognitive_training_config_t test_config;
    cognitive_training_default_config(&test_config);

    EXPECT_TRUE(test_config.enable_executive);
    EXPECT_TRUE(test_config.enable_introspection);
    EXPECT_TRUE(test_config.enable_attention);
    EXPECT_TRUE(test_config.enable_curiosity);
    EXPECT_TRUE(test_config.enable_emotion);
}

TEST_F(CognitiveTrainingBridgeTest, ConfigModulationStrengths) {
    cognitive_training_config_t test_config;
    cognitive_training_default_config(&test_config);

    EXPECT_GT(test_config.executive_strength, 0.0f);
    EXPECT_GT(test_config.introspection_strength, 0.0f);
    EXPECT_GT(test_config.attention_strength, 0.0f);
    EXPECT_GT(test_config.curiosity_strength, 0.0f);
    EXPECT_GT(test_config.emotion_strength, 0.0f);
}

TEST_F(CognitiveTrainingBridgeTest, ConfigLRLimits) {
    cognitive_training_config_t test_config;
    cognitive_training_default_config(&test_config);

    EXPECT_GT(test_config.lr_min_factor, 0.0f);
    EXPECT_LT(test_config.lr_max_factor, 10.0f);
    EXPECT_LT(test_config.lr_min_factor, test_config.lr_max_factor);
}

TEST_F(CognitiveTrainingBridgeTest, ConfigBatchLimits) {
    cognitive_training_config_t test_config;
    cognitive_training_default_config(&test_config);

    EXPECT_GT(test_config.batch_min_factor, 0.0f);
    EXPECT_LT(test_config.batch_max_factor, 10.0f);
    EXPECT_LT(test_config.batch_min_factor, test_config.batch_max_factor);
}

TEST_F(CognitiveTrainingBridgeTest, CustomConfig) {
    cognitive_training_config_t custom_config;
    cognitive_training_default_config(&custom_config);
    custom_config.executive_strength = 0.8f;
    custom_config.enable_bio_async = false;

    cognitive_training_bridge_t* custom_bridge = cognitive_training_create(&custom_config);
    EXPECT_NE(custom_bridge, nullptr);
    cognitive_training_destroy(custom_bridge);
}

TEST_F(CognitiveTrainingBridgeTest, DisabledModules) {
    cognitive_training_config_t custom_config;
    cognitive_training_default_config(&custom_config);
    custom_config.enable_executive = false;
    custom_config.enable_introspection = false;
    custom_config.enable_bio_async = false;

    cognitive_training_bridge_t* custom_bridge = cognitive_training_create(&custom_config);
    EXPECT_NE(custom_bridge, nullptr);
    EXPECT_EQ(cognitive_training_start(custom_bridge), 0);
    cognitive_training_destroy(custom_bridge);
}

TEST_F(CognitiveTrainingBridgeTest, ModesWork) {
    for (int mode = 0; mode < 5; mode++) {
        cognitive_training_config_t test_config;
        cognitive_training_default_config(&test_config);
        test_config.mode = (cognitive_training_mode_t)mode;
        test_config.enable_bio_async = false;

        cognitive_training_bridge_t* temp = cognitive_training_create(&test_config);
        EXPECT_NE(temp, nullptr);
        cognitive_training_destroy(temp);
    }
}

TEST_F(CognitiveTrainingBridgeTest, PrioritiesWork) {
    for (int priority = 0; priority < 6; priority++) {
        cognitive_training_config_t test_config;
        cognitive_training_default_config(&test_config);
        test_config.priority = (cognitive_priority_t)priority;
        test_config.enable_bio_async = false;

        cognitive_training_bridge_t* temp = cognitive_training_create(&test_config);
        EXPECT_NE(temp, nullptr);
        cognitive_training_destroy(temp);
    }
}

//=============================================================================
// Connection Tests (10 tests)
//=============================================================================

TEST_F(CognitiveTrainingBridgeTest, ConnectExecutiveNull) {
    EXPECT_EQ(cognitive_training_connect_executive(bridge, nullptr), 0);
}

TEST_F(CognitiveTrainingBridgeTest, ConnectIntrospectionNull) {
    EXPECT_EQ(cognitive_training_connect_introspection(bridge, nullptr), 0);
}

TEST_F(CognitiveTrainingBridgeTest, ConnectAttentionNull) {
    EXPECT_EQ(cognitive_training_connect_attention(bridge, nullptr), 0);
}

TEST_F(CognitiveTrainingBridgeTest, ConnectCuriosityNull) {
    EXPECT_EQ(cognitive_training_connect_curiosity(bridge, nullptr), 0);
}

TEST_F(CognitiveTrainingBridgeTest, ConnectEmotionNull) {
    EXPECT_EQ(cognitive_training_connect_emotion(bridge, nullptr), 0);
}

TEST_F(CognitiveTrainingBridgeTest, ConnectTrainingContextNull) {
    EXPECT_EQ(cognitive_training_connect_training_context(bridge, nullptr), 0);
}

TEST_F(CognitiveTrainingBridgeTest, ConnectTrainingLogicNull) {
    EXPECT_EQ(cognitive_training_connect_training_logic(bridge, nullptr), 0);
}

TEST_F(CognitiveTrainingBridgeTest, ConnectTrainingPlasticityNull) {
    EXPECT_EQ(cognitive_training_connect_training_plasticity(bridge, nullptr), 0);
}

TEST_F(CognitiveTrainingBridgeTest, ConnectTrainingImmuneNull) {
    EXPECT_EQ(cognitive_training_connect_training_immune(bridge, nullptr), 0);
}

TEST_F(CognitiveTrainingBridgeTest, ConnectAllNull) {
    EXPECT_EQ(cognitive_training_connect_executive(bridge, nullptr), 0);
    EXPECT_EQ(cognitive_training_connect_introspection(bridge, nullptr), 0);
    EXPECT_EQ(cognitive_training_connect_attention(bridge, nullptr), 0);
    EXPECT_EQ(cognitive_training_connect_curiosity(bridge, nullptr), 0);
    EXPECT_EQ(cognitive_training_connect_emotion(bridge, nullptr), 0);
    EXPECT_EQ(cognitive_training_connect_training_context(bridge, nullptr), 0);
    EXPECT_EQ(cognitive_training_connect_training_logic(bridge, nullptr), 0);
}

//=============================================================================
// Modulation Tests (15 tests)
//=============================================================================

TEST_F(CognitiveTrainingBridgeTest, GetModulatedLR) {
    EXPECT_EQ(cognitive_training_start(bridge), 0);
    float modulated = cognitive_training_get_modulated_lr(bridge, 0.001f);
    EXPECT_GT(modulated, 0.0f);
}

TEST_F(CognitiveTrainingBridgeTest, GetModulatedLRNull) {
    float modulated = cognitive_training_get_modulated_lr(nullptr, 0.001f);
    EXPECT_FLOAT_EQ(modulated, 0.001f);  /* Returns base_lr on null */
}

TEST_F(CognitiveTrainingBridgeTest, GetModulatedLRZero) {
    EXPECT_EQ(cognitive_training_start(bridge), 0);
    float modulated = cognitive_training_get_modulated_lr(bridge, 0.0f);
    EXPECT_FLOAT_EQ(modulated, 0.0f);
}

TEST_F(CognitiveTrainingBridgeTest, GetModulatedBatchSize) {
    EXPECT_EQ(cognitive_training_start(bridge), 0);
    uint32_t modulated = cognitive_training_get_modulated_batch_size(bridge, 32);
    EXPECT_GT(modulated, 0u);
}

TEST_F(CognitiveTrainingBridgeTest, GetModulatedBatchSizeNull) {
    uint32_t modulated = cognitive_training_get_modulated_batch_size(nullptr, 32);
    EXPECT_EQ(modulated, 32u);  /* Returns base on null */
}

TEST_F(CognitiveTrainingBridgeTest, GetModulatedBatchSizeOne) {
    EXPECT_EQ(cognitive_training_start(bridge), 0);
    uint32_t modulated = cognitive_training_get_modulated_batch_size(bridge, 1);
    EXPECT_GE(modulated, 1u);
}

TEST_F(CognitiveTrainingBridgeTest, ShouldCheckpoint) {
    EXPECT_EQ(cognitive_training_start(bridge), 0);
    /* Should return a boolean value */
    bool should = cognitive_training_should_checkpoint(bridge);
    (void)should;  /* Value depends on internal state */
}

TEST_F(CognitiveTrainingBridgeTest, ShouldCheckpointNull) {
    bool should = cognitive_training_should_checkpoint(nullptr);
    EXPECT_FALSE(should);
}

TEST_F(CognitiveTrainingBridgeTest, GetExplorationIntensity) {
    EXPECT_EQ(cognitive_training_start(bridge), 0);
    float intensity = cognitive_training_get_exploration_intensity(bridge);
    EXPECT_GE(intensity, 0.0f);
    EXPECT_LE(intensity, 1.0f);
}

TEST_F(CognitiveTrainingBridgeTest, GetExplorationIntensityNull) {
    float intensity = cognitive_training_get_exploration_intensity(nullptr);
    EXPECT_FLOAT_EQ(intensity, 0.0f);
}

TEST_F(CognitiveTrainingBridgeTest, GetEffects) {
    EXPECT_EQ(cognitive_training_start(bridge), 0);
    cognitive_training_effects_t effects;
    EXPECT_EQ(cognitive_training_get_effects(bridge, &effects), 0);
}

TEST_F(CognitiveTrainingBridgeTest, GetEffectsNull) {
    cognitive_training_effects_t effects;
    EXPECT_NE(cognitive_training_get_effects(nullptr, &effects), 0);
    EXPECT_NE(cognitive_training_get_effects(bridge, nullptr), 0);
}

TEST_F(CognitiveTrainingBridgeTest, GetGradientScaling) {
    EXPECT_EQ(cognitive_training_start(bridge), 0);
    float factors[4];
    int result = cognitive_training_get_gradient_scaling(bridge, factors, 4);
    EXPECT_EQ(result, 0);
}

TEST_F(CognitiveTrainingBridgeTest, GetGradientScalingNull) {
    float factors[4];
    EXPECT_NE(cognitive_training_get_gradient_scaling(nullptr, factors, 4), 0);
    EXPECT_NE(cognitive_training_get_gradient_scaling(bridge, nullptr, 4), 0);
}

TEST_F(CognitiveTrainingBridgeTest, GetGradientScalingZeroFeatures) {
    EXPECT_EQ(cognitive_training_start(bridge), 0);
    float factors[1];
    int result = cognitive_training_get_gradient_scaling(bridge, factors, 0);
    EXPECT_EQ(result, 0);
}

//=============================================================================
// Metrics Update Tests (10 tests)
//=============================================================================

TEST_F(CognitiveTrainingBridgeTest, UpdateMetrics) {
    EXPECT_EQ(cognitive_training_start(bridge), 0);
    EXPECT_EQ(cognitive_training_update_metrics(bridge, 0.5f, 1.0f, 0.001f, 100), 0);
}

TEST_F(CognitiveTrainingBridgeTest, UpdateMetricsNull) {
    EXPECT_NE(cognitive_training_update_metrics(nullptr, 0.5f, 1.0f, 0.001f, 100), 0);
}

TEST_F(CognitiveTrainingBridgeTest, UpdateMetricsMultiple) {
    EXPECT_EQ(cognitive_training_start(bridge), 0);
    for (int i = 0; i < 100; i++) {
        float loss = 1.0f / (i + 1);
        EXPECT_EQ(cognitive_training_update_metrics(bridge, loss, 1.0f, 0.001f, i), 0);
    }
}

TEST_F(CognitiveTrainingBridgeTest, SignalEvent) {
    EXPECT_EQ(cognitive_training_start(bridge), 0);
    EXPECT_EQ(cognitive_training_signal_event(bridge, COGNITIVE_TRAINING_FEEDBACK_SATISFACTION, 0.8f), 0);
}

TEST_F(CognitiveTrainingBridgeTest, SignalEventNull) {
    EXPECT_NE(cognitive_training_signal_event(nullptr, COGNITIVE_TRAINING_FEEDBACK_SATISFACTION, 0.8f), 0);
}

TEST_F(CognitiveTrainingBridgeTest, SignalAllEvents) {
    EXPECT_EQ(cognitive_training_start(bridge), 0);
    for (int event = 0; event < COGNITIVE_TRAINING_FEEDBACK_COUNT; event++) {
        EXPECT_EQ(cognitive_training_signal_event(bridge, (cognitive_training_feedback_t)event, 0.5f), 0);
    }
}

TEST_F(CognitiveTrainingBridgeTest, PatternLearned) {
    EXPECT_EQ(cognitive_training_start(bridge), 0);
    EXPECT_EQ(cognitive_training_pattern_learned(bridge, "test_pattern", 0.8f), 0);
}

TEST_F(CognitiveTrainingBridgeTest, PatternLearnedNull) {
    EXPECT_NE(cognitive_training_pattern_learned(nullptr, "test_pattern", 0.8f), 0);
}

TEST_F(CognitiveTrainingBridgeTest, CheckpointComplete) {
    EXPECT_EQ(cognitive_training_start(bridge), 0);
    EXPECT_EQ(cognitive_training_checkpoint_complete(bridge, 1000), 0);
}

TEST_F(CognitiveTrainingBridgeTest, CheckpointCompleteNull) {
    EXPECT_NE(cognitive_training_checkpoint_complete(nullptr, 1000), 0);
}

//=============================================================================
// Update Cycle Tests (5 tests)
//=============================================================================

TEST_F(CognitiveTrainingBridgeTest, UpdateCycle) {
    EXPECT_EQ(cognitive_training_start(bridge), 0);
    EXPECT_EQ(cognitive_training_update(bridge, 100), 0);
}

TEST_F(CognitiveTrainingBridgeTest, UpdateCycleNull) {
    EXPECT_NE(cognitive_training_update(nullptr, 100), 0);
}

TEST_F(CognitiveTrainingBridgeTest, UpdateCycleMultiple) {
    EXPECT_EQ(cognitive_training_start(bridge), 0);
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(cognitive_training_update(bridge, 10), 0);
    }
}

TEST_F(CognitiveTrainingBridgeTest, UpdateCognitiveState) {
    EXPECT_EQ(cognitive_training_start(bridge), 0);
    EXPECT_EQ(cognitive_training_update_cognitive_state(bridge), 0);
}

TEST_F(CognitiveTrainingBridgeTest, ApplyFeedback) {
    EXPECT_EQ(cognitive_training_start(bridge), 0);
    EXPECT_EQ(cognitive_training_apply_feedback(bridge), 0);
}

//=============================================================================
// Statistics Tests (10 tests)
//=============================================================================

TEST_F(CognitiveTrainingBridgeTest, GetStats) {
    cognitive_training_stats_t stats;
    EXPECT_EQ(cognitive_training_get_stats(bridge, &stats), 0);
}

TEST_F(CognitiveTrainingBridgeTest, GetStatsNull) {
    cognitive_training_stats_t stats;
    EXPECT_NE(cognitive_training_get_stats(nullptr, &stats), 0);
    EXPECT_NE(cognitive_training_get_stats(bridge, nullptr), 0);
}

TEST_F(CognitiveTrainingBridgeTest, ResetStats) {
    EXPECT_EQ(cognitive_training_reset_stats(bridge), 0);
}

TEST_F(CognitiveTrainingBridgeTest, ResetStatsNull) {
    EXPECT_NE(cognitive_training_reset_stats(nullptr), 0);
}

TEST_F(CognitiveTrainingBridgeTest, StatsInitialValues) {
    cognitive_training_stats_t stats;
    EXPECT_EQ(cognitive_training_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.total_modulations, 0u);
    EXPECT_EQ(stats.total_update_calls, 0u);
}

TEST_F(CognitiveTrainingBridgeTest, StatsAfterUpdates) {
    EXPECT_EQ(cognitive_training_start(bridge), 0);
    for (int i = 0; i < 10; i++) {
        cognitive_training_update(bridge, 10);
    }

    cognitive_training_stats_t stats;
    EXPECT_EQ(cognitive_training_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.total_update_calls, 10u);
}

TEST_F(CognitiveTrainingBridgeTest, StatsAfterReset) {
    EXPECT_EQ(cognitive_training_start(bridge), 0);
    cognitive_training_update(bridge, 10);

    EXPECT_EQ(cognitive_training_reset_stats(bridge), 0);

    cognitive_training_stats_t stats;
    EXPECT_EQ(cognitive_training_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.total_update_calls, 0u);
}

TEST_F(CognitiveTrainingBridgeTest, StatsAfterMetricsUpdate) {
    EXPECT_EQ(cognitive_training_start(bridge), 0);
    for (int i = 0; i < 5; i++) {
        cognitive_training_update_metrics(bridge, 0.5f, 1.0f, 0.001f, i);
    }

    cognitive_training_stats_t stats;
    EXPECT_EQ(cognitive_training_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.total_modulations, 5u);
}

TEST_F(CognitiveTrainingBridgeTest, StatsAfterEvents) {
    EXPECT_EQ(cognitive_training_start(bridge), 0);
    cognitive_training_signal_event(bridge, COGNITIVE_TRAINING_FEEDBACK_SATISFACTION, 0.8f);
    cognitive_training_signal_event(bridge, COGNITIVE_TRAINING_FEEDBACK_FRUSTRATION, 0.5f);

    cognitive_training_stats_t stats;
    EXPECT_EQ(cognitive_training_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.total_feedback_events, 2u);
}

TEST_F(CognitiveTrainingBridgeTest, StatsConnectionStatus) {
    cognitive_training_stats_t stats;
    EXPECT_EQ(cognitive_training_get_stats(bridge, &stats), 0);

    /* Initially nothing connected */
    EXPECT_FALSE(stats.executive_connected);
    EXPECT_FALSE(stats.introspection_connected);
}

//=============================================================================
// Utility Tests (6 tests)
//=============================================================================

TEST_F(CognitiveTrainingBridgeTest, ModulationToString) {
    const char* str = cognitive_training_modulation_to_string(COGNITIVE_TRAINING_MODULATION_LR);
    EXPECT_NE(str, nullptr);
    EXPECT_GT(strlen(str), 0u);
}

TEST_F(CognitiveTrainingBridgeTest, FeedbackToString) {
    const char* str = cognitive_training_feedback_to_string(COGNITIVE_TRAINING_FEEDBACK_SATISFACTION);
    EXPECT_NE(str, nullptr);
    EXPECT_GT(strlen(str), 0u);
}

TEST_F(CognitiveTrainingBridgeTest, ModeToString) {
    const char* str = cognitive_training_mode_to_string(COGNITIVE_TRAINING_MODE_AUTOMATIC);
    EXPECT_NE(str, nullptr);
    EXPECT_GT(strlen(str), 0u);
}

TEST_F(CognitiveTrainingBridgeTest, DumpState) {
    EXPECT_EQ(cognitive_training_start(bridge), 0);
    cognitive_training_dump_state(bridge);
    SUCCEED();  /* Just verify it doesn't crash */
}

TEST_F(CognitiveTrainingBridgeTest, DumpStateNull) {
    cognitive_training_dump_state(nullptr);
    SUCCEED();  /* Should not crash */
}

TEST_F(CognitiveTrainingBridgeTest, AllEnumStrings) {
    for (int i = 0; i < COGNITIVE_TRAINING_MODULATION_COUNT; i++) {
        const char* str = cognitive_training_modulation_to_string((cognitive_training_modulation_t)i);
        EXPECT_NE(str, nullptr);
    }
    for (int i = 0; i < COGNITIVE_TRAINING_FEEDBACK_COUNT; i++) {
        const char* str = cognitive_training_feedback_to_string((cognitive_training_feedback_t)i);
        EXPECT_NE(str, nullptr);
    }
}

//=============================================================================
// Bio-Async Tests (4 tests)
//=============================================================================

TEST_F(CognitiveTrainingBridgeTest, BioAsyncNotConnected) {
    EXPECT_FALSE(cognitive_training_is_bio_async_connected(bridge));
}

TEST_F(CognitiveTrainingBridgeTest, BioAsyncNullCheck) {
    EXPECT_FALSE(cognitive_training_is_bio_async_connected(nullptr));
}

TEST_F(CognitiveTrainingBridgeTest, ProcessInboxEmpty) {
    EXPECT_EQ(cognitive_training_start(bridge), 0);
    int result = cognitive_training_process_inbox(bridge);
    EXPECT_GE(result, 0);  /* 0 or more messages processed */
}

TEST_F(CognitiveTrainingBridgeTest, ProcessInboxNull) {
    EXPECT_LT(cognitive_training_process_inbox(nullptr), 0);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
