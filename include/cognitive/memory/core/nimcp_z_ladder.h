//=============================================================================
// nimcp_z_ladder.h - Z-Ladder Memory Consolidation System
//=============================================================================
/**
 * @file nimcp_z_ladder.h
 * @brief Four-tier memory system with automatic promotion and decay
 *
 * WHAT: Z-Ladder implements a biologically-inspired memory consolidation system
 *       with four tiers (Z0-Z3) representing working, short-term, long-term,
 *       and permanent memory storage
 * WHY:  Human memory is not monolithic - it transitions through stages based
 *       on rehearsal, emotional salience, and time, enabling efficient storage
 *       and retrieval of information
 * HOW:  Manages memory nodes across tiers with automatic promotion/demotion,
 *       decay, eviction, and consolidation based on configurable thresholds
 *
 * ARCHITECTURE:
 *
 *   Z-Ladder Memory Tiers:
 *   +-----------------------------------------------------------------------+
 *   |                                                                        |
 *   |  Z0 (Working Memory)     Z1 (Short-Term)     Z2 (Long-Term)    Z3     |
 *   |  +------------------+    +------------------+  +---------------+ +---+ |
 *   |  | Capacity: 7+/-2  |    | Capacity: ~100   |  | Capacity: 10K | | @ | |
 *   |  | Decay: seconds   |    | Decay: hours     |  | Decay: days   | | Pg| |
 *   |  | CoW: Direct      |    | CoW: Direct      |  | CoW: Object   | +---+ |
 *   |  +--------+---------+    +--------+---------+  +-------+-------+   |   |
 *   |           |                       |                    |           |   |
 *   |           +------ PROMOTION ------+------ PROMOTION ---+--- PROM --+   |
 *   |           +------ DECAY/FORGET ---+------ DECAY -------+--- (rare)-+   |
 *   |           |                       |                    |               |
 *   |  +--------v---------+    +--------v---------+  +-------v-------+       |
 *   |  |  FORGOTTEN       |    |  FORGOTTEN       |  |  FORGOTTEN    |       |
 *   |  |  (freed)         |    |  (freed)         |  |  (archived?)  |       |
 *   |  +------------------+    +------------------+  +---------------+       |
 *   +-----------------------------------------------------------------------+
 *
 *   Promotion Criteria:
 *   +-----------------------------------------------------------------------+
 *   | Transition | Requirements                                             |
 *   |------------|----------------------------------------------------------|
 *   | Z0 -> Z1   | access_count > threshold AND salience (quat.y) > 0.3    |
 *   | Z1 -> Z2   | age > 1 hour AND consolidation (quat.w) > 0.5           |
 *   | Z2 -> Z3   | age > 1 day AND consolidation > 0.8 AND entangle > 5    |
 *   +-----------------------------------------------------------------------+
 *
 *   Decay Rates (per second, exponential):
 *   +-----------------------------------------------------------------------+
 *   | Tier | Rate     | Half-Life    | Biological Analog                    |
 *   |------|----------|--------------|--------------------------------------|
 *   | Z0   | 0.1      | ~10 seconds  | Prefrontal working memory buffer     |
 *   | Z1   | 0.0001   | ~2 hours     | Hippocampal short-term binding       |
 *   | Z2   | 0.00001  | ~19 hours    | Neocortical pattern consolidation    |
 *   | Z3   | 0        | Infinite     | Semantic/procedural permanence       |
 *   +-----------------------------------------------------------------------+
 *
 *   Consolidation Algorithm:
 *   +-----------------------------------------------------------------------+
 *   | Phase 1: Apply decay to all tiers                                     |
 *   | Phase 2: Process demotions (strength below threshold)                 |
 *   | Phase 3: Process promotions (eligible nodes)                          |
 *   | Phase 4: Evict if over capacity                                       |
 *   +-----------------------------------------------------------------------+
 *
 * PERFORMANCE:
 * - Insert: O(1) average, O(log n) if heap rebalance needed
 * - Find: O(1) via hash lookup
 * - Promote/Demote: O(log n) for heap operations
 * - Full consolidation: O(n) where n = total nodes
 * - Sleep consolidation: O(n log n) with sorting
 *
 * MEMORY:
 * - z_ladder_t: ~2KB base structure
 * - Per tier: capacity * 8 bytes (pointers) + heap overhead
 * - Total for default config: ~100KB
 *
 * THREAD SAFETY:
 * - All public functions are thread-safe via internal mutex
 * - Callbacks invoked with mutex held (avoid long operations)
 *
 * INTEGRATION:
 * - Depends on: nimcp_pr_memory_node.h for memory nodes
 * - Depends on: nimcp_entanglement.h for consolidation scoring
 * - Depends on: nimcp_quaternion.h for eligibility checking
 * - Optional: nimcp_theta_gamma.h for phase-gated consolidation
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#ifndef NIMCP_Z_LADDER_H
#define NIMCP_Z_LADDER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Dependencies
#include "cognitive/memory/core/nimcp_pr_memory_node.h"
#include "cognitive/memory/core/nimcp_quaternion.h"

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

/** Default capacity for Z0 (working memory) - Miller's magical number */
#define Z_LADDER_Z0_CAPACITY            9       // 7 +/- 2

/** Default capacity for Z1 (short-term memory) */
#define Z_LADDER_Z1_CAPACITY            100

/** Default capacity for Z2 (long-term memory) */
#define Z_LADDER_Z2_CAPACITY            10000

/** Default capacity for Z3 (permanent) - unlimited */
#define Z_LADDER_Z3_CAPACITY            0       // 0 = unlimited

/** Default decay rate for Z0 (10 second half-life) */
#define Z_LADDER_DECAY_Z0               0.07f   // ln(2)/10

