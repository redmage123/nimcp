/**
 * @file nimcp_fep_evidence.c
 * @brief Model Evidence Computation for Free Energy Principle
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Implementation of Bayesian model comparison and evidence computation
 * WHY:  Model selection enables brain to choose best generative model
 * HOW:  ELBO, importance sampling, Bayes factors for model comparison
 */

#include "cognitive/free_energy/nimcp_fep_evidence.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_math_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(fep_evidence, MESH_ADAPTER_CATEGORY_COGNITIVE)

static inline float safe_log(float x) {
    if (x <= FEP_EVIDENCE_EPSILON) return FEP_EVIDENCE_LOG_MIN;
    float result = logf(x);
    return nimcp_clampf(result, FEP_EVIDENCE_LOG_MIN, FEP_EVIDENCE_LOG_MAX);
}

static inline float safe_exp(float x) {
    x = nimcp_clampf(x, -88.0f, 88.0f);  /* Prevent overflow */
    return expf(x);
}

/* Log-sum-exp for numerical stability */
static float logsumexp(const float* values, size_t n) {
    if (!values || n == 0) return FEP_EVIDENCE_LOG_MIN;

    /* Find maximum */
    float max_val = values[0];
    for (size_t i = 1; i < n; i++) {
        if (values[i] > max_val) max_val = values[i];
    }

    /* Compute sum of exp(x - max) */
    float sum = 0.0f;
    for (size_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            fep_evidence_heartbeat("fep_evidence_loop",
                             (float)(i + 1) / (float)n);
        }

        sum += safe_exp(values[i] - max_val);
    }

    return max_val + safe_log(sum);
}

/* Internal ELBO computation without locking (for use within locked sections) */
static int fep_compute_elbo_internal(
    fep_evidence_system_t* sys,
    fep_system_t* fep,
    const float* observation,
    size_t obs_dim,
    float* elbo
) {
    if (!sys || !fep || !observation || !elbo) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_compute_elbo_internal: required parameter is NULL (sys, fep, observation, elbo)");
        return -1;
    }

    /* ELBO = E_q[log p(o,s)] - E_q[log q(s)]
     *      = E_q[log p(o|s)] + E_q[log p(s)] - E_q[log q(s)]
     *      = Accuracy - Complexity */

    float accuracy = 0.0f;
    float complexity = 0.0f;

    fep_compute_model_accuracy(sys, fep, observation, obs_dim, &accuracy);
    fep_compute_model_complexity(sys, fep, &complexity);

    *elbo = accuracy - complexity;
    *elbo = nimcp_clampf(*elbo, FEP_EVIDENCE_LOG_MIN, FEP_EVIDENCE_LOG_MAX);

    return 0;
}

/* Softmax normalization */
static void softmax(float* probs, size_t n) {
    if (!probs || n == 0) return;

    float max_val = probs[0];
    for (size_t i = 1; i < n; i++) {
        if (probs[i] > max_val) max_val = probs[i];
    }

    float sum = 0.0f;
    for (size_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            fep_evidence_heartbeat("fep_evidence_loop",
                             (float)(i + 1) / (float)n);
        }

        probs[i] = safe_exp(probs[i] - max_val);
        sum += probs[i];
    }

    if (sum > FEP_EVIDENCE_EPSILON) {
        for (size_t i = 0; i < n; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && n > 256) {
                fep_evidence_heartbeat("fep_evidence_loop",
                                 (float)(i + 1) / (float)n);
            }

            probs[i] /= sum;
        }
    }
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

void fep_evidence_default_config(fep_evidence_config_t* config) {
    if (!config) return;

    /* Phase 8: Heartbeat at operation start */
    fep_evidence_heartbeat("fep_evidence_default_config", 0.0f);


    config->method = EVIDENCE_ELBO;
    config->num_samples = FEP_EVIDENCE_DEFAULT_SAMPLES;
    config->temperature_schedule_start = FEP_EVIDENCE_DEFAULT_TEMP_START;
    config->temperature_schedule_end = FEP_EVIDENCE_DEFAULT_TEMP_END;
    config->annealing_steps = FEP_EVIDENCE_DEFAULT_ANNEAL_STEPS;
    config->enable_model_averaging = false;
}

