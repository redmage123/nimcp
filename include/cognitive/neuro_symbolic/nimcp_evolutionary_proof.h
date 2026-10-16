/**
 * @file nimcp_evolutionary_proof.h
 * @brief Evolutionary Proof Search - GA+RL Hybrid Theorem Prover
 *
 * Implements a hybrid genetic algorithm and reinforcement learning
 * approach to automated theorem proving. Proof strategies evolve
 * over time, adapting to different problem domains.
 *
 * Key Features:
 * - Genetic algorithm for strategy evolution
 * - Q-learning for action selection
 * - Experience replay for stable learning
 * - Transfer learning between domains
 * - Multi-objective optimization (speed vs elegance)
 *
 * Inspired by:
 * - nimcp_recovery_evolution.h patterns
 * - Mathesis paper's proof search approach
 * - AlphaProof-style learned proving
 *
 * Biological Basis:
 * - Prefrontal cortex for goal-directed planning
 * - Basal ganglia for action selection
 * - Hippocampus for experience storage
 *
 * @author NIMCP Team
 * @version 2.6.3
 */

#ifndef NIMCP_EVOLUTIONARY_PROOF_H
#define NIMCP_EVOLUTIONARY_PROOF_H

#include <stdbool.h>
#include <stdint.h>
#include "utils/platform/nimcp_platform.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Module Constants
 * ============================================================================ */

/** Bio-async module identifier */
#define BIO_MODULE_EVOLUTIONARY_PROOF   0x0397

/** Maximum population size */
#define EVOPROOF_MAX_POPULATION         64

/** Maximum genes per strategy */
#define EVOPROOF_MAX_GENES              32

/** Maximum actions in a sequence */
#define EVOPROOF_MAX_ACTIONS            64

/** Maximum Q-table states */
#define EVOPROOF_MAX_STATES             1024

/** Maximum experience replay buffer */
#define EVOPROOF_MAX_EXPERIENCE         10000

/** Default learning rate */
#define EVOPROOF_DEFAULT_LEARNING_RATE  0.1f

/** Default discount factor */
#define EVOPROOF_DEFAULT_DISCOUNT       0.95f

/** Default exploration rate */
#define EVOPROOF_DEFAULT_EPSILON        0.1f

/* ============================================================================
 * Gene Types
 * ============================================================================ */

/**
 * @brief Types of genes controlling proof strategy
 */
typedef enum {
    PROOF_GENE_SEARCH_DEPTH = 0,     /**< Maximum search depth */
    PROOF_GENE_RULE_PRIORITY,        /**< Priority for rule application */
    PROOF_GENE_UNIFICATION_STRATEGY, /**< Unification approach */
    PROOF_GENE_BACKTRACK_THRESHOLD,  /**< When to backtrack */
    PROOF_GENE_HEURISTIC_WEIGHT,     /**< Weight for heuristics */
    PROOF_GENE_LEMMA_GENERATION,     /**< Lemma generation aggressiveness */
    PROOF_GENE_ANALOGY_WEIGHT,       /**< Weight for analogical reasoning */
    PROOF_GENE_QUANTUM_WEIGHT,       /**< Weight for quantum search */
    PROOF_GENE_ELEGANCE_WEIGHT,      /**< Weight for proof elegance */
    PROOF_GENE_EXPLORATION,          /**< Exploration vs exploitation */
    PROOF_GENE_COUNT                 /**< Total gene types */
} proof_gene_type_t;

/**
 * @brief Actions available during proof search
 */
typedef enum {
    PROOF_ACTION_APPLY_RULE = 0,     /**< Apply inference rule */
    PROOF_ACTION_UNIFY,              /**< Attempt unification */
    PROOF_ACTION_BACKTRACK,          /**< Backtrack to previous state */
    PROOF_ACTION_PRUNE_BRANCH,       /**< Prune unpromising branch */
    PROOF_ACTION_ASSERT_LEMMA,       /**< Assert intermediate lemma */
    PROOF_ACTION_APPLY_ANALOGY,      /**< Apply analogical reasoning */
    PROOF_ACTION_QUANTUM_SEARCH,     /**< Use quantum-enhanced search */
    PROOF_ACTION_SPLIT_GOAL,         /**< Split goal into subgoals */
    PROOF_ACTION_REWRITE,            /**< Apply rewrite rule */
    PROOF_ACTION_INDUCTION,          /**< Apply induction */
    PROOF_ACTION_CONTRAPOSITIVE,     /**< Use contrapositive */
    PROOF_ACTION_COUNT               /**< Total action types */
} proof_action_t;

