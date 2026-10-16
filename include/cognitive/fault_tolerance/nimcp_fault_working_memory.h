/**
 * @file nimcp_fault_working_memory.h
 * @brief Working Memory for Active Fault Context - Cognitive Fault Tolerance
 * @version 1.0.0
 * @date 2025-11-20
 *
 * WHAT: Working memory buffer tracking active faults and recovery state
 * WHY: Maintain context during multi-step recovery, detect cascading failures
 * HOW: Miller's 7±2 capacity limit, priority-based eviction, cascade detection
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal cortex maintains ~7 items in working memory (Miller's Law)
 * - Attention focus on highest-priority item
 * - Limited capacity forces prioritization and consolidation
 * - Cascading failures detected via temporal pattern recognition
 *
 * DESIGN PRINCIPLES:
 * - Miller's Law: 7±2 active faults max (default: 9)
 * - Priority-based eviction: Keep critical, discard minor
 * - Cascade detection: >10 faults/min triggers emergency mode
 * - Multi-step recovery: Track progress through recovery phases
 * - Attention focus: Always know most critical fault
 *
 * INTEGRATION POINTS:
 * 1. Fault Detection Module (receives new faults)
 * 2. Recovery Strategy Module (sets recovery plans)
 * 3. Episodic Memory (evicted faults archived here)
 * 4. Executive Functions (queries priority fault)
 *
 * PERFORMANCE:
 * - Add fault: O(n) where n = capacity (9), ~1us
 * - Get priority: O(n) where n = capacity (9), <100ns
 * - Remove fault: O(n) where n = capacity (9), ~500ns
 * - Memory overhead: ~2KB for default capacity
 *
 * @author NIMCP Development Team
 * @date 2025-11-20
 * @version 1.0.0
 */

#ifndef NIMCP_FAULT_WORKING_MEMORY_H
#define NIMCP_FAULT_WORKING_MEMORY_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Miller's Law minimum capacity (7 - 2) */
#define FAULT_WORKING_MEMORY_MIN_CAPACITY 5

/** Miller's Law default capacity (7 + 2) */
#define FAULT_WORKING_MEMORY_DEFAULT_CAPACITY 9

/** Miller's Law maximum capacity (for flexibility) */
#define FAULT_WORKING_MEMORY_MAX_CAPACITY 100

/** Cascade detection threshold (faults/minute) */
#define FAULT_WORKING_MEMORY_CASCADE_THRESHOLD 10

/** Cascade detection window (microseconds = 1 minute) */
#define FAULT_WORKING_MEMORY_CASCADE_WINDOW_US 60000000

/** Maximum fault description length */
#define FAULT_DESCRIPTION_MAX_LENGTH 256

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Fault severity levels
 *
 * WHAT: Classification of fault impact
 * WHY: Prioritize attention and recovery resources
 */
typedef enum {
    FAULT_SEVERITY_MINOR = 0,      /**< Low impact, can be deferred */
    FAULT_SEVERITY_MAJOR = 1,      /**< Significant impact, needs attention */
    FAULT_SEVERITY_CRITICAL = 2,   /**< Severe impact, immediate action required */
    FAULT_SEVERITY_COUNT = 3       /**< Number of severity levels */
} fault_severity_t;

/**
 * @brief Recovery strategy types
 *
 * WHAT: Approach to fault recovery
 * WHY: Different faults require different recovery methods
 */
typedef enum {
    RECOVERY_STRATEGY_NONE = 0,      /**< No recovery strategy set */
    RECOVERY_STRATEGY_RETRY,         /**< Retry the failed operation */
    RECOVERY_STRATEGY_RESTART,       /**< Restart the component */
    RECOVERY_STRATEGY_FAILOVER,      /**< Switch to backup/replica */
    RECOVERY_STRATEGY_ROLLBACK,      /**< Revert to previous state */
    RECOVERY_STRATEGY_RESTORE,       /**< Restore from checkpoint/backup */
    RECOVERY_STRATEGY_GRADUAL,       /**< Gradual degradation/recovery */
    RECOVERY_STRATEGY_EMERGENCY,     /**< Emergency shutdown/isolation */
    RECOVERY_STRATEGY_COUNT          /**< Number of recovery strategies */
} recovery_strategy_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Fault descriptor
 *
 * WHAT: Core fault information
 * WHY: Standardized fault representation
 */