/** Default decay rate for Z1 (2 hour half-life) */
#define Z_LADDER_DECAY_Z1               0.0001f // ln(2)/7200

/** Default decay rate for Z2 (19 hour half-life) */
#define Z_LADDER_DECAY_Z2               0.00001f // ln(2)/69120

/** Default decay rate for Z3 (no decay) */
#define Z_LADDER_DECAY_Z3               0.0f

/** Default promotion threshold for Z0->Z1 */
#define Z_LADDER_PROMOTE_Z0_STRENGTH    0.5f

/** Default promotion threshold for Z1->Z2 */
#define Z_LADDER_PROMOTE_Z1_STRENGTH    0.5f

/** Default promotion threshold for Z2->Z3 */
#define Z_LADDER_PROMOTE_Z2_STRENGTH    0.8f

/** Default demotion threshold */
#define Z_LADDER_DEMOTE_THRESHOLD       0.15f

/** Minimum age for Z0->Z1 promotion (ms) */
#define Z_LADDER_MIN_AGE_Z0_MS          5000    // 5 seconds

/** Minimum age for Z1->Z2 promotion (ms) */
#define Z_LADDER_MIN_AGE_Z1_MS          3600000 // 1 hour

/** Minimum age for Z2->Z3 promotion (ms) */
#define Z_LADDER_MIN_AGE_Z2_MS          86400000 // 1 day

/** Minimum access count for Z0->Z1 */
#define Z_LADDER_MIN_ACCESS_Z0          3

/** Minimum access count for Z1->Z2 */
#define Z_LADDER_MIN_ACCESS_Z1          5

/** Minimum entanglement for Z2->Z3 */
#define Z_LADDER_MIN_ENTANGLE_Z2        5

/** Minimum salience for Z0->Z1 */
#define Z_LADDER_MIN_SALIENCE_Z0        0.3f

/** Minimum consolidation for Z1->Z2 */
#define Z_LADDER_MIN_CONSOL_Z1          0.5f

/** Minimum consolidation for Z2->Z3 */
#define Z_LADDER_MIN_CONSOL_Z2          0.8f

/** Maximum consolidation events to track */
#define Z_LADDER_MAX_EVENTS             1000

/** Hash table load factor */
#define Z_LADDER_HASH_LOAD_FACTOR       0.75f

/** Initial hash table capacity */
#define Z_LADDER_HASH_INITIAL_CAP       256

/** Epsilon for floating point comparisons */
#define Z_LADDER_EPSILON                1e-6f

/** Number of tiers */
#define Z_LADDER_NUM_TIERS              4

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Eviction policy for tier capacity management
 */
typedef enum {
    Z_EVICT_WEAKEST = 0,        /**< Evict node with lowest strength */
    Z_EVICT_OLDEST,             /**< Evict node with earliest creation time */
    Z_EVICT_LRU,                /**< Evict least recently used node */
    Z_EVICT_LFU,                /**< Evict least frequently used node */
    Z_EVICT_COMBINED            /**< Combined scoring of all factors */
} z_eviction_policy_t;

/**
 * @brief Error codes for Z-Ladder operations
 */
typedef enum {
    Z_LADDER_SUCCESS = 0,               /**< Operation succeeded */
    Z_LADDER_ERROR_NULL_POINTER = -1,   /**< NULL pointer argument */
    Z_LADDER_ERROR_INVALID_TIER = -2,   /**< Invalid tier specified */
    Z_LADDER_ERROR_NOT_FOUND = -3,      /**< Node not found */
    Z_LADDER_ERROR_ALREADY_EXISTS = -4, /**< Node already in ladder */
    Z_LADDER_ERROR_CAPACITY = -5,       /**< Capacity exceeded */
    Z_LADDER_ERROR_NO_MEMORY = -6,      /**< Memory allocation failed */
    Z_LADDER_ERROR_LOCKED = -7,         /**< Node is locked */
    Z_LADDER_ERROR_ALREADY_TOP = -8,    /**< Already at Z3 */
    Z_LADDER_ERROR_ALREADY_BOTTOM = -9, /**< Already at Z0 */
    Z_LADDER_ERROR_INVALID_CONFIG = -10 /**< Invalid configuration */
} z_ladder_error_t;

/**
 * @brief Tier configuration
 *
 * Specifies parameters for each memory tier.
 */
typedef struct {
    size_t capacity;                    /**< Max items (0 = unlimited) */
    float decay_rate;                   /**< Decay per second */
    float promotion_threshold;          /**< Min strength for promotion */
    float demotion_threshold;           /**< Min strength to stay */
    uint64_t min_age_for_promotion_ms;  /**< Min time before promotion */
    uint32_t min_access_count;          /**< Min accesses for promotion */
    float min_entanglement;             /**< Min entanglement for promotion */
    float min_salience;                 /**< Min salience for promotion */
    float min_consolidation;            /**< Min consolidation for promotion */
    z_eviction_policy_t eviction_policy; /**< How to evict when full */
} z_tier_config_t;

/**
 * @brief Node location in Z-Ladder
 */
typedef struct {
    uint64_t node_id;                   /**< Node identifier */
    pr_memory_tier_t tier;              /**< Current tier */
    size_t tier_index;                  /**< Index within tier storage */
} z_node_location_t;

/**
 * @brief Consolidation event
 *
 * Records tier transitions for monitoring and analysis.
 */
typedef struct {
    uint64_t node_id;                   /**< Node identifier */
    pr_memory_tier_t from_tier;         /**< Source tier */
    pr_memory_tier_t to_tier;           /**< Destination tier */
    float strength_before;              /**< Strength before transition */
    float strength_after;               /**< Strength after transition */
    uint64_t timestamp_ms;              /**< Event timestamp */
    bool is_promotion;                  /**< True if promotion, false if demotion */
} z_consolidation_event_t;

