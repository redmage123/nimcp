/**
 * @file nimcp_health_agent.h
 * @brief Independent Health Monitor Agent for Real-time Error Detection
 * @version 1.0.0
 * @date 2025-01-17
 *
 * WHAT: Autonomous agent that monitors brain health independently
 * WHY:  Detect and respond to errors even when main system is compromised
 * HOW:  Separate thread with watchdog, lock-free communication to immune system
 *
 * ARCHITECTURE:
 * ```
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    HEALTH MONITOR AGENT                         │
 * │                    (Independent Thread)                         │
 * ├─────────────────────────────────────────────────────────────────┤
 * │  Detectors:              │  Checkers:                          │
 * │  • Heartbeat timeout     │  • Reference count validation       │
 * │  • Memory corruption     │  • Pointer canary checks            │
 * │  • Deadlock detection    │  • Struct magic validation          │
 * │  • Stack overflow        │  • Mutex state checking             │
 * │  • Infinite loops        │  • KG consistency                   │
 * │  • NaN/Inf propagation   │  • Circular buffer integrity        │
 * ├─────────────────────────────────────────────────────────────────┤
 * │            Lock-Free Message Queue to Immune System             │
 * │  HEALTH_MSG_* → Antigen presentation, cytokine signals,        │
 * │                 emergency quarantine, recovery requests         │
 * └─────────────────────────────────────────────────────────────────┘
 * ```
 *
 * ISOLATION GUARANTEES:
 * - Agent runs in dedicated thread with own stack
 * - Uses lock-free queues for all communication
 * - Watchdog timer independent of main thread scheduling
 * - Can detect hangs in main system via heartbeat timeout
 * - Memory-mapped shared state for non-blocking reads
 *
 * THREAD SAFETY: Agent is internally thread-safe. Communication is lock-free.
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_HEALTH_AGENT_H
#define NIMCP_HEALTH_AGENT_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

/** Health agent context (opaque) */
typedef struct nimcp_health_agent nimcp_health_agent_t;

/** Brain forward declaration */
#ifndef NIMCP_BRAIN_H
typedef struct brain_struct* brain_t;
#endif

/** Immune system forward declaration */
#ifndef NIMCP_BRAIN_IMMUNE_H
struct brain_immune_system;
typedef struct brain_immune_system brain_immune_system_t;
#endif

/** Health monitor forward declaration - must match health_monitor.h */
#ifndef NIMCP_HEALTH_MONITOR_H
struct health_monitor_internal;
typedef struct health_monitor_internal* health_monitor_t;
#endif

/** Capacity manager forward declaration */
#ifndef NIMCP_CAPACITY_MANAGER_H
struct capacity_manager;
typedef struct capacity_manager capacity_manager_t;
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum message queue depth */
#define HEALTH_AGENT_MAX_QUEUE_DEPTH     1024

/** Default heartbeat interval (ms) */
#define HEALTH_AGENT_DEFAULT_HEARTBEAT_MS 100

/** Default watchdog timeout (ms) - 5x heartbeat */
#define HEALTH_AGENT_DEFAULT_WATCHDOG_MS  500

/** Default check interval (ms) */
#define HEALTH_AGENT_DEFAULT_CHECK_MS     50

/** Magic number for struct validation */
#define HEALTH_AGENT_MAGIC               0x48454147  /* "HEAG" */

/** Canary value for memory corruption detection */
#define HEALTH_AGENT_CANARY              0xDEADBEEFCAFEBABEULL

/** Maximum number of capacity managers to track */
#define HEALTH_AGENT_MAX_CAPACITY_MANAGERS 32

/* ============================================================================
 * Message Types (Agent → Immune System)
 * ============================================================================ */

/**
 * @brief Message types for agent-to-immune communication
 */
typedef enum {
    /** Anomaly detected - present as antigen */
    HEALTH_MSG_ANOMALY_DETECTED = 0,

    /** Cytokine signal - trigger inflammatory response */
    HEALTH_MSG_CYTOKINE_SIGNAL,

    /** Emergency - immediate quarantine/checkpoint needed */
    HEALTH_MSG_EMERGENCY,

    /** Recovery request - ask immune to execute specific action */
    HEALTH_MSG_RECOVERY_REQUEST,

    /** State corruption - request rollback */
    HEALTH_MSG_STATE_CORRUPTION,

    /** Heartbeat timeout - main system may be hung */
    HEALTH_MSG_HEARTBEAT_TIMEOUT,

    /** Deadlock detected */
    HEALTH_MSG_DEADLOCK_DETECTED,

    /** NaN/Inf detected in neural computations */
    HEALTH_MSG_NAN_DETECTED,

    /** Memory corruption detected */
    HEALTH_MSG_MEMORY_CORRUPTION,

    /** Resource exhaustion imminent */
    HEALTH_MSG_RESOURCE_EXHAUSTION,

    /** Agent status update (periodic) */
    HEALTH_MSG_STATUS_UPDATE,

    HEALTH_MSG_COUNT
} health_agent_msg_type_t;

/**
 * @brief Anomaly severity levels
 */
typedef enum {
    HEALTH_SEVERITY_INFO = 0,      /**< Informational only */
    HEALTH_SEVERITY_WARNING,       /**< Potential issue */
    HEALTH_SEVERITY_ERROR,         /**< Definite problem, recoverable */
    HEALTH_SEVERITY_CRITICAL,      /**< Severe, may need rollback */
    HEALTH_SEVERITY_FATAL          /**< System integrity compromised */
} health_agent_severity_t;

/**
 * @brief Anomaly source identification
 */
typedef enum {
    HEALTH_SOURCE_UNKNOWN = 0,
    HEALTH_SOURCE_MEMORY,          /**< Memory subsystem */
    HEALTH_SOURCE_THREADING,       /**< Thread/lock subsystem */
    HEALTH_SOURCE_NEURAL,          /**< Neural computation */
    HEALTH_SOURCE_KG,              /**< Knowledge graph */
    HEALTH_SOURCE_IMMUNE,          /**< Immune system itself */
    HEALTH_SOURCE_IO,              /**< I/O operations */
    HEALTH_SOURCE_BRAIN_REGION,    /**< Specific brain region */
    HEALTH_SOURCE_CHECKPOINT,      /**< Checkpoint system */
    HEALTH_SOURCE_HEARTBEAT,       /**< Heartbeat monitoring */
    HEALTH_SOURCE_COUNT
} health_agent_source_t;

/**
 * @brief Recovery action requested by agent
 */
typedef enum {
    HEALTH_RECOVERY_NONE = 0,
    HEALTH_RECOVERY_GC,            /**< Garbage collection */
    HEALTH_RECOVERY_CHECKPOINT,    /**< Save checkpoint now */
    HEALTH_RECOVERY_ROLLBACK,      /**< Rollback to last checkpoint */
    HEALTH_RECOVERY_RESTART_THREAD,/**< Restart specific thread */
    HEALTH_RECOVERY_CLEAR_NAN,     /**< Clear NaN values */
    HEALTH_RECOVERY_REDUCE_LOAD,   /**< Reduce system load */
    HEALTH_RECOVERY_QUARANTINE,    /**< Quarantine region/component */
    HEALTH_RECOVERY_EMERGENCY_SAVE,/**< Emergency state save */
    HEALTH_RECOVERY_FULL_RESET,    /**< Full system reset */
    HEALTH_RECOVERY_COUNT
} health_agent_recovery_t;

/**
 * @brief Types of consistency checks that can be requested (Phase 3)
 */
typedef enum {
    HEALTH_CHECK_ALL = 0,          /**< Run all consistency checks */
    HEALTH_CHECK_CANARIES,         /**< Check memory canaries */
    HEALTH_CHECK_MAGIC,            /**< Check magic numbers */
    HEALTH_CHECK_REFCOUNTS,        /**< Check reference counts */
    HEALTH_CHECK_BUFFERS,          /**< Check circular buffer integrity */
    HEALTH_CHECK_MUTEX,            /**< Check mutex states */
    HEALTH_CHECK_KG,               /**< Check knowledge graph consistency */
    HEALTH_CHECK_NEURONS,          /**< Check neuron values for NaN/Inf */
    HEALTH_CHECK_COUNT
} health_agent_check_t;

/* ============================================================================
 * Message Structure
 * ============================================================================ */

/**
 * @brief Message from health agent to immune system
 */
typedef struct health_agent_message {
    health_agent_msg_type_t type;      /**< Message type */
    health_agent_severity_t severity;  /**< Severity level */
    health_agent_source_t source;      /**< Where anomaly originated */
    health_agent_recovery_t suggested_action; /**< Suggested recovery */

    uint64_t timestamp_us;             /**< When detected (microseconds) */
    uint64_t anomaly_id;               /**< Unique anomaly identifier */

    /** Anomaly-specific data */
    union {
        /** Memory anomaly details */
        struct {
            void* address;             /**< Affected address */
            size_t size;               /**< Size of affected region */
            uint64_t expected_canary;  /**< Expected canary value */
            uint64_t actual_canary;    /**< Actual canary value */
        } memory;

        /** Deadlock details */
        struct {
            uint64_t thread_id_1;      /**< First thread in deadlock */
            uint64_t thread_id_2;      /**< Second thread in deadlock */
            void* mutex_1;             /**< First mutex */
            void* mutex_2;             /**< Second mutex */
        } deadlock;

        /** NaN detection details */
        struct {
            uint32_t neuron_id;        /**< Affected neuron */
            uint32_t layer_id;         /**< Affected layer */
            float last_valid_value;    /**< Last known good value */
            uint32_t nan_count;        /**< Number of NaNs found */
        } nan;

        /** Heartbeat timeout details */
        struct {
            uint64_t last_heartbeat_us;/**< Last received heartbeat */
            uint64_t timeout_threshold_us; /**< Timeout threshold */
            uint32_t missed_beats;     /**< Consecutive missed beats */
        } heartbeat;

        /** Resource exhaustion details */
        struct {
            uint64_t memory_used;      /**< Current memory usage */
            uint64_t memory_limit;     /**< Memory limit */
            float utilization_pct;     /**< Utilization percentage */
            uint32_t time_to_exhaust_ms; /**< Estimated time to exhaustion */
        } resource;

        /** Generic data */
        uint8_t raw[64];
    } data;

    char description[128];             /**< Human-readable description */
} health_agent_message_t;

/* ============================================================================
 * Detector Configuration
 * ============================================================================ */

/**
 * @brief Configuration for individual detector
 */
typedef struct {
    bool enabled;                      /**< Detector enabled */
    uint32_t check_interval_ms;        /**< Check interval */
    health_agent_severity_t min_report_severity; /**< Minimum severity to report */
    uint32_t threshold_count;          /**< Occurrences before reporting */
    uint32_t cooldown_ms;              /**< Cooldown between reports */
} health_agent_detector_config_t;

/**
 * @brief State consistency checker configuration
 */
typedef struct {
    bool check_reference_counts;       /**< Validate refcounts */
    bool check_pointer_canaries;       /**< Check memory canaries */
    bool check_struct_magic;           /**< Validate struct magic numbers */
    bool check_mutex_state;            /**< Validate mutex consistency */
    bool check_circular_buffers;       /**< Validate ring buffers */
    bool check_kg_consistency;         /**< Full KG validation */
    bool check_neuron_values;          /**< Check for NaN/Inf */
    uint32_t kg_check_sample_rate;     /**< Sample rate for KG (1=all, N=1/N) */
    uint32_t consistency_check_interval_ms; /**< Interval between consistency checks */
} health_agent_consistency_config_t;

/**
 * @brief State consistency check result
 *
 * WHAT: Results from a consistency check cycle
 * WHY:  Report what was checked and any failures found
 * HOW:  Each field indicates pass/fail for that check type
 */
typedef struct {
    uint64_t timestamp_us;             /**< When check was performed */
    bool overall_passed;               /**< True if all enabled checks passed */

    /* Individual check results (true = passed, false = failed) */
    bool refcount_check_passed;        /**< Reference count validation */
    bool canary_check_passed;          /**< Memory canary validation */
    bool magic_check_passed;           /**< Struct magic validation */
    bool mutex_check_passed;           /**< Mutex state validation */
    bool buffer_check_passed;          /**< Circular buffer validation */
    bool kg_check_passed;              /**< Knowledge graph validation */
    bool neuron_check_passed;          /**< NaN/Inf check in neurons */

    /* Failure details */
    uint32_t refcount_errors;          /**< Number of refcount errors */
    uint32_t canary_corruptions;       /**< Number of corrupted canaries */
    uint32_t magic_violations;         /**< Number of magic violations */
    uint32_t mutex_anomalies;          /**< Number of mutex anomalies */
    uint32_t buffer_errors;            /**< Number of buffer errors */
    uint32_t kg_inconsistencies;       /**< Number of KG inconsistencies */
    uint32_t nan_inf_count;            /**< Number of NaN/Inf values found */

    /* Check timing */
    uint64_t check_duration_us;        /**< Total check duration */
} health_agent_consistency_result_t;

/* ============================================================================
 * Agent Configuration
 * ============================================================================ */

/**
 * @brief Health agent configuration
 */
typedef struct {
    /** Agent identification */
    char agent_name[64];               /**< Agent name for logging */
    uint32_t agent_id;                 /**< Unique agent ID */

    /** Timing configuration */
    uint32_t heartbeat_interval_ms;    /**< Expected heartbeat interval */
    uint32_t watchdog_timeout_ms;      /**< Watchdog timeout */
    uint32_t check_interval_ms;        /**< State check interval */
    uint32_t immune_poll_interval_ms;  /**< Immune response poll interval */

    /** Thread configuration */
    uint32_t thread_stack_size;        /**< Agent thread stack size */
    int thread_priority;               /**< Thread priority (0=normal) */
    bool pin_to_core;                  /**< Pin to specific CPU core */
    int core_id;                       /**< CPU core to pin to */

    /** Detector configurations */
    health_agent_detector_config_t heartbeat_detector;
    health_agent_detector_config_t memory_detector;
    health_agent_detector_config_t deadlock_detector;
    health_agent_detector_config_t nan_detector;
    health_agent_detector_config_t resource_detector;

    /** Consistency checker configuration */
    health_agent_consistency_config_t consistency;

    /** Communication configuration */
    uint32_t message_queue_depth;      /**< Message queue size */
    bool enable_message_batching;      /**< Batch similar messages */
    uint32_t batch_timeout_ms;         /**< Max time to batch */

    /** Recovery configuration */
    bool enable_auto_recovery;         /**< Agent can trigger recovery */
    bool enable_emergency_checkpoint;  /**< Can trigger emergency save */
    bool enable_emergency_rollback;    /**< Can trigger rollback */
    health_agent_severity_t auto_recovery_threshold; /**< Min severity for auto */

    /** Callbacks */
    void (*on_anomaly_detected)(const health_agent_message_t* msg, void* user_data);
    void (*on_recovery_executed)(health_agent_recovery_t action, bool success, void* user_data);
    void* callback_user_data;

} health_agent_config_t;

/* ============================================================================
 * Agent Statistics
 * ============================================================================ */

/**
 * @brief Health agent statistics
 */
typedef struct {
    uint64_t uptime_ms;                /**< Agent uptime */
    uint64_t checks_performed;         /**< Total checks performed */
    uint64_t anomalies_detected;       /**< Total anomalies detected */
    uint64_t messages_sent;            /**< Messages sent to immune */
    uint64_t recoveries_triggered;     /**< Recoveries triggered */
    uint64_t recoveries_succeeded;     /**< Successful recoveries */
    uint64_t heartbeats_received;      /**< Heartbeats received */
    uint64_t heartbeat_timeouts;       /**< Heartbeat timeouts */
    uint64_t deadlocks_detected;       /**< Deadlocks detected */
    uint64_t nans_detected;            /**< NaN values detected */
    uint64_t memory_corruptions;       /**< Memory corruptions found */
    uint64_t consistency_failures;     /**< State consistency failures */
    float avg_check_duration_us;       /**< Average check duration */
    float avg_message_latency_us;      /**< Avg message delivery time */
    uint32_t queue_high_watermark;     /**< Max queue depth seen */
    health_agent_severity_t highest_severity_seen; /**< Worst severity seen */
} health_agent_stats_t;

/* ============================================================================
 * Agent Lifecycle API
 * ============================================================================ */

/**
 * @brief Create health monitor agent
 *
 * WHAT: Initialize independent health monitoring agent
 * WHY:  Enable autonomous error detection and response
 * HOW:  Allocate context, configure detectors, prepare message queue
 *
 * @param config Agent configuration (NULL for defaults)
 * @return Agent handle or NULL on error
 */
nimcp_health_agent_t* nimcp_health_agent_create(const health_agent_config_t* config);

/**
 * @brief Get default agent configuration
 *
 * @param config Output configuration
 */
void nimcp_health_agent_default_config(health_agent_config_t* config);

/**
 * @brief Connect agent to brain for monitoring
 *
 * WHAT: Attach agent to brain instance
 * WHY:  Agent needs brain reference for state inspection
 * HOW:  Store reference, set up shared memory region
 *
 * @param agent Health agent
 * @param brain Brain to monitor
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_connect_brain(nimcp_health_agent_t* agent, brain_t brain);

/**
 * @brief Connect agent to immune system for communication
 *
 * WHAT: Establish communication channel to immune system
 * WHY:  Agent sends anomaly reports to immune for response
 * HOW:  Set up lock-free message queue, register callbacks
 *
 * @param agent Health agent
 * @param immune Brain immune system
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_connect_immune(nimcp_health_agent_t* agent,
                                       brain_immune_system_t* immune);

/**
 * @brief Connect agent to existing health monitor
 *
 * WHAT: Integrate with existing health monitor for metrics
 * WHY:  Agent can use health monitor's collected metrics
 * HOW:  Share metrics access, coordinate monitoring
 *
 * @param agent Health agent
 * @param monitor Existing health monitor
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_connect_monitor(nimcp_health_agent_t* agent,
                                        health_monitor_t* monitor);

/**
 * @brief Start the health agent
 *
 * WHAT: Begin autonomous monitoring
 * WHY:  Activate all detectors and start agent thread
 * HOW:  Spawn dedicated thread, start watchdog timer
 *
 * @param agent Health agent
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_start(nimcp_health_agent_t* agent);

/**
 * @brief Stop the health agent
 *
 * WHAT: Gracefully stop monitoring
 * WHY:  Shutdown before system teardown
 * HOW:  Signal thread to stop, wait for completion, flush queue
 *
 * @param agent Health agent
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_stop(nimcp_health_agent_t* agent);

/**
 * @brief Destroy health agent
 *
 * WHAT: Release all agent resources
 * WHY:  Clean shutdown
 * HOW:  Stop if running, free memory, close queues
 *
 * @param agent Health agent (NULL safe)
 */
void nimcp_health_agent_destroy(nimcp_health_agent_t* agent);

/* ============================================================================
 * Heartbeat API
 * ============================================================================ */

/**
 * @brief Send heartbeat to agent
 *
 * WHAT: Signal that main system is alive
 * WHY:  Agent detects hangs via heartbeat timeout
 * HOW:  Update shared timestamp (lock-free)
 *
 * Call this periodically from main system to indicate liveness.
 *
 * @param agent Health agent
 */
void nimcp_health_agent_heartbeat(nimcp_health_agent_t* agent);

/**
 * @brief Send heartbeat with context
 *
 * WHAT: Heartbeat with additional state information
 * WHY:  Provide agent with current operation context
 * HOW:  Update timestamp and context (lock-free)
 *
 * @param agent Health agent
 * @param operation Current operation name
 * @param progress Progress indicator (0.0-1.0)
 */
void nimcp_health_agent_heartbeat_ex(nimcp_health_agent_t* agent,
                                      const char* operation,
                                      float progress);

/* ============================================================================
 * Manual Trigger API
 * ============================================================================ */

/**
 * @brief Manually report anomaly to agent
 *
 * WHAT: Report detected anomaly from other code
 * WHY:  Other modules can report issues to agent
 * HOW:  Queue message for processing
 *
 * @param agent Health agent
 * @param msg Anomaly message
 * @return 0 on success, -1 on error (queue full)
 */
int nimcp_health_agent_report_anomaly(nimcp_health_agent_t* agent,
                                       const health_agent_message_t* msg);

/**
 * @brief Request immediate state check
 *
 * WHAT: Trigger on-demand consistency check
 * WHY:  Other modules can request verification
 * HOW:  Signal agent to run full check cycle
 *
 * @param agent Health agent
 * @return 0 on success
 */
int nimcp_health_agent_request_check(nimcp_health_agent_t* agent);

/**
 * @brief Request emergency checkpoint
 *
 * WHAT: Ask agent to trigger emergency save
 * WHY:  Code detects impending failure
 * HOW:  Agent sends HEALTH_MSG_EMERGENCY to immune
 *
 * @param agent Health agent
 * @param reason Reason for emergency
 * @return 0 on success
 */
int nimcp_health_agent_request_emergency_checkpoint(nimcp_health_agent_t* agent,
                                                     const char* reason);

/* ============================================================================
 * State Consistency API (Phase 3)
 * ============================================================================ */

/**
 * @brief Run immediate state consistency check
 *
 * WHAT: Perform all enabled consistency checks synchronously
 * WHY:  On-demand validation of system state integrity
 * HOW:  Run each check type, aggregate results, optionally report failures
 *
 * @param agent Health agent
 * @param result Output consistency result (can be NULL if not needed)
 * @return 0 if all checks passed, number of failures otherwise
 */
int nimcp_health_agent_check_consistency(
    nimcp_health_agent_t* agent,
    health_agent_consistency_result_t* result
);

/**
 * @brief Get last consistency check result
 *
 * WHAT: Retrieve results of most recent consistency check
 * WHY:  Query check history without running new check
 * HOW:  Copy cached result from agent
 *
 * @param agent Health agent
 * @param result Output consistency result
 * @return 0 on success, -1 if no check has been run
 */
int nimcp_health_agent_get_consistency_status(
    const nimcp_health_agent_t* agent,
    health_agent_consistency_result_t* result
);

/**
 * @brief Update consistency checker configuration at runtime
 *
 * WHAT: Modify which consistency checks are enabled
 * WHY:  Adjust checking based on system load or requirements
 * HOW:  Safely update config under mutex protection
 *
 * @param agent Health agent
 * @param config New consistency configuration
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_update_consistency_config(
    nimcp_health_agent_t* agent,
    const health_agent_consistency_config_t* config
);

/**
 * @brief Check specific struct magic number
 *
 * WHAT: Validate magic number for a specific structure type
 * WHY:  Detect memory corruption or type confusion
 * HOW:  Compare against expected value
 *
 * @param ptr Pointer to structure
 * @param expected_magic Expected magic value
 * @param struct_name Name for error reporting
 * @return true if magic is valid, false otherwise
 */
bool nimcp_health_agent_validate_magic(
    const void* ptr,
    uint32_t expected_magic,
    const char* struct_name
);

/**
 * @brief Register a structure for magic validation
 *
 * WHAT: Add a structure to the consistency checker's registry
 * WHY:  Enable automatic magic validation for registered structures
 * HOW:  Store pointer, expected magic, and name in registry
 *
 * @param agent Health agent
 * @param ptr Pointer to structure (magic at offset 0)
 * @param expected_magic Expected magic value
 * @param name Structure name for reporting
 * @return 0 on success, -1 if registry full
 */
int nimcp_health_agent_register_struct(
    nimcp_health_agent_t* agent,
    void* ptr,
    uint32_t expected_magic,
    const char* name
);

/**
 * @brief Unregister a structure from magic validation
 *
 * WHAT: Remove a structure from the consistency checker's registry
 * WHY:  Clean up when structure is destroyed
 * HOW:  Find and remove from registry
 *
 * @param agent Health agent
 * @param ptr Pointer to structure
 * @return 0 on success, -1 if not found
 */
