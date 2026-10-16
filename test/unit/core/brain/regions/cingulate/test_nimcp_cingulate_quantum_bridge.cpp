/**
 * @file test_nimcp_cingulate_quantum_bridge.cpp
 * @brief Comprehensive unit tests for nimcp_cingulate_quantum_bridge.c
 *
 * WHAT: Unit tests for the Cingulate Cortex quantum bridge
 * WHY:  Ensure correct quantum conflict resolution, error propagation, and superposition
 * HOW:  Use Google Test framework to test quantum operations.
 *
 * COVERAGE TARGET: 100%
 */

#include <gtest/gtest.h>
#include <string.h>
#include <stdlib.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/brain/regions/cingulate/nimcp_cingulate_adapter.h"
#include "core/brain/regions/cingulate/nimcp_cingulate_quantum_bridge.h"

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class CingulateQuantumBridgeTest : public ::testing::Test {
protected:
    cingulate_adapter_t* adapter;
    cingulate_quantum_bridge_t* bridge;
    cingulate_config_t adapter_config;
    cingulate_quantum_config_t quantum_config;

    void SetUp() override {
        adapter_config = cingulate_default_config();
        adapter = cingulate_create(&adapter_config);
        ASSERT_NE(nullptr, adapter) << "Failed to create cingulate adapter";

        quantum_config = cingulate_quantum_default_config();
        bridge = cingulate_quantum_bridge_create(adapter, &quantum_config);
        ASSERT_NE(nullptr, bridge) << "Failed to create quantum bridge";
    }

    void TearDown() override {
        cingulate_quantum_bridge_destroy(bridge);
        bridge = nullptr;
        cingulate_destroy(adapter);
        adapter = nullptr;
    }

    // Helper to create response options for encoding
    void create_test_options(cingulate_response_option_t* options, uint32_t count) {
        for (uint32_t i = 0; i < count; i++) {
            memset(&options[i], 0, sizeof(cingulate_response_option_t));
            options[i].option_id = i;
            options[i].activation = 0.5f + 0.1f * (float)i;
            options[i].evidence = 0.5f;
            options[i].prior_probability = 1.0f / (float)count;
        }
    }
};

/*=============================================================================
 * LIFECYCLE TESTS
 *===========================================================================*/

TEST_F(CingulateQuantumBridgeTest, DefaultConfigHasReasonableValues) {
    cingulate_quantum_config_t default_config = cingulate_quantum_default_config();

    EXPECT_EQ(default_config.max_qubits, CINGULATE_QUANTUM_DEFAULT_MAX_QUBITS);
    EXPECT_EQ(default_config.max_iterations, CINGULATE_QUANTUM_DEFAULT_MAX_ITERATIONS);
    EXPECT_FLOAT_EQ(default_config.min_confidence, CINGULATE_QUANTUM_DEFAULT_MIN_CONFIDENCE);
    EXPECT_TRUE(default_config.enable_error_propagation);
    EXPECT_TRUE(default_config.enable_classical_fallback);
}

TEST_F(CingulateQuantumBridgeTest, CreateWithNullConfigUsesDefaults) {
    cingulate_quantum_bridge_t* bridge_null = cingulate_quantum_bridge_create(adapter, NULL);
    ASSERT_NE(nullptr, bridge_null);

    cingulate_quantum_config_t retrieved;
    EXPECT_TRUE(cingulate_quantum_get_config(bridge_null, &retrieved));
    EXPECT_EQ(retrieved.max_qubits, CINGULATE_QUANTUM_DEFAULT_MAX_QUBITS);

    cingulate_quantum_bridge_destroy(bridge_null);
}

TEST_F(CingulateQuantumBridgeTest, CreateWithNullAdapterSucceeds) {
    cingulate_quantum_bridge_t* bridge_no_adapter = cingulate_quantum_bridge_create(NULL, &quantum_config);
    ASSERT_NE(nullptr, bridge_no_adapter);

    cingulate_quantum_bridge_destroy(bridge_no_adapter);
}

TEST_F(CingulateQuantumBridgeTest, DestroyNullDoesNotCrash) {
    cingulate_quantum_bridge_destroy(NULL);
    // Should not crash
}

