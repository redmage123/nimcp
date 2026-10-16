//=============================================================================
// nimcp_transactive.h - Transactive Memory System for Distributed Expertise
//=============================================================================
/**
 * @file nimcp_transactive.h
 * @brief "Who knows what" memory system for distributed expertise management
 *
 * WHAT: Transactive memory system for tracking and leveraging distributed knowledge
 * WHY:  Groups outperform individuals by distributing expertise and coordinating retrieval
 * HOW:  Directory of expertise mapped to agents, with delegation and verification mechanisms
 *
 * NEUROSCIENCE/COGNITIVE FOUNDATION:
 *
 *   Transactive Memory Theory (Wegner, 1987):
 *   +-----------------------------------------------------------------------+
 *   |  Humans naturally develop shared memory systems in groups:            |
 *   |                                                                       |
 *   |  1. DIFFERENTIATION: Members specialize in different knowledge areas |
 *   |  2. COORDINATION: Members know who knows what                         |
 *   |  3. DELEGATION: Memory retrieval is outsourced to experts            |
 *   |  4. TRUST: Reliance on others' expertise                              |
 *   |                                                                       |
 *   |  Examples:                                                            |
 *   |  - Couples: "Ask your mother about family history"                    |
 *   |  - Teams: "The architect knows the system design"                     |
 *   |  - Organizations: Institutional knowledge distribution                |
 *   +-----------------------------------------------------------------------+
 *
 *   Implementation Model:
 *   +-----------------------------------------------------------------------+
 *   |  Agent Registry                                                        |
 *   |  +-------------------+                                                 |
 *   |  | Agent A           |     Domain Coverage                             |
 *   |  | - expertise: [...] |     +------------------+                        |
 *   |  | - reliability: 0.9 |     | Domain X: A(0.8), B(0.6)                 |
 *   |  +-------------------+     | Domain Y: C(0.9)                          |
 *   |  +-------------------+     | Domain Z: A(0.7), D(0.5)                  |
 *   |  | Agent B           |     +------------------+                        |
 *   |  | - expertise: [...] |                                                 |
 *   |  | - reliability: 0.85|     Delegation Flow                            |
 *   |  +-------------------+     +------------------+                        |
 *   |       ...                  | Query -> Lookup -> Delegate -> Receive   |
 *   |                            | -> Update expertise model                 |
 *   |                            +------------------+                        |
 *   +-----------------------------------------------------------------------+
 *
 *   Prime Resonant Integration:
 *   +-----------------------------------------------------------------------+
 *   |  - Domain signatures: Prime signatures represent knowledge domains     |
 *   |  - Expertise matching: Jaccard similarity for domain overlap          |
 *   |  - Entanglement: Agents connected through shared expertise            |
 *   |  - Resonance: Query-expert matching uses resonance scoring            |
 *   +-----------------------------------------------------------------------+
 *
 * PERFORMANCE:
 * - Agent registration: O(1) average, O(N) rehash
 * - Expertise lookup: O(K) where K = average expertise per agent
 * - Top-K expert selection: O(A * K + K log K) where A = agents
 * - Delegation round-trip: Application-dependent
 *
 * MEMORY:
 * - transactive_memory_t: ~200 bytes + per-agent storage
 * - transactive_agent_t: ~120 bytes + expertise entries
 * - expertise_entry_t: ~40 bytes per domain
 *
 * INTEGRATION:
 * - Prime Signature: Domain representation
 * - Entanglement Graph: Agent-domain connections
 * - Resonance Engine: Query-expert matching
 * - PR Memory Node: Expertise storage
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#ifndef NIMCP_TRANSACTIVE_H
#define NIMCP_TRANSACTIVE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "cognitive/memory/core/nimcp_prime_signature.h"
#include "cognitive/memory/core/nimcp_entanglement.h"
#include "cognitive/memory/core/nimcp_pr_memory_node.h"

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

/** Maximum expertise entries per agent */
#define TRANSACTIVE_MAX_EXPERTISE_PER_AGENT     64

/** Maximum agents in system */
#define TRANSACTIVE_MAX_AGENTS                  1024

/** Maximum domains tracked */
#define TRANSACTIVE_MAX_DOMAINS                 256

/** Maximum concurrent delegations */
#define TRANSACTIVE_MAX_DELEGATIONS             128

/** Maximum recommendations per lookup */
#define TRANSACTIVE_MAX_RECOMMENDATIONS         16

/** Default minimum expertise level for consideration */
#define TRANSACTIVE_MIN_EXPERTISE_THRESHOLD     0.1f

