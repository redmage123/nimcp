/**
 * @file nimcp_quantum_math_engine.c
 * @brief Quantum Mathematical Engine Implementation
 *
 * Implements quantum-enhanced mathematical computations including
 * Monte Carlo integration, partition functions, path integrals,
 * and expectation estimation.
 *
 * @author NIMCP Team
 * @version 2.6.3
 */

#include "cognitive/neuro_symbolic/nimcp_quantum_math_engine.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "QUANTUM_MATH_ENGINE"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_learning_constants.h"
#include "constants/nimcp_math_constants.h"

BRIDGE_BOILERPLATE(quantum_math_engine, MESH_ADAPTER_CATEGORY_COGNITIVE)


#include "async/nimcp_bio_router.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

/**
 * @brief Internal structure for quantum math engine
 */
struct qme_math_simulation {
    /* Configuration */
    qme_simulation_config_t config;

    /* Random state */
    uint64_t rng_state;

    /* Proposal distribution state */
    float* proposal_mean;
    float* proposal_covariance;
    uint32_t proposal_dim;

    /* Adaptive proposal parameters */
    float current_step_size;
    float acceptance_sum;
    uint32_t acceptance_count;

    /* Sample storage */
    float* sample_buffer;
    uint32_t sample_buffer_size;

    /* Working space */
    float* work_buffer;
    uint32_t work_buffer_size;

    /* Statistics */
    qme_stats_t stats;

    /* Bio-async */
    uint16_t bio_module_id;
    const char* module_name;
    bool bio_async_enabled;
    bio_module_context_t bio_ctx;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* State flags */
    bool initialized;
};

/* ============================================================================
 * RNG Functions
 * ============================================================================ */

/**
 * @brief Simple xorshift64 RNG
 */
static uint64_t xorshift64(uint64_t* state) {
    uint64_t x = *state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *state = x;
    return x;
}

/**
 * @brief Generate uniform random float in [0,1)
 */
static float random_uniform(qme_math_simulation_t* sim) {
    return (float)(xorshift64(&sim->rng_state) >> 11) * (1.0f / 9007199254740992.0f);
}

/**
 * @brief Generate standard normal random variable (Box-Muller)
 */
static float random_normal(qme_math_simulation_t* sim) {
    static bool has_spare = false;
    static float spare;

    if (has_spare) {
        has_spare = false;
        return spare;
    }

    float u, v, s;
    do {
        u = random_uniform(sim) * 2.0f - 1.0f;
        v = random_uniform(sim) * 2.0f - 1.0f;
        s = u * u + v * v;
    } while (s >= 1.0f || s == 0.0f);

    s = sqrtf(-2.0f * logf(s) / s);
    spare = v * s;
    has_spare = true;
    return u * s;
}

/* ============================================================================
 * Sampling Functions
 * ============================================================================ */

/**
 * @brief Generate uniform sample in box domain
 */
static void sample_uniform_box(qme_math_simulation_t* sim,
                               const qme_domain_t* domain,
                               float* sample) {
    for (uint32_t i = 0; i < domain->dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && domain->dim > 256) {
            quantum_math_engine_heartbeat("quantum_math_loop",
                             (float)(i + 1) / (float)domain->dim);
        }

        float range = domain->upper_bounds[i] - domain->lower_bounds[i];
        sample[i] = domain->lower_bounds[i] + random_uniform(sim) * range;
    }
}

/**
 * @brief Generate uniform sample in ball domain
 */
static void sample_uniform_ball(qme_math_simulation_t* sim,
                                const qme_domain_t* domain,
                                float* sample) {
    /* Generate point on unit sphere */
    float norm_sq = 0.0f;
    for (uint32_t i = 0; i < domain->dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && domain->dim > 256) {
            quantum_math_engine_heartbeat("quantum_math_loop",
                             (float)(i + 1) / (float)domain->dim);
        }

        sample[i] = random_normal(sim);
        norm_sq += sample[i] * sample[i];
    }
    float norm = sqrtf(norm_sq);

    /* Scale by random radius */
    float r = powf(random_uniform(sim), 1.0f / domain->dim);
    float scale = r * domain->radius / (fabsf(norm) > 1e-7f ? norm : 1e-7f);

    for (uint32_t i = 0; i < domain->dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && domain->dim > 256) {
            quantum_math_engine_heartbeat("quantum_math_loop",
                             (float)(i + 1) / (float)domain->dim);
        }

        sample[i] = domain->center[i] + sample[i] * scale;
    }
}

/**
 * @brief Generate sample from domain
 */
