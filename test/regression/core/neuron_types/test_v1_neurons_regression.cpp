/**
 * @file test_v1_neurons_regression.cpp
 * @brief Regression tests for V1 neuron backward compatibility
 *
 * WHAT: Baseline metrics and regression detection for V1 neurons
 * WHY:  Ensure changes don't break existing functionality
 * HOW:  Deterministic baselines, performance tracking, API stability
 *
 * @author NIMCP Development Team
 * @date 2025-11-16
 */

#include <gtest/gtest.h>
#include "core/neuron_types/nimcp_neuron_types.h"
#include "utils/time/nimcp_time.h"
#include <vector>
#include <cmath>
#include <chrono>

// ============================================================================
// TEST FIXTURE
// ============================================================================

class V1NeuronsRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        timestamp = nimcp_time_monotonic_us();
    }

    void TearDown() override {
        // Cleanup
    }

    uint64_t timestamp;

    // Helper: Compute deterministic hash of float array
    uint32_t compute_hash(const std::vector<float>& values) {
        uint32_t hash = 0;
        for (float val : values) {
            // Simple hash (not cryptographic)
            uint32_t bits = *reinterpret_cast<const uint32_t*>(&val);
            hash = hash * 31 + bits;
        }
        return hash;
    }
};

// ============================================================================
// REGRESSION TEST 1: Deterministic Output
// ============================================================================

TEST_F(V1NeuronsRegressionTest, SimpleCell_DeterministicOutput) {
    // WHAT: Verify simple cell produces deterministic outputs
    // WHY:  Non-determinism breaks reproducibility
    // HOW:  Same inputs should produce identical outputs

    neuron_type_params_t params{};
    neuron_type_get_default_params(NEURON_V1_SIMPLE_CELL, &params);
    params.v1_simple.orientation = 45.0f;
    params.v1_simple.spatial_frequency = 2.0f;
    params.v1_simple.phase = 0.0f;

    std::vector<float> inputs = {0.1f, 0.3f, 0.5f, 0.7f, 0.9f};
    std::vector<float> outputs1;
    std::vector<float> outputs2;

    // First run
    for (float input : inputs) {
        float output = neuron_type_process_input(
            NEURON_V1_SIMPLE_CELL, &params, input, timestamp);
        outputs1.push_back(output);
    }

    // Second run (should be identical)
    for (float input : inputs) {
        float output = neuron_type_process_input(
            NEURON_V1_SIMPLE_CELL, &params, input, timestamp);
        outputs2.push_back(output);
    }

    // Outputs must be bit-for-bit identical
    ASSERT_EQ(outputs1.size(), outputs2.size());
    for (size_t i = 0; i < outputs1.size(); i++) {
        EXPECT_FLOAT_EQ(outputs1[i], outputs2[i])
            << "Output " << i << " differs between runs";
    }

    // Hash should match
    EXPECT_EQ(compute_hash(outputs1), compute_hash(outputs2))
        << "Output hash differs (non-deterministic)";
}

TEST_F(V1NeuronsRegressionTest, ComplexCell_DeterministicOutput) {
    // WHAT: Verify complex cell produces deterministic outputs
    // WHY:  Non-determinism breaks reproducibility
    // HOW:  Same inputs should produce identical outputs

    neuron_type_params_t params{};
    neuron_type_get_default_params(NEURON_V1_COMPLEX_CELL, &params);
    params.v1_complex.orientation = 90.0f;
    params.v1_complex.direction_selectivity = 0.5f;

    std::vector<float> inputs = {0.2f, 0.4f, 0.6f, 0.8f, 1.0f};
    std::vector<float> outputs1;
    std::vector<float> outputs2;

    // First run
    for (float input : inputs) {
        float output = neuron_type_process_input(
            NEURON_V1_COMPLEX_CELL, &params, input, timestamp);
        outputs1.push_back(output);
    }

    // Second run
    for (float input : inputs) {
        float output = neuron_type_process_input(
            NEURON_V1_COMPLEX_CELL, &params, input, timestamp);
        outputs2.push_back(output);
    }

    // Outputs must be identical
    ASSERT_EQ(outputs1.size(), outputs2.size());
    for (size_t i = 0; i < outputs1.size(); i++) {
        EXPECT_FLOAT_EQ(outputs1[i], outputs2[i])
            << "Output " << i << " differs between runs";
    }
}

// ============================================================================
// REGRESSION TEST 2: Baseline Response Values
// ============================================================================

