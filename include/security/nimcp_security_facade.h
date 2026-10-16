/**
 * @file nimcp_security_facade.h
 * @brief Unified Security Facade API - High-level interface to security subsystem
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: A unified facade providing a single entry point to the entire NIMCP
 *       security subsystem, including all security bridges, FEP bridges, and
 *       the security orchestrator.
 *
 * WHY: The NIMCP security infrastructure includes 14+ security bridges and
 *      their corresponding FEP bridges, plus the security orchestrator and
 *      cognitive hub bridge. Managing these components individually is complex
 *      and error-prone. This facade simplifies:
 *      - Initialization of all security components in correct order
 *      - Graceful shutdown with proper cleanup
 *      - Unified threat assessment across all security sources
 *      - Module enable/disable for runtime security configuration
 *      - Status monitoring for all security components
 *
 * HOW: The facade wraps:
 *      - Security Orchestrator (central coordination)
 *      - Security-Cognitive Hub Bridge (cognitive integration)
 *      - 14 Security Bridges:
 *        1. Distributed Training Bridge (Byzantine fault detection)
 *        2. Knowledge Graph Bridge (Query injection prevention)
 *        3. Game Theory Bridge (Strategy manipulation detection)
 *        4. Imagination Bridge (Confabulation detection)
 *        5. Continual Learning Bridge (Catastrophic forgetting protection)
 *        6. Epistemic Bridge (Belief integrity verification)
 *        7. Collective Bridge (Swarm consensus validation)
 *        8. Hippocampus Bridge (Memory consolidation security)
 *        9. Memory Bridge (Working memory protection)
 *        10. Training Bridge (Training pipeline security)
 *        11. Immune Bridge (Brain immune system integration)
 *        12. Language Bridge (Language processing security)
 *        13. Async Bridge (Asynchronous operation security)
 *        14. Logging Bridge (Security audit logging)
 *      - 14+ corresponding FEP bridges for predictive processing integration
 *
 * DESIGN PATTERNS:
 * - Facade: Simplifies complex subsystem with unified interface
 * - Factory: Creates and configures all security components
 * - Mediator: Coordinates component communication through orchestrator
 * - Observer: Event-driven threat propagation
 *
 * THREAD SAFETY: All functions are thread-safe. The facade uses internal
 * synchronization for concurrent access.
 *
 * USAGE EXAMPLE:
 * ```c
 * // Create facade with default configuration
 * security_facade_config_t config;
 * security_facade_default_config(&config);
 * security_facade_t facade = security_facade_create(&config);
 *
 * // Initialize all security components
 * security_facade_init_all(facade);
 *
 * // Main loop
 * while (running) {
 *     // Process security events
 *     security_facade_process_events(facade);
 *
 *     // Check threat level
 *     float threat;
 *     security_facade_get_threat_level(facade, &threat);
 *     if (threat > 0.8f) {
 *         // Take protective action
 *     }
 * }
 *
 * // Shutdown
 * security_facade_shutdown(facade);
 * security_facade_destroy(facade);
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SECURITY_FACADE_H
#define NIMCP_SECURITY_FACADE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Security orchestrator types */
#include "security/nimcp_security_orchestrator.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * OPAQUE TYPE DECLARATIONS
 * ============================================================================ */

/**
 * WHAT: Opaque handle to security facade
 * WHY: Encapsulation - hide internal complexity of security subsystem
 * HOW: Pimpl idiom - pointer to internal structure
 */
typedef struct security_facade_struct* security_facade_t;

/* ============================================================================
 * CONSTANTS AND LIMITS
 * ============================================================================ */

/** Maximum number of security modules */
#define SEC_FACADE_MAX_MODULES 32

/** Maximum event queue size for processing */
#define SEC_FACADE_MAX_EVENT_QUEUE 1024

/** Facade module name for logging */
#define SEC_FACADE_MODULE_NAME "security_facade"

/** Facade version string */
#define SEC_FACADE_VERSION "1.0.0"

/* ============================================================================
 * MODULE IDENTIFIERS
 * ============================================================================ */

