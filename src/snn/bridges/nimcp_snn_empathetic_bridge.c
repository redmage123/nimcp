/**
 * @file nimcp_snn_empathetic_bridge.c
 * @brief SNN-Empathy integration bridge implementation
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#include "snn/bridges/nimcp_snn_empathetic_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(snn_empathetic_bridge)

#define BIO_MODULE_SNN_EMPATHETIC_BRIDGE 0x0622
#define DEFAULT_MAX_RESPONSES 256

void snn_empathetic_config_default(snn_empathetic_config_t* config) {
    if (!config) return;

    config->mirror_activation_threshold = 30.0f;
    config->empathy_gain = 1.0f;
    config->resonance_decay_rate = 0.2f;
    config->action_observation_weight = 0.7f;
    config->self_other_discrimination = 0.6f;
    config->mirror_neuron_pop_id = 0;
    config->sts_pop_id = 0;
    config->ipl_pop_id = 0;
    config->acc_pop_id = 0;
    config->enable_emotional_contagion = true;
    config->enable_perspective_taking = true;
    config->update_interval_ms = 50.0f;
    config->enable_bio_async = false;
}

snn_empathetic_bridge_t* snn_empathetic_bridge_create(
    const snn_empathetic_config_t* config,
    snn_network_t* snn,
    empathetic_system_t* empathetic_system
) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_empathetic_bridge_create: config is NULL");
        return NULL;
    }
    if (!snn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_empathetic_bridge_create: snn is NULL");
        return NULL;
    }

    snn_empathetic_bridge_t* bridge = nimcp_malloc(sizeof(snn_empathetic_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_empathetic_bridge_create: failed to allocate bridge");
        return NULL;
    }

    memset(bridge, 0, sizeof(snn_empathetic_bridge_t));
    if (bridge_base_init(&bridge->base, 0, "snn_empathetic") != 0) { nimcp_free(bridge); return NULL; }
    bridge->snn = snn;
    bridge->empathetic_system = empathetic_system;
    bridge->config = *config;
    bridge->max_responses = DEFAULT_MAX_RESPONSES;

    bridge->responses = nimcp_malloc(sizeof(snn_empathy_response_t) * bridge->max_responses);
    if (!bridge->responses) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_empathetic_bridge_create: failed to allocate response array");
        nimcp_free(bridge);
        return NULL;
    }
    memset(bridge->responses, 0, sizeof(snn_empathy_response_t) * bridge->max_responses);

    if (config->mirror_neuron_pop_id > 0) {
        bridge->mirror_pop = snn_network_get_population(snn, config->mirror_neuron_pop_id);
    }
    if (config->sts_pop_id > 0) {
        bridge->sts_pop = snn_network_get_population(snn, config->sts_pop_id);
    }
    if (config->ipl_pop_id > 0) {
        bridge->ipl_pop = snn_network_get_population(snn, config->ipl_pop_id);
    }
    if (config->acc_pop_id > 0) {
        bridge->acc_pop = snn_network_get_population(snn, config->acc_pop_id);
    }

    NIMCP_LOGGING_INFO("Created SNN-empathetic bridge");
    return bridge;
}

void snn_empathetic_bridge_destroy(snn_empathetic_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->base.bio_async_enabled) {
        snn_empathetic_bridge_disconnect_bio_async(bridge);
    }
    if (bridge->encoder) snn_encoder_destroy(bridge->encoder);
    if (bridge->decoder) snn_decoder_destroy(bridge->decoder);
    if (bridge->responses) nimcp_free(bridge->responses);

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Destroyed SNN-empathetic bridge");
}

int snn_empathetic_bridge_connect_bio_async(snn_empathetic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_empathetic_bridge_connect_bio_async: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SNN_EMPATHETIC_BRIDGE,
        .module_name = "snn_empathetic_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
        return 0;
    }

    NIMCP_LOGGING_WARN("Bio-async router not available");
    return SNN_ERROR_OPERATION_FAILED;
}

int snn_empathetic_bridge_disconnect_bio_async(snn_empathetic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_empathetic_bridge_disconnect_bio_async: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!bridge->base.bio_async_enabled) return 0;
    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    return 0;
}

bool snn_empathetic_bridge_is_bio_async_connected(const snn_empathetic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_empathetic_bridge_is_bio_async_connected: bridge is NULL");
        return false;
    }
    return bridge->base.bio_async_enabled;
}

int snn_empathetic_bridge_update(snn_empathetic_bridge_t* bridge, float dt) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_empathetic_bridge_update: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    bridge->last_update_time += dt;
    if (bridge->last_update_time < bridge->config.update_interval_ms) {
        return 0;
    }
    bridge->last_update_time = 0.0f;

    /* Get current mirror neuron activity */
    float mirror_rate = 0.0f;
    if (bridge->mirror_pop) {
        mirror_rate = snn_population_get_firing_rate(bridge->mirror_pop);
    }

    /* Compute mirror activation level */
    float activation = mirror_rate / bridge->config.mirror_activation_threshold;
    if (activation > 1.0f) activation = 1.0f;
    bridge->state.mirror_activation_level = activation;

    /* Decay resonance */
    snn_empathetic_decay_activation(bridge, dt);

    /* Update statistics */
    bridge->state.avg_mirror_activation = bridge->state.avg_mirror_activation * 0.95f +
                                           activation * 0.05f;
    if (!isfinite(bridge->state.avg_mirror_activation)) bridge->state.avg_mirror_activation = activation;

    return 0;
}

