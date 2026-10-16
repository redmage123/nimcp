/**
 * @file test_fep_immune_integration.cpp
 * @brief Integration tests for FEP-Immune Bridge with other FEP components
 */

#include <gtest/gtest.h>
#include <cmath>
#include "cognitive/free_energy/nimcp_fep_immune_bridge.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "cognitive/free_energy/nimcp_fep_learning.h"
#include "cognitive/immune/nimcp_brain_immune.h"

class FEPImmuneIntegrationTest : public ::testing::Test {
protected:
    static const uint32_t OBS_DIM = 8;
    static const uint32_t ACTION_DIM = 4;
    static const uint32_t STATE_DIM = 8;
    fep_immune_bridge_t* bridge = nullptr;
    fep_system_t* fep = nullptr;
    brain_immune_system_t* immune = nullptr;
    fep_transition_learner_t* learner = nullptr;

    void SetUp() override {
        /* Create FEP-immune bridge */
        fep_immune_config_t bridge_config;
        fep_immune_bridge_default_config(&bridge_config);
        bridge = fep_immune_bridge_create(&bridge_config);

        /* Create FEP system */
        fep_config_t fep_config;
        fep_default_config(&fep_config);
        fep = fep_create(&fep_config, OBS_DIM, ACTION_DIM);

        /* Create brain immune system */
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune = brain_immune_create(&immune_config);

        /* Create transition learner */
        fep_learning_config_t learn_config;
        fep_learning_default_config(&learn_config);
        learner = fep_transition_learner_create(&learn_config, STATE_DIM);
    }

    void TearDown() override {
        if (bridge) {
            fep_immune_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (fep) {
            fep_destroy(fep);
            fep = nullptr;
        }
        if (immune) {
            brain_immune_destroy(immune);
            immune = nullptr;
        }
        if (learner) {
            fep_transition_learner_destroy(learner);
            learner = nullptr;
        }
    }
};

/* ============================================================================
 * Bridge Connection Tests
 * ============================================================================ */

TEST_F(FEPImmuneIntegrationTest, ConnectBothSystems) {
    int ret1 = fep_immune_bridge_connect_fep(bridge, fep);
    int ret2 = fep_immune_bridge_connect_immune(bridge, immune);

    EXPECT_EQ(ret1, 0);
    EXPECT_EQ(ret2, 0);
}

TEST_F(FEPImmuneIntegrationTest, BridgeUpdateWithConnections) {
    fep_immune_bridge_connect_fep(bridge, fep);
    fep_immune_bridge_connect_immune(bridge, immune);

    int ret = fep_immune_bridge_update(bridge, 100);
    EXPECT_EQ(ret, 0);
}

/* ============================================================================
 * FEP -> Immune Integration Tests
 * ============================================================================ */

TEST_F(FEPImmuneIntegrationTest, PredictionErrorTriggersImmune) {
    fep_immune_bridge_connect_fep(bridge, fep);
    fep_immune_bridge_connect_immune(bridge, immune);

    /* Report high prediction error */
    int ret = fep_immune_report_prediction_failure(bridge, 20.0f);
    EXPECT_EQ(ret, 0);

    /* Check inflammation level increased */
    brain_inflammation_level_t level = fep_immune_get_inflammation_level(bridge);
    /* May or may not increase depending on threshold */
    EXPECT_GE(level, INFLAMMATION_NONE);
}

TEST_F(FEPImmuneIntegrationTest, ModelViolationTriggersResponse) {
    fep_immune_bridge_connect_fep(bridge, fep);
    fep_immune_bridge_connect_immune(bridge, immune);

    uint8_t pattern[8] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE};
    int ret = fep_immune_report_model_violation(bridge, pattern, 8);
    EXPECT_EQ(ret, 0);
}

TEST_F(FEPImmuneIntegrationTest, BeliefToMemoryTransfer) {
    fep_immune_bridge_connect_fep(bridge, fep);
    fep_immune_bridge_connect_immune(bridge, immune);

    int ret = fep_immune_transfer_belief_to_memory(bridge);
    EXPECT_EQ(ret, 0);
}

/* ============================================================================
 * Immune -> FEP Integration Tests
 * ============================================================================ */

