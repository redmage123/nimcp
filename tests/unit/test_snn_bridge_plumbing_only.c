/**
 * @file test_snn_bridge_plumbing_only.c
 * @brief Slice A (Option-1 rebuild) — verify the bridge is transport-only.
 *
 * Coverage:
 *   1. test_transport_api_exists:
 *      route_concept_to_word / route_word_to_concept accept valid inputs,
 *      return 0, write a reasonable number of output entries. Slice A's
 *      identity-mapping stub is enough to validate the API shape; the
 *      registry-backed mapping is a Slice B+A walkthrough concern.
 *
 *   2. test_learning_apis_are_noops:
 *      Every former learning API (apply_stdp, strengthen_binding,
 *      strengthen_binding_riemannian, prune, bind, concept_spike,
 *      word_spike, echo_correct, reset_weights, set_da_modulation_*,
 *      set_comprehend_stdp_enabled, set_trigram_learning_enabled,
 *      set_ltd_margin, sleep_consolidate) returns success on a valid
 *      bridge handle and produces no observable weight side effects.
 *      Stats counters tied to weight updates stay flat at zero.
 *
 *   3. test_transport_telemetry_counts_messages_not_weights:
 *      total_spike_routes_concept_to_word / _word_to_concept bump exactly
 *      once per call. total_stdp_updates / total_ltp_events /
 *      total_ltd_events / echo_correct_* stay at zero.
 *
 *   4. test_save_load_roundtrip_no_bindings:
 *      Create a bridge, save it, load it back, verify zero bindings
 *      survived (because there are none to save). active_bindings == 0,
 *      avg_binding_weight == 0.
 *
 * Pattern: standalone smoke test (no GTest dep), matches the style of
 * test_lang_bridge_cosine_norm.c. Compile:
 *   gcc -I include tests/unit/test_snn_bridge_plumbing_only.c \
 *       -L build/lib -lnimcp -lm \
 *       -Wl,-rpath,/home/bbrelin/nimcp/build/lib \
 *       -o /tmp/test_snn_bridge_plumbing_only
 */

#include "snn/bridges/nimcp_snn_language_bridge.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

static int g_failures = 0;

#define EXPECT(cond, ...) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d " #cond " : ", __func__, __LINE__); \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n"); \
        g_failures++; \
    } \
} while (0)