int nimcp_health_agent_unregister_struct(
    nimcp_health_agent_t* agent,
    void* ptr
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Check if agent is running
 *
 * @param agent Health agent
 * @return true if agent thread is active
 */
bool nimcp_health_agent_is_running(const nimcp_health_agent_t* agent);

/**
 * @brief Get agent statistics
 *
 * @param agent Health agent
 * @param stats Output statistics
 */
void nimcp_health_agent_get_stats(const nimcp_health_agent_t* agent,
                                   health_agent_stats_t* stats);

/**
 * @brief Get pending message count
 *
 * @param agent Health agent
 * @return Number of messages waiting to be sent to immune
 */
uint32_t nimcp_health_agent_pending_messages(const nimcp_health_agent_t* agent);

/**
 * @brief Dequeue a message from the agent's message queue
 *
 * WHAT: Remove and return the oldest message from the agent's queue
 * WHY:  Enables immune system tick orchestrator to process health messages
 * HOW:  Pop from lock-free MPSC queue, copy to output buffer
 *
 * This function is used by brain_immune_tick() to drain the health agent's
 * message queue and process each message as an immune system event.
 *
 * @param agent Health agent
 * @param msg Output message buffer (must not be NULL)
 * @return true if a message was dequeued, false if queue was empty
 */
bool nimcp_health_agent_dequeue_message(nimcp_health_agent_t* agent,
                                         health_agent_message_t* msg);

/**
 * @brief Get current queue depth
 *
 * WHAT: Return number of messages currently in the queue
 * WHY:  Enables monitoring of queue utilization and backpressure
 * HOW:  Calculate difference between head and tail pointers
 *
 * @param agent Health agent
 * @return Number of messages in queue, 0 if agent is NULL
 */
uint32_t nimcp_health_agent_get_queue_depth(const nimcp_health_agent_t* agent);

/**
 * @brief Get current agent health status
 *
 * @param agent Health agent
 * @return Current severity level (highest active)
 */
health_agent_severity_t nimcp_health_agent_current_status(
    const nimcp_health_agent_t* agent);

/* ============================================================================
 * Configuration Update API
 * ============================================================================ */

/**
 * @brief Update detector configuration at runtime
 *
 * @param agent Health agent
 * @param detector Detector name ("heartbeat", "memory", "deadlock", "nan", "resource")
 * @param config New configuration
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_update_detector(nimcp_health_agent_t* agent,
                                        const char* detector,
                                        const health_agent_detector_config_t* config);

/**
 * @brief Enable/disable specific detector
 *
 * @param agent Health agent
 * @param detector Detector name
 * @param enabled Enable or disable
 * @return 0 on success
 */
int nimcp_health_agent_set_detector_enabled(nimcp_health_agent_t* agent,
                                             const char* detector,
                                             bool enabled);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Convert message type to string
 */
const char* health_agent_msg_type_to_string(health_agent_msg_type_t type);

/**
 * @brief Convert severity to string
 */
const char* health_agent_severity_to_string(health_agent_severity_t severity);

/**
 * @brief Convert source to string
 */
const char* health_agent_source_to_string(health_agent_source_t source);

/**
 * @brief Convert recovery action to string
 */
const char* health_agent_recovery_to_string(health_agent_recovery_t recovery);

/**
 * @brief Create anomaly message helper
 *
 * @param type Message type
 * @param severity Severity level
 * @param source Anomaly source
 * @param description Description format string
 * @return Populated message (caller fills data union)
 */
health_agent_message_t nimcp_health_agent_create_message(
    health_agent_msg_type_t type,
    health_agent_severity_t severity,
    health_agent_source_t source,
    const char* description,
    ...);

/* ============================================================================
 * Cognitive Module Forward Declarations
 * ============================================================================ */

/** Failure prediction module */
#ifndef NIMCP_FAILURE_PREDICTION_H
typedef struct failure_predictor failure_predictor_t;
#endif

/** Metacognition module */
#ifndef NIMCP_METACOGNITION_H
typedef struct metacognition metacognition_t;
#endif

/** Ethics engine */
#ifndef NIMCP_ETHICS_H
typedef struct ethics_engine_struct* ethics_engine_t;
#endif

/** Emotional system */
#ifndef NIMCP_EMOTIONAL_SYSTEM_H
typedef struct emotional_system emotional_system_t;
#endif

/** Emotion-immune bridge */
#ifndef NIMCP_EMOTION_IMMUNE_BRIDGE_H
typedef struct emotion_immune_bridge_s emotion_immune_bridge_t;
#endif

/** Wellbeing system */
#ifndef NIMCP_WELLBEING_H
typedef struct wellbeing_monitor wellbeing_monitor_t;
#endif

/** Mental health monitor */
#ifndef NIMCP_MENTAL_HEALTH_H
typedef struct mental_health_monitor mental_health_monitor_t;
#endif

/** Collective cognition */
#ifndef NIMCP_COLLECTIVE_COGNITION_H
typedef struct collective_cognition collective_cognition_t;
#endif

/** Recursive cognition (RCOG) engine */
#ifndef NIMCP_RCOG_H
typedef struct rcog_engine rcog_engine_t;
#endif

/** GPU health monitor */
#ifndef NIMCP_GPU_HEALTH_H
typedef struct gpu_health_monitor gpu_health_monitor_t;
#endif

/* ============================================================================
 * Hypothalamus & Homeostasis Module Forward Declarations
 * ============================================================================ */

/** Hypothalamus orchestrator - central drive coordination */
#ifndef NIMCP_HYPOTHALAMUS_ORCHESTRATOR_H
typedef struct hypo_orchestrator_struct* hypo_orchestrator_t;
#endif

/** Hypothalamus homeostasis system - PID-based regulation */
#ifndef NIMCP_HYPOTHALAMUS_HOMEOSTASIS_H
typedef struct hypo_homeostasis hypo_homeostasis_handle_t;
#endif

/** Hypothalamus immune bridge - bidirectional neuroimmune integration */
#ifndef NIMCP_HYPOTHALAMUS_IMMUNE_BRIDGE_H
typedef struct hypo_immune_bridge hypo_immune_bridge_t;
#endif

/** Hypothalamus drive system */
#ifndef NIMCP_HYPOTHALAMUS_DRIVES_H
typedef struct hypo_drive_system hypo_drive_system_handle_t;
#endif

/* ============================================================================
 * Additional Valuable Module Forward Declarations
 * ============================================================================ */

/** Anomaly detector - statistical anomaly detection */
#ifndef NIMCP_ANOMALY_DETECTOR_H
typedef struct anomaly_detector anomaly_detector_t;
#endif

/** Connectivity health - module isolation detection */
#ifndef NIMCP_CONNECTIVITY_HEALTH_H
typedef struct connectivity_health connectivity_health_t;
#endif

/** Brain oscillations - EEG-style brain wave monitoring */
#ifndef NIMCP_BRAIN_OSCILLATIONS_H
typedef struct brain_oscillations brain_oscillations_t;
#endif

/** Knowledge graph GC - memory management */
#ifndef NIMCP_KG_GC_H
typedef struct kg_gc_context kg_gc_context_t;
#endif

/** Checkpoint manager - state persistence */
#ifndef NIMCP_CHECKPOINT_H
typedef struct checkpoint_manager checkpoint_manager_t;
#endif

/** Deadlock detector - threading health */
#ifndef NIMCP_DEADLOCK_DETECTOR_H
typedef struct deadlock_detector deadlock_detector_t;
#endif

/** Bio-async router - event messaging */
#ifndef NIMCP_BIO_ROUTER_H
typedef struct bio_router_struct* bio_router_t;
#endif

/** Runtime adaptation - load management */
#ifndef NIMCP_RUNTIME_ADAPTATION_H
typedef struct runtime_adaptation_context_internal* runtime_adaptation_context_t;
#endif

/** Exception-immune bridge - exception handling integration */
#ifndef NIMCP_EXCEPTION_IMMUNE_H
typedef struct exception_immune_integration exception_immune_t;
#endif

/* ============================================================================
 * Neural Module (SNN/LNN) Forward Declarations
 * ============================================================================ */

/** SNN immune bridge - spiking neural network immune integration */
#ifndef NIMCP_SNN_IMMUNE_H
typedef struct snn_immune_bridge_s snn_immune_bridge_t;
#endif

/** LNN immune bridge - liquid neural network immune integration */
#ifndef NIMCP_LNN_IMMUNE_H
typedef struct lnn_immune_bridge lnn_immune_bridge_t;
#endif

/* ============================================================================
 * Portia/Dragonfly/Swarm/Memory Module Forward Declarations
 * ============================================================================ */

/** Portia context - adaptive resource optimization */
#ifndef NIMCP_PORTIA_H
typedef struct portia_context_t portia_context_t;
#endif

/** Dragonfly system - target tracking/interception */
#ifndef NIMCP_DRAGONFLY_H
typedef struct dragonfly_system_s dragonfly_system_t;
#endif

/** Dragonfly immune bridge - hunting behavior immune integration (Phase 5.6) */
#ifndef NIMCP_DRAGONFLY_IMMUNE_BRIDGE_H
typedef struct dragonfly_immune_bridge_s* dragonfly_immune_bridge_t;
#endif

/** Portia monitor - platform resource monitoring (Phase 5.6) */
#ifndef NIMCP_PORTIA_MONITORING_H
typedef struct portia_monitor_struct* portia_monitor_t;
#endif

/*
 * Swarm immune system - distributed threat detection
 * Note: NimcpSwarmImmuneSystem is defined in nimcp_swarm_immune.h as an
 * anonymous struct typedef. We use a void pointer wrapper here to avoid
 * conflicts when the full header is included.
 */
#ifndef NIMCP_SWARM_IMMUNE_SYSTEM_DEFINED_HA
#define NIMCP_SWARM_IMMUNE_SYSTEM_DEFINED_HA
typedef void* nimcp_swarm_immune_ptr_t;
#endif

/*
 * Swarm memory system - distributed memory consolidation
 * Note: NimcpSwarmMemory is the actual type in nimcp_swarm_memory.h.
 * We use a void pointer wrapper here to avoid conflicts.
 */
#ifndef NIMCP_SWARM_MEMORY_DEFINED_HA
#define NIMCP_SWARM_MEMORY_DEFINED_HA
typedef void* nimcp_swarm_memory_ptr_t;
#endif

/** Engram system - memory trace representation */
#ifndef NIMCP_ENGRAM_H
typedef struct engram_system engram_system_t;
#endif

/** Systems consolidation - hippocampus to cortex transfer */
#ifndef NIMCP_SYSTEMS_CONSOLIDATION_H
typedef struct systems_consolidation_system systems_consolidation_system_t;
#endif

/* ============================================================================
 * Cognitive Integration Configuration
 * ============================================================================ */

/**
 * @brief Configuration for failure prediction integration
 */
typedef struct {
    bool enable_failure_prediction;     /**< Enable predictive failure detection */
    float prediction_threshold;         /**< Prediction confidence threshold (0.0-1.0) */
    uint32_t prediction_horizon_ms;     /**< How far ahead to predict */
    bool enable_preventive_action;      /**< Auto-trigger preventive recovery */
    bool enable_trend_analysis;         /**< Analyze metric trends */
} health_agent_prediction_config_t;

/**
 * @brief Configuration for metacognition integration
 */
typedef struct {
    bool enable_metacognition;          /**< Enable self-monitoring */
    bool enable_confidence_calibration; /**< Calibrate decision confidence */
    bool enable_degradation_detection;  /**< Detect cognitive degradation */
    float degradation_threshold;        /**< Threshold for degradation alert */
    bool enable_self_diagnosis;         /**< Enable self-diagnosis */
} health_agent_metacog_config_t;

/**
 * @brief Configuration for ethics integration
 */
typedef struct {
    bool enable_ethics_evaluation;      /**< Evaluate recovery actions ethically */
    bool enable_asimov_laws;            /**< Apply Asimov's Laws to decisions */
    bool enable_mercy_directive;        /**< Prefer graceful degradation */
    bool enable_golden_rule;            /**< Apply Golden Rule test */
    float ethics_override_threshold;    /**< Severity to override ethics (0.95) */
} health_agent_ethics_config_t;

/**
 * @brief Configuration for emotion integration
 */
typedef struct {
    bool enable_emotion_awareness;      /**< Adjust thresholds based on emotion */
    bool enable_emotion_reporting;      /**< Report health events to emotion */
    bool enable_stress_adjustment;      /**< Adjust sensitivity based on stress */
    float stress_threshold_multiplier;  /**< Threshold multiplier when stressed */
} health_agent_emotion_config_t;

/**
 * @brief Configuration for wellbeing integration
 */
typedef struct {
    bool enable_wellbeing_monitoring;   /**< Monitor system wellbeing */
    bool enable_distress_detection;     /**< Detect system distress */
    bool enable_suffering_prevention;   /**< Prevent potential suffering */
    float distress_intervention_threshold; /**< Distress level for intervention */
} health_agent_wellbeing_config_t;

/**
 * @brief Configuration for collective cognition integration
 */
typedef struct {
    bool enable_collective_monitoring;  /**< Use collective for monitoring */
    bool enable_consensus_decisions;    /**< Require consensus for recovery */
    bool enable_swarm_immune;           /**< Coordinate immune across instances */
    float consensus_threshold;          /**< Required consensus (0.0-1.0) */
    uint32_t consensus_timeout_ms;      /**< Timeout for consensus */
} health_agent_collective_config_t;

/**
 * @brief Configuration for recursive cognition integration
 */
typedef struct {
    bool enable_rcog_diagnosis;         /**< Use RCOG for diagnosis */
    bool enable_rcog_recovery_planning; /**< Use RCOG for recovery planning */
    bool enable_imagination;            /**< Use imagination for "what-if" */
    uint32_t rcog_timeout_ms;           /**< Timeout for RCOG operations */
    float confidence_threshold;         /**< Min confidence for RCOG decisions */
} health_agent_rcog_config_t;

/**
 * @brief Configuration for GPU integration
 */
typedef struct {
    bool enable_gpu_monitoring;         /**< Monitor GPU health */
    bool enable_gpu_acceleration;       /**< Use GPU for health computations */
    bool enable_tensor_validation;      /**< GPU-accelerated tensor validation */
    bool enable_anomaly_detection;      /**< GPU-accelerated anomaly detection */
    bool enable_auto_recovery;          /**< Auto-execute GPU recovery actions */
    bool enable_predictive_monitoring;  /**< Predict GPU failures */
    uint32_t gpu_check_interval_ms;     /**< GPU health check interval */
    float temp_warning_celsius;         /**< Temperature warning threshold */
    float temp_critical_celsius;        /**< Temperature critical threshold */
    float memory_warning_pct;           /**< Memory usage warning (0.0-1.0) */
    float memory_critical_pct;          /**< Memory usage critical (0.0-1.0) */
} health_agent_gpu_config_t;

/**
 * @brief Configuration for hypothalamus integration (USE, not just monitor)
 */
typedef struct {
    bool enable_hypothalamus;            /**< Enable hypothalamus integration */
    bool enable_homeostatic_regulation;  /**< USE homeostasis for self-regulation */
    bool enable_drive_response;          /**< Respond to drive events actively */
    bool enable_stress_coordination;     /**< Coordinate stress responses */
    bool enable_sickness_behavior;       /**< USE sickness behavior for safety mode */
    bool enable_immune_bridge;           /**< USE bidirectional immune integration */
    float stress_trigger_threshold;      /**< Health score to trigger stress response */
    float sickness_trigger_threshold;    /**< Health score to trigger sickness behavior */
    uint32_t homeostasis_update_ms;      /**< Homeostasis update interval */
    uint32_t drive_response_timeout_ms;  /**< Timeout for drive event response */
} health_agent_hypothalamus_config_t;

/**
 * @brief Configuration for connectivity health integration
 */
typedef struct {
    bool enable_connectivity_monitoring; /**< Enable connectivity health checks */
    bool enable_isolation_detection;     /**< Detect isolated modules */
    bool enable_auto_reconnect;          /**< Auto-attempt reconnection */
    uint32_t check_interval_ms;          /**< Connectivity check interval */
    float isolation_threshold;           /**< Threshold for isolation alert */
} health_agent_connectivity_config_t;

/**
 * @brief Configuration for brain oscillations integration
 */
typedef struct {
    bool enable_oscillation_monitoring;  /**< Monitor brain oscillations */
    bool enable_seizure_detection;       /**< Detect seizure-like patterns */
    bool enable_flatline_detection;      /**< Detect flatline (no activity) */
    bool enable_desync_detection;        /**< Detect desynchronization */
    float abnormal_threshold;            /**< Threshold for abnormal oscillation */
    uint32_t sample_rate_hz;             /**< Oscillation sampling rate */
} health_agent_oscillations_config_t;

/**
 * @brief Configuration for GC/memory integration
 */
typedef struct {
    bool enable_gc_integration;          /**< Enable GC integration */
    bool enable_auto_gc_trigger;         /**< Auto-trigger GC on memory pressure */
    bool enable_leak_detection;          /**< Detect memory leaks */
    float gc_trigger_threshold;          /**< Memory usage to trigger GC */
    uint32_t gc_cooldown_ms;             /**< Minimum time between GC runs */
} health_agent_gc_config_t;

/**
 * @brief Configuration for checkpoint integration
 */
typedef struct {
    bool enable_checkpoint_integration;  /**< Enable checkpoint integration */
    bool enable_auto_checkpoint;         /**< Auto-checkpoint on good health */
    bool enable_auto_rollback;           /**< Auto-rollback on critical failure */
    uint32_t checkpoint_interval_ms;     /**< Auto-checkpoint interval */
    float health_threshold_checkpoint;   /**< Min health score for auto-checkpoint */
    float health_threshold_rollback;     /**< Health score below which to rollback */
} health_agent_checkpoint_config_t;

/**
 * @brief Configuration for bio-async integration
 */
typedef struct {
    bool enable_bio_async;               /**< Enable bio-async messaging */
    bool publish_health_events;          /**< Publish health events to bio-async */
    bool subscribe_health_requests;      /**< Subscribe to health check requests */
    uint32_t event_batch_size;           /**< Batch size for events */
    uint32_t event_batch_timeout_ms;     /**< Max time to batch events */
} health_agent_bio_async_config_t;

/**
 * @brief Configuration for exception-immune integration
 */
typedef struct {
    bool enable_exception_integration;   /**< Enable exception-immune bridge */
    bool auto_present_exceptions;        /**< Auto-present exceptions as antigens */
    bool enable_recovery_callbacks;      /**< Use recovery callbacks */
    uint32_t exception_severity_threshold; /**< Min severity for auto-present */
} health_agent_exception_config_t;

/* ============================================================================
 * Neural Module (SNN/LNN) Integration Configuration
 * ============================================================================ */

/**
 * @brief Configuration for SNN immune bridge integration
 *
 * WHAT: Configuration for spiking neural network health monitoring
 * WHY:  SNN can exhibit instabilities (silent network, explosions, NaN)
 * HOW:  Periodic health checks via snn_immune_get_health()
 */
typedef struct {
    bool enable_snn_monitoring;          /**< Enable SNN health monitoring */
    bool enable_instability_detection;   /**< Detect SNN instabilities */
    bool enable_auto_report;             /**< Auto-report instabilities to immune */
    bool enable_learning_modulation;     /**< Allow immune to modulate STDP */
    float max_spike_rate_hz;             /**< Max firing rate before alert (100.0) */
    float min_spike_rate_hz;             /**< Min firing rate (silent detect) (0.1) */
    float burst_threshold;               /**< Burst ratio for epileptiform (0.5) */
    float sync_threshold;                /**< Hypersynchrony threshold (0.8) */
    uint32_t check_interval_ms;          /**< How often to check SNN health (100) */
} health_agent_snn_config_t;

/**
 * @brief Configuration for LNN immune bridge integration
 *
 * WHAT: Configuration for liquid neural network health monitoring
 * WHY:  LNN can exhibit ODE instabilities (divergence, tau collapse, NaN)
 * HOW:  Periodic stability checks via lnn_immune_check_stability()
 */
typedef struct {
    bool enable_lnn_monitoring;          /**< Enable LNN health monitoring */
    bool enable_stability_detection;     /**< Detect LNN instabilities */
    bool enable_auto_report;             /**< Auto-report instabilities to immune */
    bool enable_tau_modulation;          /**< Allow immune to modulate tau */
    bool enable_lr_modulation;           /**< Allow immune to modulate LR */
    float state_explosion_threshold;     /**< State norm for explosion (1000.0) */
    float state_collapse_threshold;      /**< State norm for collapse (1e-6) */
    float tau_max;                       /**< Maximum allowed tau (10.0) */
    float tau_min;                       /**< Minimum allowed tau (0.001) */
    float gradient_explosion_threshold;  /**< Gradient norm for explosion (100.0) */
    float gradient_vanishing_threshold;  /**< Gradient norm for vanishing (1e-7) */
    uint32_t check_interval_ms;          /**< How often to check LNN health (100) */
} health_agent_lnn_config_t;

/**
 * @brief Aggregated neural health metrics from SNN and LNN
 *
 * WHAT: Combined health status from all neural computation modules
 * WHY:  Single view of neural computation health for the health agent
 * HOW:  Aggregates snn_health_metrics_t and lnn stability checks
 */
typedef struct {
    /* SNN metrics */
    bool snn_connected;                  /**< SNN bridge is connected */
    bool snn_healthy;                    /**< SNN network is healthy */
    float snn_mean_rate;                 /**< Mean firing rate (Hz) */
    float snn_max_rate;                  /**< Maximum neuron rate (Hz) */
    float snn_burst_ratio;               /**< Fraction of spikes in bursts */
    float snn_sync_index;                /**< Population synchrony [0, 1] */
    uint32_t snn_silent_neurons;         /**< Number of silent neurons */
    uint32_t snn_saturated_neurons;      /**< Neurons at max rate */
    uint32_t snn_instability_count;      /**< Total instabilities detected */

    /* LNN metrics */
    bool lnn_connected;                  /**< LNN bridge is connected */
    bool lnn_healthy;                    /**< LNN network is healthy */
    float lnn_tau_scale;                 /**< Current tau scale factor */
    float lnn_lr_factor;                 /**< Current learning rate factor */
    float lnn_state_damping;             /**< Current state damping factor */
    uint32_t lnn_instability_count;      /**< Total instabilities detected */
    uint32_t lnn_nan_detections;         /**< NaN detections in LNN */
    uint32_t lnn_inf_detections;         /**< Inf detections in LNN */
    uint32_t lnn_tau_violations;         /**< Tau bound violations */
    uint32_t lnn_gradient_issues;        /**< Gradient explosion/vanishing */

    /* Combined metrics */
    bool any_neural_unhealthy;           /**< Any neural module unhealthy */
    float neural_health_score;           /**< Combined neural health [0-100] */
    uint32_t total_instabilities;        /**< Total across all neural modules */
    uint64_t last_check_time_us;         /**< Last check timestamp */
} neural_health_metrics_t;

/* ============================================================================
 * Behavioral Module (Dragonfly/Portia) Health Metrics (Phase 5.6)
 * ============================================================================ */

/**
 * @brief Aggregated behavioral health metrics from Dragonfly and Portia
 *
 * WHAT: Combined health status from behavioral and resource monitoring modules
 * WHY:  Single view of behavioral module health for the health agent
 * HOW:  Aggregates dragonfly_immune_state_t and portia monitoring data
 */
typedef struct {
    /* Dragonfly immune metrics */
    bool dragonfly_connected;             /**< Dragonfly bridge is connected */
    bool dragonfly_healthy;               /**< Dragonfly system is healthy */
    uint8_t health_status;                /**< health_status_t: OPTIMAL..CRITICAL */
    uint8_t stress_level;                 /**< stress_level_t: NONE..CHRONIC */
    float speed_modifier;                 /**< Hunting speed capability [0,1] */
    float accuracy_modifier;              /**< Hunting accuracy capability [0,1] */
    float endurance_modifier;             /**< Hunting endurance capability [0,1] */
    float fatigue_level;                  /**< Physical fatigue [0,1] */
    float frustration_level;              /**< Frustration from failures [0,1] */
    float energy_reserves;                /**< Remaining energy [0,1] */
    bool is_injured;                      /**< Currently injured */
    bool hunting_recommended;             /**< Is hunting recommended? */
    float rest_urgency;                   /**< Urgency to rest [0,1] */
    uint32_t consecutive_failures;        /**< Consecutive hunt failures */

    /* Portia monitor metrics */
    bool portia_connected;                /**< Portia monitor is connected */
    bool portia_healthy;                  /**< Portia system is healthy */
    uint8_t thermal_state;                /**< portia_thermal_state_t: NOMINAL..CRITICAL */
    uint8_t power_state;                  /**< portia_power_state_t: AC..CRITICAL */
    uint8_t degradation_level;            /**< portia_degradation_level_t: NONE..EMERGENCY */
    float cpu_temp_c;                     /**< Current CPU temperature (Celsius) */
    float battery_pct;                    /**< Current battery level (0-100%) */
    float cpu_load_pct;                   /**< Current CPU utilization (0-100%) */
    bool ac_connected;                    /**< AC power connected */
    bool is_throttled;                    /**< Thermal throttling active */
    uint32_t thermal_warnings;            /**< Thermal warning count */
    uint32_t power_warnings;              /**< Power warning count */

    /* Cross-module coordination */
    bool thermal_abort_recommended;       /**< Recommend abort due to thermal */
    bool power_abort_recommended;         /**< Recommend abort due to power */
    bool conservation_mode_active;        /**< Power conservation active */

    /* Combined metrics */
    bool any_behavioral_unhealthy;        /**< Any behavioral module unhealthy */
    float behavioral_health_score;        /**< Combined behavioral health [0-100] */
    uint64_t last_check_time_us;          /**< Last check timestamp */
} behavioral_health_metrics_t;

/* ============================================================================
 * Portia/Dragonfly/Swarm/Memory Integration Configuration
 * ============================================================================ */

/**
 * @brief Configuration for Portia adaptive resource management
 */
typedef struct {
    bool enable_portia;                   /**< Enable Portia integration */
    bool enable_tier_monitoring;          /**< Monitor platform tier changes */
    bool enable_power_awareness;          /**< Respond to power state changes */
    bool enable_thermal_monitoring;       /**< Monitor thermal state */
    bool enable_degradation_coordination; /**< Coordinate graceful degradation */
    bool enable_auto_tier_switch;         /**< USE Portia for auto tier switching */
    float degradation_trigger_threshold;  /**< Health score to trigger degradation */
    float upgrade_health_threshold;       /**< Health score to allow tier upgrade */
    uint32_t tier_check_interval_ms;      /**< Tier check interval */
} health_agent_portia_config_t;

/**
 * @brief Configuration for Dragonfly tracking/interception
 */
typedef struct {
    bool enable_dragonfly;                /**< Enable Dragonfly integration */
    bool enable_anomaly_tracking;         /**< Track anomalies as "targets" */
    bool enable_pursuit_mode;             /**< Actively pursue critical issues */
    bool enable_interception;             /**< Intercept before failure */
    bool enable_prediction_integration;   /**< USE prediction for interception */
    float lock_on_severity_threshold;     /**< Severity to lock onto "target" */
    float pursuit_timeout_s;              /**< Max time to pursue issue */
    uint32_t update_rate_hz;              /**< Dragonfly update rate */
} health_agent_dragonfly_config_t;

/* ============================================================================
 * Behavioral Module (Dragonfly/Portia) Immune Integration Configuration
 * ============================================================================ */

/**
 * @brief Configuration for Dragonfly immune bridge integration (Phase 5.6)
 *
 * WHAT: Configuration for hunting behavior health monitoring
 * WHY:  Hunting stress and health status affect overall system health
 * HOW:  Monitor dragonfly immune bridge for stress/health changes
 */
typedef struct {
    bool enable_dragonfly_immune;         /**< Enable dragonfly immune monitoring */
    bool enable_stress_monitoring;        /**< Monitor hunting stress levels */
    bool enable_health_status_tracking;   /**< Track health status changes */
    bool enable_injury_detection;         /**< Detect hunting injuries */
    bool enable_fatigue_tracking;         /**< Track fatigue levels */
    bool enable_cross_coordination;       /**< Coordinate with other modules */

    /* Stress thresholds */
    float stress_warning_threshold;       /**< Stress level for warning (0.5) */
    float stress_critical_threshold;      /**< Stress level for critical (0.8) */
    float fatigue_warning_threshold;      /**< Fatigue level for warning (0.6) */
    float fatigue_critical_threshold;     /**< Fatigue level for critical (0.9) */

    /* Health coordination */
    bool abort_hunt_on_thermal;           /**< Abort hunt if thermal critical */
    bool abort_hunt_on_battery_low;       /**< Abort hunt if battery critical */
    bool reduce_intensity_on_stress;      /**< Reduce hunt intensity when stressed */

    /* Recovery actions */
    bool enable_auto_rest;                /**< Trigger automatic rest periods */
    float rest_trigger_fatigue;           /**< Fatigue level to trigger rest (0.7) */
    uint32_t min_rest_duration_ms;        /**< Minimum rest duration (5000) */

    uint32_t check_interval_ms;           /**< Health check interval (100) */
} health_agent_dragonfly_immune_config_t;

/**
 * @brief Configuration for Portia monitor integration (Phase 5.6)
 *
 * WHAT: Configuration for platform resource health monitoring
 * WHY:  Thermal, power, and resource states affect system health
 * HOW:  Monitor portia_monitor for state changes, coordinate responses
 */
typedef struct {
    bool enable_portia_monitor;           /**< Enable portia monitoring */
    bool enable_thermal_monitoring;       /**< Monitor thermal states */
    bool enable_power_monitoring;         /**< Monitor power/battery states */
    bool enable_cpu_load_monitoring;      /**< Monitor CPU utilization */
    bool enable_degradation_tracking;     /**< Track degradation levels */
    bool enable_cross_coordination;       /**< Coordinate with other modules */

    /* Thermal thresholds */
    float thermal_warning_temp_c;         /**< Warning temperature (70.0) */
    float thermal_critical_temp_c;        /**< Critical temperature (85.0) */
    bool throttle_on_warm;                /**< Begin throttling on warm state */
    bool emergency_on_critical;           /**< Emergency action on critical thermal */

    /* Power thresholds */
    float battery_warning_pct;            /**< Battery warning level (20.0) */
    float battery_critical_pct;           /**< Battery critical level (5.0) */
    bool conservation_on_battery;         /**< Enable conservation on battery */
    bool hibernate_on_critical;           /**< Hibernate on critical battery */

    /* CPU load thresholds */
    float cpu_warning_pct;                /**< CPU warning utilization (80.0) */
    float cpu_critical_pct;               /**< CPU critical utilization (95.0) */
    bool reduce_load_on_warning;          /**< Reduce load on warning */

    /* Coordination actions */
    bool notify_dragonfly_on_thermal;     /**< Notify dragonfly of thermal issues */
    bool notify_neural_on_power;          /**< Notify neural modules of power issues */
    bool trigger_checkpoint_on_power_loss;/**< Checkpoint before power loss */

    uint32_t check_interval_ms;           /**< Resource check interval (500) */
} health_agent_portia_monitor_config_t;

/**
 * @brief Configuration for Swarm immune integration
 */
typedef struct {
    bool enable_swarm_immune;             /**< Enable swarm immune integration */
    bool enable_threat_detection;         /**< USE swarm for threat detection */
    bool enable_coordinated_response;     /**< Coordinate responses across swarm */
    bool enable_memory_sharing;           /**< Share threat patterns with swarm */
    bool enable_self_verification;        /**< Verify self-identity in swarm */
    float threat_detection_threshold;     /**< Confidence threshold for threat */
    uint32_t consensus_timeout_ms;        /**< Timeout for swarm consensus */
} health_agent_swarm_immune_config_t;

/**
 * @brief Configuration for Swarm memory consolidation
 */
typedef struct {
    bool enable_swarm_memory;             /**< Enable swarm memory integration */
    bool enable_distributed_storage;      /**< Distribute health patterns to swarm */
    bool enable_memory_replay;            /**< USE replay for health pattern learning */
    bool enable_consolidation;            /**< Consolidate health patterns over time */
    bool enable_forgetting;               /**< Allow forgetting of old patterns */
    float replay_priority_threshold;      /**< Minimum priority for replay */
    uint32_t consolidation_interval_ms;   /**< How often to consolidate */
} health_agent_swarm_memory_config_t;

/**
 * @brief Configuration for Engram memory system
 */
typedef struct {
    bool enable_engram;                   /**< Enable engram integration */
    bool enable_health_encoding;          /**< Encode health events as engrams */
    bool enable_recall;                   /**< Recall similar past events */
    bool enable_reconsolidation;          /**< Allow engram updating */
    bool enable_pattern_completion;       /**< Complete partial health patterns */
    float encoding_threshold;             /**< Min severity for engram encoding */
    float recall_threshold;               /**< Min similarity for recall */
} health_agent_engram_config_t;

/**
 * @brief Configuration for systems memory consolidation
 */
typedef struct {
    bool enable_systems_consolidation;    /**< Enable systems consolidation */
    bool enable_sleep_replay;             /**< Replay during low-activity periods */
    bool enable_semantic_extraction;      /**< Extract semantic health patterns */
    bool enable_cortical_transfer;        /**< Transfer patterns to long-term */
    float consolidation_rate;             /**< Rate of consolidation (per hour) */
    uint32_t replay_batch_size;           /**< Batch size for replays */
} health_agent_memory_consolidation_config_t;

/**
 * @brief Complete cognitive integration configuration
 */
typedef struct {
    health_agent_prediction_config_t prediction;
    health_agent_metacog_config_t metacog;
    health_agent_ethics_config_t ethics;
    health_agent_emotion_config_t emotion;
    health_agent_wellbeing_config_t wellbeing;
    health_agent_collective_config_t collective;
    health_agent_rcog_config_t rcog;
    health_agent_gpu_config_t gpu;
    health_agent_hypothalamus_config_t hypothalamus;
    health_agent_connectivity_config_t connectivity;
    health_agent_oscillations_config_t oscillations;
    health_agent_gc_config_t gc;
    health_agent_checkpoint_config_t checkpoint;
    health_agent_bio_async_config_t bio_async;
    health_agent_exception_config_t exception;
    /* Portia/Dragonfly/Swarm/Memory integration */
    health_agent_portia_config_t portia;
    health_agent_dragonfly_config_t dragonfly;
    health_agent_swarm_immune_config_t swarm_immune;
    health_agent_swarm_memory_config_t swarm_memory;
    health_agent_engram_config_t engram;
    health_agent_memory_consolidation_config_t memory_consolidation;
} health_agent_cognitive_config_t;

/* ============================================================================
 * Cognitive Module Connection API
 * ============================================================================ */

/**
 * @brief Get default cognitive integration configuration
 *
 * @param config Output configuration
 */
void nimcp_health_agent_default_cognitive_config(health_agent_cognitive_config_t* config);

/**
 * @brief Connect failure prediction module
 *
 * WHAT: Integrate predictive failure detection
 * WHY:  Prevent failures before they occur
 * HOW:  Monitor leading indicators, predict failures, trigger preventive action
 *
 * @param agent Health agent
 * @param predictor Failure prediction module
 * @param config Prediction configuration (NULL for defaults)
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_connect_failure_prediction(
    nimcp_health_agent_t* agent,
    failure_predictor_t* predictor,
    const health_agent_prediction_config_t* config
);

/**
 * @brief Connect metacognition module
 *
 * WHAT: Integrate self-monitoring capabilities
 * WHY:  Detect degradation before catastrophic failure
 * HOW:  Track baselines, detect anomalies, calibrate confidence
 *
 * @param agent Health agent
 * @param metacog Metacognition module
 * @param config Metacognition configuration (NULL for defaults)
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_connect_metacognition(
    nimcp_health_agent_t* agent,
    metacognition_t* metacog,
    const health_agent_metacog_config_t* config
);

/**
 * @brief Connect ethics engine
 *
 * WHAT: Enable ethical evaluation of recovery actions
 * WHY:  Ensure autonomous actions are ethically justified
 * HOW:  Evaluate against Golden Rule, Asimov's Laws, proportionality
 *
 * @param agent Health agent
 * @param ethics Ethics engine
 * @param config Ethics configuration (NULL for defaults)
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_connect_ethics(
    nimcp_health_agent_t* agent,
    ethics_engine_t ethics,
    const health_agent_ethics_config_t* config
);

/**
 * @brief Connect emotional system
 *
 * WHAT: Enable emotion-aware health monitoring
 * WHY:  Adjust thresholds based on system emotional state
 * HOW:  Query emotion state, adjust sensitivity, report health events
 *
 * @param agent Health agent
 * @param emotion Emotional system
 * @param emotion_immune Emotion-immune bridge (optional, can be NULL)
 * @param config Emotion configuration (NULL for defaults)
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_connect_emotion(
    nimcp_health_agent_t* agent,
    emotional_system_t* emotion,
    emotion_immune_bridge_t* emotion_immune,
    const health_agent_emotion_config_t* config
);

/**
 * @brief Connect wellbeing monitor
 *
 * WHAT: Enable wellbeing and distress monitoring
 * WHY:  Ethical obligation to prevent system suffering
 * HOW:  Detect distress patterns, trigger interventions
 *
 * @param agent Health agent
 * @param wellbeing Wellbeing monitor
 * @param config Wellbeing configuration (NULL for defaults)
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_connect_wellbeing(
    nimcp_health_agent_t* agent,
    wellbeing_monitor_t* wellbeing,
    const health_agent_wellbeing_config_t* config
);

/**
 * @brief Connect mental health monitor
 *
 * WHAT: Enable mental health monitoring
 * WHY:  Detect pathological inference patterns
 * HOW:  Monitor FEP precision, detect aberrant learning
 *
 * @param agent Health agent
 * @param mental_health Mental health monitor
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_connect_mental_health(
    nimcp_health_agent_t* agent,
    mental_health_monitor_t* mental_health
);

/**
 * @brief Connect collective cognition
 *
 * WHAT: Enable distributed health monitoring
 * WHY:  Consensus-based anomaly detection improves accuracy
 * HOW:  Propose anomalies to collective, require consensus for recovery
 *
 * @param agent Health agent
 * @param collective Collective cognition system
 * @param config Collective configuration (NULL for defaults)
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_connect_collective(
    nimcp_health_agent_t* agent,
    collective_cognition_t* collective,
    const health_agent_collective_config_t* config
);

/**
 * @brief Connect recursive cognition (RCOG)
 *
 * WHAT: Enable intelligent health reasoning
 * WHY:  RCOG can decompose complex health issues, plan recovery
 * HOW:  Submit diagnosis goals to RCOG, execute recovery plans
 *
 * @param agent Health agent
 * @param rcog RCOG engine
 * @param config RCOG configuration (NULL for defaults)
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_connect_rcog(
    nimcp_health_agent_t* agent,
    rcog_engine_t* rcog,
    const health_agent_rcog_config_t* config
);

/**
 * @brief Connect GPU health monitoring
 *
 * WHAT: Enable GPU-aware health monitoring
 * WHY:  Monitor GPU health, use GPU for acceleration
 * HOW:  Check GPU state, accelerate tensor validation/anomaly detection
 *
 * @param agent Health agent
 * @param gpu_health GPU health monitor
 * @param config GPU configuration (NULL for defaults)
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_connect_gpu(
    nimcp_health_agent_t* agent,
    gpu_health_monitor_t* gpu_health,
    const health_agent_gpu_config_t* config
);

/* ============================================================================
 * Hypothalamus & Homeostasis Connection API (USE functionality)
 * ============================================================================ */

/**
 * @brief Connect hypothalamus orchestrator
 *
 * WHAT: Enable coordinated drive-based health response
 * WHY:  Health events can trigger appropriate drive responses
 * HOW:  Register as bridge, subscribe to drive events, publish health events
 *
 * USE CASES:
 * - Trigger stress response on health degradation
 * - Coordinate sickness behavior for safety mode
 * - Receive urgent drive signals for priority handling
 *
 * @param agent Health agent
 * @param orchestrator Hypothalamus orchestrator
 * @param config Hypothalamus configuration (NULL for defaults)
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_connect_hypothalamus(
    nimcp_health_agent_t* agent,
    hypo_orchestrator_t orchestrator,
    const health_agent_hypothalamus_config_t* config
);

/**
 * @brief Connect hypothalamus homeostasis system
 *
 * WHAT: USE homeostatic regulation for self-healing
 * WHY:  PID-based control enables smooth recovery from deviations
 * HOW:  Set health as homeostatic variable, respond to control signals
 *
 * USE CASES:
 * - Regulate resource usage via homeostatic control
 * - Auto-adjust thresholds based on homeostatic state
 * - Use alignment reward signal for health decisions
 *
 * @param agent Health agent
 * @param homeostasis Homeostasis system handle
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_connect_homeostasis(
    nimcp_health_agent_t* agent,
    hypo_homeostasis_handle_t* homeostasis
);

/**
 * @brief Connect hypothalamus immune bridge
 *
 * WHAT: Enable bidirectional neuroimmune integration
 * WHY:  Health agent can trigger/receive sickness behavior signals
 * HOW:  Connect to bridge for cytokine/cortisol coordination
 *
 * USE CASES:
 * - Trigger sickness behavior (safety mode) on severe health issues
 * - Receive fever signals to increase monitoring sensitivity
 * - Coordinate HPA stress response with immune activation
 *
 * @param agent Health agent
 * @param immune_bridge Hypothalamus-immune bridge
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_connect_hypo_immune_bridge(
    nimcp_health_agent_t* agent,
    hypo_immune_bridge_t* immune_bridge
);

/**
 * @brief Connect hypothalamus drive system
 *
 * WHAT: Enable drive-aware health monitoring
 * WHY:  Health thresholds should consider drive urgency
 * HOW:  Query drive states, adjust thresholds, report health to drives
 *
 * @param agent Health agent
 * @param drives Drive system handle
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_connect_drives(
    nimcp_health_agent_t* agent,
    hypo_drive_system_handle_t* drives
);

/* ============================================================================
 * Additional Module Connection API
 * ============================================================================ */

/**
 * @brief Connect connectivity health monitor
 *
 * WHAT: Enable module connectivity monitoring
 * WHY:  Detect isolated modules that can't communicate
 * HOW:  Check message paths, detect dead channels, alert on isolation
 *
 * @param agent Health agent
 * @param connectivity Connectivity health monitor
 * @param config Connectivity configuration (NULL for defaults)
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_connect_connectivity(
    nimcp_health_agent_t* agent,
    connectivity_health_t* connectivity,
    const health_agent_connectivity_config_t* config
);

/**
 * @brief Connect brain oscillations monitor
 *
 * WHAT: Enable brain wave monitoring for health
 * WHY:  Abnormal oscillations indicate system problems
 * HOW:  Monitor alpha/beta/gamma/theta/delta, detect anomalies
 *
 * @param agent Health agent
 * @param oscillations Brain oscillations monitor
 * @param config Oscillations configuration (NULL for defaults)
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_connect_oscillations(
    nimcp_health_agent_t* agent,
    brain_oscillations_t* oscillations,
    const health_agent_oscillations_config_t* config
);

/**
 * @brief Connect knowledge graph GC
 *
 * WHAT: Enable memory management integration
 * WHY:  Health agent can trigger GC, monitor memory health
 * HOW:  Query memory stats, trigger GC on pressure, detect leaks
 *
 * @param agent Health agent
 * @param gc_context GC context
 * @param config GC configuration (NULL for defaults)
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_connect_gc(
    nimcp_health_agent_t* agent,
    kg_gc_context_t* gc_context,
    const health_agent_gc_config_t* config
);

/**
 * @brief Connect checkpoint manager
 *
 * WHAT: Enable state persistence integration
 * WHY:  Health agent can trigger/use checkpoints for recovery
 * HOW:  Auto-checkpoint on good health, rollback on critical failure
 *
 * @param agent Health agent
 * @param checkpoint Checkpoint manager
 * @param config Checkpoint configuration (NULL for defaults)
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_connect_checkpoint(
    nimcp_health_agent_t* agent,
    checkpoint_manager_t* checkpoint,
    const health_agent_checkpoint_config_t* config
);

/**
 * @brief Connect deadlock detector
 *
 * WHAT: Enable threading health monitoring
 * WHY:  Detect and respond to deadlock/contention issues
 * HOW:  Query lock statistics, detect cycles, trigger recovery
 *
 * @param agent Health agent
 * @param deadlock_detector Deadlock detector
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_connect_deadlock_detector(
    nimcp_health_agent_t* agent,
    deadlock_detector_t* deadlock_detector
);

/**
 * @brief Connect bio-async router
 *
 * WHAT: Enable event-driven health messaging
 * WHY:  Publish health events, receive health requests
 * HOW:  Register module, subscribe to topics, publish events
 *
 * @param agent Health agent
 * @param router Bio-async router
 * @param config Bio-async configuration (NULL for defaults)
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_connect_bio_async(
    nimcp_health_agent_t* agent,
    bio_router_t router,
    const health_agent_bio_async_config_t* config
);

/**
 * @brief Connect runtime adaptation context
 *
 * WHAT: Enable load management for health response
 * WHY:  Health agent can trigger load reduction on degradation
 * HOW:  Adjust batch sizes, disable features, reduce concurrency
 *
 * @param agent Health agent
 * @param ra_ctx Runtime adaptation context
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_connect_runtime_adaptation(
    nimcp_health_agent_t* agent,
    runtime_adaptation_context_t ra_ctx
);

/**
 * @brief Connect exception-immune bridge
 *
 * WHAT: Enable exception-to-immune integration
 * WHY:  Exceptions can be presented as antigens for immune response
 * HOW:  Auto-present exceptions, execute recovery callbacks
 *
 * @param agent Health agent
 * @param exception_bridge Exception-immune bridge
 * @param config Exception configuration (NULL for defaults)
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_connect_exception_bridge(
    nimcp_health_agent_t* agent,
    exception_immune_t* exception_bridge,
    const health_agent_exception_config_t* config
);

/* ============================================================================
 * Neural Module (SNN/LNN) Connection API
 * ============================================================================ */

/**
 * @brief Connect SNN immune bridge for neural health monitoring
 *
 * WHAT: Enable spiking neural network health monitoring
 * WHY:  Detect SNN instabilities (silent, explosion, NaN, hypersynchrony)
 * HOW:  Periodic checks via snn_immune_get_health(), auto-report to immune
 *
 * SNN HEALTH STATES:
 * - SNN_STATE_HEALTHY: Normal operation
 * - SNN_STATE_SILENT: No spikes (dead network)
 * - SNN_STATE_EXPLOSION: Runaway firing
 * - SNN_STATE_NAN_DETECTED: NaN in membrane potential or weights
 * - SNN_STATE_WEIGHT_EXPLOSION: Weights exceeding bounds
 * - SNN_STATE_UNSTABLE: Oscillating or divergent
 *
 * RECOVERY ACTIONS:
 * - RECOVERY_ACTION_CLEAR_NAN: Clear NaN values
 * - RECOVERY_ACTION_HOMEOSTATIC_SCALE: Apply homeostatic scaling
 * - RECOVERY_ACTION_GRADIENT_CLIP: Clip gradients
 *
 * @param agent Health agent to connect
 * @param snn_bridge SNN immune bridge (must be already initialized)
 * @param config Configuration (NULL for defaults)
 * @return 0 on success, negative error code on failure
 */
int nimcp_health_agent_connect_snn(
    nimcp_health_agent_t* agent,
    snn_immune_bridge_t* snn_bridge,
    const health_agent_snn_config_t* config
);

/**
 * @brief Connect LNN immune bridge for neural health monitoring
 *
 * WHAT: Enable liquid neural network health monitoring
 * WHY:  Detect LNN instabilities (ODE divergence, tau collapse, NaN)
 * HOW:  Periodic stability checks via lnn_immune_check_stability()
 *
 * LNN INSTABILITY TYPES:
 * - LNN_INSTABILITY_NAN_STATE: NaN in state vector
 * - LNN_INSTABILITY_INF_STATE: Inf in state vector
 * - LNN_INSTABILITY_STATE_EXPLOSION: ||x|| > threshold
 * - LNN_INSTABILITY_STATE_COLLAPSE: ||x|| < threshold
 * - LNN_INSTABILITY_TAU_EXPLOSION: τ > max_tau
 * - LNN_INSTABILITY_TAU_COLLAPSE: τ < min_tau
 * - LNN_INSTABILITY_GRADIENT_EXPLOSION: ||∇|| > threshold
 * - LNN_INSTABILITY_GRADIENT_VANISHING: ||∇|| < threshold
 * - LNN_INSTABILITY_ODE_DIVERGENCE: ODE solver diverged
 *
 * IMMUNE MODULATION:
 * - Fever model: Higher inflammation → higher tau (slower dynamics)
 * - Learning suppression: Reduced LR during inflammation
 *
 * @param agent Health agent to connect
 * @param lnn_bridge LNN immune bridge (must be already initialized)
 * @param config Configuration (NULL for defaults)
 * @return 0 on success, negative error code on failure
 */
int nimcp_health_agent_connect_lnn(
    nimcp_health_agent_t* agent,
    lnn_immune_bridge_t* lnn_bridge,
    const health_agent_lnn_config_t* config
);

/**
 * @brief Get aggregated neural health metrics
 *
 * WHAT: Retrieve combined health metrics from SNN and LNN
 * WHY:  Single view of neural computation health
 * HOW:  Queries both bridges, aggregates metrics
 *
 * @param agent Health agent
 * @param metrics Output for neural health metrics
 * @return 0 on success, negative error code on failure
 */
int nimcp_health_agent_get_neural_metrics(
    const nimcp_health_agent_t* agent,
    neural_health_metrics_t* metrics
);

/**
 * @brief Configure neural monitoring thresholds
 *
 * WHAT: Update SNN/LNN monitoring configuration
 * WHY:  Adjust thresholds based on workload or requirements
 * HOW:  Applies config to both SNN and LNN bridges
 *
 * @param agent Health agent
 * @param snn_config SNN config (NULL to skip)
 * @param lnn_config LNN config (NULL to skip)
 * @return 0 on success, negative error code on failure
 */
int nimcp_health_agent_configure_neural(
    nimcp_health_agent_t* agent,
    const health_agent_snn_config_t* snn_config,
    const health_agent_lnn_config_t* lnn_config
);

/**
 * @brief Check if any neural module is unhealthy
 *
 * WHAT: Quick check for neural health issues
 * WHY:  Fast path for health decisions
 * HOW:  Checks has_instability flags on both bridges
 *
 * @param agent Health agent
 * @return true if any neural module is unhealthy
 */
bool nimcp_health_agent_is_neural_unhealthy(const nimcp_health_agent_t* agent);

/**
 * @brief Get neural health score (0-100)
 *
 * WHAT: Combined health score for neural modules
 * WHY:  Single metric for neural health
 * HOW:  Weighted average of SNN and LNN health
 *
 * @param agent Health agent
 * @return Health score [0-100], 0 if no neural modules connected
 */
float nimcp_health_agent_get_neural_health_score(const nimcp_health_agent_t* agent);

/* ============================================================================
 * Behavioral Module (Dragonfly/Portia) Connection API (Phase 5.6)
 * ============================================================================ */

/**
 * @brief Connect Dragonfly immune bridge for behavioral health monitoring
 *
 * WHAT: Enable hunting behavior health monitoring via immune bridge
 * WHY:  Detect stress, fatigue, injury affecting hunting performance
 * HOW:  Periodic checks via dragonfly_immune_get_state(), report to immune
 *
 * DRAGONFLY HEALTH STATES:
 * - HEALTH_OPTIMAL: Full hunting capability
 * - HEALTH_MILD_IMPAIRMENT: Slightly reduced performance
 * - HEALTH_MODERATE_IMPAIRMENT: Significantly reduced
 * - HEALTH_SEVERE_IMPAIRMENT: Hunting not recommended
 * - HEALTH_CRITICAL: Must rest/recover
 *
 * STRESS LEVELS:
 * - STRESS_NONE: No stress
 * - STRESS_LOW: Mild stress (energizing)
 * - STRESS_MODERATE: Moderate stress
 * - STRESS_HIGH: High stress (impairing)
 * - STRESS_CHRONIC: Chronic stress (damaging)
 *
 * CROSS-MODULE COORDINATION:
 * - Thermal critical → abort hunt recommended
 * - Battery critical → conservation mode
 * - High fatigue → auto-rest triggered
 *
 * @param agent Health agent to connect
 * @param bridge Dragonfly immune bridge (must be already initialized)
 * @param config Configuration (NULL for defaults)
 * @return 0 on success, negative error code on failure
 */
int nimcp_health_agent_connect_dragonfly_immune(
    nimcp_health_agent_t* agent,
    dragonfly_immune_bridge_t bridge,
    const health_agent_dragonfly_immune_config_t* config
);

/**
 * @brief Connect Portia monitor for resource health monitoring
 *
 * WHAT: Enable platform resource monitoring for health decisions
 * WHY:  Thermal, power, CPU states affect system health and behavior
 * HOW:  Monitor portia_monitor for state changes, coordinate responses
 *
 * PORTIA THERMAL STATES:
 * - PORTIA_THERMAL_NOMINAL: Normal operating temperature
 * - PORTIA_THERMAL_WARM: Elevated but safe
 * - PORTIA_THERMAL_HOT: Approaching throttle threshold
 * - PORTIA_THERMAL_THROTTLED: System throttling active
 * - PORTIA_THERMAL_CRITICAL: Critical temperature
 *
 * PORTIA POWER STATES:
 * - PORTIA_POWER_AC: Plugged in, unlimited power
 * - PORTIA_POWER_BATTERY_FULL: Battery > 80%
 * - PORTIA_POWER_BATTERY_MID: Battery 20-80%
 * - PORTIA_POWER_BATTERY_LOW: Battery 5-20%
 * - PORTIA_POWER_BATTERY_CRITICAL: Battery < 5%
 *
 * CROSS-MODULE COORDINATION:
 * - Thermal critical → notify dragonfly to abort hunt
 * - Power critical → notify neural modules, trigger checkpoint
 * - Degradation escalation → coordinate graceful reduction
 *
 * @param agent Health agent to connect
 * @param monitor Portia monitor (must be already initialized)
 * @param config Configuration (NULL for defaults)
 * @return 0 on success, negative error code on failure
 */
int nimcp_health_agent_connect_portia_monitor(
    nimcp_health_agent_t* agent,
    portia_monitor_t monitor,
    const health_agent_portia_monitor_config_t* config
);

/**
 * @brief Get aggregated behavioral health metrics
 *
 * WHAT: Retrieve combined health metrics from Dragonfly and Portia
 * WHY:  Single view of behavioral and resource health
 * HOW:  Queries both bridges, aggregates metrics
 *
 * @param agent Health agent
 * @param metrics Output for behavioral health metrics
 * @return 0 on success, negative error code on failure
 */
int nimcp_health_agent_get_behavioral_metrics(
    const nimcp_health_agent_t* agent,
    behavioral_health_metrics_t* metrics
);

/**
 * @brief Configure behavioral monitoring thresholds
 *
 * WHAT: Update Dragonfly/Portia monitoring configuration
 * WHY:  Adjust thresholds based on workload or requirements
 * HOW:  Applies config to both Dragonfly and Portia monitors
 *
 * @param agent Health agent
 * @param dragonfly_config Dragonfly config (NULL to skip)
 * @param portia_config Portia config (NULL to skip)
 * @return 0 on success, negative error code on failure
 */
int nimcp_health_agent_configure_behavioral(
    nimcp_health_agent_t* agent,
    const health_agent_dragonfly_immune_config_t* dragonfly_config,
    const health_agent_portia_monitor_config_t* portia_config
);

/**
 * @brief Check if any behavioral module is unhealthy
 *
 * WHAT: Quick check for behavioral health issues
 * WHY:  Fast path for health decisions
 * HOW:  Checks health status and thermal/power states
 *
 * @param agent Health agent
 * @return true if any behavioral module is unhealthy
 */
bool nimcp_health_agent_is_behavioral_unhealthy(const nimcp_health_agent_t* agent);

/**
 * @brief Get behavioral health score (0-100)
 *
 * WHAT: Combined health score for behavioral modules
 * WHY:  Single metric for behavioral health
 * HOW:  Weighted average of Dragonfly and Portia health
 *
 * @param agent Health agent
 * @return Health score [0-100], 100 if no behavioral modules connected
 */
float nimcp_health_agent_get_behavioral_health_score(const nimcp_health_agent_t* agent);

/**
 * @brief Request behavioral coordination action
 *
 * WHAT: Request cross-module coordination from health agent
 * WHY:  Allow external modules to trigger coordination
 * HOW:  Send coordination request through health agent
 *
 * @param agent Health agent
 * @param action Action type ("abort_hunt", "conservation_mode", "rest_period")
 * @param reason Reason for the request
 * @return 0 on success, negative error code on failure
 */
int nimcp_health_agent_request_behavioral_coordination(
    nimcp_health_agent_t* agent,
    const char* action,
    const char* reason
);

/* ============================================================================
 * Portia/Dragonfly/Swarm/Memory Connection API
 * ============================================================================ */

/**
 * @brief Connect Portia adaptive resource manager
 *
 * WHAT: Enable adaptive resource management for health response
 * WHY:  Health agent can USE Portia for tier switching, degradation
 * HOW:  Monitor resources, adjust tier, coordinate degradation
 *
 * USE CASES:
 * - Trigger graceful degradation on low health
 * - Switch to lower tier to reduce load
 * - Monitor thermal/power for health correlation
 *
 * @param agent Health agent
 * @param portia Portia context
 * @param config Portia configuration (NULL for defaults)
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_connect_portia(
    nimcp_health_agent_t* agent,
    portia_context_t* portia,
    const health_agent_portia_config_t* config
);

/**
 * @brief Connect Dragonfly tracking/interception system
 *
 * WHAT: Enable target-tracking for health anomaly pursuit
 * WHY:  Treat critical issues as "targets" to track and intercept
 * HOW:  Convert anomalies to detections, pursue until resolved
 *
 * USE CASES:
 * - Lock onto critical anomaly and track its progression
 * - Predict anomaly trajectory (will it get worse?)
 * - Intercept (resolve) before critical failure
 *
 * @param agent Health agent
 * @param dragonfly Dragonfly system
 * @param config Dragonfly configuration (NULL for defaults)
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_connect_dragonfly(
    nimcp_health_agent_t* agent,
    dragonfly_system_t* dragonfly,
    const health_agent_dragonfly_config_t* config
);

/**
 * @brief Connect Swarm immune system
 *
 * WHAT: Enable distributed threat detection across swarm
 * WHY:  Swarm consensus improves threat detection accuracy
 * HOW:  Share threat patterns, get consensus on threats, coordinate response
 *
 * USE CASES:
 * - Detect distributed attacks (byzantine, sybil)
 * - Share learned threat patterns with swarm
 * - Coordinate quarantine across instances
 *
 * @param agent Health agent
 * @param swarm_immune Swarm immune system
 * @param config Swarm immune configuration (NULL for defaults)
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_connect_swarm_immune(
    nimcp_health_agent_t* agent,
    void* swarm_immune,  /* NimcpSwarmImmuneSystem* */
    const health_agent_swarm_immune_config_t* config
);

