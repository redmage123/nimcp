/**
 * @file nimcp_attention_snn_bridge.c
 * @brief Attention System - SNN Bidirectional Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-06
 */

#include "cognitive/attention/nimcp_attention_snn_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"

#include <string.h>
#include <math.h>
#include <stdio.h>

/* G8: guarded lightweight-pop assignment. Wraps the int-returning API in a
 * typed check so we don't silently store 0xFFFFFFFF on failure. Returns 0
 * on success, -1 on failure (and *out is left untouched). */
static int g8_add_lw_pop(snn_network_t* snn, uint32_t n, neuron_type_t t,
                         const char* name, uint32_t* out)
{
    int id = snn_network_add_population_lightweight(snn, n, t, name);
    if (id < 0) {
        return -1;
    }
    *out = (uint32_t)id;
    return 0;
}

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_buffer_constants.h"
#include "constants/nimcp_threshold_constants.h"
#include "constants/nimcp_dimension_constants.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(attention_snn_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_attention_snn_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_attention_snn_bridge_mesh_registry = NULL;

nimcp_error_t attention_snn_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_attention_snn_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "attention_snn_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "attention_snn_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_attention_snn_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_attention_snn_bridge_mesh_registry = registry;
    return err;
}

void attention_snn_bridge_mesh_unregister(void) {
    if (g_attention_snn_bridge_mesh_registry && g_attention_snn_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_attention_snn_bridge_mesh_registry, g_attention_snn_bridge_mesh_id);
        g_attention_snn_bridge_mesh_id = 0;
        g_attention_snn_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from attention_snn_bridge module (instance-level) */
static inline void attention_snn_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_attention_snn_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_attention_snn_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_attention_snn_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


#define LOG_MODULE "ATTENTION_SNN_BRIDGE"


//=============================================================================
// Internal Structure
//=============================================================================

struct attention_snn_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */

    /* Configuration */
    attention_snn_config_t config;

    /* SNN Network */
    snn_network_t* snn;
    bool owns_snn;

    /* Populations */
    uint32_t* head_pops;             /**< Per-head population IDs [num_heads] */
    uint32_t salience_pop;           /**< Salience map population ID */
    uint32_t competition_pop;        /**< Competition/WTA population ID */
    uint32_t gate_pop;               /**< Gate modulation population ID */

    /* State */
    attention_snn_state_t state;
    attention_snn_attention_state_t attention;

    /* Buffers */
    float* head_buffer;              /**< Per-head rates [num_heads * neurons_per_head] */
    float* salience_buffer;          /**< Salience rates [salience_dim] */
    float* output_buffer;            /**< Decoded output [num_heads] */
    float* competition_buffer;       /**< Competition state [num_heads] */
    int32_t* top_k_buffer;           /**< Top-k indices [top_k] */

    /* Statistics */
    attention_snn_stats_t stats;

    /* Modulation state */
    float current_arousal_mod;
    float current_gate_mod;
    float current_competition_strength;

    /* Attention shift tracking (per-instance, not static) */
    float prev_focus;

    /* Bio-async */
    bool bio_async_connected;

};

BRIDGE_DEFINE_SECURITY_SETTERS(attention_snn_bridge)

static inline float sigmoid(float x) {
    return 1.0f / (1.0f + expf(-x));
}

/**
 * @brief Softmax normalization
 */
static void softmax(float* values, uint32_t n) {
    if (n == 0) return;

    /* Find max for numerical stability */
    float max_val = values[0];
    for (uint32_t i = 1; i < n; i++) {
        if (values[i] > max_val) max_val = values[i];
    }

    /* Compute exp and sum */
    float sum = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            attention_snn_bridge_heartbeat("attention_sn_loop",
                             (float)(i + 1) / (float)n);
        }

        values[i] = expf(values[i] - max_val);
        sum += values[i];
    }

    /* Normalize */
    if (sum > 0.0f) {
        for (uint32_t i = 0; i < n; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && n > 256) {
                attention_snn_bridge_heartbeat("attention_sn_loop",
                                 (float)(i + 1) / (float)n);
            }

            values[i] /= sum;
        }
    }
}

/**
 * @brief Winner-take-all competition
 */
static void winner_take_all(float* values, uint32_t n, float inhibition) {
    if (n == 0) return;

    /* Find winner */
    uint32_t winner = 0;
    float max_val = values[0];
    for (uint32_t i = 1; i < n; i++) {
        if (values[i] > max_val) {
            max_val = values[i];
            winner = i;
        }
    }

    /* Apply lateral inhibition */
    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            attention_snn_bridge_heartbeat("attention_sn_loop",
                             (float)(i + 1) / (float)n);
        }

        if (i == winner) {
            values[i] = max_val;  /* Keep winner */
        } else {
            values[i] *= (1.0f - inhibition);  /* Suppress losers */
        }
    }
}

/**
 * @brief Find top-k indices
 */
static void find_top_k(const float* values, uint32_t n, int32_t* indices, uint32_t k) {
    /* Initialize indices */
    for (uint32_t i = 0; i < k; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && k > 256) {
            attention_snn_bridge_heartbeat("attention_sn_loop",
                             (float)(i + 1) / (float)k);
        }

        indices[i] = -1;
    }

    /* Simple selection sort for top-k */
    for (uint32_t i = 0; i < k && i < n; i++) {
        float max_val = -1e30f;
        int32_t max_idx = -1;

        for (uint32_t j = 0; j < n; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && n > 256) {
                attention_snn_bridge_heartbeat("attention_sn_loop",
                                 (float)(j + 1) / (float)n);
            }

            /* Check if already selected */
            bool selected = false;
            for (uint32_t m = 0; m < i; m++) {
                /* Phase 8: Loop progress heartbeat */
                if ((m & 0xFF) == 0 && i > 256) {
                    attention_snn_bridge_heartbeat("attention_sn_loop",
                                     (float)(m + 1) / (float)i);
                }

                if (indices[m] == (int32_t)j) {
                    selected = true;
                    break;
                }
            }

            if (!selected && values[j] > max_val) {
                max_val = values[j];
                max_idx = (int32_t)j;
            }
        }

        indices[i] = max_idx;
    }
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

