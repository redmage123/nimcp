/**
 * @file nimcp_probability.c
 * @brief Probability, statistics, and hypothesis testing implementation
 */

#include "cognitive/math/nimcp_probability.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define LOG_TAG "PROBABILITY"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef M_SQRT2
#define M_SQRT2 1.41421356237309504880
#endif

/* ================================================================
 * INTERNAL HELPERS
 * ================================================================ */

/* xorshift128+ RNG */
static uint64_t xorshift128plus(uint64_t* state) {
    uint64_t s1 = state[0];
    uint64_t s0 = state[1];
    state[0] = s0;
    s1 ^= s1 << 23;
    s1 ^= s1 >> 17;
    s1 ^= s0;
    s1 ^= s0 >> 26;
    state[1] = s1;
    return state[0] + state[1];
}

/* Error function approximation (Abramowitz & Stegun 7.1.26) */
static double erf_approx(double x) {
    double sign = (x >= 0) ? 1.0 : -1.0;
    x = fabs(x);
    double t = 1.0 / (1.0 + 0.3275911 * x);
    double t2 = t * t, t3 = t2 * t, t4 = t3 * t, t5 = t4 * t;
    double y = 1.0 - (0.254829592 * t - 0.284496736 * t2 + 1.421413741 * t3
                       - 1.453152027 * t4 + 1.061405429 * t5) * exp(-x * x);
    return sign * y;
}

/* Standard normal CDF (Abramowitz & Stegun) */
static double normal_cdf(double x) {
    return 0.5 * (1.0 + erf_approx(x / M_SQRT2));
}

/* Standard normal PDF */
static double normal_pdf(double x) {
    return exp(-0.5 * x * x) / sqrt(2.0 * M_PI);
}

/* Inverse normal CDF (rational approximation, Beasley-Springer-Moro) */
static double normal_quantile(double p) {
    if (p <= 0.0) return -INFINITY;
    if (p >= 1.0) return INFINITY;
    if (fabs(p - 0.5) < 1e-15) return 0.0;

    /* Rational approximation */
    double t;
    if (p < 0.5) {
        t = sqrt(-2.0 * log(p));
    } else {
        t = sqrt(-2.0 * log(1.0 - p));
    }
    /* Coefficients for approximation */
    double c0 = 2.515517, c1 = 0.802853, c2 = 0.010328;
    double d1 = 1.432788, d2 = 0.189269, d3 = 0.001308;
    double result = t - (c0 + c1 * t + c2 * t * t) / (1.0 + d1 * t + d2 * t * t + d3 * t * t * t);
    return (p < 0.5) ? -result : result;
}

/* Log-gamma via Stirling approximation for chi-squared/gamma CDF */
static double lgamma_approx(double x) {
    /* Use Lanczos approximation */
    static const double c[7] = {
        1.000000000190015, 76.18009172947146, -86.50532032941677,
        24.01409824083091, -1.231739572450155, 0.001208650973866179,
        -0.000005395239384953
    };
    double sum = c[0];
    for (int i = 1; i < 7; i++) sum += c[i] / (x + (double)i);
    double t = x + 5.5;
    return 0.5 * log(2.0 * M_PI) + (x + 0.5) * log(t) - t + log(sum / x);
}

/* Regularized lower incomplete gamma function P(a, x) via series */
static double gamma_inc_lower(double a, double x) {
    if (x < 0.0) return 0.0;
    if (x == 0.0) return 0.0;
    double sum = 0.0, term = 1.0 / a;
    sum = term;
    for (int n = 1; n < 200; n++) {
        term *= x / (a + (double)n);
        sum += term;
        if (fabs(term) < 1e-15 * fabs(sum)) break;
    }
    return sum * exp(-x + a * log(x) - lgamma_approx(a));
}

/* Factorial for small n (used in Poisson) */
static double factorial_d(int n) {
    double r = 1.0;
    for (int i = 2; i <= n; i++) r *= (double)i;
    return r;
}

static int compare_doubles(const void* a, const void* b) {
    double da = *(const double*)a, db = *(const double*)b;
    return (da > db) - (da < db);
}

/* ================================================================
 * LIFECYCLE
 * ================================================================ */

