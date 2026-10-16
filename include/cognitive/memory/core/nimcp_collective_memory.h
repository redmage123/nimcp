//=============================================================================
// nimcp_collective_memory.h - Collective Memory System for Multi-Agent Sync
//=============================================================================
/**
 * @file nimcp_collective_memory.h
 * @brief Shared memory across multiple agents with cultural memory support
 *
 * WHAT: Implements collective memory for multi-agent systems with cultural
 *       knowledge, consensus mechanisms, and memory propagation
 * WHY:  Agents in a collective need to share experiences, build cultural
 *       knowledge, and reach consensus on shared memories
 * HOW:  Uses entanglement graphs for inter-agent links, prime signatures
 *       for content-addressable shared memories, and propagation models
 *       for memory spreading through agent populations
 *
 * NEUROSCIENCE & SOCIOLOGY FOUNDATION:
 *
 *   Collective Memory Theory:
 *   +-----------------------------------------------------------------------+
 *   |  Based on research in social psychology and cultural cognition:       |
 *   |                                                                       |
 *   |  1. CULTURAL MEMORY (Assmann)                                         |
 *   |     - Shared knowledge transmitted across generations                  |
 *   |     - Institutionalized through rituals, symbols, practices            |
 *   |     - Highly stable, slow mutation rate                                |
 *   |                                                                       |
 *   |  2. COMMUNICATIVE MEMORY (Halbwachs)                                   |
 *   |     - Everyday shared experiences within groups                        |
 *   |     - Higher mutation rate as memories are retold                      |
 *   |     - Lifespan of ~3-4 generations without institutionalization        |
 *   |                                                                       |
 *   |  3. CONSENSUS MEMORY (Social Constructionism)                          |
 *   |     - Group agreement shapes "truth" of shared memories                |
 *   |     - Conformity pressure drives convergence                           |
 *   |     - Minority viewpoints may be preserved or lost                     |
 *   +-----------------------------------------------------------------------+
 *
 *   Memory Propagation Model:
 *   +-----------------------------------------------------------------------+
 *   |  Memories spread through agent populations via:                        |
 *   |                                                                       |
 *   |  1. DIRECT TRANSMISSION: Agent-to-agent memory sharing                 |
 *   |     - Fidelity depends on encoding strength                            |
 *   |     - Modulated by trust/reliability scores                            |
 *   |                                                                       |
 *   |  2. SOCIAL CONTAGION: Epidemic-style spreading (SIR model)             |
 *   |     - Propagation rate = f(relevance, novelty, emotional valence)      |
 *   |     - Network topology affects spread patterns                         |
 *   |                                                                       |
 *   |  3. CULTURAL SELECTION: Some memories become institutionalized         |
 *   |     - High consensus + high retention = cultural memory                |
 *   |     - Natural selection on memory "fitness"                            |
 *   |                                                                       |
 *   |  4. DRIFT: Random variation accumulates over time                      |
 *   |     - Mutation rate per transmission                                   |
 *   |     - Can measure drift from original via signature distance           |
 *   +-----------------------------------------------------------------------+
 *
 *   Consensus Mechanisms:
 *   +-----------------------------------------------------------------------+
 *   |  How agents agree on shared memory content:                            |
 *   |                                                                       |
 *   |  VOTING: Majority version wins                                         |
 *   |  WEIGHTED: Trust-weighted consensus                                    |
 *   |  BAYESIAN: Probabilistic belief update                                 |
 *   |  MERGING: Combine compatible versions                                  |
 *   +-----------------------------------------------------------------------+
 *
 * PERFORMANCE:
 * - Memory share: O(1) average
 * - Sync between agents: O(M) where M = shared memories
 * - Consensus computation: O(N * M) where N = agents, M = memory versions
 * - Propagation simulation: O(N * E) where E = network edges
 *
 * MEMORY:
 * - collective_memory_t: ~128 bytes per memory
 * - agent_memory_state_t: ~64 bytes + memory array
 * - collective_memory_system_t: ~256 bytes + arrays
 *
 * INTEGRATION:
 * - Core: Uses PR memory nodes, entanglement graphs, prime signatures
 * - Middleware: Integrates with resonance scoring for similarity
 * - API: Multi-agent memory coordination
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#ifndef NIMCP_COLLECTIVE_MEMORY_H
#define NIMCP_COLLECTIVE_MEMORY_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "nimcp_quaternion.h"
#include "nimcp_prime_signature.h"
#include "nimcp_entanglement.h"
#include "nimcp_pr_memory_node.h"

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

/** Maximum number of agents in a collective */
#define COLLECTIVE_MAX_AGENTS               1024

/** Maximum number of collective memories */
#define COLLECTIVE_MAX_MEMORIES             65536

