/**
 * @file nimcp_hh_dynamics_adapter.c
 * @brief Hodgkin-Huxley Dynamics Adapter Implementation
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Implements adapter wrapping neuron models for Physics layer
 * WHY:  Enables existing neuron models to participate in layer messaging
 * HOW:  Implements nimcp_module_interface_t callbacks
 *
 * @author NIMCP Development Team
 */

#include "integration/adapters/physics/nimcp_hh_dynamics_adapter.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_neural_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(hh_dynamics_adapter)

//=============================================================================
// Internal Structure
//=============================================================================

struct nimcp_hh_adapter_struct {
    nimcp_hh_adapter_config_t config;
    nimcp_module_interface_t interface;

    /* Neuron state arrays */
    float* membrane_potentials;         /**< Membrane potentials (mV) */
    float* injected_currents;           /**< Current injections (nA) */
    bool* spike_flags;                  /**< Spike output flags */

    /* State tracking */
    nimcp_hh_adapter_state_t state;
    nimcp_hh_adapter_stats_t stats;
    bool is_initialized;
};

//=============================================================================
// Module Interface Callbacks
//=============================================================================

static nimcp_layer_error_t hh_init(void* module, void* config) {
    nimcp_hh_adapter_t adapter = (nimcp_hh_adapter_t)module;
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;

    if (config) {
        adapter->config = *(nimcp_hh_adapter_config_t*)config;
    }

    /* Allocate neuron arrays */
    uint32_t n = adapter->config.num_neurons;
    adapter->membrane_potentials = (float*)nimcp_calloc(n, sizeof(float));
    adapter->injected_currents = (float*)nimcp_calloc(n, sizeof(float));
    adapter->spike_flags = (bool*)nimcp_calloc(n, sizeof(bool));

    if (!adapter->membrane_potentials || !adapter->injected_currents || !adapter->spike_flags) {
        return NIMCP_LAYER_ERR_NO_MEMORY;
    }

    /* Initialize membrane potentials to resting potential (-65 mV) */
    for (uint32_t i = 0; i < n; i++) {
        adapter->membrane_potentials[i] = -65.0f;
    }

    adapter->is_initialized = true;
    adapter->state.is_active = true;

    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t hh_shutdown(void* module) {
    nimcp_hh_adapter_t adapter = (nimcp_hh_adapter_t)module;
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;

    /* Free neuron arrays */
    nimcp_free(adapter->membrane_potentials);
    nimcp_free(adapter->injected_currents);
    nimcp_free(adapter->spike_flags);

    adapter->membrane_potentials = NULL;
    adapter->injected_currents = NULL;
    adapter->spike_flags = NULL;

    adapter->is_initialized = false;
    adapter->state.is_active = false;

    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t hh_update(void* module, float dt) {
    nimcp_hh_adapter_t adapter = (nimcp_hh_adapter_t)module;
    if (!adapter || !adapter->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;

    uint32_t n = adapter->config.num_neurons;
    float dt_ms = dt * 1000.0f;  /* Convert to ms */

    /* Simple Izhikevich-like dynamics for demonstration */
    /* In production, this would call the actual neuron model API */

    float sum_v = 0.0f;
    uint32_t spike_count = 0;

    for (uint32_t i = 0; i < n; i++) {
        float v = adapter->membrane_potentials[i];
        float I = adapter->injected_currents[i];

        /* Simplified leaky integrate-and-fire dynamics */
        float tau_m = NIMCP_MEMBRANE_TAU_MS;  /* Membrane time constant (ms) */
        float v_rest = -65.0f;
        float v_thresh = -50.0f;
        float v_reset = -70.0f;
        float R = 10.0f;  /* Input resistance (MOhm) */

        /* dV/dt = (V_rest - V + R*I) / tau_m */
        float dv = (v_rest - v + R * I) / tau_m;
        v += dv * dt_ms;

        /* Check for spike */
        adapter->spike_flags[i] = false;
        if (v >= v_thresh) {
            v = v_reset;
            adapter->spike_flags[i] = true;
            spike_count++;
            adapter->state.total_spikes++;
            adapter->stats.spikes_generated++;
        }

        adapter->membrane_potentials[i] = v;
        sum_v += v;

        /* Clear injected current after use */
        adapter->injected_currents[i] = 0.0f;
    }

    /* Update state */
    adapter->state.mean_membrane_potential = sum_v / (float)n;
    adapter->state.mean_firing_rate = (float)spike_count / (dt * (float)n) * 1000.0f;  /* Hz */

    /* Energy tracking (simplified) */
    if (adapter->config.enable_energy_tracking) {
        adapter->state.total_energy_consumed += (float)spike_count * 0.001f;  /* Arbitrary units */
    }

    adapter->stats.updates_processed++;

    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t hh_handle_message(void* module, const nimcp_layer_msg_t* msg) {
    nimcp_hh_adapter_t adapter = (nimcp_hh_adapter_t)module;
    if (!adapter || !msg) return NIMCP_LAYER_ERR_NULL_PTR;

    adapter->stats.messages_handled++;

    /* Handle physics layer messages */
    switch (msg->header.msg_type) {
        case NIMCP_LAYER_MSG_MODULATE:
            /* Modulation affects excitability - adjust threshold */
            break;

        case NIMCP_LAYER_MSG_DATA_PUSH:
            /* Incoming current injection */
            if (msg->payload && msg->header.payload_size >= sizeof(float)) {
                float current = *(float*)msg->payload;
                /* Distribute to all neurons */
                for (uint32_t i = 0; i < adapter->config.num_neurons; i++) {
                    adapter->injected_currents[i] += current;
                }
            }
            break;

        default:
            /* Unknown message type - ignore */
            break;
    }

    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t hh_get_state(void* module, void* state_out, size_t* size) {
    nimcp_hh_adapter_t adapter = (nimcp_hh_adapter_t)module;
    if (!adapter || !state_out || !size) return NIMCP_LAYER_ERR_NULL_PTR;

    if (*size < sizeof(nimcp_hh_adapter_state_t)) {
        *size = sizeof(nimcp_hh_adapter_state_t);
        return NIMCP_LAYER_ERR_CAPACITY;
    }

    *(nimcp_hh_adapter_state_t*)state_out = adapter->state;
    *size = sizeof(nimcp_hh_adapter_state_t);

    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t hh_set_state(void* module, const void* state, size_t size) {
    nimcp_hh_adapter_t adapter = (nimcp_hh_adapter_t)module;
    if (!adapter || !state) return NIMCP_LAYER_ERR_NULL_PTR;

    if (size < sizeof(nimcp_hh_adapter_state_t)) {
        return NIMCP_LAYER_ERR_INVALID_MSG;
    }

    adapter->state = *(const nimcp_hh_adapter_state_t*)state;

    return NIMCP_LAYER_OK;
}

static const char* hh_get_name(void* module) {
    (void)module;
    return "HH_Dynamics_Adapter";
}

//=============================================================================
// Public API Implementation
//=============================================================================

nimcp_hh_adapter_config_t nimcp_hh_adapter_default_config(void) {
    nimcp_hh_adapter_config_t config = {
        .model_type = NEURON_MODEL_IZHIKEVICH,
        .num_neurons = 100,
        .dt_ms = 0.5f,
        .enable_detailed_biophysics = false,
        .enable_energy_tracking = true,
        .enable_logging = false
    };
    return config;
}

nimcp_hh_adapter_t nimcp_hh_adapter_create(const nimcp_hh_adapter_config_t* config) {
    nimcp_hh_adapter_t adapter = (nimcp_hh_adapter_t)nimcp_calloc(1, sizeof(struct nimcp_hh_adapter_struct));
    NIMCP_API_CHECK_ALLOC(adapter, "Failed to allocate HH dynamics adapter");

    adapter->config = config ? *config : nimcp_hh_adapter_default_config();

    /* Set up module interface */
    adapter->interface.init = hh_init;
    adapter->interface.shutdown = hh_shutdown;
    adapter->interface.update = hh_update;
    adapter->interface.handle_message = hh_handle_message;
    adapter->interface.get_state = hh_get_state;
    adapter->interface.set_state = hh_set_state;
    adapter->interface.get_name = hh_get_name;

    return adapter;
}

void nimcp_hh_adapter_destroy(nimcp_hh_adapter_t adapter) {
    if (!adapter) return;

    if (adapter->is_initialized) {
        hh_shutdown(adapter);
    }

    nimcp_free(adapter);
}

nimcp_module_interface_t* nimcp_hh_adapter_get_interface(nimcp_hh_adapter_t adapter) {
    return adapter ? &adapter->interface : NULL;
}

nimcp_layer_error_t nimcp_hh_adapter_inject_current(
    nimcp_hh_adapter_t adapter,
    int neuron_idx,
    float current_nA
) {
    if (!adapter || !adapter->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;

    if (neuron_idx < 0) {
        /* Inject to all neurons */
        for (uint32_t i = 0; i < adapter->config.num_neurons; i++) {
            adapter->injected_currents[i] += current_nA;
        }
    } else if ((uint32_t)neuron_idx < adapter->config.num_neurons) {
        adapter->injected_currents[neuron_idx] += current_nA;
    } else {
        return NIMCP_LAYER_ERR_INVALID_MODULE;
    }

    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_hh_adapter_get_spikes(
    nimcp_hh_adapter_t adapter,
    bool* spikes_out,
    uint32_t max_spikes,
    uint32_t* count_out
) {
    if (!adapter || !spikes_out || !count_out) return NIMCP_LAYER_ERR_NULL_PTR;
    if (!adapter->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;

    uint32_t count = adapter->config.num_neurons;
    if (count > max_spikes) count = max_spikes;

    memcpy(spikes_out, adapter->spike_flags, count * sizeof(bool));
    *count_out = count;

    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_hh_adapter_get_potentials(
    nimcp_hh_adapter_t adapter,
    float* potentials_out,
    uint32_t max_potentials,
    uint32_t* count_out
) {
    if (!adapter || !potentials_out || !count_out) return NIMCP_LAYER_ERR_NULL_PTR;
    if (!adapter->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;

    uint32_t count = adapter->config.num_neurons;
    if (count > max_potentials) count = max_potentials;

    memcpy(potentials_out, adapter->membrane_potentials, count * sizeof(float));
    *count_out = count;

    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_hh_adapter_get_state(
    nimcp_hh_adapter_t adapter,
    nimcp_hh_adapter_state_t* state_out
) {
    if (!adapter || !state_out) return NIMCP_LAYER_ERR_NULL_PTR;
    *state_out = adapter->state;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_hh_adapter_get_stats(
    nimcp_hh_adapter_t adapter,
    nimcp_hh_adapter_stats_t* stats_out
) {
    if (!adapter || !stats_out) return NIMCP_LAYER_ERR_NULL_PTR;
    *stats_out = adapter->stats;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_hh_adapter_reset_stats(nimcp_hh_adapter_t adapter) {
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;
    memset(&adapter->stats, 0, sizeof(adapter->stats));
    return NIMCP_LAYER_OK;
}
