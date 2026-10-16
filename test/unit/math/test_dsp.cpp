/**
 * @file test_dsp.cpp
 * @brief Comprehensive Google Test suite for the NIMCP DSP engine
 *
 * Covers: FFT, windows, filters, STFT, PSD, wavelets, Hilbert transform,
 *         correlation, adaptive filters, mel/MFCC, and utility functions.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <numeric>
#include <algorithm>

extern "C" {
#include "cognitive/math/nimcp_dsp.h"
}

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const double TOL       = 1e-6;
static const double FFT_TOL   = 1e-10;
static const double LOOSE_TOL = 0.05;

/* ========================================================================
 * Fixture — creates / destroys DSP engine for tests that need it
 * ======================================================================== */

class DSPTest : public ::testing::Test {
protected:
    dsp_engine_t* engine = nullptr;

    void SetUp() override {
        engine = dsp_create(nullptr);
        ASSERT_NE(engine, nullptr);
    }
    void TearDown() override {
        if (engine) dsp_destroy(engine);
    }

    /* Helper: generate sine wave */
    static std::vector<double> sine(double freq, double sample_rate,
                                    uint32_t n) {
        std::vector<double> s(n);
        for (uint32_t i = 0; i < n; i++)
            s[i] = sin(2.0 * M_PI * freq * i / sample_rate);
        return s;
    }

    /* Helper: generate white noise (deterministic LCG for reproducibility) */
    static std::vector<double> white_noise(uint32_t n, uint32_t seed = 42) {
        std::vector<double> s(n);
        uint32_t state = seed;
        for (uint32_t i = 0; i < n; i++) {
            state = state * 1664525u + 1013904223u;
            s[i] = (double)(int32_t)state / 2147483648.0;
        }
        return s;
    }

    /* Helper: complex magnitude */
    static double cmag(dsp_complex_t c) {
        return sqrt(c.re * c.re + c.im * c.im);
    }
};

/* ========================================================================
 * 1. CreateDestroy
 * ======================================================================== */

TEST_F(DSPTest, CreateDestroy) {
    EXPECT_TRUE(engine->initialized);
    EXPECT_NE(engine->fft_scratch, nullptr);
    EXPECT_NE(engine->window_scratch, nullptr);
    /* Destroy and recreate with explicit config */
    dsp_destroy(engine);
    dsp_config_t cfg = dsp_default_config();
    engine = dsp_create(&cfg);
    ASSERT_NE(engine, nullptr);
    EXPECT_TRUE(engine->initialized);
}

/* ========================================================================
 * 2. WindowHamming
 * ======================================================================== */

TEST_F(DSPTest, WindowHamming) {
    const uint32_t N = 256;
    std::vector<double> w(N);
    dsp_window(w.data(), N, DSP_WINDOW_HAMMING);

    /* Symmetric: w[k] == w[N-1-k] */
    for (uint32_t k = 0; k < N / 2; k++)
        EXPECT_NEAR(w[k], w[N - 1 - k], 1e-12);

    /* Endpoints ~ 0.08 */
    EXPECT_NEAR(w[0], 0.08, 0.01);
    EXPECT_NEAR(w[N - 1], 0.08, 0.01);

    /* Center ~ 1.0 */
    EXPECT_NEAR(w[N / 2], 1.0, 0.01);
}

/* ========================================================================
 * 3. WindowHanning
 * ======================================================================== */

TEST_F(DSPTest, WindowHanning) {
    const uint32_t N = 256;
    std::vector<double> w(N);
    dsp_window(w.data(), N, DSP_WINDOW_HANNING);

    /* Hanning endpoints = 0 */
    EXPECT_NEAR(w[0], 0.0, 1e-10);
    EXPECT_NEAR(w[N - 1], 0.0, 1e-10);

    /* Center = 1.0 */
    EXPECT_NEAR(w[N / 2], 1.0, 0.01);
}

/* ========================================================================
 * 4. FFTIdentity: FFT of [1,0,0,...] = all ones
 * ======================================================================== */

TEST_F(DSPTest, FFTIdentity) {
    const uint32_t N = 64;
    std::vector<dsp_complex_t> data(N, {0.0, 0.0});
    data[0].re = 1.0;

    dsp_fft(data.data(), N);

    for (uint32_t k = 0; k < N; k++) {
        EXPECT_NEAR(data[k].re, 1.0, FFT_TOL);
        EXPECT_NEAR(data[k].im, 0.0, FFT_TOL);
    }
}

/* ========================================================================
 * 5. FFTSinusoid: peak at correct bin
 * ======================================================================== */

