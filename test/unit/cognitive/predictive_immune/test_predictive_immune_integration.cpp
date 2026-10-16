/**
 * @file test_predictive_immune_integration.cpp
 * @brief Unit tests for predictive-immune integration
 * @version 1.0.0
 * @date 2025-12-11
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "cognitive/nimcp_predictive_immune.h"
#include "cognitive/nimcp_predictive.h"
#include "cognitive/immune/nimcp_brain_immune.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class PredictiveImmuneTest : public ::testing::Test {
protected:
    predictive_network_t pred_net;
    brain_immune_system_t* immune_sys;
    predictive_immune_system_t* integration;
    predictive_immune_config_t config;

    void SetUp() override {
        /* Create predictive network */
        predictive_config_t pred_cfg = predictive_default_config();
        pred_net = predictive_create(&pred_cfg);
        ASSERT_NE(pred_net, nullptr);

        /* Create immune system */
        brain_immune_config_t immune_cfg;
        brain_immune_default_config(&immune_cfg);
        immune_sys = brain_immune_create(&immune_cfg);
        ASSERT_NE(immune_sys, nullptr);
        brain_immune_start(immune_sys);

        /* Get default config */
        predictive_immune_default_config(&config);

        integration = nullptr;
    }

    void TearDown() override {
        if (integration) {
            predictive_immune_destroy(integration);
        }
        if (immune_sys) {
            brain_immune_stop(immune_sys);
            brain_immune_destroy(immune_sys);
        }
        if (pred_net) {
            predictive_destroy(pred_net);
        }
    }
};

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST_F(PredictiveImmuneTest, DefaultConfiguration) {
    predictive_immune_config_t cfg;
    ASSERT_EQ(predictive_immune_default_config(&cfg), NIMCP_SUCCESS);

    EXPECT_EQ(cfg.intero_mode, INTERO_PREDICT_INFLAMMATION);
    EXPECT_EQ(cfg.modulation_strategy, IMMUNE_MOD_PRECISION_ONLY);
    EXPECT_EQ(cfg.error_response_mode, PRED_ERROR_RESPONSE_THRESHOLD);
    EXPECT_GT(cfg.precision_reduction_factor, 0.0f);
    EXPECT_GT(cfg.prediction_error_threshold, 0.0f);
}