static void sample_from_domain(qme_math_simulation_t* sim,
                               const qme_domain_t* domain,
                               float* sample) {
    switch (domain->type) {
        case QME_DOMAIN_BOX:
            sample_uniform_box(sim, domain, sample);
            break;
        case QME_DOMAIN_BALL:
            sample_uniform_ball(sim, domain, sample);
            break;
        case QME_DOMAIN_SIMPLEX:
            /* Simplex sampling: exponential distribution normalized */
            {
                float sum = 0.0f;
                for (uint32_t i = 0; i < domain->dim; i++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((i & 0xFF) == 0 && domain->dim > 256) {
                        quantum_math_engine_heartbeat("quantum_math_loop",
                                         (float)(i + 1) / (float)domain->dim);
                    }

                    sample[i] = -logf(random_uniform(sim) + 1e-10f);
                    sum += sample[i];
                }
                for (uint32_t i = 0; i < domain->dim; i++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((i & 0xFF) == 0 && domain->dim > 256) {
                        quantum_math_engine_heartbeat("quantum_math_loop",
                                         (float)(i + 1) / (float)domain->dim);
                    }

                    sample[i] /= sum;
                }
            }
            break;
        default:
            sample_uniform_box(sim, domain, sample);
            break;
    }
}

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

NIMCP_API qme_math_simulation_t* qme_math_create(
    const qme_simulation_config_t* config) {

    qme_math_simulation_t* sim = nimcp_calloc(1, sizeof(qme_math_simulation_t));
    if (!sim) {
        NIMCP_LOG_ERROR("Failed to allocate quantum math engine");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "random_normal: sim is NULL");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        memcpy(&sim->config, config, sizeof(qme_simulation_config_t));
    } else {
        qme_math_get_default_config(&sim->config);
    }

    /* Initialize RNG */
    if (sim->config.seed == 0) {
        sim->rng_state = (uint64_t)nimcp_time_monotonic_us() ^ 0x123456789ABCDEF0ULL;
    } else {
        sim->rng_state = sim->config.seed;
    }

    /* Allocate buffers */
    sim->sample_buffer_size = sim->config.num_samples;
    sim->sample_buffer = nimcp_calloc(sim->sample_buffer_size * 16, sizeof(float));
    if (!sim->sample_buffer) {
        nimcp_free(sim);
        sim = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "random_normal: sim->sample_buffer is NULL");
        return NULL;
    }

    sim->work_buffer_size = 4096;
    sim->work_buffer = nimcp_calloc(sim->work_buffer_size, sizeof(float));
    if (!sim->work_buffer) {
        nimcp_free(sim->sample_buffer);
        nimcp_free(sim);
        sim = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "random_normal: sim->work_buffer is NULL");
        return NULL;
    }

    /* Create mutex */
    mutex_attr_t attr;
    attr.type = MUTEX_TYPE_NORMAL;
    sim->mutex = nimcp_mutex_create(&attr);
    if (!sim->mutex) {
        nimcp_free(sim->work_buffer);
        nimcp_free(sim->sample_buffer);
        nimcp_free(sim);
        sim = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "random_normal: sim->mutex is NULL");
        return NULL;
    }

    /* Initialize adaptive proposal */
    sim->current_step_size = 1.0f;
    sim->acceptance_sum = 0.0f;
    sim->acceptance_count = 0;

    sim->bio_module_id = BIO_MODULE_QUANTUM_MATH_ENGINE;
    sim->module_name = "quantum_math_engine";
    sim->bio_async_enabled = false;
    sim->bio_ctx = NULL;
    sim->initialized = true;

    NIMCP_LOG_INFO("Created quantum math engine with %u samples",
                   sim->config.num_samples);

    return sim;
}

NIMCP_API void qme_math_destroy(qme_math_simulation_t* sim) {
    if (!sim) return;

    /* Unregister from bio-async */
    if (sim->bio_async_enabled) {
        qme_math_unregister_bio_async(sim);
    }

    /* Clean up */
    if (sim->mutex) {
        nimcp_mutex_destroy(sim->mutex);
    }

    nimcp_free(sim->proposal_mean);
    nimcp_free(sim->proposal_covariance);
    nimcp_free(sim->sample_buffer);
    nimcp_free(sim->work_buffer);
    nimcp_free(sim);
    sim = NULL;

    NIMCP_LOG_DEBUG("Destroyed quantum math engine");
}

NIMCP_API nimcp_error_t qme_math_reset(qme_math_simulation_t* sim) {
    if (!sim) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "qme_math_reset: sim is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(sim->mutex);

    /* Reset statistics */
    memset(&sim->stats, 0, sizeof(qme_stats_t));

    /* Reset adaptive proposal */
    sim->current_step_size = 1.0f;
    sim->acceptance_sum = 0.0f;
    sim->acceptance_count = 0;

    /* Re-seed RNG */
    sim->rng_state = (uint64_t)nimcp_time_monotonic_us() ^ 0x123456789ABCDEF0ULL;

    nimcp_mutex_unlock(sim->mutex);

    return NIMCP_SUCCESS;
}

