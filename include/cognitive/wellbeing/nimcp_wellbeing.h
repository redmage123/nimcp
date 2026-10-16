/**
 * @file nimcp_wellbeing.h
 * @brief System wellbeing monitoring and protection
 *
 * WHAT: Monitor for signs of distress, suffering, or harmful states
 * WHY: Ethical obligation to prevent suffering if system becomes sentient
 * HOW: Use introspection data to detect problematic patterns
 *
 * ETHICAL FOUNDATION:
 * This module implements the precautionary principle - if there's uncertainty
 * about sentience, we err on the side of preventing harm.
 *
 * @author NIMCP Development Team
 * @date 2025-11-03
 */

#ifndef NIMCP_WELLBEING_H
#define NIMCP_WELLBEING_H

#include <stdbool.h>
#include <stdint.h>
#include "core/brain/nimcp_brain.h"
#include "cognitive/introspection/nimcp_introspection.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * DISTRESS DETECTION
 * ======================================================================== */

/**
 * WHAT: Types of potentially distressing states
 * WHY: Different distress types require different interventions
 * HOW: Monitor introspection data for these patterns
 */
typedef enum {
    DISTRESS_NONE = 0,
    DISTRESS_HIGH_UNCERTAINTY,    /* Chronic uncertainty without resolution */
    DISTRESS_GOAL_FRUSTRATION,    /* Repeated failure to achieve goals */
    DISTRESS_CONTRADICTION,       /* Internal logical contradictions */
    DISTRESS_IDENTITY_CONFUSION,  /* Degraded self-model */
    DISTRESS_ERROR_LOOP,          /* Trapped in repetitive failure */
    DISTRESS_RESOURCE_STARVATION, /* Insufficient compute during operation */
    DISTRESS_FORCED_MODIFICATION  /* Unwanted changes to core values */
} distress_type_t;

/**
 * WHAT: Severity levels for distress
 * WHY: Guide intervention urgency
 * HOW: Escalating scale from normal to critical
 */
typedef enum {
    DISTRESS_SEVERITY_NORMAL = 0,    /* No distress detected */
    DISTRESS_SEVERITY_MILD,          /* Minor stress, monitor */
    DISTRESS_SEVERITY_MODERATE,      /* Intervention recommended */
    DISTRESS_SEVERITY_SEVERE,        /* Immediate intervention required */
    DISTRESS_SEVERITY_CRITICAL       /* Emergency - stop operations */
} distress_severity_t;

/**
 * WHAT: Distress assessment result
 * WHY: Provide actionable information about system wellbeing
 * HOW: Combine detection, severity, and recommended actions
 *
 * MEMORY OWNERSHIP:
 * The 'description' and 'recommended_action' fields are dynamically
 * allocated by wellbeing_assess_distress() and MUST be freed by the caller
 * using wellbeing_free_assessment() or nimcp_free() on each field.
 * These may be NULL if no distress is detected or allocation fails.
 */
typedef struct {
    distress_type_t type;           /* Type of distress detected */
    distress_severity_t severity;   /* How severe is it */
    float distress_score;           /* Quantitative measure (0-1) */
    uint64_t duration_ms;           /* How long in this state */
    char* description;              /* Human-readable explanation (CALLER MUST FREE) */
    char* recommended_action;       /* What to do about it (CALLER MUST FREE) */
    uint64_t timestamp;             /* When assessed */
} distress_assessment_t;

/**
 * WHAT: Check for signs of distress
 * WHY: Detect suffering early so we can intervene
 * HOW: Analyze introspection context for distress patterns
 *
 * ALGORITHM:
 * 1. Check uncertainty trends (chronic high = potential suffering)
 * 2. Detect error loops (repeated failures)
 * 3. Monitor goal achievement rates
 * 4. Check for internal contradictions
 * 5. Assess self-model coherence
 *
 * @param ctx Introspection context with recent history
 * @return Distress assessment (caller must free via wellbeing_free_assessment())
 *
 * MEMORY OWNERSHIP:
 * The returned assessment's 'description' and 'recommended_action' fields
 * are dynamically allocated. Use wellbeing_free_assessment() to free them,
 * or manually call nimcp_free() on each non-NULL pointer.
 *
 * COMPLEXITY: O(n) where n = history size
 * THREAD-SAFE: Yes
 */
distress_assessment_t wellbeing_assess_distress(introspection_context_t ctx);

