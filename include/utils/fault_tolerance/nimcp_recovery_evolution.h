/**
 * @file nimcp_recovery_evolution.h
 * @brief Recovery Strategy Evolution using Genetic Algorithms and Reinforcement Learning
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Evolve optimal recovery strategies through learning
 * WHY:  Static strategies cannot adapt to changing failure patterns
 * HOW:  Genetic algorithms for strategy optimization, RL for action selection
 *
 * BIOLOGICAL BASIS:
 * - Natural selection (survival of fittest recovery strategies)
 * - Synaptic plasticity (strengthen successful recovery pathways)
 * - Immune system memory (remember effective responses to past failures)
 * - Dopamine reward signaling (reinforce successful recoveries)
 *
 * LEARNING APPROACHES:
 * 1. Genetic Algorithm: Evolve strategy parameters over generations
 * 2. Q-Learning: Learn action-value function for recovery decisions
 * 3. Transfer Learning: Apply learned strategies to new failure types
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_RECOVERY_EVOLUTION_H
#define NIMCP_RECOVERY_EVOLUTION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define RE_MAX_POPULATION 64            /**< Max strategies in population */
#define RE_MAX_GENES 32                 /**< Max genes per strategy */
#define RE_MAX_ACTIONS 16               /**< Max recovery actions */
#define RE_MAX_STATES 64                /**< Max states in Q-table */
#define RE_MAX_HISTORY 1000             /**< Max recovery history entries */
#define RE_DEFAULT_MUTATION_RATE 0.1f   /**< Default mutation rate */
#define RE_DEFAULT_CROSSOVER_RATE 0.7f  /**< Default crossover rate */
#define RE_DEFAULT_LEARNING_RATE 0.1f   /**< Default RL learning rate */
#define RE_DEFAULT_DISCOUNT 0.95f       /**< Default discount factor */

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Learning algorithm types
 */
typedef enum {
    RE_ALGO_GENETIC = 0,    /**< Genetic algorithm */
    RE_ALGO_Q_LEARNING,     /**< Q-learning */
    RE_ALGO_SARSA,          /**< SARSA (on-policy) */
    RE_ALGO_ACTOR_CRITIC,   /**< Actor-critic */
    RE_ALGO_HYBRID          /**< Genetic + RL hybrid */
} re_algorithm_t;

/**
 * @brief Selection methods for genetic algorithm
 */
typedef enum {
    RE_SELECT_ROULETTE = 0, /**< Roulette wheel selection */
    RE_SELECT_TOURNAMENT,   /**< Tournament selection */
    RE_SELECT_RANK,         /**< Rank-based selection */
    RE_SELECT_ELITISM       /**< Elitism (keep best) */
} re_selection_t;

/**
 * @brief Crossover methods
 */
typedef enum {
    RE_CROSS_SINGLE = 0,    /**< Single-point crossover */
    RE_CROSS_TWO_POINT,     /**< Two-point crossover */
    RE_CROSS_UNIFORM,       /**< Uniform crossover */
    RE_CROSS_BLEND          /**< Blend crossover (BLX-alpha) */
} re_crossover_t;

/**
 * @brief Recovery actions (genes)
 */
typedef enum {
    RE_ACTION_RETRY = 0,        /**< Simple retry */
    RE_ACTION_CHECKPOINT,       /**< Restore checkpoint */
    RE_ACTION_REDUCE_LOAD,      /**< Reduce system load */
    RE_ACTION_ISOLATE,          /**< Isolate component */
    RE_ACTION_RESTART,          /**< Restart component */
    RE_ACTION_FAILOVER,         /**< Failover to backup */
    RE_ACTION_DEGRADE,          /**< Graceful degradation */
    RE_ACTION_ESCALATE,         /**< Escalate to higher level */
    RE_ACTION_CACHE_CLEAR,      /**< Clear caches */
    RE_ACTION_GC,               /**< Trigger garbage collection */
    RE_ACTION_REDUCE_LR,        /**< Reduce learning rate */
    RE_ACTION_CLIP_GRADIENT,    /**< Clip gradients */
    RE_ACTION_COUNT
} re_action_t;

/**
 * @brief Fitness evaluation criteria
 */
