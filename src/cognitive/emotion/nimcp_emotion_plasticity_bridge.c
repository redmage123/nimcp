/**
 * @file nimcp_emotion_plasticity_bridge.c
 * @brief Emotion System - Plasticity Bidirectional Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-06
 */

#include "cognitive/emotion/nimcp_emotion_plasticity_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"
#include "glial/myelin_sheath/nimcp_myelin_math.h"

#include <string.h>
#include <math.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_constants.h"

BRIDGE_BOILERPLATE(emotion_plasticity_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)

#define LOG_MODULE "EMOTION_PLASTICITY_BRIDGE"


//=============================================================================
// Internal Structure
//=============================================================================

struct emotion_plasticity_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    emotion_plasticity_config_t config;

    /* Synapses */
    emotion_plasticity_synapse_t synapses[EMOTION_PLASTICITY_MAX_SYNAPSES];
    uint32_t n_synapses;

    /* State */
    emotion_plasticity_state_t state;
    float global_learning_rate;
    float current_valence_mod;
    float current_arousal_mod;

    /* Per-emotion tracking */
    float emotion_sensitivity[EMOTION_COUNT];
    float emotion_extinction[EMOTION_COUNT];
    uint64_t emotion_last_stimulus[EMOTION_COUNT];
    uint64_t emotion_last_response[EMOTION_COUNT];

    /* Reward state */
    float pending_reward;
    uint64_t last_reward_time_us;

    /* Statistics */
    emotion_plasticity_stats_t stats;

    /* Callbacks */
    emotion_weight_change_cb weight_callback;
    void* callback_user_data;

    /* Bio-async */
    bool bio_async_connected;

    /* Phase 8: Instance-level health agent */
    nimcp_health_agent_t* health_agent;
};

//=============================================================================
// Helper Functions
//=============================================================================

static emotion_plasticity_synapse_t* find_synapse(
    emotion_plasticity_bridge_t* bridge,
    uint32_t synapse_id)
{
    for (uint32_t i = 0; i < bridge->n_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->n_synapses > 256) {
            emotion_plasticity_bridge_heartbeat("emotion_plas_loop",
                             (float)(i + 1) / (float)bridge->n_synapses);
        }

        if (bridge->synapses[i].synapse_id == synapse_id) {
            return &bridge->synapses[i];
        }
    }
    return NULL;  /* Not found is normal */
}

/**
 * @brief Compute STDP weight change
 */
static float compute_stdp_delta(
    emotion_plasticity_bridge_t* bridge,
    float dt_ms)
{
    float delta = 0.0f;

    if (dt_ms > 0) {
        /* Pre before post -> LTP */
        float tau = bridge->config.stdp_tau_plus;
        delta = bridge->config.stdp_a_plus * expf(-dt_ms / tau);

        /* Positive valence boosts LTP */
        if (bridge->config.enable_valence_modulation && bridge->current_valence_mod > 0) {
            delta *= (1.0f + bridge->current_valence_mod * bridge->config.positive_valence_ltp_boost);
        }
    } else {
        /* Post before pre -> LTD */
        float tau = bridge->config.stdp_tau_minus;
        delta = -bridge->config.stdp_a_minus * expf(dt_ms / tau);

        /* Negative valence boosts LTD */
        if (bridge->config.enable_valence_modulation && bridge->current_valence_mod < 0) {
            delta *= (1.0f - bridge->current_valence_mod * bridge->config.negative_valence_ltd_boost);
        }
    }

    /* Arousal modulation */
    if (bridge->config.enable_arousal_modulation) {
        delta *= (1.0f + bridge->current_arousal_mod * bridge->config.arousal_learning_gain);
    }

    return delta;
}

/**
 * @brief Update eligibility trace for synapse (unlocked version)
 */
static void update_eligibility_trace_unlocked(
    emotion_plasticity_bridge_t* bridge,
    emotion_plasticity_synapse_t* synapse,
    float dt_ms)
{
    if (!bridge->config.enable_eligibility) return;

    /* Decay trace */
    float decay = expf(-dt_ms / (1000.0f / bridge->config.eligibility_decay));
    synapse->eligibility_trace *= decay;
}

/**
 * @brief Apply reward-modulated plasticity (unlocked version)
 */
