/**
 * @file nimcp_snn_stdp_bridge.c
 * @brief SNN-STDP Plasticity Integration Bridge Implementation
 *
 * @author NIMCP Team
 * @date 2024
 */

#include "snn/bridges/nimcp_snn_stdp_bridge.h"
#include "constants/nimcp_constants.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(snn_stdp_bridge)

//=============================================================================
// Bio-async Module ID
//=============================================================================

#define BIO_MODULE_SNN_STDP_BRIDGE 0x0610

//=============================================================================
// Default Configuration
//=============================================================================

void snn_stdp_bridge_config_default(snn_stdp_bridge_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_stdp_bridge_config_default: null config pointer");
        return;
    }

    /* Timing windows from Bi & Poo (1998) */
    config->ltp_window_ms = NIMCP_STDP_TAU_PLUS_MS;
    config->ltd_window_ms = NIMCP_STDP_TAU_MINUS_MS;
    config->min_spike_interval_ms = 1.0f;

    /* Learning rate coordination */
    config->sync_learning_rates = true;
    config->lr_scaling_factor = 1.0f;

    /* Weight update control */
    config->weight_update_interval_ms = 1.0f;
    config->max_weight_change_per_step = 0.1f;
    config->bidirectional_updates = true;

    /* Dopamine modulation */
    config->enable_da_modulation = true;
    config->da_threshold = 0.1f;

    /* Bio-async */
    config->enable_bio_async = true;
    config->spike_buffer_size = 1024;
}

//=============================================================================
// Bridge Lifecycle
//=============================================================================

snn_stdp_bridge_t* snn_stdp_bridge_create(
    const snn_stdp_bridge_config_t* config,
    snn_network_t* network,
    stdp_synapse_t* stdp_synapses,
    uint32_t n_synapses
) {
    /* Guard clauses */
    if (!config || !network || !stdp_synapses || n_synapses == 0) {
        NIMCP_LOGGING_ERROR("Invalid parameters for SNN-STDP bridge creation");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_stdp_bridge_create: config/network/synapses is NULL or n_synapses=0");
        return NULL;
    }

    /* Allocate bridge */
    snn_stdp_bridge_t* bridge = (snn_stdp_bridge_t*)nimcp_malloc(sizeof(snn_stdp_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate SNN-STDP bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_stdp_bridge_create: failed to allocate bridge");
        return NULL;
    }

    memset(bridge, 0, sizeof(snn_stdp_bridge_t));

    /* Store references */
    bridge->network = network;
    bridge->stdp_synapses = stdp_synapses;
    bridge->n_synapses = n_synapses;
    bridge->config = *config;

    /* Allocate weight change buffer */
    bridge->weight_change_capacity = config->spike_buffer_size;
    bridge->weight_changes = (weight_change_record_t*)nimcp_malloc(
        sizeof(weight_change_record_t) * bridge->weight_change_capacity
    );
    if (!bridge->weight_changes) {
        NIMCP_LOGGING_ERROR("Failed to allocate weight change buffer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_stdp_bridge_create: failed to allocate weight_changes");
        nimcp_free(bridge);
        return NULL;
    }

    memset(bridge->weight_changes, 0,
           sizeof(weight_change_record_t) * bridge->weight_change_capacity);

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "snn_stdp") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_WARN("Failed to create mutex for SNN-STDP bridge");
        /* Continue without mutex */
    }

    /* Initialize state */
    bridge->connected = true;
    bridge->last_update_time_ms = 0.0f;
    bridge->plasticity_events = 0;
    bridge->weight_syncs = 0;
    bridge->weight_change_count = 0;
    bridge->weight_change_write_idx = 0;

    /* Initialize effects */
    bridge->effects.effective_a_plus = 0.005f;
    bridge->effects.effective_a_minus = 0.00525f;
    bridge->effects.effective_tau_plus = 20.0f;
    bridge->effects.effective_tau_minus = 20.0f;
    bridge->effects.da_modulation_factor = 1.0f;
    bridge->effects.learning_rate_factor = 1.0f;

    /* Connect bio-async if enabled */
    if (config->enable_bio_async) {
        snn_stdp_bridge_connect_bio_async(bridge);
    }

    NIMCP_LOGGING_INFO("Created SNN-STDP bridge with %u synapses", n_synapses);
    return bridge;
}

void snn_stdp_bridge_destroy(snn_stdp_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_stdp_bridge_destroy: null bridge pointer");
        return;
    }

    /* Disconnect bio-async */
    if (bridge->base.bio_async_enabled) {
        snn_stdp_bridge_disconnect_bio_async(bridge);
    }

    /* Free resources */
    if (bridge->weight_changes) {
        nimcp_free(bridge->weight_changes);
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
        bridge->base.mutex = NULL;
    }

    nimcp_free(bridge);
}