/** Default minimum confidence for expertise claims */
#define TRANSACTIVE_MIN_CONFIDENCE_THRESHOLD    0.5f

/** Default verification decay period (seconds) */
#define TRANSACTIVE_VERIFICATION_DECAY_PERIOD   86400.0f

/** Epsilon for floating point comparisons */
#define TRANSACTIVE_EPSILON                     1e-6f

/** Invalid agent ID sentinel */
#define TRANSACTIVE_INVALID_AGENT_ID            UINT64_MAX

/** Invalid delegation ID sentinel */
#define TRANSACTIVE_INVALID_DELEGATION_ID       UINT64_MAX

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Expertise entry for a specific domain
 *
 * WHAT: An agent's claimed expertise in a knowledge domain
 * WHY:  Track who knows what, with confidence and accessibility
 * HOW:  Prime signature identifies domain, levels track capability
 */
typedef struct {
    uint64_t agent_id;                  /**< Owner agent ID */
    prime_signature_t domain_signature; /**< Knowledge domain (prime representation) */
    float expertise_level;              /**< Self-assessed or measured expertise [0, 1] */
    float confidence;                   /**< Confidence in this assessment [0, 1] */
    float accessibility;                /**< How available is this agent [0, 1] */
    float last_verification;            /**< Timestamp of last verification (seconds since epoch) */
    uint64_t successful_queries;        /**< Count of successful delegations */
    uint64_t failed_queries;            /**< Count of failed delegations */
} expertise_entry_t;

/**
 * @brief Agent in the transactive memory system
 *
 * WHAT: A knowledge holder with expertise profile and meta-information
 * WHY:  Enable "who knows what" lookups for delegation
 * HOW:  Maintains expertise array and performance metrics
 */
typedef struct {
    uint64_t agent_id;                  /**< Unique agent identifier */
    char* agent_name;                   /**< Human-readable name (optional) */

    // Expertise profile
    expertise_entry_t* expertise;       /**< Array of expertise entries */
    size_t num_expertise;               /**< Number of expertise entries */
    size_t expertise_capacity;          /**< Allocated capacity */

    // Meta-knowledge about agent
    float overall_reliability;          /**< General trustworthiness [0, 1] */
    float retrieval_success_rate;       /**< Historical success rate [0, 1] */
    float avg_response_time;            /**< Average response time (ms) */

    // Relationship metrics
    float familiarity;                  /**< How well known is this agent [0, 1] */
    float trust;                        /**< Trust level [0, 1] */

    // Activity tracking
    uint64_t last_interaction_ms;       /**< Last interaction timestamp */
    uint64_t total_interactions;        /**< Total interaction count */
    uint64_t total_delegations;         /**< Times delegated to */
    uint64_t successful_delegations;    /**< Successful delegations */

    // Status
    bool is_active;                     /**< Whether agent is currently available */
    bool is_verified;                   /**< Whether expertise has been verified */
} transactive_agent_t;

/**
 * @brief Result from directory lookup
 *
 * WHAT: Ranked list of agents who may know about a topic
 * WHY:  Enable informed delegation decisions
 * HOW:  Agents sorted by combined expertise and reliability score
 */
typedef struct {
    prime_signature_t query_signature;  /**< What knowledge was requested */
    uint64_t* recommended_agents;       /**< Array of agent IDs (ranked) */
    float* agent_scores;                /**< Score for each agent */
    size_t num_recommendations;         /**< Number of recommendations */
    float best_score;                   /**< Highest scoring agent's score */
    float coverage;                     /**< Estimated domain coverage [0, 1] */
} directory_result_t;

/**
 * @brief Delegation request tracking
 *
 * WHAT: State of an outsourced memory query
 * WHY:  Track pending delegations for timeout/retry
 * HOW:  Records query, target, and timing
 */
typedef struct {
    uint64_t delegation_id;             /**< Unique delegation identifier */
    uint64_t query_id;                  /**< Original query identifier */
    uint64_t target_agent;              /**< Agent delegated to */
    prime_signature_t query_signature;  /**< What was asked */
    uint64_t start_time_ms;             /**< When delegation started */
    uint64_t timeout_ms;                /**< Timeout period */
    bool is_active;                     /**< Whether delegation is pending */
    bool has_response;                  /**< Whether response received */
    float confidence_prediction;        /**< Predicted success probability */
} delegation_state_t;

/**
 * @brief Delegation result
 *
 * WHAT: Outcome of a delegated query
 * WHY:  Process results and update expertise model
 */
