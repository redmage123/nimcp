//=============================================================================
// nimcp_pr_memory_node.h - Prime Resonant Memory Node with COW Support
//=============================================================================
/**
 * @file nimcp_pr_memory_node.h
 * @brief COW-enabled memory nodes with prime signatures and quaternion states
 *
 * WHAT: Memory nodes that store content with prime signatures, quaternion states,
 *       and Z-ladder tier information, using Copy-on-Write for efficiency
 * WHY:  Enable content-addressable retrieval with semantic metadata while
 *       minimizing memory duplication through COW sharing
 * HOW:  Combines prime signatures for content fingerprinting, quaternions for
 *       semantic state (consolidation, emotion, salience, accessibility), and
 *       unified memory with COW for efficient data sharing
 *
 * ARCHITECTURE:
 *
 *   Prime Resonant Memory Node Structure:
 *   +-----------------------------------------------------------------------+
 *   |                         PR Memory Node                                |
 *   |  +---------------------+  +----------------------+                    |
 *   |  | Prime Signature     |  | Quaternion State     |                    |
 *   |  | (content fingerprint)|  | (w,x,y,z semantics)  |                    |
 *   |  +---------------------+  +----------------------+                    |
 *   |  +---------------------+  +----------------------+                    |
 *   |  | Z-Ladder Tier       |  | Temporal Info        |                    |
 *   |  | (Z0-Z3 consolidation)|  | (timestamps, access) |                    |
 *   |  +---------------------+  +----------------------+                    |
 *   |  +-----------------------------------------------------+              |
 *   |  | COW-Managed Data Handle (unified_mem_handle_t)     |              |
 *   |  | - Shared template for efficient cloning             |              |
 *   |  | - Private copy on first write                       |              |
 *   |  +-----------------------------------------------------+              |
 *   +-----------------------------------------------------------------------+
 *
 *   Z-Ladder Memory Tiers (Biological Analogs):
 *   +-----------------------------------------------------------------------+
 *   | Tier | Name        | Capacity   | Decay Rate | Biological Analog      |
 *   |------|-------------|------------|------------|------------------------|
 *   | Z0   | Working     | 7+/-2 items| ~30 sec    | Prefrontal cortex      |
 *   | Z1   | Short-term  | ~100 items | ~hours     | Hippocampus            |
 *   | Z2   | Long-term   | ~10K items | ~days      | Neocortical patterns   |
 *   | Z3   | Permanent   | unlimited  | never      | Semantic/procedural    |
 *   +-----------------------------------------------------------------------+
 *
 *   Quaternion State Encoding (from nimcp_quaternion.h):
 *   +-----------------------------------------------------------------------+
 *   | Component | Meaning              | Range     | Effect on Memory       |
 *   |-----------|----------------------|-----------|------------------------|
 *   | w         | Consolidation        | [0, 1]    | Resistance to decay    |
 *   | x         | Emotional valence    | [-1, +1]  | Priority in retrieval  |
 *   | y         | Salience             | [0, 1]    | Attention weight       |
 *   | z         | Accessibility        | [0, 1]    | Ease of retrieval      |
 *   +-----------------------------------------------------------------------+
 *
 *   COW Lifecycle:
 *   +-----------------------------------------------------------------------+
 *   |  1. Create node with initial data                                     |
 *   |     -> Data stored in unified_mem_handle_t (may be shared)            |
 *   |                                                                       |
 *   |  2. Clone node for parallel access                                    |
 *   |     -> Fast O(1) clone via COW reference counting                     |
 *   |     -> Both nodes share same underlying data                          |
 *   |                                                                       |
 *   |  3. Read data (multiple concurrent readers OK)                        |
 *   |     -> Returns const pointer, no copy triggered                       |
 *   |                                                                       |
 *   |  4. Write data (first writer triggers COW)                            |
 *   |     -> Private copy allocated, node becomes independent               |
 *   |     -> Other clones continue sharing original                         |
 *   +-----------------------------------------------------------------------+
 *
 * PERFORMANCE:
 * - Node creation: O(n) where n = data_size
 * - Node clone: O(1) via COW reference counting
 * - Read access: O(1) lock-free
 * - Write access: O(1) if private, O(n) if COW trigger
 * - Signature update: O(n) hash computation
 * - State update: O(1) quaternion assignment
 *
 * MEMORY:
 * - pr_memory_node_t: ~192 bytes (fixed overhead)
 * - Prime signature: embedded (64 exponents + hash)
 * - Data: shared via COW until modified
 *
 * THREAD SAFETY:
 * - Creation/destruction: NOT thread-safe (external sync required)
 * - Read operations: Thread-safe (lock-free)
 * - Write operations: Thread-safe (mutex on COW trigger)
 * - State updates: Atomic operations used where possible
 *
 * INTEGRATION:
 * - Prime Signature: Content-based retrieval and similarity
 * - Quaternion: Semantic state for resonance scoring
 * - Unified Memory: COW sharing and snapshot support
 * - Entanglement Graph: Links managed externally via entanglement_count
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#ifndef NIMCP_PR_MEMORY_NODE_H
#define NIMCP_PR_MEMORY_NODE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>

// Dependencies
#include "cognitive/memory/core/nimcp_quaternion.h"
#include "cognitive/memory/core/nimcp_prime_signature.h"
#include "utils/memory/nimcp_unified_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

// Export macro (for shared library builds)
#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

//=============================================================================
// Constants
//=============================================================================

/** Maximum number of entanglement links per node */
#define PR_NODE_MAX_ENTANGLEMENTS 64

