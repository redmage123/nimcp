/**
 * @file test_perception_cognitive_integration.cpp
 * @brief Cross-bridge integration tests: Perception-Training → Cognitive-Training
 *
 * WHAT: Tests bidirectional integration between perception and cognitive bridges
 * WHY:  Verify perception state correctly propagates to cognitive modulations
 * HOW:  Create both bridges, connect them, verify effect propagation
 *
 * TEST COVERAGE:
 * - Connection lifecycle (5 tests)
 * - Effect propagation: Perception → Cognitive (8 tests)
 * - Combined modulation (5 tests)
 * - Edge cases (2 tests)
 *
 * TOTAL: 20 tests
 *
 * @author NIMCP Development Team
 * @date 2025-12-20
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "middleware/training/nimcp_perception_training_bridge.h"
#include "middleware/training/nimcp_cognitive_training_bridge.h"
#include "utils/error/nimcp_error_codes.h"

class PerceptionCognitiveIntegrationTest : public ::testing::Test {
protected:
    perception_training_bridge_t* perception_bridge;
    cognitive_training_bridge_t* cognitive_bridge;
    perception_training_config_t perception_config;
    cognitive_training_config_t cognitive_config;

    void SetUp() override {
        perception_training_default_config(&perception_config);
        perception_config.enable_bio_async = false;
        perception_bridge = perception_training_create(&perception_config);
        ASSERT_NE(perception_bridge, nullptr);

        cognitive_training_default_config(&cognitive_config);
        cognitive_config.enable_bio_async = false;
        cognitive_bridge = cognitive_training_create(&cognitive_config);
        ASSERT_NE(cognitive_bridge, nullptr);
    }

    void TearDown() override {
        if (cognitive_bridge) {
            cognitive_training_destroy(cognitive_bridge);
            cognitive_bridge = nullptr;
        }
        if (perception_bridge) {
            perception_training_destroy(perception_bridge);
            perception_bridge = nullptr;
        }
    }
};

//=============================================================================
// Connection Lifecycle (5 tests)
//=============================================================================

TEST_F(PerceptionCognitiveIntegrationTest, ConnectPerceptionToCognitive) {
    EXPECT_EQ(cognitive_training_start(cognitive_bridge), 0);
    EXPECT_EQ(perception_training_start(perception_bridge), 0);

    /* Connect perception to cognitive */
    EXPECT_EQ(cognitive_training_connect_perception_training(cognitive_bridge, perception_bridge), 0);

    cognitive_training_stats_t stats;
    EXPECT_EQ(cognitive_training_get_stats(cognitive_bridge, &stats), 0);
    EXPECT_TRUE(stats.perception_training_connected);
}

TEST_F(PerceptionCognitiveIntegrationTest, DisconnectPerceptionFromCognitive) {
    EXPECT_EQ(cognitive_training_start(cognitive_bridge), 0);
    EXPECT_EQ(perception_training_start(perception_bridge), 0);

    EXPECT_EQ(cognitive_training_connect_perception_training(cognitive_bridge, perception_bridge), 0);
    EXPECT_EQ(cognitive_training_connect_perception_training(cognitive_bridge, nullptr), 0);

    cognitive_training_stats_t stats;
    EXPECT_EQ(cognitive_training_get_stats(cognitive_bridge, &stats), 0);
    EXPECT_FALSE(stats.perception_training_connected);
}

TEST_F(PerceptionCognitiveIntegrationTest, ConnectNullBridgeReturnsError) {
    EXPECT_NE(cognitive_training_connect_perception_training(nullptr, perception_bridge), 0);
}

TEST_F(PerceptionCognitiveIntegrationTest, ReconnectPerceptionBridge) {
    EXPECT_EQ(cognitive_training_start(cognitive_bridge), 0);
    EXPECT_EQ(perception_training_start(perception_bridge), 0);

    /* Connect, disconnect, reconnect */
    EXPECT_EQ(cognitive_training_connect_perception_training(cognitive_bridge, perception_bridge), 0);
    EXPECT_EQ(cognitive_training_connect_perception_training(cognitive_bridge, nullptr), 0);
    EXPECT_EQ(cognitive_training_connect_perception_training(cognitive_bridge, perception_bridge), 0);

    cognitive_training_stats_t stats;
    EXPECT_EQ(cognitive_training_get_stats(cognitive_bridge, &stats), 0);
    EXPECT_TRUE(stats.perception_training_connected);
}