typedef struct {
    uint64_t delegation_id;             /**< Delegation identifier */
    uint64_t responding_agent;          /**< Agent that responded */
    bool success;                       /**< Whether query was successful */
    float response_quality;             /**< Quality of response [0, 1] */
    uint64_t response_time_ms;          /**< Time to respond */
    void* response_data;                /**< Response payload (opaque) */
    size_t response_size;               /**< Size of response */
} delegation_result_t;

/**
 * @brief Domain coverage information
 *
 * WHAT: How well a domain is covered by the agent network
 * WHY:  Identify expertise gaps and coverage
 */
typedef struct {
    prime_signature_t domain_signature; /**< Domain being analyzed */
    float total_coverage;               /**< Combined coverage level [0, 1] */
    size_t expert_count;                /**< Number of agents with expertise */
    float avg_expertise;                /**< Average expertise level */
    float max_expertise;                /**< Highest expertise level */
    uint64_t top_expert;                /**< Best expert's agent ID */
    float redundancy;                   /**< Overlap between experts [0, 1] */
} domain_coverage_t;

/**
 * @brief Transactive memory system configuration
 */
typedef struct {
    size_t initial_agent_capacity;      /**< Initial agent registry size */
    size_t initial_domain_capacity;     /**< Initial domain taxonomy size */
    float min_expertise_threshold;      /**< Minimum expertise for consideration */
    float min_confidence_threshold;     /**< Minimum confidence required */
    float verification_decay_rate;      /**< How fast verification expires (per day) */
    float expertise_weight;             /**< Weight for expertise in scoring */
    float reliability_weight;           /**< Weight for reliability in scoring */
    float accessibility_weight;         /**< Weight for accessibility in scoring */
    float familiarity_weight;           /**< Weight for familiarity in scoring */
    uint64_t default_timeout_ms;        /**< Default delegation timeout */
    bool auto_verify;                   /**< Auto-verify expertise on successful delegation */
    bool track_history;                 /**< Track delegation history */
} transactive_config_t;

/**
 * @brief Statistics for transactive memory system
 */
typedef struct {
    size_t num_agents;                  /**< Number of registered agents */
    size_t num_domains;                 /**< Number of tracked domains */
    size_t num_expertise_entries;       /**< Total expertise entries */
    size_t active_delegations;          /**< Currently pending delegations */
    uint64_t total_lookups;             /**< Total directory lookups */
    uint64_t total_delegations;         /**< Total delegations made */
    uint64_t successful_delegations;    /**< Successful delegations */
    float avg_delegation_time_ms;       /**< Average delegation round-trip */
    float avg_success_rate;             /**< Average delegation success rate */
    float overall_coverage;             /**< System-wide domain coverage */
    size_t memory_bytes;                /**< Approximate memory usage */
} transactive_stats_t;

/**
 * @brief Opaque transactive memory system handle
 */
typedef struct transactive_memory_struct* transactive_memory_t;

/**
 * @brief Error codes for transactive memory operations
 */
typedef enum {
    TRANSACTIVE_SUCCESS = 0,            /**< Operation succeeded */
    TRANSACTIVE_ERROR_NULL = -1,        /**< NULL pointer argument */
    TRANSACTIVE_ERROR_NOT_FOUND = -2,   /**< Agent or domain not found */
    TRANSACTIVE_ERROR_EXISTS = -3,      /**< Agent already exists */
    TRANSACTIVE_ERROR_FULL = -4,        /**< Capacity exceeded */
    TRANSACTIVE_ERROR_INVALID = -5,     /**< Invalid parameter */
    TRANSACTIVE_ERROR_NO_MEMORY = -6,   /**< Memory allocation failed */
    TRANSACTIVE_ERROR_TIMEOUT = -7,     /**< Delegation timed out */
    TRANSACTIVE_ERROR_UNAVAILABLE = -8  /**< Agent unavailable */
} transactive_error_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default transactive memory configuration
 *
 * WHAT: Returns sensible default configuration
 * WHY:  Provides starting point for most use cases
 *
 * @return Default configuration with:
 *         - initial_agent_capacity: 64
 *         - initial_domain_capacity: 32
 *         - min_expertise_threshold: 0.1
 *         - min_confidence_threshold: 0.5
 *         - verification_decay_rate: 0.1 (per day)
 *         - expertise_weight: 0.4
 *         - reliability_weight: 0.3
 *         - accessibility_weight: 0.2
 *         - familiarity_weight: 0.1
 *         - default_timeout_ms: 5000
 *         - auto_verify: true
 *         - track_history: true
 *
 * Performance: ~5ns
 */
NIMCP_EXPORT transactive_config_t transactive_config_default(void);

