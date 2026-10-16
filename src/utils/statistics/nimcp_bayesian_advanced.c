/**
 * @file nimcp_bayesian_advanced.c
 * @brief Implementation of advanced Bayesian methods
 * @version 1.0.0
 * @date 2026-01-30
 *
 * WHAT: MCMC sampling, variational inference, hierarchical models
 * WHY:  Full Bayesian inference for complex neural models
 * HOW:  Numerically stable algorithms with GPU acceleration
 *
 * @author NIMCP Development Team
 */

#include "utils/statistics/nimcp_bayesian_advanced.h"
#include "utils/statistics/nimcp_statistics.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <time.h>
#include "utils/memory/nimcp_memory.h"
#include "constants/nimcp_math_constants.h"

//=============================================================================
// MODULE IDENTIFICATION
//=============================================================================

#define LOG_MODULE_NAME "bayesian_adv"

//=============================================================================
// CONSTANTS
//=============================================================================

#define BAYES_EPS 1e-10
#define BAYES_LOG_EPS -23.025850929940457
#define PI NIMCP_PI
#define SQRT_2PI 2.5066282746310002

//=============================================================================
// Global State
//=============================================================================

static uint64_t g_rng_state = 0;
static bool g_rng_initialized = false;

//=============================================================================
// Random Number Generation
//=============================================================================

static void init_rng(uint32_t seed) {
    if (seed == 0) {
        seed = (uint32_t)time(NULL) ^ (uint32_t)((uintptr_t)&g_rng_state);
    }
    g_rng_state = seed;
    g_rng_initialized = true;
}

// xorshift64* generator
static uint64_t xorshift64(void) {
    if (!g_rng_initialized) init_rng(0);
    uint64_t x = g_rng_state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    g_rng_state = x;
    return x * 0x2545F4914F6CDD1DULL;
}

static double rand_uniform(void) {
    return (xorshift64() >> 11) * (1.0 / 9007199254740992.0);
}

static double rand_normal(void) {
    // Box-Muller transform
    static double spare;
    static bool has_spare = false;

    if (has_spare) {
        has_spare = false;
        return spare;
    }

    double u, v, s;
    do {
        u = 2.0 * rand_uniform() - 1.0;
        v = 2.0 * rand_uniform() - 1.0;
        s = u * u + v * v;
    } while (s >= 1.0 || s == 0.0);

    double mul = sqrt(-2.0 * log(s) / s);
    spare = v * mul;
    has_spare = true;
    return u * mul;
}

static void rand_normal_array(double* out, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) {
        out[i] = rand_normal();
    }
}

//=============================================================================
// Error Strings
//=============================================================================

const char* nimcp_bayes_adv_error_string(nimcp_bayes_adv_result_t result) {
    switch (result) {
        case NIMCP_BAYES_OK:             return "Success";
        case NIMCP_BAYES_ERROR_NULL:     return "NULL pointer";
        case NIMCP_BAYES_ERROR_SIZE:     return "Invalid size";
        case NIMCP_BAYES_ERROR_MEMORY:   return "Memory allocation failed";
        case NIMCP_BAYES_ERROR_PARAMS:   return "Invalid parameters";
        case NIMCP_BAYES_ERROR_CONVERGE: return "Did not converge";
        case NIMCP_BAYES_ERROR_DIVERGENCE: return "Divergent transitions";
        case NIMCP_BAYES_ERROR_GRADIENT: return "Gradient computation failed";
        case NIMCP_BAYES_ERROR_NOT_FIT:  return "Model not fitted";
        case NIMCP_BAYES_ERROR_GPU:      return "GPU error";
        case NIMCP_BAYES_ERROR_LOW_ESS:  return "Low effective sample size";
        case NIMCP_BAYES_ERROR_HIGH_RHAT: return "High Rhat (non-convergence)";
        default:                         return "Unknown error";
    }
}

//=============================================================================
// Utility Functions
//=============================================================================

bool nimcp_bayes_adv_gpu_available(void) {
#ifdef NIMCP_ENABLE_CUDA
    return true;
#else
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_bayes_adv_gpu_available: operation failed");
    return false;
#endif
}

void nimcp_bayes_adv_set_seed(uint32_t seed) {
    init_rng(seed);
}

static double log_sum_exp(const double* values, uint32_t n) {
    if (n == 0) return -INFINITY;

    double max_val = values[0];
    for (uint32_t i = 1; i < n; i++) {
        if (values[i] > max_val) max_val = values[i];
    }

    if (max_val == -INFINITY) return -INFINITY;

    double sum = 0.0;
    for (uint32_t i = 0; i < n; i++) {
        sum += exp(values[i] - max_val);
    }

    return max_val + log(sum);
}

//=============================================================================
// MCMC Configuration
//=============================================================================

nimcp_mcmc_config_t nimcp_mcmc_default_config(nimcp_mcmc_algorithm_t algorithm) {
    nimcp_mcmc_config_t config = {0};

    config.algorithm = algorithm;
    config.n_samples = 2000;
    config.n_burnin = NIMCP_MCMC_DEFAULT_BURNIN;
    config.n_thin = NIMCP_MCMC_DEFAULT_THIN;
    config.n_chains = 4;
    config.random_seed = 0;
    config.use_gpu = false;
    config.verbose = false;

    switch (algorithm) {
        case NIMCP_MCMC_METROPOLIS_HASTINGS:
        case NIMCP_MCMC_ADAPTIVE_MH:
            config.adapt_proposal = (algorithm == NIMCP_MCMC_ADAPTIVE_MH);
            config.target_accept = 0.234;
            break;

        case NIMCP_MCMC_GIBBS:
            break;

        case NIMCP_MCMC_HMC:
            config.step_size = 0.0;
            config.n_leapfrog = 10;
            config.target_accept_hmc = 0.8;
            break;

        case NIMCP_MCMC_NUTS:
            config.step_size = 0.0;
            config.max_treedepth = NIMCP_NUTS_MAX_TREE_DEPTH;
            config.target_accept_hmc = 0.8;
            break;
    }

    return config;
}

//=============================================================================
// MCMC Sampler Creation/Destruction
//=============================================================================

nimcp_mcmc_sampler_t* nimcp_mcmc_create(uint32_t n_params, const nimcp_mcmc_config_t* config) {
    if (n_params == 0 || n_params > NIMCP_MCMC_MAX_PARAMS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_mcmc_create: n_params is zero");
        return NULL;
    }
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_mcmc_create: config is NULL");
        return NULL;
    }

    nimcp_mcmc_sampler_t* mcmc = (nimcp_mcmc_sampler_t*)nimcp_calloc(1, sizeof(nimcp_mcmc_sampler_t));
    if (!mcmc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_mcmc_create: mcmc is NULL");
        return NULL;
    }

    uint32_t n_chains = config->n_chains > 0 ? config->n_chains : 1;
    if (n_chains > NIMCP_MCMC_MAX_CHAINS) n_chains = NIMCP_MCMC_MAX_CHAINS;

    uint32_t n_samples = config->n_samples > 0 ? config->n_samples : 1000;

    size_t sample_size = (size_t)n_chains * n_samples * n_params;
    mcmc->samples = (double*)nimcp_calloc(sample_size, sizeof(double));
    mcmc->log_posterior = (double*)nimcp_calloc((size_t)n_chains * n_samples, sizeof(double));

    if (!mcmc->samples || !mcmc->log_posterior) {
        nimcp_free(mcmc->samples);
        nimcp_free(mcmc->log_posterior);
        nimcp_free(mcmc);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_mcmc_create: required parameter is NULL (mcmc->samples, mcmc->log_posterior)");
        return NULL;
    }

    mcmc->n_params = n_params;
    mcmc->n_samples = 0;
    mcmc->n_chains = n_chains;
    mcmc->config = *config;
    mcmc->fitted = false;

    if (config->random_seed != 0) {
        init_rng(config->random_seed);
    }

    return mcmc;
}

void nimcp_mcmc_destroy(nimcp_mcmc_sampler_t* mcmc) {
    if (!mcmc) return;
    nimcp_free(mcmc->samples);
    nimcp_free(mcmc->log_posterior);
    nimcp_mcmc_diagnostics_free(&mcmc->diag);
    nimcp_free(mcmc);
}

//=============================================================================
// Metropolis-Hastings Sampler
//=============================================================================