probability_engine_t* probability_create(uint64_t seed) {
    probability_engine_t* eng = (probability_engine_t*)nimcp_calloc(1, sizeof(probability_engine_t));
    if (!eng) {
        LOG_ERROR(LOG_TAG, "Failed to allocate probability engine");
        return NULL;
    }
    eng->rng_state[0] = seed ? seed : 0x12345678ABCDEF01ULL;
    eng->rng_state[1] = seed ? (seed * 6364136223846793005ULL + 1) : 0xFEDCBA9876543210ULL;
    eng->mc_default_samples = PROB_MONTE_CARLO_DEFAULT;
    LOG_INFO(LOG_TAG, "Probability engine created (seed=%llu)", (unsigned long long)seed);
    return eng;
}

void probability_destroy(probability_engine_t* eng) {
    if (!eng) return;
    nimcp_free(eng);
}

/* ================================================================
 * RNG
 * ================================================================ */

double prob_random_uniform(probability_engine_t* eng) {
    if (!eng) return 0.0;
    uint64_t r = xorshift128plus(eng->rng_state);
    return (double)(r >> 11) / (double)(1ULL << 53);
}

double prob_random_normal(probability_engine_t* eng, double mu, double sigma) {
    /* Box-Muller transform */
    double u1 = prob_random_uniform(eng);
    double u2 = prob_random_uniform(eng);
    if (u1 <= 1e-15) u1 = 1e-15;
    if (u1 >= 1.0 - 1e-15) u1 = 1.0 - 1e-15;  /* log(1)=0 → sqrt(0)=0 */
    double z = sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
    return mu + sigma * z;
}

/* ================================================================
 * DISTRIBUTION FUNCTIONS
 * ================================================================ */

double prob_pdf(const dist_params_t* d, double x) {
    if (!d) return 0.0;
    switch (d->type) {
        case DIST_NORMAL:
            return normal_pdf((x - d->mu) / d->sigma) / d->sigma;
        case DIST_UNIFORM:
            return (x >= d->a && x <= d->b) ? 1.0 / (d->b - d->a) : 0.0;
        case DIST_EXPONENTIAL:
            return (x >= 0.0) ? d->lambda * exp(-d->lambda * x) : 0.0;
        case DIST_POISSON: {
            int k = (int)x;
            if (k < 0) return 0.0;
            return exp(-d->lambda) * pow(d->lambda, (double)k) / factorial_d(k);
        }
        case DIST_BINOMIAL: {
            int k = (int)x;
            if (k < 0 || k > d->n_trials) return 0.0;
            /* C(n,k) * p^k * (1-p)^(n-k) via log to avoid overflow */
            double log_binom = lgamma_approx((double)(d->n_trials + 1))
                             - lgamma_approx((double)(k + 1))
                             - lgamma_approx((double)(d->n_trials - k + 1));
            return exp(log_binom + (double)k * log(d->p) +
                       (double)(d->n_trials - k) * log(1.0 - d->p));
        }
        case DIST_GEOMETRIC:
            if (x < 1.0) return 0.0;
            return d->p * pow(1.0 - d->p, x - 1.0);
        case DIST_CHI_SQUARED: {
            if (x <= 0.0) return 0.0;
            double k2 = d->df / 2.0;
            return exp((k2 - 1.0) * log(x) - x / 2.0 - k2 * log(2.0) - lgamma_approx(k2));
        }
    }
    return 0.0;
}

double prob_cdf(const dist_params_t* d, double x) {
    if (!d) return 0.0;
    switch (d->type) {
        case DIST_NORMAL:
            return normal_cdf((x - d->mu) / d->sigma);
        case DIST_UNIFORM:
            if (x < d->a) return 0.0;
            if (x > d->b) return 1.0;
            return (x - d->a) / (d->b - d->a);
        case DIST_EXPONENTIAL:
            return (x >= 0.0) ? 1.0 - exp(-d->lambda * x) : 0.0;
        case DIST_POISSON: {
            double sum = 0.0;
            for (int k = 0; k <= (int)x; k++) {
                dist_params_t tmp = *d;
                sum += prob_pdf(&tmp, (double)k);
            }
            return sum;
        }
        case DIST_BINOMIAL: {
            double sum = 0.0;
            for (int k = 0; k <= (int)x && k <= d->n_trials; k++) {
                dist_params_t tmp = *d;
                sum += prob_pdf(&tmp, (double)k);
            }
            return sum;
        }
        case DIST_GEOMETRIC: {
            if (x < 1.0) return 0.0;
            return 1.0 - pow(1.0 - d->p, floor(x));
        }
        case DIST_CHI_SQUARED:
            return gamma_inc_lower(d->df / 2.0, x / 2.0);
    }
    return 0.0;
}

