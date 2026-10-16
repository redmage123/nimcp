/**
 * @file nimcp_real_analysis.h
 * @brief Real analysis engine: differentiation, integration, series, limits,
 *        sequences, and metric spaces.
 *
 * Numerical differentiation (forward, central, Richardson extrapolation).
 * Numerical integration (trapezoidal, Simpson's 1/3, Simpson's 3/8, Gaussian
 * quadrature 2-4 points, adaptive Simpson). Series convergence tests.
 * Taylor series evaluation. Epsilon-delta limits. Cauchy/monotone sequence
 * tests. Metric space distance functions.
 */

#ifndef NIMCP_REAL_ANALYSIS_H
#define NIMCP_REAL_ANALYSIS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------------------------
 * Constants
 * -------------------------------------------------------------------------- */

#define RA_MAX_SERIES_TERMS     10000
#define RA_MAX_GAUSS_POINTS     4
#define RA_ADAPTIVE_MAX_DEPTH   50
#define RA_DEFAULT_EPSILON      1.0e-10
#define RA_RICHARDSON_LEVELS    6
#define RA_MAX_DIMENSION        64

/* --------------------------------------------------------------------------
 * Function pointer types
 * -------------------------------------------------------------------------- */

/** Real-valued function of one real variable. */
typedef double (*ra_func_t)(double x, void *ctx);

/** Sequence a_n given index n. */
typedef double (*ra_sequence_t)(uint32_t n, void *ctx);

/** Series term a_n given index n. */
typedef double (*ra_series_term_t)(uint32_t n, void *ctx);

/* --------------------------------------------------------------------------
 * Convergence test results
 * -------------------------------------------------------------------------- */

typedef enum {
    RA_CONVERGES,
    RA_DIVERGES,
    RA_INCONCLUSIVE
} ra_convergence_t;

typedef struct {
    ra_convergence_t result;
    double           limit_value;      /* ratio/root limit if applicable */
    const char      *test_name;
} ra_convergence_result_t;

/* --------------------------------------------------------------------------
 * Integration result
 * -------------------------------------------------------------------------- */

typedef struct {
    double   value;
    double   error_estimate;
    uint32_t evaluations;
    bool     converged;
} ra_integral_result_t;

/* --------------------------------------------------------------------------
 * Metric space types
 * -------------------------------------------------------------------------- */

typedef enum {
    RA_METRIC_EUCLIDEAN,
    RA_METRIC_MANHATTAN,
    RA_METRIC_CHEBYSHEV,
    RA_METRIC_DISCRETE
} ra_metric_type_t;

/* --------------------------------------------------------------------------
 * Engine struct
 * -------------------------------------------------------------------------- */

typedef struct {
    double epsilon;                    /* default tolerance           */
    uint32_t max_series_terms;         /* default cap for series sums */
    uint32_t adaptive_max_depth;       /* recursion depth for adaptive */
} real_analysis_t;

/* --------------------------------------------------------------------------
 * Lifecycle
 * -------------------------------------------------------------------------- */

real_analysis_t *ra_create(void);
void             ra_destroy(real_analysis_t *ra);

/* --------------------------------------------------------------------------
 * Differentiation
 * -------------------------------------------------------------------------- */

double ra_diff_forward(ra_func_t f, double x, double h, void *ctx);
double ra_diff_central(ra_func_t f, double x, double h, void *ctx);
double ra_diff_richardson(ra_func_t f, double x, double h, void *ctx);
double ra_diff_second(ra_func_t f, double x, double h, void *ctx);

/* --------------------------------------------------------------------------
 * Integration
 * -------------------------------------------------------------------------- */

double ra_integrate_trapezoidal(ra_func_t f, double a, double b,
                                uint32_t n, void *ctx);
double ra_integrate_simpson13(ra_func_t f, double a, double b,
                              uint32_t n, void *ctx);
double ra_integrate_simpson38(ra_func_t f, double a, double b,
                              uint32_t n, void *ctx);
double ra_integrate_gauss(ra_func_t f, double a, double b,
                          uint32_t points, void *ctx);
ra_integral_result_t ra_integrate_adaptive_simpson(
    real_analysis_t *ra, ra_func_t f, double a, double b,
    double tol, void *ctx);

/* --------------------------------------------------------------------------
 * Series
 * -------------------------------------------------------------------------- */

/** Compute partial sum S_N = sum_{n=start}^{N} a_n. */
double ra_series_partial_sum(ra_series_term_t a, uint32_t start,
                             uint32_t N, void *ctx);

/** Convergence tests */
ra_convergence_result_t ra_test_ratio(ra_series_term_t a, uint32_t start,
                                      uint32_t terms, void *ctx);
ra_convergence_result_t ra_test_root(ra_series_term_t a, uint32_t start,
                                     uint32_t terms, void *ctx);
ra_convergence_result_t ra_test_alternating(ra_series_term_t a, uint32_t start,
                                            uint32_t terms, void *ctx);

/** Taylor series evaluations from series definition */
double ra_taylor_exp(double x, uint32_t terms);
double ra_taylor_sin(double x, uint32_t terms);
double ra_taylor_cos(double x, uint32_t terms);
double ra_taylor_ln1p(double x, uint32_t terms);  /* ln(1+x), |x|<=1 */

/* --------------------------------------------------------------------------
 * Limits
 * -------------------------------------------------------------------------- */

/** Numerical limit evaluation via sequence x_n -> x0 with decreasing step. */
typedef struct {
    double value;
    double estimated_error;
    bool   converged;
} ra_limit_result_t;

ra_limit_result_t ra_limit(ra_func_t f, double x0, double h_start,
                           double tol, void *ctx);

/* --------------------------------------------------------------------------
 * Sequences
 * -------------------------------------------------------------------------- */

/** Check Cauchy criterion: for all eps>0, exists N such that |a_m - a_n| < eps
 *  for m,n >= N.  Tests with supplied tolerance. */
bool ra_sequence_is_cauchy(ra_sequence_t a, uint32_t max_n,
                           double tol, void *ctx);

/** Check if sequence is monotone increasing (or decreasing). */
bool ra_sequence_is_monotone_increasing(ra_sequence_t a, uint32_t max_n,
                                        void *ctx);
bool ra_sequence_is_monotone_decreasing(ra_sequence_t a, uint32_t max_n,
                                        void *ctx);

/* --------------------------------------------------------------------------
 * Metric spaces
 * -------------------------------------------------------------------------- */

/** Distance between two points in R^dim under the given metric. */
double ra_metric_distance(const double *x, const double *y,
                          uint32_t dim, ra_metric_type_t metric);

/** Check Lipschitz condition: |f(x)-f(y)| <= L * |x-y| over sampled points.
 *  Returns estimated Lipschitz constant L, or -1 if not Lipschitz. */
double ra_lipschitz_estimate(ra_func_t f, double a, double b,
                             uint32_t samples, void *ctx);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_REAL_ANALYSIS_H */
