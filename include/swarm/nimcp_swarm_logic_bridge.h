/**
 * @file nimcp_swarm_logic_bridge.h
 * @brief Logic-Swarm Bridge: Neural Logic Gates for Swarm Intelligence
 *
 * WHAT: Bridges neural logic gate system with swarm intelligence modules
 * WHY:  Enables swarm agents to use symbolic logic for consensus, validation, and inference
 * HOW:  Combines logic gates (AND/OR/NOT/XOR/IMPLIES) with swarm agent states
 *
 * ARCHITECTURE:
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                   Logic-Swarm Bridge                            │
 * ├─────────────────────────────────────────────────────────────────┤
 * │                                                                 │
 * │  Swarm Agent States  ──┐                                       │
 * │  (beliefs, votes)      │                                       │
 * │                        ├──►  Logic Gate Network                │
 * │  Consensus Rules  ─────┘     (AND/OR/NOT/XOR/IMPLIES)         │
 * │                                      │                         │
 * │                                      ▼                         │
 * │                               Validated Results                │
 * │                              (consensus, beliefs)              │
 * └─────────────────────────────────────────────────────────────────┘
 *
 * USE CASES:
 * 1. Consensus Validation: AND/OR voting across agents
 * 2. Threat Detection: IMPLIES rules for security
 * 3. Belief Validation: XOR for contradiction detection
 * 4. Multi-agent Reasoning: Distributed inference
 *
 * BIOLOGICAL INSPIRATION:
 * - Honeybee waggle dance consensus (AND gates for agreement)
 * - Ant pheromone trail logic (OR gates for path finding)
 * - Neural coincidence detection (temporal AND gates)
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 * @version 1.0.0
 */

#ifndef NIMCP_SWARM_LOGIC_BRIDGE_H
#define NIMCP_SWARM_LOGIC_BRIDGE_H

#include "utils/validation/nimcp_common.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "core/neuron_types/nimcp_neural_logic.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/platform/nimcp_platform.h"
#include "swarm/nimcp_swarm_consensus.h"
#include "swarm/nimcp_swarm_quorum.h"
#include "swarm/nimcp_swarm_emergence.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * FORWARD DECLARATIONS
 *============================================================================*/

typedef struct swarm_logic_bridge swarm_logic_bridge_t;

/*
 * NOTE: This header depends on types from:
 * - swarm/nimcp_swarm_consensus.h (swarm_vote_response_t)
 * - swarm/nimcp_swarm_quorum.h (nimcp_signal_molecule_t, nimcp_signal_type_t)
 * - swarm/nimcp_swarm_emergence.h (swarm_state_t, swarm_emergence_tier_t)
 *
 * Those headers must be included before this header to provide the necessary types.
 */

/*=============================================================================
 * CONFIGURATION STRUCTURES
 *============================================================================*/

/**
 * @brief Logic-Swarm Bridge configuration
 */
typedef struct {
    uint32_t max_rules;              /**< Maximum logic rules (default: 1000) */
    uint32_t rule_cache_size;        /**< Cached rule evaluations (default: 256) */
    bool enable_bio_async;           /**< Enable bio-async messaging (default: true) */
    float inference_threshold;       /**< Min confidence for inference (default: 0.7) */
    uint32_t max_agents;             /**< Maximum swarm agents (default: 100) */
    float timeout_ms;                /**< Rule evaluation timeout (default: 1000.0) */
} swarm_logic_bridge_config_t;

/**
 * @brief Logic rule for swarm validation
 */
typedef struct {
    uint32_t rule_id;                /**< Unique rule identifier */
    logic_gate_type_t gate_type;     /**< Logic gate type (AND/OR/NOT/XOR/IMPLIES) */
    uint32_t* input_agent_ids;       /**< Input agent IDs */
    uint32_t num_inputs;             /**< Number of input agents */
    float confidence_weight;         /**< Confidence weighting (0-1) */
    float threshold;                 /**< Activation threshold (0-1) */
    char description[128];           /**< Human-readable description */
} swarm_logic_rule_t;

/**
 * @brief Agent state for logic evaluation
 */
typedef struct {
    uint32_t agent_id;               /**< Agent identifier */
    float belief_value;              /**< Belief/vote value (0-1) */
    float confidence;                /**< Confidence in belief (0-1) */
    uint64_t timestamp_us;           /**< Timestamp (microseconds) */
    bool is_active;                  /**< Agent active flag */
} swarm_agent_state_t;

/**
 * @brief Logic evaluation result
 */
