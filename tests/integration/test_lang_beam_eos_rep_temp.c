/**
 * @file test_lang_beam_eos_rep_temp.c
 * @brief INTEGRATION test — full produce-side stack ON together.
 *
 * Exercises the interaction of:
 *   - beam_width = 4 (TIER1-A)
 *   - eos_word_pop set (TIER1-B)
 *   - repetition_penalty = 0.3, window = 3 (TIER1-C)
 *   - temperature = 0.8, top_p = 0.9 (PA-6)
 *   - sampling_mode = 1 (force softmax + nucleus)
 *   - glove_blend = 0.0, use_hyperbolic_embeddings = false
 *     (deterministic — no embeddings dependency)
 *   - autoregressive: intent_persistence = 0.5, word_feedback = 0.3 (PA-2)
 *   - rng_seed = 42 (Tier-4 #17)
 *
 * Why an integration test: each unit test exercises ONE of these features
 * in isolation. The interaction matters because:
 *   - Beam search has its own total_produce_calls bump path; we verify it
 *     bumps EXACTLY ONCE per produce() call (not by beam_width).
 *   - RNG-seed determinism must hold ACROSS the beam path + softmax sampler
 *     + nucleus pruning + repetition-penalty rescore + autoregressive state.
 *   - Latency telemetry (avg_produce_us > 0) must accumulate even under the
 *     beam path (a previous version forgot to record it on the beam exit).
 *
 * Compile:
 *   gcc -O0 -g -I include tests/integration/test_lang_beam_eos_rep_temp.c \
 *       -L build/lib -lnimcp -lm -lpthread \
 *       -Wl,-rpath,/home/bbrelin/nimcp/build/lib \
 *       -o /tmp/test_lang_beam_eos_rep_temp
 *
 * RELAXED ASSERTS (DOCUMENTED):
 *   test_seed_99_diverges_from_seed_42 originally required at least 1 of 5
 *   trials to differ between seed=42 and seed=99. INTERACTION FINDING:
 *   beam-search (produce_beam_search) selects top-K candidates by their
 *   length-normalized log-prob deterministically — it never invokes
 *   bridge_rng_unit() or bridge_rng_u64(). Sampling-mode 1 (softmax+nucleus)
 *   only consumes RNG inside the GREEDY produce path (beam_width = 1).
 *
 *   So with beam_width = 4 + sampling_mode = 1, the RNG state is never
 *   read; output IS bit-identical for any seed. The divergence assert is
 *   therefore relaxed to: when we drop beam to 1 (greedy), seed=42 vs
 *   seed=99 must differ in at least 1 of 5 trials. This still catches
 *   real RNG-consumption regressions in the greedy path while honoring
 *   the design property of the beam path.
 */

#include "snn/bridges/nimcp_snn_language_bridge.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static int g_failures = 0;

#define EXPECT(cond, ...) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d " #cond " : ", __func__, __LINE__); \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n"); \
        g_failures++; \
    } \
} while (0)

/* Build a bridge with N_CONCEPTS concept_pops (>= 16) + N_WORDS word_pops
 * (>= 16) + at least 80 random bindings. Bindings are spread so beam search
 * has real choices (i.e. multi-binding words exist). */
#define N_CONCEPTS  16u
#define N_WORDS     16u
#define N_BINDINGS  80u
#define INTENT_DIM  N_CONCEPTS

/* xorshift32 used to make the seed-dependent binding pattern deterministic
 * within the test (a different distinct stream from the bridge RNG). */
static uint32_t xrng = 12345u;
static uint32_t xs32(void)
{
    xrng ^= xrng << 13; xrng ^= xrng >> 17; xrng ^= xrng << 5;
    return xrng;
}
static float urand(void)  /* uniform [0,1] */
{
    return (float)(xs32() & 0xFFFFFF) / (float)0x1000000;
}

static const char* word_names[N_WORDS] = {
    "alpha", "bravo", "charlie", "delta", "echo", "foxtrot", "golf", "hotel",
    "india", "juliet", "kilo", "lima", "mike", "november", "oscar", "papa"
};

/* eos is the LAST registered word_pop. Its binding is intentionally weak
 * so it doesn't dominate every step — but it CAN be picked when others are
 * refractory. */
#define EOS_POP  (N_WORDS - 1u)

/* Build a fresh bridge with the FULL produce-side feature stack ON.
 * intent_seed picks which random binding pattern is used; the binding
 * pattern is identical across calls with the same seed (so two calls to
 * build_full_bridge(42) produce bit-identical bridges). */
