/**
 * @file nimcp_emotion_snn_bridge.c
 * @brief Emotion System - SNN Bidirectional Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-06
 */

#include "cognitive/emotion/nimcp_emotion_snn_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"
#include "glial/myelin_sheath/nimcp_myelin_math.h"

#include <string.h>
#include <math.h>

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
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_dimension_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(emotion_snn_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_emotion_snn_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_emotion_snn_bridge_mesh_registry = NULL;

nimcp_error_t emotion_snn_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_emotion_snn_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "emotion_snn_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "emotion_snn_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_emotion_snn_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_emotion_snn_bridge_mesh_registry = registry;
    return err;
}

void emotion_snn_bridge_mesh_unregister(void) {
    if (g_emotion_snn_bridge_mesh_registry && g_emotion_snn_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_emotion_snn_bridge_mesh_registry, g_emotion_snn_bridge_mesh_id);
        g_emotion_snn_bridge_mesh_id = 0;
        g_emotion_snn_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from emotion_snn_bridge module (instance-level) */
static inline void emotion_snn_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_emotion_snn_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_emotion_snn_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_emotion_snn_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "EMOTION_SNN_BRIDGE"


//=============================================================================
// Internal Structure
//=============================================================================

struct emotion_snn_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    emotion_snn_config_t config;

    /* SNN Network */
    snn_network_t* snn;
    bool owns_snn;

    /* Populations */
    uint32_t input_pop;              /**< Input population ID */
    uint32_t hidden_pop;             /**< Hidden population ID */
    uint32_t output_pop;             /**< Output population ID */
    uint32_t va_pop;                 /**< Valence-arousal population ID */

    /* State */
    emotion_snn_state_t state;
    emotion_snn_emotion_state_t emotion_state;
    emotion_category_t prev_category;

    /* Buffers */
    float* input_buffer;
    float* hidden_buffer;
    float* output_buffer;
    float* va_buffer;

    /* Statistics */
    emotion_snn_stats_t stats;

    /* Modulation state */
    float current_intensity_mod;
    float current_arousal_mod;

    /* Bio-async */
    bool bio_async_connected;

    /* Phase 8: Instance-level health agent */
    nimcp_health_agent_t* health_agent;
};

//=============================================================================
// Helper Functions
//=============================================================================

static inline float sigmoid(float x) {
    return 1.0f / (1.0f + expf(-x));
}

/**
 * @brief Convert valence to population index
 */
static uint32_t valence_to_index(float valence, uint32_t n_neurons) {
    /* Map [-1, 1] to [0, n_neurons-1] */
    float normalized = (valence + 1.0f) * 0.5f;
    uint32_t idx = (uint32_t)(normalized * (n_neurons - 1));
    return (idx >= n_neurons) ? n_neurons - 1 : idx;
}

/**
 * @brief Convert arousal to population index
 */
static uint32_t arousal_to_index(float arousal, uint32_t n_neurons) {
    uint32_t idx = (uint32_t)(arousal * (n_neurons - 1));
    return (idx >= n_neurons) ? n_neurons - 1 : idx;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

emotion_snn_config_t emotion_snn_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    emotion_snn_bridge_heartbeat("emotion_snn__emotion_snn_config_d", 0.0f);


    emotion_snn_config_t config = {
        .input_dim = EMOTION_SNN_INPUT_DIM,
        .hidden_dim = NIMCP_MEDIUM_HIDDEN_SIZE,
        .output_dim = EMOTION_COUNT,
        .va_dim = EMOTION_SNN_VA_DIM,

        .encoding = EMOTION_SNN_ENCODE_POPULATION,
        .encoding_gain = 1.0f,
        .intensity_gain = 2.0f,
        .baseline_rate_hz = 5.0f,
        .max_rate_hz = 100.0f,

        .decoding = EMOTION_SNN_DECODE_SOFTMAX,
        .decoding_threshold = 0.3f,
        .confidence_gain = 1.0f,
        .temporal_smoothing = 0.8f,

        .enable_va_encoding = true,
        .va_encoding_gain = 1.5f,

        .dt_ms = EMOTION_SNN_DEFAULT_DT,
        .simulation_window_ms = EMOTION_SNN_ENCODING_WINDOW,

        .enable_bio_async = false,
        .enable_plasticity_integration = true,
        .enable_immune_modulation = false
    };
    return config;
}

