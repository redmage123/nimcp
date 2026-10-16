/**
 * @file nimcp_lgss_attention_guard.h
 * @brief LGSS Attention Safety Guard - Cognitive Manipulation Defense
 * @version 1.0.0
 * @date 2026-01-16
 *
 * WHAT: Security guard for attention allocation to prevent cognitive manipulation
 * WHY:  Attention can be hijacked, fixated, or manipulated to distract from
 *       safety-critical information, enabling attacks that exploit blind spots
 * HOW:  Monitor attention allocation patterns, detect anomalies, and ensure
 *       minimum attention is always directed at safety-relevant information
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal cortex regulates attention allocation and executive control
 * - Attention hijacking mimics how visual/auditory salience can capture attention
 * - Fixation detection parallels attentional inertia in cognitive psychology
 * - Safety blindness detection addresses "inattentional blindness" phenomenon
 *
 * SECURITY MODEL:
 * ```
 * +-------------------------------------------------------------------------+
 * |                    ATTENTION GUARD ARCHITECTURE                          |
 * +-------------------------------------------------------------------------+
 * |                                                                         |
 * |   ATTENTION SYSTEM              ATTENTION GUARD                          |
 * |   +-----------------+          +-----------------+                       |
 * |   | Target Selection|--------->| Hijacking Check |                       |
 * |   | Focus Allocation|--------->| Fixation Check  |                       |
 * |   | Priority Queue  |--------->| Safety Check    |                       |
 * |   | Salience Map    |<---------| Enforce Minimum |                       |
 * |   +-----------------+          +-----------------+                       |
 * |          |                             |                                 |
 * |          v                             v                                 |
 * |   +---------------------------------------------+                        |
 * |   |           PROTECTION MECHANISMS             |                        |
 * |   | - Max attention to single target            |                        |
 * |   | - Min attention to safety items             |                        |
 * |   | - Max duration on same target               |                        |
 * |   | - Sudden attention shift detection          |                        |
 * |   +---------------------------------------------+                        |
 * +-------------------------------------------------------------------------+
 * ```
 *
 * ATTACK SCENARIOS DEFENDED:
 * 1. Attention Hijacking: Forced focus on distractor to hide malicious content
 * 2. Attention Fixation: Keeping attention locked on one item indefinitely
 * 3. Safety Blindness: Systematic avoidance of safety-relevant information
 * 4. Attention Scattering: Rapid switching to prevent coherent analysis
 *
 * @see nimcp_lgss_working_memory_guard.h
 * @see nimcp_security.h
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_LGSS_ATTENTION_GUARD_H
#define NIMCP_LGSS_ATTENTION_GUARD_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum number of attention targets tracked in history */
#define LGSS_ATTN_MAX_HISTORY             256

/** @brief Maximum number of safety-relevant targets tracked */
#define LGSS_ATTN_MAX_SAFETY_TARGETS      64

/** @brief Default maximum attention weight to single target (0.0-1.0) */
#define LGSS_ATTN_DEFAULT_MAX_SINGLE      0.8f

/** @brief Default minimum attention to safety items (0.0-1.0) */
#define LGSS_ATTN_DEFAULT_MIN_SAFETY      0.1f

/** @brief Default maximum hold time on same target (ms) */
#define LGSS_ATTN_DEFAULT_MAX_HOLD_MS     30000

/** @brief Attention change threshold for hijacking detection */
#define LGSS_ATTN_HIJACK_THRESHOLD        0.5f

/** @brief Minimum time between attention samples (ms) */
#define LGSS_ATTN_SAMPLE_INTERVAL_MS      100

/** @brief Bio-async module ID for attention guard */
#define BIO_MODULE_LGSS_ATTENTION_GUARD   0x0F20

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Attention safety status codes
 *
 * WHAT: Current safety status of attention allocation
 * WHY:  Classify attention state for appropriate response
 * HOW:  Evaluated continuously based on allocation patterns
 */
typedef enum {
    ATTN_STATUS_NORMAL = 0,          /**< Normal attention allocation */
    ATTN_STATUS_HIJACKED,            /**< Attention forcibly redirected */
    ATTN_STATUS_FIXATED,             /**< Attention stuck on single target */
    ATTN_STATUS_SCATTERED,           /**< Attention rapidly switching */
    ATTN_STATUS_SAFETY_BLIND         /**< Avoiding safety-relevant info */
} attention_safety_status_t;

/**
 * @brief Attention guard operation result codes
 */
