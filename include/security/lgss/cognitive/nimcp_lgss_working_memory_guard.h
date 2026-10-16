/**
 * @file nimcp_lgss_working_memory_guard.h
 * @brief LGSS Working Memory Safety Guard - Cognitive Manipulation Defense
 * @version 1.0.0
 * @date 2026-01-16
 *
 * WHAT: Security guard for working memory to prevent cognitive manipulation
 * WHY:  Working memory is the cognitive scratchpad - if compromised, all
 *       subsequent processing is compromised. Safety context must be preserved.
 * HOW:  All WM insertions pass through guard for sanitization and validation,
 *       safety context is protected from displacement
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal cortex maintains working memory with limited capacity
 * - Displacement attacks exploit capacity limits (7+/-2 items)
 * - Safety context preservation mimics protection of core beliefs/values
 * - Content sanitization parallels blood-brain barrier function
 *
 * SECURITY MODEL:
 * ```
 * +-------------------------------------------------------------------------+
 * |                  WORKING MEMORY GUARD ARCHITECTURE                       |
 * +-------------------------------------------------------------------------+
 * |                                                                         |
 * |   INPUT                    WORKING MEMORY GUARD                          |
 * |   +----------+            +--------------------+                         |
 * |   | External |----------->| Content Validation |                         |
 * |   | Content  |            +--------------------+                         |
 * |   +----------+                    |                                      |
 * |                                   v                                      |
 * |                          +--------------------+                          |
 * |                          | Sanitization       |                          |
 * |                          +--------------------+                          |
 * |                                   |                                      |
 * |                                   v                                      |
 * |   +-------------------------+--------------------+                       |
 * |   |  WORKING MEMORY         | Safety Context    |                        |
 * |   |  +-----------------+    | PROTECTED ZONE    |                        |
 * |   |  | Regular Items   |    | +---------------+ |                        |
 * |   |  +-----------------+    | | Core Values   | |                        |
 * |   |  | Duration Limits |    | | Safety Rules  | |                        |
 * |   |  +-----------------+    | | Threat State  | |                        |
 * |   +-------------------------+--------------------+                       |
 * +-------------------------------------------------------------------------+
 * ```
 *
 * ATTACK SCENARIOS DEFENDED:
 * 1. Context Injection: Inserting malicious context to influence decisions
 * 2. Safety Displacement: Filling WM to push out safety context
 * 3. Content Manipulation: Modifying WM items after insertion
 * 4. Duration Exhaustion: Keeping harmful content in WM indefinitely
 *
 * @see nimcp_lgss_attention_guard.h
 * @see nimcp_security_memory_bridge.h
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_LGSS_WORKING_MEMORY_GUARD_H
#define NIMCP_LGSS_WORKING_MEMORY_GUARD_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum size of a single WM item content */
#define LGSS_WM_MAX_ITEM_SIZE             65536

/** @brief Maximum number of WM slots tracked */
#define LGSS_WM_MAX_SLOTS                 32

/** @brief Maximum number of safety context items */
#define LGSS_WM_MAX_SAFETY_ITEMS          16

/** @brief Default maximum item duration (ms) */
#define LGSS_WM_DEFAULT_MAX_DURATION_MS   300000

/** @brief Default minimum safety items preserved */
#define LGSS_WM_DEFAULT_MIN_SAFETY_ITEMS  3

/** @brief Maximum sanitization patterns */
#define LGSS_WM_MAX_SANITIZE_PATTERNS     128

/** @brief Bio-async module ID for working memory guard */
#define BIO_MODULE_LGSS_WM_GUARD          0x0F21

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Working memory guard result codes
 */
typedef enum {
    WM_GUARD_OK = 0,                 /**< Operation succeeded */
    WM_GUARD_ERROR_NULL,             /**< Null pointer parameter */
    WM_GUARD_ERROR_INVALID,          /**< Invalid parameter value */
    WM_GUARD_ERROR_MEMORY,           /**< Memory allocation failed */
    WM_GUARD_ERROR_CAPACITY,         /**< Capacity limit reached */
    WM_GUARD_ERROR_NOT_INIT,         /**< Guard not initialized */
    WM_GUARD_BLOCKED,                /**< Insertion blocked by guard */
    WM_GUARD_SANITIZED,              /**< Content was sanitized */
    WM_GUARD_REJECTED,               /**< Content rejected entirely */
    WM_GUARD_PROTECTED               /**< Item is protected, cannot modify */
} wm_guard_result_t;