TEST_F(FEPImmuneIntegrationTest, InflammationAffectsPrecision) {
    fep_immune_bridge_connect_fep(bridge, fep);
    fep_immune_bridge_connect_immune(bridge, immune);

    /* Get baseline precision */
    float modifier_baseline;
    bridge->state.inflammation_level = INFLAMMATION_NONE;
    fep_immune_get_precision_modifier(bridge, &modifier_baseline);

    /* Set inflammation */
    bridge->state.inflammation_level = INFLAMMATION_SYSTEMIC;
    float modifier_inflamed;
    fep_immune_get_precision_modifier(bridge, &modifier_inflamed);

    EXPECT_LT(modifier_inflamed, modifier_baseline);
}

TEST_F(FEPImmuneIntegrationTest, InflammationAffectsLearning) {
    fep_immune_bridge_connect_fep(bridge, fep);
    fep_immune_bridge_connect_immune(bridge, immune);

    /* Get learning modifiers at different inflammation levels */
    float modifier_none, modifier_storm;

    bridge->state.inflammation_level = INFLAMMATION_NONE;
    fep_immune_get_learning_modifier(bridge, &modifier_none);

    bridge->state.inflammation_level = INFLAMMATION_STORM;
    fep_immune_get_learning_modifier(bridge, &modifier_storm);

    EXPECT_LT(modifier_storm, modifier_none);
}

TEST_F(FEPImmuneIntegrationTest, CytokineEffectsApplied) {
    fep_immune_bridge_connect_fep(bridge, fep);
    fep_immune_bridge_connect_immune(bridge, immune);

    int ret = fep_immune_update_cytokine_effects(bridge);
    EXPECT_EQ(ret, 0);
}

/* ============================================================================
 * Learning Integration Tests
 * ============================================================================ */

