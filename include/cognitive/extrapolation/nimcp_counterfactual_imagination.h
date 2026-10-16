/**
 * @file nimcp_counterfactual_imagination.h
 * @brief Counterfactual reasoning and "what if" scenario imagination
 *
 * WHAT: Enables counterfactual reasoning - imagining alternative scenarios
 * WHY:  Essential for causal understanding, planning, and learning from mistakes
 * HOW:  Pearl's causal inference framework with do-calculus interventions
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal cortex: Executive functions for hypothetical reasoning
 * - Hippocampus: Episodic memory for constructing alternative pasts
 * - Default mode network: Mental simulation of counterfactuals
 *
 * INTEGRATION POINTS:
 * - World model for state simulation
 * - Episodic memory for factual grounding
 * - Planning systems for future counterfactuals
 *
 * @version 1.0.0
 * @date 2025-01-13
 */

#ifndef NIMCP_COUNTERFACTUAL_IMAGINATION_H
#define NIMCP_COUNTERFACTUAL_IMAGINATION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

#define CF_MAX_VARIABLES        256
#define CF_MAX_INTERVENTIONS    64
#define CF_MAX_SCENARIOS        128
#define CF_MAX_HISTORY          1024
#define CF_MAX_NAME_LENGTH      64
#define CF_MAX_OUTCOMES         32

/*=============================================================================
 * ERROR CODES
 *===========================================================================*/

typedef enum {
    CF_OK = 0,
    CF_ERR_NULL_PTR,
    CF_ERR_NOT_INITIALIZED,
    CF_ERR_INVALID_VARIABLE,
    CF_ERR_VARIABLE_NOT_FOUND,
    CF_ERR_INTERVENTION_FAILED,
    CF_ERR_SCENARIO_FAILED,
    CF_ERR_MEMORY_ALLOC,
    CF_ERR_CAPACITY_EXCEEDED,
    CF_ERR_INVALID_CONFIG,
    CF_ERR_CAUSAL_CYCLE,
    CF_ERR_UNIDENTIFIABLE,
    CF_ERR_SIMULATION_FAILED
} cf_error_t;

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief Type of counterfactual scenario
 *
 * INTERVENTION:  "What if X had been set to Y?" (do-operator)
 * PREVENTION:    "What if X had NOT happened?"
 * ALTERNATIVE:   "What if X instead of Y had occurred?"
 * HYPOTHETICAL:  "What would happen if X were to occur?"
 */
typedef enum {
    CF_SCENARIO_INTERVENTION = 0,   /**< do(X=x) intervention */
    CF_SCENARIO_PREVENTION,         /**< Prevent an event */
    CF_SCENARIO_ALTERNATIVE,        /**< Alternative event path */
    CF_SCENARIO_HYPOTHETICAL,       /**< Future hypothetical */
    CF_SCENARIO_COUNT
} cf_scenario_type_t;

/**
 * @brief Evaluation mode for counterfactual queries
 *
 * CAUSAL:        Use causal inference (do-calculus)
 * PROBABILISTIC: Use probabilistic inference
 * STRUCTURAL:    Use structural equation models
 */
typedef enum {
    CF_EVAL_CAUSAL = 0,             /**< Causal (interventional) */
    CF_EVAL_PROBABILISTIC,          /**< Probabilistic (observational) */
    CF_EVAL_STRUCTURAL,             /**< Structural equation model */
    CF_EVAL_COUNT
} cf_evaluation_mode_t;

/**
 * @brief Variable type in causal model
 */
typedef enum {
    CF_VAR_CONTINUOUS = 0,          /**< Continuous real value */
    CF_VAR_DISCRETE,                /**< Discrete integer value */
    CF_VAR_BINARY,                  /**< Binary (0 or 1) */
    CF_VAR_CATEGORICAL,             /**< Categorical (enum) */
    CF_VAR_COUNT
} cf_variable_type_t;

