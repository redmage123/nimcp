/**
 * @file nimcp_financial_neuromod_bridge.c
 * @brief Financial Neuromodulator Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-29
 *
 * WHAT: Bridge for modeling neuromodulatory influences on financial decision-making,
 *       tracking five key neurotransmitters (dopamine, serotonin, norepinephrine,
 *       acetylcholine, adenosine) and their effects on trading parameters.
 *
 * WHY:  Financial decisions are profoundly influenced by neurochemical state.
 *       By modeling the neuromodulatory system, we can modulate trading behavior
 *       based on simulated neurochemical state and apply appropriate parameter
 *       adjustments to investor archetypes.
 *
 * HOW:  Neuromodulator levels are updated based on market events. Effects on
 *       trading parameters are computed via weighted influence functions.
 *       Natural decay/recovery processes are simulated over time.
 *
 * @author NIMCP Development Team
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>

#include "cognitive/parietal/nimcp_financial_neuromod_bridge.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Health Agent Integration (Phase 8: System-Wide Health Integration)
 * ============================================================================ */
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(fin_neuromod)

/* Stub heartbeat for migration compatibility */
static inline void fin_neuromod_heartbeat_global(const char* op, float progress) {
    (void)op; (void)progress;
}

BRIDGE_DEFINE_MESH_REGISTRATION(fin_neuromod, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * Immune/BBB Integration (Phase 9: Security Integration)
 * ============================================================================ */

struct brain_immune_system;
typedef struct brain_immune_system brain_immune_system_t;
extern int brain_immune_validate_operation(brain_immune_system_t* immune,
                                            const char* operation,
                                            uint32_t severity);

struct bbb_system_struct;
extern int bbb_validate_data(bbb_system_t bbb, const void* data,
                              size_t size, const char* context);

/* ============================================================================
 * Thread-Local Error Handling
 * ============================================================================ */

static _Thread_local char fin_neuromod_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(fin_neuromod_last_error, sizeof(fin_neuromod_last_error), fmt, args);
    va_end(args);
}

const char* financial_neuromod_bridge_get_last_error(void) {
    return fin_neuromod_last_error;
}

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

/* Use fin_neuromod_op_state_t from header for operational state */

struct financial_neuromod_bridge {
    uint32_t magic;
    fin_neuromod_config_t config;
    fin_neuromod_op_state_t op_state;
    fin_neuromod_bridge_stats_t stats;

    /* Current neuromodulator state */
    fin_neuromod_state_t neuromod_state;

    /* Subsystem pointers */
    void* immune;
    void* bbb;
    void* health_agent;
    void* kg_wiring;
    void* logger;
    void* security;
    void* ethics;
    const void* lgss;
    void* coordinator;
    void* bio_router;

    /* Callbacks */
    fin_neuromod_state_callback_t state_callback;
    void* state_callback_data;
    fin_neuromod_fatigue_callback_t fatigue_callback;
    void* fatigue_callback_data;
    fin_neuromod_arousal_callback_t arousal_callback;
    void* arousal_callback_data;

    /* Last update timestamp */
    uint64_t last_update_ms;
};

static inline float lerpf(float a, float b, float t) {
    return a + t * (b - a);
}

/* Exponential decay toward target */
static inline float decay_toward(float current, float target, float rate, float dt) {
    float factor = expf(-rate * dt);
    return target + (current - target) * factor;
}

/* ============================================================================
 * KG Wiring Message Types
 * ============================================================================ */

#define KG_MSG_FIN_NEUROMOD_UPDATE      "FIN_NEUROMOD_UPDATE"
#define KG_MSG_FIN_NEUROMOD_EFFECTS     "FIN_NEUROMOD_EFFECTS"
#define KG_MSG_FIN_NEUROMOD_ARCHETYPE   "FIN_NEUROMOD_ARCHETYPE"
#define KG_MSG_FIN_NEUROMOD_AROUSAL     "FIN_NEUROMOD_AROUSAL"
#define KG_MSG_FIN_NEUROMOD_FATIGUE     "FIN_NEUROMOD_FATIGUE"

