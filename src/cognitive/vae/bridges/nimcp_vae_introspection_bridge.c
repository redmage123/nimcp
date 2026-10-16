/**
 * @file nimcp_vae_introspection_bridge.c
 * @brief Implementation of VAE-Introspection bridge for internal state encoding
 * @version 1.0.0
 * @date 2026-01-30
 */

#include "cognitive/vae/bridges/nimcp_vae_introspection_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/tensor/nimcp_tensor_internal.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "utils/thread/nimcp_thread_rand.h"

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static uint64_t get_time_us(void) {
    return nimcp_time_now_us();
}

/**
 * @brief Convert brain state to flat vector for VAE encoding
 */
static int flatten_brain_state(const vae_intro_brain_state_t* state,
                                float* vector, uint32_t max_dim,
                                uint32_t* actual_dim) {
    uint32_t idx = 0;

    /* Global metrics (4 values) */
    if (idx + 4 <= max_dim) {
        vector[idx++] = state->global_activity;
        vector[idx++] = state->cognitive_load;
        vector[idx++] = state->attention_level;
        vector[idx++] = state->arousal_level;
    }

    /* Uncertainty estimates (2 values) */
    if (idx + 2 <= max_dim) {
        vector[idx++] = state->epistemic_uncertainty;
        vector[idx++] = state->aleatoric_uncertainty;
    }

    /* Module states */
    for (uint32_t m = 0; m < state->num_modules && idx + 4 <= max_dim; m++) {
        vector[idx++] = state->modules[m].activity_level;
        vector[idx++] = state->modules[m].resource_usage;
        vector[idx++] = (float)state->modules[m].active_neurons / 10000.0f;  /* Normalize */
        vector[idx++] = state->modules[m].avg_firing_rate / 100.0f;  /* Normalize Hz */
    }

    /* Pattern states */
    for (uint32_t p = 0; p < state->num_patterns && idx + 2 <= max_dim; p++) {
        vector[idx++] = state->patterns[p].activation;
        vector[idx++] = state->patterns[p].is_active ? 1.0f : 0.0f;
    }

    *actual_dim = idx;
    return 0;
}

/**
 * @brief Reconstruct brain state from flat vector
 */
static int unflatten_to_brain_state(const float* vector, uint32_t dim,
                                     vae_intro_brain_state_t* state) {
    uint32_t idx = 0;

    /* Global metrics */
    if (idx + 4 <= dim) {
        state->global_activity = vector[idx++];
        state->cognitive_load = vector[idx++];
        state->attention_level = vector[idx++];
        state->arousal_level = vector[idx++];
    }

    /* Uncertainty */
    if (idx + 2 <= dim) {
        state->epistemic_uncertainty = vector[idx++];
        state->aleatoric_uncertainty = vector[idx++];
    }

    /* Modules would be reconstructed if allocated */
    /* Patterns would be reconstructed if allocated */

    return 0;
}

/**
 * @brief Compute uncertainty from latent variance
 */
static float compute_uncertainty_from_variance(const float* variance, uint32_t dim) {
    if (!variance || dim == 0) return 0.0f;

    float total_var = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        total_var += variance[i];
    }

    /* Convert mean variance to uncertainty [0, 1] */
    float mean_var = total_var / (float)dim;
    float uncertainty = 1.0f - expf(-mean_var);

    return uncertainty;
}

/**
 * @brief Update latent history buffer
 */
