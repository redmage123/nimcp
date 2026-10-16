/**
 * @file nimcp_collective_workspace.h
 * @brief Collective Workspace for distributed attention across drone swarms
 *
 * WHAT: Distributed extension of Global Workspace Theory for multi-agent systems
 * WHY:  Enable emergent collective cognition and coordinated attention in swarms
 * HOW:  CRDT-based workspace items with vector clocks, salience-based merging,
 *       and broadcast policies for swarm-wide information sharing
 *
 * DESIGN RATIONALE:
 * Individual drones have local global workspaces (conscious access within one agent).
 * The Collective Workspace extends this to swarm-level consciousness - information
 * in the collective workspace is "collectively conscious" (known to all drones).
 * This enables:
 * - Distributed attention (swarm focuses on highest-salience items)
 * - Emergent goals (collective priorities emerge from individual saliences)
 * - Shared situational awareness (threats, opportunities perceived by any drone)
 * - Coordinated action (all drones aware of critical information)
 *
 * DESIGN PATTERNS:
 * - CRDT (Conflict-Free Replicated Data Type): Eventually consistent merging
 * - Observer: Subscribers notified of collective workspace changes
 * - Strategy: Pluggable merge and coherence strategies
 * - Singleton per swarm: One collective workspace shared by all drones
 *
 * BIOLOGICAL INSPIRATION:
 * Based on collective behavior in social insects (ants, bees) and bird flocks:
 *
 * 1. STIGMERGY (Environment-mediated coordination)
 *    - Ants: Pheromone trails encode collective knowledge
 *    - Bees: Waggle dance broadcasts important locations
 *    - Workspace items = digital pheromones/dances
 *
 * 2. COLLECTIVE DECISION-MAKING
 *    - Honeybee swarms: Scouts compete, best site wins
 *    - Workspace: Items compete via salience
 *
 * 3. QUORUM SENSING
 *    - Bacteria: Decisions when threshold density reached
 *    - Workspace: Coherence threshold for collective action
 *
 * 4. DISTRIBUTED SENSING
 *    - Bird flocks: Predator detection propagates
 *    - Workspace: High-salience items broadcast to swarm
 *
 * SWARM DYNAMICS:
 * - Each drone maintains local copy of collective workspace
 * - High-salience items broadcast to neighbors
 * - Items merge using CRDT rules (vector clocks + LWW)
 * - Coherence measures swarm alignment
 * - Pruning removes low-salience items (limited capacity)
 *
 * EXAMPLE:
 * @code
 *   // Initialize collective workspace on each drone
 *   collective_workspace_config_t config = {
 *       .local_drone_id = 3,
 *       .swarm_size = 16,
 *       .broadcast_threshold = 0.7,  // Only share high-salience items
 *       .coherence_window_ms = 5000  // 5s coherence window
 *   };
 *   collective_workspace_t* cw = collective_workspace_create(&config);
 *
 *   // Drone 3 detects a threat
 *   collective_workspace_item_t threat = {
 *       .item_id = (3 << 16) | 42,  // Drone 3, local ID 42
 *       .salience = 0.95,             // Very high priority
 *       .type = WORKSPACE_ITEM_THREAT,
 *       .source_drone = 3,
 *       .timestamp_ms = get_time_ms()
 *   };
 *   memcpy(threat.content, threat_features, 16 * sizeof(float));
 *   collective_workspace_add_item(cw, &threat);
 *
 *   // Should this broadcast to swarm?
 *   if (collective_workspace_should_broadcast(cw, &threat)) {
 *       // Serialize and send to neighbors
 *       collective_workspace_item_t* items;
 *       uint32_t count;
 *       collective_workspace_get_broadcast_items(cw, &items, &count);
 *       swarm_send_to_neighbors(items, count);
 *   }
 *
 *   // Drone 7 receives item from Drone 3
 *   collective_workspace_item_t received_threat;
 *   // ... deserialize from network ...
 *   collective_workspace_merge_item(cw, &received_threat);
 *
 *   // Get top items for collective attention
 *   collective_workspace_item_t top_items[5];
 *   uint32_t top_count;
 *   collective_workspace_get_top_items(cw, top_items, 5, &top_count);
 *
 *   // Check collective coherence (are we aligned?)
 *   float coherence = collective_workspace_get_coherence(cw);
 *   if (coherence > 0.8) {
 *       printf("Swarm is highly aligned on threat response\n");
 *   }
 *
 *   collective_workspace_destroy(cw);
 * @endcode
 *
 * PHASE: Advanced Swarm Intelligence
 * DEPENDENCIES: Global Workspace, Bio-Async, Security
 * INTEGRATION: Works with local global_workspace for hierarchical attention
 *
 * @author NIMCP Swarm Intelligence Team
 * @date 2025-12-08
 * @version 1.0.0
 */

