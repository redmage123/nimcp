/**
 * @file test_lang_produce_frequency_penalty.c
 * @brief Produce-score frequency penalty — IDF-style filler damping knob.
 *
 * Covers grounded_language_{set,get}_produce_frequency_penalty:
 *   1. default 0.0 (OFF — produce scoring unchanged).
 *   2. setter/getter round-trips for non-negative values (no upper clamp:
 *      the penalty is an open-ended coefficient, not a [0,1] weight).
 *   3. negative + NaN are rejected to 0.0 (the !isfinite/<0 guard).
 *   4. NULL-safe: getter returns the default, setter is a no-op.
 *
 * This knob is runtime-only by design (applied from lang_runtime_default.json
 * at daemon start), so there is no LANC persistence round-trip to cover —
 * contrast test_lang_produce_distributional_weight.c which does persist.
 *
 * The scoring effect itself (raw *= 1/(1 + p*log1p(frequency))) is exercised
 * against the real trained lexicon on the pod, where word frequencies exist;
 * score_word_against_vector is static so it is not unit-callable here.
 *
 * Compile (CMake wires this into lang_smoke):
 *   gcc -O0 -g -I include tests/unit/test_lang_produce_frequency_penalty.c \
 *       -L build/lib -lnimcp -lm -lpthread \
 *       -Wl,-rpath,build/lib -o /tmp/test_lang_pfp
 */

#include "language/nimcp_grounded_language.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
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
#define DEF 0.0f   /* GL_PRODUCE_FREQUENCY_PENALTY_DEFAULT */

static void test_setter_getter(void) {
    grounded_language_t* gl = grounded_language_create(SDIM, NULL);
    EXPECT(gl != NULL, "create"); if (!gl) return;

    EXPECT(NEAR(grounded_language_get_produce_frequency_penalty(gl), DEF),
           "default = 0.0 (OFF), got %.4f",
           grounded_language_get_produce_frequency_penalty(gl));

    grounded_language_set_produce_frequency_penalty(gl, 0.2f);
    EXPECT(NEAR(grounded_language_get_produce_frequency_penalty(gl), 0.2f),
           "0.2 round-trips, got %.4f",
           grounded_language_get_produce_frequency_penalty(gl));

    /* No upper clamp — open-ended coefficient. */
    grounded_language_set_produce_frequency_penalty(gl, 3.5f);
    EXPECT(NEAR(grounded_language_get_produce_frequency_penalty(gl), 3.5f),
           "3.5 passes (no upper clamp), got %.4f",
           grounded_language_get_produce_frequency_penalty(gl));

    /* Negative -> 0.0 */
    grounded_language_set_produce_frequency_penalty(gl, -0.3f);
    EXPECT(NEAR(grounded_language_get_produce_frequency_penalty(gl), 0.0f),
           "negative -> 0.0, got %.4f",
           grounded_language_get_produce_frequency_penalty(gl));

    /* NaN -> 0.0 (rejected by the !isfinite guard) */
    grounded_language_set_produce_frequency_penalty(gl, NAN);
    EXPECT(NEAR(grounded_language_get_produce_frequency_penalty(gl), 0.0f),
           "NaN -> 0.0, got %.4f",
           grounded_language_get_produce_frequency_penalty(gl));

    /* Boundary: 0.0 passes through (back to OFF). */
    grounded_language_set_produce_frequency_penalty(gl, 0.0f);
    EXPECT(NEAR(grounded_language_get_produce_frequency_penalty(gl), 0.0f), "0.0 ok");

    /* NULL-safe: getter returns the default, setter is a no-op. */
    EXPECT(NEAR(grounded_language_get_produce_frequency_penalty(NULL), DEF),
           "NULL getter -> default");
    grounded_language_set_produce_frequency_penalty(NULL, 0.5f); /* must not crash */

    grounded_language_destroy(gl);
}

int main(void) {
    test_setter_getter();
    if (g_failures == 0) {
        printf("test_lang_produce_frequency_penalty: ALL PASS\n");
        return 0;
    }
    fprintf(stderr, "test_lang_produce_frequency_penalty: %d FAILURE(S)\n", g_failures);
    return 1;
}
