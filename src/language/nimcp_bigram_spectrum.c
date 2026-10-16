/**
 * @file nimcp_bigram_spectrum.c
 * @brief FFT-based bigram spectral analytics implementation.
 *
 * 2D-FFT trick: row-then-column 1D FFT. We use the existing real FFT
 * (fft_execute_real) on each row, then on each column of the resulting
 * complex matrix (treating as complex-to-complex via fft_execute_complex).
 *
 * Memory: counts[vocab_cap × vocab_cap] uint32. Working buffers are
 * allocated lazily on compute() and freed before returning so a long-lived
 * spectrum tracker keeps a small steady-state footprint.
 *
 * @author NIMCP Development Team
 * @date 2026-05-07
 */

#include "language/nimcp_bigram_spectrum.h"
#include "utils/spectral/nimcp_fft.h"
#include "utils/memory/nimcp_memory.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

/*---------------------------------------------------------------------------
 * Internal layout
 *-------------------------------------------------------------------------*/

#define BIGRAM_VOCAB_CAP_MIN  2u
#define BIGRAM_VOCAB_CAP_MAX  4096u

struct bigram_spectrum_struct {
    uint32_t   vocab_cap;          /* matrix side */
    uint32_t   fft_size;           /* next power-of-2 ≥ vocab_cap */
    uint32_t*  counts;             /* [vocab_cap * vocab_cap] */
    uint64_t   total_events;
    /* CC-1 cycle-coordinator periodic-compute support. The brain's
     * BRAIN_CYCLE_LANGUAGE tick (16ms) calls
     * bigram_spectrum_maybe_compute() at a sub-rate (1Hz) so trend
     * metrics stay fresh without operator-demanded compute() calls.
     * last_compute_event_count gates against repeating compute on
     * unchanged data. cached_metrics is the most recent result; valid
     * only when cached_metrics_valid is true. */
    uint64_t                  last_compute_event_count;
    bigram_spectral_metrics_t cached_metrics;
    bool                      cached_metrics_valid;
};

/*---------------------------------------------------------------------------
 * Helpers
 *-------------------------------------------------------------------------*/

static uint32_t next_pow2(uint32_t n) {
    if (n < 2u) return 2u;
    /* Use the FFT util's helper; safe even if n already power of 2. */
    return fft_next_power_of_2(n);
}

/*---------------------------------------------------------------------------
 * Lifecycle
 *-------------------------------------------------------------------------*/

bigram_spectrum_t* bigram_spectrum_create(uint32_t vocab_cap) {
    if (vocab_cap < BIGRAM_VOCAB_CAP_MIN || vocab_cap > BIGRAM_VOCAB_CAP_MAX) {
        return NULL;
    }

    bigram_spectrum_t* bs =
        (bigram_spectrum_t*)nimcp_calloc(1, sizeof(*bs));
    if (!bs) return NULL;

    bs->vocab_cap    = vocab_cap;
    bs->fft_size     = next_pow2(vocab_cap);
    bs->total_events = 0;

    size_t cells = (size_t)vocab_cap * (size_t)vocab_cap;
    bs->counts = (uint32_t*)nimcp_calloc(cells, sizeof(uint32_t));
    if (!bs->counts) {
        nimcp_free(bs);
        return NULL;
    }
    return bs;
}

void bigram_spectrum_destroy(bigram_spectrum_t* spectrum) {
    if (!spectrum) return;
    if (spectrum->counts) nimcp_free(spectrum->counts);
    nimcp_free(spectrum);
}

/*---------------------------------------------------------------------------
 * Record
 *-------------------------------------------------------------------------*/

int bigram_spectrum_record(bigram_spectrum_t* spectrum,
                           uint32_t prev_id,
                           uint32_t next_id) {
    if (!spectrum) return 0;
    if (prev_id >= spectrum->vocab_cap) return 0;
    if (next_id >= spectrum->vocab_cap) return 0;

    size_t idx = (size_t)prev_id * (size_t)spectrum->vocab_cap +
                 (size_t)next_id;
    /* Saturate at UINT32_MAX. */
    if (spectrum->counts[idx] != UINT32_MAX) {
        spectrum->counts[idx]++;
    }
    if (spectrum->total_events != UINT64_MAX) {
        spectrum->total_events++;
    }
    return 1;
}

/*---------------------------------------------------------------------------
 * Reset
 *-------------------------------------------------------------------------*/

