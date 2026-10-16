/**
 * @file nimcp_brain_immune_integration.h
 * @brief Full integration helper for exception-immune system setup
 * @version 1.0.0
 * @date 2025-01-25
 *
 * WHAT: Helper functions for complete exception-immune integration setup
 * WHY:  Setting up the full exception → immune → recovery pipeline requires
 *       many steps that are easy to miss or get wrong
 * HOW:  Single function call performs all initialization in correct order
 *
 * PROBLEM SOLVED:
 * Without this helper, users must manually:
 * 1. nimcp_exception_system_init()
 * 2. nimcp_install_default_handlers()
 * 3. brain_immune_create()
 * 4. brain_immune_start()
 * 5. nimcp_exception_immune_init()
 * 6. nimcp_exception_immune_connect()
 * 7. brain_immune_tick_init()
 * 8. nimcp_exception_install_default_recovery_callbacks()
 * 9. Optionally: brain_immune_tick_connect_health_agent()
 *
 * Missing any step causes silent failures:
 * - Missing step 2: SEVERE+ exceptions not auto-presented
 * - Missing step 4: brain_immune_update() returns -1
 * - Missing step 6: Antigens never reach immune system
 * - Missing step 7: Async queue never processed
 *
 * USAGE:
 * ```c
 * // Simple full setup
 * nimcp_immune_integration_t* integration = nimcp_immune_integration_create(NULL);
 * if (!integration) { handle_error(); }
 *
 * // In your event loop
 * while (running) {
 *     nimcp_immune_integration_tick(integration, delta_ms);
 *     // ... other work
 * }
 *
 * // Cleanup
 * nimcp_immune_integration_destroy(integration);
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_BRAIN_IMMUNE_INTEGRATION_H
#define NIMCP_BRAIN_IMMUNE_INTEGRATION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

/* Brain immune system */
#ifndef NIMCP_BRAIN_IMMUNE_H
struct brain_immune_system;
typedef struct brain_immune_system brain_immune_system_t;
#endif

/* Health agent */
#ifndef NIMCP_HEALTH_AGENT_H
typedef struct nimcp_health_agent nimcp_health_agent_t;
#endif

/* Brain instance for recovery context */
#ifndef NIMCP_BRAIN_H
typedef struct brain_struct* brain_t;
#endif

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Integration configuration options
 */
typedef struct {
    /* Exception system options */
    bool install_default_handlers;     /**< Install logging + immune handlers (default: true) */
    bool install_recovery_callbacks;   /**< Install default recovery callbacks (default: true) */

    /* Immune system options */
    bool create_immune_system;         /**< Create immune system internally (default: true) */
    brain_immune_system_t* external_immune; /**< Use external immune system (if create = false) */

    /* Tick orchestrator options */
    bool init_tick_orchestrator;       /**< Initialize tick orchestrator (default: true) */
    uint32_t max_exceptions_per_tick;  /**< Max exceptions per tick (default: 10) */
    uint32_t max_health_msgs_per_tick; /**< Max health messages per tick (default: 20) */

    /* Health agent options */
    nimcp_health_agent_t* health_agent; /**< Health agent to connect (NULL = none) */

    /* Recovery context (optional) */
    brain_t brain;                     /**< Brain for recovery operations */
    void* gc_context;                  /**< GC context for garbage collection */
    void* bbb_system;                  /**< BBB system for quarantine */
    void* runtime_adaptation;          /**< Runtime adaptation for load reduction */
    const char* checkpoint_dir;        /**< Directory for checkpoints */

    /* Behavior options */
    bool auto_start;                   /**< Auto-start immune system (default: true) */
    bool enable_logging;               /**< Enable integration logging (default: true) */
} nimcp_immune_integration_config_t;

/**
 * @brief Integration instance handle
 */
typedef struct nimcp_immune_integration nimcp_immune_integration_t;

/**
 * @brief Integration statistics
 */
typedef struct {
    /* Lifecycle */
    uint64_t ticks_executed;           /**< Total tick calls */
    uint64_t uptime_ms;                /**< Time since creation */

    /* Exception flow */
    uint64_t exceptions_handled;       /**< Via exception handlers */
    uint64_t exceptions_presented;     /**< Presented to immune */
    uint64_t exceptions_async_queued;  /**< Queued for async processing */

    /* Immune activity */
    uint64_t antigens_created;         /**< Antigens from exceptions */
    uint64_t health_msgs_processed;    /**< Health agent messages */
    uint64_t recoveries_triggered;     /**< Recovery actions triggered */
    uint64_t recoveries_succeeded;     /**< Successful recoveries */

    /* Performance */
    float avg_tick_duration_us;        /**< Average tick duration */
    uint64_t max_tick_duration_us;     /**< Maximum tick duration */
} nimcp_immune_integration_stats_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default integration configuration
 *
 * WHAT: Provide sensible defaults for full integration
 * WHY:  Most users want complete setup with defaults
 * HOW:  Enable all features, use default limits
 *
 * @param config Output configuration (must not be NULL)
 */
void nimcp_immune_integration_default_config(nimcp_immune_integration_config_t* config);