emotion_snn_bridge_t* emotion_snn_create(const emotion_snn_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    emotion_snn_bridge_heartbeat("emotion_snn__emotion_snn_create", 0.0f);


    emotion_snn_bridge_t* bridge = nimcp_calloc(1, sizeof(emotion_snn_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = emotion_snn_config_default();
    }

    /* Initialize bridge base infrastructure (includes mutex) */
    if (bridge_base_init(&bridge->base, 0, "emotion_snn") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Failed to initialize bridge base in emotion_snn_create");
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }

    /* Create SNN network */
    snn_config_t snn_config;
    snn_config_feedforward(&snn_config,
        bridge->config.input_dim,
        bridge->config.hidden_dim,
        bridge->config.output_dim);

    snn_config.dt = bridge->config.dt_ms;
    snn_config.enable_bio_async = bridge->config.enable_bio_async;
    snn_config.n_populations = 0;  /* Use SNN_MAX_POPULATIONS */

    bridge->snn = snn_network_create(&snn_config);
    if (!bridge->snn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Failed to create SNN network in emotion_snn_create");
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }
    bridge->owns_snn = true;

    /* Create populations for emotion processing (G8: guarded lightweight)
     * API: snn_network_add_population_lightweight(network, n_neurons, neuron_type, name) */
    if (g8_add_lw_pop(bridge->snn, bridge->config.input_dim, NEURON_GENERIC_LIF,
                      "emotion_input", &bridge->input_pop) != 0
     || g8_add_lw_pop(bridge->snn, bridge->config.hidden_dim, NEURON_GENERIC_LIF,
                      "emotion_hidden", &bridge->hidden_pop) != 0
     || g8_add_lw_pop(bridge->snn, bridge->config.output_dim, NEURON_GENERIC_LIF,
                      "emotion_output", &bridge->output_pop) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "emotion_snn_create: core lightweight pop alloc failed");
        emotion_snn_destroy(bridge);
        return NULL;
    }

    /* Valence-arousal population (optional) */
    if (bridge->config.enable_va_encoding) {
        if (g8_add_lw_pop(bridge->snn, bridge->config.va_dim * 2,
                          NEURON_GENERIC_LIF, "emotion_va",
                          &bridge->va_pop) != 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
                "emotion_snn_create: va_pop alloc failed");
            emotion_snn_destroy(bridge);
            return NULL;
        }
    }

    /* Add connections using correct API:
     * snn_network_connect_populations(network, src_pop, dst_pop,
     *     topology, connectivity, synapse_type, weight_mean, weight_std) */
    snn_network_connect_populations(bridge->snn,
        bridge->input_pop, bridge->hidden_pop,
        SNN_TOPO_RANDOM, 0.3f, SYNAPSE_AMPA, 0.5f, 0.1f);

    snn_network_connect_populations(bridge->snn,
        bridge->hidden_pop, bridge->output_pop,
        SNN_TOPO_RANDOM, 0.5f, SYNAPSE_AMPA, 0.5f, 0.1f);

    /* G8/H1 input shunt: dense input_pop (id 0) → bridge's lightweight input. */
    snn_network_connect_populations(bridge->snn,
        /*src=dense input_pop*/ 0, bridge->input_pop,
        SNN_TOPO_FEEDFORWARD, 1.0f, SYNAPSE_AMPA, 0.6f, 0.1f);

    /* G8/H1 output shunt: bridge's output_pop → dense output_pop (id 2). */
    snn_network_connect_populations(bridge->snn,
        bridge->output_pop, /*dst=dense output_pop*/ 2,
        SNN_TOPO_FEEDFORWARD, 1.0f, SYNAPSE_AMPA, 0.7f, 0.05f);

    /* G8: finalize lightweight CSR after all connections built. */
    snn_network_finalize_connections(bridge->snn);

    /* Allocate buffers */
    bridge->input_buffer = nimcp_calloc(bridge->config.input_dim, sizeof(float));
    bridge->hidden_buffer = nimcp_calloc(bridge->config.hidden_dim, sizeof(float));
    bridge->output_buffer = nimcp_calloc(bridge->config.output_dim, sizeof(float));
    bridge->va_buffer = nimcp_calloc(bridge->config.va_dim * 2, sizeof(float));

    if (!bridge->input_buffer || !bridge->hidden_buffer ||
        !bridge->output_buffer || !bridge->va_buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate buffers in emotion_snn_create");
        emotion_snn_destroy(bridge);
        return NULL;
    }

    /* Initialize state */
    bridge->state = EMOTION_SNN_STATE_IDLE;
    bridge->emotion_state.current_category = EMOTION_NEUTRAL;
    bridge->prev_category = EMOTION_NEUTRAL;
    bridge->current_intensity_mod = 1.0f;
    bridge->current_arousal_mod = 1.0f;
    bridge->bio_async_connected = false;

    NIMCP_LOGGING_INFO("Created %s bridge", "emotion_snn");
    return bridge;
}