/* ============================================================================
 * Strategy Structures
 * ============================================================================ */

/**
 * @brief Single gene in a proof strategy
 */
typedef struct proof_gene {
    proof_gene_type_t type;          /**< Gene type */
    float value;                     /**< Gene value */
    float min_value;                 /**< Minimum allowed */
    float max_value;                 /**< Maximum allowed */
    float mutation_sigma;            /**< Mutation standard deviation */
} proof_gene_t;

/**
 * @brief Proof strategy (individual in GA population)
 */
typedef struct proof_strategy {
    uint32_t id;                     /**< Strategy identifier */
    proof_gene_t genes[PROOF_GENE_COUNT]; /**< Gene array */
    proof_action_t action_prefs[PROOF_ACTION_COUNT]; /**< Action preferences */
    float action_weights[PROOF_ACTION_COUNT]; /**< Action weights */

    /* Fitness metrics */
    float fitness;                   /**< Overall fitness */
    float proof_success_rate;        /**< Success rate */
    float avg_proof_length;          /**< Average proof length */
    float avg_proof_time_ms;         /**< Average proof time */
    float elegance_score;            /**< Average elegance */

    /* Usage statistics */
    uint32_t proofs_attempted;       /**< Total attempts */
    uint32_t proofs_succeeded;       /**< Successful proofs */
    uint32_t generation;             /**< Generation born */
    uint64_t total_time_us;          /**< Total time used */
} proof_strategy_t;

/* ============================================================================
 * Q-Learning Structures
 * ============================================================================ */

/**
 * @brief State representation for Q-learning
 */
typedef struct proof_state {
    uint32_t state_id;               /**< State identifier */
    uint32_t goal_complexity;        /**< Complexity of current goal */
    uint32_t depth;                  /**< Current proof depth */
    uint32_t available_rules;        /**< Number of applicable rules */
    uint32_t backtrack_count;        /**< Times backtracked */
    float progress_estimate;         /**< Estimated progress */
    uint64_t state_hash;             /**< Hash for lookup */
} proof_state_t;

/**
 * @brief Q-table entry
 */
typedef struct proof_q_entry {
    uint64_t state_hash;             /**< State hash for lookup */
    float q_values[PROOF_ACTION_COUNT]; /**< Q-value per action */
    uint32_t visit_count;            /**< Visit count */
    uint64_t last_update_us;         /**< Last update time */
} proof_q_entry_t;

/**
 * @brief Experience tuple for replay
 */
typedef struct proof_experience {
    proof_state_t state;             /**< State */
    proof_action_t action;           /**< Action taken */
    float reward;                    /**< Reward received */
    proof_state_t next_state;        /**< Resulting state */
    bool terminal;                   /**< Whether terminal */
    uint64_t timestamp_us;           /**< When recorded */
} proof_experience_t;

/* ============================================================================
 * Proof Trace
 * ============================================================================ */

/**
 * @brief Single step in proof trace
 */
typedef struct evoproof_step {
    uint32_t step_id;                /**< Step identifier */
    proof_action_t action;           /**< Action taken */
    char* statement;                 /**< Statement produced */
    char* justification;             /**< Rule/justification used */
    uint32_t* premise_ids;           /**< Premise step IDs */
    uint32_t num_premises;           /**< Number of premises */
    float confidence;                /**< Step confidence */
    uint64_t time_us;                /**< Time to produce */
} evoproof_step_t;

/**
 * @brief Complete proof trace from evolutionary search
 */
typedef struct evoproof_trace {
    evoproof_step_t* steps;          /**< Array of steps */
    uint32_t num_steps;              /**< Number of steps */
    uint32_t capacity;               /**< Allocated capacity */
    bool is_complete;                /**< Proof complete? */
    bool is_valid;                   /**< Proof validated? */
    float elegance_score;            /**< Elegance score */
    uint32_t strategy_id;            /**< Strategy that found it */
    uint64_t search_time_us;         /**< Time to find */
} evoproof_trace_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Algorithm selection
 */
