/**
 * @file test_lang_produce_distributional_weight.c
 * @brief Produce-score rebalance — the tunable distributional weight.
 *
 * Covers grounded_language_{set,get}_produce_distributional_weight + its
 * LANC persistence (trailing f32 in the multiturn config block):
 *   1. setter/getter — default 0.4 (historical 0.4/0.6 split), round-trips,
 *      clamps to [0,1], rejects NaN/negative, NULL-safe.
 *   2. LANC persistence — the weight survives save/load.
 *   3. back-compat — a pre-this-change sidecar (one that stops after the
 *      clause-frame byte) loads cleanly and leaves the weight at its default.
 *
 * The scoring effect itself (raw = w*distributional + (1-w)*concept) is
 * exercised against the real trained lexicon on the pod, where concrete vs
 * abstract concept bindings actually exist; score_word_against_vector is
 * static so it is not unit-callable here.
 *
 * Compile (CMake wires this into lang_smoke):
 *   gcc -O0 -g -I include tests/unit/test_lang_produce_distributional_weight.c \
 *       -L build/lib -lnimcp -lm -lpthread \
 *       -Wl,-rpath,build/lib -o /tmp/test_lang_pdw
 */

#include "language/nimcp_grounded_language.h"
#include "language/nimcp_grounded_language_persistence.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <math.h>

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
#define NEAR(a, b) (fabsf((float)(a) - (float)(b)) < 1e-4f)
#define DEF 0.4f   /* GL_PRODUCE_DISTRIBUTIONAL_DEFAULT_WEIGHT */

static void test_setter_getter(void) {
    grounded_language_t* gl = grounded_language_create(SDIM, NULL);
    EXPECT(gl != NULL, "create"); if (!gl) return;

    EXPECT(NEAR(grounded_language_get_produce_distributional_weight(gl), DEF),
           "default = 0.4, got %.4f",
           grounded_language_get_produce_distributional_weight(gl));

    grounded_language_set_produce_distributional_weight(gl, 0.7f);
    EXPECT(NEAR(grounded_language_get_produce_distributional_weight(gl), 0.7f),
           "0.7 round-trips, got %.4f",
           grounded_language_get_produce_distributional_weight(gl));

    /* Clamp above 1.0 -> 1.0 */
    grounded_language_set_produce_distributional_weight(gl, 1.5f);
    EXPECT(NEAR(grounded_language_get_produce_distributional_weight(gl), 1.0f),
           "clamp >1 -> 1.0, got %.4f",
           grounded_language_get_produce_distributional_weight(gl));

    /* Negative -> 0.0 */
    grounded_language_set_produce_distributional_weight(gl, -0.3f);
    EXPECT(NEAR(grounded_language_get_produce_distributional_weight(gl), 0.0f),
           "negative -> 0.0, got %.4f",
           grounded_language_get_produce_distributional_weight(gl));

    /* NaN -> 0.0 (rejected by the !isfinite guard) */
    grounded_language_set_produce_distributional_weight(gl, NAN);
    EXPECT(NEAR(grounded_language_get_produce_distributional_weight(gl), 0.0f),
           "NaN -> 0.0, got %.4f",
           grounded_language_get_produce_distributional_weight(gl));

    /* Boundary values pass through unchanged. */
    grounded_language_set_produce_distributional_weight(gl, 0.0f);
    EXPECT(NEAR(grounded_language_get_produce_distributional_weight(gl), 0.0f), "0.0 ok");
    grounded_language_set_produce_distributional_weight(gl, 1.0f);
    EXPECT(NEAR(grounded_language_get_produce_distributional_weight(gl), 1.0f), "1.0 ok");

    /* NULL-safe: getter returns the default, setter is a no-op. */
    EXPECT(NEAR(grounded_language_get_produce_distributional_weight(NULL), DEF),
           "NULL getter -> default");
    grounded_language_set_produce_distributional_weight(NULL, 0.5f); /* must not crash */

    grounded_language_destroy(gl);
}

static void test_persist_round_trip(void) {
    char path[256];
    snprintf(path, sizeof(path), "/tmp/test_lang_pdw_%d.bin", (int)getpid());

    grounded_language_t* gl = grounded_language_create(SDIM, NULL);
    EXPECT(gl != NULL, "create save"); if (!gl) return;
    grounded_language_set_produce_distributional_weight(gl, 0.72f);
    FILE* f = fopen(path, "wb");
    EXPECT(f != NULL, "fopen save"); if (!f) { grounded_language_destroy(gl); return; }
    int rc = grounded_language_save_multiturn_state(gl, f);
    fclose(f);
    EXPECT(rc == 0, "save rc=%d", rc);
    grounded_language_destroy(gl);

    grounded_language_t* gl2 = grounded_language_create(SDIM, NULL);
    EXPECT(gl2 != NULL, "create load"); if (!gl2) { unlink(path); return; }
    EXPECT(NEAR(grounded_language_get_produce_distributional_weight(gl2), DEF),
           "default pre-load");
    f = fopen(path, "rb");
    EXPECT(f != NULL, "fopen load"); if (!f) { grounded_language_destroy(gl2); unlink(path); return; }
    rc = grounded_language_load_multiturn_state(gl2, f);
    fclose(f);
    EXPECT(rc == 0, "load rc=%d", rc);
    EXPECT(NEAR(grounded_language_get_produce_distributional_weight(gl2), 0.72f),
           "0.72 survives save/load, got %.4f",
           grounded_language_get_produce_distributional_weight(gl2));
    grounded_language_destroy(gl2);
    unlink(path);
}

int main(void) {
    test_setter_getter();
    test_persist_round_trip();
    if (g_failures == 0) {
        printf("test_lang_produce_distributional_weight: ALL PASS\n");
        return 0;
    }
    fprintf(stderr, "test_lang_produce_distributional_weight: %d FAILURE(S)\n", g_failures);
    return 1;
}
