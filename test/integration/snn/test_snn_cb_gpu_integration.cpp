//=============================================================================
// test_snn_cb_gpu_integration.cpp — GPU CB ⇄ CPU CB bit-near-identity
// (CB-GPU-3 + CB-GPU-5).
//=============================================================================
/**
 * @file test_snn_cb_gpu_integration.cpp
 * @brief Multi-neuron CB integration: drive identical g_ampa / g_nmda /
 *        g_gaba_a / g_gaba_b inputs through the GPU `nimcp_gpu_lif_forward_cb`
 *        kernel and the CPU `snn_membrane_compute_dv` reference, step-for-step,
 *        and verify the resulting V trajectories agree within FP32 numerics.
 *
 * WHAT: 32-neuron pool, 50 steps, mixed E/I drive (replenished each step),
 *       NMDA Mg block enabled (mg_mm = 1 mM). After each step the GPU V
 *       and CPU V are compared. Threshold is set high enough that no
 *       neuron spikes — the test isolates pure membrane integration.
 * WHY:  CB-GPU-4 will swap snn_network_step over to the GPU CB kernel.
 *       This contract pins the GPU kernel to the proven CPU reference so
 *       a regression in either side is caught immediately.
 * HOW:  Tolerance: per-neuron |Δv| < 1e-3 mV averaged over 50 steps with
 *       max < 1e-2. FP32 round-off + per-step decay accumulation makes
 *       tighter bounds unrealistic; this is well above what would mask a
 *       real divergence.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

#include "nimcp.h"
#include "gpu/snn/nimcp_snn_gpu.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"

extern "C" {
#include "snn/nimcp_snn_membrane.h"
}

namespace {

class SnnCbGpuIntegrationTest : public ::testing::Test {
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
            GTEST_SKIP() << "no GPU available — CB GPU integration test skipped";
        }
    }

    nimcp_gpu_context_t* gpu_ = nullptr;
    nimcp_lif_state_t*   state_ = nullptr;
    bool gpu_available_ = false;
};

// ─────────────────────────────────────────────────────────────────────────────
// 32 neurons, 50 steps, mixed E/I + NMDA + Mg block. GPU vs CPU.
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(SnnCbGpuIntegrationTest, MultiNeuronCb_GpuMatchesCpuReference) {
    RequireGPU();

    constexpr size_t N = 32;
    constexpr int STEPS = 50;
    constexpr float V_REST = -65.0f;
    constexpr float TAU_MEM = 20.0f;
    constexpr float DT = 1.0f;
    constexpr float V_THRESH = +100.0f;          // never spike — isolate membrane
    constexpr float V_RESET  = -65.0f;
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

    // Pin the GPU initial-V contract: state->v must be initialized to v_rest
    // (otherwise step-0 dynamics would diverge from the CPU mirror that
    // starts from V_REST, but might still converge under tolerance).
    {
        std::vector<float> v_init(N, 0.0f);
        ASSERT_TRUE(nimcp_gpu_tensor_to_host(state_->v, v_init.data()));
        for (size_t i = 0; i < N; ++i) {
            EXPECT_NEAR(v_init[i], V_REST, 1e-5f)
                << "GPU v[" << i << "] not initialized to v_rest";
        }
    }

    // CPU mirror state.
    std::vector<float> v_cpu(N, V_REST);
    std::vector<float> g_ampa(N), g_nmda(N), g_gaba_a(N), g_gaba_b(N);
    for (size_t i = 0; i < N; ++i) {
        // Per-neuron heterogeneous drive — favors a noisy but reproducible mix.
        const float fi = static_cast<float>(i);
        g_ampa[i]   = 0.05f + 0.01f * fi;
        g_nmda[i]   = 0.10f + 0.005f * fi;
        g_gaba_a[i] = 0.02f * (static_cast<float>(N - i));
        g_gaba_b[i] = 0.005f;
    }

    // Pre-compute decay factors (host-side, same as CPU + kernel wrapper).
    const float decay_ampa   = std::exp(-DT / TAU_AMPA);
    const float decay_nmda   = std::exp(-DT / TAU_NMDA);
    const float decay_gaba_a = std::exp(-DT / TAU_GABA_A);
    const float decay_gaba_b = std::exp(-DT / TAU_GABA_B);

    double max_abs_diff = 0.0;
    double sum_abs_diff = 0.0;
    int    samples = 0;

    for (int s = 0; s < STEPS; ++s) {
        // Replenish (mimic per-step deposit — no synaptic events here, just
        // a fresh "instantaneous deposit" each tick so we can compare across
        // many steps without saturation hiding bugs).
        std::vector<float> g_ampa_step   = g_ampa;
        std::vector<float> g_nmda_step   = g_nmda;
        std::vector<float> g_gaba_a_step = g_gaba_a;
        std::vector<float> g_gaba_b_step = g_gaba_b;

        // GPU side: upload the just-deposited g_*, run kernel, download V.
        ASSERT_TRUE(nimcp_gpu_lif_state_upload_g(
            gpu_, state_, g_ampa_step.data(), g_nmda_step.data(),
            g_gaba_a_step.data(), g_gaba_b_step.data(), N));
        ASSERT_TRUE(nimcp_gpu_lif_forward_cb(
            gpu_, state_,
            E_AMPA, E_NMDA, E_GABA_A, E_GABA_B,
            TAU_AMPA, TAU_NMDA, TAU_GABA_A, TAU_GABA_B,
            MG_MM));

        std::vector<float> v_gpu(N, 0.0f);
        ASSERT_TRUE(nimcp_gpu_tensor_to_host(state_->v, v_gpu.data()));

        // CPU mirror: same equation, same per-step decay AFTER membrane update.
        for (size_t i = 0; i < N; ++i) {
            float dv = snn_membrane_compute_dv(
                v_cpu[i], V_REST, TAU_MEM, DT,
                /*i_syn=*/0.0f,
                g_ampa_step[i], g_nmda_step[i],
                g_gaba_a_step[i], g_gaba_b_step[i],
                E_AMPA, E_NMDA, E_GABA_A, E_GABA_B,
                MG_MM, /*conductance_mode=*/true);
            v_cpu[i] += dv;
            // Per-receptor decay applied AFTER membrane update — kernel does
            // the same.
            g_ampa_step[i]   *= decay_ampa;
            g_nmda_step[i]   *= decay_nmda;
            g_gaba_a_step[i] *= decay_gaba_a;
            g_gaba_b_step[i] *= decay_gaba_b;
            // Update the persistent CPU "deposit baseline" so next step starts
            // from the same decayed level the GPU kernel does.
            g_ampa[i]   = g_ampa_step[i];
            g_nmda[i]   = g_nmda_step[i];
            g_gaba_a[i] = g_gaba_a_step[i];
            g_gaba_b[i] = g_gaba_b_step[i];
        }

        // Compare V step-by-step.
        for (size_t i = 0; i < N; ++i) {
            const double d = std::fabs(static_cast<double>(v_gpu[i]) - v_cpu[i]);
            if (d > max_abs_diff) max_abs_diff = d;
            sum_abs_diff += d;
            ++samples;
        }
    }

    const double mean_abs_diff = sum_abs_diff / static_cast<double>(samples);

    // FP32 + per-step decay accumulation — these tolerances are loose enough
    // to avoid flakes but tight enough to catch real divergence (e.g. wrong
    // reversal-bound clamp, wrong Mg block formula).
    EXPECT_LT(mean_abs_diff, 1e-3) << "GPU vs CPU CB mean |Δv| too large";
    EXPECT_LT(max_abs_diff,  1e-2) << "GPU vs CPU CB max  |Δv| too large";
}

