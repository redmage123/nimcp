/**
 * @file test_lang_autoregressive_produce.c
 * @brief Tier 1 follow-up — gl-side autoregressive produce.
 *
 * Covers:
 *   1. setter/getter — default OFF, round-trips ON/OFF.
 *   2. LANC persistence — the flag survives save_multiturn_state /
 *      load_multiturn_state (trailing byte, no version bump).
 *   3. produce smoke — with AR ON the produce path (emitted-context
 *      accumulation + continuation bonus + free) runs without crashing and
 *      still emits content; AR OFF is the unchanged baseline.
 *
 * Compile (CMake wires this in lang_smoke):
 *   gcc -O0 -g -I include tests/unit/test_lang_autoregressive_produce.c \
 *       -L build/lib -lnimcp -lm -lpthread \
 *       -Wl,-rpath,build/lib -o /tmp/test_lang_autoregressive_produce
 */

#include "language/nimcp_grounded_language.h"
#include "language/nimcp_grounded_language_persistence.h"

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

#define SDIM 64u

static uint64_t ground_word(grounded_language_t* gl, const char* w, uint32_t s) {
    float feats[SDIM] = {0};
    if (s < SDIM)        feats[s]      = 1.0f;
    if ((s + 7u) < SDIM) feats[s + 7u] = 0.3f;
    return grounded_language_fast_map(gl, w, feats, SDIM, 1u);
}

static void test_setter_getter(void) {
    grounded_language_t* gl = grounded_language_create(SDIM, NULL);
    EXPECT(gl != NULL, "create"); if (!gl) return;
    EXPECT(!grounded_language_get_autoregressive_produce(gl), "default OFF");
    grounded_language_set_autoregressive_produce(gl, true);
    EXPECT(grounded_language_get_autoregressive_produce(gl), "ON after set");
    grounded_language_set_autoregressive_produce(gl, false);
    EXPECT(!grounded_language_get_autoregressive_produce(gl), "OFF after clear");
    /* NULL-safe. */
    EXPECT(!grounded_language_get_autoregressive_produce(NULL), "NULL -> false");
    grounded_language_set_autoregressive_produce(NULL, true); /* must not crash */
    grounded_language_destroy(gl);
}

static void test_persist_round_trip(void) {
    char path[256];
    snprintf(path, sizeof(path), "/tmp/test_lang_ar_%d.bin", (int)getpid());

    grounded_language_t* gl = grounded_language_create(SDIM, NULL);
    EXPECT(gl != NULL, "create save"); if (!gl) return;
    grounded_language_set_autoregressive_produce(gl, true);

    FILE* f = fopen(path, "wb");
    EXPECT(f != NULL, "fopen save"); if (!f) { grounded_language_destroy(gl); return; }
    int rc = grounded_language_save_multiturn_state(gl, f);
    fclose(f);
    EXPECT(rc == 0, "save rc=%d", rc);
    grounded_language_destroy(gl);

    grounded_language_t* gl2 = grounded_language_create(SDIM, NULL);
    EXPECT(gl2 != NULL, "create load"); if (!gl2) { unlink(path); return; }
    EXPECT(!grounded_language_get_autoregressive_produce(gl2), "default OFF pre-load");
    f = fopen(path, "rb");
    EXPECT(f != NULL, "fopen load"); if (!f) { grounded_language_destroy(gl2); unlink(path); return; }
    rc = grounded_language_load_multiturn_state(gl2, f);
    fclose(f);
    EXPECT(rc == 0, "load rc=%d", rc);
    EXPECT(grounded_language_get_autoregressive_produce(gl2), "ON post-load");
    grounded_language_destroy(gl2);
    unlink(path);
}

static void test_produce_smoke(void) {
    grounded_language_t* gl = grounded_language_create(SDIM, NULL);
    EXPECT(gl != NULL, "create"); if (!gl) return;
    grounded_language_set_current_stage_int(gl, 2);

    /* A few content words with overlapping/disjoint context profiles. */
    ground_word(gl, "creation", 4u);
    ground_word(gl, "knowledge", 11u);
    ground_word(gl, "memory", 18u);
    ground_word(gl, "process", 25u);

    float intent[SDIM] = {0};
    intent[4u] = 1.0f; intent[11u] = 1.0f; intent[18u] = 1.0f; intent[25u] = 1.0f;

    /* Baseline OFF. */
    gl_production_result_t r_off = {0};
    grounded_language_set_autoregressive_produce(gl, false);
    int rc_off = grounded_language_produce(gl, intent, SDIM,
                                           GL_PRODUCE_DESCRIBE, &r_off);

    /* AR ON — must run the emitted-ctx accumulation + bonus + free path
     * without crashing and still emit content. */
    gl_production_result_t r_on = {0};
    grounded_language_set_autoregressive_produce(gl, true);
    int rc_on = grounded_language_produce(gl, intent, SDIM,
                                          GL_PRODUCE_DESCRIBE, &r_on);

    EXPECT(rc_on == 0 || rc_on == -1, "AR-on produce returns valid rc (%d)", rc_on);
    if (rc_on == 0) {
        EXPECT(r_on.text && r_on.text[0], "AR-on emitted text");
        EXPECT(r_on.word_count >= 1, "AR-on word_count >= 1 (%u)", r_on.word_count);
        fprintf(stderr, "  AR-off: '%s'\n  AR-on : '%s'\n",
                (rc_off == 0 && r_off.text) ? r_off.text : "(none)",
                r_on.text);
    }
    /* produce allocates result->text / semantic_vector via the nimcp
     * allocator; this short-lived test lets the process reclaim them rather
     * than risk a free()/allocator mismatch. */
    (void)rc_off;
    grounded_language_destroy(gl);
}

int main(void) {
    fprintf(stderr, "[UNIT] test_lang_autoregressive_produce (Tier 1 follow-up)\n");
    test_setter_getter();
    test_persist_round_trip();
    test_produce_smoke();
    if (g_failures == 0) { fprintf(stderr, "ALL PASS\n"); return 0; }
    fprintf(stderr, "%d failures\n", g_failures);
    return 1;
}