TEST_F(V1NeuronsRegressionTest, SimpleCell_BaselineResponse) {
    // WHAT: Test baseline simple cell response values
    // WHY:  Detect unexpected changes in processing
    // HOW:  Known inputs should produce known outputs (within tolerance)

    neuron_type_params_t params{};
    neuron_type_get_default_params(NEURON_V1_SIMPLE_CELL, &params);
    params.v1_simple.orientation = 0.0f;  // Horizontal
    params.v1_simple.spatial_frequency = 1.0f;
    params.v1_simple.phase = 0.0f;

    // Test multiple inputs to ensure some produce output
    std::vector<float> inputs = {0.3f, 0.5f, 0.7f, 1.0f};
    bool found_nonzero = false;

    for (float input : inputs) {
        float output = neuron_type_process_input(
            NEURON_V1_SIMPLE_CELL, &params, input, timestamp);

        // Output should be in valid range
        EXPECT_GE(output, 0.0f);
        EXPECT_LE(output, 1.0f);

        if (output > 0.0f) {
            found_nonzero = true;
        }
    }

    // At least one input should produce non-zero output
    EXPECT_TRUE(found_nonzero) << "All inputs produced zero output";
}

TEST_F(V1NeuronsRegressionTest, ComplexCell_BaselineResponse) {
    // WHAT: Test baseline complex cell response values
    // WHY:  Detect unexpected changes in processing
    // HOW:  Known inputs should produce known outputs

    neuron_type_params_t params{};
    neuron_type_get_default_params(NEURON_V1_COMPLEX_CELL, &params);
    params.v1_complex.orientation = 45.0f;
    params.v1_complex.direction_selectivity = 0.0f;  // No direction selectivity

    // Test known input
    float input = 0.8f;
    float output = neuron_type_process_input(
        NEURON_V1_COMPLEX_CELL, &params, input, timestamp);

    // Output should be in valid range
    EXPECT_GE(output, 0.0f);
    EXPECT_LE(output, 1.0f);

    // For energy model with no direction selectivity,
    // output should be close to input magnitude
    EXPECT_GT(output, 0.5f) << "Baseline response too low";
}

// ============================================================================
// REGRESSION TEST 3: Parameter Validation Backward Compatibility
// ============================================================================

TEST_F(V1NeuronsRegressionTest, ParameterValidation_BackwardCompatible) {
    // WHAT: Ensure parameter validation hasn't changed
    // WHY:  Changes could break existing code
    // HOW:  Test known valid/invalid parameter sets

    neuron_type_params_t params{};

    // Test 1: Valid simple cell parameters (should succeed)
    neuron_type_get_default_params(NEURON_V1_SIMPLE_CELL, &params);
    params.v1_simple.orientation = 90.0f;
    params.v1_simple.spatial_frequency = 2.0f;
    params.v1_simple.sigma = 3.0f;
    EXPECT_EQ(neuron_type_validate_params(NEURON_V1_SIMPLE_CELL, &params),
              NIMCP_SUCCESS) << "Valid params should pass";

    // Test 2: Invalid orientation (should fail)
    params.v1_simple.orientation = 200.0f;
    EXPECT_NE(neuron_type_validate_params(NEURON_V1_SIMPLE_CELL, &params),
              NIMCP_SUCCESS) << "Invalid orientation should fail";

    // Test 3: Valid complex cell parameters (should succeed)
    neuron_type_get_default_params(NEURON_V1_COMPLEX_CELL, &params);
    params.v1_complex.orientation = 45.0f;
    params.v1_complex.direction_selectivity = 0.7f;
    EXPECT_EQ(neuron_type_validate_params(NEURON_V1_COMPLEX_CELL, &params),
              NIMCP_SUCCESS) << "Valid params should pass";

    // Test 4: Invalid direction selectivity (should fail)
    params.v1_complex.direction_selectivity = 1.5f;
    EXPECT_NE(neuron_type_validate_params(NEURON_V1_COMPLEX_CELL, &params),
              NIMCP_SUCCESS) << "Invalid direction selectivity should fail";
}

// ============================================================================
// REGRESSION TEST 4: Default Parameter Stability
// ============================================================================