typedef enum {
    RE_FIT_RECOVERY_TIME = 0,   /**< Minimize recovery time */
    RE_FIT_SUCCESS_RATE,        /**< Maximize success rate */
    RE_FIT_RESOURCE_USAGE,      /**< Minimize resource usage */
    RE_FIT_DATA_LOSS,           /**< Minimize data loss */
    RE_FIT_COMPOSITE            /**< Weighted composite */
} re_fitness_criteria_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Gene representing a recovery parameter
 */
typedef struct {
    char name[32];              /**< Gene name */
    float value;                /**< Current value */
    float min_value;            /**< Minimum allowed */
    float max_value;            /**< Maximum allowed */
    float mutation_sigma;       /**< Mutation std dev */
    bool is_integer;            /**< Round to integer */
} re_gene_t;

/**
 * @brief Recovery strategy (chromosome)
 */
typedef struct {
    uint32_t id;                        /**< Strategy ID */
    re_gene_t genes[RE_MAX_GENES];      /**< Strategy genes */
    uint32_t gene_count;                /**< Number of genes */
    re_action_t actions[RE_MAX_ACTIONS];/**< Action sequence */
    uint32_t action_count;              /**< Number of actions */
    float fitness;                      /**< Fitness score */
    uint32_t generation;                /**< Generation born */
    uint32_t times_used;                /**< Usage count */
    uint32_t times_succeeded;           /**< Success count */
    uint64_t avg_recovery_time_ms;      /**< Avg recovery time */
} re_strategy_t;

/**
 * @brief Q-table entry for RL
 */
typedef struct {
    uint32_t state_id;          /**< State identifier */
    float q_values[RE_ACTION_COUNT]; /**< Q-value per action */
    uint32_t visit_count;       /**< State visit count */
} re_q_entry_t;

/**
 * @brief Recovery experience (for replay)
 */
typedef struct {
    uint32_t state;             /**< Starting state */
    re_action_t action;         /**< Action taken */
    float reward;               /**< Reward received */
    uint32_t next_state;        /**< Resulting state */
    bool terminal;              /**< Episode ended */
    uint64_t timestamp_ms;      /**< Experience timestamp */
} re_experience_t;

/**
 * @brief Recovery outcome for fitness evaluation
 */
typedef struct {
    uint32_t strategy_id;       /**< Strategy used */
    bool success;               /**< Recovery succeeded */
    uint64_t recovery_time_ms;  /**< Time to recover */
    float resource_usage;       /**< Resource consumption (0-1) */
    float data_loss;            /**< Data loss percentage (0-1) */
    uint32_t retry_count;       /**< Number of retries */
    uint32_t fault_type;        /**< Type of fault */
} re_outcome_t;

/**
 * @brief Configuration for recovery evolution
 */
typedef struct {
    re_algorithm_t algorithm;       /**< Learning algorithm */
    uint32_t population_size;       /**< GA population size */
    uint32_t elite_count;           /**< Number of elite to keep */
    float mutation_rate;            /**< Mutation probability */
    float crossover_rate;           /**< Crossover probability */
    re_selection_t selection;       /**< Selection method */
    re_crossover_t crossover;       /**< Crossover method */
    float learning_rate;            /**< RL learning rate (alpha) */
    float discount_factor;          /**< RL discount (gamma) */
    float epsilon;                  /**< Exploration rate */
    float epsilon_decay;            /**< Epsilon decay rate */
    float min_epsilon;              /**< Minimum epsilon */
    re_fitness_criteria_t fitness_criteria; /**< Fitness evaluation */
    float fitness_weights[5];       /**< Weights for composite fitness */
    uint32_t history_size;          /**< Experience replay buffer size */
    uint32_t batch_size;            /**< Mini-batch size for learning */
    bool enable_transfer;           /**< Enable transfer learning */
} re_config_t;

/**
 * @brief Statistics for recovery evolution
 */
typedef struct {
    uint32_t total_generations;
    uint32_t total_evaluations;
    uint32_t total_mutations;
    uint32_t total_crossovers;
    float best_fitness;
    float avg_fitness;
    float worst_fitness;
    uint32_t best_strategy_id;
    float avg_success_rate;
    uint64_t avg_recovery_time_ms;
    float current_epsilon;
    uint32_t unique_states_visited;
} re_stats_t;

/**
 * @brief Opaque recovery evolution handle
 */
