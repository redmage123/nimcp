/**
 * @file nimcp_neuromodulatory_logging_bridge.h
 * @brief Unified Neuromodulatory-Logging System Integration Bridge
 * @version 1.0.0
 * @date 2026-01-11
 *
 * WHAT: Bidirectional bridge connecting all neuromodulatory centers (LC, VTA, Raphe, Habenula)
 *       to the logging system for comprehensive audit trails and pattern analysis.
 *
 * WHY: Neuromodulatory state changes are critical for understanding system behavior:
 *      - LC events: Log arousal changes, phasic bursts, stress responses
 *      - VTA events: Log reward signals, RPE, motivation changes
 *      - Raphe events: Log mood changes, impulse control, patience
 *      - Habenula events: Log aversive signals, punishment, avoidance
 *      - Enables post-hoc analysis of neuromodulatory patterns
 *      - Supports debugging and optimization of neuromodulatory dynamics
 *
 * HOW: Neuromodulatory events are formatted and logged with appropriate severity;
 *      log analysis can feed back patterns to neuromodulatory systems for learning.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * NEUROLOGICAL RECORDING ANALOGY:
 * This bridge mirrors techniques used in neuroscience research:
 *
 * 1. Single-Unit Recording (LC neurons):
 *    - Phasic bursts during novel/unexpected events
 *    - Tonic activity levels reflecting arousal state
 *    - Logged for correlation with behavior
 *
 * 2. Voltammetry (DA release):
 *    - Sub-second DA concentration changes
 *    - RPE encoding captured in release patterns
 *    - Logged for learning/reward analysis
 *
 * 3. Microdialysis (5-HT levels):
 *    - Extracellular 5-HT concentration over time
 *    - Mood/state correlations
 *    - Logged for mood tracking
 *
 * LOG CATEGORIES:
 * - NEUROMOD_LC:  Locus coeruleus NE events (arousal, attention, stress)
 * - NEUROMOD_VTA: VTA dopamine events (reward, motivation, learning)
 * - NEUROMOD_RAPHE: Raphe serotonin events (mood, patience, inhibition)
 * - NEUROMOD_HAB: Habenula events (aversion, punishment, avoidance)
 * - NEUROMOD_SYNC: Cross-neuromodulator synchronization events
 *
 * ARCHITECTURE:
 * ```
 * +===========================================================================+
 * |          NEUROMODULATORY-LOGGING UNIFIED BRIDGE                           |
 * +===========================================================================+
 * |                                                                           |
 * |   NEUROMODULATORY CENTERS              LOGGING SYSTEM                     |
 * |   +-------------------+                +-------------------+              |
 * |   | LC Events         |--------------->| Arousal Logs     |              |
 * |   | - Phasic bursts   |                | - Timestamps     |              |
 * |   | - Arousal changes |                | - Correlations   |              |
 * |   +-------------------+                +-------------------+              |
 * |   +-------------------+                +-------------------+              |
 * |   | VTA Events        |--------------->| Reward Logs      |              |
 * |   | - RPE signals     |                | - Learning trace |              |
 * |   | - Motivation      |                | - Value updates  |              |
 * |   +-------------------+                +-------------------+              |
 * |   +-------------------+                +-------------------+              |
 * |   | Raphe Events      |--------------->| Mood Logs        |              |
 * |   | - Mood changes    |                | - Patience trace |              |
 * |   | - Impulse control |                | - Social context |              |
 * |   +-------------------+                +-------------------+              |
 * |   +-------------------+                +-------------------+              |
 * |   | Habenula Events   |--------------->| Aversion Logs    |              |
 * |   | - Punishment      |                | - Avoidance trace|              |
 * |   | - Disappointment  |                | - Negative RPE   |              |
 * |   +-------------------+                +-------------------+              |
 * |                                                                           |
 * |                    PATTERN ANALYSIS FEEDBACK                              |
 * |   +-------------------+                +-------------------+              |
 * |   | Pattern Detector  |<---------------| Log Analysis     |              |
 * |   | - Anomaly alerts  |                | - Trend detection|              |
 * |   | - State prediction|                | - Correlation    |              |
 * |   +-------------------+                +-------------------+              |
 * |                                                                           |
 * +===========================================================================+
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_NEUROMODULATORY_LOGGING_BRIDGE_H
#define NIMCP_NEUROMODULATORY_LOGGING_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define NEUROMOD_LOG_MAX_MESSAGE_LEN        512
#define NEUROMOD_LOG_MAX_EVENT_BUFFER       256
#define NEUROMOD_LOG_DEFAULT_FLUSH_MS       100

/* Magic number for validation */
#define NEUROMOD_LOGGING_BRIDGE_MAGIC       0x4E4C4F47  /* "NLOG" */

