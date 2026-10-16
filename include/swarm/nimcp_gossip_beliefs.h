/**
 * @file nimcp_gossip_beliefs.h
 * @brief Gossip-Based Belief Propagation System for NIMCP
 *
 * WHAT: Spread beliefs through the swarm using gossip protocol
 * WHY:  Enable distributed consensus, rumor spreading, and belief convergence across agents
 * HOW:  Probabilistic gossip with belief decay, credibility weighting, and contradiction detection
 *
 * BIOLOGICAL INSPIRATION: Social information transmission in animal groups and human societies
 *
 * Gossip protocols are nature's way of spreading information through decentralized networks.
 * Observed in primate groups, bird flocks, and human societies, gossip enables:
 * - Rapid information diffusion without central coordination
 * - Emergent consensus from local interactions
 * - Credibility-based information filtering
 * - Detection of contradictory information
 *
 * KEY FEATURES:
 * - Probabilistic gossip propagation (tunable gossip rate)
 * - Belief strength decay over time
 * - Source credibility weighting
 * - Automatic contradiction detection
 * - Neural encoding of belief content
 * - Propagation tracking for diffusion analysis
 * - Consensus belief identification
 * - Full bio-async integration
 *
 * @version 1.0
 * @date 2025
 */

#ifndef NIMCP_GOSSIP_BELIEFS_H
#define NIMCP_GOSSIP_BELIEFS_H

#include "core/brain/nimcp_brain.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "utils/time/nimcp_time.h"
#include "utils/containers/nimcp_hash_table.h"
#include "utils/thread/nimcp_thread.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct gossip_beliefs gossip_beliefs_t;

/* ============================================================================
 * Belief Structures
 * ============================================================================ */

/**
 * @brief Individual belief held by an agent
 *
 * WHAT: A belief with certainty, source, and propagation tracking
 * WHY:  Represent uncertain knowledge that spreads through the swarm
 * HOW:  Neural vector encoding with certainty weight and metadata
 */
typedef struct {
    uint32_t belief_id;             /**< Unique belief identifier */
    char topic[128];                /**< Belief topic (human-readable) */
    float certainty;                /**< Belief certainty: 0.0 (uncertain) to 1.0 (certain) */
    float* belief_vector;           /**< Neural representation of belief */
    uint32_t vector_size;           /**< Size of belief vector */
    uint32_t source_agent_id;       /**< Agent who originated the belief */
    uint32_t propagation_count;     /**< Number of times propagated (diffusion metric) */
    uint64_t first_heard_ms;        /**< When belief was first heard */
} belief_t;

/**
 * @brief Configuration for gossip beliefs system
 *
 * WHAT: Tunable parameters for gossip behavior
 * WHY:  Allow customization of gossip dynamics and filtering
 * HOW:  Configuration structure passed at creation time
 */
typedef struct {
    float gossip_probability;           /**< Probability of gossiping per round (0.0-1.0) */
    uint32_t max_gossip_targets;        /**< Max agents to gossip to per round */
    float belief_decay_rate;            /**< How fast beliefs weaken (per time unit) */
    float credibility_weight;           /**< Weight of source credibility (0.0-1.0) */
    bool enable_contradiction_detection; /**< Detect contradictory beliefs */
    bool enable_bio_async;              /**< Enable bio-async messaging */
} gossip_beliefs_config_t;

/* ============================================================================
 * Core API Functions
 * ============================================================================ */

/**
 * @brief Create a new gossip beliefs system
 *
 * WHAT: Allocates and initializes the gossip beliefs system
 * WHY:  Required before any gossip operations can be performed
 * HOW:  Allocates structures, creates hash tables, initializes gossip state
 *
 * @param config System configuration (required)
 * @return Pointer to created system, or NULL on failure
 */
gossip_beliefs_t* gossip_beliefs_create(const gossip_beliefs_config_t* config);

/**
 * @brief Destroy gossip beliefs system and free all resources
 *
 * WHAT: Cleanly deallocates all memory and resources
 * WHY:  Prevent memory leaks and ensure proper cleanup
 * HOW:  Frees beliefs, hash tables, and main structure
 *
 * @param gb Gossip system to destroy (NULL-safe)
 */
void gossip_beliefs_destroy(gossip_beliefs_t* gb);

/**
 * @brief Initialize gossip system with optional bio-async context
 *
 * WHAT: Sets up bio-async messaging and registers message handlers
 * WHY:  Enable swarm communication for belief propagation
 * HOW:  Registers with bio-router, sets up inbox processing
 *
 * @param gb Gossip system
 * @param bio_ctx Bio-async context (optional, can be NULL)
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
nimcp_result_t gossip_beliefs_init(gossip_beliefs_t* gb, void* bio_ctx);

/* ============================================================================
 * Belief Management API
 * ============================================================================ */