/**
 * @brief Z-Ladder statistics
 */
typedef struct {
    // Per-tier counts
    size_t tier_counts[Z_LADDER_NUM_TIERS];     /**< Nodes per tier */
    size_t tier_capacities[Z_LADDER_NUM_TIERS]; /**< Capacity per tier */

    // Total counts
    size_t total_nodes;                 /**< Total nodes in ladder */

    // Transition counts
    uint64_t promotions[3];             /**< Z0->Z1, Z1->Z2, Z2->Z3 */
    uint64_t demotions[3];              /**< Z1->Z0, Z2->Z1, Z3->Z2 */
    uint64_t evictions[Z_LADDER_NUM_TIERS]; /**< Per-tier evictions */

    // Decay statistics
    float avg_strength[Z_LADDER_NUM_TIERS]; /**< Average strength per tier */
    float min_strength[Z_LADDER_NUM_TIERS]; /**< Minimum strength per tier */
    float max_strength[Z_LADDER_NUM_TIERS]; /**< Maximum strength per tier */

    // Time statistics
    uint64_t last_decay_time_ms;        /**< Last decay application time */
    uint64_t last_consolidation_time_ms; /**< Last consolidation time */
    uint64_t total_consolidations;       /**< Total consolidation cycles */

    // Memory usage
    size_t memory_bytes;                /**< Approximate memory usage */
} z_ladder_stats_t;

/**
 * @brief Z-Ladder configuration
 */
typedef struct {
    z_tier_config_t tier_configs[Z_LADDER_NUM_TIERS]; /**< Per-tier configs */
    size_t hash_initial_capacity;       /**< Initial hash table size */
    bool enable_callbacks;              /**< Enable promotion/eviction callbacks */
    bool enable_event_tracking;         /**< Track consolidation events */
    size_t max_events;                  /**< Max events to track */
    pr_node_manager_t node_manager;     /**< Node manager for unified memory */
} z_ladder_config_t;

/**
 * @brief Callback for promotion events
 *
 * @param node The promoted node
 * @param from_tier Source tier
 * @param to_tier Destination tier
 * @param user_data User-provided context
 */
typedef void (*z_promotion_callback_t)(
    pr_memory_node_t* node,
    pr_memory_tier_t from_tier,
    pr_memory_tier_t to_tier,
    void* user_data
);

/**
 * @brief Callback for eviction events
 *
 * @param node The evicted node (will be destroyed after callback)
 * @param tier The tier from which node was evicted
 * @param user_data User-provided context
 */
typedef void (*z_eviction_callback_t)(
    pr_memory_node_t* node,
    pr_memory_tier_t tier,
    void* user_data
);

/**
 * @brief Opaque Z-Ladder handle
 */
typedef struct z_ladder_struct* z_ladder_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default Z-Ladder configuration
 *
 * WHAT: Returns sensible default configuration for Z-Ladder
 * WHY:  Provides starting point with biologically-inspired parameters
 *
 * @return Default configuration
 *
 * Performance: ~10ns
 *
 * Example:
 *   z_ladder_config_t config = z_ladder_default_config();
 *   config.tier_configs[0].capacity = 5;  // Reduce working memory
 *   z_ladder_t ladder = z_ladder_create(&config);
 */
NIMCP_EXPORT z_ladder_config_t z_ladder_default_config(void);

/**
 * @brief Get default tier configuration for a specific tier
 *
 * @param tier The tier to get config for
 * @return Default configuration for the tier
 */
NIMCP_EXPORT z_tier_config_t z_ladder_default_tier_config(pr_memory_tier_t tier);

/**
 * @brief Validate Z-Ladder configuration
 *
 * @param config Configuration to validate
 * @return true if valid, false otherwise
 */
NIMCP_EXPORT bool z_ladder_config_validate(const z_ladder_config_t* config);

//=============================================================================
// Manager API
//=============================================================================

/**
 * @brief Create Z-Ladder memory manager
 *
 * WHAT: Creates a Z-Ladder with four memory tiers
 * WHY:  Central manager for memory consolidation
 * HOW:  Allocates tier storage, hash tables, and initializes parameters
 *
 * @param config Configuration (NULL for defaults)
 * @return Z-Ladder handle or NULL on failure
 *
 * Performance: O(capacity) for storage allocation
 * Memory: ~100KB for default configuration
 *
 * Example:
 *   z_ladder_t ladder = z_ladder_create(NULL);
 *   if (!ladder) {
 *       fprintf(stderr, "Failed to create Z-Ladder\n");
 *   }
 */
NIMCP_EXPORT z_ladder_t z_ladder_create(const z_ladder_config_t* config);

/**
 * @brief Destroy Z-Ladder and free all resources
 *
 * WHAT: Releases all memory and resources
 * WHY:  Clean shutdown
 * HOW:  Destroys all nodes, frees tier storage, releases mutex
 *
 * @param ladder Z-Ladder handle (NULL safe)
 *
 * Performance: O(n) where n = total nodes
 *
 * WARNING: All nodes are destroyed - retrieve important data first
 */
NIMCP_EXPORT void z_ladder_destroy(z_ladder_t ladder);

/**
 * @brief Clear all tiers (remove all nodes)
 *
 * WHAT: Removes all nodes from all tiers
 * WHY:  Reset to empty state without reallocating
 * HOW:  Destroys all nodes, clears storage arrays
 *
 * @param ladder Z-Ladder handle
 * @return Z_LADDER_SUCCESS or error code
 *
 * Performance: O(n)
 */
NIMCP_EXPORT z_ladder_error_t z_ladder_clear(z_ladder_t ladder);

//=============================================================================
// Node Management API
//=============================================================================