//=============================================================================
// Bio-async Integration
//=============================================================================

int snn_stdp_bridge_connect_bio_async(snn_stdp_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_stdp_bridge_connect_bio_async: null bridge pointer");
        return -1;
    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SNN_STDP_BRIDGE,
        .module_name = "snn_stdp_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected SNN-STDP bridge to bio-async router");
    }

    return 0;
}

int snn_stdp_bridge_disconnect_bio_async(snn_stdp_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_stdp_bridge_disconnect_bio_async: null bridge pointer");
        return -1;
    }
    if (!bridge->base.bio_async_enabled) return 0;

    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    bridge->base.bio_ctx = NULL;

    return 0;
}

bool snn_stdp_bridge_is_bio_async_connected(const snn_stdp_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}

//=============================================================================
// Update Functions
//=============================================================================

int snn_stdp_bridge_update_effects(snn_stdp_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_stdp_bridge_update_effects: null bridge pointer");
        return -1;
    }
    if (!bridge->stdp_synapses) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "snn_stdp_bridge_update_effects: null stdp_synapses");
        return -1;
    }

    if (bridge->base.mutex) {
        nimcp_platform_mutex_lock(bridge->base.mutex);
    }

    /* Average STDP parameters across synapses */
    float avg_a_plus = 0.0f;
    float avg_a_minus = 0.0f;
    float avg_tau_plus = 0.0f;
    float avg_tau_minus = 0.0f;
    float avg_lr = 0.0f;

    for (uint32_t i = 0; i < bridge->n_synapses; i++) {
        avg_a_plus += bridge->stdp_synapses[i].a_plus;
        avg_a_minus += bridge->stdp_synapses[i].a_minus;
        avg_tau_plus += bridge->stdp_synapses[i].tau_plus;
        avg_tau_minus += bridge->stdp_synapses[i].tau_minus;
        avg_lr += bridge->stdp_synapses[i].learning_rate;
    }

    float n_inv = 1.0f / (float)bridge->n_synapses;
    avg_a_plus *= n_inv;
    avg_a_minus *= n_inv;
    avg_tau_plus *= n_inv;
    avg_tau_minus *= n_inv;
    avg_lr *= n_inv;

    /* Update effects */
    bridge->effects.effective_a_plus = avg_a_plus * bridge->config.lr_scaling_factor;
    bridge->effects.effective_a_minus = avg_a_minus * bridge->config.lr_scaling_factor;
    bridge->effects.effective_tau_plus = avg_tau_plus;
    bridge->effects.effective_tau_minus = avg_tau_minus;
    bridge->effects.learning_rate_factor = avg_lr;

    /* Dopamine modulation - use first synapse as representative */
    if (bridge->config.enable_da_modulation && bridge->n_synapses > 0) {
        /* Note: In real implementation, would query neuromodulator system */
        /* For now, use baseline modulation */
        bridge->effects.da_modulation_factor = 1.0f;
    }

    if (bridge->base.mutex) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
    }

    return 0;
}

int snn_stdp_bridge_apply_plasticity(snn_stdp_bridge_t* bridge, float dt) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_stdp_bridge_apply_plasticity: null bridge pointer");
        return -1;
    }
    if (!bridge->network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "snn_stdp_bridge_apply_plasticity: null network");
        return -1;
    }

    if (bridge->base.mutex) {
        nimcp_platform_mutex_lock(bridge->base.mutex);
    }

    /* Apply STDP-based weight updates to SNN synapses */
    /* This would integrate with SNN's spike trains and update weights */
    /* For now, increment plasticity event counter */
    bridge->plasticity_events++;

    if (bridge->base.mutex) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
    }

    return 0;
}

int snn_stdp_bridge_update(snn_stdp_bridge_t* bridge, float dt) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_stdp_bridge_update: null bridge pointer");
        return -1;
    }

    /* Update effects from STDP module */
    int ret = snn_stdp_bridge_update_effects(bridge);
    if (ret != 0) {
        return ret;
    }

    /* Apply plasticity to SNN */
    ret = snn_stdp_bridge_apply_plasticity(bridge, dt);
    if (ret != 0) {
        return ret;
    }

    bridge->last_update_time_ms += dt;

    return 0;
}

//=============================================================================
// Weight Synchronization
//=============================================================================

