/**
 * @file nimcp_zeta_functions.c
 * @brief Riemann Zeta Function — evaluation, zeros, RMT, L-functions
 *
 * WHAT: ζ(s), Z(t), zero-finding, Gram points, pair correlation, GUE fit
 * WHY:  Computational access to the deepest connection between primes and analysis
 * HOW:  Euler-Maclaurin for Re(s)>1, functional equation for Re(s)<0,
 *       Riemann-Siegel for critical strip, sign-change zero detection
 */

#include "cognitive/math/nimcp_zeta_functions.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>
#include <string.h>

#define LOG_TAG "ZETA"
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ============================================================================
 * Complex Arithmetic (double precision)
 * ============================================================================ */

static inline zeta_complex_t zc(double re, double im) { return (zeta_complex_t){re, im}; }
static inline zeta_complex_t zc_add(zeta_complex_t a, zeta_complex_t b) { return zc(a.re+b.re, a.im+b.im); }
static inline zeta_complex_t zc_sub(zeta_complex_t a, zeta_complex_t b) { return zc(a.re-b.re, a.im-b.im); }
static inline zeta_complex_t zc_mul(zeta_complex_t a, zeta_complex_t b) {
    return zc(a.re*b.re - a.im*b.im, a.re*b.im + a.im*b.re);
}
static inline zeta_complex_t zc_div(zeta_complex_t a, zeta_complex_t b) {
    double d = b.re*b.re + b.im*b.im;
    if (d < 1e-300) return zc(0, 0);
    return zc((a.re*b.re + a.im*b.im)/d, (a.im*b.re - a.re*b.im)/d);
}
static inline double zc_abs(zeta_complex_t z) { return sqrt(z.re*z.re + z.im*z.im); }
static inline zeta_complex_t zc_conj(zeta_complex_t z) { return zc(z.re, -z.im); }

/* Complex power: n^(-s) = exp(-s·ln(n)) */
static zeta_complex_t zc_pow_n(double n, zeta_complex_t s) {
    double ln_n = log(n);
    /* n^(-s) = exp(-(sigma+it)·ln(n)) = exp(-sigma·ln(n)) · (cos(t·ln(n)) - i·sin(t·ln(n))) */
    double mag = exp(-s.re * ln_n);
    double phase = -s.im * ln_n;
    return zc(mag * cos(phase), mag * sin(phase));
}

/* Complex exponential */
static zeta_complex_t zc_exp(zeta_complex_t z) {
    double mag = exp(z.re);
    return zc(mag * cos(z.im), mag * sin(z.im));
}

/* Complex log */
static zeta_complex_t zc_log(zeta_complex_t z) {
    return zc(log(zc_abs(z)), atan2(z.im, z.re));
}

/* Complex sin */
static zeta_complex_t zc_sin(zeta_complex_t z) {
    return zc(sin(z.re)*cosh(z.im), cos(z.re)*sinh(z.im));
}

/* ============================================================================
 * Bernoulli Numbers (precomputed)
 * ============================================================================ */

static void init_bernoulli(double* B, uint32_t max) {
    /* B_0=1, B_1=-1/2, B_2=1/6, B_4=-1/30, ... (odd B_n=0 for n>1) */
    memset(B, 0, max * sizeof(double));
    B[0] = 1.0;
    B[1] = -0.5;
    /* Akiyama-Tanigawa algorithm */
    double* a = (double*)nimcp_calloc(max, sizeof(double));
    if (!a) return;
    for (uint32_t m = 0; m < max; m++) {
        a[m] = 1.0 / (double)(m + 1);
        for (int j = (int)m; j >= 1; j--) {
            a[j-1] = (double)j * (a[j-1] - a[j]);
        }
        B[m] = a[0];
    }
    nimcp_free(a);
}

/* ============================================================================
 * Log-Gamma (Stirling for complex argument)
 * ============================================================================ */