static void apply_reward_modulated_plasticity_unlocked(
    emotion_plasticity_bridge_t* bridge)
{
    if (fabsf(bridge->pending_reward) < 0.001f) return;

    for (uint32_t i = 0; i < bridge->n_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->n_synapses > 256) {
            emotion_plasticity_bridge_heartbeat("emotion_plas_loop",
                             (float)(i + 1) / (float)bridge->n_synapses);
        }

        emotion_plasticity_synapse_t* syn = &bridge->synapses[i];

        if (syn->eligibility_trace > 0.001f) {
            float old_weight = syn->weight;
            float delta = syn->eligibility_trace *
                         bridge->pending_reward *
                         bridge->config.reward_modulation_gain;

            syn->weight = nimcp_myelin_clamp(syn->weight + delta,
                                 bridge->config.weight_min,
                                 bridge->config.weight_max);

            /* Update stats */
            if (delta > 0) {
                bridge->stats.ltp_events++;
            } else {
                bridge->stats.ltd_events++;
            }

            /* Notify callback */
            if (bridge->weight_callback && fabsf(syn->weight - old_weight) > 0.001f) {
                emotion_learn_event_t event = (bridge->pending_reward > 0) ?
                    EMOTION_LEARN_REWARD : EMOTION_LEARN_PUNISHMENT;

                bridge->weight_callback(
                    syn->synapse_id,
                    syn->associated_emotion,
                    old_weight,
                    syn->weight,
                    event,
                    bridge->callback_user_data
                );
            }
        }
    }

    /* Clear pending reward */
    if (bridge->pending_reward > 0) {
        bridge->stats.total_reward += bridge->pending_reward;
    } else {
        bridge->stats.total_punishment += fabsf(bridge->pending_reward);
    }
    bridge->pending_reward = 0.0f;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

emotion_plasticity_config_t emotion_plasticity_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    emotion_plasticity_bridge_heartbeat("emotion_plas_emotion_plasticity_c", 0.0f);


    emotion_plasticity_config_t config = {
        /* STDP parameters */
        .stdp_ltp_window_ms = 20.0f,
        .stdp_ltd_window_ms = 25.0f,
        .stdp_a_plus = NIMCP_STDP_A_PLUS,
        .stdp_a_minus = NIMCP_STDP_A_MINUS,
        .stdp_tau_plus = NIMCP_STDP_TAU_PLUS_MS,
        .stdp_tau_minus = 25.0f,

        /* Valence modulation */
        .enable_valence_modulation = true,
        .positive_valence_ltp_boost = 0.5f,
        .negative_valence_ltd_boost = 0.5f,

        /* Arousal modulation */
        .enable_arousal_modulation = true,
        .arousal_learning_gain = 1.0f,
        .low_arousal_decay_boost = 1.5f,

        /* BCM */
        .enable_bcm = true,
        .bcm_threshold_tau = 1000.0f,
        .bcm_activity_tau = 500.0f,

        /* Homeostatic */
        .enable_homeostatic = true,
        .target_response_rate = 0.5f,
        .homeostatic_tau_ms = 10000.0f,

        /* Eligibility */
        .enable_eligibility = true,
        .eligibility_decay = 0.1f,
        .reward_modulation_gain = 1.0f,

        /* Weight bounds */
        .weight_min = 0.0f,
        .weight_max = 1.0f,
        .initial_weight = NIMCP_SYNAPSE_WEIGHT_INIT,

        /* Extinction */
        .enable_extinction = true,
        .extinction_rate = 0.05f,
        .spontaneous_recovery_tau = 86400000.0f,  /* 24 hours in ms */

        /* Integration */
        .enable_bio_async = false,
        .enable_immune_modulation = false,
        .enable_sleep_consolidation = true
    };
    return config;
}