static int bridge_kg_publish(financial_neuromod_bridge_t* bridge,
                              const char* msg_type,
                              const void* payload,
                              size_t size) {
    if (bridge && bridge->kg_wiring && bridge->config.enable_kg_messaging) {
        bridge->stats.kg_messages_sent++;
        /* kg_wiring_publish would be called here */
        (void)msg_type; (void)payload; (void)size;
        return 0;
    }
    return 0;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* fin_neuromod_op_state_name(fin_neuromod_op_state_t state) {
    switch (state) {
        case FIN_NEUROMOD_OP_STATE_UNINITIALIZED: return "UNINITIALIZED";
        case FIN_NEUROMOD_OP_STATE_INITIALIZED:   return "INITIALIZED";
        case FIN_NEUROMOD_OP_STATE_ACTIVE:        return "ACTIVE";
        case FIN_NEUROMOD_OP_STATE_DEGRADED:      return "DEGRADED";
        case FIN_NEUROMOD_OP_STATE_ERROR:         return "ERROR";
        default: return "UNKNOWN";
    }
}

const char* fin_neuromod_event_name(fin_neuromod_event_type_t type) {
    switch (type) {
        case FIN_NEUROMOD_EVENT_NONE:             return "NONE";
        case FIN_NEUROMOD_EVENT_GAIN:             return "GAIN";
        case FIN_NEUROMOD_EVENT_LOSS:             return "LOSS";
        case FIN_NEUROMOD_EVENT_UNEXPECTED_GAIN:  return "UNEXPECTED_GAIN";
        case FIN_NEUROMOD_EVENT_UNEXPECTED_LOSS:  return "UNEXPECTED_LOSS";
        case FIN_NEUROMOD_EVENT_OPPORTUNITY:      return "OPPORTUNITY";
        case FIN_NEUROMOD_EVENT_MISSED_OPP:       return "MISSED_OPPORTUNITY";
        case FIN_NEUROMOD_EVENT_VOLATILITY_SPIKE: return "VOLATILITY_SPIKE";
        case FIN_NEUROMOD_EVENT_STRESS:           return "STRESS";
        case FIN_NEUROMOD_EVENT_COGNITIVE_EFFORT: return "COGNITIVE_EFFORT";
        case FIN_NEUROMOD_EVENT_REST:             return "REST";
        default: return "UNKNOWN";
    }
}

const char* fin_neuromod_arousal_name(fin_arousal_level_t level) {
    switch (level) {
        case FIN_AROUSAL_VERY_LOW:  return "VERY_LOW";
        case FIN_AROUSAL_LOW:       return "LOW";
        case FIN_AROUSAL_OPTIMAL:   return "OPTIMAL";
        case FIN_AROUSAL_HIGH:      return "HIGH";
        case FIN_AROUSAL_VERY_HIGH: return "VERY_HIGH";
        default: return "UNKNOWN";
    }
}

const char* fin_neuromod_fatigue_name(fin_fatigue_level_t level) {
    switch (level) {
        case FIN_FATIGUE_NONE:     return "NONE";
        case FIN_FATIGUE_MILD:     return "MILD";
        case FIN_FATIGUE_MODERATE: return "MODERATE";
        case FIN_FATIGUE_SEVERE:   return "SEVERE";
        case FIN_FATIGUE_CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

const char* financial_neuromod_bridge_version(void) {
    return FINANCIAL_NEUROMOD_BRIDGE_VERSION;
}

/* ============================================================================
 * Configuration
 * ============================================================================ */

int financial_neuromod_bridge_default_config(fin_neuromod_config_t* config) {
    if (!config) {
        set_error("config is NULL");
        return FIN_NEUROMOD_ERR_NULL;
    }

    memset(config, 0, sizeof(fin_neuromod_config_t));

    /* Baseline neuromodulator levels (neutral state) */
    config->baseline_dopamine = 0.5f;
    config->baseline_serotonin = 0.5f;
    config->baseline_norepinephrine = 0.4f;
    config->baseline_acetylcholine = 0.5f;
    config->baseline_adenosine = 0.2f;

    /* Decay rates (half-life ~30 seconds for fast neurotransmitters) */
    config->dopamine_decay = 0.02f;         /* DA: fast decay */
    config->serotonin_decay = 0.005f;       /* 5HT: slow decay */
    config->norepinephrine_decay = 0.015f;  /* NE: medium decay */
    config->acetylcholine_decay = 0.01f;    /* ACh: medium decay */
    config->adenosine_decay = 0.001f;       /* Adenosine: very slow decay (rest) */
    config->adenosine_accumulation = 0.002f; /* Accumulation during activity */

    /* Event sensitivity multipliers */
    config->gain_sensitivity = 1.0f;
    config->loss_sensitivity = 1.5f;  /* Loss aversion: losses feel ~1.5x gains */
    config->stress_sensitivity = 1.2f;
    config->effort_sensitivity = 0.5f;

    /* Effect computation weights */
    config->da_risk_weight = 0.6f;
    config->serotonin_risk_weight = 0.4f;
    config->serotonin_patience_weight = 0.8f;
    config->ach_learning_weight = 0.5f;
    config->da_learning_weight = 0.5f;
    config->ne_arousal_weight = 0.9f;
    config->adenosine_fatigue_weight = 0.9f;

    /* Arousal optimization (Yerkes-Dodson optimal is ~0.5-0.6) */
    config->optimal_arousal = 0.55f;
    config->arousal_tolerance = 0.15f;

    /* Fatigue thresholds */
    config->fatigue_mild_threshold = 0.3f;
    config->fatigue_moderate_threshold = 0.5f;
    config->fatigue_severe_threshold = 0.7f;
    config->fatigue_critical_threshold = 0.85f;

    /* Integration settings */
    config->enable_immune_integration = true;
    config->enable_bbb_validation = true;
    config->enable_kg_messaging = true;
    config->enable_health_monitoring = true;

    /* Logging */
    config->verbose_logging = false;

    return FIN_NEUROMOD_ERR_OK;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

financial_neuromod_bridge_t* financial_neuromod_bridge_create(
    const fin_neuromod_config_t* config)
{
    fin_neuromod_heartbeat_global("fin_neuromod_create", 0.0f);

    financial_neuromod_bridge_t* bridge = (financial_neuromod_bridge_t*)
        nimcp_malloc(sizeof(financial_neuromod_bridge_t));
    if (!bridge) {
        set_error("Failed to allocate financial_neuromod_bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate financial_neuromod_bridge");
        return NULL;
    }
    memset(bridge, 0, sizeof(financial_neuromod_bridge_t));

    bridge->magic = FINANCIAL_NEUROMOD_BRIDGE_MAGIC;

    /* Copy configuration or use defaults */
    if (config) {
        bridge->config = *config;
    } else {
        financial_neuromod_bridge_default_config(&bridge->config);
    }

    /* Initialize neuromodulator state to baseline */
    bridge->neuromod_state.dopamine = bridge->config.baseline_dopamine;
    bridge->neuromod_state.serotonin = bridge->config.baseline_serotonin;
    bridge->neuromod_state.norepinephrine = bridge->config.baseline_norepinephrine;
    bridge->neuromod_state.acetylcholine = bridge->config.baseline_acetylcholine;
    bridge->neuromod_state.adenosine = bridge->config.baseline_adenosine;

    bridge->op_state = FIN_NEUROMOD_OP_STATE_INITIALIZED;
    bridge->last_update_ms = 0;

    fin_neuromod_heartbeat_global("fin_neuromod_create", 1.0f);
    return bridge;
}

void financial_neuromod_bridge_destroy(financial_neuromod_bridge_t* bridge) {
    fin_neuromod_heartbeat_global("fin_neuromod_destroy", 0.0f);

    if (bridge) {
        bridge->magic = 0;
        bridge->op_state = FIN_NEUROMOD_OP_STATE_UNINITIALIZED;
        nimcp_free(bridge);
        bridge = NULL;
    }
}

int financial_neuromod_bridge_reset(financial_neuromod_bridge_t* bridge) {
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_neuromod_bridge_reset: bridge is NULL");
        return FIN_NEUROMOD_ERR_NULL;
    }

    if (bridge->magic != FINANCIAL_NEUROMOD_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_INVALID_STATE,
            "financial_neuromod_bridge_reset: invalid magic");
        return FIN_NEUROMOD_ERR_STATE;
    }

    fin_neuromod_heartbeat_global("fin_neuromod_reset", 0.0f);

    /* Reset stats */
    memset(&bridge->stats, 0, sizeof(fin_neuromod_bridge_stats_t));

    /* Reset neuromodulator state to baseline */
    bridge->neuromod_state.dopamine = bridge->config.baseline_dopamine;
    bridge->neuromod_state.serotonin = bridge->config.baseline_serotonin;
    bridge->neuromod_state.norepinephrine = bridge->config.baseline_norepinephrine;
    bridge->neuromod_state.acetylcholine = bridge->config.baseline_acetylcholine;
    bridge->neuromod_state.adenosine = bridge->config.baseline_adenosine;

    bridge->op_state = FIN_NEUROMOD_OP_STATE_INITIALIZED;
    bridge->last_update_ms = 0;

    fin_neuromod_heartbeat_global("fin_neuromod_reset", 1.0f);
    return FIN_NEUROMOD_ERR_OK;
}

/* ============================================================================
 * Subsystem Setters
 * ============================================================================ */

#define FIN_NEUROMOD_SETTER(name, field) \
    int financial_neuromod_bridge_set_##name( \
        financial_neuromod_bridge_t* bridge, void* ptr) { \
        if (!bridge) { \
            set_error("bridge is NULL in set_" #name); \
            NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, \
                "financial_neuromod_bridge_set_" #name ": bridge is NULL"); \
            return FIN_NEUROMOD_ERR_NULL; \
        } \
        if (bridge->magic != FINANCIAL_NEUROMOD_BRIDGE_MAGIC) { \
            set_error("Invalid bridge magic in set_" #name); \
            return FIN_NEUROMOD_ERR_STATE; \
        } \
        bridge->field = ptr; \
        return FIN_NEUROMOD_ERR_OK; \
    }

FIN_NEUROMOD_SETTER(immune, immune)
FIN_NEUROMOD_SETTER(health_agent, health_agent)
FIN_NEUROMOD_SETTER(kg_wiring, kg_wiring)
FIN_NEUROMOD_SETTER(logger, logger)
FIN_NEUROMOD_SETTER(security, security)
FIN_NEUROMOD_SETTER(bio_router, bio_router)

int financial_neuromod_bridge_set_coordinator(
    financial_neuromod_bridge_t* bridge,
    brain_cycle_coordinator_t* coordinator)
{
    if (!bridge) {
        set_error("bridge is NULL in set_coordinator");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_neuromod_bridge_set_coordinator: bridge is NULL");
        return FIN_NEUROMOD_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_NEUROMOD_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic in set_coordinator");
        return FIN_NEUROMOD_ERR_STATE;
    }
    bridge->coordinator = (void*)coordinator;
    return FIN_NEUROMOD_ERR_OK;
}

int financial_neuromod_bridge_set_bbb(
    financial_neuromod_bridge_t* bridge,
    bbb_system_t bbb)
{
    if (!bridge) {
        set_error("bridge is NULL in set_bbb");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_neuromod_bridge_set_bbb: bridge is NULL");
        return FIN_NEUROMOD_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_NEUROMOD_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic in set_bbb");
        return FIN_NEUROMOD_ERR_STATE;
    }
    bridge->bbb = (void*)bbb;
    return FIN_NEUROMOD_ERR_OK;
}

int financial_neuromod_bridge_set_ethics(
    financial_neuromod_bridge_t* bridge,
    ethics_engine_t ethics)
{
    if (!bridge) {
        set_error("bridge is NULL in set_ethics");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_neuromod_bridge_set_ethics: bridge is NULL");
        return FIN_NEUROMOD_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_NEUROMOD_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic in set_ethics");
        return FIN_NEUROMOD_ERR_STATE;
    }
    bridge->ethics = (void*)ethics;
    return FIN_NEUROMOD_ERR_OK;
}

int financial_neuromod_bridge_set_lgss(
    financial_neuromod_bridge_t* bridge,
    const void* lgss)
{
    if (!bridge) {
        set_error("bridge is NULL in set_lgss");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_neuromod_bridge_set_lgss: bridge is NULL");
        return FIN_NEUROMOD_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_NEUROMOD_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic in set_lgss");
        return FIN_NEUROMOD_ERR_STATE;
    }
    bridge->lgss = lgss;
    return FIN_NEUROMOD_ERR_OK;
}

/* ============================================================================
 * Callback Registration
 * ============================================================================ */

int financial_neuromod_bridge_set_state_callback(
    financial_neuromod_bridge_t* bridge,
    fin_neuromod_state_callback_t callback,
    void* user_data)
{
    if (!bridge) {
        set_error("bridge is NULL in set_state_callback");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_neuromod_bridge_set_state_callback: bridge is NULL");
        return FIN_NEUROMOD_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_NEUROMOD_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic in set_state_callback");
        return FIN_NEUROMOD_ERR_STATE;
    }
    bridge->state_callback = callback;
    bridge->state_callback_data = user_data;
    return FIN_NEUROMOD_ERR_OK;
}

int financial_neuromod_bridge_set_fatigue_callback(
    financial_neuromod_bridge_t* bridge,
    fin_neuromod_fatigue_callback_t callback,
    void* user_data)
{
    if (!bridge) {
        set_error("bridge is NULL in set_fatigue_callback");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_neuromod_bridge_set_fatigue_callback: bridge is NULL");
        return FIN_NEUROMOD_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_NEUROMOD_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic in set_fatigue_callback");
        return FIN_NEUROMOD_ERR_STATE;
    }
    bridge->fatigue_callback = callback;
    bridge->fatigue_callback_data = user_data;
    return FIN_NEUROMOD_ERR_OK;
}

int financial_neuromod_bridge_set_arousal_callback(
    financial_neuromod_bridge_t* bridge,
    fin_neuromod_arousal_callback_t callback,
    void* user_data)
{
    if (!bridge) {
        set_error("bridge is NULL in set_arousal_callback");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_neuromod_bridge_set_arousal_callback: bridge is NULL");
        return FIN_NEUROMOD_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_NEUROMOD_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic in set_arousal_callback");
        return FIN_NEUROMOD_ERR_STATE;
    }
    bridge->arousal_callback = callback;
    bridge->arousal_callback_data = user_data;
    return FIN_NEUROMOD_ERR_OK;
}

/* ============================================================================
 * Core API - Neuromodulator State Updates
 * ============================================================================ */

int financial_neuromod_bridge_update(
    financial_neuromod_bridge_t* bridge,
    const fin_neuromod_event_t* event)
{
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_neuromod_bridge_update: bridge is NULL");
        return FIN_NEUROMOD_ERR_NULL;
    }
    if (!event) {
        set_error("event is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_neuromod_bridge_update: event is NULL");
        return FIN_NEUROMOD_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_NEUROMOD_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_NEUROMOD_ERR_STATE;
    }

    fin_neuromod_heartbeat_global("fin_neuromod_update", 0.0f);

    /* BBB validation if enabled */
    if (bridge->config.enable_bbb_validation && bridge->bbb) {
        bridge->stats.bbb_validations++;
        int rc = bbb_validate_data((bbb_system_t)bridge->bbb, event,
                                    sizeof(*event), "neuromod_update");
        if (rc != 0) {
            set_error("BBB validation failed for neuromod_update");
            return FIN_NEUROMOD_ERR_BBB;
        }
    }

    /* Immune check if enabled */
    if (bridge->config.enable_immune_integration && bridge->immune) {
        bridge->stats.immune_checks++;
        int rc = brain_immune_validate_operation(
            (brain_immune_system_t*)bridge->immune,
            "neuromod_update", 5);
        if (rc != 0) {
            set_error("Immune validation failed for neuromod_update");
            return FIN_NEUROMOD_ERR_IMMUNE;
        }
    }

    /* Store old state for callback */
    fin_neuromod_state_t old_state = bridge->neuromod_state;

    /* Compute neuromodulator deltas based on event type */
    float mag = nimcp_clampf(event->magnitude, 0.0f, 1.0f);
    float pe = nimcp_clampf(event->prediction_error, -1.0f, 1.0f);

    float delta_da = 0.0f;
    float delta_5ht = 0.0f;
    float delta_ne = 0.0f;
    float delta_ach = 0.0f;
    float delta_adenosine = 0.0f;

    switch (event->type) {
        case FIN_NEUROMOD_EVENT_GAIN:
            /* Dopamine burst on reward, mild serotonin boost */
            delta_da = mag * bridge->config.gain_sensitivity * 0.3f;
            delta_5ht = mag * 0.1f;
            delta_ach = mag * 0.1f;  /* Enhanced attention on success */
            break;

        case FIN_NEUROMOD_EVENT_LOSS:
            /* Dopamine dip on loss, serotonin depletion, NE spike */
            delta_da = -mag * bridge->config.loss_sensitivity * 0.4f;
            delta_5ht = -mag * bridge->config.loss_sensitivity * 0.2f;
            delta_ne = mag * 0.3f;  /* Stress response */
            break;

        case FIN_NEUROMOD_EVENT_UNEXPECTED_GAIN:
            /* Large dopamine burst (positive prediction error) */
            delta_da = (mag + pe * 0.5f) * bridge->config.gain_sensitivity * 0.5f;
            delta_5ht = mag * 0.15f;
            delta_ach = mag * 0.2f;  /* Enhanced learning */
            break;

        case FIN_NEUROMOD_EVENT_UNEXPECTED_LOSS:
            /* Large dopamine dip (negative prediction error), stress response */
            delta_da = (-mag + pe * 0.5f) * bridge->config.loss_sensitivity * 0.5f;
            delta_5ht = -mag * bridge->config.loss_sensitivity * 0.3f;
            delta_ne = mag * 0.4f;  /* Strong stress response */
            delta_adenosine = mag * 0.1f;  /* Fatigue from stress */
            break;

        case FIN_NEUROMOD_EVENT_OPPORTUNITY:
            /* Anticipatory dopamine, mild NE for alertness */
            delta_da = mag * 0.25f;
            delta_ne = mag * 0.2f;
            delta_ach = mag * 0.15f;
            break;

        case FIN_NEUROMOD_EVENT_MISSED_OPP:
            /* Dopamine dip, regret response */
            delta_da = -mag * 0.3f;
            delta_5ht = -mag * 0.15f;
            delta_ne = mag * 0.15f;
            break;

        case FIN_NEUROMOD_EVENT_VOLATILITY_SPIKE:
            /* Strong NE spike (arousal), DA depending on position */
            delta_ne = mag * 0.4f;
            delta_da = pe * mag * 0.2f;  /* Depends on whether positioned well */
            delta_5ht = -mag * 0.1f;  /* Slight mood dip from uncertainty */
            break;

        case FIN_NEUROMOD_EVENT_STRESS:
            /* Generalized stress response */
            delta_5ht = -mag * bridge->config.stress_sensitivity * 0.3f;
            delta_ne = mag * bridge->config.stress_sensitivity * 0.35f;
            delta_adenosine = mag * bridge->config.stress_sensitivity * 0.15f;
            break;

        case FIN_NEUROMOD_EVENT_COGNITIVE_EFFORT:
            /* ACh boost for attention, adenosine accumulation */
            delta_ach = mag * 0.2f;
            delta_adenosine = mag * bridge->config.effort_sensitivity * 0.3f;
            delta_ne = mag * 0.1f;  /* Mild arousal */
            break;

        case FIN_NEUROMOD_EVENT_REST:
            /* Recovery: boost serotonin, clear adenosine */
            delta_5ht = mag * 0.2f;
            delta_adenosine = -mag * 0.4f;  /* Adenosine clearance */
            delta_ne = -mag * 0.15f;  /* Relax */
            break;

        case FIN_NEUROMOD_EVENT_NONE:
        default:
            /* No change */
            break;
    }

    /* Apply deltas and clamp to [0, 1] */
    bridge->neuromod_state.dopamine = nimcp_clampf(
        bridge->neuromod_state.dopamine + delta_da, 0.0f, 1.0f);
    bridge->neuromod_state.serotonin = nimcp_clampf(
        bridge->neuromod_state.serotonin + delta_5ht, 0.0f, 1.0f);
    bridge->neuromod_state.norepinephrine = nimcp_clampf(
        bridge->neuromod_state.norepinephrine + delta_ne, 0.0f, 1.0f);
    bridge->neuromod_state.acetylcholine = nimcp_clampf(
        bridge->neuromod_state.acetylcholine + delta_ach, 0.0f, 1.0f);
    bridge->neuromod_state.adenosine = nimcp_clampf(
        bridge->neuromod_state.adenosine + delta_adenosine, 0.0f, 1.0f);

    /* Update state */
    bridge->op_state = FIN_NEUROMOD_OP_STATE_ACTIVE;
    bridge->last_update_ms = event->timestamp_ms;
    bridge->stats.state_updates++;

    /* Fire state callback if registered */
    if (bridge->state_callback) {
        bridge->state_callback(&old_state, &bridge->neuromod_state,
                               event, bridge->state_callback_data);
    }

    /* Check fatigue and fire callback if needed */
    if (bridge->fatigue_callback &&
        bridge->neuromod_state.adenosine >= bridge->config.fatigue_moderate_threshold) {
        fin_fatigue_result_t fatigue;
        financial_neuromod_bridge_analyze_fatigue(bridge, &fatigue);
        bridge->fatigue_callback(&fatigue, bridge->fatigue_callback_data);
    }

    /* Check arousal and fire callback if outside optimal range */
    if (bridge->arousal_callback) {
        float arousal = bridge->neuromod_state.norepinephrine *
                        bridge->config.ne_arousal_weight;
        float dist = fabsf(arousal - bridge->config.optimal_arousal);
        if (dist > bridge->config.arousal_tolerance) {
            fin_arousal_result_t arousal_result;
            financial_neuromod_bridge_analyze_arousal(bridge, &arousal_result);
            bridge->arousal_callback(&arousal_result, bridge->arousal_callback_data);
        }
    }

    /* KG messaging */
    bridge_kg_publish(bridge, KG_MSG_FIN_NEUROMOD_UPDATE,
                      &bridge->neuromod_state, sizeof(bridge->neuromod_state));

    fin_neuromod_heartbeat_global("fin_neuromod_update", 1.0f);
    return FIN_NEUROMOD_ERR_OK;
}

int financial_neuromod_bridge_decay(
    financial_neuromod_bridge_t* bridge,
    uint64_t elapsed_ms)
{
    if (!bridge) {
        set_error("bridge is NULL");
        return FIN_NEUROMOD_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_NEUROMOD_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_NEUROMOD_ERR_STATE;
    }

    /* Convert elapsed time to seconds for decay calculations */
    float dt = (float)elapsed_ms / 1000.0f;

    /* Apply exponential decay toward baseline for each neuromodulator */
    bridge->neuromod_state.dopamine = decay_toward(
        bridge->neuromod_state.dopamine,
        bridge->config.baseline_dopamine,
        bridge->config.dopamine_decay, dt);

    bridge->neuromod_state.serotonin = decay_toward(
        bridge->neuromod_state.serotonin,
        bridge->config.baseline_serotonin,
        bridge->config.serotonin_decay, dt);

    bridge->neuromod_state.norepinephrine = decay_toward(
        bridge->neuromod_state.norepinephrine,
        bridge->config.baseline_norepinephrine,
        bridge->config.norepinephrine_decay, dt);

    bridge->neuromod_state.acetylcholine = decay_toward(
        bridge->neuromod_state.acetylcholine,
        bridge->config.baseline_acetylcholine,
        bridge->config.acetylcholine_decay, dt);

    /* Adenosine: slowly accumulates during activity, decays during rest
     * Here we apply slow accumulation (can be overridden by REST events) */
    bridge->neuromod_state.adenosine = nimcp_clampf(
        bridge->neuromod_state.adenosine +
        bridge->config.adenosine_accumulation * dt,
        0.0f, 1.0f);

    return FIN_NEUROMOD_ERR_OK;
}

int financial_neuromod_bridge_get_state(
    const financial_neuromod_bridge_t* bridge,
    fin_neuromod_state_t* out_state)
{
    if (!bridge) {
        set_error("bridge is NULL");
        return FIN_NEUROMOD_ERR_NULL;
    }
    if (!out_state) {
        set_error("out_state is NULL");
        return FIN_NEUROMOD_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_NEUROMOD_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_NEUROMOD_ERR_STATE;
    }

    *out_state = bridge->neuromod_state;
    return FIN_NEUROMOD_ERR_OK;
}

int financial_neuromod_bridge_set_state(
    financial_neuromod_bridge_t* bridge,
    const fin_neuromod_state_t* state)
{
    if (!bridge) {
        set_error("bridge is NULL");
        return FIN_NEUROMOD_ERR_NULL;
    }
    if (!state) {
        set_error("state is NULL");
        return FIN_NEUROMOD_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_NEUROMOD_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_NEUROMOD_ERR_STATE;
    }

    /* Validate ranges */
    if (state->dopamine < 0.0f || state->dopamine > 1.0f ||
        state->serotonin < 0.0f || state->serotonin > 1.0f ||
        state->norepinephrine < 0.0f || state->norepinephrine > 1.0f ||
        state->acetylcholine < 0.0f || state->acetylcholine > 1.0f ||
        state->adenosine < 0.0f || state->adenosine > 1.0f) {
        set_error("Neuromodulator values must be in [0, 1] range");
        return FIN_NEUROMOD_ERR_RANGE;
    }

    bridge->neuromod_state = *state;
    bridge->op_state = FIN_NEUROMOD_OP_STATE_ACTIVE;
    return FIN_NEUROMOD_ERR_OK;
}

/* ============================================================================
 * Core API - Effect Computation
 * ============================================================================ */

int financial_neuromod_bridge_compute_effects(
    financial_neuromod_bridge_t* bridge,
    fin_neuromod_effects_t* out_effects)
{
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_neuromod_bridge_compute_effects: bridge is NULL");
        return FIN_NEUROMOD_ERR_NULL;
    }
    if (!out_effects) {
        set_error("out_effects is NULL");
        return FIN_NEUROMOD_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_NEUROMOD_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_NEUROMOD_ERR_STATE;
    }

    return financial_neuromod_bridge_compute_effects_from_state(
        bridge, &bridge->neuromod_state, out_effects);
}

int financial_neuromod_bridge_compute_effects_from_state(
    financial_neuromod_bridge_t* bridge,
    const fin_neuromod_state_t* state,
    fin_neuromod_effects_t* out_effects)
{
    if (!bridge) {
        set_error("bridge is NULL");
        return FIN_NEUROMOD_ERR_NULL;
    }
    if (!state || !out_effects) {
        set_error("state or out_effects is NULL");
        return FIN_NEUROMOD_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_NEUROMOD_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_NEUROMOD_ERR_STATE;
    }

    fin_neuromod_heartbeat_global("fin_neuromod_compute", 0.0f);

    memset(out_effects, 0, sizeof(fin_neuromod_effects_t));

    /* Risk Tolerance: Modulated by dopamine (+) and serotonin (stabilizing)
     * High DA -> increased risk-seeking
     * High 5HT -> more stable, less extreme risk-taking
     */
    float da_contrib = (state->dopamine - 0.5f) * 2.0f;  /* Map [0,1] to [-1,1] */
    float sht_contrib = state->serotonin;  /* 5HT provides stability */

    out_effects->risk_tolerance = nimcp_clampf(
        0.5f +
        da_contrib * bridge->config.da_risk_weight * 0.3f +
        (sht_contrib - 0.5f) * bridge->config.serotonin_risk_weight * 0.2f,
        0.0f, 1.0f);

    /* Patience: Primarily modulated by serotonin
     * High 5HT -> more patient, can delay gratification
     * Low 5HT -> impulsive, wants immediate rewards
     */
    out_effects->patience = nimcp_clampf(
        state->serotonin * bridge->config.serotonin_patience_weight +
        (1.0f - state->adenosine) * 0.2f,  /* Fatigue reduces patience */
        0.0f, 1.0f);

    /* Learning Rate: Modulated by acetylcholine and dopamine
     * High ACh -> enhanced attention and encoding
     * High DA -> enhanced plasticity, reward-based learning
     */
    out_effects->learning_rate = nimcp_clampf(
        state->acetylcholine * bridge->config.ach_learning_weight +
        state->dopamine * bridge->config.da_learning_weight,
        0.0f, 1.0f);

    /* Arousal Level: Primarily modulated by norepinephrine
     * Following Yerkes-Dodson: optimal performance at medium arousal
     */
    out_effects->arousal_level = nimcp_clampf(
        state->norepinephrine * bridge->config.ne_arousal_weight,
        0.0f, 1.0f);

    /* Fatigue Level: Primarily modulated by adenosine
     * High adenosine -> high fatigue, need for rest
     */
    out_effects->fatigue_level = nimcp_clampf(
        state->adenosine * bridge->config.adenosine_fatigue_weight,
        0.0f, 1.0f);

    /* Update stats */
    bridge->stats.effect_computations++;

    /* KG messaging */
    bridge_kg_publish(bridge, KG_MSG_FIN_NEUROMOD_EFFECTS,
                      out_effects, sizeof(*out_effects));

    fin_neuromod_heartbeat_global("fin_neuromod_compute", 1.0f);
    return FIN_NEUROMOD_ERR_OK;
}

/* ============================================================================
 * Core API - Archetype Modulation
 * ============================================================================ */

int financial_neuromod_bridge_modulate_archetype(
    financial_neuromod_bridge_t* bridge,
    const fin_archetype_params_t* base_params,
    fin_archetype_modulation_t* out_modulated)
{
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_neuromod_bridge_modulate_archetype: bridge is NULL");
        return FIN_NEUROMOD_ERR_NULL;
    }
    if (!base_params || !out_modulated) {
        set_error("base_params or out_modulated is NULL");
        return FIN_NEUROMOD_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_NEUROMOD_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_NEUROMOD_ERR_STATE;
    }

    fin_neuromod_heartbeat_global("fin_neuromod_modulate", 0.0f);

    /* Compute current effects */
    fin_neuromod_effects_t effects;
    int rc = financial_neuromod_bridge_compute_effects(bridge, &effects);
    if (rc != FIN_NEUROMOD_ERR_OK) {
        return rc;
    }

    memset(out_modulated, 0, sizeof(fin_archetype_modulation_t));

    /* Modulate risk tolerance
     * Base is shifted toward effect level with a blending factor
     */
    float risk_blend = 0.4f;  /* How much neuromod affects base */
    out_modulated->modulated_risk_tolerance = nimcp_clampf(
        lerpf(base_params->base_risk_tolerance, effects.risk_tolerance, risk_blend),
        0.0f, 1.0f);

    /* Modulate patience
     * Low patience from neuromod can override even patient archetypes
     */
    float patience_blend = 0.35f;
    out_modulated->modulated_patience = nimcp_clampf(
        base_params->base_patience * (0.5f + effects.patience * 0.5f) *
        (1.0f - effects.fatigue_level * 0.3f),  /* Fatigue reduces patience */
        0.0f, 1.0f);

    /* Modulate learning rate
     * Neuromod can boost or dampen archetype's base learning rate
     */
    float learning_blend = 0.5f;
    out_modulated->modulated_learning_rate = nimcp_clampf(
        lerpf(base_params->base_learning_rate, effects.learning_rate, learning_blend),
        0.0f, 1.0f);

    /* Modulate concentration (inverse of risk tolerance effect)
     * High arousal -> reduced concentration (scattered attention)
     * Fatigue -> reduced concentration
     */
    float concentration_penalty = 0.0f;
    float arousal_dist = fabsf(effects.arousal_level - bridge->config.optimal_arousal);
    if (arousal_dist > bridge->config.arousal_tolerance) {
        concentration_penalty = (arousal_dist - bridge->config.arousal_tolerance) * 0.5f;
    }
    concentration_penalty += effects.fatigue_level * 0.3f;

    out_modulated->modulated_concentration = nimcp_clampf(
        base_params->base_concentration * (1.0f - concentration_penalty),
        0.0f, 1.0f);

    /* Modulate contrarian tendency
     * Low serotonin -> more reactive to crowd, less contrarian
     * High dopamine + stress -> may flip to follow crowd (panic)
     */
    float contrarian_mod = 1.0f;
    if (bridge->neuromod_state.serotonin < 0.3f) {
        contrarian_mod -= (0.3f - bridge->neuromod_state.serotonin) * 0.5f;
    }
    if (bridge->neuromod_state.norepinephrine > 0.7f &&
        bridge->neuromod_state.serotonin < 0.4f) {
        /* Panic state: contrarian becomes follower */
        contrarian_mod *= 0.5f;
    }

    out_modulated->modulated_contrarian = nimcp_clampf(
        base_params->base_contrarian_tendency * contrarian_mod,
        0.0f, 1.0f);

    /* Compute overall modulation factor (how much change occurred) */
    float total_change = 0.0f;
    total_change += fabsf(out_modulated->modulated_risk_tolerance -
                          base_params->base_risk_tolerance);
    total_change += fabsf(out_modulated->modulated_patience -
                          base_params->base_patience);
    total_change += fabsf(out_modulated->modulated_learning_rate -
                          base_params->base_learning_rate);
    total_change += fabsf(out_modulated->modulated_concentration -
                          base_params->base_concentration);
    total_change += fabsf(out_modulated->modulated_contrarian -
                          base_params->base_contrarian_tendency);

    out_modulated->overall_modulation_factor = nimcp_clampf(total_change / 5.0f, 0.0f, 1.0f);

    /* Generate summary */
    snprintf(out_modulated->modulation_summary, FIN_NEUROMOD_DESC_LEN,
             "Risk:%.2f Pat:%.2f Lrn:%.2f Conc:%.2f Contr:%.2f",
             out_modulated->modulated_risk_tolerance,
             out_modulated->modulated_patience,
             out_modulated->modulated_learning_rate,
             out_modulated->modulated_concentration,
             out_modulated->modulated_contrarian);

    /* Update stats */
    bridge->stats.archetype_modulations++;

    /* KG messaging */
    bridge_kg_publish(bridge, KG_MSG_FIN_NEUROMOD_ARCHETYPE,
                      out_modulated, sizeof(*out_modulated));

    fin_neuromod_heartbeat_global("fin_neuromod_modulate", 1.0f);
    return FIN_NEUROMOD_ERR_OK;
}

