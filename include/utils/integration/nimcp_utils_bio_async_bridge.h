/**
 * @file nimcp_utils_bio_async_bridge.h
 * @brief Utils Module Bio-Async Integration Bridge
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Central bio-async integration for utility services that provides
 *       comprehensive message routing for memory, timer, and logging events.
 *
 * WHY: The utils module needs to communicate:
 *      - Memory allocation/deallocation events for resource tracking
 *      - Timer events for coordinated scheduling
 *      - Log entries for centralized diagnostics
 *      - Metrics broadcasts for system-wide monitoring
 *      - Service coordination across modules
 *
 * HOW: Registers utilities as a bio-router module, maintains subscriptions,
 *      provides typed message broadcast APIs, and processes incoming requests.
 *
 * BIOLOGICAL BASIS:
 * =================================================================================
 *
 * UTILITY OUTPUT SIGNALS:
 * -----------------------
 * 1. Memory events:
 *    - Allocation/deallocation notifications
 *    - Memory pressure warnings
 *    - Pool exhaustion alerts
 *    - Mapped to: UTILS_MSG_MEMORY_ALLOC, UTILS_MSG_MEMORY_FREE,
 *                 UTILS_MSG_MEMORY_PRESSURE
 *
 * 2. Timer events:
 *    - Timer fired notifications
 *    - Scheduling updates
 *    - Deadline warnings
 *    - Mapped to: UTILS_MSG_TIMER_EVENT, UTILS_MSG_TIMER_SCHEDULE
 *
 * 3. Logging events:
 *    - Log entry broadcasts
 *    - Error/warning aggregation
 *    - Diagnostic data
 *    - Mapped to: UTILS_MSG_LOG_ENTRY, UTILS_MSG_LOG_ERROR
 *
 * 4. Metrics/Service:
 *    - Performance metrics
 *    - Service health status
 *    - Module coordination
 *    - Mapped to: UTILS_MSG_METRICS, UTILS_MSG_SERVICE_STATUS
 *
 * UTILITY INPUT SIGNALS:
 * ----------------------
 * 1. Memory requests from modules
 * 2. Timer registration requests
 * 3. Log level configuration
 * 4. Service discovery queries
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_UTILS_BIO_ASYNC_BRIDGE_H
#define NIMCP_UTILS_BIO_ASYNC_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations (avoid circular dependencies)
 * ============================================================================ */

/* Include canonical error type definition */
#ifndef NIMCP_ERROR_TYPE_DEFINED
#include "utils/error/nimcp_error_codes.h"
#endif

/* Bio-async types - forward declare to avoid header dependency */
typedef struct bio_router_struct* bio_router_t;

/* Neuromodulator channel types */
typedef enum {
    UTILS_BIO_CHANNEL_DOPAMINE = 0,
    UTILS_BIO_CHANNEL_SEROTONIN = 1,
    UTILS_BIO_CHANNEL_NOREPINEPHRINE = 2,
    UTILS_BIO_CHANNEL_ACETYLCHOLINE = 3
} utils_bio_channel_type_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum number of module subscriptions */
#define UTILS_BIO_MAX_SUBSCRIPTIONS         64

/** Maximum pending messages in inbox */
#define UTILS_BIO_MAX_INBOX_SIZE            256

/** Maximum pending messages in outbox */
#define UTILS_BIO_MAX_OUTBOX_SIZE           128

/** Default broadcast interval for state (ms) */
#define UTILS_BIO_DEFAULT_BROADCAST_INTERVAL_MS  50

/** Message expiry time (ms) */
#define UTILS_BIO_MESSAGE_TTL_MS            5000

/** Maximum log message length */
#define UTILS_BIO_MAX_LOG_MESSAGE_LEN       256

/** Maximum service name length */
#define UTILS_BIO_MAX_SERVICE_NAME_LEN      64

/** Maximum timer name length */
#define UTILS_BIO_MAX_TIMER_NAME_LEN        32

/** Maximum metrics entries per broadcast */
#define UTILS_BIO_MAX_METRICS_ENTRIES       16

/** Memory pressure warning threshold (fraction of pool) */
#define UTILS_BIO_MEMORY_WARNING_THRESHOLD  0.75f

/** Memory pressure critical threshold (fraction of pool) */
#define UTILS_BIO_MEMORY_CRITICAL_THRESHOLD 0.90f

/* ============================================================================
 * Error Codes (0x5000-0x50FF range for utils bio-async)
 * ============================================================================ */

#define UTILS_BIO_ERROR_BASE                0x5000
#define UTILS_BIO_ERROR_NOT_INITIALIZED     (UTILS_BIO_ERROR_BASE + 1)
#define UTILS_BIO_ERROR_INVALID_PARAM       (UTILS_BIO_ERROR_BASE + 2)
#define UTILS_BIO_ERROR_NOT_CONNECTED       (UTILS_BIO_ERROR_BASE + 3)
#define UTILS_BIO_ERROR_SUBSCRIPTION_FULL   (UTILS_BIO_ERROR_BASE + 4)
#define UTILS_BIO_ERROR_MESSAGE_TOO_LARGE   (UTILS_BIO_ERROR_BASE + 5)
#define UTILS_BIO_ERROR_INBOX_FULL          (UTILS_BIO_ERROR_BASE + 6)
#define UTILS_BIO_ERROR_OUTBOX_FULL         (UTILS_BIO_ERROR_BASE + 7)
#define UTILS_BIO_ERROR_SERVICE_NOT_FOUND   (UTILS_BIO_ERROR_BASE + 8)
#define UTILS_BIO_ERROR_TIMER_NOT_FOUND     (UTILS_BIO_ERROR_BASE + 9)
#define UTILS_BIO_ERROR_MEMORY_EXHAUSTED    (UTILS_BIO_ERROR_BASE + 10)

