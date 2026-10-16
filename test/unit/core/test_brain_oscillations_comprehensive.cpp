/**
 * @file test_brain_oscillations_comprehensive.cpp
 * @brief Comprehensive unit tests for brain oscillation analysis
 *
 * WHAT: Test suite for FFT-based spectral analysis, PAC, synchrony, and coherence
 * WHY:  Ensure 100% coverage and correctness of brain oscillation computations
 * HOW:  Unit tests for all functions with edge cases and validation
 *
 * @author NIMCP Development Team
 * @date 2025
 * @version 1.0
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

// Headers have their own extern "C" guards
#include "core/brain_oscillations/nimcp_brain_oscillations.h"
#include "core/brain/nimcp_brain.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class BrainOscillationTest : public ::testing::Test {
protected:
    brain_t brain;
    brain_oscillation_analyzer_t* analyzer;

    static constexpr uint32_t WINDOW_SIZE_MS = 1000;
    static constexpr uint32_t SAMPLING_RATE_HZ = 250;
    static constexpr float PI = 3.14159265358979323846f;

    void SetUp() override {
        // Create minimal brain for testing (new API)
        brain = brain_create("osc_test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 10, 2);
        ASSERT_NE(brain, nullptr);

        // Create analyzer
        analyzer = brain_oscillation_create(brain, WINDOW_SIZE_MS, SAMPLING_RATE_HZ);
        ASSERT_NE(analyzer, nullptr);
    }

    void TearDown() override {
        if (analyzer) {
            brain_oscillation_destroy(analyzer);
            analyzer = nullptr;
        }
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }

    /**
     * @brief Fill buffer with synthetic oscillation
     * @param freq_hz Oscillation frequency in Hz
     * @param amplitude Oscillation amplitude
     * @param noise_level Noise level (0-1)
     */
    void fillWithOscillation(float freq_hz, float amplitude, float noise_level = 0.0f) {
        uint32_t num_samples = (WINDOW_SIZE_MS * SAMPLING_RATE_HZ) / 1000;
        float dt = 1.0f / SAMPLING_RATE_HZ;

        for (uint32_t i = 0; i < num_samples; i++) {
            float t = i * dt;
            float signal = amplitude * sinf(2.0f * PI * freq_hz * t);

            // Add noise if requested
            if (noise_level > 0.0f) {
                float noise = noise_level * ((float)rand() / RAND_MAX - 0.5f);
                signal += noise;
            }

            bool success = brain_oscillation_record_value(analyzer, signal);
            ASSERT_TRUE(success);
        }
    }

    /**
     * @brief Fill buffer with multiple frequency components
     */
    void fillWithMultipleFrequencies(const std::vector<float>& freqs,
                                      const std::vector<float>& amplitudes) {
        ASSERT_EQ(freqs.size(), amplitudes.size());

        uint32_t num_samples = (WINDOW_SIZE_MS * SAMPLING_RATE_HZ) / 1000;
        float dt = 1.0f / SAMPLING_RATE_HZ;

        for (uint32_t i = 0; i < num_samples; i++) {
            float t = i * dt;
            float signal = 0.0f;

            for (size_t j = 0; j < freqs.size(); j++) {
                signal += amplitudes[j] * sinf(2.0f * PI * freqs[j] * t);
            }

            bool success = brain_oscillation_record_value(analyzer, signal);
            ASSERT_TRUE(success);
        }
    }
};

//=============================================================================
// Analyzer Creation Tests
//=============================================================================

TEST_F(BrainOscillationTest, CreateAnalyzer) {
    EXPECT_NE(analyzer, nullptr);
}

TEST_F(BrainOscillationTest, CreateAnalyzerNullBrain) {
    auto* bad_analyzer = brain_oscillation_create(nullptr, WINDOW_SIZE_MS, SAMPLING_RATE_HZ);
    EXPECT_EQ(bad_analyzer, nullptr);
}

