/**
 * @file test_snn_depression_tuning.cpp
 * @brief Unit tests for runtime-tunable SNN short-term synaptic depression.
 *
 * WHAT: Verify setter/getter plumbing for g_snn_dep_{inc,tau_ms,cap} and the
 *       math that the hot-path uses (exp decay, per-spike bump, cap).
 * WHY:  STD dynamics are critical for preventing hot-pathway runaway on the
 *       millisecond timescale. Making them tunable lets operators adjust
 *       recovery speed and bump strength without a rebuild.
 * HOW:  Directly exercise the tunable API, then reproduce the per-neuron
 *       update math on a stub depression[4] array. No network needed.
 *
 * @date 2026-04-22
 */

#include <gtest/gtest.h>
#include <cmath>

#include "snn/nimcp_snn_training.h"

/* =====================================================================
 * Helper: apply one step of the depression update to a single slot.
 * Matches the logic in nimcp_snn_network.c's CSR path:
 *
 *   pop->depression[n] *= dep_decay;
 *   if (fired_this_step) {
 *       pop->depression[n] = fminf(dep_cap, pop->depression[n] + dep_inc);
 *   }
 *
 * where dep_decay = exp(-dt_ms / dep_tau).
 * ===================================================================== */
static inline float step_depression(float cur, bool fired,
                                    float dep_inc, float dep_cap,
                                    float dep_decay)
{
    cur *= dep_decay;
    if (fired) {
        cur += dep_inc;
        if (cur > dep_cap) cur = dep_cap;
    }
    return cur;
}

class SNNDepressionTuningTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* Record defaults so other tests aren't disturbed. */
        saved_inc = snn_tune_get_depression_inc();
        saved_tau = snn_tune_get_depression_tau_ms();
        saved_cap = snn_tune_get_depression_cap();
    }
    void TearDown() override {
        /* Restore defaults between tests. */
        snn_tune_set_depression_inc(saved_inc);
        snn_tune_set_depression_tau_ms(saved_tau);
        snn_tune_set_depression_cap(saved_cap);
    }

    float saved_inc = 0.0f;
    float saved_tau = 0.0f;
    float saved_cap = 0.0f;
};

/* =====================================================================
 * Setter/getter sanity — values round-trip, out-of-range is rejected.
 * ===================================================================== */
TEST_F(SNNDepressionTuningTest, SettersAcceptInRangeValues) {
    snn_tune_set_depression_inc(0.3f);
    snn_tune_set_depression_tau_ms(50.0f);
    snn_tune_set_depression_cap(0.5f);

    EXPECT_FLOAT_EQ(snn_tune_get_depression_inc(), 0.3f);
    EXPECT_FLOAT_EQ(snn_tune_get_depression_tau_ms(), 50.0f);
    EXPECT_FLOAT_EQ(snn_tune_get_depression_cap(), 0.5f);
}

TEST_F(SNNDepressionTuningTest, SettersRejectOutOfRangeInc) {
    snn_tune_set_depression_inc(0.25f);  /* valid baseline */
    EXPECT_FLOAT_EQ(snn_tune_get_depression_inc(), 0.25f);

    /* Negative: should be rejected, getter keeps previous. */
    snn_tune_set_depression_inc(-0.1f);
    EXPECT_FLOAT_EQ(snn_tune_get_depression_inc(), 0.25f);

    /* > 1.0: should be rejected. */
    snn_tune_set_depression_inc(1.5f);
    EXPECT_FLOAT_EQ(snn_tune_get_depression_inc(), 0.25f);

    /* Edge: 0.0 and 1.0 are valid. */
    snn_tune_set_depression_inc(0.0f);
    EXPECT_FLOAT_EQ(snn_tune_get_depression_inc(), 0.0f);
    snn_tune_set_depression_inc(1.0f);
    EXPECT_FLOAT_EQ(snn_tune_get_depression_inc(), 1.0f);
}