/** Default consensus threshold for memory acceptance */
#define COLLECTIVE_DEFAULT_CONSENSUS        0.5f

/** Default propagation rate */
#define COLLECTIVE_DEFAULT_PROPAGATION      0.1f

/** Default retention rate */
#define COLLECTIVE_DEFAULT_RETENTION        0.95f

/** Default mutation rate */
#define COLLECTIVE_DEFAULT_MUTATION         0.01f

/** Default sync interval in milliseconds */
#define COLLECTIVE_DEFAULT_SYNC_INTERVAL    1000.0f

/** Minimum number of agents for consensus */
#define COLLECTIVE_MIN_AGENTS_CONSENSUS     2

/** Epsilon for floating point comparisons */
#define COLLECTIVE_EPSILON                  1e-6f

/** Maximum drift threshold before memory is considered divergent */
#define COLLECTIVE_MAX_DRIFT                0.5f

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Types of collective memory
 *
 * WHAT: Classification of shared memories by their nature and persistence
 * WHY:  Different memory types have different propagation and retention behaviors
 */
typedef enum {
    COLLECTIVE_CULTURAL = 0,    /**< Long-standing cultural knowledge (slow change) */
    COLLECTIVE_EPISODIC,        /**< Shared experiences (moderate change) */
    COLLECTIVE_PROCEDURAL,      /**< Shared skills/practices (slow change) */
    COLLECTIVE_SEMANTIC,        /**< Shared facts/concepts (moderate change) */
    COLLECTIVE_TYPE_COUNT       /**< Number of collective types */
} collective_type_t;

/**
 * @brief Consensus computation methods
 *
 * WHAT: How to compute consensus version of a memory
 * WHY:  Different consensus methods suit different scenarios
 */
typedef enum {
    CONSENSUS_MAJORITY = 0,     /**< Simple majority voting */
    CONSENSUS_WEIGHTED,         /**< Trust-weighted voting */
    CONSENSUS_BAYESIAN,         /**< Bayesian belief aggregation */
    CONSENSUS_MERGE,            /**< Merge compatible versions */
    CONSENSUS_LEADER,           /**< Accept leader's version */
    CONSENSUS_METHOD_COUNT      /**< Number of consensus methods */
} consensus_method_t;

/**
 * @brief Propagation model types
 *
 * WHAT: How memories spread through agent networks
 * WHY:  Different models capture different social dynamics
 */
typedef enum {
    PROPAGATION_DIRECT = 0,     /**< Direct agent-to-agent transmission */
    PROPAGATION_BROADCAST,      /**< One-to-many broadcast */
    PROPAGATION_EPIDEMIC,       /**< SIR-style epidemic spreading */
    PROPAGATION_CASCADE,        /**< Cascade through network structure */
    PROPAGATION_MODEL_COUNT     /**< Number of propagation models */
} propagation_model_t;

/**
 * @brief Memory sync status
 *
 * WHAT: Current synchronization state of a memory
 * WHY:  Track progress of memory synchronization across agents
 */
typedef enum {
    SYNC_UNKNOWN = 0,           /**< Status not determined */
    SYNC_LOCAL_ONLY,            /**< Memory exists only locally */
    SYNC_SYNCING,               /**< Currently synchronizing */
    SYNC_SYNCED,                /**< Fully synchronized across agents */
    SYNC_CONFLICTED,            /**< Conflict detected between versions */
    SYNC_DIVERGED               /**< Memory has diverged beyond threshold */
} sync_status_t;

/**
 * @brief Error codes for collective memory operations
 */
typedef enum {
    COLLECTIVE_SUCCESS = 0,              /**< Operation succeeded */
    COLLECTIVE_ERROR_NULL_POINTER = -1,  /**< NULL pointer argument */
    COLLECTIVE_ERROR_INVALID_ID = -2,    /**< Invalid memory or agent ID */
    COLLECTIVE_ERROR_NO_MEMORY = -3,     /**< Memory allocation failed */
    COLLECTIVE_ERROR_MAX_AGENTS = -4,    /**< Maximum agents exceeded */
    COLLECTIVE_ERROR_MAX_MEMORIES = -5,  /**< Maximum memories exceeded */
    COLLECTIVE_ERROR_NOT_FOUND = -6,     /**< Memory or agent not found */
    COLLECTIVE_ERROR_CONFLICT = -7,      /**< Unresolvable conflict */
    COLLECTIVE_ERROR_SYNC_FAILED = -8,   /**< Synchronization failed */
    COLLECTIVE_ERROR_INVALID_CONFIG = -9 /**< Invalid configuration */
} collective_error_t;

/**
 * @brief A memory shared across multiple agents
 *
 * WHAT: Core structure representing a collective memory
 * WHY:  Tracks all aspects of a shared memory including versions and consensus
 * HOW:  Combines PR memory node with multi-agent metadata
 *
 * Memory layout: ~128 bytes (excluding dynamic arrays)
 */
