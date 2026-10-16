/**
 * @file nimcp_dynamical_systems.c
 * @brief Dynamical Systems Analysis Implementation
 * @version 1.0.0
 * @date 2026-01-16
 */

#include "physics/dynamics/nimcp_dynamical_systems.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdio.h>

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(dynamical_systems)

#define LOG_TAG "dynamical_systems"

//=============================================================================
// Internal Structures
//=============================================================================

struct dynsys_system_struct {
    dynsys_config_t config;
    dynsys_func_t func;
    dynsys_jacobian_t jacobian;
    void* context;
    float* params;
    uint32_t param_dim;
    dynsys_stats_t stats;
    bool initialized;
};

struct dynsys_lyapunov_struct {
    dynsys_lyapunov_config_t config;
    dynsys_system_t sys;
    float* perturbations;
};

struct dynsys_bifurcation_struct {
    dynsys_bifurcation_config_t config;
    dynsys_system_t sys;
};

struct dynsys_attractor_struct {
    dynsys_attractor_config_t config;
};

struct dynsys_energy_struct {
    dynsys_energy_config_t config;
    dynsys_system_t sys;
};

struct dynsys_slowfast_struct {
    dynsys_slowfast_config_t config;
    dynsys_system_t sys;
};

struct dynsys_bridge_struct {
    dynsys_bridge_config_t config;
    dynsys_system_t sys;
    brain_kg_t* kg;
    void* exception_handler;
    void* bio_router;
    void* immune;
    bool initialized;
    /** W15: brain backpointer for runtime emit admin elevation. */
    brain_t kg_brain;
};

//=============================================================================
// Default Configurations
//=============================================================================

dynsys_config_t dynsys_default_config(void)
{
    dynsys_config_t config = {0};
    config.state_dim = 3;
    config.param_dim = 1;
    config.dt = DYNSYS_DEFAULT_DT;
    config.transient_time = 10.0f;
    config.analysis_time = 100.0f;
    config.enable_logging = true;
    config.enable_metrics = true;
    config.enable_kg_wiring = true;
    config.enable_exception_handling = true;
    return config;
}

dynsys_lyapunov_config_t dynsys_lyapunov_default_config(void)
{
    dynsys_lyapunov_config_t config = {0};
    config.num_exponents = 3;
    config.orthonormalization_interval = 1.0f;
    config.transient_steps = 1000;
    config.analysis_steps = 10000;
    config.perturbation_size = 1e-6f;
    return config;
}

dynsys_bifurcation_config_t dynsys_bifurcation_default_config(void)
{
    dynsys_bifurcation_config_t config = {0};
    config.param_index = 0;
    config.param_start = 0.0f;
    config.param_end = 4.0f;
    config.num_points = 200;
    config.transient_steps = 500;
    config.sample_steps = 100;
    config.tolerance = 1e-4f;
    return config;
}

dynsys_attractor_config_t dynsys_attractor_default_config(void)
{
    dynsys_attractor_config_t config = {0};
    config.embedding_dim = 3;
    config.time_delay = 10;
    config.observable_index = 0;
    config.num_samples = 10000;
    config.estimate_dimension = true;
    config.epsilon_min = 0.01f;
    config.epsilon_max = 1.0f;
    return config;
}

dynsys_energy_config_t dynsys_energy_default_config(void)
{
    dynsys_energy_config_t config = {0};
    config.grid_resolution = 50;
    config.state_min = -2.0f;
    config.state_max = 2.0f;
    config.find_minima = true;
    config.find_saddles = true;
    config.compute_barriers = false;
    return config;
}

dynsys_slowfast_config_t dynsys_slowfast_default_config(void)
{
    dynsys_slowfast_config_t config = {0};
    config.epsilon = 0.1f;
    config.num_slow = 1;
    config.num_fast = 1;
    config.slow_indices = NULL;
    config.fast_indices = NULL;
    config.compute_manifold = true;
    return config;
}

dynsys_bridge_config_t dynsys_bridge_default_config(void)
{
    dynsys_bridge_config_t config = {0};
    config.enable_kg_wiring = true;
    config.enable_exception_handling = true;
    config.enable_bio_async = true;
    config.enable_immune_presentation = true;
    config.enable_logging = true;
    config.log_level = 2;
    return config;
}

//=============================================================================
// System Lifecycle
//=============================================================================

dynsys_system_t dynsys_create(
    const dynsys_config_t* config,
    dynsys_func_t func,
    dynsys_jacobian_t jacobian,
    void* context)
{
    if (!config || !func) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dynsys_create: required parameter is NULL (config, func)");
        return NULL;
    }

    struct dynsys_system_struct* sys = nimcp_calloc(1, sizeof(struct dynsys_system_struct));
    NIMCP_API_CHECK_ALLOC(sys, "Failed to allocate dynamical system");

    memcpy(&sys->config, config, sizeof(dynsys_config_t));
    sys->func = func;
    sys->jacobian = jacobian;
    sys->context = context;
    sys->initialized = false;

    if (config->param_dim > 0) {
        sys->params = nimcp_calloc(config->param_dim, sizeof(float));
        if (!sys->params) {
            LOG_ERROR("Failed to allocate dynamical system parameters");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate dynamical system parameters");
            nimcp_free(sys);
            return NULL;
        }
        sys->param_dim = config->param_dim;
    }

    return sys;
}

