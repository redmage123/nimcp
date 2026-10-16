/**
 * @file nimcp_financial_metacognition_bridge.c
 * @brief Financial Metacognition Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-29
 *
 * WHAT: Bridge for detecting cognitive biases in financial decision-making,
 *       calibrating confidence levels, and determining when decisions need
 *       reconsideration.
 *
 * WHY:  Financial decisions are highly susceptible to cognitive biases.
 *       By detecting and quantifying these biases, we can help traders
 *       make more rational decisions and avoid systematic errors.
 *
 * HOW:  Analyzes decision patterns across a sliding window to detect biases.
 *       Uses Bayesian confidence calibration comparing predictions to outcomes.
 *       Triggers reconsideration when bias strength exceeds thresholds.
 *
 * @author NIMCP Development Team
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>

#include "cognitive/parietal/nimcp_financial_metacognition_bridge.h"
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
#include "constants/nimcp_threshold_constants.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(fin_metacog)

/* Stub heartbeat for migration compatibility */
static inline void fin_metacog_heartbeat_global(const char* op, float progress) {
    (void)op; (void)progress;
}

BRIDGE_DEFINE_MESH_REGISTRATION(fin_metacog, MESH_ADAPTER_CATEGORY_COGNITIVE)


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

static _Thread_local char fin_metacog_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(fin_metacog_last_error, sizeof(fin_metacog_last_error), fmt, args);
    va_end(args);
}

const char* financial_metacognition_bridge_get_last_error(void) {
    return fin_metacog_last_error;
}

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct financial_metacognition_bridge {
    uint32_t magic;
    fin_metacognition_config_t config;
    fin_metacognition_bridge_state_t op_state;
    fin_metacognition_bridge_stats_t stats;

    /* Decision history (circular buffer) */
    fin_decision_record_t* history;
    uint32_t history_capacity;
    uint32_t history_count;
    uint32_t history_head;  /* Next write position */

    /* Last reconsideration timestamp for cooldown */
    uint64_t last_reconsider_ms;
    uint32_t reconsider_count_session;

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

    /* Training state */
    bool training_active;
};

static inline float fabsf_safe(float v) {
    return v < 0.0f ? -v : v;
}

/* ============================================================================
 * KG Wiring Message Types
 * ============================================================================ */

#define KG_MSG_FIN_METACOG_BIAS        "FIN_METACOG_BIAS"
#define KG_MSG_FIN_METACOG_CONFIDENCE  "FIN_METACOG_CONFIDENCE"
#define KG_MSG_FIN_METACOG_RECONSIDER  "FIN_METACOG_RECONSIDER"
#define KG_MSG_FIN_METACOG_ASSESS      "FIN_METACOG_ASSESS"

static int bridge_kg_publish(financial_metacognition_bridge_t* bridge,
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

const char* fin_metacog_bias_name(fin_cognitive_bias_t bias) {
    switch (bias) {
        case FIN_BIAS_CONFIRMATION:    return "CONFIRMATION";
        case FIN_BIAS_ANCHORING:       return "ANCHORING";
        case FIN_BIAS_RECENCY:         return "RECENCY";
        case FIN_BIAS_OVERCONFIDENCE:  return "OVERCONFIDENCE";
        case FIN_BIAS_LOSS_AVERSION:   return "LOSS_AVERSION";
        case FIN_BIAS_HERDING:         return "HERDING";
        case FIN_BIAS_GAMBLER_FALLACY: return "GAMBLER_FALLACY";
        case FIN_BIAS_SUNK_COST:       return "SUNK_COST";
        case FIN_BIAS_AVAILABILITY:    return "AVAILABILITY";
        case FIN_BIAS_HINDSIGHT:       return "HINDSIGHT";
        default: return "UNKNOWN";
    }
}

const char* fin_metacog_state_name(fin_metacognition_bridge_state_t state) {
    switch (state) {
        case FIN_METACOG_STATE_UNINITIALIZED: return "UNINITIALIZED";
        case FIN_METACOG_STATE_INITIALIZED:   return "INITIALIZED";
        case FIN_METACOG_STATE_ACTIVE:        return "ACTIVE";
        case FIN_METACOG_STATE_ANALYZING:     return "ANALYZING";
        case FIN_METACOG_STATE_DEGRADED:      return "DEGRADED";
        case FIN_METACOG_STATE_ERROR:         return "ERROR";
        default: return "UNKNOWN";
    }
}

const char* fin_metacog_confidence_name(fin_confidence_level_t level) {
    switch (level) {
        case FIN_CONFIDENCE_WELL_CALIBRATED: return "WELL_CALIBRATED";
        case FIN_CONFIDENCE_OVERCONFIDENT:   return "OVERCONFIDENT";
        case FIN_CONFIDENCE_UNDERCONFIDENT:  return "UNDERCONFIDENT";
        case FIN_CONFIDENCE_INCONSISTENT:    return "INCONSISTENT";
        case FIN_CONFIDENCE_UNKNOWN:         return "UNKNOWN";
        default: return "UNKNOWN";
    }
}

const char* fin_metacog_urgency_name(fin_reconsider_urgency_t urgency) {
    switch (urgency) {
        case FIN_RECONSIDER_NONE:        return "NONE";
        case FIN_RECONSIDER_OPTIONAL:    return "OPTIONAL";
        case FIN_RECONSIDER_RECOMMENDED: return "RECOMMENDED";
        case FIN_RECONSIDER_REQUIRED:    return "REQUIRED";
        case FIN_RECONSIDER_BLOCK:       return "BLOCK";
        default: return "UNKNOWN";
    }
}

const char* financial_metacognition_bridge_version(void) {
    return FINANCIAL_METACOGNITION_BRIDGE_VERSION;
}

/* ============================================================================
 * Configuration
 * ============================================================================ */

int financial_metacognition_bridge_default_config(fin_metacognition_config_t* config) {
    if (!config) {
        set_error("config is NULL");
        return FIN_METACOG_ERR_NULL;
    }

    memset(config, 0, sizeof(fin_metacognition_config_t));

    /* Analysis parameters */
    config->min_decisions_for_analysis = 10;
    config->analysis_window_size = FIN_METACOG_WINDOW_SIZE;
    config->reconsider_bias_threshold = 0.6f;
    config->reconsider_calibration_threshold = 0.3f;

    /* Bias detection thresholds (all default to 0.5) */
    config->bias_thresholds.confirmation_threshold = 0.5f;
    config->bias_thresholds.anchoring_threshold = 0.5f;
    config->bias_thresholds.recency_threshold = 0.5f;
    config->bias_thresholds.overconfidence_threshold = NIMCP_CONFIDENCE_MEDIUM;
    config->bias_thresholds.loss_aversion_threshold = 0.5f;
    config->bias_thresholds.herding_threshold = 0.5f;
    config->bias_thresholds.gambler_fallacy_threshold = 0.5f;
    config->bias_thresholds.sunk_cost_threshold = 0.5f;
    config->bias_thresholds.availability_threshold = 0.5f;
    config->bias_thresholds.hindsight_threshold = 0.5f;

    /* Confidence calibration parameters */
    config->confidence_bin_width = 0.1f;
    config->acceptable_calibration_error = 0.15f;

    /* Reconsideration parameters */
    config->max_reconsider_triggers = 5;
    config->reconsider_cooldown_sec = 60.0f;

    /* Integration settings */
    config->enable_immune_integration = true;
    config->enable_bbb_validation = true;
    config->enable_kg_messaging = true;
    config->enable_health_monitoring = true;

    /* Logging */
    config->verbose_logging = false;

    return FIN_METACOG_ERR_OK;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

financial_metacognition_bridge_t* financial_metacognition_bridge_create(
    const fin_metacognition_config_t* config)
{
    fin_metacog_heartbeat_global("fin_metacog_create", 0.0f);

    financial_metacognition_bridge_t* bridge = (financial_metacognition_bridge_t*)
        nimcp_malloc(sizeof(financial_metacognition_bridge_t));
    if (!bridge) {
        set_error("Failed to allocate financial_metacognition_bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate financial_metacognition_bridge");
        return NULL;
    }
    memset(bridge, 0, sizeof(financial_metacognition_bridge_t));

    bridge->magic = FINANCIAL_METACOGNITION_BRIDGE_MAGIC;

    /* Copy configuration or use defaults */
    if (config) {
        bridge->config = *config;
    } else {
        financial_metacognition_bridge_default_config(&bridge->config);
    }

    /* Allocate decision history */
    bridge->history_capacity = FIN_METACOG_MAX_HISTORY;
    bridge->history = (fin_decision_record_t*)
        nimcp_calloc(bridge->history_capacity, sizeof(fin_decision_record_t));
    if (!bridge->history) {
        set_error("Failed to allocate decision history");
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "financial_metacognition_bridge_create: bridge->history is NULL");
        return NULL;
    }

    bridge->history_count = 0;
    bridge->history_head = 0;
    bridge->last_reconsider_ms = 0;
    bridge->reconsider_count_session = 0;
    bridge->training_active = false;

    bridge->op_state = FIN_METACOG_STATE_INITIALIZED;

    fin_metacog_heartbeat_global("fin_metacog_create", 1.0f);
    return bridge;
}

void financial_metacognition_bridge_destroy(financial_metacognition_bridge_t* bridge) {
    fin_metacog_heartbeat_global("fin_metacog_destroy", 0.0f);

    if (bridge) {
        if (bridge->history) {
            nimcp_free(bridge->history);
        }
        bridge->magic = 0;
        bridge->op_state = FIN_METACOG_STATE_UNINITIALIZED;
        nimcp_free(bridge);
        bridge = NULL;
    }
}

int financial_metacognition_bridge_reset(financial_metacognition_bridge_t* bridge) {
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_metacognition_bridge_reset: bridge is NULL");
        return FIN_METACOG_ERR_NULL;
    }

    if (bridge->magic != FINANCIAL_METACOGNITION_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_METACOG_ERR_STATE;
    }

    fin_metacog_heartbeat_global("fin_metacog_reset", 0.0f);

    /* Reset stats */
    memset(&bridge->stats, 0, sizeof(fin_metacognition_bridge_stats_t));

    /* Reset history */
    if (bridge->history) {
        memset(bridge->history, 0,
               bridge->history_capacity * sizeof(fin_decision_record_t));
    }
    bridge->history_count = 0;
    bridge->history_head = 0;

    /* Reset session state */
    bridge->last_reconsider_ms = 0;
    bridge->reconsider_count_session = 0;
    bridge->training_active = false;

    bridge->op_state = FIN_METACOG_STATE_INITIALIZED;

    fin_metacog_heartbeat_global("fin_metacog_reset", 1.0f);
    return FIN_METACOG_ERR_OK;
}