/** Default decay rate for Z0 (working memory) - ~30 seconds */
#define PR_NODE_DECAY_Z0 0.033f

/** Default decay rate for Z1 (short-term) - ~1 hour = 3600s */
#define PR_NODE_DECAY_Z1 0.00028f

/** Default decay rate for Z2 (long-term) - ~1 day = 86400s */
#define PR_NODE_DECAY_Z2 0.0000116f

/** Default decay rate for Z3 (permanent) - no decay */
#define PR_NODE_DECAY_Z3 0.0f

/** Minimum promotion eligibility threshold */
#define PR_NODE_MIN_PROMOTION_ELIGIBILITY 0.0f

/** Maximum promotion eligibility threshold */
#define PR_NODE_MAX_PROMOTION_ELIGIBILITY 1.0f

/** Default promotion threshold for tier transition */
#define PR_NODE_PROMOTION_THRESHOLD 0.75f

/** Epsilon for floating point comparisons */
#define PR_NODE_EPSILON 1e-6f

/** Invalid node ID sentinel value */
#define PR_NODE_INVALID_ID UINT64_MAX

/** Serialization format version */
#define PR_NODE_SERIALIZATION_VERSION 1

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Memory tier enumeration (Z-ladder positions)
 *
 * Models biological memory consolidation stages:
 * - Z0: Working memory (prefrontal, ~7 items, seconds)
 * - Z1: Short-term (hippocampal, ~100 items, hours)
 * - Z2: Long-term (neocortical, ~10K items, days)
 * - Z3: Permanent (semantic/procedural, unlimited)
 */
typedef enum {
    PR_MEMORY_TIER_Z0 = 0,  /**< Working memory (seconds, 7+/-2 items) */
    PR_MEMORY_TIER_Z1,      /**< Short-term (hours, ~100 items) */
    PR_MEMORY_TIER_Z2,      /**< Long-term (days, ~10,000 items) */
    PR_MEMORY_TIER_Z3,      /**< Permanent (unlimited) */
    PR_MEMORY_TIER_COUNT    /**< Number of tiers (for array sizing) */
} pr_memory_tier_t;

/**
 * @brief Memory node state flags
 */
typedef enum {
    PR_NODE_FLAG_NONE         = 0x00,  /**< No flags set */
    PR_NODE_FLAG_LOCKED       = 0x01,  /**< Node is locked for consolidation */
    PR_NODE_FLAG_DECAYING     = 0x02,  /**< Node is currently decaying */
    PR_NODE_FLAG_PROMOTING    = 0x04,  /**< Node is being promoted */
    PR_NODE_FLAG_DEMOTING     = 0x08,  /**< Node is being demoted */
    PR_NODE_FLAG_DIRTY        = 0x10,  /**< Signature needs recomputation */
    PR_NODE_FLAG_SERIALIZING  = 0x20   /**< Node is being serialized */
} pr_node_flags_t;

/**
 * @brief Error codes for memory node operations
 */
typedef enum {
    PR_NODE_SUCCESS = 0,               /**< Operation succeeded */
    PR_NODE_ERROR_NULL_POINTER = -1,   /**< NULL pointer argument */
    PR_NODE_ERROR_INVALID_TIER = -2,   /**< Invalid tier specified */
    PR_NODE_ERROR_INVALID_STATE = -3,  /**< Invalid quaternion state */
    PR_NODE_ERROR_NO_MEMORY = -4,      /**< Memory allocation failed */
    PR_NODE_ERROR_COW_FAILED = -5,     /**< COW operation failed */
    PR_NODE_ERROR_LOCKED = -6,         /**< Node is locked */
    PR_NODE_ERROR_ALREADY_TOP = -7,    /**< Already at highest tier */
    PR_NODE_ERROR_ALREADY_BOTTOM = -8, /**< Already at lowest tier */
    PR_NODE_ERROR_SERIALIZE = -9,      /**< Serialization failed */
    PR_NODE_ERROR_DESERIALIZE = -10,   /**< Deserialization failed */
    PR_NODE_ERROR_VERSION = -11,       /**< Incompatible version */
    PR_NODE_ERROR_INVALID_SIZE = -12   /**< Invalid data size */
} pr_node_error_t;