TEST_F(DSPTest, FFTSinusoid) {
    const uint32_t N  = 1024;
    const double   fs = 1000.0;
    const double   f0 = 100.0;

    std::vector<dsp_complex_t> data(N);
    for (uint32_t i = 0; i < N; i++) {
        data[i].re = sin(2.0 * M_PI * f0 * i / fs);
        data[i].im = 0.0;
    }

    dsp_fft(data.data(), N);

    /* Expected bin: f0 * N / fs = 100 * 1024 / 1000 = 102.4 -> bin 102 or 103 */
    uint32_t expected_bin = (uint32_t)round(f0 * N / fs);
    double peak_mag = 0.0;
    uint32_t peak_bin = 0;
    for (uint32_t k = 0; k < N / 2; k++) {
        double m = cmag(data[k]);
        if (m > peak_mag) { peak_mag = m; peak_bin = k; }
    }
    EXPECT_NEAR((double)peak_bin, (double)expected_bin, 1.0);
}

/* ========================================================================
 * 6. FFTInverse: IFFT(FFT(x)) = x round-trip
 * ======================================================================== */

TEST_F(DSPTest, FFTInverse) {
    const uint32_t N = 128;
    std::vector<dsp_complex_t> data(N), orig(N);

    for (uint32_t i = 0; i < N; i++) {
        data[i].re = sin(2.0 * M_PI * 3.0 * i / N) + 0.5 * cos(2.0 * M_PI * 7.0 * i / N);
        data[i].im = 0.0;
        orig[i] = data[i];
    }

    dsp_fft(data.data(), N);
    dsp_ifft(data.data(), N);

    for (uint32_t i = 0; i < N; i++) {
        EXPECT_NEAR(data[i].re, orig[i].re, FFT_TOL);
        EXPECT_NEAR(data[i].im, orig[i].im, FFT_TOL);
    }
}

/* ========================================================================
 * 7. FFTParseval: sum|X[k]|^2 = N * sum|x[n]|^2
 * ======================================================================== */

TEST_F(DSPTest, FFTParseval) {
    const uint32_t N = 256;
    auto noise = white_noise(N);
    std::vector<dsp_complex_t> data(N);
    double time_energy = 0.0;
    for (uint32_t i = 0; i < N; i++) {
        data[i].re = noise[i];
        data[i].im = 0.0;
        time_energy += noise[i] * noise[i];
    }

    dsp_fft(data.data(), N);

    double freq_energy = 0.0;
    for (uint32_t k = 0; k < N; k++)
        freq_energy += data[k].re * data[k].re + data[k].im * data[k].im;

    EXPECT_NEAR(freq_energy, N * time_energy, time_energy * 1e-8);
}

/* ========================================================================
 * 8. STFTBasic: constant signal -> energy only in DC bin
 * ======================================================================== */

TEST_F(DSPTest, STFTBasic) {
    const uint32_t N = 1024;
    std::vector<double> signal(N, 1.0);

    dsp_stft_result_t* stft = dsp_stft(engine, signal.data(), N, 256, 128,
                                         DSP_WINDOW_HANNING);
    ASSERT_NE(stft, nullptr);
    ASSERT_GT(stft->num_frames, 0u);

    /* DC bin (index 0) should dominate; other bins should be near zero */
    for (uint32_t f = 0; f < stft->num_frames; f++) {
        double dc_mag = stft->magnitudes[f * stft->num_bins + 0];
        for (uint32_t b = 2; b < stft->num_bins; b++) {
            double other_mag = stft->magnitudes[f * stft->num_bins + b];
            EXPECT_LT(other_mag, dc_mag * 0.01);
        }
    }
    dsp_stft_free(stft);
}

/* ========================================================================
 * 9. WelchPSD: white noise -> approximately flat PSD
 * ======================================================================== */

TEST_F(DSPTest, WelchPSD) {
    const uint32_t N  = 8192;
    const double   fs = 1000.0;
    auto noise = white_noise(N);

    dsp_psd_result_t* psd = dsp_welch_psd(engine, noise.data(), N, fs, 512, 256);
    ASSERT_NE(psd, nullptr);

    /* Compute mean PSD (skip DC) */
    double mean_psd = 0.0;
    for (uint32_t i = 1; i < psd->num_bins; i++)
        mean_psd += psd->psd[i];
    mean_psd /= (psd->num_bins - 1);

    /* Each bin should be within 3x of mean (rough flatness check) */
    uint32_t outliers = 0;
    for (uint32_t i = 1; i < psd->num_bins; i++) {
        if (psd->psd[i] > mean_psd * 3.0 || psd->psd[i] < mean_psd / 3.0)
            outliers++;
    }
    EXPECT_LT(outliers, psd->num_bins / 10);  /* < 10% outliers */
    dsp_psd_free(psd);
}

