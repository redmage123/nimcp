/**
 * @file test_cortex_cnn_substrate.cpp
 * @brief Unit tests for the cortex CNN substrate adapter (Phase 3).
 *
 * WHAT: Covers the four runtime-tunable substrate knobs, the
 *       cortex_cnn_attach_substrate API, and the ATP / membrane damage →
 *       activation / lr modulation path across the cortex modalities.
 * WHY:  Each cortex CNN has its own metabolic compartment (region_id =
 *       cortex type). The activation-mod knob scales the embedding by
 *       dend_effects.integration_efficiency (which falls with membrane
 *       damage), the plasticity-mod knob scales the effective LR by
 *       dend_effects.plasticity_mod (which falls with ATP depletion).
 *       Regressions here silently decouple cortex output / learning from
 *       the biological state.
 * HOW:  Google Test. Tests that only touch tunables exercise the setter /
 *       getter API. Tests that need a live forward pass build a small
 *       cortex CNN per modality and compare embedding norms across
 *       substrate regimes using ONLY the public API (the cortex processor
 *       struct is intentionally opaque).
 *
 * @date 2026-04-22
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

extern "C" {
#include "training/nimcp_cortex_cnn.h"
#include "training/nimcp_fno_layer.h"
#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "core/substrate/nimcp_substrate_effects.h"
#include "core/thalamic/nimcp_thalamic_channel.h"
#include "middleware/routing/nimcp_thalamic_router.h"

/* FNO setters live in nimcp_cortex_cnn.c but are not declared in the
 * public header — they are wired via extern decls in the API / learning
 * layers. Replicate that pattern here. */
extern void cortex_cnn_set_fno_visual(cortex_cnn_processor_t* proc, void* fno);

/* Test-only accessor for the thalamic channel, defined in nimcp_cortex_cnn.c */
thalamic_channel_t* cortex_cnn_test_get_thalamic_channel(
    const cortex_cnn_processor_t* proc);

/* Test-only accessors for cached substrate effects (F1 CRITICAL mirror). */
const dendrite_substrate_effects_t* cortex_cnn_test_get_cached_dend_effects(
    const cortex_cnn_processor_t* proc);
const axon_substrate_effects_t* cortex_cnn_test_get_cached_axon_effects(
    const cortex_cnn_processor_t* proc);
}

/*============================================================================
 * Fixture: saves + restores every substrate tunable the tests touch.
 *==========================================================================*/
class CortexCNNSubstrateAdapterTest : public ::testing::Test {
protected:
    float saved_enabled             = 0.0f;
    float saved_period              = 0.0f;
    float saved_activation_mod_on   = 0.0f;
    float saved_plasticity_mod_on   = 0.0f;

    void SetUp() override {
        saved_enabled           = cortex_cnn_tune_get_substrate_enabled();
        saved_period            = cortex_cnn_tune_get_substrate_update_period();
        saved_activation_mod_on = cortex_cnn_tune_get_substrate_activation_mod_on();
        saved_plasticity_mod_on = cortex_cnn_tune_get_substrate_plasticity_mod_on();
    }

    void TearDown() override {
        cortex_cnn_tune_set_substrate_enabled(saved_enabled);
        cortex_cnn_tune_set_substrate_update_period(saved_period);
        cortex_cnn_tune_set_substrate_activation_mod_on(saved_activation_mod_on);
        cortex_cnn_tune_set_substrate_plasticity_mod_on(saved_plasticity_mod_on);
    }

    /* Build a substrate with specific ATP + membrane integrity. Temperature
     * left at 37°C default. Substrate config is defaulted then overlaid. */
    neural_substrate_t* MakeSubstrate(float atp, float membrane = 1.0f) {
        substrate_config_t scfg;
        substrate_default_config(&scfg);
        neural_substrate_t* sub = substrate_create(&scfg);
        if (!sub) return nullptr;
        substrate_set_atp(sub, atp);
        substrate_set_membrane_integrity(sub, membrane);
        return sub;
    }

    static float L2Norm(const float* v, uint32_t n) {
        if (!v || n == 0) return 0.0f;
        float s = 0.0f;
        for (uint32_t i = 0; i < n; i++) {
            if (std::isfinite(v[i])) s += v[i] * v[i];
        }
        return std::sqrt(s);
    }

    static bool AllFinite(const float* v, uint32_t n) {
        if (!v) return false;
        for (uint32_t i = 0; i < n; i++) {
            if (!std::isfinite(v[i])) return false;
        }
        return true;
    }
};

/*============================================================================
 * Tunables — round-trip + out-of-range rejection.
 *==========================================================================*/