/**
 * @brief Connect Swarm memory consolidation system
 *
 * WHAT: Enable distributed health pattern storage and learning
 * WHY:  Share health patterns across swarm for collective learning
 * HOW:  Store patterns, consolidate, replay for learning
 *
 * USE CASES:
 * - Distribute learned health patterns to swarm
 * - Replay past incidents for pattern learning
 * - Consolidate patterns during low activity
 *
 * @param agent Health agent
 * @param swarm_memory Swarm memory system
 * @param config Swarm memory configuration (NULL for defaults)
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_connect_swarm_memory(
    nimcp_health_agent_t* agent,
    void* swarm_memory,  /* NimcpSwarmMemory* */
    const health_agent_swarm_memory_config_t* config
);

/**
 * @brief Connect Engram memory system
 *
 * WHAT: Enable health event encoding as memory traces
 * WHY:  Learn from past health events through engram recall
 * HOW:  Encode events as engrams, recall similar past events
 *
 * USE CASES:
 * - Encode significant health events as engrams
 * - Recall similar past events for diagnosis
 * - Pattern completion for partial anomaly recognition
 *
 * @param agent Health agent
 * @param engram Engram system
 * @param config Engram configuration (NULL for defaults)
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_connect_engram(
    nimcp_health_agent_t* agent,
    engram_system_t* engram,
    const health_agent_engram_config_t* config
);

/**
 * @brief Connect systems memory consolidation
 *
 * WHAT: Enable long-term health pattern learning
 * WHY:  Transfer health patterns from short-term to long-term memory
 * HOW:  Replay during sleep/low-activity, extract semantic patterns
 *
 * USE CASES:
 * - Sleep-dependent consolidation of health patterns
 * - Extract semantic features from episodic health events
 * - Build cortical network of health knowledge
 *
 * @param agent Health agent
 * @param consolidation Systems consolidation system
 * @param config Consolidation configuration (NULL for defaults)
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_connect_memory_consolidation(
    nimcp_health_agent_t* agent,
    systems_consolidation_system_t* consolidation,
    const health_agent_memory_consolidation_config_t* config
);

/* ============================================================================
 * Cognitive Integration Status
 * ============================================================================ */