double prob_quantile(const dist_params_t* d, double p) {
    if (!d || p < 0.0 || p > 1.0) return 0.0;
    switch (d->type) {
        case DIST_NORMAL:
            return d->mu + d->sigma * normal_quantile(p);
        case DIST_UNIFORM:
            return d->a + p * (d->b - d->a);
        case DIST_EXPONENTIAL:
            return (d->lambda > 0.0) ? -log(1.0 - p) / d->lambda : 0.0;
        default: {
            /* Bisection for other distributions */
            double lo = -1000.0, hi = 1000.0;
            for (int i = 0; i < 100; i++) {
                double mid = (lo + hi) / 2.0;
                if (prob_cdf(d, mid) < p) lo = mid; else hi = mid;
            }
            return (lo + hi) / 2.0;
        }
    }
}

/* ================================================================
 * DESCRIPTIVE STATISTICS
 * ================================================================ */

descriptive_stats_t prob_descriptive(const double* data, int n) {
    descriptive_stats_t s;
    memset(&s, 0, sizeof(s));
    if (!data || n <= 0) return s;
    s.n = n;

    /* Mean */
    double sum = 0.0;
    s.min = data[0]; s.max = data[0];
    for (int i = 0; i < n; i++) {
        sum += data[i];
        if (data[i] < s.min) s.min = data[i];
        if (data[i] > s.max) s.max = data[i];
    }
    s.mean = sum / (double)n;

    /* Variance (Bessel-corrected), skewness, kurtosis */
    double m2 = 0.0, m3 = 0.0, m4 = 0.0;
    for (int i = 0; i < n; i++) {
        double d = data[i] - s.mean;
        m2 += d * d;
        m3 += d * d * d;
        m4 += d * d * d * d;
    }
    s.variance = (n > 1) ? m2 / (double)(n - 1) : 0.0;
    s.std_dev = sqrt(s.variance);

    if (s.std_dev > 1e-15 && n > 2) {
        double s3 = s.std_dev * s.std_dev * s.std_dev;
        s.skewness = (m3 / (double)n) / s3;
    }
    if (s.std_dev > 1e-15 && n > 3) {
        double s4 = s.variance * s.variance;
        s.kurtosis = (m4 / (double)n) / s4 - 3.0; /* excess kurtosis */
    }

    /* Median (copy and sort) */
    double* sorted = (double*)nimcp_calloc((size_t)n, sizeof(double));
    if (sorted) {
        memcpy(sorted, data, (size_t)n * sizeof(double));
        qsort(sorted, (size_t)n, sizeof(double), compare_doubles);
        if (n % 2 == 1) {
            s.median = sorted[n / 2];
        } else {
            s.median = (sorted[n / 2 - 1] + sorted[n / 2]) / 2.0;
        }
        nimcp_free(sorted);
    }
    return s;
}

double prob_percentile(const double* data, int n, double p) {
    if (!data || n <= 0) return 0.0;
    double* sorted = (double*)nimcp_calloc((size_t)n, sizeof(double));
    if (!sorted) return 0.0;
    memcpy(sorted, data, (size_t)n * sizeof(double));
    qsort(sorted, (size_t)n, sizeof(double), compare_doubles);

    double idx = p / 100.0 * (double)(n - 1);
    int lo = (int)floor(idx);
    int hi = (int)ceil(idx);
    if (lo < 0) lo = 0;
    if (hi >= n) hi = n - 1;
    double frac = idx - (double)lo;
    double result = sorted[lo] * (1.0 - frac) + sorted[hi] * frac;
    nimcp_free(sorted);
    return result;
}

/* ================================================================
 * CORRELATION
 * ================================================================ */

double prob_pearson(const double* x, const double* y, int n) {
    if (!x || !y || n < 2) return 0.0;
    double sx = 0, sy = 0, sxy = 0, sx2 = 0, sy2 = 0;
    for (int i = 0; i < n; i++) {
        sx += x[i]; sy += y[i];
        sxy += x[i] * y[i];
        sx2 += x[i] * x[i]; sy2 += y[i] * y[i];
    }
    double num = (double)n * sxy - sx * sy;
    double den = sqrt(((double)n * sx2 - sx * sx) * ((double)n * sy2 - sy * sy));
    return (den > 1e-15) ? num / den : 0.0;
}