void dynsys_destroy(dynsys_system_t sys)
{
    if (!sys) return;
    nimcp_free(sys->params);
    nimcp_free(sys);
}

dynsys_error_t dynsys_init(dynsys_system_t sys, nimcp_brain_t brain)
{
    if (!sys) return DYNSYS_ERR_NULL_PTR;
    (void)brain;
    sys->initialized = true;
    return DYNSYS_OK;
}

dynsys_error_t dynsys_shutdown(dynsys_system_t sys)
{
    if (!sys) return DYNSYS_ERR_NULL_PTR;
    sys->initialized = false;
    return DYNSYS_OK;
}

dynsys_error_t dynsys_set_params(dynsys_system_t sys, const float* params, uint32_t param_dim)
{
    if (!sys || !params) return DYNSYS_ERR_NULL_PTR;
    if (param_dim != sys->param_dim) return DYNSYS_ERR_INVALID_DIM;

    memcpy(sys->params, params, param_dim * sizeof(float));
    return DYNSYS_OK;
}

dynsys_error_t dynsys_integrate(dynsys_system_t sys, float* state, uint32_t num_steps, float* trajectory)
{
    if (!sys || !state) return DYNSYS_ERR_NULL_PTR;

    for (uint32_t i = 0; i < num_steps; i++) {
        dynsys_error_t err = dynsys_step_rk4(sys, state, sys->config.dt);
        if (err != DYNSYS_OK) return err;

        if (trajectory) {
            memcpy(&trajectory[i * sys->config.state_dim], state,
                   sys->config.state_dim * sizeof(float));
        }

        sys->stats.integrations++;
    }

    return DYNSYS_OK;
}

dynsys_error_t dynsys_step_rk4(dynsys_system_t sys, float* state, float dt)
{
    if (!sys || !state) return DYNSYS_ERR_NULL_PTR;

    uint32_t dim = sys->config.state_dim;
    float* k1 = nimcp_calloc(dim, sizeof(float));
    float* k2 = nimcp_calloc(dim, sizeof(float));
    float* k3 = nimcp_calloc(dim, sizeof(float));
    float* k4 = nimcp_calloc(dim, sizeof(float));
    float* temp = nimcp_calloc(dim, sizeof(float));

    if (!k1 || !k2 || !k3 || !k4 || !temp) {
        nimcp_free(k1); nimcp_free(k2); nimcp_free(k3); nimcp_free(k4); nimcp_free(temp);
        return DYNSYS_ERR_NO_MEMORY;
    }

    /* k1 = f(t, y) */
    sys->func(state, dim, sys->params, sys->param_dim, k1, sys->context);

    /* k2 = f(t + dt/2, y + dt/2 * k1) */
    for (uint32_t i = 0; i < dim; i++) {
        temp[i] = state[i] + 0.5f * dt * k1[i];
    }
    sys->func(temp, dim, sys->params, sys->param_dim, k2, sys->context);

    /* k3 = f(t + dt/2, y + dt/2 * k2) */
    for (uint32_t i = 0; i < dim; i++) {
        temp[i] = state[i] + 0.5f * dt * k2[i];
    }
    sys->func(temp, dim, sys->params, sys->param_dim, k3, sys->context);

    /* k4 = f(t + dt, y + dt * k3) */
    for (uint32_t i = 0; i < dim; i++) {
        temp[i] = state[i] + dt * k3[i];
    }
    sys->func(temp, dim, sys->params, sys->param_dim, k4, sys->context);

    /* y(t+dt) = y(t) + dt/6 * (k1 + 2*k2 + 2*k3 + k4) */
    for (uint32_t i = 0; i < dim; i++) {
        state[i] += dt / 6.0f * (k1[i] + 2.0f * k2[i] + 2.0f * k3[i] + k4[i]);

        /* Check for divergence */
        if (!isfinite(state[i])) {
            nimcp_free(k1); nimcp_free(k2); nimcp_free(k3); nimcp_free(k4); nimcp_free(temp);
            sys->stats.divergences++;
            return DYNSYS_ERR_DIVERGENCE;
        }
    }

    nimcp_free(k1); nimcp_free(k2); nimcp_free(k3); nimcp_free(k4); nimcp_free(temp);
    return DYNSYS_OK;
}

dynsys_error_t dynsys_get_stats(dynsys_system_t sys, dynsys_stats_t* stats)
{
    if (!sys || !stats) return DYNSYS_ERR_NULL_PTR;
    memcpy(stats, &sys->stats, sizeof(dynsys_stats_t));
    return DYNSYS_OK;
}

//=============================================================================
// Lyapunov Exponents
//=============================================================================

dynsys_lyapunov_t dynsys_lyapunov_create(const dynsys_lyapunov_config_t* config, dynsys_system_t sys)
{
    if (!config || !sys) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dynsys_lyapunov_create: required parameter is NULL (config, sys)");
        return NULL;
    }

    struct dynsys_lyapunov_struct* lyap = nimcp_calloc(1, sizeof(struct dynsys_lyapunov_struct));
    if (!lyap) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dynsys_lyapunov_create: lyap is NULL");
        return NULL;
    }

    memcpy(&lyap->config, config, sizeof(dynsys_lyapunov_config_t));
    lyap->sys = sys;

    uint32_t dim = sys->config.state_dim;
    uint32_t num_exp = config->num_exponents;
    if (num_exp > dim) num_exp = dim;

    lyap->perturbations = nimcp_calloc(dim * num_exp, sizeof(float));
    if (!lyap->perturbations) {
        nimcp_free(lyap);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dynsys_lyapunov_create: lyap->perturbations is NULL");
        return NULL;
    }

    return lyap;
}