TEST_F(SNNDepressionTuningTest, SettersRejectOutOfRangeTau) {
    snn_tune_set_depression_tau_ms(50.0f);
    EXPECT_FLOAT_EQ(snn_tune_get_depression_tau_ms(), 50.0f);

    /* < 1.0 rejected. */
    snn_tune_set_depression_tau_ms(0.5f);
    EXPECT_FLOAT_EQ(snn_tune_get_depression_tau_ms(), 50.0f);
    snn_tune_set_depression_tau_ms(0.0f);
    EXPECT_FLOAT_EQ(snn_tune_get_depression_tau_ms(), 50.0f);
    snn_tune_set_depression_tau_ms(-5.0f);
    EXPECT_FLOAT_EQ(snn_tune_get_depression_tau_ms(), 50.0f);

    /* > 10000 rejected. */
    snn_tune_set_depression_tau_ms(20000.0f);
    EXPECT_FLOAT_EQ(snn_tune_get_depression_tau_ms(), 50.0f);

    /* Edge: 1.0 and 10000.0 are valid. */
    snn_tune_set_depression_tau_ms(1.0f);
    EXPECT_FLOAT_EQ(snn_tune_get_depression_tau_ms(), 1.0f);
    snn_tune_set_depression_tau_ms(10000.0f);
    EXPECT_FLOAT_EQ(snn_tune_get_depression_tau_ms(), 10000.0f);
}

TEST_F(SNNDepressionTuningTest, SettersRejectOutOfRangeCap) {
    snn_tune_set_depression_cap(0.5f);
    EXPECT_FLOAT_EQ(snn_tune_get_depression_cap(), 0.5f);

    snn_tune_set_depression_cap(-0.1f);
    EXPECT_FLOAT_EQ(snn_tune_get_depression_cap(), 0.5f);
    snn_tune_set_depression_cap(1.5f);
    EXPECT_FLOAT_EQ(snn_tune_get_depression_cap(), 0.5f);

    /* Edges 0.0 and 1.0 are valid. */
    snn_tune_set_depression_cap(0.0f);
    EXPECT_FLOAT_EQ(snn_tune_get_depression_cap(), 0.0f);
    snn_tune_set_depression_cap(1.0f);
    EXPECT_FLOAT_EQ(snn_tune_get_depression_cap(), 1.0f);
}

/* =====================================================================
 * Math tests: validate the update formula against hand-computed values.
 * These mirror what nimcp_snn_network.c does per-neuron.
 * ===================================================================== */
TEST_F(SNNDepressionTuningTest, DecayMatchesExpFormula) {
    /* tau=50ms, dt=1ms => decay = exp(-1/50) ≈ 0.9802 */
    const float dt_ms = 1.0f;
    const float tau   = 50.0f;
    snn_tune_set_depression_tau_ms(tau);

    const float decay = std::exp(-dt_ms / snn_tune_get_depression_tau_ms());
    EXPECT_NEAR(decay, 0.9802f, 1e-3f);

    /* Starting from 1.0, one decay step => 0.9802. */
    float dep = 1.0f;
    dep *= decay;
    EXPECT_NEAR(dep, 0.9802f, 1e-3f);
}

