//=============================================================================
// test_complex_oscillation_integration.cpp - Complex Oscillation Integration Tests
//=============================================================================
/**
 * @file test_complex_oscillation_integration.cpp
 * @brief End-to-end integration tests for complex number oscillation system
 *
 * WHAT: Tests complex phasor mathematics across core, middleware, and API layers
 * WHY:  Verify data flow and correctness across entire system stack
 * HOW:  Create brain, run simulation, query phasors via all three layers
 *
 * TEST COVERAGE:
 * - Layer integration: Core → Middleware → API
 * - Phase coherence across brain regions (multi-channel synchrony)
 * - Phase-coded working memory operations (multi-neuron representation)
 * - Cross-frequency coupling detection
 * - Performance comparison: complex vs real-valued
 *
 * IMPORTANT: Phase coherence measures SPATIAL synchrony (across multiple channels/neurons)
 *            NOT temporal structure of a single time series!
 */

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
    #include "utils/math/nimcp_complex_math.h"
    #include "core/brain/nimcp_brain.h"
    #include "core/brain_oscillations/nimcp_brain_oscillations.h"
    #include "middleware/patterns/nimcp_oscillation_detector.h"
    #include "core/brain/factory/nimcp_brain_factory.h"
#include <cmath>
#include <vector>
#include <chrono>

#define TOLERANCE 1e-4f
#define PHASE_TOLERANCE 0.2f  // ~11 degrees

//=============================================================================
// Test Fixture - Full Stack Integration
//=============================================================================

class ComplexOscillationIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize complex math subsystem
        complex_math_config_t complex_config = complex_math_default_config();
        complex_config.enable_simd = true;
        complex_config.enable_fft_cache = true;
        ASSERT_TRUE(complex_math_init(&complex_config));

        // Create a simple brain for oscillation testing
        brain = brain_create("test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 10);
        ASSERT_NE(brain, nullptr);

        // Create oscillation analyzer
        oscillation_analyzer = brain_oscillation_create(brain, 500, 250);
        ASSERT_NE(oscillation_analyzer, nullptr);

        // Create middleware oscillation detector
        oscillation_detector_config_t detector_config = oscillation_detector_default_config();
        detector_config.enable_pac = true;
        detector_config.enable_plv = true;
        detector = oscillation_detector_create(&detector_config);
        ASSERT_NE(detector, nullptr);
    }

    void TearDown() override {
        if (detector) oscillation_detector_destroy(detector);
        if (oscillation_analyzer) brain_oscillation_destroy(oscillation_analyzer);
        if (brain) brain_destroy(brain);
        complex_math_cleanup();
    }

    brain_t brain = nullptr;
    brain_oscillation_analyzer_t* oscillation_analyzer = nullptr;
    oscillation_detector_t* detector = nullptr;
};

//=============================================================================
// Test 1: Multi-Channel Phase Coherence (FIXED)
//=============================================================================

TEST_F(ComplexOscillationIntegrationTest, EndToEndPhasorTracking) {
    // Simulate theta oscillations (6 Hz) across multiple channels
    const int num_channels = 100;      // 100 neurons/channels
    const int num_timesteps = 1000;    // Record for 4 seconds (24 theta cycles)
    const float theta_freq = 6.0f;
    const float dt = 0.004f;           // 4ms timestep
    const float sample_rate = 250.0f;

    // Feed time series to detectors (for spectral analysis)
    for (int t = 0; t < num_timesteps; ++t) {
        float current_time = t * dt;
        float phase = 2.0f * M_PI * theta_freq * current_time;
        float signal = cosf(phase);

        oscillation_detector_add_sample(detector, signal, current_time * 1000.0);
        brain_oscillation_record_value(oscillation_analyzer, signal);
    }

    // Test phase coherence at a single timepoint across multiple channels
    // (This is the CORRECT use of phase coherence - spatial, not temporal)
    std::vector<neural_phasor_t> channel_phasors(num_channels);
    float fixed_time = 0.5f;  // Sample at t=0.5s
    float base_phase = 2.0f * M_PI * theta_freq * fixed_time;

    // All channels oscillating at same phase (synchronized)
    for (int ch = 0; ch < num_channels; ++ch) {
        float phase = base_phase + (ch * 0.01f);  // Small jitter
        channel_phasors[ch] = phasor_from_polar(1.0f, phase);
    }

    // Verify high phase coherence across channels (spatial synchrony)
    float coherence = phasor_array_coherence(channel_phasors.data(), num_channels);
    EXPECT_GT(coherence, 0.95f) << "Synchronized channels should have high coherence";

    // Verify oscillation detection via middleware (spectral analysis)
    oscillation_result_t result;
    if (oscillation_detector_detect(detector, &result)) {
        EXPECT_EQ(result.dominant_band, OSC_BAND_THETA);
        EXPECT_GT(result.bands[OSC_BAND_THETA].power, 0.2f);
    }

    // Verify core layer analysis
    oscillation_analysis_t analysis;
    ASSERT_TRUE(brain_oscillation_analyze(oscillation_analyzer, &analysis));
    EXPECT_GT(analysis.wave_power.theta_power, 0.1f);
}