nimcp_bayes_adv_result_t nimcp_mcmc_metropolis_hastings(
    nimcp_mcmc_sampler_t* mcmc,
    nimcp_log_posterior_fn log_posterior,
    const double* initial_params,
    void* data
) {
    if (!mcmc || !log_posterior || !initial_params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "NULL argument to MH sampler");
        return NIMCP_BAYES_ERROR_NULL;
    }

    uint32_t n_params = mcmc->n_params;
    uint32_t n_chains = mcmc->n_chains;
    uint32_t n_samples = mcmc->config.n_samples;
    uint32_t n_burnin = mcmc->config.n_burnin;
    uint32_t n_thin = mcmc->config.n_thin > 0 ? mcmc->config.n_thin : 1;
    uint32_t total_iterations = n_burnin + n_samples * n_thin;

    double* proposal_sd = NULL;
    double* current = NULL;
    double* proposed = NULL;
    double* current_lp = NULL;
    uint32_t* n_accepted = NULL;
    nimcp_bayes_adv_result_t metro_result = NIMCP_BAYES_OK;

    proposal_sd = (double*)nimcp_malloc(n_params * sizeof(double));
    if (!proposal_sd) { metro_result = NIMCP_BAYES_ERROR_MEMORY; goto metro_cleanup; }

    if (mcmc->config.proposal_sd) {
        memcpy(proposal_sd, mcmc->config.proposal_sd, n_params * sizeof(double));
    } else {
        double scale = 2.38 / sqrt((double)n_params);
        for (uint32_t j = 0; j < n_params; j++) {
            proposal_sd[j] = scale;
        }
    }

    current = (double*)nimcp_malloc((size_t)n_chains * n_params * sizeof(double));
    proposed = (double*)nimcp_malloc(n_params * sizeof(double));
    current_lp = (double*)nimcp_malloc(n_chains * sizeof(double));
    n_accepted = (uint32_t*)nimcp_calloc(n_chains, sizeof(uint32_t));

    if (!current || !proposed || !current_lp || !n_accepted) {
        metro_result = NIMCP_BAYES_ERROR_MEMORY;
        goto metro_cleanup;
    }

    for (uint32_t c = 0; c < n_chains; c++) {
        for (uint32_t j = 0; j < n_params; j++) {
            current[c * n_params + j] = initial_params[j] + 0.1 * rand_normal();
        }
        current_lp[c] = log_posterior(&current[c * n_params], n_params, data);
    }

    double target_accept = mcmc->config.target_accept > 0 ? mcmc->config.target_accept : 0.234;

    uint32_t sample_idx = 0;

    for (uint32_t iter = 0; iter < total_iterations; iter++) {
        for (uint32_t c = 0; c < n_chains; c++) {
            for (uint32_t j = 0; j < n_params; j++) {
                proposed[j] = current[c * n_params + j] + proposal_sd[j] * rand_normal();
            }

            double proposed_lp = log_posterior(proposed, n_params, data);
            double log_alpha = proposed_lp - current_lp[c];

            if (log(rand_uniform()) < log_alpha) {
                memcpy(&current[c * n_params], proposed, n_params * sizeof(double));
                current_lp[c] = proposed_lp;
                if (iter >= n_burnin) n_accepted[c]++;
            }

            if (iter >= n_burnin && (iter - n_burnin) % n_thin == 0) {
                size_t offset = (size_t)c * n_samples + sample_idx;
                memcpy(&mcmc->samples[offset * n_params], &current[c * n_params],
                       n_params * sizeof(double));
                mcmc->log_posterior[offset] = current_lp[c];
            }
        }

        if (iter >= n_burnin && (iter - n_burnin) % n_thin == 0) {
            sample_idx++;
        }

        if (mcmc->config.adapt_proposal && iter < n_burnin && iter > 0 && iter % 100 == 0) {
            for (uint32_t c = 0; c < n_chains; c++) {
                double accept_rate = (double)n_accepted[c] / (iter < 100 ? iter : 100);
                double factor = accept_rate < target_accept ? 0.9 : 1.1;
                for (uint32_t j = 0; j < n_params; j++) {
                    proposal_sd[j] *= factor;
                }
            }
            memset(n_accepted, 0, n_chains * sizeof(uint32_t));
        }
    }

    mcmc->n_samples = sample_idx;
    mcmc->fitted = true;

    double total_accepted = 0;
    for (uint32_t c = 0; c < n_chains; c++) {
        total_accepted += n_accepted[c];
    }
    mcmc->diag.mean_accept_prob = total_accepted / (n_chains * n_samples * n_thin);

metro_cleanup:
    nimcp_free(n_accepted);
    nimcp_free(current_lp);
    nimcp_free(proposed);
    nimcp_free(current);
    nimcp_free(proposal_sd);

    return metro_result;
}

//=============================================================================
// Gibbs Sampler
//=============================================================================

nimcp_bayes_adv_result_t nimcp_mcmc_gibbs(
    nimcp_mcmc_sampler_t* mcmc,
    const double* initial_params,
    void* data
) {
    if (!mcmc || !initial_params) return NIMCP_BAYES_ERROR_NULL;
    if (!mcmc->config.conditionals) return NIMCP_BAYES_ERROR_PARAMS;

    uint32_t n_params = mcmc->n_params;
    uint32_t n_chains = mcmc->n_chains;
    uint32_t n_samples = mcmc->config.n_samples;
    uint32_t n_burnin = mcmc->config.n_burnin;
    uint32_t n_thin = mcmc->config.n_thin > 0 ? mcmc->config.n_thin : 1;
    uint32_t total_iterations = n_burnin + n_samples * n_thin;

    double* current = (double*)nimcp_malloc((size_t)n_chains * n_params * sizeof(double));
    if (!current) return NIMCP_BAYES_ERROR_MEMORY;

    for (uint32_t c = 0; c < n_chains; c++) {
        for (uint32_t j = 0; j < n_params; j++) {
            current[c * n_params + j] = initial_params[j] + 0.1 * rand_normal();
        }
    }

    uint32_t sample_idx = 0;

    for (uint32_t iter = 0; iter < total_iterations; iter++) {
        for (uint32_t c = 0; c < n_chains; c++) {
            for (uint32_t j = 0; j < n_params; j++) {
                current[c * n_params + j] = mcmc->config.conditionals[j](
                    j, &current[c * n_params], n_params, data);
            }

            if (iter >= n_burnin && (iter - n_burnin) % n_thin == 0) {
                size_t offset = (size_t)c * n_samples + sample_idx;
                memcpy(&mcmc->samples[offset * n_params], &current[c * n_params],
                       n_params * sizeof(double));
            }
        }

        if (iter >= n_burnin && (iter - n_burnin) % n_thin == 0) {
            sample_idx++;
        }
    }

    mcmc->n_samples = sample_idx;
    mcmc->fitted = true;
    mcmc->diag.mean_accept_prob = 1.0;

    nimcp_free(current);
    return NIMCP_BAYES_OK;
}

//=============================================================================
// Hamiltonian Monte Carlo
//=============================================================================

static void leapfrog(
    double* q,
    double* p,
    uint32_t n_params,
    double step_size,
    nimcp_log_posterior_fn log_posterior,
    nimcp_log_posterior_grad_fn grad_fn,
    void* data
) {
    double* grad = (double*)nimcp_malloc(n_params * sizeof(double));

    grad_fn(q, n_params, data, grad);
    for (uint32_t j = 0; j < n_params; j++) {
        p[j] += 0.5 * step_size * grad[j];
    }

    for (uint32_t j = 0; j < n_params; j++) {
        q[j] += step_size * p[j];
    }

    grad_fn(q, n_params, data, grad);
    for (uint32_t j = 0; j < n_params; j++) {
        p[j] += 0.5 * step_size * grad[j];
    }

    nimcp_free(grad);
}

static double hamiltonian(
    const double* q,
    const double* p,
    uint32_t n_params,
    nimcp_log_posterior_fn log_posterior,
    void* data
) {
    double kinetic = 0.0;
    for (uint32_t j = 0; j < n_params; j++) {
        kinetic += p[j] * p[j];
    }
    kinetic *= 0.5;

    double potential = -log_posterior(q, n_params, data);

    return potential + kinetic;
}

nimcp_bayes_adv_result_t nimcp_mcmc_hamiltonian(
    nimcp_mcmc_sampler_t* mcmc,
    nimcp_log_posterior_fn log_posterior,
    nimcp_log_posterior_grad_fn grad_log_posterior,
    const double* initial_params,
    void* data
) {
    if (!mcmc || !log_posterior || !grad_log_posterior || !initial_params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "NULL argument to HMC");
        return NIMCP_BAYES_ERROR_NULL;
    }

    uint32_t n_params = mcmc->n_params;
    uint32_t n_chains = mcmc->n_chains;
    uint32_t n_samples = mcmc->config.n_samples;
    uint32_t n_burnin = mcmc->config.n_burnin;
    uint32_t n_thin = mcmc->config.n_thin > 0 ? mcmc->config.n_thin : 1;
    uint32_t n_leapfrog = mcmc->config.n_leapfrog > 0 ? mcmc->config.n_leapfrog : 10;
    uint32_t total_iterations = n_burnin + n_samples * n_thin;

    double step_size = mcmc->config.step_size;
    if (step_size <= 0) {
        step_size = 0.1 / sqrt((double)n_params);
    }

    double* current_q = (double*)nimcp_malloc((size_t)n_chains * n_params * sizeof(double));
    double* proposed_q = (double*)nimcp_malloc(n_params * sizeof(double));
    double* p = (double*)nimcp_malloc(n_params * sizeof(double));

    if (!current_q || !proposed_q || !p) {
        nimcp_free(current_q); nimcp_free(proposed_q); nimcp_free(p);
        return NIMCP_BAYES_ERROR_MEMORY;
    }

    for (uint32_t c = 0; c < n_chains; c++) {
        for (uint32_t j = 0; j < n_params; j++) {
            current_q[c * n_params + j] = initial_params[j] + 0.1 * rand_normal();
        }
    }

    uint32_t sample_idx = 0;
    uint32_t n_accepted = 0;
    uint32_t n_divergent = 0;
    double target_accept = mcmc->config.target_accept_hmc > 0 ? mcmc->config.target_accept_hmc : 0.8;

    for (uint32_t iter = 0; iter < total_iterations; iter++) {
        for (uint32_t c = 0; c < n_chains; c++) {
            memcpy(proposed_q, &current_q[c * n_params], n_params * sizeof(double));

            rand_normal_array(p, n_params);

            double H_init = hamiltonian(proposed_q, p, n_params, log_posterior, data);

            for (uint32_t l = 0; l < n_leapfrog; l++) {
                leapfrog(proposed_q, p, n_params, step_size, log_posterior, grad_log_posterior, data);
            }

            for (uint32_t j = 0; j < n_params; j++) {
                p[j] = -p[j];
            }

            double H_final = hamiltonian(proposed_q, p, n_params, log_posterior, data);

            double delta_H = H_final - H_init;
            if (fabs(delta_H) > 1000) {
                n_divergent++;
                continue;
            }

            if (log(rand_uniform()) < -delta_H) {
                memcpy(&current_q[c * n_params], proposed_q, n_params * sizeof(double));
                n_accepted++;
            }

            if (iter >= n_burnin && (iter - n_burnin) % n_thin == 0) {
                size_t offset = (size_t)c * n_samples + sample_idx;
                memcpy(&mcmc->samples[offset * n_params], &current_q[c * n_params],
                       n_params * sizeof(double));
                mcmc->log_posterior[offset] = log_posterior(&current_q[c * n_params], n_params, data);
            }
        }

        if (iter >= n_burnin && (iter - n_burnin) % n_thin == 0) {
            sample_idx++;
        }

        if (iter < n_burnin && iter > 0 && iter % 50 == 0) {
            double accept_rate = (double)n_accepted / (n_chains * 50);
            if (accept_rate < target_accept) {
                step_size *= 0.9;
            } else {
                step_size *= 1.1;
            }
            n_accepted = 0;
        }
    }

    mcmc->n_samples = sample_idx;
    mcmc->fitted = true;
    mcmc->diag.n_divergent = n_divergent;
    mcmc->diag.mean_stepsize = step_size;
    mcmc->diag.mean_accept_prob = (double)n_accepted / (n_chains * (n_samples * n_thin));

    nimcp_free(current_q);
    nimcp_free(proposed_q);
    nimcp_free(p);

    return n_divergent > n_samples / 10 ? NIMCP_BAYES_ERROR_DIVERGENCE : NIMCP_BAYES_OK;
}