/**
 * @brief Prime Resonant Memory Node
 *
 * Core data structure for the Prime Resonant cognitive memory architecture.
 * Combines content-addressable indexing via prime signatures with semantic
 * state encoding via quaternions, supporting efficient COW cloning.
 *
 * Memory layout: ~192 bytes (excluding variable-size data payload)
 */
typedef struct {
    //-------------------------------------------------------------------------
    // Identity
    //-------------------------------------------------------------------------
    uint64_t node_id;                    /**< Unique node identifier */

    //-------------------------------------------------------------------------
    // Content Signature
    //-------------------------------------------------------------------------
    prime_signature_t signature;         /**< Content signature for retrieval */

    //-------------------------------------------------------------------------
    // Semantic State
    //-------------------------------------------------------------------------
    nimcp_quaternion_t state;            /**< Semantic metadata (w,x,y,z) */

    //-------------------------------------------------------------------------
    // Tier Information
    //-------------------------------------------------------------------------
    pr_memory_tier_t tier;               /**< Z-ladder tier (Z0-Z3) */

    //-------------------------------------------------------------------------
    // COW-Managed Payload
    //-------------------------------------------------------------------------
    unified_mem_handle_t data_handle;    /**< Unified memory with COW */
    size_t data_size;                    /**< Size of data payload in bytes */

    //-------------------------------------------------------------------------
    // Temporal Information
    //-------------------------------------------------------------------------
    uint64_t created_time_ms;            /**< Creation timestamp (ms since epoch) */
    uint64_t last_accessed_ms;           /**< Last access timestamp */
    _Atomic uint64_t access_count;       /**< Total access count (atomic) */

    //-------------------------------------------------------------------------
    // Consolidation State
    //-------------------------------------------------------------------------
    float decay_rate;                    /**< Per-tier decay rate (per second) */
    float promotion_eligibility;         /**< 0-1 readiness for tier promotion */
    float current_strength;              /**< Current memory strength [0, 1] */

    //-------------------------------------------------------------------------
    // Entanglement Information
    //-------------------------------------------------------------------------
    _Atomic uint32_t entanglement_count; /**< Number of entangled nodes */

    //-------------------------------------------------------------------------
    // Flags and Metadata
    //-------------------------------------------------------------------------
    _Atomic uint32_t flags;              /**< State flags (pr_node_flags_t) */
    uint32_t reserved;                   /**< Reserved for future use */

} pr_memory_node_t;

/**
 * @brief Configuration for node creation
 */
typedef struct {
    pr_memory_tier_t initial_tier;       /**< Starting tier (default: Z0) */
    float initial_strength;              /**< Initial strength [0, 1] (default: 1.0) */
    float emotional_valence;             /**< Initial emotion [-1, +1] (default: 0) */
    float salience;                      /**< Initial salience [0, 1] (default: 0.5) */
    float accessibility;                 /**< Initial accessibility [0, 1] (default: 1.0) */
    bool compute_signature;              /**< Auto-compute prime signature (default: true) */
    bool enable_cow;                     /**< Enable COW for data (default: true) */
} pr_node_config_t;

/**
 * @brief Statistics for a memory node
 */
typedef struct {
    uint64_t node_id;                    /**< Node identifier */
    pr_memory_tier_t tier;               /**< Current tier */
    size_t data_size;                    /**< Data payload size */
    uint64_t access_count;               /**< Total accesses */
    uint64_t age_ms;                     /**< Age since creation */
    uint64_t idle_ms;                    /**< Time since last access */
    float current_strength;              /**< Current memory strength */
    float promotion_eligibility;         /**< Promotion readiness */
    bool is_cow_shared;                  /**< Using shared COW data */
    uint32_t entanglement_count;         /**< Number of entanglements */
    uint32_t flags;                      /**< Current flags */
} pr_node_stats_t;

/**
 * @brief Serialized node header
 */
typedef struct {
    uint32_t magic;                      /**< Magic number for validation */
    uint32_t version;                    /**< Serialization version */
    uint64_t node_id;                    /**< Node identifier */
    uint64_t data_size;                  /**< Size of serialized data */
    uint64_t total_size;                 /**< Total serialized size */
    uint32_t checksum;                   /**< CRC32 checksum */
    uint32_t flags;                      /**< Serialization flags */
} pr_node_serial_header_t;

//=============================================================================
// Node Manager (for ID generation and tracking)
//=============================================================================

/**
 * @brief Memory node manager handle (opaque)
 *
 * Manages node ID generation and optionally tracks all nodes.
 * Required for node creation to ensure unique IDs.
 */
typedef struct pr_node_manager_struct* pr_node_manager_t;

/**
 * @brief Node manager configuration
 */
typedef struct {
    unified_mem_manager_t mem_manager;   /**< Unified memory manager to use */
    uint64_t starting_id;                /**< Starting ID for nodes (default: 1) */
    bool track_nodes;                    /**< Track all created nodes (default: false) */
    size_t max_tracked_nodes;            /**< Max nodes to track (if tracking enabled) */
} pr_node_manager_config_t;

//=============================================================================
// Node Manager API
//=============================================================================