/* ============================================================================
 * Message Types (0x5100-0x513F range)
 * ============================================================================ */

/**
 * @brief Utils bio-async message types
 *
 * WHAT: Message type enumeration for utils bio-async routing
 * WHY:  Enables typed message handling and subscription filtering
 * HOW:  Each type corresponds to a specific utility output
 */
typedef enum {
    /* Memory messages (0x5100-0x5107) */
    UTILS_MSG_MEMORY_ALLOC = 0,         /**< Memory allocation event */
    UTILS_MSG_MEMORY_FREE,              /**< Memory deallocation event */
    UTILS_MSG_MEMORY_PRESSURE,          /**< Memory pressure warning */
    UTILS_MSG_MEMORY_POOL_STATUS,       /**< Pool status report */

    /* Timer messages (0x5108-0x510F) */
    UTILS_MSG_TIMER_EVENT,              /**< Timer fired event */
    UTILS_MSG_TIMER_SCHEDULE,           /**< Timer scheduling update */
    UTILS_MSG_TIMER_CANCEL,             /**< Timer cancelled */
    UTILS_MSG_TIMER_DEADLINE,           /**< Deadline approaching */

    /* Logging messages (0x5110-0x5117) */
    UTILS_MSG_LOG_ENTRY,                /**< General log entry */
    UTILS_MSG_LOG_ERROR,                /**< Error log entry */
    UTILS_MSG_LOG_WARNING,              /**< Warning log entry */
    UTILS_MSG_LOG_DEBUG,                /**< Debug log entry */

    /* Metrics messages (0x5118-0x511F) */
    UTILS_MSG_METRICS,                  /**< Performance metrics */
    UTILS_MSG_METRICS_SNAPSHOT,         /**< Full metrics snapshot */

    /* Service messages (0x5120-0x5127) */
    UTILS_MSG_SERVICE_STATUS,           /**< Service health status */
    UTILS_MSG_SERVICE_REGISTER,         /**< Service registration */
    UTILS_MSG_SERVICE_UNREGISTER,       /**< Service unregistration */
    UTILS_MSG_SERVICE_QUERY,            /**< Service discovery query */
    UTILS_MSG_SERVICE_RESPONSE,         /**< Service discovery response */

    /* Coordination messages (0x5128-0x512F) */
    UTILS_MSG_HEARTBEAT,                /**< Module heartbeat */
    UTILS_MSG_SYNC_REQUEST,             /**< Synchronization request */
    UTILS_MSG_SYNC_RESPONSE,            /**< Synchronization response */
    UTILS_MSG_SHUTDOWN_NOTICE,          /**< Graceful shutdown notice */

    UTILS_MSG_TYPE_COUNT
} utils_bio_msg_type_t;

/**
 * @brief Bitmask for message type subscriptions
 */
#define UTILS_BIO_SUB_MEMORY_ALLOC      (1U << UTILS_MSG_MEMORY_ALLOC)
#define UTILS_BIO_SUB_MEMORY_FREE       (1U << UTILS_MSG_MEMORY_FREE)
#define UTILS_BIO_SUB_MEMORY_PRESSURE   (1U << UTILS_MSG_MEMORY_PRESSURE)
#define UTILS_BIO_SUB_MEMORY_POOL       (1U << UTILS_MSG_MEMORY_POOL_STATUS)
#define UTILS_BIO_SUB_TIMER_EVENT       (1U << UTILS_MSG_TIMER_EVENT)
#define UTILS_BIO_SUB_TIMER_SCHEDULE    (1U << UTILS_MSG_TIMER_SCHEDULE)
#define UTILS_BIO_SUB_TIMER_CANCEL      (1U << UTILS_MSG_TIMER_CANCEL)
#define UTILS_BIO_SUB_TIMER_DEADLINE    (1U << UTILS_MSG_TIMER_DEADLINE)
#define UTILS_BIO_SUB_LOG_ENTRY         (1U << UTILS_MSG_LOG_ENTRY)
#define UTILS_BIO_SUB_LOG_ERROR         (1U << UTILS_MSG_LOG_ERROR)
#define UTILS_BIO_SUB_LOG_WARNING       (1U << UTILS_MSG_LOG_WARNING)
#define UTILS_BIO_SUB_LOG_DEBUG         (1U << UTILS_MSG_LOG_DEBUG)
#define UTILS_BIO_SUB_METRICS           (1U << UTILS_MSG_METRICS)
#define UTILS_BIO_SUB_SERVICE_STATUS    (1U << UTILS_MSG_SERVICE_STATUS)
#define UTILS_BIO_SUB_SERVICE_REGISTER  (1U << UTILS_MSG_SERVICE_REGISTER)
#define UTILS_BIO_SUB_HEARTBEAT         (1U << UTILS_MSG_HEARTBEAT)
#define UTILS_BIO_SUB_ALL_MEMORY        (UTILS_BIO_SUB_MEMORY_ALLOC | \
                                         UTILS_BIO_SUB_MEMORY_FREE | \
                                         UTILS_BIO_SUB_MEMORY_PRESSURE | \
                                         UTILS_BIO_SUB_MEMORY_POOL)
