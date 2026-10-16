#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_language_cingulate_bridge.c - Language-Cingulate Error Monitoring Bridge
//=============================================================================
/**
 * @file nimcp_language_cingulate_bridge.c
 * @brief Implementation of Language-Cingulate error monitoring bridge
 *
 * Provides bidirectional integration between Language Layer and Cingulate
 * Cortex for speech error detection, conflict monitoring, and self-correction.
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "language/bridges/nimcp_language_cingulate_bridge.h"
#include "core/brain/regions/broca/nimcp_broca_adapter.h"
#include "core/brain/regions/wernicke/nimcp_wernicke_adapter.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(language_cingulate_bridge)

#define LOG_MODULE "LANGUAGE_CINGULATE_BRIDGE"


//=============================================================================
// Internal Constants
//=============================================================================

#define MAX_ERROR_HISTORY       64
#define MAX_CONFLICT_HISTORY    32
#define ERN_BASE_AMPLITUDE      1.0f
#define PE_BASE_AMPLITUDE       0.5f

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Error history entry
 */
typedef struct {
    speech_error_event_t error;
    bool is_valid;
} error_history_entry_t;

/**
 * @brief Conflict history entry
 */
typedef struct {
    language_conflict_event_t conflict;
    bool is_valid;
} conflict_history_entry_t;

/**
 * @brief Bridge internal structure
 */
struct language_cingulate_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    /* Configuration */
    language_cingulate_config_t config;

    /* Connected modules */
    language_orchestrator_t* language;
    cingulate_adapter_t* cingulate;
    broca_adapter_t* broca;
    wernicke_adapter_t* wernicke;
    bio_router_t router;

    /* State */
    lc_bridge_state_t state;
    monitor_state_t monitor_state;
    bool is_initialized;
    bool is_monitoring;

    /* Current monitoring */
    uint32_t current_utterance_id;
    float current_control_level;
    float current_speech_rate;

    /* Error history */
    error_history_entry_t error_history[MAX_ERROR_HISTORY];
    uint32_t error_history_head;
    uint32_t error_count;
    uint32_t next_error_id;

    /* Conflict history */
    conflict_history_entry_t conflict_history[MAX_CONFLICT_HISTORY];
    uint32_t conflict_history_head;
    uint32_t conflict_count;
    uint32_t next_conflict_id;

    /* Pending correction */
    correction_signal_t pending_correction;
    bool has_pending_correction;

    /* Timing */
    uint64_t last_update_ms;
    uint64_t last_error_time_ms;

    /* Statistics */
    language_cingulate_stats_t stats;

    /* Callbacks */
    lc_error_callback_t error_callback;
    void* error_callback_data;
    lc_conflict_callback_t conflict_callback;
    void* conflict_callback_data;
    lc_correction_callback_t correction_callback;
    void* correction_callback_data;
    lc_control_callback_t control_callback;
    void* control_callback_data;

    /* Logging */
    nimcp_log_context_t log_ctx;
};

//=============================================================================
// Helper Functions
//=============================================================================

static void add_error_to_history(language_cingulate_bridge_t* bridge,
                                  const speech_error_event_t* error)
{
    bridge->error_history[bridge->error_history_head].error = *error;
    bridge->error_history[bridge->error_history_head].is_valid = true;

    bridge->error_history_head = (bridge->error_history_head + 1) % MAX_ERROR_HISTORY;
    if (bridge->error_count < MAX_ERROR_HISTORY) {
        bridge->error_count++;
    }

    /* Update stats */
    bridge->stats.errors_detected++;
    if (error->type < SPEECH_ERROR_COUNT) {
        bridge->stats.errors_by_type[error->type]++;
    }

    /* Update running averages */
    float alpha = 0.1f;
    bridge->stats.avg_error_severity =
        (1.0f - alpha) * bridge->stats.avg_error_severity + alpha * error->severity;
    bridge->stats.avg_ern_amplitude =
        (1.0f - alpha) * bridge->stats.avg_ern_amplitude + alpha * error->ern_amplitude;

    if (error->is_conscious) {
        bridge->stats.error_awareness_rate =
            (1.0f - alpha) * bridge->stats.error_awareness_rate + alpha * 1.0f;
    } else {
        bridge->stats.error_awareness_rate =
            (1.0f - alpha) * bridge->stats.error_awareness_rate + alpha * 0.0f;
    }
}