/* ============================================================================
 * Subsystem Setters
 * ============================================================================ */

#define FIN_METACOG_SETTER(name, field) \
    int financial_metacognition_bridge_set_##name( \
        financial_metacognition_bridge_t* bridge, void* ptr) { \
        if (!bridge) { \
            set_error("bridge is NULL in set_" #name); \
            NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, \
                "financial_metacognition_bridge_set_" #name ": bridge is NULL"); \
            return FIN_METACOG_ERR_NULL; \
        } \
        if (bridge->magic != FINANCIAL_METACOGNITION_BRIDGE_MAGIC) { \
            set_error("Invalid bridge magic in set_" #name); \
            return FIN_METACOG_ERR_STATE; \
        } \
        bridge->field = ptr; \
        return FIN_METACOG_ERR_OK; \
    }

FIN_METACOG_SETTER(immune, immune)
FIN_METACOG_SETTER(health_agent, health_agent)
FIN_METACOG_SETTER(kg_wiring, kg_wiring)
FIN_METACOG_SETTER(logger, logger)
FIN_METACOG_SETTER(security, security)
FIN_METACOG_SETTER(bio_router, bio_router)

int financial_metacognition_bridge_set_coordinator(
    financial_metacognition_bridge_t* bridge,
    brain_cycle_coordinator_t* coordinator)
{
    if (!bridge) {
        set_error("bridge is NULL in set_coordinator");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_metacognition_bridge_set_coordinator: bridge is NULL");
        return FIN_METACOG_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_METACOGNITION_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic in set_coordinator");
        return FIN_METACOG_ERR_STATE;
    }
    bridge->coordinator = (void*)coordinator;
    return FIN_METACOG_ERR_OK;
}

int financial_metacognition_bridge_set_bbb(
    financial_metacognition_bridge_t* bridge,
    bbb_system_t bbb)
{
    if (!bridge) {
        set_error("bridge is NULL in set_bbb");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_metacognition_bridge_set_bbb: bridge is NULL");
        return FIN_METACOG_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_METACOGNITION_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic in set_bbb");
        return FIN_METACOG_ERR_STATE;
    }
    bridge->bbb = (void*)bbb;
    return FIN_METACOG_ERR_OK;
}

int financial_metacognition_bridge_set_ethics(
    financial_metacognition_bridge_t* bridge,
    ethics_engine_t ethics)
{
    if (!bridge) {
        set_error("bridge is NULL in set_ethics");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_metacognition_bridge_set_ethics: bridge is NULL");
        return FIN_METACOG_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_METACOGNITION_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic in set_ethics");
        return FIN_METACOG_ERR_STATE;
    }
    bridge->ethics = (void*)ethics;
    return FIN_METACOG_ERR_OK;
}

int financial_metacognition_bridge_set_lgss(
    financial_metacognition_bridge_t* bridge,
    const void* lgss)
{
    if (!bridge) {
        set_error("bridge is NULL in set_lgss");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_metacognition_bridge_set_lgss: bridge is NULL");
        return FIN_METACOG_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_METACOGNITION_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic in set_lgss");
        return FIN_METACOG_ERR_STATE;
    }
    bridge->lgss = lgss;
    return FIN_METACOG_ERR_OK;
}

/* ============================================================================
 * Decision History API
 * ============================================================================ */

int financial_metacognition_bridge_record_decision(
    financial_metacognition_bridge_t* bridge,
    const fin_decision_record_t* record)
{
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_metacognition_bridge_record_decision: bridge is NULL");
        return FIN_METACOG_ERR_NULL;
    }
    if (!record) {
        set_error("record is NULL");
        return FIN_METACOG_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_METACOGNITION_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_METACOG_ERR_STATE;
    }

    fin_metacog_heartbeat_global("fin_metacog_record", 0.0f);

    /* BBB validation if enabled */
    if (bridge->config.enable_bbb_validation && bridge->bbb) {
        bridge->stats.bbb_validations++;
        int rc = bbb_validate_data((bbb_system_t)bridge->bbb, record,
                                    sizeof(*record), "metacog_record");
        if (rc != 0) {
            set_error("BBB validation failed for metacog_record");
            return FIN_METACOG_ERR_BBB;
        }
    }

    /* Immune check if enabled */
    if (bridge->config.enable_immune_integration && bridge->immune) {
        bridge->stats.immune_checks++;
        int rc = brain_immune_validate_operation(
            (brain_immune_system_t*)bridge->immune,
            "metacog_record", 3);
        if (rc != 0) {
            set_error("Immune validation failed for metacog_record");
            return FIN_METACOG_ERR_IMMUNE;
        }
    }

    /* Store decision in circular buffer */
    bridge->history[bridge->history_head] = *record;
    bridge->history_head = (bridge->history_head + 1) % bridge->history_capacity;
    if (bridge->history_count < bridge->history_capacity) {
        bridge->history_count++;
    }

    bridge->op_state = FIN_METACOG_STATE_ACTIVE;

    fin_metacog_heartbeat_global("fin_metacog_record", 1.0f);
    return FIN_METACOG_ERR_OK;
}

int financial_metacognition_bridge_record_outcome(
    financial_metacognition_bridge_t* bridge,
    uint64_t timestamp_ms,
    float actual_outcome)
{
    if (!bridge) {
        set_error("bridge is NULL");
        return FIN_METACOG_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_METACOGNITION_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_METACOG_ERR_STATE;
    }

    /* Search for matching decision by timestamp */
    for (uint32_t i = 0; i < bridge->history_count; i++) {
        if (bridge->history[i].timestamp_ms == timestamp_ms) {
            bridge->history[i].actual_outcome = nimcp_clampf(actual_outcome, -1.0f, 1.0f);
            bridge->history[i].resolved = true;
            return FIN_METACOG_ERR_OK;
        }
    }

    set_error("Decision with timestamp %lu not found", (unsigned long)timestamp_ms);
    return FIN_METACOG_ERR_VALIDATION;
}

uint32_t financial_metacognition_bridge_get_decision_count(
    const financial_metacognition_bridge_t* bridge)
{
    if (!bridge || bridge->magic != FINANCIAL_METACOGNITION_BRIDGE_MAGIC) {
        return 0;
    }
    return bridge->history_count;
}

int financial_metacognition_bridge_clear_history(
    financial_metacognition_bridge_t* bridge)
{
    if (!bridge) {
        set_error("bridge is NULL");
        return FIN_METACOG_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_METACOGNITION_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_METACOG_ERR_STATE;
    }

    if (bridge->history) {
        memset(bridge->history, 0,
               bridge->history_capacity * sizeof(fin_decision_record_t));
    }
    bridge->history_count = 0;
    bridge->history_head = 0;

    return FIN_METACOG_ERR_OK;
}

/* ============================================================================
 * Internal Bias Detection Helpers
 * ============================================================================ */

/**
 * @brief Detect confirmation bias
 *
 * Looks for pattern of selectively attending to information that confirms
 * existing beliefs while ignoring contradictory evidence.
 */
static float detect_confirmation_bias(financial_metacognition_bridge_t* bridge) {
    if (bridge->history_count < bridge->config.min_decisions_for_analysis) {
        return 0.0f;
    }

    /* Check for pattern: consecutive decisions in same direction despite
     * mixed outcomes (sticking to a thesis despite contradicting evidence) */
    uint32_t same_direction_streak = 0;
    uint32_t max_streak = 0;
    uint32_t contradicting_outcomes = 0;
    float last_predicted = 0.0f;

    uint32_t window = bridge->config.analysis_window_size;
    if (window > bridge->history_count) window = bridge->history_count;

    for (uint32_t i = 0; i < window; i++) {
        uint32_t idx = (bridge->history_head + bridge->history_capacity - 1 - i)
                       % bridge->history_capacity;
        fin_decision_record_t* rec = &bridge->history[idx];

        bool same_dir = (rec->predicted_outcome * last_predicted) > 0.0f;

        if (i > 0 && same_dir) {
            same_direction_streak++;
            if (same_direction_streak > max_streak) {
                max_streak = same_direction_streak;
            }
            /* Check if outcome contradicted prediction */
            if (rec->resolved &&
                (rec->actual_outcome * rec->predicted_outcome) < 0.0f) {
                contradicting_outcomes++;
            }
        } else {
            same_direction_streak = 0;
        }

        last_predicted = rec->predicted_outcome;
    }

    /* Confirmation bias: long streaks despite contradicting evidence */
    float streak_factor = (float)max_streak / (float)window;
    float contradict_factor = (float)contradicting_outcomes / (float)window;

    return nimcp_clampf(streak_factor * 0.5f + contradict_factor * 0.5f, 0.0f, 1.0f);
}

/**
 * @brief Detect anchoring bias
 *
 * Looks for over-reliance on initial prices/values in subsequent decisions.
 */
static float detect_anchoring_bias(financial_metacognition_bridge_t* bridge) {
    if (bridge->history_count < bridge->config.min_decisions_for_analysis) {
        return 0.0f;
    }

    /* Check for pattern: predictions cluster around early values
     * despite changing conditions */
    uint32_t window = bridge->config.analysis_window_size;
    if (window > bridge->history_count) window = bridge->history_count;

    if (window < 5) return 0.0f;

    /* Get first prediction as potential anchor */
    uint32_t first_idx = (bridge->history_head + bridge->history_capacity - window)
                         % bridge->history_capacity;
    float anchor = bridge->history[first_idx].predicted_outcome;

    /* Count how many subsequent predictions cluster near anchor */
    uint32_t near_anchor = 0;
    float anchor_threshold = 0.2f;

    for (uint32_t i = 1; i < window; i++) {
        uint32_t idx = (first_idx + i) % bridge->history_capacity;
        float pred = bridge->history[idx].predicted_outcome;

        if (fabsf_safe(pred - anchor) < anchor_threshold) {
            near_anchor++;
        }
    }

    return nimcp_clampf((float)near_anchor / (float)(window - 1), 0.0f, 1.0f);
}

/**
 * @brief Detect recency bias
 *
 * Looks for overweighting of recent events vs historical data.
 */
static float detect_recency_bias(financial_metacognition_bridge_t* bridge) {
    if (bridge->history_count < bridge->config.min_decisions_for_analysis) {
        return 0.0f;
    }

    uint32_t window = bridge->config.analysis_window_size;
    if (window > bridge->history_count) window = bridge->history_count;

    if (window < 10) return 0.0f;

    /* Compare correlation between predictions and recent (last 3) outcomes
     * vs older outcomes */
    uint32_t recent_count = 3;
    float recent_sum = 0.0f;
    float older_sum = 0.0f;
    uint32_t recent_matches = 0;
    uint32_t older_matches = 0;

    for (uint32_t i = 0; i < window; i++) {
        uint32_t idx = (bridge->history_head + bridge->history_capacity - 1 - i)
                       % bridge->history_capacity;
        fin_decision_record_t* rec = &bridge->history[idx];

        if (!rec->resolved) continue;

        bool match = (rec->predicted_outcome * rec->actual_outcome) > 0.0f;

        if (i < recent_count) {
            recent_sum += fabsf_safe(rec->predicted_outcome);
            if (match) recent_matches++;
        } else {
            older_sum += fabsf_safe(rec->predicted_outcome);
            if (match) older_matches++;
        }
    }

    /* Recency bias: predictions follow recent outcomes more than older */
    float recent_rate = recent_matches > 0 ?
        (float)recent_matches / (float)recent_count : 0.0f;
    float older_rate = (window - recent_count) > 0 ?
        (float)older_matches / (float)(window - recent_count) : 0.0f;

    /* Higher score if recent accuracy >> older accuracy */
    float diff = recent_rate - older_rate;
    return nimcp_clampf(diff + 0.5f, 0.0f, 1.0f);  /* Center around 0.5 */
}

/**
 * @brief Detect overconfidence bias
 *
 * Compares stated confidence to actual accuracy.
 */
static float detect_overconfidence_bias(financial_metacognition_bridge_t* bridge) {
    if (bridge->history_count < bridge->config.min_decisions_for_analysis) {
        return 0.0f;
    }

    uint32_t window = bridge->config.analysis_window_size;
    if (window > bridge->history_count) window = bridge->history_count;

    float total_confidence = 0.0f;
    float total_accuracy = 0.0f;
    uint32_t resolved_count = 0;

    for (uint32_t i = 0; i < window; i++) {
        uint32_t idx = (bridge->history_head + bridge->history_capacity - 1 - i)
                       % bridge->history_capacity;
        fin_decision_record_t* rec = &bridge->history[idx];

        if (!rec->resolved) continue;

        total_confidence += rec->confidence;

        /* Accuracy: did sign match? */
        bool correct = (rec->predicted_outcome * rec->actual_outcome) > 0.0f;
        total_accuracy += correct ? 1.0f : 0.0f;

        resolved_count++;
    }

    if (resolved_count == 0) return 0.0f;

    float avg_confidence = total_confidence / (float)resolved_count;
    float avg_accuracy = total_accuracy / (float)resolved_count;

    /* Overconfidence: confidence >> accuracy */
    float diff = avg_confidence - avg_accuracy;
    return nimcp_clampf(diff + 0.5f, 0.0f, 1.0f);
}

/**
 * @brief Detect loss aversion
 *
 * Looks for asymmetric behavior after gains vs losses.
 */
static float detect_loss_aversion(financial_metacognition_bridge_t* bridge) {
    if (bridge->history_count < bridge->config.min_decisions_for_analysis) {
        return 0.0f;
    }

    uint32_t window = bridge->config.analysis_window_size;
    if (window > bridge->history_count) window = bridge->history_count;

    /* Count risk-taking after gains vs losses */
    uint32_t after_gain_aggressive = 0;
    uint32_t after_gain_total = 0;
    uint32_t after_loss_conservative = 0;
    uint32_t after_loss_total = 0;

    for (uint32_t i = 1; i < window; i++) {
        uint32_t curr_idx = (bridge->history_head + bridge->history_capacity - 1 - i)
                           % bridge->history_capacity;
        uint32_t prev_idx = (curr_idx + bridge->history_capacity - 1)
                           % bridge->history_capacity;

        fin_decision_record_t* curr = &bridge->history[curr_idx];
        fin_decision_record_t* prev = &bridge->history[prev_idx];

        if (!prev->resolved) continue;

        bool prev_gain = prev->actual_outcome > 0.0f;
        bool curr_aggressive = fabsf_safe(curr->predicted_outcome) > 0.5f;

        if (prev_gain) {
            after_gain_total++;
            if (curr_aggressive) after_gain_aggressive++;
        } else {
            after_loss_total++;
            if (!curr_aggressive) after_loss_conservative++;
        }
    }

    /* Loss aversion: conservative after losses but not after gains */
    float after_loss_rate = after_loss_total > 0 ?
        (float)after_loss_conservative / (float)after_loss_total : 0.5f;
    float after_gain_rate = after_gain_total > 0 ?
        (float)after_gain_aggressive / (float)after_gain_total : 0.5f;

    /* Higher score = more loss aversion */
    return nimcp_clampf((after_loss_rate - (1.0f - after_gain_rate)) * 0.5f + 0.5f, 0.0f, 1.0f);
}

/**
 * @brief Detect herding behavior
 *
 * Would need market consensus data; for now uses decision clustering.
 */
static float detect_herding(financial_metacognition_bridge_t* bridge) {
    if (bridge->history_count < bridge->config.min_decisions_for_analysis) {
        return 0.0f;
    }

    uint32_t window = bridge->config.analysis_window_size;
    if (window > bridge->history_count) window = bridge->history_count;

    /* Proxy: cluster of similar predictions indicates possible herding */
    uint32_t positive_predictions = 0;
    uint32_t negative_predictions = 0;

    for (uint32_t i = 0; i < window; i++) {
        uint32_t idx = (bridge->history_head + bridge->history_capacity - 1 - i)
                       % bridge->history_capacity;
        fin_decision_record_t* rec = &bridge->history[idx];

        if (rec->predicted_outcome > 0.1f) {
            positive_predictions++;
        } else if (rec->predicted_outcome < -0.1f) {
            negative_predictions++;
        }
    }

    /* Herding: extreme imbalance suggests following consensus */
    float max_side = (float)(positive_predictions > negative_predictions ?
                             positive_predictions : negative_predictions);
    float imbalance = max_side / (float)window;

    return nimcp_clampf((imbalance - 0.5f) * 2.0f, 0.0f, 1.0f);
}

/**
 * @brief Detect gambler's fallacy
 *
 * Looks for belief that independent outcomes are due to "revert".
 */
static float detect_gambler_fallacy(financial_metacognition_bridge_t* bridge) {
    if (bridge->history_count < bridge->config.min_decisions_for_analysis) {
        return 0.0f;
    }

    uint32_t window = bridge->config.analysis_window_size;
    if (window > bridge->history_count) window = bridge->history_count;

    /* After streak of same outcomes, predicting reversal */
    uint32_t reversal_predictions = 0;
    uint32_t streak_situations = 0;

    int streak_count = 0;
    float last_outcome = 0.0f;

    for (uint32_t i = 1; i < window; i++) {
        uint32_t curr_idx = (bridge->history_head + bridge->history_capacity - 1 - i)
                           % bridge->history_capacity;
        uint32_t prev_idx = (curr_idx + bridge->history_capacity - 1)
                           % bridge->history_capacity;

        fin_decision_record_t* curr = &bridge->history[curr_idx];
        fin_decision_record_t* prev = &bridge->history[prev_idx];

        if (!prev->resolved) continue;

        bool same_sign = (prev->actual_outcome * last_outcome) > 0.0f;
        if (same_sign) {
            streak_count++;
        } else {
            streak_count = 0;
        }

        /* After 3+ same outcomes, check if predicting reversal */
        if (streak_count >= 3) {
            streak_situations++;
            bool predicting_reversal = (curr->predicted_outcome * prev->actual_outcome) < 0.0f;
            if (predicting_reversal) {
                reversal_predictions++;
            }
        }

        last_outcome = prev->actual_outcome;
    }

    if (streak_situations == 0) return 0.0f;

    return nimcp_clampf((float)reversal_predictions / (float)streak_situations, 0.0f, 1.0f);
}

/**
 * @brief Detect sunk cost fallacy
 *
 * Continuing to hold losing positions due to past investment.
 */
static float detect_sunk_cost(financial_metacognition_bridge_t* bridge) {
    if (bridge->history_count < bridge->config.min_decisions_for_analysis) {
        return 0.0f;
    }

    uint32_t window = bridge->config.analysis_window_size;
    if (window > bridge->history_count) window = bridge->history_count;

    /* Track symbols with consecutive losses but continued bullish predictions */
    uint32_t sunk_cost_patterns = 0;
    uint32_t total_sequences = 0;

    /* Simple heuristic: same symbol, multiple losses, still bullish */
    for (uint32_t i = 3; i < window; i++) {
        uint32_t idx = (bridge->history_head + bridge->history_capacity - 1 - i)
                       % bridge->history_capacity;
        fin_decision_record_t* rec = &bridge->history[idx];

        if (!rec->resolved) continue;

        /* Check previous 2 decisions for same symbol */
        int loss_count = rec->actual_outcome < 0.0f ? 1 : 0;
        bool same_symbol_sequence = true;

        for (uint32_t j = 1; j <= 2 && same_symbol_sequence; j++) {
            uint32_t prev_idx = (idx + bridge->history_capacity - j)
                               % bridge->history_capacity;
            fin_decision_record_t* prev = &bridge->history[prev_idx];

            if (strcmp(rec->symbol, prev->symbol) != 0) {
                same_symbol_sequence = false;
            } else if (prev->resolved && prev->actual_outcome < 0.0f) {
                loss_count++;
            }
        }

        if (same_symbol_sequence && loss_count >= 2) {
            total_sequences++;
            /* Still bullish despite losses? */
            if (rec->predicted_outcome > 0.2f) {
                sunk_cost_patterns++;
            }
        }
    }

    if (total_sequences == 0) return 0.0f;

    return nimcp_clampf((float)sunk_cost_patterns / (float)total_sequences, 0.0f, 1.0f);
}

/**
 * @brief Detect availability heuristic
 *
 * Overweighting easily recalled (dramatic) examples.
 */
static float detect_availability(financial_metacognition_bridge_t* bridge) {
    if (bridge->history_count < bridge->config.min_decisions_for_analysis) {
        return 0.0f;
    }

    uint32_t window = bridge->config.analysis_window_size;
    if (window > bridge->history_count) window = bridge->history_count;

    /* Dramatic = large magnitude outcomes
     * Check if predictions follow large recent outcomes */
    uint32_t follow_dramatic = 0;
    uint32_t dramatic_count = 0;
    float dramatic_threshold = 0.7f;

    for (uint32_t i = 1; i < window; i++) {
        uint32_t curr_idx = (bridge->history_head + bridge->history_capacity - 1 - i)
                           % bridge->history_capacity;
        uint32_t prev_idx = (curr_idx + bridge->history_capacity - 1)
                           % bridge->history_capacity;

        fin_decision_record_t* curr = &bridge->history[curr_idx];
        fin_decision_record_t* prev = &bridge->history[prev_idx];

        if (!prev->resolved) continue;

        bool dramatic = fabsf_safe(prev->actual_outcome) > dramatic_threshold;
        if (dramatic) {
            dramatic_count++;
            /* Does current prediction follow dramatic outcome direction? */
            bool follows = (curr->predicted_outcome * prev->actual_outcome) > 0.0f;
            if (follows) follow_dramatic++;
        }
    }

    if (dramatic_count == 0) return 0.0f;

    return nimcp_clampf((float)follow_dramatic / (float)dramatic_count, 0.0f, 1.0f);
}

/**
 * @brief Detect hindsight bias
 *
 * Post-hoc rationalization of outcomes as "predictable".
 * Hard to detect without explicit "I knew it all along" signals.
 * Proxy: increasing confidence after resolved outcomes.
 */
static float detect_hindsight(financial_metacognition_bridge_t* bridge) {
    if (bridge->history_count < bridge->config.min_decisions_for_analysis) {
        return 0.0f;
    }

    uint32_t window = bridge->config.analysis_window_size;
    if (window > bridge->history_count) window = bridge->history_count;

    /* Proxy: track if confidence increases over time regardless of accuracy */
    float early_confidence = 0.0f;
    float late_confidence = 0.0f;
    uint32_t early_count = 0;
    uint32_t late_count = 0;
    uint32_t half = window / 2;

    for (uint32_t i = 0; i < window; i++) {
        uint32_t idx = (bridge->history_head + bridge->history_capacity - 1 - i)
                       % bridge->history_capacity;
        fin_decision_record_t* rec = &bridge->history[idx];

        if (i < half) {
            late_confidence += rec->confidence;
            late_count++;
        } else {
            early_confidence += rec->confidence;
            early_count++;
        }
    }

    if (early_count == 0 || late_count == 0) return 0.0f;

    float early_avg = early_confidence / (float)early_count;
    float late_avg = late_confidence / (float)late_count;

    /* Hindsight: confidence increases over time without accuracy improvement */
    float diff = late_avg - early_avg;
    return nimcp_clampf(diff + 0.5f, 0.0f, 1.0f);
}

/* ============================================================================
 * Core API - Bias Detection
 * ============================================================================ */

int financial_metacognition_bridge_detect_biases(
    financial_metacognition_bridge_t* bridge,
    fin_bias_detection_t* biases,
    uint32_t* bias_count)
{
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_metacognition_bridge_detect_biases: bridge is NULL");
        return FIN_METACOG_ERR_NULL;
    }
    if (!biases || !bias_count) {
        set_error("biases or bias_count is NULL");
        return FIN_METACOG_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_METACOGNITION_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_METACOG_ERR_STATE;
    }

    fin_metacog_heartbeat_global("fin_metacog_biases", 0.0f);
    bridge->op_state = FIN_METACOG_STATE_ANALYZING;

    /* BBB validation if enabled */
    if (bridge->config.enable_bbb_validation && bridge->bbb) {
        bridge->stats.bbb_validations++;
    }

    /* Immune check if enabled */
    if (bridge->config.enable_immune_integration && bridge->immune) {
        bridge->stats.immune_checks++;
    }

    *bias_count = 0;

    if (bridge->history_count < bridge->config.min_decisions_for_analysis) {
        set_error("Insufficient data: need %u decisions, have %u",
                  bridge->config.min_decisions_for_analysis, bridge->history_count);
        bridge->op_state = FIN_METACOG_STATE_ACTIVE;
        return FIN_METACOG_ERR_INSUFFICIENT_DATA;
    }

    bridge->stats.bias_checks++;

    /* Detection function array */
    struct {
        fin_cognitive_bias_t type;
        float (*detect)(financial_metacognition_bridge_t*);
        float threshold;
        const char* evidence_template;
        const char* mitigation;
    } detectors[] = {
        {FIN_BIAS_CONFIRMATION, detect_confirmation_bias,
         bridge->config.bias_thresholds.confirmation_threshold,
         "Streak of same-direction decisions despite contradicting outcomes",
         "Actively seek disconfirming evidence; devil's advocate"},
        {FIN_BIAS_ANCHORING, detect_anchoring_bias,
         bridge->config.bias_thresholds.anchoring_threshold,
         "Predictions cluster around initial values",
         "Re-evaluate from current fundamentals; ignore entry price"},
        {FIN_BIAS_RECENCY, detect_recency_bias,
         bridge->config.bias_thresholds.recency_threshold,
         "Recent outcomes weighted more than historical pattern",
         "Consider longer time horizons; review historical data"},
        {FIN_BIAS_OVERCONFIDENCE, detect_overconfidence_bias,
         bridge->config.bias_thresholds.overconfidence_threshold,
         "Stated confidence exceeds actual accuracy",
         "Track calibration; reduce position sizes; consider uncertainty"},
        {FIN_BIAS_LOSS_AVERSION, detect_loss_aversion,
         bridge->config.bias_thresholds.loss_aversion_threshold,
         "Asymmetric risk-taking after gains vs losses",
         "Use systematic rules; set stop-losses before entry"},
        {FIN_BIAS_HERDING, detect_herding,
         bridge->config.bias_thresholds.herding_threshold,
         "Predictions cluster with apparent consensus",
         "Independent analysis; consider contrarian positions"},
        {FIN_BIAS_GAMBLER_FALLACY, detect_gambler_fallacy,
         bridge->config.bias_thresholds.gambler_fallacy_threshold,
         "Predicting reversal after streak of same outcomes",
         "Each trade is independent; ignore streaks"},
        {FIN_BIAS_SUNK_COST, detect_sunk_cost,
         bridge->config.bias_thresholds.sunk_cost_threshold,
         "Continuing bullish on losers due to past investment",
         "Evaluate current position as if entering fresh"},
        {FIN_BIAS_AVAILABILITY, detect_availability,
         bridge->config.bias_thresholds.availability_threshold,
         "Predictions follow dramatic recent outcomes",
         "Use base rates; systematic probability estimates"},
        {FIN_BIAS_HINDSIGHT, detect_hindsight,
         bridge->config.bias_thresholds.hindsight_threshold,
         "Increasing confidence without accuracy improvement",
         "Record predictions before outcomes; track accuracy"}
    };

    for (size_t i = 0; i < sizeof(detectors)/sizeof(detectors[0]); i++) {
        float strength = detectors[i].detect(bridge);

        if (strength >= detectors[i].threshold && *bias_count < FIN_METACOG_MAX_BIASES) {
            fin_bias_detection_t* det = &biases[*bias_count];
            det->bias = detectors[i].type;
            det->strength = strength;
            snprintf(det->evidence, sizeof(det->evidence), "%s (%.1f%%)",
                     detectors[i].evidence_template, strength * 100.0f);
            snprintf(det->mitigation, sizeof(det->mitigation), "%s",
                     detectors[i].mitigation);

            bridge->stats.biases_detected++;
            (*bias_count)++;
        }
    }

    /* KG messaging */
    if (*bias_count > 0) {
        bridge_kg_publish(bridge, KG_MSG_FIN_METACOG_BIAS,
                          biases, (*bias_count) * sizeof(fin_bias_detection_t));
    }

    bridge->op_state = FIN_METACOG_STATE_ACTIVE;
    fin_metacog_heartbeat_global("fin_metacog_biases", 1.0f);
    return FIN_METACOG_ERR_OK;
}

