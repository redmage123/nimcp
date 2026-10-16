/**
 * @file test_snn_adaptation.cpp
 * @brief Unit tests for shared SNN adaptation mechanism (AHP / Na-K pump).
 *
 * WHAT: Covers snn_adaptation_create/destroy/reset/update/compute_hyperpol
 * WHY:  DRY adaptation state is used for both fast M-current and slow pump —
 *       a bug here affects both cell-intrinsic brakes simultaneously.
 * HOW:  GoogleTest; no SNN network dependency — the adaptation state is
 *       self-contained and must be testable in isolation.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

extern "C" {
#include "snn/nimcp_snn_adaptation.h"
}

//=============================================================================
// Construction / destruction
//=============================================================================

TEST(SNNAdaptation, CreateReturnsNonNullWithValidArgs) {
    snn_adaptation_state_t* a = snn_adaptation_create(16, 150.0f, 0.6f, 1.0f);
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->n_neurons, 16u);
    EXPECT_FLOAT_EQ(a->tau_ms, 150.0f);
    EXPECT_FLOAT_EQ(a->gain_mv, 0.6f);
    EXPECT_FLOAT_EQ(a->spike_bump, 1.0f);
    ASSERT_NE(a->adapt_var, nullptr);
    /* Initial adapt_var must be zero. */
    for (uint32_t i = 0; i < a->n_neurons; ++i) {
        EXPECT_FLOAT_EQ(a->adapt_var[i], 0.0f);
    }
    snn_adaptation_destroy(a);
}

TEST(SNNAdaptation, CreateRejectsZeroNeurons) {
    EXPECT_EQ(snn_adaptation_create(0, 150.0f, 0.6f, 1.0f), nullptr);
}

TEST(SNNAdaptation, CreateRejectsNonPositiveTau) {
    EXPECT_EQ(snn_adaptation_create(4, 0.0f, 0.6f, 1.0f), nullptr);
    EXPECT_EQ(snn_adaptation_create(4, -1.0f, 0.6f, 1.0f), nullptr);
}

TEST(SNNAdaptation, CreateRejectsNegativeGain) {
    EXPECT_EQ(snn_adaptation_create(4, 150.0f, -0.1f, 1.0f), nullptr);
}

TEST(SNNAdaptation, DestroyNullDoesNotCrash) {
    snn_adaptation_destroy(nullptr);
    SUCCEED();
}

//=============================================================================
// Reset
//=============================================================================

TEST(SNNAdaptation, ResetZerosAdaptVarKeepsParams) {
    snn_adaptation_state_t* a = snn_adaptation_create(8, 150.0f, 0.6f, 1.0f);
    ASSERT_NE(a, nullptr);

    /* Dirty the state. */
    for (uint32_t i = 0; i < a->n_neurons; ++i) {
        a->adapt_var[i] = 3.14f;
    }

    snn_adaptation_reset(a);

    for (uint32_t i = 0; i < a->n_neurons; ++i) {
        EXPECT_FLOAT_EQ(a->adapt_var[i], 0.0f);
    }
    /* Parameters must survive reset. */
    EXPECT_FLOAT_EQ(a->tau_ms, 150.0f);
    EXPECT_FLOAT_EQ(a->gain_mv, 0.6f);
    EXPECT_FLOAT_EQ(a->spike_bump, 1.0f);

    snn_adaptation_destroy(a);
}

TEST(SNNAdaptation, ResetNullDoesNotCrash) {
    snn_adaptation_reset(nullptr);
    SUCCEED();
}

//=============================================================================
// Update accumulation
//=============================================================================

TEST(SNNAdaptation, UpdateAllFiredSingleCallGivesBump) {
    const uint32_t n = 5;
    const float tau = 150.0f;
    const float bump = 1.0f;
    const float dt = 1.0f;
    snn_adaptation_state_t* a = snn_adaptation_create(n, tau, 0.6f, bump);
    ASSERT_NE(a, nullptr);

    std::vector<float> fired(n, 1.0f);
    snn_adaptation_update(a, fired.data(), dt);

    /* After one step: adapt_var = 0*decay + bump = bump. */
    for (uint32_t i = 0; i < n; ++i) {
        EXPECT_FLOAT_EQ(a->adapt_var[i], bump);
    }
    snn_adaptation_destroy(a);
}

TEST(SNNAdaptation, UpdateAllFiredTwoCallsAccumulates) {
    const uint32_t n = 5;
    const float tau = 150.0f;
    const float bump = 1.0f;
    const float dt = 1.0f;
    snn_adaptation_state_t* a = snn_adaptation_create(n, tau, 0.6f, bump);
    ASSERT_NE(a, nullptr);

    std::vector<float> fired(n, 1.0f);

    /* Step 1: adapt_var = bump. */
    snn_adaptation_update(a, fired.data(), dt);
    /* Step 2: adapt_var = bump * decay + bump. */
    snn_adaptation_update(a, fired.data(), dt);

    const float decay = std::exp(-dt / tau);
    const float expected = bump * decay + bump;

    for (uint32_t i = 0; i < n; ++i) {
        EXPECT_NEAR(a->adapt_var[i], expected, 1e-6f);
    }
    snn_adaptation_destroy(a);
}

