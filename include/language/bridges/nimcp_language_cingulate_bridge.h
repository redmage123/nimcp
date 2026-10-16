//=============================================================================
// nimcp_language_cingulate_bridge.h - Language-Cingulate Error Monitoring Bridge
//=============================================================================
/**
 * @file nimcp_language_cingulate_bridge.h
 * @brief Bidirectional bridge integrating Language Layer with Cingulate Cortex
 *
 * WHAT: Bridge connecting Language Layer with Anterior Cingulate for error
 *       monitoring, conflict detection, and speech self-correction
 * WHY:  Enable detection and correction of speech errors, word selection
 *       conflicts, and fluency disruptions through ACC error monitoring
 * HOW:  Language reports production events; Cingulate detects errors and
 *       generates control signals for adaptive speech adjustment
 *
 * BIOLOGICAL BASIS:
 * - Anterior Cingulate Cortex (ACC): Error and conflict monitoring
 * - Dorsal ACC (dACC): Speech error detection, ERN generation
 * - Rostral ACC (rACC): Emotional aspects of speech
 * - Error-Related Negativity (ERN): ~50-100ms post-error
 * - Pe (Error Positivity): ~200-500ms, conscious error awareness
 *
 * KEY CONNECTIONS:
 * - Language → Cingulate: Speech production events, error reports
 * - Cingulate → Language: Error signals, control adjustments, slow-down
 * - Broca → ACC: Word selection conflicts, syntactic ambiguity
 * - Wernicke → ACC: Comprehension errors, semantic mismatches
 *
 * ERROR MONITORING FUNCTIONS:
 * - Speech error detection: Wrong word, mispronunciation, disfluency
 * - Conflict monitoring: Competing word choices, syntactic alternatives
 * - Self-monitoring: Compare intended vs produced speech
 * - Error correction: Trigger repairs, restarts, hesitations
 * - Fluency control: Adjust speech rate based on error likelihood
 *
 * @version 1.0.0 - Phase LC1: Language-Cingulate Integration
 * @author NIMCP Development Team
 * @date 2026-01-05
 */

#ifndef NIMCP_LANGUAGE_CINGULATE_BRIDGE_H
#define NIMCP_LANGUAGE_CINGULATE_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

/* Include cingulate adapter first to avoid include order issues */
#include "core/brain/regions/cingulate/nimcp_cingulate_adapter.h"

/* Language types after bio_messages */
#include "language/nimcp_language_types.h"
#include "language/nimcp_language_config.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct language_cingulate_bridge language_cingulate_bridge_t;
typedef struct language_orchestrator language_orchestrator_t;
typedef struct broca_adapter broca_adapter_t;
typedef struct wernicke_adapter wernicke_adapter_t;

#ifndef NIMCP_BIO_ROUTER_H
typedef void* bio_router_t;
#endif

//=============================================================================
// Constants
//=============================================================================

#define LANGUAGE_CINGULATE_MODULE_NAME      "language_cingulate_bridge"
#define LANGUAGE_CINGULATE_MODULE_VERSION   "1.0.0"
#define LANGUAGE_CINGULATE_BIO_MODULE_ID    0x0823

/* Default configuration values */
#define LC_DEFAULT_UPDATE_INTERVAL_MS        20
#define LC_DEFAULT_MAX_ERROR_HISTORY         32
#define LC_DEFAULT_ERROR_THRESHOLD           0.3f
#define LC_DEFAULT_CONFLICT_THRESHOLD        0.5f
#define LC_DEFAULT_ERN_WINDOW_MS             100.0f
#define LC_DEFAULT_SLOWDOWN_FACTOR           0.8f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Speech error type
 */
typedef enum {
    SPEECH_ERROR_NONE = 0,                /**< No error */
    SPEECH_ERROR_LEXICAL,                 /**< Wrong word selected */
    SPEECH_ERROR_PHONOLOGICAL,            /**< Phoneme substitution/omission */
    SPEECH_ERROR_SYNTACTIC,               /**< Grammar error */
    SPEECH_ERROR_SEMANTIC,                /**< Meaning mismatch */
    SPEECH_ERROR_DISFLUENCY,              /**< Hesitation, restart, repetition */
    SPEECH_ERROR_TIMING,                  /**< Timing/rhythm error */
    SPEECH_ERROR_PROSODY,                 /**< Intonation error */
    SPEECH_ERROR_COUNT
} speech_error_type_t;