/**
 * @brief Validate transactive memory configuration
 *
 * @param config Configuration to validate
 * @return true if valid, false otherwise
 *
 * Validation rules:
 * - All weights must be >= 0
 * - Weights should sum to approximately 1.0
 * - Thresholds must be in [0, 1]
 * - Capacities must be > 0
 */
NIMCP_EXPORT bool transactive_config_validate(const transactive_config_t* config);

//=============================================================================
// Lifecycle Management
//=============================================================================

/**
 * @brief Create a new transactive memory system
 *
 * WHAT: Allocates and initializes transactive memory system
 * WHY:  Enable distributed expertise tracking and delegation
 * HOW:  Creates agent registry, domain taxonomy, and delegation tracking
 *
 * @param config Configuration (NULL for defaults)
 * @return System handle or NULL on failure
 *
 * Performance: O(capacity) for initialization
 * Memory: ~10KB for default configuration
 *
 * Example:
 *   transactive_memory_t tm = transactive_create(NULL);
 *   if (!tm) {
 *       fprintf(stderr, "Failed: %s\n", transactive_get_last_error());
 *   }
 */
NIMCP_EXPORT transactive_memory_t transactive_create(const transactive_config_t* config);

/**
 * @brief Create transactive memory with PR integration
 *
 * WHAT: Creates system integrated with Prime Resonant components
 * WHY:  Full integration with PR memory architecture
 *
 * @param config Configuration
 * @param entanglement Entanglement graph for connections
 * @param node_manager PR node manager for storage
 * @return System handle or NULL on failure
 */
NIMCP_EXPORT transactive_memory_t transactive_create_with_pr(
    const transactive_config_t* config,
    entangle_graph_t entanglement,
    pr_node_manager_t node_manager
);

/**
 * @brief Destroy transactive memory system
 *
 * WHAT: Releases all resources
 * WHY:  Cleanup
 *
 * @param tm System to destroy (NULL safe)
 *
 * NOTE: Does not destroy connected entanglement graph or node manager
 */
NIMCP_EXPORT void transactive_destroy(transactive_memory_t tm);

//=============================================================================
// Agent Registration
//=============================================================================

/**
 * @brief Register a new agent in the system
 *
 * WHAT: Adds agent to the transactive memory network
 * WHY:  Agents must be registered before expertise can be tracked
 * HOW:  Creates agent entry with initial metadata
 *
 * @param tm Transactive memory system
 * @param agent_id Unique agent identifier
 * @param agent_name Human-readable name (can be NULL)
 * @return TRANSACTIVE_SUCCESS or error code
 *
 * Performance: O(1) average
 *
 * Example:
 *   int err = transactive_register_agent(tm, 42, "AliceAgent");
 *   if (err == TRANSACTIVE_ERROR_EXISTS) {
 *       // Agent already registered
 *   }
 */
NIMCP_EXPORT transactive_error_t transactive_register_agent(
    transactive_memory_t tm,
    uint64_t agent_id,
    const char* agent_name
);

/**
 * @brief Register agent with initial expertise
 *
 * @param tm Transactive memory system
 * @param agent_id Unique agent identifier
 * @param agent_name Human-readable name
 * @param expertise Initial expertise entries
 * @param num_expertise Number of expertise entries
 * @return TRANSACTIVE_SUCCESS or error code
 */
NIMCP_EXPORT transactive_error_t transactive_register_agent_with_expertise(
    transactive_memory_t tm,
    uint64_t agent_id,
    const char* agent_name,
    const expertise_entry_t* expertise,
    size_t num_expertise
);

/**
 * @brief Unregister an agent
 *
 * WHAT: Removes agent from the system
 * WHY:  Agent leaving or being removed
 *
 * @param tm Transactive memory system
 * @param agent_id Agent to remove
 * @return TRANSACTIVE_SUCCESS or error code
 *
 * NOTE: Pending delegations to this agent will timeout
 */
NIMCP_EXPORT transactive_error_t transactive_unregister_agent(
    transactive_memory_t tm,
    uint64_t agent_id
);

/**
 * @brief Get agent information
 *
 * @param tm Transactive memory system
 * @param agent_id Agent to query
 * @param agent Output agent information (caller-allocated)
 * @return TRANSACTIVE_SUCCESS or error code
 */
NIMCP_EXPORT transactive_error_t transactive_get_agent(
    transactive_memory_t tm,
    uint64_t agent_id,
    transactive_agent_t* agent
);

/**
 * @brief Update agent metadata
 *
 * @param tm Transactive memory system
 * @param agent_id Agent to update
 * @param reliability New reliability score (-1 to keep current)
 * @param accessibility New accessibility (-1 to keep current)
 * @param is_active New active status
 * @return TRANSACTIVE_SUCCESS or error code
 */