/**
 * @brief Introduce a new belief into the swarm
 *
 * WHAT: Add a belief to an agent's belief set
 * WHY:  Inject new information into the gossip network
 * HOW:  Creates belief entry, stores in agent's belief hash table
 *
 * @param gb Gossip system
 * @param agent_id Agent adopting the belief
 * @param belief Belief to introduce (copied into system, belief_id updated with assigned ID)
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
int gossip_introduce_belief(gossip_beliefs_t* gb, uint32_t agent_id, belief_t* belief);

/**
 * @brief Update certainty of an existing belief
 *
 * WHAT: Modify how certain an agent is about a belief
 * WHY:  Beliefs change strength based on evidence and repetition
 * HOW:  Updates certainty field, bounded to [0.0, 1.0]
 *
 * @param gb Gossip system
 * @param agent_id Agent holding the belief
 * @param belief_id Belief to update
 * @param new_certainty New certainty value (0.0-1.0)
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
int gossip_update_belief(gossip_beliefs_t* gb, uint32_t agent_id, uint32_t belief_id,
                          float new_certainty);

/**
 * @brief Remove a belief from an agent
 *
 * WHAT: Delete a belief from an agent's belief set
 * WHY:  Beliefs can be forgotten or explicitly rejected
 * HOW:  Removes from hash table, frees belief structure
 *
 * @param gb Gossip system
 * @param agent_id Agent holding the belief
 * @param belief_id Belief to remove
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
int gossip_remove_belief(gossip_beliefs_t* gb, uint32_t agent_id, uint32_t belief_id);

/* ============================================================================
 * Gossip Propagation API
 * ============================================================================ */

/**
 * @brief Execute one round of gossip propagation
 *
 * WHAT: Propagate beliefs according to gossip protocol
 * WHY:  Drive belief diffusion through the swarm
 * HOW:  For each agent, probabilistically gossip beliefs to random neighbors
 *
 * @param gb Gossip system
 * @param current_time_ms Current simulation time
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
int gossip_propagate_round(gossip_beliefs_t* gb, uint64_t current_time_ms);

/**
 * @brief Get all beliefs held by a specific agent
 *
 * WHAT: Retrieve agent's current belief set
 * WHY:  Query what an agent believes
 * HOW:  Returns array of beliefs from agent's hash table
 *
 * @param gb Gossip system
 * @param agent_id Agent to query
 * @param beliefs Output: array of beliefs (allocated by callee, caller must free)
 * @param count Output: number of beliefs returned
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
int gossip_get_agent_beliefs(gossip_beliefs_t* gb, uint32_t agent_id,
                              belief_t** beliefs, uint32_t* count);

/**
 * @brief Apply belief decay over time
 *
 * WHAT: Weaken beliefs according to decay rate
 * WHY:  Beliefs fade without reinforcement
 * HOW:  Reduces certainty of all beliefs exponentially
 *
 * @param gb Gossip system
 * @param current_time_ms Current simulation time
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
int gossip_apply_decay(gossip_beliefs_t* gb, uint64_t current_time_ms);

/* ============================================================================
 * Consensus and Analysis API
 * ============================================================================ */

/**
 * @brief Get consensus beliefs across the swarm
 *
 * WHAT: Identify beliefs held by majority of agents
 * WHY:  Determine what the swarm collectively believes
 * HOW:  Counts belief holders, returns beliefs above threshold
 *
 * @param gb Gossip system
 * @param consensus Output: array of consensus beliefs (allocated by callee)
 * @param count Output: number of consensus beliefs
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
int gossip_get_consensus_beliefs(gossip_beliefs_t* gb, belief_t** consensus, uint32_t* count);

/**
 * @brief Detect contradictory beliefs in the swarm
 *
 * WHAT: Find pairs of beliefs that contradict each other
 * WHY:  Identify information conflicts for resolution
 * HOW:  Compares belief vectors using semantic similarity/opposition
 *
 * @param gb Gossip system
 * @param contradiction_pairs Output: array of (belief_id1, belief_id2) pairs
 * @param num_contradictions Output: number of contradiction pairs
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
int gossip_detect_contradictions(gossip_beliefs_t* gb, uint32_t** contradiction_pairs,
                                  uint32_t* num_contradictions);

/**
 * @brief Calculate belief entropy (diversity) in the swarm
 *
 * WHAT: Measure how diverse beliefs are across agents
 * WHY:  High entropy = disagreement, low entropy = consensus
 * HOW:  Shannon entropy over belief distribution
 *
 * @param gb Gossip system
 * @return Entropy value (higher = more diverse beliefs)
 */
float gossip_calculate_entropy(gossip_beliefs_t* gb);