TEST_F(SNNDepressionTuningTest, BumpFromZeroMatchesInc) {
    /* depression[4] stub; slot 0 exercised. */
    float depression[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    snn_tune_set_depression_inc(0.3f);
    snn_tune_set_depression_tau_ms(50.0f);
    snn_tune_set_depression_cap(0.5f);

    const float dt_ms = 1.0f;
    const float decay = std::exp(-dt_ms / snn_tune_get_depression_tau_ms());
    const float inc   = snn_tune_get_depression_inc();
    const float cap   = snn_tune_get_depression_cap();

    /* Single "fired" step: 0 × decay + 0.3 = 0.3 */
    depression[0] = step_depression(depression[0], true, inc, cap, decay);
    EXPECT_NEAR(depression[0], 0.3f, 1e-5f);
}

TEST_F(SNNDepressionTuningTest, DecayOverManyStepsMatchesExp) {
    snn_tune_set_depression_inc(0.3f);
    snn_tune_set_depression_tau_ms(50.0f);
    snn_tune_set_depression_cap(0.5f);

    const float dt_ms = 1.0f;
    const float decay = std::exp(-dt_ms / snn_tune_get_depression_tau_ms());
    const float inc   = snn_tune_get_depression_inc();
    const float cap   = snn_tune_get_depression_cap();

    /* Start at 0.3 after a single bump, then 25 steps of no-fire decay.
     * Expected: 0.3 × exp(-25/50) = 0.3 × 0.6065 ≈ 0.1820 */
    float dep = 0.0f;
    dep = step_depression(dep, true, inc, cap, decay);  /* 0 -> 0.3 */
    for (int i = 0; i < 25; i++) {
        dep = step_depression(dep, false, inc, cap, decay);
    }
    const float expected = 0.3f * std::exp(-25.0f / 50.0f);
    EXPECT_NEAR(dep, expected, 1e-3f);
    EXPECT_NEAR(dep, 0.182f, 1e-3f);
}

TEST_F(SNNDepressionTuningTest, BumpCapsAtDepCap) {
    snn_tune_set_depression_inc(0.3f);
    snn_tune_set_depression_tau_ms(50.0f);
    snn_tune_set_depression_cap(0.5f);

    const float dt_ms = 1.0f;
    const float decay = std::exp(-dt_ms / snn_tune_get_depression_tau_ms());
    const float inc   = snn_tune_get_depression_inc();
    const float cap   = snn_tune_get_depression_cap();

    /* Start at 0.4. One fire: 0.4 × 0.9802 + 0.3 = 0.692 -> cap 0.5. */
    float dep = 0.4f;
    dep = step_depression(dep, true, inc, cap, decay);
    EXPECT_NEAR(dep, 0.5f, 1e-5f);
}

TEST_F(SNNDepressionTuningTest, ZeroIncDisablesBumps) {
    /* dep_inc=0 simulates "depression disabled" — bumps are no-ops. */
    snn_tune_set_depression_inc(0.0f);
    EXPECT_FLOAT_EQ(snn_tune_get_depression_inc(), 0.0f);

    snn_tune_set_depression_tau_ms(50.0f);
    snn_tune_set_depression_cap(0.5f);

    const float dt_ms = 1.0f;
    const float decay = std::exp(-dt_ms / snn_tune_get_depression_tau_ms());
    const float inc   = snn_tune_get_depression_inc();
    const float cap   = snn_tune_get_depression_cap();

    /* Fire every step from zero; depression should remain at zero. */
    float dep = 0.0f;
    for (int i = 0; i < 100; i++) {
        dep = step_depression(dep, true, inc, cap, decay);
    }
    EXPECT_NEAR(dep, 0.0f, 1e-6f);

    /* And a non-zero starting value should decay toward zero. */
    dep = 0.4f;
    for (int i = 0; i < 500; i++) {
        dep = step_depression(dep, true, inc, cap, decay);
    }
    EXPECT_LT(dep, 1e-3f);
}

/* =====================================================================
 * Stub-population test: depression[4] array exercised as the hot path
 * would touch it (fire vector => per-neuron update). No real
 * snn_population_t needed; we mirror the inner loop behavior.
 * ===================================================================== */
TEST_F(SNNDepressionTuningTest, StubPopulationFourNeurons) {
    float depression[4] = {0.0f, 0.1f, 0.4f, 0.0f};
    const bool fired[4] = {true, false, true, false};

    snn_tune_set_depression_inc(0.3f);
    snn_tune_set_depression_tau_ms(50.0f);
    snn_tune_set_depression_cap(0.5f);

    const float dt_ms = 1.0f;
    const float decay = std::exp(-dt_ms / snn_tune_get_depression_tau_ms());
    const float inc   = snn_tune_get_depression_inc();
    const float cap   = snn_tune_get_depression_cap();

    /* One step. */
    for (int i = 0; i < 4; i++) {
        depression[i] = step_depression(depression[i], fired[i], inc, cap, decay);
    }

    /* n=0: 0 × 0.9802 + 0.3 = 0.3 */
    EXPECT_NEAR(depression[0], 0.3f, 1e-4f);
    /* n=1: 0.1 × 0.9802 ≈ 0.09802, no fire */
    EXPECT_NEAR(depression[1], 0.09802f, 1e-4f);
    /* n=2: 0.4 × 0.9802 + 0.3 = 0.692 -> cap 0.5 */
    EXPECT_NEAR(depression[2], 0.5f, 1e-4f);
    /* n=3: 0 × 0.9802 = 0, no fire */
    EXPECT_NEAR(depression[3], 0.0f, 1e-6f);
}