void dynsys_lyapunov_destroy(dynsys_lyapunov_t lyap)
{
    if (!lyap) return;
    nimcp_free(lyap->perturbations);
    nimcp_free(lyap);
}

static void gram_schmidt(float* vectors, uint32_t num_vectors, uint32_t dim, float* norms)
{
    for (uint32_t i = 0; i < num_vectors; i++) {
        float* vi = &vectors[i * dim];

        /* Orthogonalize against previous vectors */
        for (uint32_t j = 0; j < i; j++) {
            float* vj = &vectors[j * dim];
            float dot = 0.0f;
            for (uint32_t k = 0; k < dim; k++) {
                dot += vi[k] * vj[k];
            }
            for (uint32_t k = 0; k < dim; k++) {
                vi[k] -= dot * vj[k];
            }
        }

        /* Normalize */
        float norm = 0.0f;
        for (uint32_t k = 0; k < dim; k++) {
            norm += vi[k] * vi[k];
        }
        norm = sqrtf(norm);
        norms[i] = norm;

        if (norm > 1e-10f) {
            for (uint32_t k = 0; k < dim; k++) {
                vi[k] /= norm;
            }
        }
    }
}

dynsys_error_t dynsys_lyapunov_compute(dynsys_lyapunov_t lyap, const float* initial_state,
                                        dynsys_lyapunov_result_t* result)
{
    if (!lyap || !initial_state || !result) return DYNSYS_ERR_NULL_PTR;

    dynsys_system_t sys = lyap->sys;
    uint32_t dim = sys->config.state_dim;
    uint32_t num_exp = lyap->config.num_exponents;
    if (num_exp > dim) num_exp = dim;
    if (num_exp > DYNSYS_MAX_LYAPUNOV) num_exp = DYNSYS_MAX_LYAPUNOV;

    float* state = nimcp_calloc(dim, sizeof(float));
    float* perturb_state = nimcp_calloc(dim, sizeof(float));
    float* norms = nimcp_calloc(num_exp, sizeof(float));

    if (!state || !perturb_state || !norms) {
        nimcp_free(state); nimcp_free(perturb_state); nimcp_free(norms);
        return DYNSYS_ERR_NO_MEMORY;
    }

    memcpy(state, initial_state, dim * sizeof(float));
    memset(result, 0, sizeof(dynsys_lyapunov_result_t));
    result->num_exponents = num_exp;

    /* Initialize orthonormal perturbation vectors */
    for (uint32_t i = 0; i < num_exp; i++) {
        for (uint32_t j = 0; j < dim; j++) {
            lyap->perturbations[i * dim + j] = (i == j) ? lyap->config.perturbation_size : 0.0f;
        }
    }

    /* Discard transient */
    dynsys_integrate(sys, state, lyap->config.transient_steps, NULL);

    /* Main Lyapunov computation loop */
    float dt = sys->config.dt;
    uint32_t ortho_steps = (uint32_t)(lyap->config.orthonormalization_interval / dt);
    if (ortho_steps < 1) ortho_steps = 1;

    float* exponent_sums = nimcp_calloc(num_exp, sizeof(float));
    if (!exponent_sums) {
        nimcp_free(state); nimcp_free(perturb_state); nimcp_free(norms);
        return DYNSYS_ERR_NO_MEMORY;
    }

    uint32_t total_steps = lyap->config.analysis_steps;
    uint32_t ortho_count = 0;

    for (uint32_t step = 0; step < total_steps; step++) {
        /* Evolve reference trajectory */
        dynsys_step_rk4(sys, state, dt);

        /* Evolve perturbation vectors */
        for (uint32_t i = 0; i < num_exp; i++) {
            memcpy(perturb_state, state, dim * sizeof(float));
            for (uint32_t j = 0; j < dim; j++) {
                perturb_state[j] += lyap->perturbations[i * dim + j];
            }
            dynsys_step_rk4(sys, perturb_state, dt);

            for (uint32_t j = 0; j < dim; j++) {
                lyap->perturbations[i * dim + j] = perturb_state[j] - state[j];
            }
        }

        /* Orthonormalize periodically */
        if ((step + 1) % ortho_steps == 0) {
            gram_schmidt(lyap->perturbations, num_exp, dim, norms);

            for (uint32_t i = 0; i < num_exp; i++) {
                if (norms[i] > 1e-10f) {
                    exponent_sums[i] += logf(norms[i] / lyap->config.perturbation_size);
                }
            }
            ortho_count++;

            /* Rescale perturbations */
            for (uint32_t i = 0; i < num_exp; i++) {
                for (uint32_t j = 0; j < dim; j++) {
                    lyap->perturbations[i * dim + j] *= lyap->config.perturbation_size;
                }
            }
        }
    }

    /* Compute final exponents */
    float total_time = total_steps * dt;
    result->sum_lyapunov = 0.0f;

    for (uint32_t i = 0; i < num_exp; i++) {
        result->exponents[i] = exponent_sums[i] / total_time;
        result->sum_lyapunov += result->exponents[i];
    }

    result->max_lyapunov = result->exponents[0];
    result->is_chaotic = result->max_lyapunov > 0.0f;

    /* Kaplan-Yorke dimension */
    float ky_sum = 0.0f;
    uint32_t j = 0;
    for (j = 0; j < num_exp && ky_sum + result->exponents[j] >= 0; j++) {
        ky_sum += result->exponents[j];
    }
    if (j > 0 && j < num_exp && result->exponents[j] < 0) {
        result->kaplan_yorke_dim = j + ky_sum / fabsf(result->exponents[j]);
    } else {
        result->kaplan_yorke_dim = (float)j;
    }

    /* Kolmogorov-Sinai entropy (sum of positive exponents) */
    result->entropy_rate = 0.0f;
    for (uint32_t i = 0; i < num_exp; i++) {
        if (result->exponents[i] > 0) {
            result->entropy_rate += result->exponents[i];
        }
    }

    sys->stats.lyapunov_computations++;

    nimcp_free(state); nimcp_free(perturb_state); nimcp_free(norms); nimcp_free(exponent_sums);
    return DYNSYS_OK;
}

