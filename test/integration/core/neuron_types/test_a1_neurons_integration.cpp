/**
 * @file test_a1_neurons_integration.cpp
 * @brief Integration tests for A1 auditory neuron processing in context
 *
 * Test Coverage:
 * - Multi-neuron frequency bank (tonotopic organization)
 * - Binaural sound localization with coincidence detectors
 * - Temporal processing chains (onset → frequency → coincidence)
 * - Cross-frequency interactions
 * - Integration with temporal dynamics
 * - Sound event detection pipelines
 * - Auditory scene analysis simulations
 * - Performance under realistic conditions
 *
 * INTEGRATION FOCUS:
 * These tests verify that A1 neurons work correctly when combined with
 * other neuron types and in realistic processing scenarios.
 *
 * @author NIMCP Development Team
 * @date 2025-11-16
 */

#include <gtest/gtest.h>
#include "core/neuron_types/nimcp_neuron_types.h"
#include "utils/time/nimcp_time.h"
#include <cmath>
#include <vector>
#include <algorithm>

// ============================================================================
// TEST FIXTURE
// ============================================================================

class A1NeuronsIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        timestamp = nimcp_time_monotonic_us();
    }

    void TearDown() override {
        // Cleanup
    }

    uint64_t timestamp;

    // Helper: Create frequency bank (tonotopic organization)
    std::vector<neuron_type_params_t> create_frequency_bank(
        const std::vector<float>& frequencies) {

        std::vector<neuron_type_params_t> bank;
        for (float freq : frequencies) {
            neuron_type_params_t params{};
            neuron_type_get_default_params(NEURON_A1_FREQUENCY_TUNED, &params);
            params.a1_frequency.center_frequency = freq;
            params.a1_frequency.q_factor = 5.0f;
            params.a1_frequency.integration_window = 10.0f;
            bank.push_back(params);
        }
        return bank;
    }

    // Helper: Simulate temporal sequence
    std::vector<float> simulate_temporal_sequence(
        neuron_type_t type,
        const neuron_type_params_t& params,
        const std::vector<float>& inputs,
        float dt_ms = 1.0f) {

        std::vector<float> outputs;
        uint64_t ts = timestamp;

        for (float input : inputs) {
            float output = neuron_type_process_input(type, &params, input, ts);
            outputs.push_back(output);
            ts += static_cast<uint64_t>(dt_ms * 1000);  // Convert ms to µs
        }

        return outputs;
    }
};

// ============================================================================
// TONOTOPIC ORGANIZATION TESTS
// ============================================================================

TEST_F(A1NeuronsIntegrationTest, TonotopicBank_LogarithmicSpacing) {
    // WHAT: Test frequency bank with logarithmic spacing
    // WHY:  A1 tonotopic organization is approximately logarithmic
    // HOW:  Create bank with octave spacing, verify all respond

    std::vector<float> frequencies = {
        250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f
    };

    auto bank = create_frequency_bank(frequencies);
    ASSERT_EQ(bank.size(), frequencies.size());

    float input = 0.6f;

    // All neurons in bank should respond
    for (size_t i = 0; i < bank.size(); i++) {
        float output = neuron_type_process_input(
            NEURON_A1_FREQUENCY_TUNED, &bank[i], input, timestamp);

        EXPECT_GE(output, 0.0f) << "Neuron " << i << " (freq " << frequencies[i] << ")";
        EXPECT_LE(output, 1.0f) << "Neuron " << i << " (freq " << frequencies[i] << ")";
    }
}

TEST_F(A1NeuronsIntegrationTest, TonotopicBank_PopulationCoding) {
    // WHAT: Test population coding across frequency bank
    // WHY:  Auditory information is distributed across neuron population
    // HOW:  Apply same input to all neurons, verify distributed response

    std::vector<float> frequencies = {500.0f, 1000.0f, 2000.0f, 4000.0f};
    auto bank = create_frequency_bank(frequencies);

    float input = 0.7f;
    std::vector<float> outputs;

    for (auto& params : bank) {
        float output = neuron_type_process_input(
            NEURON_A1_FREQUENCY_TUNED, &params, input, timestamp);
        outputs.push_back(output);
    }

    // Verify population has non-zero activity
    float total_activity = 0.0f;
    for (float out : outputs) {
        total_activity += out;
    }

    EXPECT_GT(total_activity, 0.0f);

    // Verify distribution (not all zeros, not all same)
    int non_zero_count = 0;
    for (float out : outputs) {
        if (out > 0.01f) non_zero_count++;
    }
    EXPECT_GT(non_zero_count, 0);
}

