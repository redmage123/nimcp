/**
 * @file test_cortical_immune_integration.cpp
 * @brief Cross-bridge integration tests: Cortical-Training → Training-Immune
 *
 * WHAT: Tests bidirectional integration between cortical and immune bridges
 * WHY:  Verify cortical instabilities (FE explosion, burst collapse) trigger immune response
 * HOW:  Create both bridges, connect them, verify instability detection and modulation
 *
 * TEST COVERAGE:
 * - Connection lifecycle (4 tests)
 * - Free energy explosion detection (3 tests)
 * - Burst rate collapse detection (3 tests)
 * - Combined instability handling (3 tests)
 * - Statistics tracking (2 tests)
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
#include "middleware/training/nimcp_cortical_training_bridge.h"
#include "middleware/immune/nimcp_training_immune.h"
#include "utils/error/nimcp_error_codes.h"

class CorticalImmuneIntegrationTest : public ::testing::Test {
protected:
    cortical_training_bridge_t* cortical_bridge;
    training_immune_system_t* immune_system;
    cortical_training_config_t cortical_config;
    training_immune_config_t immune_config;

    void SetUp() override {
        cortical_training_default_config(&cortical_config);
        cortical_config.enable_bio_async = false;
        cortical_bridge = cortical_training_create(&cortical_config);
        ASSERT_NE(cortical_bridge, nullptr);

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
        if (cortical_bridge) {
            cortical_training_destroy(cortical_bridge);
            cortical_bridge = nullptr;
        }
    }
};

//=============================================================================
// Connection Lifecycle (4 tests)
//=============================================================================

TEST_F(CorticalImmuneIntegrationTest, ConnectCorticalToImmune) {
    EXPECT_EQ(training_immune_start(immune_system), 0);
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);

    EXPECT_EQ(training_immune_connect_cortical_training(immune_system, cortical_bridge), 0);

    training_immune_stats_t stats;
    EXPECT_EQ(training_immune_get_stats(immune_system, &stats), 0);
    EXPECT_TRUE(stats.cortical_training_connected);
}

TEST_F(CorticalImmuneIntegrationTest, DisconnectCorticalFromImmune) {
    EXPECT_EQ(training_immune_start(immune_system), 0);
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);

    EXPECT_EQ(training_immune_connect_cortical_training(immune_system, cortical_bridge), 0);
    EXPECT_EQ(training_immune_connect_cortical_training(immune_system, nullptr), 0);

    training_immune_stats_t stats;
    EXPECT_EQ(training_immune_get_stats(immune_system, &stats), 0);
    EXPECT_FALSE(stats.cortical_training_connected);
}

TEST_F(CorticalImmuneIntegrationTest, ConnectNullImmuneReturnsError) {
    EXPECT_NE(training_immune_connect_cortical_training(nullptr, cortical_bridge), 0);
}

TEST_F(CorticalImmuneIntegrationTest, ReconnectCorticalBridge) {
    EXPECT_EQ(training_immune_start(immune_system), 0);
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);

    EXPECT_EQ(training_immune_connect_cortical_training(immune_system, cortical_bridge), 0);
    EXPECT_EQ(training_immune_connect_cortical_training(immune_system, nullptr), 0);
    EXPECT_EQ(training_immune_connect_cortical_training(immune_system, cortical_bridge), 0);

    training_immune_stats_t stats;
    EXPECT_EQ(training_immune_get_stats(immune_system, &stats), 0);
    EXPECT_TRUE(stats.cortical_training_connected);
}

//=============================================================================
// Free Energy Explosion Detection (3 tests)
//=============================================================================

TEST_F(CorticalImmuneIntegrationTest, HealthyCorticalNoInstability) {
    EXPECT_EQ(training_immune_start(immune_system), 0);
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(training_immune_connect_cortical_training(immune_system, cortical_bridge), 0);

    /* Set healthy cortical state */
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.free_energy = 5.0f;   /* Normal, below explosion threshold (100.0) */
    effects.burst_rate = 0.6f;    /* Normal, above collapse threshold (0.1) */
    effects.valid = true;
    EXPECT_EQ(cortical_training_set_effects_for_testing(cortical_bridge, &effects), 0);

    training_instability_type_t instability = training_immune_check_perception_cortical_stability(immune_system);
    EXPECT_EQ(instability, TRAINING_INSTABILITY_NONE);
}