/**
 * @brief Status of counterfactual system
 */
typedef enum {
    CF_STATUS_IDLE = 0,
    CF_STATUS_SIMULATING,
    CF_STATUS_EVALUATING,
    CF_STATUS_INTERVENING,
    CF_STATUS_ERROR
} cf_status_t;

/*=============================================================================
 * STRUCTURES
 *===========================================================================*/

/**
 * @brief Causal variable in the model
 *
 * WHAT: A variable that can be observed, intervened on, or predicted
 * WHY:  Building block of causal models
 * HOW:  Store current/counterfactual values with metadata
 */
typedef struct {
    uint32_t id;                            /**< Unique variable ID */
    char name[CF_MAX_NAME_LENGTH];          /**< Human-readable name */
    cf_variable_type_t type;                /**< Variable type */

    /* Values */
    float value;                            /**< Current/factual value */
    float counterfactual_value;             /**< Counterfactual value */

    /* Observability */
    bool is_observed;                       /**< Was this observed? */
    float observation_confidence;           /**< Confidence in observation */
    uint64_t observation_time;              /**< When observed */

    /* Causal info */
    uint32_t* parent_ids;                   /**< Parent variable IDs */
    uint32_t num_parents;                   /**< Number of parents */
    float* causal_weights;                  /**< Weights from parents */

    /* Statistics */
    float mean;                             /**< Historical mean */
    float variance;                         /**< Historical variance */
} cf_variable_t;

/**
 * @brief Intervention on a variable
 *
 * WHAT: A do(X=x) operation in causal inference
 * WHY:  Core operation for counterfactual reasoning
 * HOW:  Force variable to value, breaking causal links from parents
 */
typedef struct {
    uint32_t variable_id;                   /**< Variable to intervene on */
    float original_value;                   /**< Value before intervention */
    float new_value;                        /**< Intervention value */
    float counterfactual_value;             /**< Resulting counterfactual */
    bool applied;                           /**< Has intervention been applied? */
    uint64_t timestamp;                     /**< When intervention applied */
} cf_intervention_t;

/**
 * @brief Counterfactual outcome
 *
 * WHAT: Result of a counterfactual query
 * WHY:  Store predicted outcomes with uncertainty
 */
typedef struct {
    uint32_t variable_id;                   /**< Variable of interest */
    float factual_value;                    /**< What actually happened */
    float counterfactual_value;             /**< What would have happened */
    float probability;                      /**< P(Y_x | evidence) */
    float confidence;                       /**< Confidence in estimate */
    float causal_effect;                    /**< Causal effect size */
} cf_outcome_t;

/**
 * @brief Counterfactual scenario
 *
 * WHAT: A complete counterfactual scenario with interventions and outcomes
 * WHY:  Encapsulate "what if" reasoning in a structured form
 * HOW:  List of interventions, simulated outcomes, probability
 */
typedef struct {
    uint32_t scenario_id;                   /**< Unique scenario ID */
    cf_scenario_type_t type;                /**< Type of scenario */
    char description[256];                  /**< Human-readable description */

    /* Interventions */
    cf_intervention_t interventions[CF_MAX_INTERVENTIONS];
    uint32_t num_interventions;

    /* Outcomes */
    cf_outcome_t outcomes[CF_MAX_OUTCOMES];
    uint32_t num_outcomes;

    /* Evaluation */
    float probability;                      /**< Scenario probability */
    float plausibility;                     /**< How plausible (0-1) */
    float surprise;                         /**< How surprising vs factual */
    bool evaluated;                         /**< Has been evaluated? */

    /* Metadata */
    uint64_t created_time;
    uint64_t evaluated_time;
} cf_scenario_t;

/**
 * @brief Intervention history entry
 */
typedef struct {
    uint32_t variable_id;
    float value_before;
    float value_after;
    float effect_size;
    uint64_t timestamp;
} cf_history_entry_t;