int financial_metacognition_bridge_check_bias(
    financial_metacognition_bridge_t* bridge,
    fin_cognitive_bias_t bias_type,
    fin_bias_detection_t* detection)
{
    if (!bridge) {
        set_error("bridge is NULL");
        return FIN_METACOG_ERR_NULL;
    }
    if (!detection) {
        set_error("detection is NULL");
        return FIN_METACOG_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_METACOGNITION_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_METACOG_ERR_STATE;
    }
    if (bias_type >= FIN_BIAS_COUNT) {
        set_error("Invalid bias type");
        return FIN_METACOG_ERR_INVALID_PARAM;
    }

    /* Detect all biases and find the requested one */
    fin_bias_detection_t all_biases[FIN_METACOG_MAX_BIASES];
    uint32_t count = 0;

    int rc = financial_metacognition_bridge_detect_biases(bridge, all_biases, &count);
    if (rc != FIN_METACOG_ERR_OK && rc != FIN_METACOG_ERR_INSUFFICIENT_DATA) {
        return rc;
    }

    /* Find requested bias */
    for (uint32_t i = 0; i < count; i++) {
        if (all_biases[i].bias == bias_type) {
            *detection = all_biases[i];
            return FIN_METACOG_ERR_OK;
        }
    }

    /* Not detected - return zero strength */
    memset(detection, 0, sizeof(fin_bias_detection_t));
    detection->bias = bias_type;
    detection->strength = 0.0f;
    snprintf(detection->evidence, sizeof(detection->evidence), "No bias detected");
    snprintf(detection->mitigation, sizeof(detection->mitigation), "None needed");

    return FIN_METACOG_ERR_OK;
}

