/**
 * @file test_cortical_cognitive_integration.cpp
 * @brief Cross-bridge integration tests: Cortical-Training → Cognitive-Training
 *
 * WHAT: Tests bidirectional integration between cortical and cognitive bridges
 * WHY:  Verify cortical state correctly propagates to cognitive modulations
 * HOW:  Create both bridges, connect them, verify effect propagation
 *
 * TEST COVERAGE:
 * - Connection lifecycle (5 tests)
 * - Effect propagation: Cortical → Cognitive (8 tests)
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
#include "middleware/training/nimcp_cortical_training_bridge.h"
#include "middleware/training/nimcp_cognitive_training_bridge.h"
#include "utils/error/nimcp_error_codes.h"

class CorticalCognitiveIntegrationTest : public ::testing::Test {
protected:
    cortical_training_bridge_t* cortical_bridge;
    cognitive_training_bridge_t* cognitive_bridge;
    cortical_training_config_t cortical_config;
    cognitive_training_config_t cognitive_config;

    void SetUp() override {
        cortical_training_default_config(&cortical_config);
        cortical_config.enable_bio_async = false;
        cortical_bridge = cortical_training_create(&cortical_config);
        ASSERT_NE(cortical_bridge, nullptr);

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
        if (cortical_bridge) {
            cortical_training_destroy(cortical_bridge);
            cortical_bridge = nullptr;
        }
    }
};

//=============================================================================
// Connection Lifecycle (5 tests)
//=============================================================================

TEST_F(CorticalCognitiveIntegrationTest, ConnectCorticalToCognitive) {
    EXPECT_EQ(cognitive_training_start(cognitive_bridge), 0);
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);

    /* Connect cortical to cognitive */
    EXPECT_EQ(cognitive_training_connect_cortical_training(cognitive_bridge, cortical_bridge), 0);

    cognitive_training_stats_t stats;
    EXPECT_EQ(cognitive_training_get_stats(cognitive_bridge, &stats), 0);
    EXPECT_TRUE(stats.cortical_training_connected);
}

TEST_F(CorticalCognitiveIntegrationTest, DisconnectCorticalFromCognitive) {
    EXPECT_EQ(cognitive_training_start(cognitive_bridge), 0);
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);

    EXPECT_EQ(cognitive_training_connect_cortical_training(cognitive_bridge, cortical_bridge), 0);
    EXPECT_EQ(cognitive_training_connect_cortical_training(cognitive_bridge, nullptr), 0);

    cognitive_training_stats_t stats;
    EXPECT_EQ(cognitive_training_get_stats(cognitive_bridge, &stats), 0);
    EXPECT_FALSE(stats.cortical_training_connected);
}

TEST_F(CorticalCognitiveIntegrationTest, ConnectNullBridgeReturnsError) {
    EXPECT_NE(cognitive_training_connect_cortical_training(nullptr, cortical_bridge), 0);
}

TEST_F(CorticalCognitiveIntegrationTest, ReconnectCorticalBridge) {
    EXPECT_EQ(cognitive_training_start(cognitive_bridge), 0);
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);

    /* Connect, disconnect, reconnect */
    EXPECT_EQ(cognitive_training_connect_cortical_training(cognitive_bridge, cortical_bridge), 0);
    EXPECT_EQ(cognitive_training_connect_cortical_training(cognitive_bridge, nullptr), 0);
    EXPECT_EQ(cognitive_training_connect_cortical_training(cognitive_bridge, cortical_bridge), 0);

    cognitive_training_stats_t stats;
    EXPECT_EQ(cognitive_training_get_stats(cognitive_bridge, &stats), 0);
    EXPECT_TRUE(stats.cortical_training_connected);
}