dynsys_error_t dynsys_lyapunov_max(dynsys_lyapunov_t lyap, const float* initial_state, float* max_exponent)
{
    if (!lyap || !initial_state || !max_exponent) return DYNSYS_ERR_NULL_PTR;

    dynsys_lyapunov_result_t result;
    dynsys_error_t err = dynsys_lyapunov_compute(lyap, initial_state, &result);
    if (err != DYNSYS_OK) return err;

    *max_exponent = result.max_lyapunov;
    return DYNSYS_OK;
}

//=============================================================================
// Bifurcation Analysis
//=============================================================================

dynsys_bifurcation_t dynsys_bifurcation_create(const dynsys_bifurcation_config_t* config, dynsys_system_t sys)
{
    if (!config || !sys) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dynsys_bifurcation_create: required parameter is NULL (config, sys)");
        return NULL;
    }

    struct dynsys_bifurcation_struct* bif = nimcp_calloc(1, sizeof(struct dynsys_bifurcation_struct));
    if (!bif) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dynsys_bifurcation_create: bif is NULL");
        return NULL;
    }

    memcpy(&bif->config, config, sizeof(dynsys_bifurcation_config_t));
    bif->sys = sys;

    return bif;
}

void dynsys_bifurcation_destroy(dynsys_bifurcation_t bif)
{
    if (!bif) return;
    nimcp_free(bif);
}

dynsys_error_t dynsys_bifurcation_scan(dynsys_bifurcation_t bif, const float* initial_state,
                                        dynsys_bifurcation_result_t* result)
{
    if (!bif || !initial_state || !result) return DYNSYS_ERR_NULL_PTR;

    dynsys_system_t sys = bif->sys;
    uint32_t dim = sys->config.state_dim;
    uint32_t num_points = bif->config.num_points;
    if (num_points < 2) return DYNSYS_ERR_INVALID_DIM;
    uint32_t samples_per_point = bif->config.sample_steps;

    memset(result, 0, sizeof(dynsys_bifurcation_result_t));

    result->parameter_values = nimcp_calloc(num_points, sizeof(float));
    result->state_samples = nimcp_calloc(num_points * samples_per_point * dim, sizeof(float));
    result->bifurcations = nimcp_calloc(num_points, sizeof(dynsys_bifurcation_point_t));

    if (!result->parameter_values || !result->state_samples || !result->bifurcations) {
        dynsys_bifurcation_result_free(result);
        return DYNSYS_ERR_NO_MEMORY;
    }

    float* state = nimcp_calloc(dim, sizeof(float));
    float* params = nimcp_calloc(sys->param_dim, sizeof(float));
    if (!state || !params) {
        nimcp_free(state); nimcp_free(params);
        dynsys_bifurcation_result_free(result);
        return DYNSYS_ERR_NO_MEMORY;
    }

    memcpy(params, sys->params, sys->param_dim * sizeof(float));
    memcpy(state, initial_state, dim * sizeof(float));

    float param_step = (bif->config.param_end - bif->config.param_start) / (float)(num_points - 1);
    result->num_param_points = num_points;
    result->samples_per_point = samples_per_point;

    for (uint32_t p = 0; p < num_points; p++) {
        float param_val = bif->config.param_start + p * param_step;
        result->parameter_values[p] = param_val;

        params[bif->config.param_index] = param_val;
        dynsys_set_params(sys, params, sys->param_dim);

        /* Discard transient */
        dynsys_integrate(sys, state, bif->config.transient_steps, NULL);

        /* Collect samples */
        for (uint32_t s = 0; s < samples_per_point; s++) {
            dynsys_integrate(sys, state, 1, NULL);
            memcpy(&result->state_samples[(p * samples_per_point + s) * dim],
                   state, dim * sizeof(float));
        }
    }

    sys->stats.bifurcation_scans++;

    nimcp_free(state);
    nimcp_free(params);
    return DYNSYS_OK;
}

void dynsys_bifurcation_result_free(dynsys_bifurcation_result_t* result)
{
    if (!result) return;
    nimcp_free(result->parameter_values);
    nimcp_free(result->state_samples);
    nimcp_free(result->bifurcations);
    memset(result, 0, sizeof(dynsys_bifurcation_result_t));
}