void emotion_snn_destroy(emotion_snn_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "emotion_snn");

    /* Phase 8: Heartbeat at operation start */
    emotion_snn_bridge_heartbeat("emotion_snn__emotion_snn_destroy", 0.0f);


    if (bridge->bio_async_connected) {
        emotion_snn_disconnect_bio_async(bridge);
    }

    if (bridge->snn && bridge->owns_snn) {
        snn_network_destroy(bridge->snn);
    }

    if (bridge->input_buffer) nimcp_free(bridge->input_buffer);
    if (bridge->hidden_buffer) nimcp_free(bridge->hidden_buffer);
    if (bridge->output_buffer) nimcp_free(bridge->output_buffer);
    if (bridge->va_buffer) nimcp_free(bridge->va_buffer);

    /* Cleanup base bridge infrastructure */
    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge);
    bridge = NULL;
}

int emotion_snn_reset(emotion_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_snn_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_snn_bridge_heartbeat("emotion_snn__emotion_snn_reset", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->snn) {
        snn_network_reset(bridge->snn);
    }

    /* Reset state */
    bridge->state = EMOTION_SNN_STATE_IDLE;
    memset(&bridge->emotion_state, 0, sizeof(bridge->emotion_state));
    bridge->emotion_state.current_category = EMOTION_NEUTRAL;
    bridge->prev_category = EMOTION_NEUTRAL;

    /* Clear buffers */
    memset(bridge->input_buffer, 0, bridge->config.input_dim * sizeof(float));
    memset(bridge->hidden_buffer, 0, bridge->config.hidden_dim * sizeof(float));
    memset(bridge->output_buffer, 0, bridge->config.output_dim * sizeof(float));
    memset(bridge->va_buffer, 0, bridge->config.va_dim * 2 * sizeof(float));

    bridge->current_intensity_mod = 1.0f;
    bridge->current_arousal_mod = 1.0f;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Encoding Functions
//=============================================================================

int emotion_snn_encode_observation(
    emotion_snn_bridge_t* bridge,
    const emotion_recognition_result_t* result)
{
    if (!bridge || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_snn_encode_observation: required parameter is NULL (bridge, result)");
        return -1;
    }
    BRIDGE_BBB_VALIDATE(bridge, result, sizeof(emotion_recognition_result_t));

    /* Phase 8: Heartbeat at operation start */
    emotion_snn_bridge_heartbeat("emotion_snn__emotion_snn_encode_o", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->state = EMOTION_SNN_STATE_ENCODING;

    /* Encode emotion category as rate-coded population activity */
    float rate_scale = bridge->config.encoding_gain * bridge->current_intensity_mod;
    int total_spikes = 0;

    /* Clear input buffer */
    memset(bridge->input_buffer, 0, bridge->config.input_dim * sizeof(float));

    /* Encode based on detected emotion category and confidence
     * emotion_recognition_result_t has: category, confidence, valence, arousal */
    uint32_t neurons_per_emotion = bridge->config.input_dim / (EMOTION_COUNT > 0 ? EMOTION_COUNT : 1);

    /* Set high activity for the detected emotion category */
    uint32_t detected_cat = (uint32_t)result->category;
    if (detected_cat < EMOTION_COUNT) {
        float rate = bridge->config.baseline_rate_hz +
                    result->confidence * rate_scale *
                    (bridge->config.max_rate_hz - bridge->config.baseline_rate_hz);

        for (uint32_t n = 0; n < neurons_per_emotion; n++) {
            /* Phase 8: Loop progress heartbeat */
            if ((n & 0xFF) == 0 && neurons_per_emotion > 256) {
                emotion_snn_bridge_heartbeat("emotion_snn__loop",
                                 (float)(n + 1) / (float)neurons_per_emotion);
            }

            uint32_t idx = detected_cat * neurons_per_emotion + n;
            if (idx < bridge->config.input_dim) {
                bridge->input_buffer[idx] = rate;
            }
        }
    }

    /* Set baseline activity for other emotion categories */
    for (uint32_t cat = 0; cat < EMOTION_COUNT; cat++) {
        /* Phase 8: Loop progress heartbeat */
        if ((cat & 0xFF) == 0 && EMOTION_COUNT > 256) {
            emotion_snn_bridge_heartbeat("emotion_snn__loop",
                             (float)(cat + 1) / (float)EMOTION_COUNT);
        }

        if (cat == detected_cat) continue;
        float rate = bridge->config.baseline_rate_hz;
        for (uint32_t n = 0; n < neurons_per_emotion; n++) {
            /* Phase 8: Loop progress heartbeat */
            if ((n & 0xFF) == 0 && neurons_per_emotion > 256) {
                emotion_snn_bridge_heartbeat("emotion_snn__loop",
                                 (float)(n + 1) / (float)neurons_per_emotion);
            }

            uint32_t idx = cat * neurons_per_emotion + n;
            if (idx < bridge->config.input_dim) {
                bridge->input_buffer[idx] = rate;
            }
        }
    }

    /* Set inputs to SNN network using snn_network_set_inputs */
    int ret = snn_network_set_inputs(bridge->snn, bridge->input_buffer, bridge->config.input_dim);
    if (ret != SNN_SUCCESS) {
        bridge->state = EMOTION_SNN_STATE_IDLE;
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "emotion_snn_encode_observation: validation failed");
        return -1;
    }

    /* Count active inputs */
    for (uint32_t i = 0; i < bridge->config.input_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->config.input_dim > 256) {
            emotion_snn_bridge_heartbeat("emotion_snn__loop",
                             (float)(i + 1) / (float)bridge->config.input_dim);
        }

        if (bridge->input_buffer[i] > bridge->config.baseline_rate_hz * 1.5f) {
            total_spikes++;
        }
    }

    /* Encode valence-arousal if enabled */
    if (bridge->config.enable_va_encoding) {
        emotion_snn_encode_valence_arousal(bridge,
            result->valence, result->arousal, result->intensity);
    }

    /* Update stats */
    bridge->stats.total_observations++;
    bridge->stats.total_spikes_generated += total_spikes;

    bridge->state = EMOTION_SNN_STATE_IDLE;

    nimcp_mutex_unlock(bridge->base.mutex);

    return total_spikes;
}