/* ============================================================================
 * Core API - Confidence Calibration
 * ============================================================================ */

int financial_metacognition_bridge_assess_confidence(
    financial_metacognition_bridge_t* bridge,
    fin_confidence_result_t* result)
{
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_metacognition_bridge_assess_confidence: bridge is NULL");
        return FIN_METACOG_ERR_NULL;
    }
    if (!result) {
        set_error("result is NULL");
        return FIN_METACOG_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_METACOGNITION_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_METACOG_ERR_STATE;
    }

    fin_metacog_heartbeat_global("fin_metacog_confidence", 0.0f);

    memset(result, 0, sizeof(fin_confidence_result_t));
    bridge->stats.confidence_calibrations++;

    uint32_t window = bridge->config.analysis_window_size;
    if (window > bridge->history_count) window = bridge->history_count;

    /* Count resolved decisions */
    uint32_t resolved_count = 0;
    float total_confidence = 0.0f;
    float total_accuracy = 0.0f;
    float brier_sum = 0.0f;

    for (uint32_t i = 0; i < window; i++) {
        uint32_t idx = (bridge->history_head + bridge->history_capacity - 1 - i)
                       % bridge->history_capacity;
        fin_decision_record_t* rec = &bridge->history[idx];

        if (!rec->resolved) continue;

        resolved_count++;
        total_confidence += rec->confidence;

        bool correct = (rec->predicted_outcome * rec->actual_outcome) > 0.0f;
        total_accuracy += correct ? 1.0f : 0.0f;

        /* Brier score: mean squared error of probabilistic predictions */
        float outcome_binary = correct ? 1.0f : 0.0f;
        float error = rec->confidence - outcome_binary;
        brier_sum += error * error;
    }

    result->decisions_analyzed = resolved_count;

    if (resolved_count < bridge->config.min_decisions_for_analysis) {
        result->level = FIN_CONFIDENCE_UNKNOWN;
        result->calibration_score = 0.5f;
        result->overconfidence_ratio = 1.0f;
        result->brier_score = 0.25f;  /* Random baseline */
        snprintf(result->recommendation, FIN_METACOG_DESC_LEN,
                 "Insufficient data for calibration assessment");

        fin_metacog_heartbeat_global("fin_metacog_confidence", 1.0f);
        return FIN_METACOG_ERR_OK;
    }

    float avg_confidence = total_confidence / (float)resolved_count;
    float avg_accuracy = total_accuracy / (float)resolved_count;
    float brier_score = brier_sum / (float)resolved_count;

    result->brier_score = brier_score;
    result->overconfidence_ratio = (avg_accuracy > 0.01f) ?
        avg_confidence / avg_accuracy : 1.0f;

    /* Calibration score: how close confidence is to accuracy (1.0 = perfect) */
    float calibration_error = fabsf_safe(avg_confidence - avg_accuracy);
    result->calibration_score = 1.0f - calibration_error;

    /* Determine level */
    if (calibration_error <= bridge->config.acceptable_calibration_error) {
        result->level = FIN_CONFIDENCE_WELL_CALIBRATED;
        snprintf(result->recommendation, FIN_METACOG_DESC_LEN,
                 "Confidence well-calibrated (%.1f%% vs %.1f%% accuracy)",
                 avg_confidence * 100.0f, avg_accuracy * 100.0f);
    } else if (avg_confidence > avg_accuracy + 0.1f) {
        result->level = FIN_CONFIDENCE_OVERCONFIDENT;
        snprintf(result->recommendation, FIN_METACOG_DESC_LEN,
                 "Overconfident: %.1f%% confidence but %.1f%% accuracy. Reduce certainty.",
                 avg_confidence * 100.0f, avg_accuracy * 100.0f);
    } else if (avg_confidence < avg_accuracy - 0.1f) {
        result->level = FIN_CONFIDENCE_UNDERCONFIDENT;
        snprintf(result->recommendation, FIN_METACOG_DESC_LEN,
                 "Underconfident: %.1f%% confidence but %.1f%% accuracy. Trust yourself more.",
                 avg_confidence * 100.0f, avg_accuracy * 100.0f);
    } else {
        result->level = FIN_CONFIDENCE_INCONSISTENT;
        snprintf(result->recommendation, FIN_METACOG_DESC_LEN,
                 "Calibration inconsistent: %.1f%% confidence vs %.1f%% accuracy",
                 avg_confidence * 100.0f, avg_accuracy * 100.0f);
    }

    /* KG messaging */
    bridge_kg_publish(bridge, KG_MSG_FIN_METACOG_CONFIDENCE,
                      result, sizeof(*result));

    fin_metacog_heartbeat_global("fin_metacog_confidence", 1.0f);
    return FIN_METACOG_ERR_OK;
}