typedef struct {
    uint32_t rule_id;                /**< Rule that was evaluated */
    bool result;                     /**< Logical result (true/false) */
    float confidence;                /**< Result confidence (0-1) */
    uint32_t num_inputs_used;        /**< Number of inputs evaluated */
    uint64_t evaluation_time_us;     /**< Evaluation time (microseconds) */
    char explanation[256];           /**< Result explanation */
} swarm_logic_result_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_evaluations;      /**< Total rule evaluations */
    uint64_t successful_evaluations; /**< Successful evaluations */
    uint64_t failed_evaluations;     /**< Failed evaluations */
    uint64_t cache_hits;             /**< Cache hits */
    uint64_t cache_misses;           /**< Cache misses */
    float avg_evaluation_time_us;    /**< Average evaluation time */
    uint32_t active_rules;           /**< Currently active rules */
    uint32_t active_agents;          /**< Currently active agents */
    uint64_t byzantine_detections;   /**< Byzantine patterns detected */
    uint64_t consensus_validations;  /**< Consensus validations */
    uint64_t quorum_validations;     /**< Quorum validations */
    uint64_t tier_validations;       /**< Tier transition validations */
} swarm_logic_bridge_stats_t;

/**
 * @brief Byzantine detection result
 */
typedef struct {
    uint32_t agent_id;               /**< Agent identifier */
    float suspicion_score;           /**< Suspicion score [0-1] */
    uint32_t contradiction_count;    /**< Number of contradictions */
    char reason[128];                /**< Detection reason */
} byzantine_detection_t;

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *============================================================================*/

/**
 * @brief Get default bridge configuration
 *
 * WHAT: Provides default configuration for logic-swarm bridge
 * WHY:  Ensures safe defaults for all parameters
 * HOW:  Returns struct with sensible defaults
 *
 * @param config Configuration to fill with defaults
 */
void swarm_logic_bridge_get_default_config(swarm_logic_bridge_config_t* config);

/**
 * @brief Create logic-swarm bridge
 *
 * WHAT: Creates and initializes logic-swarm bridge
 * WHY:  Enables symbolic logic for swarm agents
 * HOW:  Allocates memory, initializes neural logic network, registers with bio-async
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
swarm_logic_bridge_t* swarm_logic_bridge_create(const swarm_logic_bridge_config_t* config);

/**
 * @brief Destroy logic-swarm bridge
 *
 * WHAT: Cleans up and destroys logic-swarm bridge
 * WHY:  Prevents memory leaks
 * HOW:  Unregisters from bio-async, frees memory
 *
 * @param bridge Bridge to destroy (NULL-safe)
 */
void swarm_logic_bridge_destroy(swarm_logic_bridge_t* bridge);

/*=============================================================================
 * RULE MANAGEMENT
 *============================================================================*/

/**
 * @brief Add logic rule
 *
 * WHAT: Adds new logic rule to bridge
 * WHY:  Enables custom validation logic
 * HOW:  Creates rule, validates parameters, adds to rule table
 *
 * @param bridge Bridge handle
 * @param rule Rule to add
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t swarm_logic_bridge_add_rule(swarm_logic_bridge_t* bridge,
                                           const swarm_logic_rule_t* rule);

/**
 * @brief Remove logic rule
 *
 * WHAT: Removes logic rule by ID
 * WHY:  Allows dynamic rule management
 * HOW:  Finds rule, removes from table, frees memory
 *
 * @param bridge Bridge handle
 * @param rule_id Rule ID to remove
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t swarm_logic_bridge_remove_rule(swarm_logic_bridge_t* bridge,
                                              uint32_t rule_id);

/**
 * @brief Get rule by ID
 *
 * WHAT: Retrieves logic rule by ID
 * WHY:  Allows rule inspection
 * HOW:  Searches rule table, returns pointer
 *
 * @param bridge Bridge handle
 * @param rule_id Rule ID to find
 * @return Rule pointer or NULL if not found
 */
const swarm_logic_rule_t* swarm_logic_bridge_get_rule(swarm_logic_bridge_t* bridge,
                                                       uint32_t rule_id);

/**
 * @brief Get all rules
 *
 * WHAT: Retrieves all active rules
 * WHY:  Enables rule enumeration
 * HOW:  Returns array of rule pointers
 *
 * @param bridge Bridge handle
 * @param rules Output array of rule pointers
 * @param max_rules Maximum rules to return
 * @return Number of rules returned
 */