typedef struct {
    uint32_t fault_id;                              /**< Unique fault identifier */
    fault_severity_t severity;                      /**< Fault severity level */
    uint64_t timestamp_us;                          /**< When fault occurred (microseconds) */
    char description[FAULT_DESCRIPTION_MAX_LENGTH]; /**< Human-readable description */
    uint32_t recovery_attempts;                     /**< Number of recovery attempts */
    bool is_resolved;                               /**< Whether fault is resolved */
} fault_t;

/**
 * @brief Active fault in working memory
 *
 * WHAT: Fault plus working memory metadata
 * WHY: Track how long fault has been active
 */
typedef struct {
    fault_t fault;              /**< The fault itself */
    uint64_t time_in_memory_us; /**< How long this fault has been in working memory */
} active_fault_t;

/**
 * @brief Working memory configuration
 *
 * WHAT: Configurable parameters for working memory
 * WHY: Allow tuning for different use cases
 */
typedef struct {
    uint32_t max_capacity;       /**< Maximum faults in working memory (default: 9) */
    uint32_t cascade_threshold;  /**< Faults/min to trigger cascade (default: 10) */
    uint64_t cascade_window_us;  /**< Time window for cascade detection (default: 60s) */
} fault_working_memory_config_t;

/**
 * @brief Opaque working memory structure
 *
 * WHAT: Hidden implementation details
 * WHY: Encapsulation, allow future changes without breaking API
 */
typedef struct fault_working_memory fault_working_memory_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create fault working memory with default configuration
 *
 * WHAT: Allocate and initialize working memory
 * WHY: Initialize fault tracking system
 * HOW: Allocate structure, set defaults, initialize arrays
 *
 * COMPLEXITY: O(1)
 * MEMORY: ~2KB for default capacity (9 faults)
 *
 * @return Pointer to working memory or NULL on allocation failure
 *
 * @note Caller must call fault_working_memory_destroy() to free
 */
fault_working_memory_t* fault_working_memory_create(void);

/**
 * @brief Create fault working memory with custom configuration
 *
 * WHAT: Allocate and initialize working memory with custom settings
 * WHY: Allow tuning for specific use cases
 * HOW: Validate config, allocate structure, initialize with custom params
 *
 * COMPLEXITY: O(1)
 * MEMORY: Varies with config.max_capacity
 *
 * @param config Configuration parameters (must be non-NULL)
 * @return Pointer to working memory or NULL on error
 *
 * @note Returns NULL if config is NULL or invalid
 */
fault_working_memory_t* fault_working_memory_create_custom(
    const fault_working_memory_config_t* config
);

/**
 * @brief Destroy fault working memory
 *
 * WHAT: Free all resources associated with working memory
 * WHY: Prevent memory leaks
 * HOW: Free fault arrays, free structure
 *
 * COMPLEXITY: O(1)
 * MEMORY: Frees all allocated memory
 *
 * @param wm Working memory to destroy (can be NULL)
 *
 * @note Safe to call with NULL pointer
 */
void fault_working_memory_destroy(fault_working_memory_t* wm);

/**
 * @brief Get default configuration
 *
 * WHAT: Return default configuration values
 * WHY: Convenient starting point for custom configs
 * HOW: Return struct with default values
 *
 * COMPLEXITY: O(1)
 *
 * @return Default configuration struct
 */
fault_working_memory_config_t fault_working_memory_default_config(void);

//=============================================================================
// Fault Management Functions
//=============================================================================