TEST_F(CingulateQuantumBridgeTest, ResetClearsState) {
    // Encode some options first
    cingulate_response_option_t options[4];
    create_test_options(options, 4);
    cingulate_quantum_encode_options(bridge, options, 4);

    // Reset
    EXPECT_TRUE(cingulate_quantum_bridge_reset(bridge));

    // State should be cleared
    cingulate_quantum_state_t state;
    memset(&state, 0, sizeof(state));
    cingulate_quantum_get_state(bridge, &state);
    EXPECT_EQ(state.num_qubits, 0u);
}

TEST_F(CingulateQuantumBridgeTest, ResetNullReturnsFalse) {
    EXPECT_FALSE(cingulate_quantum_bridge_reset(NULL));
}

/*=============================================================================
 * QUANTUM ENCODING TESTS
 *===========================================================================*/

TEST_F(CingulateQuantumBridgeTest, EncodeOptionsSuccess) {
    cingulate_response_option_t options[4];
    create_test_options(options, 4);

    EXPECT_TRUE(cingulate_quantum_encode_options(bridge, options, 4));
}

TEST_F(CingulateQuantumBridgeTest, EncodeOptionsCreatesProperQubits) {
    cingulate_response_option_t options[4];
    create_test_options(options, 4);

    EXPECT_TRUE(cingulate_quantum_encode_options(bridge, options, 4));

    cingulate_quantum_state_t state;
    memset(&state, 0, sizeof(state));
    EXPECT_TRUE(cingulate_quantum_get_state(bridge, &state));

    // 4 options requires 2 qubits (2^2 = 4)
    EXPECT_EQ(state.num_qubits, 2u);
    EXPECT_EQ(state.num_states, 4u);
}

TEST_F(CingulateQuantumBridgeTest, EncodeOptionsNormalizesState) {
    cingulate_response_option_t options[4];
    create_test_options(options, 4);

    EXPECT_TRUE(cingulate_quantum_encode_options(bridge, options, 4));

    cingulate_quantum_state_t state;
    memset(&state, 0, sizeof(state));
    state.amplitudes = (float*)malloc(256 * sizeof(float));
    EXPECT_TRUE(cingulate_quantum_get_state(bridge, &state));

    // Total probability should be approximately 1.0
    EXPECT_NEAR(state.total_probability, 1.0f, 0.01f);

    free(state.amplitudes);
}

TEST_F(CingulateQuantumBridgeTest, EncodeOptionsNullFails) {
    EXPECT_FALSE(cingulate_quantum_encode_options(NULL, NULL, 0));
    EXPECT_FALSE(cingulate_quantum_encode_options(bridge, NULL, 4));
}

TEST_F(CingulateQuantumBridgeTest, EncodeOptionsZeroFails) {
    cingulate_response_option_t options[4];
    EXPECT_FALSE(cingulate_quantum_encode_options(bridge, options, 0));
}

/*=============================================================================
 * CONFLICT RESOLUTION TESTS
 *===========================================================================*/

TEST_F(CingulateQuantumBridgeTest, ApplyConstraintsSuccess) {
    cingulate_response_option_t options[4];
    create_test_options(options, 4);
    EXPECT_TRUE(cingulate_quantum_encode_options(bridge, options, 4));

    cingulate_conflict_t conflict;
    memset(&conflict, 0, sizeof(conflict));
    conflict.conflict_id = 1;
    conflict.option_a_id = 0;
    conflict.option_b_id = 1;
    conflict.conflict_level = 0.7f;

    EXPECT_TRUE(cingulate_quantum_apply_constraints(bridge, &conflict));
}

TEST_F(CingulateQuantumBridgeTest, ResolveConflictSuccess) {
    cingulate_response_option_t options[4];
    create_test_options(options, 4);
    EXPECT_TRUE(cingulate_quantum_encode_options(bridge, options, 4));

    cingulate_quantum_result_t result;
    EXPECT_TRUE(cingulate_quantum_resolve_conflict(bridge, &result));

    // Should select an option
    EXPECT_LT(result.selected_option, 4u);
    // Should have some confidence
    EXPECT_GE(result.selection_confidence, 0.0f);
    EXPECT_LE(result.selection_confidence, 1.0f);
}