/**
 * @brief Insert node into Z-Ladder
 *
 * WHAT: Adds memory node to specified tier
 * WHY:  Entry point for new memories
 * HOW:  Validates tier, inserts into storage, updates hash table
 *
 * @param ladder Z-Ladder handle
 * @param node Memory node to insert (ownership transferred)
 * @param tier Initial tier (usually Z0)
 * @return Z_LADDER_SUCCESS or error code
 *
 * Performance: O(1) average, O(n) worst case
 * Thread safety: Thread-safe
 *
 * Notes:
 * - Node tier field is updated to match specified tier
 * - May trigger eviction if tier is at capacity
 * - Node ownership transfers to Z-Ladder
 *
 * Example:
 *   pr_memory_node_t* node = pr_memory_node_create(mgr, data, size, NULL);
 *   z_ladder_error_t err = z_ladder_insert(ladder, node, PR_MEMORY_TIER_Z0);
 */
NIMCP_EXPORT z_ladder_error_t z_ladder_insert(
    z_ladder_t ladder,
    pr_memory_node_t* node,
    pr_memory_tier_t tier
);

/**
 * @brief Remove node from Z-Ladder
 *
 * WHAT: Removes and destroys node
 * WHY:  Explicit forgetting or cleanup
 * HOW:  Locates node, removes from tier, destroys node
 *
 * @param ladder Z-Ladder handle
 * @param node_id Node identifier
 * @return Z_LADDER_SUCCESS or error code
 *
 * Performance: O(n) for tier search (or O(1) with hash)
 */
NIMCP_EXPORT z_ladder_error_t z_ladder_remove(z_ladder_t ladder, uint64_t node_id);

/**
 * @brief Find node by ID
 *
 * WHAT: Looks up node in Z-Ladder
 * WHY:  Node retrieval for access or modification
 * HOW:  Hash table lookup
 *
 * @param ladder Z-Ladder handle
 * @param node_id Node identifier
 * @return Node pointer or NULL if not found
 *
 * Performance: O(1) average
 * Thread safety: Returns pointer (caller must not modify without sync)
 *
 * Note: Updates access timestamp and count on the node
 */
NIMCP_EXPORT pr_memory_node_t* z_ladder_find(z_ladder_t ladder, uint64_t node_id);

/**
 * @brief Get the tier containing a node
 *
 * @param ladder Z-Ladder handle
 * @param node_id Node identifier
 * @param tier Output: tier containing the node
 * @return Z_LADDER_SUCCESS or Z_LADDER_ERROR_NOT_FOUND
 *
 * Performance: O(1) via hash
 */
NIMCP_EXPORT z_ladder_error_t z_ladder_get_tier(
    z_ladder_t ladder,
    uint64_t node_id,
    pr_memory_tier_t* tier
);

/**
 * @brief Force move node to specified tier
 *
 * WHAT: Moves node directly to a tier
 * WHY:  Manual tier assignment (e.g., important memory)
 * HOW:  Removes from current tier, inserts into new tier
 *
 * @param ladder Z-Ladder handle
 * @param node_id Node identifier
 * @param new_tier Destination tier
 * @return Z_LADDER_SUCCESS or error code
 *
 * Performance: O(n) for tier search + O(1) insert
 *
 * Notes:
 * - Bypasses normal promotion criteria
 * - May trigger eviction if new tier is full
 */
NIMCP_EXPORT z_ladder_error_t z_ladder_move(
    z_ladder_t ladder,
    uint64_t node_id,
    pr_memory_tier_t new_tier
);

//=============================================================================
// Promotion/Demotion API
//=============================================================================

/**
 * @brief Check if node is eligible for promotion
 *
 * WHAT: Evaluates promotion criteria for a node
 * WHY:  Determine if memory should be consolidated to higher tier
 * HOW:  Checks strength, age, access count, salience, consolidation
 *
 * @param ladder Z-Ladder handle
 * @param node Node to check
 * @return true if eligible for promotion
 *
 * Performance: O(1)
 *
 * Promotion criteria by tier:
 * - Z0->Z1: access_count > min AND salience > threshold
 * - Z1->Z2: age > 1hr AND consolidation > 0.5
 * - Z2->Z3: age > 1day AND consolidation > 0.8 AND entanglement > 5
 */
NIMCP_EXPORT bool z_ladder_check_promotion(
    z_ladder_t ladder,
    const pr_memory_node_t* node
);

/**
 * @brief Check if node should be demoted
 *
 * WHAT: Evaluates demotion criteria
 * WHY:  Determine if memory should decay to lower tier
 * HOW:  Checks strength against demotion threshold
 *
 * @param ladder Z-Ladder handle
 * @param node Node to check
 * @return true if should be demoted
 *
 * Performance: O(1)
 */
NIMCP_EXPORT bool z_ladder_check_demotion(
    z_ladder_t ladder,
    const pr_memory_node_t* node
);

/**
 * @brief Promote node one tier up
 *
 * WHAT: Moves node to next higher tier
 * WHY:  Memory consolidation
 * HOW:  Removes from current tier, inserts into next tier
 *
 * @param ladder Z-Ladder handle
 * @param node_id Node to promote
 * @return Z_LADDER_SUCCESS, Z_LADDER_ERROR_ALREADY_TOP, or other error
 *
 * Performance: O(n) for removal + O(1) for insert
 *
 * Notes:
 * - Updates node's tier field
 * - Updates node's decay rate
 * - Invokes promotion callback if set
 * - May trigger eviction if destination tier is full
 */
NIMCP_EXPORT z_ladder_error_t z_ladder_promote(z_ladder_t ladder, uint64_t node_id);

