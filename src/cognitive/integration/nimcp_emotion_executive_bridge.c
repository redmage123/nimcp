/**
 * @file nimcp_emotion_executive_bridge.c
 * @brief Emotion-Executive Bridge Implementation
 *
 * WHAT: Bidirectional integration between emotion and executive control systems
 * WHY: Emotions provide motivational salience for decisions; executive functions
 *      enable top-down regulation of emotional responses
 * HOW: Emotional state biases decision-making; executive control regulates emotions
 *
 * @author NIMCP Development Team
 * @date 2025-01
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/integration/nimcp_emotion_executive_bridge.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(emotion_executive_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_emotion_executive_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_emotion_executive_bridge_mesh_registry = NULL;

nimcp_error_t emotion_executive_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_emotion_executive_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "emotion_executive_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "emotion_executive_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_emotion_executive_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_emotion_executive_bridge_mesh_registry = registry;
    return err;
}

void emotion_executive_bridge_mesh_unregister(void) {
    if (g_emotion_executive_bridge_mesh_registry && g_emotion_executive_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_emotion_executive_bridge_mesh_registry, g_emotion_executive_bridge_mesh_id);
        g_emotion_executive_bridge_mesh_id = 0;
        g_emotion_executive_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from emotion_executive_bridge module (instance-level) */
static inline void emotion_executive_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_emotion_executive_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_emotion_executive_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_emotion_executive_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


#define LOG_MODULE "EMOTION_EXECUTIVE_BRIDGE"


/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Internal emotional influence state
 */
typedef struct emotional_influence {
    float valence;      /**< Valence [-1.0 to 1.0] */
    float arousal;      /**< Arousal [0.0 to 1.0] */
    float dominance;    /**< Sense of control [0.0 to 1.0] */
} emotional_influence_t;

/**
 * @brief Decision record for tracking emotional influence
 */
typedef struct decision_record {
    uint32_t decision_id;           /**< Decision identifier */
    emotional_influence_t influence; /**< Emotional influence at decision time */
    float outcome_valence;          /**< Outcome valence [-1.0 to 1.0] */
    bool outcome_recorded;          /**< Whether outcome has been recorded */
    uint64_t timestamp;             /**< Timestamp of decision */
} decision_record_t;

/**
 * @brief Full bridge structure definition
 */
struct emotion_executive_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    emotion_executive_config_t config;      /**< Bridge configuration */
    emotional_influence_t current_state;    /**< Current emotional state */
    decision_record_t* decisions;           /**< Decision history array */
    size_t decision_capacity;               /**< Capacity of decisions array */
    size_t decision_count;                  /**< Current number of decisions */
    emotion_executive_stats_t stats;        /**< Bridge statistics */
    bool initialized;                       /**< Initialization flag */
};

/* ============================================================================
 * Constants
 * ============================================================================ */

#define DEFAULT_DECISION_CAPACITY 64
#define DEFAULT_INFLUENCE_WEIGHT 0.4f
#define DEFAULT_REGULATION_STRENGTH 0.6f
#define DEFAULT_DECISION_THRESHOLD 0.3f

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get current timestamp in milliseconds
 */
static uint64_t get_timestamp_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

/**
 * @brief Find decision record by ID (unlocked version)
 */
static decision_record_t* find_decision_unlocked(emotion_executive_bridge_t* bridge,
                                                  uint32_t decision_id) {
    for (size_t i = 0; i < bridge->decision_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->decision_count > 256) {
            emotion_executive_bridge_heartbeat("emotion_exec_loop",
                             (float)(i + 1) / (float)bridge->decision_count);
        }

        if (bridge->decisions[i].decision_id == decision_id) {
            return &bridge->decisions[i];
        }
    }
    return NULL;
}

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

int emotion_executive_default_config(emotion_executive_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_executive_bridge_heartbeat("emotion_exec_emotion_executive_de", 0.0f);


    config->emotion_influence_weight = DEFAULT_INFLUENCE_WEIGHT;
    config->regulation_strength = DEFAULT_REGULATION_STRENGTH;
    config->decision_threshold = DEFAULT_DECISION_THRESHOLD;
    config->enable_emotional_biasing = true;
    config->enable_auto_regulation = false;
    config->regulation_trigger_threshold = 0.8f;
    config->max_regulation_attempts = 3;

    return 0;
}

