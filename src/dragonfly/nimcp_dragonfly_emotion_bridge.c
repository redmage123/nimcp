/**
 * @file nimcp_dragonfly_emotion_bridge.c
 * @brief Emotion-Dragonfly Integration Bridge Implementation
 *
 * WHAT: Integrates emotional/motivational states with hunting
 * WHY:  Enables realistic behavioral modulation by internal states
 * HOW:  Bidirectional communication with emotion system
 *
 * @author NIMCP Team
 * @date 2024-12-29
 */

#include "dragonfly/nimcp_dragonfly_emotion_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include <time.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_learning_constants.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(dragonfly_emotion_bridge)

#define LOG_MODULE "DRAGONFLY_EMOTION_BRIDGE"


//=============================================================================
// Local Helpers
//=============================================================================

static inline uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

//=============================================================================
// Internal Structure
//=============================================================================

struct dragonfly_emotion_bridge_s {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    dragonfly_emotion_config_t config;

    /* Connected systems */
    dragonfly_system_t* dragonfly;
    emotion_system_t emotion;
    bool connected;

    /* Current state */
    emotional_state_t state;
    emotion_modulation_t modulation;

    /* Statistics */
    dragonfly_emotion_stats_t stats;

    /* Timing */
    uint64_t creation_time_us;
    uint64_t last_update_us;
};

//=============================================================================
// Name Functions
//=============================================================================

const char* dragonfly_drive_name(motivational_drive_t drive) {
    switch (drive) {
        case DRIVE_HUNGER:     return "hunger";
        case DRIVE_FEAR:       return "fear";
        case DRIVE_CURIOSITY:  return "curiosity";
        case DRIVE_AGGRESSION: return "aggression";
        case DRIVE_REST:       return "rest";
        case DRIVE_MATING:     return "mating";
        default:               return "unknown";
    }
}

const char* dragonfly_valence_name(emotional_valence_t valence) {
    switch (valence) {
        case VALENCE_VERY_NEGATIVE: return "very_negative";
        case VALENCE_NEGATIVE:      return "negative";
        case VALENCE_NEUTRAL:       return "neutral";
        case VALENCE_POSITIVE:      return "positive";
        case VALENCE_VERY_POSITIVE: return "very_positive";
        default:                    return "unknown";
    }
}

const char* dragonfly_arousal_name(arousal_level_t arousal) {
    switch (arousal) {
        case AROUSAL_DORMANT:  return "dormant";
        case AROUSAL_CALM:     return "calm";
        case AROUSAL_ALERT:    return "alert";
        case AROUSAL_EXCITED:  return "excited";
        case AROUSAL_FRENZIED: return "frenzied";
        default:               return "unknown";
    }
}

//=============================================================================
// Configuration Functions
//=============================================================================

dragonfly_emotion_config_t dragonfly_emotion_default_config(void) {
    dragonfly_emotion_config_t config = {
        /* Drive parameters */
        .hunger_decay_rate = 0.01f,
        .hunger_satisfaction = 0.5f,
        .fear_decay_rate = 0.1f,
        .frustration_buildup_rate = 0.15f,

        /* Thresholds */
        .hunt_motivation_threshold = 0.3f,
        .fear_abort_threshold = 0.8f,
        .frustration_rest_threshold = 0.7f,

        /* Modulation strengths */
        .hunger_pursuit_strength = 0.3f,
        .fear_performance_penalty = 0.2f,
        .confidence_performance_bonus = 0.2f,

        /* Learning */
        .success_confidence_boost = 0.1f,
        .failure_confidence_penalty = 0.05f,
        .emotional_learning_rate = NIMCP_LEARNING_RATE_COARSE,

        /* Homeostasis */
        .enable_emotional_homeostasis = true,
        .homeostasis_rate = 0.05f
    };
    return config;
}

bool dragonfly_emotion_validate_config(const dragonfly_emotion_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_emotion_validate_config: config is NULL");
        return false;
    }

    /* Check ranges */
    if (config->hunger_decay_rate < 0.0f) {
        return false;
    }
    if (config->hunger_satisfaction < 0.0f || config->hunger_satisfaction > 1.0f) {
        return false;
    }
    if (config->fear_decay_rate < 0.0f) {
        return false;
    }
    if (config->frustration_buildup_rate < 0.0f) {
        return false;
    }

    if (config->hunt_motivation_threshold < 0.0f || config->hunt_motivation_threshold > 1.0f) {
        return false;
    }
    if (config->fear_abort_threshold < 0.0f || config->fear_abort_threshold > 1.0f) {
        return false;
    }
    if (config->frustration_rest_threshold < 0.0f || config->frustration_rest_threshold > 1.0f) {
        return false;
    }

    if (config->emotional_learning_rate < 0.0f || config->emotional_learning_rate > 1.0f) {
        return false;
    }
    if (config->homeostasis_rate < 0.0f || config->homeostasis_rate > 1.0f) {
        return false;
    }

    return true;
}

