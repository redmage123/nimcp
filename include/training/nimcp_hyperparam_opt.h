/**
 * @file nimcp_hyperparam_opt.h
 * @brief Hyperparameter Optimization (HPO) for NIMCP
 *
 * WHAT: Automatic hyperparameter tuning for training configurations
 * WHY:  Optimal hyperparameters crucial for training success
 * HOW:  Bayesian optimization, random search, population-based training
 *
 * FRAMEWORK COMPARISON:
 * - PyTorch: Optuna, Ray Tune, Ax
 * - JAX: Vizier, Optuna integration
 * - TensorFlow: Keras Tuner, Vizier
 *
 * NIMCP APPROACH:
 * - Builds on existing auto_architecture NAS infrastructure
 * - Integrates with distributed training for parallel search
 * - Bio-inspired via dopaminergic learning rate modulation
 *
 * BIOLOGICAL GROUNDING:
 * - Dopamine: Modulates learning rate based on prediction error
 * - Neuromodulation: Dynamic adjustment of synaptic plasticity
 * - Homeostatic regulation: Auto-tuning of neural gain
 *
 * INTEGRATION POINTS:
 * - auto_architecture: Share search infrastructure
 * - distributed_training: Parallel trial execution
 * - training_module: Apply optimized hyperparams
 * - gradient_manager: Gradient-based sensitivity analysis
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 * @version 1.0.0
 */

#ifndef NIMCP_HYPERPARAM_OPT_H
#define NIMCP_HYPERPARAM_OPT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "utils/tensor/nimcp_tensor.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define HPO_MAX_PARAMS                64       /**< Maximum hyperparameters */
#define HPO_MAX_TRIALS                10000    /**< Maximum trials */
#define HPO_DEFAULT_TRIALS            100      /**< Default trial count */
#define HPO_MAX_PARALLEL              32       /**< Maximum parallel trials */

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief HPO search algorithm
 *
 * COMPARISON:
 * - Random: Baseline, surprisingly effective
 * - Grid: Exhaustive, exponential scaling
 * - Bayesian: Sample efficient, best for expensive evals
 * - PBT: Population-based, good for schedules
 */
typedef enum {
    HPO_ALG_RANDOM = 0,              /**< Random search */
    HPO_ALG_GRID,                    /**< Grid search */
    HPO_ALG_BAYESIAN_TPE,            /**< Tree-structured Parzen Estimator */
    HPO_ALG_BAYESIAN_GP,             /**< Gaussian Process */
    HPO_ALG_CMA_ES,                  /**< Covariance Matrix Adaptation ES */
    HPO_ALG_HYPERBAND,               /**< Hyperband (successive halving) */
    HPO_ALG_BOHB,                    /**< BOHB (Bayesian + Hyperband) */
    HPO_ALG_PBT,                     /**< Population-Based Training */
    HPO_ALG_OPTUNA,                  /**< Optuna TPE + pruning */
    HPO_ALG_COUNT
} hpo_algorithm_t;

/**
 * @brief Hyperparameter type
 */
typedef enum {
    HPO_PARAM_FLOAT = 0,             /**< Continuous float */
    HPO_PARAM_INT,                   /**< Discrete integer */
    HPO_PARAM_CATEGORICAL,           /**< Categorical choice */
    HPO_PARAM_LOGUNIFORM,            /**< Log-uniform float */
    HPO_PARAM_QUNIFORM,              /**< Quantized uniform */
    HPO_PARAM_BOOL,                  /**< Boolean */
    HPO_PARAM_CONDITIONAL,           /**< Depends on other param */
    HPO_PARAM_TYPE_COUNT
} hpo_param_type_t;

/**
 * @brief Trial status
 */
typedef enum {
    HPO_TRIAL_PENDING = 0,           /**< Not yet started */
    HPO_TRIAL_RUNNING,               /**< Currently running */
    HPO_TRIAL_COMPLETED,             /**< Completed successfully */
    HPO_TRIAL_PRUNED,                /**< Pruned early */
    HPO_TRIAL_FAILED,                /**< Failed with error */
    HPO_TRIAL_STATUS_COUNT
} hpo_trial_status_t;

/**
 * @brief Optimization direction
 */
typedef enum {
    HPO_MINIMIZE = 0,                /**< Minimize objective */
    HPO_MAXIMIZE                     /**< Maximize objective */
} hpo_direction_t;

/**
 * @brief Pruning strategy
 *
 * BIOLOGICAL BASIS:
 * - Early termination of unpromising strategies
 * - Similar to attentional gating of irrelevant information
 */