/**
 * @brief Create a node manager
 *
 * WHAT: Creates manager for generating unique node IDs
 * WHY:  Ensure globally unique node identifiers
 * HOW:  Atomic counter for ID generation, optional node tracking
 *
 * @param config Configuration (NULL for defaults)
 * @return Manager handle or NULL on failure
 *
 * COMPLEXITY: O(1)
 *
 * EXAMPLE:
 * ```c
 * pr_node_manager_config_t cfg = {
 *     .mem_manager = my_mem_manager,
 *     .starting_id = 1,
 *     .track_nodes = false
 * };
 * pr_node_manager_t mgr = pr_node_manager_create(&cfg);
 * ```
 */
NIMCP_EXPORT pr_node_manager_t pr_node_manager_create(
    const pr_node_manager_config_t* config
);

/**
 * @brief Destroy a node manager
 *
 * @param manager Manager to destroy
 *
 * WARNING: Does not destroy tracked nodes - destroy them first
 */
NIMCP_EXPORT void pr_node_manager_destroy(pr_node_manager_t manager);

/**
 * @brief Get the unified memory manager
 *
 * @param manager Node manager
 * @return Unified memory manager handle
 */
NIMCP_EXPORT unified_mem_manager_t pr_node_manager_get_mem_manager(
    pr_node_manager_t manager
);

/**
 * @brief Get current node count
 *
 * @param manager Node manager
 * @return Number of nodes created (or tracked if tracking enabled)
 */
NIMCP_EXPORT uint64_t pr_node_manager_get_node_count(pr_node_manager_t manager);

/**
 * @brief Get default node manager configuration
 *
 * @return Default configuration values
 */
NIMCP_EXPORT pr_node_manager_config_t pr_node_manager_default_config(void);

//=============================================================================
// Node Creation and Destruction
//=============================================================================

/**
 * @brief Create a new memory node
 *
 * WHAT: Creates PR memory node with initial data and configuration
 * WHY:  Primary entry point for adding content to memory system
 * HOW:  Allocates node, computes signature, initializes state, stores data via COW
 *
 * @param manager Node manager for ID generation
 * @param data Initial data to store (copied to COW handle)
 * @param data_size Size of data in bytes
 * @param config Node configuration (NULL for defaults)
 * @return New node or NULL on failure
 *
 * COMPLEXITY: O(n) where n = data_size (for signature computation)
 * MEMORY: ~192 bytes + data_size (shared via COW)
 * THREAD SAFETY: Thread-safe (atomic ID generation)
 *
 * EXAMPLE:
 * ```c
 * const char* content = "Important memory content";
 * pr_node_config_t cfg = {
 *     .initial_tier = PR_MEMORY_TIER_Z0,
 *     .initial_strength = 1.0f,
 *     .emotional_valence = 0.5f,  // Positive emotion
 *     .compute_signature = true
 * };
 * pr_memory_node_t* node = pr_memory_node_create(
 *     manager, content, strlen(content), &cfg);
 * ```
 */
NIMCP_EXPORT pr_memory_node_t* pr_memory_node_create(
    pr_node_manager_t manager,
    const void* data,
    size_t data_size,
    const pr_node_config_t* config
);

/**
 * @brief Create node with explicit signature
 *
 * Creates node with a pre-computed or custom signature.
 * Useful when content is derived from external sources.
 *
 * @param manager Node manager
 * @param data Initial data
 * @param data_size Data size
 * @param signature Pre-computed signature
 * @param config Node configuration
 * @return New node or NULL on failure
 */
NIMCP_EXPORT pr_memory_node_t* pr_memory_node_create_with_signature(
    pr_node_manager_t manager,
    const void* data,
    size_t data_size,
    const prime_signature_t* signature,
    const pr_node_config_t* config
);

/**
 * @brief Destroy a memory node
 *
 * WHAT: Releases node and its COW handle
 * WHY:  Return resources to system
 * HOW:  Releases COW handle (decrements refcount), frees node
 *
 * @param node Node to destroy (NULL safe)
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Thread-safe for the release, but don't destroy
 *                nodes that other threads are accessing
 */
NIMCP_EXPORT void pr_memory_node_destroy(pr_memory_node_t* node);

/**
 * @brief Clone a memory node (COW)
 *
 * WHAT: Creates fast COW clone sharing underlying data
 * WHY:  Efficient duplication for parallel processing
 * HOW:  Copies metadata, acquires COW reference to data
 *
 * @param node Node to clone
 * @param manager Manager for new node ID
 * @return Cloned node or NULL on failure
 *
 * COMPLEXITY: O(1) - just reference counting
 * MEMORY: ~192 bytes (data shared until write)
 *
 * The cloned node:
 * - Gets a new unique ID
 * - Shares the underlying data via COW
 * - Has identical signature and state
 * - Has independent temporal counters (reset to now)
 *
 * EXAMPLE:
 * ```c
 * pr_memory_node_t* clone = pr_memory_node_clone(original, manager);
 * // clone shares data with original until modified
 * void* data = pr_memory_node_write(clone);  // Triggers COW
 * // Now clone has independent copy
 * ```
 */
