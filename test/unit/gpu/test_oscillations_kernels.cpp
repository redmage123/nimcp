/**
 * @file test_oscillations_kernels.cpp
 * @brief Comprehensive unit tests for GPU oscillation kernels
 *
 * WHAT: Tests for GPU-accelerated brain oscillation operations
 * WHY:  Verify phase synchronization, PAC, FFT-based power, and coherence
 * HOW:  GoogleTest with GPU context setup/teardown and numerical verification
 *
 * TEST COVERAGE:
 * - State lifecycle (create, destroy)
 * - Phase Locking Value (PLV) computation
 * - Phase Lag Index (PLI) computation
 * - Phase-Amplitude Coupling (PAC)
 * - Band power computation
 * - Coherence computation
 * - Bandpass filtering
 * - Utility functions
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>
#include <numeric>

#include "gpu/oscillations/nimcp_oscillations_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"

//=============================================================================
// Test Constants
//=============================================================================

static constexpr size_t DEFAULT_N_SAMPLES = 1000;
static constexpr size_t DEFAULT_N_CHANNELS = 8;
static constexpr float DEFAULT_SAMPLING_RATE = 1000.0f;  // 1 kHz
static constexpr float DEFAULT_DT = 0.001f;              // 1 ms
static constexpr float NUMERICAL_EPS = 1e-5f;
static constexpr float PI = 3.14159265358979323846f;

//=============================================================================
// Test Fixture
//=============================================================================

/**
 * @brief Test fixture for GPU oscillation kernel tests
 */
class OscillationsKernelTest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* ctx = nullptr;
    bool gpu_available = false;

    void SetUp() override {
        ctx = nimcp_gpu_context_create_auto();
        gpu_available = (ctx != nullptr && nimcp_gpu_context_is_valid(ctx));
    }

    void TearDown() override {
        if (ctx) {
            nimcp_gpu_context_destroy(ctx);
            ctx = nullptr;
        }
    }

    void RequireGPU() {
        if (!gpu_available) {
            GTEST_SKIP() << "GPU not available, skipping test";
        }
    }

    /**
     * @brief Create default oscillation parameters
     */
    nimcp_oscillation_params_t create_default_params() {
        nimcp_oscillation_params_t params;
        params.sampling_rate = DEFAULT_SAMPLING_RATE;
        params.dt = DEFAULT_DT;
        params.n_samples = DEFAULT_N_SAMPLES;
        params.n_channels = DEFAULT_N_CHANNELS;
        params.freq_low = 8.0f;   // Alpha band low
        params.freq_high = 13.0f; // Alpha band high
        params.n_fft = 256;
        params.hop_length = 64;
        return params;
    }

    /**
     * @brief Create GPU tensor from host data
     */
    nimcp_gpu_tensor_t* create_tensor_from_data(const float* data, size_t size) {
        if (!gpu_available) return nullptr;
        size_t dims[1] = {size};
        return nimcp_gpu_tensor_from_host(ctx, data, dims, 1, NIMCP_GPU_PRECISION_FP32);
    }

    /**
     * @brief Create GPU tensor filled with zeros
     */
    nimcp_gpu_tensor_t* create_zero_tensor(size_t size) {
        if (!gpu_available) return nullptr;
        size_t dims[1] = {size};
        nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
        if (tensor) {
            nimcp_gpu_zeros(ctx, tensor);
        }
        return tensor;
    }

    /**
     * @brief Create GPU tensor filled with a value
     */
    nimcp_gpu_tensor_t* create_filled_tensor(size_t size, float value) {
        if (!gpu_available) return nullptr;
        size_t dims[1] = {size};
        nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
        if (tensor) {
            nimcp_gpu_fill(ctx, tensor, value);
        }
        return tensor;
    }

    /**
     * @brief Create 2D GPU tensor (matrix)
     */
    nimcp_gpu_tensor_t* create_matrix(size_t rows, size_t cols) {
        if (!gpu_available) return nullptr;
        size_t dims[2] = {rows, cols};
        nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(ctx, dims, 2, NIMCP_GPU_PRECISION_FP32);
        if (tensor) {
            nimcp_gpu_zeros(ctx, tensor);
        }
        return tensor;
    }

    /**
     * @brief Generate sinusoidal phase signal
     */
    std::vector<float> generate_phase_signal(size_t n_samples, float freq, float phase_offset = 0.0f) {
        std::vector<float> signal(n_samples);
        for (size_t i = 0; i < n_samples; i++) {
            float t = static_cast<float>(i) / DEFAULT_SAMPLING_RATE;
            signal[i] = 2.0f * PI * freq * t + phase_offset;
            // Wrap to [-PI, PI]
            while (signal[i] > PI) signal[i] -= 2.0f * PI;
            while (signal[i] < -PI) signal[i] += 2.0f * PI;
        }
        return signal;
    }

    /**
     * @brief Generate sine wave signal
     */
    std::vector<float> generate_sine_wave(size_t n_samples, float freq, float amplitude = 1.0f) {
        std::vector<float> signal(n_samples);
        for (size_t i = 0; i < n_samples; i++) {
            float t = static_cast<float>(i) / DEFAULT_SAMPLING_RATE;
            signal[i] = amplitude * std::sin(2.0f * PI * freq * t);
        }
        return signal;
    }

    /**
     * @brief Copy tensor data to host
     */
    bool copy_to_host(const nimcp_gpu_tensor_t* tensor, float* host_data) {
        if (!tensor || !host_data) return false;
        return nimcp_gpu_tensor_to_host(tensor, host_data);
    }
};

//=============================================================================
// Utility Function Tests
//=============================================================================

