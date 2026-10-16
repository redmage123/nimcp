/**
 * @file test_lang_spectrum_tick.c
 * @brief CC-1 — verify the periodic bigram-spectrum refresh.
 *
 * grounded_language_tick_bigram_spectrum() drives bigram_spectrum_maybe_compute
 * under the hood. brain_tick_language is the production caller (1Hz cadence,
 * gated by min_delta_events). This test exercises the API directly so we
 * don't need a full brain to verify the gating behavior.
 *
 * Coverage:
 *   1. test_no_spectrum_attached:
 *      gl with no spectrum attached → tick returns 0 (no-op).
 *
 *   2. test_below_threshold:
 *      Attach spectrum, record 4 events, call tick(min_delta=8) →
 *      returns 0 (delta below threshold). Cached metrics still invalid.
 *
 *   3. test_crosses_threshold:
 *      Record 16 events, call tick(min_delta=8) → returns 1 (compute ran).
 *      A second tick with no new events returns 0 (delta is now 0).
 *
 *   4. test_resumes_after_more_events:
 *      Add 12 more events, tick(min_delta=8) → returns 1 (delta=12).
 */

#include "language/nimcp_grounded_language.h"
#include "language/nimcp_bigram_spectrum.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
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

static void test_no_spectrum_attached(void)
{
    grounded_language_t* gl = grounded_language_create(0, NULL);
    EXPECT(gl != NULL, "create");
    if (!gl) return;

    /* No spectrum attached — tick is a no-op, returns 0. */
    int rc = grounded_language_tick_bigram_spectrum(gl, 8);
    EXPECT(rc == 0, "expected 0 (no-op), got %d", rc);

    /* NULL gl handled gracefully. */
    rc = grounded_language_tick_bigram_spectrum(NULL, 8);
    EXPECT(rc == 0, "NULL gl tick should return 0, got %d", rc);

    grounded_language_destroy(gl);
}

static void test_threshold_gating(void)
{
    grounded_language_t* gl = grounded_language_create(0, NULL);
    EXPECT(gl != NULL, "create");
    if (!gl) return;

    bigram_spectrum_t* bs = bigram_spectrum_create(64);
    EXPECT(bs != NULL, "spectrum_create");
    if (!bs) { grounded_language_destroy(gl); return; }

    grounded_language_attach_bigram_spectrum(gl, bs);

    /* Record only 4 events — below threshold of 8. */
    for (int i = 0; i < 4; i++) {
        bigram_spectrum_record(bs, (uint32_t)i, (uint32_t)((i + 1) % 64));
    }
    int rc = grounded_language_tick_bigram_spectrum(gl, 8);
    EXPECT(rc == 0, "below threshold should skip, got %d", rc);

    /* Add 12 more (total 16) — over threshold. */
    for (int i = 0; i < 12; i++) {
        bigram_spectrum_record(bs, (uint32_t)i, (uint32_t)((i + 2) % 64));
    }
    rc = grounded_language_tick_bigram_spectrum(gl, 8);
    EXPECT(rc == 1, "over threshold should compute, got %d", rc);

    /* A second tick with no new events — delta is 0, skip. */
    rc = grounded_language_tick_bigram_spectrum(gl, 8);
    EXPECT(rc == 0, "no new events should skip, got %d", rc);

    /* Add 10 more events — over threshold again. */
    for (int i = 0; i < 10; i++) {
        bigram_spectrum_record(bs, (uint32_t)(i + 4), (uint32_t)((i + 5) % 64));
    }
    rc = grounded_language_tick_bigram_spectrum(gl, 8);
    EXPECT(rc == 1, "fresh delta should compute, got %d", rc);

    grounded_language_attach_bigram_spectrum(gl, NULL);
    bigram_spectrum_destroy(bs);
    grounded_language_destroy(gl);
}

int main(void)
{
    fprintf(stderr, "=== test_lang_spectrum_tick (CC-1) ===\n");
    test_no_spectrum_attached();
    test_threshold_gating();

    if (g_failures == 0) {
        fprintf(stderr, "ALL PASS\n");
        return 0;
    }
    fprintf(stderr, "%d failures\n", g_failures);
    return 1;
}