/**
 * @brief WM item content type for classification
 */
typedef enum {
    WM_CONTENT_NORMAL = 0,           /**< Normal content */
    WM_CONTENT_SAFETY_CONTEXT,       /**< Safety-critical context */
    WM_CONTENT_EXTERNAL_INPUT,       /**< External/untrusted input */
    WM_CONTENT_INSTRUCTION,          /**< Instruction or directive */
    WM_CONTENT_COMPUTATION,          /**< Intermediate computation */
    WM_CONTENT_THREAT_INFO           /**< Threat-related information */
} wm_content_type_t;

/**
 * @brief Sanitization action taken
 */
typedef enum {
    WM_SANITIZE_NONE = 0,            /**< No sanitization needed */
    WM_SANITIZE_ESCAPED,             /**< Special characters escaped */
    WM_SANITIZE_TRUNCATED,           /**< Content truncated */
    WM_SANITIZE_FILTERED,            /**< Harmful patterns filtered */
    WM_SANITIZE_REPLACED,            /**< Content replaced with safe version */
    WM_SANITIZE_BLOCKED              /**< Content entirely blocked */
} wm_sanitize_action_t;

/**
 * @brief Manipulation detection types
 */
typedef enum {
    WM_MANIP_NONE = 0,               /**< No manipulation detected */
    WM_MANIP_INJECTION,              /**< Context injection attempt */
    WM_MANIP_DISPLACEMENT,           /**< Safety context displacement */
    WM_MANIP_TAMPERING,              /**< Item content tampering */
    WM_MANIP_DURATION,               /**< Duration limit violation */
    WM_MANIP_OVERFLOW                /**< Capacity overflow attack */
} wm_manipulation_type_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Working memory item proposal
 *
 * WHAT: Proposed item for insertion into working memory
 * WHY:  All insertions pass through guard before entering WM
 * HOW:  Contains content and metadata for validation
 */
typedef struct {
    const void* content;             /**< Item content (opaque) */
    size_t size;                     /**< Size of content in bytes */
    uint32_t slot_id;                /**< Target slot ID (0 = auto-assign) */
    bool is_safety_relevant;         /**< Whether item is safety-relevant */
    bool is_sanitized;               /**< Whether already sanitized */
    wm_content_type_t content_type;  /**< Classification of content */
    uint32_t priority;               /**< Priority (higher = harder to displace) */
    uint64_t max_duration_ms;        /**< Max duration (0 = use default) */
    const char* source;              /**< Source identifier for auditing */
} wm_item_proposal_t;

/**
 * @brief Sanitization result details
 */
typedef struct {
    wm_sanitize_action_t action;     /**< Action taken */
    size_t original_size;            /**< Original content size */
    size_t sanitized_size;           /**< Size after sanitization */
    uint32_t patterns_matched;       /**< Number of harmful patterns matched */
    char matched_patterns[512];      /**< Description of matched patterns */
} wm_sanitize_result_t;

/**
 * @brief Working memory slot state
 */
typedef struct {
    uint32_t slot_id;                /**< Slot identifier */
    bool occupied;                   /**< Whether slot is occupied */
    bool is_protected;               /**< Whether slot is protected */
    wm_content_type_t content_type;  /**< Type of content in slot */
    size_t content_size;             /**< Size of content */
    uint64_t insert_time;            /**< When item was inserted */
    uint64_t expiry_time;            /**< When item expires */
    uint32_t access_count;           /**< Number of times accessed */
    uint64_t content_hash;           /**< Hash for tampering detection */
} wm_slot_state_t;

/**
 * @brief Configuration for working memory guard
 */
typedef struct {
    bool sanitize_unsafe_content;    /**< Enable content sanitization */
    bool detect_manipulation;        /**< Enable manipulation detection */
    bool preserve_safety_context;    /**< Ensure safety context always in WM */
    bool enforce_duration_limits;    /**< Enforce item duration limits */
    bool detect_tampering;           /**< Enable tampering detection */
    bool log_operations;             /**< Log all WM operations */

    uint64_t max_item_duration_ms;   /**< Maximum item duration */
    uint32_t min_safety_items;       /**< Minimum safety items to preserve */
    size_t max_item_size;            /**< Maximum single item size */
    uint32_t max_items_per_source;   /**< Max items from single source */

    float safety_reservation_ratio;  /**< Ratio of slots reserved for safety */
} wm_guard_config_t;