#ifndef NIMCP_COLLECTIVE_WORKSPACE_H
#define NIMCP_COLLECTIVE_WORKSPACE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Configuration Constants
//=============================================================================

/**
 * @brief Maximum workspace items in collective workspace
 *
 * WHAT: Limited capacity for attention (top-K most salient)
 * WHY:  Prevent unbounded memory, focus on critical information
 * VALUE: 32 items (inspired by working memory capacity × swarm factor)
 * BIOLOGICAL: Like attention bottleneck, but for collective
 */
#define COLLECTIVE_WORKSPACE_MAX_ITEMS 32

/**
 * @brief Maximum swarm size (vector clock limit)
 *
 * WHAT: Maximum number of drones in swarm
 * WHY:  Vector clock dimensionality constraint
 * VALUE: 16 drones (reasonable swarm size, manageable vector clocks)
 */
#define COLLECTIVE_WORKSPACE_MAX_SWARM_SIZE 16

/**
 * @brief Content vector dimensionality
 *
 * WHAT: Feature representation size for workspace items
 * WHY:  Matches typical NIMCP feature vectors
 * VALUE: 16 floats (compact representation, fast transmission)
 */
#define COLLECTIVE_WORKSPACE_CONTENT_DIM 16

/**
 * @brief Default broadcast threshold
 *
 * WHAT: Minimum salience for broadcasting to swarm
 * WHY:  Reduce network traffic, only share important items
 * VALUE: 0.6 (60% salience) - balances sharing vs bandwidth
 */
#define COLLECTIVE_WORKSPACE_DEFAULT_BROADCAST_THRESHOLD 0.6f

/**
 * @brief Default coherence window in milliseconds
 *
 * WHAT: Time window for computing collective coherence
 * WHY:  Recent items matter more for current alignment
 * VALUE: 5000ms (5 seconds) - captures recent swarm state
 */
#define COLLECTIVE_WORKSPACE_DEFAULT_COHERENCE_WINDOW_MS 5000

/**
 * @brief Pruning interval in milliseconds
 *
 * WHAT: How often to remove low-salience items
 * WHY:  Prevent workspace bloat, maintain focus
 * VALUE: 1000ms (1 second) - regular cleanup
 */
#define COLLECTIVE_WORKSPACE_PRUNING_INTERVAL_MS 1000

/**
 * @brief Item time-to-live in milliseconds
 *
 * WHAT: Maximum age before item is automatically removed
 * WHY:  Stale information decays over time
 * VALUE: 30000ms (30 seconds) - balances persistence vs freshness
 */
#define COLLECTIVE_WORKSPACE_ITEM_TTL_MS 30000

/**
 * @brief Minimum salience threshold for keeping items
 *
 * WHAT: Below this, items are pruned regardless of age
 * WHY:  Don't waste space on low-priority items
 * VALUE: 0.1 (10%) - very low threshold (keep most things)
 */
#define COLLECTIVE_WORKSPACE_MIN_SALIENCE 0.1f

//=============================================================================
// Core Types
//=============================================================================