NIMCP_EXPORT transactive_error_t transactive_update_agent(
    transactive_memory_t tm,
    uint64_t agent_id,
    float reliability,
    float accessibility,
    bool is_active
);

/**
 * @brief Check if agent exists
 *
 * @param tm Transactive memory system
 * @param agent_id Agent ID to check
 * @return true if agent is registered
 */
NIMCP_EXPORT bool transactive_agent_exists(
    transactive_memory_t tm,
    uint64_t agent_id
);

/**
 * @brief Get all registered agent IDs
 *
 * @param tm Transactive memory system
 * @param agents Output array of agent IDs
 * @param max_agents Maximum to return
 * @param count Output: actual count
 * @return TRANSACTIVE_SUCCESS or error code
 */
NIMCP_EXPORT transactive_error_t transactive_get_all_agents(
    transactive_memory_t tm,
    uint64_t* agents,
    size_t max_agents,
    size_t* count
);

//=============================================================================
// Expertise Management
//=============================================================================

/**
 * @brief Add or update expertise for an agent
 *
 * WHAT: Records that agent has expertise in a domain
 * WHY:  Build the "who knows what" directory
 * HOW:  Stores domain signature with expertise level
 *
 * @param tm Transactive memory system
 * @param agent_id Agent gaining/updating expertise
 * @param domain_signature Prime signature of knowledge domain
 * @param expertise_level Level of expertise [0, 1]
 * @param confidence Confidence in this assessment [0, 1]
 * @return TRANSACTIVE_SUCCESS or error code
 *
 * Performance: O(K) where K = agent's current expertise count
 *
 * Example:
 *   // Agent 42 knows about "databases"
 *   prime_signature_t* db_sig = prime_sig_from_text("databases SQL query optimization");
 *   transactive_update_expertise(tm, 42, db_sig, 0.9f, 0.8f);
 */
NIMCP_EXPORT transactive_error_t transactive_update_expertise(
    transactive_memory_t tm,
    uint64_t agent_id,
    const prime_signature_t* domain_signature,
    float expertise_level,
    float confidence
);

/**
 * @brief Remove expertise from an agent
 *
 * @param tm Transactive memory system
 * @param agent_id Agent ID
 * @param domain_signature Domain to remove
 * @return TRANSACTIVE_SUCCESS or error code
 */
NIMCP_EXPORT transactive_error_t transactive_remove_expertise(
    transactive_memory_t tm,
    uint64_t agent_id,
    const prime_signature_t* domain_signature
);

/**
 * @brief Get agent's expertise in a specific domain
 *
 * @param tm Transactive memory system
 * @param agent_id Agent to query
 * @param domain_signature Domain to check
 * @param entry Output expertise entry (caller-allocated)
 * @return TRANSACTIVE_SUCCESS or TRANSACTIVE_ERROR_NOT_FOUND
 */
NIMCP_EXPORT transactive_error_t transactive_get_expertise(
    transactive_memory_t tm,
    uint64_t agent_id,
    const prime_signature_t* domain_signature,
    expertise_entry_t* entry
);

/**
 * @brief Get all expertise for an agent
 *
 * @param tm Transactive memory system
 * @param agent_id Agent to query
 * @param entries Output array
 * @param max_entries Maximum to return
 * @param count Output: actual count
 * @return TRANSACTIVE_SUCCESS or error code
 */
NIMCP_EXPORT transactive_error_t transactive_get_agent_expertise(
    transactive_memory_t tm,
    uint64_t agent_id,
    expertise_entry_t* entries,
    size_t max_entries,
    size_t* count
);

/**
 * @brief Verify an agent's claimed expertise
 *
 * WHAT: Mark expertise as verified (or not) based on evidence
 * WHY:  Build confidence in expertise claims over time
 * HOW:  Updates verification timestamp and confidence
 *
 * @param tm Transactive memory system
 * @param agent_id Agent whose expertise is being verified
 * @param domain_signature Domain being verified
 * @param verification_success Whether verification succeeded
 * @param new_confidence New confidence level [0, 1] or -1 to auto-calculate
 * @return TRANSACTIVE_SUCCESS or error code
 *
 * Example:
 *   // Agent successfully answered database query
 *   transactive_verify_expertise(tm, 42, &db_sig, true, -1);
 */
NIMCP_EXPORT transactive_error_t transactive_verify_expertise(
    transactive_memory_t tm,
    uint64_t agent_id,
    const prime_signature_t* domain_signature,
    bool verification_success,
    float new_confidence
);

