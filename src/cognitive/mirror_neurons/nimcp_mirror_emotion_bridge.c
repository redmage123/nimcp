/**
 * @file nimcp_mirror_emotion_bridge.c
 * @brief Mirror Neuron - Emotion Recognition Bidirectional Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-31
 *
 * SIMD-optimized implementation of mirror-emotion bridge for emotional
 * contagion and empathy processing.
 */

#include "cognitive/mirror_neurons/nimcp_mirror_emotion_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/tensor/nimcp_tensor_simd.h"
#include "utils/containers/nimcp_vector.h"
#include "utils/thread/nimcp_thread.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"

#include <math.h>
#include <string.h>
#include <stdio.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(mirror_emotion_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_mirror_emotion_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_mirror_emotion_bridge_mesh_registry = NULL;

nimcp_error_t mirror_emotion_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_mirror_emotion_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "mirror_emotion_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "mirror_emotion_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_mirror_emotion_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_mirror_emotion_bridge_mesh_registry = registry;
    return err;
}

void mirror_emotion_bridge_mesh_unregister(void) {
    if (g_mirror_emotion_bridge_mesh_registry && g_mirror_emotion_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_mirror_emotion_bridge_mesh_registry, g_mirror_emotion_bridge_mesh_id);
        g_mirror_emotion_bridge_mesh_id = 0;
        g_mirror_emotion_bridge_mesh_registry = NULL;
    }
}


#define LOG_MODULE "MIRROR_EMOTION_BRIDGE"

static inline void mirror_emotion_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_mirror_emotion_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_mirror_emotion_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_mirror_emotion_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


//=============================================================================
// Internal Structures
//=============================================================================

struct mirror_emotion_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */

    mirror_emotion_config_t config;
    mirror_emotion_state_t state;

    /* Agent tracking */
    mirror_emotion_agent_state_t agents[MIRROR_EMOTION_MAX_AGENTS];
    uint32_t active_agent_count;

    /* Current emotional context (for modulation) */
    uint32_t current_emotion;
    float current_intensity;
    float current_valence;
    float current_arousal;
    bool crisis_mode;

    /* Mirror sensitivity modulation */
    float sensitivity_multiplier;

    /* Statistics */
    mirror_emotion_stats_t stats;

    /* Bio-async registration */
    bool bio_async_registered;
    uint32_t handler_id;

    /* Instance-level health agent (B22) */
    nimcp_health_agent_t* health_agent;
};

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(mirror_emotion_bridge)

/* Cosine similarity uses nimcp_vector_cosine_similarity from utils/containers/nimcp_vector.h */

//=============================================================================
// Emotion Templates (Basic Emotions AU Patterns)
//=============================================================================

/* Facial Action Unit templates for basic emotions (Ekman) */
static const float EMOTION_AU_TEMPLATES[MIRROR_EMOTION_BASIC_COUNT][MIRROR_EMOTION_ACTION_UNITS] = {
    /* Happiness: AU6 (cheek raiser) + AU12 (lip corner puller) */
    {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.8f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.9f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},

    /* Sadness: AU1 (inner brow raiser) + AU4 (brow lowerer) + AU15 (lip corner depressor) */
    {0.8f, 0.0f, 0.0f, 0.7f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.8f, 0.0f, 0.0f},

    /* Anger: AU4 (brow lowerer) + AU5 (upper lid raiser) + AU7 (lid tightener) + AU23 (lip tightener) */
    {0.0f, 0.0f, 0.0f, 0.9f, 0.7f, 0.0f, 0.8f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.7f},

    /* Fear: AU1 + AU2 (outer brow raiser) + AU4 + AU5 + AU20 (lip stretcher) */
    {0.8f, 0.7f, 0.0f, 0.6f, 0.8f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},

    /* Disgust: AU9 (nose wrinkler) + AU15 + AU16 (lower lip depressor) */
    {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.8f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.7f, 0.6f, 0.0f},

    /* Surprise: AU1 + AU2 + AU5 + AU26 (jaw drop) */
    {0.8f, 0.8f, 0.0f, 0.0f, 0.7f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}
};