/* Log level thresholds for neuromodulatory events */
#define NEUROMOD_LOG_THRESHOLD_PHASIC       0.7f    /* Phasic burst logging */
#define NEUROMOD_LOG_THRESHOLD_CRITICAL     0.9f    /* Critical event logging */
#define NEUROMOD_LOG_THRESHOLD_CHANGE       0.1f    /* Minimum change for logging */

/* ============================================================================
 * Log Categories and Levels
 * ============================================================================ */

typedef enum {
    NEUROMOD_LOG_CAT_LC = 0,        /**< Locus Coeruleus events */
    NEUROMOD_LOG_CAT_VTA,           /**< VTA events */
    NEUROMOD_LOG_CAT_RAPHE,         /**< Raphe nuclei events */
    NEUROMOD_LOG_CAT_HABENULA,      /**< Habenula events */
    NEUROMOD_LOG_CAT_SYNC,          /**< Cross-neuromodulator sync */
    NEUROMOD_LOG_CAT_ANALYSIS,      /**< Log analysis/pattern events */
    NEUROMOD_LOG_CAT_COUNT
} neuromod_log_category_t;

typedef enum {
    NEUROMOD_LOG_LEVEL_TRACE = 0,   /**< Detailed trace (all changes) */
    NEUROMOD_LOG_LEVEL_DEBUG,       /**< Debug info (significant changes) */
    NEUROMOD_LOG_LEVEL_INFO,        /**< Informational (state updates) */
    NEUROMOD_LOG_LEVEL_WARN,        /**< Warning (unusual patterns) */
    NEUROMOD_LOG_LEVEL_ERROR,       /**< Error (anomalous states) */
    NEUROMOD_LOG_LEVEL_CRITICAL,    /**< Critical (emergency states) */
    NEUROMOD_LOG_LEVEL_COUNT
} neuromod_log_level_t;

/* ============================================================================
 * Log Event Types
 * ============================================================================ */

typedef enum {
    /* LC log events */
    NEUROMOD_LOG_EVENT_AROUSAL_CHANGE = 0,
    NEUROMOD_LOG_EVENT_PHASIC_BURST,
    NEUROMOD_LOG_EVENT_TONIC_SHIFT,
    NEUROMOD_LOG_EVENT_STRESS_ONSET,
    NEUROMOD_LOG_EVENT_STRESS_OFFSET,
    NEUROMOD_LOG_EVENT_GAIN_CHANGE,

    /* VTA log events */
    NEUROMOD_LOG_EVENT_RPE_POSITIVE,
    NEUROMOD_LOG_EVENT_RPE_NEGATIVE,
    NEUROMOD_LOG_EVENT_MOTIVATION_CHANGE,
    NEUROMOD_LOG_EVENT_VALUE_UPDATE,
    NEUROMOD_LOG_EVENT_REWARD_RECEIVED,
    NEUROMOD_LOG_EVENT_REWARD_OMITTED,

    /* Raphe log events */
    NEUROMOD_LOG_EVENT_MOOD_POSITIVE,
    NEUROMOD_LOG_EVENT_MOOD_NEGATIVE,
    NEUROMOD_LOG_EVENT_PATIENCE_CHANGE,
    NEUROMOD_LOG_EVENT_IMPULSE_BLOCKED,
    NEUROMOD_LOG_EVENT_SOCIAL_CONTEXT,

    /* Habenula log events */
    NEUROMOD_LOG_EVENT_PUNISHMENT_DETECTED,
    NEUROMOD_LOG_EVENT_DISAPPOINTMENT,
    NEUROMOD_LOG_EVENT_AVOIDANCE_TRIGGERED,
    NEUROMOD_LOG_EVENT_NEGATIVE_RPE_HAB,

    /* Sync events */
    NEUROMOD_LOG_EVENT_NE_DA_SYNC,
    NEUROMOD_LOG_EVENT_DA_5HT_SYNC,
    NEUROMOD_LOG_EVENT_FULL_SYNC,

    /* Analysis events */
    NEUROMOD_LOG_EVENT_PATTERN_DETECTED,
    NEUROMOD_LOG_EVENT_ANOMALY_DETECTED,
    NEUROMOD_LOG_EVENT_TREND_IDENTIFIED,

    NEUROMOD_LOG_EVENT_COUNT
} neuromod_log_event_t;