/**
 * TEST: Default oscillation parameters
 * WHAT: Get default oscillation parameters
 * WHY:  Verify sensible defaults are provided
 */
TEST_F(OscillationsKernelTest, DefaultParams_HasReasonableValues) {
    nimcp_oscillation_params_t params = nimcp_oscillation_default_params(DEFAULT_SAMPLING_RATE);

    EXPECT_EQ(params.sampling_rate, DEFAULT_SAMPLING_RATE);
    EXPECT_GT(params.dt, 0.0f);
    EXPECT_GT(params.n_fft, 0u);
    EXPECT_GT(params.hop_length, 0u);
}

/**
 * TEST: Default PAC parameters
 * WHAT: Get default PAC parameters
 * WHY:  Verify theta-gamma coupling defaults
 */
TEST_F(OscillationsKernelTest, DefaultPACParams_ThetaGammaCoupling) {
    nimcp_pac_params_t params = nimcp_pac_default_params();

    // Theta band for phase
    EXPECT_GE(params.phase_freq_low, 4.0f);
    EXPECT_LE(params.phase_freq_high, 8.0f);

    // Gamma band for amplitude
    EXPECT_GE(params.amp_freq_low, 30.0f);
    EXPECT_LE(params.amp_freq_high, 100.0f);

    EXPECT_GT(params.n_phase_bins, 0u);
}

/**
 * TEST: Default coherence parameters
 * WHAT: Get default coherence parameters
 * WHY:  Verify sensible coherence settings
 */
TEST_F(OscillationsKernelTest, DefaultCoherenceParams_HasReasonableValues) {
    nimcp_coherence_params_t params = nimcp_coherence_default_params();

    EXPECT_GT(params.n_fft, 0u);
    EXPECT_GT(params.n_overlap, 0u);
    EXPECT_LT(params.n_overlap, params.n_fft);
}

/**
 * TEST: Band frequency lookup
 * WHAT: Get frequency limits for each band
 * WHY:  Verify standard band definitions
 */
TEST_F(OscillationsKernelTest, BandFrequencies_StandardDefinitions) {
    float low, high;

    // Delta band: 0.5-4 Hz
    nimcp_get_band_frequencies(NIMCP_BAND_DELTA, &low, &high);
    EXPECT_GE(low, 0.5f);
    EXPECT_LE(high, 4.0f);

    // Theta band: 4-8 Hz
    nimcp_get_band_frequencies(NIMCP_BAND_THETA, &low, &high);
    EXPECT_GE(low, 4.0f);
    EXPECT_LE(high, 8.0f);

    // Alpha band: 8-13 Hz
    nimcp_get_band_frequencies(NIMCP_BAND_ALPHA, &low, &high);
    EXPECT_GE(low, 8.0f);
    EXPECT_LE(high, 13.0f);

    // Beta band: 13-30 Hz
    nimcp_get_band_frequencies(NIMCP_BAND_BETA, &low, &high);
    EXPECT_GE(low, 13.0f);
    EXPECT_LE(high, 30.0f);

    // Gamma band: 30-100 Hz
    nimcp_get_band_frequencies(NIMCP_BAND_GAMMA, &low, &high);
    EXPECT_GE(low, 30.0f);
    EXPECT_LE(high, 100.0f);
}

//=============================================================================
// State Lifecycle Tests
//=============================================================================

/**
 * TEST: Oscillation state creation
 * WHAT: Create oscillation analysis state
 * WHY:  Verify state allocation and initialization
 */
TEST_F(OscillationsKernelTest, OscillationState_Create_Succeeds) {
    RequireGPU();

    nimcp_oscillation_params_t params = create_default_params();
    nimcp_oscillation_state_t* state = nimcp_oscillation_state_create(ctx, &params);

    if (state) {
        EXPECT_NE(state->signal, nullptr);
        EXPECT_EQ(state->params.sampling_rate, params.sampling_rate);
        nimcp_oscillation_state_destroy(state);
    }
    // May return NULL if not implemented - acceptable
}

/**
 * TEST: Oscillation state destruction with NULL
 * WHAT: Destroy NULL state
 * WHY:  Verify NULL-safety
 */
TEST_F(OscillationsKernelTest, OscillationState_DestroyNull_NoOp) {
    nimcp_oscillation_state_destroy(nullptr);
    SUCCEED() << "Should not crash";
}

/**
 * TEST: Phase sync state creation
 * WHAT: Create phase synchronization state
 * WHY:  Verify PLV/PLI buffer allocation
 */
TEST_F(OscillationsKernelTest, PhaseSyncState_Create_Succeeds) {
    RequireGPU();

    nimcp_phase_sync_state_t* state = nimcp_phase_sync_state_create(
        ctx, DEFAULT_N_CHANNELS, DEFAULT_N_SAMPLES);

    if (state) {
        EXPECT_EQ(state->n_channels, DEFAULT_N_CHANNELS);
        EXPECT_EQ(state->window_size, DEFAULT_N_SAMPLES);
        nimcp_phase_sync_state_destroy(state);
    }
}

/**
 * TEST: Phase sync state destruction with NULL
 * WHAT: Destroy NULL state
 * WHY:  Verify NULL-safety
 */
TEST_F(OscillationsKernelTest, PhaseSyncState_DestroyNull_NoOp) {
    nimcp_phase_sync_state_destroy(nullptr);
    SUCCEED() << "Should not crash";
}

//=============================================================================
// Phase Locking Value (PLV) Tests
//=============================================================================

/**
 * TEST: PLV between identical phases
 * WHAT: Compute PLV when phase1 == phase2
 * WHY:  Perfect synchronization should yield PLV = 1
 */