typedef struct {
    //-------------------------------------------------------------------------
    // Identity
    //-------------------------------------------------------------------------
    uint64_t memory_id;                 /**< Unique collective memory identifier */
    collective_type_t type;             /**< Type of collective memory */

    //-------------------------------------------------------------------------
    // Content
    //-------------------------------------------------------------------------
    prime_signature_t content_signature; /**< Content signature for matching */
    nimcp_quaternion_t shared_quaternion; /**< Shared semantic state */

    //-------------------------------------------------------------------------
    // Sharing State
    //-------------------------------------------------------------------------
    uint64_t* agent_ids;                /**< Array of agents sharing this memory */
    size_t num_agents;                  /**< Number of sharing agents */
    size_t agent_capacity;              /**< Capacity of agent_ids array */
    float* agent_versions;              /**< Version number per agent */
    float consensus_strength;           /**< Agreement level [0, 1] */

    //-------------------------------------------------------------------------
    // Propagation Parameters
    //-------------------------------------------------------------------------
    float propagation_rate;             /**< How fast memory spreads [0, 1] */
    float retention_rate;               /**< How well retained [0, 1] */
    float mutation_rate;                /**< How much it changes per transmission */

    //-------------------------------------------------------------------------
    // Origin Information
    //-------------------------------------------------------------------------
    uint64_t origin_agent_id;           /**< Agent who created this memory */
    float origin_time;                  /**< Time of creation (simulation time) */

    //-------------------------------------------------------------------------
    // Sync Status
    //-------------------------------------------------------------------------
    sync_status_t sync_status;          /**< Current synchronization status */
    float last_sync_time;               /**< Last synchronization timestamp */

    //-------------------------------------------------------------------------
    // Underlying Memory Node
    //-------------------------------------------------------------------------
    pr_memory_node_t* memory_node;      /**< PR memory node for content storage */

} collective_memory_t;

/**
 * @brief State of an agent's memories
 *
 * WHAT: Tracks all memories held by a specific agent
 * WHY:  Enables per-agent sync tracking and reliability scoring
 */
typedef struct {
    uint64_t agent_id;                  /**< Unique agent identifier */
    prime_signature_t* memories;        /**< Agent's versions of memories */
    uint64_t* memory_ids;               /**< IDs of memories held */
    float* memory_versions;             /**< Version numbers per memory */
    size_t num_memories;                /**< Number of memories held */
    size_t memory_capacity;             /**< Capacity of memory arrays */
    float sync_time;                    /**< Last synchronization time */
    float reliability;                  /**< How reliable this agent's memories [0,1] */
    bool is_leader;                     /**< Whether this agent is a group leader */
} agent_memory_state_t;

/**
 * @brief Configuration for collective memory system
 *
 * WHAT: Parameters controlling collective memory behavior
 * WHY:  Different collectives have different requirements
 */
typedef struct {
    float sync_interval;                /**< Time between sync cycles */
    float consensus_threshold;          /**< Minimum consensus for acceptance */
    float drift_threshold;              /**< Maximum allowed memory drift */
    consensus_method_t consensus_method; /**< How to compute consensus */
    propagation_model_t propagation_model; /**< How memories spread */
    bool auto_sync;                     /**< Enable automatic synchronization */
    bool preserve_minorities;           /**< Keep minority memory versions */
    float cultural_threshold;           /**< Consensus level for cultural promotion */
} collective_memory_config_t;

/**
 * @brief Collective memory system manager
 *
 * WHAT: Top-level structure managing the collective memory system
 * WHY:  Central coordination of multi-agent memory sharing
 * HOW:  Integrates entanglement graphs, memory nodes, and agent states
 *
 * Memory layout: ~256 bytes (excluding dynamic arrays)
 */
typedef struct {
    //-------------------------------------------------------------------------
    // PR Integration
    //-------------------------------------------------------------------------
    entangle_graph_t entanglement;      /**< Graph for inter-memory associations */
    pr_node_manager_t node_manager;     /**< Node manager for memory creation */

    //-------------------------------------------------------------------------
    // Collective Memories
    //-------------------------------------------------------------------------
    collective_memory_t** memories;     /**< Array of collective memories */
    size_t num_memories;                /**< Number of collective memories */
    size_t memory_capacity;             /**< Capacity of memories array */

    //-------------------------------------------------------------------------
    // Agent States
    //-------------------------------------------------------------------------
    agent_memory_state_t* agents;       /**< Array of agent states */
    size_t num_agents;                  /**< Number of agents */
    size_t agent_capacity;              /**< Capacity of agents array */

    //-------------------------------------------------------------------------
    // Sync State
    //-------------------------------------------------------------------------
    float current_time;                 /**< Current simulation time */
    float last_sync_time;               /**< Last global sync time */
    uint64_t next_memory_id;            /**< Next memory ID to assign */
    uint64_t next_agent_id;             /**< Next agent ID to assign */

    //-------------------------------------------------------------------------
    // Configuration
    //-------------------------------------------------------------------------
    collective_memory_config_t config;  /**< System configuration */

} collective_memory_system_t;

