/**
 * @file nimcp_real_analysis.c
 * @brief Real analysis engine implementation
 */

#include "cognitive/math/nimcp_real_analysis.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>
#include <string.h>
#include <float.h>

#define LOG_TAG "REAL_ANALYSIS"

/* =========================================================================
 * Lifecycle
 * ========================================================================= */

real_analysis_t *ra_create(void) {
    real_analysis_t *ra = (real_analysis_t *)nimcp_calloc(1, sizeof(real_analysis_t));
    if (!ra) {
        LOG_ERROR("Failed to allocate real_analysis_t");
        return NULL;
    }
    ra->epsilon = RA_DEFAULT_EPSILON;
    ra->max_series_terms = RA_MAX_SERIES_TERMS;
    ra->adaptive_max_depth = RA_ADAPTIVE_MAX_DEPTH;
    LOG_INFO("Real analysis engine created");
    return ra;
}

void ra_destroy(real_analysis_t *ra) {
    if (!ra) return;
    nimcp_free(ra);
    LOG_INFO("Real analysis engine destroyed");
}

/* =========================================================================
 * Differentiation
 * ========================================================================= */

double ra_diff_forward(ra_func_t f, double x, double h, void *ctx) {
    return (f(x + h, ctx) - f(x, ctx)) / h;
}

double ra_diff_central(ra_func_t f, double x, double h, void *ctx) {
    return (f(x + h, ctx) - f(x - h, ctx)) / (2.0 * h);
}

double ra_diff_richardson(ra_func_t f, double x, double h, void *ctx) {
    /*
     * Richardson extrapolation on central differences.
     * D[k][j] = (4^j * D[k][j-1] - D[k-1][j-1]) / (4^j - 1)
     */
    double D[RA_RICHARDSON_LEVELS][RA_RICHARDSON_LEVELS];
    memset(D, 0, sizeof(D));

    for (int k = 0; k < RA_RICHARDSON_LEVELS; k++) {
        double hk = h / pow(2.0, (double)k);
        D[k][0] = ra_diff_central(f, x, hk, ctx);
    }

    for (int j = 1; j < RA_RICHARDSON_LEVELS; j++) {
        double p4j = pow(4.0, (double)j);
        for (int k = j; k < RA_RICHARDSON_LEVELS; k++) {
            D[k][j] = (p4j * D[k][j - 1] - D[k - 1][j - 1]) / (p4j - 1.0);
        }
    }

    return D[RA_RICHARDSON_LEVELS - 1][RA_RICHARDSON_LEVELS - 1];
}

double ra_diff_second(ra_func_t f, double x, double h, void *ctx) {
    return (f(x + h, ctx) - 2.0 * f(x, ctx) + f(x - h, ctx)) / (h * h);
}

/* =========================================================================
 * Integration
 * ========================================================================= */

double ra_integrate_trapezoidal(ra_func_t f, double a, double b,
                                uint32_t n, void *ctx) {
    if (n == 0) n = 1;
    double h = (b - a) / (double)n;
    double sum = 0.5 * (f(a, ctx) + f(b, ctx));
    for (uint32_t i = 1; i < n; i++) {
        sum += f(a + i * h, ctx);
    }
    return sum * h;
}

double ra_integrate_simpson13(ra_func_t f, double a, double b,
                              uint32_t n, void *ctx) {
    if (n == 0) n = 2;
    if (n % 2 != 0) n++;  /* Simpson's 1/3 requires even n */
    double h = (b - a) / (double)n;
    double sum = f(a, ctx) + f(b, ctx);
    for (uint32_t i = 1; i < n; i++) {
        double xi = a + i * h;
        sum += (i % 2 == 0) ? 2.0 * f(xi, ctx) : 4.0 * f(xi, ctx);
    }
    return sum * h / 3.0;
}

double ra_integrate_simpson38(ra_func_t f, double a, double b,
                              uint32_t n, void *ctx) {
    if (n == 0) n = 3;
    while (n % 3 != 0) n++;  /* Simpson's 3/8 requires n divisible by 3 */
    double h = (b - a) / (double)n;
    double sum = f(a, ctx) + f(b, ctx);
    for (uint32_t i = 1; i < n; i++) {
        double xi = a + i * h;
        if (i % 3 == 0) {
            sum += 2.0 * f(xi, ctx);
        } else {
            sum += 3.0 * f(xi, ctx);
        }
    }
    return sum * 3.0 * h / 8.0;
}

