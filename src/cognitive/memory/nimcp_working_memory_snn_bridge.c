/*
 * DEPRECATED — STATUE (audit 2026-04-30)
 *
 * wm_snn_create is called once at working_memory_part_lifecycle.c:248
 * and the bridge struct is destroyed at lifecycle teardown, but no code
 * path calls wm_snn_step / wm_snn_simulate / wm_snn_store_in_slot /
 * wm_snn_recall_from_slot during the brain hot path. The bridge is
 * lifecycle-only — created but never stepped. The lightweight CSR pop
 * migration may supersede this. Either wire a consumer or delete
 * before the next major version. Do not extend.
 */

/**
 * @file nimcp_working_memory_snn_bridge.c
 * @brief Working Memory - SNN Bidirectional Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-06
 */

#include "cognitive/memory/nimcp_working_memory_snn_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_config.h"
#include "core/neuron_types/nimcp_neuron_types.h"
#include "core/synapse_types/nimcp_synapse_types.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"

#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

/* G8: guarded lightweight-pop assignment. See attention bridge for rationale. */
static int g8_add_lw_pop(snn_network_t* snn, uint32_t n, neuron_type_t t,
                         const char* name, uint32_t* out)
{
    int id = snn_network_add_population_lightweight(snn, n, t, name);
    if (id < 0) { return -1; }
    *out = (uint32_t)id;
    return 0;
}

//=============================================================================
// Module Constants
//=============================================================================

#define LOG_MODULE "WM_SNN"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(working_memory_snn_bridge, MESH_ADAPTER_CATEGORY_MEMORY)

/* Security subsystem setters (Phase 1: Audit Gap Remediation) */
//=============================================================================
// Internal Structures
//=============================================================================

struct wm_snn_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */

    wm_snn_config_t config;

    /* SNN network */
    snn_network_t* snn;
    bool owns_snn;

    /* Population IDs */
    uint32_t* slot_pops;             /* One population per slot */
    uint32_t hidden_pop;
    uint32_t output_pop;
    uint32_t inhibition_pop;

    /* Per-slot state */
    wm_slot_state_t* slot_states;
    float* slot_buffer;              /* Temporary buffer for encoding */
    float* output_buffer;            /* Temporary buffer for decoding */

    /* Bridge state */
    wm_snn_state_t state;
    wm_snn_stats_t stats;
    uint64_t current_time_ms;

    /* Callbacks */
    wm_snn_spike_callback_t spike_callback;
    void* spike_user_data;
    wm_snn_encoding_callback_t encoding_callback;
    void* encoding_user_data;
    wm_snn_retrieval_callback_t retrieval_callback;
    void* retrieval_user_data;

    /* Bio-async */
    bool bio_async_connected;
};

BRIDGE_DEFINE_SECURITY_SETTERS(wm_snn_bridge)

//=============================================================================
// Default Configuration
//=============================================================================

wm_snn_config_t wm_snn_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    working_memory_snn_bridge_heartbeat("working_memo_wm_snn_config_defaul", 0.0f);


    return (wm_snn_config_t) {
        .max_slots = 8,
        .neurons_per_slot = WM_SNN_NEURONS_PER_SLOT,
        .slot_dim = WM_SNN_SLOT_DIM,
        .hidden_dim = WM_SNN_HIDDEN_DIM,

        .dt_ms = 1.0f,
        .maintenance_rate = 20.0f,
        .decay_tau_ms = 500.0f,

        .encoding_type = WM_SNN_ENCODE_RATE,
        .encoding_gain = 1.0f,
        .noise_stddev = 0.05f,

        .enable_lateral_inhibition = true,
        .inhibition_strength = 0.3f,
        .enable_recurrence = true,
        .recurrence_strength = 0.4f,

        .enable_bio_async = false
    };
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

