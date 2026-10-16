/**
 * @file nimcp_cochlea_cortical_deep_bridge.c
 * @brief Deep bidirectional Cochlea-Cortical Columns integration implementation
 *
 * @author NIMCP Development Team
 * @date 2026
 * @version 3.0
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "perception/bridges/nimcp_cochlea_cortical_deep_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <time.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(cochlea_cortical_deep_bridge)

#define LOG_MODULE "COCHLEA_CORTICAL_DEEP_BRIDGE"

//=============================================================================
// Internal Structure
//=============================================================================

/** Default number of hypercolumns if not configured */
#define COCHLEA_CORTICAL_DEFAULT_HYPERCOLUMNS 64

/** Bridge internal structure */
struct cochlea_cortical_deep_bridge {
    bridge_base_t base;                         /**< MUST be first */
    cochlea_t* cochlea;                         /**< Cochlea instance */
    cortical_column_pool_t* pool;               /**< Cortical column pool */
    cochlea_cortical_deep_config_t config;     /**< Configuration */

    /* Bottom-up state */
    float* column_activations;                  /**< Per-column activation [num_columns] */
    float* prediction_errors;                   /**< Per-column prediction errors */
    uint32_t num_columns;                       /**< Total columns (hypercolumns) */

    /* Top-down state */
    float* attention_gain;                      /**< Per-column attention gain */
    float* expected_activation;                 /**< Predictive coding expectations */

    /* Synaptic weights (STDP) */
    float* weights;                             /**< Synaptic weights [num_columns] */
    uint32_t num_weights;

    /* Lateral inhibition state */
    float* lateral_excitation;                  /**< Lateral excitatory connections */
    float* lateral_inhibition;                  /**< Lateral inhibitory connections */

    /* Pre/post spike timing for STDP */
    float* pre_spike_time;                      /**< Pre-synaptic spike times */
    float* post_spike_time;                     /**< Post-synaptic spike times */

    /* Bidirectional timestamps */
    uint64_t last_outbound_ts;
    uint64_t last_inbound_ts;
};

//=============================================================================
// Helper: Current time in ms
//=============================================================================

static uint64_t cochlea_cortical_deep_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

//=============================================================================
// Configuration
//=============================================================================

cochlea_cortical_deep_config_t cochlea_cortical_deep_config_default(void) {
    cochlea_cortical_deep_config_t config;
    memset(&config, 0, sizeof(config));
    config.num_hypercolumns = COCHLEA_CORTICAL_DEFAULT_HYPERCOLUMNS;
    config.minicolumns_per_hypercolumn = COCHLEA_CORTICAL_MINICOLUMNS_PER;
    config.excitatory_radius_octaves = 0.25f;
    config.inhibitory_radius_octaves = 1.0f;
    config.excitatory_strength = 0.5f;
    config.inhibitory_strength = 0.3f;
    config.enable_plasticity = true;
    config.stdp_config.tau_plus_ms = 20.0f;
    config.stdp_config.tau_minus_ms = 20.0f;
    config.stdp_config.a_plus = 0.01f;
    config.stdp_config.a_minus = 0.012f;
    config.stdp_config.w_max = 1.0f;
    config.stdp_config.w_min = 0.0f;
    config.enable_top_down = true;
    config.top_down_gain_range = 2.0f;
    return config;
}

//=============================================================================
// Helper: Allocate/free arrays
//=============================================================================