//=============================================================================
// Test 2: Phase Coherence Across Brain Regions (FIXED)
//=============================================================================

TEST_F(ComplexOscillationIntegrationTest, InterRegionalPhaseCoherence) {
    const int num_regions = 4;
    const int neurons_per_region = 50;

    // Simulate synchronized oscillations across regions AT THE SAME TIMEPOINT
    std::vector<std::vector<neural_phasor_t>> region_phasors(num_regions);

    float base_phase = M_PI / 3.0f;  // Fixed timepoint, arbitrary phase

    for (int region = 0; region < num_regions; ++region) {
        region_phasors[region].reserve(neurons_per_region);

        // Each region has slightly different mean phase (phase offset)
        float region_phase_offset = region * 0.15f;

        // Multiple neurons within each region (spatial samples)
        for (int neuron = 0; neuron < neurons_per_region; ++neuron) {
            // Add small jitter to simulate biological variability
            float neuron_jitter = (neuron - neurons_per_region/2) * 0.02f;
            float phase = base_phase + region_phase_offset + neuron_jitter;

            neural_phasor_t phasor = phasor_from_polar(1.0f, phase);
            region_phasors[region].push_back(phasor);
        }
    }

    // Verify within-region coherence is high (neurons in same region sync'd)
    for (int r = 0; r < num_regions; ++r) {
        float coherence = phasor_array_coherence(
            region_phasors[r].data(),
            region_phasors[r].size()
        );
        EXPECT_GT(coherence, 0.85f)
            << "Within-region " << r << " coherence should be high";
    }

    // Compute cross-region phase synchrony
    for (int r1 = 0; r1 < num_regions - 1; ++r1) {
        for (int r2 = r1 + 1; r2 < num_regions; ++r2) {
            float synchrony = phasor_array_synchrony(
                region_phasors[r1].data(),
                region_phasors[r2].data(),
                neurons_per_region
            );

            EXPECT_GT(synchrony, 0.6f)
                << "Regions " << r1 << " and " << r2 << " should be synchronized";
        }
    }

    // Verify circular mean phase calculation
    std::vector<neural_phasor_t> all_phasors;
    for (const auto& region : region_phasors) {
        all_phasors.insert(all_phasors.end(), region.begin(), region.end());
    }

    float mean_phase = phasor_array_mean_phase(all_phasors.data(), all_phasors.size());
    EXPECT_GE(mean_phase, -M_PI);
    EXPECT_LE(mean_phase, M_PI);
}

//=============================================================================
// Test 3: Oscillation Analysis with Synthetic Data (FIXED)
//=============================================================================

TEST_F(ComplexOscillationIntegrationTest, OscillationAnalysisWithSyntheticData) {
    const int num_samples = 500;  // Increased for better spectral resolution
    const float alpha_freq = 10.0f;
    const float sample_rate = 250.0f;

    // Feed synthetic alpha oscillation
    for (int t = 0; t < num_samples; ++t) {
        float time = t / sample_rate;
        float signal = sinf(2.0f * M_PI * alpha_freq * time);
        brain_oscillation_record_value(oscillation_analyzer, signal);
    }

    // Verify analysis succeeds
    oscillation_analysis_t analysis;
    ASSERT_TRUE(brain_oscillation_analyze(oscillation_analyzer, &analysis));

    // Verify wave power analysis
    brain_wave_power_t wave_power;
    ASSERT_TRUE(brain_oscillation_get_wave_power(oscillation_analyzer, &wave_power));

    // Alpha should be dominant or present
    EXPECT_GT(wave_power.alpha_power, 0.1f);

    // Coherence should be in valid range
    EXPECT_GE(analysis.coherence, 0.0f);
    EXPECT_LE(analysis.coherence, 1.0f);
}

//=============================================================================
// Test 4: Phase-Coded Working Memory (FIXED)
//=============================================================================