/**
 * @brief Result of a consensus computation
 *
 * WHAT: Outcome of consensus algorithm on a memory
 * WHY:  Reports consensus version and agreement statistics
 */
typedef struct {
    prime_signature_t consensus_signature; /**< Agreed-upon content signature */
    nimcp_quaternion_t consensus_state;    /**< Agreed-upon semantic state */
    float consensus_level;                 /**< Level of agreement [0, 1] */
    size_t agreeing_agents;                /**< Number of agents in agreement */
    size_t total_agents;                   /**< Total agents considered */
    bool conflict_resolved;                /**< Whether conflicts were resolved */
} consensus_result_t;

/**
 * @brief Memory drift measurement
 *
 * WHAT: How much a memory has changed from its origin
 * WHY:  Track memory evolution and detect divergence
 */
typedef struct {
    uint64_t memory_id;                 /**< Memory being measured */
    float signature_drift;              /**< Signature distance from origin */
    float state_drift;                  /**< Quaternion distance from origin */
    float total_drift;                  /**< Combined drift metric */
    float drift_rate;                   /**< Rate of drift per time unit */
    bool is_divergent;                  /**< Whether drift exceeds threshold */
} drift_measurement_t;

/**
 * @brief Propagation simulation result
 *
 * WHAT: Outcome of memory propagation simulation
 * WHY:  Understand how memory will spread through network
 */
typedef struct {
    uint64_t memory_id;                 /**< Memory being propagated */
    size_t initial_agents;              /**< Agents with memory at start */
    size_t final_agents;                /**< Agents with memory at end */
    float propagation_time;             /**< Time to reach steady state */
    float coverage;                     /**< Fraction of agents reached */
    float final_mutation;               /**< Average mutation from original */
} propagation_result_t;

/**
 * @brief Statistics for collective memory system
 *
 * WHAT: Operational metrics for the collective memory system
 * WHY:  Monitoring, debugging, and analysis
 */
typedef struct {
    size_t num_agents;                  /**< Current agent count */
    size_t num_memories;                /**< Current memory count */
    size_t memories_by_type[COLLECTIVE_TYPE_COUNT]; /**< Count per type */
    float avg_consensus;                /**< Average consensus across memories */
    float avg_agents_per_memory;        /**< Average sharing */
    size_t sync_operations;             /**< Total sync operations */
    size_t conflicts_detected;          /**< Total conflicts detected */
    size_t conflicts_resolved;          /**< Conflicts successfully resolved */
    float avg_drift;                    /**< Average memory drift */
    uint64_t total_propagations;        /**< Total propagation events */
} collective_stats_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default collective memory configuration
 *
 * WHAT: Returns sensible default configuration
 * WHY:  Provides starting point for most use cases
 *
 * @return Default configuration with:
 *         - sync_interval: 1000.0ms
 *         - consensus_threshold: 0.5
 *         - drift_threshold: 0.5
 *         - consensus_method: CONSENSUS_WEIGHTED
 *         - propagation_model: PROPAGATION_EPIDEMIC
 *         - auto_sync: true
 *         - preserve_minorities: true
 *         - cultural_threshold: 0.8
 *
 * Performance: ~5ns
 */
NIMCP_EXPORT collective_memory_config_t collective_memory_default_config(void);

/**
 * @brief Validate collective memory configuration
 *
 * WHAT: Checks configuration for validity
 * WHY:  Prevent invalid configs causing errors
 *
 * @param config Configuration to validate
 * @return true if valid, false otherwise
 *
 * Performance: ~10ns
 */
NIMCP_EXPORT bool collective_memory_config_validate(
    const collective_memory_config_t* config);

//=============================================================================
// System Lifecycle Functions
//=============================================================================

/**
 * @brief Create a collective memory system
 *
 * WHAT: Allocates and initializes the collective memory system
 * WHY:  Entry point for multi-agent memory coordination
 * HOW:  Creates entanglement graph, initializes arrays, sets config
 *
 * @param node_manager PR node manager for memory creation
 * @param config System configuration (NULL for defaults)
 * @return Collective memory system or NULL on failure
 *
 * Performance: O(1)
 * Memory: ~256 bytes + initial arrays
 *
 * Example:
 *   collective_memory_config_t config = collective_memory_default_config();
 *   config.auto_sync = false;  // Manual sync control
 *   collective_memory_system_t* system = collective_memory_create(mgr, &config);
 */