// ─────────────────────────────────────────────────────────────────────────────
// Strong-drive reversal-bound clamp — drive g_ampa hard enough that V wants
// to overshoot E_ampa, then verify the GPU and CPU clamp logic both pin V at
// E_ampa identically. This exercises the kernel's `if (v_after > e_max) dv =
// e_max - V` branch (lines 591-597 of nimcp_snn_kernels.cu) which the main
// integration test never trips.
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(SnnCbGpuIntegrationTest, ReversalClamp_StrongAmpaDrive_BothPinAtEAmpa) {
    RequireGPU();

    constexpr size_t N = 8;
    constexpr int STEPS = 30;
    constexpr float V_REST = -65.0f, DT = 1.0f, TAU_MEM = 20.0f;
    constexpr float V_THRESH = +100.0f, V_RESET = -65.0f;
    constexpr float E_AMPA = 0.0f,  E_NMDA = 0.0f;
    constexpr float E_GABA_A = -75.0f, E_GABA_B = -90.0f;
    constexpr float TAU_AMPA = 2.0f, TAU_NMDA = 100.0f;
    constexpr float TAU_GABA_A = 10.0f, TAU_GABA_B = 150.0f;
    constexpr float MG_MM = 0.0f;  // disable Mg block for clean clamp test

    nimcp_lif_params_t p = {};
    p.tau_mem  = TAU_MEM;  p.tau_syn = 5.0f;
    p.v_thresh = V_THRESH; p.v_reset = V_RESET;
    p.v_rest   = V_REST;   p.dt = DT;  p.hard_reset = true;
    state_ = nimcp_lif_state_create(gpu_, N, &p);
    ASSERT_NE(state_, nullptr);
    ASSERT_TRUE(nimcp_gpu_lif_state_alloc_cb_arrays(gpu_, state_, N));

    std::vector<float> v_cpu(N, V_REST);
    // Strong AMPA drive — re-deposit large g each step to overwhelm the
    // 100-mV-per-step dv clamp and exercise the reversal bound.
    constexpr float G_DEPOSIT = 50.0f;

    for (int s = 0; s < STEPS; ++s) {
        std::vector<float> g_ampa(N, G_DEPOSIT);
        std::vector<float> zero(N, 0.0f);

        ASSERT_TRUE(nimcp_gpu_lif_state_upload_g(
            gpu_, state_, g_ampa.data(), zero.data(),
            zero.data(), zero.data(), N));
        ASSERT_TRUE(nimcp_gpu_lif_forward_cb(
            gpu_, state_,
            E_AMPA, E_NMDA, E_GABA_A, E_GABA_B,
            TAU_AMPA, TAU_NMDA, TAU_GABA_A, TAU_GABA_B,
            MG_MM));

        // CPU mirror — same equation. compute_dv applies the same reversal
        // clamp so we expect bit-near-identity at the bound.
        for (size_t i = 0; i < N; ++i) {
            float dv = snn_membrane_compute_dv(
                v_cpu[i], V_REST, TAU_MEM, DT, 0.0f,
                G_DEPOSIT, 0.0f, 0.0f, 0.0f,
                E_AMPA, E_NMDA, E_GABA_A, E_GABA_B,
                MG_MM, /*conductance_mode=*/true);
            v_cpu[i] += dv;
        }
    }

    std::vector<float> v_gpu(N, 0.0f);
    ASSERT_TRUE(nimcp_gpu_tensor_to_host(state_->v, v_gpu.data()));

    // Real contract: GPU and CPU implement the same reversal-bound clamp, so
    // the trajectory under heavy AMPA drive must be bit-near-identical. The
    // exact end-state is governed by the (clamp + |dv|≤100 + per-step
    // integration) interaction — we don't pin a specific value, only that
    // (a) V is depolarized far above rest (clamp branch did fire), (b) V
    // never crosses E_ampa, (c) GPU and CPU agree.
    for (size_t i = 0; i < N; ++i) {
        EXPECT_GT(v_gpu[i], -10.0f)
            << "GPU V did not approach E_ampa (i=" << i << "): " << v_gpu[i];
        EXPECT_LE(v_gpu[i], E_AMPA + 1e-3f)
            << "GPU V crossed E_ampa (i=" << i << "): " << v_gpu[i];
        EXPECT_NEAR(v_gpu[i], v_cpu[i], 1e-4f)
            << "GPU/CPU clamp diverged (i=" << i
            << ", gpu=" << v_gpu[i] << ", cpu=" << v_cpu[i] << ")";
    }
}

}  // namespace