/**
 * WHAT: Enumeration of security modules managed by the facade
 * WHY: Identify modules for enable/disable and status queries
 * HOW: Each module has a unique identifier
 */
typedef enum {
    SEC_MODULE_ORCHESTRATOR = 0,        /**< Security Orchestrator (core) */
    SEC_MODULE_COGNITIVE_HUB,           /**< Security-Cognitive Hub Bridge */

    /* Security Bridges */
    SEC_MODULE_DISTRIBUTED_TRAINING,    /**< Distributed Training Bridge */
    SEC_MODULE_KNOWLEDGE_GRAPH,         /**< Knowledge Graph Bridge */
    SEC_MODULE_GAME_THEORY,             /**< Game Theory Bridge */
    SEC_MODULE_IMAGINATION,             /**< Imagination Bridge */
    SEC_MODULE_CONTINUAL_LEARNING,      /**< Continual Learning Bridge */
    SEC_MODULE_EPISTEMIC,               /**< Epistemic Bridge */
    SEC_MODULE_COLLECTIVE,              /**< Collective Bridge */
    SEC_MODULE_HIPPOCAMPUS,             /**< Hippocampus Bridge */
    SEC_MODULE_MEMORY,                  /**< Memory Bridge */
    SEC_MODULE_TRAINING,                /**< Training Bridge */
    SEC_MODULE_IMMUNE,                  /**< Immune Bridge */
    SEC_MODULE_LANGUAGE,                /**< Language Bridge */
    SEC_MODULE_ASYNC,                   /**< Async Bridge */
    SEC_MODULE_LOGGING,                 /**< Logging Bridge */

    /* FEP Bridges */
    SEC_MODULE_FEP_DISTRIBUTED_TRAINING,/**< Distributed Training FEP Bridge */
    SEC_MODULE_FEP_KNOWLEDGE_GRAPH,     /**< Knowledge Graph FEP Bridge */
    SEC_MODULE_FEP_GAME_THEORY,         /**< Game Theory FEP Bridge */
    SEC_MODULE_FEP_IMAGINATION,         /**< Imagination FEP Bridge */
    SEC_MODULE_FEP_CONTINUAL_LEARNING,  /**< Continual Learning FEP Bridge */
    SEC_MODULE_FEP_EPISTEMIC,           /**< Epistemic FEP Bridge */
    SEC_MODULE_FEP_COLLECTIVE,          /**< Collective FEP Bridge */
    SEC_MODULE_FEP_HIPPOCAMPUS,         /**< Hippocampus FEP Bridge */
    SEC_MODULE_FEP_MEMORY,              /**< Memory FEP Bridge */
    SEC_MODULE_FEP_TRAINING,            /**< Training FEP Bridge */
    SEC_MODULE_FEP_IMMUNE,              /**< Immune FEP Bridge */
    SEC_MODULE_FEP_LANGUAGE,            /**< Language FEP Bridge */
    SEC_MODULE_FEP_ASYNC,               /**< Async FEP Bridge */
    SEC_MODULE_FEP_LOGGING,             /**< Logging FEP Bridge */

    SEC_MODULE_COUNT                    /**< Total module count */
} security_module_id_t;

/**
 * WHAT: Facade operational states
 * WHY: Track facade lifecycle and operational mode
 * HOW: State machine for facade behavior
 */
typedef enum {
    SEC_FACADE_STATE_UNINITIALIZED = 0, /**< Not yet initialized */
    SEC_FACADE_STATE_CREATED,           /**< Created but not initialized */
    SEC_FACADE_STATE_INITIALIZING,      /**< Initialization in progress */
    SEC_FACADE_STATE_READY,             /**< Ready for operation */
    SEC_FACADE_STATE_ACTIVE,            /**< Actively processing */
    SEC_FACADE_STATE_ALERT,             /**< Elevated threat detected */
    SEC_FACADE_STATE_LOCKDOWN,          /**< System in lockdown */
    SEC_FACADE_STATE_SHUTTING_DOWN,     /**< Shutdown in progress */
    SEC_FACADE_STATE_ERROR              /**< Error state */
} security_facade_state_t;