NIMCP_EXPORT collective_memory_system_t* collective_memory_create(
    pr_node_manager_t node_manager,
    const collective_memory_config_t* config);

/**
 * @brief Destroy collective memory system
 *
 * WHAT: Frees all resources associated with the system
 * WHY:  Resource cleanup
 * HOW:  Frees all memories, agent states, and internal structures
 *
 * @param system System to destroy (NULL safe)
 *
 * Performance: O(N + M) where N = agents, M = memories
 */
NIMCP_EXPORT void collective_memory_destroy(collective_memory_system_t* system);

/**
 * @brief Reset collective memory system
 *
 * WHAT: Clears all memories and agents while keeping system allocated
 * WHY:  Restart without reallocation overhead
 *
 * @param system System to reset
 * @return COLLECTIVE_SUCCESS or error code
 *
 * Performance: O(N + M)
 */
NIMCP_EXPORT collective_error_t collective_memory_reset(
    collective_memory_system_t* system);

//=============================================================================
// Agent Management Functions
//=============================================================================

/**
 * @brief Add an agent to the collective
 *
 * WHAT: Registers a new agent in the collective memory system
 * WHY:  Agents must be registered before sharing memories
 * HOW:  Creates agent state, assigns ID, initializes memory arrays
 *
 * @param system Collective memory system
 * @param reliability Agent's reliability score [0, 1]
 * @param is_leader Whether agent is a group leader
 * @param agent_id_out Output: assigned agent ID
 * @return COLLECTIVE_SUCCESS or error code
 *
 * Performance: O(1) average
 *
 * Example:
 *   uint64_t agent_id;
 *   collective_memory_add_agent(system, 0.9f, false, &agent_id);
 */
NIMCP_EXPORT collective_error_t collective_memory_add_agent(
    collective_memory_system_t* system,
    float reliability,
    bool is_leader,
    uint64_t* agent_id_out);

/**
 * @brief Remove an agent from the collective
 *
 * WHAT: Unregisters an agent and updates memory sharing
 * WHY:  Agents may leave the collective
 * HOW:  Removes agent from all shared memories, frees state
 *
 * @param system Collective memory system
 * @param agent_id Agent to remove
 * @return COLLECTIVE_SUCCESS or error code
 *
 * Performance: O(M) where M = memories shared by agent
 */
NIMCP_EXPORT collective_error_t collective_memory_remove_agent(
    collective_memory_system_t* system,
    uint64_t agent_id);

/**
 * @brief Get agent state
 *
 * WHAT: Retrieves current state of an agent
 * WHY:  Inspect agent's memories and sync status
 *
 * @param system Collective memory system
 * @param agent_id Agent to query
 * @param state_out Output: agent state (caller-allocated)
 * @return COLLECTIVE_SUCCESS or error code
 *
 * Performance: O(1)
 */
NIMCP_EXPORT collective_error_t collective_memory_get_agent(
    collective_memory_system_t* system,
    uint64_t agent_id,
    agent_memory_state_t* state_out);

/**
 * @brief Update agent reliability
 *
 * WHAT: Modifies an agent's reliability score
 * WHY:  Reliability may change based on behavior
 *
 * @param system Collective memory system
 * @param agent_id Agent to update
 * @param reliability New reliability score [0, 1]
 * @return COLLECTIVE_SUCCESS or error code
 *
 * Performance: O(1)
 */
NIMCP_EXPORT collective_error_t collective_memory_update_agent_reliability(
    collective_memory_system_t* system,
    uint64_t agent_id,
    float reliability);

//=============================================================================
// Memory Sharing Functions
//=============================================================================

/**
 * @brief Share a memory with the collective
 *
 * WHAT: Creates a new collective memory from a local memory
 * WHY:  Allow agents to share experiences with the group
 * HOW:  Creates collective memory, links to origin agent
 *
 * @param system Collective memory system
 * @param agent_id Agent sharing the memory
 * @param memory_node PR memory node to share
 * @param type Type of collective memory
 * @param memory_id_out Output: assigned collective memory ID
 * @return COLLECTIVE_SUCCESS or error code
 *
 * Performance: O(1)
 *
 * Example:
 *   uint64_t col_id;
 *   collective_memory_share(system, agent_id, node, COLLECTIVE_EPISODIC, &col_id);
 */
NIMCP_EXPORT collective_error_t collective_memory_share(
    collective_memory_system_t* system,
    uint64_t agent_id,
    pr_memory_node_t* memory_node,
    collective_type_t type,
    uint64_t* memory_id_out);