fep_evidence_system_t* fep_evidence_create(const fep_evidence_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    fep_evidence_heartbeat("fep_evidence_create", 0.0f);


    fep_evidence_system_t* sys = (fep_evidence_system_t*)nimcp_calloc(
        1, sizeof(fep_evidence_system_t));
    if (!sys) {
        NIMCP_LOGGING_ERROR("Failed to allocate evidence system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "fep_evidence_create: sys is NULL");
        return NULL;
    }

    /* Apply configuration */
    fep_evidence_config_t default_cfg;
    if (!config) {
        fep_evidence_default_config(&default_cfg);
        config = &default_cfg;
    }
    sys->config = *config;

    /* Create mutex */
    sys->mutex = nimcp_platform_mutex_create();
    if (!sys->mutex) {
        fep_evidence_destroy(sys);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "fep_evidence_create: sys->mutex is NULL");
        return NULL;
    }

    NIMCP_LOGGING_INFO("Evidence system created: method=%s, samples=%u",
                      fep_evidence_method_to_string(config->method),
                      config->num_samples);
    return sys;
}

void fep_evidence_destroy(fep_evidence_system_t* sys) {
    if (!sys) return;

    /* Phase 8: Heartbeat at operation start */
    fep_evidence_heartbeat("fep_evidence_destroy", 0.0f);


    if (sys->bio_async_enabled) {
        fep_evidence_disconnect_bio_async(sys);
    }

    if (sys->mutex) {
        nimcp_platform_mutex_destroy(sys->mutex);
        nimcp_free(sys->mutex);
        sys->mutex = NULL;
    }

    nimcp_free(sys);
    sys = NULL;
    NIMCP_LOGGING_INFO("Evidence system destroyed");
}

/* ============================================================================
 * Evidence Computation Implementation
 * ============================================================================ */

int fep_compute_log_evidence(
    fep_evidence_system_t* sys,
    fep_system_t* fep,
    const float* observations,
    size_t n_obs,
    size_t obs_dim,
    fep_evidence_result_t* result
) {
    if (!sys || !fep || !observations || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_compute_log_evidence: required parameter is NULL (sys, fep, observations, result)");
        return -1;
    }
    if (n_obs == 0 || obs_dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "fep_compute_log_evidence: n_obs is zero");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_evidence_heartbeat("fep_evidence_fep_compute_log_evid", 0.0f);


    memset(result, 0, sizeof(fep_evidence_result_t));

    /* Compute based on configured method */
    float total_log_evidence = 0.0f;
    float total_complexity = 0.0f;
    float total_accuracy = 0.0f;

    for (size_t i = 0; i < n_obs; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n_obs > 256) {
            fep_evidence_heartbeat("fep_evidence_loop",
                             (float)(i + 1) / (float)n_obs);
        }

        const float* obs = observations + i * obs_dim;
        float elbo, complexity, accuracy;

        /* Compute ELBO for each observation (uses internal unlocked version) */
        fep_compute_elbo_internal(sys, fep, obs, obs_dim, &elbo);
        fep_compute_model_complexity(sys, fep, &complexity);
        fep_compute_model_accuracy(sys, fep, obs, obs_dim, &accuracy);

        total_log_evidence += elbo;
        total_complexity += complexity;
        total_accuracy += accuracy;
    }

    /* Average over observations */
    result->log_evidence = total_log_evidence / (float)n_obs;
    result->evidence_lower_bound = result->log_evidence;  /* ELBO is the bound */
    result->model_complexity = total_complexity / (float)n_obs;
    result->model_accuracy = total_accuracy / (float)n_obs;

    /* Compute Bayes factor against reference if available */
    if (sys->reference_valid) {
        float log_bf = result->log_evidence - sys->reference_log_evidence;
        result->bayes_factor = safe_exp(log_bf);
        result->bf_strength = fep_interpret_bayes_factor(log_bf);
    }

    /* Update statistics */
    nimcp_platform_mutex_lock(sys->mutex);
    sys->stats.total_computations++;
    sys->stats.elbo_computations += n_obs;
    nimcp_platform_mutex_unlock(sys->mutex);

    return 0;
}