/* ============================================================================
 * Analysis API
 * ============================================================================ */

int financial_neuromod_bridge_analyze_arousal(
    financial_neuromod_bridge_t* bridge,
    fin_arousal_result_t* out_arousal)
{
    if (!bridge) {
        set_error("bridge is NULL");
        return FIN_NEUROMOD_ERR_NULL;
    }
    if (!out_arousal) {
        set_error("out_arousal is NULL");
        return FIN_NEUROMOD_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_NEUROMOD_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_NEUROMOD_ERR_STATE;
    }

    memset(out_arousal, 0, sizeof(fin_arousal_result_t));

    /* Compute raw arousal from norepinephrine */
    float arousal = bridge->neuromod_state.norepinephrine *
                    bridge->config.ne_arousal_weight;

    out_arousal->raw_arousal = arousal;
    out_arousal->optimal_distance = fabsf(arousal - bridge->config.optimal_arousal);

    /* Yerkes-Dodson performance curve (inverted U)
     * Performance peaks at optimal arousal
     */
    float norm_dist = out_arousal->optimal_distance / bridge->config.optimal_arousal;
    out_arousal->performance_factor = nimcp_clampf(1.0f - norm_dist * norm_dist, 0.3f, 1.0f);

    /* Categorize arousal level */
    if (arousal < 0.2f) {
        out_arousal->level = FIN_AROUSAL_VERY_LOW;
        snprintf(out_arousal->recommendation, FIN_NEUROMOD_DESC_LEN,
                 "Arousal very low - consider stimulating activity");
    } else if (arousal < 0.35f) {
        out_arousal->level = FIN_AROUSAL_LOW;
        snprintf(out_arousal->recommendation, FIN_NEUROMOD_DESC_LEN,
                 "Arousal low - may need to increase engagement");
    } else if (arousal <= bridge->config.optimal_arousal + bridge->config.arousal_tolerance) {
        out_arousal->level = FIN_AROUSAL_OPTIMAL;
        snprintf(out_arousal->recommendation, FIN_NEUROMOD_DESC_LEN,
                 "Arousal optimal - good state for decision-making");
    } else if (arousal < 0.8f) {
        out_arousal->level = FIN_AROUSAL_HIGH;
        snprintf(out_arousal->recommendation, FIN_NEUROMOD_DESC_LEN,
                 "Arousal elevated - consider calming techniques");
    } else {
        out_arousal->level = FIN_AROUSAL_VERY_HIGH;
        snprintf(out_arousal->recommendation, FIN_NEUROMOD_DESC_LEN,
                 "Arousal very high - defer major decisions");
    }

    /* KG messaging */
    bridge_kg_publish(bridge, KG_MSG_FIN_NEUROMOD_AROUSAL,
                      out_arousal, sizeof(*out_arousal));

    return FIN_NEUROMOD_ERR_OK;
}