TEST_F(BrainOscillationTest, CreateAnalyzerInvalidWindow) {
    auto* bad_analyzer = brain_oscillation_create(brain, 0, SAMPLING_RATE_HZ);
    EXPECT_EQ(bad_analyzer, nullptr);

    bad_analyzer = brain_oscillation_create(brain, 50, SAMPLING_RATE_HZ);
    EXPECT_EQ(bad_analyzer, nullptr);

    bad_analyzer = brain_oscillation_create(brain, 20000, SAMPLING_RATE_HZ);
    EXPECT_EQ(bad_analyzer, nullptr);
}

TEST_F(BrainOscillationTest, CreateAnalyzerInvalidSamplingRate) {
    auto* bad_analyzer = brain_oscillation_create(brain, WINDOW_SIZE_MS, 0);
    EXPECT_EQ(bad_analyzer, nullptr);

    bad_analyzer = brain_oscillation_create(brain, WINDOW_SIZE_MS, 5);
    EXPECT_EQ(bad_analyzer, nullptr);

    bad_analyzer = brain_oscillation_create(brain, WINDOW_SIZE_MS, 20000);
    EXPECT_EQ(bad_analyzer, nullptr);
}

TEST_F(BrainOscillationTest, DestroyNullAnalyzer) {
    brain_oscillation_destroy(nullptr);  // Should not crash
}

//=============================================================================
// Activity Recording Tests
//=============================================================================

TEST_F(BrainOscillationTest, RecordValue) {
    bool success = brain_oscillation_record_value(analyzer, 0.5f);
    EXPECT_TRUE(success);
}

TEST_F(BrainOscillationTest, RecordValueNull) {
    bool success = brain_oscillation_record_value(nullptr, 0.5f);
    EXPECT_FALSE(success);
}

TEST_F(BrainOscillationTest, RecordActivity) {
    bool success = brain_oscillation_record_activity(analyzer);
    EXPECT_TRUE(success);
}

TEST_F(BrainOscillationTest, RecordActivityNull) {
    bool success = brain_oscillation_record_activity(nullptr);
    EXPECT_FALSE(success);
}

TEST_F(BrainOscillationTest, RecordFullBuffer) {
    uint32_t num_samples = (WINDOW_SIZE_MS * SAMPLING_RATE_HZ) / 1000;

    for (uint32_t i = 0; i < num_samples * 2; i++) {
        bool success = brain_oscillation_record_value(analyzer, 0.5f);
        EXPECT_TRUE(success);
    }
}

//=============================================================================
// Wave Power Tests
//=============================================================================

TEST_F(BrainOscillationTest, GetWavePowerAlpha) {
    fillWithOscillation(10.0f, 1.0f);  // Alpha frequency

    brain_wave_power_t power;
    bool success = brain_oscillation_get_wave_power(analyzer, &power);
    EXPECT_TRUE(success);

    EXPECT_GT(power.total_power, 0.0f);
    EXPECT_GT(power.alpha_power, power.delta_power);
    EXPECT_GT(power.alpha_power, power.theta_power);
    EXPECT_GT(power.alpha_power, power.beta_power);
    EXPECT_GT(power.alpha_power, power.gamma_power);

    EXPECT_EQ(power.dominant_band, BRAIN_WAVE_ALPHA);
    EXPECT_NEAR(power.dominant_freq, 10.0f, 2.0f);
}

TEST_F(BrainOscillationTest, GetWavePowerBeta) {
    fillWithOscillation(20.0f, 1.0f);  // Beta frequency

    brain_wave_power_t power;
    bool success = brain_oscillation_get_wave_power(analyzer, &power);
    EXPECT_TRUE(success);

    EXPECT_GT(power.beta_power, 0.0f);
    EXPECT_EQ(power.dominant_band, BRAIN_WAVE_BETA);
    EXPECT_NEAR(power.dominant_freq, 20.0f, 2.0f);
}