#define UTILS_BIO_SUB_ALL_TIMER         (UTILS_BIO_SUB_TIMER_EVENT | \
                                         UTILS_BIO_SUB_TIMER_SCHEDULE | \
                                         UTILS_BIO_SUB_TIMER_CANCEL | \
                                         UTILS_BIO_SUB_TIMER_DEADLINE)
#define UTILS_BIO_SUB_ALL_LOG           (UTILS_BIO_SUB_LOG_ENTRY | \
                                         UTILS_BIO_SUB_LOG_ERROR | \
                                         UTILS_BIO_SUB_LOG_WARNING | \
                                         UTILS_BIO_SUB_LOG_DEBUG)
#define UTILS_BIO_SUB_ALL               (0xFFFFFFFFU)

/* ============================================================================
 * Log Levels
 * ============================================================================ */

/**
 * @brief Log severity levels
 */
typedef enum {
    UTILS_LOG_LEVEL_TRACE = 0,          /**< Trace level (most verbose) */
    UTILS_LOG_LEVEL_DEBUG,              /**< Debug level */
    UTILS_LOG_LEVEL_INFO,               /**< Informational */
    UTILS_LOG_LEVEL_WARNING,            /**< Warning */
    UTILS_LOG_LEVEL_ERROR,              /**< Error */
    UTILS_LOG_LEVEL_FATAL,              /**< Fatal error */
    UTILS_LOG_LEVEL_COUNT
} utils_log_level_t;

/* ============================================================================
 * Service Status
 * ============================================================================ */

/**
 * @brief Service health status
 */
typedef enum {
    UTILS_SERVICE_STATUS_UNKNOWN = 0,   /**< Status unknown */
    UTILS_SERVICE_STATUS_STARTING,      /**< Service starting up */
    UTILS_SERVICE_STATUS_HEALTHY,       /**< Service healthy */
    UTILS_SERVICE_STATUS_DEGRADED,      /**< Service degraded */
    UTILS_SERVICE_STATUS_UNHEALTHY,     /**< Service unhealthy */
    UTILS_SERVICE_STATUS_STOPPING,      /**< Service stopping */
    UTILS_SERVICE_STATUS_STOPPED        /**< Service stopped */
} utils_service_status_t;

/* ============================================================================
 * Message Header Structure
 * ============================================================================ */

/**
 * @brief Common header for all utils bio-async messages
 */
typedef struct {
    utils_bio_msg_type_t type;          /**< Message type identifier */
    uint32_t sequence_id;               /**< Sequence number for ordering */
    uint32_t source_module;             /**< Source module identifier */
    uint32_t target_module;             /**< Target module (0 = broadcast) */
    uint64_t timestamp_us;              /**< Timestamp in microseconds */
    utils_bio_channel_type_t channel;   /**< Recommended channel for response */
    uint32_t payload_size;              /**< Size of payload in bytes */
    uint32_t flags;                     /**< Message flags */
} utils_bio_msg_header_t;

/** Message flags */
#define UTILS_BIO_MSG_FLAG_URGENT       (1 << 0)  /**< Priority handling */
#define UTILS_BIO_MSG_FLAG_REQUIRES_ACK (1 << 1)  /**< Sender expects ack */
#define UTILS_BIO_MSG_FLAG_BROADCAST    (1 << 2)  /**< Send to all modules */
#define UTILS_BIO_MSG_FLAG_COMPRESSED   (1 << 3)  /**< Payload is compressed */
#define UTILS_BIO_MSG_FLAG_ENCRYPTED    (1 << 4)  /**< Payload is encrypted */

/* ============================================================================
 * Message Payload Structures
 * ============================================================================ */

/**
 * @brief Memory allocation event message
 */
typedef struct {
    utils_bio_msg_header_t header;      /**< Standard header */

    void* address;                      /**< Allocated address */
    size_t size;                        /**< Allocation size (bytes) */
    size_t alignment;                   /**< Alignment requirement */
    uint32_t pool_id;                   /**< Memory pool ID */
    const char* tag;                    /**< Allocation tag/name */

    size_t total_allocated;             /**< Total currently allocated */
    size_t pool_capacity;               /**< Pool total capacity */
    float utilization;                  /**< Pool utilization [0, 1] */

    uint32_t source_module;             /**< Requesting module */
    uint64_t timestamp_us;              /**< Event timestamp */
} utils_bio_memory_alloc_msg_t;

/**
 * @brief Memory free event message
 */
typedef struct {
    utils_bio_msg_header_t header;      /**< Standard header */

    void* address;                      /**< Freed address */
    size_t size;                        /**< Freed size (bytes) */
    uint32_t pool_id;                   /**< Memory pool ID */

    size_t total_allocated;             /**< Total after free */
    size_t pool_capacity;               /**< Pool total capacity */
    float utilization;                  /**< Pool utilization [0, 1] */

    uint32_t source_module;             /**< Freeing module */
    uint64_t timestamp_us;              /**< Event timestamp */
} utils_bio_memory_free_msg_t;