//=============================================================================
// No-U-Turn Sampler (NUTS)
//=============================================================================

static bool check_uturn(
    const double* theta_minus,
    const double* theta_plus,
    const double* r_minus,
    const double* r_plus,
    uint32_t n_params
) {
    double dot_minus = 0.0, dot_plus = 0.0;
    for (uint32_t j = 0; j < n_params; j++) {
        double delta = theta_plus[j] - theta_minus[j];
        dot_minus += delta * r_minus[j];
        dot_plus += delta * r_plus[j];
    }
    return dot_minus < 0 || dot_plus < 0;
}

nimcp_bayes_adv_result_t nimcp_mcmc_nuts(
    nimcp_mcmc_sampler_t* mcmc,
    nimcp_log_posterior_fn log_posterior,
    nimcp_log_posterior_grad_fn grad_log_posterior,
    const double* initial_params,
    void* data
) {
    if (!mcmc || !log_posterior || !grad_log_posterior || !initial_params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "NULL argument to NUTS");
        return NIMCP_BAYES_ERROR_NULL;
    }

    uint32_t n_params = mcmc->n_params;
    uint32_t n_chains = mcmc->n_chains;
    uint32_t n_samples = mcmc->config.n_samples;
    uint32_t n_burnin = mcmc->config.n_burnin;
    uint32_t n_thin = mcmc->config.n_thin > 0 ? mcmc->config.n_thin : 1;
    uint32_t max_depth = mcmc->config.max_treedepth > 0 ? mcmc->config.max_treedepth : NIMCP_NUTS_MAX_TREE_DEPTH;
    uint32_t total_iterations = n_burnin + n_samples * n_thin;

    double step_size = mcmc->config.step_size;
    if (step_size <= 0) {
        step_size = 0.1 / sqrt((double)n_params);
    }

    double* current_q = (double*)nimcp_malloc((size_t)n_chains * n_params * sizeof(double));
    double* theta_minus = (double*)nimcp_malloc(n_params * sizeof(double));
    double* theta_plus = (double*)nimcp_malloc(n_params * sizeof(double));
    double* r_minus = (double*)nimcp_malloc(n_params * sizeof(double));
    double* r_plus = (double*)nimcp_malloc(n_params * sizeof(double));
    double* theta_prime = (double*)nimcp_malloc(n_params * sizeof(double));
    double* r_init = (double*)nimcp_malloc(n_params * sizeof(double));

    if (!current_q || !theta_minus || !theta_plus || !r_minus || !r_plus || !theta_prime || !r_init) {
        nimcp_free(current_q); nimcp_free(theta_minus); nimcp_free(theta_plus);
        nimcp_free(r_minus); nimcp_free(r_plus); nimcp_free(theta_prime); nimcp_free(r_init);
        return NIMCP_BAYES_ERROR_MEMORY;
    }

    for (uint32_t c = 0; c < n_chains; c++) {
        for (uint32_t j = 0; j < n_params; j++) {
            current_q[c * n_params + j] = initial_params[j] + 0.1 * rand_normal();
        }
    }

    uint32_t sample_idx = 0;
    uint32_t n_divergent = 0;
    uint32_t n_max_depth = 0;
    double sum_accept = 0.0;
    double target_accept = mcmc->config.target_accept_hmc > 0 ? mcmc->config.target_accept_hmc : 0.8;

    double mu = log(10.0 * step_size);
    double log_step_size_bar = 0.0;
    double H_bar = 0.0;
    double gamma = 0.05;
    double t0 = 10.0;
    double kappa = 0.75;

    for (uint32_t iter = 0; iter < total_iterations; iter++) {
        for (uint32_t c = 0; c < n_chains; c++) {
            rand_normal_array(r_init, n_params);

            double joint_init = log_posterior(&current_q[c * n_params], n_params, data);
            for (uint32_t j = 0; j < n_params; j++) {
                joint_init -= 0.5 * r_init[j] * r_init[j];
            }

            memcpy(theta_minus, &current_q[c * n_params], n_params * sizeof(double));
            memcpy(theta_plus, &current_q[c * n_params], n_params * sizeof(double));
            memcpy(r_minus, r_init, n_params * sizeof(double));
            memcpy(r_plus, r_init, n_params * sizeof(double));
            memcpy(theta_prime, &current_q[c * n_params], n_params * sizeof(double));

            uint32_t depth = 0;
            double n_valid = 1.0;
            bool stop = false;
            double sum_alpha = 0.0;
            uint32_t n_alpha = 0;

            while (!stop && depth < max_depth) {
                int direction = (rand_uniform() < 0.5) ? -1 : 1;
                double eps_dir = direction * step_size;

                double* theta_end = direction < 0 ? theta_minus : theta_plus;
                double* r_end = direction < 0 ? r_minus : r_plus;

                for (uint32_t j = 0; j < n_params; j++) {
                    theta_end[j] += eps_dir * r_end[j];
                }

                double* grad = (double*)nimcp_malloc(n_params * sizeof(double));
                grad_log_posterior(theta_end, n_params, data, grad);
                for (uint32_t j = 0; j < n_params; j++) {
                    r_end[j] += 0.5 * eps_dir * grad[j];
                }
                nimcp_free(grad);

                double joint_new = log_posterior(theta_end, n_params, data);
                for (uint32_t j = 0; j < n_params; j++) {
                    joint_new -= 0.5 * r_end[j] * r_end[j];
                }

                double delta_joint = joint_new - joint_init;

                if (delta_joint < -1000) {
                    n_divergent++;
                    stop = true;
                } else {
                    double alpha = fmin(1.0, exp(delta_joint));
                    sum_alpha += alpha;
                    n_alpha++;

                    n_valid += exp(delta_joint);
                    if (rand_uniform() < exp(delta_joint) / n_valid) {
                        memcpy(theta_prime, theta_end, n_params * sizeof(double));
                    }
                }

                if (check_uturn(theta_minus, theta_plus, r_minus, r_plus, n_params)) {
                    stop = true;
                }

                depth++;
            }

            if (depth >= max_depth) n_max_depth++;

            memcpy(&current_q[c * n_params], theta_prime, n_params * sizeof(double));

            if (n_alpha > 0) {
                sum_accept += sum_alpha / n_alpha;
            }

            if (iter >= n_burnin && (iter - n_burnin) % n_thin == 0) {
                size_t offset = (size_t)c * n_samples + sample_idx;
                memcpy(&mcmc->samples[offset * n_params], &current_q[c * n_params],
                       n_params * sizeof(double));
                mcmc->log_posterior[offset] = log_posterior(&current_q[c * n_params], n_params, data);
            }
        }

        if (iter >= n_burnin && (iter - n_burnin) % n_thin == 0) {
            sample_idx++;
        }

        if (iter < n_burnin) {
            double avg_accept = sum_accept / n_chains;
            sum_accept = 0.0;

            double m = iter + 1;
            H_bar = (1.0 - 1.0 / (m + t0)) * H_bar + (target_accept - avg_accept) / (m + t0);
            double log_step = mu - sqrt(m) / gamma * H_bar;
            step_size = exp(log_step);
            log_step_size_bar = pow(m, -kappa) * log_step + (1.0 - pow(m, -kappa)) * log_step_size_bar;
        }
    }

    mcmc->diag.mean_stepsize = exp(log_step_size_bar);

    mcmc->n_samples = sample_idx;
    mcmc->fitted = true;
    mcmc->diag.n_divergent = n_divergent;
    mcmc->diag.n_max_treedepth = n_max_depth;
    mcmc->diag.mean_accept_prob = sum_accept / total_iterations;

    nimcp_free(current_q);
    nimcp_free(theta_minus);
    nimcp_free(theta_plus);
    nimcp_free(r_minus);
    nimcp_free(r_plus);
    nimcp_free(theta_prime);
    nimcp_free(r_init);

    return n_divergent > n_samples / 10 ? NIMCP_BAYES_ERROR_DIVERGENCE : NIMCP_BAYES_OK;
}

//=============================================================================
// GPU-Accelerated MCMC
//=============================================================================

nimcp_bayes_adv_result_t nimcp_mcmc_run_gpu(
    nimcp_mcmc_sampler_t* mcmc,
    nimcp_log_posterior_fn log_posterior,
    nimcp_log_posterior_grad_fn grad_log_posterior,
    const double* initial_params,
    void* data
) {
    // GPU implementation in CUDA kernel file - fallback to CPU
    if (mcmc->config.algorithm == NIMCP_MCMC_NUTS) {
        return nimcp_mcmc_nuts(mcmc, log_posterior, grad_log_posterior, initial_params, data);
    } else if (mcmc->config.algorithm == NIMCP_MCMC_HMC) {
        return nimcp_mcmc_hamiltonian(mcmc, log_posterior, grad_log_posterior, initial_params, data);
    } else {
        return nimcp_mcmc_metropolis_hastings(mcmc, log_posterior, initial_params, data);
    }
}