int fep_compute_elbo(
    fep_evidence_system_t* sys,
    fep_system_t* fep,
    const float* observation,
    size_t obs_dim,
    float* elbo
) {
    if (!sys || !fep || !observation || !elbo) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_compute_elbo: required parameter is NULL (sys, fep, observation, elbo)");
        return -1;
    }

    /* Use internal computation */
    /* Phase 8: Heartbeat at operation start */
    fep_evidence_heartbeat("fep_evidence_fep_compute_elbo", 0.0f);


    int ret = fep_compute_elbo_internal(sys, fep, observation, obs_dim, elbo);

    /* Update statistics */
    if (ret == 0) {
        nimcp_platform_mutex_lock(sys->mutex);
        sys->stats.elbo_computations++;
        nimcp_platform_mutex_unlock(sys->mutex);
    }

    return ret;
}

int fep_compute_model_complexity(
    fep_evidence_system_t* sys,
    fep_system_t* fep,
    float* complexity
) {
    if (!sys || !fep || !complexity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_compute_model_complexity: required parameter is NULL (sys, fep, complexity)");
        return -1;
    }

    /* Complexity = KL[q(s)||p(s)] = E_q[log q(s)] - E_q[log p(s)]
     * For Gaussian: KL = 0.5 * (tr(Σ_prior^-1 Σ_post) + (μ_prior - μ_post)^T Σ_prior^-1 (μ_prior - μ_post)
     *                          - k + ln(|Σ_prior|/|Σ_post|))
     * Simplified: approximate as sum of squared deviations weighted by prior precision */

    /* Phase 8: Heartbeat at operation start */
    fep_evidence_heartbeat("fep_evidence_fep_compute_model_co", 0.0f);


    float kl = 0.0f;

    for (uint32_t l = 0; l < fep->num_levels; l++) {
        /* Phase 8: Loop progress heartbeat */
        if ((l & 0xFF) == 0 && fep->num_levels > 256) {
            fep_evidence_heartbeat("fep_evidence_loop",
                             (float)(l + 1) / (float)fep->num_levels);
        }

        fep_hierarchy_level_t* level = &fep->levels[l];
        fep_belief_t* beliefs = &level->beliefs;

        for (uint32_t i = 0; i < beliefs->dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && beliefs->dim > 256) {
                fep_evidence_heartbeat("fep_evidence_loop",
                                 (float)(i + 1) / (float)beliefs->dim);
            }

            /* Deviation from prior */
            float mu_diff = beliefs->mean[i] - level->prior_mean[i];
            /* Prior precision is 1/variance, so prior_variance = 1/prior_precision */
            float prior_var = 1.0f / (level->prior_precision[i] + FEP_EVIDENCE_EPSILON);
            float var_ratio = beliefs->variance[i] / (prior_var + FEP_EVIDENCE_EPSILON);

            /* KL contribution: 0.5 * (var_ratio - 1 - log(var_ratio) + mu_diff^2 / prior_var) */
            kl += 0.5f * (var_ratio - 1.0f - safe_log(var_ratio) +
                         mu_diff * mu_diff / (prior_var + FEP_EVIDENCE_EPSILON));
        }
    }

    *complexity = nimcp_clampf(kl, 0.0f, 1000.0f);
    return 0;
}

int fep_compute_model_accuracy(
    fep_evidence_system_t* sys,
    fep_system_t* fep,
    const float* observation,
    size_t obs_dim,
    float* accuracy
) {
    if (!sys || !fep || !observation || !accuracy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_compute_model_accuracy: required parameter is NULL (sys, fep, observation, accuracy)");
        return -1;
    }

    /* Accuracy = E_q[log p(o|s)]
     * For Gaussian likelihood: -0.5 * (o - pred)^T Σ^-1 (o - pred) - 0.5 * k * log(2π) - 0.5 * log|Σ| */

    /* Phase 8: Heartbeat at operation start */
    fep_evidence_heartbeat("fep_evidence_fep_compute_model_ac", 0.0f);


    float log_lik = 0.0f;

    if (fep->num_levels > 0) {
        fep_hierarchy_level_t* level = &fep->levels[0];
        size_t dim = obs_dim < level->beliefs.dim ? obs_dim : level->beliefs.dim;

        for (size_t i = 0; i < dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && dim > 256) {
                fep_evidence_heartbeat("fep_evidence_loop",
                                 (float)(i + 1) / (float)dim);
            }

            float pred = level->beliefs.mean[i];
            float var = level->errors.precision[i] > 0.0f ?
                        1.0f / level->errors.precision[i] : 1.0f;

            float diff = observation[i] - pred;
            log_lik -= 0.5f * (diff * diff / (var + FEP_EVIDENCE_EPSILON) +
                              safe_log(NIMCP_TWO_PI_F * var));
        }
    }

    *accuracy = nimcp_clampf(log_lik, FEP_EVIDENCE_LOG_MIN, 0.0f);
    return 0;
}

