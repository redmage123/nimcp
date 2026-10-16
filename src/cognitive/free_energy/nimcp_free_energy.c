/**
 * @file nimcp_free_energy.c
 * @brief Free Energy Principle Implementation
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Implementation of variational free energy minimization
 * WHY:  Unifying framework for perception, action, and learning
 * HOW:  Hierarchical predictive coding with precision-weighted errors
 */

#include "cognitive/free_energy/nimcp_free_energy.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "cognitive/world_model/nimcp_world_model_kg_events.h"  /* W8 */
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/thread/nimcp_thread_rand.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(free_energy_instance, MESH_ADAPTER_CATEGORY_COGNITIVE)

/* Alias: tests reference free_energy_set_health_agent (without _instance suffix) */
static nimcp_health_agent_t* g_free_energy_health_agent = NULL;
void free_energy_set_health_agent(struct nimcp_health_agent* agent) { g_free_energy_health_agent = agent; }

/**
 * @brief Compute L2 norm of vector
 */
static float vector_norm(const float* vec, uint32_t dim) {
    if (!vec || dim == 0) return 0.0f;

    float sum = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dim > 256) {
            free_energy_instance_heartbeat("free_energy_loop",
                             (float)(i + 1) / (float)dim);
        }

        sum += vec[i] * vec[i];
    }
    return sqrtf(sum);
}

/**
 * @brief Compute dot product
 */
static float vector_dot(const float* a, const float* b, uint32_t dim) {
    if (!a || !b || dim == 0) return 0.0f;

    float sum = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dim > 256) {
            free_energy_instance_heartbeat("free_energy_loop",
                             (float)(i + 1) / (float)dim);
        }

        sum += a[i] * b[i];
    }
    return sum;
}

/**
 * @brief Safe log (avoid log(0))
 */
static inline float safe_log(float x) {
    if (x <= 0.0f) return -100.0f;
    return logf(x);
}

/**
 * @brief Softmax over array (in-place)
 *
 * NUMERICAL STABILITY NOTES:
 * - Temperature guard at 1e-6f prevents division by zero/near-zero.
 *   This value chosen because:
 *   1. Lower bound where softmax still differentiates inputs meaningfully
 *   2. Prevents expf() overflow for typical input ranges (-100 to 100)
 *   3. Results in max exponent arg ~1e8 which is well within float range
 * - Max subtraction prevents overflow: exp(x-max) <= 1.0 always
 * - Exponent clamping at +/-88 prevents: expf(89) -> Inf, expf(-104) -> 0
 */
#define SOFTMAX_MIN_TEMPERATURE 1e-6f
#define SOFTMAX_MAX_EXPONENT 88.0f  /* expf(88.72) overflows to Inf */

static void softmax(float* values, uint32_t n, float temperature) {
    if (!values || n == 0) return;

    /* Guard: Prevent division by zero.
     * WHY 1e-6f: At T=1e-6, softmax becomes nearly argmax (one-hot).
     * Values below this cause numerical instability without meaningful
     * improvement in "hardness" of the distribution. */
    if (temperature < SOFTMAX_MIN_TEMPERATURE) {
        temperature = SOFTMAX_MIN_TEMPERATURE;
    }

    /* Find max for numerical stability (log-sum-exp trick) */
    float max_val = values[0];
    for (uint32_t i = 1; i < n; i++) {
        if (values[i] > max_val) max_val = values[i];
    }

    /* Compute exp and sum with exponent clamping */
    float sum = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            free_energy_instance_heartbeat("free_energy_loop",
                             (float)(i + 1) / (float)n);
        }

        float exponent = (values[i] - max_val) / temperature;

        /* NUMERICAL STABILITY: Clamp exponent to prevent overflow/underflow.
         * After max subtraction, exponent should be <= 0, but floating point
         * edge cases (NaN in input, etc.) could violate this. */
        exponent = fmaxf(-SOFTMAX_MAX_EXPONENT, fminf(SOFTMAX_MAX_EXPONENT, exponent));

        /* Check for NaN propagation from corrupted inputs */
        if (isnan(exponent)) {
            exponent = -SOFTMAX_MAX_EXPONENT;  /* Treat NaN as very unlikely */
        }

        values[i] = expf(exponent);
        sum += values[i];
    }

    /* Normalize with sum protection */
    if (sum > 0.0f && !isnan(sum) && !isinf(sum)) {
        float inv_sum = 1.0f / sum;
        for (uint32_t i = 0; i < n; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && n > 256) {
                free_energy_instance_heartbeat("free_energy_loop",
                                 (float)(i + 1) / (float)n);
            }

            values[i] *= inv_sum;
        }
    } else {
        /* Fallback: uniform distribution if sum is invalid */
        float uniform = 1.0f / (float)n;
        for (uint32_t i = 0; i < n; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && n > 256) {
                free_energy_instance_heartbeat("free_energy_loop",
                                 (float)(i + 1) / (float)n);
            }

            values[i] = uniform;
        }
    }
}

/**
 * @brief Initialize belief structure
 */
static int init_belief(fep_belief_t* belief, uint32_t dim, float initial_precision) {
    if (!belief || dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_belief: belief is NULL");
        return -1;
    }

    /* Guard: Ensure precision is positive to prevent division by zero */
    if (initial_precision < FEP_MIN_PRECISION) {
        initial_precision = FEP_MIN_PRECISION;
    }

    belief->dim = dim;
    belief->mean = (float*)nimcp_calloc(dim, sizeof(float));
    belief->variance = (float*)nimcp_calloc(dim, sizeof(float));
    if (!belief->variance) return -1;
    belief->precision = (float*)nimcp_calloc(dim, sizeof(float));

    if (!belief->mean || !belief->variance || !belief->precision) {
        /* Clean up partially allocated memory on failure */
        if (belief->mean) nimcp_free(belief->mean);
        if (belief->variance) nimcp_free(belief->variance);
        if (belief->precision) nimcp_free(belief->precision);
        belief->mean = NULL;
        belief->variance = NULL;
        belief->precision = NULL;
        belief->dim = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_belief: validation failed");
        return -1;
    }

    /* Initialize with prior */
    for (uint32_t i = 0; i < dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dim > 256) {
            free_energy_instance_heartbeat("free_energy_loop",
                             (float)(i + 1) / (float)dim);
        }

        belief->mean[i] = 0.0f;
        belief->variance[i] = 1.0f / initial_precision;
        belief->precision[i] = initial_precision;
    }

    return 0;
}