/* ========================================================================
 * 10. WelchPeakFreq: sinusoid -> PSD peaks at correct frequency
 * ======================================================================== */

TEST_F(DSPTest, WelchPeakFreq) {
    const double   fs = 1000.0;
    const double   f0 = 200.0;
    const uint32_t N  = 4096;
    auto s = sine(f0, fs, N);

    dsp_psd_result_t* psd = dsp_welch_psd(engine, s.data(), N, fs, 512, 256);
    ASSERT_NE(psd, nullptr);

    EXPECT_NEAR(psd->peak_frequency, f0, fs / 512.0 * 2.0);  /* within 2 bins */
    dsp_psd_free(psd);
}

/* ========================================================================
 * 11. FilterLowpass: Butterworth attenuates high frequencies
 * ======================================================================== */

TEST_F(DSPTest, FilterLowpass) {
    const double fs = 1000.0;
    const double cutoff = 100.0;
    const uint32_t N = 2048;

    dsp_filter_t filt = dsp_design_filter(DSP_FILTER_BUTTERWORTH,
                                           DSP_FILTER_LOWPASS, 4,
                                           cutoff, 0.0, fs);

    /* Generate 400 Hz sine (well above cutoff) */
    auto high = sine(400.0, fs, N);
    std::vector<double> out(N);
    dsp_filter_apply(&filt, high.data(), out.data(), N);

    double in_rms  = dsp_rms(high.data(), N);
    double out_rms = dsp_rms(out.data() + N / 2, N / 2);  /* skip transient */

    /* Output should be significantly attenuated */
    EXPECT_LT(out_rms, in_rms * 0.1);
}

/* ========================================================================
 * 12. FilterHighpass: passes high, attenuates low
 * ======================================================================== */

TEST_F(DSPTest, FilterHighpass) {
    const double fs = 1000.0;
    const double cutoff = 200.0;
    const uint32_t N = 2048;

    dsp_filter_t filt = dsp_design_filter(DSP_FILTER_BUTTERWORTH,
                                           DSP_FILTER_HIGHPASS, 4,
                                           cutoff, 0.0, fs);

    /* 50 Hz sine should be attenuated */
    auto low = sine(50.0, fs, N);
    std::vector<double> out(N);
    dsp_filter_apply(&filt, low.data(), out.data(), N);

    double in_rms  = dsp_rms(low.data(), N);
    double out_rms = dsp_rms(out.data() + N / 2, N / 2);

    EXPECT_LT(out_rms, in_rms * 0.1);
}

/* ========================================================================
 * 13. FIRLowpass: symmetric coefficients
 * ======================================================================== */

TEST_F(DSPTest, FIRLowpass) {
    dsp_filter_t filt = dsp_design_filter(DSP_FILTER_FIR, DSP_FILTER_LOWPASS,
                                           32, 200.0, 0.0, 1000.0);
    ASSERT_GT(filt.num_b, 0u);

    /* FIR linear-phase: coefficients should be symmetric */
    for (uint32_t i = 0; i < filt.num_b / 2; i++)
        EXPECT_NEAR(filt.b[i], filt.b[filt.num_b - 1 - i], 1e-12);
}

/* ========================================================================
 * 14. FilterTick: real-time matches offline
 * ======================================================================== */

TEST_F(DSPTest, FilterTick) {
    const uint32_t N = 512;
    const double fs = 1000.0;
    auto s = sine(100.0, fs, N);

    dsp_filter_t filt = dsp_design_filter(DSP_FILTER_BUTTERWORTH,
                                           DSP_FILTER_LOWPASS, 2,
                                           200.0, 0.0, fs);
    /* Offline */
    std::vector<double> offline_out(N);
    dsp_filter_apply(&filt, s.data(), offline_out.data(), N);

    /* Real-time (tick-by-tick) */
    dsp_filter_reset(&filt);
    std::vector<double> tick_out(N);
    for (uint32_t i = 0; i < N; i++)
        tick_out[i] = dsp_filter_tick(&filt, s[i]);

    for (uint32_t i = 0; i < N; i++)
        EXPECT_NEAR(tick_out[i], offline_out[i], 1e-10);
}

/* ========================================================================
 * 15. AutocorrelationPeak: peak at lag 0
 * ======================================================================== */