/* Emotion valence lookup */
static const float EMOTION_VALENCE[MIRROR_EMOTION_TOTAL_COUNT] = {
    0.8f,   /* Happiness */
    -0.6f,  /* Sadness */
    -0.7f,  /* Anger */
    -0.8f,  /* Fear */
    -0.6f,  /* Disgust */
    0.1f,   /* Surprise */
    0.5f,   /* Interest */
    -0.2f,  /* Confusion */
    -0.5f,  /* Frustration */
    -0.3f,  /* Boredom */
    0.6f,   /* Pride */
    -0.7f,  /* Shame */
    -0.9f,  /* Rage */
    -0.9f,  /* Hate */
    -0.95f, /* Despair */
    -0.85f, /* Panic */
    0.4f,   /* Calm */
    -0.6f,  /* Contempt */
    0.0f    /* Neutral */
};

/* Emotion arousal lookup */
static const float EMOTION_AROUSAL[MIRROR_EMOTION_TOTAL_COUNT] = {
    0.6f,   /* Happiness */
    0.3f,   /* Sadness */
    0.8f,   /* Anger */
    0.9f,   /* Fear */
    0.5f,   /* Disgust */
    0.8f,   /* Surprise */
    0.6f,   /* Interest */
    0.4f,   /* Confusion */
    0.7f,   /* Frustration */
    0.2f,   /* Boredom */
    0.5f,   /* Pride */
    0.4f,   /* Shame */
    0.95f,  /* Rage */
    0.7f,   /* Hate */
    0.3f,   /* Despair */
    0.95f,  /* Panic */
    0.2f,   /* Calm */
    0.4f,   /* Contempt */
    0.1f    /* Neutral */
};

//=============================================================================
// Internal Functions
//=============================================================================

/**
 * @brief Find best matching emotion from AU pattern
 */
static uint32_t match_emotion_from_aus(const float* aus, float* confidence) {
    float best_score = -1.0f;
    uint32_t best_emotion = MIRROR_EMOTION_TOTAL_COUNT - 1; /* Neutral */

    for (uint32_t i = 0; i < MIRROR_EMOTION_BASIC_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && MIRROR_EMOTION_BASIC_COUNT > 256) {
            mirror_emotion_bridge_heartbeat("mirror_emoti_loop",
                             (float)(i + 1) / (float)MIRROR_EMOTION_BASIC_COUNT);
        }

        float sim = nimcp_vector_cosine_similarity(aus, EMOTION_AU_TEMPLATES[i],
                                                   MIRROR_EMOTION_ACTION_UNITS);
        if (sim > best_score) {
            best_score = sim;
            best_emotion = i;
        }
    }

    if (confidence) {
        *confidence = (best_score > 0.0f) ? best_score : 0.0f;
    }

    /* Require minimum similarity for non-neutral */
    if (best_score < 0.3f) {
        return MIRROR_EMOTION_TOTAL_COUNT - 1; /* Neutral */
    }

    return best_emotion;
}

/**
 * @brief Get or create agent slot
 */
static mirror_emotion_agent_state_t* get_or_create_agent(
    mirror_emotion_bridge_t* bridge,
    uint32_t agent_id
) {
    /* Find existing */
    for (uint32_t i = 0; i < MIRROR_EMOTION_MAX_AGENTS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && MIRROR_EMOTION_MAX_AGENTS > 256) {
            mirror_emotion_bridge_heartbeat("mirror_emoti_loop",
                             (float)(i + 1) / (float)MIRROR_EMOTION_MAX_AGENTS);
        }

        if (bridge->agents[i].active && bridge->agents[i].agent_id == agent_id) {
            return &bridge->agents[i];
        }
    }

    /* Find empty slot */
    for (uint32_t i = 0; i < MIRROR_EMOTION_MAX_AGENTS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && MIRROR_EMOTION_MAX_AGENTS > 256) {
            mirror_emotion_bridge_heartbeat("mirror_emoti_loop",
                             (float)(i + 1) / (float)MIRROR_EMOTION_MAX_AGENTS);
        }

        if (!bridge->agents[i].active) {
            memset(&bridge->agents[i], 0, sizeof(mirror_emotion_agent_state_t));
            bridge->agents[i].agent_id = agent_id;
            bridge->agents[i].active = true;
            bridge->agents[i].familiarity = 0.1f;  /* Start low */
            bridge->agents[i].empathy_tendency = 0.5f;
            bridge->agents[i].contagion_susceptibility = 0.5f;
            bridge->agents[i].first_observation_us = nimcp_time_now_us();
            bridge->active_agent_count++;
            return &bridge->agents[i];
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "get_or_create_agent: operation failed");
    return NULL; /* No slots available */
}