emotion_plasticity_bridge_t* emotion_plasticity_create(
    const emotion_plasticity_config_t* config)
{
    /* Phase 8: Heartbeat at operation start */
    emotion_plasticity_bridge_heartbeat("emotion_plas_emotion_plasticity_c", 0.0f);


    emotion_plasticity_bridge_t* bridge = nimcp_calloc(1, sizeof(emotion_plasticity_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = emotion_plasticity_config_default();
    }

    /* Initialize bridge base infrastructure (includes mutex) */
    if (bridge_base_init(&bridge->base, 0, "emotion_plasticity") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Failed to initialize bridge base in emotion_plasticity_create");
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }

    /* Initialize state */
    bridge->state = EMOTION_PLASTICITY_STATE_IDLE;
    bridge->n_synapses = 0;
    bridge->global_learning_rate = 1.0f;
    bridge->current_valence_mod = 0.0f;
    bridge->current_arousal_mod = 0.5f;
    bridge->pending_reward = 0.0f;
    bridge->bio_async_connected = false;

    /* Initialize per-emotion tracking */
    for (uint32_t i = 0; i < EMOTION_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && EMOTION_COUNT > 256) {
            emotion_plasticity_bridge_heartbeat("emotion_plas_loop",
                             (float)(i + 1) / (float)EMOTION_COUNT);
        }

        bridge->emotion_sensitivity[i] = 1.0f;
        bridge->emotion_extinction[i] = 0.0f;
        bridge->emotion_last_stimulus[i] = 0;
        bridge->emotion_last_response[i] = 0;
    }

    return bridge;
}

void emotion_plasticity_destroy(emotion_plasticity_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "emotion_plasticity");

    /* Phase 8: Heartbeat at operation start */
    emotion_plasticity_bridge_heartbeat("emotion_plas_emotion_plasticity_d", 0.0f);


    if (bridge->bio_async_connected) {
        emotion_plasticity_disconnect_bio_async(bridge);
    }

    /* Cleanup base bridge infrastructure */
    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge);
    bridge = NULL;
}

int emotion_plasticity_reset(emotion_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_plasticity_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_plasticity_bridge_heartbeat("emotion_plas_emotion_plasticity_r", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset synapses to initial state */
    for (uint32_t i = 0; i < bridge->n_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->n_synapses > 256) {
            emotion_plasticity_bridge_heartbeat("emotion_plas_loop",
                             (float)(i + 1) / (float)bridge->n_synapses);
        }

        bridge->synapses[i].weight = bridge->synapses[i].initial_weight;
        bridge->synapses[i].eligibility_trace = 0.0f;
        bridge->synapses[i].extinction_level = 0.0f;
        bridge->synapses[i].bcm_threshold = 0.5f;
        bridge->synapses[i].avg_activity = 0.0f;
    }

    /* Reset state */
    bridge->state = EMOTION_PLASTICITY_STATE_IDLE;
    bridge->global_learning_rate = 1.0f;
    bridge->current_valence_mod = 0.0f;
    bridge->current_arousal_mod = 0.5f;
    bridge->pending_reward = 0.0f;

    /* Reset per-emotion tracking */
    for (uint32_t i = 0; i < EMOTION_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && EMOTION_COUNT > 256) {
            emotion_plasticity_bridge_heartbeat("emotion_plas_loop",
                             (float)(i + 1) / (float)EMOTION_COUNT);
        }

        bridge->emotion_sensitivity[i] = 1.0f;
        bridge->emotion_extinction[i] = 0.0f;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Synapse Management
//=============================================================================

int emotion_plasticity_register_synapse(
    emotion_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    emotion_synapse_type_t type,
    emotion_category_t associated_emotion,
    float initial_weight)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_plasticity_register_synapse: bridge is NULL");
        return -1;
    }
    if (associated_emotion >= EMOTION_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "emotion_plasticity_register_synapse: capacity exceeded");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_plasticity_bridge_heartbeat("emotion_plas_emotion_plasticity_r", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Check if already registered */
    if (find_synapse(bridge, synapse_id) != NULL) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;  /* Already registered */
    }

    /* Check capacity */
    if (bridge->n_synapses >= EMOTION_PLASTICITY_MAX_SYNAPSES) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "emotion_plasticity_register_synapse: capacity exceeded");
        return -1;
    }

    /* Add synapse */
    emotion_plasticity_synapse_t* syn = &bridge->synapses[bridge->n_synapses];
    syn->synapse_id = synapse_id;
    syn->type = type;
    syn->associated_emotion = associated_emotion;
    syn->weight = nimcp_myelin_clamp(initial_weight, bridge->config.weight_min, bridge->config.weight_max);
    syn->initial_weight = syn->weight;
    syn->last_pre_spike_us = 0;
    syn->last_post_spike_us = 0;
    syn->eligibility_trace = 0.0f;
    syn->bcm_threshold = 0.5f;
    syn->avg_activity = 0.0f;
    syn->extinction_level = 0.0f;
    syn->original_strength = syn->weight;

    bridge->n_synapses++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int emotion_plasticity_unregister_synapse(
    emotion_plasticity_bridge_t* bridge,
    uint32_t synapse_id)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_plasticity_unregister_synapse: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_plasticity_bridge_heartbeat("emotion_plas_emotion_plasticity_u", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    for (uint32_t i = 0; i < bridge->n_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->n_synapses > 256) {
            emotion_plasticity_bridge_heartbeat("emotion_plas_loop",
                             (float)(i + 1) / (float)bridge->n_synapses);
        }

        if (bridge->synapses[i].synapse_id == synapse_id) {
            /* Shift remaining synapses */
            for (uint32_t j = i; j < bridge->n_synapses - 1; j++) {
                bridge->synapses[j] = bridge->synapses[j + 1];
            }
            bridge->n_synapses--;
            nimcp_mutex_unlock(bridge->base.mutex);
            return 0;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "emotion_plasticity_unregister_synapse: operation failed");
    return -1;  /* Not found */
}