TEST_F(CingulateQuantumBridgeTest, ResolveConflictWithoutEncodingFails) {
    cingulate_quantum_result_t result;
    EXPECT_FALSE(cingulate_quantum_resolve_conflict(bridge, &result));
}

TEST_F(CingulateQuantumBridgeTest, ResolveConflictUsesIterations) {
    cingulate_response_option_t options[8];
    create_test_options(options, 8);
    EXPECT_TRUE(cingulate_quantum_encode_options(bridge, options, 8));

    cingulate_quantum_result_t result;
    EXPECT_TRUE(cingulate_quantum_resolve_conflict(bridge, &result));

    // Should use some iterations
    EXPECT_GT(result.iterations_used, 0u);
}

TEST_F(CingulateQuantumBridgeTest, ResolveConflictReportsSpeedup) {
    cingulate_response_option_t options[4];
    create_test_options(options, 4);
    EXPECT_TRUE(cingulate_quantum_encode_options(bridge, options, 4));

    cingulate_quantum_result_t result;
    EXPECT_TRUE(cingulate_quantum_resolve_conflict(bridge, &result));

    // Quantum should provide some speedup
    EXPECT_GT(result.speedup_achieved, 0.0f);
}

TEST_F(CingulateQuantumBridgeTest, GetProbabilitiesSuccess) {
    cingulate_response_option_t options[4];
    create_test_options(options, 4);
    EXPECT_TRUE(cingulate_quantum_encode_options(bridge, options, 4));

    float probabilities[4];
    EXPECT_TRUE(cingulate_quantum_get_probabilities(bridge, probabilities, 4));

    // All probabilities should be non-negative
    float total = 0.0f;
    for (int i = 0; i < 4; i++) {
        EXPECT_GE(probabilities[i], 0.0f);
        total += probabilities[i];
    }
    // Total should be approximately 1.0
    EXPECT_NEAR(total, 1.0f, 0.01f);
}

/*=============================================================================
 * ERROR PROPAGATION TESTS
 *===========================================================================*/

TEST_F(CingulateQuantumBridgeTest, EncodeErrorSuccess) {
    cingulate_response_option_t options[4];
    create_test_options(options, 4);
    EXPECT_TRUE(cingulate_quantum_encode_options(bridge, options, 4));

    cingulate_error_event_t error;
    memset(&error, 0, sizeof(error));
    error.error_id = 1;
    error.executed_option = 0;
    error.intended_option = 1;
    error.error_magnitude = 0.8f;

    cingulate_quantum_error_t quantum_error;
    EXPECT_TRUE(cingulate_quantum_encode_error(bridge, &error, &quantum_error));

    EXPECT_FLOAT_EQ(quantum_error.error_magnitude, 0.8f);
    EXPECT_EQ(quantum_error.source_option, 0u);
}

TEST_F(CingulateQuantumBridgeTest, PropagateErrorSuccess) {
    cingulate_response_option_t options[4];
    create_test_options(options, 4);
    EXPECT_TRUE(cingulate_quantum_encode_options(bridge, options, 4));

    cingulate_quantum_error_t quantum_error;
    memset(&quantum_error, 0, sizeof(quantum_error));
    quantum_error.error_magnitude = 0.8f;
    quantum_error.phase = (float)M_PI * 0.8f;
    quantum_error.source_option = 0;

    EXPECT_TRUE(cingulate_quantum_propagate_error(bridge, &quantum_error));

    // State should be modified (probability redistributed)
    cingulate_quantum_state_t state;
    memset(&state, 0, sizeof(state));
    state.amplitudes = (float*)malloc(256 * sizeof(float));
    EXPECT_TRUE(cingulate_quantum_get_state(bridge, &state));

    // Total probability should still be approximately 1.0
    EXPECT_NEAR(state.total_probability, 1.0f, 0.1f);

    free(state.amplitudes);
}

TEST_F(CingulateQuantumBridgeTest, ErrorGradientSuccess) {
    cingulate_response_option_t options[4];
    create_test_options(options, 4);
    EXPECT_TRUE(cingulate_quantum_encode_options(bridge, options, 4));

    cingulate_quantum_error_t quantum_error;
    memset(&quantum_error, 0, sizeof(quantum_error));
    quantum_error.error_magnitude = 0.8f;
    quantum_error.source_option = 0;

    float gradients[4];
    EXPECT_TRUE(cingulate_quantum_error_gradient(bridge, &quantum_error, gradients, 4));

    // Source option should have negative gradient
    EXPECT_LT(gradients[0], 0.0f);
}