/**
 * @brief Memory pressure warning message
 */
typedef struct {
    utils_bio_msg_header_t header;      /**< Standard header */

    uint32_t pool_id;                   /**< Affected pool ID */
    size_t bytes_available;             /**< Bytes still available */
    size_t bytes_total;                 /**< Total pool size */
    float utilization;                  /**< Current utilization [0, 1] */

    uint32_t severity;                  /**< 0=warning, 1=critical, 2=emergency */
    uint32_t largest_free_block;        /**< Largest contiguous free block */
    uint32_t fragmentation_percent;     /**< Fragmentation level [0, 100] */

    uint64_t timestamp_us;              /**< Alert timestamp */
} utils_bio_memory_pressure_msg_t;

/**
 * @brief Memory pool status message
 */
typedef struct {
    utils_bio_msg_header_t header;      /**< Standard header */

    uint32_t pool_id;                   /**< Pool ID */
    char pool_name[UTILS_BIO_MAX_SERVICE_NAME_LEN]; /**< Pool name */

    size_t total_capacity;              /**< Total pool capacity */
    size_t bytes_allocated;             /**< Currently allocated */
    size_t bytes_free;                  /**< Currently free */
    size_t peak_usage;                  /**< Peak usage ever */

    uint64_t allocation_count;          /**< Total allocations */
    uint64_t free_count;                /**< Total frees */
    float avg_allocation_size;          /**< Average allocation size */

    uint32_t block_count;               /**< Number of blocks */
    uint32_t fragmentation_percent;     /**< Fragmentation [0, 100] */

    uint64_t timestamp_us;              /**< Report timestamp */
} utils_bio_memory_pool_msg_t;

/**
 * @brief Timer event message
 */
typedef struct {
    utils_bio_msg_header_t header;      /**< Standard header */

    uint32_t timer_id;                  /**< Timer identifier */
    char timer_name[UTILS_BIO_MAX_TIMER_NAME_LEN]; /**< Timer name */

    uint64_t scheduled_time_us;         /**< When timer was scheduled */
    uint64_t fired_time_us;             /**< When timer actually fired */
    int64_t drift_us;                   /**< Drift from scheduled time */

    uint32_t interval_ms;               /**< Timer interval (0 = one-shot) */
    bool is_periodic;                   /**< Periodic timer flag */
    uint32_t fire_count;                /**< Number of times fired */

    uint32_t owner_module;              /**< Module that owns timer */
    void* user_data;                    /**< User context pointer */

    uint64_t timestamp_us;              /**< Event timestamp */
} utils_bio_timer_event_msg_t;

/**
 * @brief Timer schedule message
 */
typedef struct {
    utils_bio_msg_header_t header;      /**< Standard header */

    uint32_t timer_id;                  /**< Timer identifier */
    char timer_name[UTILS_BIO_MAX_TIMER_NAME_LEN]; /**< Timer name */

    uint64_t fire_time_us;              /**< Scheduled fire time */
    uint32_t interval_ms;               /**< Interval (0 = one-shot) */
    bool is_periodic;                   /**< Periodic flag */

    uint32_t owner_module;              /**< Owning module */
    uint32_t priority;                  /**< Timer priority */

    uint64_t timestamp_us;              /**< Schedule timestamp */
} utils_bio_timer_schedule_msg_t;

/**
 * @brief Log entry message
 */
typedef struct {
    utils_bio_msg_header_t header;      /**< Standard header */

    utils_log_level_t level;            /**< Log level */
    uint32_t source_module;             /**< Source module ID */
    char module_name[UTILS_BIO_MAX_SERVICE_NAME_LEN]; /**< Module name */

    char message[UTILS_BIO_MAX_LOG_MESSAGE_LEN]; /**< Log message */
    char file[64];                      /**< Source file (optional) */
    uint32_t line;                      /**< Source line (optional) */
    char function[64];                  /**< Function name (optional) */

    uint64_t timestamp_us;              /**< Log timestamp */
} utils_bio_log_entry_msg_t;

/**
 * @brief Metrics entry
 */
typedef struct {
    char name[32];                      /**< Metric name */
    double value;                       /**< Metric value */
    char unit[16];                      /**< Unit (e.g., "ms", "bytes") */
} utils_bio_metric_entry_t;

/**
 * @brief Metrics broadcast message
 */
typedef struct {
    utils_bio_msg_header_t header;      /**< Standard header */

    uint32_t source_module;             /**< Source module ID */
    char module_name[UTILS_BIO_MAX_SERVICE_NAME_LEN]; /**< Module name */

    uint32_t metric_count;              /**< Number of metrics */
    utils_bio_metric_entry_t metrics[UTILS_BIO_MAX_METRICS_ENTRIES];

    uint64_t period_start_us;           /**< Metrics period start */
    uint64_t period_end_us;             /**< Metrics period end */

    uint64_t timestamp_us;              /**< Report timestamp */
} utils_bio_metrics_msg_t;