const char* dynsys_bifurcation_type_name(bifurcation_type_t type)
{
    switch (type) {
        case BIFURCATION_NONE: return "none";
        case BIFURCATION_SADDLE_NODE: return "saddle-node";
        case BIFURCATION_TRANSCRITICAL: return "transcritical";
        case BIFURCATION_PITCHFORK: return "pitchfork";
        case BIFURCATION_HOPF: return "Hopf";
        case BIFURCATION_PERIOD_DOUBLING: return "period-doubling";
        case BIFURCATION_NEIMARK_SACKER: return "Neimark-Sacker";
        case BIFURCATION_HOMOCLINIC: return "homoclinic";
        case BIFURCATION_HETEROCLINIC: return "heteroclinic";
        default: return "unknown";
    }
}

//=============================================================================
// Attractor Reconstruction
//=============================================================================

dynsys_attractor_t dynsys_attractor_create(const dynsys_attractor_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dynsys_attractor_create: config is NULL");
        return NULL;
    }

    struct dynsys_attractor_struct* attr = nimcp_calloc(1, sizeof(struct dynsys_attractor_struct));
    if (!attr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dynsys_attractor_create: attr is NULL");
        return NULL;
    }

    memcpy(&attr->config, config, sizeof(dynsys_attractor_config_t));
    return attr;
}

void dynsys_attractor_destroy(dynsys_attractor_t attr)
{
    if (!attr) return;
    nimcp_free(attr);
}

dynsys_error_t dynsys_attractor_reconstruct(dynsys_attractor_t attr, const float* time_series,
                                             uint32_t series_length, dynsys_attractor_result_t* result)
{
    if (!attr || !time_series || !result) return DYNSYS_ERR_NULL_PTR;

    uint32_t embed_dim = attr->config.embedding_dim;
    uint32_t delay = attr->config.time_delay;

    /* Guard against uint32_t underflow: (embed_dim - 1) * delay may exceed series_length */
    uint32_t product = (embed_dim > 0) ? (embed_dim - 1) * delay : 0;
    if (product >= series_length) return DYNSYS_ERR_INVALID_DIM;

    uint32_t num_points = series_length - product;

    if (num_points < 10) return DYNSYS_ERR_INVALID_DIM;

    memset(result, 0, sizeof(dynsys_attractor_result_t));

    result->embedded_points = nimcp_calloc(num_points * embed_dim, sizeof(float));
    if (!result->embedded_points) return DYNSYS_ERR_NO_MEMORY;

    result->num_points = num_points;
    result->embedding_dim = embed_dim;

    /* Takens embedding */
    for (uint32_t i = 0; i < num_points; i++) {
        for (uint32_t d = 0; d < embed_dim; d++) {
            result->embedded_points[i * embed_dim + d] = time_series[i + d * delay];
        }
    }

    /* Simple attractor type detection */
    result->type = ATTRACTOR_STRANGE;  /* Placeholder */
    result->correlation_dim = 2.0f;     /* Placeholder */
    result->box_dim = 2.1f;
    result->recurrence_rate = 0.05f;
    result->determinism = 0.8f;

    return DYNSYS_OK;
}

void dynsys_attractor_result_free(dynsys_attractor_result_t* result)
{
    if (!result) return;
    nimcp_free(result->embedded_points);
    memset(result, 0, sizeof(dynsys_attractor_result_t));
}

dynsys_error_t dynsys_attractor_estimate_params(dynsys_attractor_t attr, const float* time_series,
                                                 uint32_t series_length, uint32_t* optimal_dim,
                                                 uint32_t* optimal_delay)
{
    if (!attr || !time_series || !optimal_dim || !optimal_delay) return DYNSYS_ERR_NULL_PTR;
    (void)series_length;

    /* Simplified: return defaults */
    *optimal_dim = 3;
    *optimal_delay = 10;
    return DYNSYS_OK;
}

const char* dynsys_attractor_type_name(attractor_type_t type)
{
    switch (type) {
        case ATTRACTOR_UNKNOWN: return "unknown";
        case ATTRACTOR_FIXED_POINT: return "fixed point";
        case ATTRACTOR_LIMIT_CYCLE: return "limit cycle";
        case ATTRACTOR_TORUS: return "torus";
        case ATTRACTOR_STRANGE: return "strange attractor";
        case ATTRACTOR_CHAOTIC_SADDLE: return "chaotic saddle";
        default: return "unknown";
    }
}

//=============================================================================
// Energy Landscape
//=============================================================================

dynsys_energy_t dynsys_energy_create(const dynsys_energy_config_t* config, dynsys_system_t sys)
{
    if (!config || !sys) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dynsys_energy_create: required parameter is NULL (config, sys)");
        return NULL;
    }

    struct dynsys_energy_struct* energy = nimcp_calloc(1, sizeof(struct dynsys_energy_struct));
    if (!energy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dynsys_energy_create: energy is NULL");
        return NULL;
    }

    memcpy(&energy->config, config, sizeof(dynsys_energy_config_t));
    energy->sys = sys;
    return energy;
}

void dynsys_energy_destroy(dynsys_energy_t energy)
{
    if (!energy) return;
    nimcp_free(energy);
}