static int cochlea_cortical_deep_alloc(cochlea_cortical_deep_bridge_t* bridge) {
    uint32_t n = bridge->config.num_hypercolumns;

    bridge->column_activations = (float*)nimcp_calloc(n, sizeof(float));
    bridge->prediction_errors = (float*)nimcp_calloc(n, sizeof(float));
    bridge->attention_gain = (float*)nimcp_calloc(n, sizeof(float));
    bridge->expected_activation = (float*)nimcp_calloc(n, sizeof(float));
    bridge->weights = (float*)nimcp_calloc(n, sizeof(float));
    bridge->lateral_excitation = (float*)nimcp_calloc(n, sizeof(float));
    bridge->lateral_inhibition = (float*)nimcp_calloc(n, sizeof(float));
    bridge->pre_spike_time = (float*)nimcp_calloc(n, sizeof(float));
    bridge->post_spike_time = (float*)nimcp_calloc(n, sizeof(float));

    bridge->num_columns = n;
    bridge->num_weights = n;

    if (!bridge->column_activations || !bridge->prediction_errors ||
        !bridge->attention_gain || !bridge->expected_activation ||
        !bridge->weights || !bridge->lateral_excitation ||
        !bridge->lateral_inhibition || !bridge->pre_spike_time ||
        !bridge->post_spike_time) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cochlea_cortical_deep_alloc: operation failed");
        return -1;
    }

    /* Initialize defaults */
    for (uint32_t i = 0; i < n; i++) {
        bridge->attention_gain[i] = 1.0f;
        bridge->weights[i] = 0.5f; /* Mid-range initial weight */
    }

    return 0;
}

static void cochlea_cortical_deep_free_arrays(cochlea_cortical_deep_bridge_t* bridge) {
    if (bridge->column_activations) { nimcp_free(bridge->column_activations); bridge->column_activations = NULL; }
    if (bridge->prediction_errors) { nimcp_free(bridge->prediction_errors); bridge->prediction_errors = NULL; }
    if (bridge->attention_gain) { nimcp_free(bridge->attention_gain); bridge->attention_gain = NULL; }
    if (bridge->expected_activation) { nimcp_free(bridge->expected_activation); bridge->expected_activation = NULL; }
    if (bridge->weights) { nimcp_free(bridge->weights); bridge->weights = NULL; }
    if (bridge->lateral_excitation) { nimcp_free(bridge->lateral_excitation); bridge->lateral_excitation = NULL; }
    if (bridge->lateral_inhibition) { nimcp_free(bridge->lateral_inhibition); bridge->lateral_inhibition = NULL; }
    if (bridge->pre_spike_time) { nimcp_free(bridge->pre_spike_time); bridge->pre_spike_time = NULL; }
    if (bridge->post_spike_time) { nimcp_free(bridge->post_spike_time); bridge->post_spike_time = NULL; }
}

//=============================================================================
// Core API
//=============================================================================

cochlea_cortical_deep_bridge_t* cochlea_cortical_deep_bridge_create(
    cochlea_t* cochlea,
    cortical_column_pool_t* pool,
    const cochlea_cortical_deep_config_t* config)
{
    cochlea_cortical_deep_bridge_t* bridge = (cochlea_cortical_deep_bridge_t*)nimcp_calloc(1, sizeof(cochlea_cortical_deep_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cochlea_cortical_deep_bridge_create: alloc failed");
        return NULL;
    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = cochlea_cortical_deep_config_default();
    }

    if (bridge_base_init(&bridge->base, 0, "cochlea_cortical_deep_bridge") != 0) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "cochlea_cortical_deep_bridge_create: validation failed");
        return NULL;
    }

    bridge->cochlea = cochlea;
    bridge->pool = pool;

    if (cochlea) {
        bridge_base_connect_a_unlocked(&bridge->base, cochlea);
    }
    if (pool) {
        bridge_base_connect_b_unlocked(&bridge->base, pool);
    }

    if (cochlea_cortical_deep_alloc(bridge) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cochlea_cortical_deep_bridge_create: array alloc failed");
        bridge_base_cleanup(&bridge->base);
        cochlea_cortical_deep_free_arrays(bridge);
        nimcp_free(bridge);
        return NULL;
    }

    cochlea_cortical_deep_bridge_heartbeat("create", 1.0f);
    return bridge;
}

