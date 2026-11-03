//=============================================================================
// test_snn_gpu_refractory_integration.cpp — GPU homeostasis parity (2026-11-03)
//=============================================================================
/**
 * @file test_snn_gpu_refractory_integration.cpp
 * @brief Pins the current-based GPU LIF kernel to the CPU step semantics
 *        for the refractory period, the intrinsic-plasticity threshold
 *        offset, and short-term depression in the CSR synaptic-current
 *        kernel.
 *
 * WHY:  Before 2026-11-03 the current-based GPU kernel had no refractory
 *       period and ignored the threshold offset, so a strongly driven
 *       population fired on every tick (Vast: all 348K L4 neurons for
 *       12+ consecutive ticks). The CPU path sets ref = t_ref on a spike
 *       and skips the neuron while ref > 0, decrementing by dt.
 *
 * HOW:  Small populations driven directly through nimcp_gpu_lif_forward;
 *       spikes and state read back each step. Skipped without a GPU.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

#include "nimcp.h"
#include "gpu/snn/nimcp_snn_gpu.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"

namespace {

constexpr float V_REST   = -65.0f;
constexpr float V_RESET  = -65.0f;
constexpr float V_THRESH = -50.0f;
constexpr float TAU_MEM  = 20.0f;
constexpr float TAU_SYN  = 5.0f;
constexpr float DT       = 1.0f;

class SnnGpuRefractoryTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() { ASSERT_EQ(nimcp_init(), NIMCP_SUCCESS); }
    static void TearDownTestSuite() { nimcp_shutdown(); }

    void SetUp() override {
        gpu_ = nimcp_gpu_context_create_auto();
    }

    void TearDown() override {
        if (state_) { nimcp_lif_state_destroy(state_); state_ = nullptr; }
        if (gpu_)   { nimcp_gpu_context_destroy(gpu_); gpu_ = nullptr; }
    }

    void RequireGPU() {
        if (!gpu_) GTEST_SKIP() << "no GPU available";
    }

    void CreateState(size_t n) {
        nimcp_lif_params_t p = {};
        p.tau_mem = TAU_MEM;   p.tau_syn = TAU_SYN;
        p.v_thresh = V_THRESH; p.v_reset = V_RESET;
        p.v_rest = V_REST;     p.dt = DT;  p.hard_reset = true;
        state_ = nimcp_lif_state_create(gpu_, n, &p);
        ASSERT_NE(state_, nullptr);
    }

    /** Run one LIF step with the given per-neuron input; return spikes. */
    std::vector<float> Step(const std::vector<float>& input) {
        size_t dims[1] = { input.size() };
        nimcp_gpu_tensor_t* in = nimcp_gpu_tensor_from_host(
            gpu_, input.data(), dims, 1, NIMCP_GPU_PRECISION_FP32);
        EXPECT_NE(in, nullptr);
        std::vector<float> spikes(input.size(), -1.0f);
        if (!in) return spikes;
        EXPECT_TRUE(nimcp_gpu_lif_forward(gpu_, state_, in));
        EXPECT_TRUE(nimcp_gpu_tensor_to_host(state_->spikes, spikes.data()));
        nimcp_gpu_tensor_destroy(in);
        return spikes;
    }

    nimcp_gpu_context_t* gpu_ = nullptr;
    nimcp_lif_state_t*   state_ = nullptr;
};

// Strong constant drive crosses threshold on every step without refractory.
constexpr float STRONG_DRIVE = 1000.0f;