typedef enum {
    EVOPROOF_ALGO_GENETIC = 0,       /**< Pure genetic algorithm */
    EVOPROOF_ALGO_QLEARNING,         /**< Pure Q-learning */
    EVOPROOF_ALGO_SARSA,             /**< SARSA algorithm */
    EVOPROOF_ALGO_ACTOR_CRITIC,      /**< Actor-critic */
    EVOPROOF_ALGO_HYBRID             /**< GA + RL hybrid */
} evoproof_algorithm_t;

/**
 * @brief Selection method for GA
 */
typedef enum {
    EVOPROOF_SELECT_ROULETTE = 0,    /**< Roulette wheel selection */
    EVOPROOF_SELECT_TOURNAMENT,      /**< Tournament selection */
    EVOPROOF_SELECT_RANK,            /**< Rank-based selection */
    EVOPROOF_SELECT_ELITISM          /**< Elitism selection */
} evoproof_selection_t;

/**
 * @brief Crossover method for GA
 */
typedef enum {
    EVOPROOF_CROSS_SINGLE = 0,       /**< Single-point crossover */
    EVOPROOF_CROSS_TWO_POINT,        /**< Two-point crossover */
    EVOPROOF_CROSS_UNIFORM,          /**< Uniform crossover */
    EVOPROOF_CROSS_BLEND             /**< Blend crossover (BLX-alpha) */
} evoproof_crossover_t;

/**
 * @brief Configuration for evolutionary proof search
 */
typedef struct evoproof_config {
    /* Algorithm selection */
    evoproof_algorithm_t algorithm;  /**< Which algorithm to use */

    /* GA parameters */
    uint32_t population_size;        /**< Population size */
    float mutation_rate;             /**< Mutation probability */
    float crossover_rate;            /**< Crossover probability */
    evoproof_selection_t selection;  /**< Selection method */
    evoproof_crossover_t crossover;  /**< Crossover method */
    uint32_t elite_count;            /**< Elites to preserve */
    uint32_t tournament_size;        /**< Tournament size */

    /* RL parameters */
    float learning_rate;             /**< Q-learning rate */
    float discount_factor;           /**< Future reward discount */
    float initial_epsilon;           /**< Initial exploration */
    float epsilon_decay;             /**< Epsilon decay rate */
    float min_epsilon;               /**< Minimum epsilon */
    uint32_t replay_batch_size;      /**< Experience replay batch */
    uint32_t target_update_freq;     /**< Target network update freq */

    /* Proof parameters */
    uint32_t max_proof_depth;        /**< Maximum proof depth */
    uint32_t max_proof_steps;        /**< Maximum proof steps */
    uint64_t proof_timeout_ms;       /**< Timeout per proof */

    /* Fitness weights */
    float weight_success;            /**< Weight for success */
    float weight_speed;              /**< Weight for speed */
    float weight_elegance;           /**< Weight for elegance */
    float weight_generalization;     /**< Weight for generalization */

    /* Integration */
    bool enable_quantum_actions;     /**< Allow quantum search actions */
    bool enable_analogy_actions;     /**< Allow analogy actions */
    bool enable_bio_async;           /**< Enable async messaging */
    bool enable_transfer_learning;   /**< Enable knowledge transfer */

    /* Modulation */
    float atp_sensitivity;           /**< Sensitivity to ATP levels */
} evoproof_config_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Statistics for evolutionary proof search
 */