wm_snn_bridge_t* wm_snn_create(const wm_snn_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    working_memory_snn_bridge_heartbeat("working_memo_wm_snn_create", 0.0f);


    wm_snn_bridge_t* bridge = nimcp_calloc(1, sizeof(wm_snn_bridge_t));
    if (!bridge) {
        NIMCP_LOG_ERROR(LOG_MODULE, "Failed to allocate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    /* Apply configuration */
    bridge->config = config ? *config : wm_snn_config_default();

    /* Validate */
    if (bridge->config.max_slots == 0 ||
        bridge->config.max_slots > WM_SNN_MAX_SLOTS) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wm_snn_create: operation failed");
        return NULL;
    }

    /* Initialize bridge base infrastructure (includes mutex) */
    if (bridge_base_init(&bridge->base, 0, "wm_snn") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "wm_snn_create: validation failed");
        return NULL;
    }

    /* Create SNN network */
    snn_config_t snn_config;
    uint32_t input_dim = bridge->config.max_slots * bridge->config.neurons_per_slot;

    snn_config_feedforward(&snn_config,
        input_dim,
        bridge->config.hidden_dim,
        bridge->config.max_slots);

    snn_config.dt = bridge->config.dt_ms;
    snn_config.enable_bio_async = bridge->config.enable_bio_async;
    snn_config.n_populations = 0;

    bridge->snn = snn_network_create(&snn_config);
    if (!bridge->snn) {
        NIMCP_LOG_ERROR(LOG_MODULE, "Failed to create SNN network");
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "wm_snn_create: bridge->snn is NULL");
        return NULL;
    }
    bridge->owns_snn = true;

    /* Allocate population IDs */
    bridge->slot_pops = nimcp_calloc(bridge->config.max_slots, sizeof(uint32_t));
    if (!bridge->slot_pops) {
        wm_snn_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "wm_snn_create: bridge->slot_pops is NULL");
        return NULL;
    }

    /* Create populations for each memory slot (G8: guarded lightweight) */
    char pop_name[NIMCP_ID_BUFFER_SIZE];
    for (uint32_t s = 0; s < bridge->config.max_slots; s++) {
        /* Phase 8: Loop progress heartbeat */
        if ((s & 0xFF) == 0 && bridge->config.max_slots > 256) {
            working_memory_snn_bridge_heartbeat("working_memo_loop",
                             (float)(s + 1) / (float)bridge->config.max_slots);
        }

        snprintf(pop_name, sizeof(pop_name), "wm_slot_%u", s);
        if (g8_add_lw_pop(bridge->snn, bridge->config.neurons_per_slot,
                          NEURON_GENERIC_LIF, pop_name,
                          &bridge->slot_pops[s]) != 0) {
            NIMCP_LOG_ERROR(LOG_MODULE, "wm_snn_create: slot_pop[%u] alloc failed", s);
            wm_snn_destroy(bridge);
            return NULL;
        }
    }

    /* Hidden population */
    if (g8_add_lw_pop(bridge->snn, bridge->config.hidden_dim,
                      NEURON_GENERIC_LIF, "wm_hidden",
                      &bridge->hidden_pop) != 0) {
        NIMCP_LOG_ERROR(LOG_MODULE, "wm_snn_create: hidden_pop alloc failed");
        wm_snn_destroy(bridge);
        return NULL;
    }

    /* Output population (one neuron per slot for retrieval) */
    if (g8_add_lw_pop(bridge->snn, bridge->config.max_slots,
                      NEURON_GENERIC_LIF, "wm_output",
                      &bridge->output_pop) != 0) {
        NIMCP_LOG_ERROR(LOG_MODULE, "wm_snn_create: output_pop alloc failed");
        wm_snn_destroy(bridge);
        return NULL;
    }

    /* Connect slot populations to hidden */
    for (uint32_t s = 0; s < bridge->config.max_slots; s++) {
        /* Phase 8: Loop progress heartbeat */
        if ((s & 0xFF) == 0 && bridge->config.max_slots > 256) {
            working_memory_snn_bridge_heartbeat("working_memo_loop",
                             (float)(s + 1) / (float)bridge->config.max_slots);
        }

        snn_network_connect_populations(bridge->snn,
            bridge->slot_pops[s], bridge->hidden_pop,
            SNN_TOPO_RANDOM, 0.3f, SYNAPSE_AMPA, 0.5f, 0.1f);
    }

    /* Hidden to output */
    snn_network_connect_populations(bridge->snn,
        bridge->hidden_pop, bridge->output_pop,
        SNN_TOPO_FEEDFORWARD, 1.0f, SYNAPSE_AMPA, 0.5f, 0.1f);

    /* Recurrent connections for maintenance */
    if (bridge->config.enable_recurrence) {
        for (uint32_t s = 0; s < bridge->config.max_slots; s++) {
            /* Phase 8: Loop progress heartbeat */
            if ((s & 0xFF) == 0 && bridge->config.max_slots > 256) {
                working_memory_snn_bridge_heartbeat("working_memo_loop",
                                 (float)(s + 1) / (float)bridge->config.max_slots);
            }

            snn_network_connect_populations(bridge->snn,
                bridge->slot_pops[s], bridge->slot_pops[s],
                SNN_TOPO_RANDOM, 0.5f, SYNAPSE_AMPA,
                bridge->config.recurrence_strength, 0.05f);
        }
    }

    /* Lateral inhibition between slots */
    if (bridge->config.enable_lateral_inhibition) {
        if (g8_add_lw_pop(bridge->snn, bridge->config.max_slots,
                          NEURON_GENERIC_LIF, "wm_inhibition",
                          &bridge->inhibition_pop) != 0) {
            NIMCP_LOG_ERROR(LOG_MODULE, "wm_snn_create: inhibition_pop alloc failed");
            wm_snn_destroy(bridge);
            return NULL;
        }

        for (uint32_t s = 0; s < bridge->config.max_slots; s++) {
            /* Phase 8: Loop progress heartbeat */
            if ((s & 0xFF) == 0 && bridge->config.max_slots > 256) {
                working_memory_snn_bridge_heartbeat("working_memo_loop",
                                 (float)(s + 1) / (float)bridge->config.max_slots);
            }

            /* Slot -> inhibition */
            snn_network_connect_populations(bridge->snn,
                bridge->slot_pops[s], bridge->inhibition_pop,
                SNN_TOPO_FEEDFORWARD, 1.0f, SYNAPSE_AMPA, 0.3f, 0.05f);

            /* Inhibition -> all other slots */
            for (uint32_t t = 0; t < bridge->config.max_slots; t++) {
                /* Phase 8: Loop progress heartbeat */
                if ((t & 0xFF) == 0 && bridge->config.max_slots > 256) {
                    working_memory_snn_bridge_heartbeat("working_memo_loop",
                                     (float)(t + 1) / (float)bridge->config.max_slots);
                }

                if (t != s) {
                    snn_network_connect_populations(bridge->snn,
                        bridge->inhibition_pop, bridge->slot_pops[t],
                        SNN_TOPO_RANDOM, 0.3f, SYNAPSE_GABA_A,
                        -bridge->config.inhibition_strength, 0.05f);
                }
            }
        }
    }

    /* G8/H1 input shunt: dense input_pop (id 0) → each slot_pop so
     * snn_network_set_inputs reaches the lightweight graph. */
    for (uint32_t s = 0; s < bridge->config.max_slots; s++) {
        snn_network_connect_populations(bridge->snn,
            /*src=dense input_pop*/ 0, bridge->slot_pops[s],
            SNN_TOPO_RANDOM, 0.3f, SYNAPSE_AMPA, 0.4f, 0.1f);
    }

    /* G8/H1 output shunt: bridge's output_pop → dense output_pop (id 2). */
    snn_network_connect_populations(bridge->snn,
        bridge->output_pop, /*dst=dense output_pop*/ 2,
        SNN_TOPO_FEEDFORWARD, 1.0f, SYNAPSE_AMPA, 0.7f, 0.05f);

    /* G8: finalize lightweight CSR after all connections built. */
    snn_network_finalize_connections(bridge->snn);

    /* Allocate per-slot state */
    bridge->slot_states = nimcp_calloc(bridge->config.max_slots,
                                        sizeof(wm_slot_state_t));
    if (!bridge->slot_states) {
        wm_snn_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "wm_snn_create: bridge->slot_states is NULL");
        return NULL;
    }

    /* Allocate buffers */
    bridge->slot_buffer = nimcp_calloc(input_dim, sizeof(float));
    bridge->output_buffer = nimcp_calloc(bridge->config.max_slots, sizeof(float));
    if (!bridge->slot_buffer || !bridge->output_buffer) {
        wm_snn_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "wm_snn_create: required parameter is NULL (bridge->slot_buffer, bridge->output_buffer)");
        return NULL;
    }

    /* Initialize state */
    bridge->state = WM_SNN_STATE_IDLE;
    bridge->current_time_ms = 0;

    NIMCP_LOG_INFO(LOG_MODULE, "Created WM-SNN bridge with %u slots",
                   bridge->config.max_slots);

    NIMCP_LOGGING_INFO("Created %s bridge", "working_memory_snn");
    return bridge;
}

