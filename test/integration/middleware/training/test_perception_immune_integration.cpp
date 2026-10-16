/**
 * @file test_perception_immune_integration.cpp
 * @brief Cross-bridge integration tests: Perception-Training → Training-Immune
 *
 * WHAT: Tests bidirectional integration between perception and immune bridges
 * WHY:  Verify perception collapse triggers immune response, inflammation reduces sensitivity
 * HOW:  Create both bridges, connect them, verify instability detection and modulation
 *
 * TEST COVERAGE:
 * - Connection lifecycle (4 tests)
 * - Perception collapse detection (4 tests)
 * - Immune sensitivity modulation (4 tests)
 * - Bidirectional flow (3 tests)
 *
 * TOTAL: 15 tests
 *
 * @author NIMCP Development Team
 * @date 2025-12-20
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "middleware/training/nimcp_perception_training_bridge.h"
#include "middleware/immune/nimcp_training_immune.h"
#include "utils/error/nimcp_error_codes.h"

class PerceptionImmuneIntegrationTest : public ::testing::Test {
protected:
    perception_training_bridge_t* perception_bridge;
    training_immune_system_t* immune_system;
    perception_training_config_t perception_config;
    training_immune_config_t immune_config;

    void SetUp() override {
        perception_training_default_config(&perception_config);
        perception_config.enable_bio_async = false;
        perception_bridge = perception_training_create(&perception_config);
        ASSERT_NE(perception_bridge, nullptr);

        training_immune_default_config(&immune_config);
        immune_config.enable_auto_immune_response = false;  /* Manual control */
        immune_system = training_immune_create(&immune_config);
        ASSERT_NE(immune_system, nullptr);
    }

    void TearDown() override {
        if (immune_system) {
            training_immune_destroy(immune_system);
            immune_system = nullptr;
        }
        if (perception_bridge) {
            perception_training_destroy(perception_bridge);
            perception_bridge = nullptr;
        }
    }
};

//=============================================================================
// Connection Lifecycle (4 tests)
//=============================================================================

TEST_F(PerceptionImmuneIntegrationTest, ConnectPerceptionToImmune) {
    EXPECT_EQ(training_immune_start(immune_system), 0);
    EXPECT_EQ(perception_training_start(perception_bridge), 0);

    EXPECT_EQ(training_immune_connect_perception_training(immune_system, perception_bridge), 0);

    training_immune_stats_t stats;
    EXPECT_EQ(training_immune_get_stats(immune_system, &stats), 0);
    EXPECT_TRUE(stats.perception_training_connected);
}

TEST_F(PerceptionImmuneIntegrationTest, DisconnectPerceptionFromImmune) {
    EXPECT_EQ(training_immune_start(immune_system), 0);
    EXPECT_EQ(perception_training_start(perception_bridge), 0);

    EXPECT_EQ(training_immune_connect_perception_training(immune_system, perception_bridge), 0);
    EXPECT_EQ(training_immune_connect_perception_training(immune_system, nullptr), 0);

    training_immune_stats_t stats;
    EXPECT_EQ(training_immune_get_stats(immune_system, &stats), 0);
    EXPECT_FALSE(stats.perception_training_connected);
}

TEST_F(PerceptionImmuneIntegrationTest, ConnectNullImmuneReturnsError) {
    EXPECT_NE(training_immune_connect_perception_training(nullptr, perception_bridge), 0);
}

TEST_F(PerceptionImmuneIntegrationTest, ReconnectPerceptionBridge) {
    EXPECT_EQ(training_immune_start(immune_system), 0);
    EXPECT_EQ(perception_training_start(perception_bridge), 0);

    EXPECT_EQ(training_immune_connect_perception_training(immune_system, perception_bridge), 0);
    EXPECT_EQ(training_immune_connect_perception_training(immune_system, nullptr), 0);
    EXPECT_EQ(training_immune_connect_perception_training(immune_system, perception_bridge), 0);

    training_immune_stats_t stats;
    EXPECT_EQ(training_immune_get_stats(immune_system, &stats), 0);
    EXPECT_TRUE(stats.perception_training_connected);
}

//=============================================================================
// Perception Collapse Detection (4 tests)
//=============================================================================