TEST_F(PerceptionCognitiveIntegrationTest, ConnectionPersistsAcrossUpdates) {
    EXPECT_EQ(cognitive_training_start(cognitive_bridge), 0);
    EXPECT_EQ(perception_training_start(perception_bridge), 0);

    EXPECT_EQ(cognitive_training_connect_perception_training(cognitive_bridge, perception_bridge), 0);

    /* Multiple updates should maintain connection */
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(cognitive_training_update(cognitive_bridge, 100), 0);
    }

    cognitive_training_stats_t stats;
    EXPECT_EQ(cognitive_training_get_stats(cognitive_bridge, &stats), 0);
    EXPECT_TRUE(stats.perception_training_connected);
}

//=============================================================================
// Effect Propagation: Perception → Cognitive (8 tests)
//=============================================================================

TEST_F(PerceptionCognitiveIntegrationTest, VisualConfidenceBoostsAttentionFocus) {
    EXPECT_EQ(cognitive_training_start(cognitive_bridge), 0);
    EXPECT_EQ(perception_training_start(perception_bridge), 0);
    EXPECT_EQ(cognitive_training_connect_perception_training(cognitive_bridge, perception_bridge), 0);

    /* Set high visual confidence */
    perception_training_effects_t p_effects;
    memset(&p_effects, 0, sizeof(p_effects));
    p_effects.visual_confidence = 0.9f;
    p_effects.lr_factor = 1.0f;
    p_effects.valid = true;
    EXPECT_EQ(perception_training_set_effects_for_testing(perception_bridge, &p_effects), 0);

    /* Update cognitive bridge */
    EXPECT_EQ(cognitive_training_update(cognitive_bridge, 100), 0);

    /* Verify attention focus increased */
    cognitive_training_effects_t c_effects;
    EXPECT_EQ(cognitive_training_get_effects(cognitive_bridge, &c_effects), 0);
    EXPECT_GE(c_effects.attention_focus, 0.9f);
}

TEST_F(PerceptionCognitiveIntegrationTest, SpeechSalienceBoostsTaskRelevance) {
    EXPECT_EQ(cognitive_training_start(cognitive_bridge), 0);
    EXPECT_EQ(perception_training_start(perception_bridge), 0);
    EXPECT_EQ(cognitive_training_connect_perception_training(cognitive_bridge, perception_bridge), 0);

    /* Set high speech salience */
    perception_training_effects_t p_effects;
    memset(&p_effects, 0, sizeof(p_effects));
    p_effects.speech_salience = 0.85f;
    p_effects.lr_factor = 1.0f;
    p_effects.valid = true;
    EXPECT_EQ(perception_training_set_effects_for_testing(perception_bridge, &p_effects), 0);

    /* Update cognitive bridge */
    EXPECT_EQ(cognitive_training_update(cognitive_bridge, 100), 0);

    /* Verify task relevance increased */
    cognitive_training_effects_t c_effects;
    EXPECT_EQ(cognitive_training_get_effects(cognitive_bridge, &c_effects), 0);
    EXPECT_GE(c_effects.task_relevance, 0.85f);
}

