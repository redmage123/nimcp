/**
 * @file test_lang_persistence_roundtrip.c
 * @brief Integration test — V3 save/load round-trip for the SNN language bridge.
 *
 * Three phases:
 *   A. Create a bridge with EVERY config knob deviated from defaults plus
 *      registrations + 50 bindings; save → destroy → load; assert every
 *      field round-trips and binding/concept/word counts are restored.
 *
 *   B. Bit-identical produce: build a sibling original bridge (same config,
 *      same registrations, same bindings — never saved/loaded) and compare
 *      its produce outputs against the loaded bridge with rng_seed=12345 on
 *      both. Mismatches are real persistence bugs, not test bugs.
 *
 *   C. Backward-compat sanity: load a V2 fixture (skipped here — see comment).
 *
 * Compile:
 *   gcc -O0 -g -I include tests/integration/test_lang_persistence_roundtrip.c \
 *       -L build/lib -lnimcp -lm -lpthread \
 *       -Wl,-rpath,/home/bbrelin/nimcp/build/lib \
 *       -o /tmp/test_lang_persistence_roundtrip
 */

#include "snn/bridges/nimcp_snn_language_bridge.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

static int g_failures = 0;
static int g_phaseA_field_checks = 0;

#define EXPECT(cond, ...) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d " #cond " : ", __func__, __LINE__); \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n"); \
        g_failures++; \
    } \
} while (0)

#define EXPECT_FLOAT_EQ(a, b, tol) do { \
    float _a = (a), _b = (b); \
    if (!(fabsf(_a - _b) <= (tol))) { \
        fprintf(stderr, "FAIL %s:%d %s ~= %s : %.9f vs %.9f (tol %.6g)\n", \
                __func__, __LINE__, #a, #b, _a, _b, (double)(tol)); \
        g_failures++; \
    } \
} while (0)

#define EXPECT_FIELD_EQ_F(field, expected, tol) do { \
    EXPECT_FLOAT_EQ((field), (expected), (tol)); \
    g_phaseA_field_checks++; \
} while (0)

#define EXPECT_FIELD_EQ_U(field, expected) do { \
    EXPECT((field) == (expected), \
           "%s: got %u expected %u", #field, (unsigned)(field), (unsigned)(expected)); \
    g_phaseA_field_checks++; \
} while (0)

#define EXPECT_FIELD_EQ_I(field, expected) do { \
    EXPECT((field) == (expected), \
           "%s: got %d expected %d", #field, (int)(field), (int)(expected)); \
    g_phaseA_field_checks++; \
} while (0)

#define EXPECT_FIELD_EQ_B(field, expected) do { \
    EXPECT(((field) ? 1 : 0) == ((expected) ? 1 : 0), \
           "%s: got %d expected %d", #field, (field) ? 1 : 0, (expected) ? 1 : 0); \
    g_phaseA_field_checks++; \
} while (0)

/* Per-pid temp paths to play nice with parallel runs. */
static void tmp_path(char* buf, size_t len, const char* tag)
{
    snprintf(buf, len, "/tmp/lang_bridge_roundtrip_%d_%s.bin",
             (int)getpid(), tag);
}

/* The single canonical custom config used by Phase A and Phase B's sibling
 * bridge — every knob explicitly deviated from the defaults. */
static snn_lang_config_t make_custom_config(void)
{
    snn_lang_config_t cfg = snn_lang_config_default();

    /* Cap to small sizes so the test stays cheap. */
    cfg.max_concept_pops = 8;
    cfg.max_word_pops    = 16;
    cfg.neurons_per_pop  = SNN_LANG_NEURONS_PER_POP;

    /* PA-6 sampling */
    cfg.temperature  = 0.7f;
    cfg.top_p        = 0.85f;
    cfg.produce_topk = 10;

    /* PA-5 GloVe blend + autoregressive */
    cfg.glove_blend          = 0.4f;
    cfg.intent_persistence   = 0.5f;
    cfg.word_feedback        = 0.3f;

    /* PA-3 SNN spike routing */
    cfg.enable_snn_spike_routing = true;
    cfg.activation_tau_ms        = 150.0f;

    /* PA-5+ + PA-6+ */
    cfg.use_hyperbolic_embeddings = true;
    cfg.sampling_mode             = 2;

    /* TIER1 beam / EOS / repetition */
    cfg.produce_beam_width = 3;
    cfg.eos_word_pop       = 42u;
    cfg.repetition_penalty = 0.25f;
    cfg.repetition_window  = 4;

    /* Existing knobs (already in cfg blob, V3 ext block does NOT cover these
     * — they ride along inside snn_lang_config_t directly). */
    cfg.enable_da_modulation       = true;
    cfg.da_modulation_gain         = 1.2f;
    cfg.enable_imagination         = true;
    cfg.enable_curiosity           = true;
    cfg.enable_sleep_consolidation = true;
    cfg.prune_threshold            = 0.05f;

    /* STDP knobs */
    cfg.stdp_tau_plus      = 22.0f;
    cfg.stdp_tau_minus     = 18.0f;
    cfg.stdp_a_plus        = 0.012f;
    cfg.stdp_a_minus       = 0.011f;
    cfg.stdp_learning_rate = 0.015f;

    /* Binding / decode knobs */
    cfg.binding_w_max    = 2.5f;
    cfg.decode_window_ms = 20.0f;
    cfg.decay_rate       = 0.9f;
    cfg.spike_blend      = 0.55f;

    return cfg;
}