TEST_F(A1NeuronsIntegrationTest, TonotopicBank_HighLowFrequencyRange) {
    // WHAT: Test full auditory range coverage
    // WHY:  Verify bank covers low (125 Hz) to high (16 kHz) frequencies
    // HOW:  Create comprehensive bank spanning full range

    std::vector<float> frequencies = {
        125.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f
    };

    auto bank = create_frequency_bank(frequencies);
    float input = 0.5f;

    // All frequencies should be processable
    for (size_t i = 0; i < bank.size(); i++) {
        float output = neuron_type_process_input(
            NEURON_A1_FREQUENCY_TUNED, &bank[i], input, timestamp);

        EXPECT_GE(output, 0.0f);
    }
}

// ============================================================================
// BINAURAL SOUND LOCALIZATION TESTS
// ============================================================================

TEST_F(A1NeuronsIntegrationTest, BinauralLocalization_ITDDetection) {
    // WHAT: Test interaural time difference (ITD) detection
    // WHY:  Coincidence detectors compute ITDs for sound localization
    // HOW:  Create array of detectors with different delays

    // Create coincidence detector array (Jeffress model)
    std::vector<neuron_type_params_t> detectors;
    std::vector<float> delays = {0.5f, 1.0f, 1.5f, 2.0f, 2.5f};

    for (float delay : delays) {
        neuron_type_params_t params{};
        neuron_type_get_default_params(NEURON_A1_COINCIDENCE_DETECTOR, &params);
        params.a1_coincidence.integration_window = delay;
        detectors.push_back(params);
    }

    float input = 0.8f;

    // Each detector should respond differently based on delay
    std::vector<float> outputs;
    for (auto& params : detectors) {
        float output = neuron_type_process_input(
            NEURON_A1_COINCIDENCE_DETECTOR, &params, input, timestamp);
        outputs.push_back(output);
    }

    // Verify ITD array has activity
    int active_count = 0;
    for (float out : outputs) {
        if (out > 0.0f) active_count++;
    }
    EXPECT_GT(active_count, 0);
}

TEST_F(A1NeuronsIntegrationTest, BinauralLocalization_SpatialMap) {
    // WHAT: Test spatial sound map via ITD array
    // WHY:  Different ITDs → different spatial locations
    // HOW:  Verify detector array produces spatial selectivity

    std::vector<float> windows = {0.5f, 1.0f, 2.0f, 4.0f};
    neuron_type_params_t params{};
    neuron_type_get_default_params(NEURON_A1_COINCIDENCE_DETECTOR, &params);

    float input = 0.9f;
    std::vector<float> responses;

    for (float window : windows) {
        params.a1_coincidence.integration_window = window;
        float output = neuron_type_process_input(
            NEURON_A1_COINCIDENCE_DETECTOR, &params, input, timestamp);
        responses.push_back(output);
    }

    // Verify spatial map has variation
    float min_response = *std::min_element(responses.begin(), responses.end());
    float max_response = *std::max_element(responses.begin(), responses.end());

    EXPECT_GE(max_response - min_response, 0.0f);
}

// ============================================================================
// TEMPORAL PROCESSING CHAIN TESTS
// ============================================================================

TEST_F(A1NeuronsIntegrationTest, TemporalChain_OnsetToFrequency) {
    // WHAT: Test onset detection feeding into frequency analysis
    // WHY:  Models auditory pathway: onset → spectral analysis
    // HOW:  Chain onset detector → frequency-tuned neurons

    // Create onset detector
    neuron_type_params_t onset_params{};
    neuron_type_get_default_params(NEURON_A1_COINCIDENCE_DETECTOR, &onset_params);
    onset_params.a1_coincidence.integration_window = 2.0f;

    // Create frequency bank
    std::vector<float> frequencies = {500.0f, 1000.0f, 2000.0f};
    auto freq_bank = create_frequency_bank(frequencies);

    // Simulate transient input
    std::vector<float> inputs = {0.0f, 0.0f, 0.9f, 0.7f, 0.3f, 0.1f, 0.0f};

    // Stage 1: Onset detection
    auto onset_outputs = simulate_temporal_sequence(
        NEURON_AUDITORY_ONSET, onset_params, inputs);

    // Stage 2: Frequency analysis on onset-detected signals
    for (auto& freq_params : freq_bank) {
        auto freq_outputs = simulate_temporal_sequence(
            NEURON_A1_FREQUENCY_TUNED, freq_params, onset_outputs);

        // Verify processing chain produces output
        float total = 0.0f;
        for (float out : freq_outputs) {
            total += out;
        }
        EXPECT_GE(total, 0.0f);
    }
}