static void update_history(vae_intro_bridge_t* bridge, const float* latent) {
    if (!bridge->config.enable_history || !bridge->latent_history) return;

    uint32_t dim = bridge->latent_dim;
    uint32_t hist_len = bridge->config.history_length;

    /* Copy to history buffer */
    uint32_t head = bridge->history_head;
    memcpy(bridge->latent_history[head], latent, dim * sizeof(float));

    /* Advance head */
    bridge->history_head = (head + 1) % hist_len;
    if (bridge->history_count < hist_len) {
        bridge->history_count++;
    }
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int vae_intro_bridge_default_config(vae_intro_bridge_config_t* config) {
    if (!config) return NIMCP_ERROR_VAE_INTRO_NULL;

    memset(config, 0, sizeof(*config));

    config->default_strategy = VAE_INTRO_STRATEGY_BALANCED;
    config->default_focus = VAE_INTRO_FOCUS_GLOBAL;

    config->sampling_rate = 10.0f;  /* 10 Hz */
    config->max_neurons_sampled = 10000;
    config->track_temporal_dynamics = true;

    config->module_latent_dim = 8;
    config->pattern_latent_dim = 4;
    config->global_latent_dim = 16;

    config->compute_uncertainty = true;
    config->uncertainty_threshold = 0.7f;

    config->enable_history = true;
    config->history_length = 100;

    config->enable_logging = false;

    return 0;
}

vae_intro_bridge_t* vae_intro_bridge_create(const vae_intro_bridge_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vae_intro_bridge_create: config is NULL");
        return NULL;
    }

    vae_intro_bridge_t* bridge = nimcp_calloc(1, sizeof(vae_intro_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vae_intro_bridge_create: bridge is NULL");
        return NULL;
    }

    bridge->config = *config;
    bridge->state = VAE_INTRO_STATE_DISCONNECTED;
    bridge->creation_time_us = get_time_us();

    bridge->stats.creation_time_us = bridge->creation_time_us;

    bridge->is_initialized = true;

    if (config->enable_logging) {
        LOG_INFO("VAE-Introspection bridge created (strategy=%d, focus=%d)",
                 config->default_strategy, config->default_focus);
    }

    return bridge;
}

void vae_intro_bridge_destroy(vae_intro_bridge_t* bridge) {
    if (!bridge) return;

    /* Free current state allocations */
    if (bridge->current_state.modules) {
        nimcp_free(bridge->current_state.modules);
    }
    if (bridge->current_state.patterns) {
        nimcp_free(bridge->current_state.patterns);
    }

    if (bridge->current_latent) {
        nimcp_free(bridge->current_latent);
    }

    /* Free history */
    if (bridge->latent_history) {
        for (uint32_t i = 0; i < bridge->config.history_length; i++) {
            if (bridge->latent_history[i]) {
                nimcp_free(bridge->latent_history[i]);
            }
        }
        nimcp_free(bridge->latent_history);
    }

    if (bridge->encode_buffer) {
        nimcp_free(bridge->encode_buffer);
    }
    if (bridge->variance_buffer) {
        nimcp_free(bridge->variance_buffer);
    }

    nimcp_free(bridge);
    bridge = NULL;
}

int vae_intro_bridge_connect_vae(vae_intro_bridge_t* bridge, vae_system_t* vae) {
    if (!bridge || !vae) return NIMCP_ERROR_VAE_INTRO_NULL;

    bridge->vae = vae;

    uint32_t latent_dim = vae_get_latent_dim(vae);
    bridge->latent_dim = latent_dim;

    /* Allocate working buffers */
    bridge->current_latent = nimcp_calloc(latent_dim, sizeof(float));
    if (!bridge->current_latent) return NIMCP_ERROR_VAE_INTRO_NO_MEMORY;

    uint32_t max_input_dim = 6 +  /* Global metrics + uncertainty */
                              VAE_INTRO_MAX_MODULES * 4 +
                              VAE_INTRO_MAX_PATTERNS * 2;

    bridge->encode_buffer = nimcp_calloc(max_input_dim, sizeof(float));
    if (!bridge->encode_buffer) return NIMCP_ERROR_VAE_INTRO_NO_MEMORY;

    bridge->variance_buffer = nimcp_calloc(latent_dim, sizeof(float));
    if (!bridge->variance_buffer) return NIMCP_ERROR_VAE_INTRO_NO_MEMORY;

    /* Allocate history */
    if (bridge->config.enable_history) {
        bridge->latent_history = nimcp_calloc(bridge->config.history_length,
                                               sizeof(float*));
        if (!bridge->latent_history) return NIMCP_ERROR_VAE_INTRO_NO_MEMORY;

        for (uint32_t i = 0; i < bridge->config.history_length; i++) {
            bridge->latent_history[i] = nimcp_calloc(latent_dim, sizeof(float));
            if (!bridge->latent_history[i]) return NIMCP_ERROR_VAE_INTRO_NO_MEMORY;
        }
        bridge->history_head = 0;
        bridge->history_count = 0;
    }

    if (bridge->introspection_ctx) {
        bridge->state = VAE_INTRO_STATE_CONNECTED;
    }

    if (bridge->config.enable_logging) {
        LOG_INFO("VAE-Introspection bridge connected to VAE (latent_dim=%u)", latent_dim);
    }

    return 0;
}

int vae_intro_bridge_connect_introspection(vae_intro_bridge_t* bridge, void* introspection) {
    if (!bridge) return NIMCP_ERROR_VAE_INTRO_NULL;

    bridge->introspection_ctx = introspection;

    if (bridge->vae) {
        bridge->state = VAE_INTRO_STATE_CONNECTED;
    }

    return 0;
}

int vae_intro_bridge_disconnect(vae_intro_bridge_t* bridge) {
    if (!bridge) return NIMCP_ERROR_VAE_INTRO_NULL;

    bridge->state = VAE_INTRO_STATE_DISCONNECTED;

    return 0;
}

bool vae_intro_bridge_is_connected(const vae_intro_bridge_t* bridge) {
    return bridge && bridge->state == VAE_INTRO_STATE_CONNECTED;
}

/* ============================================================================
 * Sampling API
 * ============================================================================ */

int vae_intro_sample_state(vae_intro_bridge_t* bridge,
                            vae_intro_strategy_t strategy,
                            vae_intro_focus_t focus,
                            vae_intro_brain_state_t* state) {
    if (!bridge || !state) return NIMCP_ERROR_VAE_INTRO_NULL;

    uint64_t start_time = get_time_us();
    bridge->state = VAE_INTRO_STATE_SAMPLING;

    memset(state, 0, sizeof(*state));

    /* Determine sampling parameters based on strategy */
    float sample_fraction = 0.0f;
    switch (strategy) {
        case VAE_INTRO_STRATEGY_FAST:
            sample_fraction = 0.1f;
            break;
        case VAE_INTRO_STRATEGY_DETAILED:
            sample_fraction = 1.0f;
            break;
        default:  /* BALANCED */
            sample_fraction = 0.3f;
            break;
    }

    (void)sample_fraction;  /* Would be used with real introspection system */
    (void)focus;

    /* In a real implementation, this would query the introspection system */
    /* For now, create simulated brain state */

    /* Global metrics - simulated values */
    state->global_activity = 0.4f + 0.2f * ((float)nimcp_tl_rand() / RAND_MAX);
    state->cognitive_load = 0.3f + 0.3f * ((float)nimcp_tl_rand() / RAND_MAX);
    state->attention_level = 0.5f + 0.3f * ((float)nimcp_tl_rand() / RAND_MAX);
    state->arousal_level = 0.4f + 0.2f * ((float)nimcp_tl_rand() / RAND_MAX);

    /* Uncertainty estimates */
    state->epistemic_uncertainty = 0.2f + 0.3f * ((float)nimcp_tl_rand() / RAND_MAX);
    state->aleatoric_uncertainty = 0.1f + 0.2f * ((float)nimcp_tl_rand() / RAND_MAX);

    /* Allocate and populate module states */
    uint32_t num_modules = 8;  /* Simulated */
    state->modules = nimcp_calloc(num_modules, sizeof(vae_intro_module_state_t));
    if (state->modules) {
        state->num_modules = num_modules;
        for (uint32_t i = 0; i < num_modules; i++) {
            state->modules[i].module_id = i;
            state->modules[i].module_name = NULL;  /* Would be set from introspection */
            state->modules[i].activity_level = 0.3f + 0.4f * ((float)nimcp_tl_rand() / RAND_MAX);
            state->modules[i].resource_usage = 0.2f + 0.3f * ((float)nimcp_tl_rand() / RAND_MAX);
            state->modules[i].active_neurons = 100 + (nimcp_tl_rand() % 900);
            state->modules[i].avg_firing_rate = 10.0f + 20.0f * ((float)nimcp_tl_rand() / RAND_MAX);
        }
    }

    /* Allocate and populate pattern states */
    uint32_t num_patterns = 16;  /* Simulated */
    state->patterns = nimcp_calloc(num_patterns, sizeof(vae_intro_pattern_state_t));
    if (state->patterns) {
        state->num_patterns = num_patterns;
        for (uint32_t i = 0; i < num_patterns; i++) {
            state->patterns[i].pattern_id = i;
            state->patterns[i].pattern_name = NULL;
            state->patterns[i].activation = ((float)nimcp_tl_rand() / RAND_MAX);
            state->patterns[i].is_active = state->patterns[i].activation > 0.5f;
            state->patterns[i].last_active_time_us = get_time_us() - (nimcp_tl_rand() % 1000000);
        }
    }

    state->snapshot_time_us = get_time_us();
    state->sampling_duration_us = (float)(get_time_us() - start_time);

    bridge->stats.total_samples++;
    bridge->stats.avg_global_activity =
        (bridge->stats.avg_global_activity * (bridge->stats.total_samples - 1) +
         state->global_activity) / bridge->stats.total_samples;
    bridge->stats.avg_cognitive_load =
        (bridge->stats.avg_cognitive_load * (bridge->stats.total_samples - 1) +
         state->cognitive_load) / bridge->stats.total_samples;
    bridge->stats.last_sample_us = get_time_us();

    bridge->state = VAE_INTRO_STATE_CONNECTED;

    return 0;
}

int vae_intro_sample_current(vae_intro_bridge_t* bridge) {
    if (!bridge) return NIMCP_ERROR_VAE_INTRO_NULL;

    /* Free previous state */
    if (bridge->current_state.modules) {
        nimcp_free(bridge->current_state.modules);
        bridge->current_state.modules = NULL;
    }
    if (bridge->current_state.patterns) {
        nimcp_free(bridge->current_state.patterns);
        bridge->current_state.patterns = NULL;
    }

    return vae_intro_sample_state(bridge,
                                   bridge->config.default_strategy,
                                   bridge->config.default_focus,
                                   &bridge->current_state);
}

/* ============================================================================
 * Encoding API
 * ============================================================================ */

int vae_intro_encode_state(vae_intro_bridge_t* bridge,
                            const vae_intro_brain_state_t* state,
                            vae_intro_encode_result_t* result) {
    if (!bridge || !state || !result) return NIMCP_ERROR_VAE_INTRO_NULL;
    if (bridge->state != VAE_INTRO_STATE_CONNECTED) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_INTRO_NOT_CONNECTED, "vae_introspection_bridge: error condition");
        return NIMCP_ERROR_VAE_INTRO_NOT_CONNECTED;
    }

    uint64_t start_time = get_time_us();
    bridge->state = VAE_INTRO_STATE_ENCODING;

    memset(result, 0, sizeof(*result));

    /* Flatten brain state to vector */
    uint32_t max_dim = 6 + VAE_INTRO_MAX_MODULES * 4 + VAE_INTRO_MAX_PATTERNS * 2;
    uint32_t actual_dim = 0;
    flatten_brain_state(state, bridge->encode_buffer, max_dim, &actual_dim);

    /* Encode with VAE using tensor API */
    uint32_t latent_dim = bridge->latent_dim;
    nimcp_tensor_t* input_tensor = nimcp_tensor_create(&actual_dim, 1, NIMCP_DTYPE_F32);
    nimcp_tensor_t* mu_tensor = nimcp_tensor_create(&latent_dim, 1, NIMCP_DTYPE_F32);
    nimcp_tensor_t* log_var_tensor = nimcp_tensor_create(&latent_dim, 1, NIMCP_DTYPE_F32);

    if (!input_tensor || !mu_tensor || !log_var_tensor) {
        nimcp_tensor_destroy(input_tensor);
        nimcp_tensor_destroy(mu_tensor);
        nimcp_tensor_destroy(log_var_tensor);
        bridge->state = VAE_INTRO_STATE_CONNECTED;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_INTRO_NO_MEMORY, "vae_introspection_bridge: error condition");
        return NIMCP_ERROR_VAE_INTRO_NO_MEMORY;
    }

    memcpy(TENSOR_DATA_F32(input_tensor), bridge->encode_buffer, actual_dim * sizeof(float));

    int ret = vae_encode(bridge->vae, input_tensor, mu_tensor, log_var_tensor);

    if (ret != 0) {
        nimcp_tensor_destroy(input_tensor);
        nimcp_tensor_destroy(mu_tensor);
        nimcp_tensor_destroy(log_var_tensor);
        bridge->state = VAE_INTRO_STATE_CONNECTED;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_INTRO_ENCODE_FAILED, "vae_introspection_bridge: error condition");
        return NIMCP_ERROR_VAE_INTRO_ENCODE_FAILED;
    }

    /* Allocate result */
    result->latent = nimcp_calloc(latent_dim, sizeof(float));
    if (!result->latent) {
        nimcp_tensor_destroy(input_tensor);
        nimcp_tensor_destroy(mu_tensor);
        nimcp_tensor_destroy(log_var_tensor);
        bridge->state = VAE_INTRO_STATE_CONNECTED;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_INTRO_NO_MEMORY, "vae_introspection_bridge: error condition");
        return NIMCP_ERROR_VAE_INTRO_NO_MEMORY;
    }
    memcpy(result->latent, TENSOR_DATA_F32(mu_tensor), latent_dim * sizeof(float));
    result->latent_dim = latent_dim;

    /* Copy variance for uncertainty computation */
    result->variance = nimcp_calloc(latent_dim, sizeof(float));
    if (result->variance) {
        memcpy(result->variance, TENSOR_DATA_F32(log_var_tensor), latent_dim * sizeof(float));
        /* Convert log-variance to variance */
        for (uint32_t i = 0; i < latent_dim; i++) {
            result->variance[i] = expf(result->variance[i]);
        }
    }

    nimcp_tensor_destroy(input_tensor);
    nimcp_tensor_destroy(mu_tensor);
    nimcp_tensor_destroy(log_var_tensor);

    /* Compute uncertainty */
    if (bridge->config.compute_uncertainty && result->variance) {
        result->estimated_uncertainty = compute_uncertainty_from_variance(
            result->variance, latent_dim
        );
    }

    /* Copy original state reference */
    result->state = *state;
    result->state.modules = NULL;  /* Don't copy pointers */
    result->state.patterns = NULL;

    /* Update current latent and history */
    memcpy(bridge->current_latent, result->latent, latent_dim * sizeof(float));
    update_history(bridge, result->latent);

    result->encoding_time_us = get_time_us() - start_time;

    bridge->stats.total_encodes++;
    bridge->stats.avg_uncertainty =
        (bridge->stats.avg_uncertainty * (bridge->stats.total_encodes - 1) +
         result->estimated_uncertainty) / bridge->stats.total_encodes;
    bridge->stats.avg_encoding_latency_us =
        (bridge->stats.avg_encoding_latency_us * (bridge->stats.total_encodes - 1) +
         (float)result->encoding_time_us) / bridge->stats.total_encodes;

    bridge->state = VAE_INTRO_STATE_CONNECTED;

    return 0;
}

