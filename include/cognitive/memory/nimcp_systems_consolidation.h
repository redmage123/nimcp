/**
 * @file nimcp_systems_consolidation.h
 * @brief Phase M2: Systems consolidation - Hippocampus to Cortex memory transfer
 *
 * WHAT: Implements gradual transfer of memories from hippocampus to cortex
 * WHY:  Models sleep-dependent systems consolidation (McClelland et al., 1995)
 * HOW:  Replay during sleep extracts semantic features and builds cortical networks
 *
 * BIOLOGICAL BASIS:
 * - Hippocampus rapidly encodes episodic memories (Phase M1)
 * - During sleep, memories replay at 10-20x speed (Wilson & McNaughton, 1994)
 * - Replay drives cortical plasticity, creating stable semantic representations
 * - Over time, cortical traces become independent of hippocampus
 * - Episodic details fade, semantic/gist remains (Winocur & Moscovitch, 2011)
 *
 * KEY FEATURES:
 * - Sleep replay (SWS prioritizes, REM integrates)
 * - Semantic feature extraction (episodic → semantic)
 * - Cortical network building (similarity-based linking)
 * - Gradual hippocampal dependency reduction
 * - Forgetting of unrehearsed memories
 *
 * REFERENCES:
 * - McClelland, J.L. et al. (1995). "Why there are complementary learning systems"
 * - Wilson, M.A. & McNaughton, B.L. (1994). "Reactivation of hippocampal ensemble"
 * - Born, J. & Wilhelm, I. (2012). "System consolidation during sleep"
 * - Winocur, G. & Moscovitch, M. (2011). "Memory transformation and systems consolidation"
 *
 * @version Phase M2
 * @date 2025-11-13
 */

#ifndef NIMCP_SYSTEMS_CONSOLIDATION_H
#define NIMCP_SYSTEMS_CONSOLIDATION_H

#include <stdint.h>
#include <stdbool.h>
#include "cognitive/memory/nimcp_engram.h"  // Phase M1 dependency

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants and Configuration
//=============================================================================

// Default capacities
#define CONSOLIDATION_DEFAULT_CORTICAL_CAPACITY    2048  // Max cortical nodes
#define CONSOLIDATION_DEFAULT_REPLAY_QUEUE_SIZE    4096  // Max replay events per cycle
#define CONSOLIDATION_DEFAULT_NEIGHBORS_PER_NODE      8  // Semantic similarity links

// Consolidation parameters (biological timescales)
#define CONSOLIDATION_TRANSFER_RATE_SWS         0.05f    // 5% per hour in SWS
#define CONSOLIDATION_TRANSFER_RATE_AWAKE       0.001f   // 0.1% per hour awake
#define CONSOLIDATION_SEMANTIC_THRESHOLD        0.7f     // When episodic → semantic
#define CONSOLIDATION_FORGETTING_RATE           0.002f   // Decay per hour
#define CONSOLIDATION_REPLAY_FREQUENCY_SWS      10.0f    // Hz during SWS
#define CONSOLIDATION_REPLAY_SPEED_MULTIPLIER   15.0f    // 15x biological speed

// Memory types
typedef enum {
    CORTICAL_MEMORY_EPISODIC,     // Still has episodic details
    CORTICAL_MEMORY_SEMANTIC,     // Abstracted to semantic gist
    CORTICAL_MEMORY_SCHEMA        // Generalized schema/concept
} cortical_memory_type_t;

//=============================================================================
// Core Data Structures
//=============================================================================

/**
 * @struct cortical_memory_node_t
 * @brief A single memory representation in the cortex
 *
 * WHAT: Stores abstracted/semantic memory features in cortical network
 * WHY:  Cortex learns slowly but retains stably (complementary learning systems)
 * HOW:  Extracted from engrams during replay, linked by semantic similarity
 *
 * BIOLOGICAL MAPPING:
 * - Represents distributed cortical activity patterns
 * - Features = semantic dimensions (e.g., "animal", "food", "danger")
 * - Neighbors = lateral cortical connections (similar concepts)
 * - Consolidation strength = synaptic weight stability
 * - Hippocampal dependency = reliance on hippocampal reactivation
 */
