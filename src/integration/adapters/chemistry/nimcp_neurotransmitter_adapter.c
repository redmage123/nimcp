/**
 * @file nimcp_neurotransmitter_adapter.c
 * @brief Neurotransmitter Dynamics Adapter Implementation
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Implements neurotransmitter release/uptake for Chemistry layer
 * WHY:  Chemical signaling is the primary mode of neural communication
 * HOW:  Models vesicle release, diffusion, binding, and reuptake
 *
 * @author NIMCP Development Team
 */

#include "integration/adapters/chemistry/nimcp_neurotransmitter_adapter.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(neurotransmitter_adapter)

//=============================================================================
// Internal Structure
//=============================================================================

struct nimcp_nt_adapter_struct {
    nimcp_nt_adapter_config_t config;
    nimcp_module_interface_t interface;

    /* Synaptic state */
    float* cleft_conc;                  /**< Cleft [NT] per synapse (mM) */
    float* receptor_occupancy;          /**< Receptor binding per synapse */
    float* vesicle_pool;                /**< Available vesicles per synapse */
    float extrasynaptic;                /**< Extrasynaptic concentration */

    /* State tracking */
    nimcp_nt_adapter_state_t state;
    nimcp_nt_adapter_stats_t stats;
    bool is_initialized;
};

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Binding kinetics (Hill equation variant)
 */
static float binding_kinetics(float conc, float kd, float n) {
    float cn = powf(conc, n);
    float kdn = powf(kd, n);
    return cn / (cn + kdn);
}

//=============================================================================
// Module Interface Callbacks
//=============================================================================