static void add_conflict_to_history(language_cingulate_bridge_t* bridge,
                                     const language_conflict_event_t* conflict)
{
    bridge->conflict_history[bridge->conflict_history_head].conflict = *conflict;
    bridge->conflict_history[bridge->conflict_history_head].is_valid = true;

    bridge->conflict_history_head = (bridge->conflict_history_head + 1) % MAX_CONFLICT_HISTORY;
    if (bridge->conflict_count < MAX_CONFLICT_HISTORY) {
        bridge->conflict_count++;
    }

    /* Update stats */
    bridge->stats.conflicts_detected++;
    if (conflict->type < LANG_CONFLICT_COUNT) {
        bridge->stats.conflicts_by_type[conflict->type]++;
    }

    float alpha = 0.1f;
    bridge->stats.avg_conflict_level =
        (1.0f - alpha) * bridge->stats.avg_conflict_level + alpha * conflict->conflict_level;
}

static float compute_ern_amplitude(speech_error_type_t type, float severity)
{
    /* ERN amplitude varies by error type and severity */
    float base = ERN_BASE_AMPLITUDE;

    switch (type) {
    case SPEECH_ERROR_LEXICAL:
        base *= 1.2f;  /* Lexical errors generate strong ERN */
        break;
    case SPEECH_ERROR_PHONOLOGICAL:
        base *= 1.0f;
        break;
    case SPEECH_ERROR_SYNTACTIC:
        base *= 1.1f;
        break;
    case SPEECH_ERROR_SEMANTIC:
        base *= 1.3f;  /* Semantic errors highly salient */
        break;
    case SPEECH_ERROR_DISFLUENCY:
        base *= 0.8f;  /* Disfluencies less impactful */
        break;
    default:
        break;
    }

    return base * severity;
}

static float compute_pe_amplitude(float ern_amplitude, bool is_aware)
{
    /* Pe amplitude depends on ERN and awareness */
    if (is_aware) {
        return PE_BASE_AMPLITUDE + ern_amplitude * 0.5f;
    } else {
        return PE_BASE_AMPLITUDE * 0.3f;
    }
}

static bool should_trigger_correction(language_cingulate_bridge_t* bridge,
                                       const speech_error_event_t* error)
{
    if (!bridge->config.enable_self_correction) {
        return false;  /* Self-correction disabled - normal config */
    }

    /* Trigger correction if error severity exceeds threshold */
    if (error->severity >= bridge->config.correction_threshold) {
        return true;
    }

    /* Or if error is conscious (detected by speaker) */
    if (error->is_conscious && error->severity >= bridge->config.error_threshold) {
        return true;
    }

    return false;  /* Error does not meet correction threshold - normal behavior */
}

static correction_action_t determine_correction_action(speech_error_type_t type,
                                                        float severity)
{
    /* Determine appropriate correction based on error type and severity */
    if (severity >= 0.8f) {
        /* Severe errors require restart */
        return CORRECTION_RESTART;
    }

    switch (type) {
    case SPEECH_ERROR_LEXICAL:
        return CORRECTION_SUBSTITUTE;

    case SPEECH_ERROR_PHONOLOGICAL:
        if (severity >= 0.5f) {
            return CORRECTION_REPAIR;
        } else {
            return CORRECTION_NONE;  /* Minor mispronunciations often ignored */
        }

    case SPEECH_ERROR_SYNTACTIC:
        return CORRECTION_RESTART;  /* Grammar errors need restart */

    case SPEECH_ERROR_SEMANTIC:
        return CORRECTION_SUBSTITUTE;

    case SPEECH_ERROR_DISFLUENCY:
        return CORRECTION_HESITATE;

    case SPEECH_ERROR_TIMING:
        return CORRECTION_SLOW_DOWN;

    default:
        return CORRECTION_NONE;
    }
}

static void send_to_cingulate(language_cingulate_bridge_t* bridge,
                               const speech_error_event_t* error)
{
    if (!bridge->cingulate) {
        return;
    }

    /* Report error to cingulate as outcome feedback */
    cingulate_report_outcome(bridge->cingulate, 1.0f - error->severity, 1.0f);
}

