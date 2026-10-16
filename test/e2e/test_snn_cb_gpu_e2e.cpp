//=============================================================================
// test_snn_cb_gpu_e2e.cpp — End-to-end CB GPU LIF run: 200 steps with mixed
// E/I drive on a 1024-neuron pool, no NaN, no runaway.
//=============================================================================
/**
 * @file test_snn_cb_gpu_e2e.cpp
 * @brief End-to-end exercise of the CB GPU forward path. We do NOT yet
 *        instantiate a full SNN network here (CB-GPU-4 will wire the kernel
 *        into snn_network_step) — what this test pins is that the kernel +
 *        state machinery survive a long run with realistic mixed drive
 *        without producing NaN, without saturating beyond reversal
 *        potentials, and that the resulting firing rate is in the healthy
 *        biological band when the population is driven by background poisson-
 *        like AMPA + small tonic GABA_A.
 *
 * WHAT: 1024 LIF neurons, 200 ms (200 steps × 1 ms), random per-step AMPA
 *       deposit ∈ [0, 0.1] + small NMDA deposit ∈ [0, 0.05] + tonic GABA_A
 *       0.02. NMDA Mg block enabled. Threshold = -50 mV (default), hard
 *       reset to -65 mV.
 * ASSERTIONS:
 *         (a) No neuron's V is NaN/Inf at any point.
 *         (b) No V exceeds E_ampa + 1 mV (CB cap).
 *         (c) No V drops below E_gaba_a - 1 mV (lower CB cap).
 *         (d) Mean firing rate across the population over the last 100 ms
 *             lies within [5 Hz, 100 Hz] — healthy biological band, not the
 *             0 Hz "dead" or 200+ Hz runaway that current-based PSCs hit
 *             without runaway brakes.
 * WHY:  Phase 5 cutover safety: before CB-GPU-4 flips real SNN populations
 *       over to this kernel on the pod, a long-run smoke test on a
 *       realistic-sized pool catches NaN / clamp / firing-rate issues that
 *       per-step unit tests miss.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <random>
#include <vector>

#include "nimcp.h"
#include "gpu/snn/nimcp_snn_gpu.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"

namespace {

class SnnCbGpuE2ETest : public ::testing::Test {
protected:
    static void SetUpTestSuite() { ASSERT_EQ(nimcp_init(), NIMCP_SUCCESS); }
    static void TearDownTestSuite() { nimcp_shutdown(); }

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
            GTEST_SKIP() << "no GPU available — CB GPU e2e test skipped";
        }
    }

    nimcp_gpu_context_t* gpu_ = nullptr;
    nimcp_lif_state_t*   state_ = nullptr;
    bool gpu_available_ = false;
};

TEST_F(SnnCbGpuE2ETest, LongRun_NoNaN_NoRunaway_HealthyFiring) {
    RequireGPU();

    constexpr size_t N = 1024;
    constexpr int    STEPS = 200;
    constexpr int    LAST_WINDOW = 100;     // last 100 ms for rate measurement
    constexpr float  DT = 1.0f;             // ms

    constexpr float V_REST   = -65.0f;
    constexpr float V_RESET  = -65.0f;
    constexpr float V_THRESH = -50.0f;
    constexpr float TAU_MEM  =  20.0f;
    constexpr float E_AMPA   =   0.0f;
    constexpr float E_NMDA   =   0.0f;
    constexpr float E_GABA_A = -75.0f;
    constexpr float E_GABA_B = -90.0f;
    constexpr float TAU_AMPA   =   2.0f;
    constexpr float TAU_NMDA   = 100.0f;
    constexpr float TAU_GABA_A =  10.0f;
    constexpr float TAU_GABA_B = 150.0f;
    constexpr float MG_MM      =   1.0f;

    nimcp_lif_params_t p = {};
    p.tau_mem  = TAU_MEM;  p.tau_syn = 5.0f;
    p.v_thresh = V_THRESH; p.v_reset = V_RESET;
    p.v_rest   = V_REST;   p.dt = DT;  p.hard_reset = true;
    state_ = nimcp_lif_state_create(gpu_, N, &p);
    ASSERT_NE(state_, nullptr);
    ASSERT_TRUE(nimcp_gpu_lif_state_alloc_cb_arrays(gpu_, state_, N));

    // Persistent host-side g buffers. We download g after each step so the
    // host buffer matches what the kernel just decayed, then deposit fresh
    // input on top.
    std::vector<float> g_ampa(N, 0.0f);
    std::vector<float> g_nmda(N, 0.0f);
    std::vector<float> g_gaba_a(N, 0.02f);   // small tonic inhibition
    std::vector<float> g_gaba_b(N, 0.0f);

    std::mt19937 rng(0x5EEDu);
    std::uniform_real_distribution<float> ampa_inc(0.0f, 0.10f);
    std::uniform_real_distribution<float> nmda_inc(0.0f, 0.05f);

    int total_spikes_in_window = 0;
    std::vector<float> spk(N, 0.0f);

    for (int s = 0; s < STEPS; ++s) {
        // Deposit fresh background drive on top of last decay.
        for (size_t i = 0; i < N; ++i) {
            g_ampa[i]   += ampa_inc(rng);
            g_nmda[i]   += nmda_inc(rng);
            // Tonic g_gaba_a — top up so the host baseline matches what the
            // CB hot loop would deliver each step.
            if (g_gaba_a[i] < 0.02f) g_gaba_a[i] = 0.02f;
        }

        ASSERT_TRUE(nimcp_gpu_lif_state_upload_g(
            gpu_, state_, g_ampa.data(), g_nmda.data(),
            g_gaba_a.data(), g_gaba_b.data(), N));
        ASSERT_TRUE(nimcp_gpu_lif_forward_cb(
            gpu_, state_,
            E_AMPA, E_NMDA, E_GABA_A, E_GABA_B,
            TAU_AMPA, TAU_NMDA, TAU_GABA_A, TAU_GABA_B,
            MG_MM));

        // Pull V + spikes back. V check every step (cheap), rate count only
        // in the trailing window (so initial transient doesn't bias).
        std::vector<float> v(N, 0.0f);
        ASSERT_TRUE(nimcp_gpu_tensor_to_host(state_->v, v.data()));
        for (size_t i = 0; i < N; ++i) {
            ASSERT_TRUE(std::isfinite(v[i]))
                << "Neuron " << i << " V is non-finite at step " << s
                << ": " << v[i];
            EXPECT_LE(v[i], E_AMPA + 1.0f)
                << "Neuron " << i << " V crossed E_ampa at step " << s;
            // Lower CB cap is the most-negative E_r that any active receptor
            // pulls toward. With g_gaba_b=0 throughout this test, only
            // GABA_A (E=-75) actually drags V down, so that's the bound.
            EXPECT_GE(v[i], E_GABA_A - 1.0f)
                << "Neuron " << i << " V crossed E_gaba_a at step " << s;
        }

        ASSERT_TRUE(nimcp_gpu_tensor_to_host(state_->spikes, spk.data()));
        if (s >= STEPS - LAST_WINDOW) {
            for (size_t i = 0; i < N; ++i) {
                if (spk[i] > 0.5f) ++total_spikes_in_window;
            }
        }

        // Re-sync host g_* with the kernel's decayed state.
        ASSERT_TRUE(nimcp_gpu_lif_state_download_g(
            gpu_, state_, g_ampa.data(), g_nmda.data(),
            g_gaba_a.data(), g_gaba_b.data(), N));
    }

    // Mean firing rate over the last LAST_WINDOW ms.
    const float window_seconds = LAST_WINDOW * DT * 1e-3f;
    const float mean_rate_hz = static_cast<float>(total_spikes_in_window)
                               / static_cast<float>(N) / window_seconds;

    EXPECT_GT(mean_rate_hz,   5.0f) << "Population is dead (rate too low)";
    EXPECT_LT(mean_rate_hz, 100.0f) << "Population is in runaway (rate too high)";
}

}  // namespace