uint32_t swarm_logic_bridge_get_all_rules(swarm_logic_bridge_t* bridge,
                                           const swarm_logic_rule_t** rules,
                                           uint32_t max_rules);

/*=============================================================================
 * EVALUATION FUNCTIONS
 *============================================================================*/

/**
 * @brief Evaluate logic rules over agent states
 *
 * WHAT: Evaluates all logic rules given current agent states
 * WHY:  Core function for swarm consensus and validation
 * HOW:  Iterates rules, evaluates logic gates, returns results
 *
 * @param bridge Bridge handle
 * @param agent_states Array of agent states
 * @param num_agents Number of agents
 * @param results Output array for results
 * @param max_results Maximum results to return
 * @return Number of results returned, -1 on error
 */
int swarm_logic_bridge_evaluate(swarm_logic_bridge_t* bridge,
                                 const swarm_agent_state_t* agent_states,
                                 uint32_t num_agents,
                                 swarm_logic_result_t* results,
                                 uint32_t max_results);

/**
 * @brief Evaluate single rule
 *
 * WHAT: Evaluates specific rule given agent states
 * WHY:  Allows targeted rule evaluation
 * HOW:  Finds rule, evaluates gate, returns result
 *
 * @param bridge Bridge handle
 * @param rule_id Rule to evaluate
 * @param agent_states Array of agent states
 * @param num_agents Number of agents
 * @param result Output result
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t swarm_logic_bridge_evaluate_rule(swarm_logic_bridge_t* bridge,
                                                uint32_t rule_id,
                                                const swarm_agent_state_t* agent_states,
                                                uint32_t num_agents,
                                                swarm_logic_result_t* result);

/*=============================================================================
 * CONSENSUS VALIDATION
 *============================================================================*/

/**
 * @brief Validate consensus across agents
 *
 * WHAT: Validates consensus using AND/OR voting
 * WHY:  Enables democratic decision making
 * HOW:  Applies logic gates to agent votes
 *
 * @param bridge Bridge handle
 * @param votes Array of agent votes (0-1)
 * @param num_votes Number of votes
 * @param consensus_type Gate type (LOGIC_GATE_AND for unanimous, LOGIC_GATE_OR for majority)
 * @param result Output consensus result
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t swarm_logic_bridge_validate_consensus(swarm_logic_bridge_t* bridge,
                                                     const float* votes,
                                                     uint32_t num_votes,
                                                     logic_gate_type_t consensus_type,
                                                     swarm_logic_result_t* result);

/**
 * @brief Detect belief contradictions
 *
 * WHAT: Detects contradictions in agent beliefs using XOR
 * WHY:  Ensures logical consistency
 * HOW:  Applies XOR gates to belief pairs
 *
 * @param bridge Bridge handle
 * @param beliefs Array of belief states
 * @param num_beliefs Number of beliefs
 * @param contradictions Output array of contradiction pairs
 * @param max_contradictions Maximum contradictions to return
 * @return Number of contradictions found, -1 on error
 */
int swarm_logic_bridge_detect_contradiction(swarm_logic_bridge_t* bridge,
                                            const swarm_agent_state_t* beliefs,
                                            uint32_t num_beliefs,
                                            uint32_t (*contradictions)[2],
                                            uint32_t max_contradictions);

/**
 * @brief Validate implication rule
 *
 * WHAT: Validates if A implies B for agent states
 * WHY:  Enables conditional reasoning
 * HOW:  Applies IMPLIES gate to agent pairs
 *
 * @param bridge Bridge handle
 * @param antecedent_agent Agent ID for antecedent (if A...)
 * @param consequent_agent Agent ID for consequent (then B...)
 * @param agent_states Array of agent states
 * @param num_agents Number of agents
 * @param result Output implication result
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t swarm_logic_bridge_validate_implication(swarm_logic_bridge_t* bridge,
                                                       uint32_t antecedent_agent,
                                                       uint32_t consequent_agent,
                                                       const swarm_agent_state_t* agent_states,
                                                       uint32_t num_agents,
                                                       swarm_logic_result_t* result);

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *============================================================================*/

/**
 * @brief Process bio-async inbox
 *
 * WHAT: Processes pending bio-async messages
 * WHY:  Enables asynchronous communication with other modules
 * HOW:  Calls bio_router_process_inbox with bridge context
 *
 * @param bridge Bridge handle
 * @return Number of messages processed, -1 on error
 */
int swarm_logic_bridge_process_inbox(swarm_logic_bridge_t* bridge);