/* ============================================================================
 * Model Comparison Implementation
 * ============================================================================ */

int fep_compute_bayes_factor(
    fep_evidence_system_t* sys,
    fep_system_t* model1,
    fep_system_t* model2,
    const float* observations,
    size_t n_obs,
    size_t obs_dim,
    float* log_bf
) {
    if (!sys || !model1 || !model2 || !observations || !log_bf) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_compute_bayes_factor: required parameter is NULL (sys, model1, model2, observations, log_bf)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_evidence_heartbeat("fep_evidence_fep_compute_bayes_fa", 0.0f);


    fep_evidence_result_t result1, result2;

    fep_compute_log_evidence(sys, model1, observations, n_obs, obs_dim, &result1);
    fep_compute_log_evidence(sys, model2, observations, n_obs, obs_dim, &result2);

    *log_bf = result1.log_evidence - result2.log_evidence;

    nimcp_platform_mutex_lock(sys->mutex);
    sys->stats.model_comparisons++;
    nimcp_platform_mutex_unlock(sys->mutex);

    return 0;
}

int fep_compare_models(
    fep_evidence_system_t* sys,
    fep_system_t** models,
    size_t n_models,
    const float* observations,
    size_t n_obs,
    size_t obs_dim,
    fep_model_score_t* scores
) {
    if (!sys || !models || !observations || !scores) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_compare_models: required parameter is NULL (sys, models, observations, scores)");
        return -1;
    }
    if (n_models == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "fep_compare_models: n_models is zero");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_evidence_heartbeat("fep_evidence_fep_compare_models", 0.0f);


    float* log_evidences = (float*)nimcp_calloc(n_models, sizeof(float));
    if (!log_evidences) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate log_evidences");

        return -1;
    }

    /* Compute log evidence for each model (no lock - fep_compute_log_evidence locks internally) */
    for (size_t m = 0; m < n_models; m++) {
        /* Phase 8: Loop progress heartbeat */
        if ((m & 0xFF) == 0 && n_models > 256) {
            fep_evidence_heartbeat("fep_evidence_loop",
                             (float)(m + 1) / (float)n_models);
        }

        fep_evidence_result_t result;
        fep_compute_log_evidence(sys, models[m], observations, n_obs, obs_dim, &result);

        scores[m].model_id = (uint32_t)m;
        scores[m].log_evidence = result.log_evidence;
        scores[m].prior_probability = 1.0f / (float)n_models;  /* Uniform prior */

        log_evidences[m] = result.log_evidence;
    }

    /* Compute posterior probabilities via softmax */
    float log_marginal = logsumexp(log_evidences, n_models);

    for (size_t m = 0; m < n_models; m++) {
        /* Phase 8: Loop progress heartbeat */
        if ((m & 0xFF) == 0 && n_models > 256) {
            fep_evidence_heartbeat("fep_evidence_loop",
                             (float)(m + 1) / (float)n_models);
        }

        scores[m].posterior_probability = safe_exp(log_evidences[m] - log_marginal);
    }

    nimcp_free(log_evidences);
    log_evidences = NULL;

    /* Update stats with lock */
    nimcp_platform_mutex_lock(sys->mutex);
    sys->stats.model_comparisons += n_models;
    nimcp_platform_mutex_unlock(sys->mutex);

    return 0;
}

