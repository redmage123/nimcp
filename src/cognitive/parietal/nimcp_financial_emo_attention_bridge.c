/**
 * @file nimcp_financial_emo_attention_bridge.c
 * @brief Financial Emotion-Attention Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-29
 *
 * WHAT: Bridge for modulating attention width and stimulus boosting based on
 *       emotional states. Part of Phase 5 Attention Systems integration.
 *
 * WHY:  Emotions profoundly affect attention allocation in trading. This bridge
 *       enables dynamic attention width modulation, emotion-congruent stimulus
 *       boosting, and detection of dangerous tunnel vision states.
 *
 * HOW:  Emotional state vector is processed using Broaden-and-Build theory
 *       principles to compute attention width and stimulus boost factors.
 *
 * @author NIMCP Development Team
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>

#include "cognitive/parietal/nimcp_financial_emo_attention_bridge.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

#define LOG_MODULE "financial_emo_attention"

/* ============================================================================
 * Health Agent Integration (Phase 8: System-Wide Health Integration)
 * ============================================================================ */
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

/* Health agent: using pre-existing custom implementation */
static nimcp_health_agent_t* g_financial_emo_attention_bridge_health_agent = NULL;

BRIDGE_DEFINE_MESH_REGISTRATION(financial_emo_attention_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


/** @brief Send heartbeat from financial_emo_attention_bridge module */
static inline void fin_emo_attn_heartbeat(const char* operation, float progress) {
    if (g_financial_emo_attention_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_financial_emo_attention_bridge_health_agent, operation, progress);
    }
}

/** @brief Send heartbeat from financial_emo_attention_bridge module (instance-level) */
static inline void fin_emo_attn_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_financial_emo_attention_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_financial_emo_attention_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_financial_emo_attention_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

/* ============================================================================
 * Thread-Local Error
 * ============================================================================ */

static _Thread_local char fin_emo_attn_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(fin_emo_attn_last_error, sizeof(fin_emo_attn_last_error), fmt, args);
    va_end(args);
}

/* ============================================================================
 * KG Wiring Integration
 * ============================================================================ */

#define KG_MSG_FIN_EMO_ATTN_MODULATE     "FIN_EMO_ATTN_MODULATE"
#define KG_MSG_FIN_EMO_ATTN_TUNNEL       "FIN_EMO_ATTN_TUNNEL_VISION"
#define KG_MSG_FIN_EMO_ATTN_BOOST        "FIN_EMO_ATTN_BOOST"
#define KG_MSG_FIN_EMO_ATTN_ERROR        "FIN_EMO_ATTN_ERROR"

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

/**
 * @brief Financial emotion-attention bridge structure
 */
struct financial_emo_attention_bridge {
    bridge_base_t base;                     /**< MUST be first: base bridge infrastructure */
    uint32_t magic;
    fin_emo_attention_bridge_state_t state;

    /* Configuration */
    fin_emo_attention_config_t config;

    /* Current attention state */
    float current_width;                    /**< Current attention width [0,1] */
    float stimulus_boosts[FIN_STIMULUS_COUNT]; /**< Boost factors per stimulus type */
    bool tunnel_vision_active;              /**< Tunnel vision currently detected */

    /* Last emotion input for reference */
    fin_emotion_input_t last_emotion;

    /* Timestamp of last update */
    uint64_t last_update_ms;

    /* Subsystem pointers */
    void* immune;
    void* bbb;
    void* health_agent;
    void* kg_wiring;
    void* logger;
    void* security;
    void* ethics;
    void* lgss;
    void* cycle;
    void* bio_router;

    /* Statistics */
    fin_emo_attention_bridge_stats_t stats;
};

/* Security integration via bridge_base */
BRIDGE_DEFINE_SECURITY_SETTERS(financial_emo_attention_bridge)

/* ============================================================================
 * Static Name Tables
 * ============================================================================ */

static const char* stimulus_type_names[] = {
    "opportunity",
    "threat",
    "novelty",
    "routine",
    "social",
    "confirmation",
    "contradiction",
    "neutral"
};