int snn_stdp_bridge_get_weight_changes(
    const snn_stdp_bridge_t* bridge,
    weight_change_record_t* changes,
    uint32_t max_changes,
    uint32_t* n_changes
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_stdp_bridge_get_weight_changes: null bridge pointer");
        return -1;
    }
    if (!changes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_stdp_bridge_get_weight_changes: null changes pointer");
        return -1;
    }
    if (!n_changes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_stdp_bridge_get_weight_changes: null n_changes pointer");
        return -1;
    }

    if (bridge->base.mutex) {
        nimcp_platform_mutex_lock((void*)bridge->base.mutex);
    }

    uint32_t count = 0;
    uint32_t to_copy = bridge->weight_change_count < max_changes ?
                       bridge->weight_change_count : max_changes;

    for (uint32_t i = 0; i < to_copy; i++) {
        if (!bridge->weight_changes[i].applied_to_snn) {
            changes[count++] = bridge->weight_changes[i];
        }
    }

    *n_changes = count;

    if (bridge->base.mutex) {
        nimcp_platform_mutex_unlock((void*)bridge->base.mutex);
    }

    return 0;
}

int snn_stdp_bridge_mark_synced(snn_stdp_bridge_t* bridge, uint32_t n_synced) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_stdp_bridge_mark_synced: null bridge pointer");
        return -1;
    }

    if (bridge->base.mutex) {
        nimcp_platform_mutex_lock(bridge->base.mutex);
    }

    /* Mark first n_synced unsynced changes as synced */
    uint32_t marked = 0;
    for (uint32_t i = 0; i < bridge->weight_change_count && marked < n_synced; i++) {
        if (!bridge->weight_changes[i].applied_to_snn) {
            bridge->weight_changes[i].applied_to_snn = true;
            marked++;
        }
    }

    bridge->weight_syncs += marked;

    if (bridge->base.mutex) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
    }

    return 0;
}

int snn_stdp_bridge_record_weight_change(
    snn_stdp_bridge_t* bridge,
    uint32_t synapse_id,
    float delta_weight
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_stdp_bridge_record_weight_change: null bridge pointer");
        return -1;
    }

    if (bridge->base.mutex) {
        nimcp_platform_mutex_lock(bridge->base.mutex);
    }

    /* Add to circular buffer */
    uint32_t idx = bridge->weight_change_write_idx;
    bridge->weight_changes[idx].synapse_id = synapse_id;
    bridge->weight_changes[idx].delta_weight = delta_weight;
    bridge->weight_changes[idx].timestamp_us = bridge->last_update_time_ms * 1000.0f;
    bridge->weight_changes[idx].applied_to_snn = false;

    bridge->weight_change_write_idx = (idx + 1) % bridge->weight_change_capacity;
    if (bridge->weight_change_count < bridge->weight_change_capacity) {
        bridge->weight_change_count++;
    }

    if (bridge->base.mutex) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
    }

    return 0;
}

//=============================================================================
// Query Functions
//=============================================================================

int snn_stdp_bridge_get_effects(
    const snn_stdp_bridge_t* bridge,
    snn_stdp_effects_t* effects
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_stdp_bridge_get_effects: null bridge pointer");
        return -1;
    }
    if (!effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_stdp_bridge_get_effects: null effects pointer");
        return -1;
    }

    if (bridge->base.mutex) {
        nimcp_platform_mutex_lock((void*)bridge->base.mutex);
    }

    *effects = bridge->effects;

    if (bridge->base.mutex) {
        nimcp_platform_mutex_unlock((void*)bridge->base.mutex);
    }

    return 0;
}

float snn_stdp_bridge_get_effective_a_plus(const snn_stdp_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_stdp_bridge_get_effective_a_plus: null bridge pointer");
        return 0.0f;
    }
    return bridge->effects.effective_a_plus;
}

float snn_stdp_bridge_get_effective_a_minus(const snn_stdp_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_stdp_bridge_get_effective_a_minus: null bridge pointer");
        return 0.0f;
    }
    return bridge->effects.effective_a_minus;
}

float snn_stdp_bridge_get_da_modulation(const snn_stdp_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_stdp_bridge_get_da_modulation: null bridge pointer");
        return 1.0f;
    }
    return bridge->effects.da_modulation_factor;
}

//=============================================================================
// Statistics
//=============================================================================

int snn_stdp_bridge_get_stats(
    const snn_stdp_bridge_t* bridge,
    uint32_t* plasticity_events,
    uint32_t* weight_syncs,
    uint32_t* updates
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_stdp_bridge_get_stats: null bridge pointer");
        return -1;
    }

    if (plasticity_events) {
        *plasticity_events = bridge->plasticity_events;
    }
    if (weight_syncs) {
        *weight_syncs = bridge->weight_syncs;
    }
    if (updates) {
        *updates = (uint32_t)(bridge->last_update_time_ms /
                              bridge->config.weight_update_interval_ms);
    }

    return 0;
}

void snn_stdp_bridge_reset_stats(snn_stdp_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_stdp_bridge_reset_stats: null bridge pointer");
        return;
    }

    if (bridge->base.mutex) {
        nimcp_platform_mutex_lock(bridge->base.mutex);
    }

    bridge->plasticity_events = 0;
    bridge->weight_syncs = 0;
    bridge->last_update_time_ms = 0.0f;

    if (bridge->base.mutex) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
    }
}