// ─────────────────────────────────────────────────────────────────────────────
// Without a t_ref upload the kernel keeps its pre-2026-11-03 behavior:
// a saturating drive fires every step.
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(SnnGpuRefractoryTest, NoTref_StrongDriveFiresEveryStep) {
    RequireGPU();
    CreateState(4);
    std::vector<float> drive(4, STRONG_DRIVE);
    for (int s = 0; s < 20; ++s) {
        auto spk = Step(drive);
        for (size_t i = 0; i < spk.size(); ++i) {
            EXPECT_FLOAT_EQ(spk[i], 1.0f) << "step " << s << " neuron " << i;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// With per-neuron t_ref the inter-spike interval is t_ref/dt + 1 steps
// (CPU semantics: spike sets ref=t_ref; skipped while ref > 0; ref -= dt).
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(SnnGpuRefractoryTest, PerNeuronTref_LimitsFiringToCpuPeriod) {
    RequireGPU();
    constexpr size_t N = 3;
    CreateState(N);
    const std::vector<float> t_ref = { 1.0f, 2.0f, 4.0f };   // PV, default, SOM
    ASSERT_TRUE(nimcp_gpu_lif_state_upload_t_ref(gpu_, state_, t_ref.data(), N));
    ASSERT_NE(state_->refractory_timer, nullptr);
    ASSERT_NE(state_->t_ref_per_neuron, nullptr);

    constexpr int STEPS = 30;
    std::vector<std::vector<int>> spike_steps(N);
    std::vector<float> drive(N, STRONG_DRIVE);
    for (int s = 0; s < STEPS; ++s) {
        auto spk = Step(drive);
        for (size_t i = 0; i < N; ++i) {
            if (spk[i] > 0.5f) spike_steps[i].push_back(s);
        }
    }

    for (size_t i = 0; i < N; ++i) {
        const int period = static_cast<int>(t_ref[i] / DT) + 1;
        ASSERT_FALSE(spike_steps[i].empty()) << "neuron " << i << " never fired";
        EXPECT_EQ(spike_steps[i].front(), 0) << "neuron " << i;
        for (size_t k = 1; k < spike_steps[i].size(); ++k) {
            EXPECT_EQ(spike_steps[i][k] - spike_steps[i][k - 1], period)
                << "neuron " << i << " ISI at spike " << k;
        }
        const int expected = (STEPS + period - 1) / period;
        EXPECT_EQ(static_cast<int>(spike_steps[i].size()), expected)
            << "neuron " << i << " spike count";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// During refractory the membrane is held at reset (no integration).
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(SnnGpuRefractoryTest, RefractoryHoldsMembraneAtReset) {
    RequireGPU();
    CreateState(1);
    const float t_ref = 2.0f;
    ASSERT_TRUE(nimcp_gpu_lif_state_upload_t_ref(gpu_, state_, &t_ref, 1));

    auto spk0 = Step({ STRONG_DRIVE });
    ASSERT_FLOAT_EQ(spk0[0], 1.0f);
    for (int s = 1; s <= 2; ++s) {
        auto spk = Step({ STRONG_DRIVE });
        EXPECT_FLOAT_EQ(spk[0], 0.0f) << "refractory step " << s;
        float v = 0.0f;
        ASSERT_TRUE(nimcp_gpu_tensor_to_host(state_->v, &v));
        EXPECT_FLOAT_EQ(v, V_RESET) << "refractory step " << s;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Uploading NULL t_ref frees the arrays and restores firing every step.
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(SnnGpuRefractoryTest, NullTrefDisablesRefractory) {
    RequireGPU();
    CreateState(2);
    const std::vector<float> t_ref(2, 2.0f);
    ASSERT_TRUE(nimcp_gpu_lif_state_upload_t_ref(gpu_, state_, t_ref.data(), 2));
    ASSERT_TRUE(nimcp_gpu_lif_state_upload_t_ref(gpu_, state_, nullptr, 2));
    EXPECT_EQ(state_->refractory_timer, nullptr);
    EXPECT_EQ(state_->t_ref_per_neuron, nullptr);

    std::vector<float> drive(2, STRONG_DRIVE);
    for (int s = 0; s < 10; ++s) {
        auto spk = Step(drive);
        EXPECT_FLOAT_EQ(spk[0], 1.0f) << "step " << s;
        EXPECT_FLOAT_EQ(spk[1], 1.0f) << "step " << s;
    }
}

TEST_F(SnnGpuRefractoryTest, UploadRejectsSizeMismatch) {
    RequireGPU();
    CreateState(4);
    const std::vector<float> buf(3, 1.0f);
    EXPECT_FALSE(nimcp_gpu_lif_state_upload_t_ref(gpu_, state_, buf.data(), 3));
    EXPECT_FALSE(nimcp_gpu_lif_state_upload_thresh_offset(gpu_, state_, buf.data(), 3));
}

// ─────────────────────────────────────────────────────────────────────────────
// Threshold offset: first step from rest with i_syn=0 gives
//   v = v_rest + (1 - exp(-dt/tau_mem)) * input.
// Pick inputs that land just above / below the base threshold and check the
// offset moves the decision both ways.
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(SnnGpuRefractoryTest, ThresholdOffsetShiftsSpikeDecision) {
    RequireGPU();
    constexpr size_t N = 4;
    CreateState(N);
    const float gain = 1.0f - std::exp(-DT / TAU_MEM);
    const float in_above = (V_THRESH + 1.0f - V_REST) / gain;  // v = -49 mV
    const float in_below = (V_THRESH - 3.0f - V_REST) / gain;  // v = -53 mV

    // n0: -49 mV, offset 0   → spike
    // n1: -49 mV, offset +5  → threshold -45 → no spike
    // n2: -53 mV, offset 0   → no spike
    // n3: -53 mV, offset -5  → threshold -55 → spike
    const std::vector<float> offset = { 0.0f, 5.0f, 0.0f, -5.0f };
    ASSERT_TRUE(nimcp_gpu_lif_state_upload_thresh_offset(
        gpu_, state_, offset.data(), N));
    ASSERT_NE(state_->thresh_offset, nullptr);

    auto spk = Step({ in_above, in_above, in_below, in_below });
    EXPECT_FLOAT_EQ(spk[0], 1.0f);
    EXPECT_FLOAT_EQ(spk[1], 0.0f);
    EXPECT_FLOAT_EQ(spk[2], 0.0f);
    EXPECT_FLOAT_EQ(spk[3], 1.0f);

    ASSERT_TRUE(nimcp_gpu_lif_state_upload_thresh_offset(gpu_, state_, nullptr, N));
    EXPECT_EQ(state_->thresh_offset, nullptr);
}

// ─────────────────────────────────────────────────────────────────────────────
// CSR synaptic current: a spiking source carries (1 - depression), which
// scales its weight; 0/1 spike vectors give the pre-2026-11-03 result.
// ─────────────────────────────────────────────────────────────────────────────
class SnnGpuIsynDepressionTest : public SnnGpuRefractoryTest {
protected:
    /** dest0 <- src0 (w=2), src1 (w=3); dest1 <- src1 (w=4). */
    std::vector<float> RunIsyn(const std::vector<float>& spike_vec) {
        const std::vector<float> weights = { 2.0f, 3.0f, 4.0f };
        const std::vector<unsigned int> col = { 0u, 1u, 1u };
        const std::vector<unsigned int> row = { 0u, 2u, 3u };
        const std::vector<float> ext(2, 0.0f);
        std::vector<float> out(2, -1.0f);

        size_t d_spk[1] = { spike_vec.size() };
        size_t d_w[1] = { weights.size() };
        size_t d_r[1] = { row.size() };
        size_t d_n[1] = { 2 };
        auto* t_spk = nimcp_gpu_tensor_from_host(gpu_, spike_vec.data(), d_spk, 1, NIMCP_GPU_PRECISION_FP32);
        auto* t_w   = nimcp_gpu_tensor_from_host(gpu_, weights.data(),   d_w,   1, NIMCP_GPU_PRECISION_FP32);
        auto* t_c   = nimcp_gpu_tensor_from_host(gpu_, col.data(),       d_w,   1, NIMCP_GPU_PRECISION_UINT32);
        auto* t_r   = nimcp_gpu_tensor_from_host(gpu_, row.data(),       d_r,   1, NIMCP_GPU_PRECISION_UINT32);
        auto* t_e   = nimcp_gpu_tensor_from_host(gpu_, ext.data(),       d_n,   1, NIMCP_GPU_PRECISION_FP32);
        auto* t_o   = nimcp_gpu_tensor_from_host(gpu_, out.data(),       d_n,   1, NIMCP_GPU_PRECISION_FP32);
        EXPECT_TRUE(t_spk && t_w && t_c && t_r && t_e && t_o);
        if (t_spk && t_w && t_c && t_r && t_e && t_o) {
            EXPECT_TRUE(nimcp_gpu_snn_isyn_csr(gpu_,
                (const float*)t_spk->data, (const float*)t_w->data,
                (const unsigned int*)t_c->data, (const unsigned int*)t_r->data,
                (const float*)t_e->data, (float*)t_o->data, 2));
            EXPECT_TRUE(nimcp_gpu_tensor_to_host(t_o, out.data()));
        }
        for (auto* t : { t_spk, t_w, t_c, t_r, t_e, t_o }) {
            if (t) nimcp_gpu_tensor_destroy(t);
        }
        return out;
    }
};

TEST_F(SnnGpuIsynDepressionTest, BinarySpikesUnchanged) {
    RequireGPU();
    auto out = RunIsyn({ 1.0f, 0.0f });
    EXPECT_FLOAT_EQ(out[0], 2.0f);
    EXPECT_FLOAT_EQ(out[1], 0.0f);
    out = RunIsyn({ 1.0f, 1.0f });
    EXPECT_FLOAT_EQ(out[0], 5.0f);
    EXPECT_FLOAT_EQ(out[1], 4.0f);
}

TEST_F(SnnGpuIsynDepressionTest, DepressedSourceScalesWeight) {
    RequireGPU();
    // src1 spiked with depression 0.5 → carries 0.5.
    auto out = RunIsyn({ 1.0f, 0.5f });
    EXPECT_FLOAT_EQ(out[0], 2.0f + 1.5f);
    EXPECT_FLOAT_EQ(out[1], 2.0f);
}

}  // namespace