dynsys_error_t dynsys_energy_compute(dynsys_energy_t energy, dynsys_energy_result_t* result)
{
    if (!energy || !result) return DYNSYS_ERR_NULL_PTR;

    uint32_t res = energy->config.grid_resolution;
    if (res < 2) return DYNSYS_ERR_INVALID_DIM;
    uint32_t grid_size = res * res;  /* 2D for simplicity */

    memset(result, 0, sizeof(dynsys_energy_result_t));

    result->energy_values = nimcp_calloc(grid_size, sizeof(float));
    result->gradient = nimcp_calloc(grid_size * 2, sizeof(float));
    result->minima_locations = nimcp_calloc(10 * 2, sizeof(float));
    result->minima_energies = nimcp_calloc(10, sizeof(float));

    if (!result->energy_values || !result->gradient) {
        dynsys_energy_result_free(result);
        return DYNSYS_ERR_NO_MEMORY;
    }

    result->grid_size = grid_size;

    float range = energy->config.state_max - energy->config.state_min;
    float step = range / (float)(res - 1);

    /* Compute energy landscape (placeholder: use quadratic well) */
    for (uint32_t i = 0; i < res; i++) {
        for (uint32_t j = 0; j < res; j++) {
            float x = energy->config.state_min + i * step;
            float y = energy->config.state_min + j * step;
            result->energy_values[i * res + j] = x * x + y * y;  /* Simple quadratic */
        }
    }

    result->num_minima = 1;
    result->minima_locations[0] = 0.0f;
    result->minima_locations[1] = 0.0f;
    result->minima_energies[0] = 0.0f;
    result->global_minimum = 0.0f;

    energy->sys->stats.energy_analyses++;

    return DYNSYS_OK;
}

void dynsys_energy_result_free(dynsys_energy_result_t* result)
{
    if (!result) return;
    nimcp_free(result->energy_values);
    nimcp_free(result->gradient);
    nimcp_free(result->minima_locations);
    nimcp_free(result->minima_energies);
    nimcp_free(result->saddle_locations);
    nimcp_free(result->barriers);
    memset(result, 0, sizeof(dynsys_energy_result_t));
}

dynsys_error_t dynsys_energy_find_minimum(dynsys_energy_t energy, const float* initial_state,
                                           float* minimum_state, float* minimum_energy)
{
    if (!energy || !initial_state || !minimum_state || !minimum_energy) return DYNSYS_ERR_NULL_PTR;

    uint32_t dim = energy->sys->config.state_dim;
    memcpy(minimum_state, initial_state, dim * sizeof(float));

    /* Simple gradient descent placeholder */
    *minimum_energy = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        *minimum_energy += minimum_state[i] * minimum_state[i];
        minimum_state[i] = 0.0f;  /* Move to origin */
    }
    *minimum_energy = 0.0f;

    return DYNSYS_OK;
}

dynsys_error_t dynsys_energy_barrier(dynsys_energy_t energy, const float* state_a, const float* state_b,
                                      float* barrier_height, float* saddle_state)
{
    if (!energy || !state_a || !state_b || !barrier_height) return DYNSYS_ERR_NULL_PTR;

    uint32_t dim = energy->sys->config.state_dim;

    /* Placeholder: compute simple interpolation */
    *barrier_height = 1.0f;

    if (saddle_state) {
        for (uint32_t i = 0; i < dim; i++) {
            saddle_state[i] = 0.5f * (state_a[i] + state_b[i]);
        }
    }

    return DYNSYS_OK;
}

//=============================================================================
// Slow-Fast Decomposition
//=============================================================================

dynsys_slowfast_t dynsys_slowfast_create(const dynsys_slowfast_config_t* config, dynsys_system_t sys)
{
    if (!config || !sys) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dynsys_slowfast_create: required parameter is NULL (config, sys)");
        return NULL;
    }

    struct dynsys_slowfast_struct* sf = nimcp_calloc(1, sizeof(struct dynsys_slowfast_struct));
    if (!sf) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dynsys_slowfast_create: sf is NULL");
        return NULL;
    }

    memcpy(&sf->config, config, sizeof(dynsys_slowfast_config_t));
    sf->sys = sys;
    return sf;
}

void dynsys_slowfast_destroy(dynsys_slowfast_t sf)
{
    if (!sf) return;
    nimcp_free(sf);
}

dynsys_error_t dynsys_slowfast_compute(dynsys_slowfast_t sf, dynsys_slowfast_result_t* result)
{
    if (!sf || !result) return DYNSYS_ERR_NULL_PTR;

    memset(result, 0, sizeof(dynsys_slowfast_result_t));
    result->timescale_ratio = 1.0f / sf->config.epsilon;
    result->manifold_exists = true;
    result->manifold_points = 100;

    return DYNSYS_OK;
}

void dynsys_slowfast_result_free(dynsys_slowfast_result_t* result)
{
    if (!result) return;
    nimcp_free(result->slow_manifold);
    nimcp_free(result->fast_manifold);
    nimcp_free(result->slow_flow);
    memset(result, 0, sizeof(dynsys_slowfast_result_t));
}

dynsys_error_t dynsys_slowfast_project(dynsys_slowfast_t sf, const float* state, float* projected_state)
{
    if (!sf || !state || !projected_state) return DYNSYS_ERR_NULL_PTR;

    uint32_t dim = sf->sys->config.state_dim;
    memcpy(projected_state, state, dim * sizeof(float));

    return DYNSYS_OK;
}

//=============================================================================
// Bridge Implementation
//=============================================================================

