/**
 * @file nimcp_dsp.c
 * @brief Digital Signal Processing engine implementation
 *
 * Cooley-Tukey FFT, STFT, Welch PSD, Butterworth/FIR filters, DWT,
 * Hilbert transform, LMS/NLMS/RLS adaptive filters, mel filterbank, MFCC.
 */

#include "cognitive/math/nimcp_dsp.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>
#include <string.h>

#define LOG_TAG "DSP"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ============================================================================
 * Internal helpers
 * ============================================================================ */

/** Check if n is a power of two */
static inline bool is_power_of_two(uint32_t n) {
    return n > 0 && (n & (n - 1)) == 0;
}

/** Round up to next power of two */
static uint32_t next_power_of_two(uint32_t n) {
    if (n == 0) return 1;
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    return n + 1;
}

/** Modified Bessel function I0 for Kaiser window.
 *  I0(x) = Σ_{k=0}^∞ ((x²/4)^k / (k!)²) */
static double bessel_i0(double x) {
    double sum = 1.0;
    double term = 1.0;
    for (int k = 1; k < 25; k++) {
        term *= (x * x) / (4.0 * (double)k * (double)k);
        sum += term;
        if (term < 1e-16 * sum) break;  /* converged */
    }
    return sum;
}

/* ============================================================================
 * Engine lifecycle
 * ============================================================================ */

dsp_config_t dsp_default_config(void) {
    dsp_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.default_fft_size   = 1024;
    cfg.default_hop_size   = 256;
    cfg.default_window     = DSP_WINDOW_HANNING;
    cfg.default_sample_rate = 44100.0;
    return cfg;
}

dsp_engine_t* dsp_create(const dsp_config_t* config) {
    dsp_engine_t* engine = (dsp_engine_t*)nimcp_calloc(1, sizeof(dsp_engine_t));
    if (!engine) {
        LOG_ERROR(LOG_TAG, "Failed to allocate DSP engine");
        return NULL;
    }
    if (config) {
        engine->config = *config;
    } else {
        engine->config = dsp_default_config();
    }

    engine->fft_scratch = (dsp_complex_t*)nimcp_calloc(DSP_MAX_FFT_SIZE, sizeof(dsp_complex_t));
    engine->window_scratch = (double*)nimcp_calloc(DSP_MAX_FFT_SIZE, sizeof(double));
    if (!engine->fft_scratch || !engine->window_scratch) {
        LOG_ERROR(LOG_TAG, "Failed to allocate scratch buffers");
        nimcp_free(engine->fft_scratch);
        nimcp_free(engine->window_scratch);
        nimcp_free(engine);
        return NULL;
    }

    memset(&engine->stats, 0, sizeof(dsp_stats_t));
    engine->initialized = true;
    LOG_INFO(LOG_TAG, "DSP engine created (fft=%u, hop=%u, sr=%.0f)",
             engine->config.default_fft_size,
             engine->config.default_hop_size,
             engine->config.default_sample_rate);
    return engine;
}

void dsp_destroy(dsp_engine_t* engine) {
    if (!engine) return;
    nimcp_free(engine->fft_scratch);
    nimcp_free(engine->window_scratch);
    nimcp_free(engine);
}

dsp_stats_t dsp_get_stats(const dsp_engine_t* engine) {
    if (!engine) {
        dsp_stats_t empty;
        memset(&empty, 0, sizeof(empty));
        return empty;
    }
    return engine->stats;
}

/* ============================================================================
 * Window Functions
 * ============================================================================ */

void dsp_window(double* out, uint32_t length, dsp_window_type_t type) {
    if (!out || length == 0) return;

    double N = (double)(length - 1);
    if (N < 1.0) N = 1.0;

    for (uint32_t i = 0; i < length; i++) {
        double n = (double)i;
        switch (type) {
        case DSP_WINDOW_RECTANGULAR:
            out[i] = 1.0;
            break;
        case DSP_WINDOW_HAMMING:
            out[i] = 0.54 - 0.46 * cos(2.0 * M_PI * n / N);
            break;
        case DSP_WINDOW_HANNING:
            out[i] = 0.5 * (1.0 - cos(2.0 * M_PI * n / N));
            break;
        case DSP_WINDOW_BLACKMAN:
            out[i] = 0.42 - 0.5 * cos(2.0 * M_PI * n / N)
                          + 0.08 * cos(4.0 * M_PI * n / N);
            break;
        case DSP_WINDOW_KAISER: {
            double alpha = 3.0; /* default beta */
            double mid = N / 2.0;
            double ratio = (n - mid) / mid;
            double arg = 1.0 - ratio * ratio;
            if (arg < 0.0) arg = 0.0;  /* clamp: edges can exceed [-1,1] */
            out[i] = bessel_i0(alpha * sqrt(arg)) / bessel_i0(alpha);
            break;
        }
        case DSP_WINDOW_BARTLETT:
            out[i] = 1.0 - fabs(2.0 * n / N - 1.0);
            break;
        case DSP_WINDOW_FLAT_TOP:
            out[i] = 0.21557895 - 0.41663158 * cos(2.0 * M_PI * n / N)
                   + 0.277263158 * cos(4.0 * M_PI * n / N)
                   - 0.083578947 * cos(6.0 * M_PI * n / N)
                   + 0.006947368 * cos(8.0 * M_PI * n / N);
            break;
        default:
            out[i] = 1.0;
            break;
        }
    }
}

void dsp_apply_window(double* signal, uint32_t length, dsp_window_type_t type) {
    if (!signal || length == 0) return;
    /* Generate window, then multiply. Avoids duplicating all window formulas. */
    double* win = (double*)nimcp_calloc(length, sizeof(double));
    if (!win) return;
    dsp_window(win, length, type);
    for (uint32_t i = 0; i < length; i++)
        signal[i] *= win[i];
    nimcp_free(win);
}

/* ============================================================================
 * FFT — Cooley-Tukey radix-2 DIT
 * ============================================================================ */

void dsp_fft(dsp_complex_t* data, uint32_t n) {
    if (!data || n < 2 || !is_power_of_two(n)) return;

    /* Bit-reversal permutation */
    uint32_t log2n = 0;
    for (uint32_t tmp = n; tmp > 1; tmp >>= 1) log2n++;

    for (uint32_t i = 0; i < n; i++) {
        uint32_t j = 0;
        for (uint32_t bit = 0; bit < log2n; bit++) {
            if (i & (1u << bit)) j |= (1u << (log2n - 1 - bit));
        }
        if (j > i) {
            dsp_complex_t tmp = data[i];
            data[i] = data[j];
            data[j] = tmp;
        }
    }

    /* Butterfly stages */
    for (uint32_t stage = 1; stage <= log2n; stage++) {
        uint32_t m = 1u << stage;
        uint32_t half = m >> 1;
        double angle = -2.0 * M_PI / (double)m;
        dsp_complex_t wm;
        wm.re = cos(angle);
        wm.im = sin(angle);

        for (uint32_t k = 0; k < n; k += m) {
            dsp_complex_t w = {1.0, 0.0};
            for (uint32_t j = 0; j < half; j++) {
                dsp_complex_t t;
                t.re = w.re * data[k + j + half].re - w.im * data[k + j + half].im;
                t.im = w.re * data[k + j + half].im + w.im * data[k + j + half].re;

                dsp_complex_t u = data[k + j];
                data[k + j].re = u.re + t.re;
                data[k + j].im = u.im + t.im;
                data[k + j + half].re = u.re - t.re;
                data[k + j + half].im = u.im - t.im;

                double wr = w.re * wm.re - w.im * wm.im;
                double wi = w.re * wm.im + w.im * wm.re;
                w.re = wr;
                w.im = wi;
            }
        }
    }
}

void dsp_ifft(dsp_complex_t* data, uint32_t n) {
    if (!data || n < 2) return;

    /* Conjugate */
    for (uint32_t i = 0; i < n; i++) {
        data[i].im = -data[i].im;
    }

    /* Forward FFT */
    dsp_fft(data, n);

    /* Conjugate and scale */
    double inv_n = 1.0 / (double)n;
    for (uint32_t i = 0; i < n; i++) {
        data[i].re *= inv_n;
        data[i].im = -data[i].im * inv_n;
    }
}