NIMCP_EXPORT pr_memory_node_t* pr_memory_node_clone(
    const pr_memory_node_t* node,
    pr_node_manager_t manager
);

/**
 * @brief Get default node configuration
 *
 * @return Default configuration values
 */
NIMCP_EXPORT pr_node_config_t pr_memory_node_default_config(void);

//=============================================================================
// Data Access
//=============================================================================

/**
 * @brief Get read-only pointer to node data
 *
 * WHAT: Returns const pointer for reading without COW trigger
 * WHY:  Fast read access to shared data
 * HOW:  Direct pointer from COW handle
 *
 * @param node Memory node
 * @return const pointer to data, or NULL on error
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Lock-free, thread-safe for concurrent reads
 *
 * IMPORTANT: Do NOT cast away const and write!
 *
 * Side effects:
 * - Updates last_accessed_ms
 * - Increments access_count (atomic)
 */
NIMCP_EXPORT const void* pr_memory_node_read(pr_memory_node_t* node);

/**
 * @brief Get writable pointer to node data
 *
 * WHAT: Returns writable pointer, triggers COW if shared
 * WHY:  Enable modification while preserving shared copies
 * HOW:  Calls unified_mem_write on data handle
 *
 * @param node Memory node
 * @return Writable pointer or NULL on failure
 *
 * COMPLEXITY: O(1) if private, O(n) if COW triggered
 * THREAD SAFETY: Thread-safe (mutex on COW)
 *
 * Side effects:
 * - May trigger COW copy (if data was shared)
 * - Sets FLAG_DIRTY (signature may need recomputation)
 * - Updates last_accessed_ms
 * - Increments access_count (atomic)
 */
NIMCP_EXPORT void* pr_memory_node_write(pr_memory_node_t* node);

/**
 * @brief Check if node is using shared COW data
 *
 * @param node Memory node
 * @return true if data is shared, false if private or invalid
 */
NIMCP_EXPORT bool pr_memory_node_is_shared(const pr_memory_node_t* node);

/**
 * @brief Get node data size
 *
 * @param node Memory node
 * @return Data size in bytes
 */
NIMCP_EXPORT size_t pr_memory_node_get_data_size(const pr_memory_node_t* node);

/**
 * @brief Force node to have private data copy
 *
 * Pre-emptively triggers COW if data is shared.
 *
 * @param node Memory node
 * @return PR_NODE_SUCCESS or error code
 */
NIMCP_EXPORT pr_node_error_t pr_memory_node_make_private(pr_memory_node_t* node);

//=============================================================================
// State Management
//=============================================================================

/**
 * @brief Update node quaternion state
 *
 * WHAT: Sets new quaternion state for semantic metadata
 * WHY:  Allow runtime modification of consolidation/emotion/salience/access
 * HOW:  Direct assignment with validation
 *
 * @param node Memory node
 * @param state New quaternion state
 * @return PR_NODE_SUCCESS or error code
 *
 * The state components:
 * - w (consolidation): [0, 1] - how firmly encoded
 * - x (emotion): [-1, +1] - affective valence
 * - y (salience): [0, 1] - attention weight
 * - z (accessibility): [0, 1] - retrieval ease
 *
 * Note: State is NOT normalized; components are clamped to valid ranges.
 */
NIMCP_EXPORT pr_node_error_t pr_memory_node_update_state(
    pr_memory_node_t* node,
    nimcp_quaternion_t state
);

/**
 * @brief Get current quaternion state
 *
 * @param node Memory node
 * @return Current state (identity quaternion if node is NULL)
 */
NIMCP_EXPORT nimcp_quaternion_t pr_memory_node_get_state(
    const pr_memory_node_t* node
);

/**
 * @brief Blend state with another quaternion
 *
 * Uses SLERP interpolation for smooth blending.
 *
 * @param node Memory node
 * @param other State to blend with
 * @param t Blend factor [0, 1] (0=keep current, 1=use other)
 * @return PR_NODE_SUCCESS or error code
 */
NIMCP_EXPORT pr_node_error_t pr_memory_node_blend_state(
    pr_memory_node_t* node,
    nimcp_quaternion_t other,
    float t
);

/**
 * @brief Update individual state components
 *
 * @param node Memory node
 * @param consolidation New consolidation value [0, 1] or NaN to keep
 * @param emotion New emotion value [-1, +1] or NaN to keep
 * @param salience New salience value [0, 1] or NaN to keep
 * @param accessibility New accessibility value [0, 1] or NaN to keep
 * @return PR_NODE_SUCCESS or error code
 */
NIMCP_EXPORT pr_node_error_t pr_memory_node_update_state_components(
    pr_memory_node_t* node,
    float consolidation,
    float emotion,
    float salience,
    float accessibility
);

//=============================================================================
// Signature Management
//=============================================================================