/**
 * @brief Add fault to working memory
 *
 * WHAT: Insert new fault into working memory buffer
 * WHY: Track active faults for recovery coordination
 * HOW: Check capacity → Evict if full (lowest priority) → Insert new fault
 *
 * ALGORITHM:
 * 1. Validate parameters (NULL checks)
 * 2. If at capacity, find and evict lowest-priority fault
 * 3. Copy fault to next slot
 * 4. Record time added to memory
 * 5. Update cascade detection counters
 *
 * EVICTION POLICY:
 * - Priority 1: Evict resolved faults
 * - Priority 2: Evict lowest severity (minor > major > critical)
 * - Priority 3: Evict oldest fault
 *
 * COMPLEXITY: O(n) where n = capacity (typically 9)
 * MEMORY: O(1) - reuses existing slots
 *
 * @param wm Working memory instance (non-NULL)
 * @param fault Fault to add (non-NULL)
 * @return true on success, false on error (NULL params)
 *
 * @note If capacity full, lowest-priority fault is evicted
 * @note Increments cascade detection counter
 */
bool fault_working_memory_add_fault(
    fault_working_memory_t* wm,
    const fault_t* fault
);

/**
 * @brief Remove resolved fault from working memory
 *
 * WHAT: Remove fault by ID from working memory
 * WHY: Clear resolved faults to free slots
 * HOW: Find fault by ID → Shift array to remove gap
 *
 * COMPLEXITY: O(n) where n = capacity
 * MEMORY: O(1)
 *
 * @param wm Working memory instance (can be NULL)
 * @param fault_id Fault ID to remove
 *
 * @note No-op if wm is NULL or fault_id not found
 */
void fault_working_memory_remove_fault(
    fault_working_memory_t* wm,
    uint32_t fault_id
);

/**
 * @brief Clear all faults from working memory
 *
 * WHAT: Remove all faults, reset state
 * WHY: Reset after system recovery or testing
 * HOW: Set count to 0, reset recovery state
 *
 * COMPLEXITY: O(1)
 * MEMORY: O(1)
 *
 * @param wm Working memory instance (can be NULL)
 */
void fault_working_memory_clear(fault_working_memory_t* wm);

/**
 * @brief Get fault at specific index
 *
 * WHAT: Retrieve fault by array index
 * WHY: Iterate through all active faults
 * HOW: Bounds check → Return pointer to fault
 *
 * COMPLEXITY: O(1)
 *
 * @param wm Working memory instance (can be NULL)
 * @param index Index in range [0, count)
 * @return Pointer to active fault or NULL if out of bounds
 *
 * @note Returns NULL if index >= count
 * @note Returned pointer is valid until next add/remove/destroy
 */
const active_fault_t* fault_working_memory_get_fault_at(
    const fault_working_memory_t* wm,
    uint32_t index
);

/**
 * @brief Get number of active faults
 *
 * WHAT: Return current fault count
 * WHY: Know how many faults are active
 * HOW: Return count field
 *
 * COMPLEXITY: O(1)
 *
 * @param wm Working memory instance (can be NULL)
 * @return Number of faults in working memory (0 if NULL)
 */
uint32_t fault_working_memory_get_count(const fault_working_memory_t* wm);

//=============================================================================
// Priority and Attention Functions
//=============================================================================

/**
 * @brief Get most critical fault (attention focus)
 *
 * WHAT: Return fault requiring immediate attention
 * WHY: Direct cognitive resources to most important issue
 * HOW: Find fault with highest severity → If tie, oldest fault
 *
 * PRIORITY ORDER:
 * 1. Highest severity (CRITICAL > MAJOR > MINOR)
 * 2. If equal severity, oldest fault (first added)
 *
 * COMPLEXITY: O(n) where n = capacity
 *
 * @param wm Working memory instance (can be NULL)
 * @return Pointer to priority fault or NULL if empty
 *
 * @note Returns NULL if working memory is empty
 * @note Pointer valid until next add/remove/destroy
 */
active_fault_t* fault_working_memory_get_priority_fault(
    fault_working_memory_t* wm
);

//=============================================================================
// Recovery Progress Functions
//=============================================================================