//=============================================================================
// Directory Lookup
//=============================================================================

/**
 * @brief Look up who knows about a topic
 *
 * WHAT: Find agents with expertise in a knowledge domain
 * WHY:  Core transactive memory function - "who should I ask?"
 * HOW:  Searches expertise directory by signature similarity
 *
 * @param tm Transactive memory system
 * @param query_signature What knowledge is needed
 * @param result Output directory result (caller-allocated)
 * @return TRANSACTIVE_SUCCESS or error code
 *
 * Performance: O(A * K) where A = agents, K = avg expertise per agent
 *
 * Example:
 *   directory_result_t result;
 *   transactive_lookup(tm, &query_sig, &result);
 *   if (result.num_recommendations > 0) {
 *       printf("Best expert: %lu (score: %.2f)\n",
 *              result.recommended_agents[0], result.agent_scores[0]);
 *   }
 *
 * NOTE: Caller must free result.recommended_agents and result.agent_scores
 */
NIMCP_EXPORT transactive_error_t transactive_lookup(
    transactive_memory_t tm,
    const prime_signature_t* query_signature,
    directory_result_t* result
);

/**
 * @brief Look up with additional constraints
 *
 * @param tm Transactive memory system
 * @param query_signature What knowledge is needed
 * @param min_expertise Minimum expertise level required
 * @param min_reliability Minimum reliability required
 * @param exclude_agents Agents to exclude (can be NULL)
 * @param num_exclude Number of excluded agents
 * @param result Output directory result
 * @return TRANSACTIVE_SUCCESS or error code
 */
NIMCP_EXPORT transactive_error_t transactive_lookup_constrained(
    transactive_memory_t tm,
    const prime_signature_t* query_signature,
    float min_expertise,
    float min_reliability,
    const uint64_t* exclude_agents,
    size_t num_exclude,
    directory_result_t* result
);

/**
 * @brief Get top K experts in a domain
 *
 * WHAT: Find the K best experts for a knowledge area
 * WHY:  Often want to consider multiple experts
 *
 * @param tm Transactive memory system
 * @param domain_signature Knowledge domain
 * @param k Number of experts to return
 * @param agent_ids Output array of agent IDs (size >= k)
 * @param scores Output array of scores (size >= k, can be NULL)
 * @param count Output: actual count returned (<= k)
 * @return TRANSACTIVE_SUCCESS or error code
 */
NIMCP_EXPORT transactive_error_t transactive_get_top_experts(
    transactive_memory_t tm,
    const prime_signature_t* domain_signature,
    size_t k,
    uint64_t* agent_ids,
    float* scores,
    size_t* count
);

/**
 * @brief Free directory result resources
 *
 * @param result Directory result to free (NULL safe)
 */
NIMCP_EXPORT void transactive_free_directory_result(directory_result_t* result);

//=============================================================================
// Delegation
//=============================================================================

/**
 * @brief Delegate a query to an expert
 *
 * WHAT: Outsource memory retrieval to a domain expert
 * WHY:  Leverage distributed expertise
 * HOW:  Selects expert and tracks delegation state
 *
 * @param tm Transactive memory system
 * @param query_signature What knowledge is needed
 * @param target_agent Agent to delegate to (0 = auto-select)
 * @param timeout_ms Timeout in milliseconds (0 = use default)
 * @param delegation_id Output: delegation tracking ID
 * @return TRANSACTIVE_SUCCESS or error code
 *
 * Performance: O(lookup) if auto-select, O(1) if target specified
 *
 * Example:
 *   uint64_t delegation_id;
 *   transactive_delegate(tm, &query_sig, 0, 5000, &delegation_id);
 *   // ... later ...
 *   delegation_result_t result;
 *   transactive_receive_answer(tm, delegation_id, &result);
 */
NIMCP_EXPORT transactive_error_t transactive_delegate(
    transactive_memory_t tm,
    const prime_signature_t* query_signature,
    uint64_t target_agent,
    uint64_t timeout_ms,
    uint64_t* delegation_id
);

/**
 * @brief Process received answer from delegation
 *
 * WHAT: Handle response from delegated query
 * WHY:  Complete delegation cycle and update expertise model
 *
 * @param tm Transactive memory system
 * @param delegation_id Delegation identifier
 * @param result Delegation result
 * @return TRANSACTIVE_SUCCESS or error code
 *
 * Side effects:
 * - Updates agent's success rate
 * - May verify expertise (if auto_verify enabled)
 * - Updates response time statistics
 */
NIMCP_EXPORT transactive_error_t transactive_receive_answer(
    transactive_memory_t tm,
    uint64_t delegation_id,
    const delegation_result_t* result
);