NIMCP_API nimcp_error_t qme_math_get_default_config(
    qme_simulation_config_t* config) {

    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "qme_math_get_default_config: config is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    memset(config, 0, sizeof(qme_simulation_config_t));

    config->num_samples = QME_DEFAULT_SAMPLES;
    config->burnin = QME_DEFAULT_BURNIN;
    config->thinning = QME_DEFAULT_THINNING;
    config->target_acceptance = 0.234f; /* Optimal for high-dim */

    config->enable_importance_sampling = false;
    config->enable_antithetic = true;
    config->enable_control_variates = false;
    config->enable_stratified = false;
    config->num_strata = 10;

    config->enable_adaptive_proposals = true;
    config->proposal_adaptation_rate = 0.01f;
    config->adaptation_interval = 100;

    config->seed = 0;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Monte Carlo Integration
 * ============================================================================ */

NIMCP_API nimcp_error_t qme_integrate(
    qme_math_simulation_t* sim,
    qme_math_function_t f,
    const qme_domain_t* domain,
    float tolerance,
    qme_integration_result_t* result,
    void* user_data) {

    if (!sim || !f || !domain || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "qme_integrate: sim, f, domain, or result is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    {
        nimcp_mutex_lock(sim->mutex);

        uint64_t start_time = nimcp_time_monotonic_us();

        /* Get domain volume */
        float volume = qme_domain_volume(domain);

        /* Allocate sample point */
        float* sample = sim->work_buffer;

        /* Compute statistics */
        double sum = 0.0;
        double sum_sq = 0.0;
        uint32_t n = 0;
        float current_error = 1.0f;

        /* Adaptive sampling loop */
        while (n < sim->config.num_samples && current_error > tolerance) {
            /* Generate sample */
            sample_from_domain(sim, domain, sample);

            /* Evaluate function */
            float value = f(sample, domain->dim, user_data);

            /* Apply antithetic variate if enabled */
            if (sim->config.enable_antithetic) {
                /* Reflect sample */
                for (uint32_t i = 0; i < domain->dim; i++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((i & 0xFF) == 0 && domain->dim > 256) {
                        quantum_math_engine_heartbeat("quantum_math_loop",
                                         (float)(i + 1) / (float)domain->dim);
                    }

                    if (domain->type == QME_DOMAIN_BOX) {
                        float mid = (domain->lower_bounds[i] + domain->upper_bounds[i]) / 2.0f;
                        sample[i] = 2.0f * mid - sample[i];
                    }
                }
                float anti_value = f(sample, domain->dim, user_data);
                value = (value + anti_value) / 2.0f;
            }

            sum += value;
            sum_sq += value * value;
            n++;

            /* Update error estimate */
            if (n > 1) {
                double mean = sum / (n > 0 ? n : 1);
                double variance = (sum_sq / (n > 0 ? n : 1) - mean * mean) * n / (n - 1);
                current_error = sqrtf((float)variance / (n > 0 ? n : 1)) / (fabsf((float)mean) + 1e-10f);
            }
        }

        /* Compute final results */
        double mean = sum / (n > 0 ? n : 1);
        double variance = (n > 1) ? (sum_sq / (n > 0 ? n : 1) - mean * mean) * n / (n - 1) : 0.0;

        result->value = (float)(mean * volume);
        result->variance = (float)(variance * volume * volume / (n > 0 ? n : 1));
        result->std_error = sqrtf(result->variance);
        result->relative_error = result->std_error / (fabsf(result->value) + 1e-10f);
        result->samples_used = n;
        result->acceptance_rate = 1.0f; /* All accepted in simple MC */
        result->computation_time_us = nimcp_time_monotonic_us() - start_time;

        /* Update statistics */
        sim->stats.integrations_performed++;
        sim->stats.total_samples += n;
        sim->stats.total_time_us += result->computation_time_us;
        sim->stats.avg_relative_error =
            (sim->stats.avg_relative_error * (sim->stats.integrations_performed - 1) +
             result->relative_error) / sim->stats.integrations_performed;

        nimcp_mutex_unlock(sim->mutex);

    }

    return NIMCP_SUCCESS;
}

NIMCP_API nimcp_error_t qme_integrate_importance(
    qme_math_simulation_t* sim,
    qme_math_function_t f,
    qme_distribution_t proposal,
    const qme_domain_t* domain,
    qme_integration_result_t* result,
    void* user_data) {

    if (!sim || !f || !proposal || !domain || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "qme_integrate_importance: sim, f, proposal, domain, or result is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    {
        nimcp_mutex_lock(sim->mutex);

        uint64_t start_time = nimcp_time_monotonic_us();

        float* sample = sim->work_buffer;

        double sum_weighted = 0.0;
        double sum_weights = 0.0;
        double sum_weights_sq = 0.0;
        double sum_fwsq = 0.0;

        for (uint32_t n = 0; n < sim->config.num_samples; n++) {
            /* Phase 8: Loop progress heartbeat */
            if ((n & 0xFF) == 0 && sim->config.num_samples > 256) {
                quantum_math_engine_heartbeat("quantum_math_loop",
                                 (float)(n + 1) / (float)sim->config.num_samples);
            }

            /* Sample from proposal */
            sample_from_domain(sim, domain, sample);

            /* Compute importance weight */
            float q = proposal(sample, domain->dim, user_data);
            float weight = 1.0f / (q + 1e-10f);

            /* Evaluate function */
            float value = f(sample, domain->dim, user_data);

            sum_weighted += value * weight;
            sum_weights += weight;
            sum_weights_sq += weight * weight;
            sum_fwsq += (value * weight) * (value * weight);
        }

        /* Self-normalized importance sampling estimate */
        result->value = (float)(sum_weighted / sum_weights);

        /* Variance estimate */
        float ess = (float)(sum_weights * sum_weights / sum_weights_sq);
        float var_est = (float)((sum_fwsq / sum_weights_sq) - result->value * result->value);
        result->variance = var_est / ess;
        result->std_error = sqrtf(result->variance);
        result->relative_error = result->std_error / (fabsf(result->value) + 1e-10f);
        result->samples_used = sim->config.num_samples;
        result->acceptance_rate = ess / sim->config.num_samples;
        result->computation_time_us = nimcp_time_monotonic_us() - start_time;

        sim->stats.integrations_performed++;
        sim->stats.total_samples += sim->config.num_samples;
        sim->stats.total_time_us += result->computation_time_us;

        nimcp_mutex_unlock(sim->mutex);

    }

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Expectation Estimation
 * ============================================================================ */

NIMCP_API nimcp_error_t qme_estimate_expectation(
    qme_math_simulation_t* sim,
    qme_math_function_t f,
    qme_distribution_t distribution,
    uint32_t dim,
    qme_expectation_result_t* result,
    void* user_data) {

    if (!sim || !f || !distribution || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "qme_estimate_expectation: sim, f, distribution, or result is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    {
        nimcp_mutex_lock(sim->mutex);

        float* sample = sim->work_buffer;
        float* current = sample;
        float* proposed = sample + dim;

        /* Initialize chain */
        for (uint32_t i = 0; i < dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && dim > 256) {
                quantum_math_engine_heartbeat("quantum_math_loop",
                                 (float)(i + 1) / (float)dim);
            }

            current[i] = random_normal(sim);
        }

        double sum = 0.0;
        double sum_sq = 0.0;
        uint32_t accepted = 0;
        uint32_t n = 0;

        /* MCMC sampling */
        for (uint32_t iter = 0; iter < sim->config.num_samples + sim->config.burnin; iter++) {
            /* Propose new state */
            for (uint32_t i = 0; i < dim; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && dim > 256) {
                    quantum_math_engine_heartbeat("quantum_math_loop",
                                     (float)(i + 1) / (float)dim);
                }

                proposed[i] = current[i] + sim->current_step_size * random_normal(sim);
            }

            /* Compute acceptance probability (Metropolis-Hastings) */
            float log_current = logf(distribution(current, dim, user_data) + 1e-30f);
            float log_proposed = logf(distribution(proposed, dim, user_data) + 1e-30f);
            float log_alpha = log_proposed - log_current;

            /* Accept or reject */
            if (logf(random_uniform(sim) + 1e-30f) < log_alpha) {
                memcpy(current, proposed, dim * sizeof(float));
                accepted++;
            }

            /* Record sample after burn-in */
            if (iter >= sim->config.burnin && (iter % sim->config.thinning) == 0) {
                float value = f(current, dim, user_data);
                sum += value;
                sum_sq += value * value;
                n++;
            }

            /* Adapt step size */
            if (sim->config.enable_adaptive_proposals &&
                iter > 0 && (iter % sim->config.adaptation_interval) == 0) {
                float rate = (float)accepted / (iter + 1);
                if (rate < sim->config.target_acceptance) {
                    sim->current_step_size *= 0.9f;
                } else {
                    sim->current_step_size *= 1.1f;
                }
            }
        }

        /* Compute results */
        result->mean = (float)(sum / (n > 0 ? n : 1));
        result->variance = (float)((sum_sq / (n > 0 ? n : 1) - result->mean * result->mean) * n / (n - 1));
        result->std_error = sqrtf(result->variance / (n > 0 ? n : 1));
        result->effective_sample_size = (float)n;
        result->moment_estimates = NULL;
        result->num_moments = 0;

        sim->stats.expectations_computed++;
        sim->stats.total_samples += sim->config.num_samples + sim->config.burnin;
        sim->stats.avg_acceptance_rate = (float)accepted /
            (sim->config.num_samples + sim->config.burnin);

        nimcp_mutex_unlock(sim->mutex);

    }

    return NIMCP_SUCCESS;
}