/**
 * @brief Demote node one tier down
 *
 * WHAT: Moves node to next lower tier
 * WHY:  Memory decay/forgetting
 * HOW:  Removes from current tier, inserts into lower tier (or evicts from Z0)
 *
 * @param ladder Z-Ladder handle
 * @param node_id Node to demote
 * @return Z_LADDER_SUCCESS, Z_LADDER_ERROR_ALREADY_BOTTOM, or other error
 *
 * Performance: O(n) for removal + O(1) for insert
 *
 * Notes:
 * - If already at Z0, node is evicted (destroyed)
 * - Updates node's tier field and decay rate
 */
NIMCP_EXPORT z_ladder_error_t z_ladder_demote(z_ladder_t ladder, uint64_t node_id);

/**
 * @brief Process all eligible promotions
 *
 * WHAT: Batch promotes all nodes meeting criteria
 * WHY:  Efficient bulk consolidation
 * HOW:  Iterates all tiers (Z0-Z2), promotes eligible nodes
 *
 * @param ladder Z-Ladder handle
 * @return Number of nodes promoted
 *
 * Performance: O(n) where n = total nodes
 */
NIMCP_EXPORT size_t z_ladder_process_promotions(z_ladder_t ladder);

/**
 * @brief Process all necessary demotions
 *
 * WHAT: Batch demotes all nodes below threshold
 * WHY:  Efficient bulk forgetting
 * HOW:  Iterates all tiers (Z1-Z3), demotes weak nodes
 *
 * @param ladder Z-Ladder handle
 * @return Number of nodes demoted (or evicted from Z0)
 *
 * Performance: O(n)
 */
NIMCP_EXPORT size_t z_ladder_process_demotions(z_ladder_t ladder);

//=============================================================================
// Decay API
//=============================================================================

/**
 * @brief Apply decay to all nodes
 *
 * WHAT: Reduces strength of all nodes based on time and tier decay rates
 * WHY:  Model biological memory decay
 * HOW:  Exponential decay: strength *= exp(-decay_rate * dt)
 *
 * @param ladder Z-Ladder handle
 * @param dt_seconds Time since last decay application
 * @return Z_LADDER_SUCCESS or error code
 *
 * Performance: O(n)
 *
 * Notes:
 * - Z3 nodes never decay (decay_rate = 0)
 * - Updates last_decay_time in stats
 */
NIMCP_EXPORT z_ladder_error_t z_ladder_apply_decay(
    z_ladder_t ladder,
    float dt_seconds
);

/**
 * @brief Apply decay to single tier
 *
 * @param ladder Z-Ladder handle
 * @param tier Tier to decay
 * @param dt_seconds Time since last decay
 * @return Z_LADDER_SUCCESS or error code
 *
 * Performance: O(m) where m = nodes in tier
 */
NIMCP_EXPORT z_ladder_error_t z_ladder_decay_tier(
    z_ladder_t ladder,
    pr_memory_tier_t tier,
    float dt_seconds
);

/**
 * @brief Get decay rate for a tier
 *
 * @param ladder Z-Ladder handle
 * @param tier Tier to query
 * @return Decay rate (per second)
 */
NIMCP_EXPORT float z_ladder_get_decay_rate(z_ladder_t ladder, pr_memory_tier_t tier);

/**
 * @brief Set decay rate for a tier
 *
 * @param ladder Z-Ladder handle
 * @param tier Tier to modify
 * @param rate New decay rate
 * @return Z_LADDER_SUCCESS or error code
 */
NIMCP_EXPORT z_ladder_error_t z_ladder_set_decay_rate(
    z_ladder_t ladder,
    pr_memory_tier_t tier,
    float rate
);

//=============================================================================
// Eviction API
//=============================================================================

/**
 * @brief Evict weakest node from tier
 *
 * WHAT: Removes and destroys node with lowest strength
 * WHY:  Make room for new memories
 * HOW:  Finds minimum strength node, invokes callback, destroys
 *
 * @param ladder Z-Ladder handle
 * @param tier Tier to evict from
 * @return Z_LADDER_SUCCESS, Z_LADDER_ERROR_NOT_FOUND (empty tier), or error
 *
 * Performance: O(m) for linear scan, O(log m) with heap
 */
NIMCP_EXPORT z_ladder_error_t z_ladder_evict_weakest(
    z_ladder_t ladder,
    pr_memory_tier_t tier
);

/**
 * @brief Evict only if tier exceeds capacity
 *
 * WHAT: Conditional eviction
 * WHY:  Maintain tier capacity constraints
 * HOW:  Checks count vs capacity, evicts if needed
 *
 * @param ladder Z-Ladder handle
 * @param tier Tier to check
 * @return Number of nodes evicted
 *
 * Performance: O(m) per eviction
 */
NIMCP_EXPORT size_t z_ladder_evict_if_full(z_ladder_t ladder, pr_memory_tier_t tier);

/**
 * @brief Set eviction policy for a tier
 *
 * @param ladder Z-Ladder handle
 * @param tier Tier to configure
 * @param policy Eviction policy
 * @return Z_LADDER_SUCCESS or error code
 */
NIMCP_EXPORT z_ladder_error_t z_ladder_set_eviction_policy(
    z_ladder_t ladder,
    pr_memory_tier_t tier,
    z_eviction_policy_t policy
);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get all nodes in a tier
 *
 * WHAT: Returns array of node pointers for a tier
 * WHY:  Iteration over tier contents
 * HOW:  Copies pointers to caller-provided array
 *
 * @param ladder Z-Ladder handle
 * @param tier Tier to query
 * @param nodes Output array (caller-allocated)
 * @param max_nodes Maximum nodes to return
 * @param count Output: actual count returned
 * @return Z_LADDER_SUCCESS or error code
 *
 * Performance: O(min(m, max_nodes))
 *
 * Example:
 *   pr_memory_node_t* nodes[100];
 *   size_t count;
 *   z_ladder_get_nodes(ladder, PR_MEMORY_TIER_Z0, nodes, 100, &count);
 */