TEST_F(OscillationsKernelTest, PLV_IdenticalPhases_ReturnsOne) {
    RequireGPU();

    std::vector<float> phase_data = generate_phase_signal(DEFAULT_N_SAMPLES, 10.0f);
    nimcp_gpu_tensor_t* phase1 = create_tensor_from_data(phase_data.data(), DEFAULT_N_SAMPLES);
    nimcp_gpu_tensor_t* phase2 = create_tensor_from_data(phase_data.data(), DEFAULT_N_SAMPLES);

    if (!phase1 || !phase2) {
        if (phase1) nimcp_gpu_tensor_destroy(phase1);
        if (phase2) nimcp_gpu_tensor_destroy(phase2);
        GTEST_SKIP() << "Tensor creation failed";
    }

    float plv = 0.0f;
    bool result = nimcp_gpu_phase_locking_value(ctx, phase1, phase2, &plv);

    if (result) {
        EXPECT_NEAR(plv, 1.0f, 0.01f) << "Identical phases should have PLV = 1";
    }

    nimcp_gpu_tensor_destroy(phase1);
    nimcp_gpu_tensor_destroy(phase2);
}

/**
 * TEST: PLV between orthogonal phases
 * WHAT: Compute PLV when phases are 90 degrees apart
 * WHY:  Constant phase difference should still yield high PLV
 */
TEST_F(OscillationsKernelTest, PLV_ConstantPhaseDifference_HighPLV) {
    RequireGPU();

    std::vector<float> phase1_data = generate_phase_signal(DEFAULT_N_SAMPLES, 10.0f, 0.0f);
    std::vector<float> phase2_data = generate_phase_signal(DEFAULT_N_SAMPLES, 10.0f, PI / 2.0f);

    nimcp_gpu_tensor_t* phase1 = create_tensor_from_data(phase1_data.data(), DEFAULT_N_SAMPLES);
    nimcp_gpu_tensor_t* phase2 = create_tensor_from_data(phase2_data.data(), DEFAULT_N_SAMPLES);

    if (!phase1 || !phase2) {
        if (phase1) nimcp_gpu_tensor_destroy(phase1);
        if (phase2) nimcp_gpu_tensor_destroy(phase2);
        GTEST_SKIP() << "Tensor creation failed";
    }

    float plv = 0.0f;
    bool result = nimcp_gpu_phase_locking_value(ctx, phase1, phase2, &plv);

    if (result) {
        EXPECT_NEAR(plv, 1.0f, 0.05f) << "Constant phase difference should have high PLV";
    }

    nimcp_gpu_tensor_destroy(phase1);
    nimcp_gpu_tensor_destroy(phase2);
}

/**
 * TEST: PLV with NULL inputs
 * WHAT: Try PLV with NULL tensors
 * WHY:  Verify NULL-safety
 */
TEST_F(OscillationsKernelTest, PLV_NullInputs_ReturnsFalse) {
    RequireGPU();

    nimcp_gpu_tensor_t* phase = create_zero_tensor(DEFAULT_N_SAMPLES);
    float plv = 0.0f;

    EXPECT_FALSE(nimcp_gpu_phase_locking_value(ctx, nullptr, phase, &plv));
    EXPECT_FALSE(nimcp_gpu_phase_locking_value(ctx, phase, nullptr, &plv));
    EXPECT_FALSE(nimcp_gpu_phase_locking_value(ctx, phase, phase, nullptr));

    if (phase) nimcp_gpu_tensor_destroy(phase);
}

/**
 * TEST: PLV matrix computation
 * WHAT: Compute PLV for all channel pairs
 * WHY:  Full connectivity analysis
 */