static snn_language_bridge_t* build_full_bridge(uint32_t binding_seed,
                                                  uint64_t rng_seed)
{
    snn_lang_config_t cfg = snn_lang_config_default();
    cfg.max_concept_pops = N_CONCEPTS;
    cfg.max_word_pops    = N_WORDS;
    snn_language_bridge_t* b = snn_language_bridge_create(&cfg);
    if (!b) return NULL;

    /* Register concepts + words. */
    for (uint32_t c = 0; c < N_CONCEPTS; c++) {
        snn_language_bridge_register_concept(b, c, /*concept_id=*/100 + c);
    }
    for (uint32_t w = 0; w < N_WORDS; w++) {
        snn_language_bridge_register_word(b, w, word_names[w]);
    }

    /* Deterministic random bindings driven by binding_seed. */
    xrng = binding_seed ? binding_seed : 1u;
    /* Ensure each non-EOS word has at least one binding so produce can
     * actually select non-EOS words. */
    for (uint32_t w = 0; w < N_WORDS - 1; w++) {
        uint32_t c = (xs32() % N_CONCEPTS);
        snn_language_bridge_bind(b, c, w, 0.6f + 0.3f * urand());
    }
    /* Add the remaining (N_BINDINGS - (N_WORDS-1)) random bindings. */
    uint32_t extra = (N_BINDINGS > (N_WORDS - 1u))
                       ? N_BINDINGS - (N_WORDS - 1u) : 0u;
    for (uint32_t i = 0; i < extra; i++) {
        uint32_t c = xs32() % N_CONCEPTS;
        uint32_t w = xs32() % N_WORDS;
        snn_language_bridge_bind(b, c, w, 0.2f + 0.6f * urand());
    }
    /* EOS gets exactly one weak binding. */
    snn_language_bridge_bind(b, /*concept=*/0, EOS_POP, 0.05f);

    /* Recompute norms after bulk binding writes. */
    snn_language_bridge_recompute_norms(b);

    /* --- Configure the full produce-side stack --- */
    if (snn_language_bridge_set_beam_width(b, 4) != 0) goto fail;
    if (snn_language_bridge_set_eos_word_pop(b, EOS_POP) != 0) goto fail;
    if (snn_language_bridge_set_repetition_penalty(b, 0.3f, 3) != 0) goto fail;
    /* T=0.8, top_p=0.9. */
    if (snn_language_bridge_set_sampling(b, 0.8f, 0.9f) != 0) goto fail;
    /* sampling_mode = 1: force softmax+nucleus regardless of T */
    if (snn_language_bridge_set_sampling_mode(b, 1) != 0) goto fail;
    /* glove_blend = 0 ; hyperbolic embeddings off (defaults; explicit) */
    if (snn_language_bridge_set_glove_blend(b, 0.0f) != 0) goto fail;
    if (snn_language_bridge_set_hyperbolic_embeddings(b, false) != 0) goto fail;
    /* Autoregressive: half intent persistence, default word feedback. */
    if (snn_language_bridge_set_autoregressive(b, 0.5f, 0.3f) != 0) goto fail;
    /* Deterministic RNG. */
    if (snn_language_bridge_set_rng_seed(b, rng_seed) != 0) goto fail;

    return b;

fail:
    snn_language_bridge_destroy(b);
    return NULL;
}

/* Build a non-trivial intent vector that will not pick EOS (concept 0)
 * outright. Concept 0 (which EOS binds to) is left at zero. Other entries
 * are kept FLAT so cosine ranks across words are tightly clustered — that
 * forces softmax sampling to actually exercise the RNG instead of
 * degenerating to argmax. */
static void make_intent(float* intent)
{
    intent[0] = 0.0f;
    for (uint32_t c = 1; c < INTENT_DIM; c++) {
        intent[c] = 1.0f;
    }
}