TEST_F(PerceptionImmuneIntegrationTest, HealthyPerceptionNoInstability) {
    EXPECT_EQ(training_immune_start(immune_system), 0);
    EXPECT_EQ(perception_training_start(perception_bridge), 0);
    EXPECT_EQ(training_immune_connect_perception_training(immune_system, perception_bridge), 0);

    /* Set healthy perception */
    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_confidence = 0.8f;
    effects.audio_quality = 0.7f;
    effects.comprehension = 0.75f;
    effects.valid = true;
    EXPECT_EQ(perception_training_set_effects_for_testing(perception_bridge, &effects), 0);

    /* Check stability - should return NONE */
    training_instability_type_t instability = training_immune_check_perception_cortical_stability(immune_system);
    EXPECT_EQ(instability, TRAINING_INSTABILITY_NONE);
}

TEST_F(PerceptionImmuneIntegrationTest, PerceptionCollapseDetected) {
    EXPECT_EQ(training_immune_start(immune_system), 0);
    EXPECT_EQ(perception_training_start(perception_bridge), 0);
    EXPECT_EQ(training_immune_connect_perception_training(immune_system, perception_bridge), 0);

    /* Set collapsed perception (all confidences near 0) */
    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_confidence = 0.02f;
    effects.audio_quality = 0.03f;
    effects.comprehension = 0.01f;  /* Average = 0.02, below threshold of 0.1 */
    effects.valid = true;
    EXPECT_EQ(perception_training_set_effects_for_testing(perception_bridge, &effects), 0);

    /* Check stability - should detect perception collapse */
    training_instability_type_t instability = training_immune_check_perception_cortical_stability(immune_system);
    EXPECT_EQ(instability, TRAINING_INSTABILITY_PERCEPTION_COLLAPSE);
}

TEST_F(PerceptionImmuneIntegrationTest, PartialPerceptionOK) {
    EXPECT_EQ(training_immune_start(immune_system), 0);
    EXPECT_EQ(perception_training_start(perception_bridge), 0);
    EXPECT_EQ(training_immune_connect_perception_training(immune_system, perception_bridge), 0);

    /* One modality low, others OK - should be stable */
    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_confidence = 0.05f;  /* Low */
    effects.audio_quality = 0.8f;       /* OK */
    effects.comprehension = 0.7f;       /* OK */
    effects.valid = true;  /* Average = 0.52, above threshold */
    EXPECT_EQ(perception_training_set_effects_for_testing(perception_bridge, &effects), 0);

    training_instability_type_t instability = training_immune_check_perception_cortical_stability(immune_system);
    EXPECT_EQ(instability, TRAINING_INSTABILITY_NONE);
}

TEST_F(PerceptionImmuneIntegrationTest, CollapseCountsTracked) {
    EXPECT_EQ(training_immune_start(immune_system), 0);
    EXPECT_EQ(perception_training_start(perception_bridge), 0);
    EXPECT_EQ(training_immune_connect_perception_training(immune_system, perception_bridge), 0);

    /* Set collapsed perception */
    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_confidence = 0.02f;
    effects.audio_quality = 0.03f;
    effects.comprehension = 0.01f;
    effects.valid = true;
    EXPECT_EQ(perception_training_set_effects_for_testing(perception_bridge, &effects), 0);

    /* Trigger detection multiple times */
    training_immune_check_perception_cortical_stability(immune_system);
    training_immune_check_perception_cortical_stability(immune_system);
    training_immune_check_perception_cortical_stability(immune_system);

    training_immune_stats_t stats;
    EXPECT_EQ(training_immune_get_stats(immune_system, &stats), 0);
    EXPECT_EQ(stats.perception_collapse_count, 3u);
}

//=============================================================================
// Immune Sensitivity Modulation (4 tests)
//=============================================================================

TEST_F(PerceptionImmuneIntegrationTest, NoInflammationFullSensitivity) {
    EXPECT_EQ(training_immune_start(immune_system), 0);

    /* No inflammation = full sensitivity */
    EXPECT_EQ(training_immune_update_inflammation(immune_system, INFLAMMATION_NONE), 0);
    float sensitivity = training_immune_get_perception_sensitivity(immune_system);
    EXPECT_FLOAT_EQ(sensitivity, 1.0f);
}

TEST_F(PerceptionImmuneIntegrationTest, LocalInflammationReducesSensitivity) {
    EXPECT_EQ(training_immune_start(immune_system), 0);

    EXPECT_EQ(training_immune_update_inflammation(immune_system, INFLAMMATION_LOCAL), 0);
    float sensitivity = training_immune_get_perception_sensitivity(immune_system);
    EXPECT_FLOAT_EQ(sensitivity, 0.90f);
}

