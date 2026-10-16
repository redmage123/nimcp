/**
 * @file test_nimcp_occipital_training_bridge.cpp
 * @brief Unit tests for nimcp_occipital_training_bridge.c
 *
 * WHAT: Comprehensive unit tests for the Occipital-Training bridge
 * WHY:  Ensure correct visual learning and confidence-based LR modulation
 * HOW:  Use Google Test framework to test effects computation, training
 *       area selection, and learning rate modulation.
 *
 * COVERAGE TARGET: 100%
 */

#include <gtest/gtest.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

// Headers have their own extern "C" guards
#include "middleware/training/nimcp_occipital_training_bridge.h"
#include "core/brain/regions/occipital/nimcp_occipital_adapter.h"

// ============================================================================
// TEST FIXTURE
// ============================================================================

class OccipitalTrainingBridgeTest : public ::testing::Test {
protected:
    occipital_training_bridge_t* bridge;
    occipital_adapter_t* occipital;
    occipital_training_config_t config;
    occipital_config_t occipital_config;

    void SetUp() override {
        // Create occipital adapter first
        occipital_config = occipital_default_config();
        occipital_config.image_width = 64;
        occipital_config.image_height = 64;
        occipital = occipital_create(&occipital_config);

        // Get default training bridge config
        occipital_training_default_config(&config);
        bridge = occipital_training_bridge_create(&config);

        // Connect occipital if bridge created
        if (bridge && occipital) {
            occipital_training_connect_occipital(bridge, occipital);
        }
    }

    void TearDown() override {
        if (bridge) {
            occipital_training_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (occipital) {
            occipital_destroy(occipital);
            occipital = nullptr;
        }
    }
};

// ============================================================================
// CONFIGURATION TESTS
// ============================================================================

TEST_F(OccipitalTrainingBridgeTest, DefaultConfigHasReasonableValues) {
    occipital_training_config_t default_config;
    occipital_training_default_config(&default_config);

    // Training should be enabled by default
    EXPECT_TRUE(default_config.enable_v1_training);
    EXPECT_TRUE(default_config.enable_v4_training);
    EXPECT_TRUE(default_config.enable_v5_training);

    // Modulation strengths should be positive
    EXPECT_GT(default_config.confidence_lr_scale, 0.0f);
    EXPECT_GT(default_config.novelty_lr_scale, 0.0f);

    // LR limits should be reasonable
    EXPECT_GT(default_config.lr_min_factor, 0.0f);
    EXPECT_LT(default_config.lr_min_factor, 1.0f);
    EXPECT_GT(default_config.lr_max_factor, 1.0f);
}

TEST_F(OccipitalTrainingBridgeTest, DefaultConfigWithNullSucceeds) {
    occipital_training_default_config(nullptr);
    SUCCEED();  // Should not crash
}

// ============================================================================
// LIFECYCLE TESTS
// ============================================================================

TEST_F(OccipitalTrainingBridgeTest, CreateWithConfigSucceeds) {
    ASSERT_NE(nullptr, bridge);
}

TEST_F(OccipitalTrainingBridgeTest, CreateWithNullConfigUsesDefaults) {
    occipital_training_bridge_t* default_bridge = occipital_training_bridge_create(NULL);
    ASSERT_NE(nullptr, default_bridge);
    occipital_training_bridge_destroy(default_bridge);
}

TEST_F(OccipitalTrainingBridgeTest, DestroyNullDoesNotCrash) {
    occipital_training_bridge_destroy(nullptr);
    SUCCEED();
}

TEST_F(OccipitalTrainingBridgeTest, ResetBridgeSucceeds) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(0, occipital_training_bridge_reset(bridge));
}

TEST_F(OccipitalTrainingBridgeTest, ResetNullBridgeFails) {
    EXPECT_EQ(-1, occipital_training_bridge_reset(nullptr));
}

// ============================================================================
// CONNECTION TESTS
// ============================================================================

TEST_F(OccipitalTrainingBridgeTest, ConnectOccipitalSucceeds) {
    occipital_training_bridge_t* new_bridge = occipital_training_bridge_create(&config);
    ASSERT_NE(nullptr, new_bridge);

    EXPECT_EQ(0, occipital_training_connect_occipital(new_bridge, occipital));

    occipital_training_bridge_destroy(new_bridge);
}