int emotion_plasticity_get_synapse(
    emotion_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    emotion_plasticity_synapse_t* synapse)
{
    if (!bridge || !synapse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_plasticity_get_synapse: required parameter is NULL (bridge, synapse)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_plasticity_bridge_heartbeat("emotion_plas_emotion_plasticity_g", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    emotion_plasticity_synapse_t* syn = find_synapse(bridge, synapse_id);
    if (syn) {
        *synapse = *syn;
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "emotion_plasticity_get_synapse: validation failed");
    return -1;
}

//=============================================================================
// Event Recording
//=============================================================================

int emotion_plasticity_stimulus(
    emotion_plasticity_bridge_t* bridge,
    emotion_category_t emotion,
    float intensity,
    uint64_t timestamp_us)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_plasticity_stimulus: bridge is NULL");
        return -1;
    }
    if (emotion >= EMOTION_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "emotion_plasticity_stimulus: capacity exceeded");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_plasticity_bridge_heartbeat("emotion_plas_emotion_plasticity_s", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->state = EMOTION_PLASTICITY_STATE_OBSERVING;

    /* Record pre-synaptic spike timing for associated synapses */
    for (uint32_t i = 0; i < bridge->n_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->n_synapses > 256) {
            emotion_plasticity_bridge_heartbeat("emotion_plas_loop",
                             (float)(i + 1) / (float)bridge->n_synapses);
        }

        emotion_plasticity_synapse_t* syn = &bridge->synapses[i];
        if (syn->associated_emotion == emotion) {
            /* Calculate time since last post-spike for STDP */
            if (syn->last_post_spike_us > 0) {
                float dt_ms = (float)(timestamp_us - syn->last_post_spike_us) / 1000.0f;

                if (dt_ms < bridge->config.stdp_ltp_window_ms) {
                    /* Pre after post -> LTD */
                    float delta = compute_stdp_delta(bridge, -dt_ms);
                    float old_weight = syn->weight;
                    syn->weight = nimcp_myelin_clamp(syn->weight + delta,
                                         bridge->config.weight_min,
                                         bridge->config.weight_max);

                    if (delta < 0) bridge->stats.ltd_events++;

                    /* Notify callback */
                    if (bridge->weight_callback && fabsf(syn->weight - old_weight) > 0.001f) {
                        bridge->weight_callback(
                            syn->synapse_id, emotion, old_weight, syn->weight,
                            EMOTION_LEARN_CONDITIONING, bridge->callback_user_data
                        );
                    }
                }
            }

            /* Update pre-spike time */
            syn->last_pre_spike_us = timestamp_us;

            /* Set eligibility trace */
            if (bridge->config.enable_eligibility) {
                syn->eligibility_trace = intensity;
            }
        }
    }

    /* Update emotion tracking */
    bridge->emotion_last_stimulus[emotion] = timestamp_us;
    bridge->stats.total_observations++;
    bridge->stats.total_pre_spikes++;

    bridge->state = EMOTION_PLASTICITY_STATE_IDLE;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int emotion_plasticity_response(
    emotion_plasticity_bridge_t* bridge,
    emotion_category_t emotion,
    float response_strength,
    uint64_t timestamp_us)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_plasticity_response: bridge is NULL");
        return -1;
    }
    if (emotion >= EMOTION_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "emotion_plasticity_response: capacity exceeded");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_plasticity_bridge_heartbeat("emotion_plas_emotion_plasticity_r", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->state = EMOTION_PLASTICITY_STATE_RESPONDING;

    /* Record post-synaptic spike timing */
    for (uint32_t i = 0; i < bridge->n_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->n_synapses > 256) {
            emotion_plasticity_bridge_heartbeat("emotion_plas_loop",
                             (float)(i + 1) / (float)bridge->n_synapses);
        }

        emotion_plasticity_synapse_t* syn = &bridge->synapses[i];
        if (syn->associated_emotion == emotion) {
            /* Calculate time since last pre-spike for STDP */
            if (syn->last_pre_spike_us > 0) {
                float dt_ms = (float)(timestamp_us - syn->last_pre_spike_us) / 1000.0f;

                if (dt_ms < bridge->config.stdp_ltd_window_ms) {
                    /* Post after pre -> LTP */
                    float delta = compute_stdp_delta(bridge, dt_ms);
                    float old_weight = syn->weight;
                    syn->weight = nimcp_myelin_clamp(syn->weight + delta,
                                         bridge->config.weight_min,
                                         bridge->config.weight_max);

                    if (delta > 0) bridge->stats.ltp_events++;

                    /* Notify callback */
                    if (bridge->weight_callback && fabsf(syn->weight - old_weight) > 0.001f) {
                        bridge->weight_callback(
                            syn->synapse_id, emotion, old_weight, syn->weight,
                            EMOTION_LEARN_CONDITIONING, bridge->callback_user_data
                        );
                    }
                }
            }

            /* Update post-spike time */
            syn->last_post_spike_us = timestamp_us;

            /* Update BCM activity */
            if (bridge->config.enable_bcm) {
                float alpha = 1.0f / bridge->config.bcm_activity_tau;
                syn->avg_activity = (1.0f - alpha) * syn->avg_activity + alpha * response_strength;
            }
        }
    }

    /* Update emotion tracking */
    bridge->emotion_last_response[emotion] = timestamp_us;
    bridge->stats.total_responses++;
    bridge->stats.total_post_spikes++;

    bridge->state = EMOTION_PLASTICITY_STATE_IDLE;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int emotion_plasticity_reward(
    emotion_plasticity_bridge_t* bridge,
    float reward,
    uint64_t timestamp_us)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_plasticity_reward: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_plasticity_bridge_heartbeat("emotion_plas_emotion_plasticity_r", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->pending_reward = reward;
    bridge->last_reward_time_us = timestamp_us;

    /* Apply reward-modulated plasticity */
    apply_reward_modulated_plasticity_unlocked(bridge);

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int emotion_plasticity_extinction_trial(
    emotion_plasticity_bridge_t* bridge,
    emotion_category_t emotion,
    uint64_t timestamp_us)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_plasticity_extinction_trial: bridge is NULL");
        return -1;
    }
    if (emotion >= EMOTION_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "emotion_plasticity_extinction_trial: capacity exceeded");
        return -1;
    }
    if (!bridge->config.enable_extinction) return 0;

    /* Phase 8: Heartbeat at operation start */
    emotion_plasticity_bridge_heartbeat("emotion_plas_emotion_plasticity_e", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Apply extinction to all synapses associated with this emotion */
    for (uint32_t i = 0; i < bridge->n_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->n_synapses > 256) {
            emotion_plasticity_bridge_heartbeat("emotion_plas_loop",
                             (float)(i + 1) / (float)bridge->n_synapses);
        }

        emotion_plasticity_synapse_t* syn = &bridge->synapses[i];
        if (syn->associated_emotion == emotion) {
            float old_weight = syn->weight;

            /* Increase extinction level */
            syn->extinction_level = nimcp_myelin_clamp(
                syn->extinction_level + bridge->config.extinction_rate,
                0.0f, 1.0f
            );

            /* Reduce weight proportionally */
            syn->weight = syn->original_strength * (1.0f - syn->extinction_level);
            syn->weight = nimcp_myelin_clamp(syn->weight,
                                 bridge->config.weight_min,
                                 bridge->config.weight_max);

            /* Notify callback */
            if (bridge->weight_callback && fabsf(syn->weight - old_weight) > 0.001f) {
                bridge->weight_callback(
                    syn->synapse_id, emotion, old_weight, syn->weight,
                    EMOTION_LEARN_EXTINCTION, bridge->callback_user_data
                );
            }
        }
    }

    /* Update global extinction level for emotion */
    bridge->emotion_extinction[emotion] = nimcp_myelin_clamp(
        bridge->emotion_extinction[emotion] + bridge->config.extinction_rate,
        0.0f, 1.0f
    );

    bridge->stats.extinction_events++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Update Functions