/**
 * @brief Service status message
 */
typedef struct {
    utils_bio_msg_header_t header;      /**< Standard header */

    uint32_t service_id;                /**< Service identifier */
    char service_name[UTILS_BIO_MAX_SERVICE_NAME_LEN]; /**< Service name */

    utils_service_status_t status;      /**< Current status */
    utils_service_status_t prev_status; /**< Previous status */

    float health_score;                 /**< Health score [0, 1] */
    uint32_t error_count;               /**< Recent error count */
    uint64_t uptime_us;                 /**< Uptime in microseconds */

    char status_message[128];           /**< Human-readable status */

    uint64_t timestamp_us;              /**< Status timestamp */
} utils_bio_service_status_msg_t;

/**
 * @brief Service registration message
 */
typedef struct {
    utils_bio_msg_header_t header;      /**< Standard header */

    uint32_t service_id;                /**< Service identifier */
    char service_name[UTILS_BIO_MAX_SERVICE_NAME_LEN]; /**< Service name */
    char service_type[32];              /**< Service type/category */

    uint32_t module_id;                 /**< Providing module ID */
    uint32_t capabilities;              /**< Capability flags */
    uint32_t version;                   /**< Service version */

    uint64_t timestamp_us;              /**< Registration timestamp */
} utils_bio_service_register_msg_t;

/**
 * @brief Heartbeat message
 */
typedef struct {
    utils_bio_msg_header_t header;      /**< Standard header */

    uint32_t module_id;                 /**< Module identifier */
    char module_name[UTILS_BIO_MAX_SERVICE_NAME_LEN]; /**< Module name */

    utils_service_status_t status;      /**< Current status */
    float load;                         /**< Current load [0, 1] */
    uint64_t messages_processed;        /**< Messages processed */
    uint64_t errors;                    /**< Error count */

    uint32_t heartbeat_sequence;        /**< Heartbeat sequence number */
    uint64_t last_activity_us;          /**< Last activity timestamp */

    uint64_t timestamp_us;              /**< Heartbeat timestamp */
} utils_bio_heartbeat_msg_t;

/* ============================================================================
 * Subscription Structure
 * ============================================================================ */

/**
 * @brief Module subscription entry
 */
typedef struct {
    uint32_t module_id;                 /**< Subscribed module ID */
    uint32_t msg_type_mask;             /**< Bitmask of subscribed types */
    bool active;                        /**< Subscription active */
    uint64_t subscription_time;         /**< When subscribed */
    uint64_t messages_sent;             /**< Messages sent to this sub */
} utils_bio_subscription_t;

/* ============================================================================
 * Bridge Configuration
 * ============================================================================ */

/**
 * @brief Utils bio-async bridge configuration
 */
typedef struct {
    /* Broadcast timing */
    uint32_t broadcast_interval_ms;          /**< State broadcast interval */
    bool enable_auto_broadcast;              /**< Auto-broadcast state */

    /* Message handling */
    uint32_t max_inbox_process_per_update;   /**< Max inbox msgs per update */
    uint32_t message_ttl_ms;                 /**< Message time-to-live */

    /* Priority settings */
    utils_bio_channel_type_t default_channel; /**< Default channel */
    utils_bio_channel_type_t alert_channel;   /**< Channel for alerts */
    utils_bio_channel_type_t log_channel;     /**< Channel for logs */

    /* Subscription limits */
    uint32_t max_subscriptions;              /**< Maximum subscriptions */

    /* Memory thresholds */
    float memory_warning_threshold;          /**< Memory warning level [0, 1] */
    float memory_critical_threshold;         /**< Memory critical level [0, 1] */

    /* Logging settings */
    utils_log_level_t min_log_level;         /**< Minimum log level to broadcast */
    bool enable_log_broadcast;               /**< Enable log broadcasting */
    bool enable_file_info;                   /**< Include file/line in logs */

    /* Feature flags */
    bool enable_memory_tracking;             /**< Enable memory event tracking */
    bool enable_timer_tracking;              /**< Enable timer event tracking */
    bool enable_metrics_broadcast;           /**< Enable metrics broadcasts */
    bool enable_service_coordination;        /**< Enable service coordination */
    bool enable_heartbeat;                   /**< Enable heartbeat messages */
    uint32_t heartbeat_interval_ms;          /**< Heartbeat interval */

    /* Debug */
    bool enable_debug_logging;               /**< Enable internal debug logs */
} utils_bio_bridge_config_t;

/* ============================================================================
 * Bridge Statistics
 * ============================================================================ */

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Message counts */
    uint64_t messages_sent;                  /**< Total messages sent */
    uint64_t messages_received;              /**< Total messages received */
    uint64_t messages_dropped;               /**< Messages dropped */
    uint64_t broadcasts_sent;                /**< Broadcast messages */

    /* Per-category counts */
    uint64_t memory_events_sent;             /**< Memory event messages */
    uint64_t timer_events_sent;              /**< Timer event messages */
    uint64_t log_entries_sent;               /**< Log entry messages */
    uint64_t metrics_broadcasts;             /**< Metrics broadcasts */
    uint64_t service_updates;                /**< Service status updates */
    uint64_t heartbeats_sent;                /**< Heartbeats sent */

    /* Alert counts */
    uint64_t memory_warnings;                /**< Memory warnings sent */
    uint64_t memory_criticals;               /**< Memory critical alerts */

    /* Subscription stats */
    uint32_t active_subscriptions;           /**< Currently active */
    uint32_t peak_subscriptions;             /**< Peak count */

    /* Timing stats */
    uint64_t last_broadcast_time_us;         /**< Last broadcast */
    float avg_message_latency_us;            /**< Average latency */

    /* Error counts */
    uint64_t handler_errors;                 /**< Handler errors */
    uint64_t routing_errors;                 /**< Routing failures */

    /* Service coordination */
    uint32_t registered_services;            /**< Registered services */
    uint32_t active_timers;                  /**< Active timers */
} utils_bio_bridge_stats_t;