/**
 * @brief Free belief structure
 */
static void free_belief(fep_belief_t* belief) {
    if (!belief) return;
    if (belief->mean) nimcp_free(belief->mean);
    if (belief->variance) nimcp_free(belief->variance);
    if (belief->precision) nimcp_free(belief->precision);
    belief->mean = NULL;
    belief->variance = NULL;
    belief->precision = NULL;
    belief->dim = 0;
}

/**
 * @brief Initialize prediction error structure
 */
static int init_prediction_error(fep_prediction_error_t* error, uint32_t dim) {
    if (!error || dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_prediction_error: error is NULL");
        return -1;
    }

    error->dim = dim;
    error->error = (float*)nimcp_calloc(dim, sizeof(float));
    error->weighted_error = (float*)nimcp_calloc(dim, sizeof(float));
    if (!error->weighted_error) return -1;
    error->precision = (float*)nimcp_calloc(dim, sizeof(float));

    if (!error->error || !error->weighted_error || !error->precision) {
        /* Clean up partially allocated memory on failure */
        if (error->error) nimcp_free(error->error);
        if (error->weighted_error) nimcp_free(error->weighted_error);
        if (error->precision) nimcp_free(error->precision);
        error->error = NULL;
        error->weighted_error = NULL;
        error->precision = NULL;
        error->dim = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_prediction_error: validation failed");
        return -1;
    }

    for (uint32_t i = 0; i < dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dim > 256) {
            free_energy_instance_heartbeat("free_energy_loop",
                             (float)(i + 1) / (float)dim);
        }

        error->precision[i] = FEP_DEFAULT_PRECISION;
    }

    return 0;
}

/**
 * @brief Free prediction error structure
 */
static void free_prediction_error(fep_prediction_error_t* error) {
    if (!error) return;
    if (error->error) nimcp_free(error->error);
    if (error->weighted_error) nimcp_free(error->weighted_error);
    if (error->precision) nimcp_free(error->precision);
    error->error = NULL;
    error->weighted_error = NULL;
    error->precision = NULL;
    error->dim = 0;
}

/**
 * @brief Initialize hierarchy level
 */
static int init_level(
    fep_hierarchy_level_t* level,
    uint32_t level_id,
    uint32_t state_dim,
    uint32_t prediction_dim,
    float initial_precision
) {
    if (!level) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "level is NULL");

        return -1;

    }

    memset(level, 0, sizeof(fep_hierarchy_level_t));
    level->level_id = level_id;

    /* Initialize beliefs */
    if (init_belief(&level->beliefs, state_dim, initial_precision) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "init_level: validation failed");
        return -1;
    }

    /* Initialize predictions */
    level->prediction_dim = prediction_dim;
    if (prediction_dim > 0) {
        level->predictions = (float*)nimcp_calloc(prediction_dim, sizeof(float));
        if (!level->predictions) {
            /* Clean up beliefs before returning */
            free_belief(&level->beliefs);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_level: level->predictions is NULL");
            return -1;
        }
    }

    /* Initialize prediction errors */
    if (init_prediction_error(&level->errors, prediction_dim > 0 ? prediction_dim : state_dim) != 0) {
        /* Clean up beliefs and predictions before returning */
        free_belief(&level->beliefs);
        if (level->predictions) nimcp_free(level->predictions);
        level->predictions = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_level: validation failed");
        return -1;
    }

    /* Prior initialization */
    level->prior_mean = (float*)nimcp_calloc(state_dim, sizeof(float));
    level->prior_precision = (float*)nimcp_calloc(state_dim, sizeof(float));
    if (!level->prior_mean || !level->prior_precision) {
        /* Clean up all previously allocated resources */
        free_belief(&level->beliefs);
        free_prediction_error(&level->errors);
        if (level->predictions) nimcp_free(level->predictions);
        if (level->prior_mean) nimcp_free(level->prior_mean);
        if (level->prior_precision) nimcp_free(level->prior_precision);
        level->predictions = NULL;
        level->prior_mean = NULL;
        level->prior_precision = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_level: validation failed");
        return -1;
    }

    for (uint32_t i = 0; i < state_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && state_dim > 256) {
            free_energy_instance_heartbeat("free_energy_loop",
                             (float)(i + 1) / (float)state_dim);
        }

        level->prior_precision[i] = initial_precision;
    }

    return 0;
}

/**
 * @brief Free hierarchy level
 */