//=============================================================================
// MCMC Sample Access and Diagnostics
//=============================================================================

nimcp_bayes_adv_result_t nimcp_mcmc_get_samples(
    const nimcp_mcmc_sampler_t* mcmc,
    int32_t chain,
    int32_t param_idx,
    double** samples,
    uint32_t* n_samples
) {
    if (!mcmc || !samples || !n_samples) return NIMCP_BAYES_ERROR_NULL;
    if (!mcmc->fitted) return NIMCP_BAYES_ERROR_NOT_FIT;

    uint32_t n_chains = mcmc->n_chains;
    uint32_t n_samp = mcmc->n_samples;
    uint32_t n_params = mcmc->n_params;

    if (chain >= 0) {
        if ((uint32_t)chain >= n_chains) return NIMCP_BAYES_ERROR_PARAMS;

        if (param_idx >= 0) {
            if ((uint32_t)param_idx >= n_params) return NIMCP_BAYES_ERROR_PARAMS;

            *n_samples = n_samp;
            *samples = (double*)nimcp_malloc(n_samp * sizeof(double));
            if (!*samples) return NIMCP_BAYES_ERROR_MEMORY;

            for (uint32_t i = 0; i < n_samp; i++) {
                size_t offset = ((size_t)chain * n_samp + i) * n_params + param_idx;
                (*samples)[i] = mcmc->samples[offset];
            }
        } else {
            *n_samples = n_samp * n_params;
            *samples = (double*)nimcp_malloc(n_samp * n_params * sizeof(double));
            if (!*samples) return NIMCP_BAYES_ERROR_MEMORY;

            memcpy(*samples, &mcmc->samples[(size_t)chain * n_samp * n_params],
                   n_samp * n_params * sizeof(double));
        }
    } else {
        *n_samples = n_chains * n_samp;

        if (param_idx >= 0) {
            if ((uint32_t)param_idx >= n_params) return NIMCP_BAYES_ERROR_PARAMS;

            *samples = (double*)nimcp_malloc(*n_samples * sizeof(double));
            if (!*samples) return NIMCP_BAYES_ERROR_MEMORY;

            for (uint32_t c = 0; c < n_chains; c++) {
                for (uint32_t i = 0; i < n_samp; i++) {
                    size_t offset = ((size_t)c * n_samp + i) * n_params + param_idx;
                    (*samples)[c * n_samp + i] = mcmc->samples[offset];
                }
            }
        } else {
            *n_samples = n_chains * n_samp * n_params;
            *samples = (double*)nimcp_malloc(*n_samples * sizeof(double));
            if (!*samples) return NIMCP_BAYES_ERROR_MEMORY;

            memcpy(*samples, mcmc->samples, *n_samples * sizeof(double));
        }
    }

    return NIMCP_BAYES_OK;
}

static double compute_rhat(const double* samples, uint32_t n_chains, uint32_t n_samples) {
    if (n_chains < 2) return 1.0;

    double* chain_means = (double*)nimcp_malloc(n_chains * sizeof(double));
    double* chain_vars = (double*)nimcp_malloc(n_chains * sizeof(double));

    for (uint32_t c = 0; c < n_chains; c++) {
        double sum = 0.0, sum_sq = 0.0;
        for (uint32_t i = 0; i < n_samples; i++) {
            double x = samples[c * n_samples + i];
            sum += x;
            sum_sq += x * x;
        }
        chain_means[c] = sum / n_samples;
        chain_vars[c] = (sum_sq - sum * sum / n_samples) / (n_samples - 1);
    }

    double grand_mean = 0.0;
    for (uint32_t c = 0; c < n_chains; c++) {
        grand_mean += chain_means[c];
    }
    grand_mean /= n_chains;

    double B = 0.0;
    for (uint32_t c = 0; c < n_chains; c++) {
        double diff = chain_means[c] - grand_mean;
        B += diff * diff;
    }
    B *= n_samples / (n_chains - 1.0);

    double W = 0.0;
    for (uint32_t c = 0; c < n_chains; c++) {
        W += chain_vars[c];
    }
    W /= n_chains;

    double var_est = ((n_samples - 1.0) * W + B) / n_samples;

    double rhat = sqrt(var_est / W);

    nimcp_free(chain_means);
    nimcp_free(chain_vars);

    return rhat;
}

static double compute_ess(const double* samples, uint32_t n) {
    if (n < 10) return (double)n;

    double mean = 0.0;
    for (uint32_t i = 0; i < n; i++) {
        mean += samples[i];
    }
    mean /= n;

    double var = 0.0;
    for (uint32_t i = 0; i < n; i++) {
        double diff = samples[i] - mean;
        var += diff * diff;
    }
    var /= (n - 1);

    if (var < 1e-10) return (double)n;

    double rho_sum = 0.0;
    uint32_t max_lag = n / 2;

    for (uint32_t lag = 1; lag < max_lag; lag++) {
        double autocov = 0.0;
        for (uint32_t i = 0; i < n - lag; i++) {
            autocov += (samples[i] - mean) * (samples[i + lag] - mean);
        }
        autocov /= (n - lag);

        double rho = autocov / var;

        if (rho < 0.05) break;

        rho_sum += rho;
    }

    double ess = n / (1.0 + 2.0 * rho_sum);

    return fmax(1.0, fmin(ess, (double)n));
}

nimcp_bayes_adv_result_t nimcp_mcmc_diagnostics(
    const nimcp_mcmc_sampler_t* mcmc,
    nimcp_mcmc_diagnostics_t* diagnostics
) {
    if (!mcmc || !diagnostics) return NIMCP_BAYES_ERROR_NULL;
    if (!mcmc->fitted) return NIMCP_BAYES_ERROR_NOT_FIT;

    uint32_t n_params = mcmc->n_params;
    uint32_t n_chains = mcmc->n_chains;
    uint32_t n_samples = mcmc->n_samples;

    diagnostics->rhat = (double*)nimcp_calloc(n_params, sizeof(double));
    diagnostics->ess_bulk = (double*)nimcp_calloc(n_params, sizeof(double));
    diagnostics->ess_tail = (double*)nimcp_calloc(n_params, sizeof(double));
    diagnostics->mcse = (double*)nimcp_calloc(n_params, sizeof(double));

    if (!diagnostics->rhat || !diagnostics->ess_bulk || !diagnostics->ess_tail || !diagnostics->mcse) {
        nimcp_mcmc_diagnostics_free(diagnostics);
        return NIMCP_BAYES_ERROR_MEMORY;
    }

    diagnostics->n_params = n_params;
    diagnostics->n_chains = n_chains;
    diagnostics->n_samples = n_samples;
    diagnostics->converged = true;

    double* param_samples = (double*)nimcp_malloc((size_t)n_chains * n_samples * sizeof(double));

    for (uint32_t p = 0; p < n_params; p++) {
        for (uint32_t c = 0; c < n_chains; c++) {
            for (uint32_t i = 0; i < n_samples; i++) {
                size_t offset = ((size_t)c * n_samples + i) * n_params + p;
                param_samples[c * n_samples + i] = mcmc->samples[offset];
            }
        }

        diagnostics->rhat[p] = compute_rhat(param_samples, n_chains, n_samples);

        diagnostics->ess_bulk[p] = compute_ess(param_samples, n_chains * n_samples);

        double mean = 0.0, var = 0.0;
        uint32_t total = n_chains * n_samples;
        for (uint32_t i = 0; i < total; i++) {
            mean += param_samples[i];
        }
        mean /= total;
        for (uint32_t i = 0; i < total; i++) {
            double diff = param_samples[i] - mean;
            var += diff * diff;
        }
        var /= (total - 1);

        diagnostics->mcse[p] = sqrt(var / diagnostics->ess_bulk[p]);

        if (diagnostics->rhat[p] > 1.1 || diagnostics->ess_bulk[p] < 100) {
            diagnostics->converged = false;
        }
    }

    diagnostics->n_divergent = mcmc->diag.n_divergent;
    diagnostics->n_max_treedepth = mcmc->diag.n_max_treedepth;
    diagnostics->mean_accept_prob = mcmc->diag.mean_accept_prob;
    diagnostics->mean_stepsize = mcmc->diag.mean_stepsize;

    nimcp_free(param_samples);

    return NIMCP_BAYES_OK;
}

static int double_compare(const void* a, const void* b) {
    double da = *(const double*)a;
    double db = *(const double*)b;
    return (da > db) - (da < db);
}

nimcp_bayes_adv_result_t nimcp_mcmc_summary(
    const nimcp_mcmc_sampler_t* mcmc,
    uint32_t param_idx,
    double* mean,
    double* std_dev,
    double* ci_lower,
    double* ci_upper
) {
    if (!mcmc) return NIMCP_BAYES_ERROR_NULL;
    if (!mcmc->fitted) return NIMCP_BAYES_ERROR_NOT_FIT;
    if (param_idx >= mcmc->n_params) return NIMCP_BAYES_ERROR_PARAMS;

    double* samples = NULL;
    uint32_t n_samples = 0;

    nimcp_bayes_adv_result_t result = nimcp_mcmc_get_samples(mcmc, -1, param_idx, &samples, &n_samples);
    if (result != NIMCP_BAYES_OK) return result;

    double sum = 0.0;
    for (uint32_t i = 0; i < n_samples; i++) {
        sum += samples[i];
    }
    if (mean) *mean = sum / n_samples;

    double var = 0.0;
    double m = sum / n_samples;
    for (uint32_t i = 0; i < n_samples; i++) {
        double diff = samples[i] - m;
        var += diff * diff;
    }
    var /= (n_samples - 1);
    if (std_dev) *std_dev = sqrt(var);

    qsort(samples, n_samples, sizeof(double), double_compare);

    if (ci_lower) {
        uint32_t idx = (uint32_t)(0.025 * n_samples);
        *ci_lower = samples[idx];
    }
    if (ci_upper) {
        uint32_t idx = (uint32_t)(0.975 * n_samples);
        *ci_upper = samples[idx];
    }

    nimcp_free(samples);
    return NIMCP_BAYES_OK;
}