static void generate_control_signal(language_cingulate_bridge_t* bridge,
                                     language_control_signal_t* signal)
{
    if (!bridge->cingulate) {
        /* Generate default signal */
        signal->control_level = bridge->current_control_level;
        signal->attention_boost = 0.0f;
        signal->rate_adjustment = bridge->current_speech_rate;
        signal->threshold_shift = 0.0f;
        signal->increase_monitoring = false;
        signal->slow_production = false;
        return;
    }

    /* Get control level from cingulate */
    signal->control_level = cingulate_get_control_level(bridge->cingulate);

    /* Generate full control signal */
    cingulate_control_signal_t cing_signal;
    if (cingulate_generate_control_signal(bridge->cingulate, &cing_signal)) {
        signal->attention_boost = cing_signal.attention_boost;
        signal->threshold_shift = cing_signal.threshold_shift;

        /* Check if we need to slow down */
        if (cing_signal.control_level > 0.7f) {
            signal->slow_production = true;
            signal->rate_adjustment = bridge->config.slowdown_factor;
        } else {
            signal->slow_production = false;
            signal->rate_adjustment = 1.0f;
        }

        signal->increase_monitoring = (cing_signal.control_level > 0.5f);
    }

    bridge->current_control_level = signal->control_level;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

language_cingulate_config_t language_cingulate_default_config(void)
{
    language_cingulate_config_t config;
    memset(&config, 0, sizeof(config));

    config.update_interval_ms = LC_DEFAULT_UPDATE_INTERVAL_MS;
    config.max_error_history = LC_DEFAULT_MAX_ERROR_HISTORY;

    config.error_threshold = LC_DEFAULT_ERROR_THRESHOLD;
    config.conflict_threshold = LC_DEFAULT_CONFLICT_THRESHOLD;
    config.correction_threshold = 0.5f;

    config.ern_window_ms = LC_DEFAULT_ERN_WINDOW_MS;
    config.slowdown_factor = LC_DEFAULT_SLOWDOWN_FACTOR;

    config.enable_error_detection = true;
    config.enable_conflict_monitoring = true;
    config.enable_self_correction = true;
    config.enable_rate_adaptation = true;
    config.enable_conscious_monitoring = true;

    config.enable_bio_async = true;

    return config;
}

language_cingulate_bridge_t* language_cingulate_bridge_create(
    language_orchestrator_t* language,
    cingulate_adapter_t* cingulate,
    const language_cingulate_config_t* config)
{
    language_cingulate_bridge_t* bridge = nimcp_calloc(1, sizeof(language_cingulate_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }

    /* Store configuration */
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = language_cingulate_default_config();
    }

    /* Store module references */
    bridge->language = language;
    bridge->cingulate = cingulate;
    bridge->broca = NULL;
    bridge->wernicke = NULL;
    bridge->router = NULL;

    /* Initialize state */
    bridge->state = LC_STATE_IDLE;
    bridge->monitor_state = MONITOR_STATE_IDLE;
    bridge->current_control_level = 0.5f;
    bridge->current_speech_rate = 1.0f;

    /* Initialize histories */
    memset(bridge->error_history, 0, sizeof(bridge->error_history));
    bridge->error_history_head = 0;
    bridge->error_count = 0;
    bridge->next_error_id = 1;

    memset(bridge->conflict_history, 0, sizeof(bridge->conflict_history));
    bridge->conflict_history_head = 0;
    bridge->conflict_count = 0;
    bridge->next_conflict_id = 1;

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    /* Create logging context */
    bridge->log_ctx = nimcp_log_context_create(NULL, LANGUAGE_CINGULATE_MODULE_NAME);

    bridge->is_initialized = true;

    if (bridge->log_ctx) {
        NIMCP_LOG_INFO("Language-Cingulate bridge created");
    }

    return bridge;
}

void language_cingulate_bridge_destroy(language_cingulate_bridge_t* bridge)
{
    if (!bridge) {
        return;
    }

    if (bridge->log_ctx) {
        NIMCP_LOG_INFO("Language-Cingulate bridge destroyed");
        nimcp_log_context_destroy(bridge->log_ctx);
    }

    nimcp_free(bridge);
}

int language_cingulate_bridge_reset(language_cingulate_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Reset state */
    bridge->state = LC_STATE_IDLE;
    bridge->monitor_state = MONITOR_STATE_IDLE;
    bridge->is_monitoring = false;
    bridge->current_control_level = 0.5f;
    bridge->current_speech_rate = 1.0f;

    /* Clear histories */
    memset(bridge->error_history, 0, sizeof(bridge->error_history));
    bridge->error_history_head = 0;
    bridge->error_count = 0;

    memset(bridge->conflict_history, 0, sizeof(bridge->conflict_history));
    bridge->conflict_history_head = 0;
    bridge->conflict_count = 0;

    /* Clear pending correction */
    bridge->has_pending_correction = false;

    return 0;
}

//=============================================================================
// Connection Functions
//=============================================================================

int language_cingulate_connect_broca(
    language_cingulate_bridge_t* bridge,
    broca_adapter_t* broca)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    bridge->broca = broca;

    if (bridge->log_ctx) {
        NIMCP_LOG_INFO("Connected to Broca's area adapter");
    }

    return 0;
}