int vae_intro_encode_current(vae_intro_bridge_t* bridge,
                              vae_intro_encode_result_t* result) {
    if (!bridge || !result) return NIMCP_ERROR_VAE_INTRO_NULL;

    /* Sample current state first */
    int ret = vae_intro_sample_current(bridge);
    if (ret != 0) return ret;

    return vae_intro_encode_state(bridge, &bridge->current_state, result);
}

int vae_intro_encode_modules(vae_intro_bridge_t* bridge,
                              const vae_intro_module_state_t* modules,
                              uint32_t num_modules,
                              float* latent, uint32_t* latent_dim) {
    if (!bridge || !modules || !latent || !latent_dim) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_INTRO_NULL, "vae_introspection_bridge: error condition");
        return NIMCP_ERROR_VAE_INTRO_NULL;
    }
    if (bridge->state != VAE_INTRO_STATE_CONNECTED) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_INTRO_NOT_CONNECTED, "vae_introspection_bridge: error condition");
        return NIMCP_ERROR_VAE_INTRO_NOT_CONNECTED;
    }

    /* Create temporary brain state with just modules */
    vae_intro_brain_state_t state;
    memset(&state, 0, sizeof(state));
    state.modules = (vae_intro_module_state_t*)modules;  /* Const cast for read-only use */
    state.num_modules = num_modules;

    /* Flatten just module data */
    uint32_t max_dim = num_modules * 4;
    float* buffer = nimcp_calloc(max_dim, sizeof(float));
    if (!buffer) return NIMCP_ERROR_VAE_INTRO_NO_MEMORY;

    uint32_t idx = 0;
    for (uint32_t m = 0; m < num_modules; m++) {
        buffer[idx++] = modules[m].activity_level;
        buffer[idx++] = modules[m].resource_usage;
        buffer[idx++] = (float)modules[m].active_neurons / 10000.0f;
        buffer[idx++] = modules[m].avg_firing_rate / 100.0f;
    }

    /* Encode using tensor API */
    uint32_t dim = bridge->latent_dim;
    nimcp_tensor_t* input_tensor = nimcp_tensor_create(&idx, 1, NIMCP_DTYPE_F32);
    nimcp_tensor_t* mu_tensor = nimcp_tensor_create(&dim, 1, NIMCP_DTYPE_F32);
    nimcp_tensor_t* log_var_tensor = nimcp_tensor_create(&dim, 1, NIMCP_DTYPE_F32);

    if (!input_tensor || !mu_tensor || !log_var_tensor) {
        nimcp_tensor_destroy(input_tensor);
        nimcp_tensor_destroy(mu_tensor);
        nimcp_tensor_destroy(log_var_tensor);
        nimcp_free(buffer);
        buffer = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_INTRO_NO_MEMORY, "vae_introspection_bridge: error condition");
        return NIMCP_ERROR_VAE_INTRO_NO_MEMORY;
    }

    memcpy(TENSOR_DATA_F32(input_tensor), buffer, idx * sizeof(float));
    nimcp_free(buffer);
    buffer = NULL;

    int ret = vae_encode(bridge->vae, input_tensor, mu_tensor, log_var_tensor);

    if (ret != 0) {
        nimcp_tensor_destroy(input_tensor);
        nimcp_tensor_destroy(mu_tensor);
        nimcp_tensor_destroy(log_var_tensor);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_INTRO_ENCODE_FAILED, "vae_introspection_bridge: error condition");
        return NIMCP_ERROR_VAE_INTRO_ENCODE_FAILED;
    }

    memcpy(latent, TENSOR_DATA_F32(mu_tensor), dim * sizeof(float));
    *latent_dim = dim;

    nimcp_tensor_destroy(input_tensor);
    nimcp_tensor_destroy(mu_tensor);
    nimcp_tensor_destroy(log_var_tensor);

    return 0;
}