/**
 * @brief Set recovery strategy for current fault
 *
 * WHAT: Set active recovery plan and expected steps
 * WHY: Track multi-step recovery progress
 * HOW: Store strategy type and total steps
 *
 * COMPLEXITY: O(1)
 *
 * @param wm Working memory instance (non-NULL)
 * @param strategy Recovery strategy type
 * @param total_steps Number of steps in recovery plan
 * @return true on success, false if wm is NULL
 */
bool fault_working_memory_set_recovery_strategy(
    fault_working_memory_t* wm,
    recovery_strategy_t strategy,
    uint32_t total_steps
);

/**
 * @brief Update recovery progress
 *
 * WHAT: Record completion of recovery step
 * WHY: Track progress through multi-step recovery
 * HOW: Update current step counter
 *
 * COMPLEXITY: O(1)
 *
 * @param wm Working memory instance (can be NULL)
 * @param step_completed Step number just completed
 */
void fault_working_memory_update_progress(
    fault_working_memory_t* wm,
    uint32_t step_completed
);

/**
 * @brief Get current recovery step
 *
 * WHAT: Return current position in recovery plan
 * WHY: Know recovery progress
 * HOW: Return step counter
 *
 * COMPLEXITY: O(1)
 *
 * @param wm Working memory instance (can be NULL)
 * @return Current step number (0 if NULL or no active recovery)
 */
uint32_t fault_working_memory_get_recovery_step(
    const fault_working_memory_t* wm
);

/**
 * @brief Get total recovery steps
 *
 * WHAT: Return total steps in current recovery plan
 * WHY: Calculate recovery progress percentage
 * HOW: Return total_steps field
 *
 * COMPLEXITY: O(1)
 *
 * @param wm Working memory instance (can be NULL)
 * @return Total steps (0 if NULL or no active recovery)
 */
uint32_t fault_working_memory_get_total_steps(
    const fault_working_memory_t* wm
);

//=============================================================================
// Cascade Detection Functions
//=============================================================================

/**
 * @brief Check if cascading failure is occurring
 *
 * WHAT: Determine if fault rate indicates cascading failure
 * WHY: Trigger emergency recovery mode
 * HOW: Check if faults/minute exceeds threshold
 *
 * CASCADE CRITERIA:
 * - >10 faults added in last 60 seconds (default)
 * - Indicates systemic failure, not isolated issues
 *
 * COMPLEXITY: O(1)
 *
 * @param wm Working memory instance (can be NULL)
 * @return true if cascading failure detected, false otherwise
 */
bool fault_working_memory_is_cascading(const fault_working_memory_t* wm);

/**
 * @brief Update cascade detection state
 *
 * WHAT: Recalculate cascade detection based on current state
 * WHY: Keep cascade detection up-to-date
 * HOW: Count faults in time window, compare to threshold
 *
 * COMPLEXITY: O(n) where n = capacity
 *
 * @param wm Working memory instance (can be NULL)
 *
 * @note Call periodically or after adding faults
 */
void fault_working_memory_update_cascade_detection(fault_working_memory_t* wm);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get current timestamp in microseconds
 *
 * WHAT: Return current time as microseconds since epoch
 * WHY: Consistent time source for all fault timestamps
 * HOW: Use clock_gettime with CLOCK_REALTIME
 *
 * COMPLEXITY: O(1)
 *
 * @return Current timestamp in microseconds
 */
uint64_t fault_working_memory_get_timestamp_us(void);

//=============================================================================
// String Conversion Functions (for debugging/logging)
//=============================================================================

/**
 * @brief Convert severity to string
 *
 * WHAT: Get human-readable severity name
 * WHY: Logging and debugging
 * HOW: Map enum to string
 *
 * @param severity Fault severity
 * @return String representation
 */
const char* fault_severity_to_string(fault_severity_t severity);

/**
 * @brief Convert recovery strategy to string
 *
 * WHAT: Get human-readable strategy name
 * WHY: Logging and debugging
 * HOW: Map enum to string
 *
 * @param strategy Recovery strategy
 * @return String representation
 */
const char* recovery_strategy_to_string(recovery_strategy_t strategy);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_FAULT_WORKING_MEMORY_H