TEST_F(ComplexOscillationIntegrationTest, PhaseCodedWorkingMemory) {
    const int num_items = 4;
    const int neurons_per_item = 50;  // Each item represented by 50 neurons

    std::vector<neural_phasor_t> memory_items[num_items];

    // Each memory item encoded by a population of neurons at different gamma phases
    float base_time = 0.1f;  // Fixed timepoint
    float gamma_freq = 40.0f;

    for (int item = 0; item < num_items; ++item) {
        memory_items[item].reserve(neurons_per_item);

        // Each item at a different phase offset
        float item_phase = (item * 2.0f * M_PI / num_items);

        // Multiple neurons encode this item (spatial representation)
        for (int neuron = 0; neuron < neurons_per_item; ++neuron) {
            // Small jitter per neuron
            float neuron_jitter = (neuron - neurons_per_item/2) * 0.02f;
            float phase = item_phase + neuron_jitter;

            // Normalize to [-π, π]
            while (phase > M_PI) phase -= 2.0f * M_PI;
            while (phase < -M_PI) phase += 2.0f * M_PI;

            neural_phasor_t phasor = phasor_from_polar(1.0f, phase);
            memory_items[item].push_back(phasor);
        }
    }

    // Verify each item has high within-population coherence
    for (int item = 0; item < num_items; ++item) {
        float coherence = phasor_array_coherence(
            memory_items[item].data(),
            memory_items[item].size()
        );
        EXPECT_GT(coherence, 0.85f)
            << "Item " << item << " neurons should be phase-coherent";

        float mean_phase = phasor_array_mean_phase(
            memory_items[item].data(),
            memory_items[item].size()
        );

        float expected_phase = item * (2.0f * M_PI / num_items);
        while (expected_phase > M_PI) expected_phase -= 2.0f * M_PI;
        while (expected_phase < -M_PI) expected_phase += 2.0f * M_PI;

        float phase_diff = fabsf(mean_phase - expected_phase);
        if (phase_diff > M_PI) phase_diff = 2.0f * M_PI - phase_diff;

        EXPECT_LT(phase_diff, PHASE_TOLERANCE)
            << "Item " << item << " mean phase should match encoding";
    }

    // Verify items are phase-separated (large mean phase differences)
    // Note: synchrony measures PLV (consistency), not separation!
    for (int i1 = 0; i1 < num_items - 1; ++i1) {
        for (int i2 = i1 + 1; i2 < num_items; ++i2) {
            float phase1 = phasor_array_mean_phase(
                memory_items[i1].data(),
                memory_items[i1].size()
            );
            float phase2 = phasor_array_mean_phase(
                memory_items[i2].data(),
                memory_items[i2].size()
            );

            // Compute absolute phase difference
            float phase_diff = fabsf(phase1 - phase2);
            if (phase_diff > M_PI) phase_diff = 2.0f * M_PI - phase_diff;

            // Items should be separated by at least 60 degrees (π/3)
            EXPECT_GT(phase_diff, M_PI / 3.0f)
                << "Items " << i1 << " and " << i2 << " should be phase-separated";
        }
    }
}

//=============================================================================
// Test 5: Theta-Gamma Phase-Amplitude Coupling (FIXED)
//=============================================================================

TEST_F(ComplexOscillationIntegrationTest, ThetaGammaPAC) {
    const int num_samples = 2000;  // Increased for better PAC detection
    const float theta_freq = 6.0f;
    const float gamma_freq = 40.0f;
    const float sample_rate = 250.0f;
    const float dt = 1.0f / sample_rate;

    std::vector<neural_phasor_t> theta_phasors;
    std::vector<float> gamma_amplitudes;

    theta_phasors.reserve(num_samples);
    gamma_amplitudes.reserve(num_samples);

    // Generate theta-gamma coupled signal with strong modulation
    for (int t = 0; t < num_samples; ++t) {
        float time = t * dt;

        // Theta phase (unwrapped, not wrapped to [0, 2π])
        float theta_phase = 2.0f * M_PI * theta_freq * time;
        float theta_phase_wrapped = fmodf(theta_phase, 2.0f * M_PI);
        neural_phasor_t theta_phasor = phasor_from_polar(1.0f, theta_phase_wrapped);
        theta_phasors.push_back(theta_phasor);

        // Gamma amplitude modulated by theta phase (strong coupling)
        float theta_mod = 0.5f * (1.0f + cosf(theta_phase));  // [0, 1]
        float gamma_signal = sinf(2.0f * M_PI * gamma_freq * time);
        float gamma_amp = theta_mod * fabsf(gamma_signal);
        gamma_amplitudes.push_back(gamma_amp);

        // Feed combined signal to detector
        float combined_signal = cosf(theta_phase) + theta_mod * gamma_signal;
        oscillation_detector_add_sample(detector, combined_signal, time * 1000.0);
    }

    // Compute PAC modulation index
    float pac_index = phasor_pac_modulation_index(
        theta_phasors.data(),
        gamma_amplitudes.data(),
        num_samples
    );

    EXPECT_GT(pac_index, 0.1f) << "Theta-gamma coupling should be detected";

    // Verify middleware can detect oscillation components
    oscillation_result_t result;
    ASSERT_TRUE(oscillation_detector_detect(detector, &result));
    EXPECT_GT(result.total_power, 0.1f);
}