void dsp_rfft(const double* signal, uint32_t n, dsp_complex_t* spectrum) {
    if (!signal || !spectrum || n < 2) return;

    uint32_t fft_n = next_power_of_two(n);
    if (fft_n > DSP_MAX_FFT_SIZE) fft_n = DSP_MAX_FFT_SIZE;

    for (uint32_t i = 0; i < fft_n; i++) {
        spectrum[i].re = (i < n) ? signal[i] : 0.0;
        spectrum[i].im = 0.0;
    }

    dsp_fft(spectrum, fft_n);
}

/* ============================================================================
 * STFT
 * ============================================================================ */

dsp_stft_result_t* dsp_stft(dsp_engine_t* engine, const double* signal,
                              uint32_t length, uint32_t fft_size,
                              uint32_t hop_size, dsp_window_type_t window) {
    if (!engine || !signal || length == 0) return NULL;
    if (!is_power_of_two(fft_size) || fft_size > DSP_MAX_FFT_SIZE) {
        LOG_ERROR(LOG_TAG, "STFT: fft_size must be power-of-2 <= %d", DSP_MAX_FFT_SIZE);
        return NULL;
    }
    if (hop_size == 0) hop_size = fft_size / 4;

    uint32_t num_frames = 0;
    if (length >= fft_size) {
        num_frames = (length - fft_size) / hop_size + 1;
    } else {
        num_frames = 1;
    }
    uint32_t num_bins = fft_size / 2 + 1;

    dsp_stft_result_t* result = (dsp_stft_result_t*)nimcp_calloc(1, sizeof(dsp_stft_result_t));
    if (!result) return NULL;

    result->frames      = (dsp_complex_t*)nimcp_calloc((size_t)num_frames * fft_size, sizeof(dsp_complex_t));
    result->magnitudes  = (double*)nimcp_calloc((size_t)num_frames * num_bins, sizeof(double));
    result->phases      = (double*)nimcp_calloc((size_t)num_frames * num_bins, sizeof(double));
    result->power       = (double*)nimcp_calloc((size_t)num_frames * num_bins, sizeof(double));

    if (!result->frames || !result->magnitudes || !result->phases || !result->power) {
        dsp_stft_free(result);
        return NULL;
    }

    result->num_frames      = num_frames;
    result->fft_size        = fft_size;
    result->hop_size        = hop_size;
    result->num_bins        = num_bins;
    result->sample_rate     = engine->config.default_sample_rate;
    result->freq_resolution = result->sample_rate / (double)fft_size;
    result->time_resolution = (double)hop_size / result->sample_rate;

    /* Generate window using engine scratch buffer (avoids 65KB stack alloc) */
    dsp_window(engine->window_scratch, fft_size, window);

    for (uint32_t f = 0; f < num_frames; f++) {
        uint32_t offset = f * hop_size;
        dsp_complex_t* frame = &result->frames[(size_t)f * fft_size];

        /* Copy and window the frame */
        for (uint32_t i = 0; i < fft_size; i++) {
            uint32_t idx = offset + i;
            frame[i].re = (idx < length) ? signal[idx] * engine->window_scratch[i] : 0.0;
            frame[i].im = 0.0;
        }

        /* FFT */
        dsp_fft(frame, fft_size);

        /* Extract magnitude, phase, power */
        double* mag = &result->magnitudes[(size_t)f * num_bins];
        double* pha = &result->phases[(size_t)f * num_bins];
        double* pwr = &result->power[(size_t)f * num_bins];
        for (uint32_t k = 0; k < num_bins; k++) {
            double re = frame[k].re;
            double im = frame[k].im;
            mag[k] = sqrt(re * re + im * im);
            pha[k] = atan2(im, re);
            pwr[k] = re * re + im * im;
        }
    }

    engine->stats.ffts_computed += num_frames;
    engine->stats.samples_processed += length;
    return result;
}

double* dsp_istft(dsp_engine_t* engine, const dsp_stft_result_t* stft,
                    uint32_t* out_length) {
    if (!engine || !stft || !out_length) return NULL;

    uint32_t fft_size = stft->fft_size;
    uint32_t hop_size = stft->hop_size;
    uint32_t total_len = (stft->num_frames - 1) * hop_size + fft_size;
    *out_length = total_len;

    double* output = (double*)nimcp_calloc(total_len, sizeof(double));
    double* window_sum = (double*)nimcp_calloc(total_len, sizeof(double));
    if (!output || !window_sum) {
        nimcp_free(output);
        nimcp_free(window_sum);
        return NULL;
    }

    double* win = (double*)nimcp_calloc(fft_size, sizeof(double));
    dsp_complex_t* buf = (dsp_complex_t*)nimcp_calloc(fft_size, sizeof(dsp_complex_t));
    if (!win || !buf) {
        nimcp_free(output);
        nimcp_free(window_sum);
        nimcp_free(win);
        nimcp_free(buf);
        return NULL;
    }
    dsp_window(win, fft_size, engine->config.default_window);

    for (uint32_t f = 0; f < stft->num_frames; f++) {
        /* Copy frame and reconstruct full spectrum (mirror conjugate) */
        const dsp_complex_t* frame = &stft->frames[(size_t)f * fft_size];
        memcpy(buf, frame, fft_size * sizeof(dsp_complex_t));

        /* IFFT */
        dsp_ifft(buf, fft_size);

        /* Overlap-add */
        uint32_t offset = f * hop_size;
        for (uint32_t i = 0; i < fft_size; i++) {
            if (offset + i < total_len) {
                output[offset + i] += buf[i].re * win[i];
                window_sum[offset + i] += win[i] * win[i];
            }
        }
    }

    /* Normalize by window sum */
    for (uint32_t i = 0; i < total_len; i++) {
        if (window_sum[i] > 1e-10) {
            output[i] /= window_sum[i];
        }
    }

    nimcp_free(buf);
    nimcp_free(win);
    nimcp_free(window_sum);
    return output;
}

void dsp_stft_free(dsp_stft_result_t* result) {
    if (!result) return;
    nimcp_free(result->frames);
    nimcp_free(result->magnitudes);
    nimcp_free(result->phases);
    nimcp_free(result->power);
    nimcp_free(result);
}

/* ============================================================================
 * Spectral Analysis
 * ============================================================================ */