dynsys_bridge_t dynsys_bridge_create(const dynsys_bridge_config_t* config, dynsys_system_t sys)
{
    if (!config || !sys) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dynsys_bridge_create: required parameter is NULL (config, sys)");
        return NULL;
    }

    struct dynsys_bridge_struct* bridge = nimcp_calloc(1, sizeof(struct dynsys_bridge_struct));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dynsys_bridge_create: bridge is NULL");
        return NULL;
    }

    memcpy(&bridge->config, config, sizeof(dynsys_bridge_config_t));
    bridge->sys = sys;
    bridge->initialized = false;

    return bridge;
}

void dynsys_bridge_destroy(dynsys_bridge_t bridge)
{
    if (!bridge) return;
    nimcp_free(bridge);
}

int dynsys_bridge_init(dynsys_bridge_t bridge, nimcp_brain_t brain,
                       nimcp_bio_router_t router, nimcp_brain_immune_t immune)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dynsys_bridge_init: bridge is NULL");
        return -1;
    }
    (void)brain;

    bridge->bio_router = router;
    bridge->immune = immune;
    bridge->initialized = true;

    if (bridge->config.enable_logging) {
        NIMCP_LOG_INFO(LOG_TAG, "Dynamical systems bridge initialized");
    }

    return 0;
}

int dynsys_bridge_shutdown(dynsys_bridge_t bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dynsys_bridge_shutdown: bridge is NULL");
        return -1;
    }

    bridge->initialized = false;

    if (bridge->config.enable_logging) {
        NIMCP_LOG_INFO(LOG_TAG, "Dynamical systems bridge shutdown");
    }

    return 0;
}

kg_module_wiring_t* dynsys_bridge_create_wiring(dynsys_bridge_t bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dynsys_bridge_create_wiring: bridge is NULL");
        return NULL;
    }
    /* TODO: Would create module wiring descriptor - stub returns NULL */
    return NULL;
}

int dynsys_bridge_register_kg(dynsys_bridge_t bridge, brain_kg_t* kg)
{
    if (!bridge || !kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dynsys_bridge_register_kg: required parameter is NULL (bridge, kg)");
        return -1;
    }

    if (!bridge->config.enable_kg_wiring) {
        return 0;
    }

    brain_kg_node_id_t root_id = brain_kg_add_node(kg, DYNSYS_MODULE_NAME,
                                                    BRAIN_KG_NODE_INTEGRATION,
                                                    "Dynamical systems analysis module");
    if (root_id == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dynsys_bridge_register_kg: validation failed");
        return -1;
    }

    brain_kg_node_id_t lyap_id = brain_kg_add_node(kg, "lyapunov_analysis",
                                                    BRAIN_KG_NODE_UTILITY,
                                                    "Lyapunov exponent computation");
    if (lyap_id != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, root_id, lyap_id, BRAIN_KG_EDGE_CONNECTS_TO,
                          "Contains Lyapunov analysis", 1.0f);
    }

    brain_kg_node_id_t bif_id = brain_kg_add_node(kg, "bifurcation_analysis",
                                                   BRAIN_KG_NODE_UTILITY,
                                                   "Bifurcation detection");
    if (bif_id != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, root_id, bif_id, BRAIN_KG_EDGE_CONNECTS_TO,
                          "Contains bifurcation analysis", 1.0f);
    }

    brain_kg_node_id_t attr_id = brain_kg_add_node(kg, "attractor_analysis",
                                                    BRAIN_KG_NODE_UTILITY,
                                                    "Attractor reconstruction");
    if (attr_id != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, root_id, attr_id, BRAIN_KG_EDGE_CONNECTS_TO,
                          "Contains attractor analysis", 1.0f);
    }

    bridge->kg = kg;

    if (bridge->config.enable_logging) {
        NIMCP_LOG_INFO(LOG_TAG, "Registered dynamical systems nodes in KG");
    }

    return 0;
}

/*=============================================================================
 * W15: Runtime bifurcation / attractor-switch event emission + read path
 *===========================================================================*/

void dynsys_kg_register_brain(dynsys_bridge_t bridge, brain_t brain) {
    if (!bridge) return;
    bridge->kg_brain = brain;
}

void dynsys_kg_trigger_bifurcation_event(dynsys_bridge_t bridge,
                                         const char* kind,
                                         float param_value,
                                         float lyapunov_max,
                                         uint64_t ts_us) {
    if (!bridge || !kind || !bridge->kg) return;

    brain_kg_t* kg = bridge->kg;
    uint64_t token = 0;
    brain_t b = bridge->kg_brain;
    if (b) token = b->internal_kg_admin_token;
    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_ADMIN, token);

    char ev_name[160];
    snprintf(ev_name, sizeof(ev_name),
             "dynamical_systems_event_%s_%llu",
             kind, (unsigned long long)ts_us);
    char desc[240];
    snprintf(desc, sizeof(desc),
             "Dynamical-systems state transition: kind=%s param=%.4f lambda_max=%.4f",
             kind, param_value, lyapunov_max);

    brain_kg_node_id_t ev = brain_kg_add_node(kg, ev_name,
        BRAIN_KG_NODE_UTILITY, desc);
    if (ev != BRAIN_KG_INVALID_NODE) {
        brain_kg_node_id_t owner = brain_kg_find_node(kg, DYNSYS_MODULE_NAME);
        if (owner != BRAIN_KG_INVALID_NODE) {
            brain_kg_add_edge(kg, owner, ev, BRAIN_KG_EDGE_SENDS_TO,
                "bifurcation_event",
                lyapunov_max > 0.0f ? lyapunov_max : 1.0f);
        }
        char buf[48];
        snprintf(buf, sizeof(buf), "%.6f", param_value);
        brain_kg_add_metadata(kg, ev, "param", buf);
        snprintf(buf, sizeof(buf), "%.6f", lyapunov_max);
        brain_kg_add_metadata(kg, ev, "lambda_max", buf);
        brain_kg_add_metadata(kg, ev, "kind", kind);
        snprintf(buf, sizeof(buf), "%llu", (unsigned long long)ts_us);
        brain_kg_add_metadata(kg, ev, "ts_us", buf);
    }

    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_READ, 0);
}