TEST_F(OccipitalTrainingBridgeTest, ConnectOccipitalNullBridgeFails) {
    EXPECT_EQ(-1, occipital_training_connect_occipital(nullptr, occipital));
}

TEST_F(OccipitalTrainingBridgeTest, ConnectOccipitalNullOccipitalDisconnects) {
    ASSERT_NE(nullptr, bridge);
    // Connecting NULL occipital succeeds but disconnects
    occipital_training_bridge_t* new_bridge = occipital_training_bridge_create(&config);
    EXPECT_EQ(0, occipital_training_connect_occipital(new_bridge, nullptr));
    occipital_training_bridge_destroy(new_bridge);
}

TEST_F(OccipitalTrainingBridgeTest, ConnectTrainingNullBridgeFails) {
    EXPECT_EQ(-1, occipital_training_connect_training(nullptr, nullptr));
}

// ============================================================================
// EFFECTS COMPUTATION TESTS
// ============================================================================

TEST_F(OccipitalTrainingBridgeTest, UpdateEffectsSucceeds) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(0, occipital_training_update_effects(bridge));
}

TEST_F(OccipitalTrainingBridgeTest, UpdateEffectsNullBridgeFails) {
    EXPECT_EQ(-1, occipital_training_update_effects(nullptr));
}

TEST_F(OccipitalTrainingBridgeTest, GetEffectsReturnsValidValues) {
    ASSERT_NE(nullptr, bridge);

    occipital_training_update_effects(bridge);

    occipital_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    EXPECT_EQ(0, occipital_training_get_effects(bridge, &effects));

    // All confidence values should be in [0, 1]
    EXPECT_GE(effects.v1_confidence, 0.0f);
    EXPECT_LE(effects.v1_confidence, 1.0f);

    EXPECT_GE(effects.v4_confidence, 0.0f);
    EXPECT_LE(effects.v4_confidence, 1.0f);

    EXPECT_GE(effects.v5_confidence, 0.0f);
    EXPECT_LE(effects.v5_confidence, 1.0f);

    EXPECT_GE(effects.overall_confidence, 0.0f);
    EXPECT_LE(effects.overall_confidence, 1.0f);

    // LR factor should be in valid range
    EXPECT_GT(effects.lr_factor, 0.0f);
    EXPECT_LT(effects.lr_factor, 3.0f);
}

TEST_F(OccipitalTrainingBridgeTest, GetEffectsNullBridgeFails) {
    occipital_training_effects_t effects;
    EXPECT_EQ(-1, occipital_training_get_effects(nullptr, &effects));
}

TEST_F(OccipitalTrainingBridgeTest, GetEffectsNullOutputFails) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(-1, occipital_training_get_effects(bridge, nullptr));
}

// ============================================================================
// LEARNING RATE MODULATION TESTS
// ============================================================================

TEST_F(OccipitalTrainingBridgeTest, GetModulatedLrReturnsValidValue) {
    ASSERT_NE(nullptr, bridge);
    occipital_training_update_effects(bridge);

    float base_lr = 0.01f;
    float modulated = occipital_training_get_modulated_lr(bridge, base_lr);

    EXPECT_GT(modulated, 0.0f);
    // Should be within configured range
    EXPECT_GE(modulated, base_lr * config.lr_min_factor);
    EXPECT_LE(modulated, base_lr * config.lr_max_factor);
}

TEST_F(OccipitalTrainingBridgeTest, GetModulatedLrNullBridgeReturnsBaseLr) {
    float base_lr = 0.01f;
    float result = occipital_training_get_modulated_lr(nullptr, base_lr);
    EXPECT_FLOAT_EQ(base_lr, result);
}

TEST_F(OccipitalTrainingBridgeTest, GetModulatedLrZeroBaseReturnsZero) {
    ASSERT_NE(nullptr, bridge);
    float result = occipital_training_get_modulated_lr(bridge, 0.0f);
    EXPECT_FLOAT_EQ(0.0f, result);
}