dsp_psd_result_t* dsp_welch_psd(dsp_engine_t* engine, const double* signal,
                                  uint32_t length, double sample_rate,
                                  uint32_t segment_size, uint32_t overlap) {
    if (!engine || !signal || length == 0) return NULL;
    if (!is_power_of_two(segment_size)) {
        segment_size = next_power_of_two(segment_size);
    }
    if (segment_size > DSP_MAX_FFT_SIZE) segment_size = DSP_MAX_FFT_SIZE;
    if (overlap >= segment_size) overlap = segment_size / 2;

    uint32_t step = segment_size - overlap;
    uint32_t num_segments = 0;
    if (length >= segment_size) {
        num_segments = (length - segment_size) / step + 1;
    } else {
        num_segments = 1;
    }
    uint32_t num_bins = segment_size / 2 + 1;

    dsp_psd_result_t* result = (dsp_psd_result_t*)nimcp_calloc(1, sizeof(dsp_psd_result_t));
    if (!result) return NULL;

    result->psd = (double*)nimcp_calloc(num_bins, sizeof(double));
    result->frequencies = (double*)nimcp_calloc(num_bins, sizeof(double));
    if (!result->psd || !result->frequencies) {
        dsp_psd_free(result);
        return NULL;
    }

    /* Window (use heap, not stack) */
    double* win = (double*)nimcp_calloc(segment_size, sizeof(double));
    if (!win) { dsp_psd_free(result); return NULL; }
    dsp_window(win, segment_size, engine->config.default_window);

    /* Window energy for normalization */
    double win_energy = 0.0;
    for (uint32_t i = 0; i < segment_size; i++) {
        win_energy += win[i] * win[i];
    }

    dsp_complex_t* buf = (dsp_complex_t*)nimcp_calloc(segment_size, sizeof(dsp_complex_t));
    if (!buf) {
        dsp_psd_free(result);
        return NULL;
    }

    /* Accumulate PSD from each segment */
    for (uint32_t seg = 0; seg < num_segments; seg++) {
        uint32_t offset = seg * step;
        for (uint32_t i = 0; i < segment_size; i++) {
            uint32_t idx = offset + i;
            buf[i].re = (idx < length) ? signal[idx] * win[i] : 0.0;
            buf[i].im = 0.0;
        }
        dsp_fft(buf, segment_size);

        for (uint32_t k = 0; k < num_bins; k++) {
            double pwr = buf[k].re * buf[k].re + buf[k].im * buf[k].im;
            result->psd[k] += pwr;
        }
    }

    /* Normalize: PSD = (1 / (num_segments * sample_rate * win_energy)) * sum|X|^2 */
    double norm = 1.0 / ((double)num_segments * sample_rate * win_energy);
    result->total_power = 0.0;
    result->peak_power = 0.0;
    result->peak_frequency = 0.0;
    result->df = sample_rate / (double)segment_size;
    result->num_bins = num_bins;

    for (uint32_t k = 0; k < num_bins; k++) {
        result->psd[k] *= norm;
        /* Double one-sided bins (except DC and Nyquist) */
        if (k > 0 && k < num_bins - 1) {
            result->psd[k] *= 2.0;
        }
        result->frequencies[k] = (double)k * result->df;
        result->total_power += result->psd[k] * result->df;
        if (result->psd[k] > result->peak_power) {
            result->peak_power = result->psd[k];
            result->peak_frequency = result->frequencies[k];
        }
    }

    nimcp_free(buf);
    nimcp_free(win);
    engine->stats.ffts_computed += num_segments;
    return result;
}

void dsp_psd_free(dsp_psd_result_t* result) {
    if (!result) return;
    nimcp_free(result->psd);
    nimcp_free(result->frequencies);
    nimcp_free(result);
}

double* dsp_spectrogram(dsp_engine_t* engine, const double* signal,
                          uint32_t length, uint32_t fft_size,
                          uint32_t hop_size, uint32_t* num_frames,
                          uint32_t* num_bins) {
    if (!engine || !signal || !num_frames || !num_bins) return NULL;

    dsp_stft_result_t* stft = dsp_stft(engine, signal, length, fft_size,
                                         hop_size, engine->config.default_window);
    if (!stft) return NULL;

    *num_frames = stft->num_frames;
    *num_bins = stft->num_bins;

    size_t total = (size_t)stft->num_frames * stft->num_bins;
    double* spec = (double*)nimcp_calloc(total, sizeof(double));
    if (spec) {
        memcpy(spec, stft->power, total * sizeof(double));
    }

    dsp_stft_free(stft);
    return spec;
}

dsp_complex_t* dsp_cross_psd(dsp_engine_t* engine, const double* x,
                                const double* y, uint32_t length,
                                uint32_t segment_size) {
    if (!engine || !x || !y || length == 0) return NULL;
    if (!is_power_of_two(segment_size)) {
        segment_size = next_power_of_two(segment_size);
    }
    if (segment_size > DSP_MAX_FFT_SIZE) segment_size = DSP_MAX_FFT_SIZE;

    uint32_t overlap = segment_size / 2;
    uint32_t step = segment_size - overlap;
    uint32_t num_segments = (length >= segment_size) ? (length - segment_size) / step + 1 : 1;
    uint32_t num_bins = segment_size / 2 + 1;

    dsp_complex_t* cpsd = (dsp_complex_t*)nimcp_calloc(num_bins, sizeof(dsp_complex_t));
    dsp_complex_t* bx = (dsp_complex_t*)nimcp_calloc(segment_size, sizeof(dsp_complex_t));
    dsp_complex_t* by = (dsp_complex_t*)nimcp_calloc(segment_size, sizeof(dsp_complex_t));
    if (!cpsd || !bx || !by) {
        nimcp_free(cpsd);
        nimcp_free(bx);
        nimcp_free(by);
        return NULL;
    }

    double* win = (double*)nimcp_calloc(segment_size, sizeof(double));
    if (!win) { nimcp_free(cpsd); nimcp_free(bx); nimcp_free(by); return NULL; }
    dsp_window(win, segment_size, engine->config.default_window);

    for (uint32_t seg = 0; seg < num_segments; seg++) {
        uint32_t offset = seg * step;
        for (uint32_t i = 0; i < segment_size; i++) {
            uint32_t idx = offset + i;
            bx[i].re = (idx < length) ? x[idx] * win[i] : 0.0;
            bx[i].im = 0.0;
            by[i].re = (idx < length) ? y[idx] * win[i] : 0.0;
            by[i].im = 0.0;
        }
        dsp_fft(bx, segment_size);
        dsp_fft(by, segment_size);

        /* Pxy = X* . Y */
        for (uint32_t k = 0; k < num_bins; k++) {
            cpsd[k].re += bx[k].re * by[k].re + bx[k].im * by[k].im;
            cpsd[k].im += -bx[k].im * by[k].re + bx[k].re * by[k].im;
        }
    }

    double inv = 1.0 / (double)num_segments;
    for (uint32_t k = 0; k < num_bins; k++) {
        cpsd[k].re *= inv;
        cpsd[k].im *= inv;
    }

    nimcp_free(bx);
    nimcp_free(by);
    nimcp_free(win);
    return cpsd;
}

double* dsp_coherence(dsp_engine_t* engine, const double* x, const double* y,
                        uint32_t length, uint32_t segment_size,
                        uint32_t* out_num_bins) {
    if (!engine || !x || !y || !out_num_bins) return NULL;
    if (!is_power_of_two(segment_size)) {
        segment_size = next_power_of_two(segment_size);
    }
    if (segment_size > DSP_MAX_FFT_SIZE) segment_size = DSP_MAX_FFT_SIZE;

    uint32_t overlap = segment_size / 2;
    uint32_t step = segment_size - overlap;
    uint32_t num_segments = (length >= segment_size) ? (length - segment_size) / step + 1 : 1;
    uint32_t num_bins = segment_size / 2 + 1;
    *out_num_bins = num_bins;

    double* pxx = (double*)nimcp_calloc(num_bins, sizeof(double));
    double* pyy = (double*)nimcp_calloc(num_bins, sizeof(double));
    dsp_complex_t* pxy = (dsp_complex_t*)nimcp_calloc(num_bins, sizeof(dsp_complex_t));
    dsp_complex_t* bx = (dsp_complex_t*)nimcp_calloc(segment_size, sizeof(dsp_complex_t));
    dsp_complex_t* by = (dsp_complex_t*)nimcp_calloc(segment_size, sizeof(dsp_complex_t));

    if (!pxx || !pyy || !pxy || !bx || !by) {
        nimcp_free(pxx); nimcp_free(pyy); nimcp_free(pxy);
        nimcp_free(bx); nimcp_free(by);
        return NULL;
    }

    double* win = (double*)nimcp_calloc(segment_size, sizeof(double));
    if (!win) { nimcp_free(pxx); nimcp_free(pyy); nimcp_free(pxy); nimcp_free(bx); nimcp_free(by); return NULL; }
    dsp_window(win, segment_size, engine->config.default_window);

    for (uint32_t seg = 0; seg < num_segments; seg++) {
        uint32_t offset = seg * step;
        for (uint32_t i = 0; i < segment_size; i++) {
            uint32_t idx = offset + i;
            bx[i].re = (idx < length) ? x[idx] * win[i] : 0.0;
            bx[i].im = 0.0;
            by[i].re = (idx < length) ? y[idx] * win[i] : 0.0;
            by[i].im = 0.0;
        }
        dsp_fft(bx, segment_size);
        dsp_fft(by, segment_size);

        for (uint32_t k = 0; k < num_bins; k++) {
            pxx[k] += bx[k].re * bx[k].re + bx[k].im * bx[k].im;
            pyy[k] += by[k].re * by[k].re + by[k].im * by[k].im;
            pxy[k].re += bx[k].re * by[k].re + bx[k].im * by[k].im;
            pxy[k].im += -bx[k].im * by[k].re + bx[k].re * by[k].im;
        }
    }

    double* coh = (double*)nimcp_calloc(num_bins, sizeof(double));
    if (coh) {
        for (uint32_t k = 0; k < num_bins; k++) {
            double pxy_mag2 = pxy[k].re * pxy[k].re + pxy[k].im * pxy[k].im;
            double denom = pxx[k] * pyy[k];
            coh[k] = (denom > 1e-30) ? pxy_mag2 / denom : 0.0;
        }
    }

    nimcp_free(pxx); nimcp_free(pyy); nimcp_free(pxy);
    nimcp_free(bx); nimcp_free(by); nimcp_free(win);
    return coh;
}