void cochlea_cortical_deep_bridge_destroy(cochlea_cortical_deep_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "cochlea_cortical_deep");
    cochlea_cortical_deep_bridge_heartbeat("destroy", 0.0f);
    cochlea_cortical_deep_free_arrays(bridge);
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

nimcp_error_t cochlea_cortical_deep_bridge_update(
    cochlea_cortical_deep_bridge_t* bridge,
    const cochlea_output_t* cochlea_output,
    float dt_ms)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_cortical_deep_bridge_update: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!cochlea_output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_cortical_deep_bridge_update: cochlea_output is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    cochlea_cortical_deep_bridge_heartbeat("update", 0.1f);

    /* Process bottom-up first */
    nimcp_error_t result = cochlea_cortical_process_bottom_up(bridge, cochlea_output);
    if (result != NIMCP_SUCCESS) {
        return result;
    }

    /* Apply STDP if plasticity is enabled */
    if (bridge->config.enable_plasticity) {
        cochlea_cortical_apply_stdp(bridge, dt_ms);
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->last_outbound_ts = cochlea_cortical_deep_time_ms();
    bridge_base_record_update(&bridge->base);
    nimcp_mutex_unlock(bridge->base.mutex);

    cochlea_cortical_deep_bridge_heartbeat("update", 1.0f);
    return NIMCP_SUCCESS;
}

nimcp_error_t cochlea_cortical_deep_bridge_reset(cochlea_cortical_deep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_cortical_deep_bridge_reset: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    uint32_t n = bridge->num_columns;
    memset(bridge->column_activations, 0, n * sizeof(float));
    memset(bridge->prediction_errors, 0, n * sizeof(float));
    memset(bridge->expected_activation, 0, n * sizeof(float));
    memset(bridge->lateral_excitation, 0, n * sizeof(float));
    memset(bridge->lateral_inhibition, 0, n * sizeof(float));
    memset(bridge->pre_spike_time, 0, n * sizeof(float));
    memset(bridge->post_spike_time, 0, n * sizeof(float));

    for (uint32_t i = 0; i < n; i++) {
        bridge->attention_gain[i] = 1.0f;
        bridge->weights[i] = 0.5f;
    }

    bridge->last_outbound_ts = 0;
    bridge->last_inbound_ts = 0;

    bridge_base_reset_unlocked(&bridge->base);

    nimcp_mutex_unlock(bridge->base.mutex);

    cochlea_cortical_deep_bridge_heartbeat("reset", 1.0f);
    return NIMCP_SUCCESS;
}

//=============================================================================
// Bottom-Up Processing (Outbound)
//=============================================================================