//=============================================================================
// Internal Helpers
//=============================================================================

static emotional_valence_t compute_valence(const emotional_state_t* state) {
    /* Weighted sum of positive and negative influences */
    float valence_score = 0.0f;

    valence_score += state->confidence * 0.3f;
    valence_score -= state->drives[DRIVE_FEAR] * 0.5f;
    valence_score -= (1.0f - state->drives[DRIVE_HUNGER]) * 0.2f;  /* Satisfied = positive */

    if (valence_score < -0.5f) return VALENCE_VERY_NEGATIVE;
    if (valence_score < -0.1f) return VALENCE_NEGATIVE;
    if (valence_score < 0.1f) return VALENCE_NEUTRAL;
    if (valence_score < 0.5f) return VALENCE_POSITIVE;
    return VALENCE_VERY_POSITIVE;
}

static arousal_level_t compute_arousal(const emotional_state_t* state) {
    /* Arousal from drives and motivation */
    float arousal_score = state->motivation * 0.4f +
                          state->drives[DRIVE_HUNGER] * 0.3f +
                          state->drives[DRIVE_FEAR] * 0.3f;

    if (arousal_score < 0.1f) return AROUSAL_DORMANT;
    if (arousal_score < 0.3f) return AROUSAL_CALM;
    if (arousal_score < 0.5f) return AROUSAL_ALERT;
    if (arousal_score < 0.8f) return AROUSAL_EXCITED;
    return AROUSAL_FRENZIED;
}

static motivational_drive_t find_primary_drive(const emotional_state_t* state) {
    motivational_drive_t primary = DRIVE_HUNGER;
    float max_val = state->drives[0];

    for (int i = 1; i < DRIVE_COUNT; i++) {
        if (state->drives[i] > max_val) {
            max_val = state->drives[i];
            primary = (motivational_drive_t)i;
        }
    }

    return primary;
}

