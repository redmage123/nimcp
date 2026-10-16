/**
 * @file nimcp_dsp.h
 * @brief Digital Signal Processing — FFT pipeline, filters, spectral analysis, wavelets
 *
 * WHAT: Windowed STFT, FIR/IIR filters, power spectral density, wavelets,
 *       Hilbert transform, auto/cross-correlation, adaptive filters.
 * WHY:  Audio perception, SNN spike train analysis, physiological signal
 *       processing (ECG, EEG), speech understanding, world model acoustics.
 * HOW:  Cooley-Tukey FFT, Butterworth/Chebyshev filter design, Welch PSD,
 *       Haar/Daubechies DWT, LMS/RLS adaptive filtering.
 */

#ifndef NIMCP_DSP_H
#define NIMCP_DSP_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define DSP_MAX_FFT_SIZE        8192
#define DSP_MAX_FILTER_ORDER    128
#define DSP_MAX_WAVELET_LEVELS  12
#define DSP_MAX_CHANNELS        8

/* ============================================================================
 * Complex Type (double precision for FFT accuracy)
 * ============================================================================ */

typedef struct {
    double re;
    double im;
} dsp_complex_t;

/* ============================================================================
 * Window Functions
 * ============================================================================ */

typedef enum {
    DSP_WINDOW_RECTANGULAR = 0,
    DSP_WINDOW_HAMMING     = 1,
    DSP_WINDOW_HANNING     = 2,     /* von Hann */
    DSP_WINDOW_BLACKMAN    = 3,
    DSP_WINDOW_KAISER      = 4,
    DSP_WINDOW_BARTLETT    = 5,     /* triangular */
    DSP_WINDOW_FLAT_TOP    = 6,
    DSP_WINDOW_COUNT
} dsp_window_type_t;

/* ============================================================================
 * Filter Types
 * ============================================================================ */

typedef enum {
    DSP_FILTER_LOWPASS  = 0,
    DSP_FILTER_HIGHPASS = 1,
    DSP_FILTER_BANDPASS = 2,
    DSP_FILTER_BANDSTOP = 3,    /* notch */
} dsp_filter_band_t;

typedef enum {
    DSP_FILTER_FIR          = 0,
    DSP_FILTER_BUTTERWORTH  = 1,
    DSP_FILTER_CHEBYSHEV1   = 2,    /* ripple in passband */
    DSP_FILTER_CHEBYSHEV2   = 3,    /* ripple in stopband */
    DSP_FILTER_BESSEL       = 4,    /* maximally flat group delay */
} dsp_filter_type_t;

typedef struct {
    dsp_filter_type_t   type;
    dsp_filter_band_t   band;
    uint32_t            order;          /* filter order */
    double              cutoff_low;     /* Hz (normalized: 0 to Nyquist) */
    double              cutoff_high;    /* Hz (for bandpass/bandstop) */
    double              sample_rate;    /* Hz */
    double              ripple_db;      /* passband ripple (Chebyshev) */
    /* Coefficients */
    double              b[DSP_MAX_FILTER_ORDER + 1]; /* numerator (FIR/IIR) */
    double              a[DSP_MAX_FILTER_ORDER + 1]; /* denominator (IIR only, a[0]=1) */
    uint32_t            num_b;
    uint32_t            num_a;
    /* State (for real-time filtering) */
    double              state[DSP_MAX_FILTER_ORDER + 1]; /* delay line */
} dsp_filter_t;

/* ============================================================================
 * STFT (Short-Time Fourier Transform)
 * ============================================================================ */

typedef struct {
    dsp_complex_t*  frames;         /* [num_frames × fft_size] */
    double*         magnitudes;     /* [num_frames × fft_size/2+1] */
    double*         phases;         /* [num_frames × fft_size/2+1] */
    double*         power;          /* |X|² */
    uint32_t        num_frames;
    uint32_t        fft_size;
    uint32_t        hop_size;
    uint32_t        num_bins;       /* fft_size/2 + 1 */
    double          sample_rate;
    double          freq_resolution;/* sample_rate / fft_size */
    double          time_resolution;/* hop_size / sample_rate */
} dsp_stft_result_t;

/* ============================================================================
 * Power Spectral Density (Welch method)
 * ============================================================================ */

typedef struct {
    double*     psd;                /* [num_bins] power/Hz */
    double*     frequencies;        /* [num_bins] Hz */
    uint32_t    num_bins;
    double      df;                 /* frequency resolution */
    double      total_power;        /* integral of PSD */
    double      peak_frequency;     /* frequency of maximum PSD */
    double      peak_power;
} dsp_psd_result_t;

/* ============================================================================
 * Wavelet Types
 * ============================================================================ */

typedef enum {
    DSP_WAVELET_HAAR        = 0,
    DSP_WAVELET_DB2         = 1,    /* Daubechies-2 */
    DSP_WAVELET_DB4         = 2,    /* Daubechies-4 */
    DSP_WAVELET_DB8         = 3,    /* Daubechies-8 */
    DSP_WAVELET_MORLET      = 4,    /* for CWT */
    DSP_WAVELET_MEXICAN_HAT = 5,    /* for CWT */
} dsp_wavelet_type_t;

