/**
 * @file nimcp_cortical_sparse_coding.c
 * @brief Sparse Distributed Representations Implementation
 */

#include "core/cortical_columns/nimcp_cortical_sparse_coding.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/platform/nimcp_platform_time.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_threshold_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(cortical_sparse_coding)

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Internal sparse coding system state
 */
struct cortical_sparse_coding_system {
    sparse_coding_config_t config;

    /* Per-column state */
    column_sparse_state_t* column_states;
    uint32_t num_columns;

    /* Global state */
    sparse_coding_state_t state;

    /* Statistics */
    sparse_coding_stats_t stats;

    /* Activity history (for lifetime sparsity) */
    float* activity_history;
    uint32_t history_index;
    uint32_t history_size;

    /* Bio-async integration */
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;

    /* Thread safety */
    nimcp_platform_mutex_t* mutex;

    /* Timestamps */
    uint64_t start_time;
    uint64_t last_update_time;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * WHAT: Compute L1 norm (Manhattan norm) of activation vector
 * WHY:  Sparsity penalty term in loss function
 * HOW:  Sum absolute values
 */
static float compute_l1_norm(const float* activations, uint32_t n) {
    if (!activations) return 0.0f;

    float sum = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        sum += fabsf(activations[i]);
    }
    return sum;
}

/**
 * WHAT: Compute L2 norm squared (Euclidean norm squared)
 * WHY:  Reconstruction error term in loss function
 * HOW:  Sum of squared elements
 */
static float compute_l2_norm_squared(const float* vec, uint32_t n) {
    if (!vec) return 0.0f;

    float sum = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        sum += vec[i] * vec[i];
    }
    return sum;
}

/**
 * WHAT: Count number of non-zero (above threshold) activations
 * WHY:  Compute population sparsity
 * HOW:  Count elements above their thresholds
 */
static uint32_t count_active_columns(
    const cortical_sparse_coding_system_t* system,
    const float* activations,
    uint32_t n
) {
    if (!system || !activations) return 0;

    uint32_t count = 0;
    for (uint32_t i = 0; i < n; i++) {
        if (fabsf(activations[i]) > system->column_states[i].activation_threshold) {
            count++;
        }
    }
    return count;
}

/**
 * WHAT: Compute mean and variance of array
 * WHY:  Statistics computation for kurtosis
 * HOW:  Two-pass algorithm
 */
static void compute_mean_variance(
    const float* data,
    uint32_t n,
    float* mean_out,
    float* var_out
) {
    if (!data || n == 0) {
        if (mean_out) *mean_out = 0.0f;
        if (var_out) *var_out = 0.0f;
        return;
    }

    /* Compute mean */
    float sum = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        sum += data[i];
    }
    float mean = sum / (float)n;

    /* Compute variance */
    float var_sum = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        float diff = data[i] - mean;
        var_sum += diff * diff;
    }
    float variance = var_sum / (float)n;

    if (mean_out) *mean_out = mean;
    if (var_out) *var_out = variance;
}

/**
 * WHAT: Find K largest elements via partial sort
 * WHY:  K-winners-take-all sparsity enforcement
 * HOW:  QuickSelect-like algorithm
 */
static void find_k_largest_indices(
    const float* values,
    uint32_t n,
    uint32_t k,
    uint32_t* indices
) {
    if (!values || !indices || k == 0 || k > n) return;

    /* Create index-value pairs */
    typedef struct { float val; uint32_t idx; } pair_t;
    pair_t* pairs = (pair_t*)nimcp_malloc(n * sizeof(pair_t));
    if (!pairs) return;

    for (uint32_t i = 0; i < n; i++) {
        pairs[i].val = values[i];
        pairs[i].idx = i;
    }

    /* Partial sort (bubble sort for simplicity, can optimize) */
    for (uint32_t i = 0; i < k; i++) {
        uint32_t max_idx = i;
        for (uint32_t j = i + 1; j < n; j++) {
            if (pairs[j].val > pairs[max_idx].val) {
                max_idx = j;
            }
        }
        /* Swap */
        pair_t tmp = pairs[i];
        pairs[i] = pairs[max_idx];
        pairs[max_idx] = tmp;
    }

    /* Extract top-k indices */
    for (uint32_t i = 0; i < k; i++) {
        indices[i] = pairs[i].idx;
    }

    nimcp_free(pairs);
}

/**
 * WHAT: Apply soft thresholding (shrinkage operator)
 * WHY:  Differentiable sparsity enforcement
 * HOW:  S(x, λ) = sign(x) * max(|x| - λ, 0)
 */