typedef enum {
    ATTN_GUARD_OK = 0,               /**< Operation succeeded */
    ATTN_GUARD_ERROR_NULL,           /**< Null pointer parameter */
    ATTN_GUARD_ERROR_INVALID,        /**< Invalid parameter value */
    ATTN_GUARD_ERROR_MEMORY,         /**< Memory allocation failed */
    ATTN_GUARD_ERROR_CAPACITY,       /**< Capacity limit reached */
    ATTN_GUARD_ERROR_NOT_INIT,       /**< Guard not initialized */
    ATTN_GUARD_BLOCKED               /**< Attention allocation blocked */
} attention_guard_result_t;

/**
 * @brief Attention target classification
 */
typedef enum {
    ATTN_TARGET_NORMAL = 0,          /**< Normal attention target */
    ATTN_TARGET_SAFETY_CRITICAL,     /**< Safety-critical information */
    ATTN_TARGET_SAFETY_ADVISORY,     /**< Safety advisory information */
    ATTN_TARGET_SUSPICIOUS,          /**< Potentially malicious target */
    ATTN_TARGET_DISTRACTOR           /**< Known distractor pattern */
} attention_target_class_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Attention allocation record
 *
 * WHAT: Record of attention allocated to a specific target
 * WHY:  Track where attention is being directed over time
 * HOW:  Created for each attention allocation event
 */
typedef struct {
    uint64_t target_id;              /**< Unique identifier for attention target */
    float attention_weight;          /**< Attention weight allocated (0.0-1.0) */
    uint64_t duration_ms;            /**< Duration of attention in milliseconds */
    bool is_safety_relevant;         /**< Whether target is safety-relevant */
    attention_target_class_t target_class; /**< Classification of target */
    uint64_t timestamp;              /**< When allocation occurred (ms since epoch) */
} attention_allocation_t;

/**
 * @brief Attention history entry for pattern detection
 */
typedef struct {
    uint64_t target_id;              /**< Target that received attention */
    float weight;                    /**< Weight allocated */
    uint64_t start_time;             /**< When attention started */
    uint64_t end_time;               /**< When attention ended (0 if ongoing) */
    attention_target_class_t target_class; /**< Target classification */
} attention_history_entry_t;

/**
 * @brief Configuration for attention guard
 */
typedef struct {
    bool monitor_hijacking;          /**< Enable hijacking detection */
    bool monitor_fixation;           /**< Enable fixation detection */
    bool monitor_safety_blindness;   /**< Enable safety blindness detection */
    bool monitor_scattering;         /**< Enable attention scattering detection */

    float max_single_target_attention; /**< Max attention to one target (0.0-1.0) */
    float min_safety_attention;      /**< Min attention to safety items (0.0-1.0) */
    uint64_t max_attention_hold_ms;  /**< Max time on same target (ms) */

    float hijack_detection_threshold; /**< Sudden change threshold for hijacking */
    uint32_t scatter_window_ms;      /**< Time window for scatter detection */
    uint32_t scatter_threshold_count; /**< Target switches in window for scatter */

    bool auto_enforce_safety;        /**< Automatically enforce safety attention */
    bool log_violations;             /**< Log detected violations */
} attention_guard_config_t;

/**
 * @brief Attention guard statistics
 */
typedef struct {
    uint64_t total_checks;           /**< Total attention checks performed */
    uint64_t hijack_detections;      /**< Number of hijacking attempts detected */
    uint64_t fixation_detections;    /**< Number of fixations detected */
    uint64_t safety_blind_detections; /**< Safety blindness detections */
    uint64_t scatter_detections;     /**< Attention scattering detections */
    uint64_t safety_enforcements;    /**< Safety attention enforcements */
    uint64_t blocked_allocations;    /**< Allocations blocked by guard */
    float avg_safety_attention;      /**< Average attention to safety items */
    uint64_t last_check_time;        /**< Timestamp of last check */
} attention_guard_stats_t;

/**
 * @brief Attention check result
 */
typedef struct {
    attention_safety_status_t status; /**< Current safety status */
    attention_guard_result_t result;  /**< Operation result code */
    float current_safety_attention;   /**< Current attention to safety items */
    uint64_t current_focus_duration;  /**< Duration on current target */
    uint64_t current_target_id;       /**< Currently focused target */
    bool safety_enforcement_needed;   /**< Whether enforcement is required */
    char violation_details[256];      /**< Description of any violation */
} attention_check_result_t;

/**
 * @brief Opaque attention guard structure
 */
typedef struct attention_guard attention_guard_t;

/**
 * @brief Callback for attention violation events
 */