static void update_modulation(dragonfly_emotion_bridge_t bridge) {
    emotional_state_t* state = &bridge->state;
    emotion_modulation_t* mod = &bridge->modulation;

    /* Pursuit modulation from hunger and confidence */
    mod->pursuit_aggression = state->drives[DRIVE_HUNGER] * 0.5f +
                              state->confidence * 0.3f +
                              (1.0f - state->drives[DRIVE_FEAR]) * 0.2f;
    mod->pursuit_aggression = nimcp_clampf(mod->pursuit_aggression, 0.0f, 1.0f);

    /* Abort threshold (lower when hungry or confident) */
    mod->abort_threshold = 0.5f - state->drives[DRIVE_HUNGER] * 0.3f -
                           state->confidence * 0.1f +
                           state->drives[DRIVE_FEAR] * 0.3f;
    mod->abort_threshold = nimcp_clampf(mod->abort_threshold, 0.1f, 0.9f);

    /* Target selectivity (hungrier = less selective) */
    mod->target_selectivity = 1.0f - state->drives[DRIVE_HUNGER] * 0.5f;
    mod->target_selectivity = nimcp_clampf(mod->target_selectivity, 0.2f, 1.0f);

    /* Performance modulation */
    float arousal_factor = 1.0f;
    switch (state->arousal) {
        case AROUSAL_DORMANT:  arousal_factor = 0.5f; break;
        case AROUSAL_CALM:     arousal_factor = 0.8f; break;
        case AROUSAL_ALERT:    arousal_factor = 1.0f; break;
        case AROUSAL_EXCITED:  arousal_factor = 1.1f; break;
        case AROUSAL_FRENZIED: arousal_factor = 0.9f; break;  /* Too aroused */
    }

    mod->focus_level = arousal_factor * (1.0f - state->drives[DRIVE_FEAR] * 0.3f);
    mod->focus_level = nimcp_clampf(mod->focus_level, 0.0f, 1.0f);

    mod->reaction_speed = arousal_factor *
                          (1.0f + state->confidence * bridge->config.confidence_performance_bonus) *
                          (1.0f - state->drives[DRIVE_FEAR] * bridge->config.fear_performance_penalty);
    mod->reaction_speed = nimcp_clampf(mod->reaction_speed, 0.3f, 1.3f);

    mod->decision_speed = state->confidence * 0.5f + (1.0f - state->drives[DRIVE_FEAR]) * 0.5f;
    mod->decision_speed = nimcp_clampf(mod->decision_speed, 0.3f, 1.0f);

    /* Strategic modulation */
    mod->prefer_safe_targets = state->drives[DRIVE_FEAR] > 0.4f ||
                               state->confidence < 0.3f;

    mod->accept_risky_pursuits = state->drives[DRIVE_HUNGER] > 0.7f &&
                                 state->confidence > 0.5f &&
                                 state->drives[DRIVE_FEAR] < 0.3f;

    mod->energy_investment = state->drives[DRIVE_HUNGER] * 0.6f + state->confidence * 0.4f;
    mod->energy_investment = nimcp_clampf(mod->energy_investment, 0.0f, 1.0f);
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

dragonfly_emotion_bridge_t dragonfly_emotion_bridge_create(
    const dragonfly_emotion_config_t* config
) {
    dragonfly_emotion_config_t cfg = config ? *config : dragonfly_emotion_default_config();

    if (!dragonfly_emotion_validate_config(&cfg)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_emotion_bridge_create: invalid configuration");
        return NULL;
    }

    dragonfly_emotion_bridge_t bridge = nimcp_calloc(1, sizeof(struct dragonfly_emotion_bridge_s));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dragonfly_emotion_bridge_create: failed to allocate bridge");
        return NULL;
    }

    bridge->config = cfg;
    bridge->creation_time_us = get_time_us();

    /* Initialize state */
    bridge->state.valence = VALENCE_NEUTRAL;
    bridge->state.arousal = AROUSAL_CALM;
    bridge->state.dominance = 0.5f;
    bridge->state.drives[DRIVE_HUNGER] = 0.5f;  /* Moderate hunger */
    bridge->state.drives[DRIVE_FEAR] = 0.0f;
    bridge->state.drives[DRIVE_CURIOSITY] = 0.3f;
    bridge->state.drives[DRIVE_AGGRESSION] = 0.2f;
    bridge->state.drives[DRIVE_REST] = 0.0f;
    bridge->state.drives[DRIVE_MATING] = 0.0f;
    bridge->state.primary_drive = DRIVE_HUNGER;
    bridge->state.motivation = 0.5f;
    bridge->state.confidence = 0.5f;
    bridge->state.persistence = 0.5f;
    bridge->state.risk_tolerance = 0.5f;

    /* Initialize modulation */
    bridge->modulation.pursuit_aggression = 0.5f;
    bridge->modulation.abort_threshold = 0.5f;
    bridge->modulation.target_selectivity = 0.5f;
    bridge->modulation.focus_level = 1.0f;
    bridge->modulation.reaction_speed = 1.0f;
    bridge->modulation.decision_speed = 1.0f;
    bridge->modulation.energy_investment = 0.5f;

    if (bridge_base_init(&bridge->base, 0, "dragonfly_emotion") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "dragonfly_emotion_bridge_create: failed to initialize bridge base");
        nimcp_free(bridge);
        return NULL;
    }
    if (!bridge->base.mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "dragonfly_emotion_bridge_create: mutex is NULL after init");
        nimcp_free(bridge);
        return NULL;
    }

    return bridge;
}

void dragonfly_emotion_bridge_destroy(dragonfly_emotion_bridge_t bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "dragonfly_emotion");

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
}