typedef enum {
    HPO_PRUNE_NONE = 0,              /**< No pruning */
    HPO_PRUNE_MEDIAN,                /**< Median pruner */
    HPO_PRUNE_PERCENTILE,            /**< Percentile pruner */
    HPO_PRUNE_HYPERBAND,             /**< Hyperband successive halving */
    HPO_PRUNE_ASHA,                  /**< Async successive halving */
    HPO_PRUNE_COUNT
} hpo_pruner_t;

//=============================================================================
// Configuration Structures
//=============================================================================

/**
 * @brief Hyperparameter definition
 */
typedef struct {
    const char* name;                /**< Parameter name */
    hpo_param_type_t type;           /**< Parameter type */

    /* Value range (for numeric types) */
    double low;                      /**< Lower bound */
    double high;                     /**< Upper bound */
    double step;                     /**< Step size (for quantized) */

    /* Categorical values */
    const char** choices;            /**< Category strings */
    uint32_t num_choices;            /**< Number of categories */

    /* Default value */
    double default_value;            /**< Default numeric value */
    const char* default_choice;      /**< Default categorical value */

    /* Conditional dependency */
    const char* depends_on;          /**< Parent parameter name */
    const char* depends_value;       /**< Required parent value */
} hpo_param_def_t;

/**
 * @brief Search space definition
 */
typedef struct {
    hpo_param_def_t* params;         /**< Parameter definitions */
    uint32_t num_params;             /**< Number of parameters */
} hpo_search_space_t;

/**
 * @brief Bayesian optimization configuration
 */
typedef struct {
    uint32_t n_initial_points;       /**< Random samples before BO */
    float acquisition_weight;        /**< Exploration vs exploitation */
    const char* acquisition_func;    /**< "ei", "ucb", "pi" */
    uint32_t n_restarts;             /**< Acquisition restarts */
} hpo_bayesian_config_t;

/**
 * @brief Hyperband configuration
 */
typedef struct {
    uint32_t min_resource;           /**< Minimum resource per trial */
    uint32_t max_resource;           /**< Maximum resource per trial */
    uint32_t reduction_factor;       /**< Halving factor (default 3) */
    bool early_stopping;             /**< Enable early stopping */
} hpo_hyperband_config_t;

/**
 * @brief Population-Based Training configuration
 *
 * BIOLOGICAL BASIS:
 * - Evolutionary search with exploitation
 * - Models competitive synaptic competition
 */
typedef struct {
    uint32_t population_size;        /**< Population size */
    float exploit_fraction;          /**< Fraction to replace */
    float perturb_factor;            /**< Perturbation magnitude */
    uint32_t resample_probability;   /**< Resample probability (%) */
    bool log_mutations;              /**< Log mutation events */
} hpo_pbt_config_t;

/**
 * @brief Pruning configuration
 */
typedef struct {
    hpo_pruner_t strategy;           /**< Pruning strategy */
    float percentile;                /**< Percentile threshold */
    uint32_t n_startup_trials;       /**< Trials before pruning starts */
    uint32_t n_warmup_steps;         /**< Steps before pruning */
    uint32_t interval_steps;         /**< Check interval */
} hpo_pruner_config_t;

/**
 * @brief Complete HPO configuration
 */
typedef struct {
    hpo_algorithm_t algorithm;       /**< Search algorithm */
    hpo_direction_t direction;       /**< Minimize or maximize */

    /* Search limits */
    uint32_t n_trials;               /**< Maximum trials */
    float timeout_hours;             /**< Maximum time */
    uint32_t n_parallel;             /**< Parallel trials */

    /* Algorithm-specific configs */
    hpo_bayesian_config_t bayesian;
    hpo_hyperband_config_t hyperband;
    hpo_pbt_config_t pbt;

    /* Pruning */
    hpo_pruner_config_t pruner;

    /* Integration */
    bool integrate_distributed;      /**< Use distributed training */
    bool integrate_callbacks;        /**< Integrate with callbacks */

    /* Persistence */
    const char* study_name;          /**< Study name for resumption */
    const char* storage_path;        /**< Database path for persistence */

    /* Debugging */
    bool verbose;
    bool track_statistics;
} hpo_config_t;

//=============================================================================
// Trial Structures
//=============================================================================

/**
 * @brief Hyperparameter values for a trial
 */
typedef struct {
    const char** param_names;        /**< Parameter names */
    double* param_values;            /**< Numeric values */
    const char** param_choices;      /**< Categorical values */
    uint32_t num_params;             /**< Number of parameters */
} hpo_params_t;

/**
 * @brief Trial result
 */