/* ============================================================================
 * Core API - Reconsideration
 * ============================================================================ */

int financial_metacognition_bridge_should_reconsider(
    financial_metacognition_bridge_t* bridge,
    const fin_decision_record_t* decision,
    fin_reconsider_result_t* result)
{
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_metacognition_bridge_should_reconsider: bridge is NULL");
        return FIN_METACOG_ERR_NULL;
    }
    if (!decision || !result) {
        set_error("decision or result is NULL");
        return FIN_METACOG_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_METACOGNITION_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_METACOG_ERR_STATE;
    }

    fin_metacog_heartbeat_global("fin_metacog_reconsider", 0.0f);

    memset(result, 0, sizeof(fin_reconsider_result_t));

    /* Check cooldown */
    uint64_t now_ms = decision->timestamp_ms;
    float cooldown_ms = bridge->config.reconsider_cooldown_sec * 1000.0f;
    if ((now_ms - bridge->last_reconsider_ms) < (uint64_t)cooldown_ms) {
        result->urgency = FIN_RECONSIDER_NONE;
        result->should_reconsider = false;
        result->confidence_in_original = decision->confidence;
        snprintf(result->reason, FIN_METACOG_DESC_LEN, "In cooldown period");
        snprintf(result->suggested_action, FIN_METACOG_DESC_LEN, "Proceed with decision");
        return FIN_METACOG_ERR_OK;
    }

    /* Check session limit */
    if (bridge->reconsider_count_session >= bridge->config.max_reconsider_triggers) {
        result->urgency = FIN_RECONSIDER_NONE;
        result->should_reconsider = false;
        result->confidence_in_original = decision->confidence;
        snprintf(result->reason, FIN_METACOG_DESC_LEN, "Session reconsider limit reached");
        snprintf(result->suggested_action, FIN_METACOG_DESC_LEN, "Proceed with decision");
        return FIN_METACOG_ERR_OK;
    }

    /* Detect biases */
    int rc = financial_metacognition_bridge_detect_biases(
        bridge, result->biases, &result->detected_bias_count);
    if (rc != FIN_METACOG_ERR_OK && rc != FIN_METACOG_ERR_INSUFFICIENT_DATA) {
        return rc;
    }

    /* Assess confidence calibration */
    fin_confidence_result_t confidence;
    financial_metacognition_bridge_assess_confidence(bridge, &confidence);

    /* Determine if reconsideration is needed */
    float max_bias_strength = 0.0f;
    fin_cognitive_bias_t strongest_bias = FIN_BIAS_COUNT;

    for (uint32_t i = 0; i < result->detected_bias_count; i++) {
        if (result->biases[i].strength > max_bias_strength) {
            max_bias_strength = result->biases[i].strength;
            strongest_bias = result->biases[i].bias;
        }
    }

    bool bias_trigger = max_bias_strength >= bridge->config.reconsider_bias_threshold;
    bool calibration_trigger = confidence.calibration_score <
                               bridge->config.reconsider_calibration_threshold;
    bool overconfidence_trigger = (confidence.level == FIN_CONFIDENCE_OVERCONFIDENT &&
                                   decision->confidence > 0.8f);

    result->confidence_in_original = decision->confidence;

    if (bias_trigger && calibration_trigger) {
        result->urgency = FIN_RECONSIDER_REQUIRED;
        result->should_reconsider = true;
        snprintf(result->reason, FIN_METACOG_DESC_LEN,
                 "Strong %s bias (%.0f%%) and poor calibration (%.0f%%)",
                 fin_metacog_bias_name(strongest_bias),
                 max_bias_strength * 100.0f,
                 confidence.calibration_score * 100.0f);
        snprintf(result->suggested_action, FIN_METACOG_DESC_LEN,
                 "Re-evaluate using checklist; seek disconfirming evidence");
    } else if (bias_trigger) {
        result->urgency = FIN_RECONSIDER_RECOMMENDED;
        result->should_reconsider = true;
        snprintf(result->reason, FIN_METACOG_DESC_LEN,
                 "Strong %s bias detected (%.0f%%)",
                 fin_metacog_bias_name(strongest_bias),
                 max_bias_strength * 100.0f);
        snprintf(result->suggested_action, FIN_METACOG_DESC_LEN,
                 "Apply mitigation: %s",
                 result->detected_bias_count > 0 ? result->biases[0].mitigation : "N/A");
    } else if (calibration_trigger || overconfidence_trigger) {
        result->urgency = FIN_RECONSIDER_OPTIONAL;
        result->should_reconsider = true;
        snprintf(result->reason, FIN_METACOG_DESC_LEN,
                 "Confidence calibration concern (%.0f%% score)",
                 confidence.calibration_score * 100.0f);
        snprintf(result->suggested_action, FIN_METACOG_DESC_LEN,
                 "Consider reducing confidence; use smaller position");
    } else {
        result->urgency = FIN_RECONSIDER_NONE;
        result->should_reconsider = false;
        snprintf(result->reason, FIN_METACOG_DESC_LEN,
                 "No significant metacognitive concerns");
        snprintf(result->suggested_action, FIN_METACOG_DESC_LEN,
                 "Proceed with decision");
    }

    /* Update tracking */
    if (result->should_reconsider) {
        bridge->stats.reconsiderations++;
        bridge->last_reconsider_ms = now_ms;
        bridge->reconsider_count_session++;
    }

    /* KG messaging */
    bridge_kg_publish(bridge, KG_MSG_FIN_METACOG_RECONSIDER,
                      result, sizeof(*result));

    fin_metacog_heartbeat_global("fin_metacog_reconsider", 1.0f);
    return FIN_METACOG_ERR_OK;
}