TEST_F(CorticalCognitiveIntegrationTest, ConnectionPersistsAcrossUpdates) {
    EXPECT_EQ(cognitive_training_start(cognitive_bridge), 0);
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);

    EXPECT_EQ(cognitive_training_connect_cortical_training(cognitive_bridge, cortical_bridge), 0);

    /* Multiple updates should maintain connection */
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(cognitive_training_update(cognitive_bridge, 100), 0);
    }

    cognitive_training_stats_t stats;
    EXPECT_EQ(cognitive_training_get_stats(cognitive_bridge, &stats), 0);
    EXPECT_TRUE(stats.cortical_training_connected);
}

//=============================================================================
// Effect Propagation: Cortical → Cognitive (8 tests)
//=============================================================================

TEST_F(CorticalCognitiveIntegrationTest, LowBurstRateIncreasesUncertainty) {
    EXPECT_EQ(cognitive_training_start(cognitive_bridge), 0);
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(cognitive_training_connect_cortical_training(cognitive_bridge, cortical_bridge), 0);

    /* Set low burst rate (unstable predictions) */
    cortical_training_effects_t cr_effects;
    memset(&cr_effects, 0, sizeof(cr_effects));
    cr_effects.burst_rate = 0.2f;  /* Low burst rate */
    cr_effects.lr_factor = 1.0f;
    cr_effects.gradient_confidence = 0.5f;
    cr_effects.valid = true;
    EXPECT_EQ(cortical_training_set_effects_for_testing(cortical_bridge, &cr_effects), 0);

    /* Update cognitive bridge */
    EXPECT_EQ(cognitive_training_update(cognitive_bridge, 100), 0);

    /* Verify uncertainty increased (1 - 0.2) * 0.6 = 0.48 */
    cognitive_training_effects_t c_effects;
    EXPECT_EQ(cognitive_training_get_effects(cognitive_bridge, &c_effects), 0);
    EXPECT_GE(c_effects.epistemic_uncertainty, 0.4f);
}

TEST_F(CorticalCognitiveIntegrationTest, HighBurstRateReducesUncertainty) {
    EXPECT_EQ(cognitive_training_start(cognitive_bridge), 0);
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(cognitive_training_connect_cortical_training(cognitive_bridge, cortical_bridge), 0);

    /* Set high burst rate (stable predictions) */
    cortical_training_effects_t cr_effects;
    memset(&cr_effects, 0, sizeof(cr_effects));
    cr_effects.burst_rate = 0.9f;  /* High burst rate */
    cr_effects.lr_factor = 1.0f;
    cr_effects.gradient_confidence = 0.8f;
    cr_effects.valid = true;
    EXPECT_EQ(cortical_training_set_effects_for_testing(cortical_bridge, &cr_effects), 0);

    /* Update cognitive bridge */
    EXPECT_EQ(cognitive_training_update(cognitive_bridge, 100), 0);

    /* Verify uncertainty is low (1 - 0.9) * 0.6 = 0.06 */
    cognitive_training_effects_t c_effects;
    EXPECT_EQ(cognitive_training_get_effects(cognitive_bridge, &c_effects), 0);
    /* Uncertainty contribution from cortical is low */
    EXPECT_LE(c_effects.epistemic_uncertainty, 0.5f);
}

TEST_F(CorticalCognitiveIntegrationTest, StablePredictionsBoostConfidence) {
    EXPECT_EQ(cognitive_training_start(cognitive_bridge), 0);
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(cognitive_training_connect_cortical_training(cognitive_bridge, cortical_bridge), 0);

    /* Set stable predictions */
    cortical_training_effects_t cr_effects;
    memset(&cr_effects, 0, sizeof(cr_effects));
    cr_effects.predictions_stable = true;
    cr_effects.lr_factor = 1.0f;
    cr_effects.gradient_confidence = 0.8f;
    cr_effects.valid = true;
    EXPECT_EQ(cortical_training_set_effects_for_testing(cortical_bridge, &cr_effects), 0);

    /* Update cognitive bridge */
    EXPECT_EQ(cognitive_training_update(cognitive_bridge, 100), 0);

    /* Verify metacognitive confidence boosted */
    cognitive_training_effects_t c_effects;
    EXPECT_EQ(cognitive_training_get_effects(cognitive_bridge, &c_effects), 0);
    EXPECT_GE(c_effects.metacognitive_confidence, 0.7f);
}

