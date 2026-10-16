/**
 * @file test_lang_engram_integration.c
 * @brief EN-6 — verify read-only engram integration on grounded_language.
 *
 * WHAT: Creates a grounded_language + engram_system pair, attaches via
 *       grounded_language_set_engram_system, calls comprehend twice on
 *       related text, then asserts:
 *         (a) engram_encodes counter advanced by 2 (one per comprehend),
 *         (b) engram_recalls counter advanced by ≥1 on the second call
 *             (the first call's trace was recallable by the second).
 *
 * WHY: Confirms the read-only encode + recall hooks fire when the
 *      attach API enables them, and stay no-op when disabled.
 *
 * Build:
 *   gcc -O2 -I include tests/unit/test_lang_engram_integration.c \
 *       -L build/lib -lnimcp -lm -lpthread \
 *       -Wl,-rpath,/home/bbrelin/nimcp/build/lib \
 *       -o /tmp/test_lang_engram_integration
 */

#include "language/nimcp_grounded_language.h"
#include "cognitive/memory/nimcp_engram.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

static int test_disabled_is_noop(void) {
    grounded_language_t* gl = grounded_language_create(64, NULL);
    if (!gl) return 1;
    engram_system_t* es = engram_system_create();
    if (!es) { grounded_language_destroy(gl); return 1; }

    /* Default-OFF — engram_enabled flag should keep encode/recall idle. */
    grounded_language_set_engram_system(gl, es, false);

    /* Ground a couple of words so comprehend has something to activate. */
    /* Register words via fast_map (same pattern test_lang_anaphora uses). */
    float feat[256];
    for (int i = 0; i < 256; i++) feat[i] = ((i * 17) & 0xff) / 255.0f - 0.5f;
    grounded_language_fast_map(gl, "hello", feat, 256, 1);
    grounded_language_fast_map(gl, "world", feat, 256, 1);

    gl_comprehension_result_t r;
    grounded_language_comprehend(gl, "hello world", &r);
    gl_comprehension_result_cleanup(&r);

    gl_stats_t stats;
    grounded_language_get_stats(gl, &stats);
    int fail = 0;
    if (stats.engram_encodes != 0) {
        fprintf(stderr, "FAIL: encodes=%lu when disabled (expected 0)\n",
                (unsigned long)stats.engram_encodes);
        fail = 1;
    }
    if (stats.engram_recalls != 0) {
        fprintf(stderr, "FAIL: recalls=%lu when disabled (expected 0)\n",
                (unsigned long)stats.engram_recalls);
        fail = 1;
    }

    engram_system_destroy(es);
    grounded_language_destroy(gl);
    return fail;
}

static int test_enabled_encodes_and_recalls(void) {
    grounded_language_t* gl = grounded_language_create(64, NULL);
    if (!gl) return 1;
    engram_system_t* es = engram_system_create();
    if (!es) { grounded_language_destroy(gl); return 1; }

    /* Lower the engram completion threshold so a small overlap counts.
     * Default 0.4 is too strict for the tiny synthetic activation set
     * we build here. */
    es->completion_threshold = 0.05f;

    grounded_language_set_engram_system(gl, es, true);

    float feat[256];
    for (int i = 0; i < 256; i++) feat[i] = ((i * 13 + 7) & 0xff) / 255.0f - 0.5f;
    grounded_language_fast_map(gl, "alpha", feat, 256, 1);
    grounded_language_fast_map(gl, "beta",  feat, 256, 1);
    grounded_language_fast_map(gl, "gamma", feat, 256, 1);

    /* Two comprehensions on overlapping text — first lays the trace,
     * second should recall it. Between the two we manually bump the
     * just-encoded engrams to consolidation_strength=1.0 because
     * recall scores match * consolidation_strength and freshly-encoded
     * engrams start at 0 (LABILE state). In production, the brain's
     * 50ms tick consolidates them automatically; in this unit test
     * we shortcut to keep it fast and deterministic. */
    gl_comprehension_result_t r1, r2;
    grounded_language_comprehend(gl, "alpha beta gamma", &r1);
    gl_comprehension_result_cleanup(&r1);

    for (uint32_t i = 0; i < es->capacity; i++) {
        if (es->engrams[i].active) {
            es->engrams[i].consolidation_strength = 1.0f;
        }
    }

    grounded_language_comprehend(gl, "alpha beta gamma", &r2);
    gl_comprehension_result_cleanup(&r2);

    gl_stats_t stats;
    grounded_language_get_stats(gl, &stats);

    int fail = 0;
    if (stats.engram_encodes < 2) {
        fprintf(stderr, "FAIL: expected encodes>=2, got %lu\n",
                (unsigned long)stats.engram_encodes);
        fail = 1;
    }
    if (stats.engram_recalls < 1) {
        fprintf(stderr,
                "FAIL: expected recalls>=1 on second comprehend, got %lu "
                "(active engrams=%u)\n",
                (unsigned long)stats.engram_recalls,
                engram_get_active_count(es));
        fail = 1;
    }
    if (fail == 0) {
        printf("  encodes=%lu  recalls=%lu  active=%u\n",
               (unsigned long)stats.engram_encodes,
               (unsigned long)stats.engram_recalls,
               engram_get_active_count(es));
    }

    engram_system_destroy(es);
    grounded_language_destroy(gl);
    return fail;
}

int main(void) {
    printf("[test_lang_engram_integration]\n");
    int fail = 0;
    fail += test_disabled_is_noop();
    fail += test_enabled_encodes_and_recalls();
    if (fail) { printf("FAIL\n"); return 1; }
    printf("PASS\n");
    return 0;
}