/**
 * @brief Counterfactual system configuration
 */
typedef struct {
    /* Capacity */
    uint32_t max_variables;
    uint32_t max_scenarios;
    uint32_t max_history;

    /* Simulation */
    uint32_t simulation_steps;              /**< Steps for Monte Carlo */
    float learning_rate;                    /**< For causal learning */
    float probability_threshold;            /**< Min probability to consider */

    /* Evaluation */
    cf_evaluation_mode_t default_mode;
    bool enable_caching;
    bool enable_logging;

    /* Integration */
    bool enable_bio_async;
    bool enable_world_model;
} cf_config_t;

/**
 * @brief Counterfactual system statistics
 */
typedef struct {
    uint64_t variables_created;
    uint64_t observations_recorded;
    uint64_t interventions_applied;
    uint64_t scenarios_created;
    uint64_t scenarios_evaluated;
    uint64_t simulations_run;
    float mean_causal_effect;
    float mean_confidence;
    float mean_plausibility;
} cf_stats_t;

/**
 * @brief Causal model (internal representation)
 */
typedef struct {
    cf_variable_t* variables;
    uint32_t num_variables;
    uint32_t variable_capacity;

    /* Adjacency matrix for causal graph */
    float* adjacency;                       /**< [i*n + j] = edge i->j weight */
    bool* has_edge;                         /**< [i*n + j] = edge exists? */

    /* Cached computations */
    bool topological_valid;
    uint32_t* topological_order;
} cf_causal_model_t;

/**
 * @brief Main counterfactual imagination system
 */
typedef struct nimcp_counterfactual {
    cf_config_t config;
    bool initialized;
    cf_status_t status;
    cf_error_t last_error;

    /* Causal model */
    cf_causal_model_t* causal_model;

    /* Scenarios */
    cf_scenario_t* scenarios;
    uint32_t num_scenarios;
    uint32_t scenario_capacity;

    /* History */
    cf_history_entry_t* history;
    uint32_t history_count;
    uint32_t history_capacity;

    /* Statistics */
    cf_stats_t stats;

    /* Integration */
    void* bio_async_ctx;
    void* world_model_ctx;
} nimcp_counterfactual_t;

/*=============================================================================
 * LIFECYCLE API
 *===========================================================================*/

/**
 * @brief Get default configuration
 */
cf_config_t cf_default_config(void);

/**
 * @brief Create counterfactual imagination system
 *
 * @param config Configuration (NULL for defaults)
 * @return System instance or NULL on failure
 */
nimcp_counterfactual_t* cf_create(const cf_config_t* config);

/**
 * @brief Initialize counterfactual system
 *
 * @param cf System to initialize
 * @return CF_OK on success
 */
cf_error_t cf_init(nimcp_counterfactual_t* cf);

/**
 * @brief Reset system to initial state
 *
 * @param cf System to reset
 * @return CF_OK on success
 */
cf_error_t cf_reset(nimcp_counterfactual_t* cf);

/**
 * @brief Destroy counterfactual system
 *
 * @param cf System to destroy (NULL safe)
 */
void cf_destroy(nimcp_counterfactual_t* cf);

/*=============================================================================
 * VARIABLE API
 *===========================================================================*/

/**
 * @brief Add a variable to the causal model
 *
 * @param cf System
 * @param name Variable name
 * @param type Variable type
 * @param variable_id Output: assigned ID
 * @return CF_OK on success
 */
cf_error_t cf_add_variable(
    nimcp_counterfactual_t* cf,
    const char* name,
    cf_variable_type_t type,
    uint32_t* variable_id);

/**
 * @brief Set variable value
 *
 * @param cf System
 * @param variable_id Variable to set
 * @param value New value
 * @return CF_OK on success
 */
cf_error_t cf_set_variable(
    nimcp_counterfactual_t* cf,
    uint32_t variable_id,
    float value);