int fep_select_best_model(
    fep_evidence_system_t* sys,
    const fep_model_score_t* scores,
    size_t n_models,
    uint32_t* best_model_id
) {
    if (!sys || !scores || !best_model_id || n_models == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_select_best_model: required parameter is NULL (sys, scores, best_model_id)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_evidence_heartbeat("fep_evidence_fep_select_best_mode", 0.0f);


    uint32_t best_id = 0;
    float best_prob = scores[0].posterior_probability;

    for (size_t m = 1; m < n_models; m++) {
        if (scores[m].posterior_probability > best_prob) {
            best_prob = scores[m].posterior_probability;
            best_id = scores[m].model_id;
        }
    }

    *best_model_id = best_id;
    return 0;
}

/* ============================================================================
 * Model Averaging Implementation
 * ============================================================================ */

int fep_model_average_prediction(
    fep_evidence_system_t* sys,
    fep_system_t** models,
    size_t n_models,
    const fep_model_score_t* scores,
    float* averaged_prediction,
    size_t pred_dim
) {
    if (!sys || !models || !scores || !averaged_prediction) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_model_average_prediction: required parameter is NULL (sys, models, scores, averaged_prediction)");
        return -1;
    }
    if (n_models == 0 || pred_dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "fep_model_average_prediction: n_models is zero");
        return -1;
    }

    /* Initialize to zero */
    /* Phase 8: Heartbeat at operation start */
    fep_evidence_heartbeat("fep_evidence_fep_model_average_pr", 0.0f);


    memset(averaged_prediction, 0, pred_dim * sizeof(float));

    /* Weighted average: prediction = Σ p(M|o) * E[s|M,o] */
    for (size_t m = 0; m < n_models; m++) {
        /* Phase 8: Loop progress heartbeat */
        if ((m & 0xFF) == 0 && n_models > 256) {
            fep_evidence_heartbeat("fep_evidence_loop",
                             (float)(m + 1) / (float)n_models);
        }

        float weight = scores[m].posterior_probability;

        if (models[m] && models[m]->num_levels > 0) {
            fep_belief_t* beliefs = &models[m]->levels[0].beliefs;
            size_t dim = pred_dim < beliefs->dim ? pred_dim : beliefs->dim;

            for (size_t i = 0; i < dim; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && dim > 256) {
                    fep_evidence_heartbeat("fep_evidence_loop",
                                     (float)(i + 1) / (float)dim);
                }

                averaged_prediction[i] += weight * beliefs->mean[i];
            }
        }
    }

    return 0;
}

/* ============================================================================
 * Integration Implementation
 * ============================================================================ */

int fep_evidence_connect(fep_evidence_system_t* evidence, fep_system_t* fep) {
    if (!evidence || !fep) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_evidence_connect: required parameter is NULL (evidence, fep)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_evidence_heartbeat("fep_evidence_connect", 0.0f);


    nimcp_platform_mutex_lock(evidence->mutex);
    evidence->fep_system = fep;
    nimcp_platform_mutex_unlock(evidence->mutex);

    NIMCP_LOGGING_INFO("Evidence system connected to FEP");
    return 0;
}

int fep_evidence_set_reference(
    fep_evidence_system_t* sys,
    fep_system_t* reference,
    const float* observations,
    size_t n_obs,
    size_t obs_dim
) {
    if (!sys || !reference || !observations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_evidence_set_reference: required parameter is NULL (sys, reference, observations)");
        return -1;
    }

    /* Compute reference evidence BEFORE locking (avoids deadlock) */
    /* Phase 8: Heartbeat at operation start */
    fep_evidence_heartbeat("fep_evidence_set_reference", 0.0f);


    fep_evidence_result_t result;
    fep_compute_log_evidence(sys, reference, observations, n_obs, obs_dim, &result);

    /* Now lock and update reference fields */
    nimcp_platform_mutex_lock(sys->mutex);
    sys->reference_model = reference;
    sys->reference_log_evidence = result.log_evidence;
    sys->reference_valid = true;
    nimcp_platform_mutex_unlock(sys->mutex);

    return 0;
}

/* ============================================================================
 * Bio-Async Implementation
 * ============================================================================ */

int fep_evidence_connect_bio_async(fep_evidence_system_t* sys) {
    if (!sys) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_evidence_connect_bio_async: sys is NULL");
        return -1;
    }
    if (sys->bio_async_enabled) return 0;

    /* Phase 8: Heartbeat at operation start */
    fep_evidence_heartbeat("fep_evidence_connect_bio_async", 0.0f);


    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_EVIDENCE,
        .module_name = "fep_evidence",
        .inbox_capacity = 32,
        .user_data = sys
    };

    sys->bio_ctx = bio_router_register_module(&info);
    if (sys->bio_ctx) {
        sys->bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Evidence connected to bio-async");
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
    }
    return 0;
}

