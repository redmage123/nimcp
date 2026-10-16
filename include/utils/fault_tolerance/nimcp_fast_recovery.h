/**
 * @file nimcp_fast_recovery.h
 * @brief Fast path recovery for common errors with sub-millisecond latency
 *
 * WHAT: Pre-classified error patterns with immediate recovery actions (<1ms)
 * WHY:  200x speedup over full diagnostic workflow (20ms → 0.1ms typical)
 * HOW:  Pattern matching → direct action execution → no diagnostic overhead
 *
 * PERFORMANCE TARGETS:
 * - Pattern matching: <50μs (microseconds)
 * - Recovery execution: <1ms (milliseconds)
 * - Total fast path: <1ms guaranteed
 * - Fallback to full recovery if pattern doesn't match
 *
 * FAST RECOVERY TYPES:
 * 1. NaN/Inf Clearing: Replace invalid floating point values with zeros
 * 2. Memory Cache Clear: Free temporary caches and buffers
 * 3. Gradient Clipping: Prevent gradient explosion
 * 4. State Reset: Reset iteration counters and temporary state
 * 5. FPU Reset: Clear floating point exception flags
 *
 * ARCHITECTURE:
 * - Lock-free statistics tracking (atomic operations)
 * - Pre-allocated error patterns (no malloc in fast path)
 * - Direct function dispatch (no vtables or indirection)
 * - Fallback to full diagnostic path on miss
 *
 * INTEGRATION POINTS:
 * - Signal handler: Check fast path before diagnostics
 * - Health monitor: Trigger preventive fast recovery
 * - Recovery system: Fallback from fast path to full recovery
 *
 * @author NIMCP Team
 * @date 2025-11-20
 */

#ifndef NIMCP_FAST_RECOVERY_H
#define NIMCP_FAST_RECOVERY_H

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
// Fast Recovery Types
//=============================================================================

/**
 * @brief Fast recovery action types
 *
 * WHAT: Pre-classified recovery actions for common errors
 * WHY:  Enable immediate dispatch without diagnostic overhead
 * HOW:  Each type maps to a specific, well-tested recovery function
 */
typedef enum {
    FAST_RECOVERY_NONE = 0,           /**< No fast recovery applicable */

    // Numerical stability (most common, highest priority)
    FAST_RECOVERY_CLEAR_NAN,          /**< Clear NaN/Inf values (50-100μs) */
    FAST_RECOVERY_CLIP_GRADIENTS,     /**< Clip exploding gradients (100-200μs) */
    FAST_RECOVERY_RESET_FPU,          /**< Reset FPU exception flags (10-20μs) */

    // Memory management (common in training loops)
    FAST_RECOVERY_CLEAR_CACHE,        /**< Clear temporary caches (200-500μs) */
    FAST_RECOVERY_FLUSH_BUFFERS,      /**< Flush I/O buffers (100-300μs) */

    // State management (iteration errors)
    FAST_RECOVERY_RESET_STATE,        /**< Reset iteration counters (50-100μs) */
    FAST_RECOVERY_RESET_COUNTER,      /**< Reset error counter (10-20μs) */

    // Preventive actions
    FAST_RECOVERY_TRIGGER_GC,         /**< Force garbage collection (500-1000μs) */

    FAST_RECOVERY_TYPE_COUNT          /**< Number of recovery types */
} fast_recovery_type_t;

/**
 * @brief Fast recovery result status
 */
typedef enum {
    FAST_RECOVERY_SUCCESS = 0,        /**< Recovery successful */
    FAST_RECOVERY_PARTIAL,            /**< Partial recovery (degraded mode) */
    FAST_RECOVERY_NOT_APPLICABLE,     /**< Pattern not recognized, use full recovery */
    FAST_RECOVERY_FAILED,             /**< Recovery failed, escalate */
    FAST_RECOVERY_TIMEOUT             /**< Recovery exceeded 1ms timeout */
} fast_recovery_status_t;

/**
 * @brief Signal/error context for fast path matching
 *
 * WHAT: Minimal context needed for fast pattern matching
 * WHY:  Lightweight structure for sub-millisecond checks
 * HOW:  Only essential fields, no deep analysis
 */
typedef struct {
    int signal;                       /**< Signal number (SIGFPE, SIGSEGV, etc.) */
    void* fault_address;              /**< Fault address (if available) */
    uint32_t error_code;              /**< Platform-specific error code */
    bool is_numeric_error;            /**< Quick flag: NaN/Inf/FPE detected */
    bool is_memory_error;             /**< Quick flag: Memory-related */
    bool is_state_error;              /**< Quick flag: State machine error */
    void* brain_ptr;                  /**< Brain instance (for validation) */
} fast_recovery_context_t;

