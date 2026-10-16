/**
 * @file test_lang_immune_inspection.c
 * @brief IM-3 — verify Tier-3 immune content inspection on
 *        grounded_language_comprehend.
 *
 * Pattern: standalone smoke test. Compile:
 *   gcc -O2 -I include tests/unit/test_lang_immune_inspection.c \
 *       -L build/lib -lnimcp -lm -lpthread \
 *       -Wl,-rpath,/home/bbrelin/nimcp/build/lib \
 *       -o /tmp/test_lang_immune_inspection
 *
 * Coverage:
 *   1. test_disabled_default:
 *      Default OFF — comprehending any text leaves immune_inspections == 0
 *      and the confidence is unchanged from the legacy formula.
 *
 *   2. test_enabled_benign:
 *      Enable. Comprehend a benign 3-word sentence with normal
 *      activations. immune_inspections == 1, antigens_registered == 0,
 *      confidence is unchanged (no heuristic trips, inflammation == 0).
 *
 *   3. test_enabled_nonfinite:
 *      Enable. Bind a word with NaN-laced features. Comprehending text
 *      including that word produces NaN/Inf in activations → inflammation
 *      crosses the 0.5 antigen threshold; inspections >= 1, antigens >= 1,
 *      confidence is damped relative to the benign baseline.
 *
 *   4. test_strongly_inflamed_skips_engram:
 *      Enable both immune and engram. Push enough heuristic hits
 *      (NaN + repetition spam + outlier) so inflammation > 0.7 →
 *      engram_encodes does NOT advance for the inflamed call.
 */

#include "language/nimcp_grounded_language.h"
#include "cognitive/memory/nimcp_engram.h"
#include "cognitive/immune/nimcp_brain_immune.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
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

/* Build a benign feature vector — small bounded values, finite, varied. */
static void make_benign_features(float* feat, uint32_t dim, uint32_t seed) {
    for (uint32_t i = 0; i < dim; i++) {
        feat[i] = (float)((int)((i + seed * 13u) % 256u) - 128) / 256.0f;
    }
}

/* Build a NaN-laced feature vector — half the entries are NaN.
 * fast_map binds these into a lexicon entry whose concept features get
 * propagated into result->activation_levels at comprehend time. */
static void make_nan_features(float* feat, uint32_t dim) {
    float nan_val = nanf("");
    for (uint32_t i = 0; i < dim; i++) {
        feat[i] = (i & 1u) ? nan_val : 0.5f;
    }
}

/* Helper: call comprehend, copy back the confidence + concept_count, free
 * the result. Returns 0 on success, -1 on comprehend failure. */
static int run_comprehend(grounded_language_t* gl, const char* text,
                          float* out_conf, uint32_t* out_concepts) {
    gl_comprehension_result_t r;
    int rc = grounded_language_comprehend(gl, text, &r);
    if (rc != 0) return -1;
    if (out_conf) *out_conf = r.comprehension_confidence;
    if (out_concepts) *out_concepts = r.concept_count;
    gl_comprehension_result_cleanup(&r);
    return 0;
}

static void test_disabled_default(void) {
    grounded_language_t* gl = grounded_language_create(64, NULL);
    if (!gl) { g_failures++; return; }

    /* Bind a couple of benign words so comprehend has something to
     * activate. */
    float feat[64];
    make_benign_features(feat, 64, 1);
    grounded_language_fast_map(gl, "alpha", feat, 64, 1);
    make_benign_features(feat, 64, 2);
    grounded_language_fast_map(gl, "beta",  feat, 64, 1);

    /* Default OFF — no immune wiring. */
    EXPECT(!grounded_language_immune_enabled(gl),
           "expected immune disabled by default");

    float conf_before = -1.0f;
    EXPECT(run_comprehend(gl, "alpha beta", &conf_before, NULL) == 0,
           "comprehend rc");

    gl_stats_t stats;
    grounded_language_get_stats(gl, &stats);
    EXPECT(stats.immune_inspections == 0,
           "expected inspections=0 when disabled, got %llu",
           (unsigned long long)stats.immune_inspections);
    EXPECT(stats.immune_antigens_registered == 0,
           "expected antigens=0 when disabled, got %llu",
           (unsigned long long)stats.immune_antigens_registered);
    /* Confidence must be > 0 — both words known, no damp. */
    EXPECT(conf_before > 0.0f,
           "expected nonzero confidence, got %f", conf_before);
    printf("  [disabled] inspections=%llu  conf=%f\n",
           (unsigned long long)stats.immune_inspections, conf_before);

    grounded_language_destroy(gl);
}