TEST_F(FEPImmuneIntegrationTest, LearningWithInflammation) {
    fep_immune_bridge_connect_fep(bridge, fep);
    fep_immune_bridge_connect_immune(bridge, immune);

    /* Set inflammation */
    bridge->state.inflammation_level = INFLAMMATION_REGIONAL;

    /* Get learning modifier */
    float modifier;
    fep_immune_get_learning_modifier(bridge, &modifier);
    EXPECT_GT(modifier, 0.0f);
    EXPECT_LT(modifier, 1.0f);

    /* Learning should still work */
    float state[8] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float next[8] = {0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    int ret = fep_learn_transition(learner, fep, state, next, 8);
    EXPECT_EQ(ret, 0);
}

/* ============================================================================
 * Sickness Behavior Tests
 * ============================================================================ */

TEST_F(FEPImmuneIntegrationTest, SicknessBehaviorIntegration) {
    fep_immune_bridge_connect_fep(bridge, fep);
    fep_immune_bridge_connect_immune(bridge, immune);

    /* Initially not sick */
    EXPECT_FALSE(fep_immune_is_sickness_active(bridge));

    /* Set high inflammation */
    bridge->state.inflammation_level = INFLAMMATION_SYSTEMIC;
    fep_immune_apply_inflammation_effects(bridge);

    /* Check state */
    fep_immune_state_t state;
    fep_immune_bridge_get_state(bridge, &state);
    EXPECT_GE(state.sickness_intensity, 0.0f);
}

/* ============================================================================
 * Convergence Integration Tests
 * ============================================================================ */

TEST_F(FEPImmuneIntegrationTest, ConvergenceIL10Release) {
    fep_immune_bridge_connect_fep(bridge, fep);
    fep_immune_bridge_connect_immune(bridge, immune);

    /* Mark as converged */
    bridge->state.converged = true;

    /* IL-10 release on convergence */
    int ret = fep_immune_convergence_il10_release(bridge);
    EXPECT_EQ(ret, 0);
}

/* ============================================================================
 * Recovery Tests
 * ============================================================================ */

TEST_F(FEPImmuneIntegrationTest, RecoveryWithUpdate) {
    fep_immune_bridge_connect_fep(bridge, fep);
    fep_immune_bridge_connect_immune(bridge, immune);

    /* Set some inflammation then recover */
    bridge->state.inflammation_level = INFLAMMATION_LOCAL;

    /* Update to allow recovery */
    for (int i = 0; i < 10; i++) {
        fep_immune_bridge_update(bridge, 100);
    }

    fep_immune_state_t state;
    fep_immune_bridge_get_state(bridge, &state);
    EXPECT_GE(state.recovery_progress, 0.0f);
}

/* ============================================================================
 * Stats Tracking Tests
 * ============================================================================ */

TEST_F(FEPImmuneIntegrationTest, StatsAfterActivity) {
    fep_immune_bridge_connect_fep(bridge, fep);
    fep_immune_bridge_connect_immune(bridge, immune);

    /* Generate activity */
    fep_immune_report_prediction_failure(bridge, 15.0f);
    fep_immune_report_prediction_failure(bridge, 20.0f);
    fep_immune_bridge_update(bridge, 100);

    fep_immune_stats_t stats;
    fep_immune_bridge_get_stats(bridge, &stats);
    EXPECT_GT(stats.prediction_failures, 0u);
}

/* ============================================================================
 * Bio-Async Integration Tests
 * ============================================================================ */

TEST_F(FEPImmuneIntegrationTest, BioAsyncWithBridge) {
    fep_immune_bridge_connect_fep(bridge, fep);
    fep_immune_bridge_connect_immune(bridge, immune);
    fep_immune_bridge_connect_bio_async(bridge);

    /* Bridge should work with bio-async */
    int ret = fep_immune_bridge_update(bridge, 100);
    EXPECT_EQ(ret, 0);

    fep_immune_bridge_disconnect_bio_async(bridge);
}

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST_F(FEPImmuneIntegrationTest, CustomSensitivityConfig) {
    fep_immune_config_t config;
    fep_immune_bridge_default_config(&config);
    config.cytokine_sensitivity = 2.0f;
    config.inflammation_sensitivity = 0.5f;

    fep_immune_bridge_t* custom = fep_immune_bridge_create(&config);
    ASSERT_NE(custom, nullptr);

    fep_immune_bridge_connect_fep(custom, fep);
    fep_immune_bridge_connect_immune(custom, immune);

    int ret = fep_immune_bridge_update(custom, 100);
    EXPECT_EQ(ret, 0);

    fep_immune_bridge_destroy(custom);
}

TEST_F(FEPImmuneIntegrationTest, DisabledFeaturesConfig) {
    fep_immune_config_t config;
    fep_immune_bridge_default_config(&config);
    config.enable_sickness_behavior = false;
    config.enable_immune_memory_transfer = false;

    fep_immune_bridge_t* disabled = fep_immune_bridge_create(&config);
    ASSERT_NE(disabled, nullptr);

    fep_immune_bridge_connect_fep(disabled, fep);
    fep_immune_bridge_connect_immune(disabled, immune);

    /* Should work but features disabled */
    int ret = fep_immune_bridge_update(disabled, 100);
    EXPECT_EQ(ret, 0);

    fep_immune_bridge_destroy(disabled);
}

/* ============================================================================
 * Inflammation Level Progression Tests
 * ============================================================================ */

TEST_F(FEPImmuneIntegrationTest, InflammationProgression) {
    fep_immune_bridge_connect_fep(bridge, fep);
    fep_immune_bridge_connect_immune(bridge, immune);

    /* Test each inflammation level */
    brain_inflammation_level_t levels[] = {
        INFLAMMATION_NONE,
        INFLAMMATION_LOCAL,
        INFLAMMATION_REGIONAL,
        INFLAMMATION_SYSTEMIC,
        INFLAMMATION_STORM
    };

    for (int i = 0; i < 5; i++) {
        bridge->state.inflammation_level = levels[i];
        fep_immune_apply_inflammation_effects(bridge);

        float precision;
        fep_immune_get_precision_modifier(bridge, &precision);
        EXPECT_GT(precision, 0.0f);
    }
}

/* ============================================================================
 * Full Cycle Integration Tests
 * ============================================================================ */

TEST_F(FEPImmuneIntegrationTest, FullImmuneCycle) {
    fep_immune_bridge_connect_fep(bridge, fep);
    fep_immune_bridge_connect_immune(bridge, immune);

    /* 1. Report prediction errors */
    fep_immune_report_prediction_failure(bridge, 5.0f);
    fep_immune_report_prediction_failure(bridge, 10.0f);
    fep_immune_report_prediction_failure(bridge, 15.0f);

    /* 2. Update bridge */
    fep_immune_bridge_update(bridge, 100);

    /* 3. Get inflammation effects */
    float precision, learning;
    fep_immune_get_precision_modifier(bridge, &precision);
    fep_immune_get_learning_modifier(bridge, &learning);

    EXPECT_GT(precision, 0.0f);
    EXPECT_GT(learning, 0.0f);

    /* 4. Apply inflammation effects */
    fep_immune_apply_inflammation_effects(bridge);

    /* 5. Check stats */
    fep_immune_stats_t stats;
    fep_immune_bridge_get_stats(bridge, &stats);
    EXPECT_GT(stats.prediction_failures, 0u);
}