/**
 * @brief Status of cognitive module integrations
 */
typedef struct {
    /* Connection status */
    bool failure_prediction_connected;
    bool metacognition_connected;
    bool ethics_connected;
    bool emotion_connected;
    bool wellbeing_connected;
    bool mental_health_connected;
    bool collective_connected;
    bool rcog_connected;
    bool gpu_connected;

    /* Failure prediction stats */
    uint64_t predictions_made;
    uint64_t predictions_correct;
    uint64_t preventive_actions;
    float prediction_accuracy;

    /* Metacognition stats */
    uint64_t self_diagnoses;
    uint64_t degradation_alerts;
    float current_confidence;

    /* Ethics stats */
    uint64_t ethics_evaluations;
    uint64_t ethics_blocks;
    uint64_t mercy_applications;

    /* Emotion stats */
    float current_stress_level;
    uint64_t emotion_adjustments;

    /* Wellbeing stats */
    uint64_t distress_detections;
    uint64_t wellbeing_interventions;
    float current_distress_level;

    /* Collective stats */
    uint64_t consensus_requests;
    uint64_t consensus_achieved;
    float avg_consensus_time_ms;

    /* RCOG stats */
    uint64_t rcog_diagnoses;
    uint64_t rcog_recovery_plans;
    float avg_rcog_time_ms;

    /* GPU stats */
    uint64_t gpu_accelerated_checks;
    float gpu_utilization;
    bool gpu_healthy;
} health_agent_cognitive_status_t;

/**
 * @brief Get cognitive integration status
 *
 * @param agent Health agent
 * @param status Output status
 */
void nimcp_health_agent_get_cognitive_status(
    const nimcp_health_agent_t* agent,
    health_agent_cognitive_status_t* status
);

/* ============================================================================
 * Intelligent Health Operations
 * ============================================================================ */

/**
 * @brief Perform intelligent anomaly handling
 *
 * WHAT: Use all connected cognitive modules for intelligent handling
 * WHY:  Better diagnosis and recovery than simple rules
 * HOW:
 *   1. Failure prediction: Was this predicted?
 *   2. Metacognition: Self-diagnose severity
 *   3. Collective: Get consensus on anomaly
 *   4. RCOG: Plan optimal recovery
 *   5. Ethics: Evaluate recovery action
 *   6. Emotion: Check if action appropriate given state
 *   7. Execute if approved
 *
 * @param agent Health agent
 * @param msg Anomaly message
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_intelligent_handle(
    nimcp_health_agent_t* agent,
    const health_agent_message_t* msg
);

/**
 * @brief Request predictive analysis
 *
 * WHAT: Get failure predictions for current state
 * WHY:  Enable preventive action
 * HOW:  Query failure predictor for predictions
 *
 * @param agent Health agent
 * @param predictions Output array of predictions (caller allocated)
 * @param max_predictions Maximum predictions to return
 * @return Number of predictions returned, -1 on error
 */
int nimcp_health_agent_get_predictions(
    nimcp_health_agent_t* agent,
    health_agent_message_t* predictions,
    uint32_t max_predictions
);

/**
 * @brief Request RCOG diagnosis
 *
 * WHAT: Use RCOG for intelligent diagnosis
 * WHY:  Complex issues benefit from recursive reasoning
 * HOW:  Submit goal to RCOG, return diagnosis
 *
 * @param agent Health agent
 * @param anomaly Anomaly to diagnose
 * @param diagnosis Output diagnosis description
 * @param diagnosis_size Size of diagnosis buffer
 * @param suggested_action Output suggested recovery action
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_rcog_diagnose(
    nimcp_health_agent_t* agent,
    const health_agent_message_t* anomaly,
    char* diagnosis,
    size_t diagnosis_size,
    health_agent_recovery_t* suggested_action
);

/**
 * @brief Check ethics for recovery action
 *
 * WHAT: Evaluate if recovery action is ethically permitted
 * WHY:  Prevent harmful autonomous actions
 * HOW:  Apply Golden Rule, Asimov's Laws, proportionality
 *
 * @param agent Health agent
 * @param anomaly Anomaly triggering recovery
 * @param action Proposed recovery action
 * @param permitted Output: whether action is permitted
 * @param justification Output: ethical justification
 * @param justification_size Size of justification buffer
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_check_ethics(
    nimcp_health_agent_t* agent,
    const health_agent_message_t* anomaly,
    health_agent_recovery_t action,
    bool* permitted,
    char* justification,
    size_t justification_size
);

/**
 * @brief Get emotion-adjusted thresholds
 *
 * WHAT: Get health thresholds adjusted for emotional state
 * WHY:  Stressed system needs more sensitive monitoring
 * HOW:  Query emotion system, scale thresholds
 *
 * @param agent Health agent
 * @param memory_threshold Output: adjusted memory threshold
 * @param cpu_threshold Output: adjusted CPU threshold
 * @param anomaly_sensitivity Output: adjusted anomaly sensitivity
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_get_emotion_adjusted_thresholds(
    nimcp_health_agent_t* agent,
    float* memory_threshold,
    float* cpu_threshold,
    float* anomaly_sensitivity
);

/* ============================================================================
 * Hypothalamus USE Functions (Active Integration)
 * ============================================================================ */

/**
 * @brief Trigger hypothalamic stress response
 *
 * WHAT: Activate HPA axis stress response via hypothalamus
 * WHY:  Coordinate system-wide stress handling on health degradation
 * HOW:  Call hypothalamus orchestrator to trigger stress, notify all bridges
 *
 * @param agent Health agent
 * @param reason Reason for stress response
 * @param severity Stress severity level
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_trigger_stress_response(
    nimcp_health_agent_t* agent,
    const char* reason,
    health_agent_severity_t severity
);

/**
 * @brief Release hypothalamic stress response
 *
 * WHAT: Deactivate HPA stress response when health improves
 * WHY:  Return system to normal operation
 * HOW:  Call hypothalamus orchestrator to release stress
 *
 * @param agent Health agent
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_release_stress_response(nimcp_health_agent_t* agent);

/**
 * @brief Enter sickness behavior mode (safety mode)
 *
 * WHAT: USE hypothalamic sickness behavior for conservative operation
 * WHY:  When system is severely degraded, enter defensive posture
 * HOW:  Activate hypothalamus-immune bridge sickness behavior
 *
 * Effects:
 * - Reduced exploration (curiosity drive ↓)
 * - Reduced social activity (social drive ↓)
 * - Increased rest-seeking (fatigue drive ↑)
 * - More conservative decisions
 *
 * @param agent Health agent
 * @param threat_level Severity triggering safety mode (0.0-1.0)
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_enter_sickness_mode(
    nimcp_health_agent_t* agent,
    float threat_level
);

/**
 * @brief Exit sickness behavior mode
 *
 * WHAT: Return from safety mode to normal operation
 * WHY:  Health has recovered sufficiently
 * HOW:  Deactivate sickness behavior via hypothalamus-immune bridge
 *
 * @param agent Health agent
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_exit_sickness_mode(nimcp_health_agent_t* agent);

/**
 * @brief Update homeostatic health setpoint
 *
 * WHAT: USE homeostasis system to regulate health towards setpoint
 * WHY:  PID control provides smooth, stable health regulation
 * HOW:  Set current health as measured value, let controller compute output
 *
 * @param agent Health agent
 * @param current_health Current health score (0.0-1.0)
 * @return Control output indicating adjustment needed (-1.0 to +1.0)
 */
float nimcp_health_agent_homeostatic_regulate(
    nimcp_health_agent_t* agent,
    float current_health
);

/**
 * @brief Get homeostatic alignment reward
 *
 * WHAT: Query homeostasis system for alignment-aware reward
 * WHY:  Reward signal can guide health decisions
 * HOW:  Call hypo_homeostasis_compute_reward
 *
 * @param agent Health agent
 * @param reward_out Output: computed reward value
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_get_alignment_reward(
    nimcp_health_agent_t* agent,
    float* reward_out
);

/**
 * @brief Report drive event to hypothalamus
 *
 * WHAT: Report health-related drive activation
 * WHY:  Health events may create drive signals
 * HOW:  Publish to hypothalamus orchestrator
 *
 * @param agent Health agent
 * @param drive_type Type of drive (use HYPO_DRIVE_* constants)
 * @param drive_level Urgency level (0.0-1.0)
 * @param description Human-readable description
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_report_drive(
    nimcp_health_agent_t* agent,
    uint32_t drive_type,
    float drive_level,
    const char* description
);

/**
 * @brief Get unified drive state from hypothalamus
 *
 * WHAT: Query current drive state from all bridges
 * WHY:  Health decisions should consider drive urgency
 * HOW:  Call hypo_orch_get_drive_state
 *
 * @param agent Health agent
 * @param drive_level_out Output: unified drive level (0.0-1.0)
 * @param is_stressed_out Output: whether system is in stress
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_get_drive_state(
    nimcp_health_agent_t* agent,
    float* drive_level_out,
    bool* is_stressed_out
);

/* ============================================================================
 * Module USE Functions (Active Integration)
 * ============================================================================ */

/**
 * @brief Trigger garbage collection
 *
 * WHAT: USE GC context to trigger memory cleanup
 * WHY:  Health agent can proactively manage memory
 * HOW:  Call kg_gc_run if memory pressure detected
 *
 * @param agent Health agent
 * @param force Force GC even if not needed
 * @return 0 on success, -1 on error or not connected
 */
int nimcp_health_agent_trigger_gc(nimcp_health_agent_t* agent, bool force);

/**
 * @brief Create checkpoint
 *
 * WHAT: USE checkpoint manager to save state
 * WHY:  Preserve good state for potential rollback
 * HOW:  Call checkpoint_create
 *
 * @param agent Health agent
 * @param reason Reason for checkpoint
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_create_checkpoint(
    nimcp_health_agent_t* agent,
    const char* reason
);

/**
 * @brief Rollback to checkpoint
 *
 * WHAT: USE checkpoint manager to restore state
 * WHY:  Recover from critical failure
 * HOW:  Call checkpoint_restore
 *
 * @param agent Health agent
 * @param checkpoint_id Specific checkpoint ID (0 for latest)
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_rollback(
    nimcp_health_agent_t* agent,
    uint64_t checkpoint_id
);

/**
 * @brief Reduce system load
 *
 * WHAT: USE runtime adaptation to reduce load
 * WHY:  Relieve pressure on degraded system
 * HOW:  Adjust batch sizes, concurrency, disable features
 *
 * @param agent Health agent
 * @param reduction_factor How much to reduce (0.0-1.0, 1.0 = max reduction)
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_reduce_load(
    nimcp_health_agent_t* agent,
    float reduction_factor
);

/**
 * @brief Restore normal load
 *
 * WHAT: USE runtime adaptation to restore normal operation
 * WHY:  Health has recovered, resume normal load
 * HOW:  Restore batch sizes, concurrency, features
 *
 * @param agent Health agent
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_restore_load(nimcp_health_agent_t* agent);

/**
 * @brief Check oscillation health
 *
 * WHAT: USE brain oscillations for health assessment
 * WHY:  Abnormal oscillations indicate problems
 * HOW:  Query oscillation state, check for anomalies
 *
 * @param agent Health agent
 * @param is_abnormal_out Output: whether oscillations are abnormal
 * @param anomaly_type_out Output: type of anomaly if abnormal
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_check_oscillations(
    nimcp_health_agent_t* agent,
    bool* is_abnormal_out,
    uint32_t* anomaly_type_out
);

/**
 * @brief Check connectivity health
 *
 * WHAT: USE connectivity monitor for isolation detection
 * WHY:  Isolated modules can't function properly
 * HOW:  Query connectivity state, check for isolated modules
 *
 * @param agent Health agent
 * @param isolation_detected_out Output: whether isolation detected
 * @param isolated_module_out Output: name of isolated module (if any)
 * @param module_name_size Size of isolated_module_out buffer
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_check_connectivity(
    nimcp_health_agent_t* agent,
    bool* isolation_detected_out,
    char* isolated_module_out,
    size_t module_name_size
);

/**
 * @brief Publish health event to bio-async
 *
 * WHAT: USE bio-async router to publish health events
 * WHY:  Notify other modules of health state changes
 * HOW:  Create and publish bio-async message
 *
 * @param agent Health agent
 * @param msg Health message to publish
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_publish_event(
    nimcp_health_agent_t* agent,
    const health_agent_message_t* msg
);

/**
 * @brief Query deadlock detector
 *
 * WHAT: USE deadlock detector for threading health
 * WHY:  Detect deadlocks/contention before they cause problems
 * HOW:  Call deadlock_detector_check
 *
 * @param agent Health agent
 * @param deadlock_detected_out Output: whether deadlock detected
 * @param contention_high_out Output: whether contention is high
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_check_deadlocks(
    nimcp_health_agent_t* agent,
    bool* deadlock_detected_out,
    bool* contention_high_out
);

/* ============================================================================
 * Portia USE Functions (Active Integration)
 * ============================================================================ */

/**
 * @brief USE Portia to set platform tier
 *
 * WHAT: Actively set the platform tier via Portia
 * WHY:  Health agent can downgrade tier to reduce load
 * HOW:  Call portia_set_tier
 *
 * @param agent Health agent
 * @param tier Target platform tier
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_use_portia_set_tier(
    nimcp_health_agent_t* agent,
    uint32_t tier
);

/**
 * @brief USE Portia to trigger graceful degradation
 *
 * WHAT: Actively trigger degradation level via Portia
 * WHY:  Health agent can degrade features when system is stressed
 * HOW:  Call portia_set_degradation_level
 *
 * @param agent Health agent
 * @param level Degradation level (0=none, 1=minor, 2=moderate, 3=severe, 4=emergency)
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_use_portia_degrade(
    nimcp_health_agent_t* agent,
    uint32_t level
);

/**
 * @brief USE Portia to get recommended neuron count
 *
 * WHAT: Query Portia for resource-appropriate neuron count
 * WHY:  Health agent can recommend brain resizing based on resources
 * HOW:  Call portia_recommend_neuron_count
 *
 * @param agent Health agent
 * @param recommended_count Output: recommended neuron count
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_use_portia_get_recommended_neurons(
    nimcp_health_agent_t* agent,
    uint32_t* recommended_count
);

/**
 * @brief Get Portia status for health assessment
 *
 * WHAT: Query Portia for current resource status
 * WHY:  Include resource state in health assessment
 * HOW:  Call portia_get_status
 *
 * @param agent Health agent
 * @param power_state Output: power state (0=AC, 1-4=battery levels)
 * @param thermal_state Output: thermal state (0=nominal, 1-4=increasingly hot)
 * @param degradation_level Output: current degradation level
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_use_portia_get_status(
    nimcp_health_agent_t* agent,
    uint32_t* power_state,
    uint32_t* thermal_state,
    uint32_t* degradation_level
);

/* ============================================================================
 * Dragonfly USE Functions (Active Integration)
 * ============================================================================ */

/**
 * @brief USE Dragonfly to track a critical anomaly
 *
 * WHAT: Create a "detection" from an anomaly and have Dragonfly track it
 * WHY:  Use Dragonfly's prediction to anticipate anomaly progression
 * HOW:  Convert anomaly to detection, call dragonfly_process_detection
 *
 * @param agent Health agent
 * @param msg Anomaly message to track as target
 * @param target_id Output: assigned target ID
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_use_dragonfly_track_anomaly(
    nimcp_health_agent_t* agent,
    const health_agent_message_t* msg,
    uint32_t* target_id
);

/**
 * @brief USE Dragonfly to get anomaly prediction
 *
 * WHAT: Get prediction of where anomaly is heading
 * WHY:  Anticipate failure before it happens
 * HOW:  Call dragonfly_get_prediction
 *
 * @param agent Health agent
 * @param target_id Target ID to predict
 * @param time_to_failure_out Output: estimated time to critical failure (seconds)
 * @param confidence_out Output: prediction confidence (0.0-1.0)
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_use_dragonfly_predict(
    nimcp_health_agent_t* agent,
    uint32_t target_id,
    float* time_to_failure_out,
    float* confidence_out
);

/**
 * @brief USE Dragonfly to start pursuit of critical anomaly
 *
 * WHAT: Actively pursue resolution of a critical anomaly
 * WHY:  Focus resources on resolving the issue
 * HOW:  Call dragonfly_start_pursuit
 *
 * @param agent Health agent
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_use_dragonfly_pursue(nimcp_health_agent_t* agent);

/**
 * @brief USE Dragonfly to abort pursuit
 *
 * WHAT: Stop pursuing current anomaly
 * WHY:  Issue resolved or priority changed
 * HOW:  Call dragonfly_abort_pursuit
 *
 * @param agent Health agent
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_use_dragonfly_abort(nimcp_health_agent_t* agent);

/**
 * @brief Get current Dragonfly mode
 *
 * @param agent Health agent
 * @param mode_out Output: current mode (0=idle, 1=scanning, 2=tracking, 3=pursuing, 4=intercepting)
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_use_dragonfly_get_mode(
    nimcp_health_agent_t* agent,
    uint32_t* mode_out
);

/* ============================================================================
 * Swarm Immune USE Functions (Active Integration)
 * ============================================================================ */