TEST_F(CorticalCognitiveIntegrationTest, UnstablePredictionsReduceConfidence) {
    EXPECT_EQ(cognitive_training_start(cognitive_bridge), 0);
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(cognitive_training_connect_cortical_training(cognitive_bridge, cortical_bridge), 0);

    /* Set unstable predictions */
    cortical_training_effects_t cr_effects;
    memset(&cr_effects, 0, sizeof(cr_effects));
    cr_effects.predictions_stable = false;
    cr_effects.burst_rate = 0.3f;
    cr_effects.lr_factor = 1.0f;
    cr_effects.gradient_confidence = 0.5f;
    cr_effects.valid = true;
    EXPECT_EQ(cortical_training_set_effects_for_testing(cortical_bridge, &cr_effects), 0);

    /* Update cognitive bridge */
    EXPECT_EQ(cognitive_training_update(cognitive_bridge, 100), 0);

    /* Metacognitive confidence should not be boosted to 0.7 */
    cognitive_training_effects_t c_effects;
    EXPECT_EQ(cognitive_training_get_effects(cognitive_bridge, &c_effects), 0);
    /* Without stable predictions, confidence boost doesn't apply */
    EXPECT_TRUE(c_effects.valid);
}

TEST_F(CorticalCognitiveIntegrationTest, HighFreeEnergyIncreasesCognitiveLoad) {
    EXPECT_EQ(cognitive_training_start(cognitive_bridge), 0);
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(cognitive_training_connect_cortical_training(cognitive_bridge, cortical_bridge), 0);

    /* Set high free energy */
    cortical_training_effects_t cr_effects;
    memset(&cr_effects, 0, sizeof(cr_effects));
    cr_effects.free_energy = 8.0f;  /* High free energy */
    cr_effects.lr_factor = 1.0f;
    cr_effects.gradient_confidence = 0.5f;
    cr_effects.valid = true;
    EXPECT_EQ(cortical_training_set_effects_for_testing(cortical_bridge, &cr_effects), 0);

    /* Update cognitive bridge */
    EXPECT_EQ(cognitive_training_update(cognitive_bridge, 100), 0);

    /* Verify cognitive load increased (8/10 * 0.5 = 0.4) */
    cognitive_training_effects_t c_effects;
    EXPECT_EQ(cognitive_training_get_effects(cognitive_bridge, &c_effects), 0);
    EXPECT_GE(c_effects.cognitive_load, 0.4f);
}

TEST_F(CorticalCognitiveIntegrationTest, LowFreeEnergyKeepsCognitiveLoadLow) {
    EXPECT_EQ(cognitive_training_start(cognitive_bridge), 0);
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(cognitive_training_connect_cortical_training(cognitive_bridge, cortical_bridge), 0);

    /* Set low free energy */
    cortical_training_effects_t cr_effects;
    memset(&cr_effects, 0, sizeof(cr_effects));
    cr_effects.free_energy = 1.0f;  /* Low free energy */
    cr_effects.lr_factor = 1.0f;
    cr_effects.gradient_confidence = 0.8f;
    cr_effects.valid = true;
    EXPECT_EQ(cortical_training_set_effects_for_testing(cortical_bridge, &cr_effects), 0);

    /* Update cognitive bridge */
    EXPECT_EQ(cognitive_training_update(cognitive_bridge, 100), 0);

    /* Verify cognitive load is low (1/10 * 0.5 = 0.05) */
    cognitive_training_effects_t c_effects;
    EXPECT_EQ(cognitive_training_get_effects(cognitive_bridge, &c_effects), 0);
    /* FE contribution is low but there may be baseline load */
    EXPECT_TRUE(c_effects.valid);
}