zeta_complex_t zeta_log_gamma(zeta_complex_t z) {
    /* Stirling: ln Γ(z) ≈ (z-1/2)ln(z) - z + ln(2π)/2 + Σ B_{2k}/(2k(2k-1)z^(2k-1)) */
    /* Shift argument to |z| > 10 for convergence */
    zeta_complex_t result = zc(0, 0);
    while (z.re < 10.0) {
        result = zc_sub(result, zc_log(z));
        z.re += 1.0;
    }
    /* Stirling series */
    zeta_complex_t lz = zc_log(z);
    result = zc_add(result, zc_mul(zc_sub(z, zc(0.5, 0)), lz));
    result = zc_sub(result, z);
    result = zc_add(result, zc(0.5 * log(2.0 * M_PI), 0));
    /* Correction terms */
    zeta_complex_t zn = z;  /* z^1 */
    double coeffs[] = {1.0/12.0, -1.0/360.0, 1.0/1260.0, -1.0/1680.0};
    for (int k = 0; k < 4; k++) {
        result = zc_add(result, zc_div(zc(coeffs[k], 0), zn));
        zn = zc_mul(zn, zc_mul(z, z)); /* z^(2k+1) */
    }
    return result;
}

/* ============================================================================
 * Riemann-Siegel Theta
 * ============================================================================ */

double zeta_theta(double t) {
    /* θ(t) = Im(ln Γ(1/4 + it/2)) - t·ln(π)/2 */
    zeta_complex_t lg = zeta_log_gamma(zc(0.25, t * 0.5));
    return lg.im - t * log(M_PI) * 0.5;
}

/* ============================================================================
 * Hardy's Z-function (Riemann-Siegel formula)
 * ============================================================================ */

double zeta_Z(zeta_engine_t* engine, double t) {
    /* Z(t) = 2·Σ(n=1..N) cos(θ(t) - t·ln(n)) / √n
     * where N = floor(sqrt(t/(2π))) */
    if (!engine) return 0;
    engine->stats.z_evaluations++;

    double theta = zeta_theta(t);
    int N = (int)sqrt(t / (2.0 * M_PI));
    if (N < 1) N = 1;
    if (N > (int)engine->config.riemann_siegel_terms)
        N = (int)engine->config.riemann_siegel_terms;

    double sum = 0;
    for (int n = 1; n <= N; n++) {
        double phase = theta - t * log((double)n);
        sum += cos(phase) / sqrt((double)n);
    }
    return 2.0 * sum;
}

/* ============================================================================
 * Zeta for Re(s) > 1 (Euler-Maclaurin)
 * ============================================================================ */

static zeta_complex_t zeta_em(zeta_engine_t* engine, zeta_complex_t s) {
    /* ζ(s) = Σ(n=1..N) n^(-s) + N^(1-s)/(s-1) + N^(-s)/2 + Σ Bernoulli corrections */
    uint32_t N = engine->config.euler_maclaurin_terms;
    if (N < 10) N = 10;

    zeta_complex_t sum = zc(0, 0);
    for (uint32_t n = 1; n <= N; n++) {
        sum = zc_add(sum, zc_pow_n((double)n, s));
    }

    /* Integral tail: N^(1-s)/(s-1) */
    zeta_complex_t s_minus_1 = zc_sub(s, zc(1, 0));
    zeta_complex_t N_1ms = zc_pow_n((double)N, zc_sub(zc(1, 0), s));
    zeta_complex_t tail = zc_div(N_1ms, s_minus_1);
    sum = zc_add(sum, tail);

    /* Endpoint correction: sum includes n=N fully, but EM uses f(N)/2.
     * Subtract half of the N-th term to correct: -N^(-s)/2 */
    zeta_complex_t endpoint = zc_pow_n((double)N, s);
    sum = zc_sub(sum, zc(endpoint.re * 0.5, endpoint.im * 0.5));

    return sum;
}

/* ============================================================================
 * Zeta via Functional Equation (Re(s) < 0)
 * ============================================================================ */

zeta_complex_t zeta_functional_equation(zeta_engine_t* engine, zeta_complex_t s) {
    /* ζ(s) = 2^s · π^(s-1) · sin(πs/2) · Γ(1-s) · ζ(1-s) */
    zeta_complex_t one_minus_s = zc_sub(zc(1, 0), s);

    /* Compute ζ(1-s) which has Re > 1 */
    zeta_complex_t zeta_1ms = zeta_em(engine, one_minus_s);

    /* 2^s */
    zeta_complex_t two_s = zc_exp(zc_mul(s, zc(log(2.0), 0)));

    /* π^(s-1) */
    zeta_complex_t pi_sm1 = zc_exp(zc_mul(zc_sub(s, zc(1, 0)), zc(log(M_PI), 0)));

    /* sin(πs/2) */
    zeta_complex_t sin_pis2 = zc_sin(zc_mul(s, zc(M_PI * 0.5, 0)));

    /* Γ(1-s) */
    zeta_complex_t lg = zeta_log_gamma(one_minus_s);
    zeta_complex_t gamma_1ms = zc_exp(lg);

    /* Multiply all together */
    zeta_complex_t result = zc_mul(two_s, pi_sm1);
    result = zc_mul(result, sin_pis2);
    result = zc_mul(result, gamma_1ms);
    result = zc_mul(result, zeta_1ms);
    return result;
}