/* ============================================================================
 * CONFIGURATION STRUCTURES
 * ============================================================================ */

/**
 * WHAT: Configuration for individual security module
 * WHY: Allow per-module configuration customization
 * HOW: Flags and thresholds for each module
 */
typedef struct {
    security_module_id_t module_id;     /**< Module identifier */
    bool enabled;                       /**< Whether module is enabled */
    bool auto_connect;                  /**< Auto-connect on init */
    float threat_weight;                /**< Weight in unified threat calculation */
    uint32_t priority;                  /**< Processing priority (higher = more important) */
} security_module_config_t;

/**
 * WHAT: Configuration for security facade
 * WHY: Customize facade behavior at creation time
 * HOW: Struct with all facade configuration parameters
 */
typedef struct {
    /* Module configuration */
    bool enable_all_bridges;            /**< Enable all security bridges */
    bool enable_all_fep_bridges;        /**< Enable all FEP bridges */
    bool enable_cognitive_hub;          /**< Enable cognitive hub integration */

    /* Orchestrator configuration */
    security_orch_config_t orch_config; /**< Orchestrator configuration */

    /* Threat thresholds */
    float alert_threshold;              /**< Threshold for alert state */
    float lockdown_threshold;           /**< Threshold for auto-lockdown */
    float threat_decay_rate;            /**< Threat level decay per second */

    /* Processing configuration */
    bool enable_async_processing;       /**< Enable async event processing */
    uint32_t event_batch_size;          /**< Events to process per batch */
    uint32_t processing_interval_ms;    /**< Processing interval in milliseconds */

    /* Integration options */
    bool connect_bio_async;             /**< Auto-connect to bio-async router */
    bool enable_audit_logging;          /**< Enable security audit logging */
    bool enable_metrics;                /**< Enable performance metrics */

    /* Recovery options */
    bool auto_recovery;                 /**< Enable automatic recovery from errors */
    uint32_t lockdown_timeout_ms;       /**< Lockdown timeout (0 = manual release) */

    /* Module-specific overrides (NULL for defaults) */
    security_module_config_t* module_configs;  /**< Per-module configuration */
    uint32_t module_config_count;              /**< Number of module configs */
} security_facade_config_t;

/* ============================================================================
 * STATUS AND STATISTICS STRUCTURES
 * ============================================================================ */

/**
 * WHAT: Status of a single security module
 * WHY: Track individual module health and activity
 * HOW: State flags and counters
 */
typedef struct {
    security_module_id_t module_id;     /**< Module identifier */
    bool enabled;                       /**< Whether module is enabled */
    bool initialized;                   /**< Whether module is initialized */
    bool connected;                     /**< Whether module is connected */
    bool healthy;                       /**< Whether module is healthy */
    float current_threat_level;         /**< Module's current threat level */
    uint64_t events_processed;          /**< Events processed by module */
    uint64_t threats_detected;          /**< Threats detected by module */
    uint64_t last_activity_ms;          /**< Last activity timestamp */
    const char* status_message;         /**< Human-readable status */
} security_module_status_t;

/**
 * WHAT: Overall facade status
 * WHY: Provide comprehensive view of security subsystem health
 * HOW: Aggregate status from all modules
 */
typedef struct {
    security_facade_state_t state;      /**< Current facade state */

    /* Module counts */
    uint32_t total_modules;             /**< Total modules in facade */
    uint32_t enabled_modules;           /**< Currently enabled modules */
    uint32_t initialized_modules;       /**< Successfully initialized modules */
    uint32_t healthy_modules;           /**< Healthy modules */
    uint32_t error_modules;             /**< Modules in error state */

    /* Threat assessment */
    float unified_threat_level;         /**< Combined threat level [0, 1] */
    security_severity_t severity;       /**< Overall severity */
    uint32_t active_threats;            /**< Number of active threats */

    /* Lockdown status */
    bool lockdown_active;               /**< Whether in lockdown */
    uint64_t lockdown_start_ms;         /**< When lockdown started */
    const char* lockdown_reason;        /**< Reason for lockdown */

    /* Timestamps */
    uint64_t creation_time_ms;          /**< When facade was created */
    uint64_t init_time_ms;              /**< When facade was initialized */
    uint64_t last_update_ms;            /**< Last status update */
    uint64_t uptime_ms;                 /**< Facade uptime */

    /* Per-module status (array of size total_modules) */
    security_module_status_t* module_status; /**< Individual module status */
    uint32_t module_status_count;            /**< Number of module status entries */
} security_facade_status_t;