static const char* state_names[] = {
    "uninitialized",
    "initialized",
    "active",
    "degraded",
    "error"
};

static const char* tunnel_vision_names[] = {
    "none",
    "mild",
    "moderate",
    "severe",
    "critical"
};

static inline float maxf(float a, float b) {
    return (a > b) ? a : b;
}

static inline float minf(float a, float b) {
    return (a < b) ? a : b;
}

/**
 * @brief Publish message through KG wiring
 */
static int bridge_kg_publish(financial_emo_attention_bridge_t* bridge, const char* msg_type,
                              const void* payload, size_t size) {
    if (bridge && bridge->kg_wiring && bridge->config.enable_kg_messaging) {
        bridge->stats.kg_messages_sent++;
        /* kg_wiring_publish would be called here */
        (void)msg_type; (void)payload; (void)size;
        return 0;
    }
    return 0;
}

/**
 * @brief Compute attention width from emotional state
 *
 * Based on Broaden-and-Build theory:
 * - Positive emotions (joy) broaden attention scope
 * - Negative emotions (fear, anger, panic) narrow attention
 * - Sadness reduces overall attention capacity
 */
static float compute_attention_width(
    const fin_emotion_input_t* emotion,
    const fin_emo_attention_config_t* config
) {
    /* Start at baseline */
    float width = config->baseline_width;

    /* Broadening effects (positive emotions) */
    float broadening = emotion->joy * config->width_params.joy_broadening_factor;

    /* Narrowing effects (negative emotions) */
    float fear_narrowing = emotion->fear * config->width_params.fear_narrowing_factor;
    float anger_narrowing = emotion->anger * config->width_params.anger_narrowing_factor;
    float panic_narrowing = emotion->panic * config->width_params.panic_narrowing_factor;

    /* Panic has the strongest narrowing effect */
    float total_narrowing = fear_narrowing + anger_narrowing + panic_narrowing * 1.5f;

    /* Sadness reduces overall capacity (scales the range) */
    float capacity_reduction = emotion->sadness * config->width_params.sadness_reduction_factor;

    /* Compute final width */
    /* Broadening pushes toward 1.0 */
    width += broadening * (1.0f - width);

    /* Narrowing pulls toward 0.0 */
    width -= total_narrowing * width;

    /* Capacity reduction scales the result */
    width *= (1.0f - capacity_reduction * 0.5f);

    /* Surprise briefly resets attention (mild narrowing then broadening) */
    /* For now, model as slight random perturbation toward center */
    if (emotion->surprise > 0.5f) {
        float surprise_effect = (emotion->surprise - 0.5f) * 0.2f;
        width = width * (1.0f - surprise_effect) + config->baseline_width * surprise_effect;
    }

    return nimcp_clampf(width, 0.0f, 1.0f);
}

/**
 * @brief Compute stimulus boost factors based on emotional state
 *
 * Emotion-congruent processing: people attend more to stimuli that
 * match their current emotional state.
 */
static void compute_stimulus_boosts(
    const fin_emotion_input_t* emotion,
    const fin_emo_attention_config_t* config,
    float boosts[FIN_STIMULUS_COUNT]
) {
    /* Initialize all to neutral (1.0 = no boost) */
    for (int i = 0; i < FIN_STIMULUS_COUNT; i++) {
        boosts[i] = 1.0f;
    }

    /* Greed boosts opportunity signals */
    boosts[FIN_STIMULUS_OPPORTUNITY] = 1.0f + emotion->greed * config->boost_params.greed_opportunity_boost;

    /* Fear/Panic boosts threat signals */
    float fear_factor = maxf(emotion->fear, emotion->panic);
    boosts[FIN_STIMULUS_THREAT] = 1.0f + fear_factor * config->boost_params.fear_threat_boost;

    /* Anger boosts contradiction signals (looking for targets) */
    boosts[FIN_STIMULUS_CONTRADICTION] = 1.0f + emotion->anger * config->boost_params.anger_contradiction_boost;

    /* Joy boosts confirmation signals (feel-good bias) */
    boosts[FIN_STIMULUS_CONFIRMATION] = 1.0f + emotion->joy * config->boost_params.joy_confirmation_boost;

    /* Novelty is boosted by surprise */
    boosts[FIN_STIMULUS_NOVELTY] = 1.0f + emotion->surprise * 0.5f;

    /* Social signals boosted by fear (herding behavior) and joy (shared excitement) */
    boosts[FIN_STIMULUS_SOCIAL] = 1.0f + (emotion->fear * 0.3f + emotion->joy * 0.2f);

    /* Routine signals suppressed by high arousal states */
    float arousal = emotion->fear + emotion->panic + emotion->joy + emotion->anger + emotion->surprise;
    boosts[FIN_STIMULUS_ROUTINE] = nimcp_clampf(1.0f - arousal * 0.2f, 0.5f, 1.0f);

    /* Clamp all boosts to reasonable range */
    for (int i = 0; i < FIN_STIMULUS_COUNT; i++) {
        boosts[i] = nimcp_clampf(boosts[i], 0.5f, 3.0f);
    }
}