nimcp_error_t cochlea_cortical_process_bottom_up(
    cochlea_cortical_deep_bridge_t* bridge,
    const cochlea_output_t* cochlea_output)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_cortical_process_bottom_up: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!cochlea_output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_cortical_process_bottom_up: cochlea_output is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    cochlea_cortical_deep_bridge_heartbeat("bottom_up", 0.3f);

    nimcp_mutex_lock(bridge->base.mutex);

    uint32_t n = bridge->num_columns;

    /* First pass: compute raw activations with weight modulation */
    for (uint32_t i = 0; i < n; i++) {
        /* Base activation from weighted input (placeholder for cochlea mapping) */
        float raw = bridge->weights[i] * bridge->attention_gain[i];
        bridge->column_activations[i] = raw;

        /* Compute prediction error */
        bridge->prediction_errors[i] = raw - bridge->expected_activation[i];

        /* Record pre-spike time (activity-based) */
        if (raw > 0.5f) {
            bridge->pre_spike_time[i] = 0.0f; /* Reset: just spiked */
        }
    }

    /* Second pass: apply lateral connectivity (Mexican hat) */
    for (uint32_t i = 0; i < n; i++) {
        float excitation = 0.0f;
        float inhibition = 0.0f;

        for (uint32_t j = 0; j < n; j++) {
            if (i == j) continue;

            /* Compute octave distance between columns */
            float fi = 20.0f + (float)i / (float)(n > 1 ? n - 1 : 1) * (20000.0f - 20.0f);
            float fj = 20.0f + (float)j / (float)(n > 1 ? n - 1 : 1) * (20000.0f - 20.0f);
            float octave_dist = fabsf(log2f(fi / fj));

            if (octave_dist <= bridge->config.excitatory_radius_octaves) {
                excitation += bridge->column_activations[j] * bridge->config.excitatory_strength;
            } else if (octave_dist <= bridge->config.inhibitory_radius_octaves) {
                inhibition += bridge->column_activations[j] * bridge->config.inhibitory_strength;
            }
        }

        bridge->lateral_excitation[i] = excitation;
        bridge->lateral_inhibition[i] = inhibition;
    }

    /* Third pass: apply lateral modulation */
    for (uint32_t i = 0; i < n; i++) {
        float modulated = bridge->column_activations[i]
                        + bridge->lateral_excitation[i]
                        - bridge->lateral_inhibition[i];
        if (modulated < 0.0f) modulated = 0.0f;
        bridge->column_activations[i] = modulated;

        /* Record post-spike time */
        if (modulated > 0.5f) {
            bridge->post_spike_time[i] = 0.0f;
        }
    }

    bridge->last_outbound_ts = cochlea_cortical_deep_time_ms();

    nimcp_mutex_unlock(bridge->base.mutex);

    cochlea_cortical_deep_bridge_heartbeat("bottom_up", 1.0f);
    return NIMCP_SUCCESS;
}

nimcp_error_t cochlea_cortical_get_bottom_up(
    const cochlea_cortical_deep_bridge_t* bridge,
    cochlea_bottomup_state_t* state)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_cortical_get_bottom_up: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_cortical_get_bottom_up: state is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    state->column_activations = bridge->column_activations;
    state->prediction_errors = bridge->prediction_errors;
    state->num_columns = bridge->num_columns;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Top-Down Modulation (Inbound)
//=============================================================================

nimcp_error_t cochlea_cortical_apply_top_down(
    cochlea_cortical_deep_bridge_t* bridge,
    const float* attention_pattern,
    uint32_t pattern_size)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_cortical_apply_top_down: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!attention_pattern) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_cortical_apply_top_down: attention_pattern is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    cochlea_cortical_deep_bridge_heartbeat("top_down", 0.5f);

    nimcp_mutex_lock(bridge->base.mutex);

    uint32_t n = (pattern_size < bridge->num_columns) ? pattern_size : bridge->num_columns;
    float gain_range = bridge->config.top_down_gain_range;

    for (uint32_t i = 0; i < n; i++) {
        /* Clamp attention pattern to [0, 1] and scale to gain range */
        float attn = attention_pattern[i];
        if (attn < 0.0f) attn = 0.0f;
        if (attn > 1.0f) attn = 1.0f;

        /* Map [0,1] to [1/gain_range, gain_range] */
        bridge->attention_gain[i] = 1.0f + (gain_range - 1.0f) * attn;
    }

    bridge->last_inbound_ts = cochlea_cortical_deep_time_ms();

    nimcp_mutex_unlock(bridge->base.mutex);

    cochlea_cortical_deep_bridge_heartbeat("top_down", 1.0f);
    return NIMCP_SUCCESS;
}

nimcp_error_t cochlea_cortical_get_top_down(
    const cochlea_cortical_deep_bridge_t* bridge,
    cochlea_topdown_state_t* state)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_cortical_get_top_down: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_cortical_get_top_down: state is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    state->attention_gain = bridge->attention_gain;
    state->expected_activation = bridge->expected_activation;
    state->num_columns = bridge->num_columns;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Prediction Error
//=============================================================================

