/**
 * @file nimcp_probability.h
 * @brief Probability, statistics, and hypothesis testing engine
 *
 * Distributions (normal, uniform, exponential, Poisson, binomial,
 * geometric, chi-squared), Bayesian inference, hypothesis tests,
 * regression, descriptive statistics, correlation, Monte Carlo.
 */

#ifndef NIMCP_PROBABILITY_H
#define NIMCP_PROBABILITY_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- constants ---------- */
#define PROB_MAX_DATA_POINTS   4096
#define PROB_MAX_HYPOTHESES    32
#define PROB_MONTE_CARLO_DEFAULT 10000

/* ---------- enums ---------- */

typedef enum distribution_type_e {
    DIST_NORMAL,
    DIST_UNIFORM,
    DIST_EXPONENTIAL,
    DIST_POISSON,
    DIST_BINOMIAL,
    DIST_GEOMETRIC,
    DIST_CHI_SQUARED
} distribution_type_t;

typedef enum test_type_e {
    TEST_Z,
    TEST_T_WELCH,
    TEST_CHI_SQUARED
} test_type_t;

/* ---------- distribution parameters ---------- */

typedef struct dist_params_s {
    distribution_type_t type;
    double mu;          /* normal: mean */
    double sigma;       /* normal: std dev */
    double a, b;        /* uniform: [a, b] */
    double lambda;      /* exponential/Poisson rate */
    int    n_trials;    /* binomial n */
    double p;           /* binomial/geometric probability */
    double df;          /* chi-squared degrees of freedom */
} dist_params_t;

/* ---------- hypothesis test result ---------- */

typedef struct test_result_s {
    double statistic;
    double p_value;
    double df;          /* degrees of freedom (if applicable) */
    bool   reject_null; /* at alpha = 0.05 */
} test_result_t;

/* ---------- regression result ---------- */

typedef struct regression_result_s {
    double* beta;       /* coefficients: beta[0] = intercept, beta[1..p] */
    int     n_coeffs;
    double  r_squared;
    double  adj_r_squared;
    double  mse;
} regression_result_t;

/* ---------- descriptive stats ---------- */

typedef struct descriptive_stats_s {
    double mean;
    double variance;
    double std_dev;
    double median;
    double skewness;
    double kurtosis;
    double min;
    double max;
    int    n;
} descriptive_stats_t;

/* ---------- Bayesian ---------- */

typedef struct bayesian_result_s {
    double posterior[PROB_MAX_HYPOTHESES];
    double bayes_factor;    /* H0 vs H1 */
    int    n_hypotheses;
} bayesian_result_t;

/* ---------- Monte Carlo ---------- */

typedef double (*mc_target_func_t)(double x, void* params);
typedef double (*mc_proposal_func_t)(double x, void* params);

typedef struct mc_result_s {
    double estimate;
    double variance;
    int    n_samples;
} mc_result_t;

/* ---------- main context ---------- */

typedef struct probability_engine_s {
    uint64_t rng_state[2]; /* xorshift128+ state */
    int      mc_default_samples;
} probability_engine_t;

/* ---------- lifecycle ---------- */
probability_engine_t* probability_create(uint64_t seed);
void                  probability_destroy(probability_engine_t* eng);

/* ---------- RNG ---------- */
double prob_random_uniform(probability_engine_t* eng);
double prob_random_normal(probability_engine_t* eng, double mu, double sigma);

/* ---------- distribution functions ---------- */
double prob_pdf(const dist_params_t* d, double x);
double prob_cdf(const dist_params_t* d, double x);
double prob_quantile(const dist_params_t* d, double p);

/* ---------- descriptive statistics ---------- */
descriptive_stats_t prob_descriptive(const double* data, int n);
double prob_percentile(const double* data, int n, double p);

/* ---------- correlation ---------- */
double prob_pearson(const double* x, const double* y, int n);
double prob_spearman(const double* x, const double* y, int n);

/* ---------- hypothesis tests ---------- */
test_result_t prob_z_test(const double* data, int n, double mu0, double sigma);
test_result_t prob_t_test_welch(const double* x, int nx, const double* y, int ny);
test_result_t prob_chi_squared_test(const double* observed, const double* expected, int k);

/* ---------- Bayesian inference ---------- */
bayesian_result_t prob_bayesian_discrete(const double* prior, const double* likelihood,
                                         int n_hypotheses);

/* ---------- regression ---------- */
regression_result_t* prob_linear_regression(const double* x, const double* y, int n);
regression_result_t* prob_multiple_regression(const double* X, const double* y,
                                              int n, int p);
void                 prob_regression_free(regression_result_t* r);

/* ---------- Monte Carlo ---------- */
mc_result_t prob_importance_sampling(probability_engine_t* eng,
                                    mc_target_func_t target,
                                    mc_proposal_func_t proposal_pdf,
                                    mc_proposal_func_t proposal_sample,
                                    void* params, int n_samples);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PROBABILITY_H */