typedef struct cortical_memory_node {
    // Identity and content
    uint64_t id;                          // Unique node ID
    float* features;                      // Semantic feature vector
    uint32_t feature_dim;                 // Dimensionality of feature space
    cortical_memory_type_t type;          // Episodic, semantic, or schema

    // Consolidation state
    float consolidation_strength;         // 0.0 (new) → 1.0 (fully consolidated)
    float hippocampal_dependency;         // 1.0 (dependent) → 0.0 (independent)
    uint64_t creation_time_ms;            // When node was created
    uint64_t last_activation_ms;          // Last time node was activated

    // Cortical network structure
    struct cortical_memory_node** neighbors;  // Semantically similar nodes
    float* neighbor_strengths;                // Connection weights (0.0-1.0)
    uint32_t neighbor_count;                  // Number of neighbors
    uint32_t neighbor_capacity;               // Allocated neighbor slots

    // Source tracking
    uint64_t source_engram_id;            // Original hippocampal engram (Phase M1)
    bool is_transferred;                  // True if fully transferred from hippocampus

    // Phase 1.5: Copy-on-Write support for efficient feature sharing
    uint32_t* _cow_refcount;              /**< Shared reference count (NULL = owned) */
    bool _cow_is_shallow;                 /**< True if features pointer is shared */
} cortical_memory_node_t;

/**
 * @struct replay_event_t
 * @brief A single memory replay event during sleep
 *
 * WHAT: Represents coordinated hippocampal-cortical replay
 * WHY:  Replay drives cortical plasticity (Born & Wilhelm, 2012)
 * HOW:  Reactivates engram, extracts features, updates cortical node
 *
 * BIOLOGICAL MAPPING:
 * - Models sharp-wave ripples in hippocampus (SWS)
 * - Models theta sequences in hippocampus (REM)
 * - Coordinated with cortical slow oscillations
 * - Prioritizes emotional/salient memories
 */
typedef struct replay_event {
    uint64_t engram_id;                   // Which hippocampal engram to replay
    uint64_t cortical_node_id;            // Target cortical node (0 = create new)
    float priority;                       // Replay priority (0.0-1.0)
    float emotional_salience;             // From engram's emotional tag
    uint64_t scheduled_time_ms;           // When to replay
    bool is_completed;                    // True if replay executed
} replay_event_t;

/**
 * @struct systems_consolidation_system_t
 * @brief Main system managing hippocampus → cortex memory transfer
 *
 * WHAT: Orchestrates sleep replay and cortical network building
 * WHY:  Models systems consolidation over days/weeks (McClelland et al., 1995)
 * HOW:  Prioritizes replays during sleep, extracts semantics, builds cortical links
 *
 * INTEGRATION POINTS:
 * - Phase M1 (engrams): Source of memories to consolidate
 * - Sleep system: Triggers consolidation during SWS/REM
 * - Emotional system: Prioritizes salient memories
 */
typedef struct systems_consolidation_system {
    // Cortical memory storage
    cortical_memory_node_t** cortical_nodes;  // Array of cortical memory nodes
    uint32_t node_count;                      // Current number of nodes
    uint32_t node_capacity;                   // Maximum nodes (allocated)

    // Replay management
    replay_event_t* replay_queue;         // Pending replay events
    uint32_t replay_queue_size;           // Current queue size
    uint32_t replay_queue_capacity;       // Maximum queue size
    float replay_frequency_hz;            // Current replay rate (Hz)
    uint64_t last_replay_time_ms;         // Last replay timestamp

    // Consolidation parameters
    float transfer_rate;                  // Current transfer rate (per hour)
    float forgetting_rate;                // Memory decay rate (per hour)
    float semantic_threshold;             // When episodic → semantic

    // System references (not owned)
    engram_system_t* engram_system;       // Phase M1 engram system
    void* sleep_system;                   // Sleep-wake cycle system (opaque)

    // Statistics
    uint64_t total_replays;               // Lifetime replay count
    uint64_t total_transfers;             // Memories fully transferred
    uint64_t total_forgotten;             // Memories forgotten

    // Phase 1.5: Memory pools for hot-path allocations
    void* node_pool;                      /**< Pool for cortical node structs */
    void* feature_pool;                   /**< Pool for feature vectors */
    void* neighbor_pool;                  /**< Pool for neighbor arrays */

    // Bio-async integration (system-level, not per-node)
    void* bio_ctx;                        /**< bio_module_context_t pointer */
    bool bio_async_enabled;               /**< Bio-async registration status */
} systems_consolidation_system_t;

//=============================================================================
// Core API: System Management
//=============================================================================

/**
 * @brief Create a new systems consolidation system
 *
 * WHAT: Initializes hippocampus → cortex memory transfer system
 * WHY:  Required before any consolidation can occur
 * HOW:  Allocates cortical node storage and replay queue
 *
 * @return Pointer to new system, or NULL on failure
 *
 * USAGE:
 * systems_consolidation_system_t* sys = systems_consolidation_create();
 * if (!sys) // handle error
 */