int bigram_spectrum_reset(bigram_spectrum_t* spectrum) {
    if (!spectrum || !spectrum->counts) return -1;
    size_t cells = (size_t)spectrum->vocab_cap * (size_t)spectrum->vocab_cap;
    memset(spectrum->counts, 0, cells * sizeof(uint32_t));
    spectrum->total_events = 0;
    spectrum->last_compute_event_count = 0;
    spectrum->cached_metrics_valid = false;
    memset(&spectrum->cached_metrics, 0, sizeof(spectrum->cached_metrics));
    return 0;
}

/* CC-1: thresholded periodic compute. Runs compute() only if at least
 * `min_delta_events` new bigrams have been recorded since the last
 * successful compute. Caches the result on the spectrum so external
 * readers (probes / dashboards) can grab fresh metrics in O(1). Returns:
 *   1  — compute ran, *out_ran_compute set true (if non-NULL).
 *   0  — skipped (delta below threshold, or zero events). out_ran_compute false.
 *  -1  — error (NULL spectrum, compute() failure). */
int bigram_spectrum_maybe_compute(bigram_spectrum_t* spectrum,
                                    uint64_t min_delta_events,
                                    bool* out_ran_compute) {
    if (out_ran_compute) *out_ran_compute = false;
    if (!spectrum) return -1;
    if (spectrum->total_events == 0) return 0;
    uint64_t delta = spectrum->total_events - spectrum->last_compute_event_count;
    if (delta < min_delta_events) return 0;

    bigram_spectral_metrics_t m;
    memset(&m, 0, sizeof(m));
    int rc = bigram_spectrum_compute(spectrum, &m);
    if (rc != 0) return -1;

    spectrum->cached_metrics = m;
    spectrum->cached_metrics_valid = true;
    spectrum->last_compute_event_count = spectrum->total_events;
    if (out_ran_compute) *out_ran_compute = true;
    return 1;
}

/* CC-1: read the most recently cached metrics. Returns 0 on success, -1
 * if no compute has run yet (caller can fall back to the synchronous
 * bigram_spectrum_compute API to force one). */
int bigram_spectrum_get_cached_metrics(const bigram_spectrum_t* spectrum,
                                         bigram_spectral_metrics_t* out) {
    if (!spectrum || !out) return -1;
    if (!spectrum->cached_metrics_valid) return -1;
    *out = spectrum->cached_metrics;
    return 0;
}

/*---------------------------------------------------------------------------
 * Accessors
 *-------------------------------------------------------------------------*/

uint64_t bigram_spectrum_total_events(const bigram_spectrum_t* spectrum) {
    if (!spectrum) return 0;
    return spectrum->total_events;
}

uint32_t bigram_spectrum_vocab_cap(const bigram_spectrum_t* spectrum) {
    if (!spectrum) return 0;
    return spectrum->vocab_cap;
}

/* TA-1: read-only handle to internal counts buffer for serialization. */
const uint32_t* bigram_spectrum_counts(const bigram_spectrum_t* spectrum) {
    if (!spectrum) return NULL;
    return spectrum->counts;
}

/* TA-1: bulk-load counts + total_events from a flat buffer. The buffer
 * must contain exactly vocab_cap × vocab_cap uint32 entries; the caller
 * is responsible for size matching (we already know vocab_cap on this
 * spectrum, so the deserializer only needs to verify cap-equality up
 * front and then memcpy). */
int bigram_spectrum_load_counts(bigram_spectrum_t* spectrum,
                                 const uint32_t* counts,
                                 uint64_t total_events) {
    if (!spectrum || !counts || !spectrum->counts) return -1;
    size_t cells = (size_t)spectrum->vocab_cap * (size_t)spectrum->vocab_cap;
    memcpy(spectrum->counts, counts, cells * sizeof(uint32_t));
    spectrum->total_events = total_events;
    /* Drop any cached metrics; they reflect the prior count matrix. */
    spectrum->cached_metrics_valid = false;
    spectrum->last_compute_event_count = 0;
    memset(&spectrum->cached_metrics, 0, sizeof(spectrum->cached_metrics));
    return 0;
}

/*---------------------------------------------------------------------------
 * Compute spectral metrics
 *
 * 1. Build a [N × N] float matrix from the uint32 counts.
 *    N = fft_size (power-of-2). Cells outside [vocab_cap × vocab_cap]
 *    are zero-padded.
 *    Mean-center the values so the DC bin doesn't dominate the metric.
 * 2. Apply real FFT row-by-row → complex[N × N].
 * 3. Apply complex FFT column-by-column over that intermediate.
 * 4. |F[k1,k2]| = sqrt(re^2 + im^2). Build magnitude matrix.
 * 5. Compute the three metrics.
 *
 * Falls back to a degenerate "all-zero metrics" output when the matrix
 * is empty or the FFT plan fails.
 *-------------------------------------------------------------------------*/

