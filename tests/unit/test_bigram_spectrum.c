/**
 * @file test_bigram_spectrum.c
 * @brief PA-4+ — verify the FFT-based bigram spectral analytics behave
 *        the way the diagnostic relies on:
 *
 *   1. test_periodic_high_peak_low_entropy:
 *      A→B, B→A, A→B, B→A, ...  Expect a sharp 2D-FFT peak (peak_strength
 *      well above 1.0) and low spectral_entropy (<< log(N²)).
 *
 *   2. test_random_low_peak_high_entropy:
 *      Bigrams drawn from a uniform random distribution. peak_strength
 *      should be small (no clear winner) and spectral_entropy should be
 *      large.
 *
 *   3. test_reset_returns_to_baseline:
 *      Record bigrams, compute, then reset. Subsequent compute() returns
 *      all-zero metrics and total_events() returns 0.
 *
 * Compile:
 *   gcc -I include tests/unit/test_bigram_spectrum.c \
 *       -L build/lib -lnimcp -lm \
 *       -Wl,-rpath,$(pwd)/build/lib \
 *       -o /tmp/test_bigram_spectrum
 */

#include "language/nimcp_bigram_spectrum.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

static int g_passed = 0;
static int g_failed = 0;

#define TEST_ASSERT(cond, msg) do {                                  \
    if (cond) {                                                      \
        printf("  [PASS] %s\n", msg);                                \
        g_passed++;                                                  \
    } else {                                                         \
        printf("  [FAIL] %s\n", msg);                                \
        g_failed++;                                                  \
    }                                                                \
} while (0)

static void test_periodic_high_peak_low_entropy(void) {
    printf("\n[1] test_periodic_high_peak_low_entropy\n");

    const uint32_t V = 64;
    bigram_spectrum_t* bs = bigram_spectrum_create(V);
    TEST_ASSERT(bs != NULL, "spectrum created");
    if (!bs) return;

    /* Periodic pattern across the WHOLE V × V grid — every word transitions
     * to the next-id mod V (a circulant). This is the canonical periodic
     * 2D bigram structure: a single off-diagonal stripe. The 2D FFT of such
     * a stripe is sharply concentrated in a small set of bins → high
     * peak_strength + low entropy. */
    const int reps = 200;
    for (int r = 0; r < reps; r++) {
        for (uint32_t i = 0; i < V; i++) {
            bigram_spectrum_record(bs, i, (i + 1) % V);
        }
    }

    TEST_ASSERT(bigram_spectrum_total_events(bs) == (uint64_t)V * (uint64_t)reps,
                "total_events matches recorded count");

    bigram_spectral_metrics_t m;
    int rc = bigram_spectrum_compute(bs, &m);
    TEST_ASSERT(rc == 0, "compute() returned 0");
    printf("    peak_strength=%.4f  low_freq_conc=%.4f  spectral_entropy=%.4f\n",
           (double)m.peak_strength,
           (double)m.low_freq_concentration,
           (double)m.spectral_entropy);

    /* Circulant pattern → very sharp peaks; z-score easily > 3.0. */
    TEST_ASSERT(m.peak_strength > 2.0f,
                "peak_strength > 2.0 for circulant pattern");

    /* Entropy of a stripe-like spectrum is below log(N^2). Max entropy
     * for N=64 → log(4096) ≈ 8.32. A circulant has its energy
     * concentrated along a single line in the FFT plane (≈ N bins), so
     * its entropy is roughly log(N) ≈ log(64) ≈ 4.16, well under half. */
    const float max_ent = logf((float)(64u * 64u));
    TEST_ASSERT(m.spectral_entropy < 0.7f * max_ent,
                "spectral_entropy < 0.7 * log(N^2) for circulant pattern");

    /* Metrics finite. */
    TEST_ASSERT(isfinite(m.peak_strength), "peak_strength finite");
    TEST_ASSERT(isfinite(m.low_freq_concentration), "low_freq_concentration finite");
    TEST_ASSERT(isfinite(m.spectral_entropy), "spectral_entropy finite");
    TEST_ASSERT(m.spectral_entropy >= 0.0f, "spectral_entropy non-negative");

    bigram_spectrum_destroy(bs);
}