/* ============================================================================
 * Bridge Handle
 * ============================================================================ */

/**
 * @brief Utils bio-async bridge handle (opaque)
 */
typedef struct utils_bio_bridge_struct utils_bio_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Returns configuration with sensible defaults
 * WHY:  Provides a starting point for bridge configuration
 * HOW:  Fills config struct with default values
 *
 * @param config Output configuration structure
 * @return 0 on success, -1 on error
 */
int utils_bio_bridge_default_config(utils_bio_bridge_config_t* config);

/**
 * @brief Validate configuration
 *
 * WHAT: Validates configuration parameters
 * WHY:  Catch invalid config before initialization
 * HOW:  Checks all parameters are within valid ranges
 *
 * @param config Configuration to validate
 * @return 0 if valid, error code otherwise
 */
int utils_bio_bridge_validate_config(const utils_bio_bridge_config_t* config);

/**
 * @brief Create utils bio-async bridge
 *
 * WHAT: Allocates and initializes the bridge
 * WHY:  Entry point for using the utils bio-async system
 * HOW:  Allocates memory, initializes structures
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
utils_bio_bridge_t* utils_bio_bridge_create(
    const utils_bio_bridge_config_t* config
);

/**
 * @brief Initialize bridge (for static allocation)
 *
 * WHAT: Initializes a pre-allocated bridge structure
 * WHY:  Supports static allocation patterns
 * HOW:  Initializes all fields without allocation
 *
 * @param bridge Pre-allocated bridge structure
 * @param config Configuration (NULL for defaults)
 * @return 0 on success, -1 on error
 */
int utils_bio_bridge_init(
    utils_bio_bridge_t* bridge,
    const utils_bio_bridge_config_t* config
);

/**
 * @brief Destroy utils bio-async bridge
 *
 * WHAT: Cleans up and frees bridge resources
 * WHY:  Proper resource cleanup
 * HOW:  Disconnects, frees memory
 *
 * @param bridge Bridge handle (NULL safe)
 */
void utils_bio_bridge_destroy(utils_bio_bridge_t* bridge);

/**
 * @brief Reset bridge to initial state
 *
 * WHAT: Resets bridge without destroying
 * WHY:  Allows reuse without reallocation
 * HOW:  Clears state, preserves config
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int utils_bio_bridge_reset(utils_bio_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-router
 *
 * WHAT: Connects bridge to the bio-async router
 * WHY:  Enables message routing
 * HOW:  Registers with router, sets up handlers
 *
 * @param bridge Bridge handle
 * @param router Bio-router handle
 * @return 0 on success, error code on failure
 */
int utils_bio_bridge_connect(
    utils_bio_bridge_t* bridge,
    bio_router_t router
);

/**
 * @brief Disconnect bridge from router
 *
 * WHAT: Disconnects from bio-router
 * WHY:  Clean disconnection
 * HOW:  Unregisters handlers, clears router reference
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int utils_bio_bridge_disconnect(utils_bio_bridge_t* bridge);

/**
 * @brief Check if bridge is connected
 *
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool utils_bio_bridge_is_connected(const utils_bio_bridge_t* bridge);

/* ============================================================================
 * Message Processing API
 * ============================================================================ */

/**
 * @brief Process incoming messages from inbox
 *
 * WHAT: Processes pending incoming messages
 * WHY:  Handle requests from other modules
 * HOW:  Dequeues and dispatches to handlers
 *
 * @param bridge Bridge handle
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed, -1 on error
 */
int utils_bio_bridge_process_inbox(
    utils_bio_bridge_t* bridge,
    uint32_t max_messages
);

/**
 * @brief Update bridge state and auto-broadcasts
 *
 * WHAT: Periodic update function
 * WHY:  Handles auto-broadcasts, heartbeats, timeouts
 * HOW:  Called from main loop, advances timers
 *
 * @param bridge Bridge handle
 * @param delta_ms Time since last update (milliseconds)
 * @return 0 on success, -1 on error
 */
int utils_bio_bridge_update(
    utils_bio_bridge_t* bridge,
    uint32_t delta_ms
);

/* ============================================================================
 * Memory Event API
 * ============================================================================ */

/**
 * @brief Broadcast memory allocation event
 *
 * WHAT: Notifies subscribers of memory allocation
 * WHY:  Enables resource tracking
 * HOW:  Creates and sends MEMORY_ALLOC message
 *
 * @param bridge Bridge handle
 * @param address Allocated address
 * @param size Allocation size
 * @param pool_id Pool ID (0 for default)
 * @param tag Optional allocation tag
 * @return 0 on success, -1 on error
 */