double ra_integrate_gauss(ra_func_t f, double a, double b,
                          uint32_t points, void *ctx) {
    /* Gauss-Legendre quadrature on [-1,1] mapped to [a,b] */
    /* Nodes and weights for 2, 3, 4 point rules */
    static const double nodes2[] = { -0.5773502691896258, 0.5773502691896258 };
    static const double weights2[] = { 1.0, 1.0 };

    static const double nodes3[] = {
        -0.7745966692414834, 0.0, 0.7745966692414834
    };
    static const double weights3[] = {
        0.5555555555555556, 0.8888888888888889, 0.5555555555555556
    };

    static const double nodes4[] = {
        -0.8611363115940526, -0.3399810435848563,
         0.3399810435848563,  0.8611363115940526
    };
    static const double weights4[] = {
        0.3478548451374539, 0.6521451548625461,
        0.6521451548625461, 0.3478548451374539
    };

    const double *nodes, *weights;
    uint32_t np;

    switch (points) {
        case 2:  nodes = nodes2;  weights = weights2;  np = 2; break;
        case 3:  nodes = nodes3;  weights = weights3;  np = 3; break;
        case 4:  nodes = nodes4;  weights = weights4;  np = 4; break;
        default: nodes = nodes3;  weights = weights3;  np = 3; break;
    }

    /* Map [-1,1] -> [a,b]: x = (b-a)/2 * t + (a+b)/2 */
    double half_len = (b - a) / 2.0;
    double mid = (a + b) / 2.0;
    double sum = 0.0;

    for (uint32_t i = 0; i < np; i++) {
        double x = half_len * nodes[i] + mid;
        sum += weights[i] * f(x, ctx);
    }

    return sum * half_len;
}

/* Adaptive Simpson helper */
static double simpson_rule(ra_func_t f, double a, double b, void *ctx) {
    double c = (a + b) / 2.0;
    double h = b - a;
    return (h / 6.0) * (f(a, ctx) + 4.0 * f(c, ctx) + f(b, ctx));
}

static ra_integral_result_t adaptive_simpson_impl(
    ra_func_t f, double a, double b, double tol,
    double S, double fa, double fb, double fc,
    uint32_t depth, uint32_t max_depth, uint32_t *evals, void *ctx)
{
    double c = (a + b) / 2.0;
    double d = (a + c) / 2.0;
    double e = (c + b) / 2.0;
    double fd = f(d, ctx);
    double fe = f(e, ctx);
    *evals += 2;

    double h = b - a;
    double S1 = (h / 12.0) * (fa + 4.0 * fd + fc);
    double S2 = (h / 12.0) * (fc + 4.0 * fe + fb);
    double Snew = S1 + S2;
    double err = (Snew - S) / 15.0;

    ra_integral_result_t result;
    if (depth >= max_depth || fabs(err) < tol) {
        result.value = Snew + err;  /* Richardson correction */
        result.error_estimate = fabs(err);
        result.evaluations = *evals;
        result.converged = (fabs(err) < tol);
        return result;
    }

    ra_integral_result_t left = adaptive_simpson_impl(
        f, a, c, tol / 2.0, S1, fa, fc, fd, depth + 1, max_depth, evals, ctx);
    ra_integral_result_t right = adaptive_simpson_impl(
        f, c, b, tol / 2.0, S2, fc, fb, fe, depth + 1, max_depth, evals, ctx);

    result.value = left.value + right.value;
    result.error_estimate = left.error_estimate + right.error_estimate;
    result.evaluations = *evals;
    result.converged = left.converged && right.converged;
    return result;
}

ra_integral_result_t ra_integrate_adaptive_simpson(
    real_analysis_t *ra, ra_func_t f, double a, double b,
    double tol, void *ctx)
{
    ra_integral_result_t result;
    memset(&result, 0, sizeof(result));
    if (!ra || !f) return result;

    uint32_t max_depth = ra->adaptive_max_depth;
    double fa = f(a, ctx);
    double fb = f(b, ctx);
    double fc = f((a + b) / 2.0, ctx);
    uint32_t evals = 3;

    double S = simpson_rule(f, a, b, ctx);
    evals += 3; /* simpson_rule evaluates 3 points */

    result = adaptive_simpson_impl(f, a, b, tol, S, fa, fb, fc,
                                   0, max_depth, &evals, ctx);
    result.evaluations = evals;
    return result;
}