/**
 * @brief Get variable value
 *
 * @param cf System
 * @param variable_id Variable to query
 * @param value Output: current value
 * @return CF_OK on success
 */
cf_error_t cf_get_variable(
    nimcp_counterfactual_t* cf,
    uint32_t variable_id,
    float* value);

/**
 * @brief Get variable by name
 *
 * @param cf System
 * @param name Variable name
 * @param variable Output: variable data
 * @return CF_OK on success
 */
cf_error_t cf_get_variable_by_name(
    nimcp_counterfactual_t* cf,
    const char* name,
    cf_variable_t* variable);

/**
 * @brief Record observation of variable
 *
 * @param cf System
 * @param variable_id Variable observed
 * @param value Observed value
 * @param confidence Confidence in observation (0-1)
 * @return CF_OK on success
 */
cf_error_t cf_observe(
    nimcp_counterfactual_t* cf,
    uint32_t variable_id,
    float value,
    float confidence);

/**
 * @brief Add causal link between variables
 *
 * @param cf System
 * @param parent_id Parent (cause) variable
 * @param child_id Child (effect) variable
 * @param weight Causal strength
 * @return CF_OK on success
 */
cf_error_t cf_add_causal_link(
    nimcp_counterfactual_t* cf,
    uint32_t parent_id,
    uint32_t child_id,
    float weight);

/*=============================================================================
 * INTERVENTION API
 *===========================================================================*/

/**
 * @brief Apply intervention (do-operator)
 *
 * WHAT: Set variable to value, breaking causal links from parents
 * WHY:  Core operation for causal inference
 * HOW:  do(X=x) semantics from Pearl's framework
 *
 * @param cf System
 * @param variable_id Variable to intervene on
 * @param value Intervention value
 * @return CF_OK on success
 */
cf_error_t cf_intervene(
    nimcp_counterfactual_t* cf,
    uint32_t variable_id,
    float value);

/**
 * @brief Undo last intervention
 *
 * @param cf System
 * @return CF_OK on success
 */
cf_error_t cf_undo_intervention(nimcp_counterfactual_t* cf);

/**
 * @brief Clear all active interventions
 *
 * @param cf System
 * @return CF_OK on success
 */
cf_error_t cf_clear_interventions(nimcp_counterfactual_t* cf);

/*=============================================================================
 * SCENARIO API
 *===========================================================================*/

/**
 * @brief Create counterfactual scenario
 *
 * @param cf System
 * @param type Scenario type
 * @param description Human-readable description
 * @param scenario_id Output: assigned scenario ID
 * @return CF_OK on success
 */
cf_error_t cf_create_scenario(
    nimcp_counterfactual_t* cf,
    cf_scenario_type_t type,
    const char* description,
    uint32_t* scenario_id);

/**
 * @brief Add intervention to scenario
 *
 * @param cf System
 * @param scenario_id Scenario to modify
 * @param variable_id Variable to intervene on
 * @param value Intervention value
 * @return CF_OK on success
 */
cf_error_t cf_scenario_add_intervention(
    nimcp_counterfactual_t* cf,
    uint32_t scenario_id,
    uint32_t variable_id,
    float value);

/**
 * @brief Add outcome of interest to scenario
 *
 * @param cf System
 * @param scenario_id Scenario to modify
 * @param variable_id Variable to track outcome
 * @return CF_OK on success
 */
cf_error_t cf_scenario_add_outcome(
    nimcp_counterfactual_t* cf,
    uint32_t scenario_id,
    uint32_t variable_id);

/**
 * @brief Imagine (simulate) a scenario
 *
 * WHAT: Run counterfactual simulation for scenario
 * WHY:  Generate predicted outcomes under interventions
 * HOW:  Apply interventions, propagate through causal model
 *
 * @param cf System
 * @param scenario_id Scenario to imagine
 * @return CF_OK on success
 */
cf_error_t cf_imagine_scenario(
    nimcp_counterfactual_t* cf,
    uint32_t scenario_id);