int language_cingulate_connect_wernicke(
    language_cingulate_bridge_t* bridge,
    wernicke_adapter_t* wernicke)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    bridge->wernicke = wernicke;

    if (bridge->log_ctx) {
        NIMCP_LOG_INFO("Connected to Wernicke's area adapter");
    }

    return 0;
}

int language_cingulate_connect_bio_async(
    language_cingulate_bridge_t* bridge,
    bio_router_t router)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    bridge->router = router;

    if (bridge->log_ctx) {
        NIMCP_LOG_INFO("Connected to bio-async router");
    }

    return 0;
}

//=============================================================================
// Update Functions
//=============================================================================

int language_cingulate_bridge_update(
    language_cingulate_bridge_t* bridge,
    uint64_t timestamp_ms)
{
    if (!bridge || !bridge->is_initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cingulate_bridge_update: required parameter is NULL (bridge, bridge->is_initialized)");
        return -1;
    }

    bridge->last_update_ms = timestamp_ms;

    /* Process pending corrections */
    if (bridge->has_pending_correction && bridge->correction_callback) {
        bridge->correction_callback(&bridge->pending_correction,
                                    bridge->correction_callback_data);
        bridge->has_pending_correction = false;
        bridge->stats.corrections_triggered++;
    }

    /* Update control signal if monitoring */
    if (bridge->is_monitoring && bridge->control_callback) {
        language_control_signal_t signal;
        generate_control_signal(bridge, &signal);
        bridge->control_callback(&signal, bridge->control_callback_data);
    }

    /* Update state machine */
    if (bridge->has_pending_correction) {
        bridge->state = LC_STATE_CORRECTING;
    } else if (bridge->is_monitoring) {
        bridge->state = LC_STATE_MONITORING;
    } else {
        bridge->state = LC_STATE_IDLE;
    }

    /* Update stats */
    bridge->stats.bridge_state = bridge->state;
    bridge->stats.monitor_state = bridge->monitor_state;
    bridge->stats.current_control_level = bridge->current_control_level;

    return 0;
}

//=============================================================================
// Error Detection (Language -> Cingulate)
//=============================================================================

int language_cingulate_report_speech_event(
    language_cingulate_bridge_t* bridge,
    const speech_monitoring_report_t* report)
{
    if (!bridge || !report) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cingulate_report_speech_event: required parameter is NULL (bridge, report)");
        return -1;
    }

    bridge->stats.monitoring_events++;

    /* Check for potential conflicts */
    if (report->competing_options > 1 && bridge->config.enable_conflict_monitoring) {
        /* Multiple competing words - potential conflict */
        float conflict_level = 1.0f - report->production_confidence;

        if (conflict_level >= bridge->config.conflict_threshold) {
            language_conflict_event_t conflict;
            memset(&conflict, 0, sizeof(conflict));
            conflict.conflict_id = bridge->next_conflict_id++;
            conflict.type = LANG_CONFLICT_WORD_SELECTION;
            strncpy(conflict.option_a, report->current_word, sizeof(conflict.option_a) - 1);
            conflict.activation_a = report->production_confidence;
            conflict.activation_b = 1.0f - report->production_confidence;
            conflict.conflict_level = conflict_level;
            conflict.detection_time_ms = nimcp_time_now_us() / 1000;
            conflict.resolved = false;

            add_conflict_to_history(bridge, &conflict);
            bridge->monitor_state = MONITOR_STATE_CONFLICT_DETECTED;

            if (bridge->conflict_callback) {
                bridge->conflict_callback(&conflict, bridge->conflict_callback_data);
            }
        }
    }

    /* Check for high error likelihood */
    if (report->error_likelihood >= bridge->config.error_threshold) {
        bridge->monitor_state = MONITOR_STATE_ACTIVE;

        /* Increase monitoring in cingulate */
        if (bridge->cingulate) {
            cingulate_begin_monitoring(bridge->cingulate, report->competing_options);
        }
    }

    return 0;
}