NIMCP_EXPORT z_ladder_error_t z_ladder_get_nodes(
    z_ladder_t ladder,
    pr_memory_tier_t tier,
    pr_memory_node_t** nodes,
    size_t max_nodes,
    size_t* count
);

/**
 * @brief Get count of nodes in a tier
 *
 * @param ladder Z-Ladder handle
 * @param tier Tier to query
 * @return Node count (0 if invalid tier)
 *
 * Performance: O(1)
 */
NIMCP_EXPORT size_t z_ladder_get_count(z_ladder_t ladder, pr_memory_tier_t tier);

/**
 * @brief Get total count across all tiers
 *
 * @param ladder Z-Ladder handle
 * @return Total node count
 *
 * Performance: O(1)
 */
NIMCP_EXPORT size_t z_ladder_get_total_count(z_ladder_t ladder);

/**
 * @brief Get top-K strongest nodes in a tier
 *
 * WHAT: Returns K nodes with highest strength
 * WHY:  Retrieve most consolidated memories
 * HOW:  Partial sort or heap extraction
 *
 * @param ladder Z-Ladder handle
 * @param tier Tier to query
 * @param k Number of nodes to return
 * @param nodes Output array (caller-allocated, size >= k)
 * @param count Output: actual count returned (<= k)
 * @return Z_LADDER_SUCCESS or error code
 *
 * Performance: O(m + k log k)
 */
NIMCP_EXPORT z_ladder_error_t z_ladder_get_strongest(
    z_ladder_t ladder,
    pr_memory_tier_t tier,
    size_t k,
    pr_memory_node_t** nodes,
    size_t* count
);

/**
 * @brief Get bottom-K weakest nodes in a tier
 *
 * @param ladder Z-Ladder handle
 * @param tier Tier to query
 * @param k Number of nodes to return
 * @param nodes Output array
 * @param count Output: actual count
 * @return Z_LADDER_SUCCESS or error code
 *
 * Performance: O(m + k log k)
 */
NIMCP_EXPORT z_ladder_error_t z_ladder_get_weakest(
    z_ladder_t ladder,
    pr_memory_tier_t tier,
    size_t k,
    pr_memory_node_t** nodes,
    size_t* count
);

//=============================================================================
// Consolidation API
//=============================================================================

/**
 * @brief Run full consolidation cycle
 *
 * WHAT: Complete consolidation pass
 * WHY:  Periodic maintenance of memory system
 * HOW:  Decay -> Demotions -> Promotions -> Evictions
 *
 * @param ladder Z-Ladder handle
 * @return Z_LADDER_SUCCESS or error code
 *
 * Performance: O(n)
 *
 * Algorithm:
 * 1. Apply decay to all tiers
 * 2. Process demotions (Z3->Z0, strength below threshold)
 * 3. Process promotions (Z0->Z3, eligible nodes)
 * 4. Evict over-capacity tiers
 *
 * Example:
 *   // Call periodically (e.g., every second)
 *   z_ladder_consolidate(ladder);
 */
NIMCP_EXPORT z_ladder_error_t z_ladder_consolidate(z_ladder_t ladder);

/**
 * @brief Consolidate single tier
 *
 * @param ladder Z-Ladder handle
 * @param tier Tier to consolidate
 * @return Z_LADDER_SUCCESS or error code
 *
 * Performance: O(m)
 */
NIMCP_EXPORT z_ladder_error_t z_ladder_consolidate_tier(
    z_ladder_t ladder,
    pr_memory_tier_t tier
);

/**
 * @brief Sleep-like deep consolidation
 *
 * WHAT: Enhanced consolidation mimicking sleep consolidation
 * WHY:  Research shows sleep is critical for memory consolidation
 * HOW:  Multiple passes with enhanced promotion criteria
 *
 * @param ladder Z-Ladder handle
 * @return Z_LADDER_SUCCESS or error code
 *
 * Performance: O(n log n) with sorting
 *
 * Sleep consolidation features:
 * - Reduced promotion thresholds (memories consolidate easier)
 * - Enhanced emotional memory promotion
 * - Z1->Z2 consolidation prioritized
 * - Multiple passes for thorough processing
 */
NIMCP_EXPORT z_ladder_error_t z_ladder_sleep_consolidate(z_ladder_t ladder);

/**
 * @brief Get recent consolidation events
 *
 * @param ladder Z-Ladder handle
 * @param events Output array (caller-allocated)
 * @param max_events Maximum events to return
 * @param count Output: actual count returned
 * @return Z_LADDER_SUCCESS or error code
 *
 * Performance: O(min(tracked, max_events))
 */
NIMCP_EXPORT z_ladder_error_t z_ladder_get_consolidation_events(
    z_ladder_t ladder,
    z_consolidation_event_t* events,
    size_t max_events,
    size_t* count
);

/**
 * @brief Clear consolidation event history
 *
 * @param ladder Z-Ladder handle
 * @return Z_LADDER_SUCCESS or error code
 */
NIMCP_EXPORT z_ladder_error_t z_ladder_clear_events(z_ladder_t ladder);

//=============================================================================
// Reinforcement API
//=============================================================================

/**
 * @brief Reinforce a node's strength
 *
 * WHAT: Increases node strength (counters decay)
 * WHY:  Rehearsal strengthens memory
 * HOW:  Adds to strength, clamps to [0, 1]
 *
 * @param ladder Z-Ladder handle
 * @param node_id Node to reinforce
 * @param amount Reinforcement amount (typically 0.1-0.3)
 * @return New strength value, or -1.0f on error
 *
 * Performance: O(1) via hash
 */
NIMCP_EXPORT float z_ladder_reinforce(
    z_ladder_t ladder,
    uint64_t node_id,
    float amount
);