TEST_F(CorticalImmuneIntegrationTest, FreeEnergyExplosionDetected) {
    EXPECT_EQ(training_immune_start(immune_system), 0);
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(training_immune_connect_cortical_training(immune_system, cortical_bridge), 0);

    /* Set free energy explosion */
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.free_energy = 150.0f;  /* Explosion! Above threshold of 100.0 */
    effects.burst_rate = 0.5f;     /* Normal */
    effects.valid = true;
    EXPECT_EQ(cortical_training_set_effects_for_testing(cortical_bridge, &effects), 0);

    training_instability_type_t instability = training_immune_check_perception_cortical_stability(immune_system);
    EXPECT_EQ(instability, TRAINING_INSTABILITY_CORTICAL_EXPLOSION);
}

TEST_F(CorticalImmuneIntegrationTest, HighFreeEnergyBelowThresholdOK) {
    EXPECT_EQ(training_immune_start(immune_system), 0);
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(training_immune_connect_cortical_training(immune_system, cortical_bridge), 0);

    /* High but below threshold */
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.free_energy = 95.0f;  /* High but OK */
    effects.burst_rate = 0.5f;
    effects.valid = true;
    EXPECT_EQ(cortical_training_set_effects_for_testing(cortical_bridge, &effects), 0);

    training_instability_type_t instability = training_immune_check_perception_cortical_stability(immune_system);
    EXPECT_EQ(instability, TRAINING_INSTABILITY_NONE);
}

//=============================================================================
// Burst Rate Collapse Detection (3 tests)
//=============================================================================

TEST_F(CorticalImmuneIntegrationTest, BurstRateCollapseDetected) {
    EXPECT_EQ(training_immune_start(immune_system), 0);
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(training_immune_connect_cortical_training(immune_system, cortical_bridge), 0);

    /* Set burst rate collapse */
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.free_energy = 5.0f;    /* Normal */
    effects.burst_rate = 0.05f;    /* Collapse! Below threshold of 0.1 */
    effects.valid = true;
    EXPECT_EQ(cortical_training_set_effects_for_testing(cortical_bridge, &effects), 0);

    training_instability_type_t instability = training_immune_check_perception_cortical_stability(immune_system);
    EXPECT_EQ(instability, TRAINING_INSTABILITY_PREDICTION_FAILURE);
}

TEST_F(CorticalImmuneIntegrationTest, LowBurstRateAboveThresholdOK) {
    EXPECT_EQ(training_immune_start(immune_system), 0);
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(training_immune_connect_cortical_training(immune_system, cortical_bridge), 0);

    /* Low but above threshold */
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.free_energy = 5.0f;
    effects.burst_rate = 0.15f;  /* Low but OK */
    effects.valid = true;
    EXPECT_EQ(cortical_training_set_effects_for_testing(cortical_bridge, &effects), 0);

    training_instability_type_t instability = training_immune_check_perception_cortical_stability(immune_system);
    EXPECT_EQ(instability, TRAINING_INSTABILITY_NONE);
}