/**
 * @brief Agent adopts a collective memory
 *
 * WHAT: Adds an agent to the sharing list of a memory
 * WHY:  Agents receive memories from the collective
 * HOW:  Updates memory's agent list, creates local copy for agent
 *
 * @param system Collective memory system
 * @param memory_id Collective memory to adopt
 * @param agent_id Agent adopting the memory
 * @return COLLECTIVE_SUCCESS or error code
 *
 * Performance: O(1)
 */
NIMCP_EXPORT collective_error_t collective_memory_adopt(
    collective_memory_system_t* system,
    uint64_t memory_id,
    uint64_t agent_id);

/**
 * @brief Agent releases a collective memory
 *
 * WHAT: Removes an agent from memory's sharing list
 * WHY:  Agents may forget or abandon shared memories
 *
 * @param system Collective memory system
 * @param memory_id Collective memory to release
 * @param agent_id Agent releasing the memory
 * @return COLLECTIVE_SUCCESS or error code
 *
 * Performance: O(N) where N = sharing agents
 */
NIMCP_EXPORT collective_error_t collective_memory_release(
    collective_memory_system_t* system,
    uint64_t memory_id,
    uint64_t agent_id);

//=============================================================================
// Synchronization Functions
//=============================================================================

/**
 * @brief Synchronize memory across agents
 *
 * WHAT: Attempts to synchronize all agent versions of a memory
 * WHY:  Keep shared memories consistent across the collective
 * HOW:  Computes consensus, propagates to all sharing agents
 *
 * @param system Collective memory system
 * @param memory_id Memory to synchronize
 * @return COLLECTIVE_SUCCESS or error code
 *
 * Performance: O(N) where N = sharing agents
 */
NIMCP_EXPORT collective_error_t collective_memory_sync(
    collective_memory_system_t* system,
    uint64_t memory_id);

/**
 * @brief Synchronize all memories
 *
 * WHAT: Performs full sync cycle across all collective memories
 * WHY:  Batch synchronization for efficiency
 *
 * @param system Collective memory system
 * @return Number of memories successfully synced
 *
 * Performance: O(M * N) where M = memories, N = avg agents per memory
 */
NIMCP_EXPORT size_t collective_memory_sync_all(
    collective_memory_system_t* system);

/**
 * @brief Synchronize between two specific agents
 *
 * WHAT: Syncs memories shared between two agents
 * WHY:  Peer-to-peer synchronization
 *
 * @param system Collective memory system
 * @param agent1_id First agent
 * @param agent2_id Second agent
 * @return Number of memories synchronized
 *
 * Performance: O(M) where M = shared memories
 */
NIMCP_EXPORT size_t collective_memory_sync_agents(
    collective_memory_system_t* system,
    uint64_t agent1_id,
    uint64_t agent2_id);

//=============================================================================
// Consensus Functions
//=============================================================================

/**
 * @brief Compute consensus for a memory
 *
 * WHAT: Determines the consensus version of a collective memory
 * WHY:  Resolve differences between agent versions
 * HOW:  Applies configured consensus method to all agent versions
 *
 * @param system Collective memory system
 * @param memory_id Memory to compute consensus for
 * @param result Output: consensus result
 * @return COLLECTIVE_SUCCESS or error code
 *
 * Performance: O(N) where N = sharing agents
 *
 * Example:
 *   consensus_result_t result;
 *   collective_memory_compute_consensus(system, memory_id, &result);
 *   if (result.consensus_level > 0.8f) {
 *       // Strong consensus - promote to cultural memory
 *   }
 */
NIMCP_EXPORT collective_error_t collective_memory_compute_consensus(
    collective_memory_system_t* system,
    uint64_t memory_id,
    consensus_result_t* result);

/**
 * @brief Apply consensus to a memory
 *
 * WHAT: Updates memory to consensus version and propagates
 * WHY:  Enforce agreed-upon version across agents
 *
 * @param system Collective memory system
 * @param memory_id Memory to update
 * @param result Consensus result to apply
 * @return COLLECTIVE_SUCCESS or error code
 *
 * Performance: O(N) where N = sharing agents
 */
NIMCP_EXPORT collective_error_t collective_memory_apply_consensus(
    collective_memory_system_t* system,
    uint64_t memory_id,
    const consensus_result_t* result);

//=============================================================================
// Memory Propagation Functions
//=============================================================================

/**
 * @brief Propagate memory to new agents
 *
 * WHAT: Spreads memory to agents who don't have it
 * WHY:  Expand reach of collective memories
 * HOW:  Uses configured propagation model to spread
 *
 * @param system Collective memory system
 * @param memory_id Memory to propagate
 * @param target_agents Array of potential recipient agents
 * @param num_targets Number of potential recipients
 * @return Number of agents who received the memory
 *
 * Performance: O(T) where T = target agents
 */
NIMCP_EXPORT size_t collective_memory_propagate(
    collective_memory_system_t* system,
    uint64_t memory_id,
    const uint64_t* target_agents,
    size_t num_targets);

