/**
 * @file nimcp_neuromod_pool_adapter.c
 * @brief Neuromodulator Pool Adapter Implementation
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Implements neuromodulator pool dynamics for Neuromodulatory layer
 * WHY:  Neuromodulators gate plasticity and modulate neural computation
 * HOW:  Models tonic/phasic release, decay, and cross-modulation
 *
 * @author NIMCP Development Team
 */

#include "integration/adapters/neuromod/nimcp_neuromod_pool_adapter.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(neuromod_pool_adapter)

//=============================================================================
// Internal Structure
//=============================================================================

struct nimcp_neuromod_pool_adapter_struct {
    nimcp_neuromod_pool_config_t config;
    nimcp_module_interface_t interface;

    /* Neuromodulator state */
    float tonic[NMOD_COUNT];            /**< Tonic (baseline) levels */
    float phasic[NMOD_COUNT];           /**< Phasic (transient) levels */
    float current[NMOD_COUNT];          /**< Total current levels */

    /* State tracking */
    nimcp_neuromod_pool_state_t state;
    nimcp_neuromod_pool_stats_t stats;
    bool is_initialized;
};

//=============================================================================
// Module Interface Callbacks
//=============================================================================

static nimcp_layer_error_t neuromod_pool_init(void* module, void* config) {
    nimcp_neuromod_pool_adapter_t adapter = (nimcp_neuromod_pool_adapter_t)module;
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;

    if (config) {
        adapter->config = *(nimcp_neuromod_pool_config_t*)config;
    }

    /* Initialize to basal levels */
    for (int i = 0; i < NMOD_COUNT; i++) {
        adapter->tonic[i] = adapter->config.basal_levels[i];
        adapter->phasic[i] = 0.0f;
        adapter->current[i] = adapter->tonic[i];
    }

    adapter->is_initialized = true;
    adapter->state.is_active = true;

    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t neuromod_pool_shutdown(void* module) {
    nimcp_neuromod_pool_adapter_t adapter = (nimcp_neuromod_pool_adapter_t)module;
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;

    adapter->is_initialized = false;
    adapter->state.is_active = false;

    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t neuromod_pool_update(void* module, float dt) {
    nimcp_neuromod_pool_adapter_t adapter = (nimcp_neuromod_pool_adapter_t)module;
    if (!adapter || !adapter->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;

    float dt_ms = dt * 1000.0f;

    /* Update each neuromodulator */
    for (int i = 0; i < NMOD_COUNT; i++) {
        /* Phasic decay toward zero */
        float tau = adapter->config.decay_rates[i];
        if (tau > 0.0f) {
            adapter->phasic[i] *= expf(-dt_ms / tau);
        }

        /* Tonic adaptation (slow drift toward basal) */
        float basal = adapter->config.basal_levels[i];
        float tonic_tau = 10000.0f;  /* Very slow adaptation (10 sec) */
        adapter->tonic[i] += (basal - adapter->tonic[i]) / tonic_tau * dt_ms;

        /* Total = tonic + phasic */
        adapter->current[i] = adapter->tonic[i] + adapter->phasic[i];

        /* Clamp to max */
        if (adapter->current[i] > adapter->config.max_levels[i]) {
            adapter->current[i] = adapter->config.max_levels[i];
        }
        if (adapter->current[i] < 0.0f) {
            adapter->current[i] = 0.0f;
        }

        /* Track peaks */
        if (adapter->current[i] > adapter->stats.peak_levels[i]) {
            adapter->stats.peak_levels[i] = adapter->current[i];
        }

        /* Update state */
        adapter->state.levels[i] = adapter->current[i];
        adapter->state.tonic_levels[i] = adapter->tonic[i];
        adapter->state.phasic_levels[i] = adapter->phasic[i];
    }

    /* Cross-modulation effects */
    if (adapter->config.enable_cross_modulation) {
        /* High norepinephrine suppresses serotonin release */
        if (adapter->current[NMOD_NOREPINEPHRINE] > 0.7f) {
            adapter->phasic[NMOD_SEROTONIN] *= 0.99f;
        }
        /* High acetylcholine enhances dopamine sensitivity */
        /* (modeled implicitly through combined effects) */
    }

    /* Global arousal = weighted sum of NE and ACh */
    adapter->state.global_arousal =
        0.5f * adapter->current[NMOD_NOREPINEPHRINE] +
        0.5f * adapter->current[NMOD_ACETYLCHOLINE];

    adapter->stats.updates_processed++;

    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t neuromod_pool_handle_message(void* module, const nimcp_layer_msg_t* msg) {
    nimcp_neuromod_pool_adapter_t adapter = (nimcp_neuromod_pool_adapter_t)module;
    if (!adapter || !msg) return NIMCP_LAYER_ERR_NULL_PTR;

    adapter->stats.messages_handled++;

    switch (msg->header.msg_type) {
        case NIMCP_LAYER_MSG_MODULATE:
            /* Modulation signal triggers phasic release */
            if (msg->payload && msg->header.payload_size >= 2 * sizeof(float)) {
                float* data = (float*)msg->payload;
                int nmod_type = (int)data[0];
                float amount = data[1];
                if (nmod_type >= 0 && nmod_type < NMOD_COUNT) {
                    adapter->phasic[nmod_type] += amount;
                    adapter->stats.release_events[nmod_type]++;
                }
            }
            break;

        case NIMCP_LAYER_MSG_EXCITE:
            /* Excitation increases dopamine (reward prediction) */
            adapter->phasic[NMOD_DOPAMINE] += 0.1f;
            adapter->stats.release_events[NMOD_DOPAMINE]++;
            break;

        case NIMCP_LAYER_MSG_INHIBIT:
            /* Inhibition increases serotonin (patience/inhibition) */
            adapter->phasic[NMOD_SEROTONIN] += 0.1f;
            adapter->stats.release_events[NMOD_SEROTONIN]++;
            break;

        case NIMCP_LAYER_MSG_DATA_PUSH:
            /* Incoming arousal/salience signal */
            if (msg->payload && msg->header.payload_size >= sizeof(float)) {
                float salience = *(float*)msg->payload;
                /* Salience increases acetylcholine and norepinephrine */
                adapter->phasic[NMOD_ACETYLCHOLINE] += salience * 0.05f;
                adapter->phasic[NMOD_NOREPINEPHRINE] += salience * 0.03f;
                adapter->stats.release_events[NMOD_ACETYLCHOLINE]++;
                adapter->stats.release_events[NMOD_NOREPINEPHRINE]++;
            }
            break;

        default:
            break;
    }

    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t neuromod_pool_get_state(void* module, void* state_out, size_t* size) {
    nimcp_neuromod_pool_adapter_t adapter = (nimcp_neuromod_pool_adapter_t)module;
    if (!adapter || !state_out || !size) return NIMCP_LAYER_ERR_NULL_PTR;

    if (*size < sizeof(nimcp_neuromod_pool_state_t)) {
        *size = sizeof(nimcp_neuromod_pool_state_t);
        return NIMCP_LAYER_ERR_CAPACITY;
    }

    *(nimcp_neuromod_pool_state_t*)state_out = adapter->state;
    *size = sizeof(nimcp_neuromod_pool_state_t);

    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t neuromod_pool_set_state(void* module, const void* state, size_t size) {
    nimcp_neuromod_pool_adapter_t adapter = (nimcp_neuromod_pool_adapter_t)module;
    if (!adapter || !state) return NIMCP_LAYER_ERR_NULL_PTR;

    if (size < sizeof(nimcp_neuromod_pool_state_t)) {
        return NIMCP_LAYER_ERR_INVALID_MSG;
    }

    adapter->state = *(const nimcp_neuromod_pool_state_t*)state;

    return NIMCP_LAYER_OK;
}

static const char* neuromod_pool_get_name(void* module) {
    (void)module;
    return "Neuromodulator_Pool_Adapter";
}

//=============================================================================
// Public API Implementation
//=============================================================================

nimcp_neuromod_pool_config_t nimcp_neuromod_pool_adapter_default_config(void) {
    nimcp_neuromod_pool_config_t config;

    /* Basal levels (resting state) */
    config.basal_levels[NMOD_DOPAMINE] = 0.3f;
    config.basal_levels[NMOD_SEROTONIN] = 0.4f;
    config.basal_levels[NMOD_ACETYLCHOLINE] = 0.3f;
    config.basal_levels[NMOD_NOREPINEPHRINE] = 0.2f;

    /* Decay rates (ms) */
    config.decay_rates[NMOD_DOPAMINE] = 200.0f;      /* Fast - phasic signaling */
    config.decay_rates[NMOD_SEROTONIN] = 500.0f;     /* Slower - mood regulation */
    config.decay_rates[NMOD_ACETYLCHOLINE] = 100.0f; /* Fast - attention shifts */
    config.decay_rates[NMOD_NOREPINEPHRINE] = 150.0f;/* Medium - arousal */

    /* Maximum levels */
    config.max_levels[NMOD_DOPAMINE] = 1.0f;
    config.max_levels[NMOD_SEROTONIN] = 1.0f;
    config.max_levels[NMOD_ACETYLCHOLINE] = 1.0f;
    config.max_levels[NMOD_NOREPINEPHRINE] = 1.0f;

    config.enable_cross_modulation = true;
    config.enable_logging = false;

    return config;
}

nimcp_neuromod_pool_adapter_t nimcp_neuromod_pool_adapter_create(
    const nimcp_neuromod_pool_config_t* config
) {
    nimcp_neuromod_pool_adapter_t adapter = (nimcp_neuromod_pool_adapter_t)nimcp_calloc(
        1, sizeof(struct nimcp_neuromod_pool_adapter_struct));
    NIMCP_API_CHECK_ALLOC(adapter, "Failed to allocate neuromod pool adapter");

    adapter->config = config ? *config : nimcp_neuromod_pool_adapter_default_config();

    /* Set up module interface */
    adapter->interface.init = neuromod_pool_init;
    adapter->interface.shutdown = neuromod_pool_shutdown;
    adapter->interface.update = neuromod_pool_update;
    adapter->interface.handle_message = neuromod_pool_handle_message;
    adapter->interface.get_state = neuromod_pool_get_state;
    adapter->interface.set_state = neuromod_pool_set_state;
    adapter->interface.get_name = neuromod_pool_get_name;

    return adapter;
}

void nimcp_neuromod_pool_adapter_destroy(nimcp_neuromod_pool_adapter_t adapter) {
    if (!adapter) return;

    if (adapter->is_initialized) {
        neuromod_pool_shutdown(adapter);
    }

    nimcp_free(adapter);
}

nimcp_module_interface_t* nimcp_neuromod_pool_adapter_get_interface(
    nimcp_neuromod_pool_adapter_t adapter
) {
    return adapter ? &adapter->interface : NULL;
}

nimcp_layer_error_t nimcp_neuromod_pool_adapter_release(
    nimcp_neuromod_pool_adapter_t adapter,
    nmod_type_t nmod_type,
    float amount
) {
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;
    if (!adapter->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;
    if (nmod_type < 0 || nmod_type >= NMOD_COUNT) return NIMCP_LAYER_ERR_INVALID_MODULE;

    adapter->phasic[nmod_type] += amount;
    adapter->stats.release_events[nmod_type]++;

    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_neuromod_pool_adapter_set_tonic(
    nimcp_neuromod_pool_adapter_t adapter,
    nmod_type_t nmod_type,
    float level
) {
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;
    if (nmod_type < 0 || nmod_type >= NMOD_COUNT) return NIMCP_LAYER_ERR_INVALID_MODULE;

    adapter->tonic[nmod_type] = level;

    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_neuromod_pool_adapter_get_levels(
    nimcp_neuromod_pool_adapter_t adapter,
    float* levels_out
) {
    if (!adapter || !levels_out) return NIMCP_LAYER_ERR_NULL_PTR;

    memcpy(levels_out, adapter->current, NMOD_COUNT * sizeof(float));

    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_neuromod_pool_adapter_get_level(
    nimcp_neuromod_pool_adapter_t adapter,
    nmod_type_t nmod_type,
    float* level_out
) {
    if (!adapter || !level_out) return NIMCP_LAYER_ERR_NULL_PTR;
    if (nmod_type < 0 || nmod_type >= NMOD_COUNT) return NIMCP_LAYER_ERR_INVALID_MODULE;

    *level_out = adapter->current[nmod_type];

    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_neuromod_pool_adapter_get_state(
    nimcp_neuromod_pool_adapter_t adapter,
    nimcp_neuromod_pool_state_t* state_out
) {
    if (!adapter || !state_out) return NIMCP_LAYER_ERR_NULL_PTR;
    *state_out = adapter->state;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_neuromod_pool_adapter_get_stats(
    nimcp_neuromod_pool_adapter_t adapter,
    nimcp_neuromod_pool_stats_t* stats_out
) {
    if (!adapter || !stats_out) return NIMCP_LAYER_ERR_NULL_PTR;
    *stats_out = adapter->stats;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_neuromod_pool_adapter_reset_stats(nimcp_neuromod_pool_adapter_t adapter) {
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;
    memset(&adapter->stats, 0, sizeof(adapter->stats));
    return NIMCP_LAYER_OK;
}