TEST_F(CorticalImmuneIntegrationTest, ZeroBurstRateIsCollapse) {
    EXPECT_EQ(training_immune_start(immune_system), 0);
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(training_immune_connect_cortical_training(immune_system, cortical_bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.free_energy = 5.0f;
    effects.burst_rate = 0.0f;  /* Complete collapse */
    effects.valid = true;
    EXPECT_EQ(cortical_training_set_effects_for_testing(cortical_bridge, &effects), 0);

    training_instability_type_t instability = training_immune_check_perception_cortical_stability(immune_system);
    EXPECT_EQ(instability, TRAINING_INSTABILITY_PREDICTION_FAILURE);
}

//=============================================================================
// Combined Instability Handling (3 tests)
//=============================================================================

TEST_F(CorticalImmuneIntegrationTest, FreeEnergyExplosionTakesPrecedence) {
    EXPECT_EQ(training_immune_start(immune_system), 0);
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(training_immune_connect_cortical_training(immune_system, cortical_bridge), 0);

    /* Both explosion and collapse - explosion wins */
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.free_energy = 200.0f;  /* Explosion */
    effects.burst_rate = 0.01f;    /* Also collapse */
    effects.valid = true;
    EXPECT_EQ(cortical_training_set_effects_for_testing(cortical_bridge, &effects), 0);

    training_instability_type_t instability = training_immune_check_perception_cortical_stability(immune_system);
    EXPECT_EQ(instability, TRAINING_INSTABILITY_CORTICAL_EXPLOSION);
}

TEST_F(CorticalImmuneIntegrationTest, CorticalInstabilityManualReport) {
    EXPECT_EQ(training_immune_start(immune_system), 0);
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(training_immune_connect_cortical_training(immune_system, cortical_bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.free_energy = 150.0f;
    effects.burst_rate = 0.5f;
    effects.valid = true;
    EXPECT_EQ(cortical_training_set_effects_for_testing(cortical_bridge, &effects), 0);

    training_instability_type_t instability = training_immune_check_perception_cortical_stability(immune_system);
    EXPECT_EQ(instability, TRAINING_INSTABILITY_CORTICAL_EXPLOSION);

    /* Manual report */
    uint32_t event_id;
    EXPECT_EQ(training_immune_report_instability(immune_system, instability, 8, &event_id), 0);
    EXPECT_GT(event_id, 0u);

    /* Verify event recorded */
    const training_instability_event_t* event = training_immune_get_event(immune_system, event_id);
    EXPECT_NE(event, nullptr);
    EXPECT_EQ(event->type, TRAINING_INSTABILITY_CORTICAL_EXPLOSION);
    EXPECT_EQ(event->severity, 8u);
}

TEST_F(CorticalImmuneIntegrationTest, CorticalStateEvolvesOverTime) {
    EXPECT_EQ(training_immune_start(immune_system), 0);
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(training_immune_connect_cortical_training(immune_system, cortical_bridge), 0);

    /* Simulate evolving cortical state */
    for (int step = 0; step < 30; ++step) {
        cortical_training_effects_t effects;
        memset(&effects, 0, sizeof(effects));

        /* Free energy starts high, decreases */
        effects.free_energy = 150.0f - 5.0f * step;  /* 150 → 5 */

        /* Burst rate starts low, increases */
        effects.burst_rate = 0.05f + 0.02f * step;   /* 0.05 → 0.65 */

        effects.valid = true;
        EXPECT_EQ(cortical_training_set_effects_for_testing(cortical_bridge, &effects), 0);

        training_instability_type_t instability = training_immune_check_perception_cortical_stability(immune_system);

        if (step < 10) {
            /* Early: FE explosion (150 > 100) */
            EXPECT_EQ(instability, TRAINING_INSTABILITY_CORTICAL_EXPLOSION);
        } else if (step < 3) {
            /* Middle-early: burst collapse (< 0.1) - but this won't happen due to FE */
        } else {
            /* Later: both OK */
            EXPECT_EQ(instability, TRAINING_INSTABILITY_NONE);
        }
    }
}

//=============================================================================
// Statistics Tracking (2 tests)
//=============================================================================

TEST_F(CorticalImmuneIntegrationTest, ExplosionCountsTracked) {
    EXPECT_EQ(training_immune_start(immune_system), 0);
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(training_immune_connect_cortical_training(immune_system, cortical_bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.free_energy = 150.0f;
    effects.burst_rate = 0.5f;
    effects.valid = true;
    EXPECT_EQ(cortical_training_set_effects_for_testing(cortical_bridge, &effects), 0);

    /* Trigger explosion detection multiple times */
    training_immune_check_perception_cortical_stability(immune_system);
    training_immune_check_perception_cortical_stability(immune_system);

    training_immune_stats_t stats;
    EXPECT_EQ(training_immune_get_stats(immune_system, &stats), 0);
    EXPECT_EQ(stats.cortical_explosion_count, 2u);
}

TEST_F(CorticalImmuneIntegrationTest, PredictionFailureCountsTracked) {
    EXPECT_EQ(training_immune_start(immune_system), 0);
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(training_immune_connect_cortical_training(immune_system, cortical_bridge), 0);

    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.free_energy = 5.0f;
    effects.burst_rate = 0.05f;  /* Collapse */
    effects.valid = true;
    EXPECT_EQ(cortical_training_set_effects_for_testing(cortical_bridge, &effects), 0);

    training_immune_check_perception_cortical_stability(immune_system);
    training_immune_check_perception_cortical_stability(immune_system);
    training_immune_check_perception_cortical_stability(immune_system);

    training_immune_stats_t stats;
    EXPECT_EQ(training_immune_get_stats(immune_system, &stats), 0);
    EXPECT_EQ(stats.prediction_failure_count, 3u);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