/**
 * @brief Update agent emotional history
 */
static void update_agent_history(
    mirror_emotion_agent_state_t* agent,
    uint32_t emotion,
    float intensity,
    uint64_t timestamp_us
) {
    /* Add to history ring buffer */
    agent->emotion_history[agent->history_index] = emotion;
    agent->intensity_history[agent->history_index] = intensity;
    agent->timestamp_history[agent->history_index] = timestamp_us;

    agent->history_index = (agent->history_index + 1) % MIRROR_EMOTION_HISTORY_SIZE;
    if (agent->history_count < MIRROR_EMOTION_HISTORY_SIZE) {
        agent->history_count++;
    }

    /* Update current state */
    agent->current_emotion = emotion;
    agent->current_intensity = intensity;
    agent->current_valence = (emotion < MIRROR_EMOTION_TOTAL_COUNT) ?
                              EMOTION_VALENCE[emotion] : 0.0f;
    agent->current_arousal = (emotion < MIRROR_EMOTION_TOTAL_COUNT) ?
                              EMOTION_AROUSAL[emotion] : 0.0f;
    agent->last_observation_us = timestamp_us;
    agent->total_observations++;

    /* Update running statistics */
    float alpha = 0.1f;  /* EMA smoothing */
    agent->avg_valence = agent->avg_valence * (1.0f - alpha) +
                         agent->current_valence * alpha;
    agent->avg_arousal = agent->avg_arousal * (1.0f - alpha) +
                         agent->current_arousal * alpha;

    /* Update familiarity based on interaction */
    float fam_increase = 0.01f;
    agent->familiarity = fminf(1.0f, agent->familiarity + fam_increase);
}

/**
 * @brief Compute contagion strength with social factors
 */
static float compute_contagion_internal(
    mirror_emotion_bridge_t* bridge,
    mirror_emotion_agent_state_t* agent,
    float intensity,
    float resonance_strength
) {
    if (!agent) return 0.0f;

    /* Base contagion from resonance and intensity */
    float base_contagion = resonance_strength * intensity;

    /* Modulate by familiarity (familiar = stronger contagion) */
    float familiarity_factor = 0.5f + 0.5f * agent->familiarity;

    /* Modulate by individual susceptibility */
    float susceptibility = agent->contagion_susceptibility;

    /* Apply configuration gain */
    float empathy_gain = bridge->config.empathy_gain;

    /* Compute final contagion */
    float contagion = base_contagion * familiarity_factor * susceptibility * empathy_gain;

    /* Apply threshold */
    if (contagion < bridge->config.contagion_threshold) {
        return 0.0f;
    }

    /* Regulate in crisis mode */
    if (bridge->crisis_mode) {
        contagion *= 0.3f;  /* Significantly reduce during crisis */
    }

    return fminf(1.0f, contagion);
}

/**
 * @brief Determine contagion type based on emotion and context
 */
static mirror_contagion_type_t determine_contagion_type(
    mirror_emotion_bridge_t* bridge,
    uint32_t emotion,
    float intensity,
    float contagion_strength
) {
    if (contagion_strength < bridge->config.contagion_threshold) {
        return MIRROR_CONTAGION_NONE;
    }

    /* Crisis emotions get regulated */
    if (emotion == 12 || emotion == 15) { /* Rage or Panic */
        if (bridge->config.enable_crisis_suppression) {
            return MIRROR_CONTAGION_REGULATED;
        }
    }

    /* High intensity negative emotions may trigger sympathy instead of empathy */
    float valence = (emotion < MIRROR_EMOTION_TOTAL_COUNT) ?
                    EMOTION_VALENCE[emotion] : 0.0f;
    if (valence < -0.7f && intensity > 0.8f) {
        return MIRROR_CONTAGION_SYMPATHIC;
    }

    /* Standard empathic response */
    return MIRROR_CONTAGION_EMPATHIC;
}