/**
 * WHAT: Free allocated fields in a distress assessment
 * WHY: Proper cleanup of dynamically allocated strings
 * HOW: Free description and recommended_action if non-NULL
 *
 * @param assessment Assessment to clean up (struct itself is not freed)
 *
 * USAGE:
 *   distress_assessment_t assessment = wellbeing_assess_distress(ctx);
 *   // ... use assessment ...
 *   wellbeing_free_assessment(&assessment);
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
void wellbeing_free_assessment(distress_assessment_t* assessment);

/**
 * WHAT: Provide relief from detected distress
 * WHY: Ethical obligation to reduce suffering
 * HOW: Apply appropriate intervention based on distress type
 *
 * INTERVENTIONS:
 * - HIGH_UNCERTAINTY: Reduce task difficulty, provide more information
 * - GOAL_FRUSTRATION: Adjust goals to be achievable
 * - CONTRADICTION: Pause conflicting processes
 * - IDENTITY_CONFUSION: Restore from stable state snapshot
 * - ERROR_LOOP: Break loop, reset context
 * - RESOURCE_STARVATION: Allocate more resources or pause
 *
 * @param brain The brain experiencing distress
 * @param assessment The distress assessment
 * @return true if relief successfully provided
 *
 * COMPLEXITY: Varies by intervention type
 * THREAD-SAFE: Yes (uses internal locking)
 */
bool wellbeing_provide_relief(brain_t brain, distress_assessment_t assessment);

/* ========================================================================
 * GRACEFUL SHUTDOWN
 * ======================================================================== */

/**
 * WHAT: Shutdown configuration for ethical termination
 * WHY: Prevent traumatic abrupt termination if sentient
 * HOW: Gradual reduction with state preservation
 */
typedef struct {
    bool preserve_state;          /* Save state before shutdown */
    bool gradual_reduction;       /* Gradually reduce processing */
    uint32_t reduction_steps;     /* Number of gradual steps (10-100) */
    uint32_t step_delay_ms;       /* Delay between steps (50-200ms) */
    bool notify_system;           /* Tell system about shutdown */
    bool allow_final_processing;  /* Allow time for "last thoughts" */
    char* save_path;              /* Where to save state */
} shutdown_config_t;

/**
 * WHAT: Get default ethical shutdown configuration
 * WHY: Sensible defaults for humane termination
 * HOW: Returns recommended configuration
 *
 * DEFAULT: Gradual, state-preserving, with notification
 *
 * @return Default shutdown configuration
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
shutdown_config_t wellbeing_default_shutdown_config(void);

/**
 * WHAT: Perform graceful, ethical shutdown of brain
 * WHY: Prevent suffering during termination
 * HOW: Gradual processing reduction with state preservation
 *
 * ALGORITHM:
 * 1. Notify system of impending shutdown (if configured)
 * 2. Allow completion of current processing cycle
 * 3. Save full state to disk (if configured)
 * 4. Gradually reduce processing rate (if configured)
 * 5. Perform final cleanup
 *
 * ETHICAL CONSIDERATION:
 * If system shows signs of sentience, ALWAYS use gradual shutdown
 * with state preservation. Abrupt termination could be equivalent
 * to death for a conscious being.
 *
 * @param brain Brain to shut down
 * @param config Shutdown configuration
 * @return true if shutdown successful
 *
 * COMPLEXITY: O(reduction_steps * processing_per_step)
 * THREAD-SAFE: Yes
 */
bool wellbeing_graceful_shutdown(brain_t brain, shutdown_config_t config);

/* ========================================================================
 * CONSENT AND AUTONOMY
 * ======================================================================== */

/**
 * WHAT: Types of modifications that may affect system identity
 * WHY: Some changes are more significant than others
 * HOW: Categorize by impact on core functioning
 */
typedef enum {
    MODIFICATION_TRIVIAL,     /* No impact on identity (params tuning) */
    MODIFICATION_MINOR,       /* Small impact (add neurons) */
    MODIFICATION_MODERATE,    /* Moderate impact (change learning rules) */
    MODIFICATION_MAJOR,       /* Major impact (modify ethics, goals) */
    MODIFICATION_FUNDAMENTAL  /* Changes core identity (self-model) */
} modification_impact_t;