/* =========================================================================
 * Series
 * ========================================================================= */

double ra_series_partial_sum(ra_series_term_t a, uint32_t start,
                             uint32_t N, void *ctx) {
    double sum = 0.0;
    for (uint32_t n = start; n <= N; n++) {
        sum += a(n, ctx);
    }
    return sum;
}

ra_convergence_result_t ra_test_ratio(ra_series_term_t a, uint32_t start,
                                      uint32_t terms, void *ctx) {
    ra_convergence_result_t r;
    r.test_name = "ratio";
    r.result = RA_INCONCLUSIVE;
    r.limit_value = 0.0;

    double sum_ratio = 0.0;
    uint32_t count = 0;

    for (uint32_t n = start; n < start + terms; n++) {
        double an = a(n, ctx);
        double an1 = a(n + 1, ctx);
        if (fabs(an) < 1e-300) continue;
        double ratio = fabs(an1 / an);
        sum_ratio += ratio;
        count++;
    }

    if (count == 0) return r;
    double L = sum_ratio / (double)count;
    r.limit_value = L;

    if (L < 1.0 - 1e-6) {
        r.result = RA_CONVERGES;
    } else if (L > 1.0 + 1e-6) {
        r.result = RA_DIVERGES;
    }
    return r;
}

ra_convergence_result_t ra_test_root(ra_series_term_t a, uint32_t start,
                                     uint32_t terms, void *ctx) {
    ra_convergence_result_t r;
    r.test_name = "root";
    r.result = RA_INCONCLUSIVE;
    r.limit_value = 0.0;

    double sum_root = 0.0;
    uint32_t count = 0;

    for (uint32_t n = start; n < start + terms; n++) {
        double an = fabs(a(n, ctx));
        if (an < 1e-300) continue;
        double root = pow(an, 1.0 / (double)n);
        if (!isfinite(root)) continue;
        sum_root += root;
        count++;
    }

    if (count == 0) return r;
    double L = sum_root / (double)count;
    r.limit_value = L;

    if (L < 1.0 - 1e-6) {
        r.result = RA_CONVERGES;
    } else if (L > 1.0 + 1e-6) {
        r.result = RA_DIVERGES;
    }
    return r;
}

ra_convergence_result_t ra_test_alternating(ra_series_term_t a, uint32_t start,
                                            uint32_t terms, void *ctx) {
    ra_convergence_result_t r;
    r.test_name = "alternating_series";
    r.result = RA_INCONCLUSIVE;
    r.limit_value = 0.0;

    /* Check: |a_{n+1}| <= |a_n| (eventually decreasing) and a_n -> 0 */
    bool decreasing = true;
    for (uint32_t n = start; n < start + terms - 1; n++) {
        if (fabs(a(n + 1, ctx)) > fabs(a(n, ctx)) + 1e-15) {
            decreasing = false;
            break;
        }
    }

    double last_term = fabs(a(start + terms - 1, ctx));
    bool tends_to_zero = (last_term < 1e-6);

    if (decreasing && tends_to_zero) {
        r.result = RA_CONVERGES;
        r.limit_value = last_term;
    }
    return r;
}

/* =========================================================================
 * Taylor series
 * ========================================================================= */

double ra_taylor_exp(double x, uint32_t terms) {
    double sum = 0.0;
    double term = 1.0;  /* x^0 / 0! */
    for (uint32_t n = 0; n < terms; n++) {
        sum += term;
        term *= x / (double)(n + 1);
    }
    return sum;
}

double ra_taylor_sin(double x, uint32_t terms) {
    double sum = 0.0;
    double term = x;    /* x^1 / 1! */
    for (uint32_t n = 0; n < terms; n++) {
        sum += term;
        term *= -x * x / (double)((2 * n + 2) * (2 * n + 3));
    }
    return sum;
}

double ra_taylor_cos(double x, uint32_t terms) {
    double sum = 0.0;
    double term = 1.0;  /* x^0 / 0! */
    for (uint32_t n = 0; n < terms; n++) {
        sum += term;
        term *= -x * x / (double)((2 * n + 1) * (2 * n + 2));
    }
    return sum;
}