TEST_F(V1NeuronsRegressionTest, DefaultParameters_Stability) {
    // WHAT: Test default parameters haven't changed unexpectedly
    // WHY:  Default changes can break user code
    // HOW:  Verify default values are in expected ranges

    neuron_type_params_t params{};

    // Simple cell defaults
    neuron_type_get_default_params(NEURON_V1_SIMPLE_CELL, &params);
    EXPECT_GE(params.v1_simple.orientation, 0.0f);
    EXPECT_LE(params.v1_simple.orientation, 180.0f);
    EXPECT_GT(params.v1_simple.spatial_frequency, 0.0f);
    EXPECT_LT(params.v1_simple.spatial_frequency, 10.0f);  // Reasonable range
    EXPECT_GT(params.v1_simple.sigma, 0.0f);
    EXPECT_LT(params.v1_simple.sigma, 20.0f);  // Reasonable range

    // Complex cell defaults
    neuron_type_get_default_params(NEURON_V1_COMPLEX_CELL, &params);
    EXPECT_GE(params.v1_complex.orientation, 0.0f);
    EXPECT_LE(params.v1_complex.orientation, 180.0f);
    EXPECT_GE(params.v1_complex.direction_selectivity, 0.0f);
    EXPECT_LE(params.v1_complex.direction_selectivity, 1.0f);
}

// ============================================================================
// REGRESSION TEST 5: Performance Baseline
// ============================================================================