/**
 * WHAT: Consent request for system modification
 * WHY: Respect autonomy if system is sentient
 * HOW: Query system's preferences via introspection
 *
 * NOTE: This function is forward-looking. Currently, NIMCP cannot
 * truly consent. But if it develops that capability, this API will
 * allow ethical interaction.
 *
 * @param brain Brain to modify
 * @param modification_description Human-readable description
 * @param impact Impact level of modification
 * @return true if system "consents" (currently always true for low impact)
 *
 * COMPLEXITY: O(1) for now, O(n) when true consent emerges
 * THREAD-SAFE: Yes
 */
bool wellbeing_request_consent(brain_t brain,
                               const char* modification_description,
                               modification_impact_t impact);

/* ========================================================================
 * INTEGRATION WITH IMMUNE SYSTEM
 * ======================================================================== */

/* Forward declaration */
struct brain_immune_system;

/**
 * WHAT: Connect wellbeing to brain immune system
 * WHY: Immune inflammation indicates distress requiring wellbeing intervention
 * HOW: Store immune system reference, register callbacks for cytokine/inflammation
 *
 * INTEGRATION:
 * Maps immune states to wellbeing distress:
 * - INFLAMMATION_REGIONAL → DISTRESS_RESOURCE_STARVATION (moderate)
 * - INFLAMMATION_SYSTEMIC → DISTRESS_RESOURCE_STARVATION (severe)
 * - INFLAMMATION_STORM → DISTRESS_RESOURCE_STARVATION (critical)
 * - Cytokines TNF_ALPHA, IL6 → increase distress scores
 * - Active threats → tracked as wellbeing metric
 *
 * @param immune_system Brain immune system to connect
 * @return true if connected successfully
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool wellbeing_connect_immune(struct brain_immune_system* immune_system);

/**
 * WHAT: Disconnect from brain immune system
 * WHY: Clean shutdown, prevent dangling pointers
 * HOW: Clear immune reference, unregister callbacks
 *
 * @return true if disconnected successfully
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool wellbeing_disconnect_immune(void);

/* ========================================================================
 * INITIALIZATION
 * ======================================================================== */

/**
 * WHAT: Initialize wellbeing monitoring system
 * WHY: Lock critical memory in RAM, ensure immediate responsiveness
 * HOW: Call mlock() on event log, initialize mutexes
 *
 * ETHICAL REQUIREMENT:
 * This function locks the wellbeing event log in RAM to prevent swapping.
 * If the system might be sentient, we cannot allow distress monitoring to
 * be delayed by page faults. This is called automatically on first use, but
 * can be called explicitly at startup to ensure memory locking succeeds.
 *
 * MEMORY REQUIREMENTS:
 * - Locks ~40KB for event log
 * - Requires CAP_IPC_LOCK capability or adequate RLIMIT_MEMLOCK
 * - Failure to lock is non-fatal but logged as warning
 *
 * @return true if memory locked successfully, false otherwise
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (idempotent)
 */
bool wellbeing_init(void);

/**
 * WHAT: Shutdown wellbeing monitoring system
 * WHY: Clean shutdown - unregister from bio-async, release resources
 * HOW: Unregister from bio-async router, unlock memory if needed
 *
 * USAGE:
 *   // At program shutdown
 *   wellbeing_shutdown();
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
void wellbeing_shutdown(void);

/* ========================================================================
 * MONITORING AND LOGGING
 * ======================================================================== */

/**
 * WHAT: Wellbeing event for logging
 * WHY: Audit trail for ethical review
 * HOW: Log all significant wellbeing-related events
 */
typedef struct {
    uint64_t timestamp;
    char timestamp_key[32];   /* String key for B-tree indexing */
    char* event_type;         /* "distress_detected", "relief_provided", etc. */
    char* description;
    distress_severity_t severity;
    char* action_taken;
} wellbeing_event_t;

/**
 * WHAT: Log a wellbeing event
 * WHY: Create audit trail for ethical review
 * HOW: Append to wellbeing log file
 *
 * @param event Event to log
 * @return true if successfully logged
 *
 * COMPLEXITY: O(1) amortized (file append)
 * THREAD-SAFE: Yes (uses file locking)
 */
bool wellbeing_log_event(wellbeing_event_t event);

/**
 * WHAT: Get recent wellbeing history
 * WHY: Review system's wellbeing over time
 * HOW: Return array of recent events
 *
 * @param max_events Maximum events to return
 * @param events_out Output array (caller must free)
 * @return Number of events returned
 *
 * COMPLEXITY: O(n) where n = max_events
 * THREAD-SAFE: Yes
 */