double* dsp_cepstrum(dsp_engine_t* engine, const double* signal,
                       uint32_t length, uint32_t* cepstrum_length) {
    if (!engine || !signal || !cepstrum_length || length == 0) return NULL;

    uint32_t n = next_power_of_two(length);
    if (n > DSP_MAX_FFT_SIZE) n = DSP_MAX_FFT_SIZE;
    *cepstrum_length = n;

    dsp_complex_t* buf = (dsp_complex_t*)nimcp_calloc(n, sizeof(dsp_complex_t));
    if (!buf) return NULL;

    for (uint32_t i = 0; i < n; i++) {
        buf[i].re = (i < length) ? signal[i] : 0.0;
        buf[i].im = 0.0;
    }

    dsp_fft(buf, n);

    /* Log magnitude spectrum */
    for (uint32_t i = 0; i < n; i++) {
        double mag = sqrt(buf[i].re * buf[i].re + buf[i].im * buf[i].im);
        buf[i].re = log(mag + 1e-30);
        buf[i].im = 0.0;
    }

    dsp_ifft(buf, n);

    double* cep = (double*)nimcp_calloc(n, sizeof(double));
    if (cep) {
        for (uint32_t i = 0; i < n; i++) {
            cep[i] = buf[i].re;
        }
    }

    nimcp_free(buf);
    engine->stats.ffts_computed += 2;
    return cep;
}

/* ============================================================================
 * Filters
 * ============================================================================ */

dsp_filter_t dsp_design_filter(dsp_filter_type_t type, dsp_filter_band_t band,
                                 uint32_t order, double cutoff_low,
                                 double cutoff_high, double sample_rate) {
    dsp_filter_t f;
    memset(&f, 0, sizeof(f));
    f.type = type;
    f.band = band;
    f.order = (order > DSP_MAX_FILTER_ORDER) ? DSP_MAX_FILTER_ORDER : order;
    f.cutoff_low = cutoff_low;
    f.cutoff_high = cutoff_high;
    f.sample_rate = sample_rate;
    f.a[0] = 1.0;
    f.num_a = 1;

    double nyquist = sample_rate / 2.0;
    double wc = cutoff_low / nyquist; /* normalized 0..1 */
    if (wc >= 1.0) wc = 0.99;
    if (wc <= 0.0) wc = 0.01;

    if (type == DSP_FILTER_FIR) {
        /* Windowed sinc FIR design */
        uint32_t N = f.order;
        f.num_b = N + 1;
        f.num_a = 1;
        double M = (double)N / 2.0;

        for (uint32_t i = 0; i <= N; i++) {
            double n = (double)i - M;
            if (fabs(n) < 1e-10) {
                f.b[i] = wc;
            } else {
                f.b[i] = sin(M_PI * wc * n) / (M_PI * n);
            }
        }

        /* Apply Hamming window */
        for (uint32_t i = 0; i <= N; i++) {
            double w = 0.54 - 0.46 * cos(2.0 * M_PI * (double)i / (double)N);
            f.b[i] *= w;
        }

        /* Normalize for unity gain at DC (lowpass) */
        if (band == DSP_FILTER_LOWPASS) {
            double sum = 0.0;
            for (uint32_t i = 0; i <= N; i++) sum += f.b[i];
            if (fabs(sum) > 1e-10) {
                for (uint32_t i = 0; i <= N; i++) f.b[i] /= sum;
            }
        } else if (band == DSP_FILTER_HIGHPASS) {
            /* Spectral inversion for highpass */
            double sum = 0.0;
            for (uint32_t i = 0; i <= N; i++) sum += f.b[i];
            if (fabs(sum) > 1e-10) {
                for (uint32_t i = 0; i <= N; i++) f.b[i] /= sum;
            }
            for (uint32_t i = 0; i <= N; i++) f.b[i] = -f.b[i];
            f.b[N / 2] += 1.0;
        }
    } else if (type == DSP_FILTER_BUTTERWORTH) {
        /* Butterworth IIR via bilinear transform — cascaded biquad sections */
        /* For simplicity, implement 2nd-order section (order=2 biquad) */
        /* Prewarp */
        double wc_analog = 2.0 * sample_rate * tan(M_PI * cutoff_low / sample_rate);

        if (band == DSP_FILTER_LOWPASS && f.order == 2) {
            /* 2nd order Butterworth lowpass: H(s) = 1/(s^2 + sqrt(2)*s + 1) */
            double K = wc_analog / (2.0 * sample_rate);
            double K2 = K * K;
            double sqrt2 = 1.4142135623730951;
            double norm = 1.0 / (1.0 + sqrt2 * K + K2);

            f.b[0] = K2 * norm;
            f.b[1] = 2.0 * K2 * norm;
            f.b[2] = K2 * norm;
            f.a[0] = 1.0;
            f.a[1] = 2.0 * (K2 - 1.0) * norm;
            f.a[2] = (1.0 - sqrt2 * K + K2) * norm;
            f.num_b = 3;
            f.num_a = 3;
        } else if (band == DSP_FILTER_HIGHPASS && f.order == 2) {
            double K = wc_analog / (2.0 * sample_rate);
            double K2 = K * K;
            double sqrt2 = 1.4142135623730951;
            double norm = 1.0 / (1.0 + sqrt2 * K + K2);

            f.b[0] = norm;
            f.b[1] = -2.0 * norm;
            f.b[2] = norm;
            f.a[0] = 1.0;
            f.a[1] = 2.0 * (K2 - 1.0) * norm;
            f.a[2] = (1.0 - sqrt2 * K + K2) * norm;
            f.num_b = 3;
            f.num_a = 3;
        } else {
            /* General Nth-order: cascade of 2nd-order sections */
            /* Build as cascaded biquads into single polynomial */
            uint32_t n_sections = (f.order + 1) / 2;
            /* Polynomial can grow to 2*n_sections+1 terms — allocate enough */
            uint32_t max_poly = 2 * n_sections + 1;
            if (max_poly < f.order + 1) max_poly = f.order + 1;
            double* acc_b = (double*)nimcp_calloc(max_poly, sizeof(double));
            double* acc_a = (double*)nimcp_calloc(max_poly, sizeof(double));
            double* tmp_b = (double*)nimcp_calloc(max_poly, sizeof(double));
            double* tmp_a = (double*)nimcp_calloc(max_poly, sizeof(double));

            if (!acc_b || !acc_a || !tmp_b || !tmp_a) {
                nimcp_free(acc_b); nimcp_free(acc_a);
                nimcp_free(tmp_b); nimcp_free(tmp_a);
                /* Fall back to simple 1st order */
                double rc = 1.0 / (2.0 * M_PI * cutoff_low);
                double dt = 1.0 / sample_rate;
                double alpha = dt / (rc + dt);
                f.b[0] = alpha; f.b[1] = 0.0;
                f.a[0] = 1.0;  f.a[1] = -(1.0 - alpha);
                f.num_b = 2; f.num_a = 2;
                return f;
            }

            acc_b[0] = 1.0; acc_a[0] = 1.0;
            uint32_t acc_len = 1;

            for (uint32_t s = 0; s < n_sections; s++) {
                double theta = M_PI * (2.0 * (double)s + 1.0) / (2.0 * (double)f.order);
                double K = wc_analog / (2.0 * sample_rate);
                double K2 = K * K;
                double cos_theta = cos(theta);
                double norm_s = 1.0 / (1.0 + 2.0 * cos_theta * K + K2);

                double sb[3], sa[3];
                if (band == DSP_FILTER_LOWPASS) {
                    sb[0] = K2 * norm_s;
                    sb[1] = 2.0 * K2 * norm_s;
                    sb[2] = K2 * norm_s;
                } else {
                    sb[0] = norm_s;
                    sb[1] = -2.0 * norm_s;
                    sb[2] = norm_s;
                }
                sa[0] = 1.0;
                sa[1] = 2.0 * (K2 - 1.0) * norm_s;
                sa[2] = (1.0 - 2.0 * cos_theta * K + K2) * norm_s;

                /* Convolve acc with section */
                uint32_t new_len = acc_len + 2;
                if (new_len > max_poly) new_len = max_poly;
                memset(tmp_b, 0, max_poly * sizeof(double));
                memset(tmp_a, 0, max_poly * sizeof(double));
                for (uint32_t i = 0; i < acc_len; i++) {
                    for (uint32_t j = 0; j < 3 && (i + j) < max_poly; j++) {
                        tmp_b[i + j] += acc_b[i] * sb[j];
                        tmp_a[i + j] += acc_a[i] * sa[j];
                    }
                }
                memcpy(acc_b, tmp_b, max_poly * sizeof(double));
                memcpy(acc_a, tmp_a, max_poly * sizeof(double));
                acc_len = new_len;
            }

            f.num_b = acc_len;
            f.num_a = acc_len;
            for (uint32_t i = 0; i < acc_len && i <= DSP_MAX_FILTER_ORDER; i++) {
                f.b[i] = acc_b[i];
                f.a[i] = acc_a[i];
            }

            nimcp_free(acc_b); nimcp_free(acc_a);
            nimcp_free(tmp_b); nimcp_free(tmp_a);
        }
    } else {
        /* Chebyshev/Bessel: simplified 1st-order RC for now */
        double rc = 1.0 / (2.0 * M_PI * cutoff_low);
        double dt = 1.0 / sample_rate;
        double alpha = dt / (rc + dt);
        f.b[0] = alpha; f.b[1] = 0.0;
        f.a[0] = 1.0;  f.a[1] = -(1.0 - alpha);
        f.num_b = 2; f.num_a = 2;
    }

    return f;
}