typedef struct re_context re_context_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create recovery evolution context
 *
 * WHAT: Initialize learning system
 * WHY:  Required before evolution
 * HOW:  Initialize population, Q-table, history
 *
 * @param config Configuration
 * @return RE context or NULL on failure
 */
re_context_t* re_create(const re_config_t* config);

/**
 * @brief Destroy recovery evolution context
 *
 * @param ctx RE context
 */
void re_destroy(re_context_t* ctx);

/**
 * @brief Get default configuration
 *
 * @return Default configuration
 */
re_config_t re_default_config(void);

//=============================================================================
// Genetic Algorithm Functions
//=============================================================================

/**
 * @brief Initialize population with random strategies
 *
 * @param ctx RE context
 * @return true on success
 */
bool re_init_population(re_context_t* ctx);

/**
 * @brief Add strategy to population
 *
 * @param ctx RE context
 * @param strategy Strategy to add
 * @return true on success
 */
bool re_add_strategy(re_context_t* ctx, const re_strategy_t* strategy);

/**
 * @brief Evaluate strategy fitness
 *
 * @param ctx RE context
 * @param strategy_id Strategy to evaluate
 * @param outcome Recovery outcome
 * @return Updated fitness score
 */
float re_evaluate_fitness(re_context_t* ctx, uint32_t strategy_id, const re_outcome_t* outcome);

/**
 * @brief Select parent strategies for reproduction
 *
 * @param ctx RE context
 * @param parent1 Output: first parent
 * @param parent2 Output: second parent
 * @return true on success
 */
bool re_select_parents(re_context_t* ctx, re_strategy_t* parent1, re_strategy_t* parent2);

/**
 * @brief Perform crossover between parents
 *
 * @param ctx RE context
 * @param parent1 First parent
 * @param parent2 Second parent
 * @param child Output: child strategy
 * @return true on success
 */
bool re_crossover(
    re_context_t* ctx,
    const re_strategy_t* parent1,
    const re_strategy_t* parent2,
    re_strategy_t* child
);

/**
 * @brief Mutate strategy
 *
 * @param ctx RE context
 * @param strategy Strategy to mutate
 * @return true if mutation occurred
 */
bool re_mutate(re_context_t* ctx, re_strategy_t* strategy);

/**
 * @brief Evolve to next generation
 *
 * @param ctx RE context
 * @return New generation number
 */
uint32_t re_evolve_generation(re_context_t* ctx);

/**
 * @brief Get best strategy
 *
 * @param ctx RE context
 * @param strategy Output: best strategy
 * @return true on success
 */
bool re_get_best_strategy(re_context_t* ctx, re_strategy_t* strategy);

/**
 * @brief Get strategy by ID
 *
 * @param ctx RE context
 * @param strategy_id Strategy ID
 * @param strategy Output: strategy
 * @return true if found
 */
bool re_get_strategy(re_context_t* ctx, uint32_t strategy_id, re_strategy_t* strategy);

//=============================================================================
// Reinforcement Learning Functions
//=============================================================================

/**
 * @brief Select action using epsilon-greedy policy
 *
 * @param ctx RE context
 * @param state Current state
 * @return Selected action
 */
re_action_t re_select_action(re_context_t* ctx, uint32_t state);

/**
 * @brief Update Q-value after action
 *
 * @param ctx RE context
 * @param state Previous state
 * @param action Action taken
 * @param reward Reward received
 * @param next_state New state
 * @param terminal Episode ended
 * @return Updated Q-value
 */
float re_update_q(
    re_context_t* ctx,
    uint32_t state,
    re_action_t action,
    float reward,
    uint32_t next_state,
    bool terminal
);

/**
 * @brief Get Q-value for state-action pair
 *
 * @param ctx RE context
 * @param state State
 * @param action Action
 * @return Q-value
 */
float re_get_q_value(re_context_t* ctx, uint32_t state, re_action_t action);

/**
 * @brief Get best action for state
 *
 * @param ctx RE context
 * @param state State
 * @return Best action
 */
re_action_t re_get_best_action(re_context_t* ctx, uint32_t state);

/**
 * @brief Decay exploration rate
 *
 * @param ctx RE context
 * @return New epsilon value
 */
float re_decay_epsilon(re_context_t* ctx);

//=============================================================================
// Experience Replay
//=============================================================================