TEST(SNNAdaptation, UpdateNoSpikesDecaysByHalfAtTauLn2) {
    /* With no spikes, adapt_var halves after tau*ln(2) ms.
     * Run 100 steps of dt=1ms with tau = 100 / ln(2) ≈ 144.2695 ms.
     * After exactly 100ms of decay, adapt_var should be 0.5 * initial. */
    const uint32_t n = 4;
    const float tau = 100.0f / std::log(2.0f);
    const float dt = 1.0f;
    snn_adaptation_state_t* a = snn_adaptation_create(n, tau, 0.6f, 1.0f);
    ASSERT_NE(a, nullptr);

    /* Seed adapt_var to 1.0. */
    for (uint32_t i = 0; i < n; ++i) {
        a->adapt_var[i] = 1.0f;
    }

    std::vector<float> fired(n, 0.0f);
    for (int step = 0; step < 100; ++step) {
        snn_adaptation_update(a, fired.data(), dt);
    }

    for (uint32_t i = 0; i < n; ++i) {
        EXPECT_NEAR(a->adapt_var[i], 0.5f, 0.5f * 0.01f);  /* within 1% */
    }
    snn_adaptation_destroy(a);
}

TEST(SNNAdaptation, UpdateNullInputsNoOp) {
    snn_adaptation_state_t* a = snn_adaptation_create(4, 150.0f, 0.6f, 1.0f);
    ASSERT_NE(a, nullptr);
    a->adapt_var[0] = 0.7f;

    /* Both null paths must not crash or mutate state. */
    snn_adaptation_update(nullptr, nullptr, 1.0f);
    snn_adaptation_update(a, nullptr, 1.0f);

    EXPECT_FLOAT_EQ(a->adapt_var[0], 0.7f);
    snn_adaptation_destroy(a);
}

//=============================================================================
// compute_hyperpol
//=============================================================================

TEST(SNNAdaptation, ComputeHyperpolWritesGainTimesAdaptVar) {
    const uint32_t n = 3;
    const float gain = 0.6f;
    snn_adaptation_state_t* a = snn_adaptation_create(n, 150.0f, gain, 1.0f);
    ASSERT_NE(a, nullptr);

    /* Three distinct values. */
    a->adapt_var[0] = 0.25f;
    a->adapt_var[1] = 1.0f;
    a->adapt_var[2] = 2.5f;

    std::vector<float> out(n, -999.0f);
    snn_adaptation_compute_hyperpol(a, out.data(), 1.0f);

    EXPECT_FLOAT_EQ(out[0], gain * 0.25f);
    EXPECT_FLOAT_EQ(out[1], gain * 1.0f);
    EXPECT_FLOAT_EQ(out[2], gain * 2.5f);

    snn_adaptation_destroy(a);
}

TEST(SNNAdaptation, ComputeHyperpolGainZeroGivesAllZeros) {
    const uint32_t n = 4;
    snn_adaptation_state_t* a = snn_adaptation_create(n, 150.0f, 0.0f, 1.0f);
    ASSERT_NE(a, nullptr);

    /* Nonzero adapt_var — but gain is zero so output must be zero. */
    for (uint32_t i = 0; i < n; ++i) {
        a->adapt_var[i] = 10.0f * (float)(i + 1);
    }

    std::vector<float> out(n, -999.0f);
    snn_adaptation_compute_hyperpol(a, out.data(), 1.0f);

    for (uint32_t i = 0; i < n; ++i) {
        EXPECT_FLOAT_EQ(out[i], 0.0f);
    }
    snn_adaptation_destroy(a);
}

TEST(SNNAdaptation, ComputeHyperpolOverwritesBuffer) {
    /* The buffer must be overwritten, not accumulated. */
    const uint32_t n = 3;
    snn_adaptation_state_t* a = snn_adaptation_create(n, 150.0f, 0.6f, 1.0f);
    ASSERT_NE(a, nullptr);
    for (uint32_t i = 0; i < n; ++i) {
        a->adapt_var[i] = 1.0f;
    }

    std::vector<float> out(n, 123.456f);
    snn_adaptation_compute_hyperpol(a, out.data(), 1.0f);

    for (uint32_t i = 0; i < n; ++i) {
        EXPECT_FLOAT_EQ(out[i], 0.6f);
    }
    snn_adaptation_destroy(a);
}

TEST(SNNAdaptation, ComputeHyperpolNullInputsNoOp) {
    snn_adaptation_state_t* a = snn_adaptation_create(4, 150.0f, 0.6f, 1.0f);
    ASSERT_NE(a, nullptr);
    std::vector<float> out(4, 42.0f);

    snn_adaptation_compute_hyperpol(nullptr, out.data(), 1.0f);
    snn_adaptation_compute_hyperpol(a, nullptr, 1.0f);

    /* Buffer untouched when state is null. */
    for (int i = 0; i < 4; ++i) {
        EXPECT_FLOAT_EQ(out[i], 42.0f);
    }
    snn_adaptation_destroy(a);
}
