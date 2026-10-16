/**
 * @file nimcp_health_diagnostic_bridge.h
 * @brief Health Monitoring to Diagnostics Format Bridge
 * @version 1.0.0
 * @date 2025-01-20
 *
 * WHAT: Converts health monitoring outputs to diagnostic format
 * WHY:  Enable unified processing of health events through self-repair pipeline
 * HOW:  Type mapping tables, context enrichment, severity translation
 *
 * ARCHITECTURE:
 * ```
 * ┌──────────────────────┐     ┌───────────────────────┐     ┌──────────────────┐
 * │   Health Monitor     │     │  Health Diagnostic    │     │  diagnostic_     │
 * │     anomaly_t        │────>│      Bridge           │────>│    result_t      │
 * └──────────────────────┘     └───────────────────────┘     └──────────────────┘
 *                                       │
 * ┌──────────────────────┐              │
 * │   Health Agent       │              │
 * │     message_t        │──────────────┘
 * └──────────────────────┘
 * ```
 *
 * FEATURES:
 * - anomaly_t → diagnostic_result_t conversion
 * - health_agent_message_t → diagnostic_result_t conversion
 * - Type/severity mapping tables
 * - Context enrichment (stack trace, memory snapshot)
 * - Pattern analysis integration
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_HEALTH_DIAGNOSTIC_BRIDGE_H
#define NIMCP_HEALTH_DIAGNOSTIC_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Dependencies */
#include "utils/fault_tolerance/nimcp_health_monitor.h"
#include "utils/fault_tolerance/nimcp_health_agent.h"
#include "utils/fault_tolerance/nimcp_diagnostics.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define HEALTH_DIAG_BRIDGE_VERSION       "1.0.0"
#define HEALTH_DIAG_BRIDGE_MAGIC         0x48444247  /**< 'HDBG' */
#define HEALTH_DIAG_BRIDGE_MAX_MAPPINGS  64          /**< Max custom type mappings */

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Health diagnostic bridge configuration
 *
 * Controls how health events are converted to diagnostics
 */
typedef struct {
    /* Context capture options */
    bool capture_stack_trace;           /**< Capture stack trace on conversion */
    bool capture_memory_snapshot;       /**< Capture memory state on conversion */

    /* Default values */
    float default_confidence;           /**< Default confidence for conversions (0.0-1.0) */

    /* Pattern analysis */
    bool enable_pattern_analysis;       /**< Enable crash pattern analysis */

    /* Escalation */
    uint32_t escalation_threshold;      /**< Occurrences before escalating severity */

    /* Filtering */
    anomaly_severity_t min_severity;    /**< Minimum severity to convert */
    health_agent_severity_t min_agent_severity; /**< Minimum agent severity */

    /* Bio-async integration */
    bool enable_bio_async;              /**< Enable bio-async messaging */

    /* Logging */
    bool verbose_logging;               /**< Enable verbose output */
} health_diag_bridge_config_t;

/* ============================================================================
 * Type Mapping Structures
 * ============================================================================ */

/**
 * @brief Mapping from anomaly type to error type
 */
typedef struct {
    anomaly_type_t anomaly_type;        /**< Health monitor anomaly type */
    error_type_t error_type;            /**< Diagnostic error type */
    diag_severity_t default_severity;   /**< Default diagnostic severity */
    float default_confidence;           /**< Default confidence for this mapping */
    const char* description_template;   /**< Description template */
} anomaly_error_mapping_t;

/**
 * @brief Mapping from health agent message type to error type
 */
typedef struct {
    health_agent_msg_type_t msg_type;   /**< Health agent message type */
    error_type_t error_type;            /**< Diagnostic error type */
    diag_severity_t default_severity;   /**< Default diagnostic severity */
    float default_confidence;           /**< Default confidence for this mapping */
    const char* description_template;   /**< Description template */
} agent_error_mapping_t;

/**
 * @brief Mapping from anomaly severity to diagnostic severity
 */
typedef struct {
    anomaly_severity_t anomaly_severity;    /**< Health monitor severity */
    diag_severity_t diag_severity;          /**< Diagnostic severity */
} anomaly_severity_mapping_t;

/**
 * @brief Mapping from health agent severity to diagnostic severity
 */
typedef struct {
    health_agent_severity_t agent_severity; /**< Health agent severity */
    diag_severity_t diag_severity;          /**< Diagnostic severity */
} agent_severity_mapping_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Health diagnostic bridge statistics
 */