static float soft_threshold(float x, float lambda) {
    float abs_x = fabsf(x);
    if (abs_x <= lambda) return 0.0f;
    return (x > 0.0f ? 1.0f : -1.0f) * (abs_x - lambda);
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int cortical_sparse_default_config(sparse_coding_config_t* config) {
    /* Guard clause: validate parameters */
    NIMCP_CHECK_THROW(config != NULL, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    /* Sparsity constraints */
    config->sparsity_type = SPARSITY_BOTH;
    config->sparsity_method = SPARSITY_METHOD_THRESHOLD;
    config->target_sparsity = SPARSE_CODING_DEFAULT_SPARSITY;
    config->sparsity_penalty = SPARSE_CODING_DEFAULT_LAMBDA;

    /* Dictionary parameters */
    config->overcomplete_ratio = SPARSE_CODING_DEFAULT_OVERCOMPLETE;
    config->num_input_dims = 100;  /* Default, user should override */
    config->num_columns = (uint32_t)(config->num_input_dims * config->overcomplete_ratio);

    /* Lateral inhibition */
    config->enable_lateral_inhibition = true;
    config->inhibition_strength = 0.2f;
    config->inhibition_radius = 2.0f;

    /* Homeostatic adaptation */
    config->enable_homeostasis = true;
    config->adaptation_rate = SPARSE_CODING_ADAPTATION_RATE;
    config->adaptation_time_constant = 1000.0f;  /* 1 second */

    /* K-WTA */
    config->k_winners = (uint32_t)(config->num_columns * config->target_sparsity);

    /* Reconstruction */
    config->track_reconstruction_error = true;
    config->reconstruction_weight = 1.0f;

    /* Statistics */
    config->compute_kurtosis = false;  /* Expensive, off by default */
    config->stats_window_size = 100;

    /* Bio-async */
    config->enable_bio_async = false;

    return NIMCP_SUCCESS;
}

cortical_sparse_coding_system_t* cortical_sparse_create(
    const sparse_coding_config_t* config
) {
    /* Apply default config if NULL */
    sparse_coding_config_t default_cfg;
    if (!config) {
        cortical_sparse_default_config(&default_cfg);
        config = &default_cfg;
    }

    /* Guard clause: validate configuration */
    if (config->num_columns == 0 || config->num_columns > SPARSE_CODING_MAX_COLUMNS) {
        NIMCP_LOGGING_ERROR("Invalid num_columns");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cortical_sparse_create: config->num_columns is zero");
        return NULL;
    }
    if (config->target_sparsity < SPARSE_CODING_MIN_SPARSITY ||
        config->target_sparsity > SPARSE_CODING_MAX_SPARSITY) {
        NIMCP_LOGGING_ERROR("Invalid target_sparsity");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cortical_sparse_create: config->num_columns is zero");
        return NULL;
    }

    /* Allocate system */
    cortical_sparse_coding_system_t* system = (cortical_sparse_coding_system_t*)
        nimcp_malloc(sizeof(cortical_sparse_coding_system_t));
    if (!system) {
        NIMCP_LOGGING_ERROR("Failed to allocate sparse coding system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cortical_sparse_create: system is NULL");
        return NULL;
    }
    memset(system, 0, sizeof(cortical_sparse_coding_system_t));

    /* Copy configuration */
    memcpy(&system->config, config, sizeof(sparse_coding_config_t));
    system->num_columns = config->num_columns;

    /* Allocate per-column states */
    system->column_states = (column_sparse_state_t*)
        nimcp_calloc(config->num_columns, sizeof(column_sparse_state_t));
    if (!system->column_states) {
        NIMCP_LOGGING_ERROR("Failed to allocate column states");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cortical_sparse_create: system->column_states is NULL");
        goto cleanup;
    }

    /* Initialize column states */
    for (uint32_t i = 0; i < config->num_columns; i++) {
        system->column_states[i].column_id = i;
        system->column_states[i].activation_threshold = NIMCP_ACTIVATION_THRESHOLD;  /* Initial threshold */
        system->column_states[i].current_activation = 0.0f;
        system->column_states[i].lifetime_activity = 0.0f;
        system->column_states[i].inhibition_received = 0.0f;
        system->column_states[i].activation_count = 0;
        system->column_states[i].last_active_time = 0;
        system->column_states[i].is_currently_active = false;
    }

    /* Allocate activity history */
    system->history_size = config->stats_window_size;
    system->activity_history = (float*)nimcp_calloc(system->history_size, sizeof(float));
    if (!system->activity_history) {
        NIMCP_LOGGING_ERROR("Failed to allocate activity history");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cortical_sparse_create: system->activity_history is NULL");
        goto cleanup;
    }
    system->history_index = 0;

    /* Initialize state */
    memset(&system->state, 0, sizeof(sparse_coding_state_t));
    system->state.current_population_sparsity = 0.0f;
    system->state.current_lifetime_sparsity = 0.0f;
    system->state.mean_activation_threshold = NIMCP_ACTIVATION_THRESHOLD;

    /* Initialize statistics */
    memset(&system->stats, 0, sizeof(sparse_coding_stats_t));
    system->stats.total_columns = config->num_columns;

    /* Create mutex */
    system->mutex = nimcp_malloc(sizeof(nimcp_platform_mutex_t));
    if (!system->mutex) {
        NIMCP_LOGGING_ERROR("Failed to allocate mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cortical_sparse_create: system->mutex is NULL");
        goto cleanup;
    }
    nimcp_platform_mutex_init(system->mutex, false);

    /* Timestamps */
    system->start_time = nimcp_platform_time_monotonic_us();
    system->last_update_time = system->start_time;

    /* Bio-async (if enabled) */
    system->bio_async_enabled = false;
    if (config->enable_bio_async) {
        cortical_sparse_connect_bio_async(system);
    }

    NIMCP_LOGGING_INFO("Created sparse coding system with %u columns, target sparsity %.2f%%",
                       config->num_columns, config->target_sparsity * 100.0f);

    return system;

cleanup:
    nimcp_free(system->mutex);
    nimcp_free(system->activity_history);
    nimcp_free(system->column_states);
    nimcp_free(system);
    return NULL;
}

void cortical_sparse_destroy(cortical_sparse_coding_system_t* system) {
    /* Guard clause: validate pointer */
    if (!system) return;

    /* Disconnect bio-async */
    if (system->bio_async_enabled) {
        cortical_sparse_disconnect_bio_async(system);
    }

    /* Destroy mutex */
    if (system->mutex) {
        nimcp_platform_mutex_destroy(system->mutex);
        nimcp_free(system->mutex);
        system->mutex = NULL;
    }

    /* Free arrays */
    if (system->activity_history) {
        nimcp_free(system->activity_history);
    }
    if (system->column_states) {
        nimcp_free(system->column_states);
    }

    /* Free system */
    nimcp_free(system);

    NIMCP_LOGGING_INFO("Destroyed sparse coding system");
}

/* ============================================================================
 * Sparsity Enforcement Implementation
 * ============================================================================ */

int cortical_sparse_enforce_sparsity(
    cortical_sparse_coding_system_t* system,
    const float* activations,
    uint32_t num_activations,
    float* output_activations
) {
    /* Guard clause: validate parameters */
    NIMCP_CHECK_THROW(system != NULL && activations != NULL && output_activations != NULL,
                      NIMCP_ERROR_NULL_POINTER, "NULL parameter in enforce_sparsity");
    NIMCP_CHECK_THROW(num_activations == system->num_columns,
                      NIMCP_ERROR_INVALID_PARAM, "num_activations mismatch");

    nimcp_platform_mutex_lock(system->mutex);

    /* Apply method-specific sparsity enforcement */
    switch (system->config.sparsity_method) {
        case SPARSITY_METHOD_THRESHOLD: {
            /* Hard threshold: keep only activations above threshold */
            for (uint32_t i = 0; i < num_activations; i++) {
                float threshold = system->column_states[i].activation_threshold;
                output_activations[i] = (activations[i] > threshold) ? activations[i] : 0.0f;
            }
            break;
        }

        case SPARSITY_METHOD_K_WTA: {
            /* K-winners-take-all: keep only top-K activations */
            uint32_t k = system->config.k_winners;
            if (k > num_activations) k = num_activations;

            uint32_t* winner_indices = (uint32_t*)nimcp_malloc(k * sizeof(uint32_t));
            float* winner_values = (float*)nimcp_malloc(k * sizeof(float));
            if (!winner_indices || !winner_values) {
                nimcp_free(winner_indices);
                nimcp_free(winner_values);
                nimcp_platform_mutex_unlock(system->mutex);
                NIMCP_CHECK_THROW(false, NIMCP_ERROR_MEMORY, "Failed to allocate winner buffers");
            }

            find_k_largest_indices(activations, num_activations, k, winner_indices);

            /* Save winner values BEFORE zeroing (supports in-place where activations == output) */
            for (uint32_t i = 0; i < k; i++) {
                winner_values[i] = activations[winner_indices[i]];
            }

            /* Zero all outputs */
            memset(output_activations, 0, num_activations * sizeof(float));

            /* Restore winners */
            for (uint32_t i = 0; i < k; i++) {
                output_activations[winner_indices[i]] = winner_values[i];
            }

            nimcp_free(winner_indices);
            nimcp_free(winner_values);
            break;
        }

        case SPARSITY_METHOD_SOFT_THRESHOLD: {
            /* Soft thresholding (shrinkage) */
            float lambda = system->config.sparsity_penalty;
            for (uint32_t i = 0; i < num_activations; i++) {
                output_activations[i] = soft_threshold(activations[i], lambda);
            }
            break;
        }

        case SPARSITY_METHOD_LATERAL_INHIB: {
            /* Copy input to output, then apply lateral inhibition */
            memcpy(output_activations, activations, num_activations * sizeof(float));
            nimcp_platform_mutex_unlock(system->mutex);
            return cortical_sparse_apply_lateral_inhibition(system, output_activations, num_activations);
        }

        default:
            nimcp_platform_mutex_unlock(system->mutex);
            NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_PARAM, "Unknown sparsity method");
    }

    /* Update population sparsity */
    uint32_t active_count = count_active_columns(system, output_activations, num_activations);
    system->state.current_population_sparsity = (float)active_count / (float)num_activations;
    system->stats.active_columns = active_count;
    system->stats.activity_ratio = system->state.current_population_sparsity;

    /* Update column states */
    uint64_t now = nimcp_platform_time_monotonic_us();
    for (uint32_t i = 0; i < num_activations; i++) {
        system->column_states[i].current_activation = output_activations[i];
        if (output_activations[i] > system->column_states[i].activation_threshold) {
            system->column_states[i].is_currently_active = true;
            system->column_states[i].activation_count++;
            system->column_states[i].last_active_time = now;
        } else {
            system->column_states[i].is_currently_active = false;
        }
    }

    system->state.update_count++;
    system->state.last_update_time = now;

    nimcp_platform_mutex_unlock(system->mutex);
    return NIMCP_SUCCESS;
}

int cortical_sparse_apply_lateral_inhibition(
    cortical_sparse_coding_system_t* system,
    float* activations,
    uint32_t num_activations
) {
    /* Guard clause: validate parameters */
    NIMCP_CHECK_THROW(system != NULL && activations != NULL,
                      NIMCP_ERROR_NULL_POINTER, "NULL parameter in lateral_inhibition");
    NIMCP_CHECK_THROW(num_activations == system->num_columns,
                      NIMCP_ERROR_INVALID_PARAM, "num_activations mismatch");

    nimcp_platform_mutex_lock(system->mutex);

    float gamma = system->config.inhibition_strength;
    float sigma = system->config.inhibition_radius;
    float sigma_sq = sigma * sigma;

    /* Compute inhibition for each column */
    float* inhibition = (float*)nimcp_calloc(num_activations, sizeof(float));
    if (!inhibition) {
        nimcp_platform_mutex_unlock(system->mutex);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_MEMORY, "Failed to allocate inhibition array");
    }

    /* Simplified: assume 1D columnar arrangement */
    for (uint32_t i = 0; i < num_activations; i++) {
        float total_inhib = 0.0f;

        for (uint32_t j = 0; j < num_activations; j++) {
            if (i == j) continue;  /* No self-inhibition */

            /* Distance (1D approximation) */
            float dist = fabsf((float)i - (float)j);

            /* Mexican hat: inhibition falls off with distance */
            float weight = gamma * expf(-dist * dist / (2.0f * sigma_sq));
            total_inhib += weight * activations[j];
        }

        inhibition[i] = total_inhib;
        system->column_states[i].inhibition_received = total_inhib;
    }

    /* Apply inhibition */
    for (uint32_t i = 0; i < num_activations; i++) {
        activations[i] = fmaxf(0.0f, activations[i] - inhibition[i]);
    }

    nimcp_free(inhibition);
    nimcp_platform_mutex_unlock(system->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Sparsity Metrics Implementation
 * ============================================================================ */

float cortical_sparse_compute_population_sparsity(
    cortical_sparse_coding_system_t* system,
    const float* activations,
    uint32_t num_activations
) {
    /* Guard clause: validate parameters */
    if (!system || !activations) {
        return -1.0f;
    }
    if (num_activations != system->num_columns) {
        return -1.0f;
    }

    uint32_t active_count = count_active_columns(system, activations, num_activations);
    return (float)active_count / (float)num_activations;
}

float cortical_sparse_compute_lifetime_sparsity(
    cortical_sparse_coding_system_t* system
) {
    /* Guard clause: validate parameter */
    if (!system) {
        return -1.0f;
    }

    nimcp_platform_mutex_lock(system->mutex);

    /* Compute average lifetime activity across all columns */
    float total_activity = 0.0f;
    for (uint32_t i = 0; i < system->num_columns; i++) {
        total_activity += system->column_states[i].lifetime_activity;
    }

    float lifetime_sparsity = total_activity / (float)system->num_columns;
    system->state.current_lifetime_sparsity = lifetime_sparsity;

    nimcp_platform_mutex_unlock(system->mutex);

    return lifetime_sparsity;
}

float cortical_sparse_compute_kurtosis(
    const float* activations,
    uint32_t num_activations
) {
    /* Guard clause: validate parameters */
    if (!activations || num_activations < 4) {
        return -1.0f;
    }

    /* Compute mean and variance */
    float mean = 0.0f, variance = 0.0f;
    compute_mean_variance(activations, num_activations, &mean, &variance);

    if (variance < 1e-10f) {
        return -1.0f;  /* No variance, kurtosis undefined */
    }

    /* Compute fourth moment */
    float fourth_moment = 0.0f;
    for (uint32_t i = 0; i < num_activations; i++) {
        float diff = activations[i] - mean;
        float diff_sq = diff * diff;
        fourth_moment += diff_sq * diff_sq;
    }
    fourth_moment /= (float)num_activations;

    /* Kurtosis = E[(x-μ)⁴] / σ⁴ */
    float variance_sq = variance * variance;
    float kurtosis = fourth_moment / variance_sq;

    return kurtosis;
}

/* ============================================================================
 * Threshold Adaptation Implementation
 * ============================================================================ */

int cortical_sparse_update_thresholds(
    cortical_sparse_coding_system_t* system,
    float current_sparsity
) {
    /* Guard clause: validate parameters */
    NIMCP_CHECK_THROW(system != NULL, NIMCP_ERROR_NULL_POINTER, "system is NULL");

    if (!system->config.enable_homeostasis) {
        return NIMCP_SUCCESS;  /* Homeostasis disabled */
    }

    nimcp_platform_mutex_lock(system->mutex);

    float target = system->config.target_sparsity;
    float eta = system->config.adaptation_rate;
    float error = target - current_sparsity;

    /* Update thresholds for all columns */
    float threshold_sum = 0.0f;
    for (uint32_t i = 0; i < system->num_columns; i++) {
        /* Homeostatic rule: increase threshold if too active, decrease if too quiet */
        float current_activity = system->column_states[i].lifetime_activity;
        float local_error = target - current_activity;

        system->column_states[i].activation_threshold -= eta * local_error;

        /* Clamp threshold to reasonable range */
        if (system->column_states[i].activation_threshold < 0.01f) {
            system->column_states[i].activation_threshold = 0.01f;
        }
        if (system->column_states[i].activation_threshold > 10.0f) {
            system->column_states[i].activation_threshold = 10.0f;
        }

        threshold_sum += system->column_states[i].activation_threshold;
    }

    /* Update statistics */
    system->state.mean_activation_threshold = threshold_sum / (float)system->num_columns;
    system->stats.mean_threshold = system->state.mean_activation_threshold;
    system->stats.homeostatic_adjustments++;

    nimcp_platform_mutex_unlock(system->mutex);

    return NIMCP_SUCCESS;
}

int cortical_sparse_set_column_threshold(
    cortical_sparse_coding_system_t* system,
    uint32_t column_id,
    float threshold
) {
    /* Guard clause: validate parameters */
    NIMCP_CHECK_THROW(system != NULL, NIMCP_ERROR_NULL_POINTER, "system is NULL");
    NIMCP_CHECK_THROW(column_id < system->num_columns,
                      NIMCP_ERROR_INVALID_PARAM, "column_id out of range");

    nimcp_platform_mutex_lock(system->mutex);
    system->column_states[column_id].activation_threshold = threshold;
    nimcp_platform_mutex_unlock(system->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Loss Functions Implementation
 * ============================================================================ */

float cortical_sparse_compute_loss(
    cortical_sparse_coding_system_t* system,
    const float* input,
    const float* activations,
    uint32_t num_dims,
    const float* dictionary,
    float* reconstruction_error,
    float* sparsity_cost
) {
    /* Guard clause: validate parameters */
    if (!system || !input || !activations) {
        return -1.0f;
    }

    float recon_error = 0.0f;
    float sparse_cost = 0.0f;

    /* Compute reconstruction error if dictionary provided */
    if (dictionary && system->config.track_reconstruction_error) {
        /* Reconstruct: x_hat = W * a */
        float* x_hat = (float*)nimcp_calloc(num_dims, sizeof(float));
        if (x_hat) {
            for (uint32_t i = 0; i < num_dims; i++) {
                float sum = 0.0f;
                for (uint32_t j = 0; j < system->num_columns; j++) {
                    sum += dictionary[i * system->num_columns + j] * activations[j];
                }
                x_hat[i] = sum;
            }

            /* Compute ||x - x_hat||² */
            for (uint32_t i = 0; i < num_dims; i++) {
                float diff = input[i] - x_hat[i];
                recon_error += diff * diff;
            }

            nimcp_free(x_hat);
        }
    }

    /* Compute sparsity cost (L1 norm) */
    sparse_cost = compute_l1_norm(activations, system->num_columns);
    sparse_cost *= system->config.sparsity_penalty;

    /* Total loss */
    float total_loss = system->config.reconstruction_weight * recon_error + sparse_cost;

    /* Update state */
    nimcp_platform_mutex_lock(system->mutex);
    system->state.reconstruction_error = recon_error;
    system->state.sparsity_loss = sparse_cost;
    system->state.total_loss = total_loss;
    system->stats.reconstruction_error = recon_error;
    system->stats.mean_reconstruction_error =
        (system->stats.mean_reconstruction_error * 0.99f) + (recon_error * 0.01f);
    nimcp_platform_mutex_unlock(system->mutex);

    /* Output components if requested */
    if (reconstruction_error) *reconstruction_error = recon_error;
    if (sparsity_cost) *sparsity_cost = sparse_cost;

    return total_loss;
}

/* ============================================================================
 * Active Set Operations Implementation
 * ============================================================================ */

int cortical_sparse_get_active_set(
    cortical_sparse_coding_system_t* system,
    const float* activations,
    uint32_t num_activations,
    uint32_t* active_indices,
    uint32_t max_active,
    uint32_t* num_active
) {
    /* Guard clause: validate parameters */
    NIMCP_CHECK_THROW(system != NULL && activations != NULL && active_indices != NULL && num_active != NULL,
                      NIMCP_ERROR_NULL_POINTER, "NULL parameter in get_active_set");
    NIMCP_CHECK_THROW(num_activations == system->num_columns,
                      NIMCP_ERROR_INVALID_PARAM, "num_activations mismatch");

    uint32_t count = 0;
    for (uint32_t i = 0; i < num_activations && count < max_active; i++) {
        if (activations[i] > system->column_states[i].activation_threshold) {
            active_indices[count++] = i;
        }
    }

    *num_active = count;
    return NIMCP_SUCCESS;
}

int cortical_sparse_get_active_values(
    cortical_sparse_coding_system_t* system,
    const float* activations,
    uint32_t num_activations,
    float* active_values,
    uint32_t max_active,
    uint32_t* num_active
) {
    /* Guard clause: validate parameters */
    NIMCP_CHECK_THROW(system != NULL && activations != NULL && active_values != NULL && num_active != NULL,
                      NIMCP_ERROR_NULL_POINTER, "NULL parameter in get_active_values");
    NIMCP_CHECK_THROW(num_activations == system->num_columns,
                      NIMCP_ERROR_INVALID_PARAM, "num_activations mismatch");

    uint32_t count = 0;
    for (uint32_t i = 0; i < num_activations && count < max_active; i++) {
        if (activations[i] > system->column_states[i].activation_threshold) {
            active_values[count++] = activations[i];
        }
    }

    *num_active = count;
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Statistics Implementation
 * ============================================================================ */

int cortical_sparse_get_stats(
    cortical_sparse_coding_system_t* system,
    sparse_coding_stats_t* stats
) {
    /* Guard clause: validate parameters */
    NIMCP_CHECK_THROW(system != NULL && stats != NULL,
                      NIMCP_ERROR_NULL_POINTER, "NULL parameter in get_stats");

    nimcp_platform_mutex_lock(system->mutex);

    /* Compute threshold statistics */
    float threshold_sum = 0.0f;
    float threshold_min = FLT_MAX;
    float threshold_max = -FLT_MAX;

    for (uint32_t i = 0; i < system->num_columns; i++) {
        float t = system->column_states[i].activation_threshold;
        threshold_sum += t;
        if (t < threshold_min) threshold_min = t;
        if (t > threshold_max) threshold_max = t;
    }

    system->stats.mean_threshold = threshold_sum / (float)system->num_columns;
    system->stats.min_threshold = threshold_min;
    system->stats.max_threshold = threshold_max;

    /* Copy statistics */
    memcpy(stats, &system->stats, sizeof(sparse_coding_stats_t));

    nimcp_platform_mutex_unlock(system->mutex);

    return NIMCP_SUCCESS;
}

int cortical_sparse_get_state(
    cortical_sparse_coding_system_t* system,
    sparse_coding_state_t* state
) {
    /* Guard clause: validate parameters */
    NIMCP_CHECK_THROW(system != NULL && state != NULL,
                      NIMCP_ERROR_NULL_POINTER, "NULL parameter in get_state");

    nimcp_platform_mutex_lock(system->mutex);
    memcpy(state, &system->state, sizeof(sparse_coding_state_t));
    nimcp_platform_mutex_unlock(system->mutex);

    return NIMCP_SUCCESS;
}

int cortical_sparse_get_column_state(
    cortical_sparse_coding_system_t* system,
    uint32_t column_id,
    column_sparse_state_t* state
) {
    /* Guard clause: validate parameters */
    NIMCP_CHECK_THROW(system != NULL && state != NULL,
                      NIMCP_ERROR_NULL_POINTER, "NULL parameter in get_column_state");
    NIMCP_CHECK_THROW(column_id < system->num_columns,
                      NIMCP_ERROR_INVALID_PARAM, "column_id out of range");

    nimcp_platform_mutex_lock(system->mutex);
    memcpy(state, &system->column_states[column_id], sizeof(column_sparse_state_t));
    nimcp_platform_mutex_unlock(system->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bio-async Integration Implementation
 * ============================================================================ */

int cortical_sparse_connect_bio_async(
    cortical_sparse_coding_system_t* system
) {
    /* Guard clause: validate parameters */
    NIMCP_CHECK_THROW(system != NULL, NIMCP_ERROR_NULL_POINTER, "system is NULL");
    NIMCP_CHECK_THROW(!system->bio_async_enabled,
                      NIMCP_ERROR_INVALID_TYPE, "bio_async already enabled");

    /* Register with bio-async router */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_CORTICAL_SPARSE,
        .module_name = "cortical_sparse_coding",
        .inbox_capacity = 32,
        .user_data = system
    };

    system->bio_ctx = bio_router_register_module(&info);
    if (system->bio_ctx) {
        system->bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
        return NIMCP_SUCCESS;
    }

    NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
    return NIMCP_SUCCESS;
}

int cortical_sparse_disconnect_bio_async(
    cortical_sparse_coding_system_t* system
) {
    /* Guard clause: validate parameters */
    NIMCP_CHECK_THROW(system != NULL, NIMCP_ERROR_NULL_POINTER, "system is NULL");
    if (!system->bio_async_enabled) {
        return NIMCP_SUCCESS;  /* Already disconnected */
    }

    /* Unregister from bio-async router */
    if (system->bio_ctx) {
        bio_router_unregister_module(system->bio_ctx);
        system->bio_ctx = NULL;
    }

    system->bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Disconnected from bio-async router");

    return NIMCP_SUCCESS;
}

bool cortical_sparse_is_bio_async_connected(
    const cortical_sparse_coding_system_t* system
) {
    /* Guard clause: validate parameter */
    if (!system) {
        return false;
    }

    return system->bio_async_enabled;
}

/* ============================================================================
 * Ternary Sparse Coding API (NIMCP 2.10)
 * ============================================================================ */

/**
 * @brief Quantize activations to ternary sparse code
 */
int cortical_sparse_quantize_to_ternary(
    cortical_sparse_coding_system_t* system,
    const float* activations,
    uint32_t num_activations,
    trit_t* ternary_output
) {
    /* Guard clause: validate parameters */
    NIMCP_CHECK_THROW(system != NULL && activations != NULL && ternary_output != NULL,
                      NIMCP_ERROR_NULL_POINTER, "NULL parameter in quantize_to_ternary");
    NIMCP_CHECK_THROW(num_activations > 0,
                      NIMCP_ERROR_INVALID, "num_activations is zero");

    float pos_thresh = system->config.ternary_positive_threshold;
    float neg_thresh = system->config.ternary_negative_threshold;

    /* Quantize each activation to ternary */
    for (uint32_t i = 0; i < num_activations; i++) {
        if (activations[i] >= pos_thresh) {
            ternary_output[i] = TRIT_POSITIVE;
        } else if (activations[i] <= -neg_thresh) {
            ternary_output[i] = TRIT_NEGATIVE;
        } else {
            ternary_output[i] = TRIT_UNKNOWN;
        }
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Dequantize ternary sparse code to continuous
 */
int cortical_sparse_dequantize_from_ternary(
    cortical_sparse_coding_system_t* system,
    const trit_t* ternary_input,
    uint32_t num_activations,
    float scale,
    float* continuous_output
) {
    /* Guard clause: validate parameters */
    NIMCP_CHECK_THROW(system != NULL && ternary_input != NULL && continuous_output != NULL,
                      NIMCP_ERROR_NULL_POINTER, "NULL parameter in dequantize_from_ternary");
    NIMCP_CHECK_THROW(num_activations > 0,
                      NIMCP_ERROR_INVALID, "num_activations is zero");

    /* Dequantize each ternary value to continuous */
    for (uint32_t i = 0; i < num_activations; i++) {
        continuous_output[i] = (float)ternary_input[i] * scale;
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Enforce sparsity and return ternary output
 */
int cortical_sparse_enforce_sparsity_ternary(
    cortical_sparse_coding_system_t* system,
    const float* activations,
    uint32_t num_activations,
    trit_t* ternary_output
) {
    /* Guard clause: validate parameters */
    NIMCP_CHECK_THROW(system != NULL && activations != NULL && ternary_output != NULL,
                      NIMCP_ERROR_NULL_POINTER, "NULL parameter in enforce_sparsity_ternary");
    NIMCP_CHECK_THROW(num_activations > 0,
                      NIMCP_ERROR_INVALID, "num_activations is zero");

    /* First apply sparsity enforcement to get sparse continuous activations */
    float* sparse_activations = nimcp_malloc(num_activations * sizeof(float));
    NIMCP_CHECK_THROW(sparse_activations != NULL, NIMCP_ERROR_MEMORY,
                      "Failed to allocate sparse_activations");

    /* Copy activations for modification */
    memcpy(sparse_activations, activations, num_activations * sizeof(float));

    /* Apply sparsity based on method */
    switch (system->config.sparsity_method) {
        case SPARSITY_METHOD_K_WTA: {
            /* K-WTA: Keep only top k activations */
            uint32_t k = system->config.k_winners;
            if (k > num_activations) k = num_activations;

            /* Find k-th largest value (simple approach) */
            float threshold = 0.0f;
            for (uint32_t pass = 0; pass < k; pass++) {
                float max_val = -1e30f;
                uint32_t max_idx = 0;
                for (uint32_t i = 0; i < num_activations; i++) {
                    if (sparse_activations[i] > max_val) {
                        max_val = sparse_activations[i];
                        max_idx = i;
                    }
                }
                if (pass == k - 1) {
                    threshold = max_val;
                }
                sparse_activations[max_idx] = -1e30f;  /* Mark as processed */
            }

            /* Restore and threshold */
            memcpy(sparse_activations, activations, num_activations * sizeof(float));
            for (uint32_t i = 0; i < num_activations; i++) {
                if (sparse_activations[i] < threshold) {
                    sparse_activations[i] = 0.0f;
                }
            }
            break;
        }

        case SPARSITY_METHOD_THRESHOLD:
        default: {
            /* Simple threshold */
            float thresh = system->config.target_sparsity;
            for (uint32_t i = 0; i < num_activations; i++) {
                if (fabsf(sparse_activations[i]) < thresh) {
                    sparse_activations[i] = 0.0f;
                }
            }
            break;
        }
    }

    /* Now quantize to ternary */
    int result = cortical_sparse_quantize_to_ternary(
        system, sparse_activations, num_activations, ternary_output
    );

    nimcp_free(sparse_activations);
    return result;
}

/**
 * @brief Get ternary active set indices
 */
int cortical_sparse_get_ternary_active_set(
    const trit_t* ternary_activations,
    uint32_t num_activations,
    uint32_t* active_indices,
    uint32_t max_active,
    uint32_t* num_active
) {
    /* Guard clause: validate parameters */
    NIMCP_CHECK_THROW(ternary_activations != NULL && active_indices != NULL && num_active != NULL,
                      NIMCP_ERROR_NULL_POINTER, "NULL parameter in get_ternary_active_set");

    uint32_t count = 0;
    for (uint32_t i = 0; i < num_activations && count < max_active; i++) {
        if (ternary_activations[i] != TRIT_UNKNOWN) {
            active_indices[count++] = i;
        }
    }

    *num_active = count;
    return NIMCP_SUCCESS;
}

/**
 * @brief Compute ternary sparse code sparsity
 */
float cortical_sparse_compute_ternary_sparsity(
    const trit_t* ternary_activations,
    uint32_t num_activations
) {
    if (!ternary_activations || num_activations == 0) {
        return 0.0f;
    }

    uint32_t zero_count = 0;
    for (uint32_t i = 0; i < num_activations; i++) {
        if (ternary_activations[i] == TRIT_UNKNOWN) {
            zero_count++;
        }
    }

    return (float)zero_count / (float)num_activations;
}

/**
 * @brief Get ternary activation distribution
 */
void cortical_sparse_get_ternary_distribution(
    const trit_t* ternary_activations,
    uint32_t num_activations,
    uint32_t* n_negative,
    uint32_t* n_zero,
    uint32_t* n_positive
) {
    uint32_t neg = 0, zero = 0, pos = 0;

    if (ternary_activations) {
        for (uint32_t i = 0; i < num_activations; i++) {
            switch (ternary_activations[i]) {
                case TRIT_POSITIVE:
                    pos++;
                    break;
                case TRIT_NEGATIVE:
                    neg++;
                    break;
                default:
                    zero++;
                    break;
            }
        }
    }

    if (n_negative) *n_negative = neg;
    if (n_zero) *n_zero = zero;
    if (n_positive) *n_positive = pos;
}

/**
 * @brief Create packed ternary vector from activations
 */
trit_vector_t* cortical_sparse_create_ternary_vector(
    cortical_sparse_coding_system_t* system,
    const float* activations,
    uint32_t num_activations
) {
    if (!system || !activations || num_activations == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_sparse_create_ternary_vector: required parameter is NULL (system, activations)");
        return NULL;
    }

    /* Create ternary vector with configured pack mode */
    trit_vector_t* vec = trit_vector_create(
        num_activations,
        system->config.ternary_pack_mode
    );
    if (!vec) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cortical_sparse_create_ternary_vector: trit_vector_create failed");

        return NULL;
    }

    /* Quantize and store activations */
    float pos_thresh = system->config.ternary_positive_threshold;
    float neg_thresh = system->config.ternary_negative_threshold;

    for (uint32_t i = 0; i < num_activations; i++) {
        trit_t trit;
        if (activations[i] >= pos_thresh) {
            trit = TRIT_POSITIVE;
        } else if (activations[i] <= -neg_thresh) {
            trit = TRIT_NEGATIVE;
        } else {
            trit = TRIT_UNKNOWN;
        }
        trit_vector_set(vec, i, trit);
    }

    return vec;
}

/**
 * @brief Ternary sparse dot product
 */
float cortical_sparse_ternary_dot(
    const trit_t* ternary_code,
    uint32_t num_elements,
    const float* values
) {
    if (!ternary_code || !values) {
        return 0.0f;
    }

    float sum = 0.0f;
    for (uint32_t i = 0; i < num_elements; i++) {
        /* ternary_code[i] is -1, 0, or +1 */
        sum += values[i] * (float)ternary_code[i];
    }

    return sum;
}

/**
 * @brief Enable ternary mode for sparse coding system
 */
int cortical_sparse_enable_ternary_mode(
    cortical_sparse_coding_system_t* system,
    float positive_threshold,
    float negative_threshold,
    ternary_pack_mode_t pack_mode
) {
    NIMCP_CHECK_THROW(system != NULL, NIMCP_ERROR_NULL_POINTER, "system is NULL");

    system->config.enable_ternary_coefficients = true;
    system->config.ternary_positive_threshold = positive_threshold;
    system->config.ternary_negative_threshold = negative_threshold;
    system->config.ternary_pack_mode = pack_mode;

    NIMCP_LOGGING_INFO("Ternary mode enabled (pos_thresh=%.3f, neg_thresh=%.3f)",
                       positive_threshold, negative_threshold);

    return NIMCP_SUCCESS;
}

/**
 * @brief Disable ternary mode for sparse coding system
 */
int cortical_sparse_disable_ternary_mode(
    cortical_sparse_coding_system_t* system
) {
    NIMCP_CHECK_THROW(system != NULL, NIMCP_ERROR_NULL_POINTER, "system is NULL");

    system->config.enable_ternary_coefficients = false;

    NIMCP_LOGGING_INFO("Ternary mode disabled");

    return NIMCP_SUCCESS;
}

/**
 * @brief Check if ternary mode is enabled
 */
bool cortical_sparse_is_ternary_mode(
    const cortical_sparse_coding_system_t* system
) {
    if (!system) {
        return false;
    }

    return system->config.enable_ternary_coefficients;
}
