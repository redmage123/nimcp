/**
 * @file test_substrate_effects.cpp
 * @brief Unit tests for the shared substrate-effects helper.
 */

#include <gtest/gtest.h>
#include <atomic>
#include <cmath>
#include <cstring>
#include <thread>
#include <vector>

#include "core/substrate/nimcp_substrate_effects.h"
#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform_mutex.h"

namespace {

// Build a bare-bones substrate with only the fields the helper reads.
// We don't initialize the mutex; substrate_compute_effects does not lock.
struct SubstrateFixture {
    neural_substrate_t* sub = nullptr;

    SubstrateFixture(float atp, float temp_c, float ion, float mem = 1.0f) {
        sub = static_cast<neural_substrate_t*>(
            nimcp_calloc(1, sizeof(neural_substrate_t)));
        sub->metabolic.atp_level = atp;
        sub->physical.temperature = temp_c;
        sub->physical.ion_balance = ion;
        sub->physical.membrane_integrity = mem;
    }
    ~SubstrateFixture() { nimcp_free(sub); }
};

constexpr float kTol = 1e-4f;

}  // namespace

/* ============================================================================
 * substrate_compute_effects
 * ============================================================================ */

TEST(SubstrateEffects, NullSubstrateReturnsError) {
    axon_substrate_effects_t a;
    dendrite_substrate_effects_t d;
    nimcp_error_t rc = substrate_compute_effects(nullptr, &a, &d);
    EXPECT_EQ(rc, NIMCP_ERROR_NULL_POINTER);
}

TEST(SubstrateEffects, NullOutputsAreSkippedGracefully) {
    SubstrateFixture f(1.0f, 37.0f, 1.0f);
    nimcp_error_t rc = substrate_compute_effects(f.sub, nullptr, nullptr);
    EXPECT_EQ(rc, NIMCP_SUCCESS);
}

TEST(SubstrateEffects, BaselineProducesNoEffectMultipliers) {
    SubstrateFixture f(1.0f, 37.0f, 1.0f, 1.0f);
    axon_substrate_effects_t a{};
    dendrite_substrate_effects_t d{};
    ASSERT_EQ(substrate_compute_effects(f.sub, &a, &d), NIMCP_SUCCESS);

    /* Axon */
    EXPECT_NEAR(a.temperature_q10_factor,   1.0f, kTol);
    EXPECT_NEAR(a.atp_velocity_factor,      1.0f, kTol);
    EXPECT_NEAR(a.myelin_efficiency,        1.0f, kTol);
    EXPECT_NEAR(a.overall_velocity_mod,     1.0f, kTol);
    EXPECT_NEAR(a.ion_gradient_strength,    1.0f, kTol);
    EXPECT_NEAR(a.ap_amplitude_mod,         1.0f, kTol);
    EXPECT_NEAR(a.spike_reliability,        1.0f, kTol);
    EXPECT_NEAR(a.pump_activity,            1.0f, kTol);
    EXPECT_NEAR(a.refractory_period_mod,    1.0f, kTol);
    EXPECT_NEAR(a.transport_efficiency,     1.0f, kTol);
    EXPECT_NEAR(a.membrane_capacitance_mod, 1.0f, kTol);
    EXPECT_NEAR(a.membrane_leak_mod,        1.0f, kTol);
    EXPECT_NEAR(a.overall_capacity,         1.0f, kTol);

    /* Dendrite */
    EXPECT_NEAR(d.membrane_time_constant_mod, 1.0f, kTol);
    EXPECT_NEAR(d.space_constant_mod,         1.0f, kTol);
    EXPECT_NEAR(d.integration_efficiency,     1.0f, kTol);
    EXPECT_NEAR(d.attenuation_mod,            1.0f, kTol);
    EXPECT_NEAR(d.nmda_mg_block_mod,          1.2f, kTol); /* 0.8+0.4×1 */
    EXPECT_NEAR(d.na_channel_availability,    1.0f, kTol);
    /* spike_threshold_mod = nmda_mg_block_mod × (2 - q10_factor)
     *                     = 1.2 × (2 - 1) = 1.2, clamped to [0.8, 1.2] = 1.2.
     * Formula matches axon_dendrite_substrate_bridge.c:534,547. */
    EXPECT_NEAR(d.spike_threshold_mod,        1.2f, kTol);
    EXPECT_NEAR(d.ca_pump_efficiency,         1.0f, kTol);
    EXPECT_NEAR(d.ca_buffer_capacity,         1.0f, kTol);
    EXPECT_NEAR(d.ca_handling_mod,            1.0f, kTol);
    EXPECT_NEAR(d.ltp_capacity,               1.0f, kTol);
    EXPECT_NEAR(d.plasticity_mod,             1.0f, kTol);
    EXPECT_NEAR(d.overall_capacity,           1.0f, kTol);
}