/**
 * @brief Determine tunnel vision severity from attention width and emotions
 */
static fin_tunnel_vision_severity_t determine_tunnel_vision_severity(
    float width,
    const fin_emotion_input_t* emotion,
    const fin_emo_attention_config_t* config
) {
    if (width >= config->tunnel_vision_threshold) {
        return FIN_TUNNEL_VISION_NONE;
    }

    /* Check for critical (panic-induced) */
    if (width < config->critical_threshold || emotion->panic > 0.8f) {
        return FIN_TUNNEL_VISION_CRITICAL;
    }

    /* Calculate severity based on how far below threshold */
    float severity_ratio = 1.0f - (width / config->tunnel_vision_threshold);

    /* Panic amplifies severity */
    severity_ratio += emotion->panic * 0.3f;

    if (severity_ratio > 0.7f) {
        return FIN_TUNNEL_VISION_SEVERE;
    } else if (severity_ratio > 0.4f) {
        return FIN_TUNNEL_VISION_MODERATE;
    } else {
        return FIN_TUNNEL_VISION_MILD;
    }
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int financial_emo_attention_bridge_default_config(fin_emo_attention_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");
        return -1;
    }

    fin_emo_attn_heartbeat("fin_emo_attn_default_config", 0.0f);

    memset(config, 0, sizeof(*config));

    /* Width modulation parameters */
    config->width_params.joy_broadening_factor = 0.4f;
    config->width_params.fear_narrowing_factor = 0.5f;
    config->width_params.anger_narrowing_factor = 0.3f;
    config->width_params.panic_narrowing_factor = 0.7f;
    config->width_params.sadness_reduction_factor = 0.2f;

    /* Stimulus boost parameters */
    config->boost_params.greed_opportunity_boost = 0.8f;
    config->boost_params.fear_threat_boost = 1.2f;
    config->boost_params.anger_contradiction_boost = 0.6f;
    config->boost_params.joy_confirmation_boost = 0.5f;

    /* Tunnel vision detection */
    config->tunnel_vision_threshold = FIN_EMO_ATTN_TUNNEL_VISION_THRESHOLD;
    config->critical_threshold = 0.1f;

    /* Baseline */
    config->baseline_width = FIN_EMO_ATTN_DEFAULT_WIDTH;
    config->width_decay_rate = 0.1f;  /* 10% per second back to baseline */

    /* Integration settings */
    config->enable_immune_integration = true;
    config->enable_bbb_validation = true;
    config->enable_kg_messaging = true;
    config->enable_health_monitoring = true;

    /* Logging */
    config->verbose_logging = false;

    fin_emo_attn_heartbeat("fin_emo_attn_default_config", 1.0f);
    return 0;
}