typedef struct evoproof_stats {
    /* Evolution statistics */
    uint32_t generations;            /**< Generations completed */
    uint32_t total_evaluations;      /**< Total fitness evaluations */
    uint32_t mutations;              /**< Total mutations */
    uint32_t crossovers;             /**< Total crossovers */

    /* Fitness tracking */
    float best_fitness;              /**< Best fitness ever */
    float avg_fitness;               /**< Current average fitness */
    float fitness_variance;          /**< Fitness variance */

    /* Proof statistics */
    uint64_t proofs_attempted;       /**< Total proof attempts */
    uint64_t proofs_succeeded;       /**< Successful proofs */
    float proof_success_rate;        /**< Overall success rate */
    float avg_proof_length;          /**< Average proof length */
    float avg_proof_time_ms;         /**< Average proof time */

    /* Q-learning statistics */
    uint64_t q_updates;              /**< Total Q-value updates */
    uint32_t unique_states;          /**< Unique states visited */
    float current_epsilon;           /**< Current epsilon */

    /* Experience replay */
    uint32_t experiences_stored;     /**< Experiences in buffer */
    uint64_t replay_batches;         /**< Replay batches processed */
} evoproof_stats_t;

/* ============================================================================
 * Opaque Handle
 * ============================================================================ */

/**
 * @brief Opaque handle to evolutionary proof search
 */
typedef struct evolutionary_proof_search evolutionary_proof_search_t;

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/**
 * @brief Create evolutionary proof search
 *
 * @param config Configuration (NULL for defaults)
 * @return EPS handle or NULL on failure
 */
NIMCP_API evolutionary_proof_search_t* evolutionary_proof_create(
    const evoproof_config_t* config);

/**
 * @brief Destroy evolutionary proof search
 *
 * @param eps System to destroy
 */
NIMCP_API void evolutionary_proof_destroy(evolutionary_proof_search_t* eps);

/**
 * @brief Reset to initial state
 *
 * @param eps The EPS system
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t evolutionary_proof_reset(
    evolutionary_proof_search_t* eps);

/**
 * @brief Get default configuration
 *
 * @param config Configuration to fill
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t evolutionary_proof_get_default_config(
    evoproof_config_t* config);

/* ============================================================================
 * Proof Search Functions
 * ============================================================================ */

/**
 * @brief Attempt to prove a goal
 *
 * Main proving interface. Uses evolved strategies and learned
 * Q-values to search for proof.
 *
 * @param eps The EPS system
 * @param logic Knowledge base/axioms
 * @param goal Goal to prove
 * @param trace Output proof trace
 * @param max_steps Maximum steps allowed
 * @return true if proof found
 */
NIMCP_API bool evolutionary_proof_prove(
    evolutionary_proof_search_t* eps,
    const void* logic,
    const char* goal,
    evoproof_trace_t* trace,
    uint32_t max_steps);

/**
 * @brief Prove with specific strategy
 *
 * @param eps The EPS system
 * @param logic Knowledge base
 * @param goal Goal to prove
 * @param strategy_id Strategy to use
 * @param trace Output proof trace
 * @return true if proof found
 */
NIMCP_API bool evolutionary_proof_prove_with_strategy(
    evolutionary_proof_search_t* eps,
    const void* logic,
    const char* goal,
    uint32_t strategy_id,
    evoproof_trace_t* trace);

/* ============================================================================
 * Evolution Functions
 * ============================================================================ */

/**
 * @brief Initialize population with default strategies
 *
 * @param eps The EPS system
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t evolutionary_proof_init_population(
    evolutionary_proof_search_t* eps);

/**
 * @brief Evolve population for one generation
 *
 * @param eps The EPS system
 * @return Number of offspring created
 */
NIMCP_API uint32_t evolutionary_proof_evolve_generation(
    evolutionary_proof_search_t* eps);

/**
 * @brief Select parent strategies for reproduction
 *
 * @param eps The EPS system
 * @param parent1 Output first parent ID
 * @param parent2 Output second parent ID
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t evolutionary_proof_select_parents(
    evolutionary_proof_search_t* eps,
    uint32_t* parent1,
    uint32_t* parent2);

/**
 * @brief Perform crossover between strategies
 *
 * @param eps The EPS system
 * @param parent1 First parent
 * @param parent2 Second parent
 * @param child Output child strategy
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t evolutionary_proof_crossover(
    evolutionary_proof_search_t* eps,
    const proof_strategy_t* parent1,
    const proof_strategy_t* parent2,
    proof_strategy_t* child);

/**
 * @brief Mutate a strategy
 *
 * @param eps The EPS system
 * @param strategy Strategy to mutate (modified in place)
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t evolutionary_proof_mutate(
    evolutionary_proof_search_t* eps,
    proof_strategy_t* strategy);

/**
 * @brief Get best strategy in population
 *
 * @param eps The EPS system
 * @return Pointer to best strategy
 */
