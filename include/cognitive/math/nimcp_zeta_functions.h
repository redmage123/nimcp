/**
 * @file nimcp_zeta_functions.h
 * @brief Riemann Zeta Function & L-functions — zeros, analytic continuation, RH
 *
 * WHAT: Numerical evaluation of ζ(s) for complex s, non-trivial zero finding,
 *       Z-function (Hardy's function), Gram points, zero counting (N(T)),
 *       random matrix theory connection (GUE statistics), Dirichlet L-functions.
 * WHY:  The Riemann Hypothesis is the deepest unsolved problem connecting
 *       prime distribution to complex analysis. A brain that can reason about
 *       it needs computational access to the zeta function and its zeros.
 * HOW:  Euler-Maclaurin summation for ζ(s), Riemann-Siegel formula for Z(t),
 *       Odlyzko-Schönhage for high zeros, GUE pair correlation.
 *
 * THEORETICAL FOUNDATION:
 *   ζ(s) = Σ(n=1..∞) 1/n^s  (Re(s) > 1)
 *   Analytic continuation via functional equation:
 *     ζ(s) = 2^s π^(s-1) sin(πs/2) Γ(1-s) ζ(1-s)
 *   Riemann Hypothesis: all non-trivial zeros have Re(s) = 1/2
 *   Hardy's Z-function: Z(t) = e^(iθ(t)) ζ(1/2+it), real-valued on critical line
 *   Riemann-Siegel: Z(t) ≈ 2Σ(n=1..N) cos(θ(t)-t·ln(n))/√n + R(t)
 *   Zero counting: N(T) = T/(2π)·ln(T/(2πe)) + 7/8 + S(T)
 *   GUE pair correlation: 1 - (sin(πx)/(πx))² (Montgomery, 1973)
 */

#ifndef NIMCP_ZETA_FUNCTIONS_H
#define NIMCP_ZETA_FUNCTIONS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define ZETA_MAX_ZEROS          4096
#define ZETA_MAX_TERMS          10000   /* max terms in Euler-Maclaurin */
#define ZETA_EULER_MASCHERONI   0.5772156649015329

/* Known non-trivial zeros (imaginary parts, first 10) */
#define ZETA_ZERO_1     14.134725141734693
#define ZETA_ZERO_2     21.022039638771555
#define ZETA_ZERO_3     25.010857580145688
#define ZETA_ZERO_4     30.424876125859513
#define ZETA_ZERO_5     32.935061587739189
#define ZETA_ZERO_6     37.586178158825671
#define ZETA_ZERO_7     40.918719012147500
#define ZETA_ZERO_8     43.327073280914999
#define ZETA_ZERO_9     48.005150881167160
#define ZETA_ZERO_10    49.773832477672302

/* ============================================================================
 * Complex Number (double precision required for zeta)
 * ============================================================================ */

typedef struct {
    double re;
    double im;
} zeta_complex_t;

/* ============================================================================
 * Zero Record
 * ============================================================================ */

typedef struct {
    double      t;              /* imaginary part (zero is at s = 1/2 + it) */
    double      Z_value;        /* Z(t) value (should be ~0 at zero) */
    double      gram_point;     /* nearest Gram point */
    uint32_t    index;          /* ordinal: n-th zero */
    bool        verified;       /* verified to be on critical line */
} zeta_zero_t;

/* ============================================================================
 * Zero Spacing Statistics (for RMT comparison)
 * ============================================================================ */

typedef struct {
    double      mean_spacing;       /* average gap between consecutive zeros */
    double      normalized_spacing; /* spacing / mean_spacing */
    double      pair_correlation;   /* Montgomery's pair correlation */
    double      nearest_neighbor;   /* distribution of nearest spacings */
    double      gue_fit;            /* how well spacings match GUE (0=perfect) */
    uint32_t    num_zeros_analyzed;
} zeta_spacing_stats_t;

/* ============================================================================
 * Config
 * ============================================================================ */

typedef struct {
    uint32_t    euler_maclaurin_terms; /* terms in EM summation (default: 100) */
    uint32_t    riemann_siegel_terms;  /* terms in RS formula (default: 50) */
    double      zero_tolerance;        /* |Z(t)| < tol means zero (default: 1e-8) */
    bool        verify_zeros;          /* check each zero is on critical line */
} zeta_config_t;

/* ============================================================================
 * Stats
 * ============================================================================ */