/**
 * @brief Recompute prime signature from current data
 *
 * WHAT: Updates signature based on node's data content
 * WHY:  Required after data modification to maintain content-addressable indexing
 * HOW:  Reads data, computes new signature, replaces old
 *
 * @param node Memory node
 * @return PR_NODE_SUCCESS or error code
 *
 * COMPLEXITY: O(n) where n = data_size
 *
 * Clears FLAG_DIRTY on success.
 */
NIMCP_EXPORT pr_node_error_t pr_memory_node_update_signature(
    pr_memory_node_t* node
);

/**
 * @brief Set signature explicitly
 *
 * @param node Memory node
 * @param signature New signature (copied)
 * @return PR_NODE_SUCCESS or error code
 */
NIMCP_EXPORT pr_node_error_t pr_memory_node_set_signature(
    pr_memory_node_t* node,
    const prime_signature_t* signature
);

/**
 * @brief Get pointer to node's signature
 *
 * @param node Memory node
 * @return const pointer to signature, or NULL if node is NULL
 */
NIMCP_EXPORT const prime_signature_t* pr_memory_node_get_signature(
    const pr_memory_node_t* node
);

/**
 * @brief Compute similarity to another node's signature
 *
 * @param node Memory node
 * @param other Other node to compare
 * @param method Similarity method (Jaccard, Cosine, etc.)
 * @return Similarity value [0, 1] or -1 on error
 */
NIMCP_EXPORT float pr_memory_node_signature_similarity(
    const pr_memory_node_t* node,
    const pr_memory_node_t* other,
    prime_sig_similarity_method_t method
);

//=============================================================================
// Tier Management
//=============================================================================

/**
 * @brief Promote node to higher tier
 *
 * WHAT: Moves node up the Z-ladder (Z0->Z1->Z2->Z3)
 * WHY:  Consolidation of important memories to longer-term storage
 * HOW:  Updates tier, adjusts decay rate, resets promotion eligibility
 *
 * @param node Memory node
 * @return PR_NODE_SUCCESS, PR_NODE_ERROR_ALREADY_TOP, or other error
 *
 * Effects:
 * - Tier incremented
 * - Decay rate updated to new tier's default
 * - Promotion eligibility reset to 0
 * - FLAG_PROMOTING cleared
 */
NIMCP_EXPORT pr_node_error_t pr_memory_node_promote(pr_memory_node_t* node);

/**
 * @brief Demote node to lower tier
 *
 * WHAT: Moves node down the Z-ladder (Z3->Z2->Z1->Z0)
 * WHY:  Memory decay or deliberate forgetting
 * HOW:  Updates tier, adjusts decay rate
 *
 * @param node Memory node
 * @return PR_NODE_SUCCESS, PR_NODE_ERROR_ALREADY_BOTTOM, or other error
 *
 * Effects:
 * - Tier decremented
 * - Decay rate updated to new tier's default
 * - FLAG_DEMOTING cleared
 */
NIMCP_EXPORT pr_node_error_t pr_memory_node_demote(pr_memory_node_t* node);

/**
 * @brief Set tier explicitly
 *
 * @param node Memory node
 * @param tier New tier
 * @return PR_NODE_SUCCESS or error code
 */
NIMCP_EXPORT pr_node_error_t pr_memory_node_set_tier(
    pr_memory_node_t* node,
    pr_memory_tier_t tier
);

/**
 * @brief Get current tier
 *
 * @param node Memory node
 * @return Current tier or PR_MEMORY_TIER_Z0 if node is NULL
 */
NIMCP_EXPORT pr_memory_tier_t pr_memory_node_get_tier(
    const pr_memory_node_t* node
);

/**
 * @brief Get tier name as string
 *
 * @param tier Memory tier
 * @return Human-readable tier name
 */
NIMCP_EXPORT const char* pr_memory_tier_name(pr_memory_tier_t tier);

//=============================================================================
// Decay and Consolidation
//=============================================================================

/**
 * @brief Apply time-based decay to memory strength
 *
 * WHAT: Reduces current_strength based on tier's decay rate
 * WHY:  Model biological memory decay over time
 * HOW:  Exponential decay: strength *= exp(-decay_rate * elapsed_seconds)
 *
 * @param node Memory node
 * @param elapsed_seconds Time since last decay application
 * @return New strength value [0, 1]
 *
 * Formula: strength_new = strength_old * e^(-decay_rate * dt)
 *
 * Notes:
 * - Z3 (permanent) has zero decay rate
 * - Sets FLAG_DECAYING during operation
 * - If strength drops below threshold, may trigger demotion
 */
NIMCP_EXPORT float pr_memory_node_apply_decay(
    pr_memory_node_t* node,
    float elapsed_seconds
);

/**
 * @brief Reinforce memory (counter decay)
 *
 * Increases current_strength and updates promotion eligibility.
 *
 * @param node Memory node
 * @param reinforcement Amount to add to strength [0, 1]
 * @return New strength value (clamped to [0, 1])
 */