NIMCP_API const proof_strategy_t* evolutionary_proof_get_best(
    const evolutionary_proof_search_t* eps);

/* ============================================================================
 * Q-Learning Functions
 * ============================================================================ */

/**
 * @brief Select action using epsilon-greedy policy
 *
 * @param eps The EPS system
 * @param state Current state
 * @return Selected action
 */
NIMCP_API proof_action_t evolutionary_proof_select_action(
    evolutionary_proof_search_t* eps,
    const proof_state_t* state);

/**
 * @brief Update Q-value for state-action pair
 *
 * @param eps The EPS system
 * @param state State
 * @param action Action taken
 * @param reward Reward received
 * @param next_state Next state
 * @param done Whether episode ended
 * @return Updated Q-value
 */
NIMCP_API float evolutionary_proof_update_q(
    evolutionary_proof_search_t* eps,
    const proof_state_t* state,
    proof_action_t action,
    float reward,
    const proof_state_t* next_state,
    bool done);

/**
 * @brief Get Q-value for state-action pair
 *
 * @param eps The EPS system
 * @param state State
 * @param action Action
 * @return Q-value
 */
NIMCP_API float evolutionary_proof_get_q_value(
    const evolutionary_proof_search_t* eps,
    const proof_state_t* state,
    proof_action_t action);

/**
 * @brief Store experience in replay buffer
 *
 * @param eps The EPS system
 * @param exp Experience to store
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t evolutionary_proof_store_experience(
    evolutionary_proof_search_t* eps,
    const proof_experience_t* exp);

/**
 * @brief Sample and learn from experience replay
 *
 * @param eps The EPS system
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t evolutionary_proof_replay_learn(
    evolutionary_proof_search_t* eps);

/* ============================================================================
 * Transfer Learning
 * ============================================================================ */

/**
 * @brief Export learned knowledge
 *
 * @param eps The EPS system
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes written or negative on error
 */
NIMCP_API int32_t evolutionary_proof_export_knowledge(
    const evolutionary_proof_search_t* eps,
    void* buffer,
    uint32_t buffer_size);

/**
 * @brief Import learned knowledge
 *
 * @param eps The EPS system
 * @param buffer Input buffer
 * @param buffer_size Buffer size
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t evolutionary_proof_import_knowledge(
    evolutionary_proof_search_t* eps,
    const void* buffer,
    uint32_t buffer_size);

/* ============================================================================
 * Trace Management
 * ============================================================================ */

/**
 * @brief Initialize proof trace
 *
 * @param trace Trace to initialize
 * @param capacity Initial capacity
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t evolutionary_proof_trace_init(
    evoproof_trace_t* trace,
    uint32_t capacity);

/**
 * @brief Clean up proof trace
 *
 * @param trace Trace to clean up
 */
NIMCP_API void evolutionary_proof_trace_cleanup(evoproof_trace_t* trace);

/* ============================================================================
 * Modulation
 * ============================================================================ */

/**
 * @brief Apply ATP level modulation
 *
 * @param eps The EPS system
 * @param atp_level ATP level [0,1]
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t evolutionary_proof_modulate_atp(
    evolutionary_proof_search_t* eps,
    float atp_level);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Register with bio-async router
 *
 * @param eps The EPS system
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t evolutionary_proof_register_bio_async(
    evolutionary_proof_search_t* eps);

/**
 * @brief Unregister from bio-async router
 *
 * @param eps The EPS system
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t evolutionary_proof_unregister_bio_async(
    evolutionary_proof_search_t* eps);

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Get statistics
 *
 * @param eps The EPS system
 * @param stats Output statistics
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t evolutionary_proof_get_stats(
    const evolutionary_proof_search_t* eps,
    evoproof_stats_t* stats);

/**
 * @brief Print diagnostics
 *
 * @param eps The EPS system
 */
NIMCP_API void evolutionary_proof_print_diagnostics(
    const evolutionary_proof_search_t* eps);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EVOLUTIONARY_PROOF_H */