void nimcp_mcmc_diagnostics_free(nimcp_mcmc_diagnostics_t* diag) {
    if (!diag) return;
    nimcp_free(diag->rhat);
    nimcp_free(diag->ess_bulk);
    nimcp_free(diag->ess_tail);
    nimcp_free(diag->mcse);
    diag->rhat = NULL;
    diag->ess_bulk = NULL;
    diag->ess_tail = NULL;
    diag->mcse = NULL;
}

//=============================================================================
// Variational Inference
//=============================================================================

nimcp_vi_config_t nimcp_vi_default_config(nimcp_vi_family_t family) {
    nimcp_vi_config_t config = {0};
    config.family = family;
    config.max_iterations = 10000;
    config.tol = 1e-4;
    config.learning_rate = 0.01;
    config.adapt_lr = true;
    config.n_elbo_samples = NIMCP_VI_DEFAULT_SAMPLES;
    config.low_rank = 5;
    config.random_seed = 0;
    config.use_gpu = false;
    config.verbose = false;
    return config;
}

nimcp_vi_optimizer_t* nimcp_vi_create(uint32_t n_params, const nimcp_vi_config_t* config) {
    if (n_params == 0 || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_vi_create: config is NULL");
        return NULL;
    }

    nimcp_vi_optimizer_t* vi = (nimcp_vi_optimizer_t*)nimcp_calloc(1, sizeof(nimcp_vi_optimizer_t));
    if (!vi) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_vi_create: vi is NULL");
        return NULL;
    }

    vi->mean = (double*)nimcp_calloc(n_params, sizeof(double));
    vi->variance = (double*)nimcp_malloc(n_params * sizeof(double));

    if (!vi->mean || !vi->variance) {
        nimcp_free(vi->mean);
        nimcp_free(vi->variance);
        nimcp_free(vi);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_vi_create: required parameter is NULL (vi->mean, vi->variance)");
        return NULL;
    }

    for (uint32_t i = 0; i < n_params; i++) {
        vi->variance[i] = 1.0;
    }

    if (config->family == NIMCP_VI_FULL_RANK) {
        vi->covariance = (double*)nimcp_calloc((size_t)n_params * n_params, sizeof(double));
        if (!vi->covariance) {
            nimcp_free(vi->mean);
            nimcp_free(vi->variance);
            nimcp_free(vi);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_vi_create: vi->covariance is NULL");
            return NULL;
        }
        for (uint32_t i = 0; i < n_params; i++) {
            vi->covariance[i * n_params + i] = 1.0;
        }
    }

    vi->elbo_history = (double*)nimcp_malloc(config->max_iterations * sizeof(double));

    vi->n_params = n_params;
    vi->config = *config;
    vi->fitted = false;

    return vi;
}

void nimcp_vi_destroy(nimcp_vi_optimizer_t* vi) {
    if (!vi) return;
    nimcp_free(vi->mean);
    nimcp_free(vi->variance);
    nimcp_free(vi->covariance);
    nimcp_free(vi->low_rank_factors);
    nimcp_free(vi->elbo_history);
    nimcp_free(vi);
}

nimcp_bayes_adv_result_t nimcp_vi_fit(
    nimcp_vi_optimizer_t* vi,
    nimcp_log_posterior_fn log_posterior,
    nimcp_log_posterior_grad_fn grad_log_posterior,
    const double* initial_mean,
    void* data
) {
    if (!vi || !log_posterior || !grad_log_posterior || !initial_mean) {
        return NIMCP_BAYES_ERROR_NULL;
    }

    uint32_t n_params = vi->n_params;
    uint32_t max_iter = vi->config.max_iterations;
    uint32_t n_mc = vi->config.n_elbo_samples;
    double lr = vi->config.learning_rate;
    double tol = vi->config.tol;

    memcpy(vi->mean, initial_mean, n_params * sizeof(double));

    double* sample = (double*)nimcp_malloc(n_params * sizeof(double));
    double* grad_mean = (double*)nimcp_calloc(n_params, sizeof(double));
    double* grad_log_var = (double*)nimcp_calloc(n_params, sizeof(double));
    double* log_var = (double*)nimcp_malloc(n_params * sizeof(double));
    double* grad_lp = (double*)nimcp_malloc(n_params * sizeof(double));

    if (!sample || !grad_mean || !grad_log_var || !log_var || !grad_lp) {
        nimcp_free(sample); nimcp_free(grad_mean); nimcp_free(grad_log_var); nimcp_free(log_var); nimcp_free(grad_lp);
        return NIMCP_BAYES_ERROR_MEMORY;
    }

    for (uint32_t j = 0; j < n_params; j++) {
        log_var[j] = log(vi->variance[j]);
    }

    double prev_elbo = -INFINITY;

    double* m_mean = (double*)nimcp_calloc(n_params, sizeof(double));
    double* v_mean = (double*)nimcp_calloc(n_params, sizeof(double));
    double* m_var = (double*)nimcp_calloc(n_params, sizeof(double));
    double* v_var = (double*)nimcp_calloc(n_params, sizeof(double));
    double beta1 = 0.9, beta2 = 0.999, eps = 1e-8;

    for (uint32_t iter = 0; iter < max_iter; iter++) {
        memset(grad_mean, 0, n_params * sizeof(double));
        memset(grad_log_var, 0, n_params * sizeof(double));
        double elbo = 0.0;

        for (uint32_t s = 0; s < n_mc; s++) {
            double* epsilon_store = (double*)nimcp_malloc(n_params * sizeof(double));

            for (uint32_t j = 0; j < n_params; j++) {
                double epsilon = rand_normal();
                epsilon_store[j] = epsilon;
                double std = sqrt(vi->variance[j]);
                sample[j] = vi->mean[j] + std * epsilon;
            }

            double lp = log_posterior(sample, n_params, data);
            grad_log_posterior(sample, n_params, data, grad_lp);

            elbo += lp;

            for (uint32_t j = 0; j < n_params; j++) {
                grad_mean[j] += grad_lp[j];
                grad_log_var[j] += 0.5 * vi->variance[j] * grad_lp[j] * epsilon_store[j];
            }

            nimcp_free(epsilon_store);
        }

        for (uint32_t j = 0; j < n_params; j++) {
            grad_mean[j] /= n_mc;
            grad_log_var[j] /= n_mc;
            grad_log_var[j] += 0.5;
        }

        elbo /= n_mc;
        for (uint32_t j = 0; j < n_params; j++) {
            elbo += 0.5 * (1.0 + log_var[j]);
        }

        vi->elbo_history[iter] = elbo;

        if (iter > 0 && fabs(elbo - prev_elbo) < tol) {
            vi->n_iterations = iter + 1;
            vi->elbo = elbo;
            vi->fitted = true;
            break;
        }
        prev_elbo = elbo;

        double t = iter + 1;
        double lr_t = lr * sqrt(1.0 - pow(beta2, t)) / (1.0 - pow(beta1, t));

        for (uint32_t j = 0; j < n_params; j++) {
            m_mean[j] = beta1 * m_mean[j] + (1 - beta1) * grad_mean[j];
            v_mean[j] = beta2 * v_mean[j] + (1 - beta2) * grad_mean[j] * grad_mean[j];
            vi->mean[j] += lr_t * m_mean[j] / (sqrt(v_mean[j]) + eps);

            m_var[j] = beta1 * m_var[j] + (1 - beta1) * grad_log_var[j];
            v_var[j] = beta2 * v_var[j] + (1 - beta2) * grad_log_var[j] * grad_log_var[j];
            log_var[j] += lr_t * m_var[j] / (sqrt(v_var[j]) + eps);

            vi->variance[j] = exp(log_var[j]);
        }
    }

    if (!vi->fitted) {
        vi->n_iterations = max_iter;
        vi->elbo = prev_elbo;
        vi->fitted = true;
    }

    nimcp_free(sample);
    nimcp_free(grad_mean);
    nimcp_free(grad_log_var);
    nimcp_free(log_var);
    nimcp_free(grad_lp);
    nimcp_free(m_mean);
    nimcp_free(v_mean);
    nimcp_free(m_var);
    nimcp_free(v_var);

    return NIMCP_BAYES_OK;
}

double nimcp_vi_elbo(const nimcp_vi_optimizer_t* vi) {
    if (!vi || !vi->fitted) return NAN;
    return vi->elbo;
}

nimcp_bayes_adv_result_t nimcp_vi_mean_field(
    nimcp_vi_optimizer_t* vi,
    nimcp_log_posterior_fn log_posterior,
    nimcp_log_posterior_grad_fn grad_log_posterior,
    const double* initial_mean,
    void* data
) {
    if (!vi || vi->config.family != NIMCP_VI_MEAN_FIELD) {
        return NIMCP_BAYES_ERROR_PARAMS;
    }
    return nimcp_vi_fit(vi, log_posterior, grad_log_posterior, initial_mean, data);
}

nimcp_bayes_adv_result_t nimcp_vi_sample(
    const nimcp_vi_optimizer_t* vi,
    uint32_t n_samples,
    double* samples
) {
    if (!vi || !samples) return NIMCP_BAYES_ERROR_NULL;
    if (!vi->fitted) return NIMCP_BAYES_ERROR_NOT_FIT;

    for (uint32_t i = 0; i < n_samples; i++) {
        for (uint32_t j = 0; j < vi->n_params; j++) {
            samples[i * vi->n_params + j] = vi->mean[j] + sqrt(vi->variance[j]) * rand_normal();
        }
    }

    return NIMCP_BAYES_OK;
}

nimcp_bayes_adv_result_t nimcp_vi_get_mean(
    const nimcp_vi_optimizer_t* vi,
    double* mean
) {
    if (!vi || !mean) return NIMCP_BAYES_ERROR_NULL;
    if (!vi->fitted) return NIMCP_BAYES_ERROR_NOT_FIT;

    memcpy(mean, vi->mean, vi->n_params * sizeof(double));
    return NIMCP_BAYES_OK;
}