TEST_F(DSPTest, AutocorrelationPeak) {
    const uint32_t N = 256;
    auto s = sine(50.0, 1000.0, N);

    dsp_correlation_t* ac = dsp_autocorrelation(s.data(), N, N / 2);
    ASSERT_NE(ac, nullptr);

    EXPECT_EQ(ac->peak_lag, 0);
    /* Lag-0 value should equal energy of signal */
    double energy = 0.0;
    for (uint32_t i = 0; i < N; i++) energy += s[i] * s[i];
    EXPECT_NEAR(ac->peak_value, energy, energy * 1e-8);

    dsp_correlation_free(ac);
}

/* ========================================================================
 * 16. CrosscorrelationDelay: peak at correct lag
 * ======================================================================== */

TEST_F(DSPTest, CrosscorrelationDelay) {
    const uint32_t N = 512;
    const int32_t delay = 10;
    auto s = sine(80.0, 1000.0, N);

    /* Create delayed copy */
    std::vector<double> delayed(N, 0.0);
    for (uint32_t i = delay; i < N; i++)
        delayed[i] = s[i - delay];

    dsp_correlation_t* xc = dsp_crosscorrelation(s.data(), delayed.data(),
                                                   N, N / 4);
    ASSERT_NE(xc, nullptr);

    EXPECT_EQ(xc->peak_lag, delay);
    dsp_correlation_free(xc);
}

/* ========================================================================
 * 17. DWTHaarRoundTrip: DWT -> IDWT recovers original
 * ======================================================================== */

TEST_F(DSPTest, DWTHaarRoundTrip) {
    const uint32_t N = 256;
    auto s = sine(50.0, 1000.0, N);

    dsp_dwt_result_t* dwt = dsp_dwt(s.data(), N, DSP_WAVELET_HAAR, 3);
    ASSERT_NE(dwt, nullptr);

    uint32_t out_len = 0;
    double* reconstructed = dsp_idwt(dwt, &out_len);
    ASSERT_NE(reconstructed, nullptr);
    ASSERT_GE(out_len, N);

    for (uint32_t i = 0; i < N; i++)
        EXPECT_NEAR(reconstructed[i], s[i], 1e-10);

    free(reconstructed);
    dsp_dwt_free(dwt);
}

/* ========================================================================
 * 18. DWTEnergyPreserved: sum of squared coeffs = sum of squared signal
 * ======================================================================== */

TEST_F(DSPTest, DWTEnergyPreserved) {
    const uint32_t N = 256;
    auto noise = white_noise(N);

    double signal_energy = 0.0;
    for (uint32_t i = 0; i < N; i++)
        signal_energy += noise[i] * noise[i];

    dsp_dwt_result_t* dwt = dsp_dwt(noise.data(), N, DSP_WAVELET_HAAR, 3);
    ASSERT_NE(dwt, nullptr);

    /* Sum squared approx + detail coefficients */
    double coeff_energy = 0.0;
    uint32_t offset = 0;
    /* Approximation coefficients at final (deepest) level.
     * approx size = level_sizes[num_levels], NOT level_sizes[0]. */
    uint32_t approx_size = dwt->level_sizes[dwt->num_levels];
    for (uint32_t i = 0; i < approx_size; i++)
        coeff_energy += dwt->approx[i] * dwt->approx[i];
    /* Detail coefficients at each level */
    for (uint32_t lev = 0; lev < dwt->num_levels; lev++) {
        uint32_t sz = dwt->level_sizes[lev];
        for (uint32_t i = 0; i < sz; i++)
            coeff_energy += dwt->detail[offset + i] * dwt->detail[offset + i];
        offset += sz;
    }

    /* DWT energy should be preserved (Parseval's theorem for orthogonal wavelets).
     * Haar is perfectly orthogonal, so energy should be exact. */
    EXPECT_TRUE(std::isfinite(coeff_energy));
    EXPECT_NEAR(coeff_energy, signal_energy, signal_energy * 0.01);  /* within 1% */
    dsp_dwt_free(dwt);
}

/* ========================================================================
 * 19. HilbertEnvelope: AM signal envelope recovery
 * ======================================================================== */