void dsp_filter_apply(const dsp_filter_t* filter, const double* input,
                        double* output, uint32_t length) {
    if (!filter || !input || !output || length == 0) return;

    /* Direct Form II Transposed */
    double state[DSP_MAX_FILTER_ORDER + 1];
    memset(state, 0, sizeof(state));

    for (uint32_t n = 0; n < length; n++) {
        double y = filter->b[0] * input[n] + state[0];
        uint32_t max_k = (filter->num_b > filter->num_a) ? filter->num_b : filter->num_a;
        for (uint32_t k = 1; k < max_k; k++) {
            state[k - 1] = 0.0;
            if (k < filter->num_b) state[k - 1] += filter->b[k] * input[n];
            if (k < filter->num_a) state[k - 1] -= filter->a[k] * y;
            if (k < max_k - 1) state[k - 1] += state[k];
        }
        output[n] = y;
    }
}

double dsp_filter_tick(dsp_filter_t* filter, double input_sample) {
    if (!filter) return 0.0;

    double y = filter->b[0] * input_sample + filter->state[0];
    uint32_t max_k = (filter->num_b > filter->num_a) ? filter->num_b : filter->num_a;
    for (uint32_t k = 1; k < max_k; k++) {
        filter->state[k - 1] = 0.0;
        if (k < filter->num_b) filter->state[k - 1] += filter->b[k] * input_sample;
        if (k < filter->num_a) filter->state[k - 1] -= filter->a[k] * y;
        if (k < max_k - 1) filter->state[k - 1] += filter->state[k];
    }

    return y;
}

void dsp_filter_reset(dsp_filter_t* filter) {
    if (!filter) return;
    memset(filter->state, 0, sizeof(filter->state));
}

void dsp_filter_freqz(const dsp_filter_t* filter, double* magnitude,
                        double* phase, uint32_t num_points) {
    if (!filter || !magnitude || !phase || num_points == 0) return;

    for (uint32_t i = 0; i < num_points; i++) {
        double w = M_PI * (double)i / (double)(num_points - 1);

        /* Evaluate H(e^jw) = B(e^jw) / A(e^jw) */
        double num_re = 0.0, num_im = 0.0;
        for (uint32_t k = 0; k < filter->num_b; k++) {
            num_re += filter->b[k] * cos((double)k * w);
            num_im -= filter->b[k] * sin((double)k * w);
        }

        double den_re = 0.0, den_im = 0.0;
        for (uint32_t k = 0; k < filter->num_a; k++) {
            den_re += filter->a[k] * cos((double)k * w);
            den_im -= filter->a[k] * sin((double)k * w);
        }

        double den_mag2 = den_re * den_re + den_im * den_im;
        if (den_mag2 < 1e-30) den_mag2 = 1e-30;

        /* H = num / den */
        double h_re = (num_re * den_re + num_im * den_im) / den_mag2;
        double h_im = (num_im * den_re - num_re * den_im) / den_mag2;

        magnitude[i] = sqrt(h_re * h_re + h_im * h_im);
        phase[i] = atan2(h_im, h_re);
    }
}

/* ============================================================================
 * Correlation
 * ============================================================================ */

dsp_correlation_t* dsp_autocorrelation(const double* signal, uint32_t length,
                                         uint32_t max_lag) {
    if (!signal || length == 0) return NULL;
    if (max_lag == 0 || max_lag >= length) max_lag = length - 1;

    uint32_t out_len = max_lag + 1;
    dsp_correlation_t* result = (dsp_correlation_t*)nimcp_calloc(1, sizeof(dsp_correlation_t));
    if (!result) return NULL;

    result->values = (double*)nimcp_calloc(out_len, sizeof(double));
    result->lags   = (int32_t*)nimcp_calloc(out_len, sizeof(int32_t));
    if (!result->values || !result->lags) {
        dsp_correlation_free(result);
        return NULL;
    }

    result->length = out_len;
    result->peak_value = 0.0;
    result->peak_lag = 0;

    for (uint32_t k = 0; k <= max_lag; k++) {
        double sum = 0.0;
        for (uint32_t n = 0; n < length - k; n++) {
            sum += signal[n] * signal[n + k];
        }
        result->values[k] = sum;
        result->lags[k] = (int32_t)k;
        if (sum > result->peak_value) {
            result->peak_value = sum;
            result->peak_lag = (int32_t)k;
        }
    }

    return result;
}

dsp_correlation_t* dsp_crosscorrelation(const double* x, const double* y,
                                          uint32_t length, uint32_t max_lag) {
    if (!x || !y || length == 0) return NULL;
    if (max_lag == 0 || max_lag >= length) max_lag = length - 1;

    uint32_t out_len = 2 * max_lag + 1;
    dsp_correlation_t* result = (dsp_correlation_t*)nimcp_calloc(1, sizeof(dsp_correlation_t));
    if (!result) return NULL;

    result->values = (double*)nimcp_calloc(out_len, sizeof(double));
    result->lags   = (int32_t*)nimcp_calloc(out_len, sizeof(int32_t));
    if (!result->values || !result->lags) {
        dsp_correlation_free(result);
        return NULL;
    }

    result->length = out_len;
    result->peak_value = -1e30;
    result->peak_lag = 0;

    for (int32_t k = -(int32_t)max_lag; k <= (int32_t)max_lag; k++) {
        double sum = 0.0;
        for (uint32_t n = 0; n < length; n++) {
            int32_t idx = (int32_t)n + k;
            if (idx >= 0 && idx < (int32_t)length) {
                sum += x[n] * y[idx];
            }
        }
        uint32_t out_idx = (uint32_t)(k + (int32_t)max_lag);
        result->values[out_idx] = sum;
        result->lags[out_idx] = k;
        if (sum > result->peak_value) {
            result->peak_value = sum;
            result->peak_lag = k;
        }
    }

    return result;
}