/* Register concept_ids 100..107 + word_forms "wA00".."wA15" and seed 50
 * bindings. Pattern is deterministic — Phase B's sibling bridge uses the
 * same loop so its on-disk state matches the loaded bridge bit-for-bit. */
static void seed_pops_and_bindings(snn_language_bridge_t* b)
{
    /* 8 concept pops, 12 word pops (out of 16-pop capacity). */
    for (uint32_t c = 0; c < 8; c++) {
        EXPECT(snn_language_bridge_register_concept(b, c, 100u + c) == 0,
               "register_concept(%u)", c);
    }
    char wf[8];
    for (uint32_t w = 0; w < 12; w++) {
        snprintf(wf, sizeof(wf), "wA%02u", w);
        EXPECT(snn_language_bridge_register_word(b, w, wf) == 0,
               "register_word(%u)", w);
    }

    /* 50 bindings — vary weights so cosine norms aren't degenerate. */
    /* Deterministic but varied weights: w_initial = 0.05 + (i * 0.013). */
    uint32_t inserted = 0;
    for (uint32_t i = 0; inserted < 50 && i < 8u * 12u; i++) {
        uint32_t cp = i % 8u;
        uint32_t wp = (i / 8u) % 12u;
        float w_initial = 0.05f + (float)inserted * 0.013f;
        if (snn_language_bridge_bind(b, cp, wp, w_initial) == 0) {
            inserted++;
        }
    }
    EXPECT(inserted == 50u, "seeded 50 bindings; got %u", inserted);
}

