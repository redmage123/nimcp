/*
 * DEPRECATED — STATUE (audit 2026-04-30)
 *
 * snn_emotion_bridge_create has zero callers in production code. The
 * actual SNN-emotion path now goes through
 * src/cognitive/emotion/nimcp_emotion_snn_bridge.c. Do NOT delete —
 * the SNN wave campaign may revive this. Either wire a consumer or
 * delete before the next major version. Do not extend.
 */

/**
 * @file nimcp_snn_emotion_bridge.c
 * @brief SNN-Emotion integration bridge implementation
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#include "snn/bridges/nimcp_snn_emotion_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_messages.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_math_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(snn_emotion_bridge)

#define BIO_MODULE_SNN_EMOTION_BRIDGE 0x0620

void snn_emotion_config_default(snn_emotion_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_emotion_config_default: null config pointer");
        return;
    }

    config->arousal_rate_min = 5.0f;
    config->arousal_rate_max = 100.0f;
    config->valence_threshold = 0.1f;
    config->arousal_boost_factor = 1.5f;
    config->valence_weight_scaling = 0.2f;
    config->modulate_excitability = true;
    config->enable_theta_sync = true;
    config->theta_frequency = 6.0f;
    config->theta_bandwidth = 2.0f;
    config->valence_positive_pop_id = 0;
    config->valence_negative_pop_id = 0;
    config->arousal_pop_id = 0;
    config->update_interval_ms = 50.0f;
    config->enable_bio_async = false;
}

snn_emotion_bridge_t* snn_emotion_bridge_create(
    const snn_emotion_config_t* config,
    snn_network_t* snn,
    emotional_system_t* emotion_system
) {
    if (!config || !snn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Null parameters to snn_emotion_bridge_create: config=%p, snn=%p",
                             (void*)config, (void*)snn);
        return NULL;
    }

    snn_emotion_bridge_t* bridge = nimcp_malloc(sizeof(snn_emotion_bridge_t));
    if (!bridge) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(snn_emotion_bridge_t),
                          "Failed to allocate SNN-emotion bridge");
        return NULL;
    }

    memset(bridge, 0, sizeof(snn_emotion_bridge_t));
    if (bridge_base_init(&bridge->base, 0, "snn_emotion") != 0) { nimcp_free(bridge); return NULL; }
    bridge->snn = snn;
    bridge->emotion_system = emotion_system;
    bridge->config = *config;

    if (config->valence_positive_pop_id > 0) {
        bridge->valence_pos_pop = snn_network_get_population(snn, config->valence_positive_pop_id);
    }
    if (config->valence_negative_pop_id > 0) {
        bridge->valence_neg_pop = snn_network_get_population(snn, config->valence_negative_pop_id);
    }
    if (config->arousal_pop_id > 0) {
        bridge->arousal_pop = snn_network_get_population(snn, config->arousal_pop_id);
    }

    NIMCP_LOGGING_INFO("Created SNN-emotion bridge");
    return bridge;
}

void snn_emotion_bridge_destroy(snn_emotion_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_emotion_bridge_destroy: null bridge pointer");
        return;
    }

    if (bridge->base.bio_async_enabled) {
        snn_emotion_bridge_disconnect_bio_async(bridge);
    }
    if (bridge->encoder) snn_encoder_destroy(bridge->encoder);
    if (bridge->decoder) snn_decoder_destroy(bridge->decoder);

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Destroyed SNN-emotion bridge");
}

int snn_emotion_bridge_connect_bio_async(snn_emotion_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_emotion_bridge_connect_bio_async: null bridge pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SNN_EMOTION_BRIDGE,
        .module_name = "snn_emotion_bridge",
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

int snn_emotion_bridge_disconnect_bio_async(snn_emotion_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_emotion_bridge_disconnect_bio_async: null bridge pointer");
        return -1;
    }
    if (!bridge->base.bio_async_enabled) return 0;
    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    return 0;
}

bool snn_emotion_bridge_is_bio_async_connected(const snn_emotion_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}

int snn_emotion_bridge_update(snn_emotion_bridge_t* bridge, float dt) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_emotion_bridge_update: null bridge pointer");
        return SNN_ERROR_NULL_POINTER;
    }

    bridge->last_update_time += dt;
    if (bridge->last_update_time < bridge->config.update_interval_ms) {
        return 0;
    }
    bridge->last_update_time = 0.0f;

    float valence = 0.0f, arousal = 0.0f;
    snn_emotion_decode_from_spikes(bridge, &valence, &arousal);

    bridge->state.decoded_valence = valence;
    bridge->state.decoded_arousal = arousal;
    bridge->state.emotional_intensity = sqrtf(valence * valence + arousal * arousal);

    if (bridge->config.modulate_excitability) {
        snn_emotion_modulate_populations(bridge);
    }

    if (bridge->config.enable_theta_sync) {
        snn_emotion_detect_theta(bridge, &bridge->state.theta);
    }

    bridge->state.sync_count++;
    bridge->state.avg_valence = (bridge->state.avg_valence * 0.95f + valence * 0.05f);
    if (!isfinite(bridge->state.avg_valence)) bridge->state.avg_valence = valence;
    bridge->state.avg_arousal = (bridge->state.avg_arousal * 0.95f + arousal * 0.05f);
    if (!isfinite(bridge->state.avg_arousal)) bridge->state.avg_arousal = arousal;

    return 0;
}

int snn_emotion_decode_from_spikes(
    snn_emotion_bridge_t* bridge,
    float* valence_out,
    float* arousal_out
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_emotion_decode_from_spikes: null bridge pointer");
        return SNN_ERROR_NULL_POINTER;
    }

    float pos_rate = 0.0f, neg_rate = 0.0f, arousal_rate = 0.0f;

    if (bridge->valence_pos_pop) {
        pos_rate = snn_population_get_firing_rate(bridge->valence_pos_pop);
    }
    if (bridge->valence_neg_pop) {
        neg_rate = snn_population_get_firing_rate(bridge->valence_neg_pop);
    }
    if (bridge->arousal_pop) {
        arousal_rate = snn_population_get_firing_rate(bridge->arousal_pop);
    }

    float valence = (pos_rate - neg_rate) / (pos_rate + neg_rate + 1e-6f);
    float min_r = bridge->config.arousal_rate_min;
    float max_r = bridge->config.arousal_rate_max;
    float arousal = (arousal_rate - min_r) / (max_r - min_r);
    if (arousal < 0.0f) arousal = 0.0f;
    if (arousal > 1.0f) arousal = 1.0f;

    if (valence_out) *valence_out = valence;
    if (arousal_out) *arousal_out = arousal;

    return 0;
}

int snn_emotion_detect_theta(
    snn_emotion_bridge_t* bridge,
    snn_theta_state_t* theta_state
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_emotion_detect_theta: null bridge pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!theta_state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_emotion_detect_theta: null theta_state pointer");
        return SNN_ERROR_NULL_POINTER;
    }

    theta_state->frequency = bridge->config.theta_frequency;
    theta_state->amplitude = bridge->state.decoded_arousal * 0.8f;
    theta_state->phase = fmodf(bridge->last_update_time * 0.001f * NIMCP_TWO_PI_F *
                               bridge->config.theta_frequency, NIMCP_TWO_PI_F);
    theta_state->coherence = 0.5f + bridge->state.emotional_intensity * 0.5f;
    theta_state->is_synchronized = (theta_state->coherence > 0.7f);

    return 0;
}

int snn_emotion_modulate_populations(snn_emotion_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_emotion_modulate_populations: null bridge pointer");
        return SNN_ERROR_NULL_POINTER;
    }

    float arousal_mod = 1.0f + bridge->state.decoded_arousal *
                        (bridge->config.arousal_boost_factor - 1.0f);
    bridge->state.arousal_modulation = arousal_mod;
    bridge->state.valence_modulation = bridge->state.decoded_valence *
                                       bridge->config.valence_weight_scaling;

    return 0;
}

int snn_emotion_encode_to_spikes(
    snn_emotion_bridge_t* bridge,
    float valence,
    float arousal
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_emotion_encode_to_spikes: null bridge pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    return 0;
}

int snn_emotion_bridge_get_state(
    const snn_emotion_bridge_t* bridge,
    snn_emotion_state_t* state
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_emotion_bridge_get_state: null bridge pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_emotion_bridge_get_state: null state pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    *state = bridge->state;
    return 0;
}

float snn_emotion_get_decoded_valence(const snn_emotion_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_emotion_get_decoded_valence: null bridge pointer");
        return 0.0f;
    }
    return bridge->state.decoded_valence;
}

float snn_emotion_get_decoded_arousal(const snn_emotion_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_emotion_get_decoded_arousal: null bridge pointer");
        return 0.0f;
    }
    return bridge->state.decoded_arousal;
}

bool snn_emotion_is_theta_synchronized(const snn_emotion_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->state.theta.is_synchronized;
}

int snn_emotion_get_stats(
    const snn_emotion_bridge_t* bridge,
    uint32_t* sync_count,
    float* avg_valence,
    float* avg_arousal
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_emotion_get_stats: null bridge pointer");
        return SNN_ERROR_NULL_POINTER;
    }
    if (sync_count) *sync_count = bridge->state.sync_count;
    if (avg_valence) *avg_valence = bridge->state.avg_valence;
    if (avg_arousal) *avg_arousal = bridge->state.avg_arousal;
    return 0;
}

void snn_emotion_reset_stats(snn_emotion_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_emotion_reset_stats: null bridge pointer");
        return;
    }
    bridge->state.sync_count = 0;
    bridge->state.avg_valence = 0.0f;
    bridge->state.avg_arousal = 0.0f;
}