/* ============================================================================
 * Log Entry Structure
 * ============================================================================ */

/**
 * @brief Single log entry for neuromodulatory event
 */
typedef struct {
    neuromod_log_category_t category;
    neuromod_log_level_t level;
    neuromod_log_event_t event_type;

    /* Neuromodulator state at time of event */
    float ne_level;
    float da_level;
    float ht_level;
    float hab_level;

    /* Event-specific data */
    float primary_value;            /**< Main value (e.g., RPE, arousal) */
    float secondary_value;          /**< Secondary value if applicable */
    uint32_t context_id;            /**< Context identifier */

    /* Message */
    char message[NEUROMOD_LOG_MAX_MESSAGE_LEN];

    /* Timing */
    uint64_t timestamp_us;
    uint64_t sequence_number;
} neuromod_log_entry_t;

/* ============================================================================
 * Log Analysis Feedback
 * ============================================================================ */

/**
 * @brief Pattern detected from log analysis
 */
typedef struct {
    neuromod_log_event_t pattern_type;
    float confidence;               /**< Pattern confidence [0-1] */
    float frequency;                /**< How often pattern occurs */
    uint32_t occurrence_count;      /**< Total occurrences */
    char description[256];          /**< Pattern description */
    uint64_t first_seen_us;
    uint64_t last_seen_us;
} neuromod_log_pattern_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

typedef struct {
    /* Enable logging per category */
    bool enable_lc_logging;
    bool enable_vta_logging;
    bool enable_raphe_logging;
    bool enable_habenula_logging;
    bool enable_sync_logging;

    /* Log level filter */
    neuromod_log_level_t min_log_level;

    /* Change thresholds for logging */
    float arousal_change_threshold;
    float rpe_logging_threshold;
    float mood_change_threshold;
    float hab_change_threshold;

    /* Buffer and timing */
    uint32_t log_buffer_size;
    float flush_interval_ms;

    /* Pattern analysis */
    bool enable_pattern_analysis;
    float pattern_confidence_threshold;

    /* Output format */
    bool include_timestamps;
    bool include_neuromod_state;
    bool json_format;
} neuromod_logging_bridge_config_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

typedef struct {
    /* Events logged per category */
    uint32_t lc_events_logged;
    uint32_t vta_events_logged;
    uint32_t raphe_events_logged;
    uint32_t habenula_events_logged;
    uint32_t sync_events_logged;

    /* Level distribution */
    uint32_t trace_events;
    uint32_t debug_events;
    uint32_t info_events;
    uint32_t warn_events;
    uint32_t error_events;
    uint32_t critical_events;

    /* Pattern analysis */
    uint32_t patterns_detected;
    uint32_t anomalies_detected;

    /* Buffer stats */
    uint32_t total_events_logged;
    uint32_t events_dropped;        /**< Buffer overflow drops */
    uint32_t flushes_performed;

    uint64_t last_log_time_us;
} neuromod_logging_bridge_stats_t;

/* ============================================================================
 * Bridge Handle
 * ============================================================================ */

typedef struct neuromod_logging_bridge_struct neuromod_logging_bridge_t;

/* Forward declarations for adapters */
#ifndef NIMCP_LC_ADAPTER_H
typedef struct nimcp_lc_adapter_struct* nimcp_lc_adapter_t;
#endif