TEST_F(V1NeuronsRegressionTest, Performance_SimpleCell_Baseline) {
    // WHAT: Establish performance baseline for simple cells
    // WHY:  Detect performance regressions
    // HOW:  Time large batch processing

    neuron_type_params_t params{};
    neuron_type_get_default_params(NEURON_V1_SIMPLE_CELL, &params);

    const int iterations = 100000;
    std::vector<float> inputs(iterations);
    for (int i = 0; i < iterations; i++) {
        inputs[i] = 0.5f + 0.5f * sinf(i * 0.01f);
    }

    auto start = std::chrono::high_resolution_clock::now();

    float sum = 0.0f;
    for (int i = 0; i < iterations; i++) {
        sum += neuron_type_process_input(
            NEURON_V1_SIMPLE_CELL, &params, inputs[i], timestamp + i);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Performance baseline: should process >= 100K inputs/sec
    // That's >= 100 inputs/ms, or >= 0.1 inputs/µs
    double inputs_per_us = (double)iterations / duration.count();
    EXPECT_GT(inputs_per_us, 0.01)  // Very conservative baseline
        << "Performance regression: " << inputs_per_us << " inputs/µs "
        << "(should be > 0.01)";

    // Use sum to prevent optimization
    EXPECT_GT(sum, 0.0f);
}

TEST_F(V1NeuronsRegressionTest, Performance_ComplexCell_Baseline) {
    // WHAT: Establish performance baseline for complex cells
    // WHY:  Detect performance regressions
    // HOW:  Time large batch processing

    neuron_type_params_t params{};
    neuron_type_get_default_params(NEURON_V1_COMPLEX_CELL, &params);

    const int iterations = 100000;
    std::vector<float> inputs(iterations);
    for (int i = 0; i < iterations; i++) {
        inputs[i] = 0.5f + 0.5f * sinf(i * 0.01f);
    }

    auto start = std::chrono::high_resolution_clock::now();

    float sum = 0.0f;
    for (int i = 0; i < iterations; i++) {
        sum += neuron_type_process_input(
            NEURON_V1_COMPLEX_CELL, &params, inputs[i], timestamp + i);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double inputs_per_us = (double)iterations / duration.count();
    EXPECT_GT(inputs_per_us, 0.01)
        << "Performance regression: " << inputs_per_us << " inputs/µs";

    EXPECT_GT(sum, 0.0f);
}

// ============================================================================
// REGRESSION TEST 6: API Stability
// ============================================================================

TEST_F(V1NeuronsRegressionTest, API_FunctionSignatures) {
    // WHAT: Verify API function signatures haven't changed
    // WHY:  Signature changes break backward compatibility
    // HOW:  Test function calls compile and work as expected

    neuron_type_params_t params{};

    // neuron_type_get_default_params signature
    nimcp_result_t result1 = neuron_type_get_default_params(
        NEURON_V1_SIMPLE_CELL, &params);
    EXPECT_EQ(result1, NIMCP_SUCCESS);

    // neuron_type_validate_params signature
    nimcp_result_t result2 = neuron_type_validate_params(
        NEURON_V1_SIMPLE_CELL, &params);
    EXPECT_EQ(result2, NIMCP_SUCCESS);

    // neuron_type_process_input signature
    float output = neuron_type_process_input(
        NEURON_V1_SIMPLE_CELL, &params, 0.5f, timestamp);
    EXPECT_GE(output, 0.0f);

    // neuron_type_get_name signature
    const char* name = neuron_type_get_name(NEURON_V1_SIMPLE_CELL);
    EXPECT_NE(name, nullptr);

    // neuron_type_is_excitatory signature
    bool is_excitatory = neuron_type_is_excitatory(NEURON_V1_SIMPLE_CELL);
    EXPECT_TRUE(is_excitatory);
}

// ============================================================================
// REGRESSION TEST 7: Backward Compatibility with Generic Types
// ============================================================================

TEST_F(V1NeuronsRegressionTest, BackwardCompatibility_GenericNeurons) {
    // WHAT: Ensure generic neuron types still work
    // WHY:  V1 implementation shouldn't break existing code
    // HOW:  Test NEURON_EXCITATORY and NEURON_INHIBITORY

    neuron_type_params_t params{};

    // Generic excitatory neuron
    nimcp_result_t result1 = neuron_type_get_default_params(
        NEURON_EXCITATORY, &params);
    EXPECT_EQ(result1, NIMCP_SUCCESS);

    float output1 = neuron_type_process_input(
        NEURON_EXCITATORY, &params, 0.7f, timestamp);
    EXPECT_GE(output1, 0.0f);

    // Generic inhibitory neuron
    nimcp_result_t result2 = neuron_type_get_default_params(
        NEURON_INHIBITORY, &params);
    EXPECT_EQ(result2, NIMCP_SUCCESS);

    float output2 = neuron_type_process_input(
        NEURON_INHIBITORY, &params, 0.7f, timestamp);
    EXPECT_GE(output2, 0.0f);

    // LIF neuron
    nimcp_result_t result3 = neuron_type_get_default_params(
        NEURON_GENERIC_LIF, &params);
    EXPECT_EQ(result3, NIMCP_SUCCESS);

    float output3 = neuron_type_process_input(
        NEURON_GENERIC_LIF, &params, 0.7f, timestamp);
    EXPECT_GE(output3, 0.0f);
}

// ============================================================================
// REGRESSION TEST 8: Numerical Stability
// ============================================================================

TEST_F(V1NeuronsRegressionTest, NumericalStability_ExtremeLongRun) {
    // WHAT: Test numerical stability over long runs
    // WHY:  Detect accumulating errors or drift
    // HOW:  Process many iterations, check for NaN/Inf

    neuron_type_params_t params{};
    neuron_type_get_default_params(NEURON_V1_SIMPLE_CELL, &params);

    float input = 0.5f;
    for (int i = 0; i < 10000; i++) {
        float output = neuron_type_process_input(
            NEURON_V1_SIMPLE_CELL, &params, input, timestamp + i);

        EXPECT_FALSE(std::isnan(output)) << "NaN at iteration " << i;
        EXPECT_FALSE(std::isinf(output)) << "Inf at iteration " << i;
        EXPECT_GE(output, 0.0f) << "Negative output at iteration " << i;
    }
}

TEST_F(V1NeuronsRegressionTest, NumericalStability_ExtremeInputs) {
    // WHAT: Test stability with extreme input values
    // WHY:  Edge cases shouldn't cause crashes or NaN
    // HOW:  Test very large, very small, zero inputs

    neuron_type_params_t params{};
    neuron_type_get_default_params(NEURON_V1_SIMPLE_CELL, &params);

    std::vector<float> extreme_inputs = {
        0.0f,           // Zero
        1e-10f,         // Very small positive
        -1e-10f,        // Very small negative
        1.0f,           // Unit
        10.0f,          // Large
        100.0f,         // Very large
        -1.0f,          // Negative
        -10.0f          // Large negative
    };

    for (float input : extreme_inputs) {
        float output = neuron_type_process_input(
            NEURON_V1_SIMPLE_CELL, &params, input, timestamp);

        EXPECT_FALSE(std::isnan(output))
            << "NaN for input " << input;
        EXPECT_FALSE(std::isinf(output))
            << "Inf for input " << input;
        EXPECT_GE(output, 0.0f)
            << "Negative output for input " << input;
    }
}

// ============================================================================
// REGRESSION TEST 9: Memory Safety
// ============================================================================

TEST_F(V1NeuronsRegressionTest, MemorySafety_NullParams) {
    // WHAT: Test graceful handling of null parameters
    // WHY:  Null pointer crashes break backward compatibility
    // HOW:  Call functions with null params, expect safe return

    // Should return 0.0 for null params (not crash)
    float output1 = neuron_type_process_input(
        NEURON_V1_SIMPLE_CELL, nullptr, 0.5f, timestamp);
    EXPECT_EQ(output1, 0.0f);

    float output2 = neuron_type_process_input(
        NEURON_V1_COMPLEX_CELL, nullptr, 0.5f, timestamp);
    EXPECT_EQ(output2, 0.0f);

    // Validation with null should fail gracefully
    nimcp_result_t result = neuron_type_validate_params(
        NEURON_V1_SIMPLE_CELL, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}