int language_cingulate_report_error(
    language_cingulate_bridge_t* bridge,
    const speech_error_event_t* error)
{
    if (!bridge || !error) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cingulate_report_error: required parameter is NULL (bridge, error)");
        return -1;
    }

    speech_error_event_t enriched_error = *error;

    /* Assign ID if not set */
    if (enriched_error.error_id == 0) {
        enriched_error.error_id = bridge->next_error_id++;
    }

    /* Compute ERN amplitude if not set */
    if (enriched_error.ern_amplitude == 0.0f) {
        enriched_error.ern_amplitude = compute_ern_amplitude(error->type, error->severity);
    }

    /* Compute Pe amplitude */
    enriched_error.pe_amplitude = compute_pe_amplitude(enriched_error.ern_amplitude,
                                                        enriched_error.is_conscious);

    /* Set detection time */
    enriched_error.detection_time_ms = nimcp_time_now_us() / 1000;

    /* Add to history */
    add_error_to_history(bridge, &enriched_error);

    /* Update state */
    bridge->monitor_state = MONITOR_STATE_ERROR_DETECTED;
    bridge->last_error_time_ms = enriched_error.detection_time_ms;

    /* Send to cingulate */
    send_to_cingulate(bridge, &enriched_error);

    /* Invoke callback */
    if (bridge->error_callback) {
        bridge->error_callback(&enriched_error, bridge->error_callback_data);
    }

    /* Check if correction needed */
    if (should_trigger_correction(bridge, &enriched_error)) {
        correction_action_t action = determine_correction_action(error->type, error->severity);

        if (action != CORRECTION_NONE) {
            bridge->pending_correction.action = action;
            bridge->pending_correction.target_position = error->word_position;
            strncpy(bridge->pending_correction.correction_content,
                    error->intended,
                    sizeof(bridge->pending_correction.correction_content) - 1);
            bridge->pending_correction.urgency = error->severity;
            bridge->pending_correction.rate_adjustment = bridge->config.slowdown_factor;
            bridge->pending_correction.interrupt_current = (action == CORRECTION_RESTART);
            bridge->has_pending_correction = true;

            bridge->monitor_state = MONITOR_STATE_CORRECTING;
        }
    }

    /* Adjust speech rate if enabled */
    if (bridge->config.enable_rate_adaptation) {
        bridge->current_speech_rate *= bridge->config.slowdown_factor;
        if (bridge->current_speech_rate < 0.5f) {
            bridge->current_speech_rate = 0.5f;  /* Minimum rate */
        }
    }

    if (bridge->log_ctx) {
        NIMCP_LOG_DEBUG("Speech error reported: type=%d severity=%.2f",
                        error->type, error->severity);
    }

    return 0;
}

int language_cingulate_report_conflict(
    language_cingulate_bridge_t* bridge,
    const language_conflict_event_t* conflict)
{
    if (!bridge || !conflict) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cingulate_report_conflict: required parameter is NULL (bridge, conflict)");
        return -1;
    }

    language_conflict_event_t enriched_conflict = *conflict;

    /* Assign ID if not set */
    if (enriched_conflict.conflict_id == 0) {
        enriched_conflict.conflict_id = bridge->next_conflict_id++;
    }

    /* Set detection time */
    enriched_conflict.detection_time_ms = nimcp_time_now_us() / 1000;

    /* Add to history */
    add_conflict_to_history(bridge, &enriched_conflict);

    /* Update state */
    bridge->monitor_state = MONITOR_STATE_CONFLICT_DETECTED;

    /* Invoke callback */
    if (bridge->conflict_callback) {
        bridge->conflict_callback(&enriched_conflict, bridge->conflict_callback_data);
    }

    /* Send to cingulate for conflict monitoring */
    if (bridge->cingulate) {
        cingulate_response_option_t opt_a, opt_b;
        memset(&opt_a, 0, sizeof(opt_a));
        memset(&opt_b, 0, sizeof(opt_b));

        opt_a.option_id = 0;
        opt_a.activation = conflict->activation_a;
        opt_a.is_prepotent = (conflict->activation_a > conflict->activation_b);

        opt_b.option_id = 1;
        opt_b.activation = conflict->activation_b;
        opt_b.is_prepotent = (conflict->activation_b > conflict->activation_a);

        cingulate_begin_monitoring(bridge->cingulate, 2);
        cingulate_update_response(bridge->cingulate, &opt_a);
        cingulate_update_response(bridge->cingulate, &opt_b);
    }

    if (bridge->log_ctx) {
        NIMCP_LOG_DEBUG("Language conflict reported: type=%d level=%.2f",
                        conflict->type, conflict->conflict_level);
    }

    return 0;
}