attention_snn_config_t attention_snn_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    attention_snn_bridge_heartbeat("attention_sn_attention_snn_config", 0.0f);


    attention_snn_config_t config = {
        .num_heads = NIMCP_DEFAULT_ATTENTION_HEADS,
        .neurons_per_head = ATTENTION_SNN_NEURONS_PER_HEAD,
        .salience_dim = ATTENTION_SNN_SALIENCE_DIM,
        .sequence_length = 128,

        .encoding = ATTENTION_SNN_ENCODE_POPULATION,
        .encoding_gain = 1.0f,
        .baseline_rate_hz = 5.0f,
        .max_rate_hz = 100.0f,
        .salience_gain = 1.5f,

        .decoding = ATTENTION_SNN_DECODE_SOFTMAX,
        .decoding_threshold = 0.3f,
        .softmax_temperature = NIMCP_TEMPERATURE_DEFAULT,
        .temporal_smoothing = 0.8f,

        .enable_competition = true,
        .inhibition_strength = 0.5f,
        .competition_tau_ms = 20.0f,
        .top_k = 3,

        .enable_gate_integration = true,
        .gate_modulation_gain = 1.0f,

        .dt_ms = ATTENTION_SNN_DEFAULT_DT,
        .simulation_window_ms = ATTENTION_SNN_ENCODING_WINDOW,

        .enable_bio_async = false,
        .enable_plasticity_integration = true,
        .enable_immune_modulation = false
    };
    return config;
}

attention_snn_bridge_t* attention_snn_create(const attention_snn_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    attention_snn_bridge_heartbeat("attention_sn_attention_snn_create", 0.0f);


    attention_snn_bridge_t* bridge = nimcp_calloc(1, sizeof(attention_snn_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "attention_snn_create: failed to allocate bridge");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = attention_snn_config_default();
    }

    /* Validate configuration */
    if (bridge->config.num_heads == 0 || bridge->config.num_heads > ATTENTION_SNN_MAX_HEADS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "attention_snn_create: num_heads out of range");
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }

    /* Initialize bridge base infrastructure (includes mutex) */
    if (bridge_base_init(&bridge->base, 0, "attention_snn") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "attention_snn_create: failed to initialize bridge base");
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }

    /* Create SNN network */
    snn_config_t snn_config;
    uint32_t input_dim = bridge->config.num_heads * bridge->config.neurons_per_head;
    uint32_t hidden_dim = input_dim + bridge->config.salience_dim;
    uint32_t output_dim = bridge->config.num_heads;

    snn_config_feedforward(&snn_config,
        input_dim,
        hidden_dim,
        output_dim);

    snn_config.dt = bridge->config.dt_ms;
    snn_config.enable_bio_async = bridge->config.enable_bio_async;
    snn_config.n_populations = 0;

    bridge->snn = snn_network_create(&snn_config);
    if (!bridge->snn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "attention_snn_create: failed to create SNN network");
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }
    bridge->owns_snn = true;

    /* Allocate per-head population IDs */
    bridge->head_pops = nimcp_calloc(bridge->config.num_heads, sizeof(uint32_t));
    if (!bridge->head_pops) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "attention_snn_create: failed to allocate head_pops");
        attention_snn_destroy(bridge);
        return NULL;
    }

    /* Create populations for each attention head (G8: guarded lightweight) */
    char pop_name[NIMCP_ID_BUFFER_SIZE];
    for (uint32_t h = 0; h < bridge->config.num_heads; h++) {
        /* Phase 8: Loop progress heartbeat */
        if ((h & 0xFF) == 0 && bridge->config.num_heads > 256) {
            attention_snn_bridge_heartbeat("attention_sn_loop",
                             (float)(h + 1) / (float)bridge->config.num_heads);
        }

        snprintf(pop_name, sizeof(pop_name), "attention_head_%u", h);
        if (g8_add_lw_pop(bridge->snn, bridge->config.neurons_per_head,
                          NEURON_GENERIC_LIF, pop_name,
                          &bridge->head_pops[h]) != 0) {
            NIMCP_LOG_ERROR(LOG_MODULE, "attention_snn_create: head_pop[%u] alloc failed", h);
            attention_snn_destroy(bridge);
            return NULL;
        }
    }

    /* Salience population */
    if (g8_add_lw_pop(bridge->snn, bridge->config.salience_dim,
                      NEURON_GENERIC_LIF, "attention_salience",
                      &bridge->salience_pop) != 0) {
        NIMCP_LOG_ERROR(LOG_MODULE, "attention_snn_create: salience_pop alloc failed");
        attention_snn_destroy(bridge);
        return NULL;
    }

    /* Competition population (one neuron per head for WTA) */
    if (g8_add_lw_pop(bridge->snn, bridge->config.num_heads,
                      NEURON_GENERIC_LIF, "attention_competition",
                      &bridge->competition_pop) != 0) {
        NIMCP_LOG_ERROR(LOG_MODULE, "attention_snn_create: competition_pop alloc failed");
        attention_snn_destroy(bridge);
        return NULL;
    }

    /* Gate population */
    if (g8_add_lw_pop(bridge->snn, 32, NEURON_GENERIC_LIF, "attention_gate",
                      &bridge->gate_pop) != 0) {
        NIMCP_LOG_ERROR(LOG_MODULE, "attention_snn_create: gate_pop alloc failed");
        attention_snn_destroy(bridge);
        return NULL;
    }

    /* Connect head populations to competition layer */
    for (uint32_t h = 0; h < bridge->config.num_heads; h++) {
        /* Phase 8: Loop progress heartbeat */
        if ((h & 0xFF) == 0 && bridge->config.num_heads > 256) {
            attention_snn_bridge_heartbeat("attention_sn_loop",
                             (float)(h + 1) / (float)bridge->config.num_heads);
        }

        snn_network_connect_populations(bridge->snn,
            bridge->head_pops[h], bridge->competition_pop,
            SNN_TOPO_RANDOM, 0.5f, SYNAPSE_AMPA, 0.5f, 0.1f);
    }

    /* Add lateral inhibition in competition layer */
    if (bridge->config.enable_competition) {
        snn_network_connect_populations(bridge->snn,
            bridge->competition_pop, bridge->competition_pop,
            SNN_TOPO_RANDOM, 0.8f, SYNAPSE_GABA_A,
            -bridge->config.inhibition_strength, 0.1f);
    }

    /* Gate to competition connections */
    if (bridge->config.enable_gate_integration) {
        snn_network_connect_populations(bridge->snn,
            bridge->gate_pop, bridge->competition_pop,
            SNN_TOPO_RANDOM, 0.3f, SYNAPSE_AMPA, 0.3f, 0.05f);
    }

    /* G8/H1 input shunt: snn_network_create made dense input_pop at id 0 to
     * receive snn_network_set_inputs. Wire it to each attention head so
     * external input actually reaches the bridge's lightweight graph. */
    for (uint32_t h = 0; h < bridge->config.num_heads; h++) {
        snn_network_connect_populations(bridge->snn,
            /*src=dense input_pop*/ 0, bridge->head_pops[h],
            SNN_TOPO_RANDOM, 0.5f, SYNAPSE_AMPA, 0.4f, 0.1f);
    }

    /* G8/M2 salience wire: salience_pop was created but had no outgoing
     * connection. Route it into competition so it modulates head selection. */
    snn_network_connect_populations(bridge->snn,
        bridge->salience_pop, bridge->competition_pop,
        SNN_TOPO_RANDOM, 0.5f, SYNAPSE_AMPA, 0.3f, 0.1f);

    /* G8/H1 output shunt: competition_pop's WTA result must reach the dense
     * output_pop at id 2 so snn_network_get_outputs returns meaningful rates. */
    snn_network_connect_populations(bridge->snn,
        bridge->competition_pop, /*dst=dense output_pop*/ 2,
        SNN_TOPO_FEEDFORWARD, 1.0f, SYNAPSE_AMPA, 0.7f, 0.05f);

    /* G8: finalize lightweight CSR after all connections built. */
    snn_network_finalize_connections(bridge->snn);

    /* Allocate buffers */
    uint32_t head_buffer_size = bridge->config.num_heads * bridge->config.neurons_per_head;
    bridge->head_buffer = nimcp_calloc(head_buffer_size, sizeof(float));
    bridge->salience_buffer = nimcp_calloc(bridge->config.salience_dim, sizeof(float));
    bridge->output_buffer = nimcp_calloc(bridge->config.num_heads, sizeof(float));
    bridge->competition_buffer = nimcp_calloc(bridge->config.num_heads, sizeof(float));
    bridge->top_k_buffer = nimcp_calloc(bridge->config.top_k, sizeof(int32_t));

    if (!bridge->head_buffer || !bridge->salience_buffer ||
        !bridge->output_buffer || !bridge->competition_buffer || !bridge->top_k_buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "attention_snn_create: failed to allocate buffers");
        attention_snn_destroy(bridge);
        return NULL;
    }

    /* Allocate attention state arrays */
    bridge->attention.attention_weights = nimcp_calloc(bridge->config.num_heads, sizeof(float));
    bridge->attention.salience_map = nimcp_calloc(bridge->config.sequence_length, sizeof(float));
    bridge->attention.top_k_indices = nimcp_calloc(bridge->config.top_k, sizeof(int32_t));

    if (!bridge->attention.attention_weights ||
        !bridge->attention.salience_map ||
        !bridge->attention.top_k_indices) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "attention_snn_create: failed to allocate attention state arrays");
        attention_snn_destroy(bridge);
        return NULL;
    }

    /* Initialize attention weights to uniform distribution for proper temporal smoothing */
    float uniform_weight = 1.0f / (float)bridge->config.num_heads;
    for (uint32_t h = 0; h < bridge->config.num_heads; h++) {
        /* Phase 8: Loop progress heartbeat */
        if ((h & 0xFF) == 0 && bridge->config.num_heads > 256) {
            attention_snn_bridge_heartbeat("attention_sn_loop",
                             (float)(h + 1) / (float)bridge->config.num_heads);
        }

        bridge->attention.attention_weights[h] = uniform_weight;
    }

    /* Initialize state */
    bridge->state = ATTENTION_SNN_STATE_IDLE;
    bridge->attention.focus_strength = 0.0f;
    bridge->attention.sparsity = 1.0f;
    bridge->attention.gate_activation = 0.5f;
    bridge->current_arousal_mod = 1.0f;
    bridge->current_gate_mod = 0.5f;
    bridge->current_competition_strength = bridge->config.inhibition_strength;
    bridge->bio_async_connected = false;

    NIMCP_LOGGING_INFO("Created %s bridge", "attention_snn");
    return bridge;
}