systems_consolidation_system_t* systems_consolidation_create(void);

/**
 * @brief Destroy systems consolidation system and free all memory
 *
 * WHAT: Cleans up all cortical nodes, replay queue, and system resources
 * WHY:  Prevents memory leaks
 * HOW:  Frees all allocated memory in correct order
 *
 * @param system System to destroy (can be NULL)
 *
 * USAGE:
 * systems_consolidation_destroy(sys);
 */
void systems_consolidation_destroy(systems_consolidation_system_t* system);

/**
 * @brief Reset system to initial state (clears all memories)
 *
 * WHAT: Removes all cortical nodes and pending replays
 * WHY:  Useful for testing or starting fresh
 * HOW:  Frees all nodes and resets counters, keeps allocated capacity
 *
 * @param system System to reset
 *
 * USAGE:
 * systems_consolidation_reset(sys);
 */
void systems_consolidation_reset(systems_consolidation_system_t* system);

//=============================================================================
// Core API: Sleep Replay
//=============================================================================

/**
 * @brief Schedule a memory replay event during sleep
 *
 * WHAT: Adds engram to replay queue for consolidation
 * WHY:  Models sleep-dependent memory replay (Wilson & McNaughton, 1994)
 * HOW:  Prioritizes by emotional salience and consolidation state
 *
 * @param system Consolidation system
 * @param engram_id Which engram to replay (from Phase M1)
 * @param priority Replay priority (0.0-1.0, higher = sooner)
 * @return true if scheduled, false if queue full
 *
 * BIOLOGICAL BASIS:
 * - Prioritizes emotional memories (amygdala modulation)
 * - Prioritizes recent memories (recency effect)
 * - Respects replay bandwidth limits
 *
 * USAGE:
 * bool scheduled = systems_consolidation_schedule_replay(sys, engram_id, 0.8f);
 */
bool systems_consolidation_schedule_replay(
    systems_consolidation_system_t* system,
    uint64_t engram_id,
    float priority);

/**
 * @brief Execute pending replay events (called during sleep)
 *
 * WHAT: Processes replay queue and drives cortical plasticity
 * WHY:  Models coordinated hippocampal-cortical replay during SWS
 * HOW:  Dequeues events, extracts semantics, updates/creates cortical nodes
 *
 * @param system Consolidation system
 * @param time_delta_seconds Time since last update (typically ~0.1s)
 * @param is_sws true if in slow-wave sleep (high replay rate)
 * @param is_rem true if in REM sleep (integration/abstraction)
 * @return Number of replays executed
 *
 * BIOLOGICAL TIMING:
 * - SWS: ~10 Hz replay rate, prioritizes consolidation
 * - REM: Lower rate, prioritizes integration/abstraction
 * - Awake: Minimal replay (spontaneous reactivation)
 *
 * USAGE:
 * uint32_t replays = systems_consolidation_execute_replays(sys, 0.1f, true, false);
 */
uint32_t systems_consolidation_execute_replays(
    systems_consolidation_system_t* system,
    float time_delta_seconds,
    bool is_sws,
    bool is_rem);

//=============================================================================
// Core API: Cortical Transfer
//=============================================================================

/**
 * @brief Transfer engram features to cortical node (during replay)
 *
 * WHAT: Extracts semantic features from engram and updates cortical node
 * WHY:  Models cortical learning during replay (Born & Wilhelm, 2012)
 * HOW:  Computes semantic features, finds/creates cortical node, updates weights
 *
 * @param system Consolidation system
 * @param engram_id Source hippocampal engram
 * @param replay_strength How strongly to update (0.0-1.0)
 * @return ID of updated/created cortical node, or 0 on failure
 *
 * SEMANTIC EXTRACTION:
 * - Averages engram neuron activations into semantic features
 * - Reduces dimensionality (episodic → semantic)
 * - Preserves gist, loses details (Winocur & Moscovitch, 2011)
 *
 * USAGE:
 * uint64_t node_id = systems_consolidation_transfer_to_cortex(sys, engram_id, 0.5f);
 */
uint64_t systems_consolidation_transfer_to_cortex(
    systems_consolidation_system_t* system,
    uint64_t engram_id,
    float replay_strength);