int emotion_snn_encode_features(
    emotion_snn_bridge_t* bridge,
    const float* features,
    uint32_t n_features,
    float valence,
    float arousal)
{
    if (!bridge || !features) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_snn_encode_features: required parameter is NULL (bridge, features)");
        return -1;
    }
    BRIDGE_BBB_VALIDATE(bridge, features, n_features * sizeof(float));

    /* Phase 8: Heartbeat at operation start */
    emotion_snn_bridge_heartbeat("emotion_snn__emotion_snn_encode_f", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->state = EMOTION_SNN_STATE_ENCODING;

    int total_spikes = 0;
    uint32_t n = (n_features < bridge->config.input_dim) ?
                  n_features : bridge->config.input_dim;

    /* Copy and scale features to input buffer */
    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            emotion_snn_bridge_heartbeat("emotion_snn__loop",
                             (float)(i + 1) / (float)n);
        }

        float scaled = features[i] * bridge->config.encoding_gain * bridge->current_intensity_mod;
        bridge->input_buffer[i] = bridge->config.baseline_rate_hz +
            scaled * (bridge->config.max_rate_hz - bridge->config.baseline_rate_hz);
        if (bridge->input_buffer[i] > bridge->config.baseline_rate_hz * 1.5f) {
            total_spikes++;
        }
    }

    /* Zero-fill remaining buffer slots */
    for (uint32_t i = n; i < bridge->config.input_dim; i++) {
        bridge->input_buffer[i] = bridge->config.baseline_rate_hz;
    }

    /* Set inputs to SNN network */
    int ret = snn_network_set_inputs(bridge->snn, bridge->input_buffer, bridge->config.input_dim);
    if (ret != SNN_SUCCESS) {
        bridge->state = EMOTION_SNN_STATE_IDLE;
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "emotion_snn_encode_features: validation failed");
        return -1;
    }

    /* Encode valence-arousal (stored for decoding) */
    if (bridge->config.enable_va_encoding) {
        int va_count = emotion_snn_encode_valence_arousal(bridge, valence, arousal, 1.0f);
        if (va_count > 0) total_spikes += va_count;
    }

    bridge->stats.total_observations++;
    bridge->stats.total_spikes_generated += total_spikes;

    bridge->state = EMOTION_SNN_STATE_IDLE;

    nimcp_mutex_unlock(bridge->base.mutex);

    return total_spikes;
}