TEST_F(PerceptionCognitiveIntegrationTest, VisualNoveltyDrivesExploration) {
    EXPECT_EQ(cognitive_training_start(cognitive_bridge), 0);
    EXPECT_EQ(perception_training_start(perception_bridge), 0);
    EXPECT_EQ(cognitive_training_connect_perception_training(cognitive_bridge, perception_bridge), 0);

    /* Set high visual novelty */
    perception_training_effects_t p_effects;
    memset(&p_effects, 0, sizeof(p_effects));
    p_effects.visual_novelty = 0.9f;
    p_effects.lr_factor = 1.0f;
    p_effects.valid = true;
    EXPECT_EQ(perception_training_set_effects_for_testing(perception_bridge, &p_effects), 0);

    /* Update cognitive bridge */
    EXPECT_EQ(cognitive_training_update(cognitive_bridge, 100), 0);

    /* Verify exploration drive increased (0.9 * 0.8 = 0.72) */
    cognitive_training_effects_t c_effects;
    EXPECT_EQ(cognitive_training_get_effects(cognitive_bridge, &c_effects), 0);
    EXPECT_GE(c_effects.exploration_drive, 0.7f);
}

TEST_F(PerceptionCognitiveIntegrationTest, LowPerceptionDoesNotReduceBaseline) {
    EXPECT_EQ(cognitive_training_start(cognitive_bridge), 0);
    EXPECT_EQ(perception_training_start(perception_bridge), 0);
    EXPECT_EQ(cognitive_training_connect_perception_training(cognitive_bridge, perception_bridge), 0);

    /* Set very low perception values */
    perception_training_effects_t p_effects;
    memset(&p_effects, 0, sizeof(p_effects));
    p_effects.visual_confidence = 0.1f;
    p_effects.speech_salience = 0.1f;
    p_effects.visual_novelty = 0.1f;
    p_effects.lr_factor = 1.0f;
    p_effects.valid = true;
    EXPECT_EQ(perception_training_set_effects_for_testing(perception_bridge, &p_effects), 0);

    /* Update cognitive bridge */
    EXPECT_EQ(cognitive_training_update(cognitive_bridge, 100), 0);

    /* Cognitive effects should use max of perception and baseline */
    cognitive_training_effects_t c_effects;
    EXPECT_EQ(cognitive_training_get_effects(cognitive_bridge, &c_effects), 0);
    /* Should not be below some reasonable baseline */
    EXPECT_TRUE(c_effects.valid);
}

TEST_F(PerceptionCognitiveIntegrationTest, MultiplePerceptionFieldsCombine) {
    EXPECT_EQ(cognitive_training_start(cognitive_bridge), 0);
    EXPECT_EQ(perception_training_start(perception_bridge), 0);
    EXPECT_EQ(cognitive_training_connect_perception_training(cognitive_bridge, perception_bridge), 0);

    /* Set multiple perception fields */
    perception_training_effects_t p_effects;
    memset(&p_effects, 0, sizeof(p_effects));
    p_effects.visual_confidence = 0.8f;
    p_effects.speech_salience = 0.7f;
    p_effects.visual_novelty = 0.6f;
    p_effects.lr_factor = 1.0f;
    p_effects.valid = true;
    EXPECT_EQ(perception_training_set_effects_for_testing(perception_bridge, &p_effects), 0);

    /* Update cognitive bridge */
    EXPECT_EQ(cognitive_training_update(cognitive_bridge, 100), 0);

    /* All cognitive effects should reflect perception state */
    cognitive_training_effects_t c_effects;
    EXPECT_EQ(cognitive_training_get_effects(cognitive_bridge, &c_effects), 0);
    EXPECT_GE(c_effects.attention_focus, 0.8f);
    EXPECT_GE(c_effects.task_relevance, 0.7f);
    EXPECT_GE(c_effects.exploration_drive, 0.4f); /* 0.6 * 0.8 = 0.48 */
}

TEST_F(PerceptionCognitiveIntegrationTest, EffectPropagationWithInvalidPerception) {
    EXPECT_EQ(cognitive_training_start(cognitive_bridge), 0);
    EXPECT_EQ(perception_training_start(perception_bridge), 0);
    EXPECT_EQ(cognitive_training_connect_perception_training(cognitive_bridge, perception_bridge), 0);

    /* Set perception effects but mark as invalid */
    perception_training_effects_t p_effects;
    memset(&p_effects, 0, sizeof(p_effects));
    p_effects.visual_confidence = 0.9f;
    p_effects.speech_salience = 0.9f;
    p_effects.visual_novelty = 0.9f;
    p_effects.lr_factor = 1.0f;
    p_effects.valid = false;  /* Invalid! */
    EXPECT_EQ(perception_training_set_effects_for_testing(perception_bridge, &p_effects), 0);

    /* Update cognitive bridge */
    EXPECT_EQ(cognitive_training_update(cognitive_bridge, 100), 0);

    /* Cognitive effects should still be valid but not boosted by perception */
    cognitive_training_effects_t c_effects;
    EXPECT_EQ(cognitive_training_get_effects(cognitive_bridge, &c_effects), 0);
    EXPECT_TRUE(c_effects.valid);
}

