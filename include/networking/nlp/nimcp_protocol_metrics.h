/**
 * @file nimcp_protocol_metrics.h
 * @brief Protocol Metrics Dashboard and Semantic Analytics
 *
 * WHAT: Real-time monitoring and analytics for NLP protocol usage and performance
 * WHY:  Track protocol efficiency, semantic primitive usage, and system health
 * HOW:  Metrics collection, time-series tracking, semantic analysis, alerting
 *
 * ARCHITECTURE:
 *
 *   ┌──────────────────────────────────────────────────────────────┐
 *   │             Protocol Metrics System                          │
 *   │                                                              │
 *   │  ┌──────────────────┐  ┌──────────────────┐                 │
 *   │  │ Protocol Stats   │  │ Semantic Stats   │                 │
 *   │  │ - Messages       │  │ - Primitives     │                 │
 *   │  │ - Bytes          │  │ - Compression    │                 │
 *   │  │ - Latency        │  │ - Context        │                 │
 *   │  │ - Errors         │  │ - Usage Patterns │                 │
 *   │  └────────┬─────────┘  └────────┬─────────┘                 │
 *   │           │                     │                           │
 *   │           └──────────┬──────────┘                           │
 *   │                      │                                      │
 *   │            ┌─────────▼─────────┐                            │
 *   │            │ Time-Series       │                            │
 *   │            │ History Buffer    │                            │
 *   │            └─────────┬─────────┘                            │
 *   │                      │                                      │
 *   │            ┌─────────▼─────────┐                            │
 *   │            │ Alert System      │                            │
 *   │            │ - Thresholds      │                            │
 *   │            │ - Callbacks       │                            │
 *   │            └─────────┬─────────┘                            │
 *   │                      │                                      │
 *   │            ┌─────────▼─────────┐                            │
 *   │            │  Bio-Async        │                            │
 *   │            │  Integration      │                            │
 *   │            └───────────────────┘                            │
 *   └──────────────────────────────────────────────────────────────┘
 *
 * FEATURES:
 * - Real-time protocol statistics tracking
 * - Semantic primitive usage analytics
 * - Time-series history with configurable depth
 * - Alert system with callbacks
 * - Dashboard summary (JSON export)
 * - CSV export for analysis
 * - Bio-async integration
 *
 * USAGE EXAMPLE:
 * ```c
 * // Configure metrics
 * metrics_config_t config = {
 *     .metrics_window_ms = 1000,
 *     .history_depth = 100,
 *     .enable_semantic_tracking = true,
 *     .enable_real_time_alerts = true,
 *     .alert_threshold = 0.8,
 *     .enable_bio_async = true
 * };
 *
 * // Create metrics system
 * protocol_metrics_t* pm = protocol_metrics_create(&config);
 *
 * // Record message
 * metrics_record_message(pm, MSG_TYPE_DATA, 1024, 5.5f, true);
 *
 * // Record semantic primitive usage
 * metrics_record_primitive_usage(pm, primitive_id, 0.95f);
 *
 * // Get statistics
 * protocol_stats_t stats = metrics_get_protocol_stats(pm);
 * printf("Messages: %lu, Avg latency: %.2f ms\n",
 *        stats.messages_sent, stats.avg_latency_ms);
 *
 * // Export dashboard
 * char json[4096];
 * metrics_get_dashboard_summary(pm, json, sizeof(json));
 *
 * // Cleanup
 * protocol_metrics_destroy(pm);
 * ```
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 * @version 1.0.0
 */

#ifndef NIMCP_PROTOCOL_METRICS_H
#define NIMCP_PROTOCOL_METRICS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "utils/validation/nimcp_common.h"
#include "utils/platform/nimcp_tier_optimization.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

/** Opaque protocol metrics handle */
typedef struct protocol_metrics protocol_metrics_t;

//=============================================================================
// Constants
//=============================================================================

/** Maximum primitive name length */
#define METRICS_MAX_PRIMITIVE_NAME 64

/** Default metrics window (ms) */
#define METRICS_DEFAULT_WINDOW_MS 1000

/** Default history depth (number of windows) - tier-optimized */
#define METRICS_DEFAULT_HISTORY_DEPTH NIMCP_TRAINING_HISTORY_SIZE

/** Default alert threshold */
#define METRICS_DEFAULT_ALERT_THRESHOLD 0.8f

/** Maximum primitives tracked */
#define METRICS_MAX_PRIMITIVES 256

//=============================================================================
// Bio-Async Message Types
//=============================================================================

/** Bio-async message types for protocol metrics */
typedef enum {
    BIO_MSG_METRICS_UPDATE = 0x0D00,        /**< Metrics update notification */
    BIO_MSG_METRICS_ALERT = 0x0D01,         /**< Alert triggered */
    BIO_MSG_METRICS_THRESHOLD = 0x0D02,     /**< Threshold exceeded */
    BIO_MSG_METRICS_PRIMITIVE_TOP = 0x0D03, /**< Top primitive changed */
    BIO_MSG_METRICS_ERROR_SPIKE = 0x0D04    /**< Error rate spike */
} metrics_bio_msg_type_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Protocol statistics
 *
 * Aggregated statistics for protocol messages over a time window
 */