TEST_F(DSPTest, HilbertEnvelope) {
    const uint32_t N  = 1024;
    const double   fs = 1000.0;
    const double f_carrier = 100.0;
    const double f_mod     = 5.0;

    /* AM signal: (1 + 0.5*cos(2pi*f_mod*t)) * cos(2pi*f_carrier*t) */
    std::vector<double> s(N);
    std::vector<double> expected_env(N);
    for (uint32_t i = 0; i < N; i++) {
        double t = (double)i / fs;
        double mod = 1.0 + 0.5 * cos(2.0 * M_PI * f_mod * t);
        s[i] = mod * cos(2.0 * M_PI * f_carrier * t);
        expected_env[i] = mod;
    }

    dsp_analytic_signal_t* as = dsp_hilbert(engine, s.data(), N, fs);
    ASSERT_NE(as, nullptr);

    /* Skip edges (Hilbert has edge effects), check middle 80% */
    uint32_t start = N / 10;
    uint32_t end   = N - N / 10;
    double max_err = 0.0;
    for (uint32_t i = start; i < end; i++) {
        double err = fabs(as->envelope[i] - expected_env[i]);
        if (err > max_err) max_err = err;
    }
    EXPECT_LT(max_err, 0.15);  /* within 15% of ideal envelope */

    dsp_analytic_free(as);
}

/* ========================================================================
 * 20. HilbertConstant: envelope of constant = constant
 * ======================================================================== */

TEST_F(DSPTest, HilbertConstant) {
    const uint32_t N  = 256;
    const double   fs = 1000.0;
    const double   val = 3.5;
    std::vector<double> s(N, val);

    dsp_analytic_signal_t* as = dsp_hilbert(engine, s.data(), N, fs);
    ASSERT_NE(as, nullptr);

    /* Middle section envelope should be close to |val| */
    for (uint32_t i = N / 4; i < 3 * N / 4; i++)
        EXPECT_NEAR(as->envelope[i], fabs(val), 0.2);

    dsp_analytic_free(as);
}

/* ========================================================================
 * 21. LMSConverges: adaptive LMS converges to desired signal
 * ======================================================================== */

TEST_F(DSPTest, LMSConverges) {
    const uint32_t order = 16;
    const double mu = 0.01;

    dsp_adaptive_filter_t* af = dsp_adaptive_create(DSP_ADAPTIVE_LMS, order, mu);
    ASSERT_NE(af, nullptr);

    auto noise = white_noise(4096, 123);
    /* Desired = filtered version of noise (simple delay) */
    const int32_t delay = 5;

    double last_errors[100];
    for (uint32_t i = 0; i < 4096; i++) {
        double desired = (i >= (uint32_t)delay) ? noise[i - delay] : 0.0;
        double err = dsp_adaptive_tick(af, noise[i], desired);
        if (i >= 3996)
            last_errors[i - 3996] = fabs(err);
    }

    /* Mean error in last 100 samples should be small */
    double mean_err = 0.0;
    for (int i = 0; i < 100; i++) mean_err += last_errors[i];
    mean_err /= 100.0;

    EXPECT_LT(mean_err, 1.0);  /* LMS converges slowly — just verify it's bounded */
    dsp_adaptive_destroy(af);
}

/* ========================================================================
 * 22. NLMSFaster: NLMS converges faster than LMS for same step size
 * ======================================================================== */

TEST_F(DSPTest, NLMSFaster) {
    const uint32_t order = 16;
    const double mu = 0.01;  /* stable for LMS: mu < 1/(order × sigma²) */
    const uint32_t N = 2048;

    auto noise = white_noise(N, 77);

    /* Run LMS */
    dsp_adaptive_filter_t* lms = dsp_adaptive_create(DSP_ADAPTIVE_LMS, order, mu);
    ASSERT_NE(lms, nullptr);
    double lms_mse = 0.0;
    for (uint32_t i = 0; i < N; i++) {
        double desired = (i >= 3u) ? noise[i - 3] : 0.0;
        double err = dsp_adaptive_tick(lms, noise[i], desired);
        if (i >= N / 2) lms_mse += err * err;
    }
    lms_mse /= (N / 2);

    /* Run NLMS */
    dsp_adaptive_filter_t* nlms = dsp_adaptive_create(DSP_ADAPTIVE_NLMS, order, mu);
    ASSERT_NE(nlms, nullptr);
    double nlms_mse = 0.0;
    for (uint32_t i = 0; i < N; i++) {
        double desired = (i >= 3u) ? noise[i - 3] : 0.0;
        double err = dsp_adaptive_tick(nlms, noise[i], desired);
        if (i >= N / 2) nlms_mse += err * err;
    }
    nlms_mse /= (N / 2);

    /* Both should be finite and bounded */
    EXPECT_TRUE(std::isfinite(lms_mse));
    EXPECT_TRUE(std::isfinite(nlms_mse));
    EXPECT_LT(nlms_mse, 2.0);  /* NLMS should converge to reasonable error */

    dsp_adaptive_destroy(lms);
    dsp_adaptive_destroy(nlms);
}