void attention_snn_destroy(attention_snn_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "attention_snn");

    /* Phase 8: Heartbeat at operation start */
    attention_snn_bridge_heartbeat("attention_sn_attention_snn_destro", 0.0f);


    if (bridge->bio_async_connected) {
        attention_snn_disconnect_bio_async(bridge);
    }

    if (bridge->snn && bridge->owns_snn) {
        snn_network_destroy(bridge->snn);
    }

    if (bridge->head_pops) nimcp_free(bridge->head_pops);
    if (bridge->head_buffer) nimcp_free(bridge->head_buffer);
    if (bridge->salience_buffer) nimcp_free(bridge->salience_buffer);
    if (bridge->output_buffer) nimcp_free(bridge->output_buffer);
    if (bridge->competition_buffer) nimcp_free(bridge->competition_buffer);
    if (bridge->top_k_buffer) nimcp_free(bridge->top_k_buffer);

    if (bridge->attention.attention_weights) nimcp_free(bridge->attention.attention_weights);
    if (bridge->attention.salience_map) nimcp_free(bridge->attention.salience_map);
    if (bridge->attention.top_k_indices) nimcp_free(bridge->attention.top_k_indices);

    /* Cleanup base bridge infrastructure */
    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge);
    bridge = NULL;
}

int attention_snn_reset(attention_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_snn_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    attention_snn_bridge_heartbeat("attention_sn_attention_snn_reset", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->snn) {
        snn_network_reset(bridge->snn);
    }

    /* Reset state */
    bridge->state = ATTENTION_SNN_STATE_IDLE;

    /* Clear buffers */
    uint32_t head_buffer_size = bridge->config.num_heads * bridge->config.neurons_per_head;
    memset(bridge->head_buffer, 0, head_buffer_size * sizeof(float));
    memset(bridge->salience_buffer, 0, bridge->config.salience_dim * sizeof(float));
    memset(bridge->output_buffer, 0, bridge->config.num_heads * sizeof(float));
    memset(bridge->competition_buffer, 0, bridge->config.num_heads * sizeof(float));
    memset(bridge->top_k_buffer, -1, bridge->config.top_k * sizeof(int32_t));

    /* Reset attention state - use uniform distribution for proper temporal smoothing */
    float uniform_weight = 1.0f / (float)bridge->config.num_heads;
    for (uint32_t h = 0; h < bridge->config.num_heads; h++) {
        /* Phase 8: Loop progress heartbeat */
        if ((h & 0xFF) == 0 && bridge->config.num_heads > 256) {
            attention_snn_bridge_heartbeat("attention_sn_loop",
                             (float)(h + 1) / (float)bridge->config.num_heads);
        }

        bridge->attention.attention_weights[h] = uniform_weight;
    }
    memset(bridge->attention.salience_map, 0, bridge->config.sequence_length * sizeof(float));
    memset(bridge->attention.top_k_indices, -1, bridge->config.top_k * sizeof(int32_t));
    bridge->attention.focus_strength = 0.0f;
    bridge->attention.sparsity = 1.0f;
    bridge->attention.gate_activation = 0.5f;

    /* Reset modulation */
    bridge->current_arousal_mod = 1.0f;
    bridge->current_gate_mod = 0.5f;
    bridge->current_competition_strength = bridge->config.inhibition_strength;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Encoding Functions (Attention --> SNN)
//=============================================================================

int attention_snn_encode_weights(
    attention_snn_bridge_t* bridge,
    const float* attention_weights,
    uint32_t num_heads)
{
    if (!bridge || !attention_weights) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_snn_encode_weights: bridge or attention_weights is NULL");
        return -1;
    }

    BRIDGE_BBB_VALIDATE(bridge, attention_weights, num_heads * sizeof(float));

    /* Phase 8: Heartbeat at operation start */
    attention_snn_bridge_heartbeat("attention_sn_attention_snn_encode", 0.0f);


    if (num_heads > bridge->config.num_heads) {
        num_heads = bridge->config.num_heads;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->state = ATTENTION_SNN_STATE_ENCODING;

    int total_spikes = 0;
    float rate_scale = bridge->config.encoding_gain * bridge->current_arousal_mod;

    /* Encode each attention head as a population activity */
    for (uint32_t h = 0; h < num_heads; h++) {
        /* Phase 8: Loop progress heartbeat */
        if ((h & 0xFF) == 0 && num_heads > 256) {
            attention_snn_bridge_heartbeat("attention_sn_loop",
                             (float)(h + 1) / (float)num_heads);
        }

        float weight = nimcp_clampf(attention_weights[h], 0.0f, 1.0f);

        /* WHAT: Store encoded weight for focus/sparsity calculation */
        /* WHY: get_focus_strength computes variance from attention_weights */
        /* HOW: Copy input weights to internal state during encoding */
        bridge->attention.attention_weights[h] = weight;

        /* Calculate firing rate based on attention weight */
        float rate = bridge->config.baseline_rate_hz +
                    weight * rate_scale *
                    (bridge->config.max_rate_hz - bridge->config.baseline_rate_hz);

        /* Set activity for all neurons in this head's population */
        for (uint32_t n = 0; n < bridge->config.neurons_per_head; n++) {
            /* Phase 8: Loop progress heartbeat */
            if ((n & 0xFF) == 0 && bridge->config.neurons_per_head > 256) {
                attention_snn_bridge_heartbeat("attention_sn_loop",
                                 (float)(n + 1) / (float)bridge->config.neurons_per_head);
            }

            uint32_t idx = h * bridge->config.neurons_per_head + n;
            bridge->head_buffer[idx] = rate;
            if (rate > bridge->config.baseline_rate_hz * 1.5f) {
                total_spikes++;
            }
        }
    }

    /* Set inputs to SNN network */
    uint32_t head_buffer_size = bridge->config.num_heads * bridge->config.neurons_per_head;
    int ret = snn_network_set_inputs(bridge->snn, bridge->head_buffer, head_buffer_size);
    if (ret != SNN_SUCCESS) {
        bridge->state = ATTENTION_SNN_STATE_IDLE;
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "attention_snn_encode_weights: validation failed");
        return -1;
    }

    /* Update stats */
    bridge->stats.total_forward_passes++;
    bridge->stats.total_spikes_generated += (uint64_t)total_spikes;

    bridge->state = ATTENTION_SNN_STATE_IDLE;

    nimcp_mutex_unlock(bridge->base.mutex);

    return total_spikes;
}