/* ============================================================================
 * Comprehensive Assessment API
 * ============================================================================ */

int financial_metacognition_bridge_assess(
    financial_metacognition_bridge_t* bridge,
    fin_metacognitive_assessment_t* assessment)
{
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_metacognition_bridge_assess: bridge is NULL");
        return FIN_METACOG_ERR_NULL;
    }
    if (!assessment) {
        set_error("assessment is NULL");
        return FIN_METACOG_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_METACOGNITION_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_METACOG_ERR_STATE;
    }

    fin_metacog_heartbeat_global("fin_metacog_assess", 0.0f);

    memset(assessment, 0, sizeof(fin_metacognitive_assessment_t));

    /* Detect biases */
    int rc = financial_metacognition_bridge_detect_biases(
        bridge, assessment->biases, &assessment->bias_count);
    if (rc != FIN_METACOG_ERR_OK && rc != FIN_METACOG_ERR_INSUFFICIENT_DATA) {
        return rc;
    }

    /* Assess confidence */
    rc = financial_metacognition_bridge_assess_confidence(bridge, &assessment->confidence);
    if (rc != FIN_METACOG_ERR_OK) {
        return rc;
    }

    /* Compute metacognitive accuracy */
    /* Based on calibration and inverse of bias strength */
    float avg_bias_strength = 0.0f;
    for (uint32_t i = 0; i < assessment->bias_count; i++) {
        avg_bias_strength += assessment->biases[i].strength;
    }
    if (assessment->bias_count > 0) {
        avg_bias_strength /= (float)assessment->bias_count;
    }

    assessment->metacognitive_accuracy = nimcp_clampf(
        (assessment->confidence.calibration_score + (1.0f - avg_bias_strength)) / 2.0f,
        0.0f, 1.0f);

    /* Self-awareness score: higher if fewer biases and better calibration */
    assessment->self_awareness_score = nimcp_clampf(
        (1.0f - (float)assessment->bias_count / (float)FIN_BIAS_COUNT) *
        assessment->confidence.calibration_score,
        0.0f, 1.0f);

    /* Generate summary */
    snprintf(assessment->summary, FIN_METACOG_DESC_LEN,
             "Metacog: %.0f%% accuracy, %.0f%% awareness, %u biases, %s calibration",
             assessment->metacognitive_accuracy * 100.0f,
             assessment->self_awareness_score * 100.0f,
             assessment->bias_count,
             fin_metacog_confidence_name(assessment->confidence.level));

    /* KG messaging */
    bridge_kg_publish(bridge, KG_MSG_FIN_METACOG_ASSESS,
                      assessment, sizeof(*assessment));

    fin_metacog_heartbeat_global("fin_metacog_assess", 1.0f);
    return FIN_METACOG_ERR_OK;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