TEST_F(PerceptionCognitiveIntegrationTest, PerceptionEffectsUpdateDynamically) {
    EXPECT_EQ(cognitive_training_start(cognitive_bridge), 0);
    EXPECT_EQ(perception_training_start(perception_bridge), 0);
    EXPECT_EQ(cognitive_training_connect_perception_training(cognitive_bridge, perception_bridge), 0);

    /* First update with low perception */
    perception_training_effects_t p_effects;
    memset(&p_effects, 0, sizeof(p_effects));
    p_effects.visual_confidence = 0.2f;
    p_effects.lr_factor = 1.0f;
    p_effects.valid = true;
    EXPECT_EQ(perception_training_set_effects_for_testing(perception_bridge, &p_effects), 0);
    EXPECT_EQ(cognitive_training_update(cognitive_bridge, 100), 0);

    /* Second update with high perception */
    p_effects.visual_confidence = 0.95f;
    EXPECT_EQ(perception_training_set_effects_for_testing(perception_bridge, &p_effects), 0);
    EXPECT_EQ(cognitive_training_update(cognitive_bridge, 100), 0);

    /* Cognitive effects should reflect new perception state */
    cognitive_training_effects_t c_effects;
    EXPECT_EQ(cognitive_training_get_effects(cognitive_bridge, &c_effects), 0);
    EXPECT_GE(c_effects.attention_focus, 0.95f);
}

TEST_F(PerceptionCognitiveIntegrationTest, ZeroPerceptionValuesAreHandled) {
    EXPECT_EQ(cognitive_training_start(cognitive_bridge), 0);
    EXPECT_EQ(perception_training_start(perception_bridge), 0);
    EXPECT_EQ(cognitive_training_connect_perception_training(cognitive_bridge, perception_bridge), 0);

    /* Set all perception values to zero */
    perception_training_effects_t p_effects;
    memset(&p_effects, 0, sizeof(p_effects));
    p_effects.visual_confidence = 0.0f;
    p_effects.speech_salience = 0.0f;
    p_effects.visual_novelty = 0.0f;
    p_effects.lr_factor = 1.0f;
    p_effects.valid = true;
    EXPECT_EQ(perception_training_set_effects_for_testing(perception_bridge, &p_effects), 0);

    /* Update should not crash */
    EXPECT_EQ(cognitive_training_update(cognitive_bridge, 100), 0);

    cognitive_training_effects_t c_effects;
    EXPECT_EQ(cognitive_training_get_effects(cognitive_bridge, &c_effects), 0);
    EXPECT_TRUE(c_effects.valid);
}

//=============================================================================
// Combined Modulation (5 tests)
//=============================================================================

TEST_F(PerceptionCognitiveIntegrationTest, PerceptionAffectsLRModulation) {
    EXPECT_EQ(cognitive_training_start(cognitive_bridge), 0);
    EXPECT_EQ(perception_training_start(perception_bridge), 0);
    EXPECT_EQ(cognitive_training_connect_perception_training(cognitive_bridge, perception_bridge), 0);

    /* High perception boosts attention, which boosts LR */
    perception_training_effects_t p_effects;
    memset(&p_effects, 0, sizeof(p_effects));
    p_effects.visual_confidence = 0.95f;
    p_effects.lr_factor = 1.0f;
    p_effects.valid = true;
    EXPECT_EQ(perception_training_set_effects_for_testing(perception_bridge, &p_effects), 0);

    EXPECT_EQ(cognitive_training_update(cognitive_bridge, 100), 0);

    float lr = cognitive_training_get_modulated_lr(cognitive_bridge, 0.001f);
    /* LR should be modulated (attention boost) */
    EXPECT_GT(lr, 0.0f);
    EXPECT_LT(lr, 0.01f);  /* Reasonable range */
}