/**
 * @brief Simulate memory propagation
 *
 * WHAT: Models how a memory will spread over time
 * WHY:  Predict memory diffusion without actually propagating
 * HOW:  Runs epidemic model simulation
 *
 * @param system Collective memory system
 * @param memory_id Memory to simulate
 * @param time_steps Number of time steps to simulate
 * @param result Output: propagation simulation result
 * @return COLLECTIVE_SUCCESS or error code
 *
 * Performance: O(T * N * E) where T = steps, N = agents, E = connections
 */
NIMCP_EXPORT collective_error_t collective_memory_simulate_propagation(
    collective_memory_system_t* system,
    uint64_t memory_id,
    size_t time_steps,
    propagation_result_t* result);

//=============================================================================
// Conflict Resolution Functions
//=============================================================================

/**
 * @brief Merge divergent memory versions
 *
 * WHAT: Combines two versions of a memory into one
 * WHY:  Resolve conflicts by merging compatible content
 * HOW:  Combines prime signatures and interpolates states
 *
 * @param system Collective memory system
 * @param memory_id Memory with divergent versions
 * @param agent1_id First agent's version
 * @param agent2_id Second agent's version
 * @return COLLECTIVE_SUCCESS or error code
 *
 * Performance: O(1)
 */
NIMCP_EXPORT collective_error_t collective_memory_merge(
    collective_memory_system_t* system,
    uint64_t memory_id,
    uint64_t agent1_id,
    uint64_t agent2_id);

/**
 * @brief Resolve conflict between versions
 *
 * WHAT: Attempts to resolve conflicting memory versions
 * WHY:  Handle cases where simple merge isn't possible
 * HOW:  Uses consensus or leader decision
 *
 * @param system Collective memory system
 * @param memory_id Memory with conflict
 * @return COLLECTIVE_SUCCESS or COLLECTIVE_ERROR_CONFLICT
 *
 * Performance: O(N) where N = agents with conflicting versions
 */
NIMCP_EXPORT collective_error_t collective_memory_resolve_conflict(
    collective_memory_system_t* system,
    uint64_t memory_id);

//=============================================================================
// Drift and Evolution Functions
//=============================================================================

/**
 * @brief Compute memory drift from origin
 *
 * WHAT: Measures how much a memory has changed
 * WHY:  Track memory evolution and detect divergence
 * HOW:  Compares current signature/state to original
 *
 * @param system Collective memory system
 * @param memory_id Memory to measure
 * @param measurement Output: drift measurement
 * @return COLLECTIVE_SUCCESS or error code
 *
 * Performance: O(1)
 */
NIMCP_EXPORT collective_error_t collective_memory_compute_drift(
    collective_memory_system_t* system,
    uint64_t memory_id,
    drift_measurement_t* measurement);

/**
 * @brief Apply mutation to memory during propagation
 *
 * WHAT: Simulates memory mutation during transmission
 * WHY:  Models realistic memory degradation
 * HOW:  Adds noise to signature based on mutation rate
 *
 * @param system Collective memory system
 * @param memory_id Memory to mutate
 * @param mutation_strength Strength of mutation [0, 1]
 * @return COLLECTIVE_SUCCESS or error code
 *
 * Performance: O(1)
 */
NIMCP_EXPORT collective_error_t collective_memory_apply_mutation(
    collective_memory_system_t* system,
    uint64_t memory_id,
    float mutation_strength);

//=============================================================================
// Cultural Memory Functions
//=============================================================================

/**
 * @brief Get all cultural memories
 *
 * WHAT: Retrieves memories that have achieved cultural status
 * WHY:  Access institutionalized collective knowledge
 *
 * @param system Collective memory system
 * @param memories Output array (caller-allocated)
 * @param max_memories Maximum memories to return
 * @param count Output: actual count returned
 * @return COLLECTIVE_SUCCESS or error code
 *
 * Performance: O(M) where M = total memories
 */
NIMCP_EXPORT collective_error_t collective_memory_get_cultural(
    collective_memory_system_t* system,
    collective_memory_t** memories,
    size_t max_memories,
    size_t* count);

/**
 * @brief Promote memory to cultural status
 *
 * WHAT: Elevates a memory to cultural memory type
 * WHY:  Institutionalize well-established collective knowledge
 *
 * @param system Collective memory system
 * @param memory_id Memory to promote
 * @return COLLECTIVE_SUCCESS or error code
 *
 * Performance: O(1)
 */
NIMCP_EXPORT collective_error_t collective_memory_promote_cultural(
    collective_memory_system_t* system,
    uint64_t memory_id);

