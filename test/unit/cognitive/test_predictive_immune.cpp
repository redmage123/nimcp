/**
 * @file test_predictive_immune.cpp
 * @brief Unit tests for Predictive-Immune Integration module
 *
 * Tests bidirectional coupling between predictive coding and brain immune system:
 * - Interoceptive prediction of immune state
 * - Immune modulation of prediction precision
 * - Prediction errors triggering immune responses
 */

#include <gtest/gtest.h>
#include "utils/nimcp_test_base.h"

// Headers have their own extern "C" guards
#include "cognitive/nimcp_predictive_immune.h"

/**
 * @brief Test fixture for Predictive-Immune Integration tests
 */
class PredictiveImmuneTest : public NimcpTestBase {
protected:
    predictive_immune_system_t* pi_system;
    predictive_immune_config_t config;

    void SetUp() override {
        NimcpTestBase::SetUp();
        pi_system = nullptr;
        predictive_immune_default_config(&config);
    }

    void TearDown() override {
        if (pi_system) {
            predictive_immune_destroy(pi_system);
            pi_system = nullptr;
        }
        NimcpTestBase::TearDown();
    }
};

// ============================================================================
// Default Configuration Tests
// ============================================================================

TEST_F(PredictiveImmuneTest, DefaultConfigReturnsSuccess) {
    predictive_immune_config_t cfg;
    nimcp_result_t result = predictive_immune_default_config(&cfg);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(PredictiveImmuneTest, DefaultConfigNullReturnsError) {
    nimcp_result_t result = predictive_immune_default_config(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(PredictiveImmuneTest, DefaultConfigHasValidInteroMode) {
    predictive_immune_config_t cfg;
    predictive_immune_default_config(&cfg);

    // Should have a reasonable default interoceptive mode
    EXPECT_GE(cfg.intero_mode, INTERO_PREDICT_NONE);
    EXPECT_LE(cfg.intero_mode, INTERO_PREDICT_FULL_STATE);
}

TEST_F(PredictiveImmuneTest, DefaultConfigHasValidPrecisionParameters) {
    predictive_immune_config_t cfg;
    predictive_immune_default_config(&cfg);

    // Precision reduction should be bounded
    EXPECT_GE(cfg.precision_reduction_factor, 0.0f);
    EXPECT_LE(cfg.max_precision_reduction, 1.0f);
}

TEST_F(PredictiveImmuneTest, DefaultConfigHasValidErrorThreshold) {
    predictive_immune_config_t cfg;
    predictive_immune_default_config(&cfg);

    // Error threshold should be positive
    EXPECT_GT(cfg.prediction_error_threshold, 0.0f);
    EXPECT_GT(cfg.free_energy_threshold, 0.0f);
}

// ============================================================================
// Enum Value Tests
// ============================================================================

TEST_F(PredictiveImmuneTest, InteroceptivePredictionModeEnumsAreDefined) {
    EXPECT_EQ(INTERO_PREDICT_NONE, 0);
    EXPECT_NE(INTERO_PREDICT_INFLAMMATION, INTERO_PREDICT_NONE);
    EXPECT_NE(INTERO_PREDICT_CYTOKINES, INTERO_PREDICT_INFLAMMATION);
    EXPECT_NE(INTERO_PREDICT_FULL_STATE, INTERO_PREDICT_CYTOKINES);
}

TEST_F(PredictiveImmuneTest, ImmuneModulationStrategyEnumsAreDefined) {
    EXPECT_EQ(IMMUNE_MOD_NONE, 0);
    EXPECT_NE(IMMUNE_MOD_PRECISION_ONLY, IMMUNE_MOD_NONE);
    EXPECT_NE(IMMUNE_MOD_LEARNING_RATE, IMMUNE_MOD_PRECISION_ONLY);
    EXPECT_NE(IMMUNE_MOD_FULL, IMMUNE_MOD_LEARNING_RATE);
}

TEST_F(PredictiveImmuneTest, PredictionErrorResponseModeEnumsAreDefined) {
    EXPECT_EQ(PRED_ERROR_RESPONSE_NONE, 0);
    EXPECT_NE(PRED_ERROR_RESPONSE_THRESHOLD, PRED_ERROR_RESPONSE_NONE);
    EXPECT_NE(PRED_ERROR_RESPONSE_CUMULATIVE, PRED_ERROR_RESPONSE_THRESHOLD);
    EXPECT_NE(PRED_ERROR_RESPONSE_ADAPTIVE, PRED_ERROR_RESPONSE_CUMULATIVE);
}

// ============================================================================
// System Lifecycle Tests
// ============================================================================

TEST_F(PredictiveImmuneTest, CreateWithNullDependenciesReturnsNull) {
    // Without actual predictive_network and immune_system, should return NULL
    pi_system = predictive_immune_create(&config, nullptr, nullptr);
    EXPECT_EQ(pi_system, nullptr);
}

TEST_F(PredictiveImmuneTest, DestroyNullSystemIsNoOp) {
    // Should not crash
    predictive_immune_destroy(nullptr);
    SUCCEED();
}

TEST_F(PredictiveImmuneTest, CreateWithNullConfigUsesDefaults) {
    // NULL config should use defaults but still needs valid dependencies
    pi_system = predictive_immune_create(nullptr, nullptr, nullptr);
    // Without valid deps, should still be NULL
    EXPECT_EQ(pi_system, nullptr);
}

// ============================================================================
// Start/Stop Lifecycle Tests
// ============================================================================

TEST_F(PredictiveImmuneTest, StartNullSystemReturnsError) {
    nimcp_result_t result = predictive_immune_start(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(PredictiveImmuneTest, StopNullSystemReturnsError) {
    nimcp_result_t result = predictive_immune_stop(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// ============================================================================
// Interoceptive State Tests
// ============================================================================

TEST_F(PredictiveImmuneTest, GetInteroceptiveStateNullSystemReturnsError) {
    interoceptive_state_t state;
    nimcp_result_t result = predictive_immune_get_interoceptive_state(nullptr, &state);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(PredictiveImmuneTest, GetInteroceptiveStateNullStateReturnsError) {
    // Cannot test with null pi_system since we can't create one without deps
    nimcp_result_t result = predictive_immune_get_interoceptive_state(nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(PredictiveImmuneTest, InteroceptiveStateStructureIsValid) {
    interoceptive_state_t state = {};

    // Initialize fields
    state.inflammation_level = 0.5f;
    state.predicted_inflammation = 0.4f;
    state.inflammation_error = 0.1f;

    EXPECT_FLOAT_EQ(state.inflammation_level, 0.5f);
    EXPECT_FLOAT_EQ(state.predicted_inflammation, 0.4f);
    EXPECT_FLOAT_EQ(state.inflammation_error, 0.1f);
}

// ============================================================================
// Immune Modulated Precision Tests
// ============================================================================

TEST_F(PredictiveImmuneTest, ImmuneModulatedPrecisionStructureIsValid) {
    immune_modulated_precision_t precision = {};

    precision.baseline_precision = PREDICTIVE_IMMUNE_BASELINE_PRECISION;
    precision.current_precision = 0.8f;
    precision.inflammation_factor = 0.15f;
    precision.cytokine_factor = 0.05f;
    precision.total_reduction = 0.2f;

    EXPECT_FLOAT_EQ(precision.baseline_precision, 1.0f);
    EXPECT_LT(precision.current_precision, precision.baseline_precision);
}

TEST_F(PredictiveImmuneTest, GetPrecisionModulationNullSystemReturnsError) {
    immune_modulated_precision_t precision;
    nimcp_result_t result = predictive_immune_get_precision_modulation(nullptr, nullptr, &precision);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// ============================================================================
// Prediction Error Trigger Tests
// ============================================================================

TEST_F(PredictiveImmuneTest, PredictionErrorTriggerStructureIsValid) {
    prediction_error_trigger_t trigger = {};

    trigger.current_error = 2.5f;
    trigger.error_threshold = PREDICTIVE_IMMUNE_ERROR_THRESHOLD;
    trigger.cumulative_error = 5.0f;
    trigger.error_spike_count = 3;
    trigger.triggered = false;

    EXPECT_FLOAT_EQ(trigger.error_threshold, 3.0f);
    EXPECT_LT(trigger.current_error, trigger.error_threshold);
    EXPECT_FALSE(trigger.triggered);
}

TEST_F(PredictiveImmuneTest, GetErrorTriggerStateNullSystemReturnsError) {
    prediction_error_trigger_t trigger;
    nimcp_result_t result = predictive_immune_get_error_trigger_state(nullptr, nullptr, &trigger);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// ============================================================================
// Update API Tests
// ============================================================================

TEST_F(PredictiveImmuneTest, UpdateNullSystemReturnsError) {
    nimcp_result_t result = predictive_immune_update(nullptr, 16.0f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(PredictiveImmuneTest, UpdateInteroceptionNullSystemReturnsError) {
    nimcp_result_t result = predictive_immune_update_interoception(nullptr, 16.0f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// ============================================================================
// Immune Modulation API Tests
// ============================================================================

TEST_F(PredictiveImmuneTest, ApplyImmuneModulationNullSystemReturnsError) {
    nimcp_result_t result = predictive_immune_apply_immune_modulation(nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(PredictiveImmuneTest, ComputeCytokinePrecisionEffectNullSystemReturnsError) {
    float cytokines[BRAIN_CYTOKINE_COUNT] = {};
    float precision_out = 0.0f;
    nimcp_result_t result = predictive_immune_compute_cytokine_precision_effect(
        nullptr, cytokines, &precision_out
    );
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// ============================================================================
// Prediction Error Detection API Tests
// ============================================================================

TEST_F(PredictiveImmuneTest, MonitorPredictionErrorsNullSystemReturnsError) {
    nimcp_result_t result = predictive_immune_monitor_prediction_errors(nullptr, nullptr, 16.0f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(PredictiveImmuneTest, TriggerErrorResponseNullSystemReturnsError) {
    uint32_t antigen_id = 0;
    nimcp_result_t result = predictive_immune_trigger_error_response(
        nullptr, nullptr, 5.0f, &antigen_id
    );
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_F(PredictiveImmuneTest, GetStatsNullSystemReturnsError) {
    predictive_immune_stats_t stats;
    nimcp_result_t result = predictive_immune_get_stats(nullptr, &stats);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(PredictiveImmuneTest, GetStatsNullStatsReturnsError) {
    nimcp_result_t result = predictive_immune_get_stats(nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(PredictiveImmuneTest, StatsStructureIsValid) {
    predictive_immune_stats_t stats = {};

    // Initialize fields
    stats.interoceptive_updates = 100;
    stats.avg_interoceptive_error = 0.15f;
    stats.max_interoceptive_error = 0.8f;
    stats.sickness_behavior_triggers = 2;
    stats.avg_precision_reduction = 0.1f;
    stats.max_precision_reduction = 0.3f;
    stats.modulation_events = 50;
    stats.immune_triggers = 5;
    stats.false_positives = 1;
    stats.trigger_accuracy = 0.8f;

    EXPECT_EQ(stats.interoceptive_updates, 100UL);
    EXPECT_GT(stats.trigger_accuracy, 0.0f);
    EXPECT_LE(stats.trigger_accuracy, 1.0f);
}

// ============================================================================
// Sickness Behavior Tests
// ============================================================================

TEST_F(PredictiveImmuneTest, TriggerSicknessBehaviorNullSystemReturnsError) {
    nimcp_result_t result = predictive_immune_trigger_sickness_behavior(nullptr, 0.5f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// ============================================================================
// Reset Tests
// ============================================================================

TEST_F(PredictiveImmuneTest, ResetNullSystemReturnsError) {
    nimcp_result_t result = predictive_immune_reset(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// ============================================================================
// Region Connection Tests
// ============================================================================

TEST_F(PredictiveImmuneTest, ConnectRegionNullSystemReturnsError) {
    nimcp_result_t result = predictive_immune_connect_region(nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(PredictiveImmuneTest, DisconnectRegionNullSystemReturnsError) {
    nimcp_result_t result = predictive_immune_disconnect_region(nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// ============================================================================
// Constants Tests
// ============================================================================

TEST_F(PredictiveImmuneTest, ConstantsHaveReasonableValues) {
    // Max interoceptive dims should be reasonable
    EXPECT_GT(PREDICTIVE_IMMUNE_MAX_INTEROCEPTIVE_DIMS, 0U);
    EXPECT_LE(PREDICTIVE_IMMUNE_MAX_INTEROCEPTIVE_DIMS, 1024U);

    // Baseline precision should be 1.0
    EXPECT_FLOAT_EQ(PREDICTIVE_IMMUNE_BASELINE_PRECISION, 1.0f);

    // Min precision during inflammation should be small but positive
    EXPECT_GT(PREDICTIVE_IMMUNE_MIN_PRECISION, 0.0f);
    EXPECT_LT(PREDICTIVE_IMMUNE_MIN_PRECISION, PREDICTIVE_IMMUNE_BASELINE_PRECISION);

    // Error threshold should be positive
    EXPECT_GT(PREDICTIVE_IMMUNE_ERROR_THRESHOLD, 0.0f);

    // Free energy threshold should be positive
    EXPECT_GT(PREDICTIVE_IMMUNE_FREE_ENERGY_THRESHOLD, 0.0f);
}

// ============================================================================
// Configuration Validation Tests
// ============================================================================

TEST_F(PredictiveImmuneTest, ConfigurationModesAreConfigurable) {
    predictive_immune_config_t cfg;
    predictive_immune_default_config(&cfg);

    // Test interoceptive prediction modes
    cfg.intero_mode = INTERO_PREDICT_FULL_STATE;
    EXPECT_EQ(cfg.intero_mode, INTERO_PREDICT_FULL_STATE);

    cfg.intero_mode = INTERO_PREDICT_INFLAMMATION;
    EXPECT_EQ(cfg.intero_mode, INTERO_PREDICT_INFLAMMATION);

    // Test modulation strategies
    cfg.modulation_strategy = IMMUNE_MOD_FULL;
    EXPECT_EQ(cfg.modulation_strategy, IMMUNE_MOD_FULL);

    // Test error response modes
    cfg.error_response_mode = PRED_ERROR_RESPONSE_ADAPTIVE;
    EXPECT_EQ(cfg.error_response_mode, PRED_ERROR_RESPONSE_ADAPTIVE);
}

TEST_F(PredictiveImmuneTest, ConfigCytokineEffectsAreConfigurable) {
    predictive_immune_config_t cfg;
    predictive_immune_default_config(&cfg);

    // Set custom cytokine effects
    cfg.il1_precision_effect = 0.2f;
    cfg.il6_precision_effect = 0.15f;
    cfg.tnf_precision_effect = 0.25f;
    cfg.il10_recovery_boost = 0.1f;

    EXPECT_FLOAT_EQ(cfg.il1_precision_effect, 0.2f);
    EXPECT_FLOAT_EQ(cfg.il6_precision_effect, 0.15f);
    EXPECT_FLOAT_EQ(cfg.tnf_precision_effect, 0.25f);
    EXPECT_FLOAT_EQ(cfg.il10_recovery_boost, 0.1f);
}

TEST_F(PredictiveImmuneTest, ConfigBioAsyncIsConfigurable) {
    predictive_immune_config_t cfg;
    predictive_immune_default_config(&cfg);

    cfg.enable_bio_async = true;
    cfg.broadcast_intero_predictions = true;

    EXPECT_TRUE(cfg.enable_bio_async);
    EXPECT_TRUE(cfg.broadcast_intero_predictions);

    cfg.enable_bio_async = false;
    EXPECT_FALSE(cfg.enable_bio_async);
}