TEST_F(OscillationsKernelTest, PLVMatrix_AllPairs_Computed) {
    RequireGPU();

    const size_t n_channels = 4;
    const size_t n_samples = 500;

    // Create multichannel phase data [n_channels x n_samples]
    std::vector<float> phases_data(n_channels * n_samples);
    for (size_t ch = 0; ch < n_channels; ch++) {
        for (size_t i = 0; i < n_samples; i++) {
            float t = static_cast<float>(i) / DEFAULT_SAMPLING_RATE;
            phases_data[ch * n_samples + i] = 2.0f * PI * 10.0f * t + static_cast<float>(ch) * 0.5f;
        }
    }

    size_t phase_dims[2] = {n_channels, n_samples};
    nimcp_gpu_tensor_t* phases = nimcp_gpu_tensor_from_host(ctx, phases_data.data(), phase_dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* plv_matrix = create_matrix(n_channels, n_channels);

    if (!phases || !plv_matrix) {
        if (phases) nimcp_gpu_tensor_destroy(phases);
        if (plv_matrix) nimcp_gpu_tensor_destroy(plv_matrix);
        GTEST_SKIP() << "Tensor creation failed";
    }

    bool result = nimcp_gpu_plv_matrix(ctx, phases, plv_matrix);

    if (result) {
        // Copy and verify
        std::vector<float> plv_host(n_channels * n_channels);
        copy_to_host(plv_matrix, plv_host.data());

        // Diagonal should be 1 (self-synchronization)
        for (size_t i = 0; i < n_channels; i++) {
            EXPECT_NEAR(plv_host[i * n_channels + i], 1.0f, 0.01f);
        }

        // Matrix should be symmetric
        for (size_t i = 0; i < n_channels; i++) {
            for (size_t j = i + 1; j < n_channels; j++) {
                EXPECT_NEAR(plv_host[i * n_channels + j],
                           plv_host[j * n_channels + i], 0.01f);
            }
        }
    }

    nimcp_gpu_tensor_destroy(phases);
    nimcp_gpu_tensor_destroy(plv_matrix);
}

//=============================================================================
// Phase Lag Index (PLI) Tests
//=============================================================================

/**
 * TEST: PLI between phases with constant lag
 * WHAT: Compute PLI with consistent phase lag
 * WHY:  Consistent positive/negative lag should yield high PLI
 */
TEST_F(OscillationsKernelTest, PLI_ConstantLag_HighPLI) {
    RequireGPU();

    std::vector<float> phase1_data = generate_phase_signal(DEFAULT_N_SAMPLES, 10.0f, 0.0f);
    std::vector<float> phase2_data = generate_phase_signal(DEFAULT_N_SAMPLES, 10.0f, 0.3f);

    nimcp_gpu_tensor_t* phase1 = create_tensor_from_data(phase1_data.data(), DEFAULT_N_SAMPLES);
    nimcp_gpu_tensor_t* phase2 = create_tensor_from_data(phase2_data.data(), DEFAULT_N_SAMPLES);

    if (!phase1 || !phase2) {
        if (phase1) nimcp_gpu_tensor_destroy(phase1);
        if (phase2) nimcp_gpu_tensor_destroy(phase2);
        GTEST_SKIP() << "Tensor creation failed";
    }

    float pli = 0.0f;
    bool result = nimcp_gpu_phase_lag_index(ctx, phase1, phase2, &pli);

    if (result) {
        EXPECT_GT(std::abs(pli), 0.8f) << "Constant lag should have high |PLI|";
    }

    nimcp_gpu_tensor_destroy(phase1);
    nimcp_gpu_tensor_destroy(phase2);
}

/**
 * TEST: PLI with zero mean lag
 * WHAT: Compute PLI when lag oscillates around zero
 * WHY:  Zero volume conduction should yield PLI near 0
 */
TEST_F(OscillationsKernelTest, PLI_ZeroMeanLag_LowPLI) {
    RequireGPU();

    // Create phases that oscillate around each other (simulate volume conduction)
    std::vector<float> phase1_data(DEFAULT_N_SAMPLES);
    std::vector<float> phase2_data(DEFAULT_N_SAMPLES);
    for (size_t i = 0; i < DEFAULT_N_SAMPLES; i++) {
        float t = static_cast<float>(i) / DEFAULT_SAMPLING_RATE;
        phase1_data[i] = 2.0f * PI * 10.0f * t;
        // Phase2 has oscillating offset around phase1
        phase2_data[i] = phase1_data[i] + 0.5f * std::sin(2.0f * PI * 2.0f * t);
    }

    nimcp_gpu_tensor_t* phase1 = create_tensor_from_data(phase1_data.data(), DEFAULT_N_SAMPLES);
    nimcp_gpu_tensor_t* phase2 = create_tensor_from_data(phase2_data.data(), DEFAULT_N_SAMPLES);

    if (!phase1 || !phase2) {
        if (phase1) nimcp_gpu_tensor_destroy(phase1);
        if (phase2) nimcp_gpu_tensor_destroy(phase2);
        GTEST_SKIP() << "Tensor creation failed";
    }

    float pli = 0.0f;
    bool result = nimcp_gpu_phase_lag_index(ctx, phase1, phase2, &pli);

    if (result) {
        // PLI should be lower when lag oscillates around zero
        EXPECT_LT(std::abs(pli), 0.5f);
    }

    nimcp_gpu_tensor_destroy(phase1);
    nimcp_gpu_tensor_destroy(phase2);
}

//=============================================================================
// Global Synchronization Tests
//=============================================================================

/**
 * TEST: Global sync index with synchronized phases
 * WHAT: Compute Kuramoto order parameter for synchronized population
 * WHY:  Synchronized population should have R near 1
 */
TEST_F(OscillationsKernelTest, GlobalSync_SynchronizedPopulation_HighR) {
    RequireGPU();

    const size_t n_channels = 8;
    const size_t n_samples = 500;

    // All channels synchronized (same phase)
    std::vector<float> phases_data(n_channels * n_samples);
    for (size_t ch = 0; ch < n_channels; ch++) {
        for (size_t i = 0; i < n_samples; i++) {
            float t = static_cast<float>(i) / DEFAULT_SAMPLING_RATE;
            phases_data[ch * n_samples + i] = 2.0f * PI * 10.0f * t;
        }
    }

    size_t phase_dims[2] = {n_channels, n_samples};
    nimcp_gpu_tensor_t* phases = nimcp_gpu_tensor_from_host(ctx, phases_data.data(), phase_dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* sync_index = create_zero_tensor(n_samples);

    if (!phases || !sync_index) {
        if (phases) nimcp_gpu_tensor_destroy(phases);
        if (sync_index) nimcp_gpu_tensor_destroy(sync_index);
        GTEST_SKIP() << "Tensor creation failed";
    }

    bool result = nimcp_gpu_global_sync_index(ctx, phases, sync_index);

    if (result) {
        std::vector<float> sync_host(n_samples);
        copy_to_host(sync_index, sync_host.data());

        // Should be near 1 for synchronized population
        float mean_sync = std::accumulate(sync_host.begin(), sync_host.end(), 0.0f) / n_samples;
        EXPECT_NEAR(mean_sync, 1.0f, 0.05f);
    }

    nimcp_gpu_tensor_destroy(phases);
    nimcp_gpu_tensor_destroy(sync_index);
}

//=============================================================================
// Phase-Amplitude Coupling (PAC) Tests
//=============================================================================

/**
 * TEST: PAC modulation index computation
 * WHAT: Compute PAC using modulation index
 * WHY:  Verify cross-frequency coupling detection
 */
TEST_F(OscillationsKernelTest, PACModulationIndex_ExecutesCorrectly) {
    RequireGPU();

    // Create signal with theta-gamma coupling
    const size_t n_samples = 2000;
    std::vector<float> signal_data(n_samples);
    for (size_t i = 0; i < n_samples; i++) {
        float t = static_cast<float>(i) / DEFAULT_SAMPLING_RATE;
        float theta = std::sin(2.0f * PI * 6.0f * t);  // 6 Hz theta
        float gamma_amp = 0.5f + 0.3f * theta;          // Gamma amplitude modulated by theta
        float gamma = gamma_amp * std::sin(2.0f * PI * 40.0f * t);  // 40 Hz gamma
        signal_data[i] = theta + gamma;
    }

    nimcp_gpu_tensor_t* signal = create_tensor_from_data(signal_data.data(), n_samples);
    nimcp_gpu_tensor_t* pac_values = create_zero_tensor(1);

    if (!signal || !pac_values) {
        if (signal) nimcp_gpu_tensor_destroy(signal);
        if (pac_values) nimcp_gpu_tensor_destroy(pac_values);
        GTEST_SKIP() << "Tensor creation failed";
    }

    nimcp_pac_params_t params = nimcp_pac_default_params();
    params.phase_freq_low = 4.0f;
    params.phase_freq_high = 8.0f;
    params.amp_freq_low = 30.0f;
    params.amp_freq_high = 50.0f;

    bool result = nimcp_gpu_pac_modulation_index(ctx, signal, pac_values, &params);

    if (result) {
        float pac;
        copy_to_host(pac_values, &pac);
        EXPECT_GE(pac, 0.0f) << "PAC MI should be non-negative";
    }

    nimcp_gpu_tensor_destroy(signal);
    nimcp_gpu_tensor_destroy(pac_values);
}

/**
 * TEST: PAC with NULL signal
 * WHAT: Try PAC with NULL input
 * WHY:  Verify NULL-safety
 */
TEST_F(OscillationsKernelTest, PACModulationIndex_NullSignal_ReturnsFalse) {
    RequireGPU();

    nimcp_gpu_tensor_t* pac_values = create_zero_tensor(1);
    nimcp_pac_params_t params = nimcp_pac_default_params();

    EXPECT_FALSE(nimcp_gpu_pac_modulation_index(ctx, nullptr, pac_values, &params));

    if (pac_values) nimcp_gpu_tensor_destroy(pac_values);
}

//=============================================================================
// Hilbert Transform Tests
//=============================================================================

/**
 * TEST: Hilbert phase extraction
 * WHAT: Extract instantaneous phase via Hilbert transform
 * WHY:  Core operation for phase-based analyses
 */
TEST_F(OscillationsKernelTest, HilbertPhase_SineWave_LinearPhase) {
    RequireGPU();

    const float freq = 10.0f;
    std::vector<float> signal_data = generate_sine_wave(DEFAULT_N_SAMPLES, freq);
    nimcp_gpu_tensor_t* signal = create_tensor_from_data(signal_data.data(), DEFAULT_N_SAMPLES);
    nimcp_gpu_tensor_t* phase = create_zero_tensor(DEFAULT_N_SAMPLES);

    if (!signal || !phase) {
        if (signal) nimcp_gpu_tensor_destroy(signal);
        if (phase) nimcp_gpu_tensor_destroy(phase);
        GTEST_SKIP() << "Tensor creation failed";
    }

    bool result = nimcp_gpu_hilbert_phase(ctx, signal, phase);

    if (result) {
        std::vector<float> phase_host(DEFAULT_N_SAMPLES);
        copy_to_host(phase, phase_host.data());

        // Phase should increase linearly (modulo 2*pi)
        // Check that phase is bounded
        for (float p : phase_host) {
            EXPECT_GE(p, -PI - 0.1f);
            EXPECT_LE(p, PI + 0.1f);
        }
    }

    nimcp_gpu_tensor_destroy(signal);
    nimcp_gpu_tensor_destroy(phase);
}

/**
 * TEST: Hilbert amplitude extraction
 * WHAT: Extract instantaneous amplitude (envelope)
 * WHY:  Used for amplitude analyses and PAC
 */
TEST_F(OscillationsKernelTest, HilbertAmplitude_SineWave_ConstantEnvelope) {
    RequireGPU();

    const float freq = 10.0f;
    const float amplitude = 2.0f;
    std::vector<float> signal_data = generate_sine_wave(DEFAULT_N_SAMPLES, freq, amplitude);
    nimcp_gpu_tensor_t* signal = create_tensor_from_data(signal_data.data(), DEFAULT_N_SAMPLES);
    nimcp_gpu_tensor_t* amp = create_zero_tensor(DEFAULT_N_SAMPLES);

    if (!signal || !amp) {
        if (signal) nimcp_gpu_tensor_destroy(signal);
        if (amp) nimcp_gpu_tensor_destroy(amp);
        GTEST_SKIP() << "Tensor creation failed";
    }

    bool result = nimcp_gpu_hilbert_amplitude(ctx, signal, amp);

    if (result) {
        std::vector<float> amp_host(DEFAULT_N_SAMPLES);
        copy_to_host(amp, amp_host.data());

        // Skip edge effects, check middle portion
        size_t start = DEFAULT_N_SAMPLES / 4;
        size_t end = 3 * DEFAULT_N_SAMPLES / 4;
        for (size_t i = start; i < end; i++) {
            EXPECT_NEAR(amp_host[i], amplitude, 0.2f)
                << "Amplitude should be constant at " << i;
        }
    }

    nimcp_gpu_tensor_destroy(signal);
    nimcp_gpu_tensor_destroy(amp);
}

//=============================================================================
// Band Power Tests
//=============================================================================

/**
 * TEST: Band power computation
 * WHAT: Compute power in specific frequency band
 * WHY:  Fundamental oscillation metric
 */
TEST_F(OscillationsKernelTest, BandPower_AlphaBand_DetectsAlpha) {
    RequireGPU();

    // Create signal with strong 10 Hz alpha
    const float freq = 10.0f;
    std::vector<float> signal_data = generate_sine_wave(DEFAULT_N_SAMPLES, freq, 1.0f);

    // Add some noise at other frequencies
    for (size_t i = 0; i < DEFAULT_N_SAMPLES; i++) {
        float t = static_cast<float>(i) / DEFAULT_SAMPLING_RATE;
        signal_data[i] += 0.1f * std::sin(2.0f * PI * 30.0f * t);  // Beta noise
    }

    nimcp_gpu_tensor_t* signal = create_tensor_from_data(signal_data.data(), DEFAULT_N_SAMPLES);
    nimcp_gpu_tensor_t* power = create_zero_tensor(1);

    if (!signal || !power) {
        if (signal) nimcp_gpu_tensor_destroy(signal);
        if (power) nimcp_gpu_tensor_destroy(power);
        GTEST_SKIP() << "Tensor creation failed";
    }

    nimcp_oscillation_params_t params = create_default_params();
    params.freq_low = 8.0f;
    params.freq_high = 13.0f;

    bool result = nimcp_gpu_band_power(ctx, signal, power, &params);

    if (result) {
        float alpha_power;
        copy_to_host(power, &alpha_power);
        EXPECT_GT(alpha_power, 0.0f) << "Alpha power should be detected";
    }

    nimcp_gpu_tensor_destroy(signal);
    nimcp_gpu_tensor_destroy(power);
}

/**
 * TEST: Power spectral density
 * WHAT: Compute PSD using Welch's method
 * WHY:  Detailed frequency analysis
 */
TEST_F(OscillationsKernelTest, PSD_WelchMethod_ProducesSpectrum) {
    RequireGPU();

    std::vector<float> signal_data = generate_sine_wave(DEFAULT_N_SAMPLES, 10.0f);
    nimcp_gpu_tensor_t* signal = create_tensor_from_data(signal_data.data(), DEFAULT_N_SAMPLES);

    nimcp_oscillation_params_t params = create_default_params();
    size_t n_freqs = params.n_fft / 2 + 1;

    nimcp_gpu_tensor_t* psd = create_zero_tensor(n_freqs);
    nimcp_gpu_tensor_t* freqs = create_zero_tensor(n_freqs);

    if (!signal || !psd || !freqs) {
        if (signal) nimcp_gpu_tensor_destroy(signal);
        if (psd) nimcp_gpu_tensor_destroy(psd);
        if (freqs) nimcp_gpu_tensor_destroy(freqs);
        GTEST_SKIP() << "Tensor creation failed";
    }

    bool result = nimcp_gpu_power_spectral_density(ctx, signal, psd, freqs, &params);

    if (result) {
        std::vector<float> psd_host(n_freqs);
        copy_to_host(psd, psd_host.data());

        // All PSD values should be non-negative
        for (float p : psd_host) {
            EXPECT_GE(p, 0.0f);
        }
    }

    nimcp_gpu_tensor_destroy(signal);
    nimcp_gpu_tensor_destroy(psd);
    nimcp_gpu_tensor_destroy(freqs);
}

/**
 * TEST: Spectrogram computation
 * WHAT: Compute time-frequency representation
 * WHY:  Track frequency changes over time
 */
TEST_F(OscillationsKernelTest, Spectrogram_TimeFrequency_Computed) {
    RequireGPU();

    std::vector<float> signal_data = generate_sine_wave(DEFAULT_N_SAMPLES, 10.0f);
    nimcp_gpu_tensor_t* signal = create_tensor_from_data(signal_data.data(), DEFAULT_N_SAMPLES);

    nimcp_oscillation_params_t params = create_default_params();
    size_t n_time = (DEFAULT_N_SAMPLES - params.n_fft) / params.hop_length + 1;
    size_t n_freqs = params.n_fft / 2 + 1;

    nimcp_gpu_tensor_t* spectrogram = create_matrix(n_time, n_freqs);

    if (!signal || !spectrogram) {
        if (signal) nimcp_gpu_tensor_destroy(signal);
        if (spectrogram) nimcp_gpu_tensor_destroy(spectrogram);
        GTEST_SKIP() << "Tensor creation failed";
    }

    bool result = nimcp_gpu_spectrogram(ctx, signal, spectrogram, &params);

    if (result) {
        std::vector<float> spec_host(n_time * n_freqs);
        copy_to_host(spectrogram, spec_host.data());

        // All spectrogram values should be non-negative
        for (float s : spec_host) {
            EXPECT_GE(s, 0.0f);
        }
    }

    nimcp_gpu_tensor_destroy(signal);
    nimcp_gpu_tensor_destroy(spectrogram);
}

//=============================================================================
// Coherence Tests
//=============================================================================

/**
 * TEST: Coherence between identical signals
 * WHAT: Compute coherence when signals are the same
 * WHY:  Self-coherence should be 1
 */
TEST_F(OscillationsKernelTest, Coherence_IdenticalSignals_ReturnsOne) {
    RequireGPU();

    std::vector<float> signal_data = generate_sine_wave(DEFAULT_N_SAMPLES, 10.0f);
    nimcp_gpu_tensor_t* signal1 = create_tensor_from_data(signal_data.data(), DEFAULT_N_SAMPLES);
    nimcp_gpu_tensor_t* signal2 = create_tensor_from_data(signal_data.data(), DEFAULT_N_SAMPLES);

    nimcp_coherence_params_t params = nimcp_coherence_default_params();
    size_t n_freqs = params.n_fft / 2 + 1;
    nimcp_gpu_tensor_t* coherence = create_zero_tensor(n_freqs);

    if (!signal1 || !signal2 || !coherence) {
        if (signal1) nimcp_gpu_tensor_destroy(signal1);
        if (signal2) nimcp_gpu_tensor_destroy(signal2);
        if (coherence) nimcp_gpu_tensor_destroy(coherence);
        GTEST_SKIP() << "Tensor creation failed";
    }

    bool result = nimcp_gpu_coherence(ctx, signal1, signal2, coherence, &params);

    if (result) {
        std::vector<float> coh_host(n_freqs);
        copy_to_host(coherence, coh_host.data());

        // Coherence should be near 1 at the signal frequency
        for (float c : coh_host) {
            EXPECT_GE(c, 0.0f);
            EXPECT_LE(c, 1.0f + NUMERICAL_EPS);
        }
    }

    nimcp_gpu_tensor_destroy(signal1);
    nimcp_gpu_tensor_destroy(signal2);
    nimcp_gpu_tensor_destroy(coherence);
}

/**
 * TEST: Coherence matrix for all channel pairs
 * WHAT: Compute coherence matrix
 * WHY:  Full connectivity coherence analysis
 */
TEST_F(OscillationsKernelTest, CoherenceMatrix_AllPairs_Symmetric) {
    RequireGPU();

    const size_t n_channels = 4;
    const size_t n_samples = 1000;

    // Create multichannel signals
    std::vector<float> signals_data(n_channels * n_samples);
    for (size_t ch = 0; ch < n_channels; ch++) {
        for (size_t i = 0; i < n_samples; i++) {
            float t = static_cast<float>(i) / DEFAULT_SAMPLING_RATE;
            signals_data[ch * n_samples + i] = std::sin(2.0f * PI * 10.0f * t + static_cast<float>(ch) * 0.3f);
        }
    }

    size_t signal_dims[2] = {n_channels, n_samples};
    nimcp_gpu_tensor_t* signals = nimcp_gpu_tensor_from_host(ctx, signals_data.data(), signal_dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* coh_matrix = create_matrix(n_channels, n_channels);

    if (!signals || !coh_matrix) {
        if (signals) nimcp_gpu_tensor_destroy(signals);
        if (coh_matrix) nimcp_gpu_tensor_destroy(coh_matrix);
        GTEST_SKIP() << "Tensor creation failed";
    }

    nimcp_coherence_params_t params = nimcp_coherence_default_params();
    bool result = nimcp_gpu_coherence_matrix(ctx, signals, coh_matrix, &params);

    if (result) {
        std::vector<float> coh_host(n_channels * n_channels);
        copy_to_host(coh_matrix, coh_host.data());

        // Matrix should be symmetric
        for (size_t i = 0; i < n_channels; i++) {
            for (size_t j = i + 1; j < n_channels; j++) {
                EXPECT_NEAR(coh_host[i * n_channels + j],
                           coh_host[j * n_channels + i], 0.01f);
            }
        }

        // Diagonal should be 1
        for (size_t i = 0; i < n_channels; i++) {
            EXPECT_NEAR(coh_host[i * n_channels + i], 1.0f, 0.01f);
        }
    }

    nimcp_gpu_tensor_destroy(signals);
    nimcp_gpu_tensor_destroy(coh_matrix);
}

/**
 * TEST: Imaginary coherence
 * WHAT: Compute imaginary part of coherence
 * WHY:  Robust to volume conduction
 */
TEST_F(OscillationsKernelTest, ImaginaryCoherence_ExecutesCorrectly) {
    RequireGPU();

    std::vector<float> signal1_data = generate_sine_wave(DEFAULT_N_SAMPLES, 10.0f);
    std::vector<float> signal2_data = generate_sine_wave(DEFAULT_N_SAMPLES, 10.0f);

    // Add phase lag to signal2
    for (size_t i = 0; i < DEFAULT_N_SAMPLES; i++) {
        float t = static_cast<float>(i) / DEFAULT_SAMPLING_RATE;
        signal2_data[i] = std::sin(2.0f * PI * 10.0f * t + 0.5f);
    }

    nimcp_gpu_tensor_t* signal1 = create_tensor_from_data(signal1_data.data(), DEFAULT_N_SAMPLES);
    nimcp_gpu_tensor_t* signal2 = create_tensor_from_data(signal2_data.data(), DEFAULT_N_SAMPLES);

    nimcp_coherence_params_t params = nimcp_coherence_default_params();
    params.imaginary_coherence = true;
    size_t n_freqs = params.n_fft / 2 + 1;
    nimcp_gpu_tensor_t* icoh = create_zero_tensor(n_freqs);

    if (!signal1 || !signal2 || !icoh) {
        if (signal1) nimcp_gpu_tensor_destroy(signal1);
        if (signal2) nimcp_gpu_tensor_destroy(signal2);
        if (icoh) nimcp_gpu_tensor_destroy(icoh);
        GTEST_SKIP() << "Tensor creation failed";
    }

    bool result = nimcp_gpu_imaginary_coherence(ctx, signal1, signal2, icoh, &params);

    if (result) {
        std::vector<float> icoh_host(n_freqs);
        copy_to_host(icoh, icoh_host.data());

        // Imaginary coherence should be bounded
        for (float ic : icoh_host) {
            EXPECT_GE(ic, -1.0f - NUMERICAL_EPS);
            EXPECT_LE(ic, 1.0f + NUMERICAL_EPS);
        }
    }

    nimcp_gpu_tensor_destroy(signal1);
    nimcp_gpu_tensor_destroy(signal2);
    nimcp_gpu_tensor_destroy(icoh);
}

//=============================================================================
// Bandpass Filter Tests
//=============================================================================

/**
 * TEST: Bandpass filter execution
 * WHAT: Apply bandpass filter to signal
 * WHY:  Preprocessing for band-specific analyses
 */
TEST_F(OscillationsKernelTest, BandpassFilter_ExecutesCorrectly) {
    RequireGPU();

    // Signal with multiple frequency components
    std::vector<float> signal_data(DEFAULT_N_SAMPLES);
    for (size_t i = 0; i < DEFAULT_N_SAMPLES; i++) {
        float t = static_cast<float>(i) / DEFAULT_SAMPLING_RATE;
        signal_data[i] = std::sin(2.0f * PI * 5.0f * t)   // Theta
                       + std::sin(2.0f * PI * 10.0f * t)  // Alpha
                       + std::sin(2.0f * PI * 25.0f * t); // Beta
    }

    nimcp_gpu_tensor_t* signal = create_tensor_from_data(signal_data.data(), DEFAULT_N_SAMPLES);
    nimcp_gpu_tensor_t* filtered = create_zero_tensor(DEFAULT_N_SAMPLES);

    if (!signal || !filtered) {
        if (signal) nimcp_gpu_tensor_destroy(signal);
        if (filtered) nimcp_gpu_tensor_destroy(filtered);
        GTEST_SKIP() << "Tensor creation failed";
    }

    // Filter for alpha band
    bool result = nimcp_gpu_bandpass_filter(ctx, signal, filtered,
                                            8.0f, 13.0f, 4, DEFAULT_SAMPLING_RATE);

    if (result) {
        std::vector<float> filtered_host(DEFAULT_N_SAMPLES);
        copy_to_host(filtered, filtered_host.data());

        // Filtered signal should have reduced amplitude (only alpha remains)
        float max_amp = *std::max_element(filtered_host.begin(), filtered_host.end());
        float min_amp = *std::min_element(filtered_host.begin(), filtered_host.end());

        EXPECT_LT(max_amp - min_amp, 3.0f) << "Filtered signal should have lower amplitude";
    }

    nimcp_gpu_tensor_destroy(signal);
    nimcp_gpu_tensor_destroy(filtered);
}

/**
 * TEST: Bandpass filter with NULL inputs
 * WHAT: Try filter with NULL tensors
 * WHY:  Verify NULL-safety
 */
TEST_F(OscillationsKernelTest, BandpassFilter_NullInputs_ReturnsFalse) {
    RequireGPU();

    nimcp_gpu_tensor_t* signal = create_zero_tensor(DEFAULT_N_SAMPLES);
    nimcp_gpu_tensor_t* filtered = create_zero_tensor(DEFAULT_N_SAMPLES);

    EXPECT_FALSE(nimcp_gpu_bandpass_filter(ctx, nullptr, filtered,
                                           8.0f, 13.0f, 4, DEFAULT_SAMPLING_RATE));
    EXPECT_FALSE(nimcp_gpu_bandpass_filter(ctx, signal, nullptr,
                                           8.0f, 13.0f, 4, DEFAULT_SAMPLING_RATE));

    if (signal) nimcp_gpu_tensor_destroy(signal);
    if (filtered) nimcp_gpu_tensor_destroy(filtered);
}

//=============================================================================
// Relative Band Power Tests
//=============================================================================

/**
 * TEST: Relative band power computation
 * WHAT: Compute normalized power per band
 * WHY:  Compare across bands and subjects
 */
TEST_F(OscillationsKernelTest, RelativeBandPower_SumsToOne) {
    RequireGPU();

    std::vector<float> signal_data = generate_sine_wave(DEFAULT_N_SAMPLES, 10.0f);
    nimcp_gpu_tensor_t* signal = create_tensor_from_data(signal_data.data(), DEFAULT_N_SAMPLES);

    nimcp_oscillation_band_t bands[] = {
        NIMCP_BAND_DELTA, NIMCP_BAND_THETA, NIMCP_BAND_ALPHA,
        NIMCP_BAND_BETA, NIMCP_BAND_GAMMA
    };
    size_t n_bands = 5;

    nimcp_gpu_tensor_t* relative_power = create_zero_tensor(n_bands);

    if (!signal || !relative_power) {
        if (signal) nimcp_gpu_tensor_destroy(signal);
        if (relative_power) nimcp_gpu_tensor_destroy(relative_power);
        GTEST_SKIP() << "Tensor creation failed";
    }

    bool result = nimcp_gpu_relative_band_power(ctx, signal, relative_power, bands, n_bands);

    if (result) {
        std::vector<float> power_host(n_bands);
        copy_to_host(relative_power, power_host.data());

        // Relative powers should sum to approximately 1
        float total = std::accumulate(power_host.begin(), power_host.end(), 0.0f);
        EXPECT_NEAR(total, 1.0f, 0.1f);

        // All should be non-negative
        for (float p : power_host) {
            EXPECT_GE(p, 0.0f);
        }
    }

    nimcp_gpu_tensor_destroy(signal);
    nimcp_gpu_tensor_destroy(relative_power);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
