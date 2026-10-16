/**
 * @file nimcp_cortical_plasticity_bridge.c
 * @brief Cortical Column Plasticity Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-15
 */

#include "core/cortical_columns/nimcp_cortical_plasticity_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(cortical_plasticity_bridge)

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Find column state by ID
 *
 * WHAT: Locate per-column state in array
 * WHY:  Need fast column state lookup
 * HOW:  Linear search (small arrays)
 */
static cortical_column_plasticity_state_t* find_column_state(
    cortical_plasticity_bridge_t* bridge,
    uint32_t column_id
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;

    }

    for (uint32_t i = 0; i < bridge->num_columns; i++) {
        if (bridge->column_states[i].column_id == column_id) {
            return &bridge->column_states[i];
        }
    }

    return NULL;
}

/**
 * @brief Get effective learning rate with critical period boost
 *
 * WHAT: Calculate learning rate including critical period modulation
 * WHY:  Critical periods require enhanced plasticity
 * HOW:  Multiply base rate by boost if in critical period
 */
static float get_effective_learning_rate(
    const cortical_plasticity_bridge_t* bridge,
    float base_lr
) {
    if (!bridge) return base_lr;

    if (bridge->config.in_critical_period) {
        return base_lr * bridge->config.critical_period_plasticity_boost;
    }

    return base_lr;
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

int cortical_plasticity_default_config(cortical_plasticity_config_t* config) {
    /* Guard clause */
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");
        NIMCP_LOGGING_ERROR("Config is NULL");
        return -1;
    }

    memset(config, 0, sizeof(cortical_plasticity_config_t));

    /* STDP parameters */
    config->stdp_a_plus = CORTICAL_STDP_A_PLUS_DEFAULT;
    config->stdp_a_minus = CORTICAL_STDP_A_MINUS_DEFAULT;
    config->stdp_tau_plus = CORTICAL_STDP_TAU_PLUS_DEFAULT;
    config->stdp_tau_minus = CORTICAL_STDP_TAU_MINUS_DEFAULT;

    /* BCM parameters */
    config->bcm_theta_init = CORTICAL_BCM_THETA_INIT_DEFAULT;
    config->bcm_tau_theta = CORTICAL_BCM_TAU_THETA_DEFAULT;

    /* Homeostatic parameters */
    config->target_firing_rate = CORTICAL_TARGET_FIRING_RATE_DEFAULT;
    config->homeostatic_tau = CORTICAL_HOMEOSTATIC_TAU_DEFAULT;

    /* Critical period parameters */
    config->in_critical_period = false;
    config->critical_period_plasticity_boost = CORTICAL_CRITICAL_PERIOD_PLASTICITY_BOOST;
    config->critical_period_bcm_reduction = CORTICAL_CRITICAL_PERIOD_BCM_REDUCTION;
    config->critical_period_homeo_speedup = CORTICAL_CRITICAL_PERIOD_HOMEO_SPEEDUP;

    /* Layer-specific modulation */
    config->layer_4_lr_boost = CORTICAL_LAYER_4_LR_BOOST;
    config->layer_23_lr_boost = CORTICAL_LAYER_23_LR_BOOST;
    config->layer_56_lr_boost = CORTICAL_LAYER_56_LR_BOOST;

    /* Feature enables */
    config->enable_stdp = true;
    config->enable_bcm = true;
    config->enable_homeostatic = true;
    config->enable_eligibility = true;
    config->enable_bio_async = true;

    return 0;
}