/* ============================================================================
 * Uncertainty API
 * ============================================================================ */

int vae_intro_compute_uncertainty(vae_intro_bridge_t* bridge,
                                   float* epistemic, float* aleatoric) {
    if (!bridge) return NIMCP_ERROR_VAE_INTRO_NULL;

    /* Epistemic uncertainty from latent variance */
    if (epistemic && bridge->variance_buffer) {
        *epistemic = compute_uncertainty_from_variance(
            bridge->variance_buffer, bridge->latent_dim
        );
    }

    /* Aleatoric from current state */
    if (aleatoric) {
        *aleatoric = bridge->current_state.aleatoric_uncertainty;
    }

    return 0;
}

int vae_intro_get_uncertainty_from_latent(vae_intro_bridge_t* bridge,
                                           const float* variance,
                                           uint32_t dim,
                                           float* uncertainty) {
    if (!bridge || !variance || !uncertainty) return NIMCP_ERROR_VAE_INTRO_NULL;

    *uncertainty = compute_uncertainty_from_variance(variance, dim);

    return 0;
}

/* ============================================================================
 * Prediction API
 * ============================================================================ */

int vae_intro_predict_next_state(vae_intro_bridge_t* bridge,
                                  uint32_t steps_ahead,
                                  vae_intro_predict_result_t* result) {
    if (!bridge || !result) return NIMCP_ERROR_VAE_INTRO_NULL;
    if (bridge->state != VAE_INTRO_STATE_CONNECTED) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_INTRO_NOT_CONNECTED, "vae_introspection_bridge: error condition");
        return NIMCP_ERROR_VAE_INTRO_NOT_CONNECTED;
    }

    uint64_t start_time = get_time_us();

    memset(result, 0, sizeof(*result));

    uint32_t latent_dim = bridge->latent_dim;

    /* Allocate trajectory buffer */
    result->latent_trajectory = nimcp_calloc(steps_ahead * latent_dim, sizeof(float));
    if (!result->latent_trajectory) return NIMCP_ERROR_VAE_INTRO_NO_MEMORY;
    result->trajectory_length = steps_ahead;

    /* Compute latent velocity from history */
    float* velocity = nimcp_calloc(latent_dim, sizeof(float));
    if (!velocity) {
        nimcp_free(result->latent_trajectory);
        result->latent_trajectory = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_INTRO_NO_MEMORY, "vae_introspection_bridge: error condition");
        return NIMCP_ERROR_VAE_INTRO_NO_MEMORY;
    }

    if (bridge->history_count >= 2) {
        /* Get two most recent latents */
        uint32_t latest_idx = (bridge->history_head + bridge->config.history_length - 1) %
                              bridge->config.history_length;
        uint32_t prev_idx = (latest_idx + bridge->config.history_length - 1) %
                            bridge->config.history_length;

        for (uint32_t i = 0; i < latent_dim; i++) {
            velocity[i] = bridge->latent_history[latest_idx][i] -
                         bridge->latent_history[prev_idx][i];
        }
    }

    /* Extrapolate trajectory */
    float* current = bridge->current_latent;
    for (uint32_t s = 0; s < steps_ahead; s++) {
        for (uint32_t i = 0; i < latent_dim; i++) {
            result->latent_trajectory[s * latent_dim + i] =
                current[i] + velocity[i] * (float)(s + 1);
        }
    }

    nimcp_free(velocity);
    velocity = NULL;

    /* Decode final predicted state */
    float* final_latent = &result->latent_trajectory[(steps_ahead - 1) * latent_dim];

    uint32_t output_dim = vae_get_output_dim(bridge->vae);
    nimcp_tensor_t* z_tensor = nimcp_tensor_create(&latent_dim, 1, NIMCP_DTYPE_F32);
    nimcp_tensor_t* output_tensor = nimcp_tensor_create(&output_dim, 1, NIMCP_DTYPE_F32);

    if (z_tensor && output_tensor) {
        memcpy(TENSOR_DATA_F32(z_tensor), final_latent, latent_dim * sizeof(float));
        int ret = vae_decode(bridge->vae, z_tensor, output_tensor);

        if (ret == 0) {
            unflatten_to_brain_state(TENSOR_DATA_F32(output_tensor),
                                      output_dim,
                                      &result->predicted_state);
        }
    }
    nimcp_tensor_destroy(z_tensor);
    nimcp_tensor_destroy(output_tensor);

    /* Confidence decreases with prediction horizon */
    result->prediction_confidence = expf(-0.1f * (float)steps_ahead);

    result->prediction_time_us = get_time_us() - start_time;

    return 0;
}