/**
 * WHAT: Statistics for security facade operation
 * WHY: Monitor facade performance and effectiveness
 * HOW: Counters and metrics collected during operation
 */
typedef struct {
    /* Event statistics */
    uint64_t events_received;           /**< Total events received */
    uint64_t events_processed;          /**< Events successfully processed */
    uint64_t events_dropped;            /**< Events dropped (queue full) */
    uint64_t events_pending;            /**< Events pending processing */

    /* Threat statistics */
    uint64_t threats_detected;          /**< Total threats detected */
    uint64_t threats_mitigated;         /**< Threats successfully mitigated */
    uint64_t false_positives;           /**< Reported false positives */
    float avg_threat_level;             /**< Average threat level */
    float peak_threat_level;            /**< Peak threat level */

    /* Lockdown statistics */
    uint32_t lockdowns_triggered;       /**< Total lockdowns triggered */
    uint64_t total_lockdown_time_ms;    /**< Total time in lockdown */
    uint64_t avg_lockdown_duration_ms;  /**< Average lockdown duration */

    /* Performance statistics */
    float avg_processing_time_us;       /**< Average event processing time */
    float avg_threat_assessment_us;     /**< Average threat assessment time */
    float max_processing_time_us;       /**< Maximum processing time */

    /* Module statistics */
    uint32_t module_init_failures;      /**< Module initialization failures */
    uint32_t module_recoveries;         /**< Successful module recoveries */
    uint64_t inter_module_events;       /**< Events routed between modules */

    /* Timing */
    uint64_t uptime_ms;                 /**< Facade uptime */
    uint64_t last_reset_ms;             /**< When stats were last reset */
} security_facade_stats_t;

/* ============================================================================
 * DEFAULT CONFIGURATION
 * ============================================================================ */

/**
 * WHAT: Get default facade configuration
 * WHY: Provide sensible defaults for common use cases
 * HOW: Return pre-configured struct with all features enabled
 *
 * DEFAULT VALUES:
 * - enable_all_bridges: true
 * - enable_all_fep_bridges: true
 * - enable_cognitive_hub: true
 * - alert_threshold: 0.6
 * - lockdown_threshold: 0.9
 * - threat_decay_rate: 0.05
 * - enable_async_processing: true
 * - event_batch_size: 64
 * - processing_interval_ms: 100
 *
 * @param config Output: configuration structure
 * @return 0 on success, error code on failure
 */
int security_facade_default_config(security_facade_config_t* config);

/* ============================================================================
 * LIFECYCLE MANAGEMENT
 * ============================================================================ */

/**
 * WHAT: Create a new security facade
 * WHY: Initialize facade for managing security subsystem
 * HOW: Allocate resources, create orchestrator and bridges
 *
 * @param config Facade configuration (NULL for defaults)
 * @return Facade handle, or NULL on error
 *
 * ERRORS:
 * - Returns NULL if memory allocation fails
 * - Returns NULL if orchestrator creation fails
 *
 * MEMORY: Caller must call security_facade_destroy() when done
 *
 * NOTE: After creation, call security_facade_init_all() to initialize
 *       all security components.
 */
security_facade_t security_facade_create(const security_facade_config_t* config);

/**
 * WHAT: Destroy security facade
 * WHY: Release all resources and cleanup
 * HOW: Shutdown modules, destroy bridges, free memory
 *
 * @param facade Facade to destroy
 *
 * BLOCKING: May block waiting for pending events to process
 * SAFETY: Safe to call with NULL
 *
 * NOTE: Automatically calls security_facade_shutdown() if needed
 */
void security_facade_destroy(security_facade_t facade);