/**
 * @brief Fast recovery result
 *
 * WHAT: Lightweight result structure for fast path
 * WHY:  Minimal overhead, essential info only
 * HOW:  Fixed-size structure, no allocations
 */
typedef struct {
    fast_recovery_status_t status;    /**< Recovery status */
    fast_recovery_type_t type;        /**< Recovery type executed */
    uint32_t latency_us;              /**< Actual recovery latency (microseconds) */
    bool fallback_needed;             /**< Whether to fallback to full recovery */
    const char* message;              /**< Brief status message (static string) */
} fast_recovery_result_t;

//=============================================================================
// Fast Recovery Statistics (Lock-Free)
//=============================================================================

/**
 * @brief Fast recovery statistics
 *
 * WHAT: Performance metrics for fast path recovery
 * WHY:  Monitor effectiveness and performance
 * HOW:  Atomic counters, lock-free updates
 *
 * METRICS:
 * - Hit rate: fast_hits / (fast_hits + fast_misses)
 * - Avg latency: total_latency_us / fast_hits
 * - Success rate: successful_recoveries / fast_hits
 */
typedef struct {
    // Hit/miss tracking
    volatile uint64_t fast_hits;      /**< Number of fast path matches */
    volatile uint64_t fast_misses;    /**< Number of patterns not matched */

    // Per-type statistics
    volatile uint64_t clear_nan_count;       /**< NaN clearing executions */
    volatile uint64_t clip_gradients_count;  /**< Gradient clipping executions */
    volatile uint64_t reset_fpu_count;       /**< FPU reset executions */
    volatile uint64_t clear_cache_count;     /**< Cache clear executions */
    volatile uint64_t flush_buffers_count;   /**< Buffer flush executions */
    volatile uint64_t reset_state_count;     /**< State reset executions */
    volatile uint64_t reset_counter_count;   /**< Counter reset executions */
    volatile uint64_t trigger_gc_count;      /**< GC trigger executions */

    // Performance metrics
    volatile uint64_t total_latency_us;      /**< Total recovery time (μs) */
    volatile uint32_t min_latency_us;        /**< Minimum latency observed */
    volatile uint32_t max_latency_us;        /**< Maximum latency observed */

    // Outcome tracking
    volatile uint64_t successful_recoveries; /**< Successful fast recoveries */
    volatile uint64_t failed_recoveries;     /**< Failed fast recoveries */
    volatile uint64_t partial_recoveries;    /**< Partial fast recoveries */
    volatile uint64_t timeouts;              /**< Recoveries that exceeded 1ms */
} fast_recovery_stats_t;

//=============================================================================
// Fast Path Checking API
//=============================================================================

/**
 * @brief Check if fast recovery is applicable
 *
 * WHAT: Rapid pattern matching to determine if fast path applies
 * WHY:  Avoid full diagnostic overhead for common errors
 * HOW:  Match signal/error against pre-classified patterns
 *
 * PERFORMANCE: <50μs guaranteed
 *
 * PATTERNS:
 * - SIGFPE → FAST_RECOVERY_CLEAR_NAN (most common)
 * - SIGFPE + FE_OVERFLOW → FAST_RECOVERY_CLIP_GRADIENTS
 * - Memory pressure → FAST_RECOVERY_CLEAR_CACHE
 * - State errors → FAST_RECOVERY_RESET_STATE
 *
 * @param context Error context (signal, fault address, flags)
 * @return Recovery type if applicable, FAST_RECOVERY_NONE if not
 */
fast_recovery_type_t fast_recovery_is_applicable(const fast_recovery_context_t* context);

/**
 * @brief Check if fast recovery is applicable from signal only
 *
 * WHAT: Minimal fast path check with just signal number
 * WHY:  Even faster check when only signal is available
 * HOW:  Simple signal-to-recovery-type mapping
 *
 * PERFORMANCE: <10μs guaranteed
 *
 * @param signal Signal number (SIGFPE, etc.)
 * @return Recovery type if applicable, FAST_RECOVERY_NONE if not
 */
fast_recovery_type_t fast_recovery_is_applicable_signal(int signal);

//=============================================================================
// Fast Recovery Execution API
//=============================================================================

/**
 * @brief Execute fast recovery action
 *
 * WHAT: Execute pre-classified recovery action immediately
 * WHY:  Sub-millisecond recovery for common errors
 * HOW:  Direct dispatch to recovery function, no diagnostics
 *
 * PERFORMANCE: <1ms guaranteed (typically 100-500μs)
 *
 * ACTIONS:
 * - CLEAR_NAN: Scan and replace NaN/Inf with 0.0
 * - CLIP_GRADIENTS: Apply gradient clipping threshold
 * - RESET_FPU: Clear FPU exception flags
 * - CLEAR_CACHE: Free temporary caches
 * - RESET_STATE: Reset iteration counters
 *
 * @param type Recovery type to execute
 * @param brain Brain instance (NULL if not brain-specific)
 * @return Recovery result with status and latency
 */