/*=============================================================================
 * CONTROL SUPERPOSITION TESTS
 *===========================================================================*/

TEST_F(CingulateQuantumBridgeTest, SuperposeControlSuccess) {
    EXPECT_TRUE(cingulate_quantum_superpose_control(bridge, 0.0f, 1.0f, 8));
}

TEST_F(CingulateQuantumBridgeTest, EvaluateControlSuccess) {
    EXPECT_TRUE(cingulate_quantum_superpose_control(bridge, 0.0f, 1.0f, 8));

    cingulate_conflict_t conflict;
    memset(&conflict, 0, sizeof(conflict));
    conflict.conflict_level = 0.7f;

    cingulate_error_event_t error;
    memset(&error, 0, sizeof(error));
    error.error_magnitude = 0.5f;

    EXPECT_TRUE(cingulate_quantum_evaluate_control(bridge, &conflict, &error));
}

TEST_F(CingulateQuantumBridgeTest, MeasureControlSuccess) {
    EXPECT_TRUE(cingulate_quantum_superpose_control(bridge, 0.0f, 1.0f, 8));

    float optimal_control = 0.0f;
    float confidence = 0.0f;

    EXPECT_TRUE(cingulate_quantum_measure_control(bridge, &optimal_control, &confidence));

    EXPECT_GE(optimal_control, 0.0f);
    EXPECT_LE(optimal_control, 1.0f);
    EXPECT_GE(confidence, 0.0f);
    EXPECT_LE(confidence, 1.0f);
}

/*=============================================================================
 * INTEGRATION TESTS
 *===========================================================================*/

TEST_F(CingulateQuantumBridgeTest, BridgeUpdateSuccess) {
    EXPECT_TRUE(cingulate_quantum_bridge_update(bridge));
}

TEST_F(CingulateQuantumBridgeTest, ApplyResolutionSuccess) {
    cingulate_response_option_t options[4];
    create_test_options(options, 4);
    EXPECT_TRUE(cingulate_quantum_encode_options(bridge, options, 4));

    cingulate_quantum_result_t result;
    EXPECT_TRUE(cingulate_quantum_resolve_conflict(bridge, &result));
    EXPECT_TRUE(cingulate_quantum_apply_resolution(bridge, &result));
}

TEST_F(CingulateQuantumBridgeTest, FullResolutionPipeline) {
    cingulate_response_option_t options[4];
    create_test_options(options, 4);
    EXPECT_TRUE(cingulate_quantum_encode_options(bridge, options, 4));

    cingulate_quantum_result_t result;
    EXPECT_TRUE(cingulate_quantum_full_resolution(bridge, &result));

    EXPECT_LT(result.selected_option, 4u);
}

/*=============================================================================
 * STATISTICS TESTS
 *===========================================================================*/

TEST_F(CingulateQuantumBridgeTest, GetStatsSuccess) {
    cingulate_quantum_stats_t stats;
    EXPECT_TRUE(cingulate_quantum_get_stats(bridge, &stats));

    // Initial stats should be zero
    EXPECT_EQ(stats.resolutions_attempted, 0u);
    EXPECT_EQ(stats.resolutions_successful, 0u);
}

TEST_F(CingulateQuantumBridgeTest, StatsUpdateAfterResolution) {
    cingulate_response_option_t options[4];
    create_test_options(options, 4);
    EXPECT_TRUE(cingulate_quantum_encode_options(bridge, options, 4));

    cingulate_quantum_result_t result;
    EXPECT_TRUE(cingulate_quantum_resolve_conflict(bridge, &result));

    cingulate_quantum_stats_t stats;
    EXPECT_TRUE(cingulate_quantum_get_stats(bridge, &stats));

    EXPECT_EQ(stats.resolutions_attempted, 1u);
    EXPECT_EQ(stats.resolutions_successful, 1u);
}