static void free_level(fep_hierarchy_level_t* level) {
    if (!level) return;

    free_belief(&level->beliefs);
    free_prediction_error(&level->errors);

    if (level->predictions) nimcp_free(level->predictions);
    if (level->transition_matrix) nimcp_free(level->transition_matrix);
    if (level->likelihood_matrix) nimcp_free(level->likelihood_matrix);
    if (level->prior_mean) nimcp_free(level->prior_mean);
    if (level->prior_precision) nimcp_free(level->prior_precision);
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int fep_default_config(fep_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    free_energy_instance_heartbeat("free_energy_fep_default_config", 0.0f);


    config->num_levels = 2;  /* Default 2-level hierarchy */
    config->level_dims = NULL;  /* Will use defaults */

    config->belief_learning_rate = FEP_DEFAULT_BELIEF_LR;
    config->precision_learning_rate = FEP_DEFAULT_PRECISION_LR;
    config->action_learning_rate = FEP_DEFAULT_ACTION_LR;

    config->update_mode = FEP_UPDATE_PREDICTIVE_CODING;
    config->action_mode = FEP_ACTION_SOFTMAX;

    config->initial_precision = FEP_DEFAULT_PRECISION;
    config->learn_precision = true;

    config->enable_active_inference = true;
    config->planning_horizon = 3;
    config->action_temperature = 1.0f;

    config->max_iterations = 10;
    config->convergence_threshold = FEP_CONVERGENCE_THRESHOLD;

    return 0;
}

fep_system_t* fep_create(
    const fep_config_t* config,
    uint32_t observation_dim,
    uint32_t action_dim
) {
    /* Phase 8: Heartbeat at operation start */
    free_energy_instance_heartbeat("free_energy_fep_create", 0.0f);


    if (observation_dim == 0) {
        NIMCP_LOGGING_ERROR("Observation dimension must be > 0");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "fep_create: observation_dim is zero");
        return NULL;
    }

    fep_system_t* fep = (fep_system_t*)nimcp_calloc(1, sizeof(fep_system_t));
    if (!fep) {
        NIMCP_LOGGING_ERROR("FEP allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate fep");

        return NULL;
    }

    /* Apply configuration */
    fep_config_t default_cfg;
    if (!config) {
        fep_default_config(&default_cfg);
        config = &default_cfg;
    }
    fep->config = *config;

    /* Ensure at least 1 level */
    if (fep->config.num_levels == 0) {
        fep->config.num_levels = 1;
    }
    if (fep->config.num_levels > FEP_MAX_HIERARCHY_LEVELS) {
        fep->config.num_levels = FEP_MAX_HIERARCHY_LEVELS;
    }

    /* Store dimensions */
    fep->observation_dim = observation_dim;
    fep->action_dim = action_dim;
    fep->num_actions = action_dim > 0 ? action_dim : 1;

    /* Allocate observations */
    fep->observations = (float*)nimcp_calloc(observation_dim, sizeof(float));
    if (!fep->observations) {
        fep_destroy(fep);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "fep_create: fep->observations is NULL");
        return NULL;
    }

    /* Allocate hierarchy levels */
    fep->num_levels = fep->config.num_levels;
    fep->levels = (fep_hierarchy_level_t*)nimcp_calloc(
        fep->num_levels, sizeof(fep_hierarchy_level_t)
    );
    if (!fep->levels) {
        fep_destroy(fep);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "fep_create: fep->levels is NULL");
        return NULL;
    }

    /* Initialize each level */
    for (uint32_t i = 0; i < fep->num_levels; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && fep->num_levels > 256) {
            free_energy_instance_heartbeat("free_energy_loop",
                             (float)(i + 1) / (float)fep->num_levels);
        }

        uint32_t state_dim = 0;
        uint32_t pred_dim = 0;

        if (config->level_dims && config->level_dims[i] > 0) {
            state_dim = config->level_dims[i];
        } else {
            /* Default: each level halves dimension */
            state_dim = observation_dim >> i;
            if (state_dim < 4) state_dim = 4;
        }

        /* Prediction dimension is state dim of level below (or obs dim for level 0) */
        if (i == 0) {
            pred_dim = observation_dim;
        } else {
            pred_dim = fep->levels[i-1].beliefs.dim;
        }

        if (init_level(&fep->levels[i], i, state_dim, pred_dim,
                       fep->config.initial_precision) != 0) {
            NIMCP_LOGGING_ERROR("Failed to initialize level %u", i);
            fep_destroy(fep);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_create: operation failed");
            return NULL;
        }
    }

    /* Allocate policies if active inference enabled */
    if (fep->config.enable_active_inference && action_dim > 0) {
        fep->policies = (fep_policy_t*)nimcp_calloc(
            FEP_MAX_POLICIES, sizeof(fep_policy_t)
        );
        if (!fep->policies) {
            fep_destroy(fep);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "fep_create: fep->policies is NULL");
            return NULL;
        }

        /* Initialize simple one-step policies */
        fep->num_policies = action_dim < FEP_MAX_POLICIES ? action_dim : FEP_MAX_POLICIES;
        for (uint32_t i = 0; i < fep->num_policies; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && fep->num_policies > 256) {
                free_energy_instance_heartbeat("free_energy_loop",
                                 (float)(i + 1) / (float)fep->num_policies);
            }

            fep->policies[i].policy_id = i;
            fep->policies[i].action_dim = action_dim;
            fep->policies[i].num_actions = 1;
            fep->policies[i].actions = (float*)nimcp_calloc(action_dim, sizeof(float));
            if (fep->policies[i].actions) {
                /* One-hot encoding of actions */
                if (i < action_dim) {
                    fep->policies[i].actions[i] = 1.0f;
                }
            }
        }
    }

    /* Create mutex */
    fep->mutex = nimcp_platform_mutex_create();
    if (!fep->mutex) {
        fep_destroy(fep);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "fep_create: fep->mutex is NULL");
        return NULL;
    }

    NIMCP_LOGGING_INFO("FEP system created: %u levels, obs_dim=%u, act_dim=%u",
                      fep->num_levels, observation_dim, action_dim);
    return fep;
}

void fep_destroy(fep_system_t* fep) {
    if (!fep) return;

    /* Free levels */
    /* Phase 8: Heartbeat at operation start */
    free_energy_instance_heartbeat("free_energy_fep_destroy", 0.0f);


    if (fep->levels) {
        for (uint32_t i = 0; i < fep->num_levels; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && fep->num_levels > 256) {
                free_energy_instance_heartbeat("free_energy_loop",
                                 (float)(i + 1) / (float)fep->num_levels);
            }

            free_level(&fep->levels[i]);
        }
        nimcp_free(fep->levels);
    }

    /* Free observations */
    if (fep->observations) nimcp_free(fep->observations);

    /* Free action space */
    if (fep->action_space) nimcp_free(fep->action_space);

    /* Free policies */
    if (fep->policies) {
        for (uint32_t i = 0; i < fep->num_policies; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && fep->num_policies > 256) {
                free_energy_instance_heartbeat("free_energy_loop",
                                 (float)(i + 1) / (float)fep->num_policies);
            }

            if (fep->policies[i].actions) {
                nimcp_free(fep->policies[i].actions);
            }
        }
        nimcp_free(fep->policies);
    }

    /* Free mutex */
    if (fep->mutex) {
        nimcp_platform_mutex_destroy(fep->mutex);
        nimcp_free(fep->mutex);
        fep->mutex = NULL;
    }

    nimcp_free(fep);
    fep = NULL;
    NIMCP_LOGGING_INFO("FEP system destroyed");
}