int language_cingulate_report_mismatch(
    language_cingulate_bridge_t* bridge,
    const char* intended,
    const char* produced,
    speech_error_type_t type)
{
    if (!bridge || !intended || !produced) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cingulate_report_mismatch: required parameter is NULL (bridge, intended, produced)");
        return -1;
    }

    /* Create error event from mismatch */
    speech_error_event_t error;
    memset(&error, 0, sizeof(error));

    error.error_id = bridge->next_error_id++;
    error.type = type;
    strncpy(error.intended, intended, sizeof(error.intended) - 1);
    strncpy(error.produced, produced, sizeof(error.produced) - 1);

    /* Compute severity based on string difference */
    size_t len_diff = strlen(intended) > strlen(produced) ?
        strlen(intended) - strlen(produced) : strlen(produced) - strlen(intended);
    size_t max_len = strlen(intended) > strlen(produced) ?
        strlen(intended) : strlen(produced);

    error.severity = max_len > 0 ? (float)len_diff / (float)max_len : 0.0f;
    if (error.severity < 0.3f) error.severity = 0.3f;  /* Minimum severity */
    if (strcmp(intended, produced) != 0) {
        error.severity = 0.5f + error.severity * 0.5f;  /* Boost if different */
    }

    error.is_conscious = (error.severity >= 0.5f);  /* Larger errors more noticeable */

    return language_cingulate_report_error(bridge, &error);
}

//=============================================================================
// Error Correction (Cingulate -> Language)
//=============================================================================

int language_cingulate_request_correction(
    language_cingulate_bridge_t* bridge,
    correction_signal_t* signal)
{
    if (!bridge || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cingulate_request_correction: required parameter is NULL (bridge, signal)");
        return -1;
    }

    if (!bridge->has_pending_correction) {
        signal->action = CORRECTION_NONE;
        return 0;
    }

    *signal = bridge->pending_correction;
    bridge->has_pending_correction = false;
    bridge->stats.corrections_triggered++;

    return 0;
}

int language_cingulate_apply_control(
    language_cingulate_bridge_t* bridge,
    const language_control_signal_t* control)
{
    if (!bridge || !control) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cingulate_apply_control: required parameter is NULL (bridge, control)");
        return -1;
    }

    bridge->current_control_level = control->control_level;

    if (control->slow_production) {
        bridge->current_speech_rate = control->rate_adjustment;
    }

    /* Invoke callback */
    if (bridge->control_callback) {
        bridge->control_callback(control, bridge->control_callback_data);
    }

    return 0;
}

int language_cingulate_adjust_rate(
    language_cingulate_bridge_t* bridge,
    float rate_factor)
{
    if (!bridge || rate_factor <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "language_cingulate_adjust_rate: bridge is NULL");
        return -1;
    }

    bridge->current_speech_rate = rate_factor;

    /* Clamp to reasonable range */
    if (bridge->current_speech_rate < 0.5f) bridge->current_speech_rate = 0.5f;
    if (bridge->current_speech_rate > 2.0f) bridge->current_speech_rate = 2.0f;

    return 0;
}

//=============================================================================
// Monitoring Control
//=============================================================================

int language_cingulate_start_monitoring(
    language_cingulate_bridge_t* bridge,
    uint32_t utterance_id)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    bridge->is_monitoring = true;
    bridge->current_utterance_id = utterance_id;
    bridge->monitor_state = MONITOR_STATE_ACTIVE;
    bridge->state = LC_STATE_MONITORING;

    /* Reset speech rate for new utterance */
    bridge->current_speech_rate = 1.0f;

    if (bridge->log_ctx) {
        NIMCP_LOG_DEBUG("Started monitoring utterance %u", utterance_id);
    }

    return 0;
}

int language_cingulate_stop_monitoring(language_cingulate_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    bridge->is_monitoring = false;
    bridge->monitor_state = MONITOR_STATE_IDLE;
    bridge->state = LC_STATE_IDLE;

    return 0;
}