static void test_enabled_benign(void) {
    grounded_language_t* gl = grounded_language_create(64, NULL);
    if (!gl) { g_failures++; return; }

    brain_immune_config_t cfg;
    brain_immune_default_config(&cfg);
    /* Disable integrations that pull in BBB/BFT/swarm — for this unit test
     * we only need the antigen presentation surface. */
    cfg.enable_bbb_integration   = false;
    cfg.enable_bft_integration   = false;
    cfg.enable_swarm_integration = false;
    cfg.enable_bio_async         = false;
    cfg.enable_logging           = false;
    brain_immune_system_t* immune = brain_immune_create(&cfg);
    if (!immune) {
        fprintf(stderr, "SKIP test_enabled_benign — brain_immune_create failed\n");
        grounded_language_destroy(gl);
        return;
    }

    float feat[64];
    make_benign_features(feat, 64, 11);
    grounded_language_fast_map(gl, "hello", feat, 64, 1);
    make_benign_features(feat, 64, 22);
    grounded_language_fast_map(gl, "world", feat, 64, 1);

    int rc_attach = grounded_language_set_immune_system(gl, immune, true);
    EXPECT(rc_attach == 0, "set_immune_system rc=%d", rc_attach);
    EXPECT(grounded_language_immune_enabled(gl),
           "expected immune enabled after attach");

    float conf = -1.0f;
    EXPECT(run_comprehend(gl, "hello world", &conf, NULL) == 0, "comprehend rc");

    gl_stats_t stats;
    grounded_language_get_stats(gl, &stats);
    EXPECT(stats.immune_inspections == 1,
           "expected inspections=1, got %llu",
           (unsigned long long)stats.immune_inspections);
    EXPECT(stats.immune_antigens_registered == 0,
           "expected antigens=0 on benign input, got %llu",
           (unsigned long long)stats.immune_antigens_registered);
    /* Confidence should be positive (both words known) and not collapsed —
     * benign input means inflammation == 0, damp factor 1.0, base × 1.0. */
    EXPECT(conf > 0.5f,
           "expected high benign confidence, got %f", conf);
    printf("  [benign]   inspections=%llu  antigens=%llu  conf=%f\n",
           (unsigned long long)stats.immune_inspections,
           (unsigned long long)stats.immune_antigens_registered,
           conf);

    brain_immune_destroy(immune);
    grounded_language_destroy(gl);
}

static void test_enabled_nonfinite(void) {
    grounded_language_t* gl = grounded_language_create(64, NULL);
    if (!gl) { g_failures++; return; }

    brain_immune_config_t cfg;
    brain_immune_default_config(&cfg);
    cfg.enable_bbb_integration   = false;
    cfg.enable_bft_integration   = false;
    cfg.enable_swarm_integration = false;
    cfg.enable_bio_async         = false;
    cfg.enable_logging           = false;
    brain_immune_system_t* immune = brain_immune_create(&cfg);
    if (!immune) {
        fprintf(stderr, "SKIP test_enabled_nonfinite — brain_immune_create failed\n");
        grounded_language_destroy(gl);
        return;
    }

    /* Bind two benign words and one NaN-laced word. The NaN word's
     * concept features land in the comprehended semantic_vector → the
     * NaN/Inf heuristic trips on the result.
     *
     * The input is also long (>= 10 words) and >80% OOV so the lexicon-
     * collision heuristic adds another delta. Combining NaN (0.30) +
     * lex-collision (0.15) + repetition (0.20) gets us comfortably past
     * the 0.5 antigen threshold. */
    float feat[64];
    make_benign_features(feat, 64, 31);
    grounded_language_fast_map(gl, "good", feat, 64, 1);
    make_benign_features(feat, 64, 32);
    grounded_language_fast_map(gl, "fine", feat, 64, 1);
    make_nan_features(feat, 64);
    grounded_language_fast_map(gl, "poison", feat, 64, 1);

    grounded_language_set_immune_system(gl, immune, true);

    float conf = -1.0f;
    /* 13-word input designed to trip three heuristics simultaneously:
     *   - "poison" (1 occurrence) propagates NaN through its bound
     *     concept features → NaN/Inf delta (0.30).
     *   - "qzpwxn" repeats 7 times (7/13 = 54% > 50%) → repetition
     *     delta (0.20). qzpwxn is OOV (no fuzzy match — its consonant
     *     skeleton has zero overlap with our 3-word lexicon).
     *   - 12 of 13 words are OOV (only "poison" matches) → 92% > 80%
     *     OOV → lex-collision delta (0.15).
     * Total = 0.65, comfortably above the 0.5 antigen threshold. */
    EXPECT(run_comprehend(gl,
            "poison qzpwxn qzpwxn qzpwxn qzpwxn qzpwxn qzpwxn qzpwxn "
            "aaaaaaa bbbbbbb ccccccc ddddddd eeeeeee",
            &conf, NULL) == 0,
           "comprehend rc");

    gl_stats_t stats;
    grounded_language_get_stats(gl, &stats);
    EXPECT(stats.immune_inspections >= 1,
           "expected inspections>=1, got %llu",
           (unsigned long long)stats.immune_inspections);
    EXPECT(stats.immune_antigens_registered >= 1,
           "expected antigens>=1 on NaN input, got %llu",
           (unsigned long long)stats.immune_antigens_registered);
    /* Inflammation >= 0.30 (NaN delta) → damp factor <= 0.85, so the
     * confidence is at most 0.85 × base. The base for 3 known words
     * over 3 input words is 1.0 (modulated downstream). We assert
     * that some damp visibly applies — confidence < unmodulated
     * baseline. NaN may also poison the semantic vector; we just
     * check that confidence is finite and bounded. */
    EXPECT(isfinite(conf), "confidence must stay finite, got %f", conf);
    EXPECT(conf < 1.0f,
           "expected damped confidence < 1.0, got %f", conf);
    printf("  [nonfinite] inspections=%llu  antigens=%llu  conf=%f\n",
           (unsigned long long)stats.immune_inspections,
           (unsigned long long)stats.immune_antigens_registered,
           conf);

    brain_immune_destroy(immune);
    grounded_language_destroy(gl);
}

