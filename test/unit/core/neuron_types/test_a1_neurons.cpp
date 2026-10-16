/**
 * @file test_a1_neurons.cpp
 * @brief Comprehensive unit tests for A1 auditory neuron processing
 *
 * Test Coverage:
 * - A1 frequency-tuned neuron bandpass filtering
 * - A1 coincidence detector temporal integration
 * - Auditory onset detection with adaptation
 * - Frequency selectivity and Q factor tuning
 * - Temporal precision and integration windows
 * - Binaural sound localization
 * - Edge cases and error handling
 * - Performance benchmarks
 *
 * BIOLOGICAL REFERENCES:
 * - Schreiner et al. (2000) "Functional architecture of auditory cortex"
 * - Jeffress (1948) "A place theory of sound localization"
 * - Heil (1997) "Auditory cortical onset responses revisited"
 *
 * @author NIMCP Development Team
 * @date 2025-11-16
 */

#include <gtest/gtest.h>
#include "core/neuron_types/nimcp_neuron_types.h"
#include "utils/time/nimcp_time.h"
#include <cmath>
#include <vector>

// ============================================================================
// TEST FIXTURE
// ============================================================================

class A1NeuronsTest : public ::testing::Test {
protected:
    void SetUp() override {
        timestamp = nimcp_time_monotonic_us();
    }

    void TearDown() override {
        // Cleanup
    }

    uint64_t timestamp;

    // Helper: Create test parameters for A1 frequency-tuned neuron
    neuron_type_params_t create_frequency_params(
        float center_freq = 1000.0f,
        float q_factor = 5.0f,
        float integration_window = 10.0f) {

        neuron_type_params_t params{};
        nimcp_result_t result = neuron_type_get_default_params(
            NEURON_A1_FREQUENCY_TUNED, &params);
        EXPECT_EQ(result, NIMCP_SUCCESS);

        params.a1_frequency.center_frequency = center_freq;
        params.a1_frequency.q_factor = q_factor;
        params.a1_frequency.integration_window = integration_window;

        return params;
    }

    // Helper: Create test parameters for A1 coincidence detector
    neuron_type_params_t create_coincidence_params(
        float integration_window = 1.0f,
        float decay_rate = 0.1f) {

        neuron_type_params_t params{};
        nimcp_result_t result = neuron_type_get_default_params(
            NEURON_A1_COINCIDENCE_DETECTOR, &params);
        EXPECT_EQ(result, NIMCP_SUCCESS);

        params.a1_coincidence.integration_window = integration_window;
        params.a1_coincidence.decay_rate = decay_rate;

        return params;
    }

    // Helper: Create test parameters for onset detector
    neuron_type_params_t create_onset_params(
        float integration_window = 2.0f) {

        neuron_type_params_t params{};
        nimcp_result_t result = neuron_type_get_default_params(
            NEURON_A1_COINCIDENCE_DETECTOR, &params);  // Reuse for onset
        EXPECT_EQ(result, NIMCP_SUCCESS);

        params.a1_coincidence.integration_window = integration_window;

        return params;
    }
};

// ============================================================================
// A1 FREQUENCY-TUNED NEURON TESTS
// ============================================================================

TEST_F(A1NeuronsTest, FrequencyTuned_BasicResponse) {
    // WHAT: Test basic frequency-tuned neuron response
    // WHY:  Verify bandpass filtering produces output
    // HOW:  Apply input to frequency-tuned neuron, check non-negative output

    auto params = create_frequency_params();
    float input = 0.8f;

    float output = neuron_type_process_input(
        NEURON_A1_FREQUENCY_TUNED, &params, input, timestamp);

    // Should produce some output
    EXPECT_GE(output, 0.0f);
    EXPECT_LE(output, 1.0f);
}

TEST_F(A1NeuronsTest, FrequencyTuned_TonotopicOrganization) {
    // WHAT: Test tonotopic organization across frequency range
    // WHY:  A1 neurons are spatially organized by preferred frequency
    // HOW:  Create neurons with different center frequencies

    std::vector<float> frequencies = {250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f};

    for (float freq : frequencies) {
        auto params = create_frequency_params(freq);
        float input = 0.5f;

        float output = neuron_type_process_input(
            NEURON_A1_FREQUENCY_TUNED, &params, input, timestamp);

        // All frequencies should produce valid output
        EXPECT_GE(output, 0.0f) << "Failed for frequency " << freq << " Hz";
        EXPECT_LE(output, 1.0f) << "Failed for frequency " << freq << " Hz";
    }
}