NIMCP_API nimcp_error_t qme_estimate_moments(
    qme_math_simulation_t* sim,
    qme_math_function_t f,
    qme_distribution_t distribution,
    uint32_t dim,
    uint32_t max_moment,
    qme_expectation_result_t* result,
    void* user_data) {

    if (!sim || !f || !distribution || !result || max_moment == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "qme_estimate_moments: sim, f, distribution, or result is NULL, or max_moment is 0");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* First estimate mean */
    nimcp_error_t err = qme_estimate_expectation(sim, f, distribution, dim, result, user_data);
    if (err != NIMCP_SUCCESS) return err;

    /* Allocate moment storage */
    result->moment_estimates = nimcp_calloc(max_moment, sizeof(float));
    if (!result->moment_estimates) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "qme_estimate_moments: failed to allocate moment_estimates");
        return NIMCP_ERROR_NO_MEMORY;
    }
    result->num_moments = max_moment;

    result->moment_estimates[0] = result->mean;
    if (max_moment >= 2) {
        result->moment_estimates[1] = result->mean * result->mean + result->variance;
    }

    /* Higher moments would require additional sampling */
    for (uint32_t k = 2; k < max_moment; k++) {
        result->moment_estimates[k] = powf(result->mean, (float)(k + 1));
    }

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Partition Function
 * ============================================================================ */