/**
 * @brief Create and initialize full exception-immune integration
 *
 * WHAT: Perform complete setup of exception → immune → recovery pipeline
 * WHY:  Eliminates common setup mistakes and forgotten steps
 * HOW:  Execute all initialization steps in correct order
 *
 * This function performs ALL necessary setup:
 * 1. Initialize exception system
 * 2. Install default exception handlers (logging + immune presentation)
 * 3. Create brain immune system (or use provided external one)
 * 4. Start immune system
 * 5. Initialize exception-immune integration
 * 6. Connect immune system to exception system
 * 7. Initialize tick orchestrator
 * 8. Install default recovery callbacks
 * 9. Connect health agent (if provided)
 * 10. Set up recovery context (if provided)
 *
 * @param config Configuration (NULL for defaults)
 * @return Integration handle or NULL on failure
 */
nimcp_immune_integration_t* nimcp_immune_integration_create(
    const nimcp_immune_integration_config_t* config
);

/**
 * @brief Destroy integration and clean up all resources
 *
 * WHAT: Clean shutdown of entire exception-immune pipeline
 * WHY:  Proper resource cleanup in correct order
 * HOW:  Shutdown in reverse order of initialization
 *
 * @param integration Integration to destroy (NULL-safe)
 */
void nimcp_immune_integration_destroy(nimcp_immune_integration_t* integration);

/**
 * @brief Start the immune system (if not auto-started)
 *
 * @param integration Integration handle
 * @return 0 on success, -1 on error
 */
int nimcp_immune_integration_start(nimcp_immune_integration_t* integration);

/**
 * @brief Stop the immune system
 *
 * @param integration Integration handle
 * @return 0 on success, -1 on error
 */
int nimcp_immune_integration_stop(nimcp_immune_integration_t* integration);

/* ============================================================================
 * Runtime API
 * ============================================================================ */

/**
 * @brief Execute one integration tick
 *
 * WHAT: Process pending exceptions, health messages, and update immune system
 * WHY:  Central point for all periodic immune processing
 * HOW:  Delegates to brain_immune_tick()
 *
 * Call this periodically (every 50-100ms recommended) from:
 * - Your application's main loop
 * - A dedicated timer thread
 * - The health agent's monitoring thread
 *
 * @param integration Integration handle
 * @param delta_ms Time since last tick in milliseconds
 * @return 0 on success, -1 on error
 */
int nimcp_immune_integration_tick(
    nimcp_immune_integration_t* integration,
    uint64_t delta_ms
);

/**
 * @brief Connect health agent at runtime
 *
 * @param integration Integration handle
 * @param agent Health agent to connect (NULL to disconnect)
 * @return 0 on success, -1 on error
 */
int nimcp_immune_integration_connect_health_agent(
    nimcp_immune_integration_t* integration,
    nimcp_health_agent_t* agent
);

/**
 * @brief Update recovery context at runtime
 *
 * @param integration Integration handle
 * @param brain Brain instance
 * @param gc_context GC context
 * @param bbb_system BBB system
 * @param ra_ctx Runtime adaptation context
 * @param checkpoint_dir Checkpoint directory
 * @return 0 on success, -1 on error
 */
int nimcp_immune_integration_set_recovery_context(
    nimcp_immune_integration_t* integration,
    brain_t brain,
    void* gc_context,
    void* bbb_system,
    void* ra_ctx,
    const char* checkpoint_dir
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get the underlying brain immune system
 *
 * @param integration Integration handle
 * @return Brain immune system or NULL if not available
 */
brain_immune_system_t* nimcp_immune_integration_get_immune_system(
    nimcp_immune_integration_t* integration
);

/**
 * @brief Check if integration is running
 *
 * @param integration Integration handle
 * @return true if immune system is started and running
 */
bool nimcp_immune_integration_is_running(
    const nimcp_immune_integration_t* integration
);

/**
 * @brief Get integration statistics
 *
 * @param integration Integration handle
 * @param stats Output statistics (must not be NULL)
 * @return 0 on success, -1 on error
 */
int nimcp_immune_integration_get_stats(
    const nimcp_immune_integration_t* integration,
    nimcp_immune_integration_stats_t* stats
);

/**
 * @brief Reset integration statistics
 *
 * @param integration Integration handle
 */
void nimcp_immune_integration_reset_stats(nimcp_immune_integration_t* integration);

/* ============================================================================
 * Diagnostic API
 * ============================================================================ */

/**
 * @brief Check integration health and report issues
 *
 * WHAT: Verify all components are properly connected and working
 * WHY:  Diagnose silent failures in the exception-immune pipeline
 * HOW:  Check each component's state and report issues
 *
 * @param integration Integration handle
 * @param buffer Output buffer for diagnostic message
 * @param buffer_size Buffer size
 * @return Number of issues found (0 = healthy)
 */
int nimcp_immune_integration_diagnose(
    const nimcp_immune_integration_t* integration,
    char* buffer,
    size_t buffer_size
);

/**
 * @brief Log current integration state
 *
 * @param integration Integration handle
 */
void nimcp_immune_integration_log_state(
    const nimcp_immune_integration_t* integration
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_IMMUNE_INTEGRATION_H */
