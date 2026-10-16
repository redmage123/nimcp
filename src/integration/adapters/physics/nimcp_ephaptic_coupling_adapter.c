/**
 * @file nimcp_ephaptic_coupling_adapter.c
 * @brief Ephaptic Coupling Adapter Implementation
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Implements ephaptic (electric field) coupling for Physics layer
 * WHY:  Models non-synaptic neural communication through electric fields
 * HOW:  Computes local field potentials and field-mediated interactions
 *
 * @author NIMCP Development Team
 */

#include "integration/adapters/physics/nimcp_ephaptic_coupling_adapter.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_neural_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(ephaptic_coupling_adapter)

//=============================================================================
// Internal Structure
//=============================================================================

struct nimcp_ephaptic_adapter_struct {
    nimcp_ephaptic_adapter_config_t config;
    nimcp_module_interface_t interface;

    /* Neural state arrays */
    float* membrane_potentials;         /**< Input potentials (mV) */
    float* field_effects;               /**< Computed field effects (mV) */
    float* distances;                   /**< Pairwise distances (placeholder) */

    /* State tracking */
    nimcp_ephaptic_adapter_state_t state;
    nimcp_ephaptic_adapter_stats_t stats;
    bool is_initialized;
};

//=============================================================================
// Module Interface Callbacks
//=============================================================================