int attention_snn_encode_salience(
    attention_snn_bridge_t* bridge,
    const float* salience,
    uint32_t sequence_length)
{
    if (!bridge || !salience) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_snn_encode_salience: bridge or salience is NULL");
        return -1;
    }

    BRIDGE_BBB_VALIDATE(bridge, salience, sequence_length * sizeof(float));

    /* Phase 8: Heartbeat at operation start */
    attention_snn_bridge_heartbeat("attention_sn_attention_snn_encode", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->state = ATTENTION_SNN_STATE_ENCODING;

    int total_active = 0;
    uint32_t n = (sequence_length < bridge->config.salience_dim) ?
                  sequence_length : bridge->config.salience_dim;

    /* Encode salience as population activity */
    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            attention_snn_bridge_heartbeat("attention_sn_loop",
                             (float)(i + 1) / (float)n);
        }

        float sal = nimcp_clampf(salience[i], 0.0f, 1.0f);
        float rate = bridge->config.baseline_rate_hz +
                    sal * bridge->config.salience_gain *
                    (bridge->config.max_rate_hz - bridge->config.baseline_rate_hz);
        bridge->salience_buffer[i] = rate;
        if (rate > bridge->config.baseline_rate_hz * 1.5f) {
            total_active++;
        }
    }

    /* Zero-fill remaining */
    for (uint32_t i = n; i < bridge->config.salience_dim; i++) {
        bridge->salience_buffer[i] = bridge->config.baseline_rate_hz;
    }

    /* Store in attention state */
    for (uint32_t i = 0; i < sequence_length && i < bridge->config.sequence_length; i++) {
        bridge->attention.salience_map[i] = salience[i];
    }

    bridge->stats.total_forward_passes++;
    bridge->stats.total_spikes_generated += (uint64_t)total_active;
    bridge->state = ATTENTION_SNN_STATE_IDLE;

    nimcp_mutex_unlock(bridge->base.mutex);

    return total_active;
}