typedef struct {
    uint64_t messages_sent;         /**< Total messages sent */
    uint64_t messages_received;     /**< Total messages received */
    uint64_t bytes_sent;            /**< Total bytes sent */
    uint64_t bytes_received;        /**< Total bytes received */
    float avg_latency_ms;           /**< Average message latency (ms) */
    float throughput_msgs_per_sec;  /**< Message throughput (msg/s) */
    uint64_t errors;                /**< Total errors */
    uint64_t retransmissions;       /**< Total retransmissions */
} protocol_stats_t;

/**
 * @brief Semantic primitive statistics
 *
 * Usage statistics for individual semantic primitives
 */
typedef struct {
    uint32_t primitive_id;          /**< Unique primitive identifier */
    char name[METRICS_MAX_PRIMITIVE_NAME]; /**< Primitive name */
    uint64_t usage_count;           /**< Number of times used */
    float avg_context_relevance;   /**< Average context relevance (0-1) */
    uint64_t compression_savings;   /**< Bytes saved via compression */
} semantic_primitive_stats_t;

/**
 * @brief Metrics configuration
 */
typedef struct {
    uint32_t metrics_window_ms;     /**< Time window for aggregation (ms) */
    uint32_t history_depth;         /**< Number of historical windows to keep */
    bool enable_semantic_tracking;  /**< Track semantic primitive usage */
    bool enable_real_time_alerts;   /**< Enable real-time alerting */
    float alert_threshold;          /**< Alert threshold (0-1) */
    bool enable_bio_async;          /**< Enable bio-async integration */
} metrics_config_t;

/**
 * @brief Alert callback function
 *
 * @param alert Alert message string
 */
typedef void (*metrics_alert_callback_t)(const char* alert);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create protocol metrics system
 *
 * WHAT: Initializes metrics tracking and analytics
 * WHY:  Monitor protocol performance and usage
 * HOW:  Allocates state, sets up time-series buffers, registers bio-async
 *
 * @param config Configuration (NULL for defaults)
 * @return Metrics handle or NULL on failure
 */
protocol_metrics_t* protocol_metrics_create(const metrics_config_t* config);

/**
 * @brief Destroy protocol metrics system
 *
 * WHAT: Cleans up all metrics state and resources
 * WHY:  Release memory and unregister from bio-async
 * HOW:  Frees buffers, destroys mutex, unregisters callbacks
 *
 * @param pm Metrics handle (NULL safe)
 */
void protocol_metrics_destroy(protocol_metrics_t* pm);

//=============================================================================
// Protocol Tracking Functions
//=============================================================================

/**
 * @brief Record message transmission/reception
 *
 * WHAT: Logs message metadata for statistics
 * WHY:  Track protocol usage and performance
 * HOW:  Updates counters, calculates rolling averages
 *
 * @param pm Metrics handle
 * @param msg_type Message type identifier
 * @param size Message size in bytes
 * @param latency_ms Message latency in milliseconds
 * @param success True if message succeeded, false if error
 * @return NIMCP_SUCCESS or error code
 */
int metrics_record_message(
    protocol_metrics_t* pm,
    uint32_t msg_type,
    uint32_t size,
    float latency_ms,
    bool success
);

/**
 * @brief Get current protocol statistics
 *
 * WHAT: Returns aggregated stats for current window
 * WHY:  Query current protocol performance
 * HOW:  Reads current window statistics
 *
 * @param pm Metrics handle
 * @return Protocol statistics structure
 */
protocol_stats_t metrics_get_protocol_stats(protocol_metrics_t* pm);

/**
 * @brief Get historical statistics
 *
 * WHAT: Returns time-series of past statistics
 * WHY:  Analyze trends and historical performance
 * HOW:  Returns array of historical windows
 *
 * @param pm Metrics handle
 * @param history Output array pointer (allocated by function)
 * @param count Output count of history entries
 * @return NIMCP_SUCCESS or error code
 *
 * NOTE: Caller must free history array with nimcp_free()
 */
int metrics_get_stats_history(
    protocol_metrics_t* pm,
    protocol_stats_t** history,
    uint32_t* count
);

//=============================================================================
// Semantic Primitive Analytics
//=============================================================================

/**
 * @brief Record semantic primitive usage
 *
 * WHAT: Logs usage of semantic primitive with context score
 * WHY:  Track which primitives are most useful
 * HOW:  Updates usage count and rolling average relevance
 *
 * @param pm Metrics handle
 * @param primitive_id Primitive identifier
 * @param context_relevance How relevant primitive was (0-1)
 * @return NIMCP_SUCCESS or error code
 */
int metrics_record_primitive_usage(
    protocol_metrics_t* pm,
    uint32_t primitive_id,
    float context_relevance
);