int fep_reset(fep_system_t* fep) {
    if (!fep) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    free_energy_instance_heartbeat("free_energy_fep_reset", 0.0f);


    nimcp_platform_mutex_lock(fep->mutex);

    /* Reset beliefs to priors */
    for (uint32_t l = 0; l < fep->num_levels; l++) {
        /* Phase 8: Loop progress heartbeat */
        if ((l & 0xFF) == 0 && fep->num_levels > 256) {
            free_energy_instance_heartbeat("free_energy_loop",
                             (float)(l + 1) / (float)fep->num_levels);
        }

        fep_hierarchy_level_t* level = &fep->levels[l];
        /* Guard: Ensure precision is positive to prevent division by zero */
        float safe_precision = fep->config.initial_precision;
        if (safe_precision < FEP_MIN_PRECISION) {
            safe_precision = FEP_MIN_PRECISION;
        }
        for (uint32_t i = 0; i < level->beliefs.dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && level->beliefs.dim > 256) {
                free_energy_instance_heartbeat("free_energy_loop",
                                 (float)(i + 1) / (float)level->beliefs.dim);
            }

            level->beliefs.mean[i] = level->prior_mean[i];
            level->beliefs.precision[i] = safe_precision;
            level->beliefs.variance[i] = 1.0f / safe_precision;
        }

        /* Reset errors */
        for (uint32_t i = 0; i < level->errors.dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && level->errors.dim > 256) {
                free_energy_instance_heartbeat("free_energy_loop",
                                 (float)(i + 1) / (float)level->errors.dim);
            }

            level->errors.error[i] = 0.0f;
            level->errors.weighted_error[i] = 0.0f;
        }
        level->errors.magnitude = 0.0f;
        level->errors.weighted_magnitude = 0.0f;
    }

    /* Reset free energy */
    memset(&fep->free_energy, 0, sizeof(fep_free_energy_t));
    memset(&fep->expected_free_energy, 0, sizeof(fep_efe_t));

    /* Reset stats */
    memset(&fep->stats, 0, sizeof(fep_stats_t));

    nimcp_platform_mutex_unlock(fep->mutex);
    return 0;
}

/* ============================================================================
 * Observation Processing Implementation
 * ============================================================================ */

int fep_process_observation(
    fep_system_t* fep,
    const float* observation,
    uint32_t observation_dim
) {
    if (!fep || !observation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_process_observation: required parameter is NULL (fep, observation)");
        return -1;
    }
    if (observation_dim != fep->observation_dim) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "fep_process_observation: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    free_energy_instance_heartbeat("free_energy_fep_process_observat", 0.0f);


    nimcp_platform_mutex_lock(fep->mutex);

    /* Store observation */
    memcpy(fep->observations, observation, observation_dim * sizeof(float));

    /* Compute prediction errors through hierarchy */
    fep_propagate_hierarchy(fep);

    /* Update beliefs to minimize free energy */
    for (uint32_t iter = 0; iter < fep->config.max_iterations; iter++) {
        /* Phase 8: Loop progress heartbeat */
        if ((iter & 0xFF) == 0 && fep->config.max_iterations > 256) {
            free_energy_instance_heartbeat("free_energy_loop",
                             (float)(iter + 1) / (float)fep->config.max_iterations);
        }

        float prev_fe = fep->free_energy.total;

        fep_update_beliefs(fep);

        if (fep->config.learn_precision) {
            fep_update_precision(fep);
        }

        fep_free_energy_t fe;
        fep_compute_free_energy(fep, &fe);

        /* Check convergence */
        if (fabsf(fe.total - prev_fe) < fep->config.convergence_threshold) {
            break;
        }
    }

    fep->stats.total_updates++;
    nimcp_platform_mutex_unlock(fep->mutex);

    return 0;
}

uint32_t fep_compute_prediction(
    const fep_system_t* fep,
    float* prediction,
    uint32_t prediction_dim
) {
    if (!fep || !prediction) return 0;

    /* Use level 0 predictions (lowest level) */
    /* Phase 8: Heartbeat at operation start */
    free_energy_instance_heartbeat("free_energy_fep_compute_predicti", 0.0f);


    fep_hierarchy_level_t* level = &fep->levels[0];
    uint32_t dim = level->prediction_dim;
    if (dim > prediction_dim) dim = prediction_dim;

    /* Simple linear prediction: g(μ) = μ (identity for simplicity) */
    /* In full implementation, this would apply likelihood matrix */
    memcpy(prediction, level->beliefs.mean, dim * sizeof(float));

    return dim;
}

int fep_compute_prediction_error(
    const fep_system_t* fep,
    fep_prediction_error_t* error
) {
    if (!fep || !error) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_compute_prediction_error: required parameter is NULL (fep, error)");
        return -1;
    }

    /* Get level 0 */
    /* Phase 8: Heartbeat at operation start */
    free_energy_instance_heartbeat("free_energy_fep_compute_predicti", 0.0f);


    const fep_hierarchy_level_t* level = &fep->levels[0];
    uint32_t dim = level->errors.dim;

    /* Copy error structure */
    error->dim = dim;
    error->magnitude = level->errors.magnitude;
    error->weighted_magnitude = level->errors.weighted_magnitude;

    return 0;
}

/* ============================================================================
 * Belief Update Implementation
 * ============================================================================ */