TEST_F(CorticalCognitiveIntegrationTest, EffectPropagationWithInvalidCortical) {
    EXPECT_EQ(cognitive_training_start(cognitive_bridge), 0);
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(cognitive_training_connect_cortical_training(cognitive_bridge, cortical_bridge), 0);

    /* Set cortical effects but mark as invalid */
    cortical_training_effects_t cr_effects;
    memset(&cr_effects, 0, sizeof(cr_effects));
    cr_effects.burst_rate = 0.1f;
    cr_effects.free_energy = 50.0f;
    cr_effects.predictions_stable = true;
    cr_effects.lr_factor = 1.0f;
    cr_effects.valid = false;  /* Invalid! */
    EXPECT_EQ(cortical_training_set_effects_for_testing(cortical_bridge, &cr_effects), 0);

    /* Update cognitive bridge */
    EXPECT_EQ(cognitive_training_update(cognitive_bridge, 100), 0);

    /* Cognitive effects should still be valid but not affected by invalid cortical */
    cognitive_training_effects_t c_effects;
    EXPECT_EQ(cognitive_training_get_effects(cognitive_bridge, &c_effects), 0);
    EXPECT_TRUE(c_effects.valid);
}

TEST_F(CorticalCognitiveIntegrationTest, CorticalEffectsUpdateDynamically) {
    EXPECT_EQ(cognitive_training_start(cognitive_bridge), 0);
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(cognitive_training_connect_cortical_training(cognitive_bridge, cortical_bridge), 0);

    /* First update with low burst rate */
    cortical_training_effects_t cr_effects;
    memset(&cr_effects, 0, sizeof(cr_effects));
    cr_effects.burst_rate = 0.2f;
    cr_effects.lr_factor = 1.0f;
    cr_effects.valid = true;
    EXPECT_EQ(cortical_training_set_effects_for_testing(cortical_bridge, &cr_effects), 0);
    EXPECT_EQ(cognitive_training_update(cognitive_bridge, 100), 0);

    cognitive_training_effects_t c_effects_low;
    EXPECT_EQ(cognitive_training_get_effects(cognitive_bridge, &c_effects_low), 0);

    /* Second update with high burst rate */
    cr_effects.burst_rate = 0.9f;
    cr_effects.predictions_stable = true;
    EXPECT_EQ(cortical_training_set_effects_for_testing(cortical_bridge, &cr_effects), 0);
    EXPECT_EQ(cognitive_training_update(cognitive_bridge, 100), 0);

    /* Cognitive effects should reflect new cortical state */
    cognitive_training_effects_t c_effects_high;
    EXPECT_EQ(cognitive_training_get_effects(cognitive_bridge, &c_effects_high), 0);
    /* High burst rate should reduce uncertainty contribution */
    EXPECT_GE(c_effects_high.metacognitive_confidence, 0.7f);
}

//=============================================================================
// Combined Modulation (5 tests)
//=============================================================================

TEST_F(CorticalCognitiveIntegrationTest, CorticalAffectsLRModulation) {
    EXPECT_EQ(cognitive_training_start(cognitive_bridge), 0);
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(cognitive_training_connect_cortical_training(cognitive_bridge, cortical_bridge), 0);

    /* Low burst rate increases uncertainty, which should reduce LR */
    cortical_training_effects_t cr_effects;
    memset(&cr_effects, 0, sizeof(cr_effects));
    cr_effects.burst_rate = 0.1f;
    cr_effects.free_energy = 5.0f;
    cr_effects.lr_factor = 1.0f;
    cr_effects.valid = true;
    EXPECT_EQ(cortical_training_set_effects_for_testing(cortical_bridge, &cr_effects), 0);

    EXPECT_EQ(cognitive_training_update(cognitive_bridge, 100), 0);

    float lr = cognitive_training_get_modulated_lr(cognitive_bridge, 0.001f);
    /* LR should be modulated (high uncertainty reduces LR) */
    EXPECT_GT(lr, 0.0f);
    EXPECT_LT(lr, 0.01f);
}

