/**
 * @file nimcp_recovery.h
 * @brief Intelligent recovery strategies for NIMCP fault tolerance system
 *
 * WHAT: Automated recovery mechanisms for handling brain failures and degradation
 * WHY:  Production systems need self-healing to maintain uptime and reliability
 * HOW:  Multi-tier recovery strategies from immediate fixes to strategic fallbacks
 *
 * RECOVERY ARCHITECTURE:
 * - **Immediate**: Quick fixes in <1ms (clear NaN, reset counter)
 * - **Tactical**: Medium effort <100ms (reload checkpoint, adjust parameters)
 * - **Strategic**: Major changes <1s (fallback CPU, reduce model size)
 * - **Preventive**: Avoid future issues (increase memory limits, GC)
 *
 * SELF-HEALING CAPABILITIES:
 * - Automatic NaN/Inf detection and correction
 * - Learning rate adjustment on divergence
 * - Auto-restart failed operations with exponential backoff
 * - Memory compaction on fragmentation
 * - Execution mode switching (GPU → CPU)
 *
 * INTEGRATION POINTS:
 * - Signal handler: Calls recovery on crash
 * - Health monitor: Triggers preventive recovery
 * - Checkpoint system: Enables state rollback
 * - Diagnostics: Guides strategy selection
 *
 * @author NIMCP Team
 * @date 2025-11-19
 */

#ifndef NIMCP_RECOVERY_H
#define NIMCP_RECOVERY_H

#include <stdint.h>
#include <stdbool.h>
#include <signal.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct brain_struct* brain_t;

//=============================================================================
// Recovery Strategy Types
//=============================================================================

/**
 * @brief Recovery tier classification
 */
typedef enum {
    RECOVERY_TIER_IMMEDIATE,   /**< Quick fixes <1ms (clear NaN, reset counter) */
    RECOVERY_TIER_TACTICAL,    /**< Medium effort <100ms (reload checkpoint, adjust params) */
    RECOVERY_TIER_STRATEGIC,   /**< Major changes <1s (fallback CPU, reduce model) */
    RECOVERY_TIER_PREVENTIVE   /**< Avoid future issues (increase memory limits, GC) */
} recovery_tier_t;

/**
 * @brief Recovery action types
 */
typedef enum {
    RECOVERY_ACTION_NONE = 0,

    // Immediate actions (Tier 1)
    RECOVERY_ACTION_CLEAR_NAN,        /**< Replace NaN/Inf with zeros */
    RECOVERY_ACTION_RESET_COUNTER,    /**< Reset iteration counter */
    RECOVERY_ACTION_FLUSH_CACHE,      /**< Clear temporary caches */
    RECOVERY_ACTION_RESET_FPU,        /**< Reset FPU exception flags */

    // Tactical actions (Tier 2)
    RECOVERY_ACTION_RELOAD_CHECKPOINT, /**< Rollback to last checkpoint */
    RECOVERY_ACTION_REDUCE_LR,        /**< Reduce learning rate */
    RECOVERY_ACTION_REDUCE_BATCH,     /**< Reduce batch size */
    RECOVERY_ACTION_TRIGGER_GC,       /**< Run garbage collection */
    RECOVERY_ACTION_RESTART_OP,       /**< Retry failed operation */

    // Strategic actions (Tier 3)
    RECOVERY_ACTION_FALLBACK_CPU,     /**< Switch from GPU to CPU */
    RECOVERY_ACTION_REDUCE_MODEL,     /**< Reduce model complexity */
    RECOVERY_ACTION_REINIT_LAYER,     /**< Reinitialize corrupted layer */
    RECOVERY_ACTION_EMERGENCY_SAVE,   /**< Save state before crash */

    // Preventive actions (Tier 4)
    RECOVERY_ACTION_INCREASE_MEMORY,  /**< Increase memory limits */
    RECOVERY_ACTION_COMPACT_MEMORY,   /**< Defragment memory */
    RECOVERY_ACTION_ENABLE_CHECKS,    /**< Enable extra validation */
    RECOVERY_ACTION_AUTO_CHECKPOINT   /**< Enable auto-checkpointing */
} recovery_action_t;

/**
 * @brief Parameter adjustment types
 */
typedef enum {
    ADJUSTMENT_LEARNING_RATE,    /**< Reduce learning rate by factor */
    ADJUSTMENT_BATCH_SIZE,       /**< Reduce batch size */
    ADJUSTMENT_MEMORY_LIMIT,     /**< Increase memory allocation */
    ADJUSTMENT_TIMEOUT,          /**< Increase operation timeout */
    ADJUSTMENT_PRECISION         /**< Switch to higher precision (f64) */
} adjustment_type_t;