int fep_update_beliefs(fep_system_t* fep) {
    if (!fep) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    free_energy_instance_heartbeat("free_energy_fep_update_beliefs", 0.0f);


    float lr = fep->config.belief_learning_rate;

    /* Update each level bottom-up */
    for (uint32_t l = 0; l < fep->num_levels; l++) {
        /* Phase 8: Loop progress heartbeat */
        if ((l & 0xFF) == 0 && fep->num_levels > 256) {
            free_energy_instance_heartbeat("free_energy_loop",
                             (float)(l + 1) / (float)fep->num_levels);
        }

        fep_hierarchy_level_t* level = &fep->levels[l];

        /* Gradient descent: μ' = μ - lr * ∂F/∂μ */
        /* ∂F/∂μ ≈ precision-weighted prediction error + prior deviation */
        for (uint32_t i = 0; i < level->beliefs.dim && i < level->errors.dim; i++) {
            /* Error-driven update */
            float grad = level->errors.weighted_error[i];

            /* Prior regularization */
            float prior_error = level->beliefs.mean[i] - level->prior_mean[i];
            grad += level->prior_precision[i] * prior_error * 0.1f;

            /* Apply update */
            level->beliefs.mean[i] -= lr * grad;
        }
    }

    fep->stats.belief_updates++;
    return 0;
}

int fep_update_precision(fep_system_t* fep) {
    if (!fep) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    free_energy_instance_heartbeat("free_energy_fep_update_precision", 0.0f);


    float lr = fep->config.precision_learning_rate;

    /* Update precision based on prediction error variance */
    for (uint32_t l = 0; l < fep->num_levels; l++) {
        /* Phase 8: Loop progress heartbeat */
        if ((l & 0xFF) == 0 && fep->num_levels > 256) {
            free_energy_instance_heartbeat("free_energy_loop",
                             (float)(l + 1) / (float)fep->num_levels);
        }

        fep_hierarchy_level_t* level = &fep->levels[l];

        for (uint32_t i = 0; i < level->errors.dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && level->errors.dim > 256) {
                free_energy_instance_heartbeat("free_energy_loop",
                                 (float)(i + 1) / (float)level->errors.dim);
            }

            /* Precision update: Π' = Π + lr * (ε² - 1/Π) */
            float error_sq = level->errors.error[i] * level->errors.error[i];

            /* Guard: Ensure precision is positive before computing variance */
            float safe_precision = level->errors.precision[i];
            if (safe_precision < FEP_MIN_PRECISION) {
                safe_precision = FEP_MIN_PRECISION;
            }
            float current_var = 1.0f / safe_precision;
            (void)current_var;  /* Silence unused variable warning */

            /* Move precision toward inverse error variance */
            /* Note: 0.01f epsilon prevents division by zero when error_sq is 0 */
            float target_precision = 1.0f / (error_sq + 0.01f);
            float delta = target_precision - level->errors.precision[i];

            level->errors.precision[i] += lr * delta;
            level->errors.precision[i] = nimcp_clampf(
                level->errors.precision[i],
                FEP_MIN_PRECISION,
                FEP_MAX_PRECISION
            );
        }
    }

    return 0;
}

int fep_propagate_hierarchy(fep_system_t* fep) {
    if (!fep) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep is NULL");

        return -1;

    }

    /* Bottom level: compare with observations */
    /* Phase 8: Heartbeat at operation start */
    free_energy_instance_heartbeat("free_energy_fep_propagate_hierar", 0.0f);


    fep_hierarchy_level_t* level0 = &fep->levels[0];
    uint32_t obs_dim = fep->observation_dim;
    uint32_t pred_dim = level0->prediction_dim;
    uint32_t min_dim = obs_dim < pred_dim ? obs_dim : pred_dim;

    /* Generate predictions from beliefs */
    for (uint32_t i = 0; i < pred_dim && i < level0->beliefs.dim; i++) {
        level0->predictions[i] = level0->beliefs.mean[i];
    }

    /* Compute prediction error: ε = o - g(μ) */
    for (uint32_t i = 0; i < min_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && min_dim > 256) {
            free_energy_instance_heartbeat("free_energy_loop",
                             (float)(i + 1) / (float)min_dim);
        }

        level0->errors.error[i] = fep->observations[i] - level0->predictions[i];
        level0->errors.weighted_error[i] =
            level0->errors.precision[i] * level0->errors.error[i];
    }
    level0->errors.magnitude = vector_norm(level0->errors.error, min_dim);
    level0->errors.weighted_magnitude = vector_norm(level0->errors.weighted_error, min_dim);

    /* Propagate through hierarchy */
    for (uint32_t l = 1; l < fep->num_levels; l++) {
        fep_hierarchy_level_t* current = &fep->levels[l];
        fep_hierarchy_level_t* lower = &fep->levels[l-1];

        /* Predictions: higher level predicts lower level state */
        uint32_t dim = current->prediction_dim;
        if (dim > lower->beliefs.dim) dim = lower->beliefs.dim;

        for (uint32_t i = 0; i < dim && i < current->beliefs.dim; i++) {
            current->predictions[i] = current->beliefs.mean[i];
        }

        /* Prediction error: lower level state - higher level prediction */
        for (uint32_t i = 0; i < dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && dim > 256) {
                free_energy_instance_heartbeat("free_energy_loop",
                                 (float)(i + 1) / (float)dim);
            }

            current->errors.error[i] = lower->beliefs.mean[i] - current->predictions[i];
            current->errors.weighted_error[i] =
                current->errors.precision[i] * current->errors.error[i];
        }
        current->errors.magnitude = vector_norm(current->errors.error, dim);
        current->errors.weighted_magnitude = vector_norm(current->errors.weighted_error, dim);
    }

    /* Update stats */
    fep->stats.avg_prediction_error =
        (fep->stats.avg_prediction_error * fep->stats.total_updates +
         level0->errors.magnitude) /
        (fep->stats.total_updates + 1);

    return 0;
}

/* ============================================================================
 * Free Energy Computation Implementation
 * ============================================================================ */