//=============================================================================
// Public API Implementation
//=============================================================================

mirror_emotion_config_t mirror_emotion_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    mirror_emotion_bridge_heartbeat("mirror_emoti_mirror_emotion_confi", 0.0f);


    mirror_emotion_config_t config = {
        .resonance_threshold = 0.3f,
        .contagion_threshold = 0.4f,
        .empathy_gain = 1.0f,
        .mimicry_suppression = 0.3f,

        .facial_weight = 0.4f,
        .vocal_weight = 0.3f,
        .bodily_weight = 0.2f,
        .gaze_weight = 0.1f,

        .crisis_threshold = 0.8f,
        .regulation_threshold = 0.7f,
        .enable_contagion_regulation = true,
        .enable_crisis_suppression = true,

        .enable_simd = true,
        .simd_batch_size = 16,

        .bidirectional_enabled = true,
        .bio_async_enabled = true,
        .immune_integration = true
    };
    return config;
}

mirror_emotion_bridge_t* mirror_emotion_create(const mirror_emotion_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    mirror_emotion_bridge_heartbeat("mirror_emoti_mirror_emotion_creat", 0.0f);


    mirror_emotion_bridge_t* bridge = nimcp_calloc(1, sizeof(mirror_emotion_bridge_t));
    if (!bridge) {
        nimcp_log(LOG_LEVEL_ERROR, "Mirror-Emotion: Failed to allocate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mirror_emotion_create: bridge is NULL");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = mirror_emotion_config_default();
    }

    /* Initialize state */
    bridge->state = MIRROR_EMOTION_STATE_IDLE;
    bridge->active_agent_count = 0;
    bridge->sensitivity_multiplier = 1.0f;
    bridge->crisis_mode = false;

    /* Initialize bridge base infrastructure (includes mutex) */
    if (bridge_base_init(&bridge->base, 0, "mirror_emotion") != 0) {
        nimcp_log(LOG_LEVEL_ERROR, "Mirror-Emotion: Failed to initialize bridge base");
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "mirror_emotion_create: validation failed");
        return NULL;
    }

    /* Register with bio-async if enabled */
    if (bridge->config.bio_async_enabled) {
        mirror_emotion_register_bio_async(bridge);
    }

    nimcp_log(LOG_LEVEL_INFO, "Mirror-Emotion: Created bridge (empathy_gain=%.2f, simd=%s)",
              bridge->config.empathy_gain,
              bridge->config.enable_simd ? "enabled" : "disabled");

    NIMCP_LOGGING_INFO("Created %s bridge", "mirror_emotion");
    return bridge;
}

void mirror_emotion_destroy(mirror_emotion_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "mirror_emotion");

    /* Unregister from bio-async */
    /* Phase 8: Heartbeat at operation start */
    mirror_emotion_bridge_heartbeat("mirror_emoti_mirror_emotion_destr", 0.0f);


    if (bridge->bio_async_registered) {
        mirror_emotion_unregister_bio_async(bridge);
    }

    /* Cleanup base bridge infrastructure */
    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge);
    bridge = NULL;
    nimcp_log(LOG_LEVEL_DEBUG, "Mirror-Emotion: Destroyed bridge");
}