/**
 * WHAT: Initialize all security components
 * WHY: Start up entire security subsystem in correct order
 * HOW: Initialize orchestrator, bridges, FEP bridges, cognitive hub
 *
 * @param facade Security facade
 * @return 0 on success, error code on failure
 *
 * INITIALIZATION ORDER:
 * 1. Security Orchestrator
 * 2. Core security bridges (distributed training, KG, etc.)
 * 3. FEP bridges
 * 4. Security-Cognitive Hub Bridge
 * 5. Connect all components
 *
 * ERRORS:
 * - NIMCP_ERROR_NULL_POINTER if facade is NULL
 * - NIMCP_ERROR_INVALID_STATE if already initialized
 * - Module-specific errors propagated from individual init functions
 *
 * NOTE: Partial initialization is rolled back on failure
 */
int security_facade_init_all(security_facade_t facade);

/**
 * WHAT: Gracefully shutdown security subsystem
 * WHY: Stop all security modules in correct order
 * HOW: Drain events, disconnect modules, shutdown in reverse order
 *
 * @param facade Security facade
 * @return 0 on success, error code on failure
 *
 * SHUTDOWN ORDER (reverse of init):
 * 1. Drain pending events
 * 2. Disconnect cognitive hub
 * 3. Shutdown FEP bridges
 * 4. Shutdown security bridges
 * 5. Shutdown orchestrator
 *
 * BLOCKING: May block waiting for event drain
 */
int security_facade_shutdown(security_facade_t facade);

/**
 * WHAT: Reset facade to initial state
 * WHY: Clear all state without destroying facade
 * HOW: Reset orchestrator, clear threats, reset statistics
 *
 * @param facade Security facade
 * @return 0 on success, error code on failure
 */
int security_facade_reset(security_facade_t facade);

/* ============================================================================
 * MODULE MANAGEMENT
 * ============================================================================ */

/**
 * WHAT: Enable a security module
 * WHY: Dynamically activate security modules at runtime
 * HOW: Initialize and connect module if not already active
 *
 * @param facade Security facade
 * @param module_id Module to enable
 * @return 0 on success, error code on failure
 */
int security_facade_enable_module(
    security_facade_t facade,
    security_module_id_t module_id
);

/**
 * WHAT: Disable a security module
 * WHY: Dynamically deactivate security modules at runtime
 * HOW: Disconnect and shutdown module
 *
 * @param facade Security facade
 * @param module_id Module to disable
 * @return 0 on success, error code on failure
 *
 * NOTE: Core modules (orchestrator) cannot be disabled
 */
int security_facade_disable_module(
    security_facade_t facade,
    security_module_id_t module_id
);

/**
 * WHAT: Check if a module is enabled
 * WHY: Query module activation state
 * HOW: Return module enabled flag
 *
 * @param facade Security facade
 * @param module_id Module to check
 * @param enabled Output: true if enabled
 * @return 0 on success, error code on failure
 */
int security_facade_is_module_enabled(
    security_facade_t facade,
    security_module_id_t module_id,
    bool* enabled
);

/**
 * WHAT: Get status of a specific module
 * WHY: Query individual module health and activity
 * HOW: Copy module status to output structure
 *
 * @param facade Security facade
 * @param module_id Module to query
 * @param status Output: module status
 * @return 0 on success, error code on failure
 */
int security_facade_get_module_status(
    security_facade_t facade,
    security_module_id_t module_id,
    security_module_status_t* status
);

/* ============================================================================
 * THREAT ASSESSMENT
 * ============================================================================ */

/**
 * WHAT: Get unified threat level from all security sources
 * WHY: Aggregate threat information for decision making
 * HOW: Weighted combination of all module threat levels
 *
 * @param facade Security facade
 * @param threat_level Output: unified threat level [0, 1]
 * @return 0 on success, error code on failure
 *
 * THREAT LEVEL INTERPRETATION:
 * - [0.0, 0.3): Normal operation
 * - [0.3, 0.6): Elevated monitoring
 * - [0.6, 0.8): Alert state
 * - [0.8, 0.9): High alert
 * - [0.9, 1.0]: Critical / Lockdown
 */
