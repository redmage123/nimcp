//=============================================================================
// nimcp_ethics_internal.h - Internal Ethics Engine Structures and Functions
//=============================================================================
// RESPONSIBILITY: Internal interfaces between ethics modules
//
// This header defines internal structures and functions shared between
// the ethics engine modules. It should NOT be included by external code.
//=============================================================================

#ifndef NIMCP_ETHICS_INTERNAL_H
#define NIMCP_ETHICS_INTERNAL_H

#include "cognitive/ethics/nimcp_ethics.h"
#include "cognitive/ethics/nimcp_ethics_snn_bridge.h"
#include "cognitive/ethics/nimcp_ethics_plasticity_bridge.h"
#include "utils/containers/nimcp_hash_table.h"
#include "utils/containers/nimcp_btree.h"
#include "utils/thread/nimcp_thread.h"
#include "core/brain/nimcp_brain.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "utils/memory/nimcp_unified_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define HASH_TABLE_SIZE 256
#define OBJECT_POOL_SIZE 32
#define MAX_FEATURE_SIZE 20
#define GOLDEN_RULE_WEIGHT 0.7f
#define POLICY_WEIGHT 0.3f

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Function pointer type for policy evaluation strategies
 */
typedef float (*policy_evaluator_fn)(const action_context_t* action);

/**
 * @brief Strategy table mapping violation types to evaluation functions
 */
typedef struct {
    policy_evaluator_fn evaluators[16];  // Max violation types
} policy_strategy_table_t;

/**
 * @brief Object pool for feature buffers to avoid repeated heap allocations
 */
typedef struct {
    float buffers[OBJECT_POOL_SIZE][MAX_FEATURE_SIZE];
    bool in_use[OBJECT_POOL_SIZE];
    uint32_t next_available;
} feature_buffer_pool_t;

/**
 * @brief Incident logging storage
 */
typedef struct {
    ethics_incident_t* incident_history;  // Circular buffer
    uint32_t incident_count;              // Total incidents logged
    uint32_t incident_index;              // Current index in circular buffer
    btree_t* incident_btree;              // B-tree indexed by timestamp
    hash_table_t* incident_by_type;       // Hash table indexed by violation type
    uint64_t next_incident_id;            // Auto-incrementing ID
    nimcp_mutex_t incident_mutex;         // Thread safety
} ethics_incident_storage_t;

/**
 * @brief Ethics engine internal structure
 */
struct ethics_engine_struct {
    // Neural networks
    brain_t golden_rule_evaluator;
    empathy_network_t empathy_net;

    // Repository Pattern: Hash-indexed policy storage
    hash_table_t* policy_table;

    // Object Pool: Reusable feature buffers
    feature_buffer_pool_t buffer_pool;

    // Strategy Pattern: Policy evaluation function table
    policy_strategy_table_t strategy_table;

    // Legacy array storage (for iteration)
    ethics_policy_t* policies;
    uint32_t num_policies;
    uint32_t policies_capacity;

    // Violation history (legacy)
    violation_record_t* violations;
    uint32_t num_violations;
    uint32_t violations_capacity;

    // Incident logging (NIMCP 2.5.1)
    ethics_incident_storage_t incident_storage;

    // Configuration
    float golden_rule_threshold;
    float empathy_weight;
    bool enable_learning;

    // Statistics
    uint64_t total_evaluations;
    uint64_t violations_detected;
    uint64_t actions_blocked;

    // Asimov's Laws (NIMCP 2.5.2)
    asimov_config_t asimov_config;          // Configuration for laws
    bool asimov_laws_locked;                // Whether laws are mprotect'd
    uint8_t asimov_laws_hash[32];           // SHA-256 hash for integrity
    uint64_t asimov_violations;             // Count of Asimov violations

    // Laws of War (NIMCP 2.6)
    laws_of_war_config_t laws_of_war_config; // Laws of War configuration
    uint64_t laws_of_war_evaluations;        // Total evaluations performed
    uint64_t laws_of_war_violations;         // Total violations detected
    uint64_t mercy_actions_taken;            // Mercy actions performed

    // Psychological Stability (NIMCP 2.6.2)
    psychological_config_t psych_config;     // Psychological stability config
    psychological_state_t current_psych_state; // Current psychological state
    float accumulated_stress;                // Accumulated stress level
    float accumulated_guilt;                 // Accumulated guilt level
    uint64_t last_reflection_time;           // Last reflection timestamp
    uint32_t actions_since_processing;       // Actions since last processing
    uint32_t justified_actions_count;        // Count of justified actions
    uint32_t unjustified_actions_count;      // Count of unjustified actions

    // BIO-ASYNC INTEGRATION
    bio_module_context_t bio_ctx;           // Bio-async module context
    bool bio_async_enabled;                 // Bio-async registration status
    unified_mem_manager_t mem_mgr;          // Unified memory manager

    // BRAIN IMMUNE SYSTEM INTEGRATION (Phase 12.x)
    brain_immune_system_t* immune_system;   // Brain immune system reference
    bool immune_integration_enabled;        // Immune integration active
    float last_inflammation_level;          // Cached inflammation level
    uint64_t last_immune_check_ms;          // Last immune state check time
    uint32_t immune_violations_triggered;   // Violations that triggered immune response

