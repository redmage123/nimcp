/**
 * @file test_fno_fft.c
 * @brief FNO real FFT helpers + spectral conv gradient check.
 *
 * Background (2026-10-26): the FNO "fft" was an O(n^2) DFT with a double
 * cos/sin per (k, j) pair, and the spectral conv backward ran one inverse
 * transform per (out_ch, in_ch) pair. On the visual cortex FNO (fft 1024,
 * 16 channels, 2 blocks) that cost ~5.4 s per backward. The transforms are
 * now a radix-2 FFT for power-of-two sizes and the backward accumulates in
 * the frequency domain (one inverse transform per input channel).
 *
 * Coverage:
 *   1. test_fft_matches_naive_dft      — fno_fft_real vs an independent
 *      double-precision DFT for n in {4, 8, 64, 1024} (power of two path).
 *   2. test_fft_non_pow2_fallback      — same oracle for n in {6, 100}.
 *   3. test_ifft_round_trip            — ifft(fft(x)) == x for n = 1024, and
 *      ifft matches the naive Hermitian inverse DFT on a random spectrum.
 *   4. test_spectral_conv_grad_check   — finite-difference check of
 *      grad_W_real, grad_W_imag and dl_din through
 *      fno_spectral_conv_forward/backward (validates the restructured
 *      backward against the forward).
 *
 * Signatures used (include/training/nimcp_fno_layer.h):
 *   void fno_fft_real(const float* input, uint32_t n, float* out_real, float* out_imag);
 *   void fno_ifft_real(const float* in_real, const float* in_imag, uint32_t n, float* output);
 *   fno_spectral_conv_t* fno_spectral_conv_create(uint32_t in_ch, uint32_t out_ch,
 *                                                 uint32_t n_modes, uint32_t spatial_size);
 *   void fno_spectral_conv_destroy(fno_spectral_conv_t* layer);
 *   int  fno_spectral_conv_forward(fno_spectral_conv_t* layer, const float* input, float* output);
 *   int  fno_spectral_conv_backward(fno_spectral_conv_t* layer, const float* dl_dout, float* dl_din);
 *   void fno_spectral_conv_zero_grad(fno_spectral_conv_t* layer);
 *   fields: in_channels, out_channels, n_modes, spatial_size, W_real, W_imag,
 *           grad_W_real, grad_W_imag
 */

#include "training/nimcp_fno_layer.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int g_failures = 0;
#define EXPECT(cond, ...) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d " #cond " : ", __func__, __LINE__); \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n"); \
        g_failures++; \
    } \
} while (0)

static float frand(void) { return 2.0f * ((float)rand() / (float)RAND_MAX) - 1.0f; }

/* Independent oracle: textbook DFT in double precision. */
static void naive_dft(const float* x, uint32_t n, double* re, double* im) {
    uint32_t freq_n = n / 2 + 1;
    for (uint32_t k = 0; k < freq_n; k++) {
        double sr = 0.0, si = 0.0;
        for (uint32_t j = 0; j < n; j++) {
            double a = -2.0 * M_PI * (double)k * (double)j / (double)n;
            sr += x[j] * cos(a);
            si += x[j] * sin(a);
        }
        re[k] = sr; im[k] = si;
    }
}

/* Independent oracle: Hermitian inverse DFT, DC/Nyquist purely real, 1/n. */
static void naive_idft(const float* re, const float* im, uint32_t n, double* out) {
    uint32_t freq_n = n / 2 + 1;
    for (uint32_t j = 0; j < n; j++) {
        double s = re[0];
        for (uint32_t k = 1; k < freq_n - 1; k++) {
            double a = 2.0 * M_PI * (double)k * (double)j / (double)n;
            s += 2.0 * (re[k] * cos(a) - im[k] * sin(a));
        }
        if (n % 2 == 0 && freq_n > 1) s += re[freq_n - 1] * cos(M_PI * (double)j);
        out[j] = s / (double)n;
    }
}