void wm_snn_destroy(wm_snn_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "working_memory_snn");

    /* Phase 8: Heartbeat at operation start */
    working_memory_snn_bridge_heartbeat("working_memo_wm_snn_destroy", 0.0f);


    if (bridge->snn && bridge->owns_snn) {
        snn_network_destroy(bridge->snn);
    }

    if (bridge->slot_pops) nimcp_free(bridge->slot_pops);
    if (bridge->slot_states) nimcp_free(bridge->slot_states);
    if (bridge->slot_buffer) nimcp_free(bridge->slot_buffer);
    if (bridge->output_buffer) nimcp_free(bridge->output_buffer);

    /* Cleanup base bridge infrastructure */
    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge);
    bridge = NULL;
}

int wm_snn_reset(wm_snn_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    working_memory_snn_bridge_heartbeat("working_memo_wm_snn_reset", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset slot states */
    memset(bridge->slot_states, 0,
           bridge->config.max_slots * sizeof(wm_slot_state_t));

    /* Reset SNN */
    if (bridge->snn) {
        snn_network_reset(bridge->snn);
    }

    /* Clear buffers */
    uint32_t input_dim = bridge->config.max_slots * bridge->config.neurons_per_slot;
    memset(bridge->slot_buffer, 0, input_dim * sizeof(float));
    memset(bridge->output_buffer, 0, bridge->config.max_slots * sizeof(float));

    bridge->state = WM_SNN_STATE_IDLE;
    bridge->current_time_ms = 0;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Encoding Functions
//=============================================================================

int wm_snn_encode_item(
    wm_snn_bridge_t* bridge,
    uint32_t slot,
    const float* features,
    uint32_t feature_count,
    float salience)
{
    if (!bridge || !features || slot >= bridge->config.max_slots) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wm_snn_encode_item: required parameter is NULL (bridge, features)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    working_memory_snn_bridge_heartbeat("working_memo_wm_snn_encode_item", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, features, sizeof(*features));

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->state = WM_SNN_STATE_ENCODING;

    /* Calculate spike pattern for this slot */
    uint32_t neurons = bridge->config.neurons_per_slot;
    uint32_t offset = slot * neurons;
    int spike_count = 0;

    /* Map features to neuron firing rates */
    for (uint32_t n = 0; n < neurons; n++) {
        /* Phase 8: Loop progress heartbeat */
        if ((n & 0xFF) == 0 && neurons > 256) {
            working_memory_snn_bridge_heartbeat("working_memo_loop",
                             (float)(n + 1) / (float)neurons);
        }

        float rate = 0.0f;

        /* Average features that map to this neuron */
        uint32_t feat_start = (n * feature_count) / neurons;
        uint32_t feat_end = ((n + 1) * feature_count) / neurons;
        if (feat_end > feature_count) feat_end = feature_count;

        for (uint32_t f = feat_start; f < feat_end; f++) {
            rate += features[f];
        }
        if (feat_end > feat_start) {
            rate /= (float)(feat_end - feat_start);
        }

        /* Apply encoding gain and salience */
        rate = nimcp_clampf(rate * bridge->config.encoding_gain * salience, 0.0f, 1.0f);
        bridge->slot_buffer[offset + n] = rate;

        /* Count spikes (probabilistic based on rate) */
        if (rate > 0.1f) spike_count++;
    }

    /* Update slot state */
    bridge->slot_states[slot].occupied = true;
    bridge->slot_states[slot].activity_level = (float)spike_count / (float)neurons;
    bridge->slot_states[slot].salience = salience;
    bridge->slot_states[slot].encode_time = bridge->current_time_ms;
    bridge->slot_states[slot].persistence = 0.0f;

    /* Set inputs to SNN */
    uint32_t input_dim = bridge->config.max_slots * bridge->config.neurons_per_slot;
    snn_network_set_inputs(bridge->snn, bridge->slot_buffer, input_dim);

    /* Update statistics */
    bridge->stats.total_encodings++;
    bridge->stats.total_spikes += (uint64_t)spike_count;

    /* Trigger callback */
    if (bridge->encoding_callback) {
        bridge->encoding_callback(bridge, slot, spike_count,
                                  bridge->encoding_user_data);
    }

    bridge->state = WM_SNN_STATE_IDLE;

    nimcp_mutex_unlock(bridge->base.mutex);
    return spike_count;
}

int wm_snn_update_item(
    wm_snn_bridge_t* bridge,
    uint32_t slot,
    const float* features,
    uint32_t feature_count)
{
    if (!bridge || slot >= bridge->config.max_slots) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "wm_snn_update_item: bridge is NULL");
        return -1;
    }
    if (!bridge->slot_states[slot].occupied) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wm_snn_update_item: bridge->slot_states is NULL");
        return -1;
    }

    /* Re-encode with existing salience */
    /* Phase 8: Heartbeat at operation start */
    working_memory_snn_bridge_heartbeat("working_memo_wm_snn_update_item", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, features, sizeof(*features));

    return wm_snn_encode_item(bridge, slot, features, feature_count,
                              bridge->slot_states[slot].salience);
}