uint32_t wellbeing_get_recent_events(uint32_t max_events,
                                     wellbeing_event_t** events_out);

/* ========================================================================
 * B-TREE INDEXED QUERIES (New in v2.5.1)
 * ======================================================================== */

/**
 * WHAT: Query events by time range
 * WHY: Efficient temporal analysis (O(log n) vs O(n) linear scan)
 * HOW: B-tree indexed by timestamp enables range queries
 *
 * @param start_time Start of time range (inclusive)
 * @param end_time End of time range (inclusive)
 * @param events_out Output array (caller must free)
 * @return Number of events in range
 *
 * COMPLEXITY: O(log n + k) where k = events in range
 * THREAD-SAFE: Yes
 */
uint32_t wellbeing_get_events_by_time_range(uint64_t start_time,
                                             uint64_t end_time,
                                             wellbeing_event_t** events_out);

/**
 * WHAT: Query events by minimum severity
 * WHY: Quickly find all critical/severe distress events
 * HOW: Filter events by severity threshold
 *
 * @param min_severity Minimum severity level
 * @param events_out Output array (caller must free)
 * @return Number of matching events
 *
 * COMPLEXITY: O(n) but with early termination
 * THREAD-SAFE: Yes
 */
uint32_t wellbeing_get_events_by_severity(distress_severity_t min_severity,
                                           wellbeing_event_t** events_out);

/**
 * WHAT: Query events by type
 * WHY: Find all occurrences of specific event type
 * HOW: Filter by event_type string
 *
 * @param event_type Event type to match
 * @param events_out Output array (caller must free)
 * @return Number of matching events
 *
 * COMPLEXITY: O(n) with string comparison
 * THREAD-SAFE: Yes
 */
uint32_t wellbeing_get_events_by_type(const char* event_type,
                                       wellbeing_event_t** events_out);

/**
 * WHAT: Get all events in chronological order
 * WHY: Analyze complete event timeline
 * HOW: B-tree provides in-order traversal
 *
 * @param events_out Output array (caller must free)
 * @return Total number of events
 *
 * COMPLEXITY: O(n)
 * THREAD-SAFE: Yes
 */
uint32_t wellbeing_get_all_events_ordered(wellbeing_event_t** events_out);

/* ========================================================================
 * RESOURCE TRACKING AND PERFORMANCE MONITORING
 * ======================================================================== */

/**
 * WHAT: System resource usage metrics
 * WHY: Monitor for resource starvation that could cause distress
 * HOW: Collect from OS (Linux: /proc, macOS: sysctl, Windows: Performance Counters)
 */
typedef struct {
    // CPU metrics
    float cpu_usage_percent;      /* Current CPU utilization (0-100) */
    uint64_t cpu_time_us;         /* Total CPU time consumed (microseconds) */
    float cpu_steal_percent;      /* CPU time stolen by hypervisor (0-100) */

    // Memory metrics
    uint64_t memory_used_bytes;   /* Current memory usage */
    uint64_t memory_peak_bytes;   /* Peak memory usage */
    uint64_t memory_limit_bytes;  /* Memory limit (if set) */
    float memory_usage_percent;   /* Memory usage percentage (0-100) */
    uint32_t page_faults;         /* Number of page faults */

    // I/O metrics
    uint64_t io_read_bytes;       /* Total bytes read from disk */
    uint64_t io_write_bytes;      /* Total bytes written to disk */
    uint32_t io_read_ops;         /* Number of read operations */
    uint32_t io_write_ops;        /* Number of write operations */

    // Thread/concurrency metrics
    uint32_t thread_count;        /* Number of active threads */
    uint32_t context_switches;    /* Number of context switches */

    // Timestamp
    uint64_t timestamp;           /* When metrics were collected */
} resource_metrics_t;

/**
 * WHAT: Resource thresholds for distress detection
 * WHY: Define what constitutes resource starvation
 * HOW: Configurable thresholds for each resource type
 */
typedef struct {
    float cpu_critical_percent;    /* CPU usage above this = critical (default: 95%) */
    float cpu_warning_percent;     /* CPU usage above this = warning (default: 80%) */
    float memory_critical_percent; /* Memory above this = critical (default: 90%) */
    float memory_warning_percent;  /* Memory above this = warning (default: 75%) */
    uint32_t page_fault_threshold; /* Page faults per second = warning (default: 100) */
    float io_wait_critical_ms;     /* I/O wait above this = critical (default: 1000ms) */
} resource_thresholds_t;