static void test_strongly_inflamed_skips_engram(void) {
    grounded_language_t* gl = grounded_language_create(64, NULL);
    if (!gl) { g_failures++; return; }

    brain_immune_config_t cfg;
    brain_immune_default_config(&cfg);
    cfg.enable_bbb_integration   = false;
    cfg.enable_bft_integration   = false;
    cfg.enable_swarm_integration = false;
    cfg.enable_bio_async         = false;
    cfg.enable_logging           = false;
    brain_immune_system_t* immune = brain_immune_create(&cfg);
    if (!immune) {
        fprintf(stderr, "SKIP test_strongly_inflamed — brain_immune_create failed\n");
        grounded_language_destroy(gl);
        return;
    }

    engram_system_t* engram = engram_system_create();
    if (!engram) {
        fprintf(stderr, "SKIP test_strongly_inflamed — engram_system_create failed\n");
        brain_immune_destroy(immune);
        grounded_language_destroy(gl);
        return;
    }

    /* Bind a single NaN-laced word with > 4 chars so the negation cue
     * "not" (3 chars) won't fuzzy-match it (len_ratio < 0.6 filter). */
    float feat[64];
    make_nan_features(feat, 64);
    grounded_language_fast_map(gl, "poisonword", feat, 64, 1);

    grounded_language_set_immune_system(gl, immune, true);
    grounded_language_set_engram_system(gl, engram, true);

    /* Strongly-inflamed 13-word text designed to trip 4 heuristics:
     *   - "poisonword" (1 occurrence) propagates NaN → NaN/Inf (+0.30)
     *   - "qzpwxn" repeats 7/13 = 54% > 50% → repetition (+0.20)
     *   - 12 of 13 words are OOV (only "poisonword" matches; "not" and
     *     "qzpwxn" are too short / too dissimilar to fuzzy-match the
     *     10-char bound vocab) → 92% > 80% → lex-collision (+0.15)
     *   - "not" (leading negation cue) scans ahead and marks
     *     "poisonword" for inversion → any_negated=true; word_count
     *     (13) > 4 → negation cascade (+0.10)
     * Sum: 0.30 + 0.20 + 0.15 + 0.10 = 0.75 > 0.7 (engram-skip
     * threshold). */
    const char* poison_text =
        "not poisonword qzpwxn qzpwxn qzpwxn qzpwxn qzpwxn qzpwxn qzpwxn "
        "aaaaaaa bbbbbbb ccccccc ddddddd";

    /* Snapshot engram counter before. */
    gl_stats_t before_stats;
    grounded_language_get_stats(gl, &before_stats);
    uint64_t encodes_before = before_stats.engram_encodes;

    float conf = -1.0f;
    EXPECT(run_comprehend(gl, poison_text, &conf, NULL) == 0,
           "comprehend rc");

    gl_stats_t after_stats;
    grounded_language_get_stats(gl, &after_stats);

    /* Inspection ran. */
    EXPECT(after_stats.immune_inspections >= 1,
           "expected inspections>=1, got %llu",
           (unsigned long long)after_stats.immune_inspections);
    /* Antigen was registered (inflammation crossed 0.5). */
    EXPECT(after_stats.immune_antigens_registered >= 1,
           "expected antigens>=1, got %llu",
           (unsigned long long)after_stats.immune_antigens_registered);
    /* Engram encode was suppressed — counter unchanged. */
    EXPECT(after_stats.engram_encodes == encodes_before,
           "expected engram_encodes to NOT advance under high inflammation"
           " (before=%llu, after=%llu)",
           (unsigned long long)encodes_before,
           (unsigned long long)after_stats.engram_encodes);
    printf("  [inflamed]  inspections=%llu  antigens=%llu  "
           "engram_encodes_before=%llu  after=%llu  conf=%f\n",
           (unsigned long long)after_stats.immune_inspections,
           (unsigned long long)after_stats.immune_antigens_registered,
           (unsigned long long)encodes_before,
           (unsigned long long)after_stats.engram_encodes,
           conf);

    engram_system_destroy(engram);
    brain_immune_destroy(immune);
    grounded_language_destroy(gl);
}

int main(void) {
    printf("[test_lang_immune_inspection]\n");
    test_disabled_default();
    test_enabled_benign();
    test_enabled_nonfinite();
    test_strongly_inflamed_skips_engram();
    if (g_failures > 0) {
        printf("FAIL  failures=%d\n", g_failures);
        return 1;
    }
    printf("PASS\n");
    return 0;
}