static void zero_metrics(bigram_spectral_metrics_t* out) {
    if (!out) return;
    out->peak_strength = 0.0f;
    out->low_freq_concentration = 0.0f;
    out->spectral_entropy = 0.0f;
}

int bigram_spectrum_compute(bigram_spectrum_t* spectrum,
                            bigram_spectral_metrics_t* out) {
    if (!spectrum || !out) return -1;
    zero_metrics(out);
    if (spectrum->total_events == 0) return 0;

    const uint32_t N    = spectrum->fft_size;
    const uint32_t V    = spectrum->vocab_cap;
    const size_t   cells = (size_t)N * (size_t)N;

    /* Build dense float matrix, mean-centered. */
    float* mat = (float*)nimcp_calloc(cells, sizeof(float));
    if (!mat) return -1;

    /* Fill counts; compute mean over valid V × V region. */
    double sum = 0.0;
    for (uint32_t r = 0; r < V; r++) {
        for (uint32_t c = 0; c < V; c++) {
            float v = (float)spectrum->counts[(size_t)r * V + c];
            mat[(size_t)r * N + c] = v;
            sum += (double)v;
        }
    }
    float mean = (float)(sum / ((double)V * (double)V));
    /* Subtract mean only inside valid region (zero-pad stays zero). */
    for (uint32_t r = 0; r < V; r++) {
        for (uint32_t c = 0; c < V; c++) {
            mat[(size_t)r * N + c] -= mean;
        }
    }

    /* FFT plans. */
    fft_plan_t* plan_real = fft_plan_create(N, FFT_REAL);
    fft_plan_t* plan_cplx = fft_plan_create(N, FFT_COMPLEX);
    if (!plan_real || !plan_cplx) {
        if (plan_real) fft_plan_destroy(plan_real);
        if (plan_cplx) fft_plan_destroy(plan_cplx);
        nimcp_free(mat);
        return -1;
    }

    /* Intermediate complex matrix [N × N], stored row-major. */
    fft_complex_t* spec = (fft_complex_t*)nimcp_calloc(cells, sizeof(fft_complex_t));
    if (!spec) {
        fft_plan_destroy(plan_real);
        fft_plan_destroy(plan_cplx);
        nimcp_free(mat);
        return -1;
    }

    /* Pass 1: row-wise real FFT.
     * fft_execute_real returns the lower half [0 .. N/2] of the spectrum
     * for a real input. We expand it into the full N-bin complex row
     * using Hermitian symmetry F[N-k] = conj(F[k]) so the next pass
     * (complex FFT over columns) sees a proper N-point input. */
    fft_complex_t* row_half =
        (fft_complex_t*)nimcp_calloc((size_t)N / 2 + 1, sizeof(fft_complex_t));
    if (!row_half) {
        nimcp_free(spec);
        fft_plan_destroy(plan_real);
        fft_plan_destroy(plan_cplx);
        nimcp_free(mat);
        return -1;
    }

    for (uint32_t r = 0; r < N; r++) {
        if (!fft_execute_real(plan_real, &mat[(size_t)r * N], row_half)) {
            /* Soft-fail: leave any uncomputed rows as zeros. */
            continue;
        }
        /* Lower half. */
        for (uint32_t k = 0; k <= N / 2; k++) {
            spec[(size_t)r * N + k] = row_half[k];
        }
        /* Upper half via Hermitian symmetry. */
        for (uint32_t k = N / 2 + 1; k < N; k++) {
            fft_complex_t z = row_half[N - k];
            z.imag = -z.imag;
            spec[(size_t)r * N + k] = z;
        }
    }
    nimcp_free(row_half);

    /* Pass 2: column-wise complex FFT.
     * Read col into temp buffer, run complex FFT, write back. */
    fft_complex_t* col_in  = (fft_complex_t*)nimcp_calloc(N, sizeof(fft_complex_t));
    fft_complex_t* col_out = (fft_complex_t*)nimcp_calloc(N, sizeof(fft_complex_t));
    if (!col_in || !col_out) {
        if (col_in)  nimcp_free(col_in);
        if (col_out) nimcp_free(col_out);
        nimcp_free(spec);
        fft_plan_destroy(plan_real);
        fft_plan_destroy(plan_cplx);
        nimcp_free(mat);
        return -1;
    }

    for (uint32_t c = 0; c < N; c++) {
        for (uint32_t r = 0; r < N; r++) {
            col_in[r] = spec[(size_t)r * N + c];
        }
        if (!fft_execute_complex(plan_cplx, col_in, col_out)) {
            continue;
        }
        for (uint32_t r = 0; r < N; r++) {
            spec[(size_t)r * N + c] = col_out[r];
        }
    }

    nimcp_free(col_in);
    nimcp_free(col_out);
    fft_plan_destroy(plan_real);
    fft_plan_destroy(plan_cplx);
    nimcp_free(mat);

    /* Compute magnitudes and metrics.
     * Skip the DC bin [0,0] when computing peak / mean / std so the
     * mean-centered residual doesn't trivially make DC dominate. */
    float* mag = (float*)nimcp_calloc(cells, sizeof(float));
    if (!mag) { nimcp_free(spec); return -1; }

    double sum_mag = 0.0;
    double sum_mag2 = 0.0;
    double max_mag = 0.0;
    uint64_t n_bins = 0;

    for (uint32_t r = 0; r < N; r++) {
        for (uint32_t c = 0; c < N; c++) {
            fft_complex_t z = spec[(size_t)r * N + c];
            float m = sqrtf(z.real * z.real + z.imag * z.imag);
            mag[(size_t)r * N + c] = m;
            if (r == 0 && c == 0) continue;       /* skip DC */
            sum_mag  += (double)m;
            sum_mag2 += (double)m * (double)m;
            if ((double)m > max_mag) max_mag = (double)m;
            n_bins++;
        }
    }
    nimcp_free(spec);

    if (n_bins == 0) {
        nimcp_free(mag);
        return 0;
    }

    double mean_mag = sum_mag / (double)n_bins;
    double var_mag  = (sum_mag2 / (double)n_bins) - mean_mag * mean_mag;
    if (var_mag < 0.0) var_mag = 0.0;
    double std_mag  = sqrt(var_mag);

    /* peak_strength = (max - mean) / std */
    if (std_mag > 1e-9) {
        out->peak_strength = (float)((max_mag - mean_mag) / std_mag);
    } else {
        out->peak_strength = 0.0f;
    }

    /* low_freq_concentration: inner 1/4 of the spectrum.
     * For a 2D FFT with bins indexed [0..N), low frequencies live near
     * the corners (0,0), (0,N-1), (N-1,0), (N-1,N-1). Define "low-freq"
     * as bins where min(r, N-r) < N/8 AND min(c, N-c) < N/8 — i.e. an
     * N/4 × N/4 inner region (1/16 area, but covers all corners). The
     * spec said "inner 1/4 of the spectrum"; we use a half-width of N/8
     * so |inner| / |total| = (N/4)^2 / N^2 = 1/16. We bias toward the
     * obvious "corner-quadrant sum" interpretation: sum over the bins
     * within a corner-distance of N/8 in each dim. */
    double low_p = 0.0;
    double tot_p = 0.0;
    uint32_t lf_half = N / 8u;
    if (lf_half == 0) lf_half = 1;
    for (uint32_t r = 0; r < N; r++) {
        uint32_t dr = (r <= N - r) ? r : (N - r);
        for (uint32_t c = 0; c < N; c++) {
            uint32_t dc = (c <= N - c) ? c : (N - c);
            float m = mag[(size_t)r * N + c];
            double p = (double)m * (double)m;
            tot_p += p;
            if (dr < lf_half && dc < lf_half) low_p += p;
        }
    }
    if (tot_p > 1e-30) {
        out->low_freq_concentration = (float)(low_p / tot_p);
    } else {
        out->low_freq_concentration = 0.0f;
    }

    /* spectral_entropy: -Σ p log p over the magnitude-squared
     * distribution. Skip the DC bin to focus on the non-trivial part of
     * the spectrum (mean-centering already removed most of DC, but a
     * residual can still bias the distribution). */
    double sumP = 0.0;
    for (uint32_t r = 0; r < N; r++) {
        for (uint32_t c = 0; c < N; c++) {
            if (r == 0 && c == 0) continue;
            float m = mag[(size_t)r * N + c];
            sumP += (double)m * (double)m;
        }
    }
    if (sumP > 1e-30) {
        double H = 0.0;
        for (uint32_t r = 0; r < N; r++) {
            for (uint32_t c = 0; c < N; c++) {
                if (r == 0 && c == 0) continue;
                float m = mag[(size_t)r * N + c];
                double p = ((double)m * (double)m) / sumP;
                if (p > 1e-30) H -= p * log(p);
            }
        }
        out->spectral_entropy = (float)H;
    } else {
        out->spectral_entropy = 0.0f;
    }

    nimcp_free(mag);
    return 0;
}