bool mirror_emotion_process_observation(
    mirror_emotion_bridge_t* bridge,
    const mirror_emotion_observation_t* observation,
    mirror_emotion_resonance_t* result
) {
    if (!bridge || !observation || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_emotion_process_observation: required parameter is NULL");
        return false;
    }
    BRIDGE_BBB_VALIDATE(bridge, observation, sizeof(*observation));

    /* Phase 8: Heartbeat at operation start */
    mirror_emotion_bridge_heartbeat("mirror_emoti_mirror_emotion_proce", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    memset(result, 0, sizeof(mirror_emotion_resonance_t));
    result->timestamp_us = nimcp_time_now_us();

    /* Get or create agent state */
    mirror_emotion_agent_state_t* agent = get_or_create_agent(bridge,
                                                              observation->agent_id);

    /* Match emotion from facial AUs if available */
    float au_confidence = 0.0f;
    uint32_t au_emotion = MIRROR_EMOTION_TOTAL_COUNT - 1;

    if (observation->active_au_count > 0) {
        au_emotion = match_emotion_from_aus(observation->action_units, &au_confidence);
    }

    /* Match emotion from feature vector if available */
    float feature_confidence = 0.0f;
    uint32_t feature_emotion = MIRROR_EMOTION_TOTAL_COUNT - 1;

    if (observation->feature_dim > 0) {
        /* Use resonance strength as proxy for feature-based confidence */
        feature_confidence = observation->resonance_strength;
        /* Feature emotion would come from classifier - using AU result for now */
        feature_emotion = au_emotion;
    }

    /* Fuse results */
    float combined_confidence = 0.0f;
    uint32_t final_emotion = 0;

    if (observation->modality == MIRROR_EMOTION_MODALITY_FACIAL) {
        combined_confidence = au_confidence;
        final_emotion = au_emotion;
    } else if (observation->modality == MIRROR_EMOTION_MODALITY_MULTIMODAL) {
        combined_confidence = 0.6f * au_confidence + 0.4f * feature_confidence;
        final_emotion = (au_confidence > feature_confidence) ? au_emotion : feature_emotion;
    } else {
        combined_confidence = observation->observation_confidence;
        final_emotion = au_emotion;
    }

    /* Apply resonance threshold */
    if (observation->resonance_strength < bridge->config.resonance_threshold) {
        nimcp_mutex_unlock(bridge->base.mutex);
        bridge->stats.total_observations++;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_emotion_process_observation: validation failed");
        return false;  /* Insufficient resonance */
    }

    /* Fill result */
    result->emotion_category = final_emotion;
    result->confidence = combined_confidence;

    /* Get dimensional values */
    if (final_emotion < MIRROR_EMOTION_TOTAL_COUNT) {
        result->valence = EMOTION_VALENCE[final_emotion];
        result->arousal = EMOTION_AROUSAL[final_emotion];
    }
    result->dominance = 0.0f; /* Would need separate model */

    /* Compute intensity from resonance and AU strength */
    result->intensity = observation->resonance_strength * observation->observation_confidence;

    /* Compute contagion */
    float contagion = compute_contagion_internal(bridge, agent,
                                                  result->intensity,
                                                  observation->resonance_strength);
    result->contagion_strength = contagion;
    result->contagion_type = determine_contagion_type(bridge, final_emotion,
                                                       result->intensity, contagion);

    /* Compute empathy level */
    result->empathy_level = contagion * (agent ? agent->empathy_tendency : 0.5f);

    /* Mirror-derived metrics */
    result->embodied_confidence = combined_confidence * observation->resonance_strength;
    result->facial_mimicry_strength = observation->motor_priming;
    result->motor_resonance_contribution = observation->resonance_strength;

    /* Safety assessment */
    result->distress_level = (result->valence < -0.5f && result->arousal > 0.6f) ?
                              result->intensity : 0.0f;
    result->crisis_detected = (final_emotion == 12 || final_emotion == 15) && /* Rage/Panic */
                               result->intensity > bridge->config.crisis_threshold;
    result->requires_regulation = result->distress_level > bridge->config.regulation_threshold ||
                                   result->crisis_detected;

    /* Update agent history */
    if (agent) {
        update_agent_history(agent, final_emotion, result->intensity, result->timestamp_us);
    }

    /* Update statistics */
    bridge->stats.total_observations++;
    bridge->stats.resonance_events++;
    if (contagion > 0.0f) {
        bridge->stats.contagion_events++;
        bridge->stats.avg_contagion_strength =
            bridge->stats.avg_contagion_strength * 0.95f + contagion * 0.05f;
    }
    if (result->requires_regulation) {
        bridge->stats.regulation_events++;
    }
    if (result->crisis_detected) {
        bridge->stats.crisis_detections++;
    }
    bridge->stats.avg_resonance_strength =
        bridge->stats.avg_resonance_strength * 0.95f +
        observation->resonance_strength * 0.05f;
    bridge->stats.avg_empathy_level =
        bridge->stats.avg_empathy_level * 0.95f + result->empathy_level * 0.05f;

    bridge->state = MIRROR_EMOTION_STATE_RESONATING;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Bio-async message sending would happen here when integrated
     * into a full bio-async context with proper router registration */
    (void)bridge->bio_async_registered; /* Suppress unused warning */

    return true;
}

uint32_t mirror_emotion_process_batch(
    mirror_emotion_bridge_t* bridge,
    const mirror_emotion_observation_t* observations,
    mirror_emotion_resonance_t* results,
    uint32_t count
) {
    if (!bridge || !observations || !results || count == 0) {
        if (!bridge || !observations || !results) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_emotion_process_batch: required parameter is NULL");
            return 0;
        }
        return 0;
    }
    BRIDGE_BBB_VALIDATE(bridge, observations, count * sizeof(*observations));

    /* Phase 8: Heartbeat at operation start */
    mirror_emotion_bridge_heartbeat("mirror_emoti_mirror_emotion_proce", 0.0f);


    uint32_t processed = 0;
    for (uint32_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            mirror_emotion_bridge_heartbeat("mirror_emoti_loop",
                             (float)(i + 1) / (float)count);
        }

        if (mirror_emotion_process_observation(bridge, &observations[i], &results[i])) {
            processed++;
        }
    }

    if (bridge->config.enable_simd && count >= MIRROR_EMOTION_SIMD_THRESHOLD) {
        bridge->stats.simd_operations++;
    } else {
        bridge->stats.scalar_fallbacks++;
    }

    return processed;
}