typedef struct {
    uint64_t    zeta_evaluations;
    uint64_t    z_evaluations;
    uint64_t    zeros_found;
    uint64_t    zeros_verified_on_critical_line;
    double      highest_zero_found;
    double      max_gue_deviation;
} zeta_stats_t;

/* ============================================================================
 * Engine
 * ============================================================================ */

typedef struct zeta_engine {
    zeta_zero_t     zeros[ZETA_MAX_ZEROS];
    uint32_t        num_zeros;
    zeta_spacing_stats_t spacing;
    zeta_config_t   config;
    zeta_stats_t    stats;
    /* Precomputed Bernoulli numbers for Euler-Maclaurin */
    double          bernoulli[32];
    bool            initialized;
} zeta_engine_t;

/* ============================================================================
 * API
 * ============================================================================ */

zeta_engine_t* zeta_create(const zeta_config_t* config);
void zeta_destroy(zeta_engine_t* engine);

/* === Zeta Function Evaluation === */

/** Evaluate ζ(s) for complex s (Euler-Maclaurin + functional equation) */
zeta_complex_t zeta_eval(zeta_engine_t* engine, zeta_complex_t s);

/** Evaluate ζ(s) for real s > 1 (fast path) */
double zeta_eval_real(zeta_engine_t* engine, double s);

/** Hardy's Z-function: Z(t) = e^(iθ(t))·ζ(1/2+it), real-valued */
double zeta_Z(zeta_engine_t* engine, double t);

/** Riemann-Siegel theta function: θ(t) = arg(Γ(1/4+it/2)) - t·ln(π)/2 */
double zeta_theta(double t);

/** Gram points: θ(g_n) = nπ */
double zeta_gram_point(uint32_t n);

/* === Zero Finding === */

/** Find zeros of Z(t) in range [t_low, t_high] by sign changes */
uint32_t zeta_find_zeros_in_range(zeta_engine_t* engine,
                                    double t_low, double t_high,
                                    double step_size);

/** Find the n-th non-trivial zero (using Gram point search) */
double zeta_find_nth_zero(zeta_engine_t* engine, uint32_t n);

/** Verify a zero is on the critical line (Re(s)=1/2) */
bool zeta_verify_on_critical_line(zeta_engine_t* engine, double t);

/** Load known zeros (first 10 hardcoded, more via computation) */
void zeta_load_known_zeros(zeta_engine_t* engine);

/* === Zero Counting === */

/** N(T): number of zeros with 0 < Im(s) < T */
double zeta_N(double T);

/** S(T): argument of ζ(1/2+iT)/π (oscillatory correction) */
double zeta_S(zeta_engine_t* engine, double T);

/* === Random Matrix Theory Connection === */

/** Compute pair correlation of zero spacings */
double zeta_pair_correlation(const zeta_engine_t* engine, double x);

/** GUE pair correlation prediction: 1 - (sin(πx)/(πx))² */
double zeta_gue_pair_correlation(double x);

/** Compute spacing statistics and compare to GUE */
zeta_spacing_stats_t zeta_compute_spacing_stats(zeta_engine_t* engine);

/** Wigner surmise for nearest-neighbor spacing: p(s) = (π/2)s·exp(-πs²/4) */
double zeta_wigner_surmise(double s);

/* === Dirichlet L-functions === */

/** Evaluate L(s,χ) for a Dirichlet character χ mod q */
zeta_complex_t zeta_dirichlet_L(zeta_engine_t* engine,
                                  zeta_complex_t s,
                                  uint32_t q, const int8_t* chi_values);

/* === Special Values === */

/** ζ(2) = π²/6 (Basel problem) */
double zeta_basel(void);

/** ζ(4) = π⁴/90 */
double zeta_4(void);

/** ζ(-1) = -1/12 (Ramanujan summation) */
double zeta_negative_1(void);

/** ζ(0) = -1/2 */
double zeta_at_zero(void);

/* === Functional Equation === */

/** Apply functional equation: ζ(s) = 2^s·π^(s-1)·sin(πs/2)·Γ(1-s)·ζ(1-s) */
zeta_complex_t zeta_functional_equation(zeta_engine_t* engine, zeta_complex_t s);

/** Log-gamma for complex argument (Stirling approximation) */
zeta_complex_t zeta_log_gamma(zeta_complex_t z);

/** Default config */
zeta_config_t zeta_default_config(void);

/** Get stats */
zeta_stats_t zeta_get_stats(const zeta_engine_t* engine);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ZETA_FUNCTIONS_H */