double prob_spearman(const double* x, const double* y, int n) {
    if (!x || !y || n < 2) return 0.0;

    /* Compute ranks */
    double* rx = (double*)nimcp_calloc((size_t)n, sizeof(double));
    double* ry = (double*)nimcp_calloc((size_t)n, sizeof(double));
    int* idx = (int*)nimcp_calloc((size_t)n, sizeof(int));
    if (!rx || !ry || !idx) { nimcp_free(rx); nimcp_free(ry); nimcp_free(idx); return 0.0; }

    /* Rank x */
    for (int i = 0; i < n; i++) idx[i] = i;
    /* Simple insertion sort for ranking */
    for (int i = 1; i < n; i++) {
        int key = idx[i];
        int j = i - 1;
        while (j >= 0 && x[idx[j]] > x[key]) { idx[j + 1] = idx[j]; j--; }
        idx[j + 1] = key;
    }
    for (int i = 0; i < n; i++) rx[idx[i]] = (double)(i + 1);

    /* Rank y */
    for (int i = 0; i < n; i++) idx[i] = i;
    for (int i = 1; i < n; i++) {
        int key = idx[i];
        int j = i - 1;
        while (j >= 0 && y[idx[j]] > y[key]) { idx[j + 1] = idx[j]; j--; }
        idx[j + 1] = key;
    }
    for (int i = 0; i < n; i++) ry[idx[i]] = (double)(i + 1);

    double result = prob_pearson(rx, ry, n);
    nimcp_free(rx); nimcp_free(ry); nimcp_free(idx);
    return result;
}

/* ================================================================
 * HYPOTHESIS TESTS
 * ================================================================ */

test_result_t prob_z_test(const double* data, int n, double mu0, double sigma) {
    test_result_t r;
    memset(&r, 0, sizeof(r));
    if (!data || n <= 0 || sigma <= 0.0) return r;

    double mean = 0.0;
    for (int i = 0; i < n; i++) mean += data[i];
    mean /= (double)n;

    r.statistic = (mean - mu0) / (sigma / sqrt((double)n));
    r.p_value = 2.0 * (1.0 - normal_cdf(fabs(r.statistic))); /* two-tailed */
    r.df = (double)n;
    r.reject_null = (r.p_value < 0.05);
    return r;
}

test_result_t prob_t_test_welch(const double* x, int nx, const double* y, int ny) {
    test_result_t r;
    memset(&r, 0, sizeof(r));
    if (!x || !y || nx < 2 || ny < 2) return r;

    descriptive_stats_t sx = prob_descriptive(x, nx);
    descriptive_stats_t sy = prob_descriptive(y, ny);

    double se = sqrt(sx.variance / (double)nx + sy.variance / (double)ny);
    if (se < 1e-15) return r;

    r.statistic = (sx.mean - sy.mean) / se;

    /* Welch-Satterthwaite degrees of freedom */
    double v1 = sx.variance / (double)nx, v2 = sy.variance / (double)ny;
    double num = (v1 + v2) * (v1 + v2);
    double den = v1 * v1 / (double)(nx - 1) + v2 * v2 / (double)(ny - 1);
    r.df = (den > 0) ? num / den : 1.0;

    /* Approximate p-value using normal for large df, t-distribution otherwise */
    /* For simplicity, use normal approximation (valid for df > 30) */
    r.p_value = 2.0 * (1.0 - normal_cdf(fabs(r.statistic)));
    r.reject_null = (r.p_value < 0.05);
    return r;
}

test_result_t prob_chi_squared_test(const double* observed, const double* expected, int k) {
    test_result_t r;
    memset(&r, 0, sizeof(r));
    if (!observed || !expected || k < 2) return r;

    double chi2 = 0.0;
    for (int i = 0; i < k; i++) {
        if (expected[i] > 0.0) {
            double diff = observed[i] - expected[i];
            chi2 += diff * diff / expected[i];
        }
    }
    r.statistic = chi2;
    r.df = (double)(k - 1);

    /* CDF of chi-squared distribution */
    dist_params_t dp;
    memset(&dp, 0, sizeof(dp));
    dp.type = DIST_CHI_SQUARED;
    dp.df = r.df;
    r.p_value = 1.0 - prob_cdf(&dp, chi2);
    r.reject_null = (r.p_value < 0.05);
    return r;
}

/* ================================================================
 * BAYESIAN INFERENCE (discrete)
 * ================================================================ */