/* ============================================================================
 * Main Zeta Evaluation
 * ============================================================================ */

zeta_complex_t zeta_eval(zeta_engine_t* engine, zeta_complex_t s) {
    if (!engine) return zc(0, 0);
    engine->stats.zeta_evaluations++;

    /* Pole at s=1 */
    if (fabs(s.re - 1.0) < 1e-10 && fabs(s.im) < 1e-10)
        return zc(1e15, 0);  /* ±∞ */

    /* Trivial zeros at s = -2, -4, -6, ... */
    if (s.im == 0 && s.re < 0 && fmod(-s.re, 2.0) < 1e-10)
        return zc(0, 0);

    if (s.re > 1.0) {
        /* Direct Euler-Maclaurin */
        return zeta_em(engine, s);
    } else if (s.re < 0.0) {
        /* Functional equation */
        return zeta_functional_equation(engine, s);
    } else {
        /* Critical strip 0 ≤ Re(s) ≤ 1: use Z-function approach */
        /* ζ(1/2+it) = Z(t)·exp(-iθ(t)) */
        if (fabs(s.re - 0.5) < 1e-6) {
            double t = s.im;
            double Z = zeta_Z(engine, fabs(t));
            double theta = zeta_theta(fabs(t));
            return zc(Z * cos(theta), -Z * sin(theta));
        }
        /* Off the critical line in the strip: use functional equation reflection */
        if (s.re < 0.5) {
            return zeta_functional_equation(engine, s);
        } else {
            /* Re(s) > 0.5: direct summation converges slowly but works */
            return zeta_em(engine, s);
        }
    }
}

double zeta_eval_real(zeta_engine_t* engine, double s) {
    zeta_complex_t result = zeta_eval(engine, zc(s, 0));
    return result.re;
}

/* ============================================================================
 * Zero Finding
 * ============================================================================ */

uint32_t zeta_find_zeros_in_range(zeta_engine_t* engine,
                                    double t_low, double t_high,
                                    double step_size) {
    if (!engine || t_low >= t_high || step_size <= 0) return 0;
    uint32_t found = 0;

    double prev_Z = zeta_Z(engine, t_low);
    for (double t = t_low + step_size; t <= t_high; t += step_size) {
        double curr_Z = zeta_Z(engine, t);

        /* Sign change in Z(t) means zero between t-step and t */
        if (prev_Z * curr_Z < 0 && engine->num_zeros < ZETA_MAX_ZEROS) {
            /* Bisect to refine */
            double a = t - step_size, b = t;
            double fa = prev_Z, fb = curr_Z;
            for (int iter = 0; iter < 50; iter++) {
                double mid = (a + b) * 0.5;
                double fmid = zeta_Z(engine, mid);
                if (fabs(fmid) < engine->config.zero_tolerance) {
                    a = b = mid;
                    break;
                }
                if (fa * fmid < 0) { b = mid; fb = fmid; }
                else { a = mid; fa = fmid; }
            }
            double zero_t = (a + b) * 0.5;

            zeta_zero_t z = {0};
            z.t = zero_t;
            z.Z_value = zeta_Z(engine, zero_t);
            z.index = engine->num_zeros + 1;
            z.verified = true;  /* found on critical line by construction */

            engine->zeros[engine->num_zeros++] = z;
            engine->stats.zeros_found++;
            engine->stats.zeros_verified_on_critical_line++;
            if (zero_t > engine->stats.highest_zero_found)
                engine->stats.highest_zero_found = zero_t;
            found++;
        }
        prev_Z = curr_Z;
    }
    return found;
}