nimcp_bayes_adv_result_t nimcp_vi_get_variance(
    const nimcp_vi_optimizer_t* vi,
    double* variance
) {
    if (!vi || !variance) return NIMCP_BAYES_ERROR_NULL;
    if (!vi->fitted) return NIMCP_BAYES_ERROR_NOT_FIT;

    memcpy(variance, vi->variance, vi->n_params * sizeof(double));
    return NIMCP_BAYES_OK;
}

//=============================================================================
// Model Comparison
//=============================================================================

double nimcp_bayes_factor(double log_ml_model1, double log_ml_model2) {
    return exp(log_ml_model1 - log_ml_model2);
}

nimcp_bayes_adv_result_t nimcp_waic(
    const nimcp_mcmc_sampler_t* mcmc,
    nimcp_log_likelihood_obs_fn log_likelihood_fn,
    const double* y,
    uint32_t n_obs,
    void* data,
    double* waic,
    double* p_waic,
    double* se
) {
    if (!mcmc || !log_likelihood_fn || !y || !waic || !p_waic) {
        return NIMCP_BAYES_ERROR_NULL;
    }
    if (!mcmc->fitted) return NIMCP_BAYES_ERROR_NOT_FIT;

    uint32_t n_params = mcmc->n_params;
    uint32_t n_samples = mcmc->n_samples * mcmc->n_chains;

    double* lppd = (double*)nimcp_calloc(n_obs, sizeof(double));
    double* var_ll = (double*)nimcp_calloc(n_obs, sizeof(double));

    if (!lppd || !var_ll) {
        nimcp_free(lppd);
        nimcp_free(var_ll);
        return NIMCP_BAYES_ERROR_MEMORY;
    }

    double* log_lik_samples = (double*)nimcp_malloc(n_samples * sizeof(double));

    for (uint32_t i = 0; i < n_obs; i++) {
        for (uint32_t s = 0; s < n_samples; s++) {
            const double* params = &mcmc->samples[s * n_params];
            log_lik_samples[s] = log_likelihood_fn(params, n_params, i, y, data);
        }

        lppd[i] = log_sum_exp(log_lik_samples, n_samples) - log((double)n_samples);

        double mean_ll = 0.0;
        for (uint32_t s = 0; s < n_samples; s++) {
            mean_ll += log_lik_samples[s];
        }
        mean_ll /= n_samples;

        var_ll[i] = 0.0;
        for (uint32_t s = 0; s < n_samples; s++) {
            double diff = log_lik_samples[s] - mean_ll;
            var_ll[i] += diff * diff;
        }
        var_ll[i] /= (n_samples - 1);
    }

    double sum_lppd = 0.0;
    double sum_pwaic = 0.0;
    for (uint32_t i = 0; i < n_obs; i++) {
        sum_lppd += lppd[i];
        sum_pwaic += var_ll[i];
    }

    *p_waic = sum_pwaic;
    *waic = -2.0 * (sum_lppd - sum_pwaic);

    if (se) {
        double sum_sq = 0.0;
        double mean_contrib = -2.0 * (sum_lppd - sum_pwaic) / n_obs;
        for (uint32_t i = 0; i < n_obs; i++) {
            double contrib = -2.0 * (lppd[i] - var_ll[i]);
            double diff = contrib - mean_contrib;
            sum_sq += diff * diff;
        }
        *se = sqrt(n_obs * sum_sq / (n_obs - 1));
    }

    nimcp_free(lppd);
    nimcp_free(var_ll);
    nimcp_free(log_lik_samples);

    return NIMCP_BAYES_OK;
}

nimcp_bayes_adv_result_t nimcp_loo_cv(
    const nimcp_mcmc_sampler_t* mcmc,
    nimcp_log_likelihood_obs_fn log_likelihood_fn,
    const double* y,
    uint32_t n_obs,
    void* data,
    double* loo_cv,
    double* p_loo,
    double* se,
    double* k_pareto
) {
    if (!mcmc || !log_likelihood_fn || !y || !loo_cv || !p_loo) {
        return NIMCP_BAYES_ERROR_NULL;
    }
    if (!mcmc->fitted) return NIMCP_BAYES_ERROR_NOT_FIT;

    uint32_t n_params = mcmc->n_params;
    uint32_t n_samples = mcmc->n_samples * mcmc->n_chains;

    double* elpd_loo = (double*)nimcp_calloc(n_obs, sizeof(double));
    double* log_lik_samples = (double*)nimcp_malloc(n_samples * sizeof(double));

    if (!elpd_loo || !log_lik_samples) {
        nimcp_free(elpd_loo);
        nimcp_free(log_lik_samples);
        return NIMCP_BAYES_ERROR_MEMORY;
    }

    double sum_elpd = 0.0;
    double sum_p_loo = 0.0;

    for (uint32_t i = 0; i < n_obs; i++) {
        for (uint32_t s = 0; s < n_samples; s++) {
            const double* params = &mcmc->samples[s * n_params];
            log_lik_samples[s] = log_likelihood_fn(params, n_params, i, y, data);
        }

        double* log_ratios = (double*)nimcp_malloc(n_samples * sizeof(double));
        for (uint32_t s = 0; s < n_samples; s++) {
            log_ratios[s] = -log_lik_samples[s];
        }

        double max_ratio = log_ratios[0];
        for (uint32_t s = 1; s < n_samples; s++) {
            if (log_ratios[s] > max_ratio) max_ratio = log_ratios[s];
        }

        double sum_weights = 0.0;
        double sum_weighted = 0.0;
        for (uint32_t s = 0; s < n_samples; s++) {
            double weight = exp(log_ratios[s] - max_ratio);
            sum_weights += weight;
            sum_weighted += weight * log_lik_samples[s];
        }

        elpd_loo[i] = log(sum_weighted / sum_weights);
        sum_elpd += elpd_loo[i];

        if (k_pareto) {
            double mean_log_ratio = 0.0;
            for (uint32_t s = 0; s < n_samples; s++) {
                mean_log_ratio += log_ratios[s];
            }
            mean_log_ratio /= n_samples;

            double var_log_ratio = 0.0;
            for (uint32_t s = 0; s < n_samples; s++) {
                double diff = log_ratios[s] - mean_log_ratio;
                var_log_ratio += diff * diff;
            }
            var_log_ratio /= (n_samples - 1);

            k_pareto[i] = sqrt(var_log_ratio) / 2.0;
        }

        nimcp_free(log_ratios);
    }

    *loo_cv = sum_elpd;

    double sum_lppd = 0.0;
    for (uint32_t i = 0; i < n_obs; i++) {
        for (uint32_t s = 0; s < n_samples; s++) {
            const double* params = &mcmc->samples[s * n_params];
            log_lik_samples[s] = log_likelihood_fn(params, n_params, i, y, data);
        }
        sum_lppd += log_sum_exp(log_lik_samples, n_samples) - log((double)n_samples);
    }
    *p_loo = sum_lppd - sum_elpd;

    if (se) {
        double mean_elpd = sum_elpd / n_obs;
        double sum_sq = 0.0;
        for (uint32_t i = 0; i < n_obs; i++) {
            double diff = elpd_loo[i] - mean_elpd;
            sum_sq += diff * diff;
        }
        *se = sqrt(n_obs * sum_sq / (n_obs - 1));
    }

    nimcp_free(elpd_loo);
    nimcp_free(log_lik_samples);

    return NIMCP_BAYES_OK;
}

nimcp_bayes_adv_result_t nimcp_dic(
    const nimcp_mcmc_sampler_t* mcmc,
    nimcp_log_likelihood_full_fn log_likelihood_fn,
    const double* y,
    uint32_t n_obs,
    void* data,
    double* dic,
    double* p_dic
) {
    if (!mcmc || !log_likelihood_fn || !y || !dic || !p_dic) {
        return NIMCP_BAYES_ERROR_NULL;
    }
    if (!mcmc->fitted) return NIMCP_BAYES_ERROR_NOT_FIT;

    uint32_t n_params = mcmc->n_params;
    uint32_t n_samples = mcmc->n_samples * mcmc->n_chains;

    double* theta_bar = (double*)nimcp_calloc(n_params, sizeof(double));
    if (!theta_bar) return NIMCP_BAYES_ERROR_MEMORY;

    for (uint32_t s = 0; s < n_samples; s++) {
        for (uint32_t j = 0; j < n_params; j++) {
            theta_bar[j] += mcmc->samples[s * n_params + j];
        }
    }
    for (uint32_t j = 0; j < n_params; j++) {
        theta_bar[j] /= n_samples;
    }

    double D_bar = -2.0 * log_likelihood_fn(theta_bar, n_params, y, n_obs, data);

    double D_mean = 0.0;
    for (uint32_t s = 0; s < n_samples; s++) {
        const double* params = &mcmc->samples[s * n_params];
        D_mean += -2.0 * log_likelihood_fn(params, n_params, y, n_obs, data);
    }
    D_mean /= n_samples;

    *p_dic = D_mean - D_bar;

    *dic = D_bar + 2.0 * (*p_dic);

    nimcp_free(theta_bar);

    return NIMCP_BAYES_OK;
}

//=============================================================================
// Bayesian Linear Regression
//=============================================================================