bayesian_result_t prob_bayesian_discrete(const double* prior, const double* likelihood,
                                         int n_hypotheses) {
    bayesian_result_t r;
    memset(&r, 0, sizeof(r));
    if (!prior || !likelihood || n_hypotheses <= 0 || n_hypotheses > PROB_MAX_HYPOTHESES) return r;
    r.n_hypotheses = n_hypotheses;

    /* posterior ∝ prior × likelihood */
    double evidence = 0.0;
    for (int i = 0; i < n_hypotheses; i++) {
        r.posterior[i] = prior[i] * likelihood[i];
        evidence += r.posterior[i];
    }
    if (evidence > 0.0) {
        for (int i = 0; i < n_hypotheses; i++) r.posterior[i] /= evidence;
    }

    /* Bayes factor: H0 vs H1 */
    if (n_hypotheses >= 2 && likelihood[1] > 0.0) {
        r.bayes_factor = likelihood[0] / likelihood[1];
    }

    LOG_DEBUG(LOG_TAG, "Bayesian: %d hypotheses, evidence=%.6f", n_hypotheses, evidence);
    return r;
}

/* ================================================================
 * REGRESSION
 * ================================================================ */

regression_result_t* prob_linear_regression(const double* x, const double* y, int n) {
    if (!x || !y || n < 2) return NULL;

    regression_result_t* r = (regression_result_t*)nimcp_calloc(1, sizeof(regression_result_t));
    if (!r) return NULL;
    r->n_coeffs = 2;
    r->beta = (double*)nimcp_calloc(2, sizeof(double));
    if (!r->beta) { nimcp_free(r); return NULL; }

    double sx = 0, sy = 0, sxy = 0, sx2 = 0;
    for (int i = 0; i < n; i++) {
        sx += x[i]; sy += y[i];
        sxy += x[i] * y[i]; sx2 += x[i] * x[i];
    }
    double xbar = sx / (double)n, ybar = sy / (double)n;

    double num = 0.0, den = 0.0;
    for (int i = 0; i < n; i++) {
        num += (x[i] - xbar) * (y[i] - ybar);
        den += (x[i] - xbar) * (x[i] - xbar);
    }

    r->beta[1] = (den > 1e-15) ? num / den : 0.0; /* slope */
    r->beta[0] = ybar - r->beta[1] * xbar;         /* intercept */

    /* R-squared */
    double ss_res = 0.0, ss_tot = 0.0;
    for (int i = 0; i < n; i++) {
        double pred = r->beta[0] + r->beta[1] * x[i];
        ss_res += (y[i] - pred) * (y[i] - pred);
        ss_tot += (y[i] - ybar) * (y[i] - ybar);
    }
    r->r_squared = (ss_tot > 1e-15) ? 1.0 - ss_res / ss_tot : 0.0;
    r->mse = ss_res / (double)n;
    r->adj_r_squared = 1.0 - (1.0 - r->r_squared) * (double)(n - 1) / (double)(n - 2);

    LOG_DEBUG(LOG_TAG, "Linear regression: slope=%.6f, intercept=%.6f, R2=%.4f",
              r->beta[1], r->beta[0], r->r_squared);
    return r;
}