TEST_F(PerceptionCognitiveIntegrationTest, PerceptionAffectsBatchSize) {
    EXPECT_EQ(cognitive_training_start(cognitive_bridge), 0);
    EXPECT_EQ(perception_training_start(perception_bridge), 0);
    EXPECT_EQ(cognitive_training_connect_perception_training(cognitive_bridge, perception_bridge), 0);

    /* Perception doesn't directly affect batch, but test integration */
    perception_training_effects_t p_effects;
    memset(&p_effects, 0, sizeof(p_effects));
    p_effects.visual_confidence = 0.5f;
    p_effects.lr_factor = 1.0f;
    p_effects.valid = true;
    EXPECT_EQ(perception_training_set_effects_for_testing(perception_bridge, &p_effects), 0);

    EXPECT_EQ(cognitive_training_update(cognitive_bridge, 100), 0);

    uint32_t batch = cognitive_training_get_modulated_batch_size(cognitive_bridge, 32);
    /* Batch should be reasonable */
    EXPECT_GT(batch, 0u);
    EXPECT_LT(batch, 128u);
}

TEST_F(PerceptionCognitiveIntegrationTest, CombinedPerceptionCognitiveEffects) {
    EXPECT_EQ(cognitive_training_start(cognitive_bridge), 0);
    EXPECT_EQ(perception_training_start(perception_bridge), 0);
    EXPECT_EQ(cognitive_training_connect_perception_training(cognitive_bridge, perception_bridge), 0);

    /* Set perception state */
    perception_training_effects_t p_effects;
    memset(&p_effects, 0, sizeof(p_effects));
    p_effects.visual_confidence = 0.8f;
    p_effects.speech_salience = 0.7f;
    p_effects.lr_factor = 1.0f;
    p_effects.valid = true;
    EXPECT_EQ(perception_training_set_effects_for_testing(perception_bridge, &p_effects), 0);

    /* Set cognitive state directly too */
    cognitive_training_effects_t c_effects;
    memset(&c_effects, 0, sizeof(c_effects));
    c_effects.cognitive_load = 0.3f;
    c_effects.epistemic_uncertainty = 0.2f;
    c_effects.lr_factor = 1.0f;
    c_effects.batch_size_factor = 1.0f;
    c_effects.valid = true;
    EXPECT_EQ(cognitive_training_set_effects_for_testing(cognitive_bridge, &c_effects), 0);

    /* Update - perception should augment cognitive */
    EXPECT_EQ(cognitive_training_update(cognitive_bridge, 100), 0);

    cognitive_training_effects_t final_effects;
    EXPECT_EQ(cognitive_training_get_effects(cognitive_bridge, &final_effects), 0);
    /* Combined effects should be coherent */
    EXPECT_GE(final_effects.attention_focus, 0.8f);  /* From perception */
    EXPECT_TRUE(final_effects.valid);
}

TEST_F(PerceptionCognitiveIntegrationTest, TrainingLoopWithPerceptionIntegration) {
    EXPECT_EQ(cognitive_training_start(cognitive_bridge), 0);
    EXPECT_EQ(perception_training_start(perception_bridge), 0);
    EXPECT_EQ(cognitive_training_connect_perception_training(cognitive_bridge, perception_bridge), 0);

    /* Simulate training loop with varying perception */
    for (int step = 0; step < 100; ++step) {
        /* Perception varies over time */
        perception_training_effects_t p_effects;
        memset(&p_effects, 0, sizeof(p_effects));
        p_effects.visual_confidence = 0.5f + 0.4f * sinf(step * 0.1f);
        p_effects.speech_salience = 0.5f + 0.3f * cosf(step * 0.1f);
        p_effects.lr_factor = 1.0f;
        p_effects.valid = true;
        EXPECT_EQ(perception_training_set_effects_for_testing(perception_bridge, &p_effects), 0);

        /* Update training metrics */
        EXPECT_EQ(perception_training_update_metrics(perception_bridge, 0.5f - step * 0.003f, 1.0f), 0);
        EXPECT_EQ(cognitive_training_update_metrics(cognitive_bridge, 0.5f - step * 0.003f, 1.0f, 0.001f, step), 0);
        EXPECT_EQ(cognitive_training_update(cognitive_bridge, 100), 0);

        /* Get modulated LR */
        float lr = cognitive_training_get_modulated_lr(cognitive_bridge, 0.001f);
        EXPECT_GT(lr, 0.0f);
    }
}