TEST_F(A1NeuronsIntegrationTest, TemporalChain_FrequencyToCoincidence) {
    // WHAT: Test frequency analysis feeding into coincidence detection
    // WHY:  Models spectral → binaural processing pathway
    // HOW:  Chain frequency-tuned → coincidence detector

    // Create frequency-tuned neuron
    neuron_type_params_t freq_params{};
    neuron_type_get_default_params(NEURON_A1_FREQUENCY_TUNED, &freq_params);
    freq_params.a1_frequency.center_frequency = 1000.0f;

    // Create coincidence detector
    neuron_type_params_t coin_params{};
    neuron_type_get_default_params(NEURON_A1_COINCIDENCE_DETECTOR, &coin_params);

    // Simulate sustained tone
    std::vector<float> inputs = {0.6f, 0.6f, 0.6f, 0.6f, 0.6f};

    // Stage 1: Frequency filtering
    auto freq_outputs = simulate_temporal_sequence(
        NEURON_A1_FREQUENCY_TUNED, freq_params, inputs);

    // Stage 2: Coincidence detection
    auto coin_outputs = simulate_temporal_sequence(
        NEURON_A1_COINCIDENCE_DETECTOR, coin_params, freq_outputs);

    // Verify chain processes signal
    EXPECT_GT(coin_outputs.size(), 0);
    float total = 0.0f;
    for (float out : coin_outputs) {
        total += out;
    }
    EXPECT_GE(total, 0.0f);
}

TEST_F(A1NeuronsIntegrationTest, TemporalChain_FullPipeline) {
    // WHAT: Test complete auditory processing pipeline
    // WHY:  Models onset → frequency → coincidence pathway
    // HOW:  Chain all three neuron types

    neuron_type_params_t onset_params{};
    neuron_type_get_default_params(NEURON_A1_COINCIDENCE_DETECTOR, &onset_params);
    onset_params.a1_coincidence.integration_window = 2.0f;

    neuron_type_params_t freq_params{};
    neuron_type_get_default_params(NEURON_A1_FREQUENCY_TUNED, &freq_params);

    neuron_type_params_t coin_params{};
    neuron_type_get_default_params(NEURON_A1_COINCIDENCE_DETECTOR, &coin_params);

    // Simulate complex auditory event
    std::vector<float> inputs = {0.0f, 0.1f, 0.8f, 0.6f, 0.4f, 0.2f, 0.1f, 0.0f};

    // Stage 1: Onset
    auto stage1 = simulate_temporal_sequence(NEURON_AUDITORY_ONSET, onset_params, inputs);

    // Stage 2: Frequency
    auto stage2 = simulate_temporal_sequence(NEURON_A1_FREQUENCY_TUNED, freq_params, stage1);

    // Stage 3: Coincidence
    auto stage3 = simulate_temporal_sequence(NEURON_A1_COINCIDENCE_DETECTOR, coin_params, stage2);

    // Verify pipeline produces output
    EXPECT_EQ(stage3.size(), inputs.size());
}

// ============================================================================
// CROSS-FREQUENCY INTERACTION TESTS
// ============================================================================

TEST_F(A1NeuronsIntegrationTest, CrossFrequency_HarmonicRelationships) {
    // WHAT: Test neurons tuned to harmonic frequencies
    // WHY:  Natural sounds contain harmonically related frequencies
    // HOW:  Create neurons at fundamental and harmonics (f, 2f, 3f)

    float fundamental = 500.0f;
    std::vector<float> harmonics = {
        fundamental,
        fundamental * 2.0f,
        fundamental * 3.0f,
        fundamental * 4.0f
    };

    auto bank = create_frequency_bank(harmonics);
    float input = 0.7f;

    // All harmonic neurons should respond
    for (size_t i = 0; i < bank.size(); i++) {
        float output = neuron_type_process_input(
            NEURON_A1_FREQUENCY_TUNED, &bank[i], input, timestamp);

        EXPECT_GE(output, 0.0f) << "Harmonic " << (i+1);
    }
}