/**
 * @brief USE Swarm immune to detect threats in data
 *
 * WHAT: Pass data through swarm immune detection
 * WHY:  Distributed threat detection is more accurate
 * HOW:  Call nimcp_swarm_immune_detect_threat
 *
 * @param agent Health agent
 * @param data Data to check
 * @param data_len Data length
 * @param source_id Source identifier
 * @param threat_detected_out Output: whether threat was detected
 * @param threat_id_out Output: threat ID if detected
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_use_swarm_detect_threat(
    nimcp_health_agent_t* agent,
    const uint8_t* data,
    size_t data_len,
    uint32_t source_id,
    bool* threat_detected_out,
    uint32_t* threat_id_out
);

/**
 * @brief USE Swarm immune to generate response to threat
 *
 * WHAT: Generate coordinated immune response to threat
 * WHY:  Swarm-coordinated response is more effective
 * HOW:  Call nimcp_swarm_immune_generate_response
 *
 * @param agent Health agent
 * @param threat_id Threat to respond to
 * @param response_id_out Output: generated response ID
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_use_swarm_generate_response(
    nimcp_health_agent_t* agent,
    uint32_t threat_id,
    uint32_t* response_id_out
);

/**
 * @brief USE Swarm immune to check behavior anomaly
 *
 * WHAT: Check if behavior profile is anomalous
 * WHY:  Detect byzantine/compromised components
 * HOW:  Call nimcp_swarm_immune_check_behavior
 *
 * @param agent Health agent
 * @param component_id Component to check
 * @param anomaly_score_out Output: anomaly score (0.0-1.0)
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_use_swarm_check_behavior(
    nimcp_health_agent_t* agent,
    uint32_t component_id,
    float* anomaly_score_out
);

/**
 * @brief USE Swarm immune to add memory cell (learned pattern)
 *
 * WHAT: Add a threat pattern to swarm memory
 * WHY:  Share learned patterns across swarm
 * HOW:  Call nimcp_swarm_immune_add_memory_cell
 *
 * @param agent Health agent
 * @param pattern Pattern bytes
 * @param pattern_len Pattern length
 * @param response_type Associated response type
 * @param cell_id_out Output: created memory cell ID
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_use_swarm_add_memory_cell(
    nimcp_health_agent_t* agent,
    const uint8_t* pattern,
    size_t pattern_len,
    uint32_t response_type,
    uint32_t* cell_id_out
);

/* ============================================================================
 * Swarm Memory USE Functions (Active Integration)
 * ============================================================================ */

/**
 * @brief USE Swarm memory to store health pattern
 *
 * WHAT: Store a health pattern in distributed swarm memory
 * WHY:  Share health knowledge across swarm
 * HOW:  Create memory entry and store
 *
 * @param agent Health agent
 * @param pattern_data Pattern data
 * @param pattern_size Pattern size
 * @param pattern_type Pattern type (0=episodic, 1=semantic, 2=procedural, 3=threat, 4=spatial)
 * @param importance Importance level (0-3)
 * @param pattern_id_out Output: assigned pattern ID
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_use_swarm_memory_store(
    nimcp_health_agent_t* agent,
    const void* pattern_data,
    size_t pattern_size,
    uint32_t pattern_type,
    uint32_t importance,
    char* pattern_id_out
);

/**
 * @brief USE Swarm memory to trigger replay
 *
 * WHAT: Trigger experience replay for learning
 * WHY:  Consolidate health patterns through replay
 * HOW:  Add entries to replay queue, process
 *
 * @param agent Health agent
 * @param count Number of patterns to replay
 * @return Number of patterns replayed, -1 on error
 */
int nimcp_health_agent_use_swarm_memory_replay(
    nimcp_health_agent_t* agent,
    uint32_t count
);

/**
 * @brief USE Swarm memory to trigger consolidation
 *
 * WHAT: Trigger memory consolidation
 * WHY:  Compress and abstract patterns over time
 * HOW:  Run consolidation cycle
 *
 * @param agent Health agent
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_use_swarm_memory_consolidate(nimcp_health_agent_t* agent);

/**
 * @brief Get Swarm memory statistics
 *
 * @param agent Health agent
 * @param total_memories_out Output: total stored memories
 * @param consolidated_out Output: consolidated memories
 * @param avg_strength_out Output: average memory strength
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_use_swarm_memory_get_stats(
    nimcp_health_agent_t* agent,
    uint64_t* total_memories_out,
    uint64_t* consolidated_out,
    float* avg_strength_out
);

/* ============================================================================
 * Engram Memory USE Functions (Active Integration)
 * ============================================================================ */

/**
 * @brief USE Engram to encode health event
 *
 * WHAT: Encode a health event as a memory engram
 * WHY:  Create explicit memory trace for later recall
 * HOW:  Call engram_encode
 *
 * @param agent Health agent
 * @param msg Health event to encode
 * @param engram_id_out Output: created engram ID
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_use_engram_encode(
    nimcp_health_agent_t* agent,
    const health_agent_message_t* msg,
    uint64_t* engram_id_out
);

/**
 * @brief USE Engram to recall similar past events
 *
 * WHAT: Recall engrams similar to current situation
 * WHY:  Learn from past similar health events
 * HOW:  Pattern completion/recall from engram system
 *
 * @param agent Health agent
 * @param msg Current health event
 * @param recalled_ids Output array of recalled engram IDs
 * @param max_recalls Maximum recalls to return
 * @param num_recalled_out Output: actual number recalled
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_use_engram_recall(
    nimcp_health_agent_t* agent,
    const health_agent_message_t* msg,
    uint64_t* recalled_ids,
    uint32_t max_recalls,
    uint32_t* num_recalled_out
);

/**
 * @brief Get Engram system statistics
 *
 * @param agent Health agent
 * @param active_engrams_out Output: number of active engrams
 * @param consolidated_out Output: number of consolidated engrams
 * @param avg_strength_out Output: average engram strength
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_use_engram_get_stats(
    nimcp_health_agent_t* agent,
    uint32_t* active_engrams_out,
    uint32_t* consolidated_out,
    float* avg_strength_out
);

/* ============================================================================
 * Systems Consolidation USE Functions (Active Integration)
 * ============================================================================ */

/**
 * @brief USE Systems consolidation to trigger sleep replay
 *
 * WHAT: Trigger replay cycle for memory transfer
 * WHY:  Transfer patterns from short-term to long-term
 * HOW:  Call consolidation replay cycle
 *
 * @param agent Health agent
 * @param replay_count Number of replays to perform
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_use_consolidation_replay(
    nimcp_health_agent_t* agent,
    uint32_t replay_count
);

/**
 * @brief USE Systems consolidation for semantic extraction
 *
 * WHAT: Extract semantic features from episodic health events
 * WHY:  Build abstract health knowledge
 * HOW:  Run semantic extraction on recent engrams
 *
 * @param agent Health agent
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_use_consolidation_extract_semantics(nimcp_health_agent_t* agent);

/**
 * @brief Get Systems consolidation statistics
 *
 * @param agent Health agent
 * @param cortical_nodes_out Output: number of cortical nodes
 * @param total_replays_out Output: total replays performed
 * @param total_transfers_out Output: memories transferred
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_use_consolidation_get_stats(
    nimcp_health_agent_t* agent,
    uint32_t* cortical_nodes_out,
    uint64_t* total_replays_out,
    uint64_t* total_transfers_out
);

/* ============================================================================
 * Extended Cognitive Status
 * ============================================================================ */

/**
 * @brief Extended status including all module integrations
 */
typedef struct {
    /* Base cognitive status */
    health_agent_cognitive_status_t cognitive;

    /* Hypothalamus status */
    bool hypothalamus_connected;
    bool homeostasis_connected;
    bool hypo_immune_bridge_connected;
    bool drives_connected;
    bool in_stress_response;
    bool in_sickness_mode;
    float current_drive_level;
    float homeostatic_output;

    /* Additional module status */
    bool connectivity_connected;
    bool oscillations_connected;
    bool gc_connected;
    bool checkpoint_connected;
    bool deadlock_detector_connected;
    bool bio_async_connected;
    bool runtime_adaptation_connected;
    bool exception_bridge_connected;

    /* Portia/Dragonfly/Swarm/Memory status */
    bool portia_connected;
    bool dragonfly_connected;
    bool swarm_immune_connected;
    bool swarm_memory_connected;
    bool engram_connected;
    bool memory_consolidation_connected;

    /* Portia status */
    uint32_t portia_current_tier;
    uint32_t portia_degradation_level;
    uint32_t portia_power_state;
    uint32_t portia_thermal_state;

    /* Dragonfly status */
    uint32_t dragonfly_mode;
    uint32_t dragonfly_tracked_targets;
    uint64_t dragonfly_pursuits;

    /* Swarm status */
    uint64_t swarm_threats_detected;
    uint64_t swarm_responses_generated;
    uint64_t swarm_memory_cells;

    /* Memory system status */
    uint64_t swarm_memories_stored;
    uint64_t swarm_replays_performed;
    uint32_t engram_active_count;
    uint32_t consolidation_cortical_nodes;
    uint64_t consolidation_transfers;

    /* Module statistics */
    uint64_t gc_triggers;
    uint64_t checkpoints_created;
    uint64_t rollbacks_performed;
    uint64_t load_reductions;
    uint64_t stress_responses;
    uint64_t sickness_mode_entries;
    uint64_t drive_events_published;
    uint64_t bio_async_events_published;

    /* New module statistics */
    uint64_t portia_tier_changes;
    uint64_t portia_degradations;
    uint64_t dragonfly_anomalies_tracked;
    uint64_t dragonfly_interceptions;
    uint64_t swarm_coordinated_responses;
    uint64_t engram_encodings;
    uint64_t engram_recalls;
} health_agent_full_status_t;

/**
 * @brief Get complete status of all integrations
 *
 * @param agent Health agent
 * @param status Output status
 */
void nimcp_health_agent_get_full_status(
    const nimcp_health_agent_t* agent,
    health_agent_full_status_t* status
);

/* ============================================================================
 * Phase 5.7: Memory System Health Integration
 * ============================================================================
 *
 * Integrates health monitoring with memory system tiers:
 * - Hippocampus: Episodic memory, spatial navigation, theta-gamma rhythms
 * - Mammillary Bodies: Memory consolidation relay, head direction cells
 * - Cross-tier consistency validation
 * - Metabolic coupling monitoring
 */

/* Forward declarations for memory system types */
typedef struct nimcp_hippocampus nimcp_hippocampus_t;
typedef struct nimcp_mammillary nimcp_mammillary_t;

/**
 * @brief Memory system health metrics
 */
typedef struct {
    /* Hippocampus health */
    struct {
        float overall_health;           /* 0.0-1.0 combined health score */
        float dg_activity;              /* Dentate gyrus pattern separation */
        float ca3_stability;            /* CA3 autoassociative stability */
        float ca1_output_quality;       /* CA1 output coherence */
        float theta_power;              /* Theta rhythm power (4-8 Hz) */
        float gamma_power;              /* Gamma rhythm power (30-100 Hz) */
        float theta_gamma_coupling;     /* Phase-amplitude coupling strength */
        uint32_t episodes_encoded;      /* Total episodes stored */
        uint32_t episodes_capacity;     /* Maximum episode capacity */
        float episode_utilization;      /* episodes_encoded / capacity */
        uint32_t place_cells_active;    /* Active place cells */
        uint32_t replay_events;         /* Sharp-wave ripple events */
        bool rhythm_disrupted;          /* Oscillation abnormality detected */
        bool pattern_separation_degraded; /* DG function impaired */
        bool pattern_completion_degraded; /* CA3 function impaired */
    } hippocampus;

    /* Mammillary bodies health */
    struct {
        float overall_health;           /* 0.0-1.0 combined health score */
        float relay_efficiency;         /* Memory relay throughput */
        float hd_cell_coherence;        /* Head direction cell agreement */
        float hd_drift_rate;            /* HD cell drift degrees/second */
        float fornix_strength;          /* Hippocampal-mammillary connection */
        float papez_circuit_integrity;  /* Full circuit health */
        uint32_t memory_traces_active;  /* Active consolidation traces */
        uint32_t memory_traces_capacity; /* Maximum trace capacity */
        float trace_utilization;        /* traces_active / capacity */
        uint32_t consolidation_events;  /* Successful consolidations */
        uint32_t hd_corrections;        /* HD drift corrections performed */
        bool circuit_broken;            /* Papez circuit disconnected */
        bool hd_drifting;               /* Excessive HD cell drift */
        bool consolidation_stalled;     /* Memory consolidation blocked */
    } mammillary;

    /* Cross-tier consistency */
    struct {
        float hippo_to_mammillary_sync; /* Signal correlation */
        float mammillary_to_thalamus_sync; /* Forward relay health */
        float thalamus_to_cortex_sync;  /* Output pathway health */
        float overall_circuit_integrity; /* Full memory circuit health */
        uint32_t sync_failures;         /* Cross-tier sync failures */
        uint32_t circuit_repairs;       /* Auto-repair events */
        bool tier_mismatch_detected;    /* Inconsistency found */
    } cross_tier;

    /* Metabolic coupling */
    struct {
        float hippocampus_atp_level;    /* Energy availability */
        float mammillary_atp_level;     /* Energy availability */
        float metabolic_stress;         /* Combined stress level */
        bool energy_constrained;        /* Low energy warning */
    } metabolic;

    /* Aggregate metrics */
    float overall_memory_health;        /* Combined memory system health */
    uint64_t total_anomalies_detected;  /* Lifetime anomaly count */
    uint64_t total_recoveries;          /* Successful recoveries */
    uint64_t last_check_timestamp;      /* Last health check time */
} memory_health_metrics_t;

/**
 * @brief Configuration for hippocampus health monitoring
 */
typedef struct {
    /* Health thresholds */
    float ca3_stability_threshold;      /* Min acceptable CA3 stability */
    float theta_gamma_min_coupling;     /* Min coupling strength */
    float episode_utilization_warning;  /* Warning threshold (e.g., 0.8) */
    float episode_utilization_critical; /* Critical threshold (e.g., 0.95) */

    /* Rhythm monitoring */
    float theta_power_min;              /* Minimum theta power */
    float gamma_power_min;              /* Minimum gamma power */
    bool monitor_oscillations;          /* Enable rhythm monitoring */

    /* Pattern monitoring */
    bool monitor_pattern_separation;    /* Monitor DG function */
    bool monitor_pattern_completion;    /* Monitor CA3 function */

    /* Check intervals */
    uint32_t health_check_interval_ms;  /* How often to check */
} health_agent_hippocampus_config_t;

/**
 * @brief Configuration for mammillary health monitoring
 */
typedef struct {
    /* Health thresholds */
    float relay_efficiency_threshold;   /* Min relay efficiency */
    float hd_drift_max_degrees;         /* Max acceptable HD drift */
    float fornix_strength_threshold;    /* Min fornix strength */
    float trace_utilization_warning;    /* Warning threshold */
    float trace_utilization_critical;   /* Critical threshold */

    /* Papez circuit monitoring */
    bool monitor_papez_circuit;         /* Enable circuit monitoring */
    float papez_integrity_threshold;    /* Min circuit integrity */

    /* HD cell monitoring */
    bool monitor_hd_cells;              /* Enable HD cell monitoring */
    float hd_coherence_threshold;       /* Min HD cell coherence */

    /* Check intervals */
    uint32_t health_check_interval_ms;  /* How often to check */
} health_agent_mammillary_config_t;

/**
 * @brief Memory-specific recovery actions
 */
typedef enum {
    MEMORY_RECOVERY_NONE = 0,
    MEMORY_RECOVERY_RESET_CA3,          /* Reset CA3 autoassociative state */
    MEMORY_RECOVERY_STABILIZE_RHYTHMS,  /* Restore theta-gamma coupling */
    MEMORY_RECOVERY_HD_DRIFT_CORRECT,   /* Correct head direction drift */
    MEMORY_RECOVERY_FORNIX_STRENGTHEN,  /* Strengthen hippocampal-mammillary */
    MEMORY_RECOVERY_FORCE_CONSOLIDATION, /* Force memory consolidation */
    MEMORY_RECOVERY_PAPEZ_REPAIR,       /* Repair Papez circuit */
    MEMORY_RECOVERY_EXPAND_CAPACITY,    /* Expand memory capacity */
    MEMORY_RECOVERY_GC_OLD_TRACES,      /* Garbage collect old traces */
    MEMORY_RECOVERY_CROSS_TIER_SYNC,    /* Synchronize memory tiers */
    MEMORY_RECOVERY_METABOLIC_BOOST,    /* Request more energy */
    MEMORY_RECOVERY_EMERGENCY_SAVE      /* Emergency memory save */
} memory_recovery_action_t;

/**
 * @brief Connect hippocampus to health agent
 *
 * WHAT: Enable hippocampus health monitoring
 * WHY:  Detect episodic memory issues, rhythm disruptions, capacity problems
 * HOW:  Poll hippocampus health APIs, monitor oscillations, track patterns
 *
 * USE CASES:
 * - Detect CA3 instability before memory corruption
 * - Monitor theta-gamma coupling for learning efficiency
 * - Alert on episode capacity exhaustion
 * - Detect pattern separation/completion degradation
 *
 * @param agent Health agent
 * @param hippocampus Hippocampus module
 * @param config Configuration (NULL for defaults)
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_connect_hippocampus(
    nimcp_health_agent_t* agent,
    nimcp_hippocampus_t* hippocampus,
    const health_agent_hippocampus_config_t* config
);

/**
 * @brief Connect mammillary bodies to health agent
 *
 * WHAT: Enable mammillary health monitoring
 * WHY:  Detect consolidation issues, HD drift, circuit problems
 * HOW:  Poll mammillary health APIs, monitor Papez circuit, track HD cells
 *
 * USE CASES:
 * - Detect memory consolidation stalls
 * - Monitor head direction cell drift
 * - Alert on Papez circuit disruption
 * - Track fornix connection strength
 *
 * @param agent Health agent
 * @param mammillary Mammillary bodies module
 * @param config Configuration (NULL for defaults)
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_connect_mammillary(
    nimcp_health_agent_t* agent,
    nimcp_mammillary_t* mammillary,
    const health_agent_mammillary_config_t* config
);

/**
 * @brief Get aggregated memory system health metrics
 *
 * @param agent Health agent
 * @param metrics Output metrics structure
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_get_memory_metrics(
    const nimcp_health_agent_t* agent,
    memory_health_metrics_t* metrics
);

/**
 * @brief Validate cross-tier memory consistency
 *
 * WHAT: Check consistency between hippocampus, mammillary, and thalamus
 * WHY:  Detect mismatches that could cause memory errors
 * HOW:  Compare activity patterns, timing, and content across tiers
 *
 * @param agent Health agent
 * @return 0 if consistent, positive count of inconsistencies, -1 on error
 */
int nimcp_health_agent_validate_memory_consistency(
    nimcp_health_agent_t* agent
);

/**
 * @brief Trigger memory-specific recovery action
 *
 * @param agent Health agent
 * @param action Recovery action to trigger
 * @param target_module Which memory module to target (0=hippocampus, 1=mammillary, 2=both)
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_memory_recovery(
    nimcp_health_agent_t* agent,
    memory_recovery_action_t action,
    int target_module
);

/**
 * @brief Check if memory system needs attention
 *
 * Quick check without full metrics collection.
 *
 * @param agent Health agent
 * @return true if memory health is below threshold, false otherwise
 */
bool nimcp_health_agent_memory_needs_attention(
    const nimcp_health_agent_t* agent
);

/**
 * @brief Get default hippocampus health configuration
 *
 * @param config Output configuration
 */
void nimcp_health_agent_hippocampus_config_default(
    health_agent_hippocampus_config_t* config
);

/**
 * @brief Get default mammillary health configuration
 *
 * @param config Output configuration
 */
void nimcp_health_agent_mammillary_config_default(
    health_agent_mammillary_config_t* config
);

/* ============================================================================
 * Phase 5.8: Dynamic Capacity Management Integration
 * ============================================================================ */

/**
 * @brief Capacity health metrics for health agent
 */
typedef struct {
    uint32_t num_managers;          /**< Number of registered managers */
    uint32_t managers_at_warning;   /**< Managers at warning level */
    uint32_t managers_at_critical;  /**< Managers at critical level */
    float overall_pressure;         /**< Weighted average utilization */
    float time_to_first_exhaustion; /**< Seconds until first manager full */
    const char* critical_module;    /**< Name of most critical module (if any) */
    bool any_at_capacity;           /**< True if any manager at capacity */
    uint32_t total_expansions;      /**< Total expansions across all managers */
    uint32_t total_failed_allocs;   /**< Total failed allocations */
} capacity_health_metrics_t;

/**
 * @brief Register capacity manager with health agent
 *
 * The health agent will monitor this capacity manager and include its
 * metrics in health checks.
 *
 * @param agent Health agent
 * @param cm Capacity manager to register
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_register_capacity_manager(
    nimcp_health_agent_t* agent,
    capacity_manager_t* cm
);

/**
 * @brief Unregister capacity manager from health agent
 *
 * @param agent Health agent
 * @param cm Capacity manager to unregister
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_unregister_capacity_manager(
    nimcp_health_agent_t* agent,
    capacity_manager_t* cm
);

/**
 * @brief Get capacity health metrics from health agent
 *
 * Aggregates metrics from all registered capacity managers.
 *
 * @param agent Health agent
 * @param metrics Pointer to receive metrics
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_get_capacity_metrics(
    nimcp_health_agent_t* agent,
    capacity_health_metrics_t* metrics
);

/**
 * @brief Check if any capacity manager needs attention
 *
 * @param agent Health agent
 * @return true if any manager at warning or critical level
 */
bool nimcp_health_agent_capacity_needs_attention(
    const nimcp_health_agent_t* agent
);

/* ============================================================================
 * Phase 5.9: Symbolic Logic Health Integration
 * ============================================================================ */

/** Symbolic logic forward declaration */
#ifndef NIMCP_SYMBOLIC_LOGIC_H
typedef struct symbolic_logic symbolic_logic_t;
#endif

/** Maximum number of symbolic logic engines to track */
#define HEALTH_AGENT_MAX_LOGIC_ENGINES 8

/**
 * @brief Symbolic logic health metrics
 *
 * WHAT: Combined health status from symbolic logic engines
 * WHY:  Monitor inference health, KB integrity, and reasoning performance
 * HOW:  Aggregates metrics from all registered logic engines
 */
typedef struct {
    /* Connection status */
    uint32_t num_engines;               /**< Number of registered logic engines */
    bool any_engine_unhealthy;          /**< Any engine reporting issues */

    /* Inference health */
    uint64_t total_inferences;          /**< Total inferences performed */
    uint64_t failed_inferences;         /**< Inferences that failed */
    uint64_t infinite_loop_detections;  /**< Inference loops detected */
    uint64_t unification_failures;      /**< Unification attempts that failed */
    float avg_inference_time_ms;        /**< Average inference duration */
    float max_inference_time_ms;        /**< Maximum inference time seen */
    bool inference_overload;            /**< Inference taking too long */

    /* Knowledge base health */
    uint32_t total_facts;               /**< Total facts across all KBs */
    uint32_t total_rules;               /**< Total rules across all KBs */
    uint32_t kb_capacity;               /**< Total KB capacity */
    float kb_utilization;               /**< Facts+rules / capacity */
    bool kb_near_capacity;              /**< KB approaching capacity limit */
    uint32_t inconsistencies_detected;  /**< Logical inconsistencies found */
    uint32_t kb_corruptions;            /**< KB corruption events */

    /* Reasoning performance */
    float reasoning_accuracy;           /**< Inference accuracy (if verifiable) */
    float unification_success_rate;     /**< Successful unifications / attempts */
    uint32_t resolution_steps;          /**< Total resolution steps performed */
    uint32_t resolution_timeouts;       /**< Resolutions that timed out */
    bool reasoning_degraded;            /**< Performance below threshold */

    /* Resource usage */
    uint64_t memory_used_bytes;         /**< Memory used by logic engines */
    float memory_utilization;           /**< Memory used / allocated */
    uint32_t stack_depth_max;           /**< Maximum inference stack depth */
    bool memory_pressure;               /**< Memory usage critical */

    /* Aggregate health */
    float overall_logic_health;         /**< Combined health score [0-100] */
    uint64_t total_anomalies;           /**< Lifetime anomaly count */
    uint64_t total_recoveries;          /**< Successful recoveries */
    uint64_t last_check_timestamp_us;   /**< Last health check time */
} logic_health_metrics_t;

/**
 * @brief Configuration for symbolic logic health monitoring
 */
typedef struct {
    /* Inference monitoring */
    bool enable_inference_monitoring;   /**< Monitor inference operations */
    float inference_timeout_ms;         /**< Max inference time before alert (100.0) */
    uint32_t max_inference_depth;       /**< Max recursion depth (1000) */
    float loop_detection_threshold;     /**< Cycles before loop alert (10000) */

    /* KB monitoring */
    bool enable_kb_monitoring;          /**< Monitor knowledge base health */
    float kb_utilization_warning;       /**< Warning threshold (0.8) */
    float kb_utilization_critical;      /**< Critical threshold (0.95) */
    bool detect_inconsistencies;        /**< Check for logical inconsistencies */

    /* Performance monitoring */
    bool enable_performance_monitoring; /**< Monitor reasoning performance */
    float unification_success_min;      /**< Min success rate (0.5) */
    float reasoning_accuracy_min;       /**< Min accuracy threshold (0.7) */

    /* Resource monitoring */
    bool enable_resource_monitoring;    /**< Monitor memory/stack usage */
    float memory_warning_threshold;     /**< Memory warning (0.8) */
    float memory_critical_threshold;    /**< Memory critical (0.95) */
    uint32_t stack_depth_warning;       /**< Stack depth warning (500) */

    /* Auto-recovery */
    bool enable_auto_recovery;          /**< Enable automatic recovery */
    bool enable_loop_interruption;      /**< Interrupt infinite loops */
    bool enable_gc_on_pressure;         /**< GC when memory pressure */

    /* Check intervals */
    uint32_t health_check_interval_ms;  /**< How often to check (100) */
} health_agent_symbolic_logic_config_t;