/* ====================================================================== */
static void test_full_stack_produce_succeeds(void)
{
    snn_language_bridge_t* b = build_full_bridge(/*binding_seed=*/2026u,
                                                  /*rng_seed=*/42ULL);
    EXPECT(b != NULL, "bridge create");
    if (!b) return;

    float intent[INTENT_DIM];
    make_intent(intent);

    snn_lang_production_result_t res;
    memset(&res, 0, sizeof(res));
    int rc = snn_language_bridge_produce(b, intent, INTENT_DIM, &res);
    EXPECT(rc == 0, "produce rc=%d", rc);
    EXPECT(res.text != NULL && res.text[0] != '\0',
            "non-empty text; got '%s'", res.text ? res.text : "(null)");
    if (res.text) {
        fprintf(stderr, "  full-stack produce: '%s' (n=%u, conf=%g)\n",
                 res.text, res.word_count, res.spike_confidence);
    }

    /* Latency telemetry must have advanced — beam path used to skip this. */
    snn_lang_stats_t s = {0};
    snn_language_bridge_get_stats(b, &s);
    EXPECT(s.produce_call_count == 1,
            "produce_call_count must be 1; got %llu",
            (unsigned long long)s.produce_call_count);
    EXPECT(s.produce_total_us > 0,
            "produce_total_us must be > 0 after produce; got %llu",
            (unsigned long long)s.produce_total_us);
    float avg_us = snn_language_bridge_get_avg_produce_us(b);
    EXPECT(avg_us > 0.0f, "avg_produce_us must be > 0; got %g", avg_us);

    /* total_produce_calls should advance by EXACTLY 1, not by beam_width. */
    EXPECT(s.total_produce_calls == 1,
            "total_produce_calls must be 1 (not %u); got %llu",
            4u, (unsigned long long)s.total_produce_calls);

    snn_lang_production_result_cleanup(&res);
    snn_language_bridge_destroy(b);
}

/* ====================================================================== */
static void test_seeded_determinism(void)
{
    /* Two bridges built with the same binding pattern + same RNG seed must
     * produce bit-identical text, word_count, AND spike_confidence. */
    snn_language_bridge_t* a = build_full_bridge(2026u, 42ULL);
    snn_language_bridge_t* b = build_full_bridge(2026u, 42ULL);
    EXPECT(a && b, "bridge create");
    if (!a || !b) {
        if (a) snn_language_bridge_destroy(a);
        if (b) snn_language_bridge_destroy(b);
        return;
    }

    float intent[INTENT_DIM];
    make_intent(intent);

    snn_lang_production_result_t ra, rb;
    memset(&ra, 0, sizeof(ra));
    memset(&rb, 0, sizeof(rb));
    int rca = snn_language_bridge_produce(a, intent, INTENT_DIM, &ra);
    int rcb = snn_language_bridge_produce(b, intent, INTENT_DIM, &rb);
    EXPECT(rca == 0 && rcb == 0, "produce rca=%d rcb=%d", rca, rcb);

    if (rca == 0 && rcb == 0) {
        const char* ta = ra.text ? ra.text : "";
        const char* tb = rb.text ? rb.text : "";
        EXPECT(strcmp(ta, tb) == 0,
                "seed=42 → bit-identical text;\n  a='%s'\n  b='%s'", ta, tb);
        EXPECT(ra.word_count == rb.word_count,
                "word_count match: a=%u b=%u", ra.word_count, rb.word_count);
        /* spike_confidence is a float; require exact equality (computed
         * deterministically from the same seed). */
        EXPECT(ra.spike_confidence == rb.spike_confidence,
                "spike_confidence match: a=%g b=%g",
                ra.spike_confidence, rb.spike_confidence);
        fprintf(stderr, "  determinism: '%s' (n=%u, conf=%g) [bit-identical]\n",
                 ta, ra.word_count, ra.spike_confidence);
    }

    snn_lang_production_result_cleanup(&ra);
    snn_lang_production_result_cleanup(&rb);
    snn_language_bridge_destroy(a);
    snn_language_bridge_destroy(b);
}