int financial_neuromod_bridge_analyze_fatigue(
    financial_neuromod_bridge_t* bridge,
    fin_fatigue_result_t* out_fatigue)
{
    if (!bridge) {
        set_error("bridge is NULL");
        return FIN_NEUROMOD_ERR_NULL;
    }
    if (!out_fatigue) {
        set_error("out_fatigue is NULL");
        return FIN_NEUROMOD_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_NEUROMOD_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_NEUROMOD_ERR_STATE;
    }

    memset(out_fatigue, 0, sizeof(fin_fatigue_result_t));

    /* Compute raw fatigue from adenosine */
    float fatigue = bridge->neuromod_state.adenosine *
                    bridge->config.adenosine_fatigue_weight;

    out_fatigue->raw_fatigue = fatigue;
    out_fatigue->cognitive_capacity = nimcp_clampf(1.0f - fatigue, 0.0f, 1.0f);
    out_fatigue->rest_urgency = nimcp_clampf(fatigue * 1.2f, 0.0f, 1.0f);

    /* Categorize fatigue level */
    if (fatigue < bridge->config.fatigue_mild_threshold) {
        out_fatigue->level = FIN_FATIGUE_NONE;
        out_fatigue->recommended_rest_min = 0;
        snprintf(out_fatigue->recommendation, FIN_NEUROMOD_DESC_LEN,
                 "No significant fatigue - continue as normal");
    } else if (fatigue < bridge->config.fatigue_moderate_threshold) {
        out_fatigue->level = FIN_FATIGUE_MILD;
        out_fatigue->recommended_rest_min = 5;
        snprintf(out_fatigue->recommendation, FIN_NEUROMOD_DESC_LEN,
                 "Mild fatigue - consider a short break soon");
    } else if (fatigue < bridge->config.fatigue_severe_threshold) {
        out_fatigue->level = FIN_FATIGUE_MODERATE;
        out_fatigue->recommended_rest_min = 15;
        snprintf(out_fatigue->recommendation, FIN_NEUROMOD_DESC_LEN,
                 "Moderate fatigue - take a 15-minute break");
    } else if (fatigue < bridge->config.fatigue_critical_threshold) {
        out_fatigue->level = FIN_FATIGUE_SEVERE;
        out_fatigue->recommended_rest_min = 30;
        snprintf(out_fatigue->recommendation, FIN_NEUROMOD_DESC_LEN,
                 "Severe fatigue - stop trading, rest 30+ minutes");
    } else {
        out_fatigue->level = FIN_FATIGUE_CRITICAL;
        out_fatigue->recommended_rest_min = 60;
        snprintf(out_fatigue->recommendation, FIN_NEUROMOD_DESC_LEN,
                 "CRITICAL: Cognitive impairment - stop immediately");
    }

    /* KG messaging */
    bridge_kg_publish(bridge, KG_MSG_FIN_NEUROMOD_FATIGUE,
                      out_fatigue, sizeof(*out_fatigue));

    return FIN_NEUROMOD_ERR_OK;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

fin_neuromod_op_state_t financial_neuromod_bridge_get_op_state(
    const financial_neuromod_bridge_t* bridge)
{
    if (!bridge || bridge->magic != FINANCIAL_NEUROMOD_BRIDGE_MAGIC) {
        return FIN_NEUROMOD_OP_STATE_UNINITIALIZED;
    }
    return bridge->op_state;
}

int financial_neuromod_bridge_get_stats(
    const financial_neuromod_bridge_t* bridge,
    fin_neuromod_bridge_stats_t* stats)
{
    if (!bridge) {
        set_error("bridge is NULL");
        return FIN_NEUROMOD_ERR_NULL;
    }
    if (!stats) {
        set_error("stats is NULL");
        return FIN_NEUROMOD_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_NEUROMOD_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_NEUROMOD_ERR_STATE;
    }

    *stats = bridge->stats;
    return FIN_NEUROMOD_ERR_OK;
}

void financial_neuromod_bridge_reset_stats(financial_neuromod_bridge_t* bridge) {
    if (bridge && bridge->magic == FINANCIAL_NEUROMOD_BRIDGE_MAGIC) {
        memset(&bridge->stats, 0, sizeof(fin_neuromod_bridge_stats_t));
    }
}

/* ============================================================================
 * Health Integration
 * ============================================================================ */

int financial_neuromod_bridge_heartbeat(
    financial_neuromod_bridge_t* bridge,
    const char* operation,
    float progress)
{
    if (!bridge) {
        return FIN_NEUROMOD_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_NEUROMOD_BRIDGE_MAGIC) {
        return FIN_NEUROMOD_ERR_STATE;
    }

    /* Forward to global health agent */
    fin_neuromod_heartbeat_global(
        operation ? operation : "fin_neuromod_heartbeat", progress);

    /* Forward to instance-level health agent */
    if (bridge->health_agent) {
        nimcp_health_agent_heartbeat_ex(
            (nimcp_health_agent_t*)bridge->health_agent, operation, progress);
    }

    bridge->stats.health_heartbeats++;
    return FIN_NEUROMOD_ERR_OK;
}