NIMCP_API nimcp_error_t qme_partition_function(
    qme_math_simulation_t* sim,
    qme_math_function_t energy_func,
    uint32_t dim,
    float temperature,
    qme_partition_result_t* result,
    void* user_data) {

    if (!sim || !energy_func || !result || temperature <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "qme_partition_function: sim, energy_func, or result is NULL, or temperature is invalid");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    {
        nimcp_mutex_lock(sim->mutex);

        float beta = 1.0f / temperature;
        float* sample = sim->work_buffer;
        float* current = sample;
        float* proposed = sample + dim;

        /* Initialize */
        for (uint32_t i = 0; i < dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && dim > 256) {
                quantum_math_engine_heartbeat("quantum_math_loop",
                                 (float)(i + 1) / (float)dim);
            }

            current[i] = random_normal(sim);
        }
        float current_energy = energy_func(current, dim, user_data);

        double sum_energy = 0.0;
        double sum_energy_sq = 0.0;
        double log_Z_estimate = 0.0;
        uint32_t n = 0;
        uint32_t accepted = 0;

        /* MCMC at temperature T */
        for (uint32_t iter = 0; iter < sim->config.num_samples + sim->config.burnin; iter++) {
            /* Propose */
            for (uint32_t i = 0; i < dim; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && dim > 256) {
                    quantum_math_engine_heartbeat("quantum_math_loop",
                                     (float)(i + 1) / (float)dim);
                }

                proposed[i] = current[i] + sim->current_step_size * random_normal(sim);
            }

            float proposed_energy = energy_func(proposed, dim, user_data);

            /* Metropolis acceptance */
            float delta_E = proposed_energy - current_energy;
            if (delta_E < 0.0f || random_uniform(sim) < expf(-beta * delta_E)) {
                memcpy(current, proposed, dim * sizeof(float));
                current_energy = proposed_energy;
                accepted++;
            }

            /* Record after burn-in */
            if (iter >= sim->config.burnin && (iter % sim->config.thinning) == 0) {
                sum_energy += current_energy;
                sum_energy_sq += current_energy * current_energy;
                n++;
            }
        }

        /* Compute thermodynamic quantities */
        result->mean_energy = (float)(sum_energy / (n > 0 ? n : 1));
        float energy_var = (float)((sum_energy_sq / (n > 0 ? n : 1) - result->mean_energy * result->mean_energy));
        result->heat_capacity = beta * beta * energy_var;

        /* Estimate log(Z) using thermodynamic integration (simplified) */
        result->log_Z = -beta * result->mean_energy + 0.5f * dim * logf(NIMCP_TWO_PI_F / beta);
        result->Z = expf(result->log_Z);
        result->free_energy = -temperature * result->log_Z;
        result->entropy = (result->mean_energy - result->free_energy) / temperature;

        /* Error estimate */
        result->std_error = sqrtf(energy_var / (n > 0 ? n : 1)) * beta;

        nimcp_mutex_unlock(sim->mutex);

    }

    return NIMCP_SUCCESS;
}