int emotion_snn_encode_valence_arousal(
    emotion_snn_bridge_t* bridge,
    float valence,
    float arousal,
    float intensity)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_snn_encode_valence_arousal: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_va_encoding) return 0;

    /* Clamp values */
    /* Phase 8: Heartbeat at operation start */
    emotion_snn_bridge_heartbeat("emotion_snn__emotion_snn_encode_v", 0.0f);


    valence = nimcp_myelin_clamp(valence, -1.0f, 1.0f);
    arousal = nimcp_myelin_clamp(arousal, 0.0f, 1.0f);
    intensity = nimcp_myelin_clamp(intensity, 0.0f, 1.0f);

    int total_active = 0;
    uint32_t half_dim = bridge->config.va_dim;

    /* Encode valence in first half of VA buffer (population coding)
     * Values are stored for later decoding - VA is a side-channel state */
    uint32_t v_idx = valence_to_index(valence, half_dim);
    for (uint32_t i = 0; i < half_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && half_dim > 256) {
            emotion_snn_bridge_heartbeat("emotion_snn__loop",
                             (float)(i + 1) / (float)half_dim);
        }

        float dist = fabsf((float)i - (float)v_idx) / (float)half_dim;
        float rate = bridge->config.baseline_rate_hz +
            intensity * bridge->config.va_encoding_gain *
            expf(-dist * dist * 4.0f) *
            (bridge->config.max_rate_hz - bridge->config.baseline_rate_hz);
        bridge->va_buffer[i] = rate;
        if (rate > bridge->config.baseline_rate_hz * 1.5f) total_active++;
    }

    /* Encode arousal in second half of VA buffer */
    uint32_t a_idx = arousal_to_index(arousal, half_dim);
    for (uint32_t i = 0; i < half_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && half_dim > 256) {
            emotion_snn_bridge_heartbeat("emotion_snn__loop",
                             (float)(i + 1) / (float)half_dim);
        }

        float dist = fabsf((float)i - (float)a_idx) / (float)half_dim;
        float rate = bridge->config.baseline_rate_hz +
            intensity * bridge->config.va_encoding_gain *
            expf(-dist * dist * 4.0f) *
            (bridge->config.max_rate_hz - bridge->config.baseline_rate_hz);
        bridge->va_buffer[half_dim + i] = rate;
        if (rate > bridge->config.baseline_rate_hz * 1.5f) total_active++;
    }

    /* Update emotion state with encoded values */
    bridge->emotion_state.valence = valence;
    bridge->emotion_state.arousal = arousal;
    bridge->emotion_state.intensity = intensity;

    return total_active;
}

//=============================================================================
// Simulation Functions
//=============================================================================