regression_result_t* prob_multiple_regression(const double* X, const double* y,
                                              int n, int p) {
    if (!X || !y || n < p + 1 || p < 1) return NULL;

    /* Normal equations: beta = (X^T X)^{-1} X^T y */
    /* Build X_aug = [1 | X] (n x (p+1)) */
    int pp = p + 1;
    double* XtX = (double*)nimcp_calloc((size_t)(pp * pp), sizeof(double));
    double* Xty = (double*)nimcp_calloc((size_t)pp, sizeof(double));
    if (!XtX || !Xty) { nimcp_free(XtX); nimcp_free(Xty); return NULL; }

    /* Compute X^T X and X^T y with augmented column */
    for (int i = 0; i < pp; i++) {
        for (int j = 0; j < pp; j++) {
            double sum = 0.0;
            for (int k = 0; k < n; k++) {
                double xi = (i == 0) ? 1.0 : X[k * p + (i - 1)];
                double xj = (j == 0) ? 1.0 : X[k * p + (j - 1)];
                sum += xi * xj;
            }
            XtX[i * pp + j] = sum;
        }
        double sum = 0.0;
        for (int k = 0; k < n; k++) {
            double xi = (i == 0) ? 1.0 : X[k * p + (i - 1)];
            sum += xi * y[k];
        }
        Xty[i] = sum;
    }

    /* Solve via Gauss-Jordan elimination on [XtX | Xty] */
    double* aug = (double*)nimcp_calloc((size_t)(pp * (pp + 1)), sizeof(double));
    if (!aug) { nimcp_free(XtX); nimcp_free(Xty); return NULL; }

    for (int i = 0; i < pp; i++) {
        for (int j = 0; j < pp; j++) aug[i * (pp + 1) + j] = XtX[i * pp + j];
        aug[i * (pp + 1) + pp] = Xty[i];
    }

    for (int k = 0; k < pp; k++) {
        /* Pivot */
        int max_row = k;
        double max_val = fabs(aug[k * (pp + 1) + k]);
        for (int i = k + 1; i < pp; i++) {
            double v = fabs(aug[i * (pp + 1) + k]);
            if (v > max_val) { max_val = v; max_row = i; }
        }
        if (max_val < 1e-12) { nimcp_free(aug); nimcp_free(XtX); nimcp_free(Xty); return NULL; }
        if (max_row != k) {
            for (int j = 0; j <= pp; j++) {
                double tmp = aug[k * (pp + 1) + j];
                aug[k * (pp + 1) + j] = aug[max_row * (pp + 1) + j];
                aug[max_row * (pp + 1) + j] = tmp;
            }
        }
        double piv = aug[k * (pp + 1) + k];
        for (int j = 0; j <= pp; j++) aug[k * (pp + 1) + j] /= piv;
        for (int i = 0; i < pp; i++) {
            if (i == k) continue;
            double f = aug[i * (pp + 1) + k];
            for (int j = 0; j <= pp; j++) aug[i * (pp + 1) + j] -= f * aug[k * (pp + 1) + j];
        }
    }

    regression_result_t* r = (regression_result_t*)nimcp_calloc(1, sizeof(regression_result_t));
    if (!r) { nimcp_free(aug); nimcp_free(XtX); nimcp_free(Xty); return NULL; }
    r->n_coeffs = pp;
    r->beta = (double*)nimcp_calloc((size_t)pp, sizeof(double));
    if (!r->beta) { nimcp_free(r); nimcp_free(aug); nimcp_free(XtX); nimcp_free(Xty); return NULL; }

    for (int i = 0; i < pp; i++) r->beta[i] = aug[i * (pp + 1) + pp];

    /* R-squared */
    double ybar = 0.0;
    for (int i = 0; i < n; i++) ybar += y[i];
    ybar /= (double)n;
    double ss_res = 0.0, ss_tot = 0.0;
    for (int i = 0; i < n; i++) {
        double pred = r->beta[0];
        for (int j = 1; j < pp; j++) pred += r->beta[j] * X[i * p + (j - 1)];
        ss_res += (y[i] - pred) * (y[i] - pred);
        ss_tot += (y[i] - ybar) * (y[i] - ybar);
    }
    r->r_squared = (ss_tot > 1e-15) ? 1.0 - ss_res / ss_tot : 0.0;
    r->mse = ss_res / (double)n;
    r->adj_r_squared = 1.0 - (1.0 - r->r_squared) * (double)(n - 1) / (double)(n - pp);

    nimcp_free(aug); nimcp_free(XtX); nimcp_free(Xty);
    return r;
}

void prob_regression_free(regression_result_t* r) {
    if (!r) return;
    nimcp_free(r->beta);
    nimcp_free(r);
}

/* ================================================================
 * MONTE CARLO: IMPORTANCE SAMPLING
 * ================================================================ */

mc_result_t prob_importance_sampling(probability_engine_t* eng,
                                    mc_target_func_t target,
                                    mc_proposal_func_t proposal_pdf,
                                    mc_proposal_func_t proposal_sample,
                                    void* params, int n_samples) {
    mc_result_t r;
    memset(&r, 0, sizeof(r));
    if (!eng || !target || !proposal_pdf || !proposal_sample || n_samples <= 0) return r;
    r.n_samples = n_samples;

    double sum_w = 0.0, sum_w2 = 0.0;

    for (int i = 0; i < n_samples; i++) {
        /* Sample from proposal distribution */
        double u = prob_random_uniform(eng);
        double x = proposal_sample(u, params);

        /* Importance weight = target(x) / proposal(x) */
        double q = proposal_pdf(x, params);
        if (fabs(q) < 1e-300) continue;
        double w = target(x, params) / q;

        sum_w += w;
        sum_w2 += w * w;
    }

    r.estimate = sum_w / (double)n_samples;
    r.variance = (sum_w2 / (double)n_samples - r.estimate * r.estimate) / (double)n_samples;

    LOG_DEBUG(LOG_TAG, "Importance sampling: est=%.6f, var=%.2e, n=%d",
              r.estimate, r.variance, n_samples);
    return r;
}