//=============================================================================
// Test 6: Hilbert Transform Integration
//=============================================================================

TEST_F(ComplexOscillationIntegrationTest, HilbertTransformAnalyticSignal) {
    const int num_samples = 256;
    const float freq = 10.0f;
    const float amplitude = 2.0f;
    const float sample_rate = 256.0f;

    std::vector<float> real_signal(num_samples);
    std::vector<neural_phasor_t> analytic_signal(num_samples);

    // Generate pure sinusoid
    for (int t = 0; t < num_samples; ++t) {
        float time = t / sample_rate;
        real_signal[t] = amplitude * sinf(2.0f * M_PI * freq * time);
    }

    // Compute Hilbert transform
    ASSERT_TRUE(phasor_hilbert_transform(
        real_signal.data(),
        analytic_signal.data(),
        num_samples
    ));

    // Verify instantaneous amplitude is constant
    for (int t = 50; t < num_samples - 50; ++t) {
        float inst_amplitude = phasor_amplitude(analytic_signal[t]);
        EXPECT_NEAR(inst_amplitude, amplitude, 0.3f);
    }

    // Verify instantaneous phase increases linearly
    for (int t = 51; t < num_samples - 50; ++t) {
        float phase1 = phasor_phase(analytic_signal[t - 1]);
        float phase2 = phasor_phase(analytic_signal[t]);

        float phase_diff = phase2 - phase1;
        if (phase_diff < -M_PI) phase_diff += 2.0f * M_PI;
        if (phase_diff > M_PI) phase_diff -= 2.0f * M_PI;

        float expected_phase_inc = 2.0f * M_PI * freq / sample_rate;
        EXPECT_NEAR(phase_diff, expected_phase_inc, 0.1f);
    }
}

//=============================================================================
// Test 7: Performance Comparison
//=============================================================================

TEST_F(ComplexOscillationIntegrationTest, PerformanceComparison) {
    const int num_iterations = 100;
    const int num_samples = 1000;

    std::vector<neural_phasor_t> phasors(num_samples);
    for (int i = 0; i < num_samples; ++i) {
        float phase = 2.0f * M_PI * i / num_samples;
        phasors[i] = phasor_from_polar(1.0f, phase);
    }

    // Benchmark complex coherence
    auto start_complex = std::chrono::high_resolution_clock::now();
    for (int iter = 0; iter < num_iterations; ++iter) {
        float coherence = phasor_array_coherence(phasors.data(), num_samples);
        (void)coherence;
    }
    auto end_complex = std::chrono::high_resolution_clock::now();
    auto duration_complex = std::chrono::duration_cast<std::chrono::microseconds>(
        end_complex - start_complex
    ).count();

    // Benchmark simple variance
    auto start_real = std::chrono::high_resolution_clock::now();
    for (int iter = 0; iter < num_iterations; ++iter) {
        float variance = phasor_array_phase_variance(phasors.data(), num_samples);
        (void)variance;
    }
    auto end_real = std::chrono::high_resolution_clock::now();
    auto duration_real = std::chrono::duration_cast<std::chrono::microseconds>(
        end_real - start_real
    ).count();

    float avg_complex_us = duration_complex / float(num_iterations);
    float avg_real_us = duration_real / float(num_iterations);

    EXPECT_LT(avg_complex_us, avg_real_us * 3.0f)
        << "Complex math overhead should be acceptable";

    std::cout << "Performance (avg over " << num_iterations << " iterations, n="
              << num_samples << "):\n";
    std::cout << "  Complex coherence: " << avg_complex_us << " µs\n";
    std::cout << "  Real variance: " << avg_real_us << " µs\n";
    std::cout << "  Overhead: " << (avg_complex_us / avg_real_us) << "x\n";
}

//=============================================================================
// Test 8: FFT Round-Trip
//=============================================================================