/**
 * @brief Evaluate scenario probability/plausibility
 *
 * @param cf System
 * @param scenario_id Scenario to evaluate
 * @param mode Evaluation mode
 * @return CF_OK on success
 */
cf_error_t cf_evaluate_scenario(
    nimcp_counterfactual_t* cf,
    uint32_t scenario_id,
    cf_evaluation_mode_t mode);

/**
 * @brief Get scenario data
 *
 * @param cf System
 * @param scenario_id Scenario to retrieve
 * @param scenario Output: scenario data
 * @return CF_OK on success
 */
cf_error_t cf_get_scenario(
    nimcp_counterfactual_t* cf,
    uint32_t scenario_id,
    cf_scenario_t* scenario);

/*=============================================================================
 * CAUSAL INFERENCE API
 *===========================================================================*/

/**
 * @brief Compute causal effect of X on Y
 *
 * WHAT: Estimate E[Y | do(X=x)] - E[Y | do(X=x')]
 * WHY:  Core causal query for understanding effects
 * HOW:  Apply do-calculus rules
 *
 * @param cf System
 * @param cause_id Cause variable
 * @param effect_id Effect variable
 * @param cause_value Value to set cause to
 * @param effect Output: causal effect
 * @return CF_OK on success
 */
cf_error_t cf_compute_causal_effect(
    nimcp_counterfactual_t* cf,
    uint32_t cause_id,
    uint32_t effect_id,
    float cause_value,
    float* effect);

/**
 * @brief Get counterfactual outcome for variable
 *
 * @param cf System
 * @param variable_id Variable to query
 * @param outcome Output: counterfactual outcome
 * @return CF_OK on success
 */
cf_error_t cf_get_counterfactual_outcome(
    nimcp_counterfactual_t* cf,
    uint32_t variable_id,
    cf_outcome_t* outcome);

/**
 * @brief Query: "What would Y be if X had been x?"
 *
 * @param cf System
 * @param cause_id Cause variable X
 * @param cause_value Hypothetical value x
 * @param effect_id Effect variable Y
 * @param result Output: counterfactual value of Y
 * @param confidence Output: confidence in result
 * @return CF_OK on success
 */
cf_error_t cf_query_counterfactual(
    nimcp_counterfactual_t* cf,
    uint32_t cause_id,
    float cause_value,
    uint32_t effect_id,
    float* result,
    float* confidence);

/*=============================================================================
 * UPDATE API
 *===========================================================================*/

/**
 * @brief Update system state
 *
 * @param cf System
 * @param dt_ms Time delta in milliseconds
 * @return CF_OK on success
 */
cf_error_t cf_update(nimcp_counterfactual_t* cf, float dt_ms);

/**
 * @brief Get system statistics
 *
 * @param cf System
 * @param stats Output: statistics
 * @return CF_OK on success
 */
cf_error_t cf_get_stats(nimcp_counterfactual_t* cf, cf_stats_t* stats);

/*=============================================================================
 * UTILITY API
 *===========================================================================*/

/**
 * @brief Get system status
 */
cf_status_t cf_get_status(nimcp_counterfactual_t* cf);

/**
 * @brief Get last error
 */
cf_error_t cf_get_last_error(nimcp_counterfactual_t* cf);

/**
 * @brief Error code to string
 */
const char* cf_error_string(cf_error_t error);

/**
 * @brief Status to string
 */
const char* cf_status_string(cf_status_t status);

/**
 * @brief Scenario type to string
 */
const char* cf_scenario_type_string(cf_scenario_type_t type);

/**
 * @brief Evaluation mode to string
 */
const char* cf_evaluation_mode_string(cf_evaluation_mode_t mode);

/**
 * @brief Variable type to string
 */
const char* cf_variable_type_string(cf_variable_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_COUNTERFACTUAL_IMAGINATION_H */