typedef struct {
    uint32_t trial_id;               /**< Trial identifier */
    hpo_params_t params;             /**< Trial hyperparameters */
    double objective;                /**< Objective value */
    hpo_trial_status_t status;       /**< Trial status */
    double duration_sec;             /**< Trial duration */

    /* Intermediate values (for pruning) */
    double* intermediate_values;     /**< Values at each step */
    uint32_t num_intermediate;       /**< Number of intermediate values */
} hpo_trial_result_t;

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief HPO statistics
 */
typedef struct {
    uint64_t total_trials;           /**< Total trials run */
    uint64_t completed_trials;       /**< Successfully completed */
    uint64_t pruned_trials;          /**< Pruned early */
    uint64_t failed_trials;          /**< Failed trials */

    /* Objective statistics */
    double best_objective;           /**< Best objective value */
    double avg_objective;            /**< Average objective */
    double objective_std;            /**< Objective std dev */

    /* Timing */
    double total_time_hours;         /**< Total search time */
    double avg_trial_time_sec;       /**< Average trial time */

    /* Search efficiency */
    double improvement_rate;         /**< Improvement per trial */
    uint32_t trials_to_best;         /**< Trials to find best */
    float exploration_ratio;         /**< Exploration vs exploitation */
} hpo_stats_t;

//=============================================================================
// Opaque Types
//=============================================================================

/**
 * @brief HPO context (opaque)
 */
typedef struct hpo_ctx_s hpo_ctx_t;

/**
 * @brief HPO study (persistent search state)
 */
typedef struct hpo_study_s hpo_study_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default HPO configuration
 *
 * DEFAULTS:
 * - TPE algorithm
 * - 100 trials
 * - Median pruner
 *
 * @param config Configuration to initialize
 * @return 0 on success, negative on error
 */
int hpo_default_config(hpo_config_t* config);

/**
 * @brief Create HPO context
 *
 * @param config HPO configuration
 * @param search_space Search space definition
 * @return HPO context or NULL on failure
 */
hpo_ctx_t* hpo_create(
    const hpo_config_t* config,
    const hpo_search_space_t* search_space
);

/**
 * @brief Destroy HPO context
 *
 * @param ctx Context to destroy (NULL-safe)
 */
void hpo_destroy(hpo_ctx_t* ctx);

/**
 * @brief Create or load study
 *
 * @param ctx HPO context
 * @param study_name Study name
 * @param create_if_missing Create if doesn't exist
 * @return Study or NULL on failure
 */
hpo_study_t* hpo_get_study(
    hpo_ctx_t* ctx,
    const char* study_name,
    bool create_if_missing
);

//=============================================================================
// Search Space API
//=============================================================================

/**
 * @brief Add float parameter to search space
 *
 * COMPARISON (Optuna equivalent):
 * ```python
 * trial.suggest_float("learning_rate", 1e-5, 1e-1, log=True)
 * ```
 *
 * @param space Search space
 * @param name Parameter name
 * @param low Lower bound
 * @param high Upper bound
 * @param log_scale Use log scale
 * @return 0 on success, negative on error
 */
int hpo_add_float(
    hpo_search_space_t* space,
    const char* name,
    double low,
    double high,
    bool log_scale
);

/**
 * @brief Add integer parameter
 *
 * @param space Search space
 * @param name Parameter name
 * @param low Lower bound
 * @param high Upper bound
 * @param step Step size (1 for all integers)
 * @return 0 on success, negative on error
 */
int hpo_add_int(
    hpo_search_space_t* space,
    const char* name,
    int64_t low,
    int64_t high,
    int64_t step
);

/**
 * @brief Add categorical parameter
 *
 * @param space Search space
 * @param name Parameter name
 * @param choices Array of choice strings
 * @param num_choices Number of choices
 * @return 0 on success, negative on error
 */
int hpo_add_categorical(
    hpo_search_space_t* space,
    const char* name,
    const char** choices,
    uint32_t num_choices
);

/**
 * @brief Add conditional parameter
 *
 * @param space Search space
 * @param name Parameter name
 * @param def Parameter definition
 * @param depends_on Parent parameter name
 * @param depends_value Required parent value
 * @return 0 on success, negative on error
 */
int hpo_add_conditional(
    hpo_search_space_t* space,
    const char* name,
    const hpo_param_def_t* def,
    const char* depends_on,
    const char* depends_value
);

//=============================================================================
// Optimization API
//=============================================================================

/**
 * @brief Optimize hyperparameters
 *
 * WHAT: Run full HPO search
 * WHY:  Find optimal hyperparameters
 * HOW:  Iteratively sample, evaluate, update surrogate
 *
 * COMPARISON (Optuna equivalent):
 * ```python
 * study.optimize(objective, n_trials=100)
 * ```
 *
 * @param ctx HPO context
 * @param objective_fn Objective function to minimize/maximize
 * @param user_data User data for objective function
 * @param best_params Output best parameters found
 * @return Best objective value
 */