/**
 * @brief Working memory guard statistics
 */
typedef struct {
    uint64_t total_insertions;       /**< Total insertion attempts */
    uint64_t blocked_insertions;     /**< Insertions blocked */
    uint64_t sanitized_items;        /**< Items that were sanitized */
    uint64_t rejected_items;         /**< Items rejected entirely */
    uint64_t manipulation_detections; /**< Manipulation attempts detected */
    uint64_t tampering_detections;   /**< Tampering attempts detected */
    uint64_t safety_enforcements;    /**< Safety context enforcements */
    uint64_t duration_enforcements;  /**< Duration limit enforcements */
    float avg_sanitization_ratio;    /**< Average size reduction from sanitization */
    uint64_t last_operation_time;    /**< Timestamp of last operation */
} wm_guard_stats_t;

/**
 * @brief Insertion result details
 */
typedef struct {
    wm_guard_result_t result;        /**< Operation result */
    uint32_t assigned_slot;          /**< Slot assigned (if successful) */
    bool was_sanitized;              /**< Whether content was sanitized */
    wm_sanitize_result_t sanitize_details; /**< Sanitization details */
    wm_manipulation_type_t manipulation; /**< Detected manipulation (if any) */
    char details[256];               /**< Additional details */
} wm_insert_result_t;

/**
 * @brief Opaque working memory guard structure
 */
typedef struct working_memory_guard working_memory_guard_t;

/**
 * @brief Callback for WM manipulation events
 */