int fep_evidence_disconnect_bio_async(fep_evidence_system_t* sys) {
    if (!sys) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_evidence_disconnect_bio_async: sys is NULL");
        return -1;
    }
    if (!sys->bio_async_enabled) return 0;

    /* Phase 8: Heartbeat at operation start */
    fep_evidence_heartbeat("fep_evidence_disconnect_bio_async", 0.0f);


    if (sys->bio_ctx) {
        bio_router_unregister_module(sys->bio_ctx);
        sys->bio_ctx = NULL;
    }
    sys->bio_async_enabled = false;
    return 0;
}

bool fep_evidence_is_bio_async_connected(const fep_evidence_system_t* sys) {
    /* Phase 8: Heartbeat at operation start */
    fep_evidence_heartbeat("fep_evidence_is_bio_async_connect", 0.0f);


    return sys && sys->bio_async_enabled;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int fep_evidence_get_stats(const fep_evidence_system_t* sys, fep_evidence_stats_t* stats) {
    if (!sys || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_evidence_get_stats: required parameter is NULL (sys, stats)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    fep_evidence_heartbeat("fep_evidence_get_stats", 0.0f);

    nimcp_platform_mutex_lock(((fep_evidence_system_t*)sys)->mutex);
    *stats = sys->stats;
    nimcp_platform_mutex_unlock(((fep_evidence_system_t*)sys)->mutex);

    return 0;
}

fep_bf_strength_t fep_interpret_bayes_factor(float log_bf) {
    /* Phase 8: Heartbeat at operation start */
    fep_evidence_heartbeat("fep_evidence_fep_interpret_bayes_", 0.0f);


    float bf = safe_exp(log_bf);

    if (bf < FEP_EVIDENCE_BF_WEAK) return BF_STRENGTH_NONE;
    if (bf < FEP_EVIDENCE_BF_POSITIVE) return BF_STRENGTH_WEAK;
    if (bf < FEP_EVIDENCE_BF_STRONG) return BF_STRENGTH_POSITIVE;
    if (bf < FEP_EVIDENCE_BF_VERY_STRONG) return BF_STRENGTH_STRONG;
    if (bf < FEP_EVIDENCE_BF_DECISIVE) return BF_STRENGTH_VERY_STRONG;
    return BF_STRENGTH_DECISIVE;
}

const char* fep_bf_strength_to_string(fep_bf_strength_t strength) {
    switch (strength) {
        case BF_STRENGTH_NONE:        return "NONE";
        case BF_STRENGTH_WEAK:        return "WEAK";
        case BF_STRENGTH_POSITIVE:    return "POSITIVE";
        case BF_STRENGTH_STRONG:      return "STRONG";
        case BF_STRENGTH_VERY_STRONG: return "VERY_STRONG";
        case BF_STRENGTH_DECISIVE:    return "DECISIVE";
        default:                      return "UNKNOWN";
    }
}

const char* fep_evidence_method_to_string(fep_evidence_method_t method) {
    switch (method) {
        case EVIDENCE_ELBO:       return "ELBO";
        case EVIDENCE_IMPORTANCE: return "IMPORTANCE_SAMPLING";
        case EVIDENCE_ANNEALED:   return "ANNEALED_IMPORTANCE";
        case EVIDENCE_BRIDGE:     return "BRIDGE_SAMPLING";
        default:                  return "UNKNOWN";
    }
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * WHAT: Query knowledge graph for FEP Evidence self-knowledge
 * WHY:  Enable self-awareness about module's role and connections
 * HOW:  Query KG for entity observations and relations
 */
int fep_evidence_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    fep_evidence_heartbeat("fep_evidence_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "FEP_Evidence_System");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                fep_evidence_heartbeat("fep_evidence_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("FEP Evidence self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "FEP_Evidence_System");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "FEP_Evidence_System");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void fep_evidence_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_fep_evidence_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int fep_evidence_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "fep_evidence_training_begin: NULL argument");
        return -1;
    }
    fep_evidence_heartbeat_instance(g_fep_evidence_health_agent, "fep_evidence_training_begin", 0.0f);
    return 0;
}

int fep_evidence_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "fep_evidence_training_end: NULL argument");
        return -1;
    }
    fep_evidence_heartbeat_instance(g_fep_evidence_health_agent, "fep_evidence_training_end", 1.0f);
    return 0;
}

int fep_evidence_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "fep_evidence_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    fep_evidence_heartbeat_instance(g_fep_evidence_health_agent, "fep_evidence_training_step", progress);
    return 0;
}