/**
 * @brief Language conflict type
 */
typedef enum {
    LANG_CONFLICT_NONE = 0,               /**< No conflict */
    LANG_CONFLICT_WORD_SELECTION,         /**< Multiple word candidates */
    LANG_CONFLICT_SYNTACTIC,              /**< Multiple parse options */
    LANG_CONFLICT_SEMANTIC,               /**< Ambiguous meaning */
    LANG_CONFLICT_PRAGMATIC,              /**< Contextual ambiguity */
    LANG_CONFLICT_REGISTER,               /**< Formality level conflict */
    LANG_CONFLICT_BILINGUAL,              /**< Cross-language conflict */
    LANG_CONFLICT_COUNT
} language_conflict_type_t;

/**
 * @brief Error correction action
 */
typedef enum {
    CORRECTION_NONE = 0,                  /**< No correction needed */
    CORRECTION_RESTART,                   /**< Restart utterance */
    CORRECTION_REPAIR,                    /**< Repair in-place */
    CORRECTION_HESITATE,                  /**< Insert hesitation */
    CORRECTION_SLOW_DOWN,                 /**< Reduce speech rate */
    CORRECTION_SUBSTITUTE,                /**< Substitute word */
    CORRECTION_OMIT,                      /**< Omit problematic element */
    CORRECTION_COUNT
} correction_action_t;

/**
 * @brief Monitoring state
 */
typedef enum {
    MONITOR_STATE_IDLE = 0,               /**< Not monitoring */
    MONITOR_STATE_ACTIVE,                 /**< Active monitoring */
    MONITOR_STATE_ERROR_DETECTED,         /**< Error detected */
    MONITOR_STATE_CONFLICT_DETECTED,      /**< Conflict detected */
    MONITOR_STATE_CORRECTING,             /**< Correction in progress */
    MONITOR_STATE_COUNT
} monitor_state_t;

/**
 * @brief Bridge operating state
 */
typedef enum {
    LC_STATE_IDLE = 0,                    /**< No active processing */
    LC_STATE_MONITORING,                  /**< Monitoring speech */
    LC_STATE_ERROR_PROCESSING,            /**< Processing error */
    LC_STATE_CORRECTING,                  /**< Applying correction */
    LC_STATE_ERROR,                       /**< Error state */
    LC_STATE_COUNT
} lc_bridge_state_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Configuration for Language-Cingulate bridge
 */
typedef struct {
    /* Operating parameters */
    uint32_t update_interval_ms;          /**< Update cycle interval */
    uint32_t max_error_history;           /**< Maximum error history size */

    /* Thresholds */
    float error_threshold;                /**< Error detection threshold [0-1] */
    float conflict_threshold;             /**< Conflict detection threshold [0-1] */
    float correction_threshold;           /**< Threshold to trigger correction */

    /* Timing */
    float ern_window_ms;                  /**< ERN detection window */
    float slowdown_factor;                /**< Rate reduction on error */

    /* Features */
    bool enable_error_detection;          /**< Enable error monitoring */
    bool enable_conflict_monitoring;      /**< Enable conflict detection */
    bool enable_self_correction;          /**< Enable automatic correction */
    bool enable_rate_adaptation;          /**< Enable speech rate adjustment */
    bool enable_conscious_monitoring;     /**< Enable Pe (conscious) errors */

    /* Bio-async */
    bool enable_bio_async;                /**< Enable bio-async messaging */
} language_cingulate_config_t;

/**
 * @brief Speech error event
 */
typedef struct {
    uint32_t error_id;                    /**< Error identifier */
    speech_error_type_t type;             /**< Error type */
    char intended[64];                    /**< Intended speech element */
    char produced[64];                    /**< Actually produced */
    float severity;                       /**< Error severity [0-1] */
    float ern_amplitude;                  /**< ERN amplitude */
    float pe_amplitude;                   /**< Pe amplitude (conscious) */
    uint64_t detection_time_ms;           /**< When detected */
    uint32_t word_position;               /**< Position in utterance */
    bool is_conscious;                    /**< Reached conscious awareness */
    bool was_corrected;                   /**< Already corrected */
} speech_error_event_t;