/**
 * @brief Check if memory qualifies for cultural promotion
 *
 * WHAT: Tests if memory meets cultural threshold
 * WHY:  Gate cultural promotion decisions
 *
 * @param system Collective memory system
 * @param memory_id Memory to check
 * @return true if memory qualifies, false otherwise
 *
 * Performance: O(1)
 */
NIMCP_EXPORT bool collective_memory_check_cultural_threshold(
    collective_memory_system_t* system,
    uint64_t memory_id);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get a collective memory by ID
 *
 * WHAT: Retrieves a specific collective memory
 * WHY:  Direct access to memory data
 *
 * @param system Collective memory system
 * @param memory_id Memory to retrieve
 * @return Pointer to memory or NULL if not found
 *
 * Performance: O(1)
 */
NIMCP_EXPORT collective_memory_t* collective_memory_get(
    collective_memory_system_t* system,
    uint64_t memory_id);

/**
 * @brief Find memories by type
 *
 * WHAT: Retrieves all memories of a specific type
 * WHY:  Filter memories by classification
 *
 * @param system Collective memory system
 * @param type Type to filter by
 * @param memories Output array (caller-allocated)
 * @param max_memories Maximum to return
 * @param count Output: actual count
 * @return COLLECTIVE_SUCCESS or error code
 *
 * Performance: O(M) where M = total memories
 */
NIMCP_EXPORT collective_error_t collective_memory_find_by_type(
    collective_memory_system_t* system,
    collective_type_t type,
    collective_memory_t** memories,
    size_t max_memories,
    size_t* count);

/**
 * @brief Find memories shared by an agent
 *
 * WHAT: Retrieves all memories an agent shares
 * WHY:  Query agent's collective knowledge
 *
 * @param system Collective memory system
 * @param agent_id Agent to query
 * @param memory_ids Output array of memory IDs (caller-allocated)
 * @param max_memories Maximum to return
 * @param count Output: actual count
 * @return COLLECTIVE_SUCCESS or error code
 *
 * Performance: O(M) where M = agent's memories
 */
NIMCP_EXPORT collective_error_t collective_memory_find_by_agent(
    collective_memory_system_t* system,
    uint64_t agent_id,
    uint64_t* memory_ids,
    size_t max_memories,
    size_t* count);

//=============================================================================
// Statistics Functions
//=============================================================================

/**
 * @brief Get system statistics
 *
 * WHAT: Returns operational metrics for the system
 * WHY:  Monitoring and analysis
 *
 * @param system Collective memory system
 * @param stats Output statistics (caller-allocated)
 * @return COLLECTIVE_SUCCESS or error code
 *
 * Performance: O(M) where M = total memories
 */
NIMCP_EXPORT collective_error_t collective_memory_get_stats(
    collective_memory_system_t* system,
    collective_stats_t* stats);

/**
 * @brief Reset statistics counters
 *
 * WHAT: Clears operational counters
 * WHY:  Start fresh measurement period
 *
 * @param system Collective memory system
 *
 * Performance: O(1)
 */
NIMCP_EXPORT void collective_memory_reset_stats(
    collective_memory_system_t* system);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get error string for error code
 *
 * WHAT: Converts error code to human-readable string
 * WHY:  Debugging and error reporting
 *
 * @param error Error code
 * @return Static string describing error
 */
NIMCP_EXPORT const char* collective_memory_error_string(collective_error_t error);

/**
 * @brief Get collective type name as string
 *
 * WHAT: Converts type enum to name
 * WHY:  Debugging and logging
 *
 * @param type Collective type
 * @return Static string name
 */
NIMCP_EXPORT const char* collective_type_name(collective_type_t type);

/**
 * @brief Get consensus method name as string
 *
 * @param method Consensus method
 * @return Static string name
 */
NIMCP_EXPORT const char* consensus_method_name(consensus_method_t method);

/**
 * @brief Get propagation model name as string
 *
 * @param model Propagation model
 * @return Static string name
 */
NIMCP_EXPORT const char* propagation_model_name(propagation_model_t model);

/**
 * @brief Get sync status name as string
 *
 * @param status Sync status
 * @return Static string name
 */
NIMCP_EXPORT const char* sync_status_name(sync_status_t status);

/**
 * @brief Update system simulation time
 *
 * WHAT: Advances the internal simulation clock
 * WHY:  Enable time-based operations (sync intervals, decay)
 *
 * @param system Collective memory system
 * @param delta_time Time to advance
 */
NIMCP_EXPORT void collective_memory_advance_time(
    collective_memory_system_t* system,
    float delta_time);

/**
 * @brief Print system summary to stdout
 *
 * WHAT: Outputs human-readable system state
 * WHY:  Debugging and inspection
 *
 * @param system Collective memory system
 */
NIMCP_EXPORT void collective_memory_print_summary(
    collective_memory_system_t* system);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_COLLECTIVE_MEMORY_H