/**
 * @brief Send logic evaluation result
 *
 * WHAT: Sends logic result to target module via bio-async
 * WHY:  Enables distributed logic evaluation
 * HOW:  Constructs message, sends via bio_router
 *
 * @param bridge Bridge handle
 * @param target_module Target module ID
 * @param result Result to send
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t swarm_logic_bridge_send_result(swarm_logic_bridge_t* bridge,
                                              bio_module_id_t target_module,
                                              const swarm_logic_result_t* result);

/**
 * @brief Broadcast consensus result
 *
 * WHAT: Broadcasts consensus to all swarm modules
 * WHY:  Enables swarm-wide synchronization
 * HOW:  Constructs broadcast message, sends via bio_router
 *
 * @param bridge Bridge handle
 * @param result Consensus result to broadcast
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t swarm_logic_bridge_broadcast_consensus(swarm_logic_bridge_t* bridge,
                                                      const swarm_logic_result_t* result);

/*=============================================================================
 * STATISTICS AND DEBUGGING
 *============================================================================*/

/**
 * @brief Get bridge statistics
 *
 * WHAT: Retrieves bridge performance statistics
 * WHY:  Enables monitoring and debugging
 * HOW:  Returns snapshot of internal counters
 *
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t swarm_logic_bridge_get_stats(swarm_logic_bridge_t* bridge,
                                            swarm_logic_bridge_stats_t* stats);

/**
 * @brief Reset statistics
 *
 * WHAT: Resets all statistics counters to zero
 * WHY:  Enables periodic monitoring
 * HOW:  Zeros all counters in stats structure
 *
 * @param bridge Bridge handle
 */
void swarm_logic_bridge_reset_stats(swarm_logic_bridge_t* bridge);

/**
 * @brief Clear evaluation cache
 *
 * WHAT: Clears cached evaluation results
 * WHY:  Forces re-evaluation of all rules
 * HOW:  Invalidates all cache entries
 *
 * @param bridge Bridge handle
 */
void swarm_logic_bridge_clear_cache(swarm_logic_bridge_t* bridge);

/*=============================================================================
 * ENHANCED CONSENSUS INTEGRATION
 *============================================================================*/

/**
 * @brief Validate consensus votes using logic gates
 *
 * WHAT: Validates consensus voting patterns with logic gate evaluation
 * WHY:  Detect invalid voting patterns and Byzantine behavior
 * HOW:  Apply AND/OR/XOR gates to vote responses, check for contradictions
 *
 * BIOLOGICAL BASIS: Honeybee consensus uses threshold-based decision making
 *
 * @param bridge Bridge handle
 * @param votes Array of vote responses from swarm_consensus
 * @param vote_count Number of votes
 * @param result Output validation result
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t swarm_logic_validate_consensus_votes(swarm_logic_bridge_t* bridge,
                                                     const swarm_vote_response_t* votes,
                                                     uint32_t vote_count,
                                                     swarm_logic_result_t* result);

/**
 * @brief Detect Byzantine patterns in votes
 *
 * WHAT: Identifies contradictory or malicious voting behavior
 * WHY:  Protect against faulty or adversarial agents
 * HOW:  Check for same agent voting multiple times with conflicting choices
 *
 * BIOLOGICAL BASIS: Honeybees reject scouts providing contradictory information
 *
 * @param bridge Bridge handle
 * @param votes Array of vote responses
 * @param vote_count Number of votes
 * @param byzantine_agents Output array of detected Byzantine agents
 * @param byzantine_count Output count of Byzantine agents detected
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t swarm_logic_detect_byzantine_pattern(swarm_logic_bridge_t* bridge,
                                                     const swarm_vote_response_t* votes,
                                                     uint32_t vote_count,
                                                     byzantine_detection_t* byzantine_agents,
                                                     uint32_t* byzantine_count);

/*=============================================================================
 * ENHANCED QUORUM INTEGRATION
 *============================================================================*/