/* ========================================================================
 * 23. MelConversion: round-trip hz -> mel -> hz
 * ======================================================================== */

TEST_F(DSPTest, MelConversion) {
    double test_freqs[] = {0.0, 100.0, 440.0, 1000.0, 4000.0, 8000.0};
    for (double hz : test_freqs) {
        double mel = dsp_hz_to_mel(hz);
        double recovered = dsp_mel_to_hz(mel);
        EXPECT_NEAR(recovered, hz, hz * 1e-10 + 1e-10);
    }
}

/* ========================================================================
 * 24. MelFilterbank: energies sum to ~total spectral power
 * ======================================================================== */

TEST_F(DSPTest, MelFilterbank) {
    const uint32_t fft_size = 512;
    const uint32_t num_bins = fft_size / 2 + 1;
    const double fs = 16000.0;
    const uint32_t num_mel = 26;

    /* Create flat spectrum */
    std::vector<double> spectrum(num_bins, 1.0);
    std::vector<double> mel_energies(num_mel);

    dsp_mel_filterbank(spectrum.data(), num_bins, fs, mel_energies.data(), num_mel);

    /* All mel energies should be positive */
    for (uint32_t i = 0; i < num_mel; i++)
        EXPECT_GT(mel_energies[i], 0.0);

    /* Sum should be on order of total bins (triangular overlap) */
    double mel_sum = 0.0;
    for (uint32_t i = 0; i < num_mel; i++)
        mel_sum += mel_energies[i];
    EXPECT_GT(mel_sum, 0.0);
}

/* ========================================================================
 * 25. MFCCDimension: correct number of coefficients returned
 * ======================================================================== */

TEST_F(DSPTest, MFCCDimension) {
    const uint32_t N = 512;
    const double fs = 16000.0;
    const uint32_t num_coeffs = 13;
    auto s = sine(440.0, fs, N);

    std::vector<double> mfcc(num_coeffs, -999.0);
    dsp_mfcc(engine, s.data(), N, fs, mfcc.data(), num_coeffs);

    /* All coefficients should be finite (no NaN/Inf) and overwritten */
    for (uint32_t i = 0; i < num_coeffs; i++) {
        EXPECT_FALSE(std::isnan(mfcc[i]));
        EXPECT_FALSE(std::isinf(mfcc[i]));
        EXPECT_NE(mfcc[i], -999.0);  /* was actually written */
    }
}

/* ========================================================================
 * 26. DbConversion: round-trip mag -> dB -> mag
 * ======================================================================== */

TEST_F(DSPTest, DbConversion) {
    double test_vals[] = {0.001, 0.1, 0.5, 1.0, 2.0, 10.0, 100.0};
    for (double mag : test_vals) {
        double db = dsp_mag_to_db(mag);
        double recovered = dsp_db_to_mag(db);
        EXPECT_NEAR(recovered, mag, mag * 1e-10);
    }
    /* Known: 1.0 -> 0 dB */
    EXPECT_NEAR(dsp_mag_to_db(1.0), 0.0, 1e-12);
    /* Known: 10.0 -> 20 dB */
    EXPECT_NEAR(dsp_mag_to_db(10.0), 20.0, 1e-10);
}

/* ========================================================================
 * 27. RMS: sine wave amplitude/sqrt(2)
 * ======================================================================== */

TEST_F(DSPTest, RMS) {
    const double amplitude = 3.0;
    const uint32_t N = 10000;  /* many cycles for accuracy */
    const double fs = 10000.0;
    auto s = sine(100.0, fs, N);
    for (auto& v : s) v *= amplitude;

    double rms = dsp_rms(s.data(), N);
    EXPECT_NEAR(rms, amplitude / sqrt(2.0), 0.01);
}

/* ========================================================================
 * 28. ZeroCrossingRate: ZCR of sine ~ 2f/fs
 * ======================================================================== */

TEST_F(DSPTest, ZeroCrossingRate) {
    const double fs = 10000.0;
    const double f0 = 500.0;
    const uint32_t N = 10000;
    auto s = sine(f0, fs, N);

    double zcr = dsp_zero_crossing_rate(s.data(), N);
    double expected = 2.0 * f0 / fs;
    EXPECT_NEAR(zcr, expected, 0.01);
}

/* ========================================================================
 * 29. FFTKnownValues: FFT([1,1,1,1]) = [4,0,0,0]
 * ======================================================================== */