typedef struct {
    /* Conversion counts */
    uint64_t anomalies_converted;       /**< Total anomalies converted */
    uint64_t agent_messages_converted;  /**< Total agent messages converted */
    uint64_t conversions_failed;        /**< Failed conversions */

    /* By anomaly type */
    uint64_t by_anomaly_type[ANOMALY_UNKNOWN + 1]; /**< Count by anomaly type */

    /* By agent message type */
    uint64_t by_agent_msg_type[HEALTH_MSG_COUNT];  /**< Count by agent msg type */

    /* By severity */
    uint64_t by_severity[DIAG_SEVERITY_FATAL + 1]; /**< Count by output severity */

    /* Stack traces captured */
    uint64_t stack_traces_captured;     /**< Stack traces captured */

    /* Memory snapshots captured */
    uint64_t memory_snapshots_captured; /**< Memory snapshots captured */

    /* Pattern matches */
    uint64_t patterns_detected;         /**< Patterns detected */

    /* Timing */
    float avg_conversion_time_us;       /**< Average conversion time */
} health_diag_bridge_stats_t;

/* ============================================================================
 * Bridge Handle
 * ============================================================================ */

/**
 * @brief Opaque health diagnostic bridge handle
 */
typedef struct health_diag_bridge health_diag_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default bridge configuration
 *
 * WHAT: Initialize configuration with sensible defaults
 * WHY:  Easy setup with good starting values
 * HOW:  Return struct with balanced parameters
 *
 * DEFAULTS:
 * - capture_stack_trace: true
 * - capture_memory_snapshot: true
 * - default_confidence: 0.7
 * - enable_pattern_analysis: true
 * - escalation_threshold: 3
 * - min_severity: ANOMALY_SEVERITY_WARNING
 *
 * @param config Output configuration (non-NULL)
 * @return 0 on success, -1 on error
 */
int health_diag_bridge_default_config(health_diag_bridge_config_t* config);

/**
 * @brief Create health diagnostic bridge
 *
 * WHAT: Initialize bridge for converting health events to diagnostics
 * WHY:  Enable unified processing through self-repair pipeline
 * HOW:  Allocate bridge, initialize mapping tables
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
health_diag_bridge_t* health_diag_bridge_create(
    const health_diag_bridge_config_t* config
);

/**
 * @brief Destroy health diagnostic bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free internal structures
 *
 * @param bridge Bridge handle (NULL safe)
 */
void health_diag_bridge_destroy(health_diag_bridge_t* bridge);

/* ============================================================================
 * Conversion API - Anomaly to Diagnostic
 * ============================================================================ */

/**
 * @brief Convert health monitor anomaly to diagnostic result
 *
 * WHAT: Transform anomaly_t to diagnostic_result_t
 * WHY:  Feed health anomalies into self-repair pipeline
 * HOW:  Map types, translate severity, enrich context
 *
 * @param bridge Bridge handle
 * @param anomaly Source anomaly from health monitor
 * @param result Output diagnostic result (caller must free with diagnostics_free_result)
 * @return 0 on success, -1 on error
 */
int health_diag_bridge_convert_anomaly(
    health_diag_bridge_t* bridge,
    const anomaly_t* anomaly,
    diagnostic_result_t** result
);

/**
 * @brief Convert multiple anomalies to diagnostic results
 *
 * WHAT: Batch convert anomalies
 * WHY:  Efficient batch processing
 * HOW:  Convert each anomaly, aggregate results
 *
 * @param bridge Bridge handle
 * @param anomalies Source anomalies array
 * @param anomaly_count Number of anomalies
 * @param results Output array of diagnostic results (caller allocates array, frees results)
 * @param converted_count Output: number successfully converted
 * @return 0 on success, -1 on error
 */
int health_diag_bridge_convert_anomalies(
    health_diag_bridge_t* bridge,
    const anomaly_t* anomalies,
    uint32_t anomaly_count,
    diagnostic_result_t** results,
    uint32_t* converted_count
);

/* ============================================================================
 * Conversion API - Health Agent Message to Diagnostic
 * ============================================================================ */

/**
 * @brief Convert health agent message to diagnostic result
 *
 * WHAT: Transform health_agent_message_t to diagnostic_result_t
 * WHY:  Feed agent-detected issues into self-repair pipeline
 * HOW:  Map types, extract details, enrich context
 *
 * @param bridge Bridge handle
 * @param message Source message from health agent
 * @param result Output diagnostic result (caller must free)
 * @return 0 on success, -1 on error
 */
int health_diag_bridge_convert_agent_message(
    health_diag_bridge_t* bridge,
    const health_agent_message_t* message,
    diagnostic_result_t** result
);

/* ============================================================================
 * Enrichment API
 * ============================================================================ */

/**
 * @brief Enrich diagnostic result with stack trace
 *
 * WHAT: Add current stack trace to diagnostic result
 * WHY:  Better root cause analysis
 * HOW:  Capture backtrace, symbolicate if possible
 *
 * @param bridge Bridge handle
 * @param result Diagnostic result to enrich
 * @return 0 on success, -1 on error
 */