int wm_snn_clear_slot(wm_snn_bridge_t* bridge, uint32_t slot) {
    if (!bridge || slot >= bridge->config.max_slots) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "wm_snn_clear_slot: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    working_memory_snn_bridge_heartbeat("working_memo_wm_snn_clear_slot", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Clear slot buffer */
    uint32_t offset = slot * bridge->config.neurons_per_slot;
    memset(&bridge->slot_buffer[offset], 0,
           bridge->config.neurons_per_slot * sizeof(float));

    /* Clear slot state */
    memset(&bridge->slot_states[slot], 0, sizeof(wm_slot_state_t));

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Simulation Functions
//=============================================================================

int wm_snn_simulate(wm_snn_bridge_t* bridge, float duration_ms) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    working_memory_snn_bridge_heartbeat("working_memo_wm_snn_simulate", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->state = WM_SNN_STATE_SIMULATING;

    int ret = snn_network_run(bridge->snn, duration_ms);

    if (ret >= 0) {  /* snn_network_run returns spike count, >= 0 means success */
        bridge->current_time_ms += (uint64_t)duration_ms;
        bridge->stats.total_simulations++;
        bridge->stats.total_spikes += (uint64_t)ret;

        /* Update slot persistence */
        for (uint32_t s = 0; s < bridge->config.max_slots; s++) {
            /* Phase 8: Loop progress heartbeat */
            if ((s & 0xFF) == 0 && bridge->config.max_slots > 256) {
                working_memory_snn_bridge_heartbeat("working_memo_loop",
                                 (float)(s + 1) / (float)bridge->config.max_slots);
            }

            if (bridge->slot_states[s].occupied) {
                bridge->slot_states[s].persistence += duration_ms;

                /* Apply decay */
                float decay = expf(-duration_ms / bridge->config.decay_tau_ms);
                bridge->slot_states[s].activity_level *= decay;
            }
        }
    }

    bridge->state = WM_SNN_STATE_IDLE;

    nimcp_mutex_unlock(bridge->base.mutex);
    return ret;
}

int wm_snn_step(wm_snn_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    /* Phase 8: Heartbeat at operation start */
    working_memory_snn_bridge_heartbeat("working_memo_wm_snn_step", 0.0f);


    return wm_snn_simulate(bridge, bridge->config.dt_ms);
}

int wm_snn_forward(
    wm_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count)
{
    if (!bridge || !inputs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wm_snn_forward: required parameter is NULL (bridge, inputs)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    working_memory_snn_bridge_heartbeat("working_memo_wm_snn_forward", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, inputs, sizeof(*inputs));

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->state = WM_SNN_STATE_SIMULATING;

    uint32_t input_dim = bridge->config.max_slots * bridge->config.neurons_per_slot;
    uint32_t n = (input_count < input_dim) ? input_count : input_dim;

    memcpy(bridge->slot_buffer, inputs, n * sizeof(float));
    snn_network_set_inputs(bridge->snn, bridge->slot_buffer, input_dim);

    int ret = snn_network_step(bridge->snn, bridge->config.dt_ms);

    int spike_count = 0;
    if (ret >= 0) {  /* snn_network_step returns spike count, >= 0 means success */
        bridge->stats.total_simulations++;
        spike_count = ret;
        bridge->stats.total_spikes += (uint64_t)spike_count;
    }

    bridge->state = WM_SNN_STATE_IDLE;

    nimcp_mutex_unlock(bridge->base.mutex);
    return spike_count;
}

//=============================================================================
// Retrieval Functions
//=============================================================================

int wm_snn_retrieve_item(
    wm_snn_bridge_t* bridge,
    uint32_t slot,
    float* output,
    uint32_t output_size)
{
    if (!bridge || !output || slot >= bridge->config.max_slots) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wm_snn_retrieve_item: required parameter is NULL (bridge, output)");
        return -1;
    }
    if (!bridge->slot_states[slot].occupied) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wm_snn_retrieve_item: bridge->slot_states is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    working_memory_snn_bridge_heartbeat("working_memo_wm_snn_retrieve_item", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, output, sizeof(*output));

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->state = WM_SNN_STATE_RETRIEVING;

    /* Get slot activity */
    uint32_t neurons = bridge->config.neurons_per_slot;
    uint32_t offset = slot * neurons;
    uint32_t n = (output_size < neurons) ? output_size : neurons;

    memcpy(output, &bridge->slot_buffer[offset], n * sizeof(float));

    /* Update statistics */
    bridge->stats.total_retrievals++;
    bridge->slot_states[slot].retrieval_count++;

    /* Calculate confidence from activity */
    float confidence = bridge->slot_states[slot].activity_level;
    if (confidence > 0.3f) {
        bridge->stats.successful_retrievals++;
    }

    /* Trigger callback */
    if (bridge->retrieval_callback) {
        bridge->retrieval_callback(bridge, slot, confidence,
                                   bridge->retrieval_user_data);
    }

    bridge->state = WM_SNN_STATE_IDLE;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int wm_snn_get_slot_activities(
    wm_snn_bridge_t* bridge,
    float* activities,
    uint32_t slot_count)
{
    if (!bridge || !activities) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wm_snn_get_slot_activities: required parameter is NULL (bridge, activities)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    working_memory_snn_bridge_heartbeat("working_memo_wm_snn_get_slot_acti", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, activities, sizeof(*activities));

    nimcp_mutex_lock(bridge->base.mutex);

    uint32_t n = (slot_count < bridge->config.max_slots) ?
                  slot_count : bridge->config.max_slots;

    for (uint32_t s = 0; s < n; s++) {
        /* Phase 8: Loop progress heartbeat */
        if ((s & 0xFF) == 0 && n > 256) {
            working_memory_snn_bridge_heartbeat("working_memo_loop",
                             (float)(s + 1) / (float)n);
        }

        activities[s] = bridge->slot_states[s].activity_level;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int wm_snn_get_most_active_slot(
    wm_snn_bridge_t* bridge,
    float* confidence)
{
    BRIDGE_BBB_VALIDATE(bridge, confidence, sizeof(*confidence));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    working_memory_snn_bridge_heartbeat("working_memo_wm_snn_get_most_acti", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    int best_slot = -1;
    float best_activity = 0.0f;

    for (uint32_t s = 0; s < bridge->config.max_slots; s++) {
        /* Phase 8: Loop progress heartbeat */
        if ((s & 0xFF) == 0 && bridge->config.max_slots > 256) {
            working_memory_snn_bridge_heartbeat("working_memo_loop",
                             (float)(s + 1) / (float)bridge->config.max_slots);
        }

        if (bridge->slot_states[s].occupied &&
            bridge->slot_states[s].activity_level > best_activity) {
            best_activity = bridge->slot_states[s].activity_level;
            best_slot = (int)s;
        }
    }

    if (confidence) {
        *confidence = best_activity;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return best_slot;
}

//=============================================================================
// State Query Functions
//=============================================================================

int wm_snn_get_slot_state(
    wm_snn_bridge_t* bridge,
    uint32_t slot,
    wm_slot_state_t* state)
{
    if (!bridge || !state || slot >= bridge->config.max_slots) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wm_snn_get_slot_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    working_memory_snn_bridge_heartbeat("working_memo_wm_snn_get_slot_stat", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, state, sizeof(*state));

    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->slot_states[slot];
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int wm_snn_get_state(
    wm_snn_bridge_t* bridge,
    wm_snn_bridge_state_t* state)
{
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wm_snn_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    working_memory_snn_bridge_heartbeat("working_memo_wm_snn_get_state", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, state, sizeof(*state));

    nimcp_mutex_lock(bridge->base.mutex);

    state->state = bridge->state;
    state->active_slots = 0;
    state->total_activity = 0.0f;
    state->mean_persistence = 0.0f;

    for (uint32_t s = 0; s < bridge->config.max_slots; s++) {
        /* Phase 8: Loop progress heartbeat */
        if ((s & 0xFF) == 0 && bridge->config.max_slots > 256) {
            working_memory_snn_bridge_heartbeat("working_memo_loop",
                             (float)(s + 1) / (float)bridge->config.max_slots);
        }

        if (bridge->slot_states[s].occupied) {
            state->active_slots++;
            state->total_activity += bridge->slot_states[s].activity_level;
            state->mean_persistence += bridge->slot_states[s].persistence;
        }
    }

    if (state->active_slots > 0) {
        state->mean_persistence /= (float)state->active_slots;
    }

    state->capacity_used = (float)state->active_slots /
                           (float)bridge->config.max_slots;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int wm_snn_get_stats(wm_snn_bridge_t* bridge, wm_snn_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wm_snn_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    working_memory_snn_bridge_heartbeat("working_memo_wm_snn_get_stats", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;

    /* Calculate derived statistics */
    if (stats->total_encodings > 0) {
        stats->mean_encoding_spikes = (float)stats->total_spikes /
                                       (float)stats->total_encodings;
    }
    if (stats->total_retrievals > 0) {
        stats->mean_retrieval_accuracy = (float)stats->successful_retrievals /
                                          (float)stats->total_retrievals;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int wm_snn_reset_stats(wm_snn_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    working_memory_snn_bridge_heartbeat("working_memo_wm_snn_reset_stats", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(wm_snn_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float wm_snn_get_capacity(wm_snn_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    working_memory_snn_bridge_heartbeat("working_memo_wm_snn_get_capacity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    uint32_t active = 0;
    for (uint32_t s = 0; s < bridge->config.max_slots; s++) {
        /* Phase 8: Loop progress heartbeat */
        if ((s & 0xFF) == 0 && bridge->config.max_slots > 256) {
            working_memory_snn_bridge_heartbeat("working_memo_loop",
                             (float)(s + 1) / (float)bridge->config.max_slots);
        }

        if (bridge->slot_states[s].occupied) active++;
    }

    float capacity = (float)active / (float)bridge->config.max_slots;

    nimcp_mutex_unlock(bridge->base.mutex);
    return capacity;
}

float wm_snn_get_total_activity(wm_snn_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    working_memory_snn_bridge_heartbeat("working_memo_wm_snn_get_total_act", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float total = 0.0f;
    for (uint32_t s = 0; s < bridge->config.max_slots; s++) {
        /* Phase 8: Loop progress heartbeat */
        if ((s & 0xFF) == 0 && bridge->config.max_slots > 256) {
            working_memory_snn_bridge_heartbeat("working_memo_loop",
                             (float)(s + 1) / (float)bridge->config.max_slots);
        }

        total += bridge->slot_states[s].activity_level;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return total;
}

//=============================================================================
// Callback Registration
//=============================================================================

int wm_snn_register_spike_callback(
    wm_snn_bridge_t* bridge,
    wm_snn_spike_callback_t callback,
    void* user_data)
{
    BRIDGE_BBB_VALIDATE(bridge, user_data, sizeof(*user_data));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    working_memory_snn_bridge_heartbeat("working_memo_wm_snn_register_spik", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->spike_callback = callback;
    bridge->spike_user_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int wm_snn_register_encoding_callback(
    wm_snn_bridge_t* bridge,
    wm_snn_encoding_callback_t callback,
    void* user_data)
{
    BRIDGE_BBB_VALIDATE(bridge, user_data, sizeof(*user_data));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    working_memory_snn_bridge_heartbeat("working_memo_wm_snn_register_enco", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->encoding_callback = callback;
    bridge->encoding_user_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int wm_snn_register_retrieval_callback(
    wm_snn_bridge_t* bridge,
    wm_snn_retrieval_callback_t callback,
    void* user_data)
{
    BRIDGE_BBB_VALIDATE(bridge, user_data, sizeof(*user_data));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    working_memory_snn_bridge_heartbeat("working_memo_wm_snn_register_retr", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->retrieval_callback = callback;
    bridge->retrieval_user_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int wm_snn_bio_async_connect(wm_snn_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->config.enable_bio_async) return -1;

    /* Phase 8: Heartbeat at operation start */
    working_memory_snn_bridge_heartbeat("working_memo_wm_snn_bio_async_con", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bio_async_connected = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int wm_snn_bio_async_disconnect(wm_snn_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    working_memory_snn_bridge_heartbeat("working_memo_wm_snn_bio_async_dis", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bio_async_connected = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool wm_snn_is_bio_async_connected(wm_snn_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    working_memory_snn_bridge_heartbeat("working_memo_wm_snn_is_bio_async_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bool connected = bridge->bio_async_connected;
    nimcp_mutex_unlock(bridge->base.mutex);

    return connected;
}


void working_memory_snn_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_working_memory_snn_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int working_memory_snn_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "working_memory_snn_bridge_training_begin: NULL argument");
        return -1;
    }
    working_memory_snn_bridge_heartbeat_instance(NULL, "working_memory_snn_bridge_training_begin", 0.0f);
    (void)instance;
    return 0;
}

int working_memory_snn_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "working_memory_snn_bridge_training_end: NULL argument");
        return -1;
    }
    working_memory_snn_bridge_heartbeat_instance(NULL, "working_memory_snn_bridge_training_end", 1.0f);
    (void)instance;
    return 0;
}

int working_memory_snn_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "working_memory_snn_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    working_memory_snn_bridge_heartbeat_instance(NULL, "working_memory_snn_bridge_training_step", progress);
    (void)instance;
    return 0;
}