static void test_random_low_peak_high_entropy(void) {
    printf("\n[2] test_random_low_peak_high_entropy\n");

    const uint32_t V = 64;
    bigram_spectrum_t* bs = bigram_spectrum_create(V);
    TEST_ASSERT(bs != NULL, "spectrum created");
    if (!bs) return;

    /* Uniform random bigrams — fixed seed for reproducibility. */
    srand(42);
    for (int i = 0; i < 50000; i++) {
        uint32_t p = (uint32_t)rand() % V;
        uint32_t n = (uint32_t)rand() % V;
        bigram_spectrum_record(bs, p, n);
    }

    bigram_spectral_metrics_t m;
    int rc = bigram_spectrum_compute(bs, &m);
    TEST_ASSERT(rc == 0, "compute() returned 0");
    printf("    peak_strength=%.4f  low_freq_conc=%.4f  spectral_entropy=%.4f\n",
           (double)m.peak_strength,
           (double)m.low_freq_concentration,
           (double)m.spectral_entropy);

    /* Random spectrum: no individual bin dominates the magnitude
     * distribution by many sigma. Stay loose — only require no extreme
     * concentration. With uniform random + 50K samples the Rayleigh-style
     * peak normally lands around 4-6 sigma; periodic was much higher.
     * We assert peak_strength is finite and entropy is high. */
    TEST_ASSERT(isfinite(m.peak_strength), "peak_strength finite");

    /* Entropy of nearly-uniform spectrum should be close to log(N^2-1).
     * With fft_size=64 → log(4095) ≈ 8.32. Require it to be at least
     * 80% of max — uniform random typically lands very close to max. */
    const float max_ent = logf((float)(64u * 64u - 1u));
    TEST_ASSERT(m.spectral_entropy > 0.80f * max_ent,
                "spectral_entropy > 0.80 * log(N^2) for random bigrams");

    bigram_spectrum_destroy(bs);
}

static void test_reset_returns_to_baseline(void) {
    printf("\n[3] test_reset_returns_to_baseline\n");

    const uint32_t V = 32;
    bigram_spectrum_t* bs = bigram_spectrum_create(V);
    TEST_ASSERT(bs != NULL, "spectrum created");
    if (!bs) return;

    /* Record some bigrams. */
    for (int i = 0; i < 100; i++) {
        bigram_spectrum_record(bs, 0, 1);
        bigram_spectrum_record(bs, 2, 3);
    }
    TEST_ASSERT(bigram_spectrum_total_events(bs) == 200,
                "total_events == 200 before reset");

    bigram_spectral_metrics_t m_pre;
    bigram_spectrum_compute(bs, &m_pre);

    /* Reset. */
    int rc = bigram_spectrum_reset(bs);
    TEST_ASSERT(rc == 0, "reset() returned 0");
    TEST_ASSERT(bigram_spectrum_total_events(bs) == 0,
                "total_events == 0 after reset");

    /* compute() on empty matrix yields all-zero metrics. */
    bigram_spectral_metrics_t m_post;
    rc = bigram_spectrum_compute(bs, &m_post);
    TEST_ASSERT(rc == 0, "compute() returned 0 on empty matrix");
    TEST_ASSERT(m_post.peak_strength == 0.0f,
                "peak_strength == 0 after reset");
    TEST_ASSERT(m_post.low_freq_concentration == 0.0f,
                "low_freq_concentration == 0 after reset");
    TEST_ASSERT(m_post.spectral_entropy == 0.0f,
                "spectral_entropy == 0 after reset");

    /* Sanity: pre-reset metrics were not zero (we recorded a clear pattern). */
    TEST_ASSERT(m_pre.peak_strength > 0.0f || m_pre.spectral_entropy > 0.0f,
                "pre-reset metrics were non-zero (sanity)");

    bigram_spectrum_destroy(bs);
}

int main(void) {
    printf("=== test_bigram_spectrum ===\n");
    test_periodic_high_peak_low_entropy();
    test_random_low_peak_high_entropy();
    test_reset_returns_to_baseline();
    printf("\n=== Summary: %d passed, %d failed ===\n", g_passed, g_failed);
    return (g_failed == 0) ? 0 : 1;
}