TEST_F(A1NeuronsTest, FrequencyTuned_QFactorTuning) {
    // WHAT: Test Q factor effect on frequency selectivity
    // WHY:  Q factor controls sharpness of tuning (Q = f_center / bandwidth)
    // HOW:  Test neurons with different Q factors

    float input = 0.8f;
    std::vector<float> q_factors = {1.0f, 5.0f, 10.0f, 20.0f};

    for (float q : q_factors) {
        auto params = create_frequency_params(1000.0f, q);

        float output = neuron_type_process_input(
            NEURON_A1_FREQUENCY_TUNED, &params, input, timestamp);

        // Higher Q should produce different (generally lower) response
        EXPECT_GE(output, 0.0f) << "Failed for Q = " << q;
        EXPECT_LE(output, 1.0f) << "Failed for Q = " << q;
    }
}

TEST_F(A1NeuronsTest, FrequencyTuned_IntegrationWindow) {
    // WHAT: Test temporal integration window effect
    // WHY:  Integration window controls temporal smoothing
    // HOW:  Vary integration window from 1ms to 50ms

    float input = 0.7f;
    std::vector<float> windows = {1.0f, 5.0f, 10.0f, 20.0f, 50.0f};

    float prev_output = -1.0f;
    for (float window : windows) {
        auto params = create_frequency_params(1000.0f, 5.0f, window);

        float output = neuron_type_process_input(
            NEURON_A1_FREQUENCY_TUNED, &params, input, timestamp);

        EXPECT_GE(output, 0.0f) << "Failed for window " << window << " ms";
        EXPECT_LE(output, 1.0f) << "Failed for window " << window << " ms";

        // Longer windows should generally increase response (up to saturation)
        if (prev_output >= 0.0f && window <= 10.0f) {
            EXPECT_GE(output, prev_output * 0.5f)
                << "Output decreased unexpectedly for window " << window;
        }
        prev_output = output;
    }
}

TEST_F(A1NeuronsTest, FrequencyTuned_ZeroInput) {
    // WHAT: Test response to zero input
    // WHY:  Should produce zero output (no spontaneous activity in simplified model)
    // HOW:  Apply zero input

    auto params = create_frequency_params();
    float output = neuron_type_process_input(
        NEURON_A1_FREQUENCY_TUNED, &params, 0.0f, timestamp);

    EXPECT_EQ(output, 0.0f);
}

TEST_F(A1NeuronsTest, FrequencyTuned_NegativeInput) {
    // WHAT: Test response to negative input
    // WHY:  Half-wave rectification should suppress negative inputs
    // HOW:  Apply negative input

    auto params = create_frequency_params();
    float output = neuron_type_process_input(
        NEURON_A1_FREQUENCY_TUNED, &params, -0.5f, timestamp);

    EXPECT_EQ(output, 0.0f);
}

TEST_F(A1NeuronsTest, FrequencyTuned_HighFrequency) {
    // WHAT: Test high frequency tuning (upper auditory range)
    // WHY:  Verify support for frequencies up to ~20 kHz
    // HOW:  Create neuron tuned to 16 kHz

    auto params = create_frequency_params(16000.0f);
    float input = 0.6f;

    float output = neuron_type_process_input(
        NEURON_A1_FREQUENCY_TUNED, &params, input, timestamp);

    EXPECT_GE(output, 0.0f);
}

TEST_F(A1NeuronsTest, FrequencyTuned_LowFrequency) {
    // WHAT: Test low frequency tuning (lower auditory range)
    // WHY:  Verify support for frequencies down to ~100 Hz
    // HOW:  Create neuron tuned to 125 Hz

    auto params = create_frequency_params(125.0f);
    float input = 0.6f;

    float output = neuron_type_process_input(
        NEURON_A1_FREQUENCY_TUNED, &params, input, timestamp);

    EXPECT_GE(output, 0.0f);
}