TEST_F(PerceptionImmuneIntegrationTest, SystemicInflammationSignificantReduction) {
    EXPECT_EQ(training_immune_start(immune_system), 0);

    EXPECT_EQ(training_immune_update_inflammation(immune_system, INFLAMMATION_SYSTEMIC), 0);
    float sensitivity = training_immune_get_perception_sensitivity(immune_system);
    EXPECT_FLOAT_EQ(sensitivity, 0.50f);
}

TEST_F(PerceptionImmuneIntegrationTest, CytokineStormMinimalSensitivity) {
    EXPECT_EQ(training_immune_start(immune_system), 0);

    EXPECT_EQ(training_immune_update_inflammation(immune_system, INFLAMMATION_STORM), 0);
    float sensitivity = training_immune_get_perception_sensitivity(immune_system);
    EXPECT_FLOAT_EQ(sensitivity, 0.30f);
}

//=============================================================================
// Bidirectional Flow (3 tests)
//=============================================================================

TEST_F(PerceptionImmuneIntegrationTest, PerceptionCollapseTriggerImmuneManual) {
    immune_config.enable_auto_immune_response = false;
    training_immune_destroy(immune_system);
    immune_system = training_immune_create(&immune_config);
    ASSERT_NE(immune_system, nullptr);

    EXPECT_EQ(training_immune_start(immune_system), 0);
    EXPECT_EQ(perception_training_start(perception_bridge), 0);
    EXPECT_EQ(training_immune_connect_perception_training(immune_system, perception_bridge), 0);

    /* Set collapsed perception */
    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_confidence = 0.02f;
    effects.audio_quality = 0.03f;
    effects.comprehension = 0.01f;
    effects.valid = true;
    EXPECT_EQ(perception_training_set_effects_for_testing(perception_bridge, &effects), 0);

    /* Detect instability */
    training_instability_type_t instability = training_immune_check_perception_cortical_stability(immune_system);
    EXPECT_EQ(instability, TRAINING_INSTABILITY_PERCEPTION_COLLAPSE);

    /* Manually report and trigger (no brain immune connected, so won't actually trigger) */
    uint32_t event_id;
    EXPECT_EQ(training_immune_report_instability(immune_system, instability, 6, &event_id), 0);
    EXPECT_GT(event_id, 0u);
}

TEST_F(PerceptionImmuneIntegrationTest, SensitivityRecoverAfterInflammationClears) {
    EXPECT_EQ(training_immune_start(immune_system), 0);

    /* Start with systemic inflammation */
    EXPECT_EQ(training_immune_update_inflammation(immune_system, INFLAMMATION_SYSTEMIC), 0);
    EXPECT_FLOAT_EQ(training_immune_get_perception_sensitivity(immune_system), 0.50f);

    /* Clear inflammation */
    EXPECT_EQ(training_immune_update_inflammation(immune_system, INFLAMMATION_NONE), 0);
    EXPECT_FLOAT_EQ(training_immune_get_perception_sensitivity(immune_system), 1.0f);
}

TEST_F(PerceptionImmuneIntegrationTest, IntegrationLoopWithVaryingPerception) {
    EXPECT_EQ(training_immune_start(immune_system), 0);
    EXPECT_EQ(perception_training_start(perception_bridge), 0);
    EXPECT_EQ(training_immune_connect_perception_training(immune_system, perception_bridge), 0);

    /* Simulate loop with varying perception quality */
    for (int step = 0; step < 20; ++step) {
        perception_training_effects_t effects;
        memset(&effects, 0, sizeof(effects));

        /* Perception quality varies: starts low, improves */
        float quality = 0.05f + 0.9f * (step / 20.0f);
        effects.visual_confidence = quality;
        effects.audio_quality = quality * 0.9f;
        effects.comprehension = quality * 0.95f;
        effects.valid = true;
        EXPECT_EQ(perception_training_set_effects_for_testing(perception_bridge, &effects), 0);

        /* Check stability */
        training_instability_type_t instability = training_immune_check_perception_cortical_stability(immune_system);

        /* Early steps should detect collapse (avg < 0.1), later steps should be stable
         * Step 0: avg = 0.0475 < 0.1 → collapse
         * Step 1: avg = 0.095 < 0.1 → collapse
         * Step 2+: avg >= 0.1 → stable
         */
        if (step < 2) {
            EXPECT_EQ(instability, TRAINING_INSTABILITY_PERCEPTION_COLLAPSE);
        } else {
            EXPECT_EQ(instability, TRAINING_INSTABILITY_NONE);
        }
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