int vae_intro_extrapolate_trajectory(vae_intro_bridge_t* bridge,
                                      uint32_t num_steps,
                                      float* trajectory,
                                      uint32_t* trajectory_dim) {
    if (!bridge || !trajectory || !trajectory_dim) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_INTRO_NULL, "vae_introspection_bridge: error condition");
        return NIMCP_ERROR_VAE_INTRO_NULL;
    }

    vae_intro_predict_result_t result;
    int ret = vae_intro_predict_next_state(bridge, num_steps, &result);

    if (ret != 0) return ret;

    uint32_t latent_dim = bridge->latent_dim;
    memcpy(trajectory, result.latent_trajectory, num_steps * latent_dim * sizeof(float));
    *trajectory_dim = num_steps * latent_dim;

    vae_intro_predict_result_free(&result);

    return 0;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

vae_intro_bridge_state_t vae_intro_bridge_get_state(const vae_intro_bridge_t* bridge) {
    if (!bridge) return VAE_INTRO_STATE_DISCONNECTED;
    return bridge->state;
}

int vae_intro_bridge_get_stats(const vae_intro_bridge_t* bridge,
                                vae_intro_bridge_stats_t* stats) {
    if (!bridge || !stats) return NIMCP_ERROR_VAE_INTRO_NULL;

    *stats = bridge->stats;

    return 0;
}