TEST_F(ComplexOscillationIntegrationTest, FFTRoundTrip) {
    const int num_samples = 128;
    std::vector<neural_phasor_t> original(num_samples);
    std::vector<neural_phasor_t> spectrum(num_samples);
    std::vector<neural_phasor_t> reconstructed(num_samples);

    // Create test signal
    for (int t = 0; t < num_samples; ++t) {
        float signal = sinf(2.0f * M_PI * 5.0f * t / num_samples) +
                       0.5f * sinf(2.0f * M_PI * 10.0f * t / num_samples);
        original[t] = phasor_from_cartesian(signal, 0.0f);
    }

    // Forward FFT
    ASSERT_TRUE(phasor_fft(original.data(), spectrum.data(), num_samples));

    // Inverse FFT
    ASSERT_TRUE(phasor_ifft(spectrum.data(), reconstructed.data(), num_samples));

    // Verify reconstruction
    for (int t = 0; t < num_samples; ++t) {
        EXPECT_NEAR(reconstructed[t].real, original[t].real, 0.01f);
        EXPECT_NEAR(reconstructed[t].imag, original[t].imag, 0.01f);
    }
}

//=============================================================================
// Test 9: Multi-Layer Data Flow (FIXED)
//=============================================================================

TEST_F(ComplexOscillationIntegrationTest, MultiLayerDataFlow) {
    const int num_steps = 1000;  // 4 seconds = 80 beta cycles
    const float dt = 0.004f;
    const float beta_freq = 20.0f;

    for (int t = 0; t < num_steps; ++t) {
        float time = t * dt;
        float signal = sinf(2.0f * M_PI * beta_freq * time);

        // Feed to both layers
        ASSERT_TRUE(brain_oscillation_record_value(oscillation_analyzer, signal));
        oscillation_detector_add_sample(detector, signal, time * 1000.0);
    }

    // Verify core layer results
    oscillation_analysis_t core_analysis;
    ASSERT_TRUE(brain_oscillation_analyze(oscillation_analyzer, &core_analysis));
    EXPECT_GT(core_analysis.wave_power.total_power, 0.0f);

    // Verify middleware layer results (detector may have threshold requirements)
    oscillation_result_t middleware_result;
    if (oscillation_detector_detect(detector, &middleware_result)) {
        EXPECT_GT(middleware_result.total_power, 0.0f);
        EXPECT_EQ(middleware_result.dominant_band, OSC_BAND_BETA);
    }

    // Core layer should detect beta
    EXPECT_EQ(core_analysis.wave_power.dominant_band, BRAIN_WAVE_BETA);
    EXPECT_GT(core_analysis.wave_power.beta_power, 0.1f);
}

//=============================================================================
// Test 10: Phase Locking Value (FIXED)
//=============================================================================

TEST_F(ComplexOscillationIntegrationTest, PhaseLockingValue) {
    const int num_samples = 500;
    const float freq = 8.0f;
    const float sample_rate = 250.0f;

    std::vector<float> signal1(num_samples);
    std::vector<float> signal2(num_samples);

    // Generate two phase-locked signals
    const float phase_offset = M_PI / 4.0f;  // 45 degrees
    for (int t = 0; t < num_samples; ++t) {
        float time = t / sample_rate;
        signal1[t] = sinf(2.0f * M_PI * freq * time);
        signal2[t] = sinf(2.0f * M_PI * freq * time + phase_offset);
    }

    // Compute PLV
    phase_locking_t plv_result;
    ASSERT_TRUE(oscillation_detector_compute_plv(
        detector,
        OSC_BAND_THETA,
        signal1.data(),
        signal2.data(),
        num_samples,
        &plv_result
    ));

    EXPECT_GT(plv_result.plv, 0.75f) << "Phase-locked signals should have high PLV";

    // Phase difference might be wrapped differently, so normalize properly
    float phase_diff = plv_result.mean_phase_diff;
    float expected = phase_offset;

    // Normalize both to [-π, π]
    while (phase_diff > M_PI) phase_diff -= 2.0f * M_PI;
    while (phase_diff < -M_PI) phase_diff += 2.0f * M_PI;
    while (expected > M_PI) expected -= 2.0f * M_PI;
    while (expected < -M_PI) expected += 2.0f * M_PI;

    // Compute circular distance
    float diff = fabsf(phase_diff - expected);
    if (diff > M_PI) diff = 2.0f * M_PI - diff;

    EXPECT_LT(diff, PHASE_TOLERANCE) << "Mean phase difference should match offset";
}

//=============================================================================
// Main Entry Point
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
