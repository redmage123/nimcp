/**
 * @file test_lang_bridge_get_config.c
 * @brief Tier-4 #15 — verify snn_language_bridge_get_config roundtrips every
 *        knob a setter can reach.
 *
 * Pattern: standalone smoke test, no GTest dep. Compile:
 *   gcc -I include tests/unit/test_lang_bridge_get_config.c \
 *       -L build/lib -lnimcp -lm \
 *       -Wl,-rpath,/home/bbrelin/nimcp/build/lib \
 *       -o /tmp/test_lang_bridge_get_config
 *
 * Coverage:
 *   1. defaults: get_config on a freshly-created bridge equals
 *      snn_lang_config_default() field-for-field.
 *   2. roundtrip: flip a non-trivial subset of knobs through the existing
 *      setters (sampling, glove_blend, autoregressive, spike_routing,
 *      hyperbolic, sampling_mode, blend) and verify get_config sees the
 *      new values.
 *   3. NULL safety: get_config(NULL, &cfg) and get_config(b, NULL) both
 *      return -1 without crashing.
 */

#include "snn/bridges/nimcp_snn_language_bridge.h"

#include <math.h>
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

#define EXPECT_FEQ(a, b, eps, label) do { \
    if (fabsf((a) - (b)) > (eps)) { \
        fprintf(stderr, "FAIL %s:%d %s mismatch: got %.6f expected %.6f\n", \
                __func__, __LINE__, (label), (double)(a), (double)(b)); \
        g_failures++; \
    } \
} while (0)

static void test_defaults_match(void)
{
    snn_lang_config_t want = snn_lang_config_default();
    snn_language_bridge_t* b = snn_language_bridge_create(&want);
    EXPECT(b != NULL, "bridge create");
    if (!b) return;

    snn_lang_config_t got;
    memset(&got, 0xAB, sizeof(got));  /* poison: catches "did not write" */
    int rc = snn_language_bridge_get_config(b, &got);
    EXPECT(rc == 0, "get_config rc=%d", rc);

    EXPECT_FEQ(got.temperature,        want.temperature,        1e-6f, "temperature");
    EXPECT_FEQ(got.top_p,              want.top_p,              1e-6f, "top_p");
    EXPECT_FEQ(got.glove_blend,        want.glove_blend,        1e-6f, "glove_blend");
    EXPECT_FEQ(got.intent_persistence, want.intent_persistence, 1e-6f, "intent_persistence");
    EXPECT_FEQ(got.word_feedback,      want.word_feedback,      1e-6f, "word_feedback");
    EXPECT_FEQ(got.spike_blend,        want.spike_blend,        1e-6f, "spike_blend");
    EXPECT(got.sampling_mode == want.sampling_mode,
            "sampling_mode got=%d want=%d", got.sampling_mode, want.sampling_mode);
    EXPECT(got.use_hyperbolic_embeddings == want.use_hyperbolic_embeddings,
            "use_hyperbolic_embeddings got=%d want=%d",
            (int)got.use_hyperbolic_embeddings, (int)want.use_hyperbolic_embeddings);
    EXPECT(got.enable_snn_spike_routing == want.enable_snn_spike_routing,
            "enable_snn_spike_routing got=%d want=%d",
            (int)got.enable_snn_spike_routing, (int)want.enable_snn_spike_routing);
    EXPECT(got.max_concept_pops == want.max_concept_pops,
            "max_concept_pops got=%u want=%u", got.max_concept_pops, want.max_concept_pops);
    EXPECT(got.max_word_pops == want.max_word_pops,
            "max_word_pops got=%u want=%u", got.max_word_pops, want.max_word_pops);

    snn_language_bridge_destroy(b);
}

static void test_setter_roundtrip(void)
{
    snn_lang_config_t cfg = snn_lang_config_default();
    snn_language_bridge_t* b = snn_language_bridge_create(&cfg);
    EXPECT(b != NULL, "bridge create");
    if (!b) return;

    /* Drive a non-trivial subset of knobs through the public setters. */
    EXPECT(snn_language_bridge_set_sampling(b, 0.7f, 0.85f) == 0, "set_sampling");
    EXPECT(snn_language_bridge_set_glove_blend(b, 0.4f) == 0, "set_glove_blend");
    EXPECT(snn_language_bridge_set_autoregressive(b, 0.25f, 0.6f) == 0,
            "set_autoregressive");
    EXPECT(snn_language_bridge_set_snn_spike_routing(b, true, 250.0f) == 0,
            "set_snn_spike_routing");
    EXPECT(snn_language_bridge_set_hyperbolic_embeddings(b, true) == 0,
            "set_hyperbolic_embeddings");
    EXPECT(snn_language_bridge_set_sampling_mode(b, 2) == 0, "set_sampling_mode");
    snn_language_bridge_set_blend(b, 0.42f);  /* spike_blend */

    snn_lang_config_t got;
    memset(&got, 0xCD, sizeof(got));
    int rc = snn_language_bridge_get_config(b, &got);
    EXPECT(rc == 0, "get_config rc=%d", rc);

    EXPECT_FEQ(got.temperature,        0.7f,  1e-6f, "temperature");
    EXPECT_FEQ(got.top_p,              0.85f, 1e-6f, "top_p");
    EXPECT_FEQ(got.glove_blend,        0.4f,  1e-6f, "glove_blend");
    EXPECT_FEQ(got.intent_persistence, 0.25f, 1e-6f, "intent_persistence");
    EXPECT_FEQ(got.word_feedback,      0.6f,  1e-6f, "word_feedback");
    EXPECT_FEQ(got.activation_tau_ms,  250.0f, 1e-3f, "activation_tau_ms");
    EXPECT_FEQ(got.spike_blend,        0.42f, 1e-6f, "spike_blend");
    EXPECT(got.enable_snn_spike_routing == true,
            "enable_snn_spike_routing got=%d", (int)got.enable_snn_spike_routing);
    EXPECT(got.use_hyperbolic_embeddings == true,
            "use_hyperbolic_embeddings got=%d", (int)got.use_hyperbolic_embeddings);
    EXPECT(got.sampling_mode == 2, "sampling_mode got=%d", got.sampling_mode);

    snn_language_bridge_destroy(b);
}

static void test_null_safety(void)
{
    snn_lang_config_t cfg = snn_lang_config_default();
    snn_language_bridge_t* b = snn_language_bridge_create(&cfg);
    EXPECT(b != NULL, "bridge create");
    if (!b) return;

    /* NULL bridge */
    snn_lang_config_t out;
    EXPECT(snn_language_bridge_get_config(NULL, &out) == -1,
            "NULL bridge must return -1");
    /* NULL out */
    EXPECT(snn_language_bridge_get_config(b, NULL) == -1,
            "NULL out must return -1");

    snn_language_bridge_destroy(b);
}

int main(void)
{
    fprintf(stderr, "[Tier-4 #15] test_lang_bridge_get_config\n");
    test_defaults_match();
    test_setter_roundtrip();
    test_null_safety();

    if (g_failures == 0) {
        fprintf(stderr, "OK — all 3 tests passed\n");
        return 0;
    } else {
        fprintf(stderr, "FAIL — %d failure(s)\n", g_failures);
        return 1;
    }
}