nimcp_error_t cochlea_cortical_compute_prediction_error(
    cochlea_cortical_deep_bridge_t* bridge,
    float* prediction_error,
    uint32_t* error_size)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_cortical_compute_prediction_error: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!prediction_error || !error_size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_cortical_compute_prediction_error: output is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    cochlea_cortical_deep_bridge_heartbeat("prediction_error", 0.5f);

    nimcp_mutex_lock(bridge->base.mutex);

    uint32_t n = bridge->num_columns;
    *error_size = n;

    for (uint32_t i = 0; i < n; i++) {
        prediction_error[i] = bridge->column_activations[i] - bridge->expected_activation[i];
        bridge->prediction_errors[i] = prediction_error[i];
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    cochlea_cortical_deep_bridge_heartbeat("prediction_error", 1.0f);
    return NIMCP_SUCCESS;
}

//=============================================================================
// Plasticity (STDP)
//=============================================================================

nimcp_error_t cochlea_cortical_apply_stdp(
    cochlea_cortical_deep_bridge_t* bridge,
    float dt_ms)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_cortical_apply_stdp: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    cochlea_cortical_deep_bridge_heartbeat("stdp", 0.3f);

    nimcp_mutex_lock(bridge->base.mutex);

    const cochlea_stdp_config_t* stdp = &bridge->config.stdp_config;
    uint32_t n = bridge->num_columns;

    for (uint32_t i = 0; i < n; i++) {
        /* Increment spike timing */
        bridge->pre_spike_time[i] += dt_ms;
        bridge->post_spike_time[i] += dt_ms;

        /* STDP weight update based on spike timing difference */
        float dt_spike = bridge->post_spike_time[i] - bridge->pre_spike_time[i];
        float dw = 0.0f;

        if (dt_spike > 0.0f && stdp->tau_plus_ms > 0.0f) {
            /* Post after pre: LTP */
            dw = stdp->a_plus * expf(-dt_spike / stdp->tau_plus_ms);
        } else if (dt_spike < 0.0f && stdp->tau_minus_ms > 0.0f) {
            /* Pre after post: LTD */
            dw = -stdp->a_minus * expf(dt_spike / stdp->tau_minus_ms);
        }

        /* Apply weight change with bounds */
        bridge->weights[i] += dw;
        if (bridge->weights[i] > stdp->w_max) bridge->weights[i] = stdp->w_max;
        if (bridge->weights[i] < stdp->w_min) bridge->weights[i] = stdp->w_min;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    cochlea_cortical_deep_bridge_heartbeat("stdp", 1.0f);
    return NIMCP_SUCCESS;
}

nimcp_error_t cochlea_cortical_get_weights(
    const cochlea_cortical_deep_bridge_t* bridge,
    float** weights,
    uint32_t* num_weights)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_cortical_get_weights: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!weights || !num_weights) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_cortical_get_weights: output is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    *weights = bridge->weights;
    *num_weights = bridge->num_weights;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Bidirectional Verification
//=============================================================================

bool cochlea_cortical_deep_verify_bidirectional(const cochlea_cortical_deep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_cortical_deep_verify_bidirectional: bridge is NULL");
        return false;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    bool result = (bridge->last_outbound_ts > 0 && bridge->last_inbound_ts > 0);
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

uint64_t cochlea_cortical_deep_get_last_outbound(const cochlea_cortical_deep_bridge_t* bridge) {
    if (!bridge) return 0;
    nimcp_mutex_lock(bridge->base.mutex);
    uint64_t ts = bridge->last_outbound_ts;
    nimcp_mutex_unlock(bridge->base.mutex);
    return ts;
}

uint64_t cochlea_cortical_deep_get_last_inbound(const cochlea_cortical_deep_bridge_t* bridge) {
    if (!bridge) return 0;
    nimcp_mutex_lock(bridge->base.mutex);
    uint64_t ts = bridge->last_inbound_ts;
    nimcp_mutex_unlock(bridge->base.mutex);
    return ts;
}