#ifndef NIMCP_VTA_ADAPTER_H
typedef struct nimcp_vta_adapter_struct* nimcp_vta_adapter_t;
#endif

#ifndef NIMCP_RAPHE_ADAPTER_H
typedef struct nimcp_raphe_adapter_struct* nimcp_raphe_adapter_t;
#endif

#ifndef NIMCP_HABENULA_ADAPTER_H
typedef struct nimcp_habenula_adapter_struct* nimcp_habenula_adapter_t;
#endif

/* Logging system handle (opaque) */
typedef struct nimcp_logging_context_struct* nimcp_logging_context_t;

/* ============================================================================
 * API Functions
 * ============================================================================ */

/* Lifecycle */
int neuromod_logging_bridge_default_config(neuromod_logging_bridge_config_t* config);
neuromod_logging_bridge_t* neuromod_logging_bridge_create(const neuromod_logging_bridge_config_t* config);
void neuromod_logging_bridge_destroy(neuromod_logging_bridge_t* bridge);

/* Connection */
int neuromod_logging_bridge_connect_logging(neuromod_logging_bridge_t* bridge, nimcp_logging_context_t logging);
int neuromod_logging_bridge_disconnect(neuromod_logging_bridge_t* bridge);
bool neuromod_logging_bridge_is_connected(const neuromod_logging_bridge_t* bridge);

/* Adapter registration */
int neuromod_logging_bridge_register_lc(neuromod_logging_bridge_t* bridge, nimcp_lc_adapter_t adapter);
int neuromod_logging_bridge_register_vta(neuromod_logging_bridge_t* bridge, nimcp_vta_adapter_t adapter);
int neuromod_logging_bridge_register_raphe(neuromod_logging_bridge_t* bridge, nimcp_raphe_adapter_t adapter);
int neuromod_logging_bridge_register_habenula(neuromod_logging_bridge_t* bridge, nimcp_habenula_adapter_t adapter);

/* Logging functions */
int neuromod_logging_log_lc_event(neuromod_logging_bridge_t* bridge, neuromod_log_event_t event,
                                  neuromod_log_level_t level, float ne_level, float value, const char* message);
int neuromod_logging_log_vta_event(neuromod_logging_bridge_t* bridge, neuromod_log_event_t event,
                                   neuromod_log_level_t level, float da_level, float rpe, const char* message);
int neuromod_logging_log_raphe_event(neuromod_logging_bridge_t* bridge, neuromod_log_event_t event,
                                     neuromod_log_level_t level, float ht_level, float mood, const char* message);
int neuromod_logging_log_habenula_event(neuromod_logging_bridge_t* bridge, neuromod_log_event_t event,
                                        neuromod_log_level_t level, float hab_level, float value, const char* message);

/* Generic logging */
int neuromod_logging_log_entry(neuromod_logging_bridge_t* bridge, const neuromod_log_entry_t* entry);

/* Flush and buffer management */
int neuromod_logging_flush(neuromod_logging_bridge_t* bridge);
int neuromod_logging_update(neuromod_logging_bridge_t* bridge, float delta_ms);

/* Pattern analysis (feedback) */
int neuromod_logging_analyze_patterns(neuromod_logging_bridge_t* bridge);
int neuromod_logging_get_pattern(const neuromod_logging_bridge_t* bridge, uint32_t index, neuromod_log_pattern_t* pattern);
uint32_t neuromod_logging_get_pattern_count(const neuromod_logging_bridge_t* bridge);

/* Statistics */
int neuromod_logging_bridge_get_stats(const neuromod_logging_bridge_t* bridge, neuromod_logging_bridge_stats_t* stats);
int neuromod_logging_bridge_reset_stats(neuromod_logging_bridge_t* bridge);

/* Diagnostics */
const char* neuromod_log_category_name(neuromod_log_category_t category);
const char* neuromod_log_level_name(neuromod_log_level_t level);
const char* neuromod_log_event_name(neuromod_log_event_t event);
void neuromod_logging_bridge_print_summary(const neuromod_logging_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NEUROMODULATORY_LOGGING_BRIDGE_H */