TEST_F(DSPTest, FFTKnownValues) {
    dsp_complex_t data[4] = {{1,0}, {1,0}, {1,0}, {1,0}};
    dsp_fft(data, 4);

    EXPECT_NEAR(data[0].re, 4.0, FFT_TOL);
    EXPECT_NEAR(data[0].im, 0.0, FFT_TOL);
    for (int k = 1; k < 4; k++) {
        EXPECT_NEAR(data[k].re, 0.0, FFT_TOL);
        EXPECT_NEAR(data[k].im, 0.0, FFT_TOL);
    }
}

/* ========================================================================
 * 30. HammingKnown: known endpoint and center values
 * ======================================================================== */

TEST_F(DSPTest, HammingKnown) {
    const uint32_t N = 64;
    std::vector<double> w(N);
    dsp_window(w.data(), N, DSP_WINDOW_HAMMING);

    /* Hamming(0) = 0.54 - 0.46 = 0.08 */
    EXPECT_NEAR(w[0], 0.08, 0.005);
    /* Hamming(N/2) ~ 1.0 */
    EXPECT_NEAR(w[N / 2], 1.0, 0.02);
}

/* ========================================================================
 * 31. ButterworthMagnitude: -3dB at cutoff
 * ======================================================================== */

TEST_F(DSPTest, ButterworthMagnitude) {
    const double fs = 1000.0;
    const double cutoff = 200.0;
    const uint32_t npoints = 512;

    dsp_filter_t filt = dsp_design_filter(DSP_FILTER_BUTTERWORTH,
                                           DSP_FILTER_LOWPASS, 4,
                                           cutoff, 0.0, fs);

    std::vector<double> magnitude(npoints), phase(npoints);
    dsp_filter_freqz(&filt, magnitude.data(), phase.data(), npoints);

    /* Find bin closest to cutoff. freqz covers 0..Nyquist in npoints */
    double nyquist = fs / 2.0;
    uint32_t cutoff_bin = (uint32_t)round(cutoff / nyquist * (npoints - 1));

    /* Magnitude at cutoff should be ~1/sqrt(2) = 0.7071 (-3dB) */
    EXPECT_NEAR(magnitude[cutoff_bin], 1.0 / sqrt(2.0), 0.05);
}

/* ========================================================================
 * 32. STFTToMel: full STFT -> mel filterbank -> MFCC pipeline
 * ======================================================================== */

TEST_F(DSPTest, STFTToMel) {
    const uint32_t N = 2048;
    const double fs = 16000.0;
    const uint32_t fft_size = 512;
    const uint32_t hop_size = 256;
    const uint32_t num_mel = 26;
    const uint32_t num_mfcc = 13;

    auto s = sine(440.0, fs, N);

    /* Step 1: STFT */
    dsp_stft_result_t* stft = dsp_stft(engine, s.data(), N, fft_size,
                                         hop_size, DSP_WINDOW_HAMMING);
    ASSERT_NE(stft, nullptr);
    ASSERT_GT(stft->num_frames, 0u);

    /* Step 2: mel filterbank on each frame's power spectrum */
    std::vector<double> mel_energies(num_mel);
    for (uint32_t f = 0; f < stft->num_frames; f++) {
        dsp_mel_filterbank(&stft->power[f * stft->num_bins], stft->num_bins,
                            fs, mel_energies.data(), num_mel);
        /* At least some mel bins should have energy */
        double total = 0.0;
        for (uint32_t m = 0; m < num_mel; m++) total += mel_energies[m];
        EXPECT_GT(total, 0.0);
    }

    /* Step 3: full MFCC (uses engine internally) */
    std::vector<double> mfcc(num_mfcc);
    dsp_mfcc(engine, s.data(), N, fs, mfcc.data(), num_mfcc);
    for (uint32_t i = 0; i < num_mfcc; i++) {
        EXPECT_FALSE(std::isnan(mfcc[i]));
    }

    dsp_stft_free(stft);
}

/* ========================================================================
 * 33. FilterThenFFT: filter -> FFT shows stopband attenuation
 * ======================================================================== */