fast_recovery_result_t fast_recovery_execute(fast_recovery_type_t type, brain_t brain);

/**
 * @brief Execute fast recovery with context
 *
 * WHAT: Execute recovery using full context information
 * WHY:  More accurate recovery with additional context
 * HOW:  Use context to refine recovery action
 *
 * PERFORMANCE: <1ms guaranteed
 *
 * @param context Full error context
 * @param brain Brain instance (NULL if not brain-specific)
 * @return Recovery result with status and latency
 */
fast_recovery_result_t fast_recovery_execute_with_context(
    const fast_recovery_context_t* context,
    brain_t brain
);

/**
 * @brief One-step fast recovery (check + execute)
 *
 * WHAT: Combined applicability check and execution
 * WHY:  Convenience function for signal handlers
 * HOW:  Check pattern → execute if applicable → return result
 *
 * PERFORMANCE: <1ms guaranteed
 *
 * WORKFLOW:
 * 1. Check if fast path applicable (<50μs)
 * 2. If yes, execute recovery (<1ms)
 * 3. If no, return NOT_APPLICABLE for fallback
 *
 * @param context Error context
 * @param brain Brain instance
 * @return Recovery result (NOT_APPLICABLE if no fast path)
 */
fast_recovery_result_t fast_recovery_attempt(
    const fast_recovery_context_t* context,
    brain_t brain
);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get fast recovery statistics
 *
 * WHAT: Return current fast path performance metrics
 * WHY:  Monitor effectiveness and latency
 * HOW:  Atomic read of statistics structure
 *
 * METRICS:
 * - Hit rate: fast_hits / (fast_hits + fast_misses)
 * - Avg latency: total_latency_us / fast_hits
 * - Success rate: successful_recoveries / fast_hits
 *
 * @return Statistics structure (copy, safe to use)
 */
fast_recovery_stats_t fast_recovery_get_stats(void);

/**
 * @brief Reset statistics counters
 *
 * WHAT: Zero all statistics counters
 * WHY:  Fresh start for monitoring period
 * HOW:  Atomic reset of all counters
 */
void fast_recovery_reset_stats(void);

/**
 * @brief Get average fast recovery latency
 *
 * WHAT: Calculate average recovery time
 * WHY:  Monitor performance over time
 * HOW:  total_latency_us / fast_hits
 *
 * @return Average latency in microseconds (0 if no data)
 */
uint32_t fast_recovery_get_avg_latency_us(void);

/**
 * @brief Get fast path hit rate
 *
 * WHAT: Calculate percentage of errors handled by fast path
 * WHY:  Measure fast path effectiveness
 * HOW:  fast_hits / (fast_hits + fast_misses)
 *
 * @return Hit rate as percentage (0.0-100.0)
 */
float fast_recovery_get_hit_rate(void);

/**
 * @brief Get fast recovery success rate
 *
 * WHAT: Calculate percentage of successful fast recoveries
 * WHY:  Measure recovery effectiveness
 * HOW:  successful_recoveries / fast_hits
 *
 * @return Success rate as percentage (0.0-100.0)
 */
float fast_recovery_get_success_rate(void);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get recovery type name
 *
 * WHAT: Convert recovery type enum to string
 * WHY:  Human-readable logging and debugging
 * HOW:  Static string lookup
 *
 * @param type Recovery type
 * @return Type name string (static, do not free)
 */
const char* fast_recovery_type_name(fast_recovery_type_t type);

/**
 * @brief Get recovery status name
 *
 * WHAT: Convert status enum to string
 * WHY:  Human-readable logging and debugging
 * HOW:  Static string lookup
 *
 * @param status Recovery status
 * @return Status name string (static, do not free)
 */
const char* fast_recovery_status_name(fast_recovery_status_t status);

/**
 * @brief Get estimated recovery time
 *
 * WHAT: Return typical latency for recovery type
 * WHY:  Performance planning and SLA enforcement
 * HOW:  Static lookup table of typical latencies
 *
 * @param type Recovery type
 * @return Typical latency in microseconds
 */
uint32_t fast_recovery_get_typical_latency_us(fast_recovery_type_t type);

/**
 * @brief Validate fast recovery result
 *
 * WHAT: Check if recovery result is valid
 * WHY:  Detect corruption or invalid results
 * HOW:  Validate enum values and constraints
 *
 * @param result Recovery result to validate
 * @return true if valid, false otherwise
 */
bool fast_recovery_validate_result(const fast_recovery_result_t* result);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_FAST_RECOVERY_H
