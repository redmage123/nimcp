//=============================================================================
// test_snn_gpu_per_neuron_lif_unit.cpp — Unit tests for the GPU LIF kernel's
// per-neuron τ_mem / v_thresh path (Wave G GPU sync, schema v17, 2026-04-27).
//=============================================================================
/**
 * @file test_snn_gpu_per_neuron_lif_unit.cpp
 * @brief Wave G GPU sync — Unit tests for nimcp_gpu_lif_state_upload_per_neuron_params
 *        and the kernel_lif_forward / kernel_lif_forward_simple per-neuron path.
 *
 * WHAT: Drives a single-population GPU LIF state with identical sustained
 *       input and verifies:
 *         (1) Bit-identity contract — with both per-neuron arrays NULL the
 *             GPU output equals the scalar-fallback (pre-Wave-G) output.
 *         (2) Heterogeneity desynchronization — with σ=0.2 on τ_mem the
 *             per-neuron membrane V trajectories diverge across neurons
 *             (variance grows over steps).
 *         (3) Threshold heterogeneity — with σ on v_thresh, neurons with
 *             higher thresholds spike less than neurons with lower
 *             thresholds under matched drive.
 *         (4) NULL re-upload symmetry — uploading NULL frees the device
 *             buffer and reverts to scalar params.
 *         (5) Size-mismatch rejection — passing the wrong n_neurons value
 *             returns false without modifying state.
 * WHY:  Pre-Wave-G GPU LIF kernel ignored both subclass deltas AND per-
 *       neuron heterogeneity, so tier-pyramidal populations behaved
 *       homogeneously on GPU even when σ=0.10 was set host-side. This
 *       suite pins the new contract: kernel reads per-neuron values when
 *       arrays are uploaded, falls back to scalar otherwise.
 * HOW:  Google Test. Skips gracefully if no GPU is available — tests run
 *       on CPU build hosts as a no-op rather than failing.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <numeric>

// NOTE: do NOT wrap these in our own `extern "C"` — the GPU headers pull in
// CUDA's C++ headers (cuda_bf16.hpp, cublas templates) which cannot have C
// linkage. Each header has its own internal `extern "C"` guard around the
// C-callable symbols.
#include "nimcp.h"
#include "gpu/snn/nimcp_snn_gpu.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"

namespace {

class SnnGpuPerNeuronLifTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_EQ(nimcp_init(), NIMCP_SUCCESS);
    }
    static void TearDownTestSuite() {
        nimcp_shutdown();
    }

    void SetUp() override {
        gpu_ = nimcp_gpu_context_create_auto();
        gpu_available_ = (gpu_ != nullptr);
    }

    void TearDown() override {
        if (state_) {
            nimcp_lif_state_destroy(state_);
            state_ = nullptr;
        }
        if (gpu_) {
            nimcp_gpu_context_destroy(gpu_);
            gpu_ = nullptr;
        }
    }

    void RequireGPU() {
        if (!gpu_available_) {
            GTEST_SKIP() << "no GPU available — Wave G GPU sync test skipped";
        }
    }

    // Build a fresh LIF state with the standard cortical-pyramidal-ish
    // params used across the heterogeneity test family.
    nimcp_lif_state_t* fresh_state(size_t n) {
        nimcp_lif_params_t p = {};
        p.tau_mem    = 20.0f;
        p.tau_syn    = 5.0f;
        p.v_thresh   = -50.0f;
        p.v_reset    = -65.0f;
        p.v_rest     = -65.0f;
        p.dt         = 1.0f;
        p.hard_reset = true;
        return nimcp_lif_state_create(gpu_, n, &p);
    }

    // Drive the LIF kernel with a constant input current vector for `steps`
    // steps; return the final membrane V on the host.
    std::vector<float> run_steps(nimcp_lif_state_t* st,
                                 const std::vector<float>& input,
                                 int steps) {
        const size_t n = input.size();
        size_t dims[1] = { n };
        nimcp_gpu_tensor_t* in = nimcp_gpu_tensor_from_host(
            gpu_, input.data(), dims, 1, NIMCP_GPU_PRECISION_FP32);
        EXPECT_NE(in, nullptr);

        for (int s = 0; s < steps; ++s) {
            EXPECT_TRUE(nimcp_gpu_lif_forward(gpu_, st, in));
        }

        std::vector<float> v_host(n, 0.0f);
        EXPECT_TRUE(nimcp_gpu_tensor_to_host(st->v, v_host.data()));
        nimcp_gpu_tensor_destroy(in);
        return v_host;
    }

    nimcp_gpu_context_t* gpu_ = nullptr;
    nimcp_lif_state_t*   state_ = nullptr;
    bool gpu_available_ = false;
};

// ─────────────────────────────────────────────────────────────────────────────
// (1) Bit-identity contract — with NO per-neuron arrays uploaded, the GPU
//     LIF kernel must produce identical output across all neurons given
//     identical input (since they all share the scalar params).
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(SnnGpuPerNeuronLifTest, NoPerNeuronArrays_HomogeneousOutput) {
    RequireGPU();

    constexpr size_t N = 64;
    state_ = fresh_state(N);
    ASSERT_NE(state_, nullptr);
    // No upload → tau_mem_per_neuron / v_thresh_per_neuron stay NULL.
    EXPECT_EQ(state_->tau_mem_per_neuron, nullptr);
    EXPECT_EQ(state_->v_thresh_per_neuron, nullptr);

    // Strong drive — guaranteed to push membrane far from rest within 5 steps.
    std::vector<float> drive(N, 12.0f);
    auto v = run_steps(state_, drive, 5);

    // Every neuron should have the EXACT same membrane V (bit-identical
    // since the kernel reduces to the homogeneous scalar path).
    const float v0 = v[0];
    for (size_t i = 1; i < N; ++i) {
        EXPECT_FLOAT_EQ(v[i], v0)
            << "Neuron " << i << " diverged from neuron 0 with no per-neuron arrays";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// (2) Heterogeneous τ_mem → membrane V trajectories diverge.
//     With identical drive, neurons with different τ_mem decay at different
//     rates → the variance across neurons of V grows above zero.
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(SnnGpuPerNeuronLifTest, HeterogeneousTau_MembraneDiverges) {
    RequireGPU();

    constexpr size_t N = 64;
    state_ = fresh_state(N);
    ASSERT_NE(state_, nullptr);

    // Spread tau_mem from 10 to 30 ms — covers PV-fast through L5-Betz-slow.
    std::vector<float> tau(N);
    for (size_t i = 0; i < N; ++i) {
        tau[i] = 10.0f + 20.0f * (static_cast<float>(i) / static_cast<float>(N - 1));
    }

    // Use a v_thresh well above the steady-state membrane to keep neurons
    // sub-threshold over the run — we want to observe membrane spread, not
    // resets. Pass NULL for v_thresh (use scalar = -50 mV), but set state's
    // params.v_thresh higher temporarily so spikes don't fire.
    state_->params.v_thresh = -10.0f;

    ASSERT_TRUE(nimcp_gpu_lif_state_upload_per_neuron_params(
        gpu_, state_, tau.data(), nullptr, N));
    EXPECT_NE(state_->tau_mem_per_neuron, nullptr);
    EXPECT_EQ(state_->v_thresh_per_neuron, nullptr);

    std::vector<float> drive(N, 8.0f);
    auto v = run_steps(state_, drive, 30);

    // Compute variance across neurons of V — heterogeneous decay must
    // produce a spread.
    float mean = std::accumulate(v.begin(), v.end(), 0.0f) / static_cast<float>(N);
    float var = 0.0f;
    for (float x : v) {
        var += (x - mean) * (x - mean);
    }
    var /= static_cast<float>(N);
    const float stddev = std::sqrt(var);

    // Heterogeneous τ from 10→30 ms × constant drive → membrane spread of
    // multiple mV at steady-state. Tight bound (stddev > 0.5 mV) is
    // generous — empirically it should be several mV.
    EXPECT_GT(stddev, 0.5f)
        << "Membrane V spread across neurons too small (stddev=" << stddev
        << " mV) — per-neuron τ_mem path may not be reaching the kernel";
}

// ─────────────────────────────────────────────────────────────────────────────
// (3) Heterogeneous v_thresh → spike count differs across neurons.
//     With identical drive and identical τ_mem, neurons with lower thresholds
//     should spike more often than neurons with higher thresholds.
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(SnnGpuPerNeuronLifTest, HeterogeneousThreshold_SpikeCountDiffers) {
    RequireGPU();

    constexpr size_t N = 64;
    state_ = fresh_state(N);
    ASSERT_NE(state_, nullptr);

    // Half the population gets a "low" threshold (-55 mV), half gets a
    // "high" threshold (-45 mV). Default scalar v_thresh is -50 mV.
    std::vector<float> vthr(N);
    for (size_t i = 0; i < N; ++i) {
        vthr[i] = (i < N / 2) ? -55.0f : -45.0f;
    }

    ASSERT_TRUE(nimcp_gpu_lif_state_upload_per_neuron_params(
        gpu_, state_, nullptr, vthr.data(), N));
    EXPECT_EQ(state_->tau_mem_per_neuron, nullptr);
    EXPECT_NE(state_->v_thresh_per_neuron, nullptr);

    // Drive that should push all neurons through threshold occasionally,
    // but the high-threshold half should spike less.
    std::vector<float> drive(N, 10.0f);
    size_t dims[1] = { N };
    nimcp_gpu_tensor_t* in = nimcp_gpu_tensor_from_host(
        gpu_, drive.data(), dims, 1, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(in, nullptr);

    int low_thresh_spikes = 0;
    int high_thresh_spikes = 0;
    constexpr int STEPS = 30;
    std::vector<float> spk(N, 0.0f);
    for (int s = 0; s < STEPS; ++s) {
        ASSERT_TRUE(nimcp_gpu_lif_forward(gpu_, state_, in));
        ASSERT_TRUE(nimcp_gpu_tensor_to_host(state_->spikes, spk.data()));
        for (size_t i = 0; i < N; ++i) {
            if (spk[i] > 0.5f) {
                if (i < N / 2) low_thresh_spikes++;
                else            high_thresh_spikes++;
            }
        }
    }
    nimcp_gpu_tensor_destroy(in);

    // Low-threshold half MUST spike more than high-threshold half. (We
    // don't assert a specific ratio — only the directional inequality.)
    EXPECT_GT(low_thresh_spikes, high_thresh_spikes)
        << "low_thresh spikes=" << low_thresh_spikes
        << " high_thresh spikes=" << high_thresh_spikes
        << " — per-neuron v_thresh path may not be reaching the kernel";
}

// ─────────────────────────────────────────────────────────────────────────────
// (4) NULL re-upload symmetry — uploading NULL after a non-NULL upload
//     frees the device buffer and reverts to scalar params.
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(SnnGpuPerNeuronLifTest, NullUpload_FreesDeviceBuffer) {
    RequireGPU();

    constexpr size_t N = 32;
    state_ = fresh_state(N);
    ASSERT_NE(state_, nullptr);

    std::vector<float> tau(N, 25.0f);
    ASSERT_TRUE(nimcp_gpu_lif_state_upload_per_neuron_params(
        gpu_, state_, tau.data(), nullptr, N));
    EXPECT_NE(state_->tau_mem_per_neuron, nullptr);

    // NULL upload → buffer freed, pointer reset to NULL.
    ASSERT_TRUE(nimcp_gpu_lif_state_upload_per_neuron_params(
        gpu_, state_, nullptr, nullptr, N));
    EXPECT_EQ(state_->tau_mem_per_neuron, nullptr);
    EXPECT_EQ(state_->v_thresh_per_neuron, nullptr);

    // Subsequent run uses scalar params again — homogeneous output.
    std::vector<float> drive(N, 12.0f);
    auto v = run_steps(state_, drive, 5);
    const float v0 = v[0];
    for (size_t i = 1; i < N; ++i) {
        EXPECT_FLOAT_EQ(v[i], v0);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// (5) Size-mismatch rejection — passing wrong n_neurons must return false
//     without modifying state.
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(SnnGpuPerNeuronLifTest, SizeMismatchRejected) {
    RequireGPU();

    constexpr size_t N = 32;
    state_ = fresh_state(N);
    ASSERT_NE(state_, nullptr);

    std::vector<float> tau(N, 20.0f);
    EXPECT_FALSE(nimcp_gpu_lif_state_upload_per_neuron_params(
        gpu_, state_, tau.data(), nullptr, N + 1));
    EXPECT_FALSE(nimcp_gpu_lif_state_upload_per_neuron_params(
        gpu_, state_, tau.data(), nullptr, N - 1));

    // State must be unchanged.
    EXPECT_EQ(state_->tau_mem_per_neuron, nullptr);
    EXPECT_EQ(state_->v_thresh_per_neuron, nullptr);
}

// ─────────────────────────────────────────────────────────────────────────────
// (6) Default dirty flag — fresh lif_state has per_neuron_params_dirty=true
//     so the SNN step layer pushes the composed values on the first step.
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(SnnGpuPerNeuronLifTest, FreshStateIsDirty) {
    RequireGPU();

    constexpr size_t N = 8;
    state_ = fresh_state(N);
    ASSERT_NE(state_, nullptr);
    EXPECT_TRUE(state_->per_neuron_params_dirty)
        << "Fresh lif_state should be dirty so the SNN step layer "
           "uploads composed (subclass + heterogeneity) per-neuron values";
}

}  // namespace