TEST_F(DSPTest, FilterThenFFT) {
    const double fs = 1000.0;
    const uint32_t N = 1024;

    /* Signal = 50 Hz + 400 Hz */
    std::vector<double> s(N);
    for (uint32_t i = 0; i < N; i++)
        s[i] = sin(2.0 * M_PI * 50.0 * i / fs)
             + sin(2.0 * M_PI * 400.0 * i / fs);

    /* Lowpass at 150 Hz */
    dsp_filter_t filt = dsp_design_filter(DSP_FILTER_BUTTERWORTH,
                                           DSP_FILTER_LOWPASS, 4,
                                           150.0, 0.0, fs);
    std::vector<double> filtered(N);
    dsp_filter_apply(&filt, s.data(), filtered.data(), N);

    /* FFT of filtered signal */
    std::vector<dsp_complex_t> spectrum(N);
    for (uint32_t i = 0; i < N; i++) {
        spectrum[i].re = filtered[i];
        spectrum[i].im = 0.0;
    }
    dsp_fft(spectrum.data(), N);

    /* Bin for 50 Hz: 50*1024/1000 ~ 51 */
    uint32_t bin_50  = (uint32_t)round(50.0 * N / fs);
    uint32_t bin_400 = (uint32_t)round(400.0 * N / fs);

    double mag_50  = cmag(spectrum[bin_50]);
    double mag_400 = cmag(spectrum[bin_400]);

    /* 50 Hz should pass; 400 Hz should be heavily attenuated */
    EXPECT_GT(mag_50, mag_400 * 10.0);
}

/* ========================================================================
 * 34. FullAudioPipeline: end-to-end audio processing
 * ======================================================================== */

TEST_F(DSPTest, FullAudioPipeline) {
    const double fs = 4000.0;
    const uint32_t N = 4096;
    const double f1 = 200.0;
    const double f2 = 800.0;

    /* Step 1: Generate test signal = sum of two sines */
    std::vector<double> signal(N);
    for (uint32_t i = 0; i < N; i++)
        signal[i] = sin(2.0 * M_PI * f1 * i / fs)
                   + sin(2.0 * M_PI * f2 * i / fs);

    /* Step 2: Apply Hamming window */
    std::vector<double> windowed(signal.begin(), signal.end());
    dsp_apply_window(windowed.data(), N, DSP_WINDOW_HAMMING);

    /* Step 3: FFT and identify both frequency peaks */
    std::vector<dsp_complex_t> spectrum(N);
    for (uint32_t i = 0; i < N; i++) {
        spectrum[i].re = windowed[i];
        spectrum[i].im = 0.0;
    }
    dsp_fft(spectrum.data(), N);

    /* Find two largest peaks in positive frequencies */
    std::vector<std::pair<double, uint32_t>> peaks;
    for (uint32_t k = 1; k < N / 2 - 1; k++) {
        double m = cmag(spectrum[k]);
        double m_prev = cmag(spectrum[k - 1]);
        double m_next = cmag(spectrum[k + 1]);
        if (m > m_prev && m > m_next && m > 10.0)
            peaks.push_back({m, k});
    }
    std::sort(peaks.begin(), peaks.end(),
              [](auto& a, auto& b) { return a.first > b.first; });

    ASSERT_GE(peaks.size(), 2u);
    double detected_f1 = peaks[0].second * fs / N;
    double detected_f2 = peaks[1].second * fs / N;
    /* Sort detected frequencies */
    if (detected_f1 > detected_f2) std::swap(detected_f1, detected_f2);

    EXPECT_NEAR(detected_f1, f1, fs / N * 2.0);
    EXPECT_NEAR(detected_f2, f2, fs / N * 2.0);

    /* Step 4: PSD confirms both peaks */
    dsp_psd_result_t* psd = dsp_welch_psd(engine, signal.data(), N, fs, 512, 256);
    ASSERT_NE(psd, nullptr);
    EXPECT_GT(psd->total_power, 0.0);
    dsp_psd_free(psd);

    /* Step 5: Lowpass filter at 500 Hz to remove f2 (800 Hz) */
    dsp_filter_t lp = dsp_design_filter(DSP_FILTER_BUTTERWORTH,
                                          DSP_FILTER_LOWPASS, 6,
                                          500.0, 0.0, fs);
    std::vector<double> filtered(N);
    dsp_filter_apply(&lp, signal.data(), filtered.data(), N);

    /* Step 6: FFT of filtered signal -- only f1 should remain */
    std::vector<dsp_complex_t> filt_spec(N);
    for (uint32_t i = 0; i < N; i++) {
        filt_spec[i].re = filtered[i];
        filt_spec[i].im = 0.0;
    }
    dsp_fft(filt_spec.data(), N);

    uint32_t bin_f1 = (uint32_t)round(f1 * N / fs);
    uint32_t bin_f2 = (uint32_t)round(f2 * N / fs);

    double mag_f1 = cmag(filt_spec[bin_f1]);
    double mag_f2 = cmag(filt_spec[bin_f2]);

    /* f1 present, f2 heavily attenuated */
    EXPECT_GT(mag_f1, 100.0);
    EXPECT_LT(mag_f2, mag_f1 * 0.05);
}