static void check_fft_against_oracle(uint32_t n) {
    uint32_t freq_n = n / 2 + 1;
    float* x = malloc(n * sizeof(float));
    float* fr = malloc(freq_n * sizeof(float));
    float* fi = malloc(freq_n * sizeof(float));
    double* orr = malloc(freq_n * sizeof(double));
    double* ori = malloc(freq_n * sizeof(double));
    for (uint32_t j = 0; j < n; j++) x[j] = frand();

    fno_fft_real(x, n, fr, fi);
    naive_dft(x, n, orr, ori);

    /* Sums of n terms with |x| <= 1: absolute tolerance scales with n. */
    double tol = 1e-5 * (double)n + 1e-5;
    double max_err = 0.0;
    for (uint32_t k = 0; k < freq_n; k++) {
        double e1 = fabs((double)fr[k] - orr[k]);
        double e2 = fabs((double)fi[k] - ori[k]);
        if (e1 > max_err) max_err = e1;
        if (e2 > max_err) max_err = e2;
    }
    EXPECT(max_err < tol, "n=%u max_err=%.3e tol=%.3e", n, max_err, tol);
    free(x); free(fr); free(fi); free(orr); free(ori);
}

static void test_fft_matches_naive_dft(void) {
    const uint32_t sizes[] = {4, 8, 64, 1024};
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
        check_fft_against_oracle(sizes[i]);
}

static void test_fft_non_pow2_fallback(void) {
    const uint32_t sizes[] = {6, 100};
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
        check_fft_against_oracle(sizes[i]);
}

static void test_ifft_round_trip(void) {
    const uint32_t n = 1024, freq_n = n / 2 + 1;
    float* x = malloc(n * sizeof(float));
    float* y = malloc(n * sizeof(float));
    float* fr = malloc(freq_n * sizeof(float));
    float* fi = malloc(freq_n * sizeof(float));
    for (uint32_t j = 0; j < n; j++) x[j] = frand();

    fno_fft_real(x, n, fr, fi);
    fno_ifft_real(fr, fi, n, y);
    double max_err = 0.0;
    for (uint32_t j = 0; j < n; j++) {
        double e = fabs((double)x[j] - (double)y[j]);
        if (e > max_err) max_err = e;
    }
    EXPECT(max_err < 1e-4, "round trip n=%u max_err=%.3e", n, max_err);

    /* Inverse on an arbitrary spectrum (imag at DC/Nyquist deliberately
     * non-zero: both implementations must ignore it). */
    for (uint32_t k = 0; k < freq_n; k++) { fr[k] = frand(); fi[k] = frand(); }
    double* ref = malloc(n * sizeof(double));
    naive_idft(fr, fi, n, ref);
    fno_ifft_real(fr, fi, n, y);
    max_err = 0.0;
    for (uint32_t j = 0; j < n; j++) {
        double e = fabs(ref[j] - (double)y[j]);
        if (e > max_err) max_err = e;
    }
    EXPECT(max_err < 1e-5, "ifft vs naive n=%u max_err=%.3e", n, max_err);

    free(x); free(y); free(fr); free(fi); free(ref);
}

/* Scalar loss L = sum(output) so dL/doutput = 1 everywhere. */
static double loss_of(fno_spectral_conv_t* layer, const float* input, float* out) {
    if (fno_spectral_conv_forward(layer, input, out) != 0) return NAN;
    double s = 0.0;
    size_t n = (size_t)layer->out_channels * layer->spatial_size;
    for (size_t i = 0; i < n; i++) s += out[i];
    return s;
}