/* === PHASE A ============================================================= */
static snn_language_bridge_t* phase_A_save_destroy_load(const char* save_path,
                                                          uint32_t* out_num_concepts,
                                                          uint32_t* out_num_words,
                                                          uint32_t* out_num_bindings)
{
    fprintf(stderr, "\n=== Phase A: round-trip non-default config ===\n");

    snn_lang_config_t cfg = make_custom_config();
    snn_language_bridge_t* b = snn_language_bridge_create(&cfg);
    EXPECT(b != NULL, "bridge create");
    if (!b) return NULL;

    seed_pops_and_bindings(b);

    /* Capture pre-save counts/state. */
    snn_lang_stats_t s_pre;
    EXPECT(snn_language_bridge_get_stats(b, &s_pre) == 0, "get_stats pre");
    *out_num_concepts = 8u;
    *out_num_words    = 12u;
    *out_num_bindings = s_pre.active_bindings;
    EXPECT(*out_num_bindings == 50u, "pre-save active_bindings 50; got %u",
           *out_num_bindings);

    /* Save then destroy. */
    EXPECT(snn_language_bridge_save(b, save_path) == 0, "save returns 0");
    snn_language_bridge_destroy(b);

    /* Load fresh — note we do NOT pre-create + populate; load() must rebuild
     * everything from the file. */
    snn_language_bridge_t* loaded = snn_language_bridge_load(save_path);
    EXPECT(loaded != NULL, "load returns non-NULL");
    if (!loaded) return NULL;

    /* === Verify every config field round-tripped === */
    snn_lang_config_t got;
    EXPECT(snn_language_bridge_get_config(loaded, &got) == 0,
           "get_config returns 0");

    snn_lang_config_t want = make_custom_config();
    /* PA + MQ knobs (covered by V3 ext block authoritatively). */
    EXPECT_FIELD_EQ_F(got.temperature,           want.temperature,           1e-6f);
    EXPECT_FIELD_EQ_F(got.top_p,                 want.top_p,                 1e-6f);
    EXPECT_FIELD_EQ_U(got.produce_topk,          want.produce_topk);
    EXPECT_FIELD_EQ_F(got.glove_blend,           want.glove_blend,           1e-6f);
    EXPECT_FIELD_EQ_F(got.intent_persistence,    want.intent_persistence,    1e-6f);
    EXPECT_FIELD_EQ_F(got.word_feedback,         want.word_feedback,         1e-6f);
    EXPECT_FIELD_EQ_B(got.enable_snn_spike_routing, want.enable_snn_spike_routing);
    EXPECT_FIELD_EQ_F(got.activation_tau_ms,     want.activation_tau_ms,     1e-6f);
    EXPECT_FIELD_EQ_B(got.use_hyperbolic_embeddings, want.use_hyperbolic_embeddings);
    EXPECT_FIELD_EQ_I(got.sampling_mode,         want.sampling_mode);

    /* Knobs that ride only inside the snn_lang_config_t struct blob. They
     * still must round-trip — the V3 writer dumps the whole blob first. */
    EXPECT_FIELD_EQ_U(got.max_concept_pops,      want.max_concept_pops);
    EXPECT_FIELD_EQ_U(got.max_word_pops,         want.max_word_pops);
    EXPECT_FIELD_EQ_U(got.neurons_per_pop,       want.neurons_per_pop);

    EXPECT_FIELD_EQ_F(got.stdp_tau_plus,         want.stdp_tau_plus,         1e-6f);
    EXPECT_FIELD_EQ_F(got.stdp_tau_minus,        want.stdp_tau_minus,        1e-6f);
    EXPECT_FIELD_EQ_F(got.stdp_a_plus,           want.stdp_a_plus,           1e-6f);
    EXPECT_FIELD_EQ_F(got.stdp_a_minus,          want.stdp_a_minus,          1e-6f);
    EXPECT_FIELD_EQ_F(got.stdp_learning_rate,    want.stdp_learning_rate,    1e-6f);

    EXPECT_FIELD_EQ_F(got.binding_w_max,         want.binding_w_max,         1e-6f);
    EXPECT_FIELD_EQ_F(got.decode_window_ms,      want.decode_window_ms,      1e-6f);
    EXPECT_FIELD_EQ_F(got.decay_rate,            want.decay_rate,            1e-6f);
    EXPECT_FIELD_EQ_F(got.spike_blend,           want.spike_blend,           1e-6f);

    EXPECT_FIELD_EQ_B(got.enable_da_modulation,       want.enable_da_modulation);
    EXPECT_FIELD_EQ_F(got.da_modulation_gain,    want.da_modulation_gain,    1e-6f);
    EXPECT_FIELD_EQ_B(got.enable_imagination,         want.enable_imagination);
    EXPECT_FIELD_EQ_B(got.enable_curiosity,           want.enable_curiosity);
    EXPECT_FIELD_EQ_B(got.enable_sleep_consolidation, want.enable_sleep_consolidation);
    EXPECT_FIELD_EQ_F(got.prune_threshold,       want.prune_threshold,       1e-6f);

    EXPECT_FIELD_EQ_U(got.produce_beam_width,    want.produce_beam_width);
    EXPECT_FIELD_EQ_U(got.eos_word_pop,          want.eos_word_pop);
    EXPECT_FIELD_EQ_F(got.repetition_penalty,    want.repetition_penalty,    1e-6f);
    EXPECT_FIELD_EQ_U(got.repetition_window,     want.repetition_window);

    /* === Verify population + binding counts restored === */
    snn_lang_stats_t s_post;
    EXPECT(snn_language_bridge_get_stats(loaded, &s_post) == 0,
           "get_stats post-load");
    EXPECT(s_post.active_bindings == *out_num_bindings,
           "active_bindings restored: pre=%u post=%u",
           *out_num_bindings, s_post.active_bindings);

    /* Spot-check that a known word_form round-tripped. */
    const char* wf3 = snn_language_bridge_get_word_form(loaded, 3);
    EXPECT(wf3 != NULL && strcmp(wf3, "wA03") == 0,
           "word_form[3] restored: got %s", wf3 ? wf3 : "(null)");

    /* === Verify norms cache rebuilt — calling recompute_norms should be a
     * no-op if the cache was correctly persisted/rebuilt at load.  We snapshot
     * decode output before/after recompute and check it doesn't change. ==== */
    {
        float intent[8];
        for (uint32_t i = 0; i < 8; i++) intent[i] = 0.1f * (float)(i + 1);

        snn_lang_word_result_t before[8];
        uint32_t n_before = 0;
        EXPECT(snn_language_bridge_decode_spikes(loaded, intent, 8,
                                                  before, 8, &n_before) == 0,
               "decode_spikes before recompute_norms");

        EXPECT(snn_language_bridge_recompute_norms(loaded) == 0,
               "recompute_norms");

        snn_lang_word_result_t after[8];
        uint32_t n_after = 0;
        EXPECT(snn_language_bridge_decode_spikes(loaded, intent, 8,
                                                  after, 8, &n_after) == 0,
               "decode_spikes after recompute_norms");

        EXPECT(n_before == n_after,
               "decode count steady across recompute: %u vs %u",
               n_before, n_after);
        for (uint32_t k = 0; k < n_before && k < n_after; k++) {
            /* Same top words AND same activations — within tight tolerance. */
            EXPECT(before[k].word_pop == after[k].word_pop,
                   "decode[%u].word_pop pre=%u post=%u",
                   k, before[k].word_pop, after[k].word_pop);
            float tol = 1e-5f;
            if (!(fabsf(before[k].activation - after[k].activation) <= tol)) {
                fprintf(stderr, "FAIL recompute_norms changed activation[%u]: "
                                "before=%.9f after=%.9f\n",
                        k, before[k].activation, after[k].activation);
                g_failures++;
            }
        }
    }

    fprintf(stderr, "Phase A: %d field checks, %d cumulative failures\n",
            g_phaseA_field_checks, g_failures);
    return loaded;
}