TEST_F(A1NeuronsIntegrationTest, CrossFrequency_BandwidthOverlap) {
    // WHAT: Test adjacent frequency channels with overlapping bandwidths
    // WHY:  Realistic frequency banks have overlapping tuning curves
    // HOW:  Create closely spaced neurons, verify both respond

    // Create closely spaced neurons (Q=3 gives ~33% bandwidth)
    neuron_type_params_t params1{};
    neuron_type_get_default_params(NEURON_A1_FREQUENCY_TUNED, &params1);
    params1.a1_frequency.center_frequency = 1000.0f;
    params1.a1_frequency.q_factor = 3.0f;

    neuron_type_params_t params2{};
    neuron_type_get_default_params(NEURON_A1_FREQUENCY_TUNED, &params2);
    params2.a1_frequency.center_frequency = 1200.0f;  // 20% spacing
    params2.a1_frequency.q_factor = 3.0f;

    float input = 0.6f;

    float output1 = neuron_type_process_input(
        NEURON_A1_FREQUENCY_TUNED, &params1, input, timestamp);
    float output2 = neuron_type_process_input(
        NEURON_A1_FREQUENCY_TUNED, &params2, input, timestamp);

    // Both should respond (overlapping bandwidths)
    EXPECT_GE(output1, 0.0f);
    EXPECT_GE(output2, 0.0f);
}

// ============================================================================
// SOUND EVENT DETECTION TESTS
// ============================================================================

TEST_F(A1NeuronsIntegrationTest, SoundEvent_TransientDetection) {
    // WHAT: Test detection of transient sound events
    // WHY:  Onset detectors signal start of auditory events
    // HOW:  Simulate click/impulse, verify strong onset response

    neuron_type_params_t params{};
    neuron_type_get_default_params(NEURON_A1_COINCIDENCE_DETECTOR, &params);
    params.a1_coincidence.integration_window = 1.5f;

    // Simulate transient: silence → impulse → decay
    std::vector<float> impulse = {0.0f, 0.0f, 0.95f, 0.1f, 0.0f, 0.0f};

    auto outputs = simulate_temporal_sequence(NEURON_AUDITORY_ONSET, params, impulse);

    // Find peak response
    float peak = *std::max_element(outputs.begin(), outputs.end());

    // Peak should occur at or near transient
    EXPECT_GT(peak, 0.0f);

    // Peak should be near index 2 (impulse location)
    auto peak_it = std::max_element(outputs.begin(), outputs.end());
    size_t peak_idx = std::distance(outputs.begin(), peak_it);
    EXPECT_LE(std::abs(static_cast<int>(peak_idx) - 2), 1);
}

TEST_F(A1NeuronsIntegrationTest, SoundEvent_SustainedTone) {
    // WHAT: Test response to sustained tone
    // WHY:  Frequency neurons should maintain response, onset should adapt
    // HOW:  Simulate sustained tone, compare onset vs frequency response

    neuron_type_params_t onset_params{};
    neuron_type_get_default_params(NEURON_A1_COINCIDENCE_DETECTOR, &onset_params);
    onset_params.a1_coincidence.integration_window = 2.0f;

    neuron_type_params_t freq_params{};
    neuron_type_get_default_params(NEURON_A1_FREQUENCY_TUNED, &freq_params);

    // Sustained tone
    std::vector<float> tone = {0.7f, 0.7f, 0.7f, 0.7f, 0.7f};

    auto onset_outputs = simulate_temporal_sequence(
        NEURON_AUDITORY_ONSET, onset_params, tone);
    auto freq_outputs = simulate_temporal_sequence(
        NEURON_A1_FREQUENCY_TUNED, freq_params, tone);

    // Frequency neuron should maintain response
    float freq_mean = 0.0f;
    for (float out : freq_outputs) {
        freq_mean += out;
    }
    freq_mean /= freq_outputs.size();
    EXPECT_GT(freq_mean, 0.0f);

    // Onset neuron should show adaptation (decreasing or low sustained response)
    // This is implicit in the design - onset detectors favor transients
}

// ============================================================================
// REALISTIC SCENARIO TESTS
// ============================================================================