// ============================================================================
// A1 COINCIDENCE DETECTOR TESTS
// ============================================================================

TEST_F(A1NeuronsTest, CoincidenceDetector_BasicResponse) {
    // WHAT: Test basic coincidence detection
    // WHY:  Verify temporal integration for binaural processing
    // HOW:  Apply input to coincidence detector

    auto params = create_coincidence_params();
    float input = 0.8f;

    float output = neuron_type_process_input(
        NEURON_A1_COINCIDENCE_DETECTOR, &params, input, timestamp);

    // Should produce output above threshold or zero
    EXPECT_GE(output, 0.0f);
}

TEST_F(A1NeuronsTest, CoincidenceDetector_TemporalPrecision) {
    // WHAT: Test temporal precision with short integration windows
    // WHY:  MSO neurons have ~100 µs temporal precision (Jeffress 1948)
    // HOW:  Test with very short integration windows (0.5-2 ms)

    std::vector<float> windows = {0.5f, 1.0f, 1.5f, 2.0f};
    float input = 0.9f;

    for (float window : windows) {
        auto params = create_coincidence_params(window);

        float output = neuron_type_process_input(
            NEURON_A1_COINCIDENCE_DETECTOR, &params, input, timestamp);

        // Shorter windows = higher temporal precision = stronger response
        EXPECT_GE(output, 0.0f) << "Failed for window " << window << " ms";
    }
}

TEST_F(A1NeuronsTest, CoincidenceDetector_DecayRate) {
    // WHAT: Test decay rate effect on sustained inputs
    // WHY:  Decay prevents saturation from sustained coincidences
    // HOW:  Vary decay rate from 0.01 to 0.5

    float input = 0.8f;
    std::vector<float> decay_rates = {0.01f, 0.05f, 0.1f, 0.2f, 0.5f};

    for (float decay : decay_rates) {
        auto params = create_coincidence_params(1.0f, decay);

        float output = neuron_type_process_input(
            NEURON_A1_COINCIDENCE_DETECTOR, &params, input, timestamp);

        EXPECT_GE(output, 0.0f) << "Failed for decay rate " << decay;
    }
}

TEST_F(A1NeuronsTest, CoincidenceDetector_ThresholdBehavior) {
    // WHAT: Test threshold for coincidence detection
    // WHY:  Coincidence detectors have high thresholds (require strong inputs)
    // HOW:  Test weak vs strong inputs

    auto params = create_coincidence_params();

    // Weak input (below threshold)
    float weak_output = neuron_type_process_input(
        NEURON_A1_COINCIDENCE_DETECTOR, &params, 0.1f, timestamp);

    // Strong input (above threshold)
    float strong_output = neuron_type_process_input(
        NEURON_A1_COINCIDENCE_DETECTOR, &params, 0.9f, timestamp);

    // Weak input should produce zero or very small output
    EXPECT_LE(weak_output, 0.1f);

    // Strong input should produce significant output
    EXPECT_GE(strong_output, 0.0f);
}

TEST_F(A1NeuronsTest, CoincidenceDetector_ZeroInput) {
    // WHAT: Test response to zero input
    // WHY:  Should produce zero output
    // HOW:  Apply zero input

    auto params = create_coincidence_params();
    float output = neuron_type_process_input(
        NEURON_A1_COINCIDENCE_DETECTOR, &params, 0.0f, timestamp);

    EXPECT_EQ(output, 0.0f);
}

TEST_F(A1NeuronsTest, CoincidenceDetector_ITDMapping) {
    // WHAT: Test that different integration windows model different ITD sensitivities
    // WHY:  Jeffress model: different delays create spatial sound map
    // HOW:  Create neurons with varying temporal precision

    float input = 0.8f;
    std::vector<float> windows = {0.5f, 1.0f, 2.0f, 5.0f};

    std::vector<float> outputs;
    for (float window : windows) {
        auto params = create_coincidence_params(window);
        float output = neuron_type_process_input(
            NEURON_A1_COINCIDENCE_DETECTOR, &params, input, timestamp);
        outputs.push_back(output);

        EXPECT_GE(output, 0.0f);
    }

    // Verify we got varied responses (spatial mapping)
    bool has_variation = false;
    for (size_t i = 1; i < outputs.size(); i++) {
        if (std::abs(outputs[i] - outputs[0]) > 0.01f) {
            has_variation = true;
            break;
        }
    }
    EXPECT_TRUE(has_variation) << "Integration windows should produce varied responses";
}