TEST(SubstrateEffects, HyperthermiaSpeedsKinetics) {
    SubstrateFixture f(1.0f, 47.0f, 1.0f);
    axon_substrate_effects_t a{};
    ASSERT_EQ(substrate_compute_effects(f.sub, &a, nullptr), NIMCP_SUCCESS);
    EXPECT_GT(a.temperature_q10_factor, 1.5f);
}

TEST(SubstrateEffects, HypothermiaSlowsKinetics) {
    SubstrateFixture f(1.0f, 27.0f, 1.0f);
    axon_substrate_effects_t a{};
    ASSERT_EQ(substrate_compute_effects(f.sub, &a, nullptr), NIMCP_SUCCESS);
    EXPECT_LT(a.temperature_q10_factor, 0.5f);
}

TEST(SubstrateEffects, LowATPExtendsRefractoryAndDropsVelocity) {
    SubstrateFixture f(0.5f, 37.0f, 1.0f);
    axon_substrate_effects_t a{};
    ASSERT_EQ(substrate_compute_effects(f.sub, &a, nullptr), NIMCP_SUCCESS);

    EXPECT_NEAR(a.atp_velocity_factor, 0.5f, kTol);
    EXPECT_GT(a.refractory_period_mod, 1.0f);  /* 1/0.5 = 2.0 */
}

/* ============================================================================
 * Inline helpers
 * ============================================================================ */

TEST(SubstrateEffects, ApplyTauScalesByTimeConstantMod) {
    dendrite_substrate_effects_t d{};
    d.membrane_time_constant_mod = 1.5f;
    EXPECT_NEAR(substrate_apply_tau(10.0f, &d), 15.0f, kTol);
}

TEST(SubstrateEffects, ApplyTauWithNullReturnsBase) {
    EXPECT_NEAR(substrate_apply_tau(10.0f, nullptr), 10.0f, kTol);
}

TEST(SubstrateEffects, ApplyTauFloorsSmallValues) {
    dendrite_substrate_effects_t d{};
    d.membrane_time_constant_mod = 0.001f;
    EXPECT_GE(substrate_apply_tau(1.0f, &d), 0.1f);
}

TEST(SubstrateEffects, ApplyTrefScales) {
    axon_substrate_effects_t a{};
    a.refractory_period_mod = 1.5f;
    EXPECT_NEAR(substrate_apply_tref(2.0f, &a), 3.0f, kTol);
    EXPECT_NEAR(substrate_apply_tref(2.0f, nullptr), 2.0f, kTol);
}

TEST(SubstrateEffects, ApplyLrScales) {
    dendrite_substrate_effects_t d{};
    d.plasticity_mod = 0.25f;
    EXPECT_NEAR(substrate_apply_lr(0.04f, &d), 0.01f, kTol);
    EXPECT_NEAR(substrate_apply_lr(0.04f, nullptr), 0.04f, kTol);
}

TEST(SubstrateEffects, SpikeSurvivesBelowReliability) {
    axon_substrate_effects_t a{};
    a.spike_reliability = 0.5f;
    EXPECT_TRUE(substrate_spike_survives(&a, 0.3f));
}

TEST(SubstrateEffects, SpikeFailsAboveReliability) {
    axon_substrate_effects_t a{};
    a.spike_reliability = 0.5f;
    EXPECT_FALSE(substrate_spike_survives(&a, 0.8f));
}

TEST(SubstrateEffects, SpikeSurvivesOnNullEffects) {
    EXPECT_TRUE(substrate_spike_survives(nullptr, 0.0f));
    EXPECT_TRUE(substrate_spike_survives(nullptr, 0.99f));
}

/* ============================================================================
 * substrate_debit_activity
 * ============================================================================ */

TEST(SubstrateEffects, DebitActivityNullSubstrateIsNoOp) {
    substrate_debit_activity(nullptr, 0, 10, 3);  /* must not crash */
    SUCCEED();
}

TEST(SubstrateEffects, DebitActivityDecrementsATP) {
    SubstrateFixture f(1.0f, 37.0f, 1.0f);
    float before = f.sub->metabolic.atp_level;
    substrate_debit_activity(f.sub, 0, 1000, 100);
    float after = f.sub->metabolic.atp_level;
    EXPECT_LT(after, before);
    EXPECT_GE(after, 0.0f);
}

/* ----------------------------------------------------------------------------
 * Bug #1 regression: concurrent substrate_debit_activity must not lose updates.
 *
 * Two threads each do N iterations of debit(1 spike, 0 plasticity). Expected
 * final ATP = initial - (2 × N × SUBSTRATE_EFFECTS_ATP_PER_SPIKE). Without
 * the mutex guard, the naive read-modify-write loses updates and the residual
 * ATP is > expected. With the fix, the final value matches within float
 * rounding tolerance. Uses a real mutex on the substrate.
 * -------------------------------------------------------------------------- */