int vae_intro_get_current_latent(const vae_intro_bridge_t* bridge,
                                  float* latent, uint32_t* dim) {
    if (!bridge || !latent || !dim) return NIMCP_ERROR_VAE_INTRO_NULL;

    memcpy(latent, bridge->current_latent, bridge->latent_dim * sizeof(float));
    *dim = bridge->latent_dim;

    return 0;
}

/* ============================================================================
 * Result Management
 * ============================================================================ */

void vae_intro_encode_result_free(vae_intro_encode_result_t* result) {
    if (!result) return;

    if (result->latent) {
        nimcp_free(result->latent);
        result->latent = NULL;
    }
    if (result->variance) {
        nimcp_free(result->variance);
        result->variance = NULL;
    }
}

void vae_intro_predict_result_free(vae_intro_predict_result_t* result) {
    if (!result) return;

    if (result->latent_trajectory) {
        nimcp_free(result->latent_trajectory);
        result->latent_trajectory = NULL;
    }

    /* Free predicted state allocations */
    if (result->predicted_state.modules) {
        nimcp_free(result->predicted_state.modules);
        result->predicted_state.modules = NULL;
    }
    if (result->predicted_state.patterns) {
        nimcp_free(result->predicted_state.patterns);
        result->predicted_state.patterns = NULL;
    }
}

void vae_intro_brain_state_free(vae_intro_brain_state_t* state) {
    if (!state) return;

    if (state->modules) {
        nimcp_free(state->modules);
        state->modules = NULL;
    }
    if (state->patterns) {
        nimcp_free(state->patterns);
        state->patterns = NULL;
    }
}