cortical_plasticity_bridge_t* cortical_plasticity_bridge_create(
    const cortical_plasticity_config_t* config,
    plasticity_coordinator_t* coordinator
) {
    /* Allocate bridge */
    cortical_plasticity_bridge_t* bridge = (cortical_plasticity_bridge_t*)
        nimcp_malloc(sizeof(cortical_plasticity_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate cortical plasticity bridge");
        NIMCP_LOGGING_ERROR("Failed to allocate cortical plasticity bridge");
        return NULL;
    }

    memset(bridge, 0, sizeof(cortical_plasticity_bridge_t));

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        if (cortical_plasticity_default_config(&bridge->config) != 0) {
            nimcp_free(bridge);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "cortical_plasticity_bridge_create: default config failed");
            return NULL;
        }
    }

    /* Allocate columns array */
    bridge->column_capacity = CORTICAL_PLASTICITY_MAX_COLUMNS;
    bridge->columns = (hypercolumn_t**)
        nimcp_malloc(sizeof(hypercolumn_t*) * bridge->column_capacity);
    if (!bridge->columns) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate columns array");
        NIMCP_LOGGING_ERROR("Failed to allocate columns array");
        goto cleanup;
    }

    /* Allocate column states array */
    bridge->column_states = (cortical_column_plasticity_state_t*)
        nimcp_malloc(sizeof(cortical_column_plasticity_state_t) * bridge->column_capacity);
    if (!bridge->column_states) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate column states array");
        NIMCP_LOGGING_ERROR("Failed to allocate column states array");
        goto cleanup;
    }

    memset(bridge->columns, 0, sizeof(hypercolumn_t*) * bridge->column_capacity);
    memset(bridge->column_states, 0,
           sizeof(cortical_column_plasticity_state_t) * bridge->column_capacity);

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "cortical_plasticity") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to initialize bridge base");
        goto cleanup;
    }
    if (!bridge->base.mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to create mutex");
        NIMCP_LOGGING_ERROR("Failed to create mutex");
        goto cleanup;
    }

    /* Connect coordinator if provided */
    if (coordinator) {
        bridge->coordinator = coordinator;
        cortical_plasticity_connect_coordinator(bridge, coordinator);
    }

    NIMCP_LOGGING_INFO("Cortical plasticity bridge created");
    return bridge;

cleanup:
    if (bridge) {
        nimcp_free(bridge->column_states);
        nimcp_free(bridge->columns);
        nimcp_free(bridge);
    }
    return NULL;
}