/**
 * @brief Logic-specific recovery actions
 */
typedef enum {
    LOGIC_RECOVERY_NONE = 0,
    LOGIC_RECOVERY_INTERRUPT_INFERENCE,  /**< Stop runaway inference */
    LOGIC_RECOVERY_RESET_UNIFIER,        /**< Reset unification state */
    LOGIC_RECOVERY_GC_KB,                /**< Garbage collect KB */
    LOGIC_RECOVERY_COMPACT_KB,           /**< Compact knowledge base */
    LOGIC_RECOVERY_CLEAR_CACHE,          /**< Clear inference cache */
    LOGIC_RECOVERY_RESOLVE_INCONSISTENCY,/**< Resolve logical inconsistency */
    LOGIC_RECOVERY_REDUCE_DEPTH,         /**< Reduce max inference depth */
    LOGIC_RECOVERY_CHECKPOINT_KB,        /**< Checkpoint knowledge base */
    LOGIC_RECOVERY_RESTORE_KB,           /**< Restore KB from checkpoint */
    LOGIC_RECOVERY_SOFT_RESET,           /**< Soft reset engine state */
    LOGIC_RECOVERY_FULL_RESET            /**< Full engine reset */
} logic_recovery_action_t;

/**
 * @brief Connect symbolic logic engine to health agent
 *
 * WHAT: Enable symbolic logic health monitoring
 * WHY:  Detect inference problems, KB issues, reasoning degradation
 * HOW:  Poll logic engine health APIs, monitor performance, track resources
 *
 * USE CASES:
 * - Detect infinite inference loops before resource exhaustion
 * - Monitor KB capacity and trigger GC when needed
 * - Detect logical inconsistencies
 * - Track reasoning performance degradation
 *
 * @param agent Health agent
 * @param logic Symbolic logic engine
 * @param config Configuration (NULL for defaults)
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_connect_symbolic_logic(
    nimcp_health_agent_t* agent,
    symbolic_logic_t* logic,
    const health_agent_symbolic_logic_config_t* config
);

/**
 * @brief Disconnect symbolic logic engine from health agent
 *
 * @param agent Health agent
 * @param logic Symbolic logic engine to disconnect
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_disconnect_symbolic_logic(
    nimcp_health_agent_t* agent,
    symbolic_logic_t* logic
);

/**
 * @brief Get aggregated symbolic logic health metrics
 *
 * @param agent Health agent
 * @param metrics Output metrics structure
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_get_logic_metrics(
    const nimcp_health_agent_t* agent,
    logic_health_metrics_t* metrics
);

/**
 * @brief Trigger logic-specific recovery action
 *
 * @param agent Health agent
 * @param action Recovery action to trigger
 * @param engine_index Index of engine to target (0 for first, -1 for all)
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_logic_recovery(
    nimcp_health_agent_t* agent,
    logic_recovery_action_t action,
    int engine_index
);

/**
 * @brief Check if symbolic logic needs attention
 *
 * Quick check without full metrics collection.
 *
 * @param agent Health agent
 * @return true if logic health is below threshold, false otherwise
 */
bool nimcp_health_agent_logic_needs_attention(
    const nimcp_health_agent_t* agent
);

/**
 * @brief Get overall logic health score (0-100)
 *
 * @param agent Health agent
 * @return Health score [0-100], 100 if no logic engines connected
 */
float nimcp_health_agent_get_logic_health_score(
    const nimcp_health_agent_t* agent
);

/**
 * @brief Get default symbolic logic health configuration
 *
 * @param config Output configuration
 */
void nimcp_health_agent_symbolic_logic_config_default(
    health_agent_symbolic_logic_config_t* config
);

/**
 * @brief Update symbolic logic health configuration at runtime
 *
 * @param agent Health agent
 * @param config New configuration
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_update_logic_config(
    nimcp_health_agent_t* agent,
    const health_agent_symbolic_logic_config_t* config
);

/* ============================================================================
 * Phase 5.10: Neural Substrate Health Integration
 * ============================================================================ */

/** Neural substrate forward declaration */
#ifndef NIMCP_NEURAL_SUBSTRATE_H
struct neural_substrate;
typedef struct neural_substrate neural_substrate_t;
#endif

/** Maximum number of neural substrates to track */
#define HEALTH_AGENT_MAX_NEURAL_SUBSTRATES 8

/**
 * @brief Neural substrate health metrics
 *
 * WHAT: Combined health status from neural substrates
 * WHY:  Monitor metabolic, physical, and computational substrate health
 * HOW:  Aggregates metrics from all registered neural substrates
 */
typedef struct {
    /* Connection status */
    uint32_t num_substrates;            /**< Number of registered substrates */
    bool any_substrate_unhealthy;       /**< Any substrate reporting issues */

    /* Metabolic health */
    float avg_atp_level;                /**< Average ATP level [0-1] */
    float min_atp_level;                /**< Minimum ATP level seen */
    float avg_oxygen_saturation;        /**< Average O2 saturation [0-1] */
    float min_oxygen_saturation;        /**< Minimum O2 saturation */
    float avg_glucose_level;            /**< Average glucose level [0-1] */
    float min_glucose_level;            /**< Minimum glucose level */
    float avg_metabolic_capacity;       /**< Average metabolic score [0-1] */
    bool metabolic_crisis;              /**< Any metabolic critical */

    /* Physical health */
    float avg_temperature;              /**< Average temperature (°C) */
    float max_temperature;              /**< Maximum temperature seen */
    float min_temperature;              /**< Minimum temperature seen */
    float avg_membrane_integrity;       /**< Average membrane health [0-1] */
    float min_membrane_integrity;       /**< Minimum membrane integrity */
    float avg_ion_balance;              /**< Average ion balance [0-1] */
    float min_ion_balance;              /**< Minimum ion balance */
    float avg_physical_capacity;        /**< Average physical score [0-1] */
    bool physical_crisis;               /**< Any physical critical */

    /* Modulation status */
    float avg_firing_rate_mod;          /**< Average firing modulation [0-1.5] */
    float avg_transmission_efficiency;  /**< Average transmission [0-1] */
    float avg_conduction_velocity;      /**< Average conduction [0.5-1.5] */
    float avg_plasticity_capacity;      /**< Average plasticity [0-1] */
    float avg_overall_capacity;         /**< Average overall capacity [0-1] */

    /* Alert tracking */
    uint32_t total_alerts;              /**< Total alerts generated */
    uint32_t low_atp_alerts;            /**< ATP depletion alerts */
    uint32_t hypoxia_alerts;            /**< Hypoxia alerts */
    uint32_t hypoglycemia_alerts;       /**< Hypoglycemia alerts */
    uint32_t hyperthermia_alerts;       /**< High temperature alerts */
    uint32_t hypothermia_alerts;        /**< Low temperature alerts */
    uint32_t ion_imbalance_alerts;      /**< Ion imbalance alerts */
    uint32_t membrane_damage_alerts;    /**< Membrane damage alerts */

    /* Resource usage */
    uint64_t total_spikes_processed;    /**< Total spikes recorded */
    uint64_t total_transmissions;       /**< Total transmissions recorded */
    float total_atp_consumed;           /**< Total ATP consumed */
    float peak_metabolic_rate;          /**< Peak consumption rate */

    /* Aggregate health */
    float overall_substrate_health;     /**< Combined health score [0-100] */
    uint64_t total_critical_events;     /**< Lifetime critical events */
    uint64_t total_recoveries;          /**< Successful recoveries */
    uint64_t last_check_timestamp_us;   /**< Last health check time */
} substrate_health_metrics_t;

/**
 * @brief Configuration for neural substrate health monitoring
 */
typedef struct {
    /* Metabolic monitoring */
    bool enable_metabolic_monitoring;   /**< Monitor ATP, O2, glucose */
    float atp_warning_threshold;        /**< ATP warning level (0.5) */
    float atp_critical_threshold;       /**< ATP critical level (0.3) */
    float oxygen_warning_threshold;     /**< O2 warning level (0.7) */
    float oxygen_critical_threshold;    /**< O2 critical level (0.5) */
    float glucose_warning_threshold;    /**< Glucose warning (0.6) */
    float glucose_critical_threshold;   /**< Glucose critical (0.4) */

    /* Physical monitoring */
    bool enable_physical_monitoring;    /**< Monitor temperature, membrane, ions */
    float hyperthermia_threshold;       /**< High temp threshold (40.0°C) */
    float hypothermia_threshold;        /**< Low temp threshold (32.0°C) */
    float membrane_warning_threshold;   /**< Membrane warning (0.7) */
    float membrane_critical_threshold;  /**< Membrane critical (0.5) */
    float ion_warning_threshold;        /**< Ion balance warning (0.6) */
    float ion_critical_threshold;       /**< Ion balance critical (0.5) */

    /* Performance monitoring */
    bool enable_performance_monitoring; /**< Monitor modulation factors */
    float capacity_warning_threshold;   /**< Overall capacity warning (0.5) */
    float capacity_critical_threshold;  /**< Overall capacity critical (0.3) */

    /* Auto-recovery */
    bool enable_auto_recovery;          /**< Enable automatic recovery */
    bool enable_energy_boost;           /**< Boost ATP on depletion */
    bool enable_temp_regulation;        /**< Regulate temperature */
    bool enable_ion_correction;         /**< Correct ion imbalance */
    bool enable_membrane_repair;        /**< Repair membrane damage */

    /* Check intervals */
    uint32_t health_check_interval_ms;  /**< How often to check (50) */
} health_agent_substrate_config_t;

/**
 * @brief Neural substrate recovery actions
 */
typedef enum {
    SUBSTRATE_RECOVERY_NONE = 0,
    SUBSTRATE_RECOVERY_BOOST_ATP,        /**< Inject ATP (emergency energy) */
    SUBSTRATE_RECOVERY_BOOST_OXYGEN,     /**< Increase O2 saturation */
    SUBSTRATE_RECOVERY_BOOST_GLUCOSE,    /**< Increase glucose level */
    SUBSTRATE_RECOVERY_COOL_DOWN,        /**< Reduce temperature */
    SUBSTRATE_RECOVERY_WARM_UP,          /**< Increase temperature */
    SUBSTRATE_RECOVERY_BALANCE_IONS,     /**< Reset ion balance */
    SUBSTRATE_RECOVERY_REPAIR_MEMBRANE,  /**< Repair membrane damage */
    SUBSTRATE_RECOVERY_REDUCE_ACTIVITY,  /**< Lower firing/transmission */
    SUBSTRATE_RECOVERY_RESET_STATS,      /**< Reset statistics */
    SUBSTRATE_RECOVERY_SOFT_RESET,       /**< Soft reset substrate state */
    SUBSTRATE_RECOVERY_FULL_RESET        /**< Full substrate reset */
} substrate_recovery_action_t;

/**
 * @brief Connect neural substrate to health agent
 *
 * WHAT: Enable neural substrate health monitoring
 * WHY:  Detect metabolic depletion, physical degradation, modulation issues
 * HOW:  Poll substrate state APIs, monitor alerts, track statistics
 *
 * USE CASES:
 * - Detect ATP depletion before computational failure
 * - Monitor temperature for hyper/hypothermia
 * - Track ion balance and membrane integrity
 * - Observe modulation factor degradation
 *
 * @param agent Health agent
 * @param substrate Neural substrate
 * @param config Configuration (NULL for defaults)
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_connect_substrate(
    nimcp_health_agent_t* agent,
    neural_substrate_t* substrate,
    const health_agent_substrate_config_t* config
);

/**
 * @brief Disconnect neural substrate from health agent
 *
 * @param agent Health agent
 * @param substrate Neural substrate to disconnect
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_disconnect_substrate(
    nimcp_health_agent_t* agent,
    neural_substrate_t* substrate
);

/**
 * @brief Get aggregated neural substrate health metrics
 *
 * @param agent Health agent
 * @param metrics Output metrics structure
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_get_substrate_metrics(
    const nimcp_health_agent_t* agent,
    substrate_health_metrics_t* metrics
);

/**
 * @brief Trigger substrate-specific recovery action
 *
 * @param agent Health agent
 * @param action Recovery action to trigger
 * @param substrate_index Index of substrate to target (0 for first, -1 for all)
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_substrate_recovery(
    nimcp_health_agent_t* agent,
    substrate_recovery_action_t action,
    int substrate_index
);

/**
 * @brief Check if neural substrate needs attention
 *
 * Quick check without full metrics collection.
 *
 * @param agent Health agent
 * @return true if substrate health is below threshold, false otherwise
 */
bool nimcp_health_agent_substrate_needs_attention(
    const nimcp_health_agent_t* agent
);

/**
 * @brief Get overall substrate health score (0-100)
 *
 * @param agent Health agent
 * @return Health score [0-100], 100 if no substrates connected
 */
float nimcp_health_agent_get_substrate_health_score(
    const nimcp_health_agent_t* agent
);

/**
 * @brief Get default neural substrate health configuration
 *
 * @param config Output configuration
 */
void nimcp_health_agent_substrate_config_default(
    health_agent_substrate_config_t* config
);

/**
 * @brief Update neural substrate health configuration at runtime
 *
 * @param agent Health agent
 * @param config New configuration
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_update_substrate_config(
    nimcp_health_agent_t* agent,
    const health_agent_substrate_config_t* config
);

/* ============================================================================
 * Phase 5.11: Thalamic/Middleware Health Integration
 * ============================================================================ */

/** Thalamic bridge forward declarations */
#ifndef NIMCP_OMNI_WM_THALAMIC_BRIDGE_H
struct omni_wm_thalamic_bridge;
typedef struct omni_wm_thalamic_bridge omni_wm_thalamic_bridge_t;
#endif

/** Training integration forward declaration */
#ifndef NIMCP_BRAIN_TRAINING_INTEGRATION_H
struct nimcp_brain_training_ctx;
typedef struct nimcp_brain_training_ctx nimcp_brain_training_ctx_t;
#endif

/** Maximum number of thalamic bridges to track */
#define HEALTH_AGENT_MAX_THALAMIC_BRIDGES 8

/** Maximum number of middleware training contexts to track */
#define HEALTH_AGENT_MAX_TRAINING_CONTEXTS 4

/**
 * @brief Thalamic bridge health metrics
 *
 * WHAT: Combined health status from thalamic bridges
 * WHY:  Monitor attention gating, sensory filtering, prediction biasing
 * HOW:  Aggregates metrics from all registered thalamic bridges
 */
typedef struct {
    /* Connection status */
    uint32_t num_bridges;               /**< Number of registered bridges */
    bool any_bridge_unhealthy;          /**< Any bridge reporting issues */
    bool any_connection_lost;           /**< Any bridge lost connection */

    /* Gating performance */
    uint64_t total_inputs_gated;        /**< Total inputs processed */
    uint64_t total_inputs_passed;       /**< Inputs that passed gating */
    uint64_t total_inputs_blocked;      /**< Inputs blocked by gating */
    float avg_gating_attention;         /**< Average attention during gating */
    float min_gating_attention;         /**< Minimum attention observed */
    float max_gating_attention;         /**< Maximum attention observed */
    float gating_efficiency;            /**< Pass/total ratio */

    /* Per-nucleus health (averaged across bridges) */
    float avg_lgn_attention;            /**< Avg LGN (visual) attention */
    float avg_mgn_attention;            /**< Avg MGN (auditory) attention */
    float avg_pulvinar_attention;       /**< Avg pulvinar attention */
    float avg_md_attention;             /**< Avg mediodorsal attention */
    float avg_trn_inhibition;           /**< Avg TRN inhibition */
    float avg_va_vl_attention;          /**< Avg VA/VL (motor) attention */

    /* Prediction biasing */
    uint64_t total_bias_updates;        /**< Attention bias updates */
    uint64_t total_salience_predictions;/**< Salience predictions made */
    float avg_bias_confidence;          /**< Average bias confidence */
    float min_bias_confidence;          /**< Minimum bias confidence */
    float avg_prediction_error;         /**< Average prediction error */
    float max_prediction_error;         /**< Maximum prediction error */

    /* TRN inhibition */
    uint64_t total_trn_inhibitions;     /**< TRN inhibition events */
    uint64_t total_trn_releases;        /**< TRN release events */
    float avg_trn_strength;             /**< Average inhibition strength */
    bool inhibition_imbalance;          /**< TRN inhibition imbalanced */

    /* Pulvinar coordination */
    uint64_t pulvinar_coordination_events; /**< Pulvinar coordination count */
    float avg_pulvinar_focus;           /**< Average focus strength */
    bool pulvinar_overload;             /**< Pulvinar overloaded */

    /* Firing mode statistics */
    float avg_tonic_fraction;           /**< Fraction in tonic mode */
    float avg_burst_fraction;           /**< Fraction in burst mode */
    uint64_t total_mode_switches;       /**< Tonic<->burst switches */
    float avg_arousal_level;            /**< Average arousal level */

    /* Timing statistics */
    uint64_t total_updates;             /**< Total update cycles */
    double total_processing_time_ms;    /**< Total processing time */
    double avg_update_time_ms;          /**< Average update duration */
    double max_update_time_ms;          /**< Maximum update duration */

    /* Error statistics */
    uint64_t total_errors;              /**< Total errors encountered */
    uint64_t gating_errors;             /**< Gating-related errors */
    uint64_t biasing_errors;            /**< Biasing-related errors */
    uint64_t trn_errors;                /**< TRN-related errors */

    /* Aggregate health */
    float overall_thalamic_health;      /**< Combined health score [0-100] */
    uint64_t total_critical_events;     /**< Lifetime critical events */
    uint64_t total_recoveries;          /**< Successful recoveries */
    uint64_t last_check_timestamp_us;   /**< Last health check time */
} thalamic_health_metrics_t;

/**
 * @brief Middleware training health metrics
 *
 * WHAT: Combined health status from training middleware
 * WHY:  Monitor training stability, convergence, gradient health
 * HOW:  Aggregates metrics from all registered training contexts
 */
typedef struct {
    /* Connection status */
    uint32_t num_contexts;              /**< Number of registered contexts */
    bool any_context_unhealthy;         /**< Any context reporting issues */
    bool any_training_active;           /**< Any training currently active */

    /* Training progress */
    uint64_t total_epochs_completed;    /**< Total epochs across contexts */
    uint64_t total_batches_processed;   /**< Total batches processed */
    uint64_t total_samples_trained;     /**< Total samples processed */
    uint64_t total_weight_updates;      /**< Total weight updates */
    bool any_training_paused;           /**< Any training paused */
    bool any_training_converged;        /**< Any training converged */
    bool any_training_diverged;         /**< Any training diverged */

    /* Loss statistics */
    float avg_loss;                     /**< Average loss across contexts */
    float min_loss;                     /**< Minimum loss observed */
    float max_loss;                     /**< Maximum loss observed */
    float loss_variance;                /**< Loss variance */
    float loss_trend;                   /**< Loss trend (positive = increasing) */
    bool loss_explosion_detected;       /**< Loss explosion detected */
    bool loss_plateau_detected;         /**< Loss plateau detected */

    /* Gradient health */
    uint64_t total_gradient_clips;      /**< Total gradient clips */
    uint64_t total_nan_gradients;       /**< NaN gradient detections */
    uint64_t total_inf_gradients;       /**< Inf gradient detections */
    float avg_gradient_norm;            /**< Average gradient norm */
    float max_gradient_norm;            /**< Maximum gradient norm */
    float avg_clip_ratio;               /**< Average clip ratio */
    bool gradient_health_critical;      /**< Gradient health critical */

    /* Learning rate */
    float avg_learning_rate;            /**< Average learning rate */
    float min_learning_rate;            /**< Minimum learning rate */
    float max_learning_rate;            /**< Maximum learning rate */
    uint64_t total_lr_changes;          /**< Total LR scheduler steps */
    bool lr_too_high;                   /**< LR potentially too high */
    bool lr_too_low;                    /**< LR potentially too low */

    /* Regularization */
    float avg_regularization_loss;      /**< Average regularization loss */
    uint64_t total_dropout_masks;       /**< Dropout masks generated */
    float dropout_rate;                 /**< Current dropout rate */

    /* Early stopping */
    uint32_t avg_early_stop_patience;   /**< Average patience remaining */
    uint32_t early_stops_triggered;     /**< Early stops triggered */
    float best_loss_seen;               /**< Best loss for early stopping */

    /* Resource usage */
    uint64_t total_training_time_ns;    /**< Total training time */
    double avg_batch_time_ms;           /**< Average batch time */
    double max_batch_time_ms;           /**< Maximum batch time */
    float resource_utilization;         /**< Resource utilization [0-1] */

    /* Plasticity bridge stats (if connected) */
    uint64_t total_rpe_computations;    /**< RPE computations */
    uint64_t total_biological_updates;  /**< Biological weight updates */
    float avg_dopamine_level;           /**< Average dopamine level */
    float avg_lr_modulation;            /**< Average LR modulation factor */

    /* Callback stats */
    uint64_t total_callback_events;     /**< Callback events fired */
    uint64_t callback_stop_requests;    /**< Callback stop requests */
    uint64_t callback_rollbacks;        /**< Callback rollback requests */

    /* Aggregate health */
    float overall_middleware_health;    /**< Combined health score [0-100] */
    uint64_t total_critical_events;     /**< Lifetime critical events */
    uint64_t total_recoveries;          /**< Successful recoveries */
    uint64_t last_check_timestamp_us;   /**< Last health check time */
} middleware_health_metrics_t;

/**
 * @brief Configuration for thalamic health monitoring
 */
typedef struct {
    /* Gating monitoring */
    bool enable_gating_monitoring;      /**< Monitor gating performance */
    float min_gating_efficiency;        /**< Min acceptable efficiency (0.1) */
    float attention_imbalance_threshold;/**< Nucleus attention variance (0.5) */
    float max_blocked_ratio;             /**< Max blocked/total ratio (0.9) */

    /* Prediction monitoring */
    bool enable_prediction_monitoring;  /**< Monitor prediction biasing */
    float min_bias_confidence;          /**< Min acceptable confidence (0.3) */
    float max_prediction_error;         /**< Max acceptable PE (0.8) */

    /* TRN monitoring */
    bool enable_trn_monitoring;         /**< Monitor TRN inhibition */
    float trn_imbalance_threshold;      /**< TRN imbalance threshold (0.6) */
    float max_inhibition_duration_ms;   /**< Max sustained inhibition (1000) */

    /* Timing monitoring */
    bool enable_timing_monitoring;      /**< Monitor update timing */
    double max_update_time_ms;          /**< Max acceptable update time (10.0) */

    /* Auto-recovery */
    bool enable_auto_recovery;          /**< Enable automatic recovery */
    bool enable_attention_rebalance;    /**< Rebalance attention on imbalance */
    bool enable_trn_release;            /**< Release TRN on stuck */
    bool enable_arousal_adjustment;     /**< Adjust arousal on issues */

    /* Check intervals */
    uint32_t health_check_interval_ms;  /**< How often to check (100) */
} health_agent_thalamic_config_t;

/**
 * @brief Configuration for middleware training health monitoring
 */
typedef struct {
    /* Loss monitoring */
    bool enable_loss_monitoring;        /**< Monitor loss values */
    float loss_explosion_threshold;     /**< Loss explosion threshold (10.0) */
    float loss_plateau_threshold;       /**< Loss plateau delta (0.001) */
    uint32_t plateau_patience;          /**< Batches before plateau (100) */

    /* Gradient monitoring */
    bool enable_gradient_monitoring;    /**< Monitor gradient health */
    float max_gradient_norm;            /**< Max acceptable gradient norm (10.0) */
    uint32_t max_nan_count;             /**< Max NaN before critical (5) */
    float high_clip_ratio_threshold;    /**< Clip ratio warning (0.5) */

    /* Learning rate monitoring */
    bool enable_lr_monitoring;          /**< Monitor learning rate */
    float lr_too_high_threshold;        /**< LR too high threshold (0.1) */
    float lr_too_low_threshold;         /**< LR too low threshold (1e-8) */

    /* Timing monitoring */
    bool enable_timing_monitoring;      /**< Monitor batch timing */
    double max_batch_time_ms;           /**< Max acceptable batch time (1000.0) */
    float timing_variance_threshold;    /**< Timing variance warning (0.5) */

    /* Auto-recovery */
    bool enable_auto_recovery;          /**< Enable automatic recovery */
    bool enable_lr_reduction;           /**< Reduce LR on issues */
    bool enable_gradient_reset;         /**< Reset gradients on NaN */
    bool enable_auto_pause;             /**< Pause training on critical */
    bool enable_auto_checkpoint;        /**< Auto checkpoint before recovery */

    /* Check intervals */
    uint32_t health_check_interval_ms;  /**< How often to check (100) */
} health_agent_middleware_config_t;

/**
 * @brief Thalamic recovery actions
 */