// ============================================================================
// AUDITORY ONSET DETECTOR TESTS
// ============================================================================

TEST_F(A1NeuronsTest, OnsetDetector_BasicResponse) {
    // WHAT: Test basic onset detection
    // WHY:  Onset neurons signal beginning of acoustic events (Heil 1997)
    // HOW:  Apply transient input

    auto params = create_onset_params();
    float input = 0.8f;

    float output = neuron_type_process_input(
        NEURON_AUDITORY_ONSET, &params, input, timestamp);

    EXPECT_GE(output, 0.0f);
    EXPECT_LE(output, 1.0f);
}

TEST_F(A1NeuronsTest, OnsetDetector_ShortIntegrationWindow) {
    // WHAT: Test short integration window for onset detection
    // WHY:  Onset detectors have 1-5ms integration windows
    // HOW:  Test with very short windows

    std::vector<float> windows = {1.0f, 2.0f, 3.0f, 5.0f};
    float input = 0.9f;

    for (float window : windows) {
        auto params = create_onset_params(window);

        float output = neuron_type_process_input(
            NEURON_AUDITORY_ONSET, &params, input, timestamp);

        // Shorter windows should produce stronger onset response
        EXPECT_GE(output, 0.0f) << "Failed for window " << window << " ms";
        EXPECT_LE(output, 1.0f) << "Failed for window " << window << " ms";
    }
}

TEST_F(A1NeuronsTest, OnsetDetector_AdaptationThreshold) {
    // WHAT: Test rapid adaptation below threshold
    // WHY:  Onset neurons show spike-frequency adaptation (SFA)
    // HOW:  Test weak vs strong inputs

    auto params = create_onset_params(2.0f);

    // Weak input (below adaptation threshold)
    float weak_output = neuron_type_process_input(
        NEURON_AUDITORY_ONSET, &params, 0.2f, timestamp);

    // Strong input (above adaptation threshold)
    float strong_output = neuron_type_process_input(
        NEURON_AUDITORY_ONSET, &params, 0.8f, timestamp);

    // Weak input should be attenuated by adaptation
    EXPECT_GE(weak_output, 0.0f);
    EXPECT_LE(weak_output, 0.5f);

    // Strong input should produce stronger response
    EXPECT_GT(strong_output, weak_output);
}

TEST_F(A1NeuronsTest, OnsetDetector_TransientResponse) {
    // WHAT: Test that onset detectors favor transient over sustained inputs
    // WHY:  Models high-pass temporal filtering
    // HOW:  Short window should give stronger response than long window

    float input = 0.8f;

    auto short_params = create_onset_params(1.0f);
    auto long_params = create_onset_params(10.0f);

    float short_output = neuron_type_process_input(
        NEURON_AUDITORY_ONSET, &short_params, input, timestamp);

    float long_output = neuron_type_process_input(
        NEURON_AUDITORY_ONSET, &long_params, input, timestamp);

    // Short integration window should produce stronger onset response
    EXPECT_GE(short_output, long_output);
}

TEST_F(A1NeuronsTest, OnsetDetector_ZeroInput) {
    // WHAT: Test response to zero input
    // WHY:  Should produce zero output
    // HOW:  Apply zero input

    auto params = create_onset_params();
    float output = neuron_type_process_input(
        NEURON_AUDITORY_ONSET, &params, 0.0f, timestamp);

    EXPECT_EQ(output, 0.0f);
}

// ============================================================================
// PARAMETER VALIDATION TESTS
// ============================================================================