int security_facade_get_threat_level(
    security_facade_t facade,
    float* threat_level
);

/**
 * WHAT: Get detailed threat assessment from all modules
 * WHY: Provide comprehensive threat analysis
 * HOW: Query orchestrator for full threat assessment
 *
 * @param facade Security facade
 * @param assessment Output: detailed threat assessment
 * @return 0 on success, error code on failure
 */
int security_facade_get_threat_assessment(
    security_facade_t facade,
    security_threat_assessment_t* assessment
);

/**
 * WHAT: Report a threat to the facade
 * WHY: Allow external systems to report threats
 * HOW: Route threat to orchestrator for processing
 *
 * @param facade Security facade
 * @param source_module Module reporting the threat
 * @param threat_level Threat level [0, 1]
 * @param severity Threat severity
 * @param description Human-readable description
 * @return 0 on success, error code on failure
 */
int security_facade_report_threat(
    security_facade_t facade,
    security_module_id_t source_module,
    float threat_level,
    security_severity_t severity,
    const char* description
);

/**
 * WHAT: Clear all active threats
 * WHY: Reset threat state after incident resolution
 * HOW: Clear threats in orchestrator and all modules
 *
 * @param facade Security facade
 * @return 0 on success, error code on failure
 */
int security_facade_clear_threats(security_facade_t facade);

/* ============================================================================
 * EVENT PROCESSING
 * ============================================================================ */

/**
 * WHAT: Process pending security events
 * WHY: Handle queued security events from all modules
 * HOW: Process events in batch, update threat levels
 *
 * @param facade Security facade
 * @return Number of events processed, or negative on error
 *
 * USAGE: Call this periodically in main loop for event-driven systems
 *
 * NOTE: With async processing enabled, this drains the async queue.
 *       Without async, this is a no-op (events processed synchronously).
 */
int security_facade_process_events(security_facade_t facade);

/**
 * WHAT: Subscribe to security events
 * WHY: Allow external systems to receive security notifications
 * HOW: Register callback with orchestrator
 *
 * @param facade Security facade
 * @param event_type Type of events to subscribe to
 * @param callback Function to call when event occurs
 * @param user_data User-provided context passed to callback
 * @return 0 on success, error code on failure
 */
int security_facade_subscribe(
    security_facade_t facade,
    security_event_type_t event_type,
    security_event_callback_t callback,
    void* user_data
);

/**
 * WHAT: Unsubscribe from security events
 * WHY: Stop receiving notifications for events
 * HOW: Remove callback registration from orchestrator
 *
 * @param facade Security facade
 * @param event_type Type of events to unsubscribe from
 * @return 0 on success, error code on failure
 */
int security_facade_unsubscribe(
    security_facade_t facade,
    security_event_type_t event_type
);

/* ============================================================================
 * LOCKDOWN CONTROL
 * ============================================================================ */

/**
 * WHAT: Trigger system lockdown
 * WHY: Respond to critical threats by restricting operations
 * HOW: Notify all modules to enter lockdown mode
 *
 * @param facade Security facade
 * @param reason Reason for lockdown
 * @return 0 on success, error code on failure
 */
int security_facade_trigger_lockdown(
    security_facade_t facade,
    const char* reason
);

/**
 * WHAT: Release system lockdown
 * WHY: Return to normal operation after threat cleared
 * HOW: Notify all modules to exit lockdown mode
 *
 * @param facade Security facade
 * @return 0 on success, error code on failure
 */
int security_facade_release_lockdown(security_facade_t facade);

/**
 * WHAT: Check if system is in lockdown
 * WHY: Query lockdown state for decision making
 * HOW: Return current lockdown status
 *
 * @param facade Security facade
 * @param is_locked Output: true if in lockdown
 * @return 0 on success, error code on failure
 */
int security_facade_is_locked_down(
    security_facade_t facade,
    bool* is_locked
);

/* ============================================================================
 * STATUS AND STATISTICS
 * ============================================================================ */