float mirror_emotion_compute_contagion(
    mirror_emotion_bridge_t* bridge,
    uint32_t agent_id,
    uint32_t observed_emotion,
    float intensity
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_emotion_compute_contagion: bridge is NULL");
        return 0.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_emotion_bridge_heartbeat("mirror_emoti_mirror_emotion_compu", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    mirror_emotion_agent_state_t* agent = get_or_create_agent(bridge, agent_id);
    float resonance = intensity;  /* Assume resonance equals intensity if not given */
    float contagion = compute_contagion_internal(bridge, agent, intensity, resonance);

    nimcp_mutex_unlock(bridge->base.mutex);
    return contagion;
}

bool mirror_emotion_trigger_empathy(
    mirror_emotion_bridge_t* bridge,
    const mirror_emotion_resonance_t* resonance
) {
    if (!bridge || !resonance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_emotion_trigger_empathy: required parameter is NULL");
        return false;
    }
    if (resonance->empathy_level < 0.3f) return false;

    /* Phase 8: Heartbeat at operation start */
    mirror_emotion_bridge_heartbeat("mirror_emoti_mirror_emotion_trigg", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->state = MIRROR_EMOTION_STATE_CONTAGION_ACTIVE;

    /* Update bridge's current emotional state from contagion */
    bridge->current_emotion = resonance->emotion_category;
    bridge->current_intensity = resonance->intensity * resonance->contagion_strength;
    bridge->current_valence = resonance->valence;
    bridge->current_arousal = resonance->arousal;

    nimcp_mutex_unlock(bridge->base.mutex);

    nimcp_log(LOG_LEVEL_DEBUG, "Mirror-Emotion: Empathic response triggered (emotion=%u, level=%.2f)",
              resonance->emotion_category, resonance->empathy_level);

    return true;
}

float mirror_emotion_regulate_contagion(
    mirror_emotion_bridge_t* bridge,
    uint32_t agent_id,
    float regulation_level
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_emotion_regulate_contagion: bridge is NULL");
        return 0.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_emotion_bridge_heartbeat("mirror_emoti_mirror_emotion_regul", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    mirror_emotion_agent_state_t* agent = NULL;
    for (uint32_t i = 0; i < MIRROR_EMOTION_MAX_AGENTS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && MIRROR_EMOTION_MAX_AGENTS > 256) {
            mirror_emotion_bridge_heartbeat("mirror_emoti_loop",
                             (float)(i + 1) / (float)MIRROR_EMOTION_MAX_AGENTS);
        }

        if (bridge->agents[i].active && bridge->agents[i].agent_id == agent_id) {
            agent = &bridge->agents[i];
            break;
        }
    }

    float new_contagion = 0.0f;
    if (agent) {
        /* Reduce susceptibility based on regulation */
        agent->contagion_susceptibility *= (1.0f - regulation_level * 0.5f);
        agent->contagion_susceptibility = fmaxf(0.1f, agent->contagion_susceptibility);
        new_contagion = agent->contagion_susceptibility;
    }

    bridge->state = MIRROR_EMOTION_STATE_REGULATED;
    bridge->stats.regulation_events++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return new_contagion;
}

mirror_emotion_agent_state_t* mirror_emotion_get_agent(
    mirror_emotion_bridge_t* bridge,
    uint32_t agent_id
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;

    }

    /* Phase 8: Heartbeat at operation start */
    mirror_emotion_bridge_heartbeat("mirror_emoti_mirror_emotion_get_a", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    mirror_emotion_agent_state_t* agent = get_or_create_agent(bridge, agent_id);
    nimcp_mutex_unlock(bridge->base.mutex);

    return agent;
}

void mirror_emotion_update_familiarity(
    mirror_emotion_bridge_t* bridge,
    uint32_t agent_id,
    float delta
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_emotion_update_familiarity: bridge is NULL");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_emotion_bridge_heartbeat("mirror_emoti_mirror_emotion_updat", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    mirror_emotion_agent_state_t* agent = NULL;
    for (uint32_t i = 0; i < MIRROR_EMOTION_MAX_AGENTS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && MIRROR_EMOTION_MAX_AGENTS > 256) {
            mirror_emotion_bridge_heartbeat("mirror_emoti_loop",
                             (float)(i + 1) / (float)MIRROR_EMOTION_MAX_AGENTS);
        }

        if (bridge->agents[i].active && bridge->agents[i].agent_id == agent_id) {
            agent = &bridge->agents[i];
            break;
        }
    }

    if (agent) {
        agent->familiarity = fminf(1.0f, fmaxf(0.0f, agent->familiarity + delta));
    }

    nimcp_mutex_unlock(bridge->base.mutex);
}

float mirror_emotion_simd_similarity(
    const float* features_a,
    const float* features_b,
    uint32_t dim
) {
    /* Phase 8: Heartbeat at operation start */
    mirror_emotion_bridge_heartbeat("mirror_emoti_mirror_emotion_simd_", 0.0f);


    return nimcp_vector_cosine_similarity(features_a, features_b, dim);
}

void mirror_emotion_simd_au_compare(
    const float* observed_aus,
    const float* template_aus,
    float* similarities,
    uint32_t count
) {
    /* Phase 8: Heartbeat at operation start */
    mirror_emotion_bridge_heartbeat("mirror_emoti_mirror_emotion_simd_", 0.0f);


    for (uint32_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            mirror_emotion_bridge_heartbeat("mirror_emoti_loop",
                             (float)(i + 1) / (float)count);
        }

        similarities[i] = nimcp_vector_cosine_similarity(
            observed_aus + i * MIRROR_EMOTION_ACTION_UNITS,
            template_aus,
            MIRROR_EMOTION_ACTION_UNITS
        );
    }
}

