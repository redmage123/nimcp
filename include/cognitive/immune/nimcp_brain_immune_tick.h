/**
 * @file nimcp_brain_immune_tick.h
 * @brief Brain Immune System Tick Orchestrator
 * @version 1.0.0
 * @date 2025-01-25
 *
 * WHAT: Unified tick function that sequences all immune processing
 * WHY:  Exceptions and health agent messages need periodic processing;
 *       brain_immune_update() was only called from Python, not C code
 * HOW:  Orchestrates exception processing, health queue consumption,
 *       and core immune update in a single tick
 *
 * ARCHITECTURE:
 * ```
 * brain_immune_tick(immune, delta_ms)
 *     |
 *     +-> 1. nimcp_exception_immune_process_pending()  // Async exceptions
 *     |
 *     +-> 2. brain_immune_process_health_queue()       // Health agent messages
 *     |
 *     +-> 3. brain_immune_update()                     // Core immune processing
 *             +-> process_pending_antigens()
 *             +-> decay_antibodies()
 *             +-> update_inflammation_sites()
 *             +-> update_immune_phase()
 * ```
 *
 * HEALTH MESSAGE -> IMMUNE ACTION MAPPING:
 * - HEALTH_MSG_ANOMALY_DETECTED   -> Present as antigen (severity from msg)
 * - HEALTH_MSG_CYTOKINE_SIGNAL    -> Release cytokine (type from msg data)
 * - HEALTH_MSG_EMERGENCY          -> Initiate inflammation + emergency recovery
 * - HEALTH_MSG_RECOVERY_REQUEST   -> Execute suggested recovery action
 * - HEALTH_MSG_STATE_CORRUPTION   -> Antigen + request rollback
 * - HEALTH_MSG_MEMORY_CORRUPTION  -> Antigen + quarantine region
 * - HEALTH_MSG_NAN_DETECTED       -> Antigen + clear neural caches
 * - HEALTH_MSG_DEADLOCK_DETECTED  -> Antigen + restart affected threads
 * - HEALTH_MSG_HEARTBEAT_TIMEOUT  -> High-severity antigen + emergency save
 * - HEALTH_MSG_RESOURCE_EXHAUSTION-> Antigen + reduce load + GC
 *
 * THREAD SAFETY:
 * - Uses thread-local reentry guard to prevent recursive tick calls
 * - Safe to call from health agent thread or application event loop
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_BRAIN_IMMUNE_TICK_H
#define NIMCP_BRAIN_IMMUNE_TICK_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

/* Brain immune system forward declaration */
#ifndef NIMCP_BRAIN_IMMUNE_H
struct brain_immune_system;
typedef struct brain_immune_system brain_immune_system_t;
#endif

/* Health agent forward declaration */
#ifndef NIMCP_HEALTH_AGENT_H
typedef struct nimcp_health_agent nimcp_health_agent_t;
typedef struct health_agent_message health_agent_message_t;
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Default maximum exceptions per tick (0 = unlimited) */
#define BRAIN_IMMUNE_TICK_DEFAULT_MAX_EXCEPTIONS     10

/** Default maximum health messages per tick (0 = unlimited) */
#define BRAIN_IMMUNE_TICK_DEFAULT_MAX_HEALTH_MSGS    20

/** Default tick interval in milliseconds */
#define BRAIN_IMMUNE_TICK_DEFAULT_INTERVAL_MS        50

/** Module name for logging */
#define BRAIN_IMMUNE_TICK_MODULE_NAME                "brain_immune_tick"

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Brain immune tick configuration
 */
typedef struct {
    /** Maximum exceptions to process per tick (0 = all pending) */
    uint32_t max_exceptions_per_tick;

    /** Maximum health messages to process per tick (0 = all pending) */
    uint32_t max_health_msgs_per_tick;

    /** Enable exception queue processing */
    bool enable_exception_processing;

    /** Enable health agent message processing */
    bool enable_health_agent_processing;

    /** Enable antigen processing in brain_immune_update */
    bool enable_antigen_processing;

    /** Enable antibody decay in brain_immune_update */
    bool enable_antibody_decay;

    /** Enable inflammation updates in brain_immune_update */
    bool enable_inflammation_updates;

    /** Log tick activity (for debugging) */
    bool enable_tick_logging;
} brain_immune_tick_config_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Brain immune tick statistics
 */
typedef struct {
    /** Total ticks executed */
    uint64_t ticks_executed;

    /** Total exceptions processed */
    uint64_t exceptions_processed;

    /** Total health messages processed */
    uint64_t health_messages_processed;

    /** Total antigens created from health messages */
    uint64_t health_antigens_created;

    /** Total cytokines released from health messages */
    uint64_t health_cytokines_released;

    /** Total recovery actions triggered */
    uint64_t recovery_actions_triggered;

    /** Successful recovery actions */
    uint64_t recovery_actions_succeeded;

    /** Reentrant tick calls blocked */
    uint64_t reentrant_calls_blocked;

    /** Average tick duration in microseconds */
    float avg_tick_duration_us;

    /** Maximum tick duration in microseconds */
    uint64_t max_tick_duration_us;

    /** Ticks with no work */
    uint64_t idle_ticks;
} brain_immune_tick_stats_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default tick configuration
 *
 * WHAT: Provide sensible default tick configuration
 * WHY:  Easy initialization with balanced parameters
 * HOW:  Return struct with default values
 *
 * @param config Output configuration (must not be NULL)
 */