/**
 * @brief Language conflict event
 */
typedef struct {
    uint32_t conflict_id;                 /**< Conflict identifier */
    language_conflict_type_t type;        /**< Conflict type */
    char option_a[64];                    /**< First option */
    char option_b[64];                    /**< Second option */
    float activation_a;                   /**< Activation of option A */
    float activation_b;                   /**< Activation of option B */
    float conflict_level;                 /**< Conflict intensity [0-1] */
    uint64_t detection_time_ms;           /**< When detected */
    bool resolved;                        /**< Was conflict resolved */
    uint8_t selected_option;              /**< 0=A, 1=B, 2=neither */
} language_conflict_event_t;

/**
 * @brief Correction signal from Cingulate to Language
 */
typedef struct {
    correction_action_t action;           /**< Correction action type */
    uint32_t target_position;             /**< Target position in utterance */
    char correction_content[64];          /**< Corrected content */
    float urgency;                        /**< Correction urgency [0-1] */
    float rate_adjustment;                /**< Speech rate adjustment factor */
    bool interrupt_current;               /**< Interrupt current production */
} correction_signal_t;

/**
 * @brief Speech monitoring report to Cingulate
 */
typedef struct {
    uint32_t utterance_id;                /**< Current utterance */
    uint32_t word_position;               /**< Current word position */
    char current_word[32];                /**< Currently producing */
    float production_confidence;          /**< Confidence in production */
    float fluency_score;                  /**< Current fluency [0-1] */
    float error_likelihood;               /**< Predicted error probability */
    uint32_t competing_options;           /**< Number of competing words */
    bool needs_monitoring;                /**< Needs extra attention */
} speech_monitoring_report_t;

/**
 * @brief Control signal from Cingulate
 */
typedef struct {
    float control_level;                  /**< Current control level [0-1] */
    float attention_boost;                /**< Attention enhancement */
    float rate_adjustment;                /**< Speech rate factor */
    float threshold_shift;                /**< Response threshold change */
    bool increase_monitoring;             /**< Request more monitoring */
    bool slow_production;                 /**< Slow down speech */
} language_control_signal_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Counts */
    uint64_t errors_detected;             /**< Speech errors detected */
    uint64_t conflicts_detected;          /**< Conflicts detected */
    uint64_t corrections_triggered;       /**< Corrections initiated */
    uint64_t successful_corrections;      /**< Successful corrections */
    uint64_t monitoring_events;           /**< Total monitoring events */

    /* Error breakdown */
    uint64_t errors_by_type[SPEECH_ERROR_COUNT];  /**< Errors by type */
    uint64_t conflicts_by_type[LANG_CONFLICT_COUNT]; /**< Conflicts by type */

    /* Quality */
    float avg_error_severity;             /**< Average error severity */
    float avg_conflict_level;             /**< Average conflict level */
    float avg_ern_amplitude;              /**< Average ERN amplitude */
    float error_awareness_rate;           /**< Rate of conscious errors */
    float correction_success_rate;        /**< Correction success rate */

    /* Current state */
    monitor_state_t monitor_state;        /**< Current monitor state */
    lc_bridge_state_t bridge_state;       /**< Current bridge state */
    float current_control_level;          /**< Current control level */
} language_cingulate_stats_t;

//=============================================================================
// Callback Types
//=============================================================================

typedef void (*lc_error_callback_t)(const speech_error_event_t* error, void* user_data);
typedef void (*lc_conflict_callback_t)(const language_conflict_event_t* conflict, void* user_data);
typedef void (*lc_correction_callback_t)(const correction_signal_t* signal, void* user_data);
typedef void (*lc_control_callback_t)(const language_control_signal_t* signal, void* user_data);

//=============================================================================
// Lifecycle Functions
//=============================================================================

language_cingulate_config_t language_cingulate_default_config(void);

language_cingulate_bridge_t* language_cingulate_bridge_create(
    language_orchestrator_t* language,
    cingulate_adapter_t* cingulate,
    const language_cingulate_config_t* config
);