double zeta_find_nth_zero(zeta_engine_t* engine, uint32_t n) {
    if (!engine || n == 0) return 0;

    /* Use known zeros for n ≤ 10 */
    double known[] = {0, ZETA_ZERO_1, ZETA_ZERO_2, ZETA_ZERO_3, ZETA_ZERO_4,
                      ZETA_ZERO_5, ZETA_ZERO_6, ZETA_ZERO_7, ZETA_ZERO_8,
                      ZETA_ZERO_9, ZETA_ZERO_10};
    if (n <= 10) return known[n];

    /* For n > 10, use Gram point search: g_n ≈ 2πn/ln(n) */
    /* Then refine with bisection around expected position */
    double approx_t = 2.0 * M_PI * (double)n / log((double)n);
    zeta_find_zeros_in_range(engine, approx_t - 5.0, approx_t + 5.0, 0.01);

    /* Return the closest zero we found */
    double best = 0, best_dist = 1e20;
    for (uint32_t i = 0; i < engine->num_zeros; i++) {
        double dist = fabs(engine->zeros[i].t - approx_t);
        if (dist < best_dist) { best_dist = dist; best = engine->zeros[i].t; }
    }
    return best;
}

bool zeta_verify_on_critical_line(zeta_engine_t* engine, double t) {
    /* Check if |ζ(1/2+it)| ≈ 0 */
    if (!engine) return false;
    double Z = zeta_Z(engine, t);
    return fabs(Z) < engine->config.zero_tolerance * 100.0;
}

void zeta_load_known_zeros(zeta_engine_t* engine) {
    if (!engine) return;
    double known[] = { ZETA_ZERO_1, ZETA_ZERO_2, ZETA_ZERO_3, ZETA_ZERO_4,
                       ZETA_ZERO_5, ZETA_ZERO_6, ZETA_ZERO_7, ZETA_ZERO_8,
                       ZETA_ZERO_9, ZETA_ZERO_10 };
    for (int i = 0; i < 10 && engine->num_zeros < ZETA_MAX_ZEROS; i++) {
        engine->zeros[engine->num_zeros++] = (zeta_zero_t){
            .t = known[i], .Z_value = 0, .index = (uint32_t)(i + 1), .verified = true
        };
    }
    engine->stats.zeros_found = 10;
    engine->stats.zeros_verified_on_critical_line = 10;
    engine->stats.highest_zero_found = ZETA_ZERO_10;
}

/* ============================================================================
 * Zero Counting
 * ============================================================================ */

double zeta_N(double T) {
    /* N(T) = T/(2π)·ln(T/(2πe)) + 7/8 + S(T)
     * Simplified: omit S(T) oscillatory term */
    if (T <= 0) return 0;
    return T / (2.0 * M_PI) * log(T / (2.0 * M_PI * M_E)) + 7.0 / 8.0;
}

double zeta_S(zeta_engine_t* engine, double T) {
    /* S(T) = (1/π)·arg(ζ(1/2+iT)) — oscillatory, hard to compute */
    /* Approximate from Z-function sign */
    if (!engine) return 0;
    double Z = zeta_Z(engine, T);
    return (Z > 0 ? 0.0 : 0.5);  /* crude: +0 or +1/2 */
}

/* ============================================================================
 * Random Matrix Theory
 * ============================================================================ */

double zeta_gue_pair_correlation(double x) {
    /* Montgomery's conjecture: 1 - (sin(πx)/(πx))² */
    if (fabs(x) < 1e-10) return 0;  /* limit at x→0 */
    double sinc = sin(M_PI * x) / (M_PI * x);
    return 1.0 - sinc * sinc;
}

double zeta_wigner_surmise(double s) {
    /* p(s) = (π/2)·s·exp(-πs²/4) */
    return (M_PI * 0.5) * s * exp(-M_PI * s * s * 0.25);
}

double zeta_pair_correlation(const zeta_engine_t* engine, double x) {
    /* Compute empirical pair correlation from stored zeros */
    if (!engine || engine->num_zeros < 3) return 0;
    /* Count pairs with normalized spacing near x */
    double mean_spacing = 0;
    for (uint32_t i = 1; i < engine->num_zeros; i++)
        mean_spacing += engine->zeros[i].t - engine->zeros[i-1].t;
    mean_spacing /= (double)(engine->num_zeros - 1);
    if (mean_spacing < 1e-10) return 0;

    double count = 0, total = 0;
    double window = 0.2;
    for (uint32_t i = 0; i < engine->num_zeros; i++) {
        for (uint32_t j = i + 1; j < engine->num_zeros; j++) {
            double normalized = (engine->zeros[j].t - engine->zeros[i].t) / mean_spacing;
            if (fabs(normalized - x) < window) count += 1.0;
            total += 1.0;
            if (normalized > x + 2.0) break;
        }
    }
    return (total > 0) ? count / (total * window * 2.0) : 0;
}