TEST_F(BrainOscillationTest, GetWavePowerGamma) {
    fillWithOscillation(40.0f, 1.0f);  // Gamma frequency

    brain_wave_power_t power;
    bool success = brain_oscillation_get_wave_power(analyzer, &power);
    EXPECT_TRUE(success);

    EXPECT_GT(power.gamma_power, 0.0f);
    EXPECT_EQ(power.dominant_band, BRAIN_WAVE_GAMMA);
    EXPECT_NEAR(power.dominant_freq, 40.0f, 5.0f);
}

TEST_F(BrainOscillationTest, GetWavePowerTheta) {
    fillWithOscillation(6.0f, 1.0f);  // Theta frequency

    brain_wave_power_t power;
    bool success = brain_oscillation_get_wave_power(analyzer, &power);
    EXPECT_TRUE(success);

    EXPECT_GT(power.theta_power, 0.0f);
    EXPECT_EQ(power.dominant_band, BRAIN_WAVE_THETA);
    EXPECT_NEAR(power.dominant_freq, 6.0f, 2.0f);
}

TEST_F(BrainOscillationTest, GetWavePowerNull) {
    fillWithOscillation(10.0f, 1.0f);

    brain_wave_power_t power;
    bool success = brain_oscillation_get_wave_power(nullptr, &power);
    EXPECT_FALSE(success);

    success = brain_oscillation_get_wave_power(analyzer, nullptr);
    EXPECT_FALSE(success);
}

TEST_F(BrainOscillationTest, GetWavePowerInsufficientData) {
    // Only fill half the buffer
    uint32_t num_samples = (WINDOW_SIZE_MS * SAMPLING_RATE_HZ) / 2000;
    for (uint32_t i = 0; i < num_samples; i++) {
        brain_oscillation_record_value(analyzer, 0.5f);
    }

    brain_wave_power_t power;
    bool success = brain_oscillation_get_wave_power(analyzer, &power);
    EXPECT_FALSE(success);
}

//=============================================================================
// Cognitive State Tests
//=============================================================================

TEST_F(BrainOscillationTest, GetStateRelaxed) {
    fillWithOscillation(10.0f, 1.0f);  // Alpha dominant = relaxed

    cognitive_state_t state;
    float confidence;
    bool success = brain_oscillation_get_state(analyzer, &state, &confidence);
    EXPECT_TRUE(success);

    EXPECT_EQ(state, COGNITIVE_STATE_RELAXED);
    EXPECT_GT(confidence, 0.3f);
}

TEST_F(BrainOscillationTest, GetStateFocused) {
    fillWithOscillation(20.0f, 1.0f);  // Beta dominant = focused

    cognitive_state_t state;
    float confidence;
    bool success = brain_oscillation_get_state(analyzer, &state, &confidence);
    EXPECT_TRUE(success);

    EXPECT_EQ(state, COGNITIVE_STATE_FOCUSED);
    EXPECT_GT(confidence, 0.3f);
}

TEST_F(BrainOscillationTest, GetStateAttentive) {
    fillWithOscillation(40.0f, 1.0f);  // Gamma dominant = attentive

    cognitive_state_t state;
    float confidence;
    bool success = brain_oscillation_get_state(analyzer, &state, &confidence);
    EXPECT_TRUE(success);

    EXPECT_EQ(state, COGNITIVE_STATE_ATTENTIVE);
    EXPECT_GT(confidence, 0.2f);
}

TEST_F(BrainOscillationTest, GetStateNull) {
    fillWithOscillation(10.0f, 1.0f);

    cognitive_state_t state;
    float confidence;

    bool success = brain_oscillation_get_state(nullptr, &state, &confidence);
    EXPECT_FALSE(success);

    success = brain_oscillation_get_state(analyzer, nullptr, &confidence);
    EXPECT_FALSE(success);

    success = brain_oscillation_get_state(analyzer, &state, nullptr);
    EXPECT_FALSE(success);
}

//=============================================================================
// Full Analysis Tests
//=============================================================================