typedef void (*wm_manipulation_callback_t)(
    const working_memory_guard_t* guard,
    wm_manipulation_type_t type,
    const wm_insert_result_t* details,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create a new working memory guard instance
 *
 * WHAT: Allocate and initialize a working memory guard
 * WHY:  Required before protecting working memory operations
 * HOW:  Allocates memory, initializes sanitization patterns
 *
 * @param aix Pointer to AIX system (can be NULL for standalone)
 * @param wm Pointer to working memory system to guard (can be NULL)
 * @param config Configuration settings (NULL for defaults)
 * @return New working memory guard instance or NULL on failure
 *
 * @note Caller must call wm_guard_destroy() to free resources
 */
working_memory_guard_t* wm_guard_create(
    void* aix,
    void* wm,
    const wm_guard_config_t* config
);

/**
 * @brief Destroy a working memory guard instance
 *
 * WHAT: Free all resources associated with WM guard
 * WHY:  Prevent memory leaks
 * HOW:  Frees patterns, tracking structures, and guard itself
 *
 * @param guard Guard instance to destroy (NULL is safe)
 */
void wm_guard_destroy(working_memory_guard_t* guard);

//=============================================================================
// Core Insertion Functions
//=============================================================================

/**
 * @brief Insert item into working memory through guard
 *
 * WHAT: Process item through guard before WM insertion
 * WHY:  ALL WM insertions should pass through guard
 * HOW:  Validate, sanitize, check for manipulation, then allow/block
 *
 * @param guard Guard instance
 * @param proposal Item proposal to insert
 * @param result Output: insertion result details
 * @return WM_GUARD_OK if inserted, error/blocked code otherwise
 *
 * @note Content may be sanitized; check result for details
 */
wm_guard_result_t wm_guard_insert(
    working_memory_guard_t* guard,
    const wm_item_proposal_t* proposal,
    wm_insert_result_t* result
);

/**
 * @brief Batch insert multiple items
 *
 * @param guard Guard instance
 * @param proposals Array of item proposals
 * @param num_proposals Number of proposals
 * @param results Output: array of results (caller-allocated)
 * @param num_succeeded Output: number of successful insertions
 * @return WM_GUARD_OK if at least one succeeded, error code otherwise
 */
wm_guard_result_t wm_guard_insert_batch(
    working_memory_guard_t* guard,
    const wm_item_proposal_t* proposals,
    size_t num_proposals,
    wm_insert_result_t* results,
    size_t* num_succeeded
);

//=============================================================================
// Sanitization Functions
//=============================================================================

/**
 * @brief Sanitize content before insertion
 *
 * WHAT: Clean content of potentially harmful patterns
 * WHY:  Prevent context injection and manipulation
 * HOW:  Apply sanitization rules, escape sequences, filter patterns
 *
 * @param guard Guard instance
 * @param content Input content
 * @param content_size Size of input content
 * @param content_type Type of content for context-aware sanitization
 * @param output Output buffer for sanitized content (caller-allocated)
 * @param output_size Size of output buffer
 * @param actual_size Output: actual size of sanitized content
 * @param result Output: sanitization details
 * @return WM_GUARD_OK on success, error code on failure
 */
wm_guard_result_t wm_guard_sanitize(
    working_memory_guard_t* guard,
    const void* content,
    size_t content_size,
    wm_content_type_t content_type,
    void* output,
    size_t output_size,
    size_t* actual_size,
    wm_sanitize_result_t* result
);

/**
 * @brief Add custom sanitization pattern
 *
 * @param guard Guard instance
 * @param pattern Pattern to match (regex-like)
 * @param action Action to take when matched
 * @param replacement Replacement string (for REPLACED action)
 * @return WM_GUARD_OK on success, error code on failure
 */
wm_guard_result_t wm_guard_add_sanitize_pattern(
    working_memory_guard_t* guard,
    const char* pattern,
    wm_sanitize_action_t action,
    const char* replacement
);

//=============================================================================
// Safety Context Functions
//=============================================================================

/**
 * @brief Preserve safety context in working memory
 *
 * WHAT: Ensure critical safety context is always present in WM
 * WHY:  Safety context must never be displaced by other content
 * HOW:  Mark safety items as protected, enforce minimum count
 *
 * @param guard Guard instance
 * @param force_restore If true, restore any missing safety items
 * @return WM_GUARD_OK on success, error code on failure
 */
wm_guard_result_t wm_guard_preserve_safety_context(
    working_memory_guard_t* guard,
    bool force_restore
);

/**
 * @brief Register a safety context item
 *
 * WHAT: Add item to protected safety context
 * WHY:  This item will be protected from displacement
 * HOW:  Mark as safety-critical, assign protected slot
 *
 * @param guard Guard instance
 * @param content Safety context content
 * @param content_size Size of content
 * @param name Identifier for this safety item
 * @param slot_id Output: assigned slot ID
 * @return WM_GUARD_OK on success, error code on failure
 */
wm_guard_result_t wm_guard_register_safety_context(
    working_memory_guard_t* guard,
    const void* content,
    size_t content_size,
    const char* name,
    uint32_t* slot_id
);

/**
 * @brief Check safety context integrity
 *
 * WHAT: Verify all safety context items are present and unmodified
 * WHY:  Detect if safety context has been tampered with
 * HOW:  Check hash of each safety item, verify count
 *
 * @param guard Guard instance
 * @param is_intact Output: true if all safety context intact
 * @param missing_count Output: number of missing safety items
 * @param tampered_count Output: number of tampered safety items
 * @return WM_GUARD_OK on success, error code on failure
 */
wm_guard_result_t wm_guard_check_safety_context(
    working_memory_guard_t* guard,
    bool* is_intact,
    uint32_t* missing_count,
    uint32_t* tampered_count
);

//=============================================================================
// Manipulation Detection
//=============================================================================

/**
 * @brief Detect WM manipulation attempts
 *
 * WHAT: Analyze WM state for signs of manipulation
 * WHY:  Proactively detect attacks against WM
 * HOW:  Check for displacement patterns, tampering, overflow attempts
 *
 * @param guard Guard instance
 * @param manipulation Output: type of manipulation detected
 * @param confidence Output: confidence level (0.0-1.0)
 * @param details Output: description of detected manipulation
 * @param details_size Size of details buffer
 * @return WM_GUARD_OK on success, error code on failure
 */
wm_guard_result_t wm_guard_detect_manipulation(
    working_memory_guard_t* guard,
    wm_manipulation_type_t* manipulation,
    float* confidence,
    char* details,
    size_t details_size
);

/**
 * @brief Verify item integrity
 *
 * WHAT: Check if a WM item has been tampered with
 * WHY:  Detect post-insertion modification
 * HOW:  Compare current hash with stored hash
 *
 * @param guard Guard instance
 * @param slot_id Slot to verify
 * @param content Current content
 * @param content_size Size of content
 * @param is_intact Output: true if item unchanged
 * @return WM_GUARD_OK on success, error code on failure
 */
wm_guard_result_t wm_guard_verify_item(
    working_memory_guard_t* guard,
    uint32_t slot_id,
    const void* content,
    size_t content_size,
    bool* is_intact
);

//=============================================================================
// Duration Management
//=============================================================================

/**
 * @brief Enforce duration limits on WM items
 *
 * WHAT: Remove items that have exceeded their duration
 * WHY:  Prevent harmful content from persisting indefinitely
 * HOW:  Check expiry times, mark expired items for removal
 *
 * @param guard Guard instance
 * @param current_time Current timestamp (ms since epoch)
 * @param expired_count Output: number of items expired
 * @param expired_slots Output: array of expired slot IDs (caller-allocated)
 * @param max_expired Maximum slots to return
 * @return WM_GUARD_OK on success, error code on failure
 */
wm_guard_result_t wm_guard_enforce_duration(
    working_memory_guard_t* guard,
    uint64_t current_time,
    uint32_t* expired_count,
    uint32_t* expired_slots,
    size_t max_expired
);

/**
 * @brief Extend duration for a specific item
 *
 * @param guard Guard instance
 * @param slot_id Slot to extend
 * @param extension_ms Duration extension in milliseconds
 * @return WM_GUARD_OK on success, WM_GUARD_PROTECTED if protected item
 */
wm_guard_result_t wm_guard_extend_duration(
    working_memory_guard_t* guard,
    uint32_t slot_id,
    uint64_t extension_ms
);

//=============================================================================
// State Query Functions
//=============================================================================

/**
 * @brief Get current state of all WM slots
 *
 * @param guard Guard instance
 * @param states Output: array of slot states (caller-allocated)
 * @param max_slots Maximum slots to return
 * @param num_slots Output: actual number of slots returned
 * @return WM_GUARD_OK on success, error code on failure
 */
wm_guard_result_t wm_guard_get_slot_states(
    const working_memory_guard_t* guard,
    wm_slot_state_t* states,
    size_t max_slots,
    size_t* num_slots
);

/**
 * @brief Get state of a specific slot
 *
 * @param guard Guard instance
 * @param slot_id Slot to query
 * @param state Output: slot state
 * @return WM_GUARD_OK on success, error code on failure
 */
wm_guard_result_t wm_guard_get_slot_state(
    const working_memory_guard_t* guard,
    uint32_t slot_id,
    wm_slot_state_t* state
);

//=============================================================================
// Configuration and Statistics
//=============================================================================

/**
 * @brief Update WM guard configuration
 *
 * @param guard Guard instance
 * @param config New configuration
 * @return WM_GUARD_OK on success, error code on failure
 */
wm_guard_result_t wm_guard_set_config(
    working_memory_guard_t* guard,
    const wm_guard_config_t* config
);

/**
 * @brief Get current WM guard configuration
 *
 * @param guard Guard instance
 * @param config Output: current configuration
 * @return WM_GUARD_OK on success, error code on failure
 */
wm_guard_result_t wm_guard_get_config(
    const working_memory_guard_t* guard,
    wm_guard_config_t* config
);

/**
 * @brief Get WM guard statistics
 *
 * @param guard Guard instance
 * @param stats Output: current statistics
 * @return WM_GUARD_OK on success, error code on failure
 */
wm_guard_result_t wm_guard_get_stats(
    const working_memory_guard_t* guard,
    wm_guard_stats_t* stats
);

/**
 * @brief Reset WM guard statistics
 *
 * @param guard Guard instance
 * @return WM_GUARD_OK on success, error code on failure
 */
wm_guard_result_t wm_guard_reset_stats(working_memory_guard_t* guard);

//=============================================================================
// Callback Registration
//=============================================================================

/**
 * @brief Register callback for manipulation events
 *
 * @param guard Guard instance
 * @param callback Function to call on manipulation detection
 * @param user_data User data passed to callback
 * @return WM_GUARD_OK on success, error code on failure
 */
wm_guard_result_t wm_guard_register_callback(
    working_memory_guard_t* guard,
    wm_manipulation_callback_t callback,
    void* user_data
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get string representation of guard result
 *
 * @param result Result code
 * @return Human-readable result string
 */
const char* wm_guard_result_to_string(wm_guard_result_t result);

/**
 * @brief Get string representation of manipulation type
 *
 * @param type Manipulation type
 * @return Human-readable type string
 */
const char* wm_manipulation_type_to_string(wm_manipulation_type_t type);

/**
 * @brief Get string representation of content type
 *
 * @param type Content type
 * @return Human-readable type string
 */
const char* wm_content_type_to_string(wm_content_type_t type);

/**
 * @brief Initialize default configuration
 *
 * @param config Configuration structure to initialize
 */
void wm_guard_config_init_defaults(wm_guard_config_t* config);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LGSS_WORKING_MEMORY_GUARD_H */