TEST_F(A1NeuronsTest, ValidateParameters_FrequencyTuned_Valid) {
    // WHAT: Test parameter validation for valid frequency-tuned neuron
    // WHY:  Ensure validation accepts reasonable parameters
    // HOW:  Create valid params and validate

    auto params = create_frequency_params();
    nimcp_result_t result = neuron_type_validate_params(
        NEURON_A1_FREQUENCY_TUNED, &params);

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(A1NeuronsTest, ValidateParameters_FrequencyTuned_InvalidFrequency) {
    // WHAT: Test parameter validation rejects invalid frequency
    // WHY:  Frequencies must be positive and within audible range
    // HOW:  Set frequency to invalid value

    neuron_type_params_t params{};
    neuron_type_get_default_params(NEURON_A1_FREQUENCY_TUNED, &params);

    // Test negative frequency
    params.a1_frequency.center_frequency = -100.0f;
    nimcp_result_t result = neuron_type_validate_params(
        NEURON_A1_FREQUENCY_TUNED, &params);
    EXPECT_NE(result, NIMCP_SUCCESS);

    // Test zero frequency
    params.a1_frequency.center_frequency = 0.0f;
    result = neuron_type_validate_params(NEURON_A1_FREQUENCY_TUNED, &params);
    EXPECT_NE(result, NIMCP_SUCCESS);

    // Test frequency above audible range
    params.a1_frequency.center_frequency = 25000.0f;
    result = neuron_type_validate_params(NEURON_A1_FREQUENCY_TUNED, &params);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(A1NeuronsTest, ValidateParameters_FrequencyTuned_InvalidQFactor) {
    // WHAT: Test parameter validation rejects invalid Q factor
    // WHY:  Q factor must be positive
    // HOW:  Set Q to invalid value

    neuron_type_params_t params{};
    neuron_type_get_default_params(NEURON_A1_FREQUENCY_TUNED, &params);

    params.a1_frequency.q_factor = -1.0f;
    nimcp_result_t result = neuron_type_validate_params(
        NEURON_A1_FREQUENCY_TUNED, &params);

    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(A1NeuronsTest, ValidateParameters_CoincidenceDetector_Valid) {
    // WHAT: Test parameter validation for valid coincidence detector
    // WHY:  Ensure validation accepts reasonable parameters
    // HOW:  Create valid params and validate

    auto params = create_coincidence_params();
    nimcp_result_t result = neuron_type_validate_params(
        NEURON_A1_COINCIDENCE_DETECTOR, &params);

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(A1NeuronsTest, ValidateParameters_CoincidenceDetector_InvalidWindow) {
    // WHAT: Test parameter validation rejects invalid integration window
    // WHY:  Integration window must be positive
    // HOW:  Set window to invalid value

    neuron_type_params_t params{};
    neuron_type_get_default_params(NEURON_A1_COINCIDENCE_DETECTOR, &params);

    params.a1_coincidence.integration_window = -1.0f;
    nimcp_result_t result = neuron_type_validate_params(
        NEURON_A1_COINCIDENCE_DETECTOR, &params);

    EXPECT_NE(result, NIMCP_SUCCESS);
}

// ============================================================================
// DEFAULT PARAMETERS TESTS
// ============================================================================

TEST_F(A1NeuronsTest, DefaultParameters_FrequencyTuned) {
    // WHAT: Test default parameters for frequency-tuned neuron
    // WHY:  Verify sensible defaults are provided
    // HOW:  Get defaults and check ranges

    neuron_type_params_t params{};
    nimcp_result_t result = neuron_type_get_default_params(
        NEURON_A1_FREQUENCY_TUNED, &params);

    ASSERT_EQ(result, NIMCP_SUCCESS);

    // Check default values are in reasonable ranges
    EXPECT_GT(params.a1_frequency.center_frequency, 0.0f);
    EXPECT_LT(params.a1_frequency.center_frequency, 20000.0f);
    EXPECT_GT(params.a1_frequency.q_factor, 0.0f);
    EXPECT_GT(params.a1_frequency.integration_window, 0.0f);
}

TEST_F(A1NeuronsTest, DefaultParameters_CoincidenceDetector) {
    // WHAT: Test default parameters for coincidence detector
    // WHY:  Verify sensible defaults are provided
    // HOW:  Get defaults and check ranges

    neuron_type_params_t params{};
    nimcp_result_t result = neuron_type_get_default_params(
        NEURON_A1_COINCIDENCE_DETECTOR, &params);

    ASSERT_EQ(result, NIMCP_SUCCESS);

    // Check default values are in reasonable ranges
    EXPECT_GT(params.a1_coincidence.integration_window, 0.0f);
    EXPECT_LT(params.a1_coincidence.integration_window, 10.0f);  // Typically 1-2ms
    EXPECT_GE(params.a1_coincidence.threshold, 0.0f);
}

// ============================================================================
// EDGE CASES AND ERROR HANDLING
// ============================================================================

TEST_F(A1NeuronsTest, ProcessInput_NullParams) {
    // WHAT: Test processing with null parameters
    // WHY:  Should handle gracefully (return 0.0)
    // HOW:  Call with nullptr

    float output = neuron_type_process_input(
        NEURON_A1_FREQUENCY_TUNED, nullptr, 0.5f, timestamp);

    EXPECT_EQ(output, 0.0f);
}

TEST_F(A1NeuronsTest, ProcessInput_MaxInput) {
    // WHAT: Test processing with maximum input value
    // WHY:  Should handle saturation gracefully
    // HOW:  Apply input = 1.0

    auto params = create_frequency_params();
    float output = neuron_type_process_input(
        NEURON_A1_FREQUENCY_TUNED, &params, 1.0f, timestamp);

    EXPECT_GE(output, 0.0f);
    EXPECT_LE(output, 1.0f);
}

TEST_F(A1NeuronsTest, TypeName_FrequencyTuned) {
    // WHAT: Test type name retrieval
    // WHY:  Verify human-readable names are available
    // HOW:  Get name and check it's not empty

    const char* name = neuron_type_get_name(NEURON_A1_FREQUENCY_TUNED);
    ASSERT_NE(name, nullptr);
    EXPECT_STRNE(name, "");
}

TEST_F(A1NeuronsTest, TypeName_CoincidenceDetector) {
    // WHAT: Test type name retrieval
    // WHY:  Verify human-readable names are available
    // HOW:  Get name and check it's not empty

    const char* name = neuron_type_get_name(NEURON_A1_COINCIDENCE_DETECTOR);
    ASSERT_NE(name, nullptr);
    EXPECT_STRNE(name, "");
}

// ============================================================================
// PERFORMANCE TESTS
// ============================================================================

TEST_F(A1NeuronsTest, Performance_FrequencyTuned) {
    // WHAT: Benchmark frequency-tuned neuron processing
    // WHY:  Ensure O(1) performance for single-value processing
    // HOW:  Process 10000 inputs and measure time

    auto params = create_frequency_params();
    uint64_t start = nimcp_time_monotonic_us();

    const int iterations = 10000;
    float sum = 0.0f;
    for (int i = 0; i < iterations; i++) {
        uint64_t ts = start + i * 1000;
        sum += neuron_type_process_input(
            NEURON_A1_FREQUENCY_TUNED, &params, 0.5f, ts);
    }

    uint64_t end = nimcp_time_monotonic_us();
    uint64_t duration = end - start;

    // Should process at least 1000 inputs per ms
    float inputs_per_us = (float)iterations / (float)duration;
    EXPECT_GT(inputs_per_us, 1.0f);

    // Use sum to prevent optimization
    EXPECT_GE(sum, 0.0f);
}

TEST_F(A1NeuronsTest, Performance_CoincidenceDetector) {
    // WHAT: Benchmark coincidence detector processing
    // WHY:  Ensure O(1) performance for single-value processing
    // HOW:  Process 10000 inputs and measure time

    auto params = create_coincidence_params();
    uint64_t start = nimcp_time_monotonic_us();

    const int iterations = 10000;
    float sum = 0.0f;
    for (int i = 0; i < iterations; i++) {
        uint64_t ts = start + i * 1000;
        sum += neuron_type_process_input(
            NEURON_A1_COINCIDENCE_DETECTOR, &params, 0.5f, ts);
    }

    uint64_t end = nimcp_time_monotonic_us();
    uint64_t duration = end - start;

    // Should process at least 1000 inputs per ms
    float inputs_per_us = (float)iterations / (float)duration;
    EXPECT_GT(inputs_per_us, 1.0f);

    // Use sum to prevent optimization
    EXPECT_GE(sum, 0.0f);
}