/* ============================================================================
 * Agent Management API
 * ============================================================================ */

/**
 * @brief Register an agent with the gossip system
 *
 * WHAT: Add an agent to the gossip network
 * WHY:  Agents must be registered to participate in gossip
 * HOW:  Creates belief storage for agent, adds to agent list
 *
 * @param gb Gossip system
 * @param agent_id Agent identifier
 * @param credibility Agent's credibility score (0.0-1.0)
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
int gossip_register_agent(gossip_beliefs_t* gb, uint32_t agent_id, float credibility);

/**
 * @brief Unregister an agent from the gossip system
 *
 * WHAT: Remove an agent from the gossip network
 * WHY:  Agent has left the swarm or gone offline
 * HOW:  Removes agent's beliefs, removes from agent list
 *
 * @param gb Gossip system
 * @param agent_id Agent to unregister
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
int gossip_unregister_agent(gossip_beliefs_t* gb, uint32_t agent_id);

/**
 * @brief Update agent credibility
 *
 * WHAT: Modify an agent's credibility score
 * WHY:  Credibility changes based on reliability of past information
 * HOW:  Updates credibility field, affects belief propagation weights
 *
 * @param gb Gossip system
 * @param agent_id Agent to update
 * @param credibility New credibility (0.0-1.0)
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
int gossip_update_credibility(gossip_beliefs_t* gb, uint32_t agent_id, float credibility);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Process incoming bio-async message
 *
 * WHAT: Handle received gossip messages
 * WHY:  Enable belief reception from other agents
 * HOW:  Parses message, deserializes belief, integrates locally
 *
 * @param gb Gossip system
 * @param msg Incoming message
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
nimcp_result_t gossip_beliefs_process_message(gossip_beliefs_t* gb, const void* msg);

/**
 * @brief Process inbox messages (call periodically)
 *
 * WHAT: Process pending gossip messages from bio-async inbox
 * WHY:  Receive and integrate gossiped beliefs from swarm
 * HOW:  Polls inbox, processes each message, updates local beliefs
 *
 * @param gb Gossip system
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed
 */
uint32_t gossip_beliefs_process_inbox(gossip_beliefs_t* gb, uint32_t max_messages);

/* ============================================================================
 * Statistics and Monitoring API
 * ============================================================================ */

/**
 * @brief Get gossip system statistics
 *
 * WHAT: Retrieve metrics about belief propagation
 * WHY:  Monitor gossip dynamics and convergence
 * HOW:  Returns counts, averages, and state information
 *
 * @param gb Gossip system
 * @param total_beliefs Output: total beliefs in system
 * @param total_agents Output: total registered agents
 * @param avg_certainty Output: average belief certainty
 * @param total_gossips Output: total gossip operations
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
int gossip_get_stats(gossip_beliefs_t* gb, uint32_t* total_beliefs,
                     uint32_t* total_agents, float* avg_certainty,
                     uint32_t* total_gossips);

/**
 * @brief Print gossip system status to log
 *
 * WHAT: Log current system state and statistics
 * WHY:  Debug and monitor gossip system
 * HOW:  Logs all key metrics and configuration
 *
 * @param gb Gossip system
 * @param verbose Include detailed information
 */
void gossip_beliefs_print_status(const gossip_beliefs_t* gb, bool verbose);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Create a belief
 *
 * WHAT: Helper to construct a belief structure
 * WHY:  Simplify belief creation with proper initialization
 * HOW:  Allocates and initializes belief structure
 *
 * @param topic Belief topic string
 * @param belief_vector Neural encoding of belief
 * @param vector_size Size of belief vector
 * @param certainty Initial certainty (0.0-1.0)
 * @param source_agent_id Originating agent
 * @return Allocated belief (caller must free vector and struct)
 */
belief_t* belief_create(const char* topic, const float* belief_vector,
                        uint32_t vector_size, float certainty,
                        uint32_t source_agent_id);

/**
 * @brief Destroy a belief
 *
 * WHAT: Free belief and its vector
 * WHY:  Proper cleanup of belief resources
 * HOW:  Frees vector array and belief struct
 *
 * @param belief Belief to destroy (NULL-safe)
 */
void belief_destroy(belief_t* belief);

/**
 * @brief Calculate semantic similarity between two beliefs
 *
 * WHAT: Compute how similar two beliefs are
 * WHY:  Detect redundant or contradictory beliefs
 * HOW:  Cosine similarity of belief vectors
 *
 * @param belief1 First belief
 * @param belief2 Second belief
 * @return Similarity score (-1.0 = opposite, 0.0 = orthogonal, 1.0 = identical)
 */
float belief_similarity(const belief_t* belief1, const belief_t* belief2);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GOSSIP_BELIEFS_H */