/* === PHASE B ============================================================= */
typedef struct {
    char     text[2048];
    uint32_t word_count;
    float    fluency;
    float    spike_confidence;
    float    entropy_confidence;
} produce_record_t;

static void run_10_produces(snn_language_bridge_t* b,
                             produce_record_t out[10])
{
    /* Always re-seed RNG so this is reproducible. */
    EXPECT(snn_language_bridge_set_rng_seed(b, 12345ULL) == 0,
           "set_rng_seed(12345)");

    /* Fixed 8-d intent vector. */
    float intent[8] = { 0.42f, 0.13f, -0.07f, 0.91f, 0.55f, 0.0f, 0.31f, -0.22f };

    for (int i = 0; i < 10; i++) {
        snn_lang_production_result_t pr;
        memset(&pr, 0, sizeof(pr));
        int rc = snn_language_bridge_produce(b, intent, 8, &pr);
        EXPECT(rc == 0, "produce iter %d returned %d", i, rc);
        memset(&out[i], 0, sizeof(out[i]));
        if (pr.text) {
            strncpy(out[i].text, pr.text, sizeof(out[i].text) - 1);
        }
        out[i].word_count          = pr.word_count;
        out[i].fluency             = pr.fluency;
        out[i].spike_confidence    = pr.spike_confidence;
        out[i].entropy_confidence  = pr.entropy_confidence;
        snn_lang_production_result_cleanup(&pr);
    }
}