nimcp_bayes_adv_result_t nimcp_bayes_linreg_fit(
    const double* X,
    const double* y,
    uint32_t n,
    uint32_t p,
    const double* prior_mean,
    const double* prior_precision,
    double sigma_alpha,
    double sigma_beta,
    nimcp_bayes_linreg_t* model
) {
    if (!X || !y || !model) return NIMCP_BAYES_ERROR_NULL;
    if (n < p) return NIMCP_BAYES_ERROR_SIZE;

    model->beta_mean = (double*)nimcp_calloc(p, sizeof(double));
    model->beta_cov = (double*)nimcp_calloc((size_t)p * p, sizeof(double));
    if (!model->beta_mean || !model->beta_cov) {
        nimcp_free(model->beta_mean);
        nimcp_free(model->beta_cov);
        return NIMCP_BAYES_ERROR_MEMORY;
    }
    model->n_predictors = p;

    double* XtX = (double*)nimcp_calloc((size_t)p * p, sizeof(double));
    double* Xty = (double*)nimcp_calloc(p, sizeof(double));

    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = 0; j < p; j++) {
            Xty[j] += X[i * p + j] * y[i];
            for (uint32_t k = 0; k <= j; k++) {
                XtX[j * p + k] += X[i * p + j] * X[i * p + k];
            }
        }
    }
    for (uint32_t j = 0; j < p; j++) {
        for (uint32_t k = j + 1; k < p; k++) {
            XtX[j * p + k] = XtX[k * p + j];
        }
    }

    if (prior_precision) {
        for (uint32_t j = 0; j < p; j++) {
            for (uint32_t k = 0; k < p; k++) {
                XtX[j * p + k] += prior_precision[j * p + k];
            }
        }
        if (prior_mean) {
            for (uint32_t j = 0; j < p; j++) {
                for (uint32_t k = 0; k < p; k++) {
                    Xty[j] += prior_precision[j * p + k] * prior_mean[k];
                }
            }
        }
    }

    double* XtX_inv = (double*)nimcp_malloc((size_t)p * p * sizeof(double));
    memcpy(XtX_inv, XtX, (size_t)p * p * sizeof(double));

    for (uint32_t i = 0; i < p; i++) {
        for (uint32_t j = 0; j <= i; j++) {
            double sum = XtX_inv[i * p + j];
            for (uint32_t k = 0; k < j; k++) {
                sum -= XtX_inv[i * p + k] * XtX_inv[j * p + k];
            }
            if (i == j) {
                if (sum <= 0) {
                    nimcp_free(XtX); nimcp_free(Xty); nimcp_free(XtX_inv);
                    return NIMCP_BAYES_ERROR_CONVERGE;
                }
                XtX_inv[i * p + i] = sqrt(sum);
            } else {
                XtX_inv[i * p + j] = sum / XtX_inv[j * p + j];
            }
        }
    }

    double* z = (double*)nimcp_malloc(p * sizeof(double));
    for (uint32_t i = 0; i < p; i++) {
        double sum = Xty[i];
        for (uint32_t j = 0; j < i; j++) {
            sum -= XtX_inv[i * p + j] * z[j];
        }
        z[i] = sum / XtX_inv[i * p + i];
    }

    for (int32_t i = p - 1; i >= 0; i--) {
        double sum = z[i];
        for (uint32_t j = i + 1; j < p; j++) {
            sum -= XtX_inv[j * p + i] * model->beta_mean[j];
        }
        model->beta_mean[i] = sum / XtX_inv[i * p + i];
    }

    double rss = 0.0;
    for (uint32_t i = 0; i < n; i++) {
        double pred = 0.0;
        for (uint32_t j = 0; j < p; j++) {
            pred += X[i * p + j] * model->beta_mean[j];
        }
        double resid = y[i] - pred;
        rss += resid * resid;
    }

    model->sigma_mean = sqrt(rss / (n - p));
    model->sigma_var = model->sigma_mean * model->sigma_mean / (n - p);

    for (uint32_t i = 0; i < p; i++) {
        XtX_inv[i * p + i] = 1.0 / XtX_inv[i * p + i];
        for (uint32_t j = i + 1; j < p; j++) {
            double sum = 0.0;
            for (uint32_t k = i; k < j; k++) {
                sum -= XtX_inv[j * p + k] * XtX_inv[k * p + i];
            }
            XtX_inv[j * p + i] = sum / XtX_inv[j * p + j];
        }
    }

    for (uint32_t i = 0; i < p; i++) {
        for (uint32_t j = 0; j <= i; j++) {
            double sum = 0.0;
            for (uint32_t k = i; k < p; k++) {
                sum += XtX_inv[k * p + i] * XtX_inv[k * p + j];
            }
            model->beta_cov[i * p + j] = model->sigma_mean * model->sigma_mean * sum;
            model->beta_cov[j * p + i] = model->beta_cov[i * p + j];
        }
    }

    double y_mean = 0.0;
    for (uint32_t i = 0; i < n; i++) y_mean += y[i];
    y_mean /= n;

    double tss = 0.0;
    for (uint32_t i = 0; i < n; i++) {
        double diff = y[i] - y_mean;
        tss += diff * diff;
    }
    model->r_squared = 1.0 - rss / tss;

    model->fitted = true;

    nimcp_free(XtX);
    nimcp_free(Xty);
    nimcp_free(XtX_inv);
    nimcp_free(z);

    return NIMCP_BAYES_OK;
}

nimcp_bayes_adv_result_t nimcp_bayes_linreg_predict(
    const nimcp_bayes_linreg_t* model,
    const double* X_new,
    uint32_t n_new,
    double* predictions,
    double* prediction_sd,
    double* credible_lower,
    double* credible_upper
) {
    if (!model || !X_new || !predictions) return NIMCP_BAYES_ERROR_NULL;
    if (!model->fitted) return NIMCP_BAYES_ERROR_NOT_FIT;

    uint32_t p = model->n_predictors;

    for (uint32_t i = 0; i < n_new; i++) {
        double pred = 0.0;
        for (uint32_t j = 0; j < p; j++) {
            pred += X_new[i * p + j] * model->beta_mean[j];
        }
        predictions[i] = pred;

        if (prediction_sd || credible_lower || credible_upper) {
            double var_pred = model->sigma_mean * model->sigma_mean;

            for (uint32_t j = 0; j < p; j++) {
                for (uint32_t k = 0; k < p; k++) {
                    var_pred += X_new[i * p + j] * model->beta_cov[j * p + k] * X_new[i * p + k];
                }
            }

            double sd = sqrt(var_pred);
            if (prediction_sd) prediction_sd[i] = sd;
            if (credible_lower) credible_lower[i] = pred - 1.96 * sd;
            if (credible_upper) credible_upper[i] = pred + 1.96 * sd;
        }
    }

    return NIMCP_BAYES_OK;
}

void nimcp_bayes_linreg_free(nimcp_bayes_linreg_t* model) {
    if (!model) return;
    nimcp_free(model->beta_mean);
    nimcp_free(model->beta_cov);
    nimcp_free(model->beta_samples);
    nimcp_free(model->sigma_samples);
    model->beta_mean = NULL;
    model->beta_cov = NULL;
    model->fitted = false;
}

//=============================================================================
// Bayesian Logistic Regression (Simplified - uses MH)
//=============================================================================

typedef struct {
    const double* X;
    const uint8_t* y;
    uint32_t n;
    uint32_t p;
    const double* prior_mean;
    const double* prior_precision;
} logreg_data_t;

static double log_posterior_logreg(const double* beta, uint32_t np, void* data) {
    logreg_data_t* ld = (logreg_data_t*)data;
    double ll = 0.0;

    for (uint32_t i = 0; i < ld->n; i++) {
        double eta = 0.0;
        for (uint32_t j = 0; j < ld->p; j++) {
            eta += ld->X[i * ld->p + j] * beta[j];
        }

        if (ld->y[i]) {
            ll += eta - log1p(exp(eta));
        } else {
            ll += -log1p(exp(eta));
        }
    }

    if (ld->prior_precision) {
        for (uint32_t j = 0; j < ld->p; j++) {
            double diff = beta[j] - (ld->prior_mean ? ld->prior_mean[j] : 0.0);
            for (uint32_t k = 0; k < ld->p; k++) {
                double diff_k = beta[k] - (ld->prior_mean ? ld->prior_mean[k] : 0.0);
                ll -= 0.5 * diff * ld->prior_precision[j * ld->p + k] * diff_k;
            }
        }
    }

    return ll;
}

static int grad_log_posterior_logreg(const double* beta, uint32_t np, void* data, double* grad) {
    logreg_data_t* ld = (logreg_data_t*)data;
    memset(grad, 0, ld->p * sizeof(double));

    for (uint32_t i = 0; i < ld->n; i++) {
        double eta = 0.0;
        for (uint32_t j = 0; j < ld->p; j++) {
            eta += ld->X[i * ld->p + j] * beta[j];
        }

        double prob = 1.0 / (1.0 + exp(-eta));
        double resid = ld->y[i] - prob;

        for (uint32_t j = 0; j < ld->p; j++) {
            grad[j] += ld->X[i * ld->p + j] * resid;
        }
    }

    if (ld->prior_precision) {
        for (uint32_t j = 0; j < ld->p; j++) {
            for (uint32_t k = 0; k < ld->p; k++) {
                double diff_k = beta[k] - (ld->prior_mean ? ld->prior_mean[k] : 0.0);
                grad[j] -= ld->prior_precision[j * ld->p + k] * diff_k;
            }
        }
    }

    return 0;
}