void cortical_plasticity_bridge_destroy(cortical_plasticity_bridge_t* bridge) {
    /* Guard clause */
    if (!bridge) return;

    /* Disconnect integrations */
    if (bridge->base.bio_async_enabled) {
        cortical_plasticity_disconnect_bio_async(bridge);
    }

    if (bridge->coordinator) {
        cortical_plasticity_disconnect_coordinator(bridge);
    }

    /* Free per-column eligibility traces */
    for (uint32_t i = 0; i < bridge->num_columns; i++) {
        if (bridge->column_states[i].eligibility_traces) {
            nimcp_free(bridge->column_states[i].eligibility_traces);
        }
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    /* Free arrays */
    if (bridge->column_states) {
        nimcp_free(bridge->column_states);
    }
    if (bridge->columns) {
        nimcp_free(bridge->columns);
    }

    /* Free bridge */
    nimcp_free(bridge);

    NIMCP_LOGGING_INFO("Cortical plasticity bridge destroyed");
}

/* ============================================================================
 * Coordinator Integration API Implementation
 * ============================================================================ */

int cortical_plasticity_connect_coordinator(
    cortical_plasticity_bridge_t* bridge,
    plasticity_coordinator_t* coordinator
) {
    /* Guard clauses */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        NIMCP_LOGGING_ERROR("Invalid parameters");
        return -1;
    }
    if (!coordinator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "coordinator is NULL");
        NIMCP_LOGGING_ERROR("Invalid parameters");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->coordinator = coordinator;

    /* Register STDP mechanism if enabled */
    if (bridge->config.enable_stdp) {
        /* Note: Actual registration would require STDP update callback */
        /* This is a simplified placeholder */
        NIMCP_LOGGING_INFO("STDP mechanism registered (placeholder)");
        bridge->stdp_mechanism_id = 1;
    }

    /* Register BCM mechanism if enabled */
    if (bridge->config.enable_bcm) {
        NIMCP_LOGGING_INFO("BCM mechanism registered (placeholder)");
        bridge->bcm_mechanism_id = 2;
    }

    /* Register homeostatic mechanism if enabled */
    if (bridge->config.enable_homeostatic) {
        NIMCP_LOGGING_INFO("Homeostatic mechanism registered (placeholder)");
        bridge->homeostatic_mechanism_id = 3;
    }

    /* Register eligibility mechanism if enabled */
    if (bridge->config.enable_eligibility) {
        NIMCP_LOGGING_INFO("Eligibility mechanism registered (placeholder)");
        bridge->eligibility_mechanism_id = 4;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected to plasticity coordinator");
    return 0;
}

int cortical_plasticity_disconnect_coordinator(
    cortical_plasticity_bridge_t* bridge
) {
    /* Guard clause */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Unregister mechanisms */
    if (bridge->coordinator) {
        /* Note: Would call plasticity_coordinator_unregister_mechanism() */
        /* for each registered mechanism ID */
        bridge->stdp_mechanism_id = 0;
        bridge->bcm_mechanism_id = 0;
        bridge->homeostatic_mechanism_id = 0;
        bridge->eligibility_mechanism_id = 0;
    }

    bridge->coordinator = NULL;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Disconnected from plasticity coordinator");
    return 0;
}

/* ============================================================================
 * Column Management API Implementation
 * ============================================================================ */

int cortical_plasticity_add_column(
    cortical_plasticity_bridge_t* bridge,
    hypercolumn_t* column,
    uint32_t* column_id_out
) {
    /* Guard clauses */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        NIMCP_LOGGING_ERROR("Invalid parameters");
        return -1;
    }
    if (!column) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "column is NULL");
        NIMCP_LOGGING_ERROR("Invalid parameters");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Check capacity */
    if (bridge->num_columns >= bridge->column_capacity) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_LOGGING_ERROR("Column capacity exceeded");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Column capacity exceeded");
        return -1;
    }

    /* Add column */
    uint32_t column_id = bridge->num_columns;
    bridge->columns[column_id] = column;

    /* Initialize column state */
    cortical_column_plasticity_state_t* state = &bridge->column_states[column_id];
    state->column_id = column_id;
    state->bcm_threshold = bridge->config.bcm_theta_init;
    state->homeostatic_scale = 1.0f;
    state->avg_activity = 0.0f;
    state->current_firing_rate = 0.0f;
    state->eligibility_traces = NULL;
    state->num_synapses = 0;
    state->last_update_time = 0;
    state->total_updates = 0;

    bridge->num_columns++;

    if (column_id_out) {
        *column_id_out = column_id;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Added cortical column ID %u", column_id);
    return 0;
}

int cortical_plasticity_remove_column(
    cortical_plasticity_bridge_t* bridge,
    uint32_t column_id
) {
    /* Guard clauses */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }
    if (column_id >= bridge->num_columns) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "column_id out of range");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Free eligibility traces if allocated */
    if (bridge->column_states[column_id].eligibility_traces) {
        nimcp_free(bridge->column_states[column_id].eligibility_traces);
    }

    /* Shift arrays down */
    for (uint32_t i = column_id; i < bridge->num_columns - 1; i++) {
        bridge->columns[i] = bridge->columns[i + 1];
        bridge->column_states[i] = bridge->column_states[i + 1];
        bridge->column_states[i].column_id = i;  /* Update ID */
    }

    bridge->num_columns--;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Removed cortical column ID %u", column_id);
    return 0;
}

/* ============================================================================
 * STDP API Implementation
 * ============================================================================ */