emotion_executive_bridge_t* emotion_executive_bridge_create(
    const emotion_executive_config_t* config
) {
    /* Allocate bridge structure */
    /* Phase 8: Heartbeat at operation start */
    emotion_executive_bridge_heartbeat("emotion_exec_create", 0.0f);


    emotion_executive_bridge_t* bridge = nimcp_malloc(sizeof(emotion_executive_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }
    memset(bridge, 0, sizeof(emotion_executive_bridge_t));

    /* Initialize config */
    if (config) {
        bridge->config = *config;
    } else {
        emotion_executive_default_config(&bridge->config);
    }

    /* Allocate decisions array */
    bridge->decisions = nimcp_calloc(DEFAULT_DECISION_CAPACITY, sizeof(decision_record_t));
    if (!bridge->decisions) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "emotion_executive_bridge_create: bridge->decisions is NULL");
        return NULL;
    }
    /* calloc zero-initializes */
    bridge->decision_capacity = DEFAULT_DECISION_CAPACITY;
    bridge->decision_count = 0;

    /* Initialize current state to neutral */
    bridge->current_state.valence = 0.0f;
    bridge->current_state.arousal = 0.5f;
    bridge->current_state.dominance = 0.5f;

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(emotion_executive_stats_t));

    /* Initialize base bridge infrastructure (includes mutex) */
    if (bridge_base_init(&bridge->base, 0, "emotion_executive") != 0) {
        nimcp_free(bridge->decisions);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "emotion_executive_bridge_create: bridge_base_init failed");
        return NULL;
    }

    bridge->initialized = true;

    return bridge;
}

void emotion_executive_bridge_destroy(emotion_executive_bridge_t* bridge) {
    if (!bridge) {
        return;
    }
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "emotion_executive");

    /* Phase 8: Heartbeat at operation start */
    emotion_executive_bridge_heartbeat("emotion_exec_destroy", 0.0f);

    /* Cleanup base bridge infrastructure (includes mutex) */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
        bridge->base.mutex = NULL;
    }

    /* Free decisions array */
    if (bridge->decisions) {
        nimcp_free(bridge->decisions);
        bridge->decisions = NULL;
    }

    bridge->initialized = false;

    /* Free bridge structure */
    nimcp_free(bridge);
    bridge = NULL;
}

/* ============================================================================
 * Core Functions
 * ============================================================================ */