static int phase_B_bit_identity(snn_language_bridge_t* loaded)
{
    fprintf(stderr, "\n=== Phase B: bit-identical produce after round-trip ===\n");

    /* Build a sibling original — same custom cfg, same registrations + bindings,
     * never saved or loaded. */
    snn_lang_config_t cfg = make_custom_config();
    snn_language_bridge_t* original = snn_language_bridge_create(&cfg);
    EXPECT(original != NULL, "sibling original create");
    if (!original) return -1;
    seed_pops_and_bindings(original);

    produce_record_t out_orig[10];
    produce_record_t out_loaded[10];

    run_10_produces(original, out_orig);
    run_10_produces(loaded,   out_loaded);

    int divergences = 0;
    for (int i = 0; i < 10; i++) {
        bool text_eq = (strcmp(out_orig[i].text, out_loaded[i].text) == 0);
        bool wc_eq   = (out_orig[i].word_count == out_loaded[i].word_count);
        bool fl_eq   = (fabsf(out_orig[i].fluency - out_loaded[i].fluency) <= 1e-6f);
        bool sc_eq   = (fabsf(out_orig[i].spike_confidence - out_loaded[i].spike_confidence) <= 1e-6f);
        bool ec_eq   = (fabsf(out_orig[i].entropy_confidence - out_loaded[i].entropy_confidence) <= 1e-6f);

        if (!(text_eq && wc_eq && fl_eq && sc_eq && ec_eq)) {
            divergences++;
            fprintf(stderr,
                "DIVERGE iter=%d: \n"
                "    text       : eq=%d  orig='%s'  loaded='%s'\n"
                "    word_count : eq=%d  orig=%u    loaded=%u\n"
                "    fluency    : eq=%d  orig=%.9f  loaded=%.9f\n"
                "    spike_conf : eq=%d  orig=%.9f  loaded=%.9f\n"
                "    entropy_c  : eq=%d  orig=%.9f  loaded=%.9f\n",
                i,
                text_eq, out_orig[i].text, out_loaded[i].text,
                wc_eq,   out_orig[i].word_count, out_loaded[i].word_count,
                fl_eq,   out_orig[i].fluency, out_loaded[i].fluency,
                sc_eq,   out_orig[i].spike_confidence, out_loaded[i].spike_confidence,
                ec_eq,   out_orig[i].entropy_confidence, out_loaded[i].entropy_confidence);
            g_failures++;
        }
    }

    if (divergences == 0) {
        fprintf(stderr, "Phase B: PASS — all 10 produce calls bit-identical\n");
    } else {
        fprintf(stderr, "Phase B: FAIL — %d/10 produce calls diverged\n",
                divergences);
        fprintf(stderr,
            "Likely persistence-bug culprits to investigate:\n"
            "  1) rng_state not persisted (re-seeding should mask this — test\n"
            "     does so explicitly, so this is unlikely to be the cause).\n"
            "  2) word_norm_sq cache not persisted; recompute_norms is called\n"
            "     at load() but binding_insert vs node->binding=b may have\n"
            "     left tracking inconsistencies pre-rebuild.\n"
            "  3) word_emb_cache / word_emb_cached / word_emb_norm not\n"
            "     persisted (matter only when an emb_lookup_fn is attached —\n"
            "     this test does NOT attach one, so should not bite here).\n"
            "  4) attached_pops / n_attached_pops not persisted — only\n"
            "     matters if drain_pop_spikes was used pre-save (this test\n"
            "     does not).\n"
            "  5) emb_query_override or stats counters affecting decode in\n"
            "     subtle ways (stats are post-call, so unlikely).\n");
    }

    snn_language_bridge_destroy(original);
    return divergences;
}

/* === PHASE C ============================================================= */
static void phase_C_v2_compat(void)
{
    fprintf(stderr, "\n=== Phase C: V2 backward-compat sanity ===\n");
    /* No checked-in V2 fixture exists in tests/fixtures/, and synthesizing
     * one inside this integration test would re-run the V2-vs-V3 path that
     * tests/unit/test_lang_bridge_config_persistence.c (test 2) already
     * exercises in detail. Skip cleanly so we don't double-cover. */
    printf("[Phase C SKIP — no V2 fixture available; "
           "tests/unit/test_lang_bridge_config_persistence.c::"
           "test_v2_forward_compat covers V2→V3 reader compatibility]\n");
}

int main(void)
{
    fprintf(stderr, "[Integration] test_lang_persistence_roundtrip\n");

    char save_path[256];
    tmp_path(save_path, sizeof(save_path), "phaseA");
    /* Allow the user to override via env if they prefer the documented
     * /tmp/lang_bridge_roundtrip.bin path — keeps the test self-contained
     * for parallel runners by default. */
    const char* override_path = getenv("LANG_BRIDGE_ROUNDTRIP_PATH");
    const char* path = override_path ? override_path
                                     : "/tmp/lang_bridge_roundtrip.bin";
    /* Use the documented /tmp path per task spec. */
    (void)save_path;

    uint32_t num_concepts = 0, num_words = 0, num_bindings = 0;
    snn_language_bridge_t* loaded = phase_A_save_destroy_load(
        path, &num_concepts, &num_words, &num_bindings);

    int phaseB_diverged = -1;
    if (loaded) {
        phaseB_diverged = phase_B_bit_identity(loaded);
        snn_language_bridge_destroy(loaded);
    } else {
        fprintf(stderr, "Phase A failed; skipping Phase B\n");
        g_failures++;
    }

    phase_C_v2_compat();

    unlink(path);

    fprintf(stderr, "\n--- Summary ---\n");
    fprintf(stderr, "Phase A: %d field checks (target >= 25)\n",
            g_phaseA_field_checks);
    fprintf(stderr, "Phase B: %s\n",
            phaseB_diverged == 0 ? "PASS (bit-identical)" :
            phaseB_diverged > 0  ? "FAIL (diverged — see DIVERGE lines above)" :
                                   "SKIPPED (Phase A failure)");
    fprintf(stderr, "Phase C: SKIPPED (no V2 fixture)\n");
    fprintf(stderr, "Total failures: %d\n", g_failures);

    return g_failures == 0 ? 0 : 1;
}