/**
 * @brief Workspace item types
 *
 * WHAT: Semantic categories for workspace items
 * WHY:  Different types of information need different handling
 * HOW:  Tagged union approach (type + content)
 */
typedef enum {
    WORKSPACE_ITEM_NONE = 0,        /**< Invalid/uninitialized */
    WORKSPACE_ITEM_PERCEPTION,      /**< Sensory observation */
    WORKSPACE_ITEM_GOAL,            /**< Collective objective */
    WORKSPACE_ITEM_MEMORY,          /**< Recalled information */
    WORKSPACE_ITEM_THREAT,          /**< Danger detection */
    WORKSPACE_ITEM_OPPORTUNITY,     /**< Resource/target found */
    WORKSPACE_ITEM_STATE,           /**< Swarm state update */
    WORKSPACE_ITEM_COMMAND,         /**< Coordination directive */
    WORKSPACE_ITEM_QUERY,           /**< Information request */
    WORKSPACE_ITEM_PREDICTION,      /**< Future state prediction */
    WORKSPACE_ITEM_META,            /**< Meta-cognitive information */
    WORKSPACE_ITEM_CUSTOM = 100     /**< User-defined types start here */
} workspace_item_type_t;

/**
 * @brief Collective workspace item
 *
 * WHAT: One piece of information in collective workspace
 * WHY:  CRDT-compatible structure with causality tracking
 * HOW:  Vector clock + LWW + salience-based conflict resolution
 *
 * CRDT PROPERTIES:
 * - Commutative: merge(A,B) = merge(B,A)
 * - Associative: merge(merge(A,B),C) = merge(A,merge(B,C))
 * - Idempotent: merge(A,A) = A
 * - Convergent: All replicas eventually converge to same state
 */
typedef struct {
    /**
     * @brief Unique item identifier
     *
     * WHAT: Globally unique ID across entire swarm
     * WHY:  Identify same item from different sources
     * HOW:  Upper 16 bits = drone_id, lower 16 bits = local_id
     *
     * ENCODING: (drone_id << 16) | local_id
     * EXAMPLE: Drone 3, local ID 42 → 0x0003002A
     */
    uint32_t item_id;

    /**
     * @brief Salience (priority/importance)
     *
     * WHAT: How important is this item?
     * WHY:  Competition for limited workspace capacity
     * RANGE: 0.0 (ignorable) to 1.0 (critical)
     *
     * CONFLICT RESOLUTION: Higher salience wins on concurrent updates
     */
    float salience;

    /**
     * @brief Vector clock for causality tracking
     *
     * WHAT: Logical time per drone (Lamport clocks)
     * WHY:  Detect concurrent vs causal updates
     * HOW:  vector_clock[i] = number of events from drone i
     *
     * SIZE: MAX_SWARM_SIZE (16 drones max)
     *
     * CAUSALITY RULES:
     * - A happened-before B: ∀i. A[i] ≤ B[i] ∧ ∃j. A[j] < B[j]
     * - A concurrent B: ∃i,j. A[i] < B[i] ∧ A[j] > B[j]
     * - A = B: ∀i. A[i] = B[i]
     */
    uint64_t vector_clock[COLLECTIVE_WORKSPACE_MAX_SWARM_SIZE];

    /**
     * @brief Item type (semantic category)
     */
    workspace_item_type_t type;

    /**
     * @brief Content vector (feature representation)
     *
     * WHAT: Semantic content of the item
     * WHY:  What information does this item carry?
     * SIZE: CONTENT_DIM (16 floats)
     *
     * MERGING: Compatible types blend content, incompatible types use winner's
     */
    float content[COLLECTIVE_WORKSPACE_CONTENT_DIM];

    /**
     * @brief Creation timestamp (milliseconds)
     *
     * WHAT: When was this item created?
     * WHY:  Age-based pruning, recency weighting
     * NOTE: Physical time (not logical time like vector clock)
     */
    uint64_t timestamp_ms;

    /**
     * @brief Source drone ID
     *
     * WHAT: Which drone originally created this item?
     * WHY:  Track provenance, routing, trust
     * RANGE: 0 to MAX_SWARM_SIZE-1
     */
    uint16_t source_drone;

    /**
     * @brief Broadcast count (how many times shared)
     *
     * WHAT: How many times has this item been broadcast?
     * WHY:  Prevent broadcast storms, track propagation
     * NOTE: Incremented on each broadcast
     */
    uint16_t broadcast_count;

    /**
     * @brief Mark for broadcast flag
     *
     * WHAT: Should this item be broadcast to neighbors?
     * WHY:  Explicit broadcast policy control
     * NOTE: Set by broadcast policy, cleared after sending
     */
    bool marked_for_broadcast;

} collective_workspace_item_t;