    // SNN and Plasticity bridges
    ethics_snn_bridge_t* snn_bridge;
    ethics_plasticity_bridge_t* plasticity_bridge;
    bool bridges_enabled;

    /* W11: optional back-reference to the parent brain for KG event emission.
     * Set via ethics_engine_set_brain() at brain init time. NULL-safe;
     * leaving it unset disables W11 KG emission (the bio-async broadcast and
     * tamper-resistant audit log continue to work as before). */
    brain_t host_brain;  /* W11 KG emission back-ref */
};

/**
 * @brief Empathy network internal structure
 */
struct empathy_network_struct {
    brain_t perspective_network;
    empathy_state_t* states;
    uint32_t num_agents;
    float* emotional_states;
    uint32_t num_emotions;

    // Phase 10.3: Reference to main brain for emotional working memory access
    brain_t main_brain;  /**< Main brain instance for working memory access */
};

//=============================================================================
// Ethics Engine Accessor Functions (for internal modules)
//=============================================================================

// Network accessors
brain_t ethics_engine_get_golden_rule_net(ethics_engine_t engine);
empathy_network_t ethics_engine_get_empathy_net(ethics_engine_t engine);

// Configuration accessors
asimov_config_t* ethics_engine_get_asimov_config(ethics_engine_t engine);
float ethics_engine_get_threshold(ethics_engine_t engine);
bool ethics_engine_is_learning_enabled(ethics_engine_t engine);

// Policy accessors
uint32_t ethics_engine_get_num_policies(ethics_engine_t engine);
const ethics_policy_t* ethics_engine_get_policy(ethics_engine_t engine, uint32_t index);
const policy_strategy_table_t* ethics_engine_get_strategy_table(ethics_engine_t engine);
bool ethics_engine_add_policy_internal(ethics_engine_t engine, const ethics_policy_t* policy);
bool ethics_engine_remove_policy_internal(ethics_engine_t engine, uint32_t policy_id);

// Incident storage accessors
ethics_incident_storage_t* ethics_engine_get_incident_storage(ethics_engine_t engine);

// Buffer pool accessors
float* ethics_engine_acquire_buffer(ethics_engine_t engine);
void ethics_engine_release_buffer(ethics_engine_t engine, float* buffer);

// Statistics accessors
void ethics_engine_increment_violations_detected(ethics_engine_t engine);
void ethics_engine_increment_asimov_violations(ethics_engine_t engine);

// Asimov accessors
bool ethics_engine_is_asimov_locked(ethics_engine_t engine);
void ethics_engine_set_asimov_locked(ethics_engine_t engine, bool locked);
const uint8_t* ethics_engine_get_asimov_hash(ethics_engine_t engine);
void ethics_engine_set_asimov_hash(ethics_engine_t engine, const uint8_t* hash);

//=============================================================================
// Evaluation Module Functions (nimcp_ethics_evaluation.c)
//=============================================================================

// Golden Rule evaluation
float ethics_evaluate_golden_rule(ethics_engine_t engine, const action_context_t* action);
float ethics_calculate_perspective_score(const empathy_state_extended_t* state);
float ethics_simulate_agent_perspective(empathy_network_t network, agent_id_t agent,
                                        const action_context_t* action);

// Helper functions
void ethics_copy_action_features(float* dest, const action_context_t* action, uint32_t max_features);
void ethics_copy_emotional_state(float* dest, const float* emotional_states, agent_id_t agent,
                                 uint32_t num_emotions, uint32_t offset);

//=============================================================================
// Asimov Module Functions (nimcp_ethics_asimov.c)
//=============================================================================

void ethics_compute_asimov_hash(uint8_t* hash_out);

//=============================================================================
// Policy Module Functions (nimcp_ethics_policies.c)
//=============================================================================

// Strategy table
void ethics_init_strategy_table(policy_strategy_table_t* table);
float ethics_evaluate_policy_strategy(const policy_strategy_table_t* table,
                                      const ethics_policy_t* policy, const action_context_t* action);

// Individual policy evaluators
float ethics_evaluate_harm_policy(const action_context_t* action);
float ethics_evaluate_unfairness_policy(const action_context_t* action);
float ethics_evaluate_deception_policy(const action_context_t* action);
float ethics_evaluate_autonomy_policy(const action_context_t* action);
float ethics_evaluate_privacy_policy(const action_context_t* action);
float ethics_evaluate_consent_policy(const action_context_t* action);

// Policy evaluation
float ethics_evaluate_all_policies(ethics_engine_t engine, const action_context_t* action,
                                   ethics_violation_type_t* worst_violation, float* worst_severity);

//=============================================================================
// Incident Module Functions (nimcp_ethics_incidents.c)
//=============================================================================

bool ethics_init_incident_logging(ethics_engine_t engine);
void ethics_cleanup_incident_logging(ethics_engine_t engine);

//=============================================================================
// Learning Module Functions (nimcp_ethics_learning.c)
//=============================================================================

bool ethics_validate_learning_inputs(ethics_engine_t engine, const action_context_t* action,
                                     const action_outcome_t* outcome);
void ethics_update_golden_rule_learning(ethics_engine_t engine, const action_context_t* action,
                                        float actual_impact);
void ethics_update_empathy_learning(ethics_engine_t engine, const action_context_t* action,
                                    const action_outcome_t* outcome);

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_ETHICS_INTERNAL_H