/**
 * @brief Get statistics for all primitives
 *
 * WHAT: Returns usage stats for all tracked primitives
 * WHY:  Analyze primitive usage patterns
 * HOW:  Returns array of primitive statistics
 *
 * @param pm Metrics handle
 * @param stats Output array pointer (allocated by function)
 * @param count Output count of primitives
 * @return NIMCP_SUCCESS or error code
 *
 * NOTE: Caller must free stats array with nimcp_free()
 */
int metrics_get_primitive_stats(
    protocol_metrics_t* pm,
    semantic_primitive_stats_t** stats,
    uint32_t* count
);

/**
 * @brief Get top N most-used primitives
 *
 * WHAT: Returns primitives sorted by usage count
 * WHY:  Identify most valuable primitives
 * HOW:  Sorts by usage, returns top N
 *
 * @param pm Metrics handle
 * @param top_n Number of top primitives to return
 * @param stats Output array pointer (allocated by function)
 * @return NIMCP_SUCCESS or error code
 *
 * NOTE: Caller must free stats array with nimcp_free()
 */
int metrics_get_top_primitives(
    protocol_metrics_t* pm,
    uint32_t top_n,
    semantic_primitive_stats_t** stats
);

//=============================================================================
// Dashboard and Export Functions
//=============================================================================

/**
 * @brief Get dashboard summary as JSON
 *
 * WHAT: Exports current metrics as JSON string
 * WHY:  Easy integration with dashboards and monitoring
 * HOW:  Formats current stats as JSON
 *
 * @param pm Metrics handle
 * @param json_output Output buffer for JSON
 * @param max_size Maximum buffer size
 * @return NIMCP_SUCCESS or error code
 *
 * JSON FORMAT:
 * {
 *   "protocol": {
 *     "messages_sent": 12345,
 *     "avg_latency_ms": 5.2,
 *     ...
 *   },
 *   "semantic": {
 *     "top_primitives": [...],
 *     ...
 *   },
 *   "timestamp_us": 1234567890
 * }
 */
int metrics_get_dashboard_summary(
    protocol_metrics_t* pm,
    char* json_output,
    uint32_t max_size
);

/**
 * @brief Export metrics to CSV file
 *
 * WHAT: Writes historical metrics to CSV
 * WHY:  Enable offline analysis and visualization
 * HOW:  Writes time-series data as CSV rows
 *
 * @param pm Metrics handle
 * @param filepath Output file path
 * @return NIMCP_SUCCESS or error code
 */
int metrics_export_csv(protocol_metrics_t* pm, const char* filepath);

//=============================================================================
// Alert System Functions
//=============================================================================

/**
 * @brief Set alert callback
 *
 * WHAT: Registers callback for metric alerts
 * WHY:  Real-time notification of issues
 * HOW:  Stores callback, invoked on threshold violations
 *
 * @param pm Metrics handle
 * @param callback Alert callback function
 * @return NIMCP_SUCCESS or error code
 */
int metrics_set_alert_callback(
    protocol_metrics_t* pm,
    metrics_alert_callback_t callback
);

/**
 * @brief Check for alert conditions
 *
 * WHAT: Evaluates current metrics against thresholds
 * WHY:  Trigger alerts for abnormal conditions
 * HOW:  Compares stats to configured thresholds
 *
 * @param pm Metrics handle
 * @return NIMCP_SUCCESS or error code
 *
 * TRIGGERS ALERTS FOR:
 * - High error rate (> threshold)
 * - High latency (> 2x average)
 * - Low throughput (< 0.5x average)
 */
int metrics_check_alerts(protocol_metrics_t* pm);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get default metrics configuration
 *
 * @return Default configuration structure
 */
metrics_config_t metrics_default_config(void);

/**
 * @brief Reset all metrics counters
 *
 * WHAT: Clears all statistics and history
 * WHY:  Start fresh measurement period
 * HOW:  Zeros counters, clears history buffers
 *
 * @param pm Metrics handle
 * @return NIMCP_SUCCESS or error code
 */
int metrics_reset_all(protocol_metrics_t* pm);

/**
 * @brief Get metrics uptime
 *
 * @param pm Metrics handle
 * @return Uptime in milliseconds
 */
uint64_t metrics_get_uptime_ms(protocol_metrics_t* pm);

/**
 * @brief Set primitive name
 *
 * WHAT: Associates human-readable name with primitive ID
 * WHY:  Make reports and dashboards more readable
 * HOW:  Stores name in primitive stats
 *
 * @param pm Metrics handle
 * @param primitive_id Primitive identifier
 * @param name Primitive name
 * @return NIMCP_SUCCESS or error code
 */
int metrics_set_primitive_name(
    protocol_metrics_t* pm,
    uint32_t primitive_id,
    const char* name
);

/**
 * @brief Get total compression savings
 *
 * @param pm Metrics handle
 * @return Total bytes saved via semantic compression
 */
uint64_t metrics_get_total_compression_savings(protocol_metrics_t* pm);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PROTOCOL_METRICS_H */