int emotion_snn_simulate(emotion_snn_bridge_t* bridge, float duration_ms) {
    if (!bridge || !bridge->snn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_snn_simulate: required parameter is NULL (bridge, bridge->snn)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_snn_bridge_heartbeat("emotion_snn__emotion_snn_simulate", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->state = EMOTION_SNN_STATE_SIMULATING;

    int steps = (int)(duration_ms / bridge->config.dt_ms);
    for (int s = 0; s < steps; s++) {
        /* Phase 8: Loop progress heartbeat */
        if ((s & 0xFF) == 0 && steps > 256) {
            emotion_snn_bridge_heartbeat("emotion_snn__loop",
                             (float)(s + 1) / (float)steps);
        }

        snn_network_step(bridge->snn, bridge->config.dt_ms);
    }

    bridge->state = EMOTION_SNN_STATE_IDLE;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int emotion_snn_step(emotion_snn_bridge_t* bridge) {
    if (!bridge || !bridge->snn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_snn_step: required parameter is NULL (bridge, bridge->snn)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_snn_bridge_heartbeat("emotion_snn__emotion_snn_step", 0.0f);


    /* Safety gates: ethics + LGSS pre-check */
    BRIDGE_ETHICS_GATE(bridge, "emotion_snn_step");
    BRIDGE_LGSS_GATE(bridge, "emotion_snn_step");

    nimcp_mutex_lock(bridge->base.mutex);

    snn_network_step(bridge->snn, bridge->config.dt_ms);

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Notify coordinator of step cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

//=============================================================================
// Decoding Functions
//=============================================================================

emotion_category_t emotion_snn_get_category_confidences(
    emotion_snn_bridge_t* bridge,
    float* confidences)
{
    if (!bridge || !confidences) return EMOTION_UNKNOWN;

    /* Phase 8: Heartbeat at operation start */
    emotion_snn_bridge_heartbeat("emotion_snn__emotion_snn_get_cate", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->state = EMOTION_SNN_STATE_DECODING;

    /* Get firing rates from output population using snn_network_get_outputs */
    float rates[EMOTION_COUNT];
    memset(rates, 0, sizeof(rates));

    uint32_t n_outputs = (bridge->config.output_dim < EMOTION_COUNT) ?
                          bridge->config.output_dim : EMOTION_COUNT;
    int ret = snn_network_get_outputs(bridge->snn, rates, n_outputs);
    if (ret != SNN_SUCCESS) {
        bridge->state = EMOTION_SNN_STATE_IDLE;
        nimcp_mutex_unlock(bridge->base.mutex);
        return EMOTION_UNKNOWN;
    }

    /* Normalize to confidences based on decoding method */
    emotion_category_t best_cat = EMOTION_NEUTRAL;
    float best_conf = 0.0f;

    if (bridge->config.decoding == EMOTION_SNN_DECODE_SOFTMAX) {
        /* Softmax normalization */
        float max_rate = rates[0];
        for (uint32_t i = 1; i < EMOTION_COUNT; i++) {
            if (rates[i] > max_rate) max_rate = rates[i];
        }

        float sum_exp = 0.0f;
        for (uint32_t i = 0; i < EMOTION_COUNT; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && EMOTION_COUNT > 256) {
                emotion_snn_bridge_heartbeat("emotion_snn__loop",
                                 (float)(i + 1) / (float)EMOTION_COUNT);
            }

            float normalized = (rates[i] - max_rate) * 0.1f;  /* Temperature scaling */
            confidences[i] = expf(normalized);
            sum_exp += confidences[i];
        }

        for (uint32_t i = 0; i < EMOTION_COUNT; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && EMOTION_COUNT > 256) {
                emotion_snn_bridge_heartbeat("emotion_snn__loop",
                                 (float)(i + 1) / (float)EMOTION_COUNT);
            }

            confidences[i] /= sum_exp;
            confidences[i] *= bridge->config.confidence_gain;
            confidences[i] = nimcp_myelin_clamp(confidences[i], 0.0f, 1.0f);

            if (confidences[i] > best_conf) {
                best_conf = confidences[i];
                best_cat = (emotion_category_t)i;
            }
        }
    } else {
        /* Winner-take-all */
        float max_rate = 0.0f;
        for (uint32_t i = 0; i < EMOTION_COUNT; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && EMOTION_COUNT > 256) {
                emotion_snn_bridge_heartbeat("emotion_snn__loop",
                                 (float)(i + 1) / (float)EMOTION_COUNT);
            }

            float conf = rates[i] / bridge->config.max_rate_hz;
            conf *= bridge->config.confidence_gain;
            confidences[i] = nimcp_myelin_clamp(conf, 0.0f, 1.0f);

            if (rates[i] > max_rate) {
                max_rate = rates[i];
                best_cat = (emotion_category_t)i;
                best_conf = confidences[i];
            }
        }
    }

    /* Apply temporal smoothing */
    float alpha = bridge->config.temporal_smoothing;
    for (uint32_t i = 0; i < EMOTION_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && EMOTION_COUNT > 256) {
            emotion_snn_bridge_heartbeat("emotion_snn__loop",
                             (float)(i + 1) / (float)EMOTION_COUNT);
        }

        bridge->emotion_state.category_confidences[i] =
            alpha * bridge->emotion_state.category_confidences[i] +
            (1.0f - alpha) * confidences[i];
        confidences[i] = bridge->emotion_state.category_confidences[i];
    }

    /* Update state */
    if (best_conf >= bridge->config.decoding_threshold) {
        if (bridge->emotion_state.current_category != best_cat) {
            bridge->prev_category = bridge->emotion_state.current_category;
            bridge->emotion_state.current_category = best_cat;
            bridge->stats.emotion_transitions++;
        }
    }

    bridge->stats.total_decodings++;
    bridge->stats.category_detections[best_cat]++;
    bridge->stats.avg_confidence =
        (bridge->stats.avg_confidence * (bridge->stats.total_decodings - 1) + best_conf) /
        bridge->stats.total_decodings;

    bridge->emotion_state.last_update_us = nimcp_time_get_us();

    bridge->state = EMOTION_SNN_STATE_IDLE;

    nimcp_mutex_unlock(bridge->base.mutex);

    return best_cat;
}

int emotion_snn_get_valence_arousal(
    emotion_snn_bridge_t* bridge,
    float* valence,
    float* arousal)
{
    if (!bridge || !valence || !arousal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_snn_get_valence_arousal: required parameter is NULL (bridge, valence, arousal)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_snn_bridge_heartbeat("emotion_snn__emotion_snn_get_vale", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->config.enable_va_encoding) {
        *valence = 0.0f;
        *arousal = 0.5f;
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    uint32_t half_dim = bridge->config.va_dim;

    /* Decode valence from stored VA buffer (population vector decoding)
     * VA is an encoding layer - we decode from the buffer used during encoding
     * which represents the current encoded state */
    float v_weighted_sum = 0.0f;
    float v_total_rate = 0.0f;
    for (uint32_t i = 0; i < half_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && half_dim > 256) {
            emotion_snn_bridge_heartbeat("emotion_snn__loop",
                             (float)(i + 1) / (float)half_dim);
        }

        float rate = bridge->va_buffer[i];  /* Use stored encoding buffer */
        float pos = (half_dim > 1) ? ((float)i / (float)(half_dim - 1)) * 2.0f - 1.0f : 0.0f;  /* Map to [-1, 1] */
        v_weighted_sum += rate * pos;
        v_total_rate += rate;
    }
    *valence = (v_total_rate > 0.01f) ? (v_weighted_sum / v_total_rate) : 0.0f;
    *valence = nimcp_myelin_clamp(*valence, -1.0f, 1.0f);

    /* Decode arousal from second half of VA buffer */
    float a_weighted_sum = 0.0f;
    float a_total_rate = 0.0f;
    for (uint32_t i = 0; i < half_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && half_dim > 256) {
            emotion_snn_bridge_heartbeat("emotion_snn__loop",
                             (float)(i + 1) / (float)half_dim);
        }

        float rate = bridge->va_buffer[half_dim + i];  /* Use stored encoding buffer */
        float pos = (half_dim > 1) ? (float)i / (float)(half_dim - 1) : 0.0f;  /* Map to [0, 1] */
        a_weighted_sum += rate * pos;
        a_total_rate += rate;
    }
    *arousal = (a_total_rate > 0.01f) ? (a_weighted_sum / a_total_rate) : 0.5f;
    *arousal = nimcp_myelin_clamp(*arousal, 0.0f, 1.0f);

    /* Update state */
    bridge->emotion_state.valence = *valence;
    bridge->emotion_state.arousal = *arousal;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int emotion_snn_get_emotion_state(
    emotion_snn_bridge_t* bridge,
    emotion_snn_emotion_state_t* emotion_state)
{
    if (!bridge || !emotion_state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_snn_get_emotion_state: required parameter is NULL (bridge, emotion_state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_snn_bridge_heartbeat("emotion_snn__emotion_snn_get_emot", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    *emotion_state = bridge->emotion_state;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float emotion_snn_get_transition_prob(
    emotion_snn_bridge_t* bridge,
    emotion_category_t from_category,
    emotion_category_t to_category)
{
    if (!bridge) return 0.0f;
    if (from_category >= EMOTION_COUNT || to_category >= EMOTION_COUNT) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    emotion_snn_bridge_heartbeat("emotion_snn__emotion_snn_get_tran", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Get current confidences */
    float from_conf = bridge->emotion_state.category_confidences[from_category];
    float to_conf = bridge->emotion_state.category_confidences[to_category];

    /* Simple transition probability based on relative confidences */
    float prob = 0.0f;
    if (from_conf > 0.01f) {
        prob = to_conf / (from_conf + to_conf + 0.01f);
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return prob;
}

//=============================================================================
// State and Statistics
//=============================================================================

int emotion_snn_get_state(
    const emotion_snn_bridge_t* bridge,
    emotion_snn_bridge_state_t* state)
{
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_snn_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_snn_bridge_heartbeat("emotion_snn__emotion_snn_get_stat", 0.0f);


    nimcp_mutex_lock(((emotion_snn_bridge_t*)bridge)->base.mutex);

    state->state = bridge->state;
    state->emotion = bridge->emotion_state;
    state->active_populations = 4;  /* input, hidden, output, va */
    state->avg_firing_rate = 0.0f;
    state->bio_async_connected = bridge->bio_async_connected;

    /* Calculate average firing rate using population rate API */
    if (bridge->snn) {
        /* Get overall output population firing rate over a 100ms window */
        state->avg_firing_rate = snn_network_get_population_rate(
            bridge->snn, bridge->output_pop, 100.0f);
    }

    nimcp_mutex_unlock(((emotion_snn_bridge_t*)bridge)->base.mutex);

    return 0;
}

int emotion_snn_get_stats(
    const emotion_snn_bridge_t* bridge,
    emotion_snn_stats_t* stats)
{
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_snn_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_snn_bridge_heartbeat("emotion_snn__emotion_snn_get_stat", 0.0f);


    nimcp_mutex_lock(((emotion_snn_bridge_t*)bridge)->base.mutex);

    *stats = bridge->stats;

    nimcp_mutex_unlock(((emotion_snn_bridge_t*)bridge)->base.mutex);

    return 0;
}

void emotion_snn_reset_stats(emotion_snn_bridge_t* bridge) {
    if (!bridge) return;

    /* Phase 8: Heartbeat at operation start */
    emotion_snn_bridge_heartbeat("emotion_snn__emotion_snn_reset_st", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    memset(&bridge->stats, 0, sizeof(bridge->stats));

    nimcp_mutex_unlock(bridge->base.mutex);
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int emotion_snn_connect_bio_async(emotion_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL in emotion_snn_connect_bio_async");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!bridge->config.enable_bio_async) return 0;

    /* Phase 8: Heartbeat at operation start */
    emotion_snn_bridge_heartbeat("emotion_snn__emotion_snn_connect_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Bio-async connection would be implemented here */
    bridge->bio_async_connected = true;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int emotion_snn_disconnect_bio_async(emotion_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL in emotion_snn_disconnect_bio_async");
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_snn_bridge_heartbeat("emotion_snn__emotion_snn_disconne", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->bio_async_connected = false;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool emotion_snn_is_bio_async_connected(const emotion_snn_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    emotion_snn_bridge_heartbeat("emotion_snn__emotion_snn_is_bio_a", 0.0f);


    return bridge->bio_async_connected;
}

//=============================================================================
// Modulation Functions
//=============================================================================

int emotion_snn_modulate_by_arousal(
    emotion_snn_bridge_t* bridge,
    float arousal_level)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_snn_modulate_by_arousal: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_snn_bridge_heartbeat("emotion_snn__emotion_snn_modulate", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Higher arousal increases gain */
    bridge->current_arousal_mod = 0.5f + arousal_level;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int emotion_snn_set_intensity_modulation(
    emotion_snn_bridge_t* bridge,
    float intensity)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_snn_set_intensity_modulation: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_snn_bridge_heartbeat("emotion_snn__emotion_snn_set_inte", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->current_intensity_mod = nimcp_myelin_clamp(intensity * bridge->config.intensity_gain, 0.1f, 3.0f);
    bridge->emotion_state.intensity = intensity;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void emotion_snn_bridge_set_instance_health_agent(emotion_snn_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (bridge) {
        bridge->health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training stubs
 * ============================================================================ */
int emotion_snn_bridge_training_begin(emotion_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotion_snn_bridge_training_begin: NULL argument");
        return -1;
    }
    emotion_snn_bridge_heartbeat_instance(bridge->health_agent, "emotion_snn_training_begin", 0.0f);
    return 0;
}

int emotion_snn_bridge_training_end(emotion_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotion_snn_bridge_training_end: NULL argument");
        return -1;
    }
    emotion_snn_bridge_heartbeat_instance(bridge->health_agent, "emotion_snn_training_end", 1.0f);
    return 0;
}

int emotion_snn_bridge_training_step(emotion_snn_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotion_snn_bridge_training_step: NULL argument");
        return -1;
    }
    emotion_snn_bridge_heartbeat_instance(bridge->health_agent, "emotion_snn_training_step", progress);
    return 0;
}

/* ============================================================================
 * Security Integration (BBB)
 * ============================================================================ */
BRIDGE_DEFINE_SECURITY_SETTERS(emotion_snn_bridge)