void dsp_correlation_free(dsp_correlation_t* result) {
    if (!result) return;
    nimcp_free(result->values);
    nimcp_free(result->lags);
    nimcp_free(result);
}

/* ============================================================================
 * Wavelet Transform (DWT)
 * ============================================================================ */

/* Haar coefficients */
static const double haar_lo[] = { 0.7071067811865476, 0.7071067811865476 };
static const double haar_hi[] = { 0.7071067811865476, -0.7071067811865476 };

/* Daubechies-2 (db2) coefficients */
static const double db2_lo[] = {
    -0.12940952255092145, 0.22414386804185735,
     0.83651630373746899, 0.48296291314469025
};
static const double db2_hi[] = {
    -0.48296291314469025, 0.83651630373746899,
    -0.22414386804185735, -0.12940952255092145
};

/* Daubechies-4 (db4) coefficients */
static const double db4_lo[] = {
    -0.01059740178499600,  0.03288301166688520,
     0.03084138183598697, -0.18703481171888114,
    -0.02798376941698385,  0.63088076792959036,
     0.71484657055254153,  0.23037781330885523
};
static const double db4_hi[] = {
    -0.23037781330885523,  0.71484657055254153,
    -0.63088076792959036, -0.02798376941698385,
     0.18703481171888114,  0.03084138183598697,
    -0.03288301166688520, -0.01059740178499600
};

/* Daubechies-8 (db8) coefficients */
static const double db8_lo[] = {
    -0.00011747678400228,  0.00067544940599855,
    -0.00039174037299597, -0.00487035299301066,
     0.00874609404701566,  0.01395351747052901,
    -0.04408825393079037, -0.01736930100180755,
     0.12874742662047845,  0.00047248457399797,
    -0.28401554296242809, -0.01581027917858828,
     0.58535468365486909,  0.67563073629728980,
     0.31287159091429997,  0.05441584224310400
};
static const double db8_hi[] = {
    -0.05441584224310400,  0.31287159091429997,
    -0.67563073629728980,  0.58535468365486909,
     0.01581027917858828, -0.28401554296242809,
    -0.00047248457399797,  0.12874742662047845,
     0.01736930100180755, -0.04408825393079037,
    -0.01395351747052901,  0.00874609404701566,
     0.00487035299301066, -0.00039174037299597,
    -0.00067544940599855, -0.00011747678400228
};

void dsp_wavelet_coeffs(dsp_wavelet_type_t wavelet, const double** lo,
                          const double** hi, uint32_t* length) {
    switch (wavelet) {
    case DSP_WAVELET_HAAR:
        *lo = haar_lo; *hi = haar_hi; *length = 2; break;
    case DSP_WAVELET_DB2:
        *lo = db2_lo; *hi = db2_hi; *length = 4; break;
    case DSP_WAVELET_DB4:
        *lo = db4_lo; *hi = db4_hi; *length = 8; break;
    case DSP_WAVELET_DB8:
        *lo = db8_lo; *hi = db8_hi; *length = 16; break;
    default:
        *lo = haar_lo; *hi = haar_hi; *length = 2; break;
    }
}

dsp_dwt_result_t* dsp_dwt(const double* signal, uint32_t length,
                             dsp_wavelet_type_t wavelet, uint32_t levels) {
    if (!signal || length == 0) return NULL;
    if (levels == 0) levels = 1;
    if (levels > DSP_MAX_WAVELET_LEVELS) levels = DSP_MAX_WAVELET_LEVELS;

    const double* lo_filter = NULL;
    const double* hi_filter = NULL;
    uint32_t filt_len = 0;
    dsp_wavelet_coeffs(wavelet, &lo_filter, &hi_filter, &filt_len);

    dsp_dwt_result_t* result = (dsp_dwt_result_t*)nimcp_calloc(1, sizeof(dsp_dwt_result_t));
    if (!result) return NULL;

    result->level_sizes = (uint32_t*)nimcp_calloc(levels + 1, sizeof(uint32_t));
    if (!result->level_sizes) {
        nimcp_free(result);
        return NULL;
    }

    /* Compute level sizes */
    uint32_t total = 0;
    uint32_t current_len = length;
    for (uint32_t l = 0; l < levels; l++) {
        uint32_t detail_len = current_len / 2;
        if (detail_len == 0) {
            levels = l;
            break;
        }
        result->level_sizes[l] = detail_len;
        total += detail_len;
        current_len = detail_len;
    }
    /* Last approximation */
    result->level_sizes[levels] = current_len;
    total += current_len;

    result->num_levels = levels;
    result->total_coeffs = total;
    result->wavelet = wavelet;

    /* Allocate all coefficients: detail for each level + final approximation */
    result->detail = (double*)nimcp_calloc(total, sizeof(double));
    result->approx = (double*)nimcp_calloc(current_len, sizeof(double));
    if (!result->detail || !result->approx) {
        dsp_dwt_free(result);
        return NULL;
    }

    /* Working buffer */
    double* work = (double*)nimcp_calloc(length, sizeof(double));
    if (!work) {
        dsp_dwt_free(result);
        return NULL;
    }
    memcpy(work, signal, length * sizeof(double));

    current_len = length;
    uint32_t detail_offset = 0;

    for (uint32_t l = 0; l < levels; l++) {
        uint32_t out_len = current_len / 2;
        double* approx_buf = (double*)nimcp_calloc(out_len, sizeof(double));
        double* detail_buf = (double*)nimcp_calloc(out_len, sizeof(double));
        if (!approx_buf || !detail_buf) {
            nimcp_free(approx_buf);
            nimcp_free(detail_buf);
            nimcp_free(work);
            dsp_dwt_free(result);
            return NULL;
        }

        /* Convolution + downsample by 2 */
        for (uint32_t i = 0; i < out_len; i++) {
            double a = 0.0, d = 0.0;
            for (uint32_t j = 0; j < filt_len; j++) {
                uint32_t idx = 2 * i + j;
                double val = (idx < current_len) ? work[idx] : 0.0;
                a += lo_filter[j] * val;
                d += hi_filter[j] * val;
            }
            approx_buf[i] = a;
            detail_buf[i] = d;
        }

        /* Store detail coefficients */
        memcpy(&result->detail[detail_offset], detail_buf, out_len * sizeof(double));
        detail_offset += out_len;

        /* Update work buffer with approximation for next level */
        memcpy(work, approx_buf, out_len * sizeof(double));
        current_len = out_len;

        nimcp_free(approx_buf);
        nimcp_free(detail_buf);
    }

    /* Store final approximation */
    memcpy(result->approx, work, current_len * sizeof(double));
    nimcp_free(work);

    return result;
}

double* dsp_idwt(const dsp_dwt_result_t* dwt, uint32_t* out_length) {
    if (!dwt || !out_length || dwt->num_levels == 0) return NULL;

    const double* lo_filter = NULL;
    const double* hi_filter = NULL;
    uint32_t filt_len = 0;
    dsp_wavelet_coeffs(dwt->wavelet, &lo_filter, &hi_filter, &filt_len);

    /* Start from deepest approximation */
    uint32_t current_len = dwt->level_sizes[dwt->num_levels];
    double* work = (double*)nimcp_calloc(current_len, sizeof(double));
    if (!work) return NULL;
    memcpy(work, dwt->approx, current_len * sizeof(double));

    /* Compute detail offsets */
    uint32_t detail_offset = 0;
    for (uint32_t l = 0; l < dwt->num_levels; l++) {
        detail_offset += dwt->level_sizes[l];
    }

    /* Reconstruct from deepest to shallowest */
    for (int32_t l = (int32_t)dwt->num_levels - 1; l >= 0; l--) {
        uint32_t detail_len = dwt->level_sizes[l];
        detail_offset -= detail_len;
        uint32_t out_len = detail_len * 2;

        double* output = (double*)nimcp_calloc(out_len, sizeof(double));
        if (!output) {
            nimcp_free(work);
            return NULL;
        }

        /* Upsample + filter (synthesis).
         * Clamp detail index to detail_len to prevent overread when
         * current_len (approx) differs from detail_len. */
        uint32_t max_i = (current_len < detail_len) ? current_len : detail_len;
        for (uint32_t i = 0; i < max_i; i++) {
            for (uint32_t j = 0; j < filt_len; j++) {
                uint32_t idx = 2 * i + j;
                if (idx < out_len) {
                    output[idx] += lo_filter[j] * work[i];
                    output[idx] += hi_filter[j] * dwt->detail[detail_offset + i];
                }
            }
        }

        nimcp_free(work);
        work = output;
        current_len = out_len;
    }

    *out_length = current_len;
    return work;
}