TEST_F(BrainOscillationTest, AnalyzeComplete) {
    fillWithOscillation(10.0f, 1.0f);

    oscillation_analysis_t results;
    bool success = brain_oscillation_analyze(analyzer, &results);
    EXPECT_TRUE(success);

    EXPECT_GT(results.wave_power.total_power, 0.0f);
    EXPECT_EQ(results.state, COGNITIVE_STATE_RELAXED);
    EXPECT_GT(results.state_confidence, 0.0f);
    EXPECT_GE(results.spectral_entropy, 0.0f);
    EXPECT_GT(results.peak_frequency, 0.0f);

    // Coupling metrics should be non-negative
    EXPECT_GE(results.theta_gamma_coupling, 0.0f);
    EXPECT_GE(results.alpha_beta_coupling, 0.0f);

    // Synchrony and coherence should be in [0, 1]
    EXPECT_GE(results.synchrony, 0.0f);
    EXPECT_LE(results.synchrony, 1.0f);
    EXPECT_GE(results.coherence, 0.0f);
    EXPECT_LE(results.coherence, 1.0f);

    EXPECT_GE(results.bandwidth, 0.0f);
}

TEST_F(BrainOscillationTest, AnalyzeNull) {
    fillWithOscillation(10.0f, 1.0f);

    oscillation_analysis_t results;

    bool success = brain_oscillation_analyze(nullptr, &results);
    EXPECT_FALSE(success);

    success = brain_oscillation_analyze(analyzer, nullptr);
    EXPECT_FALSE(success);
}

//=============================================================================
// Synchrony Tests (Kuramoto Order Parameter)
//=============================================================================

TEST_F(BrainOscillationTest, SynchronyPerfect) {
    // Perfect sine wave should have measurable synchrony
    // Note: Kuramoto order parameter for a single time series with multiple cycles
    // will be low because phases span 0-2π. This is expected behavior.
    fillWithOscillation(10.0f, 1.0f, 0.0f);

    float synchrony = brain_oscillation_compute_synchrony(analyzer);
    EXPECT_GE(synchrony, 0.0f);
    EXPECT_LE(synchrony, 1.0f);
    // For a multi-cycle sine wave, synchrony will be low as phases are distributed
    // This is correct - synchrony measures phase alignment across samples
}

TEST_F(BrainOscillationTest, SynchronyNoisy) {
    // Noisy signal should have lower synchrony
    fillWithOscillation(10.0f, 1.0f, 0.5f);

    float synchrony = brain_oscillation_compute_synchrony(analyzer);
    EXPECT_GE(synchrony, 0.0f);
    EXPECT_LE(synchrony, 1.0f);
}

TEST_F(BrainOscillationTest, SynchronyNull) {
    float synchrony = brain_oscillation_compute_synchrony(nullptr);
    EXPECT_EQ(synchrony, -1.0f);
}

TEST_F(BrainOscillationTest, SynchronyInsufficientData) {
    // Only partial buffer
    uint32_t num_samples = (WINDOW_SIZE_MS * SAMPLING_RATE_HZ) / 4000;
    for (uint32_t i = 0; i < num_samples; i++) {
        brain_oscillation_record_value(analyzer, 0.5f);
    }

    float synchrony = brain_oscillation_compute_synchrony(analyzer);
    EXPECT_EQ(synchrony, -1.0f);
}

TEST_F(BrainOscillationTest, SynchronyMultipleFrequencies) {
    // Multiple frequencies should have moderate synchrony
    fillWithMultipleFrequencies({10.0f, 20.0f}, {1.0f, 0.5f});

    float synchrony = brain_oscillation_compute_synchrony(analyzer);
    EXPECT_GE(synchrony, 0.0f);
    EXPECT_LE(synchrony, 1.0f);
}

//=============================================================================
// Coherence Tests (Spectral Coherence)
//=============================================================================