NIMCP_EXPORT float pr_memory_node_reinforce(
    pr_memory_node_t* node,
    float reinforcement
);

/**
 * @brief Check promotion eligibility
 *
 * WHAT: Evaluates if node is ready for tier promotion
 * WHY:  Gate promotion decisions
 * HOW:  Checks eligibility score, access patterns, strength
 *
 * @param node Memory node
 * @return true if eligible for promotion
 *
 * Criteria (all must be met):
 * - promotion_eligibility >= PR_NODE_PROMOTION_THRESHOLD
 * - current_strength >= 0.5
 * - tier < PR_MEMORY_TIER_Z3
 */
NIMCP_EXPORT bool pr_memory_node_check_eligibility(
    const pr_memory_node_t* node
);

/**
 * @brief Update promotion eligibility based on access patterns
 *
 * @param node Memory node
 * @param access_boost Boost from recent access [0, 1]
 * @param time_factor Time-based factor (older with sustained access = higher)
 * @return New eligibility value [0, 1]
 */
NIMCP_EXPORT float pr_memory_node_update_eligibility(
    pr_memory_node_t* node,
    float access_boost,
    float time_factor
);

/**
 * @brief Get default decay rate for a tier
 *
 * @param tier Memory tier
 * @return Default decay rate (per second)
 */
NIMCP_EXPORT float pr_memory_node_default_decay_rate(pr_memory_tier_t tier);

/**
 * @brief Set custom decay rate
 *
 * @param node Memory node
 * @param decay_rate New decay rate (per second)
 * @return PR_NODE_SUCCESS or error code
 */
NIMCP_EXPORT pr_node_error_t pr_memory_node_set_decay_rate(
    pr_memory_node_t* node,
    float decay_rate
);

//=============================================================================
// Entanglement Management
//=============================================================================

/**
 * @brief Increment entanglement count
 *
 * Called when this node becomes entangled with another.
 *
 * @param node Memory node
 * @return New entanglement count
 */
NIMCP_EXPORT uint32_t pr_memory_node_add_entanglement(pr_memory_node_t* node);

/**
 * @brief Decrement entanglement count
 *
 * Called when an entanglement is broken.
 *
 * @param node Memory node
 * @return New entanglement count
 */
NIMCP_EXPORT uint32_t pr_memory_node_remove_entanglement(pr_memory_node_t* node);

/**
 * @brief Get entanglement count
 *
 * @param node Memory node
 * @return Current entanglement count
 */
NIMCP_EXPORT uint32_t pr_memory_node_get_entanglement_count(
    const pr_memory_node_t* node
);

//=============================================================================
// Flags and Status
//=============================================================================

/**
 * @brief Set node flags
 *
 * @param node Memory node
 * @param flags Flags to set (OR'd)
 * @return Previous flags value
 */
NIMCP_EXPORT uint32_t pr_memory_node_set_flags(
    pr_memory_node_t* node,
    uint32_t flags
);

/**
 * @brief Clear node flags
 *
 * @param node Memory node
 * @param flags Flags to clear
 * @return Previous flags value
 */
NIMCP_EXPORT uint32_t pr_memory_node_clear_flags(
    pr_memory_node_t* node,
    uint32_t flags
);

/**
 * @brief Get current flags
 *
 * @param node Memory node
 * @return Current flags
 */
NIMCP_EXPORT uint32_t pr_memory_node_get_flags(const pr_memory_node_t* node);

/**
 * @brief Check if specific flag is set
 *
 * @param node Memory node
 * @param flag Flag to check
 * @return true if flag is set
 */
NIMCP_EXPORT bool pr_memory_node_has_flag(
    const pr_memory_node_t* node,
    pr_node_flags_t flag
);

/**
 * @brief Lock node for exclusive access
 *
 * @param node Memory node
 * @return PR_NODE_SUCCESS or PR_NODE_ERROR_LOCKED if already locked
 */
NIMCP_EXPORT pr_node_error_t pr_memory_node_lock(pr_memory_node_t* node);

/**
 * @brief Unlock node
 *
 * @param node Memory node
 * @return PR_NODE_SUCCESS or error code
 */
NIMCP_EXPORT pr_node_error_t pr_memory_node_unlock(pr_memory_node_t* node);

//=============================================================================
// Statistics and Information
//=============================================================================

/**
 * @brief Get node statistics
 *
 * @param node Memory node
 * @param stats Output statistics structure
 * @return PR_NODE_SUCCESS or error code
 */
NIMCP_EXPORT pr_node_error_t pr_memory_node_get_stats(
    const pr_memory_node_t* node,
    pr_node_stats_t* stats
);

/**
 * @brief Get node ID
 *
 * @param node Memory node
 * @return Node ID or PR_NODE_INVALID_ID if node is NULL
 */
NIMCP_EXPORT uint64_t pr_memory_node_get_id(const pr_memory_node_t* node);