//=============================================================================

int emotion_plasticity_update(
    emotion_plasticity_bridge_t* bridge,
    float dt_ms)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_plasticity_update: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_plasticity_bridge_heartbeat("emotion_plas_emotion_plasticity_u", 0.0f);


    /* Safety gates: ethics + LGSS pre-check */
    BRIDGE_ETHICS_GATE(bridge, "emotion_plasticity_update");
    BRIDGE_LGSS_GATE(bridge, "emotion_plasticity_update");

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->state = EMOTION_PLASTICITY_STATE_UPDATING;

    /* Update eligibility traces */
    for (uint32_t i = 0; i < bridge->n_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->n_synapses > 256) {
            emotion_plasticity_bridge_heartbeat("emotion_plas_loop",
                             (float)(i + 1) / (float)bridge->n_synapses);
        }

        update_eligibility_trace_unlocked(bridge, &bridge->synapses[i], dt_ms);
    }

    /* Update BCM thresholds */
    if (bridge->config.enable_bcm) {
        float alpha = dt_ms / bridge->config.bcm_threshold_tau;
        for (uint32_t i = 0; i < bridge->n_synapses; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->n_synapses > 256) {
                emotion_plasticity_bridge_heartbeat("emotion_plas_loop",
                                 (float)(i + 1) / (float)bridge->n_synapses);
            }

            emotion_plasticity_synapse_t* syn = &bridge->synapses[i];
            syn->bcm_threshold = (1.0f - alpha) * syn->bcm_threshold +
                                alpha * syn->avg_activity;
        }
    }

    /* Homeostatic scaling */
    if (bridge->config.enable_homeostatic) {
        float alpha = dt_ms / bridge->config.homeostatic_tau_ms;
        for (uint32_t i = 0; i < bridge->n_synapses; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->n_synapses > 256) {
                emotion_plasticity_bridge_heartbeat("emotion_plas_loop",
                                 (float)(i + 1) / (float)bridge->n_synapses);
            }

            emotion_plasticity_synapse_t* syn = &bridge->synapses[i];
            float error = bridge->config.target_response_rate - syn->avg_activity;
            syn->weight = nimcp_myelin_clamp(syn->weight + alpha * error * 0.01f,
                                 bridge->config.weight_min,
                                 bridge->config.weight_max);
        }
    }

    /* Update emotion sensitivities based on average synapse weights */
    for (uint32_t e = 0; e < EMOTION_COUNT; e++) {
        /* Phase 8: Loop progress heartbeat */
        if ((e & 0xFF) == 0 && EMOTION_COUNT > 256) {
            emotion_plasticity_bridge_heartbeat("emotion_plas_loop",
                             (float)(e + 1) / (float)EMOTION_COUNT);
        }

        float total_weight = 0.0f;
        uint32_t count = 0;
        for (uint32_t i = 0; i < bridge->n_synapses; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->n_synapses > 256) {
                emotion_plasticity_bridge_heartbeat("emotion_plas_loop",
                                 (float)(i + 1) / (float)bridge->n_synapses);
            }

            if (bridge->synapses[i].associated_emotion == (emotion_category_t)e) {
                total_weight += bridge->synapses[i].weight;
                count++;
            }
        }
        if (count > 0) {
            bridge->emotion_sensitivity[e] = total_weight / count;
        }
    }

    bridge->state = EMOTION_PLASTICITY_STATE_IDLE;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