int health_diag_bridge_enrich_stack_trace(
    health_diag_bridge_t* bridge,
    diagnostic_result_t* result
);

/**
 * @brief Enrich diagnostic result with memory snapshot
 *
 * WHAT: Add memory state to diagnostic result
 * WHY:  Memory context for analysis
 * HOW:  Capture allocation stats, detect potential leaks
 *
 * @param bridge Bridge handle
 * @param result Diagnostic result to enrich
 * @return 0 on success, -1 on error
 */
int health_diag_bridge_enrich_memory_snapshot(
    health_diag_bridge_t* bridge,
    diagnostic_result_t* result
);

/**
 * @brief Analyze for recurring patterns
 *
 * WHAT: Check if diagnostic matches known patterns
 * WHY:  Detect systematic issues
 * HOW:  Compare against pattern history
 *
 * @param bridge Bridge handle
 * @param result Diagnostic result to analyze
 * @return 0 on success, -1 on error
 */
int health_diag_bridge_analyze_patterns(
    health_diag_bridge_t* bridge,
    diagnostic_result_t* result
);

/* ============================================================================
 * Mapping Configuration API
 * ============================================================================ */

/**
 * @brief Add custom anomaly to error type mapping
 *
 * WHAT: Define custom mapping for specific anomaly type
 * WHY:  Override default mappings for specific needs
 * HOW:  Add to mapping table
 *
 * @param bridge Bridge handle
 * @param mapping Custom mapping
 * @return 0 on success, -1 on error
 */
int health_diag_bridge_add_anomaly_mapping(
    health_diag_bridge_t* bridge,
    const anomaly_error_mapping_t* mapping
);

/**
 * @brief Add custom agent message to error type mapping
 *
 * WHAT: Define custom mapping for specific agent message type
 * WHY:  Override default mappings for specific needs
 * HOW:  Add to mapping table
 *
 * @param bridge Bridge handle
 * @param mapping Custom mapping
 * @return 0 on success, -1 on error
 */
int health_diag_bridge_add_agent_mapping(
    health_diag_bridge_t* bridge,
    const agent_error_mapping_t* mapping
);

/**
 * @brief Get current anomaly to error mapping
 *
 * WHAT: Look up mapping for anomaly type
 * WHY:  Query current mapping configuration
 * HOW:  Search mapping table
 *
 * @param bridge Bridge handle
 * @param anomaly_type Anomaly type to look up
 * @return Mapping or NULL if not found
 */
const anomaly_error_mapping_t* health_diag_bridge_get_anomaly_mapping(
    const health_diag_bridge_t* bridge,
    anomaly_type_t anomaly_type
);

/* ============================================================================
 * Severity Translation API
 * ============================================================================ */

/**
 * @brief Translate anomaly severity to diagnostic severity
 *
 * WHAT: Convert between severity enums
 * WHY:  Unified severity handling
 * HOW:  Use mapping table
 *
 * @param anomaly_severity Input anomaly severity
 * @return Corresponding diagnostic severity
 */
diag_severity_t health_diag_bridge_translate_anomaly_severity(
    anomaly_severity_t anomaly_severity
);

/**
 * @brief Translate health agent severity to diagnostic severity
 *
 * WHAT: Convert between severity enums
 * WHY:  Unified severity handling
 * HOW:  Use mapping table
 *
 * @param agent_severity Input agent severity
 * @return Corresponding diagnostic severity
 */
diag_severity_t health_diag_bridge_translate_agent_severity(
    health_agent_severity_t agent_severity
);

/* ============================================================================
 * Statistics and Query API
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int health_diag_bridge_get_stats(
    const health_diag_bridge_t* bridge,
    health_diag_bridge_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge handle
 */
void health_diag_bridge_reset_stats(health_diag_bridge_t* bridge);

/**
 * @brief Check if bridge is ready
 *
 * @param bridge Bridge handle
 * @return true if ready, false otherwise
 */
bool health_diag_bridge_is_ready(const health_diag_bridge_t* bridge);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get anomaly type name
 *
 * @param type Anomaly type
 * @return String name (static)
 */
const char* health_diag_bridge_anomaly_type_name(anomaly_type_t type);

/**
 * @brief Get agent message type name
 *
 * @param type Agent message type
 * @return String name (static)
 */
const char* health_diag_bridge_agent_msg_type_name(health_agent_msg_type_t type);

/**
 * @brief Get bridge version string
 *
 * @return Version string
 */
const char* health_diag_bridge_version(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HEALTH_DIAGNOSTIC_BRIDGE_H */