/**
 * @brief Get age in milliseconds
 *
 * @param node Memory node
 * @param current_time_ms Current time (ms since epoch)
 * @return Age in milliseconds
 */
NIMCP_EXPORT uint64_t pr_memory_node_get_age_ms(
    const pr_memory_node_t* node,
    uint64_t current_time_ms
);

/**
 * @brief Get idle time in milliseconds
 *
 * @param node Memory node
 * @param current_time_ms Current time (ms since epoch)
 * @return Time since last access in milliseconds
 */
NIMCP_EXPORT uint64_t pr_memory_node_get_idle_ms(
    const pr_memory_node_t* node,
    uint64_t current_time_ms
);

//=============================================================================
// Serialization
//=============================================================================

/**
 * @brief Serialize node to buffer
 *
 * WHAT: Converts node to portable binary format
 * WHY:  Enable persistence and network transfer
 * HOW:  Header + metadata + data + checksum
 *
 * @param node Node to serialize
 * @param buffer Output buffer (NULL to query required size)
 * @param buffer_size Buffer size
 * @param written_size Output: bytes written (or required)
 * @return PR_NODE_SUCCESS or error code
 *
 * Format:
 * - Header (32 bytes): magic, version, sizes, checksum
 * - Signature (136 bytes)
 * - State (16 bytes)
 * - Metadata (40 bytes)
 * - Data (variable)
 */
NIMCP_EXPORT pr_node_error_t pr_memory_node_serialize(
    const pr_memory_node_t* node,
    void* buffer,
    size_t buffer_size,
    size_t* written_size
);

/**
 * @brief Deserialize node from buffer
 *
 * @param manager Node manager for ID assignment
 * @param buffer Input buffer with serialized data
 * @param buffer_size Buffer size
 * @param bytes_read Output: bytes consumed
 * @return Deserialized node or NULL on error
 */
NIMCP_EXPORT pr_memory_node_t* pr_memory_node_deserialize(
    pr_node_manager_t manager,
    const void* buffer,
    size_t buffer_size,
    size_t* bytes_read
);

/**
 * @brief Get serialization size for a node
 *
 * @param node Node to measure
 * @return Required buffer size for serialization
 */
NIMCP_EXPORT size_t pr_memory_node_serialization_size(
    const pr_memory_node_t* node
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get error string for node error code
 *
 * @param error Error code
 * @return Human-readable error string
 */
NIMCP_EXPORT const char* pr_node_error_string(pr_node_error_t error);

/**
 * @brief Get current time in milliseconds
 *
 * Utility function for timestamp generation.
 *
 * @return Milliseconds since epoch
 */
NIMCP_EXPORT uint64_t pr_node_current_time_ms(void);

/**
 * @brief Compute resonance score between two nodes
 *
 * Combines signature similarity with state distance.
 *
 * @param node1 First node
 * @param node2 Second node
 * @param signature_weight Weight for signature similarity [0, 1]
 * @param state_weight Weight for state similarity [0, 1]
 * @return Resonance score [0, 1] or -1 on error
 */
NIMCP_EXPORT float pr_memory_node_resonance(
    const pr_memory_node_t* node1,
    const pr_memory_node_t* node2,
    float signature_weight,
    float state_weight
);

//=============================================================================
// Inline Helper Functions
//=============================================================================

/**
 * @brief Create a default configuration for working memory
 *
 * @return Configuration for Z0 tier with high accessibility
 */
static inline pr_node_config_t pr_node_config_working_memory(void) {
    pr_node_config_t cfg = {
        .initial_tier = PR_MEMORY_TIER_Z0,
        .initial_strength = 1.0f,
        .emotional_valence = 0.0f,
        .salience = 0.7f,
        .accessibility = 1.0f,
        .compute_signature = true,
        .enable_cow = true
    };
    return cfg;
}

/**
 * @brief Create a configuration for emotional memory
 *
 * @param valence Emotional valence [-1, +1]
 * @return Configuration with emotional valence set
 */
static inline pr_node_config_t pr_node_config_emotional(float valence) {
    pr_node_config_t cfg = {
        .initial_tier = PR_MEMORY_TIER_Z1,
        .initial_strength = 0.8f,
        .emotional_valence = valence,
        .salience = 0.8f,
        .accessibility = 0.7f,
        .compute_signature = true,
        .enable_cow = true
    };
    return cfg;
}

/**
 * @brief Create a configuration for semantic memory (long-term)
 *
 * @return Configuration for Z2/Z3 tier semantic storage
 */
static inline pr_node_config_t pr_node_config_semantic(void) {
    pr_node_config_t cfg = {
        .initial_tier = PR_MEMORY_TIER_Z2,
        .initial_strength = 0.9f,
        .emotional_valence = 0.0f,
        .salience = 0.5f,
        .accessibility = 0.6f,
        .compute_signature = true,
        .enable_cow = true
    };
    return cfg;
}

#ifdef __cplusplus
}
#endif

#endif // NIMCP_PR_MEMORY_NODE_H