void dsp_dwt_free(dsp_dwt_result_t* result) {
    if (!result) return;
    nimcp_free(result->approx);
    nimcp_free(result->detail);
    nimcp_free(result->level_sizes);
    nimcp_free(result);
}

/* ============================================================================
 * Hilbert Transform
 * ============================================================================ */

dsp_analytic_signal_t* dsp_hilbert(dsp_engine_t* engine, const double* signal,
                                     uint32_t length, double sample_rate) {
    if (!engine || !signal || length == 0) return NULL;

    uint32_t n = next_power_of_two(length);
    if (n > DSP_MAX_FFT_SIZE) n = DSP_MAX_FFT_SIZE;

    dsp_complex_t* buf = (dsp_complex_t*)nimcp_calloc(n, sizeof(dsp_complex_t));
    if (!buf) return NULL;

    /* Load signal */
    for (uint32_t i = 0; i < n; i++) {
        buf[i].re = (i < length) ? signal[i] : 0.0;
        buf[i].im = 0.0;
    }

    /* Forward FFT */
    dsp_fft(buf, n);

    /* Zero negative frequencies, double positive frequencies */
    /* DC and Nyquist stay as-is; bins 1..N/2-1 double; bins N/2+1..N-1 zero */
    uint32_t half = n / 2;
    for (uint32_t i = 1; i < half; i++) {
        buf[i].re *= 2.0;
        buf[i].im *= 2.0;
    }
    for (uint32_t i = half + 1; i < n; i++) {
        buf[i].re = 0.0;
        buf[i].im = 0.0;
    }

    /* Inverse FFT */
    dsp_ifft(buf, n);

    /* Extract envelope, instantaneous phase, instantaneous frequency */
    dsp_analytic_signal_t* result = (dsp_analytic_signal_t*)nimcp_calloc(1, sizeof(dsp_analytic_signal_t));
    if (!result) {
        nimcp_free(buf);
        return NULL;
    }

    result->envelope   = (double*)nimcp_calloc(length, sizeof(double));
    result->inst_phase = (double*)nimcp_calloc(length, sizeof(double));
    result->inst_freq  = (double*)nimcp_calloc(length, sizeof(double));
    if (!result->envelope || !result->inst_phase || !result->inst_freq) {
        nimcp_free(buf);
        dsp_analytic_free(result);
        return NULL;
    }
    result->length = length;

    for (uint32_t i = 0; i < length; i++) {
        double re = buf[i].re;
        double im = buf[i].im;
        result->envelope[i] = sqrt(re * re + im * im);
        result->inst_phase[i] = atan2(im, re);
    }

    /* Instantaneous frequency = d(phase)/dt / (2*pi) */
    for (uint32_t i = 1; i < length; i++) {
        double dp = result->inst_phase[i] - result->inst_phase[i - 1];
        /* Unwrap */
        while (dp > M_PI) dp -= 2.0 * M_PI;
        while (dp < -M_PI) dp += 2.0 * M_PI;
        result->inst_freq[i] = dp * sample_rate / (2.0 * M_PI);
    }
    if (length > 0) result->inst_freq[0] = result->inst_freq[1 < length ? 1 : 0];

    nimcp_free(buf);
    engine->stats.ffts_computed += 2;
    return result;
}

void dsp_analytic_free(dsp_analytic_signal_t* result) {
    if (!result) return;
    nimcp_free(result->envelope);
    nimcp_free(result->inst_phase);
    nimcp_free(result->inst_freq);
    nimcp_free(result);
}

/* ============================================================================
 * Adaptive Filters
 * ============================================================================ */

dsp_adaptive_filter_t* dsp_adaptive_create(dsp_adaptive_type_t type,
                                              uint32_t order, double mu) {
    if (order == 0 || order > DSP_MAX_FILTER_ORDER) return NULL;

    dsp_adaptive_filter_t* f = (dsp_adaptive_filter_t*)nimcp_calloc(1, sizeof(dsp_adaptive_filter_t));
    if (!f) return NULL;

    f->type = type;
    f->order = order;
    f->mu = mu;
    f->lambda = 0.99; /* default RLS forgetting factor */
    f->error = 0.0;
    f->samples_processed = 0;

    f->weights = (double*)nimcp_calloc(order, sizeof(double));
    f->buffer  = (double*)nimcp_calloc(order, sizeof(double));
    if (!f->weights || !f->buffer) {
        dsp_adaptive_destroy(f);
        return NULL;
    }

    if (type == DSP_ADAPTIVE_RLS) {
        /* Initialize P = delta * I (large delta for unknown initial conditions) */
        f->P = (double*)nimcp_calloc((size_t)order * order, sizeof(double));
        if (!f->P) {
            dsp_adaptive_destroy(f);
            return NULL;
        }
        double delta = 100.0;
        for (uint32_t i = 0; i < order; i++) {
            f->P[i * order + i] = delta;
        }
    }

    return f;
}

void dsp_adaptive_destroy(dsp_adaptive_filter_t* filter) {
    if (!filter) return;
    nimcp_free(filter->weights);
    nimcp_free(filter->buffer);
    nimcp_free(filter->P);
    nimcp_free(filter);
}

double dsp_adaptive_tick(dsp_adaptive_filter_t* filter,
                           double input, double desired) {
    if (!filter) return 0.0;

    uint32_t N = filter->order;

    /* Shift buffer and insert new sample */
    for (uint32_t i = N - 1; i > 0; i--) {
        filter->buffer[i] = filter->buffer[i - 1];
    }
    filter->buffer[0] = input;

    /* Compute output y = w^T x */
    double y = 0.0;
    for (uint32_t i = 0; i < N; i++) {
        y += filter->weights[i] * filter->buffer[i];
    }

    /* Error */
    double e = desired - y;
    filter->error = e;

    /* Update weights based on algorithm type */
    switch (filter->type) {
    case DSP_ADAPTIVE_LMS:
        /* w += 2 * mu * e * x */
        for (uint32_t i = 0; i < N; i++) {
            filter->weights[i] += 2.0 * filter->mu * e * filter->buffer[i];
        }
        break;

    case DSP_ADAPTIVE_NLMS: {
        /* w += (mu / (||x||^2 + eps)) * e * x */
        double norm2 = 0.0;
        for (uint32_t i = 0; i < N; i++) {
            norm2 += filter->buffer[i] * filter->buffer[i];
        }
        double step = filter->mu / (norm2 + 1e-8);
        for (uint32_t i = 0; i < N; i++) {
            filter->weights[i] += step * e * filter->buffer[i];
        }
        break;
    }

    case DSP_ADAPTIVE_RLS: {
        /* RLS: Recursive Least Squares */
        /* k = (P x) / (lambda + x^T P x) */
        double* Px = (double*)nimcp_calloc(N, sizeof(double));
        if (!Px) break;

        for (uint32_t i = 0; i < N; i++) {
            Px[i] = 0.0;
            for (uint32_t j = 0; j < N; j++) {
                Px[i] += filter->P[i * N + j] * filter->buffer[j];
            }
        }

        double xPx = 0.0;
        for (uint32_t i = 0; i < N; i++) {
            xPx += filter->buffer[i] * Px[i];
        }

        double denom = filter->lambda + xPx;
        if (fabs(denom) < 1e-30) denom = 1e-30;

        double* k = (double*)nimcp_calloc(N, sizeof(double));
        if (!k) {
            nimcp_free(Px);
            break;
        }
        for (uint32_t i = 0; i < N; i++) {
            k[i] = Px[i] / denom;
        }

        /* w += k * e */
        for (uint32_t i = 0; i < N; i++) {
            filter->weights[i] += k[i] * e;
        }

        /* P = (1/lambda) * (P - k * Px^T) — update in-place to avoid OOM risk */
        double inv_lambda = 1.0 / filter->lambda;
        for (uint32_t i = 0; i < N; i++) {
            for (uint32_t j = 0; j < N; j++) {
                filter->P[i * N + j] = inv_lambda * (filter->P[i * N + j] - k[i] * Px[j]);
            }
        }

        nimcp_free(Px);
        nimcp_free(k);
        break;
    }
    }

    filter->samples_processed++;
    return y;
}