/**
 * @brief Cancel a pending delegation
 *
 * @param tm Transactive memory system
 * @param delegation_id Delegation to cancel
 * @return TRANSACTIVE_SUCCESS or error code
 */
NIMCP_EXPORT transactive_error_t transactive_cancel_delegation(
    transactive_memory_t tm,
    uint64_t delegation_id
);

/**
 * @brief Get delegation state
 *
 * @param tm Transactive memory system
 * @param delegation_id Delegation identifier
 * @param state Output delegation state
 * @return TRANSACTIVE_SUCCESS or error code
 */
NIMCP_EXPORT transactive_error_t transactive_get_delegation_state(
    transactive_memory_t tm,
    uint64_t delegation_id,
    delegation_state_t* state
);

/**
 * @brief Check for timed-out delegations
 *
 * WHAT: Identifies and handles expired delegations
 * WHY:  Clean up stale delegations
 *
 * @param tm Transactive memory system
 * @param current_time_ms Current timestamp
 * @param timed_out Output array of timed-out delegation IDs
 * @param max_results Maximum to return
 * @param count Output: actual count
 * @return TRANSACTIVE_SUCCESS or error code
 */
NIMCP_EXPORT transactive_error_t transactive_check_timeouts(
    transactive_memory_t tm,
    uint64_t current_time_ms,
    uint64_t* timed_out,
    size_t max_results,
    size_t* count
);

//=============================================================================
// Prediction and Analysis
//=============================================================================

/**
 * @brief Predict success probability of a delegation
 *
 * WHAT: Estimate likelihood that delegation will succeed
 * WHY:  Make informed delegation decisions
 * HOW:  Combines expertise level, agent reliability, historical success
 *
 * @param tm Transactive memory system
 * @param query_signature Query being delegated
 * @param target_agent Agent being considered
 * @return Predicted success probability [0, 1], or -1 on error
 *
 * Formula:
 *   P = expertise * confidence * reliability * (1 + log(success_rate+1))
 */
NIMCP_EXPORT float transactive_predict_retrieval_success(
    transactive_memory_t tm,
    const prime_signature_t* query_signature,
    uint64_t target_agent
);

/**
 * @brief Compute domain coverage statistics
 *
 * WHAT: Analyze how well a domain is covered by experts
 * WHY:  Identify expertise gaps and redundancy
 *
 * @param tm Transactive memory system
 * @param domain_signature Domain to analyze
 * @param coverage Output coverage information
 * @return TRANSACTIVE_SUCCESS or error code
 */
NIMCP_EXPORT transactive_error_t transactive_compute_domain_coverage(
    transactive_memory_t tm,
    const prime_signature_t* domain_signature,
    domain_coverage_t* coverage
);

/**
 * @brief Compute overall system coverage
 *
 * @param tm Transactive memory system
 * @return Overall coverage score [0, 1]
 */
NIMCP_EXPORT float transactive_compute_overall_coverage(transactive_memory_t tm);

/**
 * @brief Find domains with insufficient coverage
 *
 * WHAT: Identify knowledge gaps in the network
 * WHY:  Know where more expertise is needed
 *
 * @param tm Transactive memory system
 * @param min_coverage Minimum acceptable coverage
 * @param gaps Output array of under-covered domains
 * @param max_gaps Maximum to return
 * @param count Output: actual count
 * @return TRANSACTIVE_SUCCESS or error code
 */
NIMCP_EXPORT transactive_error_t transactive_find_coverage_gaps(
    transactive_memory_t tm,
    float min_coverage,
    domain_coverage_t* gaps,
    size_t max_gaps,
    size_t* count
);

//=============================================================================
// Learning and Adaptation
//=============================================================================

/**
 * @brief Update model from delegation interaction
 *
 * WHAT: Learn from delegation outcomes
 * WHY:  Improve expertise estimates and predictions over time
 * HOW:  Bayesian update of expertise and reliability estimates
 *
 * @param tm Transactive memory system
 * @param agent_id Agent that was delegated to
 * @param domain_signature Domain of the query
 * @param success Whether delegation succeeded
 * @param response_quality Quality score of response [0, 1]
 * @param response_time_ms How long the response took
 * @return TRANSACTIVE_SUCCESS or error code
 *
 * Updates:
 * - Agent reliability and success rate
 * - Domain expertise confidence
 * - Response time statistics
 * - Trust and familiarity
 */
NIMCP_EXPORT transactive_error_t transactive_update_from_interaction(
    transactive_memory_t tm,
    uint64_t agent_id,
    const prime_signature_t* domain_signature,
    bool success,
    float response_quality,
    uint64_t response_time_ms
);