#define EXPECT_EQ(a, b, ...) do { \
    unsigned long long _aa = (unsigned long long)(a); \
    unsigned long long _bb = (unsigned long long)(b); \
    if (_aa != _bb) { \
        fprintf(stderr, "FAIL %s:%d expected %llu == %llu : ", \
                __func__, __LINE__, _aa, _bb); \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n"); \
        g_failures++; \
    } \
} while (0)

/* Helper: spin up a small bridge with a handful of registered pops. */
static snn_language_bridge_t* build_test_bridge(void)
{
    snn_lang_config_t cfg = snn_lang_config_default();
    cfg.max_concept_pops = 16;
    cfg.max_word_pops    = 16;
    snn_language_bridge_t* b = snn_language_bridge_create(&cfg);
    if (!b) return NULL;

    for (uint32_t c = 0; c < 8; c++) {
        snn_language_bridge_register_concept(b, c, /*concept_id=*/c + 1);
    }
    snn_language_bridge_register_word(b, 0, "alpha");
    snn_language_bridge_register_word(b, 1, "beta");
    snn_language_bridge_register_word(b, 2, "gamma");
    return b;
}

/*===========================================================================
 * Test 1 — transport API exists + accepts inputs + returns success.
 *===========================================================================*/
static void test_transport_api_exists(void)
{
    snn_language_bridge_t* b = build_test_bridge();
    EXPECT(b != NULL, "bridge create failed");
    if (!b) return;

    uint32_t in_concepts[] = {0, 1, 2, 3};
    uint32_t out_words[16] = {0};
    size_t n_out = 0;
    int rc = snn_language_bridge_route_concept_to_word(
        b, in_concepts, 4, out_words, &n_out, 16);
    EXPECT_EQ(rc, 0, "route_concept_to_word should succeed");
    EXPECT_EQ(n_out, 4, "route should write 4 entries for 4 inputs");

    uint32_t in_words[] = {0, 1};
    uint32_t out_concepts[16] = {0};
    size_t n_concepts_out = 0;
    rc = snn_language_bridge_route_word_to_concept(
        b, in_words, 2, out_concepts, &n_concepts_out, 16);
    EXPECT_EQ(rc, 0, "route_word_to_concept should succeed");
    EXPECT_EQ(n_concepts_out, 2, "route should write 2 entries for 2 inputs");

    /* NULL/zero inputs are accepted (transport can be queried with empty
     * input — defensive). */
    rc = snn_language_bridge_route_concept_to_word(
        b, NULL, 0, out_words, &n_out, 16);
    EXPECT_EQ(rc, 0, "route with n=0 should succeed");
    EXPECT_EQ(n_out, 0, "route with n=0 should write 0 entries");

    /* NULL bridge → -1. */
    rc = snn_language_bridge_route_concept_to_word(
        NULL, in_concepts, 4, out_words, &n_out, 16);
    EXPECT_EQ(rc, -1, "route with NULL bridge should fail");

    snn_language_bridge_destroy(b);
}

/*===========================================================================
 * Test 2 — all learning APIs are no-ops, return 0, leave stats at zero.
 *===========================================================================*/
static void test_learning_apis_are_noops(void)
{
    snn_language_bridge_t* b = build_test_bridge();
    EXPECT(b != NULL, "bridge create failed");
    if (!b) return;

    /* Every former learning function must return 0 on a valid handle. */
    EXPECT_EQ(snn_language_bridge_apply_stdp(b, 100.0f), 0,
        "apply_stdp returns 0");
    EXPECT_EQ(snn_language_bridge_concept_spike(b, 0, 100.0f), 0,
        "concept_spike returns 0");
    EXPECT_EQ(snn_language_bridge_word_spike(b, 0, 100.0f), 0,
        "word_spike returns 0");
    EXPECT_EQ(snn_language_bridge_bind(b, 0, 0, 0.5f), 0,
        "bind returns 0");
    EXPECT_EQ(snn_language_bridge_strengthen_binding(b, 0, 0, 0.1f), 0,
        "strengthen_binding returns 0");
    EXPECT_EQ(snn_language_bridge_strengthen_binding_riemannian(b, 0, 0, 0.1f), 0,
        "strengthen_binding_riemannian returns 0");
    EXPECT_EQ(snn_language_bridge_prune(b, 0.01f), 0,
        "prune returns 0");
    EXPECT_EQ(snn_language_bridge_sleep_consolidate(b, 0.5f), 0,
        "sleep_consolidate returns 0");

    /* Telemetry-side: weight-update counters stay flat at zero. */
    snn_lang_stats_t stats = {0};
    int rc = snn_language_bridge_get_stats(b, &stats);
    EXPECT_EQ(rc, 0, "get_stats returns 0");
    EXPECT_EQ(stats.total_stdp_updates, 0, "no STDP updates");
    EXPECT_EQ(stats.total_ltp_events, 0, "no LTP events");
    EXPECT_EQ(stats.total_ltd_events, 0, "no LTD events");
    EXPECT_EQ(stats.echo_correct_calls, 0,
        "echo_correct deprecated — no calls recorded");
    EXPECT_EQ(stats.echo_correct_pairs, 0, "no echo_correct pairs");
    EXPECT_EQ(stats.comprehend_stdp_passes, 0, "no comprehend-STDP passes");
    EXPECT_EQ(stats.comprehend_stdp_pairs_fired, 0,
        "no comprehend-STDP pairs fired");
    EXPECT_EQ(stats.da_gated_stdp_passes, 0, "no DA-gated STDP passes");
    EXPECT_EQ(stats.active_bindings, 0,
        "no active bindings (transport-only bridge)");
    EXPECT(stats.avg_binding_weight == 0.0f,
        "avg_binding_weight is 0 (no bindings)");

    /* Echo-correct call: returns 0 (no pairs strengthened). */
    float intent[8] = {1, 1, 1, 1, 1, 1, 1, 1};
    int pairs = snn_language_bridge_echo_correct(b, intent, 8, "alpha", 1.0f);
    EXPECT_EQ(pairs, 0, "echo_correct produces zero pairs (no bridge weights)");

    /* DA/comprehend/trigram setters return 0 (or false). */
    EXPECT_EQ(snn_language_bridge_set_da_modulation_enabled(b, true), 0,
        "set_da_modulation_enabled returns 0");
    EXPECT_EQ(snn_language_bridge_get_da_modulation_enabled(b), false,
        "get_da_modulation_enabled returns false");
    EXPECT_EQ(snn_language_bridge_set_da_modulation_gain(b, 50.0f), 0,
        "set_da_modulation_gain returns 0");
    EXPECT_EQ(snn_language_bridge_set_comprehend_stdp_enabled(b, true), 0,
        "set_comprehend_stdp_enabled returns 0");
    EXPECT_EQ(snn_language_bridge_get_comprehend_stdp_enabled(b), false,
        "get_comprehend_stdp_enabled returns false");
    EXPECT_EQ(snn_language_bridge_set_trigram_learning_enabled(b, true), 0,
        "set_trigram_learning_enabled returns 0");
    EXPECT_EQ(snn_language_bridge_get_trigram_learning_enabled(b), false,
        "get_trigram_learning_enabled returns false");
    EXPECT_EQ(snn_language_bridge_set_ltd_margin(b, 2.0f), 0,
        "set_ltd_margin returns 0");
    EXPECT(snn_language_bridge_get_ltd_margin(b) == 0.0f,
        "get_ltd_margin returns 0.0f");
    EXPECT_EQ(snn_language_bridge_reset_weights(b, 0.0f, 0.1f), 0,
        "reset_weights returns 0 (nothing to reset)");

    snn_language_bridge_destroy(b);
}

/*===========================================================================
 * Test 3 — message-volume telemetry counts route calls, not weight updates.
 *===========================================================================*/
static void test_transport_telemetry_counts_messages_not_weights(void)
{
    snn_language_bridge_t* b = build_test_bridge();
    EXPECT(b != NULL, "bridge create failed");
    if (!b) return;

    snn_lang_stats_t stats0 = {0};
    snn_language_bridge_get_stats(b, &stats0);
    EXPECT_EQ(stats0.total_spike_routes_concept_to_word, 0,
        "concept→word route counter starts at 0");
    EXPECT_EQ(stats0.total_spike_routes_word_to_concept, 0,
        "word→concept route counter starts at 0");

    uint32_t in[] = {0, 1};
    uint32_t out[8] = {0};
    size_t n_out = 0;
    for (int i = 0; i < 5; i++) {
        snn_language_bridge_route_concept_to_word(b, in, 2, out, &n_out, 8);
    }
    for (int i = 0; i < 3; i++) {
        snn_language_bridge_route_word_to_concept(b, in, 2, out, &n_out, 8);
    }

    snn_lang_stats_t stats1 = {0};
    snn_language_bridge_get_stats(b, &stats1);
    EXPECT_EQ(stats1.total_spike_routes_concept_to_word, 5,
        "5 c→w routes counted");
    EXPECT_EQ(stats1.total_spike_routes_word_to_concept, 3,
        "3 w→c routes counted");

    /* Weight-update counters stay flat regardless of route call volume. */
    EXPECT_EQ(stats1.total_stdp_updates, 0, "no STDP updates ever");
    EXPECT_EQ(stats1.total_ltp_events, 0, "no LTP events ever");
    EXPECT_EQ(stats1.total_ltd_events, 0, "no LTD events ever");

    snn_language_bridge_destroy(b);
}

/*===========================================================================
 * Test 4 — save/load round-trip; no binding state survives.
 *===========================================================================*/
static void test_save_load_roundtrip_no_bindings(void)
{
    snn_language_bridge_t* b = build_test_bridge();
    EXPECT(b != NULL, "bridge create failed");
    if (!b) return;

    /* Even if we call the old "bind" function, no weights should land. */
    snn_language_bridge_bind(b, 0, 0, 1.0f);
    snn_language_bridge_strengthen_binding(b, 0, 1, 0.5f);

    snn_lang_stats_t stats_pre = {0};
    snn_language_bridge_get_stats(b, &stats_pre);
    EXPECT_EQ(stats_pre.active_bindings, 0,
        "no bindings created even after bind()/strengthen()");

    char path_template[] = "/tmp/test_snn_bridge_plumbing_XXXXXX.bin";
    int fd = mkstemps(path_template, 4);
    EXPECT(fd >= 0, "mkstemps failed");
    if (fd < 0) {
        snn_language_bridge_destroy(b);
        return;
    }
    close(fd);

    int save_rc = snn_language_bridge_save(b, path_template);
    EXPECT_EQ(save_rc, 0, "save succeeds");

    snn_language_bridge_destroy(b);

    snn_language_bridge_t* loaded = snn_language_bridge_load(path_template);
    EXPECT(loaded != NULL, "load returns a bridge");
    if (loaded) {
        snn_lang_stats_t stats_post = {0};
        snn_language_bridge_get_stats(loaded, &stats_post);
        EXPECT_EQ(stats_post.active_bindings, 0,
            "no bindings round-tripped through save/load");
        EXPECT(stats_post.avg_binding_weight == 0.0f,
            "avg_binding_weight is 0 after load");
        snn_language_bridge_destroy(loaded);
    }
    unlink(path_template);
}

/*===========================================================================
 * Test 5 — config struct has no live STDP fields; defaults compile + safe.
 *===========================================================================*/
static void test_default_config_compiles(void)
{
    /* Verify the default config builder still produces a usable struct.
     * The STDP fields are still present in the struct for ABI but the
     * defaults must be finite and non-negative so create() succeeds. */
    snn_lang_config_t cfg = snn_lang_config_default();
    EXPECT(cfg.max_concept_pops > 0, "max_concept_pops nonzero");
    EXPECT(cfg.max_word_pops > 0, "max_word_pops nonzero");
    EXPECT(cfg.neurons_per_pop > 0, "neurons_per_pop nonzero");
    /* These fields exist in the struct for ABI compat but the bridge
     * code never reads them in Slice A — just sanity-check finite. */
    EXPECT(isfinite(cfg.stdp_tau_plus),  "stdp_tau_plus finite");
    EXPECT(isfinite(cfg.stdp_tau_minus), "stdp_tau_minus finite");
    EXPECT(isfinite(cfg.stdp_a_plus),    "stdp_a_plus finite");
    EXPECT(isfinite(cfg.stdp_a_minus),   "stdp_a_minus finite");
}

int main(void)
{
    fprintf(stdout, "==== test_snn_bridge_plumbing_only ====\n");
    test_transport_api_exists();
    test_learning_apis_are_noops();
    test_transport_telemetry_counts_messages_not_weights();
    test_save_load_roundtrip_no_bindings();
    test_default_config_compiles();
    if (g_failures == 0) {
        fprintf(stdout, "OK — all transport-only invariants hold\n");
        return 0;
    }
    fprintf(stdout, "FAILED %d assertion(s)\n", g_failures);
    return 1;
}