TEST_F(PredictiveImmuneTest, DefaultConfigNullPointer) {
    EXPECT_EQ(predictive_immune_default_config(nullptr), NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(PredictiveImmuneTest, CreateDestroy) {
    integration = predictive_immune_create(&config, pred_net, immune_sys);
    ASSERT_NE(integration, nullptr);

    /* Destroy is tested in TearDown */
}

TEST_F(PredictiveImmuneTest, CreateWithNullConfig) {
    integration = predictive_immune_create(nullptr, pred_net, immune_sys);
    EXPECT_NE(integration, nullptr);
}

TEST_F(PredictiveImmuneTest, CreateWithNullPredictive) {
    integration = predictive_immune_create(&config, nullptr, immune_sys);
    EXPECT_EQ(integration, nullptr);
}

TEST_F(PredictiveImmuneTest, CreateWithNullImmune) {
    integration = predictive_immune_create(&config, pred_net, nullptr);
    EXPECT_EQ(integration, nullptr);
}

TEST_F(PredictiveImmuneTest, StartStop) {
    integration = predictive_immune_create(&config, pred_net, immune_sys);
    ASSERT_NE(integration, nullptr);

    EXPECT_EQ(predictive_immune_start(integration), NIMCP_SUCCESS);
    EXPECT_EQ(predictive_immune_stop(integration), NIMCP_SUCCESS);
}

TEST_F(PredictiveImmuneTest, StartStopNullPointer) {
    EXPECT_EQ(predictive_immune_start(nullptr), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(predictive_immune_stop(nullptr), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PredictiveImmuneTest, DoubleStart) {
    integration = predictive_immune_create(&config, pred_net, immune_sys);
    ASSERT_NE(integration, nullptr);

    EXPECT_EQ(predictive_immune_start(integration), NIMCP_SUCCESS);
    EXPECT_EQ(predictive_immune_start(integration), NIMCP_SUCCESS); /* Should be idempotent */
}

/* ============================================================================
 * Interoceptive Prediction Tests
 * ============================================================================ */

TEST_F(PredictiveImmuneTest, UpdateInteroceptionNone) {
    config.intero_mode = INTERO_PREDICT_NONE;
    integration = predictive_immune_create(&config, pred_net, immune_sys);
    ASSERT_NE(integration, nullptr);
    predictive_immune_start(integration);

    EXPECT_EQ(predictive_immune_update_interoception(integration, 10.0f), NIMCP_SUCCESS);
}

TEST_F(PredictiveImmuneTest, UpdateInteroceptionInflammation) {
    config.intero_mode = INTERO_PREDICT_INFLAMMATION;
    integration = predictive_immune_create(&config, pred_net, immune_sys);
    ASSERT_NE(integration, nullptr);
    predictive_immune_start(integration);

    /* Simulate inflammation by creating antigens */
    uint32_t antigen_id;
    uint8_t epitope[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    brain_immune_present_antigen(immune_sys, ANTIGEN_SOURCE_MANUAL,
                                   epitope, 8, 5, 0, &antigen_id);

    /* Update interoception */
    EXPECT_EQ(predictive_immune_update_interoception(integration, 10.0f), NIMCP_SUCCESS);

    /* Check state */
    interoceptive_state_t state;
    EXPECT_EQ(predictive_immune_get_interoceptive_state(integration, &state), NIMCP_SUCCESS);
    EXPECT_GE(state.inflammation_level, 0.0f);
    EXPECT_LE(state.inflammation_level, 1.0f);
}

TEST_F(PredictiveImmuneTest, GetInteroceptiveState) {
    integration = predictive_immune_create(&config, pred_net, immune_sys);
    ASSERT_NE(integration, nullptr);
    predictive_immune_start(integration);

    interoceptive_state_t state;
    EXPECT_EQ(predictive_immune_get_interoceptive_state(integration, &state), NIMCP_SUCCESS);

    /* Initially should be zero */
    EXPECT_EQ(state.inflammation_level, 0.0f);
    EXPECT_EQ(state.immune_activation, 0.0f);
}

TEST_F(PredictiveImmuneTest, GetInteroceptiveStateNullPointer) {
    integration = predictive_immune_create(&config, pred_net, immune_sys);
    ASSERT_NE(integration, nullptr);

    EXPECT_EQ(predictive_immune_get_interoceptive_state(nullptr, nullptr), NIMCP_ERROR_NULL_POINTER);

    interoceptive_state_t state;
    EXPECT_EQ(predictive_immune_get_interoceptive_state(integration, nullptr), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(predictive_immune_get_interoceptive_state(nullptr, &state), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PredictiveImmuneTest, TriggerSicknessBehavior) {
    integration = predictive_immune_create(&config, pred_net, immune_sys);
    ASSERT_NE(integration, nullptr);
    predictive_immune_start(integration);

    EXPECT_EQ(predictive_immune_trigger_sickness_behavior(integration, 0.7f), NIMCP_SUCCESS);

    /* Check statistics */
    predictive_immune_stats_t stats;
    EXPECT_EQ(predictive_immune_get_stats(integration, &stats), NIMCP_SUCCESS);
    EXPECT_EQ(stats.sickness_behavior_triggers, 1);
}

/* ============================================================================
 * Immune Modulation Tests
 * ============================================================================ */

TEST_F(PredictiveImmuneTest, ApplyImmuneModulationNone) {
    config.modulation_strategy = IMMUNE_MOD_NONE;
    integration = predictive_immune_create(&config, pred_net, immune_sys);
    ASSERT_NE(integration, nullptr);
    predictive_immune_start(integration);

    EXPECT_EQ(predictive_immune_apply_immune_modulation(integration, nullptr), NIMCP_SUCCESS);
}

TEST_F(PredictiveImmuneTest, ApplyImmuneModulationPrecisionOnly) {
    config.modulation_strategy = IMMUNE_MOD_PRECISION_ONLY;
    integration = predictive_immune_create(&config, pred_net, immune_sys);
    ASSERT_NE(integration, nullptr);
    predictive_immune_start(integration);

    /* Create inflammation */
    uint32_t antigen_id;
    uint8_t epitope[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    brain_immune_present_antigen(immune_sys, ANTIGEN_SOURCE_MANUAL,
                                   epitope, 8, 8, 0, &antigen_id);

    /* Update interoception to capture inflammation */
    predictive_immune_update_interoception(integration, 10.0f);

    /* Apply modulation */
    EXPECT_EQ(predictive_immune_apply_immune_modulation(integration, nullptr), NIMCP_SUCCESS);

    /* Check that modulation occurred */
    predictive_immune_stats_t stats;
    EXPECT_EQ(predictive_immune_get_stats(integration, &stats), NIMCP_SUCCESS);
    EXPECT_GT(stats.modulation_events, 0);
}

TEST_F(PredictiveImmuneTest, ComputeCytokineEffect) {
    integration = predictive_immune_create(&config, pred_net, immune_sys);
    ASSERT_NE(integration, nullptr);

    float cytokine_levels[BRAIN_CYTOKINE_COUNT] = {0};
    cytokine_levels[BRAIN_CYTOKINE_IL1] = 0.5f;
    cytokine_levels[BRAIN_CYTOKINE_IL6] = 0.3f;

    float precision;
    EXPECT_EQ(predictive_immune_compute_cytokine_precision_effect(
        integration, cytokine_levels, &precision), NIMCP_SUCCESS);

    /* Precision should be reduced from baseline */
    EXPECT_LT(precision, PREDICTIVE_IMMUNE_BASELINE_PRECISION);
    EXPECT_GE(precision, PREDICTIVE_IMMUNE_MIN_PRECISION);
}

TEST_F(PredictiveImmuneTest, ComputeCytokineEffectIL10Recovery) {
    integration = predictive_immune_create(&config, pred_net, immune_sys);
    ASSERT_NE(integration, nullptr);

    float cytokine_levels[BRAIN_CYTOKINE_COUNT] = {0};
    cytokine_levels[BRAIN_CYTOKINE_IL1] = 0.3f;
    cytokine_levels[BRAIN_CYTOKINE_IL10] = 0.5f; /* Anti-inflammatory */

    float precision;
    EXPECT_EQ(predictive_immune_compute_cytokine_precision_effect(
        integration, cytokine_levels, &precision), NIMCP_SUCCESS);

    /* IL-10 should partially counteract IL-1 */
    EXPECT_GT(precision, PREDICTIVE_IMMUNE_MIN_PRECISION);
}

TEST_F(PredictiveImmuneTest, GetPrecisionModulation) {
    integration = predictive_immune_create(&config, pred_net, immune_sys);
    ASSERT_NE(integration, nullptr);

    immune_modulated_precision_t prec_state;
    EXPECT_EQ(predictive_immune_get_precision_modulation(
        integration, nullptr, &prec_state), NIMCP_SUCCESS);

    EXPECT_EQ(prec_state.baseline_precision, PREDICTIVE_IMMUNE_BASELINE_PRECISION);
    EXPECT_EQ(prec_state.current_precision, PREDICTIVE_IMMUNE_BASELINE_PRECISION);
}

/* ============================================================================
 * Prediction Error Detection Tests
 * ============================================================================ */

TEST_F(PredictiveImmuneTest, TriggerErrorResponseThreshold) {
    config.error_response_mode = PRED_ERROR_RESPONSE_THRESHOLD;
    integration = predictive_immune_create(&config, pred_net, immune_sys);
    ASSERT_NE(integration, nullptr);
    predictive_immune_start(integration);

    float large_error = PREDICTIVE_IMMUNE_ERROR_THRESHOLD * 2.0f;
    uint32_t antigen_id;

    EXPECT_EQ(predictive_immune_trigger_error_response(
        integration, nullptr, large_error, &antigen_id), NIMCP_SUCCESS);
    EXPECT_GT(antigen_id, 0);

    /* Check that immune trigger was recorded */
    predictive_immune_stats_t stats;
    EXPECT_EQ(predictive_immune_get_stats(integration, &stats), NIMCP_SUCCESS);
    EXPECT_GT(stats.immune_triggers, 0);
}

TEST_F(PredictiveImmuneTest, TriggerErrorResponseNone) {
    config.error_response_mode = PRED_ERROR_RESPONSE_NONE;
    integration = predictive_immune_create(&config, pred_net, immune_sys);
    ASSERT_NE(integration, nullptr);
    predictive_immune_start(integration);

    /* Even with large error, should not trigger with mode NONE */
    float large_error = PREDICTIVE_IMMUNE_ERROR_THRESHOLD * 2.0f;
    uint32_t antigen_id;

    EXPECT_EQ(predictive_immune_trigger_error_response(
        integration, nullptr, large_error, &antigen_id), NIMCP_SUCCESS);

    /* Should still create antigen even in NONE mode when explicitly triggered */
    EXPECT_GT(antigen_id, 0);
}

TEST_F(PredictiveImmuneTest, GetErrorTriggerState) {
    integration = predictive_immune_create(&config, pred_net, immune_sys);
    ASSERT_NE(integration, nullptr);

    prediction_error_trigger_t trigger_state;
    EXPECT_EQ(predictive_immune_get_error_trigger_state(
        integration, nullptr, &trigger_state), NIMCP_SUCCESS);

    EXPECT_EQ(trigger_state.error_threshold, config.prediction_error_threshold);
    EXPECT_EQ(trigger_state.triggered, false);
}

/* ============================================================================
 * Integration Update Tests
 * ============================================================================ */

TEST_F(PredictiveImmuneTest, UpdateFullCycle) {
    integration = predictive_immune_create(&config, pred_net, immune_sys);
    ASSERT_NE(integration, nullptr);
    predictive_immune_start(integration);

    /* Run several update cycles */
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(predictive_immune_update(integration, 10.0f), NIMCP_SUCCESS);
    }

    /* Check statistics */
    predictive_immune_stats_t stats;
    EXPECT_EQ(predictive_immune_get_stats(integration, &stats), NIMCP_SUCCESS);
    EXPECT_GT(stats.interoceptive_updates, 0);
}

TEST_F(PredictiveImmuneTest, UpdateNotRunning) {
    integration = predictive_immune_create(&config, pred_net, immune_sys);
    ASSERT_NE(integration, nullptr);

    /* Update without starting */
    EXPECT_EQ(predictive_immune_update(integration, 10.0f), NIMCP_ERROR_INVALID_STATE);
}

TEST_F(PredictiveImmuneTest, GetStats) {
    integration = predictive_immune_create(&config, pred_net, immune_sys);
    ASSERT_NE(integration, nullptr);
    predictive_immune_start(integration);

    predictive_immune_stats_t stats;
    EXPECT_EQ(predictive_immune_get_stats(integration, &stats), NIMCP_SUCCESS);

    /* Initially zero */
    EXPECT_EQ(stats.interoceptive_updates, 0);
    EXPECT_EQ(stats.immune_triggers, 0);
    EXPECT_EQ(stats.sickness_behavior_triggers, 0);
}

TEST_F(PredictiveImmuneTest, Reset) {
    integration = predictive_immune_create(&config, pred_net, immune_sys);
    ASSERT_NE(integration, nullptr);
    predictive_immune_start(integration);

    /* Update to generate state */
    predictive_immune_update(integration, 10.0f);

    /* Reset */
    EXPECT_EQ(predictive_immune_reset(integration), NIMCP_SUCCESS);

    /* State should be cleared */
    interoceptive_state_t state;
    EXPECT_EQ(predictive_immune_get_interoceptive_state(integration, &state), NIMCP_SUCCESS);
    EXPECT_EQ(state.total_interoceptive_error, 0.0f);
}

/* ============================================================================
 * Region-Specific Integration Tests
 * ============================================================================ */

TEST_F(PredictiveImmuneTest, ConnectRegion) {
    integration = predictive_immune_create(&config, pred_net, immune_sys);
    ASSERT_NE(integration, nullptr);

    /* Connect region (pass nullptr as we don't have actual region) */
    EXPECT_EQ(predictive_immune_connect_region(integration, nullptr), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PredictiveImmuneTest, DisconnectRegion) {
    integration = predictive_immune_create(&config, pred_net, immune_sys);
    ASSERT_NE(integration, nullptr);

    /* Disconnect region (pass nullptr as we don't have actual region) */
    EXPECT_EQ(predictive_immune_disconnect_region(integration, nullptr), NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Edge Case Tests
 * ============================================================================ */

TEST_F(PredictiveImmuneTest, ExtremeCytokineConcentrations) {
    integration = predictive_immune_create(&config, pred_net, immune_sys);
    ASSERT_NE(integration, nullptr);

    /* Max cytokines */
    float cytokine_levels[BRAIN_CYTOKINE_COUNT];
    for (int i = 0; i < BRAIN_CYTOKINE_COUNT; i++) {
        cytokine_levels[i] = 1.0f;
    }

    float precision;
    EXPECT_EQ(predictive_immune_compute_cytokine_precision_effect(
        integration, cytokine_levels, &precision), NIMCP_SUCCESS);

    /* Should clamp to minimum */
    EXPECT_GE(precision, PREDICTIVE_IMMUNE_MIN_PRECISION);
}

TEST_F(PredictiveImmuneTest, ZeroInflammation) {
    integration = predictive_immune_create(&config, pred_net, immune_sys);
    ASSERT_NE(integration, nullptr);
    predictive_immune_start(integration);

    /* No inflammation - precision should remain at baseline */
    predictive_immune_update(integration, 10.0f);

    immune_modulated_precision_t prec_state;
    EXPECT_EQ(predictive_immune_get_precision_modulation(
        integration, nullptr, &prec_state), NIMCP_SUCCESS);

    EXPECT_NEAR(prec_state.current_precision, prec_state.baseline_precision, 0.01f);
}

TEST_F(PredictiveImmuneTest, HighInflammation) {
    integration = predictive_immune_create(&config, pred_net, immune_sys);
    ASSERT_NE(integration, nullptr);
    predictive_immune_start(integration);

    /* Create multiple high-severity antigens */
    for (int i = 0; i < 5; i++) {
        uint32_t antigen_id;
        uint8_t epitope[8] = {(uint8_t)i, 2, 3, 4, 5, 6, 7, 8};
        brain_immune_present_antigen(immune_sys, ANTIGEN_SOURCE_MANUAL,
                                       epitope, 8, 9, 0, &antigen_id);
    }

    /* Update to capture inflammation */
    predictive_immune_update(integration, 10.0f);

    /* Precision should be significantly reduced */
    immune_modulated_precision_t prec_state;
    predictive_immune_get_precision_modulation(integration, nullptr, &prec_state);
    EXPECT_LT(prec_state.current_precision, prec_state.baseline_precision);
}

/* ============================================================================
 * Biological Plausibility Tests
 * ============================================================================ */

TEST_F(PredictiveImmuneTest, InteroceptivePredictionError) {
    config.intero_mode = INTERO_PREDICT_INFLAMMATION;
    integration = predictive_immune_create(&config, pred_net, immune_sys);
    ASSERT_NE(integration, nullptr);
    predictive_immune_start(integration);

    /* Run prediction cycles */
    for (int i = 0; i < 5; i++) {
        predictive_immune_update_interoception(integration, 10.0f);
    }

    /* Check that prediction errors are being computed */
    interoceptive_state_t state;
    predictive_immune_get_interoceptive_state(integration, &state);
    EXPECT_GE(state.total_interoceptive_error, 0.0f);
}

TEST_F(PredictiveImmuneTest, CytokineStormEffect) {
    integration = predictive_immune_create(&config, pred_net, immune_sys);
    ASSERT_NE(integration, nullptr);

    /* Simulate cytokine storm */
    float storm_levels[BRAIN_CYTOKINE_COUNT];
    storm_levels[BRAIN_CYTOKINE_IL1] = 0.9f;
    storm_levels[BRAIN_CYTOKINE_IL6] = 0.9f;
    storm_levels[CYTOKINE_TNF_ALPHA] = 0.9f;
    storm_levels[CYTOKINE_IFN_GAMMA] = 0.8f;
    storm_levels[BRAIN_CYTOKINE_IL10] = 0.1f; /* Low anti-inflammatory */

    float precision;
    predictive_immune_compute_cytokine_precision_effect(
        integration, storm_levels, &precision);

    /* Precision should be severely reduced */
    EXPECT_LT(precision, PREDICTIVE_IMMUNE_BASELINE_PRECISION * 0.5f);
}

TEST_F(PredictiveImmuneTest, AntiInflammatoryRecovery) {
    integration = predictive_immune_create(&config, pred_net, immune_sys);
    ASSERT_NE(integration, nullptr);

    /* High anti-inflammatory cytokine */
    float recovery_levels[BRAIN_CYTOKINE_COUNT] = {0};
    recovery_levels[BRAIN_CYTOKINE_IL10] = 0.8f;

    float precision;
    predictive_immune_compute_cytokine_precision_effect(
        integration, recovery_levels, &precision);

    /* Precision should be boosted above baseline */
    EXPECT_GE(precision, PREDICTIVE_IMMUNE_BASELINE_PRECISION);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