int fep_compute_free_energy(
    const fep_system_t* fep,
    fep_free_energy_t* fe
) {
    if (!fep || !fe) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_compute_free_energy: required parameter is NULL (fep, fe)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    free_energy_instance_heartbeat("free_energy_fep_compute_free_ene", 0.0f);


    memset(fe, 0, sizeof(fep_free_energy_t));

    /* Sum over hierarchy levels */
    for (uint32_t l = 0; l < fep->num_levels; l++) {
        /* Phase 8: Loop progress heartbeat */
        if ((l & 0xFF) == 0 && fep->num_levels > 256) {
            free_energy_instance_heartbeat("free_energy_loop",
                             (float)(l + 1) / (float)fep->num_levels);
        }

        const fep_hierarchy_level_t* level = &fep->levels[l];

        /* Inaccuracy: -E[ln p(o|s)] ≈ 0.5 * Σ Π * ε² */
        for (uint32_t i = 0; i < level->errors.dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && level->errors.dim > 256) {
                free_energy_instance_heartbeat("free_energy_loop",
                                 (float)(i + 1) / (float)level->errors.dim);
            }

            float err_sq = level->errors.error[i] * level->errors.error[i];
            fe->inaccuracy += 0.5f * level->errors.precision[i] * err_sq;
        }

        /* Complexity: KL[q(s)||p(s)] ≈ 0.5 * Σ (μ-μ₀)²/σ₀² - ln(σ/σ₀) */
        for (uint32_t i = 0; i < level->beliefs.dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && level->beliefs.dim > 256) {
                free_energy_instance_heartbeat("free_energy_loop",
                                 (float)(i + 1) / (float)level->beliefs.dim);
            }

            float mean_diff = level->beliefs.mean[i] - level->prior_mean[i];
            fe->complexity += 0.5f * level->prior_precision[i] * mean_diff * mean_diff;

            /* Entropy term */
            fe->entropy += 0.5f * safe_log(2.0f * (float)M_PI * level->beliefs.variance[i]);
        }
    }

    /* Total free energy */
    fe->total = fe->complexity + fe->inaccuracy;

    /* Energy term */
    fe->energy = fe->inaccuracy + fe->complexity;

    /* Surprise bound */
    fe->surprise = fe->total;

    /* Update system state */
    ((fep_system_t*)fep)->free_energy = *fe;

    /* Stats update */
    fep_stats_t* stats = &((fep_system_t*)fep)->stats;
    if (isfinite(fe->total) && stats->total_updates < UINT64_MAX) {
        stats->avg_free_energy = (stats->avg_free_energy * stats->total_updates + fe->total) /
                                 (stats->total_updates + 1);
    }
    if (fe->total < stats->min_free_energy || stats->min_free_energy == 0.0f) {
        stats->min_free_energy = fe->total;
    }
    if (fe->surprise > stats->max_surprise) {
        stats->max_surprise = fe->surprise;
    }

    /* W8: rate-limited emit (every 64 updates). Read-back queries whether
     * the predictive root is present (common cross-consumer). */
    if ((stats->total_updates & 0x3F) == 0) {
        struct brain_struct* _kg_brain =
            world_model_kg_events_get_registered_brain();
        if (_kg_brain) {
            /* Average precision across first hierarchy level (best-effort). */
            float precision = 0.0f;
            if (fep->num_levels > 0 && fep->levels[0].errors.dim > 0 &&
                fep->levels[0].errors.precision) {
                precision = fep->levels[0].errors.precision[0];
            }
            world_model_kg_emit_surprise(_kg_brain,
                fe->total, fe->surprise, precision);
            (void)world_model_kg_has_partner(_kg_brain, "cog_predictive");
        }
    }

    return 0;
}

float fep_compute_component(
    const fep_system_t* fep,
    fep_component_t component
) {
    if (!fep) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    free_energy_instance_heartbeat("free_energy_fep_compute_componen", 0.0f);


    switch (component) {
        case FEP_COMPONENT_COMPLEXITY:
            return fep->free_energy.complexity;
        case FEP_COMPONENT_INACCURACY:
            return fep->free_energy.inaccuracy;
        case FEP_COMPONENT_ENERGY:
            return fep->free_energy.energy;
        case FEP_COMPONENT_ENTROPY:
            return fep->free_energy.entropy;
        default:
            return 0.0f;
    }
}

float fep_compute_surprise(const fep_system_t* fep) {
    if (!fep) return 0.0f;
    /* Phase 8: Heartbeat at operation start */
    free_energy_instance_heartbeat("free_energy_fep_compute_surprise", 0.0f);


    return fep->free_energy.surprise;
}

/* ============================================================================
 * Active Inference Implementation
 * ============================================================================ */

/**
 * @brief Internal policy evaluation (no mutex - caller must hold lock)
 */
static int fep_evaluate_policies_unlocked(fep_system_t* fep) {
    if (!fep || !fep->policies) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_evaluate_policies_unlocked: required parameter is NULL (fep, fep->policies)");
        return -1;
    }

    /* Compute EFE for each policy */
    for (uint32_t i = 0; i < fep->num_policies; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && fep->num_policies > 256) {
            free_energy_instance_heartbeat("free_energy_loop",
                             (float)(i + 1) / (float)fep->num_policies);
        }

        fep_efe_t efe;
        fep_compute_efe(fep, &fep->policies[i], &efe);
        fep->policies[i].expected_free_energy = efe.total;
    }

    /* Compute policy probabilities via softmax over -G(π) */
    float* neg_efe = (float*)nimcp_calloc(fep->num_policies, sizeof(float));
    if (neg_efe) {
        for (uint32_t i = 0; i < fep->num_policies; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && fep->num_policies > 256) {
                free_energy_instance_heartbeat("free_energy_loop",
                                 (float)(i + 1) / (float)fep->num_policies);
            }

            neg_efe[i] = -fep->policies[i].expected_free_energy;
        }

        softmax(neg_efe, fep->num_policies, fep->config.action_temperature);

        for (uint32_t i = 0; i < fep->num_policies; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && fep->num_policies > 256) {
                free_energy_instance_heartbeat("free_energy_loop",
                                 (float)(i + 1) / (float)fep->num_policies);
            }

            fep->policies[i].probability = neg_efe[i];
        }

        nimcp_free(neg_efe);
        neg_efe = NULL;
    }

    return 0;
}