/**
 * @brief Update consolidation state of all cortical nodes
 *
 * WHAT: Strengthens/weakens cortical nodes based on time and sleep
 * WHY:  Models gradual consolidation over days/weeks
 * HOW:  Increases consolidation_strength, decreases hippocampal_dependency
 *
 * @param system Consolidation system
 * @param time_delta_seconds Time since last update
 * @param is_sleeping true if sleeping (accelerated consolidation)
 *
 * CONSOLIDATION DYNAMICS:
 * - Sleep: Fast consolidation (~5% per hour in SWS)
 * - Awake: Slow consolidation (~0.1% per hour)
 * - Unrehearsed memories decay (forgetting)
 * - Episodic → semantic transition at threshold
 *
 * USAGE:
 * systems_consolidation_update(sys, 0.1f, true);
 */
void systems_consolidation_update(
    systems_consolidation_system_t* system,
    float time_delta_seconds,
    bool is_sleeping);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Retrieve cortical node by ID
 *
 * WHAT: Looks up cortical node in memory network
 * WHY:  Needed for inspection, testing, and recall
 * HOW:  Linear search through cortical_nodes array
 *
 * @param system Consolidation system
 * @param node_id Node ID to find
 * @return Pointer to node, or NULL if not found
 *
 * USAGE:
 * cortical_memory_node_t* node = systems_consolidation_get_node(sys, node_id);
 * if (node) // use node
 */
cortical_memory_node_t* systems_consolidation_get_node(
    const systems_consolidation_system_t* system,
    uint64_t node_id);

/**
 * @brief Find cortical nodes similar to query features
 *
 * WHAT: Semantic similarity search in cortical memory network
 * WHY:  Enables generalization and schema activation
 * HOW:  Computes cosine similarity, returns top-k matches
 *
 * @param system Consolidation system
 * @param query_features Feature vector to match
 * @param feature_dim Dimensionality of query features
 * @param max_results Maximum results to return
 * @param results_out Array to store matching node IDs
 * @param similarities_out Array to store similarity scores (0.0-1.0)
 * @return Number of results found
 *
 * USAGE:
 * uint64_t results[10];
 * float similarities[10];
 * uint32_t count = systems_consolidation_find_similar(
 *     sys, features, dim, 10, results, similarities);
 */
uint32_t systems_consolidation_find_similar(
    const systems_consolidation_system_t* system,
    const float* query_features,
    uint32_t feature_dim,
    uint32_t max_results,
    uint64_t* results_out,
    float* similarities_out);

/**
 * @brief Get system statistics
 *
 * WHAT: Retrieves consolidation system metrics
 * WHY:  Useful for monitoring, debugging, and testing
 * HOW:  Returns counts and rates from system state
 *
 * @param system Consolidation system
 * @param total_nodes_out Total cortical nodes created
 * @param total_replays_out Total replay events executed
 * @param total_transfers_out Memories fully transferred
 * @param total_forgotten_out Memories forgotten/decayed
 * @param pending_replays_out Current replay queue size
 *
 * USAGE:
 * uint32_t nodes, pending;
 * uint64_t replays, transfers, forgotten;
 * systems_consolidation_get_statistics(
 *     sys, &nodes, &replays, &transfers, &forgotten, &pending);
 */
void systems_consolidation_get_statistics(
    const systems_consolidation_system_t* system,
    uint32_t* total_nodes_out,
    uint64_t* total_replays_out,
    uint64_t* total_transfers_out,
    uint64_t* total_forgotten_out,
    uint32_t* pending_replays_out);

//=============================================================================
// Integration API
//=============================================================================

/**
 * @brief Connect consolidation system to Phase M1 engram system
 *
 * WHAT: Links consolidation system to hippocampal engram storage
 * WHY:  Required for replay and transfer operations
 * HOW:  Stores non-owning pointer to engram system
 *
 * @param system Consolidation system
 * @param engram_system Phase M1 engram system (must outlive consolidation system)
 *
 * USAGE:
 * systems_consolidation_set_engram_system(sys, brain->engram_system);
 */
void systems_consolidation_set_engram_system(
    systems_consolidation_system_t* system,
    engram_system_t* engram_system);

/**
 * @brief Connect consolidation system to sleep-wake cycle system
 *
 * WHAT: Links consolidation to sleep state information
 * WHY:  Sleep state controls consolidation rate and replay frequency
 * HOW:  Stores opaque pointer to sleep system
 *
 * @param system Consolidation system
 * @param sleep_system Sleep-wake cycle system (opaque pointer)
 *
 * USAGE:
 * systems_consolidation_set_sleep_system(sys, brain->sleep_system);
 */
void systems_consolidation_set_sleep_system(
    systems_consolidation_system_t* system,
    void* sleep_system);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_SYSTEMS_CONSOLIDATION_H