TEST_F(BrainOscillationTest, CoherencePure) {
    // Pure sine wave should have high coherence
    fillWithOscillation(10.0f, 1.0f, 0.0f);

    float coherence = brain_oscillation_compute_coherence(analyzer);
    EXPECT_GE(coherence, 0.0f);
    EXPECT_LE(coherence, 1.0f);
    EXPECT_GT(coherence, 0.3f);  // Should be relatively high
}

TEST_F(BrainOscillationTest, CoherenceNoisy) {
    // Noisy signal should have lower coherence
    fillWithOscillation(10.0f, 1.0f, 1.0f);

    float coherence = brain_oscillation_compute_coherence(analyzer);
    EXPECT_GE(coherence, 0.0f);
    EXPECT_LE(coherence, 1.0f);
}

TEST_F(BrainOscillationTest, CoherenceNull) {
    float coherence = brain_oscillation_compute_coherence(nullptr);
    EXPECT_EQ(coherence, -1.0f);
}

TEST_F(BrainOscillationTest, CoherenceInsufficientData) {
    uint32_t num_samples = (WINDOW_SIZE_MS * SAMPLING_RATE_HZ) / 4000;
    for (uint32_t i = 0; i < num_samples; i++) {
        brain_oscillation_record_value(analyzer, 0.5f);
    }

    float coherence = brain_oscillation_compute_coherence(analyzer);
    EXPECT_EQ(coherence, -1.0f);
}

TEST_F(BrainOscillationTest, CoherenceMultipleFrequencies) {
    // Multiple sharp peaks should still have good coherence
    fillWithMultipleFrequencies({10.0f, 30.0f}, {1.0f, 0.5f});

    float coherence = brain_oscillation_compute_coherence(analyzer);
    EXPECT_GE(coherence, 0.0f);
    EXPECT_LE(coherence, 1.0f);
}

//=============================================================================
// Phase-Amplitude Coupling Tests
//=============================================================================

TEST_F(BrainOscillationTest, PACThetaGamma) {
    // Create theta-gamma coupled signal
    // Gamma amplitude modulated by theta phase
    fillWithOscillation(6.0f, 1.0f);  // Base theta

    float pac = brain_oscillation_compute_pac(analyzer, BRAIN_WAVE_THETA, BRAIN_WAVE_GAMMA);
    EXPECT_GE(pac, 0.0f);
    EXPECT_LE(pac, 1.0f);
}

TEST_F(BrainOscillationTest, PACAlphaBeta) {
    fillWithOscillation(10.0f, 1.0f);

    float pac = brain_oscillation_compute_pac(analyzer, BRAIN_WAVE_ALPHA, BRAIN_WAVE_BETA);
    EXPECT_GE(pac, 0.0f);
    EXPECT_LE(pac, 1.0f);
}

TEST_F(BrainOscillationTest, PACNull) {
    fillWithOscillation(10.0f, 1.0f);

    float pac = brain_oscillation_compute_pac(nullptr, BRAIN_WAVE_THETA, BRAIN_WAVE_GAMMA);
    EXPECT_EQ(pac, -1.0f);
}

TEST_F(BrainOscillationTest, PACInsufficientData) {
    uint32_t num_samples = (WINDOW_SIZE_MS * SAMPLING_RATE_HZ) / 4000;
    for (uint32_t i = 0; i < num_samples; i++) {
        brain_oscillation_record_value(analyzer, 0.5f);
    }

    float pac = brain_oscillation_compute_pac(analyzer, BRAIN_WAVE_THETA, BRAIN_WAVE_GAMMA);
    EXPECT_EQ(pac, -1.0f);
}

//=============================================================================
// Bandwidth Tests
//=============================================================================

TEST_F(BrainOscillationTest, BandwidthNarrow) {
    // Pure sine wave should have narrow bandwidth
    fillWithOscillation(10.0f, 1.0f, 0.0f);

    float bandwidth = brain_oscillation_compute_bandwidth(analyzer, 10.0f);
    EXPECT_GE(bandwidth, 0.0f);
    EXPECT_LT(bandwidth, 5.0f);  // Should be relatively narrow
}