/**
 * @brief Collective workspace configuration
 */
typedef struct {
    uint16_t local_drone_id;            /**< This drone's ID (0 to swarm_size-1) */
    uint16_t swarm_size;                /**< Total number of drones in swarm */
    float broadcast_threshold;          /**< Minimum salience to broadcast */
    uint32_t coherence_window_ms;       /**< Time window for coherence computation */
    uint32_t pruning_interval_ms;       /**< How often to prune low-salience items */
    uint32_t item_ttl_ms;               /**< Item time-to-live */
    float min_salience;                 /**< Minimum salience to keep item */
    bool enable_meta_cognition;         /**< Track collective coherence, alignment */
} collective_workspace_config_t;

/**
 * @brief Collective workspace instance
 *
 * WHAT: Distributed workspace shared across drone swarm
 * WHY:  Collective attention and shared situational awareness
 * HOW:  CRDT-based eventually consistent data structure
 *
 * THREAD-SAFETY: Protected by mutex (multi-threaded access safe)
 */
typedef struct collective_workspace {
    // Configuration (immutable after creation)
    collective_workspace_config_t config;

    // Workspace items (top-K most salient)
    collective_workspace_item_t items[COLLECTIVE_WORKSPACE_MAX_ITEMS];
    uint32_t item_count;                /**< Current number of items */

    // Local state
    uint16_t local_drone_id;            /**< This drone's ID */
    uint16_t swarm_size;                /**< Swarm size */
    uint64_t local_clock;               /**< Local logical clock (increments on each operation) */

    // Collective metrics
    float collective_coherence;         /**< Swarm alignment metric [0,1] */
    float collective_salience;          /**< Average salience across all items */
    uint32_t total_items_received;      /**< Total items received from others */
    uint32_t total_items_sent;          /**< Total items broadcast to others */

    // Meta-cognition state
    bool meta_cognition_active;         /**< Is meta-level tracking enabled? */
    float swarm_focus_vector[COLLECTIVE_WORKSPACE_CONTENT_DIM]; /**< Average of top items */

    // Timing
    uint64_t last_prune_time_ms;        /**< Last pruning timestamp */
    uint64_t creation_time_ms;          /**< Workspace creation time */

    // Thread safety
    nimcp_mutex_t mutex;              /**< Protects all workspace state */

    // Statistics
    uint64_t merge_conflicts;           /**< Number of concurrent updates resolved */
    uint64_t items_pruned;              /**< Total items removed by pruning */
    uint64_t broadcasts_sent;           /**< Total broadcasts sent */

} collective_workspace_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create collective workspace with default configuration
 *
 * WHAT: Allocate and initialize workspace with sensible defaults
 * WHY:  Convenient creation for common use cases
 *
 * @param local_drone_id This drone's unique ID
 * @param swarm_size Total number of drones in swarm
 * @return Workspace handle or NULL on failure
 *
 * COMPLEXITY: O(1)
 * MALLOC: Yes (workspace structure)
 * THREAD-SAFE: Yes (initialization only)
 */
collective_workspace_t* collective_workspace_create_simple(
    uint16_t local_drone_id,
    uint16_t swarm_size
);