/**
 * @brief Recovery result status
 */
typedef enum {
    RECOVERY_SUCCESS = 0,        /**< Recovery successful */
    RECOVERY_PARTIAL,            /**< Partial recovery (degraded mode) */
    RECOVERY_FAILED,             /**< Recovery failed */
    RECOVERY_NOT_APPLICABLE,     /**< Strategy not applicable to failure */
    RECOVERY_REQUIRES_RESTART    /**< Requires process restart */
} recovery_status_t;

//=============================================================================
// Diagnostic Result Structure (for strategy selection)
//=============================================================================

/**
 * @brief Diagnostic result from failure analysis
 */
typedef struct {
    int signal;                  /**< Signal that triggered recovery (SIGSEGV, etc.) */
    const char* failure_type;    /**< Type of failure (e.g., "memory", "numeric") */
    uint32_t severity;           /**< Severity level (1-10) */
    bool is_recoverable;         /**< Whether recovery is possible */
    void* context;               /**< Additional context data */
} diagnostic_summary_t;

/**
 * @brief Health status for preventive recovery
 */
typedef struct {
    float memory_usage;          /**< Memory usage percentage (0.0-1.0) */
    float cpu_usage;             /**< CPU usage percentage (0.0-1.0) */
    uint32_t error_count;        /**< Recent error count */
    uint32_t nan_count;          /**< NaN detections in last N steps */
    bool is_diverging;           /**< Learning divergence detected */
    float avg_loss;              /**< Average recent loss */
} health_state_t;

//=============================================================================
// Recovery Result Structure
//=============================================================================

/**
 * @brief Result of recovery operation
 */
typedef struct {
    recovery_status_t status;    /**< Recovery outcome */
    recovery_action_t action;    /**< Action taken */
    recovery_tier_t tier;        /**< Recovery tier used */
    uint32_t time_us;            /**< Recovery time in microseconds */
    bool requires_rollback;      /**< Whether rollback was needed */
    const char* message;         /**< Human-readable status message */
    float success_probability;   /**< Estimated success probability */
} recovery_result_t;

//=============================================================================
// Recovery Strategy Structure
//=============================================================================

/**
 * @brief Recovery strategy definition
 */
typedef struct {
    recovery_tier_t tier;           /**< Recovery tier */
    recovery_action_t primary;      /**< Primary action */
    recovery_action_t fallback;     /**< Fallback action if primary fails */
    uint32_t max_retries;           /**< Maximum retry attempts */
    uint32_t timeout_ms;            /**< Timeout for recovery */
    float success_threshold;        /**< Minimum success probability to attempt */
    const char* description;        /**< Strategy description */
} recovery_strategy_t;

//=============================================================================
// Circuit Breaker Pattern
//=============================================================================

/**
 * @brief Circuit breaker state
 */
typedef enum {
    CIRCUIT_CLOSED,      /**< Normal operation */
    CIRCUIT_OPEN,        /**< Too many failures, block operations */
    CIRCUIT_HALF_OPEN    /**< Testing if service recovered */
} circuit_state_t;

/**
 * @brief Circuit breaker for preventing cascading failures
 *
 * WHAT: Prevent repeated failed operations
 * WHY:  Stop trying operations that consistently fail
 * HOW:  Track failures, open circuit after threshold, test periodically
 */
typedef struct {
    circuit_state_t state;       /**< Current circuit state */
    uint32_t failure_count;      /**< Consecutive failure count */
    uint32_t success_count;      /**< Consecutive success count */
    uint32_t failure_threshold;  /**< Failures before opening circuit */
    uint32_t timeout_ms;         /**< Time before trying half-open */
    uint64_t last_failure_time;  /**< Timestamp of last failure (ms) */
    uint64_t last_attempt_time;  /**< Timestamp of last attempt (ms) */
    uint32_t total_failures;     /**< Total failures (for stats) */
    uint32_t total_successes;    /**< Total successes (for stats) */
} circuit_breaker_t;

//=============================================================================
// Operation Structure (for retry)
//=============================================================================

/**
 * @brief Generic operation for retry mechanism
 */
typedef struct {
    const char* name;                    /**< Operation name */
    bool (*execute)(void* context);      /**< Execute function */
    void (*rollback)(void* context);     /**< Rollback function (optional) */
    void* context;                       /**< Operation context */
    uint32_t execution_count;            /**< Number of executions */
} operation_t;

//=============================================================================
// Recovery Execution API
//=============================================================================