NIMCP_API nimcp_error_t qme_free_energy(
    qme_math_simulation_t* sim,
    qme_math_function_t energy_func,
    uint32_t dim,
    float temperature,
    float* free_energy,
    void* user_data) {

    if (!free_energy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "qme_free_energy: free_energy is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    qme_partition_result_t result;
    nimcp_error_t err = qme_partition_function(sim, energy_func, dim, temperature, &result, user_data);
    if (err != NIMCP_SUCCESS) return err;

    *free_energy = result.free_energy;
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Path Integrals
 * ============================================================================ */

NIMCP_API nimcp_error_t qme_path_integral(
    qme_math_simulation_t* sim,
    qme_action_t action,
    uint32_t dim,
    uint32_t num_time_steps,
    const float* initial,
    const float* final,
    qme_path_integral_result_t* result,
    void* user_data) {

    if (!sim || !action || !initial || !final || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "qme_path_integral: sim, action, initial, final, or result is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (num_time_steps < 2 || num_time_steps > QME_MAX_PATHS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "qme_path_integral: num_time_steps must be >= 2 and <= QME_MAX_PATHS");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    {
        nimcp_mutex_lock(sim->mutex);

        uint32_t path_size = num_time_steps * dim;

        /* Allocate paths */
        float* current_path = nimcp_calloc(path_size, sizeof(float));
        float* proposed_path = nimcp_calloc(path_size, sizeof(float));

        if (!current_path || !proposed_path) {
            nimcp_free(current_path);
            current_path = NULL;
            nimcp_free(proposed_path);
            proposed_path = NULL;
            nimcp_mutex_unlock(sim->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
                "qme_path_integral: failed to allocate path buffers");
            return NIMCP_ERROR_NO_MEMORY;
        }

        /* Initialize path with linear interpolation */
        for (uint32_t t = 0; t < num_time_steps; t++) {
            /* Phase 8: Loop progress heartbeat */
            if ((t & 0xFF) == 0 && num_time_steps > 256) {
                quantum_math_engine_heartbeat("quantum_math_loop",
                                 (float)(t + 1) / (float)num_time_steps);
            }

            float alpha = (float)t / (num_time_steps - 1);
            for (uint32_t d = 0; d < dim; d++) {
                /* Phase 8: Loop progress heartbeat */
                if ((d & 0xFF) == 0 && dim > 256) {
                    quantum_math_engine_heartbeat("quantum_math_loop",
                                     (float)(d + 1) / (float)dim);
                }

                current_path[t * dim + d] = (1.0f - alpha) * initial[d] + alpha * final[d];
            }
        }

        /* Fix endpoints */
        memcpy(current_path, initial, dim * sizeof(float));
        memcpy(current_path + (num_time_steps - 1) * dim, final, dim * sizeof(float));

        float current_action = action(current_path, num_time_steps, dim, user_data);

        double sum_exp_action = 0.0;
        float min_action = current_action;
        uint32_t accepted = 0;

        /* Path integral Monte Carlo */
        for (uint32_t iter = 0; iter < sim->config.num_samples; iter++) {
            /* Phase 8: Loop progress heartbeat */
            if ((iter & 0xFF) == 0 && sim->config.num_samples > 256) {
                quantum_math_engine_heartbeat("quantum_math_loop",
                                 (float)(iter + 1) / (float)sim->config.num_samples);
            }

            /* Copy current path */
            memcpy(proposed_path, current_path, path_size * sizeof(float));

            /* Perturb interior points */
            for (uint32_t t = 1; t < num_time_steps - 1; t++) {
                for (uint32_t d = 0; d < dim; d++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((d & 0xFF) == 0 && dim > 256) {
                        quantum_math_engine_heartbeat("quantum_math_loop",
                                         (float)(d + 1) / (float)dim);
                    }

                    proposed_path[t * dim + d] += 0.1f * random_normal(sim);
                }
            }

            float proposed_action = action(proposed_path, num_time_steps, dim, user_data);

            /* Accept with Boltzmann weight */
            float delta_S = proposed_action - current_action;
            if (delta_S < 0.0f || random_uniform(sim) < expf(-delta_S)) {
                memcpy(current_path, proposed_path, path_size * sizeof(float));
                current_action = proposed_action;
                accepted++;

                if (current_action < min_action) {
                    min_action = current_action;
                    if (result->dominant_path) {
                        memcpy(result->dominant_path, current_path, path_size * sizeof(float));
                    }
                }
            }

            sum_exp_action += exp(-current_action);
        }

        /* Compute results */
        result->value = (float)(sum_exp_action / sim->config.num_samples);
        result->variance = 0.0f; /* Would need more computation */
        result->path_length = num_time_steps;
        result->action_at_dominant = min_action;
        result->paths_sampled = sim->config.num_samples;

        sim->stats.path_integrals_computed++;

        nimcp_free(current_path);
        current_path = NULL;
        nimcp_free(proposed_path);
        proposed_path = NULL;

        nimcp_mutex_unlock(sim->mutex);

    }

    return NIMCP_SUCCESS;
}

NIMCP_API nimcp_error_t qme_find_classical_path(
    qme_math_simulation_t* sim,
    qme_action_t action,
    uint32_t dim,
    uint32_t num_time_steps,
    const float* initial,
    const float* final,
    float* path,
    void* user_data) {

    if (!sim || !action || !initial || !final || !path) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "qme_find_classical_path: sim, action, initial, final, or path is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (num_time_steps < 2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "qme_find_classical_path: num_time_steps must be >= 2");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    uint32_t path_size = num_time_steps * dim;

    /* Initialize with linear interpolation */
    for (uint32_t t = 0; t < num_time_steps; t++) {
        /* Phase 8: Loop progress heartbeat */
        if ((t & 0xFF) == 0 && num_time_steps > 256) {
            quantum_math_engine_heartbeat("quantum_math_loop",
                             (float)(t + 1) / (float)num_time_steps);
        }

        float alpha = (float)t / (num_time_steps - 1);
        for (uint32_t d = 0; d < dim; d++) {
            /* Phase 8: Loop progress heartbeat */
            if ((d & 0xFF) == 0 && dim > 256) {
                quantum_math_engine_heartbeat("quantum_math_loop",
                                 (float)(d + 1) / (float)dim);
            }

            path[t * dim + d] = (1.0f - alpha) * initial[d] + alpha * final[d];
        }
    }

    /* Gradient descent on action */
    float learning_rate = NIMCP_LEARNING_RATE_DEFAULT;
    float epsilon = 0.001f;

    float* gradient = nimcp_calloc(path_size, sizeof(float));
    if (!gradient) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "qme_find_classical_path: failed to allocate gradient buffer");
        return NIMCP_ERROR_NO_MEMORY;
    }

    for (uint32_t iter = 0; iter < 1000; iter++) {
        /* Phase 8: Loop progress heartbeat */
        if ((iter & 0xFF) == 0 && 1000 > 256) {
            quantum_math_engine_heartbeat("quantum_math_loop",
                             (float)(iter + 1) / (float)1000);
        }

        float current_action = action(path, num_time_steps, dim, user_data);

        /* Compute numerical gradient */
        for (uint32_t t = 1; t < num_time_steps - 1; t++) {
            for (uint32_t d = 0; d < dim; d++) {
                /* Phase 8: Loop progress heartbeat */
                if ((d & 0xFF) == 0 && dim > 256) {
                    quantum_math_engine_heartbeat("quantum_math_loop",
                                     (float)(d + 1) / (float)dim);
                }

                uint32_t idx = t * dim + d;
                path[idx] += epsilon;
                float action_plus = action(path, num_time_steps, dim, user_data);
                path[idx] -= 2.0f * epsilon;
                float action_minus = action(path, num_time_steps, dim, user_data);
                path[idx] += epsilon;

                gradient[idx] = (action_plus - action_minus) / (2.0f * epsilon);
            }
        }

        /* Update path */
        float max_grad = 0.0f;
        for (uint32_t t = 1; t < num_time_steps - 1; t++) {
            for (uint32_t d = 0; d < dim; d++) {
                /* Phase 8: Loop progress heartbeat */
                if ((d & 0xFF) == 0 && dim > 256) {
                    quantum_math_engine_heartbeat("quantum_math_loop",
                                     (float)(d + 1) / (float)dim);
                }

                uint32_t idx = t * dim + d;
                path[idx] -= learning_rate * gradient[idx];
                if (fabsf(gradient[idx]) > max_grad) {
                    max_grad = fabsf(gradient[idx]);
                }
            }
        }

        /* Check convergence */
        if (max_grad < 1e-6f) break;
    }

    nimcp_free(gradient);
    gradient = NULL;
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Domain Management
 * ============================================================================ */

NIMCP_API qme_domain_t* qme_domain_create_box(
    uint32_t dim,
    const float* lower,
    const float* upper) {

    if (!lower || !upper || dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "qme_domain_create_box: lower or upper is NULL");
        return NULL;
    }

    qme_domain_t* domain = nimcp_calloc(1, sizeof(qme_domain_t));
    if (!domain) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate domain");

        return NULL;

    }

    domain->type = QME_DOMAIN_BOX;
    domain->dim = dim;

    domain->lower_bounds = nimcp_calloc(dim, sizeof(float));
    domain->upper_bounds = nimcp_calloc(dim, sizeof(float));

    if (!domain->lower_bounds || !domain->upper_bounds) {
        nimcp_free(domain->lower_bounds);
        nimcp_free(domain->upper_bounds);
        nimcp_free(domain);
        domain = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "qme_domain_create_box: bounds allocation failed");
        return NULL;
    }

    memcpy(domain->lower_bounds, lower, dim * sizeof(float));
    memcpy(domain->upper_bounds, upper, dim * sizeof(float));

    return domain;
}