/**
 * WHAT: Get overall facade status
 * WHY: Comprehensive view of security subsystem health
 * HOW: Aggregate status from all modules
 *
 * @param facade Security facade
 * @param status Output: facade status
 * @return 0 on success, error code on failure
 *
 * MEMORY: Caller must call security_facade_free_status() to free
 *         dynamically allocated module_status array
 */
int security_facade_get_status(
    security_facade_t facade,
    security_facade_status_t* status
);

/**
 * WHAT: Free status structure resources
 * WHY: Release dynamically allocated memory in status
 * HOW: Free module_status array
 *
 * @param status Status structure to free
 */
void security_facade_free_status(security_facade_status_t* status);

/**
 * WHAT: Get facade state
 * WHY: Quick state check without full status
 * HOW: Return current state enum
 *
 * @param facade Security facade
 * @param state Output: current state
 * @return 0 on success, error code on failure
 */
int security_facade_get_state(
    security_facade_t facade,
    security_facade_state_t* state
);

/**
 * WHAT: Get facade statistics
 * WHY: Monitor facade performance
 * HOW: Copy accumulated statistics
 *
 * @param facade Security facade
 * @param stats Output: statistics
 * @return 0 on success, error code on failure
 */
int security_facade_get_stats(
    security_facade_t facade,
    security_facade_stats_t* stats
);

/**
 * WHAT: Reset facade statistics
 * WHY: Clear statistics for fresh measurement
 * HOW: Zero all statistic counters
 *
 * @param facade Security facade
 * @return 0 on success, error code on failure
 */
int security_facade_reset_stats(security_facade_t facade);

/* ============================================================================
 * INTEGRATION CONNECTIONS
 * ============================================================================ */

/**
 * WHAT: Connect facade to cognitive hub
 * WHY: Enable security-cognitive coordination
 * HOW: Connect through security-cognitive hub bridge
 *
 * @param facade Security facade
 * @param cognitive_hub Cognitive hub handle
 * @return 0 on success, error code on failure
 */
int security_facade_connect_cognitive_hub(
    security_facade_t facade,
    void* cognitive_hub
);

/**
 * WHAT: Connect facade to immune system
 * WHY: Enable security-immune coordination
 * HOW: Connect through orchestrator
 *
 * @param facade Security facade
 * @param immune_system Immune system handle
 * @return 0 on success, error code on failure
 */
int security_facade_connect_immune_system(
    security_facade_t facade,
    void* immune_system
);

/**
 * WHAT: Connect facade to bio-async router
 * WHY: Enable event-driven async integration
 * HOW: Register all modules as bio-async modules
 *
 * @param facade Security facade
 * @return 0 on success, error code on failure
 */
int security_facade_connect_bio_async(security_facade_t facade);

/**
 * WHAT: Disconnect facade from bio-async router
 * WHY: Cleanup bio-async integration
 * HOW: Unregister all modules from bio-async
 *
 * @param facade Security facade
 * @return 0 on success, error code on failure
 */
int security_facade_disconnect_bio_async(security_facade_t facade);

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================ */

/**
 * WHAT: Get string name for module ID
 * WHY: Human-readable module identification
 * HOW: Lookup table
 *
 * @param module_id Module identifier
 * @return String name (never NULL)
 */
const char* security_module_name(security_module_id_t module_id);

/**
 * WHAT: Get string name for facade state
 * WHY: Human-readable state identification
 * HOW: Lookup table
 *
 * @param state Facade state
 * @return String name (never NULL)
 */
const char* security_facade_state_name(security_facade_state_t state);

/**
 * WHAT: Print facade summary to stdout
 * WHY: Debug and diagnostic output
 * HOW: Format and print facade state
 *
 * @param facade Security facade (NULL safe)
 */
void security_facade_print_summary(security_facade_t facade);

/**
 * WHAT: Print facade statistics to stdout
 * WHY: Debug and diagnostic output
 * HOW: Format and print statistics
 *
 * @param stats Statistics to print (NULL safe)
 */
void security_facade_print_stats(const security_facade_stats_t* stats);

/**
 * WHAT: Get facade version string
 * WHY: Version information for diagnostics
 * HOW: Return static version string
 *
 * @return Version string
 */
const char* security_facade_version(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SECURITY_FACADE_H */