/**
 * @brief Validate quorum signals using logic gates
 *
 * WHAT: Validates quorum signal patterns for logical consistency
 * WHY:  Ensure signals are mutually compatible and meet prerequisites
 * HOW:  Apply AND/OR/NOT/XOR gates to signal concentrations
 *
 * BIOLOGICAL BASIS: Bacterial quorum sensing has signal cross-inhibition
 *
 * @param bridge Bridge handle
 * @param signals Array of signal molecules from quorum system
 * @param signal_count Number of signals
 * @param result Output validation result
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t swarm_logic_validate_quorum_signals(swarm_logic_bridge_t* bridge,
                                                    const nimcp_signal_molecule_t* signals,
                                                    uint32_t signal_count,
                                                    swarm_logic_result_t* result);

/**
 * @brief Check signal exclusion rules
 *
 * WHAT: Determines if two signals are mutually exclusive
 * WHY:  Prevent logically contradictory decisions (e.g. ATTACK and RETREAT)
 * HOW:  XOR logic: signals should not both be high simultaneously
 *
 * BIOLOGICAL BASIS: Ant colonies use pheromone inhibition between trails
 *
 * @param bridge Bridge handle
 * @param signal_a First signal type
 * @param signal_b Second signal type
 * @param mutually_exclusive Output: true if signals are exclusive
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t swarm_logic_check_signal_exclusion(swarm_logic_bridge_t* bridge,
                                                   nimcp_signal_type_t signal_a,
                                                   nimcp_signal_type_t signal_b,
                                                   bool* mutually_exclusive);

/*=============================================================================
 * ENHANCED EMERGENCE INTEGRATION
 *============================================================================*/

/**
 * @brief Validate tier transition using logic gates
 *
 * WHAT: Validates emergence tier transitions meet all prerequisites
 * WHY:  Prevent invalid tier jumps that skip required capabilities
 * HOW:  AND gate: all requirements (drone count, coherence, health) must pass
 *
 * BIOLOGICAL BASIS: Insect colony task allocation has threshold-based transitions
 *
 * @param bridge Bridge handle
 * @param current_tier Current emergence tier
 * @param target_tier Desired emergence tier
 * @param state Current swarm state
 * @param valid Output: true if transition is valid
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t swarm_logic_validate_tier_transition(swarm_logic_bridge_t* bridge,
                                                     swarm_emergence_tier_t current_tier,
                                                     swarm_emergence_tier_t target_tier,
                                                     const swarm_state_t* state,
                                                     bool* valid);

/*=============================================================================
 * BRAIN INTEGRATION
 *============================================================================*/

/**
 * @brief Connect bridge to brain for neuromodulation
 *
 * WHAT: Connects logic bridge to brain for dopamine/ACh modulation
 * WHY:  Enable neuromodulator influence on logic thresholds
 * HOW:  Stores brain pointer, enables modulation in evaluations
 *
 * BIOLOGICAL BASIS: Dopamine modulates decision thresholds in basal ganglia
 *
 * @param bridge Bridge handle
 * @param brain Brain handle
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t swarm_logic_connect_brain(swarm_logic_bridge_t* bridge, void* brain);

/**
 * @brief Connect bridge to immune system
 *
 * WHAT: Connects logic bridge to brain immune system
 * WHY:  Modulate logic evaluation based on inflammation state
 * HOW:  Stores immune pointer, queries inflammation during evaluation
 *
 * BIOLOGICAL BASIS: Inflammation impairs cognitive processing
 *
 * @param bridge Bridge handle
 * @param immune_system Brain immune system handle
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t swarm_logic_connect_immune(swarm_logic_bridge_t* bridge, void* immune_system);

/**
 * @brief Connect bridge to UMM
 *
 * WHAT: Connects logic bridge to Universal Memory Manager
 * WHY:  Track memory allocations and enable rule caching in UMM
 * HOW:  Stores UMM pointer, uses UMM for allocations
 *
 * @param bridge Bridge handle
 * @param umm UMM handle
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t swarm_logic_connect_umm(swarm_logic_bridge_t* bridge, void* umm);

/**
 * @brief Evaluate rule with neuromodulation
 *
 * WHAT: Evaluates logic rule with dopamine/acetylcholine modulation
 * WHY:  Model neuromodulator effects on decision making
 * HOW:  Adjust thresholds based on DA (lowers thresholds) and ACh (increases precision)
 *
 * BIOLOGICAL BASIS: DA reduces response thresholds, ACh increases attentional precision
 *
 * @param bridge Bridge handle
 * @param rule_id Rule to evaluate
 * @param dopamine_level Dopamine concentration [0-1]
 * @param acetylcholine_level Acetylcholine concentration [0-1]
 * @param result Output result
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t swarm_logic_evaluate_with_modulation(swarm_logic_bridge_t* bridge,
                                                     uint32_t rule_id,
                                                     float dopamine_level,
                                                     float acetylcholine_level,
                                                     swarm_logic_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SWARM_LOGIC_BRIDGE_H */