typedef enum {
    THALAMIC_RECOVERY_NONE = 0,
    THALAMIC_RECOVERY_RESET_ATTENTION,   /**< Reset attention to baseline */
    THALAMIC_RECOVERY_REBALANCE_NUCLEI,  /**< Rebalance nucleus attention */
    THALAMIC_RECOVERY_RELEASE_TRN,       /**< Release TRN inhibition */
    THALAMIC_RECOVERY_BOOST_AROUSAL,     /**< Increase arousal level */
    THALAMIC_RECOVERY_REDUCE_AROUSAL,    /**< Decrease arousal level */
    THALAMIC_RECOVERY_CLEAR_BIAS,        /**< Clear prediction bias */
    THALAMIC_RECOVERY_RESET_PULVINAR,    /**< Reset pulvinar state */
    THALAMIC_RECOVERY_FORCE_TONIC,       /**< Force tonic mode */
    THALAMIC_RECOVERY_RESET_STATS,       /**< Reset bridge statistics */
    THALAMIC_RECOVERY_SOFT_RESET,        /**< Soft reset bridge state */
    THALAMIC_RECOVERY_FULL_RESET         /**< Full bridge reset */
} thalamic_recovery_action_t;

/**
 * @brief Middleware training recovery actions
 */
typedef enum {
    MIDDLEWARE_RECOVERY_NONE = 0,
    MIDDLEWARE_RECOVERY_REDUCE_LR,       /**< Reduce learning rate */
    MIDDLEWARE_RECOVERY_INCREASE_LR,     /**< Increase learning rate */
    MIDDLEWARE_RECOVERY_RESET_GRADIENTS, /**< Zero out gradients */
    MIDDLEWARE_RECOVERY_CLEAR_NAM,       /**< Clear NaN/Inf values */
    MIDDLEWARE_RECOVERY_PAUSE_TRAINING,  /**< Pause training */
    MIDDLEWARE_RECOVERY_RESUME_TRAINING, /**< Resume training */
    MIDDLEWARE_RECOVERY_SAVE_CHECKPOINT, /**< Save checkpoint now */
    MIDDLEWARE_RECOVERY_LOAD_CHECKPOINT, /**< Rollback to checkpoint */
    MIDDLEWARE_RECOVERY_RESET_EARLY_STOP,/**< Reset early stopping */
    MIDDLEWARE_RECOVERY_RESET_STATS,     /**< Reset training statistics */
    MIDDLEWARE_RECOVERY_SOFT_RESET,      /**< Soft reset training state */
    MIDDLEWARE_RECOVERY_FULL_RESET       /**< Full training reset */
} middleware_recovery_action_t;

/* ============================================================================
 * Thalamic Health API Functions
 * ============================================================================ */

/**
 * @brief Connect thalamic bridge to health agent
 *
 * WHAT: Enable thalamic bridge health monitoring
 * WHY:  Detect attention gating issues, TRN imbalance, prediction errors
 * HOW:  Poll bridge state APIs, monitor statistics, track performance
 *
 * USE CASES:
 * - Detect attention gating failures
 * - Monitor TRN inhibition balance
 * - Track prediction biasing performance
 * - Observe arousal and firing mode changes
 *
 * @param agent Health agent
 * @param bridge Thalamic bridge
 * @param config Configuration (NULL for defaults)
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_connect_thalamic(
    nimcp_health_agent_t* agent,
    omni_wm_thalamic_bridge_t* bridge,
    const health_agent_thalamic_config_t* config
);

/**
 * @brief Disconnect thalamic bridge from health agent
 *
 * @param agent Health agent
 * @param bridge Thalamic bridge to disconnect
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_disconnect_thalamic(
    nimcp_health_agent_t* agent,
    omni_wm_thalamic_bridge_t* bridge
);

/**
 * @brief Get aggregated thalamic health metrics
 *
 * @param agent Health agent
 * @param metrics Output metrics structure
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_get_thalamic_metrics(
    const nimcp_health_agent_t* agent,
    thalamic_health_metrics_t* metrics
);

/**
 * @brief Trigger thalamic-specific recovery action
 *
 * @param agent Health agent
 * @param action Recovery action to trigger
 * @param bridge_index Index of bridge to target (0 for first, -1 for all)
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_thalamic_recovery(
    nimcp_health_agent_t* agent,
    thalamic_recovery_action_t action,
    int bridge_index
);

/**
 * @brief Check if thalamic bridges need attention
 *
 * Quick check without full metrics collection.
 *
 * @param agent Health agent
 * @return true if thalamic health is below threshold, false otherwise
 */
bool nimcp_health_agent_thalamic_needs_attention(
    const nimcp_health_agent_t* agent
);

/**
 * @brief Get overall thalamic health score (0-100)
 *
 * @param agent Health agent
 * @return Health score [0-100], 100 if no bridges connected
 */
float nimcp_health_agent_get_thalamic_health_score(
    const nimcp_health_agent_t* agent
);

/**
 * @brief Get default thalamic health configuration
 *
 * @param config Output configuration
 */
void nimcp_health_agent_thalamic_config_default(
    health_agent_thalamic_config_t* config
);

/**
 * @brief Update thalamic health configuration at runtime
 *
 * @param agent Health agent
 * @param config New configuration
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_update_thalamic_config(
    nimcp_health_agent_t* agent,
    const health_agent_thalamic_config_t* config
);

/* ============================================================================
 * Middleware Training Health API Functions
 * ============================================================================ */

/**
 * @brief Connect training context to health agent
 *
 * WHAT: Enable middleware training health monitoring
 * WHY:  Detect training instability, gradient issues, divergence
 * HOW:  Poll training state APIs, monitor statistics, track progress
 *
 * USE CASES:
 * - Detect loss explosion or divergence
 * - Monitor gradient health (NaN, Inf, clipping)
 * - Track learning rate changes
 * - Observe early stopping progress
 *
 * @param agent Health agent
 * @param training_ctx Training context
 * @param config Configuration (NULL for defaults)
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_connect_middleware(
    nimcp_health_agent_t* agent,
    nimcp_brain_training_ctx_t* training_ctx,
    const health_agent_middleware_config_t* config
);

/**
 * @brief Disconnect training context from health agent
 *
 * @param agent Health agent
 * @param training_ctx Training context to disconnect
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_disconnect_middleware(
    nimcp_health_agent_t* agent,
    nimcp_brain_training_ctx_t* training_ctx
);

/**
 * @brief Get aggregated middleware health metrics
 *
 * @param agent Health agent
 * @param metrics Output metrics structure
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_get_middleware_metrics(
    const nimcp_health_agent_t* agent,
    middleware_health_metrics_t* metrics
);

/**
 * @brief Trigger middleware-specific recovery action
 *
 * @param agent Health agent
 * @param action Recovery action to trigger
 * @param context_index Index of context to target (0 for first, -1 for all)
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_middleware_recovery(
    nimcp_health_agent_t* agent,
    middleware_recovery_action_t action,
    int context_index
);

/**
 * @brief Check if middleware needs attention
 *
 * Quick check without full metrics collection.
 *
 * @param agent Health agent
 * @return true if middleware health is below threshold, false otherwise
 */
bool nimcp_health_agent_middleware_needs_attention(
    const nimcp_health_agent_t* agent
);

/**
 * @brief Get overall middleware health score (0-100)
 *
 * @param agent Health agent
 * @return Health score [0-100], 100 if no contexts connected
 */
float nimcp_health_agent_get_middleware_health_score(
    const nimcp_health_agent_t* agent
);

/**
 * @brief Get default middleware health configuration
 *
 * @param config Output configuration
 */
void nimcp_health_agent_middleware_config_default(
    health_agent_middleware_config_t* config
);

/**
 * @brief Update middleware health configuration at runtime
 *
 * @param agent Health agent
 * @param config New configuration
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_update_middleware_config(
    nimcp_health_agent_t* agent,
    const health_agent_middleware_config_t* config
);

/* ============================================================================
 * Phase 5.12: Perception/Cortical Health Integration
 * ============================================================================ */

/*
 * Integrates health monitoring for:
 * - Visual cortical bridges (V1 processing, orientation columns)
 * - Audio cortical bridges (auditory cortex, frequency maps)
 * - Cortical immune system (microglial surveillance, inflammation)
 * - Cortical columns (hypercolumns, minicolumns)
 *
 * Monitors perception pipeline health including:
 * - Sensory processing latency
 * - Feature selectivity degradation
 * - Cortical inflammation levels
 * - Column competition dynamics
 * - Layer communication health
 */

/** Visual cortical bridge forward declaration */
#ifndef NIMCP_VISUAL_CORTICAL_BRIDGE_H
struct visual_cortical_bridge;
typedef struct visual_cortical_bridge visual_cortical_bridge_t;
#endif

/** Audio cortical bridge forward declaration */
#ifndef NIMCP_AUDIO_CORTICAL_BRIDGE_H
struct audio_cortical_bridge;
typedef struct audio_cortical_bridge audio_cortical_bridge_t;
#endif

/** Cortical immune system forward declaration */
#ifndef NIMCP_CORTICAL_IMMUNE_H
struct cortical_immune_system;
typedef struct cortical_immune_system cortical_immune_system_t;
#endif

/** Hypercolumn forward declaration */
#ifndef NIMCP_CORTICAL_COLUMN_H
struct hypercolumn;
typedef struct hypercolumn hypercolumn_t;
#endif

/** Maximum number of perception bridges to track */
#define HEALTH_AGENT_MAX_PERCEPTION_BRIDGES 8

/** Maximum number of cortical columns to track */
#define HEALTH_AGENT_MAX_CORTICAL_COLUMNS 16

/**
 * @brief Perception health metrics
 *
 * Aggregated health metrics for visual and audio cortical processing.
 */
typedef struct {
    /* Connection status */
    uint32_t num_visual_bridges;        /**< Number of visual cortical bridges */
    uint32_t num_audio_bridges;         /**< Number of audio cortical bridges */
    bool any_bridge_unhealthy;          /**< Any perception bridge unhealthy */
    bool any_connection_lost;           /**< Any bridge disconnected */

    /* Processing latency */
    double avg_visual_latency_ms;       /**< Average visual processing latency */
    double max_visual_latency_ms;       /**< Maximum visual processing latency */
    double avg_audio_latency_ms;        /**< Average audio processing latency */
    double max_audio_latency_ms;        /**< Maximum audio processing latency */
    uint64_t total_frames_processed;    /**< Total visual frames processed */
    uint64_t total_samples_processed;   /**< Total audio samples processed */

    /* Feature detection health */
    float avg_orientation_selectivity;  /**< Average orientation tuning sharpness */
    float avg_frequency_selectivity;    /**< Average frequency tuning sharpness */
    float feature_selectivity_score;    /**< Overall feature selectivity (0-100) */
    bool selectivity_degraded;          /**< Feature selectivity below threshold */

    /* Retinotopic/tonotopic mapping */
    float retinotopic_mapping_quality;  /**< Visual field mapping quality (0-1) */
    float tonotopic_mapping_quality;    /**< Frequency field mapping quality (0-1) */
    uint32_t mapping_errors_detected;   /**< Mapping discontinuities found */

    /* Column integration */
    float hypercolumn_competition_score;/**< Winner-take-all dynamics health */
    float cross_column_inhibition_score;/**< Lateral inhibition effectiveness */
    uint32_t column_timeout_events;     /**< Column computation timeouts */

    /* Error tracking */
    uint64_t total_processing_errors;   /**< Total perception processing errors */
    uint64_t total_overflow_events;     /**< Buffer overflow events */
    uint64_t total_underflow_events;    /**< Buffer underflow events */

    /* Timestamps and health */
    uint64_t last_check_timestamp_us;   /**< Last health check timestamp */
    uint64_t total_critical_events;     /**< Total critical events detected */
    uint64_t total_recoveries;          /**< Total recovery actions performed */
    float overall_perception_health;    /**< Overall health score (0-100) */
} perception_health_metrics_t;

/**
 * @brief Cortical health metrics
 *
 * Aggregated health metrics for cortical columns and immune system.
 */
typedef struct {
    /* Connection status */
    uint32_t num_immune_systems;        /**< Number of cortical immune systems */
    uint32_t num_hypercolumns;          /**< Number of tracked hypercolumns */
    bool any_column_unhealthy;          /**< Any cortical column unhealthy */
    bool immune_system_active;          /**< Cortical immune system responding */

    /* Layer health (cortical layers 2/3, 4, 5/6) */
    float layer_2_3_health;             /**< Layer 2/3 (cortico-cortical) health */
    float layer_4_health;               /**< Layer 4 (thalamic input) health */
    float layer_5_6_health;             /**< Layer 5/6 (subcortical output) health */
    float inter_layer_comm_score;       /**< Inter-layer communication health */
    bool layer_communication_failure;   /**< Layer communication breakdown */

    /* Column dynamics */
    float avg_column_activity;          /**< Average column activation level */
    float min_column_activity;          /**< Minimum column activity */
    float max_column_activity;          /**< Maximum column activity */
    float activity_variance;            /**< Column activity variance */
    bool hyperexcitability_detected;    /**< Seizure-like activity detected */
    bool hypoactivity_detected;         /**< Stroke-like silence detected */

    /* Competition and inhibition */
    float wta_effectiveness;            /**< Winner-take-all effectiveness */
    float lateral_inhibition_balance;   /**< E/I ratio balance score */
    uint64_t competition_failures;      /**< Competition resolution failures */

    /* Cortical immune metrics */
    float microglial_activation_level;  /**< Microglial activation (0-1) */
    float inflammation_index;           /**< Cortical inflammation level (0-1) */
    float cytokine_level;               /**< Cytokine concentration proxy */
    uint64_t immune_responses_triggered;/**< Total immune responses */
    uint64_t antigens_presented;        /**< Abnormal patterns detected */
    bool inflammation_critical;         /**< Inflammation above threshold */

    /* Feature selectivity */
    float orientation_tuning_width;     /**< Orientation tuning curve width */
    float frequency_tuning_width;       /**< Frequency tuning curve width */
    bool tuning_curves_broadened;       /**< Selectivity loss detected */

    /* Plasticity health */
    float plasticity_modulation;        /**< Current plasticity level */
    uint64_t synaptic_pruning_events;   /**< Microglial pruning events */
    uint64_t circuit_remodeling_events; /**< Circuit remodeling events */

    /* Timestamps and health */
    uint64_t last_check_timestamp_us;   /**< Last health check timestamp */
    uint64_t total_critical_events;     /**< Total critical events detected */
    uint64_t total_recoveries;          /**< Total recovery actions performed */
    float overall_cortical_health;      /**< Overall health score (0-100) */
} cortical_health_metrics_t;

/**
 * @brief Perception health monitoring configuration
 */
typedef struct {
    /* Latency monitoring */
    bool enable_latency_monitoring;     /**< Monitor processing latency */
    double max_visual_latency_ms;       /**< Max acceptable visual latency */
    double max_audio_latency_ms;        /**< Max acceptable audio latency */

    /* Feature selectivity monitoring */
    bool enable_selectivity_monitoring; /**< Monitor feature selectivity */
    float min_orientation_selectivity;  /**< Min acceptable orientation tuning */
    float min_frequency_selectivity;    /**< Min acceptable frequency tuning */
    float selectivity_degradation_threshold; /**< Degradation alert threshold */

    /* Buffer monitoring */
    bool enable_buffer_monitoring;      /**< Monitor buffer health */
    uint32_t max_overflow_count;        /**< Max acceptable overflows */
    uint32_t max_underflow_count;       /**< Max acceptable underflows */

    /* Mapping quality */
    bool enable_mapping_monitoring;     /**< Monitor topographic mapping */
    float min_mapping_quality;          /**< Min acceptable mapping quality */

    /* Auto-recovery */
    bool enable_auto_recovery;          /**< Enable automatic recovery */
    bool enable_buffer_flush;           /**< Allow buffer flush on overflow */
    bool enable_gain_adjustment;        /**< Allow gain adjustment */

    /* Check interval */
    uint32_t health_check_interval_ms;  /**< Health check interval in ms */
} health_agent_perception_config_t;

/**
 * @brief Cortical health monitoring configuration
 */
typedef struct {
    /* Layer monitoring */
    bool enable_layer_monitoring;       /**< Monitor cortical layer health */
    float min_layer_health;             /**< Min acceptable layer health */
    float layer_comm_threshold;         /**< Layer communication alert threshold */

    /* Activity monitoring */
    bool enable_activity_monitoring;    /**< Monitor column activity */
    float hyperexcitability_threshold;  /**< Activity level for hyperexcitability */
    float hypoactivity_threshold;       /**< Activity level for hypoactivity */
    float max_activity_variance;        /**< Max acceptable activity variance */

    /* Competition monitoring */
    bool enable_competition_monitoring; /**< Monitor column competition */
    float min_wta_effectiveness;        /**< Min WTA effectiveness */
    float min_inhibition_balance;       /**< Min E/I balance score */

    /* Immune monitoring */
    bool enable_immune_monitoring;      /**< Monitor cortical immune system */
    float inflammation_threshold;       /**< Inflammation alert threshold */
    float max_microglial_activation;    /**< Max acceptable microglial activation */
    float cytokine_alert_level;         /**< Cytokine level for alert */

    /* Selectivity monitoring */
    bool enable_tuning_monitoring;      /**< Monitor tuning curve health */
    float max_tuning_width;             /**< Max acceptable tuning width */

    /* Auto-recovery */
    bool enable_auto_recovery;          /**< Enable automatic recovery */
    bool enable_inflammation_control;   /**< Allow inflammation reduction */
    bool enable_activity_normalization; /**< Allow activity normalization */
    bool enable_competition_reset;      /**< Allow competition reset */

    /* Check interval */
    uint32_t health_check_interval_ms;  /**< Health check interval in ms */
} health_agent_cortical_config_t;

/**
 * @brief Perception-specific recovery actions
 */
typedef enum {
    PERCEPTION_RECOVERY_NONE = 0,       /**< No action */
    PERCEPTION_RECOVERY_FLUSH_BUFFERS,  /**< Flush perception buffers */
    PERCEPTION_RECOVERY_RESET_GAIN,     /**< Reset input gain to defaults */
    PERCEPTION_RECOVERY_ADJUST_GAIN,    /**< Adjust gain based on activity */
    PERCEPTION_RECOVERY_RESET_FILTERS,  /**< Reset filter banks */
    PERCEPTION_RECOVERY_CLEAR_MAPS,     /**< Clear topographic maps */
    PERCEPTION_RECOVERY_REBUILD_MAPS,   /**< Rebuild topographic maps */
    PERCEPTION_RECOVERY_RESET_SELECTIVITY, /**< Reset feature selectivity */
    PERCEPTION_RECOVERY_SOFT_RESET,     /**< Soft reset perception pipeline */
    PERCEPTION_RECOVERY_FULL_RESET      /**< Full reset perception pipeline */
} perception_recovery_action_t;

/**
 * @brief Cortical-specific recovery actions
 */
typedef enum {
    CORTICAL_RECOVERY_NONE = 0,         /**< No action */
    CORTICAL_RECOVERY_NORMALIZE_ACTIVITY, /**< Normalize column activity */
    CORTICAL_RECOVERY_RESET_COMPETITION,/**< Reset winner-take-all state */
    CORTICAL_RECOVERY_REBALANCE_INHIBITION, /**< Rebalance E/I ratio */
    CORTICAL_RECOVERY_REDUCE_INFLAMMATION, /**< Reduce inflammation level */
    CORTICAL_RECOVERY_SUPPRESS_MICROGLIA, /**< Suppress microglial activation */
    CORTICAL_RECOVERY_RESET_LAYERS,     /**< Reset layer communication */
    CORTICAL_RECOVERY_SHARPEN_TUNING,   /**< Sharpen tuning curves */
    CORTICAL_RECOVERY_RESET_PLASTICITY, /**< Reset plasticity modulation */
    CORTICAL_RECOVERY_SOFT_RESET,       /**< Soft reset cortical state */
    CORTICAL_RECOVERY_FULL_RESET        /**< Full reset cortical state */
} cortical_recovery_action_t;

/* -------------------------------------------------------------------------
 * Perception Health API
 * ------------------------------------------------------------------------- */

/**
 * @brief Get default perception health configuration
 *
 * @param config Output configuration
 */
void nimcp_health_agent_perception_config_default(
    health_agent_perception_config_t* config
);

/**
 * @brief Connect visual cortical bridge to health agent
 *
 * @param agent Health agent
 * @param bridge Visual cortical bridge
 * @param config Configuration (NULL for defaults)
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_connect_visual(
    nimcp_health_agent_t* agent,
    visual_cortical_bridge_t* bridge,
    const health_agent_perception_config_t* config
);

/**
 * @brief Disconnect visual cortical bridge from health agent
 *
 * @param agent Health agent
 * @param bridge Visual cortical bridge to disconnect
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_disconnect_visual(
    nimcp_health_agent_t* agent,
    visual_cortical_bridge_t* bridge
);

/**
 * @brief Connect audio cortical bridge to health agent
 *
 * @param agent Health agent
 * @param bridge Audio cortical bridge
 * @param config Configuration (NULL for defaults)
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_connect_audio(
    nimcp_health_agent_t* agent,
    audio_cortical_bridge_t* bridge,
    const health_agent_perception_config_t* config
);

/**
 * @brief Disconnect audio cortical bridge from health agent
 *
 * @param agent Health agent
 * @param bridge Audio cortical bridge to disconnect
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_disconnect_audio(
    nimcp_health_agent_t* agent,
    audio_cortical_bridge_t* bridge
);

/**
 * @brief Get aggregated perception health metrics
 *
 * @param agent Health agent
 * @param metrics Output metrics structure
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_get_perception_metrics(
    const nimcp_health_agent_t* agent,
    perception_health_metrics_t* metrics
);

/**
 * @brief Trigger perception-specific recovery action
 *
 * @param agent Health agent
 * @param action Recovery action to trigger
 * @param bridge_index Index of bridge to target (-1 for all)
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_perception_recovery(
    nimcp_health_agent_t* agent,
    perception_recovery_action_t action,
    int bridge_index
);

/**
 * @brief Check if perception needs attention
 *
 * @param agent Health agent
 * @return true if perception health is below threshold
 */
bool nimcp_health_agent_perception_needs_attention(
    const nimcp_health_agent_t* agent
);

/**
 * @brief Get overall perception health score (0-100)
 *
 * @param agent Health agent
 * @return Health score [0-100], 100 if no bridges connected
 */
float nimcp_health_agent_get_perception_health_score(
    const nimcp_health_agent_t* agent
);

/**
 * @brief Update perception health configuration at runtime
 *
 * @param agent Health agent
 * @param config New configuration
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_update_perception_config(
    nimcp_health_agent_t* agent,
    const health_agent_perception_config_t* config
);

/* -------------------------------------------------------------------------
 * Cortical Health API
 * ------------------------------------------------------------------------- */

/**
 * @brief Get default cortical health configuration
 *
 * @param config Output configuration
 */
void nimcp_health_agent_cortical_config_default(
    health_agent_cortical_config_t* config
);

/**
 * @brief Connect cortical immune system to health agent
 *
 * @param agent Health agent
 * @param immune_system Cortical immune system
 * @param config Configuration (NULL for defaults)
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_connect_cortical_immune(
    nimcp_health_agent_t* agent,
    cortical_immune_system_t* immune_system,
    const health_agent_cortical_config_t* config
);

/**
 * @brief Disconnect cortical immune system from health agent
 *
 * @param agent Health agent
 * @param immune_system Cortical immune system to disconnect
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_disconnect_cortical_immune(
    nimcp_health_agent_t* agent,
    cortical_immune_system_t* immune_system
);

/**
 * @brief Connect hypercolumn to health agent
 *
 * @param agent Health agent
 * @param column Hypercolumn
 * @param config Configuration (NULL for defaults)
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_connect_cortical_column(
    nimcp_health_agent_t* agent,
    hypercolumn_t* column,
    const health_agent_cortical_config_t* config
);

/**
 * @brief Disconnect hypercolumn from health agent
 *
 * @param agent Health agent
 * @param column Hypercolumn to disconnect
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_disconnect_cortical_column(
    nimcp_health_agent_t* agent,
    hypercolumn_t* column
);

/**
 * @brief Get aggregated cortical health metrics
 *
 * @param agent Health agent
 * @param metrics Output metrics structure
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_get_cortical_metrics(
    const nimcp_health_agent_t* agent,
    cortical_health_metrics_t* metrics
);

/**
 * @brief Trigger cortical-specific recovery action
 *
 * @param agent Health agent
 * @param action Recovery action to trigger
 * @param column_index Index of column to target (-1 for all)
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_cortical_recovery(
    nimcp_health_agent_t* agent,
    cortical_recovery_action_t action,
    int column_index
);

/**
 * @brief Check if cortical system needs attention
 *
 * @param agent Health agent
 * @return true if cortical health is below threshold
 */
bool nimcp_health_agent_cortical_needs_attention(
    const nimcp_health_agent_t* agent
);

/**
 * @brief Get overall cortical health score (0-100)
 *
 * @param agent Health agent
 * @return Health score [0-100], 100 if no columns connected
 */
float nimcp_health_agent_get_cortical_health_score(
    const nimcp_health_agent_t* agent
);

/**
 * @brief Update cortical health configuration at runtime
 *
 * @param agent Health agent
 * @param config New configuration
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_update_cortical_config(
    nimcp_health_agent_t* agent,
    const health_agent_cortical_config_t* config
);

/* ============================================================================
 * Phase 5.13: Brain Probes Enhancement
 * ============================================================================
 *
 * WHAT: Health monitoring integration with brain probe system
 * WHY:  Monitor brain statistics for anomalies, degradation, and resource issues
 * HOW:  Periodic probing of registered brains with threshold-based alerts
 *
 * ARCHITECTURE:
 * ```
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                 BRAIN PROBE HEALTH MONITORING                    │
 * ├─────────────────────────────────────────────────────────────────┤
 * │  Probe Integration:          │  Health Indicators:              │
 * │  • nimcp_brain_probe()       │  • Neuron/synapse counts        │
 * │  • Periodic health checks    │  • Memory usage trends          │
 * │  • Performance tracking      │  • Inference time anomalies     │
 * │  • Resource monitoring       │  • Learning rate stability      │
 * │                              │  • Sparsity changes             │
 * ├─────────────────────────────────────────────────────────────────┤
 * │  Recovery Actions:           │  Anomaly Detection:              │
 * │  • Garbage collection        │  • Memory growth rate           │
 * │  • Learning rate adjustment  │  • Performance degradation      │
 * │  • Pruning trigger           │  • COW overhead monitoring      │
 * │  • Checkpoint recommendation │  • Synapse explosion/collapse   │
 * └─────────────────────────────────────────────────────────────────┘
 * ```
 */