static nimcp_layer_error_t ephaptic_init(void* module, void* config) {
    nimcp_ephaptic_adapter_t adapter = (nimcp_ephaptic_adapter_t)module;
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;

    if (config) {
        adapter->config = *(nimcp_ephaptic_adapter_config_t*)config;
    }

    /* Allocate arrays */
    uint32_t n = adapter->config.num_neurons;
    adapter->membrane_potentials = (float*)nimcp_calloc(n, sizeof(float));
    adapter->field_effects = (float*)nimcp_calloc(n, sizeof(float));

    if (!adapter->membrane_potentials || !adapter->field_effects) {
        return NIMCP_LAYER_ERR_NO_MEMORY;
    }

    /* Initialize with resting potential */
    for (uint32_t i = 0; i < n; i++) {
        adapter->membrane_potentials[i] = -65.0f;
    }

    adapter->is_initialized = true;
    adapter->state.is_active = true;

    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t ephaptic_shutdown(void* module) {
    nimcp_ephaptic_adapter_t adapter = (nimcp_ephaptic_adapter_t)module;
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;

    nimcp_free(adapter->membrane_potentials);
    nimcp_free(adapter->field_effects);
    nimcp_free(adapter->distances);

    adapter->membrane_potentials = NULL;
    adapter->field_effects = NULL;
    adapter->distances = NULL;

    adapter->is_initialized = false;
    adapter->state.is_active = false;

    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t ephaptic_update(void* module, float dt) {
    nimcp_ephaptic_adapter_t adapter = (nimcp_ephaptic_adapter_t)module;
    if (!adapter || !adapter->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;

    uint32_t n = adapter->config.num_neurons;
    float coupling = adapter->config.coupling_strength;
    float decay = adapter->config.decay_constant;

    /* Compute local field potential (sum of all potentials, weighted) */
    float lfp = 0.0f;
    float sum_field = 0.0f;
    uint32_t interaction_count = 0;

    /* Simple mean-field approximation for LFP */
    for (uint32_t i = 0; i < n; i++) {
        lfp += adapter->membrane_potentials[i];
    }
    lfp /= (float)n;
    adapter->state.lfp_amplitude = lfp + 65.0f;  /* Deviation from rest */

    /* Compute field effects on each neuron */
    /* Simplified model: each neuron feels the mean field */
    for (uint32_t i = 0; i < n; i++) {
        float v_i = adapter->membrane_potentials[i];

        /* Field effect is proportional to difference from mean */
        float field_contribution = 0.0f;

        /* For efficiency, use mean-field approximation */
        /* Each neuron is pushed toward the population mean */
        field_contribution = coupling * (lfp - v_i);

        /* Apply decay based on notional distance (simplified) */
        field_contribution *= expf(-1.0f / decay);

        /* Apply threshold */
        if (fabsf(field_contribution) < adapter->config.field_threshold) {
            field_contribution = 0.0f;
        }

        adapter->field_effects[i] = field_contribution;
        sum_field += fabsf(field_contribution);
        interaction_count++;
    }

    /* Update state */
    adapter->state.mean_field_strength = sum_field / (float)n;
    adapter->state.interactions_computed = interaction_count;

    /* Compute synchronization index (variance-based) */
    if (adapter->config.enable_synchronization) {
        float variance = 0.0f;
        for (uint32_t i = 0; i < n; i++) {
            float diff = adapter->membrane_potentials[i] - lfp;
            variance += diff * diff;
        }
        variance /= (float)n;
        /* Sync index: 1 when variance is 0, approaches 0 as variance increases */
        adapter->state.sync_index = 1.0f / (1.0f + variance / 100.0f);
    }

    adapter->stats.updates_processed++;
    adapter->stats.fields_computed += interaction_count;
    (void)dt;  /* dt used for future time-dependent effects */

    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t ephaptic_handle_message(void* module, const nimcp_layer_msg_t* msg) {
    nimcp_ephaptic_adapter_t adapter = (nimcp_ephaptic_adapter_t)module;
    if (!adapter || !msg) return NIMCP_LAYER_ERR_NULL_PTR;

    adapter->stats.messages_handled++;

    switch (msg->header.msg_type) {
        case NIMCP_LAYER_MSG_DATA_PUSH:
            /* Incoming membrane potentials from HH dynamics */
            if (msg->payload && msg->header.payload_size >= sizeof(float)) {
                uint32_t count = msg->header.payload_size / sizeof(float);
                if (count > adapter->config.num_neurons) {
                    count = adapter->config.num_neurons;
                }
                memcpy(adapter->membrane_potentials, msg->payload, count * sizeof(float));
            }
            break;

        case NIMCP_LAYER_MSG_MODULATE:
            /* Modulation affects coupling strength */
            if (msg->payload && msg->header.payload_size >= sizeof(float)) {
                float mod = *(float*)msg->payload;
                adapter->config.coupling_strength *= (1.0f + mod);
                /* Clamp to valid range */
                if (adapter->config.coupling_strength < 0.0f) {
                    adapter->config.coupling_strength = 0.0f;
                }
                if (adapter->config.coupling_strength > 1.0f) {
                    adapter->config.coupling_strength = 1.0f;
                }
            }
            break;

        default:
            break;
    }

    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t ephaptic_get_state(void* module, void* state_out, size_t* size) {
    nimcp_ephaptic_adapter_t adapter = (nimcp_ephaptic_adapter_t)module;
    if (!adapter || !state_out || !size) return NIMCP_LAYER_ERR_NULL_PTR;

    if (*size < sizeof(nimcp_ephaptic_adapter_state_t)) {
        *size = sizeof(nimcp_ephaptic_adapter_state_t);
        return NIMCP_LAYER_ERR_CAPACITY;
    }

    *(nimcp_ephaptic_adapter_state_t*)state_out = adapter->state;
    *size = sizeof(nimcp_ephaptic_adapter_state_t);

    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t ephaptic_set_state(void* module, const void* state, size_t size) {
    nimcp_ephaptic_adapter_t adapter = (nimcp_ephaptic_adapter_t)module;
    if (!adapter || !state) return NIMCP_LAYER_ERR_NULL_PTR;

    if (size < sizeof(nimcp_ephaptic_adapter_state_t)) {
        return NIMCP_LAYER_ERR_INVALID_MSG;
    }

    adapter->state = *(const nimcp_ephaptic_adapter_state_t*)state;

    return NIMCP_LAYER_OK;
}

static const char* ephaptic_get_name(void* module) {
    (void)module;
    return "Ephaptic_Coupling_Adapter";
}

//=============================================================================
// Public API Implementation
//=============================================================================

nimcp_ephaptic_adapter_config_t nimcp_ephaptic_adapter_default_config(void) {
    nimcp_ephaptic_adapter_config_t config = {
        .num_neurons = 100,
        .coupling_strength = NIMCP_OSCILLATION_COUPLING_DEFAULT,
        .decay_constant = 0.5f,      /* 0.5mm decay constant */
        .field_threshold = 0.01f,    /* 10 uV threshold */
        .enable_lfp_computation = true,
        .enable_synchronization = true,
        .enable_logging = false
    };
    return config;
}

nimcp_ephaptic_adapter_t nimcp_ephaptic_adapter_create(
    const nimcp_ephaptic_adapter_config_t* config
) {
    nimcp_ephaptic_adapter_t adapter = (nimcp_ephaptic_adapter_t)nimcp_calloc(
        1, sizeof(struct nimcp_ephaptic_adapter_struct));
    NIMCP_API_CHECK_ALLOC(adapter, "Failed to allocate ephaptic coupling adapter");

    adapter->config = config ? *config : nimcp_ephaptic_adapter_default_config();

    /* Set up module interface */
    adapter->interface.init = ephaptic_init;
    adapter->interface.shutdown = ephaptic_shutdown;
    adapter->interface.update = ephaptic_update;
    adapter->interface.handle_message = ephaptic_handle_message;
    adapter->interface.get_state = ephaptic_get_state;
    adapter->interface.set_state = ephaptic_set_state;
    adapter->interface.get_name = ephaptic_get_name;

    return adapter;
}

void nimcp_ephaptic_adapter_destroy(nimcp_ephaptic_adapter_t adapter) {
    if (!adapter) return;

    if (adapter->is_initialized) {
        ephaptic_shutdown(adapter);
    }

    nimcp_free(adapter);
}

nimcp_module_interface_t* nimcp_ephaptic_adapter_get_interface(
    nimcp_ephaptic_adapter_t adapter
) {
    return adapter ? &adapter->interface : NULL;
}

nimcp_layer_error_t nimcp_ephaptic_adapter_set_potentials(
    nimcp_ephaptic_adapter_t adapter,
    const float* potentials,
    uint32_t count
) {
    if (!adapter || !potentials) return NIMCP_LAYER_ERR_NULL_PTR;
    if (!adapter->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;

    if (count > adapter->config.num_neurons) {
        count = adapter->config.num_neurons;
    }

    memcpy(adapter->membrane_potentials, potentials, count * sizeof(float));

    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_ephaptic_adapter_compute_fields(
    nimcp_ephaptic_adapter_t adapter,
    float* field_effects_out,
    uint32_t max_effects,
    uint32_t* count_out
) {
    if (!adapter || !field_effects_out || !count_out) return NIMCP_LAYER_ERR_NULL_PTR;
    if (!adapter->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;

    uint32_t count = adapter->config.num_neurons;
    if (count > max_effects) count = max_effects;

    memcpy(field_effects_out, adapter->field_effects, count * sizeof(float));
    *count_out = count;

    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_ephaptic_adapter_get_lfp(
    nimcp_ephaptic_adapter_t adapter,
    float* lfp_out
) {
    if (!adapter || !lfp_out) return NIMCP_LAYER_ERR_NULL_PTR;
    *lfp_out = adapter->state.lfp_amplitude;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_ephaptic_adapter_get_state(
    nimcp_ephaptic_adapter_t adapter,
    nimcp_ephaptic_adapter_state_t* state_out
) {
    if (!adapter || !state_out) return NIMCP_LAYER_ERR_NULL_PTR;
    *state_out = adapter->state;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_ephaptic_adapter_get_stats(
    nimcp_ephaptic_adapter_t adapter,
    nimcp_ephaptic_adapter_stats_t* stats_out
) {
    if (!adapter || !stats_out) return NIMCP_LAYER_ERR_NULL_PTR;
    *stats_out = adapter->stats;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_ephaptic_adapter_reset_stats(nimcp_ephaptic_adapter_t adapter) {
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;
    memset(&adapter->stats, 0, sizeof(adapter->stats));
    return NIMCP_LAYER_OK;
}
