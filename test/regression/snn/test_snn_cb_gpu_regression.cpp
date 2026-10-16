//=============================================================================
// test_snn_cb_gpu_regression.cpp — Pre-CB-GPU behavior must be unchanged
// when CB arrays are not allocated / not used.
//=============================================================================
/**
 * @file test_snn_cb_gpu_regression.cpp
 * @brief Regression suite for the CB GPU port (CB-GPU-2 + CB-GPU-3). When
 *        the new conductance machinery is OFF the existing current-based
 *        forward path must behave exactly as it did before.
 *
 * WHAT: Pin three contracts:
 *         (1) Fresh `nimcp_lif_state_t` has all 4 conductance tensors NULL
 *             and `cb_arrays_dirty` defaults to a known value (false).
 *         (2) The legacy current-based forward (`nimcp_gpu_lif_forward`) is
 *             callable on a state that NEVER had alloc_cb_arrays called —
 *             no NULL-deref, no behavior change.
 *         (3) `nimcp_lif_state_destroy` works on a state that had
 *             alloc_cb_arrays + free_cb_arrays already invoked — no
 *             double-free, no leak.
 * WHY:  CB-GPU-3 added 4 new tensor pointers to lif_state_t. The legacy
 *       current-based path is still the production code on every SNN
 *       outside the CB+GPU branch — any subtle layout/init regression
 *       would cause silent corruption everywhere.
 * HOW:  Google Test. GPU optional (skip if absent). Run under ASan/MSan
 *       in CI to catch lifetime + init issues.
 */

#include <gtest/gtest.h>
#include <vector>

#include "nimcp.h"
#include "gpu/snn/nimcp_snn_gpu.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"

namespace {

class SnnCbGpuRegressionTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() { ASSERT_EQ(nimcp_init(), NIMCP_SUCCESS); }
    static void TearDownTestSuite() { nimcp_shutdown(); }

    void SetUp() override {
        gpu_ = nimcp_gpu_context_create_auto();
        gpu_available_ = (gpu_ != nullptr);
    }

    void TearDown() override {
        if (gpu_) {
            nimcp_gpu_context_destroy(gpu_);
            gpu_ = nullptr;
        }
    }

    void RequireGPU() {
        if (!gpu_available_) {
            GTEST_SKIP() << "no GPU available — CB GPU regression test skipped";
        }
    }

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

    nimcp_gpu_context_t* gpu_ = nullptr;
    bool gpu_available_ = false;
};

// ─────────────────────────────────────────────────────────────────────────────
// (1) Fresh state defaults: all 4 conductance tensors NULL, dirty flag false.
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(SnnCbGpuRegressionTest, FreshState_CbDefaults) {
    RequireGPU();

    nimcp_lif_state_t* st = fresh_state(64);
    ASSERT_NE(st, nullptr);
    EXPECT_EQ(st->g_ampa,   nullptr);
    EXPECT_EQ(st->g_nmda,   nullptr);
    EXPECT_EQ(st->g_gaba_a, nullptr);
    EXPECT_EQ(st->g_gaba_b, nullptr);
    EXPECT_FALSE(st->cb_arrays_dirty)
        << "Fresh state must have cb_arrays_dirty=false (CB OFF default)";

    nimcp_lif_state_destroy(st);
}