int snn_empathetic_observe_action(
    snn_empathetic_bridge_t* bridge,
    const float* action_spikes,
    uint32_t n_spikes,
    float* observation_rate_out
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_empathetic_observe_action: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!action_spikes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_empathetic_observe_action: action_spikes is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    /* Compute observation rate from spike train */
    float observation_rate = 0.0f;
    if (bridge->sts_pop) {
        observation_rate = snn_population_get_firing_rate(bridge->sts_pop);
    }

    /* Apply observation weight */
    observation_rate *= bridge->config.action_observation_weight;
    bridge->state.current_observed_action_rate = observation_rate;

    if (observation_rate_out) *observation_rate_out = observation_rate;

    return 0;
}

int snn_empathetic_trigger_mirror_response(
    snn_empathetic_bridge_t* bridge,
    float observation_rate,
    snn_empathy_response_t* response_out
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_empathetic_trigger_mirror_response: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    /* Check if observation exceeds threshold */
    if (observation_rate < bridge->config.mirror_activation_threshold) {
        bridge->state.failed_mirror_count++;
        return SNN_ERROR_OPERATION_FAILED;
    }

    /* Generate mirror response */
    snn_empathy_response_t response;
    memset(&response, 0, sizeof(snn_empathy_response_t));

    response.response_id = bridge->state.empathy_response_count;
    response.mirror_activation = observation_rate / bridge->config.mirror_activation_threshold;
    if (response.mirror_activation > 1.0f) response.mirror_activation = 1.0f;

    /* Apply empathy gain */
    response.mirror_activation *= bridge->config.empathy_gain;

    /* Compute emotional resonance if enabled */
    if (bridge->config.enable_emotional_contagion && bridge->acc_pop) {
        float acc_rate = snn_population_get_firing_rate(bridge->acc_pop);
        response.emotional_resonance = acc_rate / 100.0f;
        if (response.emotional_resonance > 1.0f) response.emotional_resonance = 1.0f;
    }

    /* Compute perspective taking accuracy if enabled */
    if (bridge->config.enable_perspective_taking && bridge->ipl_pop) {
        float ipl_rate = snn_population_get_firing_rate(bridge->ipl_pop);
        response.perspective_accuracy = ipl_rate / 80.0f;
        if (response.perspective_accuracy > 1.0f) response.perspective_accuracy = 1.0f;
    }

    /* Self-other distinction */
    response.self_other_distinction = bridge->config.self_other_discrimination;
    response.action_recognized = true;
    response.response_time_ms = bridge->last_update_time;

    /* Store response */
    if (bridge->response_count < bridge->max_responses) {
        bridge->responses[bridge->response_count] = response;
        bridge->response_count++;
    }

    bridge->state.empathy_response_count++;
    bridge->state.action_recognition_count++;

    if (response_out) *response_out = response;

    return 0;
}