static void test_spectral_conv_grad_check(void) {
    const uint32_t in_ch = 2, out_ch = 3, n_modes = 8, sp = 50; /* fft 64 */
    fno_spectral_conv_t* layer = fno_spectral_conv_create(in_ch, out_ch, n_modes, sp);
    EXPECT(layer != NULL, "create failed");
    if (!layer) return;

    size_t in_n = (size_t)in_ch * sp, out_n = (size_t)out_ch * sp;
    float* input = malloc(in_n * sizeof(float));
    float* out = malloc(out_n * sizeof(float));
    float* dl_dout = malloc(out_n * sizeof(float));
    float* dl_din = calloc(in_n, sizeof(float));
    for (size_t i = 0; i < in_n; i++) input[i] = frand();
    for (size_t i = 0; i < out_n; i++) dl_dout[i] = 1.0f;

    /* Analytic gradients */
    fno_spectral_conv_zero_grad(layer);
    EXPECT(fno_spectral_conv_forward(layer, input, out) == 0, "forward failed");
    EXPECT(fno_spectral_conv_backward(layer, dl_dout, dl_din) == 0, "backward failed");

    size_t w_n = (size_t)out_ch * in_ch * layer->n_modes;
    float* gWr = malloc(w_n * sizeof(float));
    float* gWi = malloc(w_n * sizeof(float));
    memcpy(gWr, layer->grad_W_real, w_n * sizeof(float));
    memcpy(gWi, layer->grad_W_imag, w_n * sizeof(float));

    /* Finite differences on a sample of weights (real + imag) and inputs.
     * Relative tolerance: float forward, central differences with eps=1e-3. */
    const double eps = 1e-3;
    const double rel_tol = 3e-2, abs_floor = 1e-3;
    const size_t w_samples[] = {0, 3, 7, w_n / 2, w_n - 1};
    for (size_t s = 0; s < sizeof(w_samples) / sizeof(w_samples[0]); s++) {
        size_t idx = w_samples[s];
        for (int which = 0; which < 2; which++) {
            float* W = which ? layer->W_imag : layer->W_real;
            float saved = W[idx];
            W[idx] = saved + (float)eps;  double lp = loss_of(layer, input, out);
            W[idx] = saved - (float)eps;  double lm = loss_of(layer, input, out);
            W[idx] = saved;
            double num = (lp - lm) / (2.0 * eps);
            double ana = which ? gWi[idx] : gWr[idx];
            double err = fabs(num - ana);
            double scale = fabs(num) > abs_floor ? fabs(num) : abs_floor;
            EXPECT(err <= rel_tol * scale + abs_floor,
                   "W_%s[%zu]: numeric=%.5f analytic=%.5f", which ? "imag" : "real",
                   idx, num, ana);
        }
    }
    const size_t in_samples[] = {0, 17, sp - 1, sp + 5, in_n - 1};
    for (size_t s = 0; s < sizeof(in_samples) / sizeof(in_samples[0]); s++) {
        size_t idx = in_samples[s];
        float saved = input[idx];
        input[idx] = saved + (float)eps;  double lp = loss_of(layer, input, out);
        input[idx] = saved - (float)eps;  double lm = loss_of(layer, input, out);
        input[idx] = saved;
        double num = (lp - lm) / (2.0 * eps);
        double ana = dl_din[idx];
        double err = fabs(num - ana);
        double scale = fabs(num) > abs_floor ? fabs(num) : abs_floor;
        EXPECT(err <= rel_tol * scale + abs_floor,
               "dl_din[%zu]: numeric=%.5f analytic=%.5f", idx, num, ana);
    }

    free(input); free(out); free(dl_dout); free(dl_din); free(gWr); free(gWi);
    fno_spectral_conv_destroy(layer);
}

int main(void) {
    srand(12345);
    test_fft_matches_naive_dft();
    test_fft_non_pow2_fallback();
    test_ifft_round_trip();
    test_spectral_conv_grad_check();
    if (g_failures) {
        fprintf(stderr, "test_fno_fft: %d failure(s)\n", g_failures);
        return 1;
    }
    printf("test_fno_fft: all passed\n");
    return 0;
}