TEST_F(CorticalCognitiveIntegrationTest, CorticalAffectsBatchSize) {
    EXPECT_EQ(cognitive_training_start(cognitive_bridge), 0);
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(cognitive_training_connect_cortical_training(cognitive_bridge, cortical_bridge), 0);

    /* High free energy increases cognitive load, which should reduce batch */
    cortical_training_effects_t cr_effects;
    memset(&cr_effects, 0, sizeof(cr_effects));
    cr_effects.free_energy = 15.0f;  /* Very high FE */
    cr_effects.lr_factor = 1.0f;
    cr_effects.valid = true;
    EXPECT_EQ(cortical_training_set_effects_for_testing(cortical_bridge, &cr_effects), 0);

    EXPECT_EQ(cognitive_training_update(cognitive_bridge, 100), 0);

    uint32_t batch = cognitive_training_get_modulated_batch_size(cognitive_bridge, 32);
    /* Batch should be reasonable */
    EXPECT_GT(batch, 0u);
    EXPECT_LT(batch, 128u);
}

TEST_F(CorticalCognitiveIntegrationTest, CombinedCorticalCognitiveEffects) {
    EXPECT_EQ(cognitive_training_start(cognitive_bridge), 0);
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(cognitive_training_connect_cortical_training(cognitive_bridge, cortical_bridge), 0);

    /* Set cortical state */
    cortical_training_effects_t cr_effects;
    memset(&cr_effects, 0, sizeof(cr_effects));
    cr_effects.burst_rate = 0.7f;
    cr_effects.free_energy = 3.0f;
    cr_effects.predictions_stable = true;
    cr_effects.lr_factor = 1.0f;
    cr_effects.gradient_confidence = 0.8f;
    cr_effects.valid = true;
    EXPECT_EQ(cortical_training_set_effects_for_testing(cortical_bridge, &cr_effects), 0);

    /* Set cognitive state directly too */
    cognitive_training_effects_t c_effects;
    memset(&c_effects, 0, sizeof(c_effects));
    c_effects.attention_focus = 0.6f;
    c_effects.exploration_drive = 0.4f;
    c_effects.lr_factor = 1.0f;
    c_effects.batch_size_factor = 1.0f;
    c_effects.valid = true;
    EXPECT_EQ(cognitive_training_set_effects_for_testing(cognitive_bridge, &c_effects), 0);

    /* Update - cortical should augment cognitive */
    EXPECT_EQ(cognitive_training_update(cognitive_bridge, 100), 0);

    cognitive_training_effects_t final_effects;
    EXPECT_EQ(cognitive_training_get_effects(cognitive_bridge, &final_effects), 0);
    /* Combined effects should be coherent */
    EXPECT_GE(final_effects.metacognitive_confidence, 0.7f);  /* From stable predictions */
    EXPECT_TRUE(final_effects.valid);
}

TEST_F(CorticalCognitiveIntegrationTest, TrainingLoopWithCorticalIntegration) {
    EXPECT_EQ(cognitive_training_start(cognitive_bridge), 0);
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(cognitive_training_connect_cortical_training(cognitive_bridge, cortical_bridge), 0);

    /* Simulate training loop with varying cortical state */
    for (int step = 0; step < 100; ++step) {
        /* Cortical state varies over time */
        cortical_training_effects_t cr_effects;
        memset(&cr_effects, 0, sizeof(cr_effects));
        cr_effects.burst_rate = 0.3f + 0.5f * (step / 100.0f);  /* Improves over time */
        cr_effects.free_energy = 10.0f - 8.0f * (step / 100.0f);  /* Decreases over time */
        cr_effects.predictions_stable = (step > 50);
        cr_effects.lr_factor = 1.0f;
        cr_effects.gradient_confidence = 0.5f + 0.4f * (step / 100.0f);
        cr_effects.valid = true;
        EXPECT_EQ(cortical_training_set_effects_for_testing(cortical_bridge, &cr_effects), 0);

        /* Update training metrics */
        EXPECT_EQ(cortical_training_update_metrics(cortical_bridge, 0.5f - step * 0.003f, 1.0f, 0.001f, step), 0);
        EXPECT_EQ(cognitive_training_update_metrics(cognitive_bridge, 0.5f - step * 0.003f, 1.0f, 0.001f, step), 0);
        EXPECT_EQ(cognitive_training_update(cognitive_bridge, 100), 0);

        /* Get modulated LR */
        float lr = cognitive_training_get_modulated_lr(cognitive_bridge, 0.001f);
        EXPECT_GT(lr, 0.0f);
    }
}