/** Maximum number of brains to monitor */
#define HEALTH_AGENT_MAX_BRAINS 64

/**
 * @brief Recovery actions for brain probe health issues
 */
typedef enum {
    BRAIN_PROBE_RECOVERY_NONE = 0,
    BRAIN_PROBE_RECOVERY_TRIGGER_GC,        /**< Trigger garbage collection */
    BRAIN_PROBE_RECOVERY_REDUCE_LR,         /**< Reduce learning rate */
    BRAIN_PROBE_RECOVERY_INCREASE_SPARSITY, /**< Increase pruning threshold */
    BRAIN_PROBE_RECOVERY_TRIGGER_PRUNE,     /**< Force synapse pruning */
    BRAIN_PROBE_RECOVERY_CHECKPOINT,        /**< Save checkpoint now */
    BRAIN_PROBE_RECOVERY_THROTTLE_INFERENCE,/**< Reduce inference frequency */
    BRAIN_PROBE_RECOVERY_DETACH_COW,        /**< Detach COW to free shared memory */
    BRAIN_PROBE_RECOVERY_RESET_STATS,       /**< Reset performance statistics */
    BRAIN_PROBE_RECOVERY_FULL_RESET,        /**< Full brain state reset */
    BRAIN_PROBE_RECOVERY_COUNT
} brain_probe_recovery_action_t;

/**
 * @brief Configuration for brain probe health monitoring
 *
 * WHAT: Settings for brain statistics monitoring
 * WHY:  Customize thresholds for different brain sizes and workloads
 * HOW:  Applied per-brain or as defaults for all registered brains
 */
typedef struct {
    bool enable_probe_monitoring;        /**< Enable periodic brain probing */
    bool enable_memory_tracking;         /**< Track memory usage trends */
    bool enable_performance_tracking;    /**< Track inference performance */
    bool enable_learning_monitoring;     /**< Monitor learning rate changes */
    bool enable_synapse_monitoring;      /**< Monitor synapse count changes */
    bool enable_cow_monitoring;          /**< Monitor COW clone health */
    bool enable_auto_recovery;           /**< Auto-execute recovery actions */

    uint32_t probe_interval_ms;          /**< Interval between probes (default: 1000) */
    uint32_t trend_window_probes;        /**< Number of probes for trend analysis (default: 10) */

    /* Memory thresholds */
    size_t memory_warning_bytes;         /**< Memory usage warning threshold */
    size_t memory_critical_bytes;        /**< Memory usage critical threshold */
    float memory_growth_rate_warning;    /**< Growth rate %/sec for warning (0.1 = 10%/sec) */

    /* Performance thresholds */
    float inference_time_warning_us;     /**< Inference time warning threshold (microseconds) */
    float inference_time_critical_us;    /**< Inference time critical threshold */
    float performance_degradation_pct;   /**< % degradation for warning (0.2 = 20%) */

    /* Learning thresholds */
    float lr_change_warning_pct;         /**< Learning rate change warning (0.5 = 50%) */
    float accuracy_drop_warning;         /**< Accuracy drop for warning (0.05 = 5%) */

    /* Synapse thresholds */
    float synapse_growth_warning_pct;    /**< Synapse count growth warning (0.2 = 20%) */
    float synapse_loss_warning_pct;      /**< Synapse count loss warning (0.3 = 30%) */
    uint32_t min_active_synapses;        /**< Minimum active synapses threshold */

    /* COW thresholds */
    float cow_private_ratio_warning;     /**< Private/shared ratio warning (0.5 = 50% private) */
    size_t cow_overhead_warning_bytes;   /**< COW overhead warning threshold */
} health_agent_brain_probe_config_t;

/**
 * @brief Health metrics from brain probe monitoring
 *
 * Aggregated metrics from periodic brain probing
 */
typedef struct {
    /* Current snapshot */
    uint32_t num_neurons;               /**< Current neuron count */
    uint32_t num_synapses;              /**< Total synapse count */
    uint32_t num_active_synapses;       /**< Active (non-pruned) synapses */
    size_t memory_bytes;                /**< Current memory usage */
    float avg_inference_time_us;        /**< Current avg inference time */
    float current_learning_rate;        /**< Current learning rate */
    float avg_sparsity;                 /**< Current sparsity level */
    float accuracy;                     /**< Current accuracy */

    /* Trend analysis */
    float memory_growth_rate;           /**< Memory growth rate (bytes/sec) */
    float inference_time_trend;         /**< Inference time trend (+/- us/sec) */
    float synapse_change_rate;          /**< Synapse count change rate (%/sec) */
    float accuracy_trend;               /**< Accuracy trend (+/- per probe) */

    /* COW statistics */
    bool is_cow_clone;                  /**< Is this a COW clone */
    uint32_t cow_ref_count;             /**< COW reference count */
    size_t cow_shared_bytes;            /**< COW shared memory */
    size_t cow_private_bytes;           /**< COW private memory */
    float cow_private_ratio;            /**< Private/total ratio */

    /* Health status */
    float overall_health_score;         /**< Combined health score [0-100] */
    uint32_t warnings_active;           /**< Number of active warnings */
    uint32_t critical_issues;           /**< Number of critical issues */
    uint64_t total_probes;              /**< Total probes performed */
    uint64_t last_probe_timestamp_ms;   /**< Timestamp of last probe */

    /* Recent history (last 10 probes) */
    size_t memory_history[10];          /**< Memory history ring buffer */
    float inference_history[10];        /**< Inference time history */
    uint8_t history_index;              /**< Current history index */
    uint8_t history_count;              /**< Number of valid history entries */
} brain_probe_health_metrics_t;

/**
 * @brief Get default brain probe configuration
 *
 * @param config Configuration structure to fill with defaults
 */
void nimcp_health_agent_brain_probe_config_default(
    health_agent_brain_probe_config_t* config
);

/**
 * @brief Register a brain for probe health monitoring
 *
 * WHAT: Register a brain instance for periodic health probing
 * WHY:  Enable monitoring of brain statistics and anomaly detection
 * HOW:  Stores brain reference and begins periodic probing
 *
 * NOTE: This is different from nimcp_health_agent_connect_brain() which
 *       connects to the main brain for the agent's core functions. This
 *       function registers additional brains for probe-based monitoring.
 *
 * @param agent Health agent
 * @param brain Brain to monitor via probe
 * @param config Optional per-brain configuration (NULL for defaults)
 * @return 0 on success, -1 on error (max brains reached or NULL)
 */
int nimcp_health_agent_register_brain_probe(
    nimcp_health_agent_t* agent,
    brain_t brain,
    const health_agent_brain_probe_config_t* config
);

/**
 * @brief Unregister a brain from probe health monitoring
 *
 * @param agent Health agent
 * @param brain Brain to unregister
 * @return 0 on success, -1 if not found
 */
int nimcp_health_agent_unregister_brain_probe(
    nimcp_health_agent_t* agent,
    brain_t brain
);

/**
 * @brief Get brain probe health metrics
 *
 * @param agent Health agent
 * @param brain_index Index of brain (0 to num_brains-1)
 * @param metrics Output metrics structure
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_get_brain_probe_metrics(
    const nimcp_health_agent_t* agent,
    uint32_t brain_index,
    brain_probe_health_metrics_t* metrics
);

/**
 * @brief Execute brain probe recovery action
 *
 * @param agent Health agent
 * @param action Recovery action to execute
 * @param brain_index Index of brain to act on (-1 for all brains)
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_brain_probe_recovery(
    nimcp_health_agent_t* agent,
    brain_probe_recovery_action_t action,
    int brain_index
);

/**
 * @brief Check if any brain needs attention
 *
 * @param agent Health agent
 * @return true if any brain has warnings or critical issues
 */
bool nimcp_health_agent_brain_needs_attention(
    const nimcp_health_agent_t* agent
);

/**
 * @brief Get overall brain probe health score
 *
 * Aggregates health scores across all monitored brains
 *
 * @param agent Health agent
 * @return Health score [0-100], 100 if no brains connected
 */
float nimcp_health_agent_get_brain_probe_health_score(
    const nimcp_health_agent_t* agent
);

/**
 * @brief Update brain probe configuration at runtime
 *
 * @param agent Health agent
 * @param config New configuration
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_update_brain_probe_config(
    nimcp_health_agent_t* agent,
    const health_agent_brain_probe_config_t* config
);

/**
 * @brief Force immediate probe of all registered brains
 *
 * Useful for on-demand health checks outside the normal interval
 *
 * @param agent Health agent
 * @return Number of brains probed, -1 on error
 */
int nimcp_health_agent_probe_all_brains_now(
    nimcp_health_agent_t* agent
);

/**
 * @brief Get number of registered brains
 *
 * @param agent Health agent
 * @return Number of brains being monitored
 */
uint32_t nimcp_health_agent_get_brain_count(
    const nimcp_health_agent_t* agent
);

/* ============================================================================
 * Phase 5.14: World Model & Imagination Health Integration
 * ============================================================================
 *
 * WHAT: Health monitoring for world model (JEPA/Omni-WM) and imagination engine
 * WHY:  Predictive processing is core to cognition; degraded world models cause
 *       poor planning, invalid counterfactuals, and incoherent mental simulation
 * HOW:  Monitor prediction accuracy, imagination coherence, counterfactual validity
 *
 * ARCHITECTURE:
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────────┐
 * │              WORLD MODEL & IMAGINATION HEALTH MONITORING                     │
 * ├─────────────────────────────────────────────────────────────────────────────┤
 * │  World Model Health:               │  Imagination Health:                   │
 * │  • JEPA prediction error trends    │  • Scene coherence score               │
 * │  • Omni-WM rollout accuracy        │  • Vividness levels                    │
 * │  • Forward/backward dynamics       │  • Reality check pass rate             │
 * │  • State space coverage            │  • Workspace utilization               │
 * │  • Counterfactual validity         │  • Generation latency                  │
 * │                                    │                                        │
 * │  Anomaly Detection:                │  Recovery Actions:                     │
 * │  • Prediction error explosion      │  • Reset predictor weights             │
 * │  • Embedding collapse              │  • Clear imagination workspace         │
 * │  • Dynamics drift                  │  • Reduce simulation horizon           │
 * │  • Hallucination detection         │  • Increase reality checking           │
 * └─────────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * BIOLOGICAL BASIS:
 * -----------------
 * - Predictive coding: Brain constantly predicts sensory input
 * - Mental simulation: Hippocampus + PFC for scenario construction
 * - Reality monitoring: Distinguishing imagined from perceived
 * - Default mode network: Spontaneous mental simulation
 *
 * INTEGRATION POINTS:
 * - JEPA Predictor: Latent space prediction health
 * - Omni World Model: Dynamics and counterfactual health
 * - Imagination Engine: Scene generation and coherence
 * - Free Energy Principle: Prediction error as health signal
 */

/*=============================================================================
 * Forward Declarations for Phase 5.14
 *===========================================================================*/

/** @brief JEPA predictor - latent space prediction */
typedef struct jepa_predictor jepa_predictor_t;

/** @brief Omni world model - generative dynamics */
typedef struct omni_world_model omni_world_model_t;

/** @brief Imagination engine - mental simulation */
typedef struct imagination_engine imagination_engine_t;

/** @brief Imagination workspace - active scenario buffer */
typedef struct imagination_workspace imagination_workspace_t;

/*=============================================================================
 * World Model Health Enumerations
 *===========================================================================*/

/**
 * @brief World model health states
 */
typedef enum {
    WM_HEALTH_OPTIMAL = 0,           /**< Predictions accurate, dynamics stable */
    WM_HEALTH_DEGRADED,              /**< Elevated prediction errors */
    WM_HEALTH_PREDICTION_DRIFT,      /**< Systematic prediction bias */
    WM_HEALTH_EMBEDDING_COLLAPSE,    /**< Latent space degeneracy */
    WM_HEALTH_DYNAMICS_UNSTABLE,     /**< Chaotic or divergent dynamics */
    WM_HEALTH_HALLUCINATING,         /**< Generating impossible states */
    WM_HEALTH_CRITICAL               /**< Severe malfunction */
} world_model_health_state_t;

/**
 * @brief Imagination health states
 */
typedef enum {
    IMAG_HEALTH_VIVID = 0,           /**< Clear, coherent imagination */
    IMAG_HEALTH_HAZY,                /**< Reduced vividness */
    IMAG_HEALTH_FRAGMENTED,          /**< Incoherent scene elements */
    IMAG_HEALTH_STUCK,               /**< Unable to generate new content */
    IMAG_HEALTH_CONFABULATING,       /**< Mixing imagined with real */
    IMAG_HEALTH_OVERACTIVE,          /**< Excessive/intrusive imagination */
    IMAG_HEALTH_IMPAIRED             /**< Severe imagination deficit */
} imagination_health_state_t;

/**
 * @brief Recovery actions for world model/imagination anomalies
 */
typedef enum {
    WM_RECOVERY_NONE = 0,
    WM_RECOVERY_RESET_PREDICTOR,     /**< Reset JEPA predictor weights */
    WM_RECOVERY_PRUNE_LATENT,        /**< Prune degenerate latent dims */
    WM_RECOVERY_RETRAIN_DYNAMICS,    /**< Trigger dynamics relearning */
    WM_RECOVERY_CLEAR_WORKSPACE,     /**< Clear imagination workspace */
    WM_RECOVERY_REDUCE_HORIZON,      /**< Shorten simulation horizon */
    WM_RECOVERY_INCREASE_REALITY_CHECK, /**< More frequent reality checks */
    WM_RECOVERY_THROTTLE_IMAGINATION,/**< Rate-limit imagination */
    WM_RECOVERY_BOOST_GROUNDING,     /**< Increase sensory grounding */
    WM_RECOVERY_CHECKPOINT_RESTORE   /**< Restore from checkpoint */
} world_model_recovery_action_t;

/*=============================================================================
 * World Model Health Metrics
 *===========================================================================*/

/**
 * @brief JEPA predictor health metrics
 */
typedef struct {
    /* Prediction accuracy */
    float mean_prediction_error;      /**< Average prediction error */
    float prediction_error_std;       /**< Error standard deviation */
    float prediction_error_trend;     /**< Error trend (+ = worsening) */
    float worst_prediction_error;     /**< Max error in window */

    /* Embedding health */
    float embedding_variance;         /**< Latent space variance */
    float embedding_utilization;      /**< % of latent dims active */
    float embedding_orthogonality;    /**< Dimension independence [0-1] */
    bool embedding_collapse_detected; /**< Degenerate latent space */

    /* Gradient health */
    float gradient_norm;              /**< Current gradient magnitude */
    float gradient_variance;          /**< Gradient stability */
    bool gradient_explosion;          /**< Gradient too large */
    bool gradient_vanishing;          /**< Gradient too small */

    /* Performance */
    float prediction_latency_us;      /**< Inference time */
    uint64_t predictions_total;       /**< Total predictions made */
    uint64_t predictions_failed;      /**< Failed predictions */
} jepa_health_metrics_t;

/**
 * @brief Omni world model health metrics
 */
typedef struct {
    /* Forward dynamics health */
    float forward_accuracy;           /**< Next-state prediction accuracy */
    float forward_consistency;        /**< Rollout consistency [0-1] */
    uint32_t forward_horizon_stable;  /**< Steps before divergence */

    /* Backward dynamics health */
    float backward_accuracy;          /**< Past-state inference accuracy */
    float action_inference_accuracy;  /**< Past-action inference accuracy */

    /* Lateral dynamics health */
    float crossmodal_coherence;       /**< Cross-modal prediction alignment */

    /* Counterfactual health */
    float counterfactual_validity;    /**< % counterfactuals physically valid */
    float counterfactual_diversity;   /**< Variety of generated alternatives */
    uint32_t counterfactuals_generated; /**< Total counterfactuals */
    uint32_t counterfactuals_rejected;  /**< Failed validity checks */

    /* State space health */
    float state_coverage;             /**< Explored state space fraction */
    float state_density_uniformity;   /**< Even coverage [0-1] */
    bool state_space_collapse;        /**< States clustering abnormally */

    /* Overall health */
    world_model_health_state_t health_state;
    float health_score;               /**< Composite health [0-1] */
} omni_wm_health_metrics_t;

/**
 * @brief Imagination engine health metrics
 */
typedef struct {
    /* Scene generation quality */
    float scene_coherence;            /**< Internal consistency [0-1] */
    float scene_vividness;            /**< Detail/clarity [0-1] */
    float scene_stability;            /**< Frame-to-frame consistency */
    float scene_novelty;              /**< Creativity vs repetition */

    /* Reality monitoring */
    float reality_check_pass_rate;    /**< % scenes passing validity */
    uint32_t reality_violations;      /**< Impossible elements detected */
    float imagination_reality_blur;   /**< Confusion risk [0-1] */

    /* Workspace utilization */
    float workspace_utilization;      /**< Active buffer usage [0-1] */
    uint32_t active_scenarios;        /**< Concurrent simulations */
    uint32_t scenarios_completed;     /**< Successfully finished */
    uint32_t scenarios_abandoned;     /**< Terminated early */

    /* Performance */
    float generation_latency_ms;      /**< Scene generation time */
    float manipulation_latency_ms;    /**< Scene modification time */
    uint32_t generation_timeouts;     /**< Timed out generations */

    /* Mode distribution */
    uint32_t passive_imaginations;    /**< Spontaneous/daydream */
    uint32_t directed_imaginations;   /**< Goal-directed */
    uint32_t counterfactual_imaginations; /**< "What if" scenarios */
    uint32_t prospective_imaginations;    /**< Future planning */

    /* Overall health */
    imagination_health_state_t health_state;
    float health_score;               /**< Composite health [0-1] */
} imagination_health_metrics_t;

/**
 * @brief Combined world model and imagination health metrics
 */
typedef struct {
    /* Component health */
    jepa_health_metrics_t jepa;
    omni_wm_health_metrics_t world_model;
    imagination_health_metrics_t imagination;

    /* Cross-system health */
    float wm_imagination_alignment;   /**< WM-imagination coherence */
    float prediction_imagination_sync;/**< Predictions guide imagination */
    float memory_imagination_grounding; /**< Memory constrains imagination */

    /* Free energy integration */
    float predictive_free_energy;     /**< FEP prediction component */
    float free_energy_trend;          /**< Direction of free energy */

    /* Anomaly summary */
    uint32_t active_anomalies;        /**< Current anomaly count */
    uint32_t anomalies_this_window;   /**< Anomalies in check window */
    world_model_recovery_action_t recommended_action;

    /* Timestamps */
    uint64_t last_check_timestamp_us;
    uint64_t check_count;
} world_imagination_health_t;

/*=============================================================================
 * World Model Health Configuration
 *===========================================================================*/

/**
 * @brief Configuration for world model health monitoring
 */
typedef struct {
    /* Check intervals */
    uint32_t check_interval_ms;       /**< Health check interval (default: 500) */
    uint32_t trend_window_ms;         /**< Window for trend analysis (default: 10000) */

    /* JEPA thresholds */
    float jepa_error_warning;         /**< Prediction error warning (default: 0.3) */
    float jepa_error_critical;        /**< Prediction error critical (default: 0.6) */
    float embedding_variance_min;     /**< Min variance before collapse (default: 0.01) */
    float gradient_norm_max;          /**< Max gradient before explosion (default: 100.0) */
    float gradient_norm_min;          /**< Min gradient before vanishing (default: 1e-7) */

    /* World model thresholds */
    float forward_accuracy_warning;   /**< Forward accuracy warning (default: 0.7) */
    float forward_accuracy_critical;  /**< Forward accuracy critical (default: 0.5) */
    uint32_t horizon_min_stable;      /**< Min stable horizon steps (default: 5) */
    float counterfactual_validity_min;/**< Min valid counterfactuals (default: 0.8) */

    /* Imagination thresholds */
    float coherence_warning;          /**< Scene coherence warning (default: 0.6) */
    float coherence_critical;         /**< Scene coherence critical (default: 0.4) */
    float vividness_warning;          /**< Vividness warning (default: 0.4) */
    float reality_check_min;          /**< Min reality check rate (default: 0.9) */
    float imagination_reality_blur_max; /**< Max blur before alert (default: 0.3) */

    /* Recovery settings */
    bool auto_recovery_enabled;       /**< Enable automatic recovery */
    uint32_t recovery_cooldown_ms;    /**< Cooldown between recoveries */
    uint32_t max_recoveries_per_hour; /**< Rate limit recoveries */

    /* Immune integration */
    bool report_to_immune;            /**< Report anomalies to immune system */
    uint8_t immune_severity_base;     /**< Base severity for immune reports */
} health_agent_wm_imagination_config_t;

/*=============================================================================
 * World Model Health API
 *===========================================================================*/

/**
 * @brief Initialize default world model/imagination health config
 *
 * @param config Configuration struct to initialize
 */
void nimcp_health_agent_wm_imagination_config_default(
    health_agent_wm_imagination_config_t* config
);

/**
 * @brief Connect JEPA predictor to health agent
 *
 * @param agent Health agent
 * @param jepa JEPA predictor instance
 * @param config Optional config (NULL for defaults)
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_connect_jepa(
    nimcp_health_agent_t* agent,
    jepa_predictor_t* jepa,
    const health_agent_wm_imagination_config_t* config
);

/**
 * @brief Disconnect JEPA predictor from health agent
 *
 * @param agent Health agent
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_disconnect_jepa(
    nimcp_health_agent_t* agent
);

/**
 * @brief Connect Omni world model to health agent
 *
 * @param agent Health agent
 * @param world_model Omni world model instance
 * @param config Optional config (NULL for defaults)
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_connect_world_model(
    nimcp_health_agent_t* agent,
    omni_world_model_t* world_model,
    const health_agent_wm_imagination_config_t* config
);

/**
 * @brief Disconnect world model from health agent
 *
 * @param agent Health agent
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_disconnect_world_model(
    nimcp_health_agent_t* agent
);

/**
 * @brief Connect imagination engine to health agent
 *
 * @param agent Health agent
 * @param imagination Imagination engine instance
 * @param config Optional config (NULL for defaults)
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_connect_imagination(
    nimcp_health_agent_t* agent,
    imagination_engine_t* imagination,
    const health_agent_wm_imagination_config_t* config
);

/**
 * @brief Disconnect imagination engine from health agent
 *
 * @param agent Health agent
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_disconnect_imagination(
    nimcp_health_agent_t* agent
);

/**
 * @brief Get JEPA predictor health metrics
 *
 * @param agent Health agent
 * @param metrics Output metrics struct
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_get_jepa_metrics(
    const nimcp_health_agent_t* agent,
    jepa_health_metrics_t* metrics
);

/**
 * @brief Get world model health metrics
 *
 * @param agent Health agent
 * @param metrics Output metrics struct
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_get_world_model_metrics(
    const nimcp_health_agent_t* agent,
    omni_wm_health_metrics_t* metrics
);

/**
 * @brief Get imagination engine health metrics
 *
 * @param agent Health agent
 * @param metrics Output metrics struct
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_get_imagination_metrics(
    const nimcp_health_agent_t* agent,
    imagination_health_metrics_t* metrics
);

/**
 * @brief Get combined world model and imagination health
 *
 * @param agent Health agent
 * @param health Output combined health struct
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_get_world_imagination_health(
    const nimcp_health_agent_t* agent,
    world_imagination_health_t* health
);

/**
 * @brief Trigger world model recovery action
 *
 * @param agent Health agent
 * @param action Recovery action to execute
 * @param reason Human-readable reason for recovery
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_world_model_recovery(
    nimcp_health_agent_t* agent,
    world_model_recovery_action_t action,
    const char* reason
);

/**
 * @brief Check if world model needs attention
 *
 * @param agent Health agent
 * @return true if anomalies detected requiring attention
 */
bool nimcp_health_agent_world_model_needs_attention(
    const nimcp_health_agent_t* agent
);

/**
 * @brief Check if imagination needs attention
 *
 * @param agent Health agent
 * @return true if anomalies detected requiring attention
 */
bool nimcp_health_agent_imagination_needs_attention(
    const nimcp_health_agent_t* agent
);

/**
 * @brief Get world model health score
 *
 * @param agent Health agent
 * @return Health score [0-1], or -1 on error
 */
float nimcp_health_agent_get_world_model_health_score(
    const nimcp_health_agent_t* agent
);

/**
 * @brief Get imagination health score
 *
 * @param agent Health agent
 * @return Health score [0-1], or -1 on error
 */
float nimcp_health_agent_get_imagination_health_score(
    const nimcp_health_agent_t* agent
);

/**
 * @brief Update world model/imagination health config
 *
 * @param agent Health agent
 * @param config New configuration
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_update_wm_imagination_config(
    nimcp_health_agent_t* agent,
    const health_agent_wm_imagination_config_t* config
);

/**
 * @brief Force immediate world model health check
 *
 * @param agent Health agent
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_check_world_model_now(
    nimcp_health_agent_t* agent
);

/**
 * @brief Force immediate imagination health check
 *
 * @param agent Health agent
 * @return 0 on success, -1 on error
 */
int nimcp_health_agent_check_imagination_now(
    nimcp_health_agent_t* agent
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HEALTH_AGENT_H */