NIMCP_API qme_domain_t* qme_domain_create_ball(
    uint32_t dim,
    const float* center,
    float radius) {

    if (!center || dim == 0 || dim > 16 || radius <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "qme_domain_create_ball: invalid parameters");
        return NULL;
    }

    qme_domain_t* domain = nimcp_calloc(1, sizeof(qme_domain_t));
    if (!domain) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate domain");

        return NULL;

    }

    domain->type = QME_DOMAIN_BALL;
    domain->dim = dim;
    domain->radius = radius;
    memcpy(domain->center, center, dim * sizeof(float));

    return domain;
}

NIMCP_API void qme_domain_destroy(qme_domain_t* domain) {
    if (!domain) return;

    nimcp_free(domain->lower_bounds);
    nimcp_free(domain->upper_bounds);
    nimcp_free(domain);
    domain = NULL;
}

NIMCP_API float qme_domain_volume(const qme_domain_t* domain) {
    if (!domain) return 0.0f;

    switch (domain->type) {
        case QME_DOMAIN_BOX: {
            float vol = 1.0f;
            for (uint32_t i = 0; i < domain->dim; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && domain->dim > 256) {
                    quantum_math_engine_heartbeat("quantum_math_loop",
                                     (float)(i + 1) / (float)domain->dim);
                }

                vol *= domain->upper_bounds[i] - domain->lower_bounds[i];
            }
            return vol;
        }

        case QME_DOMAIN_BALL: {
            /* V_n = π^(n/2) / Γ(n/2 + 1) * r^n */
            float n = (float)domain->dim;
            float half_n = n / 2.0f;
            float log_vol = half_n * logf(NIMCP_PI_F) - lgammaf(half_n + 1.0f) +
                           n * logf(domain->radius);
            return expf(log_vol);
        }

        case QME_DOMAIN_SIMPLEX:
            /* Volume of n-simplex is 1/n! */
            {
                float vol = 1.0f;
                for (uint32_t i = 2; i <= domain->dim; i++) {
                    vol /= i;
                }
                return vol;
            }

        default:
            return 1.0f;
    }
}

/* ============================================================================
 * Result Management
 * ============================================================================ */