/**
 * @brief Record access to a node
 *
 * WHAT: Updates access timestamp and count
 * WHY:  Track usage patterns for promotion/eviction
 * HOW:  Increments access count, updates last_accessed_ms
 *
 * @param ladder Z-Ladder handle
 * @param node_id Node that was accessed
 * @return Z_LADDER_SUCCESS or error code
 *
 * Performance: O(1)
 *
 * Note: Also provides small strength boost
 */
NIMCP_EXPORT z_ladder_error_t z_ladder_access(z_ladder_t ladder, uint64_t node_id);

/**
 * @brief Apply emotional boost to node
 *
 * WHAT: Emotional enhancement of memory strength
 * WHY:  Emotional memories are better remembered
 * HOW:  Boosts strength based on emotional valence magnitude
 *
 * @param ladder Z-Ladder handle
 * @param node_id Node to boost
 * @param valence Emotional valence [-1, +1]
 * @return New strength value, or -1.0f on error
 *
 * Performance: O(1)
 *
 * Notes:
 * - Both positive and negative emotions boost memory
 * - Boost proportional to |valence|
 * - Updates node's emotional valence (quaternion x component)
 */
NIMCP_EXPORT float z_ladder_emotional_boost(
    z_ladder_t ladder,
    uint64_t node_id,
    float valence
);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get Z-Ladder statistics
 *
 * @param ladder Z-Ladder handle
 * @param stats Output statistics (caller-allocated)
 * @return Z_LADDER_SUCCESS or error code
 *
 * Performance: O(n) for strength calculations
 */
NIMCP_EXPORT z_ladder_error_t z_ladder_get_stats(
    z_ladder_t ladder,
    z_ladder_stats_t* stats
);

/**
 * @brief Print debug summary to stdout
 *
 * @param ladder Z-Ladder handle
 */
NIMCP_EXPORT void z_ladder_print_summary(z_ladder_t ladder);

/**
 * @brief Reset statistics counters
 *
 * @param ladder Z-Ladder handle
 */
NIMCP_EXPORT void z_ladder_reset_stats(z_ladder_t ladder);

//=============================================================================
// Callback API
//=============================================================================

/**
 * @brief Set promotion callback
 *
 * @param ladder Z-Ladder handle
 * @param callback Function to call on promotion
 * @param user_data Context to pass to callback
 * @return Z_LADDER_SUCCESS or error code
 */
NIMCP_EXPORT z_ladder_error_t z_ladder_set_promotion_callback(
    z_ladder_t ladder,
    z_promotion_callback_t callback,
    void* user_data
);

/**
 * @brief Set eviction callback
 *
 * @param ladder Z-Ladder handle
 * @param callback Function to call on eviction
 * @param user_data Context to pass to callback
 * @return Z_LADDER_SUCCESS or error code
 */