TEST(SubstrateEffects, DebitActivityConcurrent) {
    constexpr int kIters = 10000;
    constexpr int kThreads = 2;
    /* ATP_PER_SPIKE = 1e-8f. A single-spike debit against atp=0.5 is below
     * float32 ULP (~6e-8) and rounds away, which masks both the race and the
     * fix. Pass a larger spike count per call (1000) so each debit = 1e-5,
     * which is well above ULP at the chosen initial ATP. Total drain across
     * both threads = 2 × kIters × 1000 × 1e-8 = 0.2, deterministic. */
    constexpr int kSpikesPerCall = 1000;
    const float initial_atp = 0.9f;
    const float per_spike   = 1.0e-8f;  /* must match SUBSTRATE_EFFECTS_ATP_PER_SPIKE */

    neural_substrate_t* sub = static_cast<neural_substrate_t*>(
        nimcp_calloc(1, sizeof(neural_substrate_t)));
    sub->metabolic.atp_level    = initial_atp;
    sub->physical.temperature   = 37.0f;
    sub->physical.ion_balance   = 1.0f;
    sub->physical.membrane_integrity = 1.0f;
    sub->mutex = nimcp_platform_mutex_create();
    ASSERT_NE(sub->mutex, nullptr);

    auto worker = [&]() {
        for (int i = 0; i < kIters; ++i) {
            substrate_debit_activity(sub, 0, kSpikesPerCall, 0);
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t) threads.emplace_back(worker);
    for (auto& th : threads) th.join();

    const float total_spikes = static_cast<float>(kThreads * kIters * kSpikesPerCall);
    float expected = initial_atp - total_spikes * per_spike;
    float actual   = sub->metabolic.atp_level;

    /* Float rounding across 20k subtractions of magnitude 1e-5 against a
     * 0.9-scale value: ULP ≈ 1.2e-7, worst-case accumulated drift well under
     * 1e-3. Lost-update races would leak >> 1e-3. */
    EXPECT_NEAR(actual, expected, 1e-3f)
        << "Lost updates under concurrent debit — mutex guard missing?";

    nimcp_platform_mutex_destroy(sub->mutex);
    nimcp_free(sub->mutex);
    nimcp_free(sub);
}

/* ----------------------------------------------------------------------------
 * Bug #2 regression: spike_threshold_mod must match the bridge formula
 *   nmda_mg_block_mod * (2 - temp_effect)
 * where nmda_mg_block_mod = 0.8 + 0.4 * ion and temp_effect is the Q10 rate
 * at the current temperature, all clamped to [0.8, 1.2].
 * -------------------------------------------------------------------------- */
TEST(SubstrateEffects, SpikeThresholdModMatchesBridge) {
    /* Pick a non-baseline state so the test actually exercises both factors. */
    const float ion   = 0.7f;
    const float tempC = 40.0f;   /* within the [20,45] clamp */
    const float atp   = 1.0f;
    const float mem   = 1.0f;

    SubstrateFixture f(atp, tempC, ion, mem);
    dendrite_substrate_effects_t d{};
    ASSERT_EQ(substrate_compute_effects(f.sub, nullptr, &d), NIMCP_SUCCESS);

    /* Replicate the bridge's math independently. */
    const float Q10       = 2.3f;
    const float T_ref     = 37.0f;
    float q10_factor      = std::pow(Q10, (tempC - T_ref) / 10.0f);
    float nmda_mg_block   = 0.8f + 0.4f * ion;
    float expected        = nmda_mg_block * (2.0f - q10_factor);
    /* Match the helper's clamp band. */
    if (expected < 0.8f) expected = 0.8f;
    if (expected > 1.2f) expected = 1.2f;

    EXPECT_NEAR(d.spike_threshold_mod, expected, 1e-4f);
}

/* ----------------------------------------------------------------------------
 * Bug #3 regression: passing a zero-initialized effects cache must NOT
 * silently kill the learning rate. When plasticity_mod == 0 AND
 * overall_capacity == 0, the helper treats the cache as uninitialized and
 * returns lr_base unscaled.
 * -------------------------------------------------------------------------- */
TEST(SubstrateEffects, ApplyLrReturnsUnscaledOnZeroCache) {
    dendrite_substrate_effects_t eff{};  /* zero-initialized — sentinel case */
    const float lr = 0.001f;
    EXPECT_NEAR(substrate_apply_lr(lr, &eff), lr, 1e-9f);
}

/* Complementary: an initialized cache with plasticity_mod > 0 scales normally,
 * even when overall_capacity happens to be zero. */
TEST(SubstrateEffects, ApplyLrScalesWhenPlasticityModNonZero) {
    dendrite_substrate_effects_t eff{};
    eff.plasticity_mod   = 0.5f;
    eff.overall_capacity = 0.0f;  /* only plasticity_mod is set */
    EXPECT_NEAR(substrate_apply_lr(0.02f, &eff), 0.01f, 1e-6f);
}

/* Tau sentinel: mirrors the lr sentinel. */
TEST(SubstrateEffects, ApplyTauReturnsBaseOnZeroCache) {
    dendrite_substrate_effects_t eff{};  /* zero-initialized */
    EXPECT_NEAR(substrate_apply_tau(20.0f, &eff), 20.0f, 1e-6f);
}