int dragonfly_emotion_bridge_connect(
    dragonfly_emotion_bridge_t bridge,
    dragonfly_system_t* dragonfly,
    emotion_system_t emotion
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_emotion_bridge_connect: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->dragonfly = dragonfly;
    bridge->emotion = emotion;
    bridge->connected = (dragonfly != NULL);
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int dragonfly_emotion_bridge_disconnect(dragonfly_emotion_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_emotion_bridge_disconnect: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->dragonfly = NULL;
    bridge->emotion = NULL;
    bridge->connected = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Update Functions
//=============================================================================

int dragonfly_emotion_bridge_update(
    dragonfly_emotion_bridge_t bridge,
    float dt_s
) {
    if (!bridge || dt_s <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_emotion_bridge_update: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    emotional_state_t* state = &bridge->state;

    /* Natural hunger increase */
    state->drives[DRIVE_HUNGER] += bridge->config.hunger_decay_rate * dt_s;
    state->drives[DRIVE_HUNGER] = nimcp_clampf(state->drives[DRIVE_HUNGER], 0.0f, 1.0f);

    /* Fear decay */
    state->drives[DRIVE_FEAR] *= (1.0f - bridge->config.fear_decay_rate * dt_s);
    state->drives[DRIVE_FEAR] = nimcp_clampf(state->drives[DRIVE_FEAR], 0.0f, 1.0f);

    /* Rest drive increases when motivation is low */
    if (state->motivation < 0.3f) {
        state->drives[DRIVE_REST] += 0.05f * dt_s;
    } else {
        state->drives[DRIVE_REST] *= (1.0f - 0.1f * dt_s);
    }
    state->drives[DRIVE_REST] = nimcp_clampf(state->drives[DRIVE_REST], 0.0f, 1.0f);

    /* Homeostatic regulation */
    if (bridge->config.enable_emotional_homeostasis) {
        float rate = bridge->config.homeostasis_rate * dt_s;

        /* Drive confidence toward moderate level */
        state->confidence += (0.5f - state->confidence) * rate;

        /* Reduce extreme arousal */
        if (state->arousal == AROUSAL_FRENZIED || state->arousal == AROUSAL_DORMANT) {
            /* Implicit through drive updates */
        }
    }

    /* Update derived states */
    state->motivation = state->drives[DRIVE_HUNGER] * 0.5f +
                        state->drives[DRIVE_CURIOSITY] * 0.2f +
                        state->confidence * 0.3f -
                        state->drives[DRIVE_REST] * 0.3f;
    state->motivation = nimcp_clampf(state->motivation, 0.0f, 1.0f);

    state->persistence = state->drives[DRIVE_HUNGER] * 0.4f +
                         state->confidence * 0.4f +
                         state->drives[DRIVE_AGGRESSION] * 0.2f;
    state->persistence = nimcp_clampf(state->persistence, 0.0f, 1.0f);

    state->risk_tolerance = state->drives[DRIVE_HUNGER] * 0.5f +
                            state->confidence * 0.3f -
                            state->drives[DRIVE_FEAR] * 0.4f;
    state->risk_tolerance = nimcp_clampf(state->risk_tolerance, 0.0f, 1.0f);

    /* Update primary drive */
    state->primary_drive = find_primary_drive(state);

    /* Update valence and arousal */
    state->valence = compute_valence(state);
    state->arousal = compute_arousal(state);

    /* Update modulation */
    update_modulation(bridge);

    bridge->last_update_us = get_time_us();

    /* Update statistics */
    bridge->stats.emotional_events++;
    bridge->stats.avg_valence = (bridge->stats.avg_valence * (bridge->stats.emotional_events - 1) +
                                 (float)state->valence / 4.0f) / bridge->stats.emotional_events;
    bridge->stats.avg_arousal = (bridge->stats.avg_arousal * (bridge->stats.emotional_events - 1) +
                                 (float)state->arousal / 4.0f) / bridge->stats.emotional_events;
    bridge->stats.avg_motivation = (bridge->stats.avg_motivation * (bridge->stats.emotional_events - 1) +
                                    state->motivation) / bridge->stats.emotional_events;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int dragonfly_emotion_process_event(
    dragonfly_emotion_bridge_t bridge,
    const emotional_event_t* event
) {
    if (!bridge || !event) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_emotion_process_event: required parameter is NULL (bridge, event)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    emotional_state_t* state = &bridge->state;

    if (event->is_success) {
        /* Successful hunt */
        state->drives[DRIVE_HUNGER] -= bridge->config.hunger_satisfaction;
        state->drives[DRIVE_HUNGER] = nimcp_clampf(state->drives[DRIVE_HUNGER], 0.0f, 1.0f);

        state->confidence += bridge->config.success_confidence_boost;
        state->confidence = nimcp_clampf(state->confidence, 0.0f, 1.0f);

        bridge->stats.positive_events++;
    } else if (event->is_escape) {
        /* Target escaped - frustration */
        float frustration = bridge->config.frustration_buildup_rate *
                           (1.0f + (float)event->consecutive_failures * 0.2f);

        state->confidence -= bridge->config.failure_confidence_penalty;
        state->confidence = nimcp_clampf(state->confidence, 0.0f, 1.0f);

        state->drives[DRIVE_AGGRESSION] += frustration * 0.3f;
        state->drives[DRIVE_AGGRESSION] = nimcp_clampf(state->drives[DRIVE_AGGRESSION], 0.0f, 1.0f);

        /* Check if should rest due to frustration */
        if (event->consecutive_failures >= 3) {
            state->drives[DRIVE_REST] += 0.2f;
            bridge->stats.frustration_rests++;
        }

        bridge->stats.negative_events++;
    }

    if (event->is_threat) {
        /* Predator detected */
        state->drives[DRIVE_FEAR] = nimcp_clampf(state->drives[DRIVE_FEAR] + 0.5f, 0.0f, 1.0f);

        if (state->drives[DRIVE_FEAR] > bridge->config.fear_abort_threshold) {
            bridge->stats.fear_aborts++;
        }

        bridge->stats.negative_events++;
    }

    if (event->is_competitor) {
        /* Competitor detected */
        state->drives[DRIVE_AGGRESSION] += 0.3f;
        state->drives[DRIVE_AGGRESSION] = nimcp_clampf(state->drives[DRIVE_AGGRESSION], 0.0f, 1.0f);
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int dragonfly_emotion_report_success(
    dragonfly_emotion_bridge_t bridge,
    float satisfaction_level
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_emotion_report_success: bridge is NULL");
        return -1;
    }

    emotional_event_t event = {0};
    event.is_success = true;
    event.timestamp_us = get_time_us();

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->state.drives[DRIVE_HUNGER] -= satisfaction_level * bridge->config.hunger_satisfaction;
    bridge->state.drives[DRIVE_HUNGER] = nimcp_clampf(bridge->state.drives[DRIVE_HUNGER], 0.0f, 1.0f);

    bridge->state.confidence += bridge->config.success_confidence_boost * satisfaction_level;
    bridge->state.confidence = nimcp_clampf(bridge->state.confidence, 0.0f, 1.0f);

    bridge->stats.positive_events++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int dragonfly_emotion_report_failure(
    dragonfly_emotion_bridge_t bridge,
    float frustration_level,
    const char* reason
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_emotion_report_failure: bridge is NULL");
        return -1;
    }
    (void)reason;  /* For future logging */

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->state.confidence -= bridge->config.failure_confidence_penalty * frustration_level;
    bridge->state.confidence = nimcp_clampf(bridge->state.confidence, 0.0f, 1.0f);

    bridge->state.drives[DRIVE_AGGRESSION] += frustration_level * 0.2f;
    bridge->state.drives[DRIVE_AGGRESSION] = nimcp_clampf(bridge->state.drives[DRIVE_AGGRESSION], 0.0f, 1.0f);

    bridge->stats.negative_events++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int dragonfly_emotion_report_threat(
    dragonfly_emotion_bridge_t bridge,
    float threat_level,
    const float threat_position[3]
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_emotion_report_threat: bridge is NULL");
        return -1;
    }
    (void)threat_position;  /* For future spatial awareness */

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->state.drives[DRIVE_FEAR] += threat_level * 0.5f;
    bridge->state.drives[DRIVE_FEAR] = nimcp_clampf(bridge->state.drives[DRIVE_FEAR], 0.0f, 1.0f);

    if (bridge->state.drives[DRIVE_FEAR] > bridge->config.fear_abort_threshold) {
        bridge->stats.fear_aborts++;
    }

    bridge->stats.negative_events++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Query Functions
//=============================================================================

int dragonfly_emotion_get_state(
    const dragonfly_emotion_bridge_t bridge,
    emotional_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_emotion_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->state;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int dragonfly_emotion_get_modulation(
    const dragonfly_emotion_bridge_t bridge,
    emotion_modulation_t* modulation
) {
    if (!bridge || !modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_emotion_get_modulation: required parameter is NULL (bridge, modulation)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *modulation = bridge->modulation;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool dragonfly_emotion_should_hunt(const dragonfly_emotion_bridge_t bridge) {
    if (!bridge) {
        return false;
    }

    return bridge->state.motivation >= bridge->config.hunt_motivation_threshold &&
           bridge->state.drives[DRIVE_FEAR] < bridge->config.fear_abort_threshold &&
           bridge->state.drives[DRIVE_REST] < bridge->config.frustration_rest_threshold;
}

float dragonfly_emotion_get_motivation(const dragonfly_emotion_bridge_t bridge) {
    if (!bridge) return 0.0f;
    return bridge->state.motivation;
}

float dragonfly_emotion_get_drive(
    const dragonfly_emotion_bridge_t bridge,
    motivational_drive_t drive
) {
    if (!bridge || drive >= DRIVE_COUNT) return 0.0f;
    return bridge->state.drives[drive];
}

int dragonfly_emotion_get_stats(
    const dragonfly_emotion_bridge_t bridge,
    dragonfly_emotion_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_emotion_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}