monitor_state_t language_cingulate_get_monitor_state(
    const language_cingulate_bridge_t* bridge)
{
    if (!bridge) {
        return MONITOR_STATE_IDLE;
    }

    return bridge->monitor_state;
}

float language_cingulate_get_control_level(
    const language_cingulate_bridge_t* bridge)
{
    if (!bridge) {
        return 0.5f;
    }

    return bridge->current_control_level;
}

//=============================================================================
// Error History
//=============================================================================

int language_cingulate_get_recent_errors(
    const language_cingulate_bridge_t* bridge,
    speech_error_event_t* errors,
    uint32_t max_errors)
{
    if (!bridge || !errors || max_errors == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cingulate_get_recent_errors: required parameter is NULL (bridge, errors)");
        return -1;
    }

    uint32_t count = 0;
    uint32_t idx = bridge->error_history_head;

    /* Go backwards through history */
    for (uint32_t i = 0; i < bridge->error_count && count < max_errors; i++) {
        idx = (idx + MAX_ERROR_HISTORY - 1) % MAX_ERROR_HISTORY;
        if (bridge->error_history[idx].is_valid) {
            errors[count++] = bridge->error_history[idx].error;
        }
    }

    return (int)count;
}

int language_cingulate_get_recent_conflicts(
    const language_cingulate_bridge_t* bridge,
    language_conflict_event_t* conflicts,
    uint32_t max_conflicts)
{
    if (!bridge || !conflicts || max_conflicts == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cingulate_get_recent_conflicts: required parameter is NULL (bridge, conflicts)");
        return -1;
    }

    uint32_t count = 0;
    uint32_t idx = bridge->conflict_history_head;

    for (uint32_t i = 0; i < bridge->conflict_count && count < max_conflicts; i++) {
        idx = (idx + MAX_CONFLICT_HISTORY - 1) % MAX_CONFLICT_HISTORY;
        if (bridge->conflict_history[idx].is_valid) {
            conflicts[count++] = bridge->conflict_history[idx].conflict;
        }
    }

    return (int)count;
}

bool language_cingulate_has_pending_error(const language_cingulate_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }

    return bridge->has_pending_correction;
}

//=============================================================================
// Callback Registration
//=============================================================================

int language_cingulate_set_error_callback(
    language_cingulate_bridge_t* bridge,
    lc_error_callback_t callback,
    void* user_data)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    bridge->error_callback = callback;
    bridge->error_callback_data = user_data;
    return 0;
}

int language_cingulate_set_conflict_callback(
    language_cingulate_bridge_t* bridge,
    lc_conflict_callback_t callback,
    void* user_data)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    bridge->conflict_callback = callback;
    bridge->conflict_callback_data = user_data;
    return 0;
}

int language_cingulate_set_correction_callback(
    language_cingulate_bridge_t* bridge,
    lc_correction_callback_t callback,
    void* user_data)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    bridge->correction_callback = callback;
    bridge->correction_callback_data = user_data;
    return 0;
}

int language_cingulate_set_control_callback(
    language_cingulate_bridge_t* bridge,
    lc_control_callback_t callback,
    void* user_data)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    bridge->control_callback = callback;
    bridge->control_callback_data = user_data;
    return 0;
}

//=============================================================================
// Status and Statistics
//=============================================================================

lc_bridge_state_t language_cingulate_get_state(
    const language_cingulate_bridge_t* bridge)
{
    if (!bridge) {
        return LC_STATE_ERROR;
    }

    return bridge->state;
}

int language_cingulate_get_stats(
    const language_cingulate_bridge_t* bridge,
    language_cingulate_stats_t* stats)
{
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cingulate_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}

void language_cingulate_reset_stats(language_cingulate_bridge_t* bridge)
{
    if (!bridge) {
        return;
    }

    memset(&bridge->stats, 0, sizeof(bridge->stats));
}

int language_cingulate_get_config(
    const language_cingulate_bridge_t* bridge,
    language_cingulate_config_t* config)
{
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cingulate_get_config: required parameter is NULL (bridge, config)");
        return -1;
    }

    *config = bridge->config;
    return 0;
}

int language_cingulate_set_config(
    language_cingulate_bridge_t* bridge,
    const language_cingulate_config_t* config)
{
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_cingulate_set_config: required parameter is NULL (bridge, config)");
        return -1;
    }

    bridge->config = *config;
    return 0;
}