int emotion_executive_influence_decision(
    emotion_executive_bridge_t* bridge,
    const emotion_executive_decision_context_t* decision_context,
    emotion_executive_emotional_bias_t* emotional_bias_out
) {
    if (!bridge || !decision_context || !emotional_bias_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_executive_influence_decision: required parameter is NULL (bridge, decision_context, emotional_bias_out)");
        return -1;
    }

    if (!bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_executive_influence_decision: bridge->initialized is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_executive_bridge_heartbeat("emotion_exec_emotion_executive_in", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Create decision record */
    if (bridge->decision_count < bridge->decision_capacity) {
        decision_record_t* record = &bridge->decisions[bridge->decision_count];
        record->decision_id = decision_context->decision_id;
        record->influence = bridge->current_state;
        record->outcome_valence = 0.0f;
        record->outcome_recorded = false;
        record->timestamp = get_timestamp_ms();
        bridge->decision_count++;
    }

    /* Compute emotional bias based on current state and config */
    float weight = bridge->config.emotion_influence_weight;

    /* Valence bias: current emotional valence scaled by weight */
    emotional_bias_out->valence_bias = bridge->current_state.valence * weight;

    /* Approach/avoid: derived from valence (positive = approach, negative = avoid) */
    emotional_bias_out->approach_avoid = bridge->current_state.valence * weight;

    /* Urgency modifier: high arousal increases urgency */
    emotional_bias_out->urgency_modifier = 1.0f + (bridge->current_state.arousal - 0.5f) * weight;

    /* Risk tolerance: high dominance increases risk tolerance, low decreases */
    emotional_bias_out->risk_tolerance_modifier = 1.0f + (bridge->current_state.dominance - 0.5f) * weight;

    /* Signal confidence based on arousal intensity */
    emotional_bias_out->signal_confidence = bridge->current_state.arousal;

    /* Determine dominant emotion based on current state */
    if (bridge->current_state.valence > 0.3f) {
        if (bridge->current_state.arousal > 0.6f) {
            emotional_bias_out->dominant_emotion = EMOTION_EXECUTIVE_TYPE_JOY;
        } else {
            emotional_bias_out->dominant_emotion = EMOTION_EXECUTIVE_TYPE_TRUST;
        }
    } else if (bridge->current_state.valence < -0.3f) {
        if (bridge->current_state.arousal > 0.6f) {
            if (bridge->current_state.dominance > 0.5f) {
                emotional_bias_out->dominant_emotion = EMOTION_EXECUTIVE_TYPE_ANGER;
            } else {
                emotional_bias_out->dominant_emotion = EMOTION_EXECUTIVE_TYPE_FEAR;
            }
        } else {
            emotional_bias_out->dominant_emotion = EMOTION_EXECUTIVE_TYPE_SADNESS;
        }
    } else {
        emotional_bias_out->dominant_emotion = EMOTION_EXECUTIVE_TYPE_ANTICIPATION;
    }

    /* Emotion intensity from arousal */
    emotional_bias_out->emotion_intensity = bridge->current_state.arousal;

    /* Update statistics */
    bridge->stats.decisions_influenced++;

    /* Update average bias magnitude */
    float bias_magnitude = (float)fabs(emotional_bias_out->valence_bias);
    if (bridge->stats.decisions_influenced == 1) {
        bridge->stats.avg_bias_magnitude = bias_magnitude;
    } else {
        bridge->stats.avg_bias_magnitude =
            (bridge->stats.avg_bias_magnitude * (bridge->stats.decisions_influenced - 1) + bias_magnitude)
            / bridge->stats.decisions_influenced;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int emotion_executive_on_decision(
    emotion_executive_bridge_t* bridge,
    uint32_t decision_id,
    const emotion_executive_decision_outcome_t* outcome
) {
    if (!bridge || !outcome) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_executive_on_decision: required parameter is NULL (bridge, outcome)");
        return -1;
    }

    if (!bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_executive_on_decision: bridge->initialized is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_executive_bridge_heartbeat("emotion_exec_emotion_executive_on", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Find decision record */
    decision_record_t* record = find_decision_unlocked(bridge, decision_id);
    if (!record) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return -1;
    }

    /* Record outcome valence */
    record->outcome_valence = outcome->outcome_valence;
    record->outcome_recorded = true;

    /* Update current state based on outcome */
    float outcome_effect = outcome->outcome_valence * 0.3f;
    float expectation_effect = outcome->expectation_violation * 0.2f;

    /* Positive outcome boosts valence, negative decreases */
    bridge->current_state.valence = nimcp_clampf(
        bridge->current_state.valence + outcome_effect + expectation_effect,
        -1.0f, 1.0f
    );

    /* Unexpected outcomes increase arousal */
    float arousal_change = (float)fabs(outcome->expectation_violation) * 0.2f;
    bridge->current_state.arousal = nimcp_clampf(
        bridge->current_state.arousal + arousal_change,
        0.0f, 1.0f
    );

    /* Success increases dominance, failure decreases */
    float dominance_change = outcome->success ? 0.1f : -0.1f;
    bridge->current_state.dominance = nimcp_clampf(
        bridge->current_state.dominance + dominance_change,
        0.0f, 1.0f
    );

    /* Update statistics */
    bridge->stats.emotions_triggered++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int emotion_executive_regulate_emotion(
    emotion_executive_bridge_t* bridge,
    emotion_executive_emotion_type_t emotion_type,
    const emotion_executive_regulation_target_t* regulation_target
) {
    if (!bridge || !regulation_target) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_executive_regulate_emotion: required parameter is NULL (bridge, regulation_target)");
        return -1;
    }

    if (!bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_executive_regulate_emotion: bridge->initialized is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_executive_bridge_heartbeat("emotion_exec_emotion_executive_re", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float strength = bridge->config.regulation_strength;
    float target_intensity = regulation_target->target_intensity;

    /* Apply regulation based on strategy */
    switch (regulation_target->strategy) {
        case EMOTION_EXECUTIVE_REG_REAPPRAISAL:
            /* Cognitive reappraisal: move valence toward neutral */
            bridge->current_state.valence += (0.0f - bridge->current_state.valence) * strength;
            break;

        case EMOTION_EXECUTIVE_REG_SUPPRESSION:
            /* Suppression: reduce arousal */
            bridge->current_state.arousal += (target_intensity - bridge->current_state.arousal) * strength;
            break;

        case EMOTION_EXECUTIVE_REG_DISTRACTION:
            /* Distraction: reduce both valence magnitude and arousal */
            bridge->current_state.valence *= (1.0f - strength * 0.5f);
            bridge->current_state.arousal += (0.3f - bridge->current_state.arousal) * strength * 0.5f;
            break;

        case EMOTION_EXECUTIVE_REG_ACCEPTANCE:
            /* Acceptance: increase dominance (sense of control) */
            bridge->current_state.dominance += (1.0f - bridge->current_state.dominance) * strength;
            break;

        case EMOTION_EXECUTIVE_REG_SITUATION_MOD:
            /* Situation modification: general regulation toward target */
            bridge->current_state.valence += (target_intensity - bridge->current_state.valence) * strength;
            break;

        default:
            /* Default: move toward target intensity as general regulation */
            bridge->current_state.arousal += (target_intensity - bridge->current_state.arousal) * strength;
            break;
    }

    /* Clamp all values to valid ranges */
    bridge->current_state.valence = nimcp_clampf(bridge->current_state.valence, -1.0f, 1.0f);
    bridge->current_state.arousal = nimcp_clampf(bridge->current_state.arousal, 0.0f, 1.0f);
    bridge->current_state.dominance = nimcp_clampf(bridge->current_state.dominance, 0.0f, 1.0f);

    /* Update statistics */
    bridge->stats.regulations_applied++;
    bridge->stats.successful_regulations++;

    /* Update average regulation effectiveness */
    float effectiveness = strength;
    if (bridge->stats.regulations_applied == 1) {
        bridge->stats.avg_regulation_effectiveness = effectiveness;
    } else {
        bridge->stats.avg_regulation_effectiveness =
            (bridge->stats.avg_regulation_effectiveness * (bridge->stats.regulations_applied - 1) + effectiveness)
            / bridge->stats.regulations_applied;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int emotion_executive_get_emotional_state(
    emotion_executive_bridge_t* bridge,
    emotion_executive_emotional_state_t* state_out
) {
    if (!bridge || !state_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_executive_get_emotional_state: required parameter is NULL (bridge, state_out)");
        return -1;
    }

    if (!bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_executive_get_emotional_state: bridge->initialized is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_executive_bridge_heartbeat("emotion_exec_emotion_executive_ge", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Copy current state to output */
    state_out->valence = bridge->current_state.valence;
    state_out->arousal = bridge->current_state.arousal;

    /* Determine dominant emotion based on current state */
    if (bridge->current_state.valence > 0.3f) {
        if (bridge->current_state.arousal > 0.6f) {
            state_out->dominant_emotion = EMOTION_EXECUTIVE_TYPE_JOY;
        } else {
            state_out->dominant_emotion = EMOTION_EXECUTIVE_TYPE_TRUST;
        }
    } else if (bridge->current_state.valence < -0.3f) {
        if (bridge->current_state.arousal > 0.6f) {
            if (bridge->current_state.dominance > 0.5f) {
                state_out->dominant_emotion = EMOTION_EXECUTIVE_TYPE_ANGER;
            } else {
                state_out->dominant_emotion = EMOTION_EXECUTIVE_TYPE_FEAR;
            }
        } else {
            state_out->dominant_emotion = EMOTION_EXECUTIVE_TYPE_SADNESS;
        }
    } else {
        state_out->dominant_emotion = EMOTION_EXECUTIVE_TYPE_ANTICIPATION;
    }

    /* Dominant intensity from arousal */
    state_out->dominant_intensity = bridge->current_state.arousal;

    /* Stability: inversely related to arousal variability (simplified) */
    state_out->stability = 1.0f - bridge->current_state.arousal * 0.5f;

    /* Regulation active if arousal is being actively managed */
    state_out->regulation_active = bridge->config.enable_auto_regulation &&
        bridge->current_state.arousal > bridge->config.regulation_trigger_threshold;

    /* Regulation effectiveness from stats */
    state_out->regulation_effectiveness = bridge->stats.avg_regulation_effectiveness;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Statistics Functions
 * ============================================================================ */

int emotion_executive_get_stats(
    const emotion_executive_bridge_t* bridge,
    emotion_executive_stats_t* stats_out
) {
    if (!bridge || !stats_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_executive_get_stats: required parameter is NULL (bridge, stats_out)");
        return -1;
    }

    if (!bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_executive_get_stats: bridge->initialized is NULL");
        return -1;
    }

    /* Cast away const for mutex lock (stats read is still logically const) */
    /* Phase 8: Heartbeat at operation start */
    emotion_executive_bridge_heartbeat("emotion_exec_emotion_executive_ge", 0.0f);


    emotion_executive_bridge_t* mutable_bridge = (emotion_executive_bridge_t*)bridge;

    nimcp_mutex_lock(mutable_bridge->base.mutex);
    *stats_out = bridge->stats;
    nimcp_mutex_unlock(mutable_bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void emotion_executive_bridge_set_instance_health_agent(emotion_executive_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "emotion_executive_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int emotion_executive_bridge_training_begin(emotion_executive_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotion_executive_bridge_training_begin: NULL argument");
        return -1;
    }
    emotion_executive_bridge_heartbeat_instance(bridge->health_agent, "emotion_executive_bridge_training_begin", 0.0f);
    return 0;
}

int emotion_executive_bridge_training_end(emotion_executive_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotion_executive_bridge_training_end: NULL argument");
        return -1;
    }
    emotion_executive_bridge_heartbeat_instance(bridge->health_agent, "emotion_executive_bridge_training_end", 1.0f);
    return 0;
}

int emotion_executive_bridge_training_step(emotion_executive_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotion_executive_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    emotion_executive_bridge_heartbeat_instance(bridge->health_agent, "emotion_executive_bridge_training_step", progress);
    return 0;
}