void brain_immune_tick_default_config(brain_immune_tick_config_t* config);

/**
 * @brief Initialize brain immune tick orchestrator
 *
 * WHAT: Set up tick processing state for an immune system
 * WHY:  Enable periodic immune processing from C code
 * HOW:  Store configuration, initialize stats, set up state
 *
 * Must be called before brain_immune_tick().
 *
 * @param immune Brain immune system (must not be NULL)
 * @param config Tick configuration (NULL for defaults)
 * @return 0 on success, -1 on error
 */
int brain_immune_tick_init(brain_immune_system_t* immune,
                           const brain_immune_tick_config_t* config);

/**
 * @brief Connect health agent to immune tick orchestrator
 *
 * WHAT: Link health agent's message queue to immune tick
 * WHY:  Enable health agent messages to be processed as immune events
 * HOW:  Store reference, enable health message processing
 *
 * @param immune Brain immune system
 * @param agent Health agent (NULL to disconnect)
 * @return 0 on success, -1 on error
 */
int brain_immune_tick_connect_health_agent(brain_immune_system_t* immune,
                                            nimcp_health_agent_t* agent);

/**
 * @brief Shutdown brain immune tick orchestrator
 *
 * WHAT: Clean up tick processing state
 * WHY:  Proper shutdown and resource cleanup
 * HOW:  Clear references, log final stats
 *
 * @param immune Brain immune system
 */
void brain_immune_tick_shutdown(brain_immune_system_t* immune);

/* ============================================================================
 * Core Tick API
 * ============================================================================ */

/**
 * @brief Execute one immune system tick
 *
 * WHAT: Process pending exceptions, health messages, and run immune update
 * WHY:  Unified entry point for all immune processing
 * HOW:  Sequence exception processing, health queue, and core update
 *
 * Call periodically (50-100ms recommended) from:
 * - Health agent thread (Option A - recommended)
 * - Application event loop (Option B)
 * - Hemispheric brain update (Option C)
 *
 * Thread-safe with reentry protection - recursive calls are safely ignored.
 *
 * @param immune Brain immune system
 * @param delta_ms Time since last tick in milliseconds
 * @return 0 on success, -1 on error, 1 if reentrant call blocked
 */
int brain_immune_tick(brain_immune_system_t* immune, uint64_t delta_ms);

/* ============================================================================
 * Health Message Processing API
 * ============================================================================ */

/**
 * @brief Process a single health agent message
 *
 * WHAT: Convert health agent message to immune system action
 * WHY:  Health messages represent detected threats/anomalies
 * HOW:  Map message type to immune action (antigen, cytokine, etc.)
 *
 * @param immune Brain immune system
 * @param msg Health agent message
 * @return 0 on success, -1 on error
 */
int brain_immune_process_health_message(brain_immune_system_t* immune,
                                         const health_agent_message_t* msg);

/**
 * @brief Process pending health messages from connected agent
 *
 * WHAT: Drain health agent message queue and process messages
 * WHY:  Batch processing of health messages for efficiency
 * HOW:  Dequeue messages up to max_count and process each
 *
 * @param immune Brain immune system
 * @param max_count Maximum messages to process (0 = all)
 * @return Number of messages processed, -1 on error
 */
int brain_immune_process_health_queue(brain_immune_system_t* immune,
                                       size_t max_count);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Check if tick orchestrator is initialized
 *
 * @param immune Brain immune system
 * @return true if initialized and ready for ticks
 */
bool brain_immune_tick_is_initialized(const brain_immune_system_t* immune);

/**
 * @brief Check if health agent is connected
 *
 * @param immune Brain immune system
 * @return true if a health agent is connected
 */
bool brain_immune_tick_has_health_agent(const brain_immune_system_t* immune);

/**
 * @brief Get tick statistics
 *
 * @param immune Brain immune system
 * @param stats Output statistics (must not be NULL)
 * @return 0 on success, -1 on error
 */
int brain_immune_tick_get_stats(const brain_immune_system_t* immune,
                                 brain_immune_tick_stats_t* stats);

/**
 * @brief Reset tick statistics
 *
 * @param immune Brain immune system
 */
void brain_immune_tick_reset_stats(brain_immune_system_t* immune);

/**
 * @brief Get current tick configuration
 *
 * @param immune Brain immune system
 * @param config Output configuration (must not be NULL)
 * @return 0 on success, -1 on error
 */
int brain_immune_tick_get_config(const brain_immune_system_t* immune,
                                  brain_immune_tick_config_t* config);

/**
 * @brief Update tick configuration at runtime
 *
 * @param immune Brain immune system
 * @param config New configuration
 * @return 0 on success, -1 on error
 */
int brain_immune_tick_set_config(brain_immune_system_t* immune,
                                  const brain_immune_tick_config_t* config);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Check if currently inside a tick (for reentry detection)
 *
 * Thread-local check - returns true if calling thread is executing tick.
 *
 * @return true if inside tick execution
 */
bool brain_immune_tick_in_progress(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_IMMUNE_TICK_H */