/**
 * @brief Execute recovery strategy based on diagnosis
 *
 * WHAT: Automatically select and execute best recovery strategy
 * WHY:  Recover from failures with minimal downtime
 * HOW:  Analyze diagnosis → select strategy → execute actions → verify
 *
 * STRATEGY SELECTION:
 * - SIGSEGV: Rollback checkpoint → reinitialize
 * - SIGFPE: Clear NaN → reduce learning rate
 * - Memory: Trigger GC → reduce batch size
 * - Performance: Fallback CPU → reduce model
 *
 * @param brain Brain instance to recover
 * @param diagnosis Diagnostic result from failure analysis
 * @return Recovery result with status and actions taken
 */
recovery_result_t recovery_execute_strategy(brain_t brain, diagnostic_summary_t* diagnosis);

/**
 * @brief Retry operation with exponential backoff
 *
 * WHAT: Retry failed operation with increasing delays
 * WHY:  Transient failures may succeed on retry
 * HOW:  Execute → wait → retry with backoff (10ms, 100ms, 1s, ...)
 *
 * BACKOFF: delay = initial_delay * (2^retry_count)
 * Example: 10ms, 20ms, 40ms, 80ms, 160ms
 *
 * @param brain Brain instance
 * @param op Operation to retry
 * @param max_retries Maximum retry attempts (1-10 recommended)
 * @return Recovery result with success/failure status
 */
recovery_result_t recovery_retry_operation(brain_t brain, operation_t* op, uint32_t max_retries);

/**
 * @brief Fallback to CPU execution mode
 *
 * WHAT: Switch brain from GPU to CPU execution
 * WHY:  GPU errors may not occur on CPU
 * HOW:  Disable GPU acceleration, reload state
 *
 * USE CASES:
 * - GPU memory exhaustion
 * - GPU driver crashes
 * - CUDA errors
 *
 * @param brain Brain instance
 * @return Recovery result
 */
recovery_result_t recovery_fallback_cpu(brain_t brain);

/**
 * @brief Rollback brain state to checkpoint
 *
 * WHAT: Restore brain to last known good state
 * WHY:  Recover from corrupted state
 * HOW:  Load checkpoint → verify integrity → replace current state
 *
 * REQUIREMENTS:
 * - Valid checkpoint file must exist
 * - Checkpoint must be uncorrupted
 * - Memory must be available for loading
 *
 * @param brain Brain instance
 * @param checkpoint Checkpoint name (NULL = most recent)
 * @return Recovery result
 */
recovery_result_t recovery_rollback_state(brain_t brain, const char* checkpoint);

//=============================================================================
// Strategy Selection API
//=============================================================================

/**
 * @brief Select optimal recovery strategy
 *
 * WHAT: Choose best recovery strategy for failure type
 * WHY:  Different failures need different approaches
 * HOW:  Map failure type → tier → actions
 *
 * MAPPING:
 * - SIGSEGV → STRATEGIC (rollback checkpoint)
 * - SIGFPE → IMMEDIATE (clear NaN) + TACTICAL (reduce LR)
 * - Memory → TACTICAL (GC) + STRATEGIC (reduce batch)
 * - Performance → STRATEGIC (fallback CPU)
 *
 * @param diagnosis Diagnostic result
 * @return Selected recovery strategy (caller must not free)
 */
recovery_strategy_t* recovery_select_strategy(diagnostic_summary_t* diagnosis);

/**
 * @brief Create comprehensive recovery plan
 *
 * WHAT: Generate multi-tier recovery plan
 * WHY:  Complex failures may need multiple strategies
 * HOW:  Select primary → fallback → preventive strategies
 *
 * PLAN STRUCTURE:
 * 1. Immediate action (clear symptoms)
 * 2. Tactical action (fix root cause)
 * 3. Strategic action (if tactical fails)
 * 4. Preventive action (avoid recurrence)
 *
 * @param diagnosis Diagnostic result
 * @param health Current health status
 * @return Recovery strategy (caller must not free)
 */
recovery_strategy_t* recovery_create_plan(diagnostic_summary_t* diagnosis, health_state_t health);

//=============================================================================
// Self-Healing API
//=============================================================================

/**
 * @brief Automatic recovery without user intervention
 *
 * WHAT: Detect and fix common issues automatically
 * WHY:  Reduce downtime and manual intervention
 * HOW:  Run diagnostics → select strategy → execute → verify
 *
 * AUTO-HEALED ISSUES:
 * - NaN/Inf in weights
 * - Learning divergence
 * - Memory fragmentation
 * - Temporary network errors
 *
 * @param brain Brain instance
 * @param diagnosis Diagnostic result (optional, can be NULL)
 * @return true if auto-healing successful, false otherwise
 */
bool recovery_auto_heal(brain_t brain, diagnostic_summary_t* diagnosis);