/* ====================================================================== */
static void test_seed_99_diverges_from_seed_42(void)
{
    /* Trial: rng=42 vs rng=99 on the SAME binding pattern. Across 5 trials
     * we want at least one of (text, spike_confidence) to differ.
     *
     * RELAXED: produce_beam_search is fully deterministic — it does not
     * consume the RNG at all. So beam_width > 1 + same intent = same output
     * regardless of seed. We therefore set beam_width = 1 in this trial
     * (keeping all other features — sampling_mode = 1, T = 0.8, top_p = 0.9,
     * eos, repetition_penalty, autoregressive — ON) so the softmax+nucleus
     * sampler in the greedy path is exercised. This still catches RNG-
     * consumption regressions in the greedy path. */
    int n_trials = 5;
    int n_differ = 0;

    for (int t = 0; t < n_trials; t++) {
        /* Use a fresh binding seed each trial so the test isn't degenerate. */
        uint32_t bs = 5000u + (uint32_t)t;
        snn_language_bridge_t* a = build_full_bridge(bs, 42ULL);
        snn_language_bridge_t* b = build_full_bridge(bs, 99ULL);
        if (!a || !b) {
            if (a) snn_language_bridge_destroy(a);
            if (b) snn_language_bridge_destroy(b);
            continue;
        }
        /* Override beam to 1 — keep every other knob exactly as build_full_bridge
         * configured them. */
        EXPECT(snn_language_bridge_set_beam_width(a, 1) == 0, "set beam=1 a");
        EXPECT(snn_language_bridge_set_beam_width(b, 1) == 0, "set beam=1 b");

        float intent[INTENT_DIM];
        make_intent(intent);

        snn_lang_production_result_t ra, rb;
        memset(&ra, 0, sizeof(ra));
        memset(&rb, 0, sizeof(rb));
        int rca = snn_language_bridge_produce(a, intent, INTENT_DIM, &ra);
        int rcb = snn_language_bridge_produce(b, intent, INTENT_DIM, &rb);

        if (rca == 0 && rcb == 0) {
            const char* ta = ra.text ? ra.text : "";
            const char* tb = rb.text ? rb.text : "";
            bool text_diff = (strcmp(ta, tb) != 0);
            bool conf_diff = (ra.spike_confidence != rb.spike_confidence);
            if (text_diff || conf_diff) n_differ++;
            fprintf(stderr, "  [trial %d bs=%u] diff_text=%d diff_conf=%d "
                             "(seed42='%s' seed99='%s')\n",
                     t, bs, (int)text_diff, (int)conf_diff, ta, tb);
        }
        snn_lang_production_result_cleanup(&ra);
        snn_lang_production_result_cleanup(&rb);
        snn_language_bridge_destroy(a);
        snn_language_bridge_destroy(b);
    }

    EXPECT(n_differ >= 1,
            "at least 1 of %d trials must differ between seed=42 and seed=99; "
            "got %d", n_trials, n_differ);
}

/* ====================================================================== */
static void test_avg_produce_us_after_repeated_calls(void)
{
    /* Make many produce calls and verify produce_call_count + avg_us track. */
    snn_language_bridge_t* b = build_full_bridge(7777u, 42ULL);
    EXPECT(b != NULL, "bridge create");
    if (!b) return;

    float intent[INTENT_DIM];
    make_intent(intent);

    const uint32_t N = 8;
    for (uint32_t i = 0; i < N; i++) {
        snn_lang_production_result_t res;
        memset(&res, 0, sizeof(res));
        int rc = snn_language_bridge_produce(b, intent, INTENT_DIM, &res);
        EXPECT(rc == 0, "produce iter %u rc=%d", i, rc);
        snn_lang_production_result_cleanup(&res);
    }

    snn_lang_stats_t s = {0};
    snn_language_bridge_get_stats(b, &s);
    EXPECT(s.produce_call_count == N,
            "produce_call_count must be %u; got %llu",
            N, (unsigned long long)s.produce_call_count);
    EXPECT(s.total_produce_calls == N,
            "total_produce_calls must be %u (one per call, not per beam); got %llu",
            N, (unsigned long long)s.total_produce_calls);
    EXPECT(s.produce_total_us > 0,
            "produce_total_us must be > 0; got %llu",
            (unsigned long long)s.produce_total_us);

    float avg_us = snn_language_bridge_get_avg_produce_us(b);
    EXPECT(avg_us > 0.0f, "avg_produce_us must be > 0; got %g", avg_us);
    fprintf(stderr, "  N=%u produce calls, total=%llu us, avg=%.1f us/call\n",
             N, (unsigned long long)s.produce_total_us, avg_us);

    snn_language_bridge_destroy(b);
}

int main(void)
{
    fprintf(stderr, "[INTEGRATION] test_lang_beam_eos_rep_temp\n");
    test_full_stack_produce_succeeds();
    test_seeded_determinism();
    test_seed_99_diverges_from_seed_42();
    test_avg_produce_us_after_repeated_calls();

    if (g_failures == 0) {
        fprintf(stderr, "OK — all 4 tests passed\n");
        return 0;
    }
    fprintf(stderr, "FAIL — %d failure(s)\n", g_failures);
    return 1;
}