/**
 * @brief Apply decay to expertise confidence
 *
 * WHAT: Reduce confidence in unverified expertise over time
 * WHY:  Expertise may become stale
 *
 * @param tm Transactive memory system
 * @param elapsed_seconds Time since last decay
 * @return Number of entries affected
 */
NIMCP_EXPORT size_t transactive_apply_decay(
    transactive_memory_t tm,
    float elapsed_seconds
);

/**
 * @brief Infer expertise from successful delegations
 *
 * WHAT: Automatically discover agent expertise through usage
 * WHY:  Agents may have expertise they haven't declared
 *
 * @param tm Transactive memory system
 * @param agent_id Agent to analyze
 * @param min_successes Minimum successful queries to infer expertise
 * @param inferred Output array of inferred expertise
 * @param max_inferred Maximum to return
 * @param count Output: actual count
 * @return TRANSACTIVE_SUCCESS or error code
 */
NIMCP_EXPORT transactive_error_t transactive_infer_expertise(
    transactive_memory_t tm,
    uint64_t agent_id,
    size_t min_successes,
    expertise_entry_t* inferred,
    size_t max_inferred,
    size_t* count
);

//=============================================================================
// Domain Management
//=============================================================================

/**
 * @brief Register a knowledge domain
 *
 * WHAT: Add a domain to the taxonomy
 * WHY:  Explicit domain registration enables better organization
 *
 * @param tm Transactive memory system
 * @param domain_signature Domain signature
 * @param domain_name Human-readable name (can be NULL)
 * @return TRANSACTIVE_SUCCESS or error code
 */
NIMCP_EXPORT transactive_error_t transactive_register_domain(
    transactive_memory_t tm,
    const prime_signature_t* domain_signature,
    const char* domain_name
);

/**
 * @brief Get all registered domains
 *
 * @param tm Transactive memory system
 * @param domains Output array of domain signatures
 * @param max_domains Maximum to return
 * @param count Output: actual count
 * @return TRANSACTIVE_SUCCESS or error code
 */
NIMCP_EXPORT transactive_error_t transactive_get_all_domains(
    transactive_memory_t tm,
    prime_signature_t* domains,
    size_t max_domains,
    size_t* count
);

/**
 * @brief Find similar domains
 *
 * @param tm Transactive memory system
 * @param query_signature Query domain
 * @param min_similarity Minimum Jaccard similarity
 * @param similar Output array of similar domains
 * @param max_similar Maximum to return
 * @param count Output: actual count
 * @return TRANSACTIVE_SUCCESS or error code
 */
NIMCP_EXPORT transactive_error_t transactive_find_similar_domains(
    transactive_memory_t tm,
    const prime_signature_t* query_signature,
    float min_similarity,
    prime_signature_t* similar,
    size_t max_similar,
    size_t* count
);

//=============================================================================
// Statistics and Debugging
//=============================================================================

/**
 * @brief Get system statistics
 *
 * @param tm Transactive memory system
 * @param stats Output statistics
 * @return TRANSACTIVE_SUCCESS or error code
 */
NIMCP_EXPORT transactive_error_t transactive_get_stats(
    transactive_memory_t tm,
    transactive_stats_t* stats
);

/**
 * @brief Reset statistics counters
 *
 * @param tm Transactive memory system
 */
NIMCP_EXPORT void transactive_reset_stats(transactive_memory_t tm);

/**
 * @brief Get last error message
 *
 * @return Error string or NULL if no error
 */
NIMCP_EXPORT const char* transactive_get_last_error(void);

/**
 * @brief Print system summary to stdout
 *
 * @param tm Transactive memory system
 */
NIMCP_EXPORT void transactive_print_summary(transactive_memory_t tm);

/**
 * @brief Print agent expertise to stdout
 *
 * @param tm Transactive memory system
 * @param agent_id Agent to print
 */
NIMCP_EXPORT void transactive_print_agent(
    transactive_memory_t tm,
    uint64_t agent_id
);

/**
 * @brief Validate system internal consistency
 *
 * @param tm Transactive memory system
 * @return true if consistent
 */
NIMCP_EXPORT bool transactive_validate(transactive_memory_t tm);

/**
 * @brief Get error string for error code
 *
 * @param error Error code
 * @return Human-readable error string
 */
NIMCP_EXPORT const char* transactive_error_string(transactive_error_t error);

/**
 * @brief Get current time in milliseconds
 *
 * @return Milliseconds since epoch
 */
NIMCP_EXPORT uint64_t transactive_current_time_ms(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_TRANSACTIVE_H