int snn_empathetic_compute_resonance(
    snn_empathetic_bridge_t* bridge,
    float observation_rate,
    float mirror_rate,
    float* coherence_out
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_empathetic_compute_resonance: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!coherence_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_empathetic_compute_resonance: coherence_out is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    /* Compute coherence as normalized cross-correlation */
    float mean_rate = (observation_rate + mirror_rate) / 2.0f;
    if (mean_rate < 1e-6f) {
        *coherence_out = 0.0f;
        return 0;
    }

    float coherence = 1.0f - fabsf(observation_rate - mirror_rate) / (mean_rate + 1e-6f);
    if (coherence < 0.0f) coherence = 0.0f;

    bridge->state.resonance_coherence = coherence;
    *coherence_out = coherence;

    return 0;
}

int snn_empathetic_recognize_action(
    snn_empathetic_bridge_t* bridge,
    const float* spike_pattern,
    uint32_t n_spikes,
    bool* recognized_out
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_empathetic_recognize_action: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!spike_pattern) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_empathetic_recognize_action: spike_pattern is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!recognized_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_empathetic_recognize_action: recognized_out is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    /* Simple recognition: check if spike rate is above threshold */
    float total_rate = 0.0f;
    for (uint32_t i = 0; i < n_spikes; i++) {
        total_rate += spike_pattern[i];
    }
    float avg_rate = total_rate / (float)n_spikes;

    bool recognized = (avg_rate >= bridge->config.mirror_activation_threshold);
    *recognized_out = recognized;

    return 0;
}

int snn_empathetic_decay_activation(
    snn_empathetic_bridge_t* bridge,
    float dt
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_empathetic_decay_activation: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    float decay_factor = expf(-bridge->config.resonance_decay_rate * dt / 1000.0f);
    bridge->state.mirror_activation_level *= decay_factor;

    return 0;
}

int snn_empathetic_emotional_contagion(
    snn_empathetic_bridge_t* bridge,
    float observed_emotion_intensity,
    float* mirrored_emotion_out
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_empathetic_emotional_contagion: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!mirrored_emotion_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_empathetic_emotional_contagion: mirrored_emotion_out is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    if (!bridge->config.enable_emotional_contagion) {
        *mirrored_emotion_out = 0.0f;
        return 0;
    }

    /* Mirror emotion with empathy gain */
    float mirrored = observed_emotion_intensity * bridge->config.empathy_gain;
    if (mirrored > 1.0f) mirrored = 1.0f;

    *mirrored_emotion_out = mirrored;

    return 0;
}

int snn_empathetic_bridge_get_state(
    const snn_empathetic_bridge_t* bridge,
    snn_empathetic_state_t* state
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_empathetic_bridge_get_state: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_empathetic_bridge_get_state: state is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    *state = bridge->state;
    return 0;
}

float snn_empathetic_get_mirror_activation(const snn_empathetic_bridge_t* bridge) {
    return bridge ? bridge->state.mirror_activation_level : 0.0f;
}

uint32_t snn_empathetic_get_response_count(const snn_empathetic_bridge_t* bridge) {
    return bridge ? bridge->state.empathy_response_count : 0;
}

float snn_empathetic_get_resonance_coherence(const snn_empathetic_bridge_t* bridge) {
    return bridge ? bridge->state.resonance_coherence : 0.0f;
}

int snn_empathetic_get_response(
    const snn_empathetic_bridge_t* bridge,
    uint32_t response_id,
    snn_empathy_response_t* response_out
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_empathetic_get_response: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!response_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_empathetic_get_response: response_out is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    for (uint32_t i = 0; i < bridge->response_count; i++) {
        if (bridge->responses[i].response_id == response_id) {
            *response_out = bridge->responses[i];
            return 0;
        }
    }

    return SNN_ERROR_OPERATION_FAILED;
}

int snn_empathetic_get_stats(
    const snn_empathetic_bridge_t* bridge,
    uint32_t* response_count,
    float* avg_activation,
    float* avg_resonance
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_empathetic_get_stats: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (response_count) *response_count = bridge->state.empathy_response_count;
    if (avg_activation) *avg_activation = bridge->state.avg_mirror_activation;
    if (avg_resonance) *avg_resonance = bridge->state.avg_emotional_resonance;
    return 0;
}

void snn_empathetic_reset_stats(snn_empathetic_bridge_t* bridge) {
    if (!bridge) return;
    bridge->state.empathy_response_count = 0;
    bridge->state.avg_mirror_activation = 0.0f;
    bridge->state.avg_emotional_resonance = 0.0f;
    bridge->state.action_recognition_count = 0;
    bridge->state.failed_mirror_count = 0;
    bridge->response_count = 0;
}