float mirror_emotion_modulate_sensitivity(
    mirror_emotion_bridge_t* bridge,
    uint32_t emotion,
    float intensity,
    float valence
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_emotion_modulate_sensitivity: bridge is NULL");
        return 1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_emotion_bridge_heartbeat("mirror_emoti_mirror_emotion_modul", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->current_emotion = emotion;
    bridge->current_intensity = intensity;
    bridge->current_valence = valence;

    /* Positive emotions increase mirror sensitivity */
    /* Negative high-arousal emotions decrease (protective) */
    float arousal = (emotion < MIRROR_EMOTION_TOTAL_COUNT) ?
                    EMOTION_AROUSAL[emotion] : 0.5f;

    if (valence > 0.0f) {
        bridge->sensitivity_multiplier = 1.0f + valence * 0.3f;
    } else if (arousal > 0.7f) {
        bridge->sensitivity_multiplier = 1.0f - intensity * 0.4f;
    } else {
        bridge->sensitivity_multiplier = 1.0f;
    }

    bridge->sensitivity_multiplier = fmaxf(0.5f, fminf(1.5f,
                                           bridge->sensitivity_multiplier));

    float result = bridge->sensitivity_multiplier;
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

void mirror_emotion_set_crisis_mode(
    mirror_emotion_bridge_t* bridge,
    bool crisis_active
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_emotion_set_crisis_mode: bridge is NULL");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_emotion_bridge_heartbeat("mirror_emoti_mirror_emotion_set_c", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->crisis_mode = crisis_active;
    if (crisis_active) {
        bridge->state = MIRROR_EMOTION_STATE_REGULATED;
        bridge->sensitivity_multiplier = 0.5f;
    }
    nimcp_mutex_unlock(bridge->base.mutex);
}

bool mirror_emotion_register_bio_async(mirror_emotion_bridge_t* bridge) {
    if (!bridge || bridge->bio_async_registered) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_emotion_register_bio_async: bridge is NULL");
        return false;
    }

    /* Mark as registered - actual router registration would happen
     * when integrated into a full bio-async context */
    /* Phase 8: Heartbeat at operation start */
    mirror_emotion_bridge_heartbeat("mirror_emoti_mirror_emotion_regis", 0.0f);


    bridge->bio_async_registered = true;
    return true;
}

void mirror_emotion_unregister_bio_async(mirror_emotion_bridge_t* bridge) {
    if (!bridge || !bridge->bio_async_registered) return;

    /* Phase 8: Heartbeat at operation start */
    mirror_emotion_bridge_heartbeat("mirror_emoti_mirror_emotion_unreg", 0.0f);


    bridge->bio_async_registered = false;
}

bool mirror_emotion_get_stats(
    const mirror_emotion_bridge_t* bridge,
    mirror_emotion_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_emotion_get_stats: required parameter is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_emotion_bridge_heartbeat("mirror_emoti_mirror_emotion_get_s", 0.0f);


    nimcp_mutex_lock(((mirror_emotion_bridge_t*)bridge)->base.mutex);
    *stats = bridge->stats;

    /* Update active agent count */
    stats->active_agents = 0;
    float total_familiarity = 0.0f;
    for (uint32_t i = 0; i < MIRROR_EMOTION_MAX_AGENTS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && MIRROR_EMOTION_MAX_AGENTS > 256) {
            mirror_emotion_bridge_heartbeat("mirror_emoti_loop",
                             (float)(i + 1) / (float)MIRROR_EMOTION_MAX_AGENTS);
        }

        if (bridge->agents[i].active) {
            stats->active_agents++;
            total_familiarity += bridge->agents[i].familiarity;
        }
    }
    if (stats->active_agents > 0) {
        stats->avg_agent_familiarity = total_familiarity / stats->active_agents;
    }

    nimcp_mutex_unlock(((mirror_emotion_bridge_t*)bridge)->base.mutex);
    return true;
}