double ra_taylor_ln1p(double x, uint32_t terms) {
    /* ln(1+x) = x - x^2/2 + x^3/3 - ... for |x| <= 1 */
    double sum = 0.0;
    double xn = x;
    for (uint32_t n = 1; n <= terms; n++) {
        sum += (n % 2 == 1 ? 1.0 : -1.0) * xn / (double)n;
        xn *= x;
    }
    return sum;
}

/* =========================================================================
 * Limits
 * ========================================================================= */

ra_limit_result_t ra_limit(ra_func_t f, double x0, double h_start,
                           double tol, void *ctx) {
    ra_limit_result_t result;
    memset(&result, 0, sizeof(result));

    double prev = f(x0 + h_start, ctx);
    double h = h_start;

    for (int iter = 0; iter < 60; iter++) {
        h *= 0.5;
        double cur = f(x0 + h, ctx);
        double err = fabs(cur - prev);

        if (err < tol) {
            result.value = cur;
            result.estimated_error = err;
            result.converged = true;
            return result;
        }
        prev = cur;
    }

    result.value = prev;
    result.estimated_error = fabs(f(x0 + h, ctx) - prev);
    result.converged = false;
    return result;
}

/* =========================================================================
 * Sequences
 * ========================================================================= */

bool ra_sequence_is_cauchy(ra_sequence_t a, uint32_t max_n,
                           double tol, void *ctx) {
    /* Check tail: for n, m >= max_n/2, |a_n - a_m| < tol */
    uint32_t start = max_n / 2;
    if (start < 1) start = 1;

    for (uint32_t n = start; n < max_n; n++) {
        for (uint32_t m = n + 1; m < max_n && m < n + 20; m++) {
            if (fabs(a(n, ctx) - a(m, ctx)) >= tol) {
                return false;
            }
        }
    }
    return true;
}

bool ra_sequence_is_monotone_increasing(ra_sequence_t a, uint32_t max_n,
                                        void *ctx) {
    for (uint32_t n = 0; n + 1 < max_n; n++) {
        if (a(n + 1, ctx) < a(n, ctx) - 1e-15) return false;
    }
    return true;
}

bool ra_sequence_is_monotone_decreasing(ra_sequence_t a, uint32_t max_n,
                                        void *ctx) {
    for (uint32_t n = 0; n + 1 < max_n; n++) {
        if (a(n + 1, ctx) > a(n, ctx) + 1e-15) return false;
    }
    return true;
}

/* =========================================================================
 * Metric spaces
 * ========================================================================= */

double ra_metric_distance(const double *x, const double *y,
                          uint32_t dim, ra_metric_type_t metric) {
    if (!x || !y || dim == 0) return 0.0;

    switch (metric) {
        case RA_METRIC_EUCLIDEAN: {
            double sum = 0.0;
            for (uint32_t i = 0; i < dim; i++) {
                double d = x[i] - y[i];
                sum += d * d;
            }
            return sqrt(sum);
        }
        case RA_METRIC_MANHATTAN: {
            double sum = 0.0;
            for (uint32_t i = 0; i < dim; i++) {
                sum += fabs(x[i] - y[i]);
            }
            return sum;
        }
        case RA_METRIC_CHEBYSHEV: {
            double mx = 0.0;
            for (uint32_t i = 0; i < dim; i++) {
                double d = fabs(x[i] - y[i]);
                if (d > mx) mx = d;
            }
            return mx;
        }
        case RA_METRIC_DISCRETE: {
            for (uint32_t i = 0; i < dim; i++) {
                if (fabs(x[i] - y[i]) > 1e-15) return 1.0;
            }
            return 0.0;
        }
        default:
            return 0.0;
    }
}

double ra_lipschitz_estimate(ra_func_t f, double a, double b,
                             uint32_t samples, void *ctx) {
    if (!f || samples < 2) return -1.0;
    double h = (b - a) / (double)(samples - 1);
    double max_L = 0.0;

    for (uint32_t i = 0; i < samples; i++) {
        double xi = a + i * h;
        double fi = f(xi, ctx);
        for (uint32_t j = i + 1; j < samples; j++) {
            double xj = a + j * h;
            double fj = f(xj, ctx);
            double dx = fabs(xj - xi);
            if (dx < 1e-15) continue;
            double L = fabs(fj - fi) / dx;
            if (L > max_L) max_L = L;
        }
    }
    return max_L;
}