typedef struct {
    double*     approx;             /* approximation coefficients (low freq) */
    double*     detail;             /* detail coefficients (high freq) per level */
    uint32_t*   level_sizes;        /* size of each level */
    uint32_t    num_levels;
    uint32_t    total_coeffs;
    dsp_wavelet_type_t wavelet;
} dsp_dwt_result_t;

/* ============================================================================
 * Correlation
 * ============================================================================ */

typedef struct {
    double*     values;             /* correlation values */
    int32_t*    lags;               /* lag indices */
    uint32_t    length;
    double      peak_value;
    int32_t     peak_lag;
} dsp_correlation_t;

/* ============================================================================
 * Adaptive Filter
 * ============================================================================ */

typedef enum {
    DSP_ADAPTIVE_LMS    = 0,    /* Least Mean Squares */
    DSP_ADAPTIVE_NLMS   = 1,    /* Normalized LMS */
    DSP_ADAPTIVE_RLS    = 2,    /* Recursive Least Squares */
} dsp_adaptive_type_t;

typedef struct {
    dsp_adaptive_type_t type;
    double*     weights;            /* [order] adaptive filter taps */
    double*     buffer;             /* [order] input delay line */
    uint32_t    order;
    double      mu;                 /* step size (LMS) */
    double      lambda;             /* forgetting factor (RLS) */
    double*     P;                  /* [order × order] inverse correlation (RLS) */
    double      error;              /* last prediction error */
    uint64_t    samples_processed;
} dsp_adaptive_filter_t;

/* ============================================================================
 * Hilbert Transform / Analytic Signal
 * ============================================================================ */

typedef struct {
    double*     envelope;           /* instantaneous amplitude */
    double*     inst_phase;         /* instantaneous phase (radians) */
    double*     inst_freq;          /* instantaneous frequency (Hz) */
    uint32_t    length;
} dsp_analytic_signal_t;

/* ============================================================================
 * Config
 * ============================================================================ */

typedef struct {
    uint32_t    default_fft_size;
    uint32_t    default_hop_size;
    dsp_window_type_t default_window;
    double      default_sample_rate;
} dsp_config_t;

/* ============================================================================
 * Stats
 * ============================================================================ */

typedef struct {
    uint64_t    ffts_computed;
    uint64_t    filters_applied;
    uint64_t    wavelets_computed;
    uint64_t    samples_processed;
} dsp_stats_t;

/* ============================================================================
 * Engine
 * ============================================================================ */

typedef struct dsp_engine {
    dsp_config_t    config;
    dsp_stats_t     stats;
    /* Scratch buffers (pre-allocated to avoid per-call alloc) */
    dsp_complex_t*  fft_scratch;    /* [DSP_MAX_FFT_SIZE] */
    double*         window_scratch; /* [DSP_MAX_FFT_SIZE] */
    bool            initialized;
} dsp_engine_t;

/* ============================================================================
 * API
 * ============================================================================ */

dsp_engine_t* dsp_create(const dsp_config_t* config);
void dsp_destroy(dsp_engine_t* engine);

/* === Window Functions === */

/** Generate window function coefficients */
void dsp_window(double* out, uint32_t length, dsp_window_type_t type);

/** Apply window to signal (in-place) */
void dsp_apply_window(double* signal, uint32_t length, dsp_window_type_t type);

/* === FFT === */

/** Forward FFT (complex-to-complex, in-place, power-of-2) */
void dsp_fft(dsp_complex_t* data, uint32_t n);

/** Inverse FFT */
void dsp_ifft(dsp_complex_t* data, uint32_t n);

/** Real-to-complex FFT (input: real signal, output: complex spectrum) */
void dsp_rfft(const double* signal, uint32_t n, dsp_complex_t* spectrum);

/** STFT: windowed overlapping FFT */
dsp_stft_result_t* dsp_stft(dsp_engine_t* engine, const double* signal,
                              uint32_t length, uint32_t fft_size,
                              uint32_t hop_size, dsp_window_type_t window);

/** Inverse STFT (overlap-add reconstruction) */
double* dsp_istft(dsp_engine_t* engine, const dsp_stft_result_t* stft,
                    uint32_t* out_length);

/** Free STFT result */
void dsp_stft_free(dsp_stft_result_t* result);

/* === Spectral Analysis === */

/** Welch PSD estimate */
dsp_psd_result_t* dsp_welch_psd(dsp_engine_t* engine, const double* signal,
                                  uint32_t length, double sample_rate,
                                  uint32_t segment_size, uint32_t overlap);

/** Free PSD result */
void dsp_psd_free(dsp_psd_result_t* result);