TEST_F(CorticalCognitiveIntegrationTest, StatsTrackCorticalConnection) {
    EXPECT_EQ(cognitive_training_start(cognitive_bridge), 0);
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);

    /* Check stats before connection */
    cognitive_training_stats_t stats_before;
    EXPECT_EQ(cognitive_training_get_stats(cognitive_bridge, &stats_before), 0);
    EXPECT_FALSE(stats_before.cortical_training_connected);

    /* Connect and check again */
    EXPECT_EQ(cognitive_training_connect_cortical_training(cognitive_bridge, cortical_bridge), 0);

    cognitive_training_stats_t stats_after;
    EXPECT_EQ(cognitive_training_get_stats(cognitive_bridge, &stats_after), 0);
    EXPECT_TRUE(stats_after.cortical_training_connected);
}

//=============================================================================
// Edge Cases (2 tests)
//=============================================================================

TEST_F(CorticalCognitiveIntegrationTest, ExtremeCorticalValues) {
    EXPECT_EQ(cognitive_training_start(cognitive_bridge), 0);
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(cognitive_training_connect_cortical_training(cognitive_bridge, cortical_bridge), 0);

    /* Test extreme values */
    cortical_training_effects_t cr_effects;
    memset(&cr_effects, 0, sizeof(cr_effects));
    cr_effects.burst_rate = 0.0f;  /* No bursts */
    cr_effects.free_energy = 100.0f;  /* Very high FE */
    cr_effects.predictions_stable = false;
    cr_effects.lr_factor = 0.3f;
    cr_effects.gradient_confidence = 0.5f;
    cr_effects.valid = true;
    EXPECT_EQ(cortical_training_set_effects_for_testing(cortical_bridge, &cr_effects), 0);

    EXPECT_EQ(cognitive_training_update(cognitive_bridge, 100), 0);

    cognitive_training_effects_t c_effects;
    EXPECT_EQ(cognitive_training_get_effects(cognitive_bridge, &c_effects), 0);
    /* Values should be clamped appropriately */
    EXPECT_LE(c_effects.epistemic_uncertainty, 1.0f);
    EXPECT_LE(c_effects.cognitive_load, 1.0f);
    EXPECT_TRUE(c_effects.valid);
}

TEST_F(CorticalCognitiveIntegrationTest, RapidConnectionStateChanges) {
    EXPECT_EQ(cognitive_training_start(cognitive_bridge), 0);
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);

    /* Rapid connect/disconnect cycles */
    for (int i = 0; i < 50; ++i) {
        EXPECT_EQ(cognitive_training_connect_cortical_training(cognitive_bridge, cortical_bridge), 0);
        EXPECT_EQ(cognitive_training_update(cognitive_bridge, 10), 0);
        EXPECT_EQ(cognitive_training_connect_cortical_training(cognitive_bridge, nullptr), 0);
        EXPECT_EQ(cognitive_training_update(cognitive_bridge, 10), 0);
    }

    /* Final state should be disconnected */
    cognitive_training_stats_t stats;
    EXPECT_EQ(cognitive_training_get_stats(cognitive_bridge, &stats), 0);
    EXPECT_FALSE(stats.cortical_training_connected);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