double hpo_optimize(
    hpo_ctx_t* ctx,
    double (*objective_fn)(const hpo_params_t* params, void* user_data),
    void* user_data,
    hpo_params_t** best_params
);

/**
 * @brief Suggest next trial parameters
 *
 * WHAT: Get parameters for next trial
 * WHY:  Support manual trial loop
 * HOW:  Use algorithm to suggest promising params
 *
 * @param ctx HPO context
 * @param params Output suggested parameters
 * @return Trial ID or negative on error
 */
int hpo_suggest(hpo_ctx_t* ctx, hpo_params_t** params);

/**
 * @brief Report trial result
 *
 * @param ctx HPO context
 * @param trial_id Trial ID from hpo_suggest
 * @param objective Objective value
 * @return 0 on success, negative on error
 */
int hpo_report(hpo_ctx_t* ctx, int trial_id, double objective);

/**
 * @brief Report intermediate value (for pruning)
 *
 * @param ctx HPO context
 * @param trial_id Trial ID
 * @param step Training step
 * @param value Intermediate objective value
 * @return 0 to continue, 1 if should prune
 */
int hpo_report_intermediate(
    hpo_ctx_t* ctx,
    int trial_id,
    uint32_t step,
    double value
);

/**
 * @brief Mark trial as pruned
 *
 * @param ctx HPO context
 * @param trial_id Trial ID
 * @return 0 on success, negative on error
 */
int hpo_prune_trial(hpo_ctx_t* ctx, int trial_id);

/**
 * @brief Mark trial as failed
 *
 * @param ctx HPO context
 * @param trial_id Trial ID
 * @param error_msg Error message
 * @return 0 on success, negative on error
 */
int hpo_fail_trial(hpo_ctx_t* ctx, int trial_id, const char* error_msg);

//=============================================================================
// Results API
//=============================================================================

/**
 * @brief Get best trial
 *
 * @param ctx HPO context
 * @param result Output best trial result
 * @return 0 on success, negative on error
 */
int hpo_get_best_trial(hpo_ctx_t* ctx, hpo_trial_result_t* result);

/**
 * @brief Get all trials
 *
 * @param ctx HPO context
 * @param results Output trial results array
 * @param num_results Output number of results
 * @return 0 on success, negative on error
 */
int hpo_get_all_trials(
    hpo_ctx_t* ctx,
    hpo_trial_result_t** results,
    uint32_t* num_results
);

/**
 * @brief Get parameter importance
 *
 * @param ctx HPO context
 * @param param_names Output parameter names
 * @param importance Output importance scores
 * @param num_params Output number of parameters
 * @return 0 on success, negative on error
 */
int hpo_get_importance(
    hpo_ctx_t* ctx,
    const char*** param_names,
    double** importance,
    uint32_t* num_params
);

//=============================================================================
// Integration API
//=============================================================================

/**
 * @brief Connect to distributed training
 *
 * WHAT: Enable parallel trial execution
 * WHY:  Speed up search with multiple workers
 *
 * @param ctx HPO context
 * @param dist_ctx Distributed context
 * @return 0 on success, negative on error
 */
int hpo_connect_distributed(hpo_ctx_t* ctx, void* dist_ctx);

/**
 * @brief Connect to training callbacks
 *
 * @param ctx HPO context
 * @param callbacks Training callbacks
 * @return 0 on success, negative on error
 */
int hpo_connect_callbacks(hpo_ctx_t* ctx, void* callbacks);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get HPO statistics
 *
 * @param ctx HPO context
 * @param stats Output statistics
 * @return 0 on success, negative on error
 */
int hpo_get_stats(const hpo_ctx_t* ctx, hpo_stats_t* stats);

/**
 * @brief Reset HPO statistics
 *
 * @param ctx HPO context
 */
void hpo_reset_stats(hpo_ctx_t* ctx);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get algorithm name
 */
const char* hpo_algorithm_name(hpo_algorithm_t alg);

/**
 * @brief Get parameter type name
 */
const char* hpo_param_type_name(hpo_param_type_t type);

/**
 * @brief Validate HPO configuration
 */
int hpo_validate_config(const hpo_config_t* config);

/**
 * @brief Free parameters structure
 */
void hpo_free_params(hpo_params_t* params);

/**
 * @brief Free trial result structure
 */
void hpo_free_trial_result(hpo_trial_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HYPERPARAM_OPT_H */