static nimcp_layer_error_t nt_init(void* module, void* config) {
    nimcp_nt_adapter_t adapter = (nimcp_nt_adapter_t)module;
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;

    if (config) {
        adapter->config = *(nimcp_nt_adapter_config_t*)config;
    }

    /* Allocate arrays */
    uint32_t n = adapter->config.num_synapses;
    adapter->cleft_conc = (float*)nimcp_calloc(n, sizeof(float));
    adapter->receptor_occupancy = (float*)nimcp_calloc(n, sizeof(float));
    adapter->vesicle_pool = (float*)nimcp_calloc(n, sizeof(float));

    if (!adapter->cleft_conc || !adapter->receptor_occupancy || !adapter->vesicle_pool) {
        return NIMCP_LAYER_ERR_NO_MEMORY;
    }

    /* Initialize vesicle pools */
    for (uint32_t i = 0; i < n; i++) {
        adapter->cleft_conc[i] = 0.0f;
        adapter->receptor_occupancy[i] = 0.0f;
        adapter->vesicle_pool[i] = 10.0f;  /* ~10 readily releasable vesicles */
    }

    adapter->extrasynaptic = 0.0f;
    adapter->is_initialized = true;
    adapter->state.is_active = true;

    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t nt_shutdown(void* module) {
    nimcp_nt_adapter_t adapter = (nimcp_nt_adapter_t)module;
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;

    nimcp_free(adapter->cleft_conc);
    nimcp_free(adapter->receptor_occupancy);
    nimcp_free(adapter->vesicle_pool);

    adapter->cleft_conc = NULL;
    adapter->receptor_occupancy = NULL;
    adapter->vesicle_pool = NULL;

    adapter->is_initialized = false;
    adapter->state.is_active = false;

    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t nt_update(void* module, float dt) {
    nimcp_nt_adapter_t adapter = (nimcp_nt_adapter_t)module;
    if (!adapter || !adapter->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;

    uint32_t n = adapter->config.num_synapses;
    float reuptake_rate = adapter->config.reuptake_rate;
    float diffusion = adapter->config.diffusion_rate;
    float dt_ms = dt * 1000.0f;

    float sum_conc = 0.0f;
    float sum_occupancy = 0.0f;

    /* Update each synapse */
    for (uint32_t i = 0; i < n; i++) {
        float conc = adapter->cleft_conc[i];

        /* Receptor binding (fast kinetics) */
        /* Kd ~0.5 mM for glutamate at AMPA receptors */
        float kd = 0.5f;
        float target_occupancy = binding_kinetics(conc, kd, 1.5f);
        float tau_bind = 1.0f;  /* 1 ms binding time constant */
        adapter->receptor_occupancy[i] += (target_occupancy - adapter->receptor_occupancy[i]) / tau_bind * dt_ms;

        /* Reuptake removes NT from cleft */
        float reuptake = reuptake_rate * conc * dt_ms;
        conc -= reuptake;
        if (conc < 0.0f) conc = 0.0f;
        adapter->stats.reuptake_events += (reuptake > 0.001f) ? 1 : 0;

        /* Spillover to extrasynaptic space */
        if (adapter->config.enable_spillover && conc > 0.1f) {
            float spillover = diffusion * conc * dt_ms;
            adapter->extrasynaptic += spillover;
            conc -= spillover;
        }

        /* Replenish vesicle pool (slow process) */
        if (adapter->vesicle_pool[i] < 10.0f) {
            adapter->vesicle_pool[i] += 0.1f * dt_ms;  /* ~100ms to replenish 1 vesicle */
            if (adapter->vesicle_pool[i] > 10.0f) {
                adapter->vesicle_pool[i] = 10.0f;
            }
        }

        adapter->cleft_conc[i] = conc;
        sum_conc += conc;
        sum_occupancy += adapter->receptor_occupancy[i];
    }

    /* Extrasynaptic clearance */
    adapter->extrasynaptic *= expf(-0.1f * dt_ms);

    /* Update state */
    adapter->state.mean_cleft_conc_mm = sum_conc / (float)n;
    adapter->state.mean_receptor_occupancy = sum_occupancy / (float)n;
    adapter->state.extrasynaptic_conc_mm = adapter->extrasynaptic;

    adapter->stats.updates_processed++;

    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t nt_handle_message(void* module, const nimcp_layer_msg_t* msg) {
    nimcp_nt_adapter_t adapter = (nimcp_nt_adapter_t)module;
    if (!adapter || !msg) return NIMCP_LAYER_ERR_NULL_PTR;

    adapter->stats.messages_handled++;

    switch (msg->header.msg_type) {
        case NIMCP_LAYER_MSG_DATA_PUSH:
            /* Incoming spike data triggers release */
            if (msg->payload && msg->header.payload_size >= sizeof(float)) {
                float calcium = *(float*)msg->payload;
                /* Release probability depends on calcium */
                float release_prob = adapter->config.release_probability *
                                     (1.0f + calcium * 0.01f);
                if (release_prob > 1.0f) release_prob = 1.0f;

                /* Probabilistic release at each synapse */
                for (uint32_t i = 0; i < adapter->config.num_synapses; i++) {
                    if (adapter->vesicle_pool[i] > 0.0f) {
                        /* Simple threshold model */
                        if (calcium > 100.0f) {  /* Ca threshold ~100 nM */
                            float release = adapter->config.vesicle_content_mm;
                            adapter->cleft_conc[i] += release;
                            adapter->vesicle_pool[i] -= 1.0f;
                            adapter->stats.vesicles_released++;
                            adapter->state.release_events++;
                        }
                    }
                }
            }
            break;

        case NIMCP_LAYER_MSG_EXCITE:
            /* Direct excitation = guaranteed release */
            for (uint32_t i = 0; i < adapter->config.num_synapses; i++) {
                if (adapter->vesicle_pool[i] > 0.0f) {
                    adapter->cleft_conc[i] += adapter->config.vesicle_content_mm;
                    adapter->vesicle_pool[i] -= 1.0f;
                    adapter->stats.vesicles_released++;
                    adapter->state.release_events++;
                }
            }
            break;

        default:
            break;
    }

    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t nt_get_state(void* module, void* state_out, size_t* size) {
    nimcp_nt_adapter_t adapter = (nimcp_nt_adapter_t)module;
    if (!adapter || !state_out || !size) return NIMCP_LAYER_ERR_NULL_PTR;

    if (*size < sizeof(nimcp_nt_adapter_state_t)) {
        *size = sizeof(nimcp_nt_adapter_state_t);
        return NIMCP_LAYER_ERR_CAPACITY;
    }

    *(nimcp_nt_adapter_state_t*)state_out = adapter->state;
    *size = sizeof(nimcp_nt_adapter_state_t);

    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t nt_set_state(void* module, const void* state, size_t size) {
    nimcp_nt_adapter_t adapter = (nimcp_nt_adapter_t)module;
    if (!adapter || !state) return NIMCP_LAYER_ERR_NULL_PTR;

    if (size < sizeof(nimcp_nt_adapter_state_t)) {
        return NIMCP_LAYER_ERR_INVALID_MSG;
    }

    adapter->state = *(const nimcp_nt_adapter_state_t*)state;

    return NIMCP_LAYER_OK;
}

static const char* nt_get_name(void* module) {
    (void)module;
    return "Neurotransmitter_Adapter";
}

//=============================================================================
// Public API Implementation
//=============================================================================

nimcp_nt_adapter_config_t nimcp_nt_adapter_default_config(void) {
    nimcp_nt_adapter_config_t config = {
        .num_synapses = 100,
        .release_probability = 0.3f,    /* ~30% release probability */
        .cleft_volume_fl = 0.2f,        /* ~0.2 femtoliters */
        .reuptake_rate = 0.5f,          /* 500 ms reuptake time constant */
        .diffusion_rate = 0.1f,
        .vesicle_content_mm = 1.0f,     /* ~1 mM equivalent per vesicle */
        .default_nt = NT_GLUTAMATE,
        .enable_spillover = true,
        .enable_logging = false
    };
    return config;
}

nimcp_nt_adapter_t nimcp_nt_adapter_create(
    const nimcp_nt_adapter_config_t* config
) {
    nimcp_nt_adapter_t adapter = (nimcp_nt_adapter_t)nimcp_calloc(
        1, sizeof(struct nimcp_nt_adapter_struct));
    NIMCP_API_CHECK_ALLOC(adapter, "Failed to allocate neurotransmitter adapter");

    adapter->config = config ? *config : nimcp_nt_adapter_default_config();

    /* Set up module interface */
    adapter->interface.init = nt_init;
    adapter->interface.shutdown = nt_shutdown;
    adapter->interface.update = nt_update;
    adapter->interface.handle_message = nt_handle_message;
    adapter->interface.get_state = nt_get_state;
    adapter->interface.set_state = nt_set_state;
    adapter->interface.get_name = nt_get_name;

    return adapter;
}

void nimcp_nt_adapter_destroy(nimcp_nt_adapter_t adapter) {
    if (!adapter) return;

    if (adapter->is_initialized) {
        nt_shutdown(adapter);
    }

    nimcp_free(adapter);
}

nimcp_module_interface_t* nimcp_nt_adapter_get_interface(
    nimcp_nt_adapter_t adapter
) {
    return adapter ? &adapter->interface : NULL;
}

nimcp_layer_error_t nimcp_nt_adapter_release(
    nimcp_nt_adapter_t adapter,
    int synapse_idx,
    uint32_t num_vesicles
) {
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;
    if (!adapter->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;

    if (synapse_idx < 0) {
        /* Release at all synapses */
        for (uint32_t i = 0; i < adapter->config.num_synapses; i++) {
            uint32_t release = num_vesicles;
            if (release > (uint32_t)adapter->vesicle_pool[i]) {
                release = (uint32_t)adapter->vesicle_pool[i];
            }
            adapter->cleft_conc[i] += release * adapter->config.vesicle_content_mm;
            adapter->vesicle_pool[i] -= release;
            adapter->stats.vesicles_released += release;
            adapter->state.release_events += release;
        }
    } else if ((uint32_t)synapse_idx < adapter->config.num_synapses) {
        uint32_t release = num_vesicles;
        if (release > (uint32_t)adapter->vesicle_pool[synapse_idx]) {
            release = (uint32_t)adapter->vesicle_pool[synapse_idx];
        }
        adapter->cleft_conc[synapse_idx] += release * adapter->config.vesicle_content_mm;
        adapter->vesicle_pool[synapse_idx] -= release;
        adapter->stats.vesicles_released += release;
        adapter->state.release_events += release;
    } else {
        return NIMCP_LAYER_ERR_INVALID_MODULE;
    }

    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_nt_adapter_get_concentrations(
    nimcp_nt_adapter_t adapter,
    float* conc_out,
    uint32_t max_count,
    uint32_t* count_out
) {
    if (!adapter || !conc_out || !count_out) return NIMCP_LAYER_ERR_NULL_PTR;
    if (!adapter->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;

    uint32_t count = adapter->config.num_synapses;
    if (count > max_count) count = max_count;

    memcpy(conc_out, adapter->cleft_conc, count * sizeof(float));
    *count_out = count;

    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_nt_adapter_get_occupancy(
    nimcp_nt_adapter_t adapter,
    float* occupancy_out,
    uint32_t max_count,
    uint32_t* count_out
) {
    if (!adapter || !occupancy_out || !count_out) return NIMCP_LAYER_ERR_NULL_PTR;
    if (!adapter->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;

    uint32_t count = adapter->config.num_synapses;
    if (count > max_count) count = max_count;

    memcpy(occupancy_out, adapter->receptor_occupancy, count * sizeof(float));
    *count_out = count;

    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_nt_adapter_get_state(
    nimcp_nt_adapter_t adapter,
    nimcp_nt_adapter_state_t* state_out
) {
    if (!adapter || !state_out) return NIMCP_LAYER_ERR_NULL_PTR;
    *state_out = adapter->state;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_nt_adapter_get_stats(
    nimcp_nt_adapter_t adapter,
    nimcp_nt_adapter_stats_t* stats_out
) {
    if (!adapter || !stats_out) return NIMCP_LAYER_ERR_NULL_PTR;
    *stats_out = adapter->stats;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_nt_adapter_reset_stats(nimcp_nt_adapter_t adapter) {
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;
    memset(&adapter->stats, 0, sizeof(adapter->stats));
    return NIMCP_LAYER_OK;
}