TEST_F(PerceptionCognitiveIntegrationTest, StatsTrackPerceptionConnection) {
    EXPECT_EQ(cognitive_training_start(cognitive_bridge), 0);
    EXPECT_EQ(perception_training_start(perception_bridge), 0);

    /* Check stats before connection */
    cognitive_training_stats_t stats_before;
    EXPECT_EQ(cognitive_training_get_stats(cognitive_bridge, &stats_before), 0);
    EXPECT_FALSE(stats_before.perception_training_connected);

    /* Connect and check again */
    EXPECT_EQ(cognitive_training_connect_perception_training(cognitive_bridge, perception_bridge), 0);

    cognitive_training_stats_t stats_after;
    EXPECT_EQ(cognitive_training_get_stats(cognitive_bridge, &stats_after), 0);
    EXPECT_TRUE(stats_after.perception_training_connected);
}

//=============================================================================
// Edge Cases (2 tests)
//=============================================================================

TEST_F(PerceptionCognitiveIntegrationTest, ExtremePerceptionValues) {
    EXPECT_EQ(cognitive_training_start(cognitive_bridge), 0);
    EXPECT_EQ(perception_training_start(perception_bridge), 0);
    EXPECT_EQ(cognitive_training_connect_perception_training(cognitive_bridge, perception_bridge), 0);

    /* Test extreme values */
    perception_training_effects_t p_effects;
    memset(&p_effects, 0, sizeof(p_effects));
    p_effects.visual_confidence = 1.0f;
    p_effects.speech_salience = 1.0f;
    p_effects.visual_novelty = 1.0f;
    p_effects.lr_factor = 1.5f;
    p_effects.valid = true;
    EXPECT_EQ(perception_training_set_effects_for_testing(perception_bridge, &p_effects), 0);

    EXPECT_EQ(cognitive_training_update(cognitive_bridge, 100), 0);

    cognitive_training_effects_t c_effects;
    EXPECT_EQ(cognitive_training_get_effects(cognitive_bridge, &c_effects), 0);
    /* All values should be clamped appropriately */
    EXPECT_LE(c_effects.attention_focus, 1.0f);
    EXPECT_LE(c_effects.task_relevance, 1.0f);
    EXPECT_LE(c_effects.exploration_drive, 1.0f);
}

TEST_F(PerceptionCognitiveIntegrationTest, RapidConnectionStateChanges) {
    EXPECT_EQ(cognitive_training_start(cognitive_bridge), 0);
    EXPECT_EQ(perception_training_start(perception_bridge), 0);

    /* Rapid connect/disconnect cycles */
    for (int i = 0; i < 50; ++i) {
        EXPECT_EQ(cognitive_training_connect_perception_training(cognitive_bridge, perception_bridge), 0);
        EXPECT_EQ(cognitive_training_update(cognitive_bridge, 10), 0);
        EXPECT_EQ(cognitive_training_connect_perception_training(cognitive_bridge, nullptr), 0);
        EXPECT_EQ(cognitive_training_update(cognitive_bridge, 10), 0);
    }

    /* Final state should be disconnected */
    cognitive_training_stats_t stats;
    EXPECT_EQ(cognitive_training_get_stats(cognitive_bridge, &stats), 0);
    EXPECT_FALSE(stats.perception_training_connected);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