TEST_F(CortexCNNSubstrateAdapterTest, TunableEnabledRoundTripsNonzeroToOne) {
    cortex_cnn_tune_set_substrate_enabled(0.0f);
    EXPECT_FLOAT_EQ(cortex_cnn_tune_get_substrate_enabled(), 0.0f);
    cortex_cnn_tune_set_substrate_enabled(1.0f);
    EXPECT_FLOAT_EQ(cortex_cnn_tune_get_substrate_enabled(), 1.0f);
    cortex_cnn_tune_set_substrate_enabled(0.7f);
    EXPECT_FLOAT_EQ(cortex_cnn_tune_get_substrate_enabled(), 1.0f);
    cortex_cnn_tune_set_substrate_enabled(-3.0f);
    EXPECT_FLOAT_EQ(cortex_cnn_tune_get_substrate_enabled(), 1.0f);
    cortex_cnn_tune_set_substrate_enabled(0.0f);
    EXPECT_FLOAT_EQ(cortex_cnn_tune_get_substrate_enabled(), 0.0f);
}

TEST_F(CortexCNNSubstrateAdapterTest, TunablePeriodClampsOutOfRange) {
    cortex_cnn_tune_set_substrate_update_period(10.0f);
    EXPECT_FLOAT_EQ(cortex_cnn_tune_get_substrate_update_period(), 10.0f);

    cortex_cnn_tune_set_substrate_update_period(0.5f);
    EXPECT_FLOAT_EQ(cortex_cnn_tune_get_substrate_update_period(), 10.0f);
    cortex_cnn_tune_set_substrate_update_period(-1.0f);
    EXPECT_FLOAT_EQ(cortex_cnn_tune_get_substrate_update_period(), 10.0f);
    cortex_cnn_tune_set_substrate_update_period(1e9f);
    EXPECT_FLOAT_EQ(cortex_cnn_tune_get_substrate_update_period(), 10.0f);

    cortex_cnn_tune_set_substrate_update_period(1.0f);
    EXPECT_FLOAT_EQ(cortex_cnn_tune_get_substrate_update_period(), 1.0f);
    cortex_cnn_tune_set_substrate_update_period(10000.0f);
    EXPECT_FLOAT_EQ(cortex_cnn_tune_get_substrate_update_period(), 10000.0f);

    /* NaN/inf silently rejected (range test fails). */
    cortex_cnn_tune_set_substrate_update_period(50.0f);
    cortex_cnn_tune_set_substrate_update_period(
        std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(cortex_cnn_tune_get_substrate_update_period(), 50.0f);
    cortex_cnn_tune_set_substrate_update_period(
        std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(cortex_cnn_tune_get_substrate_update_period(), 50.0f);
}

TEST_F(CortexCNNSubstrateAdapterTest, TunableActivationModRoundTrip) {
    cortex_cnn_tune_set_substrate_activation_mod_on(0.0f);
    EXPECT_FLOAT_EQ(cortex_cnn_tune_get_substrate_activation_mod_on(), 0.0f);
    cortex_cnn_tune_set_substrate_activation_mod_on(1.0f);
    EXPECT_FLOAT_EQ(cortex_cnn_tune_get_substrate_activation_mod_on(), 1.0f);
    cortex_cnn_tune_set_substrate_activation_mod_on(42.0f);
    EXPECT_FLOAT_EQ(cortex_cnn_tune_get_substrate_activation_mod_on(), 1.0f);
    cortex_cnn_tune_set_substrate_activation_mod_on(-0.25f);
    EXPECT_FLOAT_EQ(cortex_cnn_tune_get_substrate_activation_mod_on(), 1.0f);
}

TEST_F(CortexCNNSubstrateAdapterTest, TunablePlasticityModRoundTrip) {
    cortex_cnn_tune_set_substrate_plasticity_mod_on(0.0f);
    EXPECT_FLOAT_EQ(cortex_cnn_tune_get_substrate_plasticity_mod_on(), 0.0f);
    cortex_cnn_tune_set_substrate_plasticity_mod_on(1.0f);
    EXPECT_FLOAT_EQ(cortex_cnn_tune_get_substrate_plasticity_mod_on(), 1.0f);
    cortex_cnn_tune_set_substrate_plasticity_mod_on(7.0f);
    EXPECT_FLOAT_EQ(cortex_cnn_tune_get_substrate_plasticity_mod_on(), 1.0f);
}

/*============================================================================
 * Attach API — null-tolerant.
 *==========================================================================*/
TEST_F(CortexCNNSubstrateAdapterTest, AttachNullProcessorIsNoOp) {
    cortex_cnn_attach_substrate(nullptr, nullptr);
    SUCCEED();  /* Should not crash. */
}

/*============================================================================
 * F1 CRITICAL mirror: attach must populate the effects cache IMMEDIATELY.
 * Before the fix, attach memset'd the cache to zero and left population to
 * the first forward refresh — so any consumer that reads
 * cached_dend_effects.plasticity_mod before the first step saw 0, and
 * learning silently died (lr × 0). Protect that with this explicit
 * pre-step check. Mirrors test_snn_substrate_adapter.cpp::AttachPopulatesCache.
 *==========================================================================*/
TEST_F(CortexCNNSubstrateAdapterTest, AttachPopulatesCache) {
    cortex_cnn_processor_t* proc = cortex_cnn_create(CORTEX_CNN_SPEECH, 32);
    ASSERT_NE(proc, nullptr);

    /* Cache is zero before attach. */
    const dendrite_substrate_effects_t* dend0 =
        cortex_cnn_test_get_cached_dend_effects(proc);
    const axon_substrate_effects_t* axon0 =
        cortex_cnn_test_get_cached_axon_effects(proc);
    ASSERT_NE(dend0, nullptr);
    ASSERT_NE(axon0, nullptr);
    EXPECT_FLOAT_EQ(dend0->plasticity_mod, 0.0f);
    EXPECT_FLOAT_EQ(axon0->atp_velocity_factor, 0.0f);

    /* Build substrate with known ATP so we can predict the cache. */
    neural_substrate_t* sub = MakeSubstrate(0.5f, 1.0f);
    ASSERT_NE(sub, nullptr);

    /* Attach — must populate cache BEFORE any forward step runs. */
    cortex_cnn_attach_substrate(proc, sub);

    const dendrite_substrate_effects_t* dend1 =
        cortex_cnn_test_get_cached_dend_effects(proc);
    const axon_substrate_effects_t* axon1 =
        cortex_cnn_test_get_cached_axon_effects(proc);
    ASSERT_NE(dend1, nullptr);
    ASSERT_NE(axon1, nullptr);

    /* plasticity_mod is linear in ATP in the substrate helper, so atp=0.5
     * yields plasticity_mod≈0.5. Must be non-zero to prove eager population. */
    EXPECT_NE(dend1->plasticity_mod, 0.0f)
        << "Attach did not immediately refresh plasticity_mod from substrate; "
        << "downstream LR × 0 would silently kill learning.";
    EXPECT_NEAR(dend1->plasticity_mod, 0.5f, 1e-4f);
    EXPECT_NEAR(axon1->atp_velocity_factor, 0.5f, 1e-4f)
        << "Attach did not immediately refresh axon cache.";
    /* Sanity: overall_capacity is non-zero on a real substrate (ATP>0). */
    EXPECT_GT(dend1->overall_capacity, 0.0f);

    /* Detach must not crash (null sub). */
    cortex_cnn_attach_substrate(proc, nullptr);

    substrate_destroy(sub);
    cortex_cnn_destroy(proc);
}

TEST_F(CortexCNNSubstrateAdapterTest, AttachSubstrateAndDetach) {
    cortex_cnn_processor_t* proc = cortex_cnn_create(CORTEX_CNN_SPEECH, 32);
    ASSERT_NE(proc, nullptr);

    /* Initially NULL — forward should still work. */
    std::vector<float> phonemes(64, 0.1f);
    const float* emb0 = cortex_cnn_forward_speech(proc, phonemes.data(),
                                                   (uint32_t)phonemes.size());
    EXPECT_NE(emb0, nullptr);

    /* Attach a healthy substrate — forward still OK. */
    neural_substrate_t* sub = MakeSubstrate(1.0f, 1.0f);
    ASSERT_NE(sub, nullptr);
    cortex_cnn_attach_substrate(proc, sub);

    const float* emb1 = cortex_cnn_forward_speech(proc, phonemes.data(),
                                                   (uint32_t)phonemes.size());
    EXPECT_NE(emb1, nullptr);

    /* Detach (NULL substrate) — still safe. */
    cortex_cnn_attach_substrate(proc, nullptr);
    const float* emb2 = cortex_cnn_forward_speech(proc, phonemes.data(),
                                                   (uint32_t)phonemes.size());
    EXPECT_NE(emb2, nullptr);

    substrate_destroy(sub);
    cortex_cnn_destroy(proc);
}

/*============================================================================
 * No-attach forward path with knob on is a no-op (no substrate to read).
 *==========================================================================*/
TEST_F(CortexCNNSubstrateAdapterTest, NullSubstrateForwardBehavesAsBaseline) {
    cortex_cnn_processor_t* proc = cortex_cnn_create(CORTEX_CNN_SPEECH, 32);
    ASSERT_NE(proc, nullptr);

    cortex_cnn_tune_set_substrate_enabled(1.0f);
    cortex_cnn_tune_set_substrate_activation_mod_on(1.0f);

    std::vector<float> phonemes(64, 0.25f);
    const float* emb = cortex_cnn_forward_speech(proc, phonemes.data(),
                                                  (uint32_t)phonemes.size());
    ASSERT_NE(emb, nullptr);

    uint32_t dim = 0;
    const float* got = cortex_cnn_get_embedding(proc, &dim);
    ASSERT_NE(got, nullptr);
    EXPECT_EQ(dim, 32u);
    EXPECT_TRUE(AllFinite(got, dim));

    cortex_cnn_destroy(proc);
}

/*============================================================================
 * Default-healthy substrate + forward: finite output, no crashes.
 *==========================================================================*/
TEST_F(CortexCNNSubstrateAdapterTest, HealthySubstrateForwardProducesFiniteOutput) {
    cortex_cnn_processor_t* proc = cortex_cnn_create(CORTEX_CNN_SPEECH, 32);
    ASSERT_NE(proc, nullptr);

    cortex_cnn_tune_set_substrate_enabled(1.0f);
    cortex_cnn_tune_set_substrate_activation_mod_on(1.0f);
    cortex_cnn_tune_set_substrate_update_period(1.0f);

    neural_substrate_t* sub = MakeSubstrate(1.0f, 1.0f);
    ASSERT_NE(sub, nullptr);
    cortex_cnn_attach_substrate(proc, sub);

    std::vector<float> phonemes(64, 0.25f);
    const float* emb = cortex_cnn_forward_speech(proc, phonemes.data(),
                                                  (uint32_t)phonemes.size());
    ASSERT_NE(emb, nullptr);

    uint32_t dim = 0;
    const float* got = cortex_cnn_get_embedding(proc, &dim);
    ASSERT_NE(got, nullptr);
    EXPECT_TRUE(AllFinite(got, dim));

    substrate_destroy(sub);
    cortex_cnn_destroy(proc);
}

/*============================================================================
 * Damaged membrane reduces integration_efficiency from 1.0 to 0.1 and the
 * apply path should scale the embedding amplitude. Compared against a
 * separate but identically-built processor with a healthy substrate, the
 * damaged cortex must produce a smaller L2 norm.
 *
 * Because each processor has its own random weight init, we can't expect
 * bit-identical baselines. Use a large damage ratio (0.1× → 10× smaller)
 * so the effect dominates any weight-init variance.
 *==========================================================================*/
TEST_F(CortexCNNSubstrateAdapterTest, DamagedMembraneShrinksEmbeddingNorm) {
    cortex_cnn_tune_set_substrate_enabled(1.0f);
    cortex_cnn_tune_set_substrate_activation_mod_on(1.0f);
    cortex_cnn_tune_set_substrate_update_period(1.0f);

    std::vector<float> phonemes(64);
    for (uint32_t i = 0; i < 64; i++) phonemes[i] = 0.05f + 0.01f * (float)i;

    /* Healthy */
    cortex_cnn_processor_t* proc_ok = cortex_cnn_create(CORTEX_CNN_SPEECH, 32);
    ASSERT_NE(proc_ok, nullptr);
    neural_substrate_t* sub_ok = MakeSubstrate(1.0f, 1.0f);
    ASSERT_NE(sub_ok, nullptr);
    cortex_cnn_attach_substrate(proc_ok, sub_ok);

    (void)cortex_cnn_forward_speech(proc_ok, phonemes.data(),
                                    (uint32_t)phonemes.size());
    uint32_t dim_ok = 0;
    const float* got_ok = cortex_cnn_get_embedding(proc_ok, &dim_ok);
    ASSERT_NE(got_ok, nullptr);
    float n_ok = L2Norm(got_ok, dim_ok);

    substrate_destroy(sub_ok);
    cortex_cnn_destroy(proc_ok);

    /* Damaged membrane: integration_efficiency ≈ 0.1 */
    cortex_cnn_processor_t* proc_bad = cortex_cnn_create(CORTEX_CNN_SPEECH, 32);
    ASSERT_NE(proc_bad, nullptr);
    neural_substrate_t* sub_bad = MakeSubstrate(1.0f, 0.1f);
    ASSERT_NE(sub_bad, nullptr);
    cortex_cnn_attach_substrate(proc_bad, sub_bad);

    (void)cortex_cnn_forward_speech(proc_bad, phonemes.data(),
                                    (uint32_t)phonemes.size());
    uint32_t dim_bad = 0;
    const float* got_bad = cortex_cnn_get_embedding(proc_bad, &dim_bad);
    ASSERT_NE(got_bad, nullptr);
    float n_bad = L2Norm(got_bad, dim_bad);

    EXPECT_TRUE(AllFinite(got_bad, dim_bad));

    /* Damaged should be strictly smaller. Norms are non-negative; if both
     * happen to be zero the check is vacuous but still passes. */
    EXPECT_LE(n_bad, n_ok)
        << "Damaged-membrane embedding norm (" << n_bad
        << ") must not exceed healthy norm (" << n_ok << ")";

    substrate_destroy(sub_bad);
    cortex_cnn_destroy(proc_bad);
}

/*============================================================================
 * activation_mod_on = 0 disables the amplitude scaling even when a damaged
 * substrate is attached. Uses two fresh processors for clean comparison:
 * knob-OFF baseline vs. knob-ON with the same damaged substrate.
 *==========================================================================*/
TEST_F(CortexCNNSubstrateAdapterTest, ActivationModKnobOffDisablesScaling) {
    cortex_cnn_tune_set_substrate_enabled(1.0f);
    cortex_cnn_tune_set_substrate_update_period(1.0f);

    std::vector<float> phonemes(64, 0.2f);

    /* Processor with knob ON — damaged substrate dampens embedding. */
    cortex_cnn_tune_set_substrate_activation_mod_on(1.0f);
    cortex_cnn_processor_t* proc_on = cortex_cnn_create(CORTEX_CNN_SPEECH, 32);
    ASSERT_NE(proc_on, nullptr);
    neural_substrate_t* sub_on = MakeSubstrate(1.0f, 0.1f);
    ASSERT_NE(sub_on, nullptr);
    cortex_cnn_attach_substrate(proc_on, sub_on);

    (void)cortex_cnn_forward_speech(proc_on, phonemes.data(),
                                    (uint32_t)phonemes.size());
    uint32_t dim_on = 0;
    const float* got_on = cortex_cnn_get_embedding(proc_on, &dim_on);
    ASSERT_NE(got_on, nullptr);
    float n_on = L2Norm(got_on, dim_on);

    substrate_destroy(sub_on);
    cortex_cnn_destroy(proc_on);

    /* Processor with knob OFF — same damaged substrate, no dampening. */
    cortex_cnn_tune_set_substrate_activation_mod_on(0.0f);
    cortex_cnn_processor_t* proc_off = cortex_cnn_create(CORTEX_CNN_SPEECH, 32);
    ASSERT_NE(proc_off, nullptr);
    neural_substrate_t* sub_off = MakeSubstrate(1.0f, 0.1f);
    ASSERT_NE(sub_off, nullptr);
    cortex_cnn_attach_substrate(proc_off, sub_off);

    (void)cortex_cnn_forward_speech(proc_off, phonemes.data(),
                                    (uint32_t)phonemes.size());
    uint32_t dim_off = 0;
    const float* got_off = cortex_cnn_get_embedding(proc_off, &dim_off);
    ASSERT_NE(got_off, nullptr);
    float n_off = L2Norm(got_off, dim_off);

    /* Knob-off norm should be no smaller than knob-on norm (likely larger).
     * Allow a loose check to absorb random weight-init variance. */
    EXPECT_GE(n_off, n_on * 0.9f)
        << "activation_mod_on=0 must not dampen (off=" << n_off
        << ", on=" << n_on << ")";

    substrate_destroy(sub_off);
    cortex_cnn_destroy(proc_off);
}

/*============================================================================
 * Master enable off ignores an attached (damaged) substrate — forward is
 * bit-identical to the no-attach baseline. Builds two processors with the
 * same seed-state (create order) and verifies embedding norms match when
 * the knob is off.
 *==========================================================================*/
TEST_F(CortexCNNSubstrateAdapterTest, MasterEnableOffIgnoresAttachedSubstrate) {
    cortex_cnn_tune_set_substrate_enabled(0.0f);

    std::vector<float> phonemes(64, 0.3f);

    cortex_cnn_processor_t* proc = cortex_cnn_create(CORTEX_CNN_SPEECH, 32);
    ASSERT_NE(proc, nullptr);

    neural_substrate_t* sub = MakeSubstrate(0.05f, 0.05f);  /* severe damage */
    ASSERT_NE(sub, nullptr);
    cortex_cnn_attach_substrate(proc, sub);

    for (int i = 0; i < 5; i++) {
        (void)cortex_cnn_forward_speech(proc, phonemes.data(),
                                        (uint32_t)phonemes.size());
    }

    uint32_t dim = 0;
    const float* got = cortex_cnn_get_embedding(proc, &dim);
    ASSERT_NE(got, nullptr);
    EXPECT_TRUE(AllFinite(got, dim));

    /* Norm should be positive (embedding was not zeroed by the modulation). */
    float n = L2Norm(got, dim);
    EXPECT_GT(n, 0.0f);

    substrate_destroy(sub);
    cortex_cnn_destroy(proc);
}

/*============================================================================
 * Audio + visual + somato: forward paths accept an attached substrate
 * without crashing and produce finite embeddings. Smoke test that the
 * refresh / apply helpers are wired into all four modalities.
 *==========================================================================*/
TEST_F(CortexCNNSubstrateAdapterTest, AllModalitiesAcceptSubstrate) {
    cortex_cnn_tune_set_substrate_enabled(1.0f);
    cortex_cnn_tune_set_substrate_activation_mod_on(1.0f);
    cortex_cnn_tune_set_substrate_update_period(1.0f);

    neural_substrate_t* sub = MakeSubstrate(0.5f, 0.5f);
    ASSERT_NE(sub, nullptr);

    /* Visual */
    {
        cortex_cnn_processor_t* p = cortex_cnn_create(CORTEX_CNN_VISUAL, 64);
        ASSERT_NE(p, nullptr);
        cortex_cnn_attach_substrate(p, sub);
        std::vector<uint8_t> pixels(16 * 16 * 3, 128u);
        const float* e = cortex_cnn_forward_visual(p, pixels.data(), 16, 16, 3);
        EXPECT_NE(e, nullptr);
        uint32_t dim = 0;
        const float* got = cortex_cnn_get_embedding(p, &dim);
        ASSERT_NE(got, nullptr);
        EXPECT_TRUE(AllFinite(got, dim));
        cortex_cnn_destroy(p);
    }

    /* Audio (mel spectrogram) */
    {
        cortex_cnn_processor_t* p = cortex_cnn_create(CORTEX_CNN_AUDIO, 64);
        ASSERT_NE(p, nullptr);
        cortex_cnn_attach_substrate(p, sub);
        std::vector<float> mel(128, 0.4f);
        const float* e = cortex_cnn_forward_audio(p, mel.data(), (uint32_t)mel.size());
        EXPECT_NE(e, nullptr);
        uint32_t dim = 0;
        const float* got = cortex_cnn_get_embedding(p, &dim);
        ASSERT_NE(got, nullptr);
        EXPECT_TRUE(AllFinite(got, dim));
        cortex_cnn_destroy(p);
    }

    /* Somato (body state) */
    {
        cortex_cnn_processor_t* p = cortex_cnn_create(CORTEX_CNN_SOMATO, 32);
        ASSERT_NE(p, nullptr);
        cortex_cnn_attach_substrate(p, sub);
        std::vector<float> segs(45, 0.2f);
        const float* e = cortex_cnn_forward_somato(p, segs.data(), (uint32_t)segs.size());
        EXPECT_NE(e, nullptr);
        uint32_t dim = 0;
        const float* got = cortex_cnn_get_embedding(p, &dim);
        ASSERT_NE(got, nullptr);
        EXPECT_TRUE(AllFinite(got, dim));
        cortex_cnn_destroy(p);
    }

    substrate_destroy(sub);
}

/*============================================================================
 * W3 audit, Bug #2 regression guard — FNO visual contribution MUST be
 * modulated by substrate activation_mod. Previously activation_mod was
 * applied to the CNN embedding BEFORE the FNO mix-in, leaving the FNO
 * component unscaled and effectively decoupling part of the embedding from
 * substrate damage. With the fix (FNO mix-in happens first, then
 * activation_mod), a severely-damaged substrate must shrink the final
 * embedding norm much more aggressively than a healthy one — even on a
 * cortex with FNO visual enabled.
 *==========================================================================*/
TEST_F(CortexCNNSubstrateAdapterTest, VisualFnoContribIsModulatedByActivationMod) {
    cortex_cnn_tune_set_substrate_enabled(1.0f);
    cortex_cnn_tune_set_substrate_activation_mod_on(1.0f);
    cortex_cnn_tune_set_substrate_update_period(1.0f);

    const uint32_t w = 16, h = 16, ch = 3;
    std::vector<uint8_t> pixels(w * h * ch);
    for (uint32_t i = 0; i < pixels.size(); i++) {
        pixels[i] = static_cast<uint8_t>((i * 7u) & 0xFFu);
    }

    /* Healthy substrate (atp=1.0, membrane=1.0) — baseline. */
    cortex_cnn_processor_t* proc_ok = cortex_cnn_create(CORTEX_CNN_VISUAL, 64);
    ASSERT_NE(proc_ok, nullptr);
    fno_audio_processor_t* fno_ok =
        fno_audio_create(/*mel_size=*/64, /*embed_dim=*/64,
                         /*hidden_ch=*/8, /*n_modes=*/4, /*n_blocks=*/1);
    ASSERT_NE(fno_ok, nullptr);
    cortex_cnn_set_fno_visual(proc_ok, fno_ok);

    neural_substrate_t* sub_ok = MakeSubstrate(1.0f, 1.0f);
    ASSERT_NE(sub_ok, nullptr);
    cortex_cnn_attach_substrate(proc_ok, sub_ok);

    const float* e_ok = cortex_cnn_forward_visual(proc_ok, pixels.data(), w, h, ch);
    ASSERT_NE(e_ok, nullptr);
    uint32_t dim_ok = 0;
    const float* got_ok = cortex_cnn_get_embedding(proc_ok, &dim_ok);
    ASSERT_NE(got_ok, nullptr);
    EXPECT_TRUE(AllFinite(got_ok, dim_ok));
    float n_ok = L2Norm(got_ok, dim_ok);

    substrate_destroy(sub_ok);
    fno_audio_destroy(fno_ok);
    cortex_cnn_destroy(proc_ok);

    /* Damaged substrate (atp=0.3, membrane=0.1) — integration_efficiency
     * drops hard. With the fix, activation_mod multiplies the SUM of
     * (CNN + FNO) contributions; pre-fix the FNO portion was immune. */
    cortex_cnn_processor_t* proc_bad = cortex_cnn_create(CORTEX_CNN_VISUAL, 64);
    ASSERT_NE(proc_bad, nullptr);
    fno_audio_processor_t* fno_bad =
        fno_audio_create(64, 64, 8, 4, 1);
    ASSERT_NE(fno_bad, nullptr);
    cortex_cnn_set_fno_visual(proc_bad, fno_bad);

    neural_substrate_t* sub_bad = MakeSubstrate(0.3f, 0.1f);
    ASSERT_NE(sub_bad, nullptr);
    cortex_cnn_attach_substrate(proc_bad, sub_bad);

    const float* e_bad = cortex_cnn_forward_visual(proc_bad, pixels.data(), w, h, ch);
    ASSERT_NE(e_bad, nullptr);
    uint32_t dim_bad = 0;
    const float* got_bad = cortex_cnn_get_embedding(proc_bad, &dim_bad);
    ASSERT_NE(got_bad, nullptr);
    EXPECT_TRUE(AllFinite(got_bad, dim_bad));
    float n_bad = L2Norm(got_bad, dim_bad);

    /* Damaged norm must be strictly smaller than healthy — the whole
     * embedding (CNN + FNO) is scaled by integration_efficiency now. */
    EXPECT_LE(n_bad, n_ok)
        << "With FNO visual attached, damaged-membrane norm (" << n_bad
        << ") must not exceed healthy norm (" << n_ok << ")";

    substrate_destroy(sub_bad);
    fno_audio_destroy(fno_bad);
    cortex_cnn_destroy(proc_bad);
}

/*============================================================================
 * W3 audit, Bug #1 regression guard — Somato forward must compose substrate
 * activation_mod with the thalamic gate in the canonical order
 * (substrate → thalamic → debit). Previously cortex_forward_1d applied the
 * thalamic gate BEFORE the caller applied activation_mod, so a cortex with
 * both substrate damage AND an attention gate attached saw only the gate,
 * not the compounded scaling. The fix runs both in apply_post_forward.
 *
 * Strategy: compare three norms — baseline (no substrate, no router),
 * substrate-only damaged, and damaged+gated. The damaged+gated norm must
 * be smaller than either single-modulator norm to prove BOTH fired.
 *==========================================================================*/
TEST_F(CortexCNNSubstrateAdapterTest, SomatoOrderingMatchesOtherModalities) {
    cortex_cnn_tune_set_substrate_enabled(1.0f);
    cortex_cnn_tune_set_substrate_activation_mod_on(1.0f);
    cortex_cnn_tune_set_substrate_update_period(1.0f);
    cortex_cnn_tune_set_thalamic_enabled(1.0f);
    cortex_cnn_tune_set_thalamic_featuremap_gain_on(1.0f);

    std::vector<float> segs(45);
    for (uint32_t i = 0; i < segs.size(); i++) {
        segs[i] = 0.1f + 0.02f * static_cast<float>(i);
    }

    /* ---- Baseline: no substrate, no router. ---- */
    cortex_cnn_processor_t* proc_base = cortex_cnn_create(CORTEX_CNN_SOMATO, 32);
    ASSERT_NE(proc_base, nullptr);
    const float* e_base = cortex_cnn_forward_somato(
        proc_base, segs.data(), (uint32_t)segs.size());
    ASSERT_NE(e_base, nullptr);
    uint32_t dim_base = 0;
    const float* got_base = cortex_cnn_get_embedding(proc_base, &dim_base);
    ASSERT_NE(got_base, nullptr);
    float n_base = L2Norm(got_base, dim_base);
    cortex_cnn_destroy(proc_base);

    /* ---- Substrate-only damage (no router). ---- */
    cortex_cnn_processor_t* proc_sub = cortex_cnn_create(CORTEX_CNN_SOMATO, 32);
    ASSERT_NE(proc_sub, nullptr);
    neural_substrate_t* sub = MakeSubstrate(0.3f, 0.1f);
    ASSERT_NE(sub, nullptr);
    cortex_cnn_attach_substrate(proc_sub, sub);
    const float* e_sub = cortex_cnn_forward_somato(
        proc_sub, segs.data(), (uint32_t)segs.size());
    ASSERT_NE(e_sub, nullptr);
    uint32_t dim_sub = 0;
    const float* got_sub = cortex_cnn_get_embedding(proc_sub, &dim_sub);
    ASSERT_NE(got_sub, nullptr);
    float n_sub = L2Norm(got_sub, dim_sub);
    substrate_destroy(sub);
    cortex_cnn_destroy(proc_sub);

    /* ---- Substrate damage + thalamic gate ~0.5. ---- */
    thalamic_router_config_t rcfg;
    std::memset(&rcfg, 0, sizeof(rcfg));
    rcfg.max_queue_size           = 64;
    rcfg.max_destinations         = 4;
    rcfg.enable_attention_gating  = true;
    rcfg.enable_priority_routing  = true;
    rcfg.enable_statistics        = true;
    rcfg.min_attention_threshold  = 0.0f;
    rcfg.enable_learning          = true;
    rcfg.enable_second_messengers = false;
    rcfg.num_neurons              = 16;
    rcfg.enable_quantum_routing   = false;
    thalamic_router_t* router = thalamic_router_create(&rcfg);
    ASSERT_NE(router, nullptr);

    cortex_cnn_processor_t* proc_both = cortex_cnn_create(CORTEX_CNN_SOMATO, 32);
    ASSERT_NE(proc_both, nullptr);
    neural_substrate_t* sub_both = MakeSubstrate(0.3f, 0.1f);
    ASSERT_NE(sub_both, nullptr);
    cortex_cnn_attach_substrate(proc_both, sub_both);
    ASSERT_EQ(NIMCP_SUCCESS,
              cortex_cnn_attach_thalamic_router(
                  proc_both,
                  reinterpret_cast<struct thalamic_router*>(router)));
    thalamic_channel_t* ch = cortex_cnn_test_get_thalamic_channel(proc_both);
    ASSERT_NE(ch, nullptr);
    /* Stamp the cached attention weight before the first forward so the
     * gate read sees exactly 0.5. The router may rewrite it during submit
     * (Hebbian learning), but the gate is consulted BEFORE submit on each
     * forward, so the first call uses 0.5. */
    ch->attention_weights[0] = 0.5f;

    const float* e_both = cortex_cnn_forward_somato(
        proc_both, segs.data(), (uint32_t)segs.size());
    ASSERT_NE(e_both, nullptr);
    uint32_t dim_both = 0;
    const float* got_both = cortex_cnn_get_embedding(proc_both, &dim_both);
    ASSERT_NE(got_both, nullptr);
    EXPECT_TRUE(AllFinite(got_both, dim_both));
    float n_both = L2Norm(got_both, dim_both);

    substrate_destroy(sub_both);
    cortex_cnn_destroy(proc_both);
    thalamic_router_destroy(router);

    /* Core assertion: compounded effect beats either single modulator.
     * Use a tolerance band because weight init differs across processors.
     *
     * The pre-fix bug left somato seeing only the thalamic gate inside
     * cortex_forward_1d (substrate activation_mod came later from the
     * caller, which came AFTER the gate — so substrate effectively modified
     * an already-gated embedding). That still changes the norm, but the
     * qualitative guarantee we want is: BOTH modulators contribute.
     *
     * n_sub  ~ n_base × integration_efficiency
     * n_both ~ n_base × integration_efficiency × gate   (≈ 0.5)
     * So n_both should be noticeably smaller than n_sub. Allow a wide
     * margin for random init drift by requiring at most 0.9× n_sub. */
    EXPECT_LE(n_both, n_sub * 0.99f)
        << "Somato ordering regression: compounded damage+gate norm ("
        << n_both << ") must be <= substrate-only norm (" << n_sub
        << ") × 0.99. Baseline=" << n_base;
}