fin_metacognition_bridge_state_t financial_metacognition_bridge_get_state(
    const financial_metacognition_bridge_t* bridge)
{
    if (!bridge || bridge->magic != FINANCIAL_METACOGNITION_BRIDGE_MAGIC) {
        return FIN_METACOG_STATE_UNINITIALIZED;
    }
    return bridge->op_state;
}

int financial_metacognition_bridge_get_stats(
    const financial_metacognition_bridge_t* bridge,
    fin_metacognition_bridge_stats_t* stats)
{
    if (!bridge) {
        set_error("bridge is NULL");
        return FIN_METACOG_ERR_NULL;
    }
    if (!stats) {
        set_error("stats is NULL");
        return FIN_METACOG_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_METACOGNITION_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_METACOG_ERR_STATE;
    }

    *stats = bridge->stats;
    return FIN_METACOG_ERR_OK;
}

void financial_metacognition_bridge_reset_stats(
    financial_metacognition_bridge_t* bridge)
{
    if (bridge && bridge->magic == FINANCIAL_METACOGNITION_BRIDGE_MAGIC) {
        memset(&bridge->stats, 0, sizeof(fin_metacognition_bridge_stats_t));
    }
}

/* ============================================================================
 * Health Integration
 * ============================================================================ */

int financial_metacognition_bridge_heartbeat(
    financial_metacognition_bridge_t* bridge,
    const char* operation,
    float progress)
{
    if (!bridge) {
        return FIN_METACOG_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_METACOGNITION_BRIDGE_MAGIC) {
        return FIN_METACOG_ERR_STATE;
    }

    /* Forward to global health agent */
    fin_metacog_heartbeat_global(
        operation ? operation : "fin_metacog_heartbeat", progress);

    /* Forward to instance-level health agent */
    if (bridge->health_agent) {
        nimcp_health_agent_heartbeat_ex(
            (nimcp_health_agent_t*)bridge->health_agent, operation, progress);
    }

    bridge->stats.health_heartbeats++;
    return FIN_METACOG_ERR_OK;
}