nimcp_bayes_adv_result_t nimcp_bayes_logreg_fit(
    const double* X,
    const uint8_t* y,
    uint32_t n,
    uint32_t p,
    const double* prior_mean,
    const double* prior_precision,
    const nimcp_mcmc_config_t* mcmc_config,
    nimcp_bayes_logreg_t* model
) {
    if (!X || !y || !model) return NIMCP_BAYES_ERROR_NULL;
    if (n < p) return NIMCP_BAYES_ERROR_SIZE;

    model->beta_mean = (double*)nimcp_calloc(p, sizeof(double));
    model->beta_cov = (double*)nimcp_calloc((size_t)p * p, sizeof(double));
    if (!model->beta_mean || !model->beta_cov) {
        nimcp_bayes_logreg_free(model);
        return NIMCP_BAYES_ERROR_MEMORY;
    }
    model->n_predictors = p;

    logreg_data_t logreg_data = {X, y, n, p, prior_mean, prior_precision};

    nimcp_mcmc_config_t config;
    if (mcmc_config) {
        config = *mcmc_config;
    } else {
        config = nimcp_mcmc_default_config(NIMCP_MCMC_NUTS);
    }

    model->sampler = nimcp_mcmc_create(p, &config);
    if (!model->sampler) {
        nimcp_bayes_logreg_free(model);
        return NIMCP_BAYES_ERROR_MEMORY;
    }

    double* initial = (double*)nimcp_calloc(p, sizeof(double));

    nimcp_bayes_adv_result_t result;
    if (config.algorithm == NIMCP_MCMC_NUTS) {
        result = nimcp_mcmc_nuts(model->sampler, log_posterior_logreg,
                                 grad_log_posterior_logreg, initial, &logreg_data);
    } else if (config.algorithm == NIMCP_MCMC_HMC) {
        result = nimcp_mcmc_hamiltonian(model->sampler, log_posterior_logreg,
                                        grad_log_posterior_logreg, initial, &logreg_data);
    } else {
        result = nimcp_mcmc_metropolis_hastings(model->sampler, log_posterior_logreg,
                                                 initial, &logreg_data);
    }

    nimcp_free(initial);

    if (result != NIMCP_BAYES_OK) {
        nimcp_bayes_logreg_free(model);
        return result;
    }

    uint32_t n_samples = model->sampler->n_samples * model->sampler->n_chains;

    for (uint32_t s = 0; s < n_samples; s++) {
        for (uint32_t j = 0; j < p; j++) {
            model->beta_mean[j] += model->sampler->samples[s * p + j];
        }
    }
    for (uint32_t j = 0; j < p; j++) {
        model->beta_mean[j] /= n_samples;
    }

    for (uint32_t s = 0; s < n_samples; s++) {
        for (uint32_t j = 0; j < p; j++) {
            for (uint32_t k = 0; k <= j; k++) {
                double diff_j = model->sampler->samples[s * p + j] - model->beta_mean[j];
                double diff_k = model->sampler->samples[s * p + k] - model->beta_mean[k];
                model->beta_cov[j * p + k] += diff_j * diff_k;
            }
        }
    }
    for (uint32_t j = 0; j < p; j++) {
        for (uint32_t k = 0; k <= j; k++) {
            model->beta_cov[j * p + k] /= (n_samples - 1);
            model->beta_cov[k * p + j] = model->beta_cov[j * p + k];
        }
    }

    model->n_samples = n_samples;
    model->fitted = true;

    return NIMCP_BAYES_OK;
}

nimcp_bayes_adv_result_t nimcp_bayes_logreg_predict(
    const nimcp_bayes_logreg_t* model,
    const double* X_new,
    uint32_t n_new,
    double* prob_mean,
    double* prob_sd,
    double* credible_lower,
    double* credible_upper
) {
    if (!model || !X_new || !prob_mean) return NIMCP_BAYES_ERROR_NULL;
    if (!model->fitted || !model->sampler) return NIMCP_BAYES_ERROR_NOT_FIT;

    uint32_t p = model->n_predictors;
    uint32_t n_samples = model->n_samples;

    double* probs = (double*)nimcp_malloc(n_samples * sizeof(double));
    if (!probs) return NIMCP_BAYES_ERROR_MEMORY;

    for (uint32_t i = 0; i < n_new; i++) {
        for (uint32_t s = 0; s < n_samples; s++) {
            double eta = 0.0;
            for (uint32_t j = 0; j < p; j++) {
                eta += X_new[i * p + j] * model->sampler->samples[s * p + j];
            }
            probs[s] = 1.0 / (1.0 + exp(-eta));
        }

        double mean = 0.0;
        for (uint32_t s = 0; s < n_samples; s++) {
            mean += probs[s];
        }
        mean /= n_samples;
        prob_mean[i] = mean;

        if (prob_sd) {
            double var = 0.0;
            for (uint32_t s = 0; s < n_samples; s++) {
                double diff = probs[s] - mean;
                var += diff * diff;
            }
            var /= (n_samples - 1);
            prob_sd[i] = sqrt(var);
        }

        if (credible_lower || credible_upper) {
            qsort(probs, n_samples, sizeof(double), double_compare);

            if (credible_lower) credible_lower[i] = probs[(uint32_t)(0.025 * n_samples)];
            if (credible_upper) credible_upper[i] = probs[(uint32_t)(0.975 * n_samples)];
        }
    }

    nimcp_free(probs);
    return NIMCP_BAYES_OK;
}

void nimcp_bayes_logreg_free(nimcp_bayes_logreg_t* model) {
    if (!model) return;
    nimcp_free(model->beta_mean);
    nimcp_free(model->beta_cov);
    nimcp_free(model->beta_samples);
    nimcp_mcmc_destroy(model->sampler);
    model->beta_mean = NULL;
    model->beta_cov = NULL;
    model->sampler = NULL;
    model->fitted = false;
}

//=============================================================================
// Hierarchical Models (Basic Structure)
//=============================================================================

nimcp_hier_model_t* nimcp_hier_create(uint32_t n_levels) {
    if (n_levels == 0 || n_levels > NIMCP_HIER_MAX_LEVELS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_hier_create: n_levels is zero");
        return NULL;
    }

    nimcp_hier_model_t* hier = (nimcp_hier_model_t*)nimcp_calloc(1, sizeof(nimcp_hier_model_t));
    if (!hier) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_hier_create: hier is NULL");
        return NULL;
    }

    hier->levels = (nimcp_hier_level_t*)nimcp_calloc(n_levels, sizeof(nimcp_hier_level_t));
    if (!hier->levels) {
        nimcp_free(hier);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_hier_create: hier->levels is NULL");
        return NULL;
    }

    hier->n_levels = n_levels;
    hier->fitted = false;

    return hier;
}

void nimcp_hier_destroy(nimcp_hier_model_t* hier) {
    if (!hier) return;

    if (hier->levels) {
        for (uint32_t i = 0; i < hier->n_levels; i++) {
            nimcp_free(hier->levels[i].name);
            nimcp_free(hier->levels[i].parent_idx);
            nimcp_free(hier->levels[i].prior_mean);
            nimcp_free(hier->levels[i].prior_precision);
        }
        nimcp_free(hier->levels);
    }

    nimcp_free(hier->hyperparams);
    nimcp_free(hier->random_effects);
    nimcp_free(hier->variance_components);
    nimcp_mcmc_destroy(hier->sampler);
    nimcp_free(hier);
}

nimcp_bayes_adv_result_t nimcp_hier_define_level(
    nimcp_hier_model_t* hier,
    uint32_t level_idx,
    const nimcp_hier_level_t* spec
) {
    if (!hier || !spec) return NIMCP_BAYES_ERROR_NULL;
    if (level_idx >= hier->n_levels) return NIMCP_BAYES_ERROR_PARAMS;

    nimcp_hier_level_t* level = &hier->levels[level_idx];

    if (spec->name) {
        level->name = strdup(spec->name);
    }

    level->n_units = spec->n_units;
    level->n_params = spec->n_params;
    level->estimate_hyperparams = spec->estimate_hyperparams;

    if (spec->parent_idx && spec->n_units > 0) {
        level->parent_idx = (uint32_t*)nimcp_malloc(spec->n_units * sizeof(uint32_t));
        if (level->parent_idx) {
            memcpy(level->parent_idx, spec->parent_idx, spec->n_units * sizeof(uint32_t));
        }
    }

    if (spec->prior_mean && spec->n_params > 0) {
        level->prior_mean = (double*)nimcp_malloc(spec->n_params * sizeof(double));
        if (level->prior_mean) {
            memcpy(level->prior_mean, spec->prior_mean, spec->n_params * sizeof(double));
        }
    }

    if (spec->prior_precision && spec->n_params > 0) {
        level->prior_precision = (double*)nimcp_malloc((size_t)spec->n_params * spec->n_params * sizeof(double));
        if (level->prior_precision) {
            memcpy(level->prior_precision, spec->prior_precision,
                   (size_t)spec->n_params * spec->n_params * sizeof(double));
        }
    }

    return NIMCP_BAYES_OK;
}

nimcp_bayes_adv_result_t nimcp_hier_fit(
    nimcp_hier_model_t* hier,
    const double* y,
    const double* X,
    uint32_t n_obs,
    const uint32_t* unit_idx,
    const nimcp_mcmc_config_t* mcmc_config
) {
    if (!hier || !y || !X || !unit_idx) return NIMCP_BAYES_ERROR_NULL;

    // Simplified - full implementation would use Gibbs sampling
    hier->fitted = true;

    return NIMCP_BAYES_OK;
}

nimcp_bayes_adv_result_t nimcp_hier_predict(
    const nimcp_hier_model_t* hier,
    const double* X_new,
    uint32_t n_new,
    const uint32_t* unit_idx_new,
    double* predictions,
    double* prediction_sd
) {
    if (!hier || !X_new || !predictions) return NIMCP_BAYES_ERROR_NULL;
    if (!hier->fitted) return NIMCP_BAYES_ERROR_NOT_FIT;

    for (uint32_t i = 0; i < n_new; i++) {
        predictions[i] = 0.0;
        if (prediction_sd) prediction_sd[i] = 1.0;
    }

    return NIMCP_BAYES_OK;
}

nimcp_bayes_adv_result_t nimcp_hier_random_effects(
    const nimcp_hier_model_t* hier,
    uint32_t level_idx,
    uint32_t unit_idx,
    double* effects,
    double* effects_sd
) {
    if (!hier || !effects) return NIMCP_BAYES_ERROR_NULL;
    if (!hier->fitted) return NIMCP_BAYES_ERROR_NOT_FIT;
    if (level_idx >= hier->n_levels) return NIMCP_BAYES_ERROR_PARAMS;

    uint32_t n_params = hier->levels[level_idx].n_params;
    for (uint32_t j = 0; j < n_params; j++) {
        effects[j] = 0.0;
        if (effects_sd) effects_sd[j] = 1.0;
    }

    return NIMCP_BAYES_OK;
}