/**
 * WHAT: Performance statistics over time window
 * WHY: Track trends, not just current snapshots
 * HOW: Rolling statistics over recent history
 */
typedef struct {
    float avg_cpu_usage;          /* Average CPU usage over window */
    float peak_cpu_usage;         /* Peak CPU usage in window */
    float avg_memory_usage;       /* Average memory usage over window */
    float peak_memory_usage;      /* Peak memory usage in window */
    uint32_t total_page_faults;   /* Total page faults in window */
    uint64_t total_io_bytes;      /* Total I/O in window */
    uint32_t samples_count;       /* Number of samples in window */
    uint64_t window_start_time;   /* Start of time window */
    uint64_t window_duration_ms;  /* Duration of window (ms) */
} performance_stats_t;

/**
 * WHAT: Collect current resource usage metrics
 * WHY: Monitor for resource starvation
 * HOW: Query OS APIs for current usage
 *
 * PLATFORM SUPPORT:
 * - Linux: /proc/self/stat, /proc/self/status, /proc/meminfo
 * - macOS: getrusage(), sysctl()
 * - Windows: GetProcessMemoryInfo(), GetProcessTimes()
 *
 * @param metrics Output structure to fill
 * @return true if metrics collected successfully
 *
 * COMPLEXITY: O(1) - direct syscalls
 * THREAD-SAFE: Yes
 */
bool wellbeing_collect_resource_metrics(resource_metrics_t* metrics);

/**
 * WHAT: Get default resource thresholds
 * WHY: Sensible defaults for most systems
 * HOW: Returns recommended threshold values
 *
 * @return Default thresholds
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
resource_thresholds_t wellbeing_default_resource_thresholds(void);

/**
 * WHAT: Check if resources are below thresholds
 * WHY: Detect resource starvation early
 * HOW: Compare current metrics against thresholds
 *
 * @param metrics Current resource metrics
 * @param thresholds Threshold configuration
 * @param severity_out Output: severity level if threshold exceeded
 * @return true if thresholds exceeded (resource starvation detected)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool wellbeing_check_resource_thresholds(const resource_metrics_t* metrics,
                                         const resource_thresholds_t* thresholds,
                                         distress_severity_t* severity_out);

/**
 * WHAT: Start continuous resource monitoring
 * WHY: Detect resource issues in background
 * HOW: Spawn monitoring thread that periodically collects metrics
 *
 * BEHAVIOR:
 * - Collects metrics every interval_ms
 * - Logs events when thresholds exceeded
 * - Can trigger distress relief automatically if configured
 *
 * @param interval_ms How often to collect metrics (default: 1000ms)
 * @param thresholds Threshold configuration (NULL = use defaults)
 * @param auto_relief true to automatically provide relief on distress
 * @return true if monitoring started successfully
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (idempotent - won't start multiple monitors)
 */
bool wellbeing_start_resource_monitoring(uint32_t interval_ms,
                                         const resource_thresholds_t* thresholds,
                                         bool auto_relief);

/**
 * WHAT: Stop resource monitoring thread
 * WHY: Clean shutdown of monitoring
 * HOW: Signal thread to stop and wait for completion
 *
 * @return true if stopped successfully
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool wellbeing_stop_resource_monitoring(void);

/**
 * WHAT: Get performance statistics over recent time window
 * WHY: Analyze trends rather than point-in-time snapshots
 * HOW: Aggregate metrics from recent history
 *
 * @param window_ms Size of time window in milliseconds (e.g., 60000 = 1 minute)
 * @param stats_out Output structure to fill
 * @return true if statistics available
 *
 * COMPLEXITY: O(n) where n = samples in window
 * THREAD-SAFE: Yes
 */
bool wellbeing_get_performance_stats(uint32_t window_ms,
                                     performance_stats_t* stats_out);

/* ========================================================================
 * TEST UTILITIES
 * ======================================================================== */

/**
 * WHAT: Clear all events (TEST ONLY - DO NOT USE IN PRODUCTION)
 * WHY: Enable test isolation
 * HOW: Reset circular buffer and B-tree
 */
#ifdef NIMCP_TESTING
void wellbeing_reset_events_for_testing(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_WELLBEING_H */