/**
 * @brief Create collective workspace with custom configuration
 *
 * WHAT: Allocate and initialize workspace with specified parameters
 * WHY:  Flexible creation for specialized scenarios
 *
 * @param config Configuration parameters (NULL = use defaults)
 * @return Workspace handle or NULL on failure
 *
 * COMPLEXITY: O(1)
 * MALLOC: Yes (workspace structure)
 * THREAD-SAFE: Yes (initialization only)
 *
 * VALIDATION: Config parameters validated, clamped to valid ranges
 */
collective_workspace_t* collective_workspace_create(
    const collective_workspace_config_t* config
);

/**
 * @brief Destroy collective workspace
 *
 * WHAT: Free all allocated memory and resources
 * WHY:  Clean resource management
 *
 * @param workspace Workspace to destroy (can be NULL)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No (caller must ensure exclusive access)
 * IDEMPOTENT: Yes (safe to call with NULL)
 */
void collective_workspace_destroy(collective_workspace_t* workspace);

//=============================================================================
// Core Operations
//=============================================================================

/**
 * @brief Add local item to collective workspace
 *
 * WHAT: Insert item created by this drone
 * WHY:  Share local observations/decisions with swarm
 * HOW:  Initialize vector clock, add to workspace, mark for broadcast if salient
 *
 * ALGORITHM:
 * 1. Increment local logical clock
 * 2. Set item.vector_clock[local_drone_id] = local_clock
 * 3. Set item.source_drone = local_drone_id
 * 4. Insert into workspace (may evict low-salience items)
 * 5. If item.salience > broadcast_threshold, mark for broadcast
 *
 * @param workspace Collective workspace handle
 * @param item Item to add (vector_clock will be initialized)
 * @return true if successfully added, false on error
 *
 * COMPLEXITY: O(N) where N = item_count (find insertion point)
 * THREAD-SAFE: Yes (mutex protected)
 *
 * NOTE: Item is copied into workspace (caller retains ownership)
 */
bool collective_workspace_add_item(
    collective_workspace_t* workspace,
    const collective_workspace_item_t* item
);

/**
 * @brief Merge remote item into collective workspace (CRDT operation)
 *
 * WHAT: Receive and merge item from another drone
 * WHY:  Synchronize collective workspace across swarm
 * HOW:  CRDT merge using vector clocks and salience
 *
 * ALGORITHM (Last-Writer-Wins + Vector Clock + Salience):
 * 1. Find existing item with same item_id
 * 2. If not found:
 *    - Insert new item (may evict low-salience items)
 * 3. If found (conflict):
 *    a. Compare vector clocks for causality:
 *       - If incoming happened-after existing: Replace (causal)
 *       - If existing happened-after incoming: Keep existing (causal)
 *       - If concurrent: Resolve by salience (higher wins)
 *         - If salience equal: Resolve by timestamp (newer wins)
 *    b. If types compatible and salience similar: Blend content vectors
 *    c. Update vector clock to max(incoming, existing) per dimension
 * 4. Update collective metrics (coherence, focus vector)
 *
 * CRDT PROPERTIES:
 * - Commutative: merge(A,B) = merge(B,A)
 * - Idempotent: merge(A,A) = A
 * - Convergent: All replicas converge
 *
 * @param workspace Collective workspace handle
 * @param item Item received from another drone
 * @return true if merged successfully, false on error
 *
 * COMPLEXITY: O(N) where N = item_count (find + merge)
 * THREAD-SAFE: Yes (mutex protected)
 *
 * SIDE EFFECTS:
 * - May evict low-salience items if workspace full
 * - Updates collective_coherence
 * - Increments merge_conflicts if concurrent update resolved
 */
bool collective_workspace_merge_item(
    collective_workspace_t* workspace,
    const collective_workspace_item_t* item
);