void language_cingulate_bridge_destroy(language_cingulate_bridge_t* bridge);

int language_cingulate_bridge_reset(language_cingulate_bridge_t* bridge);

//=============================================================================
// Connection Functions
//=============================================================================

int language_cingulate_connect_broca(
    language_cingulate_bridge_t* bridge,
    broca_adapter_t* broca
);

int language_cingulate_connect_wernicke(
    language_cingulate_bridge_t* bridge,
    wernicke_adapter_t* wernicke
);

int language_cingulate_connect_bio_async(
    language_cingulate_bridge_t* bridge,
    bio_router_t router
);

//=============================================================================
// Update Functions
//=============================================================================

int language_cingulate_bridge_update(
    language_cingulate_bridge_t* bridge,
    uint64_t timestamp_ms
);

//=============================================================================
// Error Detection (Language -> Cingulate)
//=============================================================================

int language_cingulate_report_speech_event(
    language_cingulate_bridge_t* bridge,
    const speech_monitoring_report_t* report
);

int language_cingulate_report_error(
    language_cingulate_bridge_t* bridge,
    const speech_error_event_t* error
);

int language_cingulate_report_conflict(
    language_cingulate_bridge_t* bridge,
    const language_conflict_event_t* conflict
);

int language_cingulate_report_mismatch(
    language_cingulate_bridge_t* bridge,
    const char* intended,
    const char* produced,
    speech_error_type_t type
);

//=============================================================================
// Error Correction (Cingulate -> Language)
//=============================================================================

int language_cingulate_request_correction(
    language_cingulate_bridge_t* bridge,
    correction_signal_t* signal
);

int language_cingulate_apply_control(
    language_cingulate_bridge_t* bridge,
    const language_control_signal_t* control
);

int language_cingulate_adjust_rate(
    language_cingulate_bridge_t* bridge,
    float rate_factor
);

//=============================================================================
// Monitoring Control
//=============================================================================

int language_cingulate_start_monitoring(
    language_cingulate_bridge_t* bridge,
    uint32_t utterance_id
);

int language_cingulate_stop_monitoring(
    language_cingulate_bridge_t* bridge
);

monitor_state_t language_cingulate_get_monitor_state(
    const language_cingulate_bridge_t* bridge
);

float language_cingulate_get_control_level(
    const language_cingulate_bridge_t* bridge
);

//=============================================================================
// Error History
//=============================================================================

int language_cingulate_get_recent_errors(
    const language_cingulate_bridge_t* bridge,
    speech_error_event_t* errors,
    uint32_t max_errors
);

int language_cingulate_get_recent_conflicts(
    const language_cingulate_bridge_t* bridge,
    language_conflict_event_t* conflicts,
    uint32_t max_conflicts
);

bool language_cingulate_has_pending_error(
    const language_cingulate_bridge_t* bridge
);

//=============================================================================
// Callback Registration
//=============================================================================

int language_cingulate_set_error_callback(
    language_cingulate_bridge_t* bridge,
    lc_error_callback_t callback,
    void* user_data
);

int language_cingulate_set_conflict_callback(
    language_cingulate_bridge_t* bridge,
    lc_conflict_callback_t callback,
    void* user_data
);

int language_cingulate_set_correction_callback(
    language_cingulate_bridge_t* bridge,
    lc_correction_callback_t callback,
    void* user_data
);

int language_cingulate_set_control_callback(
    language_cingulate_bridge_t* bridge,
    lc_control_callback_t callback,
    void* user_data
);

//=============================================================================
// Status and Statistics
//=============================================================================

lc_bridge_state_t language_cingulate_get_state(
    const language_cingulate_bridge_t* bridge
);

int language_cingulate_get_stats(
    const language_cingulate_bridge_t* bridge,
    language_cingulate_stats_t* stats
);

void language_cingulate_reset_stats(language_cingulate_bridge_t* bridge);

int language_cingulate_get_config(
    const language_cingulate_bridge_t* bridge,
    language_cingulate_config_t* config
);

int language_cingulate_set_config(
    language_cingulate_bridge_t* bridge,
    const language_cingulate_config_t* config
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LANGUAGE_CINGULATE_BRIDGE_H */