int cortical_plasticity_apply_stdp(
    cortical_plasticity_bridge_t* bridge,
    uint32_t column_id,
    float pre_spike_time,
    float post_spike_time,
    uint32_t synapse_id
) {
    /* Guard clauses */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_stdp) return 0;

    cortical_column_plasticity_state_t* state = find_column_state(bridge, column_id);
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_plasticity_apply_stdp: state is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Calculate spike timing difference */
    float dt = post_spike_time - pre_spike_time;

    /* Apply STDP rule */
    float dw = 0.0f;
    float effective_lr = get_effective_learning_rate(bridge, 1.0f);

    if (dt > 0) {
        /* LTP: post after pre */
        dw = bridge->config.stdp_a_plus *
             expf(-dt / bridge->config.stdp_tau_plus) * effective_lr;
    } else if (dt < 0) {
        /* LTD: pre after post */
        dw = -bridge->config.stdp_a_minus *
             expf(dt / bridge->config.stdp_tau_minus) * effective_lr;
    }

    /* Apply weight change (placeholder - would update actual synapse) */
    (void)synapse_id;  /* Unused in placeholder */
    (void)dw;

    bridge->total_stdp_updates++;
    state->total_updates++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * BCM API Implementation
 * ============================================================================ */

int cortical_plasticity_update_bcm_threshold(
    cortical_plasticity_bridge_t* bridge,
    uint32_t column_id,
    float current_activity,
    float dt
) {
    /* Guard clauses */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_bcm) return 0;

    cortical_column_plasticity_state_t* state = find_column_state(bridge, column_id);
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_plasticity_update_bcm_threshold: state is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update average activity */
    float alpha = dt / bridge->config.bcm_tau_theta;
    state->avg_activity = (1.0f - alpha) * state->avg_activity +
                          alpha * (current_activity * current_activity);

    /* Slide BCM threshold */
    float target_theta = state->avg_activity;

    /* Apply critical period reduction if active */
    if (bridge->config.in_critical_period) {
        target_theta *= bridge->config.critical_period_bcm_reduction;
    }

    state->bcm_threshold = (1.0f - alpha) * state->bcm_threshold +
                           alpha * target_theta;

    bridge->total_bcm_updates++;
    state->total_updates++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float cortical_plasticity_get_bcm_threshold(
    const cortical_plasticity_bridge_t* bridge,
    uint32_t column_id
) {
    /* Guard clauses */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1.0f;
    }
    if (column_id >= bridge->num_columns) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "column_id out of range");
        return -1.0f;
    }

    return bridge->column_states[column_id].bcm_threshold;
}

/* ============================================================================
 * Homeostatic Scaling API Implementation
 * ============================================================================ */

int cortical_plasticity_apply_homeostatic_scaling(
    cortical_plasticity_bridge_t* bridge,
    uint32_t column_id,
    float current_rate,
    float dt
) {
    /* Guard clauses */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_homeostatic) return 0;

    cortical_column_plasticity_state_t* state = find_column_state(bridge, column_id);
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_plasticity_apply_homeostatic_scaling: state is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update current firing rate */
    state->current_firing_rate = current_rate;

    /* Calculate rate deviation */
    float rate_error = bridge->config.target_firing_rate - current_rate;

    /* Apply critical period speedup if active */
    float effective_tau = bridge->config.homeostatic_tau;
    if (bridge->config.in_critical_period) {
        effective_tau *= bridge->config.critical_period_homeo_speedup;
    }

    /* Update scaling factor */
    float alpha = dt / effective_tau;
    float scale_adjustment = alpha * (rate_error / bridge->config.target_firing_rate);
    state->homeostatic_scale *= (1.0f + scale_adjustment);

    /* Clamp scaling factor */
    if (state->homeostatic_scale < 0.1f) state->homeostatic_scale = 0.1f;
    if (state->homeostatic_scale > 10.0f) state->homeostatic_scale = 10.0f;

    bridge->total_homeostatic_updates++;
    state->total_updates++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float cortical_plasticity_get_homeostatic_scale(
    const cortical_plasticity_bridge_t* bridge,
    uint32_t column_id
) {
    /* Guard clauses */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1.0f;
    }
    if (column_id >= bridge->num_columns) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "column_id out of range");
        return -1.0f;
    }

    return bridge->column_states[column_id].homeostatic_scale;
}

/* ============================================================================
 * Critical Period API Implementation
 * ============================================================================ */

int cortical_plasticity_set_critical_period(
    cortical_plasticity_bridge_t* bridge,
    bool in_critical_period
) {
    /* Guard clause */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->config.in_critical_period = in_critical_period;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Critical period %s",
        in_critical_period ? "ENABLED" : "DISABLED");
    return 0;
}