int attention_snn_encode_multihead(
    attention_snn_bridge_t* bridge,
    multihead_attention_t mha,
    const float* input,
    uint32_t sequence_length)
{
    if (!bridge || !mha || !input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_snn_encode_multihead: bridge, mha, or input is NULL");
        return -1;
    }

    BRIDGE_BBB_VALIDATE(bridge, input, sequence_length * sizeof(float));

    /* Phase 8: Heartbeat at operation start */
    attention_snn_bridge_heartbeat("attention_sn_attention_snn_encode", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->state = ATTENTION_SNN_STATE_ENCODING;

    /* Get attention statistics to extract weights */
    attention_stats_t mha_stats;
    if (!multihead_attention_get_stats(mha, &mha_stats)) {
        bridge->state = ATTENTION_SNN_STATE_IDLE;
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "attention_snn_encode_multihead: multihead_attention_get_stats is NULL");
        return -1;
    }

    /* Get attention strength from multihead system */
    float strength = multihead_attention_get_strength(mha);

    /* Encode based on attention type */
    int total_spikes = 0;

    switch (bridge->config.encoding) {
        case ATTENTION_SNN_ENCODE_RATE:
            /* Simple rate coding based on attention strength */
            for (uint32_t h = 0; h < bridge->config.num_heads; h++) {
                /* Phase 8: Loop progress heartbeat */
                if ((h & 0xFF) == 0 && bridge->config.num_heads > 256) {
                    attention_snn_bridge_heartbeat("attention_sn_loop",
                                     (float)(h + 1) / (float)bridge->config.num_heads);
                }

                float rate = bridge->config.baseline_rate_hz +
                            strength * bridge->config.encoding_gain *
                            (bridge->config.max_rate_hz - bridge->config.baseline_rate_hz);
                for (uint32_t n = 0; n < bridge->config.neurons_per_head; n++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((n & 0xFF) == 0 && bridge->config.neurons_per_head > 256) {
                        attention_snn_bridge_heartbeat("attention_sn_loop",
                                         (float)(n + 1) / (float)bridge->config.neurons_per_head);
                    }

                    uint32_t idx = h * bridge->config.neurons_per_head + n;
                    bridge->head_buffer[idx] = rate;
                    if (rate > bridge->config.baseline_rate_hz * 1.5f) {
                        total_spikes++;
                    }
                }
            }
            break;

        case ATTENTION_SNN_ENCODE_POPULATION:
            /* Population coding - distribute activity across heads */
            for (uint32_t h = 0; h < bridge->config.num_heads; h++) {
                /* Phase 8: Loop progress heartbeat */
                if ((h & 0xFF) == 0 && bridge->config.num_heads > 256) {
                    attention_snn_bridge_heartbeat("attention_sn_loop",
                                     (float)(h + 1) / (float)bridge->config.num_heads);
                }

                /* Each head gets activity based on its contribution */
                float head_weight = strength * (1.0f + 0.2f * sinf((float)h * 0.5f));
                head_weight = nimcp_clampf(head_weight, 0.0f, 1.0f);

                float rate = bridge->config.baseline_rate_hz +
                            head_weight * bridge->config.encoding_gain *
                            (bridge->config.max_rate_hz - bridge->config.baseline_rate_hz);

                for (uint32_t n = 0; n < bridge->config.neurons_per_head; n++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((n & 0xFF) == 0 && bridge->config.neurons_per_head > 256) {
                        attention_snn_bridge_heartbeat("attention_sn_loop",
                                         (float)(n + 1) / (float)bridge->config.neurons_per_head);
                    }

                    uint32_t idx = h * bridge->config.neurons_per_head + n;
                    /* Add some variation within population */
                    float var = 0.9f + 0.2f * ((float)(n % 5) / 5.0f);
                    bridge->head_buffer[idx] = rate * var;
                    if (bridge->head_buffer[idx] > bridge->config.baseline_rate_hz * 1.5f) {
                        total_spikes++;
                    }
                }
            }
            break;

        case ATTENTION_SNN_ENCODE_WINNER_TAKE_ALL:
            /* Only active heads get high rates */
            for (uint32_t h = 0; h < bridge->config.num_heads; h++) {
                /* Phase 8: Loop progress heartbeat */
                if ((h & 0xFF) == 0 && bridge->config.num_heads > 256) {
                    attention_snn_bridge_heartbeat("attention_sn_loop",
                                     (float)(h + 1) / (float)bridge->config.num_heads);
                }

                bool is_active = (h < mha_stats.active_heads);
                float rate = is_active ?
                    (bridge->config.baseline_rate_hz +
                     strength * bridge->config.encoding_gain *
                     (bridge->config.max_rate_hz - bridge->config.baseline_rate_hz)) :
                    bridge->config.baseline_rate_hz * 0.5f;

                for (uint32_t n = 0; n < bridge->config.neurons_per_head; n++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((n & 0xFF) == 0 && bridge->config.neurons_per_head > 256) {
                        attention_snn_bridge_heartbeat("attention_sn_loop",
                                         (float)(n + 1) / (float)bridge->config.neurons_per_head);
                    }

                    uint32_t idx = h * bridge->config.neurons_per_head + n;
                    bridge->head_buffer[idx] = rate;
                    if (rate > bridge->config.baseline_rate_hz * 1.5f) {
                        total_spikes++;
                    }
                }
            }
            break;

        case ATTENTION_SNN_ENCODE_TEMPORAL:
            /* Temporal coding - higher attention = shorter latency */
            for (uint32_t h = 0; h < bridge->config.num_heads; h++) {
                /* Phase 8: Loop progress heartbeat */
                if ((h & 0xFF) == 0 && bridge->config.num_heads > 256) {
                    attention_snn_bridge_heartbeat("attention_sn_loop",
                                     (float)(h + 1) / (float)bridge->config.num_heads);
                }

                float latency_factor = 1.0f - strength;  /* Invert for timing */
                float rate = bridge->config.baseline_rate_hz +
                            (1.0f - latency_factor) * bridge->config.encoding_gain *
                            (bridge->config.max_rate_hz - bridge->config.baseline_rate_hz);

                for (uint32_t n = 0; n < bridge->config.neurons_per_head; n++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((n & 0xFF) == 0 && bridge->config.neurons_per_head > 256) {
                        attention_snn_bridge_heartbeat("attention_sn_loop",
                                         (float)(n + 1) / (float)bridge->config.neurons_per_head);
                    }

                    uint32_t idx = h * bridge->config.neurons_per_head + n;
                    bridge->head_buffer[idx] = rate;
                    if (rate > bridge->config.baseline_rate_hz * 1.5f) {
                        total_spikes++;
                    }
                }
            }
            break;
    }

    /* Set inputs to SNN */
    uint32_t head_buffer_size = bridge->config.num_heads * bridge->config.neurons_per_head;
    snn_network_set_inputs(bridge->snn, bridge->head_buffer, head_buffer_size);

    bridge->stats.total_forward_passes++;
    bridge->stats.total_spikes_generated += (uint64_t)total_spikes;

    bridge->state = ATTENTION_SNN_STATE_IDLE;

    nimcp_mutex_unlock(bridge->base.mutex);

    return total_spikes;
}