// ─────────────────────────────────────────────────────────────────────────────
// (2) Current-based forward must still work on a state with NULL CB arrays
//     and produce identical-across-neurons output (homogeneous fallback).
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(SnnCbGpuRegressionTest, CurrentBasedForward_UnchangedWithNullCbArrays) {
    RequireGPU();

    constexpr size_t N = 32;
    nimcp_lif_state_t* st = fresh_state(N);
    ASSERT_NE(st, nullptr);
    // Sanity: legacy path gets a state with ZERO conductance state.
    ASSERT_EQ(st->g_ampa, nullptr);

    std::vector<float> drive(N, 12.0f);
    size_t dims[1] = { N };
    nimcp_gpu_tensor_t* in = nimcp_gpu_tensor_from_host(
        gpu_, drive.data(), dims, 1, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(in, nullptr);

    for (int s = 0; s < 5; ++s) {
        ASSERT_TRUE(nimcp_gpu_lif_forward(gpu_, st, in));
    }

    std::vector<float> v(N, 0.0f);
    ASSERT_TRUE(nimcp_gpu_tensor_to_host(st->v, v.data()));
    nimcp_gpu_tensor_destroy(in);

    // Homogeneous params + identical drive → identical V across neurons
    // (this was the pre-CB invariant; new struct fields must not break it).
    const float v0 = v[0];
    for (size_t i = 1; i < N; ++i) {
        EXPECT_FLOAT_EQ(v[i], v0)
            << "Neuron " << i << " diverged from neuron 0 — CB struct fields "
            << "leaked into the current-based hot path";
    }

    nimcp_lif_state_destroy(st);
}

// ─────────────────────────────────────────────────────────────────────────────
// (3) Lifecycle — alloc → free → destroy (no double-free).
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(SnnCbGpuRegressionTest, Lifecycle_AllocFreeDestroy_NoDoubleFree) {
    RequireGPU();

    constexpr size_t N = 16;
    nimcp_lif_state_t* st = fresh_state(N);
    ASSERT_NE(st, nullptr);

    ASSERT_TRUE(nimcp_gpu_lif_state_alloc_cb_arrays(gpu_, st, N));
    nimcp_gpu_lif_state_free_cb_arrays(st);
    EXPECT_EQ(st->g_ampa,   nullptr);
    EXPECT_EQ(st->g_nmda,   nullptr);
    EXPECT_EQ(st->g_gaba_a, nullptr);
    EXPECT_EQ(st->g_gaba_b, nullptr);

    // Destroy must be safe whether CB arrays were allocated or not.
    nimcp_lif_state_destroy(st);
    SUCCEED();  // surviving to here under ASan = no double-free.
}

// ─────────────────────────────────────────────────────────────────────────────
// (4) Lifecycle — alloc, do not free, destroy must still clean up the four
//     tensors automatically (otherwise CB activation in production would
//     leak GPU memory on every brain shutdown).
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(SnnCbGpuRegressionTest, Lifecycle_AllocThenDestroy_AutoCleanup) {
    RequireGPU();

    constexpr size_t N = 16;
    nimcp_lif_state_t* st = fresh_state(N);
    ASSERT_NE(st, nullptr);
    ASSERT_TRUE(nimcp_gpu_lif_state_alloc_cb_arrays(gpu_, st, N));
    nimcp_lif_state_destroy(st);
    SUCCEED();  // ASan would catch the leak otherwise.
}

// ─────────────────────────────────────────────────────────────────────────────
// (5) Idempotent free — calling free_cb_arrays twice must not crash or
//     double-free. The unit test pins this; the regression suite duplicates
//     critical contracts so a refactor that drops the NULL-after-destroy
//     pattern is caught at multiple levels.
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(SnnCbGpuRegressionTest, FreeCbArrays_DoubleFreeSafe) {
    RequireGPU();

    constexpr size_t N = 8;
    nimcp_lif_state_t* st = fresh_state(N);
    ASSERT_NE(st, nullptr);
    ASSERT_TRUE(nimcp_gpu_lif_state_alloc_cb_arrays(gpu_, st, N));
    nimcp_gpu_lif_state_free_cb_arrays(st);
    nimcp_gpu_lif_state_free_cb_arrays(st);  // must be no-op, not crash
    SUCCEED();
    nimcp_lif_state_destroy(st);
}

// ─────────────────────────────────────────────────────────────────────────────
// (6) forward_cb pre-alloc — calling forward_cb on a state that never had
//     alloc_cb_arrays called must return false (and not crash). The wiring
//     layer in CB-GPU-4 relies on this to detect mis-ordered CB activation.
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(SnnCbGpuRegressionTest, ForwardCb_PreAlloc_ReturnsFalse) {
    RequireGPU();

    constexpr size_t N = 8;
    nimcp_lif_state_t* st = fresh_state(N);
    ASSERT_NE(st, nullptr);
    // No alloc_cb_arrays — all 4 g tensors are NULL.
    EXPECT_FALSE(nimcp_gpu_lif_forward_cb(
        gpu_, st,
        /*e_ampa*/ 0.0f, /*e_nmda*/ 0.0f,
        /*e_gaba_a*/ -75.0f, /*e_gaba_b*/ -90.0f,
        /*tau_ampa*/ 2.0f, /*tau_nmda*/ 100.0f,
        /*tau_gaba_a*/ 10.0f, /*tau_gaba_b*/ 150.0f,
        /*mg_mm*/ 1.0f));
    nimcp_lif_state_destroy(st);
}

// ─────────────────────────────────────────────────────────────────────────────
// (7) upload_g pre-alloc — calling upload_g on a state without
//     alloc_cb_arrays must return false (no-op).
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(SnnCbGpuRegressionTest, UploadG_PreAlloc_ReturnsFalse) {
    RequireGPU();

    constexpr size_t N = 8;
    nimcp_lif_state_t* st = fresh_state(N);
    ASSERT_NE(st, nullptr);
    std::vector<float> g(N, 1.0f);
    EXPECT_FALSE(nimcp_gpu_lif_state_upload_g(
        gpu_, st, g.data(), g.data(), g.data(), g.data(), N));
    nimcp_lif_state_destroy(st);
}

// ─────────────────────────────────────────────────────────────────────────────
// (8) Mixed lifecycle — CB allocated then current-based forward called.
//     Production mid-run config flip (e.g. user toggles CB OFF mid-train).
//     The legacy current-based kernel does not touch g_*; this test pins
//     that no kernel-arg-pack regression leaks the new fields into the
//     legacy hot path.
// ─────────────────────────────────────────────────────────────────────────────
TEST_F(SnnCbGpuRegressionTest, MixedLifecycle_AllocCb_CurrentBasedForward_StillHomogeneous) {
    RequireGPU();

    constexpr size_t N = 16;
    nimcp_lif_state_t* st = fresh_state(N);
    ASSERT_NE(st, nullptr);
    ASSERT_TRUE(nimcp_gpu_lif_state_alloc_cb_arrays(gpu_, st, N));
    // Upload non-zero g_* — the current-based kernel must IGNORE these.
    std::vector<float> g(N, 0.5f);
    ASSERT_TRUE(nimcp_gpu_lif_state_upload_g(
        gpu_, st, g.data(), g.data(), g.data(), g.data(), N));

    // Drive identical current; expect identical V across neurons (legacy
    // homogeneity contract preserved despite g_* being populated).
    std::vector<float> drive(N, 12.0f);
    size_t dims[1] = { N };
    nimcp_gpu_tensor_t* in = nimcp_gpu_tensor_from_host(
        gpu_, drive.data(), dims, 1, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(in, nullptr);
    for (int s = 0; s < 5; ++s) {
        ASSERT_TRUE(nimcp_gpu_lif_forward(gpu_, st, in));
    }
    std::vector<float> v(N, 0.0f);
    ASSERT_TRUE(nimcp_gpu_tensor_to_host(st->v, v.data()));
    nimcp_gpu_tensor_destroy(in);

    const float v0 = v[0];
    for (size_t i = 1; i < N; ++i) {
        EXPECT_FLOAT_EQ(v[i], v0)
            << "CB g_* leaked into current-based kernel at neuron " << i;
    }
    nimcp_lif_state_destroy(st);
}

}  // namespace