financial_emo_attention_bridge_t* financial_emo_attention_bridge_create(
    const fin_emo_attention_config_t* config
) {
    fin_emo_attn_heartbeat("fin_emo_attn_create", 0.0f);

    financial_emo_attention_bridge_t* bridge = nimcp_calloc(1, sizeof(financial_emo_attention_bridge_t));
    if (!bridge) {
        set_error("Failed to allocate financial_emo_attention_bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY, "Failed to allocate financial_emo_attention_bridge");
        return NULL;
    }

    bridge->magic = FINANCIAL_EMO_ATTENTION_BRIDGE_MAGIC;
    bridge->state = FIN_EMO_ATTN_STATE_UNINITIALIZED;

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        financial_emo_attention_bridge_default_config(&bridge->config);
    }

    /* Initialize bridge base (creates mutex) */
    if (bridge_base_init(&bridge->base, BIO_MODULE_FINANCIAL_EMO_ATTENTION, "financial_emo_attention") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "financial_emo_attention_bridge_create: validation failed");
        return NULL;
    }

    /* Initialize attention state to baseline */
    bridge->current_width = bridge->config.baseline_width;
    for (int i = 0; i < FIN_STIMULUS_COUNT; i++) {
        bridge->stimulus_boosts[i] = 1.0f;
    }
    bridge->tunnel_vision_active = false;

    /* Initialize last emotion to neutral */
    memset(&bridge->last_emotion, 0, sizeof(bridge->last_emotion));

    bridge->last_update_ms = nimcp_time_get_ms();

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    bridge->state = FIN_EMO_ATTN_STATE_INITIALIZED;

    fin_emo_attn_heartbeat("fin_emo_attn_create", 1.0f);
    return bridge;
}

void financial_emo_attention_bridge_destroy(financial_emo_attention_bridge_t* bridge) {
    if (!bridge || bridge->magic != FINANCIAL_EMO_ATTENTION_BRIDGE_MAGIC) {
        return;
    }

    fin_emo_attn_heartbeat("fin_emo_attn_destroy", 0.0f);

    /* Cleanup base */
    bridge_base_cleanup(&bridge->base);

    bridge->magic = 0;
    nimcp_free(bridge);
    bridge = NULL;

    fin_emo_attn_heartbeat("fin_emo_attn_destroy", 1.0f);
}