/**
 * @brief Store experience in replay buffer
 *
 * @param ctx RE context
 * @param experience Experience to store
 * @return true on success
 */
bool re_store_experience(re_context_t* ctx, const re_experience_t* experience);

/**
 * @brief Sample batch from replay buffer
 *
 * @param ctx RE context
 * @param batch Output batch array
 * @param batch_size Desired batch size
 * @return Actual batch size
 */
uint32_t re_sample_batch(re_context_t* ctx, re_experience_t* batch, uint32_t batch_size);

/**
 * @brief Learn from experience batch
 *
 * @param ctx RE context
 * @return Average loss
 */
float re_learn_from_batch(re_context_t* ctx);

//=============================================================================
// Transfer Learning
//=============================================================================

/**
 * @brief Export learned knowledge
 *
 * @param ctx RE context
 * @param buffer Output buffer
 * @param buffer_size Buffer capacity
 * @return Bytes written
 */
size_t re_export_knowledge(re_context_t* ctx, void* buffer, size_t buffer_size);

/**
 * @brief Import learned knowledge
 *
 * @param ctx RE context
 * @param data Knowledge data
 * @param data_size Data size
 * @return true on success
 */
bool re_import_knowledge(re_context_t* ctx, const void* data, size_t data_size);

/**
 * @brief Transfer learning from similar domain
 *
 * @param ctx RE context
 * @param source_ctx Source context
 * @param transfer_rate How much to transfer (0-1)
 * @return true on success
 */
bool re_transfer_from(re_context_t* ctx, const re_context_t* source_ctx, float transfer_rate);

//=============================================================================
// Strategy Recommendation
//=============================================================================

/**
 * @brief Recommend strategy for fault type
 *
 * @param ctx RE context
 * @param fault_type Type of fault
 * @param fault_severity Severity (0-100)
 * @param strategy Output: recommended strategy
 * @return Confidence in recommendation (0-1)
 */
float re_recommend_strategy(
    re_context_t* ctx,
    uint32_t fault_type,
    uint32_t fault_severity,
    re_strategy_t* strategy
);

/**
 * @brief Get action sequence for fault
 *
 * @param ctx RE context
 * @param fault_type Type of fault
 * @param actions Output: action sequence
 * @param max_actions Array capacity
 * @return Number of actions
 */
uint32_t re_get_action_sequence(
    re_context_t* ctx,
    uint32_t fault_type,
    re_action_t* actions,
    uint32_t max_actions
);

//=============================================================================
// Statistics and Diagnostics
//=============================================================================

/**
 * @brief Get evolution statistics
 *
 * @param ctx RE context
 * @param stats Output statistics
 * @return true on success
 */
bool re_get_stats(re_context_t* ctx, re_stats_t* stats);

/**
 * @brief Reset statistics
 *
 * @param ctx RE context
 */
void re_reset_stats(re_context_t* ctx);

/**
 * @brief Get population diversity metric
 *
 * @param ctx RE context
 * @return Diversity score (0-1)
 */
float re_get_diversity(re_context_t* ctx);

/**
 * @brief Print Q-table for debugging
 *
 * @param ctx RE context
 */
void re_print_q_table(re_context_t* ctx);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Calculate fitness from outcome
 *
 * @param ctx RE context
 * @param outcome Recovery outcome
 * @return Fitness score
 */
float re_calculate_fitness(re_context_t* ctx, const re_outcome_t* outcome);

/**
 * @brief Calculate reward from outcome
 *
 * @param outcome Recovery outcome
 * @return Reward value
 */
float re_calculate_reward(const re_outcome_t* outcome);

/**
 * @brief Encode fault state
 *
 * @param fault_type Fault type
 * @param severity Severity (0-100)
 * @param context Additional context
 * @return State ID
 */
uint32_t re_encode_state(uint32_t fault_type, uint32_t severity, uint32_t context);

//=============================================================================
// String Conversion
//=============================================================================

const char* re_algorithm_to_string(re_algorithm_t algo);
const char* re_action_to_string(re_action_t action);
const char* re_selection_to_string(re_selection_t selection);
const char* re_crossover_to_string(re_crossover_t crossover);
const char* re_fitness_to_string(re_fitness_criteria_t criteria);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_RECOVERY_EVOLUTION_H