zeta_spacing_stats_t zeta_compute_spacing_stats(zeta_engine_t* engine) {
    zeta_spacing_stats_t stats = {0};
    if (!engine || engine->num_zeros < 3) return stats;

    /* Mean spacing */
    double total_gap = 0;
    for (uint32_t i = 1; i < engine->num_zeros; i++)
        total_gap += engine->zeros[i].t - engine->zeros[i-1].t;
    stats.mean_spacing = total_gap / (double)(engine->num_zeros - 1);
    stats.num_zeros_analyzed = engine->num_zeros;

    /* GUE fit: compare pair correlation at x=1 to GUE prediction */
    double empirical = zeta_pair_correlation(engine, 1.0);
    double theoretical = zeta_gue_pair_correlation(1.0);
    stats.gue_fit = fabs(empirical - theoretical);
    stats.pair_correlation = empirical;

    engine->spacing = stats;
    return stats;
}

/* ============================================================================
 * Dirichlet L-functions
 * ============================================================================ */

zeta_complex_t zeta_dirichlet_L(zeta_engine_t* engine,
                                  zeta_complex_t s,
                                  uint32_t q, const int8_t* chi_values) {
    /* L(s,χ) = Σ(n=1..∞) χ(n)/n^s */
    if (!engine || !chi_values || q == 0) return zc(0, 0);

    uint32_t N = engine->config.euler_maclaurin_terms;
    zeta_complex_t sum = zc(0, 0);
    for (uint32_t n = 1; n <= N; n++) {
        int8_t chi_n = chi_values[n % q];
        if (chi_n == 0) continue;
        zeta_complex_t term = zc_pow_n((double)n, s);
        if (chi_n < 0) { term.re = -term.re; term.im = -term.im; }
        sum = zc_add(sum, term);
    }
    return sum;
}

/* ============================================================================
 * Special Values
 * ============================================================================ */

double zeta_basel(void)      { return M_PI * M_PI / 6.0; }
double zeta_4(void)          { return M_PI * M_PI * M_PI * M_PI / 90.0; }
double zeta_negative_1(void) { return -1.0 / 12.0; }
double zeta_at_zero(void)    { return -0.5; }

double zeta_gram_point(uint32_t n) {
    /* θ(g_n) = nπ. Approximate: g_n ≈ 2π·exp(W(n/(2πe))) */
    /* Simpler: g_n ≈ 2πn/ln(n) for large n */
    if (n == 0) return 0;
    double approx = 2.0 * M_PI * (double)n / log((double)n + 1.0);
    /* Newton refinement */
    for (int iter = 0; iter < 10; iter++) {
        double theta = zeta_theta(approx);
        double target = (double)n * M_PI;
        double error = theta - target;
        if (fabs(error) < 1e-10) break;
        /* dθ/dt ≈ ln(t/(2π))/2 */
        double dtheta = log(approx / (2.0 * M_PI)) * 0.5;
        if (fabs(dtheta) < 1e-15) break;
        approx -= error / dtheta;
    }
    return approx;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

zeta_config_t zeta_default_config(void) {
    return (zeta_config_t){
        .euler_maclaurin_terms = 100,
        .riemann_siegel_terms = 50,
        .zero_tolerance = 1e-8,
        .verify_zeros = true,
    };
}

zeta_engine_t* zeta_create(const zeta_config_t* config) {
    zeta_config_t cfg = config ? *config : zeta_default_config();
    zeta_engine_t* engine = nimcp_calloc(1, sizeof(*engine));
    if (!engine) return NULL;
    engine->config = cfg;
    init_bernoulli(engine->bernoulli, 32);
    engine->initialized = true;
    LOG_INFO(LOG_TAG, "Zeta engine created: EM_terms=%u, RS_terms=%u, tol=%.1e",
             cfg.euler_maclaurin_terms, cfg.riemann_siegel_terms, cfg.zero_tolerance);
    return engine;
}

void zeta_destroy(zeta_engine_t* engine) {
    if (!engine) return;
    nimcp_free(engine);
}

zeta_stats_t zeta_get_stats(const zeta_engine_t* engine) {
    if (!engine) return (zeta_stats_t){0};
    return engine->stats;
}