int fep_compute_efe(
    const fep_system_t* fep,
    const fep_policy_t* policy,
    fep_efe_t* efe
) {
    if (!fep || !policy || !efe) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_compute_efe: required parameter is NULL (fep, policy, efe)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    free_energy_instance_heartbeat("free_energy_fep_compute_efe", 0.0f);


    memset(efe, 0, sizeof(fep_efe_t));

    /* Simplified EFE computation */
    /* G(π) = Risk + Ambiguity */

    /* Risk: Expected deviation from preferred states */
    /* For simplicity: higher action entropy = higher risk */
    float action_entropy = 0.0f;
    for (uint32_t i = 0; i < policy->action_dim && policy->actions; i++) {
        if (policy->actions[i] > 0.001f) {
            action_entropy -= policy->actions[i] * safe_log(policy->actions[i]);
        }
    }
    efe->risk = action_entropy;

    /* Ambiguity: Expected uncertainty about outcomes */
    /* Use current prediction error as proxy */
    efe->ambiguity = fep->levels[0].errors.magnitude * 0.5f;

    /* Intrinsic value: Information gain (epistemic) */
    efe->intrinsic_value = efe->ambiguity;  /* High ambiguity = more to learn */

    /* Extrinsic value: Goal achievement (pragmatic) */
    efe->extrinsic_value = -efe->risk;

    /* Total EFE */
    efe->total = efe->risk + efe->ambiguity;

    return 0;
}

int fep_evaluate_policies(fep_system_t* fep) {
    if (!fep || !fep->policies) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_evaluate_policies: required parameter is NULL (fep, fep->policies)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    free_energy_instance_heartbeat("free_energy_fep_evaluate_policie", 0.0f);


    nimcp_platform_mutex_lock(fep->mutex);
    int ret = fep_evaluate_policies_unlocked(fep);
    nimcp_platform_mutex_unlock(fep->mutex);

    return ret;
}

int fep_select_action(
    fep_system_t* fep,
    float* action,
    uint32_t action_dim
) {
    if (!fep || !action) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_select_action: required parameter is NULL (fep, action)");
        return -1;
    }
    if (action_dim < fep->action_dim) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "fep_select_action: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    free_energy_instance_heartbeat("free_energy_fep_select_action", 0.0f);


    nimcp_platform_mutex_lock(fep->mutex);

    /* Evaluate policies first (use unlocked version since we hold mutex) */
    fep_evaluate_policies_unlocked(fep);

    /* Select based on mode */
    uint32_t selected = 0;

    switch (fep->config.action_mode) {
        case FEP_ACTION_GREEDY: {
            /* Select policy with minimum EFE */
            float min_efe = fep->policies[0].expected_free_energy;
            for (uint32_t i = 1; i < fep->num_policies; i++) {
                if (fep->policies[i].expected_free_energy < min_efe) {
                    min_efe = fep->policies[i].expected_free_energy;
                    selected = i;
                }
            }
            break;
        }

        case FEP_ACTION_SOFTMAX:
        case FEP_ACTION_THOMPSON:
        default: {
            /* Sample from policy distribution */
            float r = (float)nimcp_tl_rand() / (float)RAND_MAX;
            float cumsum = 0.0f;
            for (uint32_t i = 0; i < fep->num_policies; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && fep->num_policies > 256) {
                    free_energy_instance_heartbeat("free_energy_loop",
                                     (float)(i + 1) / (float)fep->num_policies);
                }

                cumsum += fep->policies[i].probability;
                if (r <= cumsum) {
                    selected = i;
                    break;
                }
            }
            break;
        }
    }

    fep->selected_policy = selected;

    /* Copy action */
    if (fep->policies[selected].actions) {
        uint32_t copy_dim = fep->action_dim < action_dim ? fep->action_dim : action_dim;
        memcpy(action, fep->policies[selected].actions, copy_dim * sizeof(float));
    }

    fep->stats.action_selections++;
    nimcp_platform_mutex_unlock(fep->mutex);

    return selected;
}

int fep_set_preferences(
    fep_system_t* fep,
    const float* preferred,
    float precision,
    uint32_t dim
) {
    if (!fep || !preferred) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_set_preferences: required parameter is NULL (fep, preferred)");
        return -1;
    }

    /* Store preferences as prior for level 0 */
    /* Phase 8: Heartbeat at operation start */
    free_energy_instance_heartbeat("free_energy_fep_set_preferences", 0.0f);


    uint32_t store_dim = dim < fep->levels[0].beliefs.dim ? dim : fep->levels[0].beliefs.dim;

    for (uint32_t i = 0; i < store_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && store_dim > 256) {
            free_energy_instance_heartbeat("free_energy_loop",
                             (float)(i + 1) / (float)store_dim);
        }

        fep->levels[0].prior_mean[i] = preferred[i];
        fep->levels[0].prior_precision[i] = precision;
    }

    return 0;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int fep_get_beliefs(
    const fep_system_t* fep,
    uint32_t level,
    fep_belief_t* beliefs
) {
    if (!fep || !beliefs || level >= fep->num_levels) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_get_beliefs: required parameter is NULL (fep, beliefs)");
        return -1;
    }

    *beliefs = fep->levels[level].beliefs;
    /* Phase 8: Heartbeat at operation start */
    free_energy_instance_heartbeat("free_energy_fep_get_beliefs", 0.0f);


    return 0;
}

float fep_get_free_energy(const fep_system_t* fep) {
    if (!fep) return 0.0f;
    /* Phase 8: Heartbeat at operation start */
    free_energy_instance_heartbeat("free_energy_fep_get_free_energy", 0.0f);


    return fep->free_energy.total;
}

float fep_get_prediction_error(const fep_system_t* fep, uint32_t level) {
    if (!fep || level >= fep->num_levels) return 0.0f;
    /* Phase 8: Heartbeat at operation start */
    free_energy_instance_heartbeat("free_energy_fep_get_prediction_e", 0.0f);


    return fep->levels[level].errors.magnitude;
}