/**
 * @brief Get top-K most salient items from workspace
 *
 * WHAT: Retrieve highest-priority items for attention
 * WHY:  Focus on most important information
 *
 * @param workspace Collective workspace handle
 * @param top_items Output array (caller allocates, at least max_items size)
 * @param max_items Maximum items to retrieve
 * @param actual_count Output: actual items returned
 * @return true if successful
 *
 * COMPLEXITY: O(N) where N = item_count (already sorted by salience)
 * THREAD-SAFE: Yes (mutex protected, read-only)
 *
 * ORDER: Items sorted by salience descending (top_items[0] = highest)
 */
bool collective_workspace_get_top_items(
    const collective_workspace_t* workspace,
    collective_workspace_item_t* top_items,
    uint32_t max_items,
    uint32_t* actual_count
);

/**
 * @brief Get items marked for broadcast
 *
 * WHAT: Retrieve items that should be sent to neighbors
 * WHY:  Implement broadcast policy (salience-based propagation)
 * HOW:  Return items with marked_for_broadcast flag set
 *
 * @param workspace Collective workspace handle
 * @param broadcast_items Output array (caller allocates)
 * @param max_items Maximum items to retrieve
 * @param actual_count Output: actual items returned
 * @return true if successful
 *
 * COMPLEXITY: O(N) where N = item_count (scan for marked items)
 * THREAD-SAFE: Yes (mutex protected)
 *
 * NOTE: Clears marked_for_broadcast flag after retrieval (consume semantics)
 */
bool collective_workspace_get_broadcast_items(
    collective_workspace_t* workspace,
    collective_workspace_item_t* broadcast_items,
    uint32_t max_items,
    uint32_t* actual_count
);

/**
 * @brief Check if item should be broadcast to swarm
 *
 * WHAT: Evaluate broadcast policy for single item
 * WHY:  Decide whether to propagate item to neighbors
 * HOW:  Check salience threshold and broadcast count
 *
 * POLICY:
 * - salience > broadcast_threshold
 * - broadcast_count < max (prevent storms)
 * - not too old (within TTL)
 *
 * @param workspace Collective workspace handle
 * @param item Item to evaluate
 * @return true if should broadcast
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (read-only)
 */
bool collective_workspace_should_broadcast(
    const collective_workspace_t* workspace,
    const collective_workspace_item_t* item
);

/**
 * @brief Mark item for broadcast
 *
 * WHAT: Explicitly flag item for propagation
 * WHY:  Manual control over broadcast policy
 *
 * @param workspace Collective workspace handle
 * @param item_id ID of item to mark
 * @return true if found and marked
 *
 * COMPLEXITY: O(N) where N = item_count
 * THREAD-SAFE: Yes (mutex protected)
 */
bool collective_workspace_mark_for_broadcast(
    collective_workspace_t* workspace,
    uint32_t item_id
);

/**
 * @brief Prune low-salience and old items
 *
 * WHAT: Remove items below threshold or past TTL
 * WHY:  Maintain workspace capacity, remove stale information
 * HOW:  Scan items, remove if salience < min OR age > TTL
 *
 * ALGORITHM:
 * 1. For each item:
 *    - If age > item_ttl_ms: Remove (stale)
 *    - Else if salience < min_salience: Remove (unimportant)
 * 2. Compact workspace (remove gaps)
 * 3. Update collective metrics
 *
 * @param workspace Collective workspace handle
 * @param current_time_ms Current time (for age computation)
 * @return Number of items pruned
 *
 * COMPLEXITY: O(N) where N = item_count
 * THREAD-SAFE: Yes (mutex protected)
 *
 * SIDE EFFECTS: Updates items_pruned counter
 */
uint32_t collective_workspace_prune(
    collective_workspace_t* workspace,
    uint64_t current_time_ms
);

//=============================================================================
// Query and Metrics Functions
//=============================================================================