bool cortical_plasticity_is_critical_period(
    const cortical_plasticity_bridge_t* bridge
) {
    if (!bridge) {
        return false;
    }
    return bridge->config.in_critical_period;
}

/* ============================================================================
 * Eligibility Trace API Implementation
 * ============================================================================ */

int cortical_plasticity_update_eligibility(
    cortical_plasticity_bridge_t* bridge,
    uint32_t column_id,
    float dt
) {
    /* Guard clauses */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_eligibility) return 0;

    cortical_column_plasticity_state_t* state = find_column_state(bridge, column_id);
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_plasticity_update_eligibility: state is NULL");
        return -1;
    }
    if (!state->eligibility_traces) return 0;  /* No traces allocated */

    nimcp_mutex_lock(bridge->base.mutex);

    /* Decay eligibility traces */
    float tau_eligibility = 1000.0f;  /* 1 second */
    float decay = expf(-dt / tau_eligibility);

    for (uint32_t i = 0; i < state->num_synapses; i++) {
        state->eligibility_traces[i] *= decay;
    }

    bridge->total_eligibility_updates++;
    state->total_updates++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int cortical_plasticity_apply_reward(
    cortical_plasticity_bridge_t* bridge,
    uint32_t column_id,
    float reward
) {
    /* Guard clauses */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_eligibility) return 0;

    cortical_column_plasticity_state_t* state = find_column_state(bridge, column_id);
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_plasticity_apply_reward: state is NULL");
        return -1;
    }
    if (!state->eligibility_traces) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Convert eligibility to weight changes */
    for (uint32_t i = 0; i < state->num_synapses; i++) {
        float dw = reward * state->eligibility_traces[i];
        (void)dw;  /* Placeholder - would apply to actual synapse */
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Statistics API Implementation
 * ============================================================================ */

int cortical_plasticity_get_stats(
    const cortical_plasticity_bridge_t* bridge,
    uint64_t* stdp_updates_out,
    uint64_t* bcm_updates_out,
    uint64_t* homeostatic_updates_out,
    uint64_t* eligibility_updates_out
) {
    /* Guard clause */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }

    if (stdp_updates_out) *stdp_updates_out = bridge->total_stdp_updates;
    if (bcm_updates_out) *bcm_updates_out = bridge->total_bcm_updates;
    if (homeostatic_updates_out) *homeostatic_updates_out = bridge->total_homeostatic_updates;
    if (eligibility_updates_out) *eligibility_updates_out = bridge->total_eligibility_updates;

    return 0;
}

int cortical_plasticity_get_column_state(
    const cortical_plasticity_bridge_t* bridge,
    uint32_t column_id,
    cortical_column_plasticity_state_t* state_out
) {
    /* Guard clauses */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }
    if (!state_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "state_out is NULL");
        return -1;
    }
    if (column_id >= bridge->num_columns) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "column_id out of range");
        return -1;
    }

    *state_out = bridge->column_states[column_id];
    return 0;
}

/* ============================================================================
 * Bio-Async Integration API Implementation
 * ============================================================================ */

int cortical_plasticity_connect_bio_async(
    cortical_plasticity_bridge_t* bridge
) {
    /* Guard clause */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }
    if (bridge->base.bio_async_enabled) return 0;

    /* Register with bio-async router */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_CORTICAL_PLASTICITY,
        .module_name = CORTICAL_PLASTICITY_MODULE_NAME,
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
        return 0;
    }

    NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
    return 0;  /* Not an error - bio-async is optional */
}

int cortical_plasticity_disconnect_bio_async(
    cortical_plasticity_bridge_t* bridge
) {
    /* Guard clause */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }
    if (!bridge->base.bio_async_enabled) return 0;

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Disconnected from bio-async router");
    return 0;
}

bool cortical_plasticity_is_bio_async_connected(
    const cortical_plasticity_bridge_t* bridge
) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}