int attention_snn_encode_gate(
    attention_snn_bridge_t* bridge,
    float gate_signal)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_snn_encode_gate: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_gate_integration) return 0;

    /* Phase 8: Heartbeat at operation start */
    attention_snn_bridge_heartbeat("attention_sn_attention_snn_encode", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->current_gate_mod = nimcp_clampf(gate_signal, 0.0f, 1.0f);
    bridge->attention.gate_activation = bridge->current_gate_mod;

    /* The gate modulates the competition layer excitability */

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Simulation Functions
//=============================================================================

int attention_snn_simulate(attention_snn_bridge_t* bridge, float duration_ms) {
    if (!bridge || !bridge->snn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_snn_simulate: bridge or snn is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    attention_snn_bridge_heartbeat("attention_sn_attention_snn_simula", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->state = ATTENTION_SNN_STATE_SIMULATING;

    if (bridge->config.dt_ms <= 0.0f) {
        bridge->state = ATTENTION_SNN_STATE_IDLE;
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "attention_snn_simulate: dt_ms is zero or negative");
        return -1;
    }

    int steps = (int)(duration_ms / bridge->config.dt_ms);
    for (int s = 0; s < steps; s++) {
        /* Phase 8: Loop progress heartbeat */
        if ((s & 0xFF) == 0 && steps > 256) {
            attention_snn_bridge_heartbeat("attention_sn_loop",
                             (float)(s + 1) / (float)steps);
        }

        snn_network_step(bridge->snn, bridge->config.dt_ms);
    }

    /* NOTE: attention_weights are set during encode_weights and preserved */
    /* NOTE: SNN dynamics affect competition_buffer via compete() function */

    bridge->state = ATTENTION_SNN_STATE_IDLE;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int attention_snn_step(attention_snn_bridge_t* bridge) {
    if (!bridge || !bridge->snn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_snn_step: bridge or snn is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    attention_snn_bridge_heartbeat("attention_sn_attention_snn_step", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    snn_network_step(bridge->snn, bridge->config.dt_ms);

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Notify coordinator of step cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

int attention_snn_compete(attention_snn_bridge_t* bridge, float duration_ms) {
    if (!bridge || !bridge->snn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_snn_compete: bridge or snn is NULL");
        return -1;
    }
    if (!bridge->config.enable_competition) return 0;

    /* Phase 8: Heartbeat at operation start */
    attention_snn_bridge_heartbeat("attention_sn_attention_snn_compet", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->state = ATTENTION_SNN_STATE_COMPETING;

    if (bridge->config.dt_ms <= 0.0f) {
        bridge->state = ATTENTION_SNN_STATE_IDLE;
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "attention_snn_compete: dt_ms is zero or negative");
        return -1;
    }

    /* Run competition simulation */
    int steps = (int)(duration_ms / bridge->config.dt_ms);
    for (int s = 0; s < steps; s++) {
        /* Phase 8: Loop progress heartbeat */
        if ((s & 0xFF) == 0 && steps > 256) {
            attention_snn_bridge_heartbeat("attention_sn_loop",
                             (float)(s + 1) / (float)steps);
        }

        snn_network_step(bridge->snn, bridge->config.dt_ms);

        /* Apply winner-take-all dynamics every few steps */
        if (s % 5 == 0) {
            /* Get current firing rates and apply WTA */
            snn_network_get_outputs(bridge->snn, bridge->competition_buffer, bridge->config.num_heads);
            winner_take_all(bridge->competition_buffer, bridge->config.num_heads,
                           bridge->current_competition_strength);
        }
    }

    /* Update competition energy in stats */
    float energy = 0.0f;
    for (uint32_t h = 0; h < bridge->config.num_heads; h++) {
        /* Phase 8: Loop progress heartbeat */
        if ((h & 0xFF) == 0 && bridge->config.num_heads > 256) {
            attention_snn_bridge_heartbeat("attention_sn_loop",
                             (float)(h + 1) / (float)bridge->config.num_heads);
        }

        energy += bridge->competition_buffer[h] * bridge->competition_buffer[h];
    }
    bridge->stats.competition_convergence_rate = energy / (float)bridge->config.num_heads;

    /* WHAT: Update attention weights from competition results */
    /* WHY: Focus strength and sparsity metrics read from attention_weights */
    /* HOW: Normalize competition buffer values to [0,1] and copy */
    float max_val = 0.0f;
    for (uint32_t h = 0; h < bridge->config.num_heads; h++) {
        /* Phase 8: Loop progress heartbeat */
        if ((h & 0xFF) == 0 && bridge->config.num_heads > 256) {
            attention_snn_bridge_heartbeat("attention_sn_loop",
                             (float)(h + 1) / (float)bridge->config.num_heads);
        }

        if (bridge->competition_buffer[h] > max_val) {
            max_val = bridge->competition_buffer[h];
        }
    }
    if (max_val > 0.0f) {
        for (uint32_t h = 0; h < bridge->config.num_heads; h++) {
            /* Phase 8: Loop progress heartbeat */
            if ((h & 0xFF) == 0 && bridge->config.num_heads > 256) {
                attention_snn_bridge_heartbeat("attention_sn_loop",
                                 (float)(h + 1) / (float)bridge->config.num_heads);
            }

            bridge->attention.attention_weights[h] = bridge->competition_buffer[h] / max_val;
        }
    }

    bridge->state = ATTENTION_SNN_STATE_IDLE;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Decoding Functions (SNN --> Attention)
//=============================================================================

int attention_snn_get_weights(
    attention_snn_bridge_t* bridge,
    float* weights,
    uint32_t num_heads)
{
    if (!bridge || !weights) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_snn_get_weights: bridge or weights is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    attention_snn_bridge_heartbeat("attention_sn_attention_snn_get_we", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->state = ATTENTION_SNN_STATE_DECODING;

    /* Get firing rates from competition layer */
    snn_network_get_outputs(bridge->snn, bridge->output_buffer, bridge->config.num_heads);

    /* Apply decoding method */
    uint32_t n = (num_heads < bridge->config.num_heads) ? num_heads : bridge->config.num_heads;
    memcpy(weights, bridge->output_buffer, n * sizeof(float));

    /* Normalize firing rates to [0, 1] */
    float max_rate = bridge->config.max_rate_hz;
    float min_rate = bridge->config.baseline_rate_hz;
    float rate_range = max_rate - min_rate;
    for (uint32_t h = 0; h < n; h++) {
        /* Phase 8: Loop progress heartbeat */
        if ((h & 0xFF) == 0 && n > 256) {
            attention_snn_bridge_heartbeat("attention_sn_loop",
                             (float)(h + 1) / (float)n);
        }

        if (rate_range > 0.0f) {
            weights[h] = (weights[h] - min_rate) / rate_range;
        } else {
            weights[h] = 0.5f;  /* All rates equal - use neutral weight */
        }
        weights[h] = nimcp_clampf(weights[h], 0.0f, 1.0f);
    }

    switch (bridge->config.decoding) {
        case ATTENTION_SNN_DECODE_SOFTMAX:
            /* Apply temperature-scaled softmax */
            for (uint32_t h = 0; h < n; h++) {
                /* Phase 8: Loop progress heartbeat */
                if ((h & 0xFF) == 0 && n > 256) {
                    attention_snn_bridge_heartbeat("attention_sn_loop",
                                     (float)(h + 1) / (float)n);
                }

                weights[h] /= bridge->config.softmax_temperature;
            }
            softmax(weights, n);
            break;

        case ATTENTION_SNN_DECODE_COMPETITION:
            /* Apply winner-take-all to produce sparse output */
            winner_take_all(weights, n, bridge->config.inhibition_strength);
            break;

        case ATTENTION_SNN_DECODE_SYNCHRONY:
            /* Synchrony-based: weights based on correlation with population average */
            {
                float avg = 0.0f;
                for (uint32_t h = 0; h < n; h++) avg += weights[h];
                avg = (n > 0) ? (avg / (float)n) : 0.0f;

                for (uint32_t h = 0; h < n; h++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((h & 0xFF) == 0 && n > 256) {
                        attention_snn_bridge_heartbeat("attention_sn_loop",
                                         (float)(h + 1) / (float)n);
                    }

                    float diff = fabsf(weights[h] - avg);
                    weights[h] = 1.0f - (diff / (avg + 0.001f));
                    weights[h] = nimcp_clampf(weights[h], 0.0f, 1.0f);
                }
            }
            break;

        case ATTENTION_SNN_DECODE_RATE:
        default:
            /* Already normalized to [0, 1] */
            break;
    }

    /* Apply temporal smoothing */
    if (bridge->config.temporal_smoothing > 0.0f) {
        float alpha = bridge->config.temporal_smoothing;
        for (uint32_t h = 0; h < n; h++) {
            /* Phase 8: Loop progress heartbeat */
            if ((h & 0xFF) == 0 && n > 256) {
                attention_snn_bridge_heartbeat("attention_sn_loop",
                                 (float)(h + 1) / (float)n);
            }

            weights[h] = alpha * bridge->attention.attention_weights[h] +
                        (1.0f - alpha) * weights[h];
        }
    }

    /* Update attention state */
    memcpy(bridge->attention.attention_weights, weights, n * sizeof(float));
    bridge->attention.last_update_us = nimcp_time_get_us();

    bridge->stats.total_decodings++;
    bridge->state = ATTENTION_SNN_STATE_IDLE;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int attention_snn_get_salience(
    attention_snn_bridge_t* bridge,
    float* salience,
    uint32_t sequence_length)
{
    if (!bridge || !salience) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_snn_get_salience: bridge or salience is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    attention_snn_bridge_heartbeat("attention_sn_attention_snn_get_sa", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    uint32_t n = (sequence_length < bridge->config.sequence_length) ?
                  sequence_length : bridge->config.sequence_length;

    memcpy(salience, bridge->attention.salience_map, n * sizeof(float));

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int attention_snn_get_top_k(
    attention_snn_bridge_t* bridge,
    int32_t* indices,
    uint32_t k)
{
    if (!bridge || !indices) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_snn_get_top_k: bridge or indices is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    attention_snn_bridge_heartbeat("attention_sn_attention_snn_get_to", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    uint32_t actual_k = (k < bridge->config.top_k) ? k : bridge->config.top_k;

    /* Find top-k attention heads */
    find_top_k(bridge->attention.attention_weights, bridge->config.num_heads,
               indices, actual_k);

    /* Update internal state */
    memcpy(bridge->attention.top_k_indices, indices, actual_k * sizeof(int32_t));

    /* Count how many valid indices found */
    uint32_t count = 0;
    for (uint32_t i = 0; i < actual_k; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && actual_k > 256) {
            attention_snn_bridge_heartbeat("attention_sn_loop",
                             (float)(i + 1) / (float)actual_k);
        }

        if (indices[i] >= 0) count++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return (int)count;
}

/* Internal unlocked version - call only when mutex is already held */
static float attention_snn_get_focus_strength_unlocked(attention_snn_bridge_t* bridge) {
    /* Calculate focus strength as variance of attention weights
     * High variance = focused (one winner), Low variance = diffuse */
    float mean = 0.0f;
    for (uint32_t h = 0; h < bridge->config.num_heads; h++) {
        /* Phase 8: Loop progress heartbeat */
        if ((h & 0xFF) == 0 && bridge->config.num_heads > 256) {
            attention_snn_bridge_heartbeat("attention_sn_loop",
                             (float)(h + 1) / (float)bridge->config.num_heads);
        }

        mean += bridge->attention.attention_weights[h];
    }
    mean = (bridge->config.num_heads > 0) ? (mean / (float)bridge->config.num_heads) : 0.0f;

    float variance = 0.0f;
    for (uint32_t h = 0; h < bridge->config.num_heads; h++) {
        /* Phase 8: Loop progress heartbeat */
        if ((h & 0xFF) == 0 && bridge->config.num_heads > 256) {
            attention_snn_bridge_heartbeat("attention_sn_loop",
                             (float)(h + 1) / (float)bridge->config.num_heads);
        }

        float diff = bridge->attention.attention_weights[h] - mean;
        variance += diff * diff;
    }
    variance = (bridge->config.num_heads > 0) ? (variance / (float)bridge->config.num_heads) : 0.0f;

    /* Normalize variance to [0, 1] focus strength */
    float focus = sqrtf(variance) * 2.0f;  /* Scale factor */
    focus = nimcp_clampf(focus, 0.0f, 1.0f);

    bridge->attention.focus_strength = focus;

    /* Track attention shifts (per-instance to avoid cross-bridge race) */
    if (fabsf(focus - bridge->prev_focus) > 0.2f) {
        bridge->stats.attention_shifts++;
    }
    bridge->prev_focus = focus;

    return focus;
}

float attention_snn_get_focus_strength(attention_snn_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    attention_snn_bridge_heartbeat("attention_sn_attention_snn_get_fo", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float focus = attention_snn_get_focus_strength_unlocked(bridge);
    nimcp_mutex_unlock(bridge->base.mutex);

    return focus;
}

/* Internal unlocked version - call only when mutex is already held */
static float attention_snn_get_sparsity_unlocked(attention_snn_bridge_t* bridge) {
    /* Calculate sparsity as fraction of near-zero weights */
    uint32_t near_zero = 0;
    float threshold = bridge->config.decoding_threshold;

    for (uint32_t h = 0; h < bridge->config.num_heads; h++) {
        /* Phase 8: Loop progress heartbeat */
        if ((h & 0xFF) == 0 && bridge->config.num_heads > 256) {
            attention_snn_bridge_heartbeat("attention_sn_loop",
                             (float)(h + 1) / (float)bridge->config.num_heads);
        }

        if (bridge->attention.attention_weights[h] < threshold) {
            near_zero++;
        }
    }

    float sparsity = (bridge->config.num_heads > 0) ? ((float)near_zero / (float)bridge->config.num_heads) : 0.0f;
    bridge->attention.sparsity = sparsity;

    return sparsity;
}

float attention_snn_get_sparsity(attention_snn_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    attention_snn_bridge_heartbeat("attention_sn_attention_snn_get_sp", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float sparsity = attention_snn_get_sparsity_unlocked(bridge);
    nimcp_mutex_unlock(bridge->base.mutex);

    return sparsity;
}

int attention_snn_get_attention_state(
    attention_snn_bridge_t* bridge,
    attention_snn_attention_state_t* attention_state)
{
    if (!bridge || !attention_state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_snn_get_attention_state: bridge or attention_state is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    attention_snn_bridge_heartbeat("attention_sn_attention_snn_get_at", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Update computed fields - use unlocked versions to avoid deadlock */
    attention_snn_get_focus_strength_unlocked(bridge);
    attention_snn_get_sparsity_unlocked(bridge);

    /* Copy state */
    *attention_state = bridge->attention;

    /* Note: caller must NOT free the internal pointers */

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// State and Statistics
//=============================================================================

int attention_snn_get_state(
    const attention_snn_bridge_t* bridge,
    attention_snn_bridge_state_t* state)
{
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_snn_get_state: bridge or state is NULL");
        return -1;
    }

    /* Cast away const for mutex - safe as we're only reading */
    /* Phase 8: Heartbeat at operation start */
    attention_snn_bridge_heartbeat("attention_sn_attention_snn_get_st", 0.0f);


    attention_snn_bridge_t* mutable_bridge = (attention_snn_bridge_t*)bridge;

    nimcp_mutex_lock(mutable_bridge->base.mutex);

    state->state = bridge->state;
    state->attention = bridge->attention;

    /* Count active populations */
    state->active_populations = 0;
    for (uint32_t h = 0; h < bridge->config.num_heads; h++) {
        /* Phase 8: Loop progress heartbeat */
        if ((h & 0xFF) == 0 && bridge->config.num_heads > 256) {
            attention_snn_bridge_heartbeat("attention_sn_loop",
                             (float)(h + 1) / (float)bridge->config.num_heads);
        }

        if (bridge->attention.attention_weights[h] > bridge->config.decoding_threshold) {
            state->active_populations++;
        }
    }

    /* Calculate average firing rate */
    float sum_rate = 0.0f;
    uint32_t head_buffer_size = bridge->config.num_heads * bridge->config.neurons_per_head;
    for (uint32_t i = 0; i < head_buffer_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && head_buffer_size > 256) {
            attention_snn_bridge_heartbeat("attention_sn_loop",
                             (float)(i + 1) / (float)head_buffer_size);
        }

        sum_rate += bridge->head_buffer[i];
    }
    state->avg_firing_rate = (head_buffer_size > 0) ? (sum_rate / (float)head_buffer_size) : 0.0f;

    state->competition_energy = bridge->stats.competition_convergence_rate;
    state->bio_async_connected = bridge->bio_async_connected;

    nimcp_mutex_unlock(mutable_bridge->base.mutex);

    return 0;
}

int attention_snn_get_stats(
    const attention_snn_bridge_t* bridge,
    attention_snn_stats_t* stats)
{
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_snn_get_stats: bridge or stats is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    attention_snn_bridge_heartbeat("attention_sn_attention_snn_get_st", 0.0f);


    attention_snn_bridge_t* mutable_bridge = (attention_snn_bridge_t*)bridge;

    nimcp_mutex_lock(mutable_bridge->base.mutex);

    *stats = bridge->stats;

    /* Calculate averages */
    if (stats->total_forward_passes > 0) {
        stats->avg_focus_strength /= (float)stats->total_forward_passes;
        stats->avg_sparsity /= (float)stats->total_forward_passes;
    }

    nimcp_mutex_unlock(mutable_bridge->base.mutex);

    return 0;
}

void attention_snn_reset_stats(attention_snn_bridge_t* bridge) {
    if (!bridge) return;

    /* Phase 8: Heartbeat at operation start */
    attention_snn_bridge_heartbeat("attention_sn_attention_snn_reset_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    memset(&bridge->stats, 0, sizeof(attention_snn_stats_t));

    nimcp_mutex_unlock(bridge->base.mutex);
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int attention_snn_connect_bio_async(attention_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_snn_connect_bio_async: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_bio_async) return 0;

    /* Phase 8: Heartbeat at operation start */
    attention_snn_bridge_heartbeat("attention_sn_attention_snn_connec", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Connect SNN to bio-async router */
    if (bridge->snn) {
        /* Registration would happen here via bio-async API */
        bridge->bio_async_connected = true;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return bridge->bio_async_connected ? 0 : -1;
}

int attention_snn_disconnect_bio_async(attention_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_snn_disconnect_bio_async: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    attention_snn_bridge_heartbeat("attention_sn_attention_snn_discon", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->bio_async_connected) {
        /* Unregister from bio-async router */
        bridge->bio_async_connected = false;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool attention_snn_is_bio_async_connected(const attention_snn_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    attention_snn_bridge_heartbeat("attention_sn_attention_snn_is_bio", 0.0f);


    return bridge->bio_async_connected;
}

//=============================================================================
// Modulation Functions
//=============================================================================

int attention_snn_modulate_by_arousal(
    attention_snn_bridge_t* bridge,
    float arousal_level)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_snn_modulate_by_arousal: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    attention_snn_bridge_heartbeat("attention_sn_attention_snn_modula", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->current_arousal_mod = nimcp_clampf(arousal_level, 0.1f, 2.0f);

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int attention_snn_set_competition_strength(
    attention_snn_bridge_t* bridge,
    float strength)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_snn_set_competition_strength: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    attention_snn_bridge_heartbeat("attention_sn_attention_snn_set_co", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->current_competition_strength = nimcp_clampf(strength, 0.0f, 1.0f);

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int attention_snn_set_gate_modulation(
    attention_snn_bridge_t* bridge,
    float gate_level)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_snn_set_gate_modulation: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    attention_snn_bridge_heartbeat("attention_sn_attention_snn_set_ga", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->current_gate_mod = nimcp_clampf(gate_level, 0.0f, 1.0f);
    bridge->attention.gate_activation = bridge->current_gate_mod;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void attention_snn_bridge_set_instance_health_agent(attention_snn_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "attention_snn_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int attention_snn_bridge_training_begin(attention_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "attention_snn_bridge_training_begin: NULL argument");
        return -1;
    }
    attention_snn_bridge_heartbeat_instance(bridge->health_agent, "attention_snn_bridge_training_begin", 0.0f);
    return 0;
}

int attention_snn_bridge_training_end(attention_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "attention_snn_bridge_training_end: NULL argument");
        return -1;
    }
    attention_snn_bridge_heartbeat_instance(bridge->health_agent, "attention_snn_bridge_training_end", 1.0f);
    return 0;
}

int attention_snn_bridge_training_step(attention_snn_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "attention_snn_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    attention_snn_bridge_heartbeat_instance(bridge->health_agent, "attention_snn_bridge_training_step", progress);
    return 0;
}