int utils_bio_bridge_broadcast_memory_alloc(
    utils_bio_bridge_t* bridge,
    void* address,
    size_t size,
    uint32_t pool_id,
    const char* tag
);

/**
 * @brief Broadcast memory free event
 *
 * WHAT: Notifies subscribers of memory deallocation
 * WHY:  Enables resource tracking
 * HOW:  Creates and sends MEMORY_FREE message
 *
 * @param bridge Bridge handle
 * @param address Freed address
 * @param size Freed size
 * @param pool_id Pool ID
 * @return 0 on success, -1 on error
 */
int utils_bio_bridge_broadcast_memory_free(
    utils_bio_bridge_t* bridge,
    void* address,
    size_t size,
    uint32_t pool_id
);

/**
 * @brief Send memory pressure alert
 *
 * WHAT: Notifies of memory pressure condition
 * WHY:  Allows modules to reduce memory usage
 * HOW:  Creates and sends MEMORY_PRESSURE message
 *
 * @param bridge Bridge handle
 * @param pool_id Affected pool ID
 * @param utilization Current utilization [0, 1]
 * @param severity Alert severity (0-2)
 * @return 0 on success, -1 on error
 */
int utils_bio_bridge_send_memory_pressure(
    utils_bio_bridge_t* bridge,
    uint32_t pool_id,
    float utilization,
    uint32_t severity
);

/**
 * @brief Broadcast memory pool status
 *
 * WHAT: Sends detailed pool status report
 * WHY:  Provides comprehensive memory visibility
 * HOW:  Creates and sends MEMORY_POOL_STATUS message
 *
 * @param bridge Bridge handle
 * @param pool_id Pool ID
 * @param pool_name Pool name
 * @param capacity Total capacity
 * @param allocated Currently allocated
 * @return 0 on success, -1 on error
 */
int utils_bio_bridge_broadcast_pool_status(
    utils_bio_bridge_t* bridge,
    uint32_t pool_id,
    const char* pool_name,
    size_t capacity,
    size_t allocated
);

/* ============================================================================
 * Timer Event API
 * ============================================================================ */

/**
 * @brief Broadcast timer fired event
 *
 * WHAT: Notifies that a timer has fired
 * WHY:  Coordinates scheduled operations
 * HOW:  Creates and sends TIMER_EVENT message
 *
 * @param bridge Bridge handle
 * @param timer_id Timer identifier
 * @param timer_name Timer name
 * @param scheduled_us When timer was scheduled to fire
 * @param is_periodic Whether timer is periodic
 * @return 0 on success, -1 on error
 */
int utils_bio_bridge_broadcast_timer_event(
    utils_bio_bridge_t* bridge,
    uint32_t timer_id,
    const char* timer_name,
    uint64_t scheduled_us,
    bool is_periodic
);

/**
 * @brief Broadcast timer schedule
 *
 * WHAT: Notifies of timer scheduling
 * WHY:  Allows modules to track scheduled events
 * HOW:  Creates and sends TIMER_SCHEDULE message
 *
 * @param bridge Bridge handle
 * @param timer_id Timer identifier
 * @param timer_name Timer name
 * @param fire_time_us When timer will fire
 * @param interval_ms Interval (0 for one-shot)
 * @return 0 on success, -1 on error
 */
int utils_bio_bridge_broadcast_timer_schedule(
    utils_bio_bridge_t* bridge,
    uint32_t timer_id,
    const char* timer_name,
    uint64_t fire_time_us,
    uint32_t interval_ms
);

/* ============================================================================
 * Logging API
 * ============================================================================ */

/**
 * @brief Broadcast log entry
 *
 * WHAT: Sends a log entry to subscribers
 * WHY:  Centralized logging infrastructure
 * HOW:  Creates and sends LOG_ENTRY message
 *
 * @param bridge Bridge handle
 * @param level Log level
 * @param module_name Source module name
 * @param message Log message
 * @return 0 on success, -1 on error
 */
int utils_bio_bridge_broadcast_log(
    utils_bio_bridge_t* bridge,
    utils_log_level_t level,
    const char* module_name,
    const char* message
);

/**
 * @brief Broadcast log entry with source location
 *
 * WHAT: Sends log entry with file/line info
 * WHY:  Detailed debugging support
 * HOW:  Creates LOG_ENTRY with source info
 *
 * @param bridge Bridge handle
 * @param level Log level
 * @param module_name Source module name
 * @param message Log message
 * @param file Source file
 * @param line Source line
 * @param function Function name
 * @return 0 on success, -1 on error
 */
int utils_bio_bridge_broadcast_log_ex(
    utils_bio_bridge_t* bridge,
    utils_log_level_t level,
    const char* module_name,
    const char* message,
    const char* file,
    uint32_t line,
    const char* function
);

/* ============================================================================
 * Metrics API
 * ============================================================================ */

/**
 * @brief Broadcast metrics
 *
 * WHAT: Sends performance metrics to subscribers
 * WHY:  System-wide performance monitoring
 * HOW:  Creates and sends METRICS message
 *
 * @param bridge Bridge handle
 * @param module_name Source module name
 * @param metrics Array of metric entries
 * @param metric_count Number of metrics
 * @return 0 on success, -1 on error
 */