/**
 * @brief Adjust brain parameters for stability
 *
 * WHAT: Modify hyperparameters to improve stability
 * WHY:  Prevent divergence and crashes
 * HOW:  Analyze current state → apply conservative adjustments
 *
 * ADJUSTMENTS:
 * - Learning rate: Reduce by 50% on divergence
 * - Batch size: Reduce by 50% on memory pressure
 * - Memory limit: Increase by 20% on allocation failures
 * - Timeout: Double on timeout errors
 * - Precision: Switch to float64 on numeric instability
 *
 * @param brain Brain instance
 * @param type Type of adjustment to make
 * @return true on success, false on failure
 */
bool recovery_adjust_parameters(brain_t brain, adjustment_type_t type);

//=============================================================================
// Circuit Breaker API
//=============================================================================

/**
 * @brief Create circuit breaker
 *
 * WHAT: Create circuit breaker for operation protection
 * WHY:  Prevent cascading failures from repeated operations
 * HOW:  Track failures → open circuit → test periodically
 *
 * EXAMPLE:
 * ```c
 * circuit_breaker_t* cb = circuit_breaker_create(5, 1000);
 * if (circuit_breaker_allow_operation(cb)) {
 *     if (perform_operation()) {
 *         circuit_breaker_record_success(cb);
 *     } else {
 *         circuit_breaker_record_failure(cb);
 *     }
 * }
 * ```
 *
 * @param failure_threshold Number of failures before opening (1-100)
 * @param timeout_ms Time before testing recovery (100-60000)
 * @return Circuit breaker instance (caller must free)
 */
circuit_breaker_t* circuit_breaker_create(uint32_t failure_threshold, uint32_t timeout_ms);

/**
 * @brief Destroy circuit breaker
 *
 * WHAT: Free circuit breaker resources
 * WHY:  Prevent memory leaks
 * HOW:  Free structure
 *
 * @param cb Circuit breaker instance (can be NULL)
 */
void circuit_breaker_destroy(circuit_breaker_t* cb);

/**
 * @brief Check if operation is allowed
 *
 * WHAT: Check if circuit allows operation
 * WHY:  Prevent operations that will likely fail
 * HOW:  Check state → CLOSED=allow, OPEN=deny, HALF_OPEN=test
 *
 * STATE MACHINE:
 * - CLOSED: Normal operation, allow all
 * - OPEN: Too many failures, deny all
 * - HALF_OPEN: Testing recovery, allow one
 *
 * @param cb Circuit breaker instance
 * @return true if operation allowed, false if blocked
 */
bool circuit_breaker_allow_operation(circuit_breaker_t* cb);

/**
 * @brief Record successful operation
 *
 * WHAT: Update circuit breaker with success
 * WHY:  Track recovery from failures
 * HOW:  Increment success count → close circuit if threshold met
 *
 * @param cb Circuit breaker instance
 */
void circuit_breaker_record_success(circuit_breaker_t* cb);

/**
 * @brief Record failed operation
 *
 * WHAT: Update circuit breaker with failure
 * WHY:  Track failure rate and open circuit if needed
 * HOW:  Increment failure count → open circuit if threshold exceeded
 *
 * @param cb Circuit breaker instance
 */
void circuit_breaker_record_failure(circuit_breaker_t* cb);

/**
 * @brief Reset circuit breaker state
 *
 * WHAT: Reset circuit breaker to initial state
 * WHY:  Manual recovery or testing
 * HOW:  Reset counters, close circuit
 *
 * @param cb Circuit breaker instance
 */
void circuit_breaker_reset(circuit_breaker_t* cb);

/**
 * @brief Get circuit breaker state
 *
 * WHAT: Get current circuit state
 * WHY:  Monitoring and diagnostics
 * HOW:  Return current state enum
 *
 * @param cb Circuit breaker instance
 * @return Current circuit state
 */
circuit_state_t circuit_breaker_get_state(circuit_breaker_t* cb);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get recovery tier name
 *
 * WHAT: Convert tier enum to string
 * WHY:  Human-readable logging
 * HOW:  Map enum values to names
 *
 * @param tier Recovery tier
 * @return Tier name string
 */
const char* recovery_tier_name(recovery_tier_t tier);

/**
 * @brief Get recovery action name
 *
 * WHAT: Convert action enum to string
 * WHY:  Human-readable logging
 * HOW:  Map enum values to names
 *
 * @param action Recovery action
 * @return Action name string
 */
const char* recovery_action_name(recovery_action_t action);

/**
 * @brief Get recovery status name
 *
 * WHAT: Convert status enum to string
 * WHY:  Human-readable logging
 * HOW:  Map enum values to names
 *
 * @param status Recovery status
 * @return Status name string
 */
const char* recovery_status_name(recovery_status_t status);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_RECOVERY_H