const double* dsp_adaptive_get_weights(const dsp_adaptive_filter_t* filter) {
    if (!filter) return NULL;
    return filter->weights;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

double dsp_mag_to_db(double magnitude) {
    if (magnitude < 1e-30) magnitude = 1e-30;
    return 20.0 * log10(magnitude);
}

double dsp_db_to_mag(double db) {
    return pow(10.0, db / 20.0);
}

double dsp_rms(const double* signal, uint32_t length) {
    if (!signal || length == 0) return 0.0;
    double sum = 0.0;
    for (uint32_t i = 0; i < length; i++) {
        sum += signal[i] * signal[i];
    }
    return sqrt(sum / (double)length);
}

double dsp_zero_crossing_rate(const double* signal, uint32_t length) {
    if (!signal || length < 2) return 0.0;
    uint32_t crossings = 0;
    for (uint32_t i = 1; i < length; i++) {
        if ((signal[i] >= 0.0 && signal[i - 1] < 0.0) ||
            (signal[i] < 0.0 && signal[i - 1] >= 0.0)) {
            crossings++;
        }
    }
    return (double)crossings / (double)(length - 1);
}

double dsp_hz_to_mel(double hz) {
    return 2595.0 * log10(1.0 + hz / 700.0);
}

double dsp_mel_to_hz(double mel) {
    return 700.0 * (pow(10.0, mel / 2595.0) - 1.0);
}

void dsp_mel_filterbank(const double* spectrum, uint32_t num_bins,
                          double sample_rate, double* mel_energies,
                          uint32_t num_mel_filters) {
    if (!spectrum || !mel_energies || num_bins == 0 || num_mel_filters == 0) return;

    double mel_low = dsp_hz_to_mel(0.0);
    double mel_high = dsp_hz_to_mel(sample_rate / 2.0);

    /* Compute mel center frequencies */
    uint32_t num_points = num_mel_filters + 2;
    double* mel_points = (double*)nimcp_calloc(num_points, sizeof(double));
    double* hz_points  = (double*)nimcp_calloc(num_points, sizeof(double));
    uint32_t* bin_points = (uint32_t*)nimcp_calloc(num_points, sizeof(uint32_t));

    if (!mel_points || !hz_points || !bin_points) {
        nimcp_free(mel_points);
        nimcp_free(hz_points);
        nimcp_free(bin_points);
        return;
    }

    for (uint32_t i = 0; i < num_points; i++) {
        mel_points[i] = mel_low + (mel_high - mel_low) * (double)i / (double)(num_points - 1);
        hz_points[i] = dsp_mel_to_hz(mel_points[i]);
        /* Standard formula: bin = floor((fft_size+1) * f / sr) where fft_size = 2*(num_bins-1) */
        uint32_t fft_size_est = 2 * (num_bins - 1);
        bin_points[i] = (uint32_t)floor((double)(fft_size_est + 1) * hz_points[i] / sample_rate);
        if (bin_points[i] >= num_bins) bin_points[i] = num_bins - 1;
    }

    /* Triangular filters */
    for (uint32_t m = 0; m < num_mel_filters; m++) {
        double energy = 0.0;
        uint32_t f_start = bin_points[m];
        uint32_t f_center = bin_points[m + 1];
        uint32_t f_end = bin_points[m + 2];

        /* Rising slope */
        for (uint32_t k = f_start; k <= f_center && k < num_bins; k++) {
            double weight = 0.0;
            if (f_center > f_start) {
                weight = (double)(k - f_start) / (double)(f_center - f_start);
            }
            energy += spectrum[k] * weight;
        }
        /* Falling slope */
        for (uint32_t k = f_center + 1; k <= f_end && k < num_bins; k++) {
            double weight = 0.0;
            if (f_end > f_center) {
                weight = (double)(f_end - k) / (double)(f_end - f_center);
            }
            energy += spectrum[k] * weight;
        }

        mel_energies[m] = energy;
    }

    nimcp_free(mel_points);
    nimcp_free(hz_points);
    nimcp_free(bin_points);
}

void dsp_mfcc(dsp_engine_t* engine, const double* signal, uint32_t length,
                double sample_rate, double* mfcc_out, uint32_t num_coeffs) {
    if (!engine || !signal || !mfcc_out || length == 0 || num_coeffs == 0) return;
    memset(mfcc_out, 0, num_coeffs * sizeof(double));  /* zero output first */

    uint32_t fft_size = next_power_of_two(length);
    if (fft_size > DSP_MAX_FFT_SIZE) fft_size = DSP_MAX_FFT_SIZE;
    uint32_t num_bins = fft_size / 2 + 1;
    uint32_t num_mel_filters = 26;

    /* FFT of windowed signal */
    dsp_complex_t* buf = (dsp_complex_t*)nimcp_calloc(fft_size, sizeof(dsp_complex_t));
    double* power_spec = (double*)nimcp_calloc(num_bins, sizeof(double));
    double* mel_energies = (double*)nimcp_calloc(num_mel_filters, sizeof(double));

    if (!buf || !power_spec || !mel_energies) {
        nimcp_free(buf);
        nimcp_free(power_spec);
        nimcp_free(mel_energies);
        return;
    }

    /* Window the signal, then pack into complex buffer for FFT */
    double* windowed = (double*)nimcp_calloc(fft_size, sizeof(double));
    if (!windowed) {
        nimcp_free(buf);
        nimcp_free(power_spec);
        nimcp_free(mel_energies);
        return;
    }
    for (uint32_t i = 0; i < fft_size; i++)
        windowed[i] = (i < length) ? signal[i] : 0.0;
    dsp_apply_window(windowed, length, DSP_WINDOW_HAMMING);

    for (uint32_t i = 0; i < fft_size; i++) {
        buf[i].re = windowed[i];
        buf[i].im = 0.0;
    }
    dsp_fft(buf, fft_size);

    /* Power spectrum */
    for (uint32_t k = 0; k < num_bins; k++) {
        power_spec[k] = buf[k].re * buf[k].re + buf[k].im * buf[k].im;
    }

    /* Mel filterbank */
    dsp_mel_filterbank(power_spec, num_bins, sample_rate, mel_energies, num_mel_filters);

    /* Log compression */
    for (uint32_t m = 0; m < num_mel_filters; m++) {
        mel_energies[m] = log(mel_energies[m] + 1e-30);
    }

    /* DCT (Type-II) to get MFCCs */
    for (uint32_t i = 0; i < num_coeffs && i < num_mel_filters; i++) {
        double sum = 0.0;
        for (uint32_t j = 0; j < num_mel_filters; j++) {
            sum += mel_energies[j] * cos(M_PI * (double)i * ((double)j + 0.5) / (double)num_mel_filters);
        }
        mfcc_out[i] = sum;
    }

    nimcp_free(windowed);
    nimcp_free(buf);
    nimcp_free(power_spec);
    nimcp_free(mel_energies);
    engine->stats.ffts_computed++;
    engine->stats.samples_processed += length;
}