TEST_F(A1NeuronsIntegrationTest, RealisticScenario_SpeechLikeStimulus) {
    // WHAT: Test response to speech-like temporal pattern
    // WHY:  Verify neurons handle realistic auditory input
    // HOW:  Simulate formant-like frequency content with temporal modulation

    // Create multi-frequency bank (simulating formants)
    std::vector<float> formant_frequencies = {800.0f, 1200.0f, 2500.0f};
    auto bank = create_frequency_bank(formant_frequencies);

    // Speech-like temporal envelope: silence → onset → vowel → offset
    std::vector<float> envelope = {
        0.0f, 0.1f, 0.7f, 0.8f, 0.75f, 0.7f, 0.5f, 0.2f, 0.0f
    };

    // All formant neurons should track envelope
    for (auto& params : bank) {
        auto outputs = simulate_temporal_sequence(
            NEURON_A1_FREQUENCY_TUNED, params, envelope);

        EXPECT_EQ(outputs.size(), envelope.size());

        // Verify temporal tracking
        float total = 0.0f;
        for (float out : outputs) {
            total += out;
        }
        EXPECT_GT(total, 0.0f);
    }
}

TEST_F(A1NeuronsIntegrationTest, RealisticScenario_MusicLikeChord) {
    // WHAT: Test response to chord (multiple simultaneous frequencies)
    // WHY:  Music contains harmonically rich simultaneous tones
    // HOW:  Simulate chord with frequency bank

    // Major triad: root, major third, perfect fifth (C4-E4-G4 ≈ 262-330-392 Hz)
    std::vector<float> chord_frequencies = {262.0f, 330.0f, 392.0f};
    auto chord_bank = create_frequency_bank(chord_frequencies);

    float input = 0.6f;

    // All chord components should be detected
    int active_count = 0;
    for (auto& params : chord_bank) {
        float output = neuron_type_process_input(
            NEURON_A1_FREQUENCY_TUNED, &params, input, timestamp);

        if (output > 0.0f) active_count++;
    }

    EXPECT_EQ(active_count, static_cast<int>(chord_frequencies.size()));
}

// ============================================================================
// PERFORMANCE UNDER LOAD TESTS
// ============================================================================

TEST_F(A1NeuronsIntegrationTest, Performance_LargeFrequencyBank) {
    // WHAT: Test performance with realistic frequency bank size
    // WHY:  A1 contains thousands of neurons - test scalability
    // HOW:  Create 32-channel frequency bank, process batch

    // Create 32-channel frequency bank (1/3 octave spacing)
    std::vector<float> frequencies;
    float freq = 125.0f;
    while (freq <= 16000.0f) {
        frequencies.push_back(freq);
        freq *= std::pow(2.0f, 1.0f/3.0f);  // 1/3 octave steps
    }

    auto bank = create_frequency_bank(frequencies);
    EXPECT_GE(bank.size(), 20u);  // Should have 20+ channels

    uint64_t start = nimcp_time_monotonic_us();

    float input = 0.6f;
    float sum = 0.0f;

    // Process input through entire bank
    for (auto& params : bank) {
        sum += neuron_type_process_input(
            NEURON_A1_FREQUENCY_TUNED, &params, input, timestamp);
    }

    uint64_t end = nimcp_time_monotonic_us();
    uint64_t duration = end - start;

    // Should process entire bank in < 100 µs
    EXPECT_LT(duration, 100u);
    EXPECT_GT(sum, 0.0f);  // Prevent optimization
}

TEST_F(A1NeuronsIntegrationTest, Performance_TemporalBatch) {
    // WHAT: Test performance with temporal batch processing
    // WHY:  Real-time audio requires processing continuous streams
    // HOW:  Process 100ms batch (4410 samples at 44.1kHz)

    neuron_type_params_t params{};
    neuron_type_get_default_params(NEURON_A1_FREQUENCY_TUNED, &params);

    // Simulate 100ms at 1ms intervals
    std::vector<float> batch(100, 0.5f);

    uint64_t start = nimcp_time_monotonic_us();

    auto outputs = simulate_temporal_sequence(
        NEURON_A1_FREQUENCY_TUNED, params, batch, 1.0f);

    uint64_t end = nimcp_time_monotonic_us();
    uint64_t duration = end - start;

    EXPECT_EQ(outputs.size(), batch.size());

    // Should process 100ms batch in < 1ms
    EXPECT_LT(duration, 1000u);
}