/** Spectrogram (magnitude STFT) — returns power[time × freq] */
double* dsp_spectrogram(dsp_engine_t* engine, const double* signal,
                          uint32_t length, uint32_t fft_size,
                          uint32_t hop_size, uint32_t* num_frames,
                          uint32_t* num_bins);

/** Cross-spectral density between two signals */
dsp_complex_t* dsp_cross_psd(dsp_engine_t* engine, const double* x,
                                const double* y, uint32_t length,
                                uint32_t segment_size);

/** Coherence between two signals: |Pxy|² / (Pxx × Pyy) */
double* dsp_coherence(dsp_engine_t* engine, const double* x, const double* y,
                        uint32_t length, uint32_t segment_size,
                        uint32_t* num_bins);

/** Cepstrum (inverse FFT of log spectrum) */
double* dsp_cepstrum(dsp_engine_t* engine, const double* signal,
                       uint32_t length, uint32_t* cepstrum_length);

/* === Filters === */

/** Design a filter (compute coefficients) */
dsp_filter_t dsp_design_filter(dsp_filter_type_t type, dsp_filter_band_t band,
                                 uint32_t order, double cutoff_low,
                                 double cutoff_high, double sample_rate);

/** Apply filter to signal (offline, full signal) */
void dsp_filter_apply(const dsp_filter_t* filter, const double* input,
                        double* output, uint32_t length);

/** Apply filter sample-by-sample (real-time, maintains state) */
double dsp_filter_tick(dsp_filter_t* filter, double input_sample);

/** Reset filter state */
void dsp_filter_reset(dsp_filter_t* filter);

/** Compute filter frequency response at N points */
void dsp_filter_freqz(const dsp_filter_t* filter, double* magnitude,
                        double* phase, uint32_t num_points);

/* === Correlation === */

/** Autocorrelation */
dsp_correlation_t* dsp_autocorrelation(const double* signal, uint32_t length,
                                         uint32_t max_lag);

/** Cross-correlation */
dsp_correlation_t* dsp_crosscorrelation(const double* x, const double* y,
                                          uint32_t length, uint32_t max_lag);

/** Free correlation result */
void dsp_correlation_free(dsp_correlation_t* result);

/* === Wavelets === */

/** Discrete Wavelet Transform (DWT) — multi-level decomposition */
dsp_dwt_result_t* dsp_dwt(const double* signal, uint32_t length,
                             dsp_wavelet_type_t wavelet, uint32_t levels);

/** Inverse DWT (reconstruction) */
double* dsp_idwt(const dsp_dwt_result_t* dwt, uint32_t* out_length);

/** Free DWT result */
void dsp_dwt_free(dsp_dwt_result_t* result);

/** Get wavelet filter coefficients */
void dsp_wavelet_coeffs(dsp_wavelet_type_t wavelet, const double** lo,
                          const double** hi, uint32_t* length);

/* === Hilbert Transform === */

/** Compute analytic signal via Hilbert transform */
dsp_analytic_signal_t* dsp_hilbert(dsp_engine_t* engine, const double* signal,
                                     uint32_t length, double sample_rate);

/** Free analytic signal */
void dsp_analytic_free(dsp_analytic_signal_t* result);

/* === Adaptive Filters === */

/** Create adaptive filter */
dsp_adaptive_filter_t* dsp_adaptive_create(dsp_adaptive_type_t type,
                                              uint32_t order, double mu);

/** Destroy adaptive filter */
void dsp_adaptive_destroy(dsp_adaptive_filter_t* filter);

/** Process one sample: predict, compute error, update weights */
double dsp_adaptive_tick(dsp_adaptive_filter_t* filter,
                           double input, double desired);

/** Get current filter weights */
const double* dsp_adaptive_get_weights(const dsp_adaptive_filter_t* filter);

/* === Utility === */

/** Convert linear magnitude to decibels */
double dsp_mag_to_db(double magnitude);

/** Convert decibels to linear magnitude */
double dsp_db_to_mag(double db);

/** Compute RMS of signal */
double dsp_rms(const double* signal, uint32_t length);

/** Zero-crossing rate */
double dsp_zero_crossing_rate(const double* signal, uint32_t length);

/** Mel frequency from Hz: mel = 2595 × log10(1 + f/700) */
double dsp_hz_to_mel(double hz);

/** Hz from mel frequency */
double dsp_mel_to_hz(double mel);

/** Mel filterbank (for audio cortex features) */
void dsp_mel_filterbank(const double* spectrum, uint32_t num_bins,
                          double sample_rate, double* mel_energies,
                          uint32_t num_mel_filters);

/** MFCC (Mel-Frequency Cepstral Coefficients) */
void dsp_mfcc(dsp_engine_t* engine, const double* signal, uint32_t length,
                double sample_rate, double* mfcc_out, uint32_t num_coeffs);

/** Default config */
dsp_config_t dsp_default_config(void);

/** Get stats */
dsp_stats_t dsp_get_stats(const dsp_engine_t* engine);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DSP_H */