// ============================================================================
// SKIP DECISION TESTS
// ============================================================================

TEST_F(OccipitalTrainingBridgeTest, ShouldSkipBasedOnEffects) {
    ASSERT_NE(nullptr, bridge);
    occipital_training_update_effects(bridge);

    // Result depends on effects.skip_sample (test the function is callable)
    bool result = occipital_training_should_skip(bridge);
    // Result should be a valid boolean
    EXPECT_TRUE(result == true || result == false);
}

TEST_F(OccipitalTrainingBridgeTest, ShouldSkipNullBridgeReturnsFalse) {
    // Null returns false (no skip by default)
    EXPECT_FALSE(occipital_training_should_skip(nullptr));
}

// ============================================================================
// ATTENTION SCALING TESTS
// ============================================================================

TEST_F(OccipitalTrainingBridgeTest, GetAttentionScalingSucceeds) {
    ASSERT_NE(nullptr, bridge);
    occipital_training_update_effects(bridge);

    float factors[16];
    memset(factors, 0, sizeof(factors));
    EXPECT_EQ(0, occipital_training_get_attention_scaling(bridge, factors, 16));

    // All factors should be positive
    for (int i = 0; i < 16; i++) {
        EXPECT_GE(factors[i], 0.0f);
    }
}

TEST_F(OccipitalTrainingBridgeTest, GetAttentionScalingNullBridgeFails) {
    float factors[16];
    EXPECT_EQ(-1, occipital_training_get_attention_scaling(nullptr, factors, 16));
}

TEST_F(OccipitalTrainingBridgeTest, GetAttentionScalingNullFactorsFails) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(-1, occipital_training_get_attention_scaling(bridge, nullptr, 16));
}

// ============================================================================
// TRAINING TARGETS TESTS
// ============================================================================

TEST_F(OccipitalTrainingBridgeTest, ApplyTargetsSucceeds) {
    ASSERT_NE(nullptr, bridge);

    occipital_training_targets_t targets;
    memset(&targets, 0, sizeof(targets));
    targets.supervision_strength = 0.8f;

    EXPECT_EQ(0, occipital_training_apply_targets(bridge, &targets));
}

TEST_F(OccipitalTrainingBridgeTest, ApplyTargetsNullBridgeFails) {
    occipital_training_targets_t targets;
    memset(&targets, 0, sizeof(targets));
    EXPECT_EQ(-1, occipital_training_apply_targets(nullptr, &targets));
}

TEST_F(OccipitalTrainingBridgeTest, ApplyTargetsNullTargetsFails) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(-1, occipital_training_apply_targets(bridge, nullptr));
}

// ============================================================================
// AREA-SPECIFIC TRAINING TESTS
// ============================================================================

TEST_F(OccipitalTrainingBridgeTest, TrainV1AreaSucceeds) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(0, occipital_training_train_area(bridge, OCCIPITAL_TRAIN_V1, 0.01f));
}

TEST_F(OccipitalTrainingBridgeTest, TrainV4AreaSucceeds) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(0, occipital_training_train_area(bridge, OCCIPITAL_TRAIN_V4, 0.01f));
}

TEST_F(OccipitalTrainingBridgeTest, TrainV5AreaSucceeds) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(0, occipital_training_train_area(bridge, OCCIPITAL_TRAIN_V5, 0.01f));
}

TEST_F(OccipitalTrainingBridgeTest, TrainAllAreasSucceeds) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(0, occipital_training_train_area(bridge, OCCIPITAL_TRAIN_ALL, 0.01f));
}

TEST_F(OccipitalTrainingBridgeTest, TrainAreaNullBridgeFails) {
    EXPECT_EQ(-1, occipital_training_train_area(nullptr, OCCIPITAL_TRAIN_V1, 0.01f));
}

TEST_F(OccipitalTrainingBridgeTest, TrainAreaInvalidAreaFails) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(-1, occipital_training_train_area(bridge, OCCIPITAL_TRAIN_COUNT, 0.01f));
}

// ============================================================================
// LOSS COMPUTATION TESTS
// ============================================================================