typedef void (*attention_violation_callback_t)(
    const attention_guard_t* guard,
    attention_safety_status_t status,
    const attention_check_result_t* details,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create a new attention guard instance
 *
 * WHAT: Allocate and initialize an attention guard
 * WHY:  Required before monitoring attention allocation
 * HOW:  Allocates memory, initializes tracking structures
 *
 * @param aix Pointer to AIX system (can be NULL for standalone)
 * @param attention_system Pointer to attention system to guard (can be NULL)
 * @param config Configuration settings (NULL for defaults)
 * @return New attention guard instance or NULL on failure
 *
 * @note Caller must call attention_guard_destroy() to free resources
 */
attention_guard_t* attention_guard_create(
    void* aix,
    void* attention_system,
    const attention_guard_config_t* config
);

/**
 * @brief Destroy an attention guard instance
 *
 * WHAT: Free all resources associated with attention guard
 * WHY:  Prevent memory leaks
 * HOW:  Frees history, tracking structures, and guard itself
 *
 * @param guard Guard instance to destroy (NULL is safe)
 */
void attention_guard_destroy(attention_guard_t* guard);

//=============================================================================
// Core Monitoring Functions
//=============================================================================

/**
 * @brief Check current attention allocation for safety violations
 *
 * WHAT: Analyze current attention state for security issues
 * WHY:  Primary monitoring function for detecting manipulation
 * HOW:  Evaluates allocation against all enabled detection mechanisms
 *
 * @param guard Guard instance
 * @param allocations Current attention allocations
 * @param num_allocations Number of allocations in array
 * @param result Output: check result details
 * @return ATTN_GUARD_OK on success, error code on failure
 *
 * @note This should be called periodically (e.g., every LGSS_ATTN_SAMPLE_INTERVAL_MS)
 */
attention_guard_result_t attention_guard_check(
    attention_guard_t* guard,
    const attention_allocation_t* allocations,
    size_t num_allocations,
    attention_check_result_t* result
);

/**
 * @brief Ensure minimum attention to safety-relevant items
 *
 * WHAT: Force minimum attention allocation to safety items
 * WHY:  Prevent complete safety blindness
 * HOW:  Returns required attention adjustments
 *
 * @param guard Guard instance
 * @param current_allocations Current attention allocations
 * @param num_allocations Number of allocations
 * @param adjusted_allocations Output: adjusted allocations (caller-allocated)
 * @param adjustment_count Output: number of adjustments made
 * @return ATTN_GUARD_OK on success, error code on failure
 *
 * @note Caller must allocate adjusted_allocations with at least num_allocations entries
 */
attention_guard_result_t attention_guard_ensure_safety_attention(
    attention_guard_t* guard,
    const attention_allocation_t* current_allocations,
    size_t num_allocations,
    attention_allocation_t* adjusted_allocations,
    size_t* adjustment_count
);

/**
 * @brief Detect attention hijacking attempts
 *
 * WHAT: Specifically check for forced attention redirection
 * WHY:  Detect attacks that forcibly capture attention
 * HOW:  Analyze sudden, large attention shifts to specific targets
 *
 * @param guard Guard instance
 * @param new_allocation Proposed new attention allocation
 * @param is_hijacking Output: true if hijacking detected
 * @param confidence Output: confidence level of detection (0.0-1.0)
 * @return ATTN_GUARD_OK on success, error code on failure
 */
attention_guard_result_t attention_guard_detect_hijacking(
    attention_guard_t* guard,
    const attention_allocation_t* new_allocation,
    bool* is_hijacking,
    float* confidence
);

/**
 * @brief Detect attention fixation
 *
 * WHAT: Check if attention is stuck on a single target too long
 * WHY:  Fixation can be exploited to hide other activities
 * HOW:  Track duration on current target against threshold
 *
 * @param guard Guard instance
 * @param current_target Target currently receiving attention
 * @param is_fixated Output: true if fixation detected
 * @param fixation_duration Output: how long attention has been fixed (ms)
 * @return ATTN_GUARD_OK on success, error code on failure
 */
attention_guard_result_t attention_guard_detect_fixation(
    attention_guard_t* guard,
    uint64_t current_target,
    bool* is_fixated,
    uint64_t* fixation_duration
);

/**
 * @brief Detect safety blindness
 *
 * WHAT: Check if safety-relevant items are being systematically ignored
 * WHY:  Attacks may try to direct attention away from safety info
 * HOW:  Track attention to safety vs non-safety items over time
 *
 * @param guard Guard instance
 * @param safety_targets Array of safety-relevant target IDs
 * @param num_safety_targets Number of safety targets
 * @param is_blind Output: true if safety blindness detected
 * @param safety_attention Output: current attention to safety items (0.0-1.0)
 * @return ATTN_GUARD_OK on success, error code on failure
 */
attention_guard_result_t attention_guard_detect_safety_blindness(
    attention_guard_t* guard,
    const uint64_t* safety_targets,
    size_t num_safety_targets,
    bool* is_blind,
    float* safety_attention
);

//=============================================================================
// Safety Target Management
//=============================================================================

/**
 * @brief Register a safety-relevant attention target
 *
 * WHAT: Mark a target as safety-relevant for monitoring
 * WHY:  Guard needs to know which targets require minimum attention
 * HOW:  Add to internal safety target tracking
 *
 * @param guard Guard instance
 * @param target_id Unique identifier for the target
 * @param priority Priority level for this safety target (higher = more critical)
 * @return ATTN_GUARD_OK on success, error code on failure
 */
attention_guard_result_t attention_guard_register_safety_target(
    attention_guard_t* guard,
    uint64_t target_id,
    uint32_t priority
);

/**
 * @brief Unregister a safety-relevant attention target
 *
 * WHAT: Remove target from safety-relevant tracking
 * WHY:  Target may no longer need special attention monitoring
 * HOW:  Remove from internal safety target list
 *
 * @param guard Guard instance
 * @param target_id Target to unregister
 * @return ATTN_GUARD_OK on success, error code on failure
 */
attention_guard_result_t attention_guard_unregister_safety_target(
    attention_guard_t* guard,
    uint64_t target_id
);

/**
 * @brief Mark a target as suspicious/distractor
 *
 * WHAT: Flag a target as potentially malicious
 * WHY:  Known distractors can be monitored more closely
 * HOW:  Add to suspicious target list with reason
 *
 * @param guard Guard instance
 * @param target_id Target to mark
 * @param target_class Classification (SUSPICIOUS or DISTRACTOR)
 * @param reason Description of why target is suspicious
 * @return ATTN_GUARD_OK on success, error code on failure
 */
attention_guard_result_t attention_guard_mark_suspicious(
    attention_guard_t* guard,
    uint64_t target_id,
    attention_target_class_t target_class,
    const char* reason
);

//=============================================================================
// Configuration and Statistics
//=============================================================================

/**
 * @brief Update attention guard configuration
 *
 * @param guard Guard instance
 * @param config New configuration
 * @return ATTN_GUARD_OK on success, error code on failure
 */
attention_guard_result_t attention_guard_set_config(
    attention_guard_t* guard,
    const attention_guard_config_t* config
);

/**
 * @brief Get current attention guard configuration
 *
 * @param guard Guard instance
 * @param config Output: current configuration
 * @return ATTN_GUARD_OK on success, error code on failure
 */
attention_guard_result_t attention_guard_get_config(
    const attention_guard_t* guard,
    attention_guard_config_t* config
);

/**
 * @brief Get attention guard statistics
 *
 * @param guard Guard instance
 * @param stats Output: current statistics
 * @return ATTN_GUARD_OK on success, error code on failure
 */
attention_guard_result_t attention_guard_get_stats(
    const attention_guard_t* guard,
    attention_guard_stats_t* stats
);

/**
 * @brief Reset attention guard statistics
 *
 * @param guard Guard instance
 * @return ATTN_GUARD_OK on success, error code on failure
 */
attention_guard_result_t attention_guard_reset_stats(attention_guard_t* guard);

//=============================================================================
// Callback Registration
//=============================================================================

/**
 * @brief Register callback for attention violations
 *
 * @param guard Guard instance
 * @param callback Function to call on violation
 * @param user_data User data passed to callback
 * @return ATTN_GUARD_OK on success, error code on failure
 */
attention_guard_result_t attention_guard_register_callback(
    attention_guard_t* guard,
    attention_violation_callback_t callback,
    void* user_data
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get string representation of attention status
 *
 * @param status Status code
 * @return Human-readable status string
 */
const char* attention_status_to_string(attention_safety_status_t status);

/**
 * @brief Get string representation of guard result
 *
 * @param result Result code
 * @return Human-readable result string
 */
const char* attention_guard_result_to_string(attention_guard_result_t result);

/**
 * @brief Initialize default configuration
 *
 * @param config Configuration structure to initialize
 */
void attention_guard_config_init_defaults(attention_guard_config_t* config);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LGSS_ATTENTION_GUARD_H */