int emotion_plasticity_consolidate(emotion_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_plasticity_consolidate: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_plasticity_bridge_heartbeat("emotion_plas_emotion_plasticity_c", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->state = EMOTION_PLASTICITY_STATE_CONSOLIDATING;

    /* Consolidation strengthens strong weights, weakens weak ones */
    for (uint32_t i = 0; i < bridge->n_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->n_synapses > 256) {
            emotion_plasticity_bridge_heartbeat("emotion_plas_loop",
                             (float)(i + 1) / (float)bridge->n_synapses);
        }

        emotion_plasticity_synapse_t* syn = &bridge->synapses[i];

        float mid = (bridge->config.weight_min + bridge->config.weight_max) / 2.0f;
        if (syn->weight > mid) {
            /* Strengthen strong synapses */
            syn->weight = nimcp_myelin_clamp(syn->weight * 1.05f,
                                 bridge->config.weight_min,
                                 bridge->config.weight_max);
        } else {
            /* Weaken weak synapses */
            syn->weight = nimcp_myelin_clamp(syn->weight * 0.95f,
                                 bridge->config.weight_min,
                                 bridge->config.weight_max);
        }

        /* Update original strength for future extinction calculations */
        syn->original_strength = syn->weight;
    }

    bridge->state = EMOTION_PLASTICITY_STATE_IDLE;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Query Functions
//=============================================================================

int emotion_plasticity_get_response_modulation(
    emotion_plasticity_bridge_t* bridge,
    emotion_category_t emotion,
    float* modulation)
{
    if (!bridge || !modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_plasticity_get_response_modulation: required parameter is NULL (bridge, modulation)");
        return -1;
    }
    BRIDGE_BBB_VALIDATE(bridge, modulation, sizeof(*modulation));
    if (emotion >= EMOTION_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "emotion_plasticity_get_response_modulation: capacity exceeded");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_plasticity_bridge_heartbeat("emotion_plas_emotion_plasticity_g", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Modulation based on learned sensitivity and extinction */
    *modulation = bridge->emotion_sensitivity[emotion] *
                 (1.0f - bridge->emotion_extinction[emotion]);

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float emotion_plasticity_get_extinction_level(
    emotion_plasticity_bridge_t* bridge,
    emotion_category_t emotion)
{
    if (!bridge) return 0.0f;
    if (emotion >= EMOTION_COUNT) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    emotion_plasticity_bridge_heartbeat("emotion_plas_emotion_plasticity_g", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float level = bridge->emotion_extinction[emotion];
    nimcp_mutex_unlock(bridge->base.mutex);

    return level;
}

float emotion_plasticity_get_sensitivity(
    emotion_plasticity_bridge_t* bridge,
    emotion_category_t emotion)
{
    if (!bridge) return 1.0f;
    if (emotion >= EMOTION_COUNT) return 1.0f;

    /* Phase 8: Heartbeat at operation start */
    emotion_plasticity_bridge_heartbeat("emotion_plas_emotion_plasticity_g", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float sens = bridge->emotion_sensitivity[emotion];
    nimcp_mutex_unlock(bridge->base.mutex);

    return sens;
}

//=============================================================================
// State and Statistics
//=============================================================================

int emotion_plasticity_get_state(
    const emotion_plasticity_bridge_t* bridge,
    emotion_plasticity_bridge_state_t* state)
{
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_plasticity_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_plasticity_bridge_heartbeat("emotion_plas_emotion_plasticity_g", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, state, sizeof(*state));

    nimcp_mutex_lock(((emotion_plasticity_bridge_t*)bridge)->base.mutex);

    state->state = bridge->state;
    state->registered_synapses = bridge->n_synapses;
    state->global_learning_rate = bridge->global_learning_rate;
    state->current_valence_mod = bridge->current_valence_mod;
    state->current_arousal_mod = bridge->current_arousal_mod;
    state->bio_async_connected = bridge->bio_async_connected;

    nimcp_mutex_unlock(((emotion_plasticity_bridge_t*)bridge)->base.mutex);

    return 0;
}

int emotion_plasticity_get_stats(
    const emotion_plasticity_bridge_t* bridge,
    emotion_plasticity_stats_t* stats)
{
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_plasticity_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_plasticity_bridge_heartbeat("emotion_plas_emotion_plasticity_g", 0.0f);


    nimcp_mutex_lock(((emotion_plasticity_bridge_t*)bridge)->base.mutex);

    *stats = bridge->stats;

    /* Calculate average weight change */
    if (bridge->stats.ltp_events + bridge->stats.ltd_events > 0) {
        /* Approximate from event counts */
        stats->avg_weight_change =
            (bridge->stats.ltp_events * 0.01f - bridge->stats.ltd_events * 0.012f) /
            (bridge->stats.ltp_events + bridge->stats.ltd_events);
    }

    nimcp_mutex_unlock(((emotion_plasticity_bridge_t*)bridge)->base.mutex);

    return 0;
}

void emotion_plasticity_reset_stats(emotion_plasticity_bridge_t* bridge) {
    if (!bridge) return;

    /* Phase 8: Heartbeat at operation start */
    emotion_plasticity_bridge_heartbeat("emotion_plas_emotion_plasticity_r", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->base.mutex);
}

//=============================================================================
// Callbacks
//=============================================================================

int emotion_plasticity_set_weight_callback(
    emotion_plasticity_bridge_t* bridge,
    emotion_weight_change_cb callback,
    void* user_data)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_plasticity_set_weight_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_plasticity_bridge_heartbeat("emotion_plas_emotion_plasticity_s", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, user_data, sizeof(*user_data));

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->weight_callback = callback;
    bridge->callback_user_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Modulation Functions
//=============================================================================

int emotion_plasticity_set_valence_modulation(
    emotion_plasticity_bridge_t* bridge,
    float valence)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_plasticity_set_valence_modulation: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_plasticity_bridge_heartbeat("emotion_plas_emotion_plasticity_s", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->current_valence_mod = nimcp_myelin_clamp(valence, -1.0f, 1.0f);
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int emotion_plasticity_set_arousal_modulation(
    emotion_plasticity_bridge_t* bridge,
    float arousal)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_plasticity_set_arousal_modulation: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_plasticity_bridge_heartbeat("emotion_plas_emotion_plasticity_s", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->current_arousal_mod = nimcp_myelin_clamp(arousal, 0.0f, 1.0f);
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int emotion_plasticity_connect_bio_async(emotion_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL in emotion_plasticity_connect_bio_async");
        return -1;
    }
    if (!bridge->config.enable_bio_async) return 0;

    /* Phase 8: Heartbeat at operation start */
    emotion_plasticity_bridge_heartbeat("emotion_plas_emotion_plasticity_c", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bio_async_connected = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int emotion_plasticity_disconnect_bio_async(emotion_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL in emotion_plasticity_disconnect_bio_async");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_plasticity_bridge_heartbeat("emotion_plas_emotion_plasticity_d", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bio_async_connected = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool emotion_plasticity_is_bio_async_connected(const emotion_plasticity_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    emotion_plasticity_bridge_heartbeat("emotion_plas_emotion_plasticity_i", 0.0f);


    return bridge->bio_async_connected;
}

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void emotion_plasticity_bridge_set_instance_health_agent(emotion_plasticity_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (bridge) {
        bridge->health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training stubs
 * ============================================================================ */
int emotion_plasticity_bridge_training_begin(emotion_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotion_plasticity_bridge_training_begin: NULL argument");
        return -1;
    }
    emotion_plasticity_bridge_heartbeat_instance(bridge->health_agent, "emotion_plas_training_begin", 0.0f);
    return 0;
}

int emotion_plasticity_bridge_training_end(emotion_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotion_plasticity_bridge_training_end: NULL argument");
        return -1;
    }
    emotion_plasticity_bridge_heartbeat_instance(bridge->health_agent, "emotion_plas_training_end", 1.0f);
    return 0;
}

int emotion_plasticity_bridge_training_step(emotion_plasticity_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotion_plasticity_bridge_training_step: NULL argument");
        return -1;
    }
    emotion_plasticity_bridge_heartbeat_instance(bridge->health_agent, "emotion_plas_training_step", progress);
    return 0;
}

/* ============================================================================
 * Security Integration (BBB)
 * ============================================================================ */
BRIDGE_DEFINE_SECURITY_SETTERS(emotion_plasticity_bridge)