void mirror_emotion_reset_stats(mirror_emotion_bridge_t* bridge) {
    if (!bridge) return;

    /* Phase 8: Heartbeat at operation start */
    mirror_emotion_bridge_heartbeat("mirror_emoti_mirror_emotion_reset", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(mirror_emotion_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);
}

//=============================================================================
// B22: Instance Health Agent Setter
//=============================================================================

void mirror_emotion_bridge_set_instance_health_agent(
    mirror_emotion_bridge_t* bridge,
    nimcp_health_agent_t* agent)
{
    if (bridge) {
        bridge->health_agent = agent;
    }
}

//=============================================================================
// B22: Training Hook Stubs
//=============================================================================

int mirror_emotion_bridge_training_begin(mirror_emotion_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_emotion_bridge_training_begin: NULL argument");
        return -1;
    }
    mirror_emotion_bridge_heartbeat("mirror_emoti_training_begin", 0.0f);
    return 0;
}

int mirror_emotion_bridge_training_end(mirror_emotion_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_emotion_bridge_training_end: NULL argument");
        return -1;
    }
    mirror_emotion_bridge_heartbeat("mirror_emoti_training_end", 1.0f);
    return 0;
}

int mirror_emotion_bridge_training_step(mirror_emotion_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_emotion_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    mirror_emotion_bridge_heartbeat_instance(bridge->health_agent, "mirror_emotion_bridge_training_step", progress);
    return 0;
}