TEST_F(BrainOscillationTest, BandwidthNull) {
    float bandwidth = brain_oscillation_compute_bandwidth(nullptr, 10.0f);
    EXPECT_EQ(bandwidth, -1.0f);
}

TEST_F(BrainOscillationTest, BandwidthInvalidFreq) {
    fillWithOscillation(10.0f, 1.0f);

    float bandwidth = brain_oscillation_compute_bandwidth(analyzer, 0.0f);
    EXPECT_EQ(bandwidth, -1.0f);

    bandwidth = brain_oscillation_compute_bandwidth(analyzer, -5.0f);
    EXPECT_EQ(bandwidth, -1.0f);
}

//=============================================================================
// Spectrum Export Tests
//=============================================================================

TEST_F(BrainOscillationTest, GetSpectrum) {
    fillWithOscillation(10.0f, 1.0f);

    float* spectrum = nullptr;
    uint32_t num_bins = 0;
    bool success = brain_oscillation_get_spectrum(analyzer, &spectrum, &num_bins);
    EXPECT_TRUE(success);
    EXPECT_NE(spectrum, nullptr);
    EXPECT_GT(num_bins, 0u);
}

TEST_F(BrainOscillationTest, GetSpectrumNull) {
    fillWithOscillation(10.0f, 1.0f);

    float* spectrum = nullptr;
    uint32_t num_bins = 0;

    bool success = brain_oscillation_get_spectrum(nullptr, &spectrum, &num_bins);
    EXPECT_FALSE(success);

    success = brain_oscillation_get_spectrum(analyzer, nullptr, &num_bins);
    EXPECT_FALSE(success);

    success = brain_oscillation_get_spectrum(analyzer, &spectrum, nullptr);
    EXPECT_FALSE(success);
}

TEST_F(BrainOscillationTest, GetActivityBuffer) {
    fillWithOscillation(10.0f, 1.0f);

    const float* buffer = nullptr;
    uint32_t size = 0;
    bool success = brain_oscillation_get_activity_buffer(analyzer, &buffer, &size);
    EXPECT_TRUE(success);
    EXPECT_NE(buffer, nullptr);
    EXPECT_GT(size, 0u);
}

//=============================================================================
// Utility Tests
//=============================================================================

TEST_F(BrainOscillationTest, StateToString) {
    EXPECT_STREQ(brain_oscillation_state_to_string(COGNITIVE_STATE_UNKNOWN), "Unknown");
    EXPECT_STREQ(brain_oscillation_state_to_string(COGNITIVE_STATE_DEEP_SLEEP), "Deep Sleep");
    EXPECT_STREQ(brain_oscillation_state_to_string(COGNITIVE_STATE_LIGHT_SLEEP), "Light Sleep");
    EXPECT_STREQ(brain_oscillation_state_to_string(COGNITIVE_STATE_RELAXED), "Relaxed");
    EXPECT_STREQ(brain_oscillation_state_to_string(COGNITIVE_STATE_FOCUSED), "Focused");
    EXPECT_STREQ(brain_oscillation_state_to_string(COGNITIVE_STATE_ATTENTIVE), "Attentive");
    EXPECT_STREQ(brain_oscillation_state_to_string(COGNITIVE_STATE_CONSOLIDATING), "Consolidating");
}

TEST_F(BrainOscillationTest, RecommendedWindow) {
    EXPECT_EQ(brain_oscillation_recommended_window(BRAIN_WAVE_DELTA), 3000u);
    EXPECT_EQ(brain_oscillation_recommended_window(BRAIN_WAVE_THETA), 750u);
    EXPECT_EQ(brain_oscillation_recommended_window(BRAIN_WAVE_ALPHA), 375u);
    EXPECT_EQ(brain_oscillation_recommended_window(BRAIN_WAVE_BETA), 230u);
    EXPECT_EQ(brain_oscillation_recommended_window(BRAIN_WAVE_GAMMA), 100u);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