TEST_F(CingulateQuantumBridgeTest, StatsTrackAverages) {
    // Run multiple resolutions
    for (int i = 0; i < 5; i++) {
        cingulate_quantum_bridge_reset(bridge);
        cingulate_response_option_t options[4];
        create_test_options(options, 4);
        cingulate_quantum_encode_options(bridge, options, 4);

        cingulate_quantum_result_t result;
        cingulate_quantum_resolve_conflict(bridge, &result);
    }

    cingulate_quantum_stats_t stats;
    EXPECT_TRUE(cingulate_quantum_get_stats(bridge, &stats));

    EXPECT_EQ(stats.resolutions_successful, 5u);
    EXPECT_GT(stats.avg_iterations, 0.0f);
    EXPECT_GT(stats.avg_confidence, 0.0f);
    EXPECT_GT(stats.avg_speedup, 0.0f);
}

TEST_F(CingulateQuantumBridgeTest, GetConfigSuccess) {
    cingulate_quantum_config_t retrieved;
    EXPECT_TRUE(cingulate_quantum_get_config(bridge, &retrieved));

    EXPECT_EQ(retrieved.max_qubits, quantum_config.max_qubits);
    EXPECT_EQ(retrieved.max_iterations, quantum_config.max_iterations);
}

TEST_F(CingulateQuantumBridgeTest, GetStateSuccess) {
    cingulate_response_option_t options[4];
    create_test_options(options, 4);
    EXPECT_TRUE(cingulate_quantum_encode_options(bridge, options, 4));

    cingulate_quantum_state_t state;
    memset(&state, 0, sizeof(state));
    state.amplitudes = (float*)malloc(256 * sizeof(float));

    EXPECT_TRUE(cingulate_quantum_get_state(bridge, &state));

    EXPECT_EQ(state.num_qubits, 2u);
    EXPECT_EQ(state.num_states, 4u);

    free(state.amplitudes);
}

/*=============================================================================
 * EDGE CASE TESTS
 *===========================================================================*/

TEST_F(CingulateQuantumBridgeTest, HandleLargeNumberOfOptions) {
    // Max is 256 (8 qubits)
    cingulate_response_option_t options[16];
    create_test_options(options, 16);

    EXPECT_TRUE(cingulate_quantum_encode_options(bridge, options, 16));

    cingulate_quantum_state_t state;
    memset(&state, 0, sizeof(state));
    EXPECT_TRUE(cingulate_quantum_get_state(bridge, &state));

    // 16 options requires 4 qubits
    EXPECT_EQ(state.num_qubits, 4u);
}

TEST_F(CingulateQuantumBridgeTest, HandleSingleOption) {
    cingulate_response_option_t options[1];
    create_test_options(options, 1);

    EXPECT_TRUE(cingulate_quantum_encode_options(bridge, options, 1));

    cingulate_quantum_result_t result;
    EXPECT_TRUE(cingulate_quantum_resolve_conflict(bridge, &result));

    // Should select the only option
    EXPECT_EQ(result.selected_option, 0u);
}

TEST_F(CingulateQuantumBridgeTest, MultipleResolutionsAfterReset) {
    for (int i = 0; i < 3; i++) {
        cingulate_quantum_bridge_reset(bridge);

        cingulate_response_option_t options[4];
        create_test_options(options, 4);
        EXPECT_TRUE(cingulate_quantum_encode_options(bridge, options, 4));

        cingulate_quantum_result_t result;
        EXPECT_TRUE(cingulate_quantum_resolve_conflict(bridge, &result));
    }
}

/*=============================================================================
 * CLASSICAL FALLBACK TESTS
 *===========================================================================*/

TEST_F(CingulateQuantumBridgeTest, FallbackWhenNoMarkedOptions) {
    // Create options with very low activations
    cingulate_response_option_t options[4];
    for (int i = 0; i < 4; i++) {
        memset(&options[i], 0, sizeof(cingulate_response_option_t));
        options[i].option_id = i;
        options[i].activation = 0.1f;  // All low activation
    }

    EXPECT_TRUE(cingulate_quantum_encode_options(bridge, options, 4));

    cingulate_quantum_result_t result;
    EXPECT_TRUE(cingulate_quantum_resolve_conflict(bridge, &result));

    // Should have used fallback
    EXPECT_TRUE(result.used_fallback);
}