NIMCP_EXPORT z_ladder_error_t z_ladder_set_eviction_callback(
    z_ladder_t ladder,
    z_eviction_callback_t callback,
    void* user_data
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get error string for error code
 *
 * @param error Error code
 * @return Human-readable error string
 */
NIMCP_EXPORT const char* z_ladder_error_string(z_ladder_error_t error);

/**
 * @brief Get tier name as string
 *
 * @param tier Memory tier
 * @return Human-readable tier name
 */
NIMCP_EXPORT const char* z_ladder_tier_name(pr_memory_tier_t tier);

/**
 * @brief Get eviction policy name as string
 *
 * @param policy Eviction policy
 * @return Human-readable policy name
 */
NIMCP_EXPORT const char* z_ladder_eviction_policy_name(z_eviction_policy_t policy);

/**
 * @brief Get current time in milliseconds
 *
 * Utility function for timestamp generation.
 *
 * @return Milliseconds since epoch
 */
NIMCP_EXPORT uint64_t z_ladder_current_time_ms(void);

/**
 * @brief Validate ladder internal consistency
 *
 * WHAT: Checks internal data structure consistency
 * WHY:  Debug/test tool for corruption detection
 *
 * @param ladder Z-Ladder handle
 * @return true if consistent, false if corruption detected
 *
 * Performance: O(n)
 */
NIMCP_EXPORT bool z_ladder_validate(z_ladder_t ladder);

//=============================================================================
// Long-Term Memory / Landmark API (Elephant-inspired enhancements)
//=============================================================================
//
// The four-tier Z-Ladder covers automatic memory dynamics well: nodes get
// promoted by use, demoted by neglect, decay by time, evict by capacity.
// But some memories should behave differently — a matriarch elephant
// remembers the water hole decades after the last drought, regardless of
// rehearsal cadence. That's the landmark role: explicit user designation
// that this memory gets elevated to Z3 permanent and is protected from
// demotion indefinitely.
//
// Three functions cover the role:
//   z_ladder_mark_landmark  — promote to Z3 + protect from demotion
//   z_ladder_audit_landmarks — periodic integrity check of landmark set
//   z_ladder_landmark_query  — prime-signature-keyed retrieval (oracle)

/** Maximum number of landmarks a ladder can hold. */
#define Z_LADDER_MAX_LANDMARKS 256u

/** Summary returned by z_ladder_audit_landmarks. */
typedef struct {
    size_t   total_landmarks;          /**< Landmarks currently tracked */
    size_t   present_in_ladder;        /**< Landmarks whose node is still in the ladder */
    size_t   missing_from_ladder;      /**< Landmarks referenced by ID but node gone */
    size_t   at_z3;                    /**< Landmarks currently in Z3 tier */
    size_t   drifted_below_z3;         /**< Landmarks that somehow fell below Z3 */
    float    min_consolidation;        /**< Lowest quat.w across landmarks */
    float    avg_consolidation;        /**< Mean quat.w across landmarks */
    uint64_t oldest_last_access_ms;    /**< Staleness signal (largest last-access age) */
} z_ladder_landmark_audit_t;

/** Result entry from z_ladder_landmark_query. */
typedef struct {
    uint64_t node_id;                  /**< Matched landmark node */
    float    similarity;               /**< Prime-signature similarity [0, 1] */
    pr_memory_tier_t tier;             /**< Current tier (should be Z3 for healthy landmarks) */
} z_ladder_landmark_hit_t;

/**
 * @brief Mark a node as a landmark — elevate to Z3 and protect from demotion.
 *
 * Promotes the node to Z3 regardless of automatic promotion criteria.
 * Subsequent demotion checks return false for this node, so natural
 * decay cannot pull it back to Z2. The reason string is stored for
 * introspection (e.g., "first_reward", "identity_event").
 *
 * No-op if node is already a landmark. Returns Z_LADDER_ERROR_NOT_FOUND
 * if the node isn't in the ladder; Z_LADDER_ERROR_CAPACITY if the
 * landmark set is full (Z_LADDER_MAX_LANDMARKS).
 *
 * @param ladder  Z-Ladder handle.
 * @param node_id Node to mark.
 * @param reason  Nul-terminated label (copied internally, up to 63 chars).
 * @return Z_LADDER_SUCCESS or error code.
 */
NIMCP_EXPORT z_ladder_error_t z_ladder_mark_landmark(z_ladder_t ladder,
                                                     uint64_t node_id,
                                                     const char* reason);

/**
 * @brief Remove landmark designation (node becomes subject to normal decay).
 *
 * @return Z_LADDER_SUCCESS, Z_LADDER_ERROR_NOT_FOUND if not a landmark.
 */
NIMCP_EXPORT z_ladder_error_t z_ladder_unmark_landmark(z_ladder_t ladder,
                                                       uint64_t node_id);

/** Test whether a node is currently flagged as a landmark. */
NIMCP_EXPORT bool z_ladder_is_landmark(z_ladder_t ladder, uint64_t node_id);

/**
 * @brief Walk the landmark set and compute an integrity summary.
 *
 * Fills out->* with current counts: total landmarks, how many still
 * resolve to real nodes in the ladder, how many are in Z3 vs have
 * drifted, and aggregate quaternion consolidation. Use to detect
 * memory system health problems (landmarks leaking out of Z3, node
 * IDs referring to removed nodes, etc.).
 *
 * @return Z_LADDER_SUCCESS, or Z_LADDER_ERROR_NULL_POINTER.
 */
NIMCP_EXPORT z_ladder_error_t z_ladder_audit_landmarks(
    z_ladder_t ladder,
    z_ladder_landmark_audit_t* out);

/**
 * @brief Query landmarks by prime-signature similarity — oracle retrieval.
 *
 * Walks the landmark set, computes prime-signature similarity between
 * each landmark's node and the query signature, sorts descending,
 * returns top-max_results entries in results[].
 *
 * This is the elephant-matriarch role: when the brain asks "what do I
 * know long-term about X", this returns high-confidence landmark hits
 * first, before degraded general-store retrieval is consulted.
 *
 * @param ladder      Z-Ladder handle.
 * @param query       Prime signature to match against (must not be NULL).
 * @param results     Output array (caller-allocated).
 * @param max_results Capacity of results[].
 * @param out_count   Receives number of matches written.
 * @return Z_LADDER_SUCCESS or error code.
 */
NIMCP_EXPORT z_ladder_error_t z_ladder_landmark_query(
    z_ladder_t ladder,
    const prime_signature_t* query,
    z_ladder_landmark_hit_t* results,
    size_t max_results,
    size_t* out_count);

/** Number of currently-tracked landmarks. */
NIMCP_EXPORT size_t z_ladder_landmark_count(z_ladder_t ladder);

/**
 * @brief Phase E4: prune landmark slots whose referenced node no longer
 *        exists in the ladder (evicted / removed by other paths). Walk
 *        the landmark array, mark any stale slot as free, decrement
 *        n_landmarks accordingly.
 *
 * Safe to call periodically (e.g. after consolidation) to reclaim ghost
 * slots before `mark_landmark` has to evict live entries.
 *
 * @return number of slots reclaimed.
 */
NIMCP_EXPORT size_t z_ladder_landmark_prune_stale(z_ladder_t ladder);

/**
 * @brief Phase E4: iterate live landmark entries (id + reason).
 *
 * Fills `out_ids` with live landmark node IDs and `out_reasons` with
 * their reason strings (each nul-terminated, up to 64 chars). Either
 * output array may be NULL if the caller only wants the count.
 *
 * @param ladder       Z-Ladder handle.
 * @param out_ids      Output array of node_id (may be NULL).
 * @param out_reasons  Output array of char[64] (may be NULL).
 * @param max          Capacity of each output array.
 * @return Number of landmarks written (≤ max, ≤ count).
 */
NIMCP_EXPORT size_t z_ladder_landmark_list(z_ladder_t ladder,
                                           uint64_t* out_ids,
                                           char (*out_reasons)[64],
                                           size_t max);

/**
 * @brief Phase E4 #5: prime-signature similarity search across ALL tiers.
 *
 * Like `z_ladder_landmark_query`, but walks every node in Z0..Z3, not
 * just the landmark set. Useful when the caller wants general oracle
 * retrieval including non-landmark memories.
 *
 * @param ladder      Z-Ladder handle.
 * @param query       Prime signature to match against (must not be NULL).
 * @param results     Output array (caller-allocated).
 * @param max_results Capacity of results[].
 * @param out_count   Receives number of matches written.
 * @return Z_LADDER_SUCCESS or error code.
 */
NIMCP_EXPORT z_ladder_error_t z_ladder_query_all_tiers(
    z_ladder_t ladder,
    const prime_signature_t* query,
    z_ladder_landmark_hit_t* results,
    size_t max_results,
    size_t* out_count);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_Z_LADDER_H