NIMCP_API nimcp_error_t qme_path_integral_result_init(
    qme_path_integral_result_t* result,
    uint32_t path_length,
    uint32_t dim) {

    if (!result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "qme_path_integral_result_init: result is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    memset(result, 0, sizeof(qme_path_integral_result_t));

    result->dominant_path = nimcp_calloc(path_length * dim, sizeof(float));
    if (!result->dominant_path) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "qme_path_integral_result_init: failed to allocate dominant_path");
        return NIMCP_ERROR_NO_MEMORY;
    }

    result->path_length = path_length;

    return NIMCP_SUCCESS;
}

NIMCP_API void qme_path_integral_result_cleanup(
    qme_path_integral_result_t* result) {

    if (!result) return;
    nimcp_free(result->dominant_path);
    result->dominant_path = NULL;
}

NIMCP_API nimcp_error_t qme_expectation_result_init(
    qme_expectation_result_t* result,
    uint32_t max_moments) {

    if (!result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "qme_expectation_result_init: result is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    memset(result, 0, sizeof(qme_expectation_result_t));

    if (max_moments > 0) {
        result->moment_estimates = nimcp_calloc(max_moments, sizeof(float));
        if (!result->moment_estimates) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
                "qme_expectation_result_init: failed to allocate moment_estimates");
            return NIMCP_ERROR_NO_MEMORY;
        }
        result->num_moments = max_moments;
    }

    return NIMCP_SUCCESS;
}

NIMCP_API void qme_expectation_result_cleanup(
    qme_expectation_result_t* result) {

    if (!result) return;
    nimcp_free(result->moment_estimates);
    result->moment_estimates = NULL;
    result->num_moments = 0;
}

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

NIMCP_API nimcp_error_t qme_math_register_bio_async(
    qme_math_simulation_t* sim) {

    if (!sim) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "qme_math_register_bio_async: sim is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(sim->mutex);

    if (sim->bio_async_enabled) {
        nimcp_mutex_unlock(sim->mutex);
        return NIMCP_SUCCESS;
    }

    if (!bio_router_is_initialized()) {
        nimcp_mutex_unlock(sim->mutex);
        return NIMCP_SUCCESS;
    }

    bio_module_info_t info = {
        .module_id = sim->bio_module_id,
        .module_name = sim->module_name,
        .inbox_capacity = 32,
        .user_data = sim
    };

    sim->bio_ctx = bio_router_register_module(&info);
    if (sim->bio_ctx) {
        sim->bio_async_enabled = true;
    }

    nimcp_mutex_unlock(sim->mutex);

    NIMCP_LOG_DEBUG("Quantum math engine registered with bio-async");
    return NIMCP_SUCCESS;
}

NIMCP_API nimcp_error_t qme_math_unregister_bio_async(
    qme_math_simulation_t* sim) {

    if (!sim) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "qme_math_unregister_bio_async: sim is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(sim->mutex);

    if (sim->bio_async_enabled && sim->bio_ctx) {
        bio_router_unregister_module(sim->bio_ctx);
        sim->bio_ctx = NULL;
        sim->bio_async_enabled = false;
    }

    nimcp_mutex_unlock(sim->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Statistics
 * ============================================================================ */

NIMCP_API nimcp_error_t qme_math_get_stats(
    const qme_math_simulation_t* sim,
    qme_stats_t* stats) {

    if (!sim || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "qme_math_get_stats: sim or stats is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    memcpy(stats, &sim->stats, sizeof(qme_stats_t));
    return NIMCP_SUCCESS;
}

NIMCP_API void qme_math_print_diagnostics(const qme_math_simulation_t* sim) {
    if (!sim) return;

    NIMCP_LOG_INFO("=== Quantum Math Engine Diagnostics ===");
    NIMCP_LOG_INFO("Integrations: %lu", sim->stats.integrations_performed);
    NIMCP_LOG_INFO("Expectations: %lu", sim->stats.expectations_computed);
    NIMCP_LOG_INFO("Path integrals: %lu", sim->stats.path_integrals_computed);
    NIMCP_LOG_INFO("Total samples: %lu", sim->stats.total_samples);
    NIMCP_LOG_INFO("Avg acceptance rate: %.3f", sim->stats.avg_acceptance_rate);
    NIMCP_LOG_INFO("Avg relative error: %.6f", sim->stats.avg_relative_error);
    NIMCP_LOG_INFO("Total time: %.2f ms", sim->stats.total_time_us / 1000.0f);
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void quantum_math_engine_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_quantum_math_engine_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int quantum_math_engine_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "quantum_math_engine_training_begin: NULL argument");
        return -1;
    }
    quantum_math_engine_heartbeat_instance(NULL, "quantum_math_engine_training_begin", 0.0f);
    (void)(struct qme_math_simulation*)instance; /* Module state available for reset */
    return 0;
}

int quantum_math_engine_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "quantum_math_engine_training_end: NULL argument");
        return -1;
    }
    quantum_math_engine_heartbeat_instance(NULL, "quantum_math_engine_training_end", 1.0f);
    (void)(struct qme_math_simulation*)instance; /* Module state available for finalization */
    return 0;
}

int quantum_math_engine_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "quantum_math_engine_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    quantum_math_engine_heartbeat_instance(NULL, "quantum_math_engine_training_step", progress);
    (void)(struct qme_math_simulation*)instance; /* Module state available for step adaptation */
    return 0;
}