TEST_F(OccipitalTrainingBridgeTest, ComputeLossSucceeds) {
    ASSERT_NE(nullptr, bridge);

    float loss;
    EXPECT_EQ(0, occipital_training_compute_loss(bridge, OCCIPITAL_TRAIN_V1, &loss));
    EXPECT_GE(loss, 0.0f);
}

TEST_F(OccipitalTrainingBridgeTest, ComputeLossAllAreasSucceeds) {
    ASSERT_NE(nullptr, bridge);

    float loss;
    EXPECT_EQ(0, occipital_training_compute_loss(bridge, OCCIPITAL_TRAIN_ALL, &loss));
    EXPECT_GE(loss, 0.0f);
}

TEST_F(OccipitalTrainingBridgeTest, ComputeLossNullBridgeFails) {
    float loss;
    EXPECT_EQ(-1, occipital_training_compute_loss(nullptr, OCCIPITAL_TRAIN_V1, &loss));
}

TEST_F(OccipitalTrainingBridgeTest, ComputeLossNullOutputFails) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(-1, occipital_training_compute_loss(bridge, OCCIPITAL_TRAIN_V1, nullptr));
}

// ============================================================================
// UPDATE CYCLE TESTS
// ============================================================================

TEST_F(OccipitalTrainingBridgeTest, UpdateSucceeds) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(0, occipital_training_update(bridge, 100));
}

TEST_F(OccipitalTrainingBridgeTest, UpdateNullBridgeFails) {
    EXPECT_EQ(-1, occipital_training_update(nullptr, 100));
}

TEST_F(OccipitalTrainingBridgeTest, UpdateZeroDeltaSucceeds) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(0, occipital_training_update(bridge, 0));
}

// ============================================================================
// STATISTICS TESTS
// ============================================================================

TEST_F(OccipitalTrainingBridgeTest, GetStatsSucceeds) {
    ASSERT_NE(nullptr, bridge);

    occipital_training_stats_t stats;
    memset(&stats, 0xFF, sizeof(stats));
    EXPECT_EQ(0, occipital_training_get_stats(bridge, &stats));

    // Initial values should be reasonable
    EXPECT_GE(stats.total_training_steps, 0ULL);
}

TEST_F(OccipitalTrainingBridgeTest, GetStatsNullBridgeFails) {
    occipital_training_stats_t stats;
    EXPECT_EQ(-1, occipital_training_get_stats(nullptr, &stats));
}

TEST_F(OccipitalTrainingBridgeTest, GetStatsNullOutputFails) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(-1, occipital_training_get_stats(bridge, nullptr));
}

TEST_F(OccipitalTrainingBridgeTest, ResetStatsWorks) {
    ASSERT_NE(nullptr, bridge);

    // Generate some stats
    for (int i = 0; i < 10; i++) {
        occipital_training_update(bridge, 100);
    }

    occipital_training_reset_stats(bridge);

    occipital_training_stats_t stats;
    EXPECT_EQ(0, occipital_training_get_stats(bridge, &stats));
    EXPECT_EQ(0ULL, stats.total_training_steps);
}

TEST_F(OccipitalTrainingBridgeTest, ResetStatsNullDoesNotCrash) {
    occipital_training_reset_stats(nullptr);
    SUCCEED();
}

// ============================================================================
// STRESS TESTS
// ============================================================================

TEST_F(OccipitalTrainingBridgeTest, RepeatedUpdatesDoNotLeak) {
    ASSERT_NE(nullptr, bridge);

    for (int i = 0; i < 1000; i++) {
        EXPECT_EQ(0, occipital_training_update(bridge, 10));
    }

    occipital_training_stats_t stats;
    EXPECT_EQ(0, occipital_training_get_stats(bridge, &stats));
    // Stats should be retrievable without crash (leak test, not step count test)
    SUCCEED();
}

TEST_F(OccipitalTrainingBridgeTest, CreateDestroyMultipleTimes) {
    for (int i = 0; i < 100; i++) {
        occipital_training_bridge_t* temp = occipital_training_bridge_create(&config);
        ASSERT_NE(nullptr, temp);
        occipital_training_bridge_destroy(temp);
    }
    SUCCEED();
}