int utils_bio_bridge_broadcast_metrics(
    utils_bio_bridge_t* bridge,
    const char* module_name,
    const utils_bio_metric_entry_t* metrics,
    uint32_t metric_count
);

/* ============================================================================
 * Service Coordination API
 * ============================================================================ */

/**
 * @brief Broadcast service status
 *
 * WHAT: Sends service health status
 * WHY:  Service discovery and health monitoring
 * HOW:  Creates and sends SERVICE_STATUS message
 *
 * @param bridge Bridge handle
 * @param service_id Service identifier
 * @param service_name Service name
 * @param status Current status
 * @param health_score Health score [0, 1]
 * @return 0 on success, -1 on error
 */
int utils_bio_bridge_broadcast_service_status(
    utils_bio_bridge_t* bridge,
    uint32_t service_id,
    const char* service_name,
    utils_service_status_t status,
    float health_score
);

/**
 * @brief Register a service
 *
 * WHAT: Registers a service for discovery
 * WHY:  Enables service coordination
 * HOW:  Creates and sends SERVICE_REGISTER message
 *
 * @param bridge Bridge handle
 * @param service_id Service identifier
 * @param service_name Service name
 * @param service_type Service type/category
 * @param capabilities Capability flags
 * @return 0 on success, -1 on error
 */
int utils_bio_bridge_register_service(
    utils_bio_bridge_t* bridge,
    uint32_t service_id,
    const char* service_name,
    const char* service_type,
    uint32_t capabilities
);

/**
 * @brief Send heartbeat
 *
 * WHAT: Sends a heartbeat message
 * WHY:  Module liveness monitoring
 * HOW:  Creates and sends HEARTBEAT message
 *
 * @param bridge Bridge handle
 * @param module_id Module identifier
 * @param module_name Module name
 * @param status Current status
 * @param load Current load [0, 1]
 * @return 0 on success, -1 on error
 */
int utils_bio_bridge_send_heartbeat(
    utils_bio_bridge_t* bridge,
    uint32_t module_id,
    const char* module_name,
    utils_service_status_t status,
    float load
);

/* ============================================================================
 * Subscription Management API
 * ============================================================================ */

/**
 * @brief Subscribe module to utils messages
 *
 * WHAT: Adds subscription for module
 * WHY:  Enables selective message reception
 * HOW:  Adds entry to subscription table
 *
 * @param bridge Bridge handle
 * @param module_id Module identifier
 * @param msg_types Bitmask of message types
 * @return 0 on success, error code on failure
 */
int utils_bio_bridge_subscribe_module(
    utils_bio_bridge_t* bridge,
    uint32_t module_id,
    uint32_t msg_types
);

/**
 * @brief Unsubscribe module from utils messages
 *
 * WHAT: Removes subscription for module
 * WHY:  Clean unsubscription
 * HOW:  Marks subscription inactive
 *
 * @param bridge Bridge handle
 * @param module_id Module identifier
 * @return 0 on success, -1 on error
 */
int utils_bio_bridge_unsubscribe_module(
    utils_bio_bridge_t* bridge,
    uint32_t module_id
);

/**
 * @brief Update module subscription types
 *
 * WHAT: Updates subscription message types
 * WHY:  Change subscriptions without unsubscribe
 * HOW:  Updates msg_type_mask for module
 *
 * @param bridge Bridge handle
 * @param module_id Module identifier
 * @param msg_types New message type bitmask
 * @return 0 on success, -1 on error
 */
int utils_bio_bridge_update_subscription(
    utils_bio_bridge_t* bridge,
    uint32_t module_id,
    uint32_t msg_types
);

/**
 * @brief Get subscription count for message type
 *
 * WHAT: Returns number of subscribers for type
 * WHY:  Diagnostics and optimization
 * HOW:  Counts active subscriptions with type
 *
 * @param bridge Bridge handle
 * @param msg_type Message type
 * @return Subscriber count
 */
uint32_t utils_bio_bridge_get_subscriber_count(
    const utils_bio_bridge_t* bridge,
    utils_bio_msg_type_t msg_type
);

/* ============================================================================
 * Statistics and Diagnostics API
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on error
 */
int utils_bio_bridge_get_stats(
    const utils_bio_bridge_t* bridge,
    utils_bio_bridge_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int utils_bio_bridge_reset_stats(utils_bio_bridge_t* bridge);

/**
 * @brief Get message type name
 *
 * @param msg_type Message type
 * @return Human-readable name string
 */
const char* utils_bio_msg_type_name(utils_bio_msg_type_t msg_type);

/**
 * @brief Get log level name
 *
 * @param level Log level
 * @return Human-readable name string
 */
const char* utils_bio_log_level_name(utils_log_level_t level);

/**
 * @brief Get service status name
 *
 * @param status Service status
 * @return Human-readable name string
 */
const char* utils_bio_service_status_name(utils_service_status_t status);

/**
 * @brief Print bridge summary to stdout
 *
 * @param bridge Bridge handle
 */
void utils_bio_bridge_print_summary(const utils_bio_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_UTILS_BIO_ASYNC_BRIDGE_H */