uint32_t dynsys_kg_count_events(dynsys_bridge_t bridge,
                                const char* kind_substr) {
    if (!bridge || !bridge->kg) return 0;
    brain_kg_node_id_t root = brain_kg_find_node(bridge->kg, DYNSYS_MODULE_NAME);
    if (root == BRAIN_KG_INVALID_NODE) return 0;
    brain_kg_edge_list_t* out = brain_kg_get_outgoing(bridge->kg, root);
    if (!out) return 0;

    uint32_t count = 0;
    for (uint32_t i = 0; i < out->count; ++i) {
        const brain_kg_edge_t* e = out->edges[i];
        if (!e) continue;
        const brain_kg_node_t* to = brain_kg_get_node(bridge->kg, e->to);
        if (!to) continue;
        if (!kind_substr || !*kind_substr) {
            count++;
        } else if ((to->name && strstr(to->name, kind_substr)) ||
                   (to->description && strstr(to->description, kind_substr))) {
            count++;
        }
    }
    brain_kg_edge_list_destroy(out);
    return count;
}

int dynsys_bridge_register_exception_handler(dynsys_bridge_t bridge, dynsys_exception_handler_t handler)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dynsys_bridge_register_exception_handler: bridge is NULL");
        return -1;
    }
    bridge->exception_handler = handler;
    return 0;
}

int dynsys_bridge_raise_exception(dynsys_bridge_t bridge, dynsys_exception_t exception,
                                   const char* message, void* context)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dynsys_bridge_raise_exception: bridge is NULL");
        return -1;
    }
    (void)exception;
    (void)context;

    if (bridge->config.enable_logging) {
        NIMCP_LOG_ERROR(LOG_TAG, "Exception: %s", message ? message : "unknown");
    }

    return 0;
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* dynsys_error_string(dynsys_error_t err)
{
    switch (err) {
        case DYNSYS_OK: return "Success";
        case DYNSYS_ERR_NULL_PTR: return "Null pointer";
        case DYNSYS_ERR_INVALID_DIM: return "Invalid dimension";
        case DYNSYS_ERR_DIVERGENCE: return "Numerical divergence";
        case DYNSYS_ERR_NOT_INITIALIZED: return "Not initialized";
        case DYNSYS_ERR_ALREADY_INITIALIZED: return "Already initialized";
        case DYNSYS_ERR_NO_MEMORY: return "Memory allocation failed";
        case DYNSYS_ERR_COMPUTATION: return "Computation error";
        case DYNSYS_ERR_INVALID_PARAMETER: return "Invalid parameter";
        default: return "Unknown error";
    }
}

dynsys_error_t dynsys_numerical_jacobian(dynsys_system_t sys, const float* state,
                                          float* jacobian, float epsilon)
{
    if (!sys || !state || !jacobian) return DYNSYS_ERR_NULL_PTR;

    uint32_t dim = sys->config.state_dim;

    float* f0 = nimcp_calloc(dim, sizeof(float));
    float* f1 = nimcp_calloc(dim, sizeof(float));
    float* perturbed = nimcp_calloc(dim, sizeof(float));

    if (!f0 || !f1 || !perturbed) {
        nimcp_free(f0); nimcp_free(f1); nimcp_free(perturbed);
        return DYNSYS_ERR_NO_MEMORY;
    }

    sys->func(state, dim, sys->params, sys->param_dim, f0, sys->context);

    for (uint32_t j = 0; j < dim; j++) {
        memcpy(perturbed, state, dim * sizeof(float));
        perturbed[j] += epsilon;

        sys->func(perturbed, dim, sys->params, sys->param_dim, f1, sys->context);

        for (uint32_t i = 0; i < dim; i++) {
            jacobian[i * dim + j] = (f1[i] - f0[i]) / epsilon;
        }
    }

    nimcp_free(f0); nimcp_free(f1); nimcp_free(perturbed);
    return DYNSYS_OK;
}

dynsys_error_t dynsys_eigenvalues(const float* matrix, uint32_t dim,
                                   float* eigenvalues_real, float* eigenvalues_imag)
{
    if (!matrix || !eigenvalues_real || !eigenvalues_imag) return DYNSYS_ERR_NULL_PTR;

    /* Simplified: return placeholder eigenvalues */
    for (uint32_t i = 0; i < dim; i++) {
        eigenvalues_real[i] = matrix[i * dim + i];  /* Diagonal approximation */
        eigenvalues_imag[i] = 0.0f;
    }

    return DYNSYS_OK;
}