int financial_emo_attention_bridge_reset(financial_emo_attention_bridge_t* bridge) {
    if (!bridge || bridge->magic != FINANCIAL_EMO_ATTENTION_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_emo_attention_bridge_reset: invalid bridge");
        return FIN_EMO_ATTN_ERR_NULL;
    }

    fin_emo_attn_heartbeat("fin_emo_attn_reset", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset attention state to baseline */
    bridge->current_width = bridge->config.baseline_width;
    for (int i = 0; i < FIN_STIMULUS_COUNT; i++) {
        bridge->stimulus_boosts[i] = 1.0f;
    }
    bridge->tunnel_vision_active = false;

    /* Reset last emotion */
    memset(&bridge->last_emotion, 0, sizeof(bridge->last_emotion));

    bridge->last_update_ms = nimcp_time_get_ms();

    /* Reset stats */
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    bridge->state = FIN_EMO_ATTN_STATE_INITIALIZED;

    nimcp_mutex_unlock(bridge->base.mutex);

    fin_emo_attn_heartbeat("fin_emo_attn_reset", 1.0f);
    return FIN_EMO_ATTN_ERR_OK;
}

/* ============================================================================
 * Subsystem Setters
 * ============================================================================ */

#define FIN_EMO_ATTN_SETTER(name, field) \
    int financial_emo_attention_bridge_set_##name(financial_emo_attention_bridge_t* bridge, void* ptr) { \
        if (!bridge || bridge->magic != FINANCIAL_EMO_ATTENTION_BRIDGE_MAGIC) { \
            set_error("bridge is NULL in set_" #name); \
            NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_emo_attention_bridge_set_" #name ": bridge is NULL"); \
            return FIN_EMO_ATTN_ERR_NULL; \
        } \
        nimcp_mutex_lock(bridge->base.mutex); \
        bridge->field = ptr; \
        nimcp_mutex_unlock(bridge->base.mutex); \
        return FIN_EMO_ATTN_ERR_OK; \
    }

FIN_EMO_ATTN_SETTER(immune,        immune)
FIN_EMO_ATTN_SETTER(health_agent,  health_agent)
FIN_EMO_ATTN_SETTER(kg_wiring,     kg_wiring)
FIN_EMO_ATTN_SETTER(logger,        logger)
FIN_EMO_ATTN_SETTER(security,      security)
FIN_EMO_ATTN_SETTER(bio_router,    bio_router)

/* Security setters for bbb, ethics, lgss, coordinator handled by bridge_base */

/* ============================================================================
 * Core Attention Modulation API
 * ============================================================================ */

int financial_emo_attention_bridge_modulate(
    financial_emo_attention_bridge_t* bridge,
    const fin_emotion_input_t* emotion,
    fin_emo_attention_state_t* state
) {
    if (!bridge || bridge->magic != FINANCIAL_EMO_ATTENTION_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_emo_attention_bridge_modulate: invalid bridge");
        return FIN_EMO_ATTN_ERR_NULL;
    }
    if (!emotion) {
        set_error("emotion is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_emo_attention_bridge_modulate: emotion is NULL");
        return FIN_EMO_ATTN_ERR_NULL;
    }

    fin_emo_attn_heartbeat("fin_emo_attn_modulate", 0.0f);

    /* BBB validation */
    if (bridge->config.enable_bbb_validation && bridge->bbb) {
        BRIDGE_BBB_VALIDATE(bridge, emotion, sizeof(*emotion));
        bridge->stats.bbb_validations++;
    }

    /* Immune check */
    if (bridge->config.enable_immune_integration && bridge->immune) {
        bridge->stats.immune_checks++;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Compute new attention width */
    bridge->current_width = compute_attention_width(emotion, &bridge->config);

    /* Compute stimulus boosts */
    compute_stimulus_boosts(emotion, &bridge->config, bridge->stimulus_boosts);

    /* Check for tunnel vision */
    bridge->tunnel_vision_active = (bridge->current_width < bridge->config.tunnel_vision_threshold);

    /* Store emotion for reference */
    bridge->last_emotion = *emotion;

    bridge->last_update_ms = nimcp_time_get_ms();
    bridge->stats.modulations++;
    bridge->state = FIN_EMO_ATTN_STATE_ACTIVE;

    /* Copy to output if requested */
    if (state) {
        state->attention_width = bridge->current_width;
        state->tunnel_vision = bridge->tunnel_vision_active;
        state->num_stimuli = FIN_STIMULUS_COUNT;
        /* Note: stimulus_boosts pointer would need to be allocated by caller
         * or we copy to a static buffer. For safety, we don't set it here. */
        state->stimulus_boosts = NULL;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    /* KG notification */
    bridge_kg_publish(bridge, KG_MSG_FIN_EMO_ATTN_MODULATE, emotion, sizeof(*emotion));

    fin_emo_attn_heartbeat("fin_emo_attn_modulate", 1.0f);
    bridge->stats.health_heartbeats++;

    return FIN_EMO_ATTN_ERR_OK;
}

int financial_emo_attention_bridge_detect_tunnel_vision(
    financial_emo_attention_bridge_t* bridge,
    fin_tunnel_vision_result_t* result
) {
    if (!bridge || bridge->magic != FINANCIAL_EMO_ATTENTION_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "detect_tunnel_vision: invalid bridge");
        return FIN_EMO_ATTN_ERR_NULL;
    }
    if (!result) {
        set_error("result is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "detect_tunnel_vision: result is NULL");
        return FIN_EMO_ATTN_ERR_NULL;
    }

    fin_emo_attn_heartbeat("fin_emo_attn_detect_tunnel", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    memset(result, 0, sizeof(*result));

    result->attention_width = bridge->current_width;
    result->contributing_fear = bridge->last_emotion.fear;
    result->contributing_panic = bridge->last_emotion.panic;

    result->severity = determine_tunnel_vision_severity(
        bridge->current_width,
        &bridge->last_emotion,
        &bridge->config
    );

    result->detected = (result->severity != FIN_TUNNEL_VISION_NONE);

    /* Determine if intervention is recommended */
    result->intervention_recommended = (result->severity >= FIN_TUNNEL_VISION_SEVERE);

    /* Generate description */
    switch (result->severity) {
        case FIN_TUNNEL_VISION_NONE:
            snprintf(result->description, sizeof(result->description),
                     "Attention width normal (%.0f%%). No tunnel vision detected.",
                     bridge->current_width * 100.0f);
            break;

        case FIN_TUNNEL_VISION_MILD:
            snprintf(result->description, sizeof(result->description),
                     "Mild attentional narrowing (width=%.0f%%). May miss peripheral signals.",
                     bridge->current_width * 100.0f);
            break;

        case FIN_TUNNEL_VISION_MODERATE:
            snprintf(result->description, sizeof(result->description),
                     "Moderate tunnel vision (width=%.0f%%, fear=%.0f%%). "
                     "Consider broadening information sources.",
                     bridge->current_width * 100.0f,
                     bridge->last_emotion.fear * 100.0f);
            break;

        case FIN_TUNNEL_VISION_SEVERE:
            snprintf(result->description, sizeof(result->description),
                     "SEVERE tunnel vision (width=%.0f%%, panic=%.0f%%). "
                     "High risk of missing critical signals. Recommend pausing.",
                     bridge->current_width * 100.0f,
                     bridge->last_emotion.panic * 100.0f);
            break;

        case FIN_TUNNEL_VISION_CRITICAL:
            snprintf(result->description, sizeof(result->description),
                     "CRITICAL tunnel vision (width=%.0f%%, panic=%.0f%%). "
                     "IMMEDIATE intervention required. STOP TRADING NOW.",
                     bridge->current_width * 100.0f,
                     bridge->last_emotion.panic * 100.0f);
            break;
    }

    bridge->stats.tunnel_vision_detections++;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* KG notification if tunnel vision detected */
    if (result->detected) {
        bridge_kg_publish(bridge, KG_MSG_FIN_EMO_ATTN_TUNNEL, result, sizeof(*result));
    }

    fin_emo_attn_heartbeat("fin_emo_attn_detect_tunnel", 1.0f);
    bridge->stats.health_heartbeats++;

    return FIN_EMO_ATTN_ERR_OK;
}

int financial_emo_attention_bridge_boost_stimuli(
    financial_emo_attention_bridge_t* bridge,
    const fin_boosted_stimulus_t* stimuli,
    size_t count,
    fin_boosted_stimulus_t* output
) {
    if (!bridge || bridge->magic != FINANCIAL_EMO_ATTENTION_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "boost_stimuli: invalid bridge");
        return FIN_EMO_ATTN_ERR_NULL;
    }
    if (!stimuli || !output) {
        set_error("stimuli or output is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "boost_stimuli: NULL array");
        return FIN_EMO_ATTN_ERR_NULL;
    }
    if (count > FIN_EMO_ATTN_MAX_STIMULI) {
        set_error("count exceeds maximum");
        return FIN_EMO_ATTN_ERR_CAPACITY;
    }

    fin_emo_attn_heartbeat("fin_emo_attn_boost", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    for (size_t i = 0; i < count; i++) {
        /* Copy input to output */
        output[i] = stimuli[i];

        /* Apply boost based on stimulus type */
        if (stimuli[i].type < FIN_STIMULUS_COUNT) {
            output[i].boost_factor = bridge->stimulus_boosts[stimuli[i].type];
        } else {
            output[i].boost_factor = 1.0f;
        }

        /* Compute boosted salience */
        output[i].boosted_salience = output[i].base_salience * output[i].boost_factor;

        /* Clamp to [0, 1] */
        output[i].boosted_salience = nimcp_clampf(output[i].boosted_salience, 0.0f, 1.0f);
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    /* KG notification */
    bridge_kg_publish(bridge, KG_MSG_FIN_EMO_ATTN_BOOST, NULL, 0);

    fin_emo_attn_heartbeat("fin_emo_attn_boost", 1.0f);

    return FIN_EMO_ATTN_ERR_OK;
}

int financial_emo_attention_bridge_get_state(
    const financial_emo_attention_bridge_t* bridge,
    fin_emo_attention_state_t* state
) {
    if (!bridge || bridge->magic != FINANCIAL_EMO_ATTENTION_BRIDGE_MAGIC || !state) {
        set_error("NULL argument in get_state");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "get_state: NULL argument");
        return FIN_EMO_ATTN_ERR_NULL;
    }

    fin_emo_attn_heartbeat("fin_emo_attn_get_state", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    state->attention_width = bridge->current_width;
    state->tunnel_vision = bridge->tunnel_vision_active;
    state->num_stimuli = FIN_STIMULUS_COUNT;
    state->stimulus_boosts = NULL;  /* Caller must allocate if needed */

    nimcp_mutex_unlock(bridge->base.mutex);

    return FIN_EMO_ATTN_ERR_OK;
}

float financial_emo_attention_bridge_get_width(
    const financial_emo_attention_bridge_t* bridge
) {
    if (!bridge || bridge->magic != FINANCIAL_EMO_ATTENTION_BRIDGE_MAGIC) {
        return -1.0f;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    float width = bridge->current_width;
    nimcp_mutex_unlock(bridge->base.mutex);

    return width;
}

int financial_emo_attention_bridge_decay(
    financial_emo_attention_bridge_t* bridge,
    uint64_t elapsed_ms
) {
    if (!bridge || bridge->magic != FINANCIAL_EMO_ATTENTION_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "decay: invalid bridge");
        return FIN_EMO_ATTN_ERR_NULL;
    }

    if (elapsed_ms == 0 || bridge->config.width_decay_rate <= 0.0f) {
        return FIN_EMO_ATTN_ERR_OK;
    }

    fin_emo_attn_heartbeat("fin_emo_attn_decay", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    /* Decay attention width toward baseline */
    float baseline = bridge->config.baseline_width;
    float decay_factor = bridge->config.width_decay_rate * (float)elapsed_ms / 1000.0f;
    decay_factor = nimcp_clampf(decay_factor, 0.0f, 1.0f);

    /* Move toward baseline by decay_factor */
    bridge->current_width = bridge->current_width + (baseline - bridge->current_width) * decay_factor;

    /* Decay stimulus boosts toward 1.0 */
    for (int i = 0; i < FIN_STIMULUS_COUNT; i++) {
        bridge->stimulus_boosts[i] = bridge->stimulus_boosts[i] +
                                      (1.0f - bridge->stimulus_boosts[i]) * decay_factor;
    }

    /* Update tunnel vision status */
    bridge->tunnel_vision_active = (bridge->current_width < bridge->config.tunnel_vision_threshold);

    bridge->last_update_ms = nimcp_time_get_ms();

    nimcp_mutex_unlock(bridge->base.mutex);

    fin_emo_attn_heartbeat("fin_emo_attn_decay", 1.0f);

    return FIN_EMO_ATTN_ERR_OK;
}

/* ============================================================================
 * Query API Implementation
 * ============================================================================ */

fin_emo_attention_bridge_state_t financial_emo_attention_bridge_get_bridge_state(
    const financial_emo_attention_bridge_t* bridge
) {
    if (!bridge || bridge->magic != FINANCIAL_EMO_ATTENTION_BRIDGE_MAGIC) {
        return FIN_EMO_ATTN_STATE_ERROR;
    }
    fin_emo_attn_heartbeat("fin_emo_attn_get_bridge_state", 0.0f);
    return bridge->state;
}

int financial_emo_attention_bridge_get_stats(
    const financial_emo_attention_bridge_t* bridge,
    fin_emo_attention_bridge_stats_t* stats
) {
    if (!bridge || bridge->magic != FINANCIAL_EMO_ATTENTION_BRIDGE_MAGIC || !stats) {
        set_error("NULL argument in get_stats");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "get_stats: NULL argument");
        return FIN_EMO_ATTN_ERR_NULL;
    }

    fin_emo_attn_heartbeat("fin_emo_attn_get_stats", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return FIN_EMO_ATTN_ERR_OK;
}

void financial_emo_attention_bridge_reset_stats(financial_emo_attention_bridge_t* bridge) {
    if (!bridge || bridge->magic != FINANCIAL_EMO_ATTENTION_BRIDGE_MAGIC) {
        return;
    }

    fin_emo_attn_heartbeat("fin_emo_attn_reset_stats", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->base.mutex);
}

const char* financial_emo_attention_bridge_get_last_error(void) {
    return fin_emo_attn_last_error;
}

/* ============================================================================
 * Health Integration
 * ============================================================================ */

int financial_emo_attention_bridge_heartbeat(
    financial_emo_attention_bridge_t* bridge,
    const char* operation,
    float progress
) {
    if (!bridge || bridge->magic != FINANCIAL_EMO_ATTENTION_BRIDGE_MAGIC) {
        return FIN_EMO_ATTN_ERR_NULL;
    }

    /* Forward to global health agent */
    fin_emo_attn_heartbeat(operation ? operation : "fin_emo_attn_heartbeat", progress);

    /* Forward to instance-level health agent */
    if (bridge->health_agent) {
        nimcp_health_agent_heartbeat_ex(
            (nimcp_health_agent_t*)bridge->health_agent, operation, progress);
    }

    bridge->stats.health_heartbeats++;
    return FIN_EMO_ATTN_ERR_OK;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* fin_emo_attn_stimulus_name(fin_stimulus_type_t stimulus_type) {
    if (stimulus_type >= FIN_STIMULUS_COUNT) {
        return "unknown";
    }
    return stimulus_type_names[stimulus_type];
}

const char* fin_emo_attn_state_name(fin_emo_attention_bridge_state_t state) {
    if (state > FIN_EMO_ATTN_STATE_ERROR) {
        return "unknown";
    }
    return state_names[state];
}

const char* fin_emo_attn_tunnel_vision_name(fin_tunnel_vision_severity_t severity) {
    if (severity > FIN_TUNNEL_VISION_CRITICAL) {
        return "unknown";
    }
    return tunnel_vision_names[severity];
}

const char* financial_emo_attention_bridge_version(void) {
    return FINANCIAL_EMO_ATTENTION_BRIDGE_VERSION;
}

/* ============================================================================
 * Training Integration (B23 Upgrade Compatibility)
 * ============================================================================ */

int financial_emo_attention_bridge_training_begin(financial_emo_attention_bridge_t* bridge) {
    if (!bridge || bridge->magic != FINANCIAL_EMO_ATTENTION_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "financial_emo_attention_bridge_training_begin: NULL argument");
        return -1;
    }
    fin_emo_attn_heartbeat_instance((nimcp_health_agent_t*)bridge->health_agent,
                                     "financial_emo_attention_bridge_training_begin", 0.0f);
    return 0;
}

int financial_emo_attention_bridge_training_end(financial_emo_attention_bridge_t* bridge) {
    if (!bridge || bridge->magic != FINANCIAL_EMO_ATTENTION_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "financial_emo_attention_bridge_training_end: NULL argument");
        return -1;
    }
    fin_emo_attn_heartbeat_instance((nimcp_health_agent_t*)bridge->health_agent,
                                     "financial_emo_attention_bridge_training_end", 1.0f);
    return 0;
}

int financial_emo_attention_bridge_training_step(financial_emo_attention_bridge_t* bridge, float progress) {
    if (!bridge || bridge->magic != FINANCIAL_EMO_ATTENTION_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "financial_emo_attention_bridge_training_step: NULL argument");
        return -1;
    }

    /* Safety gates: ethics + LGSS pre-check */
    BRIDGE_ETHICS_GATE(bridge, "financial_emo_attention_bridge_training_step");
    BRIDGE_LGSS_GATE(bridge, "financial_emo_attention_bridge_training_step");

    fin_emo_attn_heartbeat_instance((nimcp_health_agent_t*)bridge->health_agent,
                                     "financial_emo_attention_bridge_training_step", progress);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}