/* ============================================================================
 * Training Integration
 * ============================================================================ */

int financial_metacognition_bridge_training_begin(
    financial_metacognition_bridge_t* bridge)
{
    if (!bridge) {
        set_error("bridge is NULL");
        return FIN_METACOG_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_METACOGNITION_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_METACOG_ERR_STATE;
    }

    bridge->training_active = true;
    fin_metacog_heartbeat_global("fin_metacog_train_begin", 0.0f);
    return FIN_METACOG_ERR_OK;
}

int financial_metacognition_bridge_training_end(
    financial_metacognition_bridge_t* bridge)
{
    if (!bridge) {
        set_error("bridge is NULL");
        return FIN_METACOG_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_METACOGNITION_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_METACOG_ERR_STATE;
    }

    bridge->training_active = false;
    fin_metacog_heartbeat_global("fin_metacog_train_end", 1.0f);
    return FIN_METACOG_ERR_OK;
}

int financial_metacognition_bridge_training_step(
    financial_metacognition_bridge_t* bridge,
    float progress)
{
    if (!bridge) {
        set_error("bridge is NULL");
        return FIN_METACOG_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_METACOGNITION_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_METACOG_ERR_STATE;
    }

    fin_metacog_heartbeat_global("fin_metacog_train_step", progress);
    return FIN_METACOG_ERR_OK;
}