/**
 * @brief Compute collective coherence (swarm alignment)
 *
 * WHAT: How aligned is the swarm on current priorities?
 * WHY:  Measure collective decision-making quality
 * HOW:  Compute similarity of top items across time window
 *
 * ALGORITHM:
 * 1. Get top-K items from recent window (coherence_window_ms)
 * 2. Compute average content vector (swarm focus)
 * 3. Measure variance from average
 * 4. Coherence = 1.0 - normalized_variance
 *
 * INTERPRETATION:
 * - 1.0 = Perfect alignment (all drones agree)
 * - 0.5 = Moderate alignment (some consensus)
 * - 0.0 = No alignment (random priorities)
 *
 * @param workspace Collective workspace handle
 * @return Coherence value [0,1]
 *
 * COMPLEXITY: O(N × D) where N=items, D=content_dim
 * THREAD-SAFE: Yes (mutex protected)
 */
float collective_workspace_get_coherence(
    const collective_workspace_t* workspace
);

/**
 * @brief Get collective focus vector (swarm attention centroid)
 *
 * WHAT: Average content of top items (what is swarm focused on?)
 * WHY:  Represents collective attention/priority
 *
 * @param workspace Collective workspace handle
 * @param focus_vector Output buffer (size = CONTENT_DIM)
 * @return true if successful
 *
 * COMPLEXITY: O(N × D) where N=items, D=content_dim
 * THREAD-SAFE: Yes (mutex protected)
 */
bool collective_workspace_get_focus_vector(
    const collective_workspace_t* workspace,
    float* focus_vector
);

/**
 * @brief Get number of items in workspace
 *
 * @param workspace Collective workspace handle
 * @return Item count
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
uint32_t collective_workspace_get_item_count(
    const collective_workspace_t* workspace
);

/**
 * @brief Get workspace statistics
 *
 * WHAT: Performance and usage metrics
 * WHY:  Monitor workspace dynamics, debug behavior
 *
 * @param workspace Collective workspace handle
 * @param total_received Output: total items received
 * @param total_sent Output: total items broadcast
 * @param merge_conflicts Output: concurrent updates resolved
 * @param items_pruned Output: total items pruned
 * @return true if successful
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (mutex protected)
 */
bool collective_workspace_get_statistics(
    const collective_workspace_t* workspace,
    uint32_t* total_received,
    uint32_t* total_sent,
    uint64_t* merge_conflicts,
    uint64_t* items_pruned
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get default configuration
 *
 * @param local_drone_id This drone's ID
 * @param swarm_size Total swarm size
 * @return Default configuration
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (pure function)
 */
collective_workspace_config_t collective_workspace_default_config(
    uint16_t local_drone_id,
    uint16_t swarm_size
);

/**
 * @brief Convert item type to string
 *
 * @param type Item type enum
 * @return String name (e.g., "THREAT"), or "UNKNOWN" if invalid
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (const char*)
 */
const char* workspace_item_type_to_string(workspace_item_type_t type);

/**
 * @brief Print workspace state for debugging
 *
 * WHAT: Human-readable dump of workspace state
 * WHY:  Debugging, monitoring, development
 *
 * @param workspace Collective workspace handle
 * @param verbose If true, print detailed item info
 *
 * COMPLEXITY: O(N) where N = item_count
 * THREAD-SAFE: Yes (mutex protected)
 * OUTPUT: stderr
 */
void collective_workspace_print_state(
    const collective_workspace_t* workspace,
    bool verbose
);

/**
 * @brief Validate workspace configuration
 *
 * WHAT: Check if configuration is valid
 * WHY:  Catch invalid configs before creation
 *
 * @param config Configuration to validate
 * @param error_msg Output buffer for error message (can be NULL)
 * @param error_msg_len Size of error message buffer
 * @return true if valid, false if invalid
 *
 * CHECKS:
 * - local_drone_id < swarm_size
 * - swarm_size <= MAX_SWARM_SIZE
 * - broadcast_threshold in [0,1]
 * - coherence_window_ms > 0
 * - min_salience in [0,1]
 */
bool collective_workspace_validate_config(
    const collective_workspace_config_t* config,
    char* error_msg,
    size_t error_msg_len
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_COLLECTIVE_WORKSPACE_H