int fep_get_selected_policy(const fep_system_t* fep, fep_policy_t* policy) {
    if (!fep || !policy || !fep->policies) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_get_selected_policy: required parameter is NULL (fep, policy, fep->policies)");
        return -1;
    }
    if (fep->selected_policy >= fep->num_policies) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "fep_get_selected_policy: capacity exceeded");
        return -1;
    }

    *policy = fep->policies[fep->selected_policy];
    /* Phase 8: Heartbeat at operation start */
    free_energy_instance_heartbeat("free_energy_fep_get_selected_pol", 0.0f);


    return 0;
}

int fep_get_stats(const fep_system_t* fep, fep_stats_t* stats) {
    if (!fep || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_get_stats: required parameter is NULL (fep, stats)");
        return -1;
    }
    *stats = fep->stats;
    /* Phase 8: Heartbeat at operation start */
    free_energy_instance_heartbeat("free_energy_fep_get_stats", 0.0f);


    return 0;
}

/* ============================================================================
 * String Conversion Implementation
 * ============================================================================ */

const char* fep_update_mode_to_string(fep_update_mode_t mode) {
    switch (mode) {
        case FEP_UPDATE_GRADIENT_DESCENT:    return "GRADIENT_DESCENT";
        case FEP_UPDATE_PREDICTIVE_CODING:   return "PREDICTIVE_CODING";
        case FEP_UPDATE_VARIATIONAL_MESSAGE: return "VARIATIONAL_MESSAGE";
        case FEP_UPDATE_KALMAN_FILTER:       return "KALMAN_FILTER";
        default:                              return "UNKNOWN";
    }
}

const char* fep_action_mode_to_string(fep_action_mode_t mode) {
    switch (mode) {
        case FEP_ACTION_SOFTMAX:  return "SOFTMAX";
        case FEP_ACTION_GREEDY:   return "GREEDY";
        case FEP_ACTION_THOMPSON: return "THOMPSON";
        default:                   return "UNKNOWN";
    }
}

const char* fep_component_to_string(fep_component_t component) {
    switch (component) {
        case FEP_COMPONENT_COMPLEXITY:  return "COMPLEXITY";
        case FEP_COMPONENT_INACCURACY:  return "INACCURACY";
        case FEP_COMPONENT_ENERGY:      return "ENERGY";
        case FEP_COMPONENT_ENTROPY:     return "ENTROPY";
        default:                         return "UNKNOWN";
    }
}

/* ============================================================================
 * KG Self-Awareness Integration
 * ============================================================================ */

int fep_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    free_energy_instance_heartbeat("free_energy_fep_query_self_knowl", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "FEP_Module");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                free_energy_instance_heartbeat("free_energy_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("FEP self-knowledge: %s", self->observations[i]);
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "FEP_Module");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "FEP_Module");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void free_energy_set_instance_health_agent(void* ctx, nimcp_health_agent_t* agent) {
    (void)ctx;
    g_free_energy_instance_health_agent = agent;
}

/* ============================================================================
 * Phase 8: Full Training Implementation
 * ============================================================================ */
int free_energy_training_begin(void* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "free_energy_training_begin: ctx is NULL");
        return -1;
    }
    free_energy_instance_heartbeat_instance(g_free_energy_instance_health_agent, "free_energy_training_begin", 0.0f);
    fep_system_t* s = (fep_system_t*)ctx;
    s->stats.total_updates = 0;
    s->stats.belief_updates = 0;
    s->config.belief_learning_rate = (s->config.belief_learning_rate > 0.0f) ? s->config.belief_learning_rate : 0.5f;
    s->config.precision_learning_rate = (s->config.precision_learning_rate > 0.0f) ? s->config.precision_learning_rate : 0.5f;
    NIMCP_LOGGING_INFO("free_energy: training begun, counters reset");
    return 0;
}

int free_energy_training_step(void* ctx, float progress) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "free_energy_training_step: ctx is NULL");
        return -1;
    }
    float clamped = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    free_energy_instance_heartbeat_instance(g_free_energy_instance_health_agent, "free_energy_training_step", clamped);
    fep_system_t* s = (fep_system_t*)ctx;
    float p = clamped;
    s->config.belief_learning_rate += (1.0f - p) * 0.001f;
    if (s->config.belief_learning_rate > 2.0f) s->config.belief_learning_rate = 2.0f;
    if (s->config.belief_learning_rate < 0.0f) s->config.belief_learning_rate = 0.0f;
    s->config.precision_learning_rate += (1.0f - p) * 0.001f;
    if (s->config.precision_learning_rate > 2.0f) s->config.precision_learning_rate = 2.0f;
    if (s->config.precision_learning_rate < 0.0f) s->config.precision_learning_rate = 0.0f;
    s->config.action_learning_rate += (1.0f - p) * 0.001f;
    if (s->config.action_learning_rate > 2.0f) s->config.action_learning_rate = 2.0f;
    if (s->config.action_learning_rate < 0.0f) s->config.action_learning_rate = 0.0f;
    s->config.convergence_threshold += (1.0f - p) * 0.001f;
    if (s->config.convergence_threshold > 2.0f) s->config.convergence_threshold = 2.0f;
    if (s->config.convergence_threshold < 0.0f) s->config.convergence_threshold = 0.0f;
    s->stats.total_updates++;
    s->stats.belief_updates++;
    return 0;
}

int free_energy_training_end(void* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "free_energy_training_end: ctx is NULL");
        return -1;
    }
    free_energy_instance_heartbeat_instance(g_free_energy_instance_health_agent, "free_energy_training_end", 1.0f);
    fep_system_t* s = (fep_system_t*)ctx;
    float metric_sum = 0.0f;
    metric_sum += s->config.belief_learning_rate;
    metric_sum += s->config.precision_learning_rate;
    metric_sum += s->config.action_learning_rate;
    metric_sum += s->config.convergence_threshold;
    float avg_metric = metric_sum / 4.0f;
    NIMCP_LOGGING_INFO("free_energy: training complete, avg_metric=%.4f", avg_metric);
    return 0;
}
