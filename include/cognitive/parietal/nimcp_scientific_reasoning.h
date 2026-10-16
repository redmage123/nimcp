/**
 * @file nimcp_scientific_reasoning.h
 * @brief Scientific reasoning and hypothesis testing for parietal lobe
 *
 * Implements scientific reasoning capabilities:
 * - Dimensional analysis (SI units, Buckingham Pi theorem)
 * - Hypothesis generation and Bayesian updating
 * - Causal inference (basic PC algorithm)
 * - Experimental design support
 *
 * BIOLOGICAL BASIS:
 * Scientific reasoning emerges from integration of mathematical intuition
 * in the intraparietal sulcus with prefrontal executive functions.
 * The parietal cortex handles quantitative aspects while hypothesis
 * evaluation involves broader cortical networks.
 *
 * USAGE:
 * ```c
 * scientific_reasoning_t* sr = scientific_reasoning_create();
 *
 * // Dimensional analysis
 * physical_dimension_t force = {1, 1, -2, 0, 0, 0, 0}; // kg*m/s^2
 * bool valid = scientific_check_dimensions(sr, expr, force);
 *
 * // Hypothesis testing
 * hypothesis_t h = scientific_create_hypothesis(sr, "Linear relation", 0.5);
 * scientific_update_hypothesis(sr, &h, data, num_samples);
 *
 * scientific_reasoning_destroy(sr);
 * ```
 */

#ifndef NIMCP_SCIENTIFIC_REASONING_H
#define NIMCP_SCIENTIFIC_REASONING_H

#include "utils/validation/nimcp_common.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

/** Maximum number of hypotheses to track */
#define SCIENTIFIC_MAX_HYPOTHESES       32

/** Maximum number of variables for causal inference */
#define SCIENTIFIC_MAX_CAUSAL_VARS      16

/** Maximum description length */
#define SCIENTIFIC_MAX_DESCRIPTION      256

/** Bio-async module ID for scientific reasoning */
#define BIO_MODULE_SCIENTIFIC           0x0384

/* ============================================================================
 * TYPES
 * ============================================================================ */

/** Opaque handle for scientific reasoning processor */
typedef struct scientific_reasoning scientific_reasoning_t;

/**
 * @brief Physical dimension representation (SI base units)
 *
 * Represents dimensional formula as exponents of base units:
 * [L]^length [M]^mass [T]^time [I]^current [Θ]^temperature [N]^amount [J]^luminosity
 */
typedef struct {
    int8_t length;          /**< Meter (m) exponent */
    int8_t mass;            /**< Kilogram (kg) exponent */
    int8_t time;            /**< Second (s) exponent */
    int8_t current;         /**< Ampere (A) exponent */
    int8_t temperature;     /**< Kelvin (K) exponent */
    int8_t amount;          /**< Mole (mol) exponent */
    int8_t luminosity;      /**< Candela (cd) exponent */
} physical_dimension_t;

/**
 * @brief Physical quantity with value and dimensions
 */
typedef struct {
    float value;                    /**< Numerical value */
    physical_dimension_t dimension; /**< Dimensional formula */
    char symbol[32];                /**< Symbol (e.g., "F", "v") */
} physical_quantity_t;

/**
 * @brief Hypothesis structure for Bayesian updating
 */
#ifndef NIMCP_HYPOTHESIS_T_DEFINED
#define NIMCP_HYPOTHESIS_T_DEFINED
typedef struct {
    uint32_t id;                                /**< Unique hypothesis ID */
    char description[SCIENTIFIC_MAX_DESCRIPTION]; /**< Human-readable description */
    float prior;                                /**< Prior probability */
    float posterior;                            /**< Posterior probability */
    float likelihood;                           /**< Current likelihood */
    float evidence_strength;                    /**< Cumulative evidence strength */
    uint32_t observations;                      /**< Number of observations */
    float* parameters;                          /**< Hypothesis parameters */
    uint32_t num_parameters;                    /**< Number of parameters */
    bool active;                                /**< Is hypothesis still viable? */
} hypothesis_t;
#endif

/**
 * @brief Data sample for hypothesis testing
 */
typedef struct {
    float* values;              /**< Sample values */
    uint32_t num_values;        /**< Number of values */
    float weight;               /**< Sample weight (default 1.0) */
    uint64_t timestamp;         /**< When sample was collected */
} data_sample_t;

/**
 * @brief Causal relationship
 */
typedef struct {
    uint32_t cause_id;          /**< Cause variable ID */
    uint32_t effect_id;         /**< Effect variable ID */
    float strength;             /**< Relationship strength [-1, 1] */
    float confidence;           /**< Confidence in relationship [0, 1] */
    bool is_direct;             /**< Direct vs indirect causation */
} causal_relation_t;

/**
 * @brief Causal graph (DAG)
 */
typedef struct {
    char* variable_names[SCIENTIFIC_MAX_CAUSAL_VARS]; /**< Variable names */
    uint32_t num_variables;                           /**< Number of variables */
    causal_relation_t* relations;                     /**< Causal relations */
    uint32_t num_relations;                           /**< Number of relations */
    float** adjacency;                                /**< Adjacency matrix */
} causal_graph_t;

/**
 * @brief Experimental design
 */
typedef struct {
    char name[64];                  /**< Experiment name */
    uint32_t* treatment_vars;       /**< Treatment variable IDs */
    uint32_t num_treatments;        /**< Number of treatment variables */
    uint32_t* control_vars;         /**< Control variable IDs */
    uint32_t num_controls;          /**< Number of control variables */
    uint32_t* outcome_vars;         /**< Outcome variable IDs */
    uint32_t num_outcomes;          /**< Number of outcome variables */
    float sample_size;              /**< Recommended sample size */
    float power;                    /**< Statistical power */
} experimental_design_t;

/**
 * @brief Scientific reasoning configuration
 */
typedef struct {
    float hypothesis_prior_default;  /**< Default prior probability (0.5) */
    float evidence_threshold;        /**< Min evidence for hypothesis update (0.1) */
    float significance_level;        /**< Statistical significance level (0.05) */
    uint32_t max_hypotheses;         /**< Maximum tracked hypotheses (32) */
    bool enable_causal_inference;    /**< Enable causal analysis (true) */
    bool enable_bio_async;           /**< Enable bio-async messaging (false) */

    /* Modulation */
    float inflammation_sensitivity;  /**< Inflammation effect (0-1) */
    float sleep_deprivation_effect;  /**< Sleep deprivation effect (0-1) */
} scientific_config_t;

/**
 * @brief Scientific reasoning statistics
 */
typedef struct {
    uint64_t hypotheses_generated;   /**< Total hypotheses created */
    uint64_t hypotheses_rejected;    /**< Hypotheses rejected */
    uint64_t dimensional_analyses;   /**< Dimensional analysis count */
    uint64_t causal_inferences;      /**< Causal inference count */
    float avg_posterior;             /**< Average hypothesis posterior */
    uint32_t active_hypotheses;      /**< Currently active hypotheses */
} scientific_stats_t;

/* ============================================================================
 * LIFECYCLE API
 * ============================================================================ */

/**
 * @brief Create scientific reasoning processor with default configuration
 * @return Handle or NULL on error
 */
scientific_reasoning_t* scientific_reasoning_create(void);

/**
 * @brief Create scientific reasoning processor with custom configuration
 * @param config Configuration (NULL for defaults)
 * @return Handle or NULL on error
 */
scientific_reasoning_t* scientific_reasoning_create_custom(
    const scientific_config_t* config
);

/**
 * @brief Destroy scientific reasoning processor
 * @param sr Handle (NULL safe)
 */
void scientific_reasoning_destroy(scientific_reasoning_t* sr);

/**
 * @brief Get default configuration
 * @return Default configuration struct
 */
scientific_config_t scientific_default_config(void);

/**
 * @brief Validate configuration
 * @param config Configuration to validate
 * @return true if valid
 */
bool scientific_validate_config(const scientific_config_t* config);

/* ============================================================================
 * DIMENSIONAL ANALYSIS API
 * ============================================================================ */

/**
 * @brief Create dimensionless quantity
 * @return Dimensionless physical_dimension_t
 */
physical_dimension_t scientific_dimensionless(void);

/**
 * @brief Create physical quantity
 *
 * @param value Numerical value
 * @param dimension Dimensional formula
 * @param symbol Symbol (can be NULL)
 * @return Physical quantity
 */
physical_quantity_t scientific_create_quantity(
    float value,
    physical_dimension_t dimension,
    const char* symbol
);

/**
 * @brief Multiply dimensions (for multiplication of quantities)
 *
 * @param a First dimension
 * @param b Second dimension
 * @return Product dimension
 */
physical_dimension_t scientific_multiply_dimensions(
    physical_dimension_t a,
    physical_dimension_t b
);

/**
 * @brief Divide dimensions (for division of quantities)
 *
 * @param a Numerator dimension
 * @param b Denominator dimension
 * @return Quotient dimension
 */
physical_dimension_t scientific_divide_dimensions(
    physical_dimension_t a,
    physical_dimension_t b
);

/**
 * @brief Raise dimension to power
 *
 * @param d Dimension
 * @param power Exponent
 * @return Dimension raised to power
 */
physical_dimension_t scientific_power_dimension(
    physical_dimension_t d,
    int8_t power
);

/**
 * @brief Check if two dimensions are equal
 *
 * @param a First dimension
 * @param b Second dimension
 * @return true if equal
 */
bool scientific_dimensions_equal(
    physical_dimension_t a,
    physical_dimension_t b
);

/**
 * @brief Check if dimension is dimensionless
 *
 * @param d Dimension to check
 * @return true if dimensionless
 */
bool scientific_is_dimensionless(physical_dimension_t d);

/**
 * @brief Get dimension string representation
 *
 * @param d Dimension
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Pointer to buffer
 */
const char* scientific_dimension_to_string(
    physical_dimension_t d,
    char* buffer,
    uint32_t buffer_size
);

/**
 * @brief Apply Buckingham Pi theorem
 *
 * Computes dimensionless groups from a set of physical quantities.
 *
 * @param sr Scientific reasoning handle
 * @param quantities Array of physical quantities
 * @param num_quantities Number of quantities
 * @param pi_groups Output array of dimensionless groups (as exponent arrays)
 * @param max_groups Maximum groups to compute
 * @return Number of dimensionless groups found
 */
uint32_t scientific_buckingham_pi(
    scientific_reasoning_t* sr,
    const physical_quantity_t* quantities,
    uint32_t num_quantities,
    float** pi_groups,
    uint32_t max_groups
);

/* ============================================================================
 * COMMON PHYSICAL DIMENSIONS
 * ============================================================================ */

/** Length [L] */
#define DIM_LENGTH      ((physical_dimension_t){1, 0, 0, 0, 0, 0, 0})
/** Mass [M] */
#define DIM_MASS        ((physical_dimension_t){0, 1, 0, 0, 0, 0, 0})
/** Time [T] */
#define DIM_TIME        ((physical_dimension_t){0, 0, 1, 0, 0, 0, 0})
/** Velocity [L/T] */
#define DIM_VELOCITY    ((physical_dimension_t){1, 0, -1, 0, 0, 0, 0})
/** Acceleration [L/T^2] */
#define DIM_ACCEL       ((physical_dimension_t){1, 0, -2, 0, 0, 0, 0})
/** Force [M*L/T^2] */
#define DIM_FORCE       ((physical_dimension_t){1, 1, -2, 0, 0, 0, 0})
/** Energy [M*L^2/T^2] */
#define DIM_ENERGY      ((physical_dimension_t){2, 1, -2, 0, 0, 0, 0})
/** Power [M*L^2/T^3] */
#define DIM_POWER       ((physical_dimension_t){2, 1, -3, 0, 0, 0, 0})
/** Pressure [M/(L*T^2)] */
#define DIM_PRESSURE    ((physical_dimension_t){-1, 1, -2, 0, 0, 0, 0})

/* ============================================================================
 * HYPOTHESIS API
 * ============================================================================ */

/**
 * @brief Create a new hypothesis
 *
 * @param sr Scientific reasoning handle
 * @param description Human-readable description
 * @param prior Prior probability [0, 1]
 * @return Hypothesis handle
 */
hypothesis_t scientific_create_hypothesis(
    scientific_reasoning_t* sr,
    const char* description,
    float prior
);

/**
 * @brief Update hypothesis with new evidence (Bayesian update)
 *
 * @param sr Scientific reasoning handle
 * @param hypothesis Hypothesis to update
 * @param samples Data samples
 * @param num_samples Number of samples
 * @return Updated posterior probability
 */
float scientific_update_hypothesis(
    scientific_reasoning_t* sr,
    hypothesis_t* hypothesis,
    const data_sample_t* samples,
    uint32_t num_samples
);

/**
 * @brief Compare two hypotheses
 *
 * @param sr Scientific reasoning handle
 * @param h1 First hypothesis
 * @param h2 Second hypothesis
 * @return Bayes factor (>1 favors h1, <1 favors h2)
 */
float scientific_compare_hypotheses(
    scientific_reasoning_t* sr,
    const hypothesis_t* h1,
    const hypothesis_t* h2
);

/**
 * @brief Get most likely hypothesis from set
 *
 * @param sr Scientific reasoning handle
 * @param hypotheses Array of hypotheses
 * @param num_hypotheses Number of hypotheses
 * @return Index of most likely hypothesis
 */
uint32_t scientific_best_hypothesis(
    scientific_reasoning_t* sr,
    const hypothesis_t* hypotheses,
    uint32_t num_hypotheses
);

/**
 * @brief Reject hypothesis if evidence is strong enough
 *
 * @param sr Scientific reasoning handle
 * @param hypothesis Hypothesis to test
 * @return true if hypothesis should be rejected
 */
bool scientific_reject_hypothesis(
    scientific_reasoning_t* sr,
    hypothesis_t* hypothesis
);

/* ============================================================================
 * CAUSAL INFERENCE API
 * ============================================================================ */

/**
 * @brief Create causal graph
 *
 * @param sr Scientific reasoning handle
 * @param variable_names Array of variable names
 * @param num_variables Number of variables
 * @return Causal graph or NULL on error
 */
causal_graph_t* scientific_create_causal_graph(
    scientific_reasoning_t* sr,
    const char** variable_names,
    uint32_t num_variables
);

/**
 * @brief Destroy causal graph
 *
 * @param graph Causal graph (NULL safe)
 */
void scientific_destroy_causal_graph(causal_graph_t* graph);

/**
 * @brief Learn causal structure from data (basic PC algorithm)
 *
 * @param sr Scientific reasoning handle
 * @param graph Causal graph to populate
 * @param data Data matrix [num_samples x num_variables]
 * @param num_samples Number of samples
 * @return 0 on success
 */
int scientific_learn_causal_structure(
    scientific_reasoning_t* sr,
    causal_graph_t* graph,
    const float** data,
    uint32_t num_samples
);

/**
 * @brief Add causal relation to graph
 *
 * @param graph Causal graph
 * @param cause_id Cause variable ID
 * @param effect_id Effect variable ID
 * @param strength Relationship strength
 * @return 0 on success
 */
int scientific_add_causal_relation(
    causal_graph_t* graph,
    uint32_t cause_id,
    uint32_t effect_id,
    float strength
);

/**
 * @brief Estimate causal effect (do-calculus)
 *
 * Estimates P(Y | do(X = x)) using adjustment formula.
 *
 * @param sr Scientific reasoning handle
 * @param graph Causal graph
 * @param treatment_id Treatment variable ID
 * @param outcome_id Outcome variable ID
 * @param treatment_value Treatment value
 * @return Estimated causal effect
 */
float scientific_estimate_causal_effect(
    scientific_reasoning_t* sr,
    const causal_graph_t* graph,
    uint32_t treatment_id,
    uint32_t outcome_id,
    float treatment_value
);

/**
 * @brief Check if path is blocked by conditioning
 *
 * @param graph Causal graph
 * @param from_id Start variable
 * @param to_id End variable
 * @param conditioning_set IDs of conditioning variables
 * @param num_conditioning Number of conditioning variables
 * @return true if path is blocked
 */
bool scientific_is_path_blocked(
    const causal_graph_t* graph,
    uint32_t from_id,
    uint32_t to_id,
    const uint32_t* conditioning_set,
    uint32_t num_conditioning
);

/* ============================================================================
 * EXPERIMENTAL DESIGN API
 * ============================================================================ */

/**
 * @brief Suggest experimental design
 *
 * @param sr Scientific reasoning handle
 * @param graph Causal graph
 * @param target_effect_id Target effect variable
 * @param design Output experimental design
 * @return 0 on success
 */
int scientific_suggest_experiment(
    scientific_reasoning_t* sr,
    const causal_graph_t* graph,
    uint32_t target_effect_id,
    experimental_design_t* design
);

/**
 * @brief Calculate required sample size
 *
 * @param sr Scientific reasoning handle
 * @param effect_size Expected effect size
 * @param power Desired power (e.g., 0.8)
 * @param alpha Significance level (e.g., 0.05)
 * @return Required sample size
 */
uint32_t scientific_required_sample_size(
    scientific_reasoning_t* sr,
    float effect_size,
    float power,
    float alpha
);

/* ============================================================================
 * MODULATION API
 * ============================================================================ */

/**
 * @brief Set inflammation level
 *
 * @param sr Scientific reasoning handle
 * @param level Inflammation level [0,1]
 * @return 0 on success
 */
int scientific_set_inflammation(
    scientific_reasoning_t* sr,
    float level
);

/**
 * @brief Set sleep deprivation level
 *
 * @param sr Scientific reasoning handle
 * @param level Sleep deprivation level [0,1]
 * @return 0 on success
 */
int scientific_set_sleep_deprivation(
    scientific_reasoning_t* sr,
    float level
);

/* ============================================================================
 * STATISTICS API
 * ============================================================================ */

/**
 * @brief Get statistics
 *
 * @param sr Scientific reasoning handle
 * @param stats Output statistics
 * @return 0 on success
 */
int scientific_get_stats(
    scientific_reasoning_t* sr,
    scientific_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param sr Scientific reasoning handle
 */
void scientific_reset_stats(scientific_reasoning_t* sr);

/**
 * @brief Get last error message
 * @return Thread-local error message
 */
const char* scientific_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SCIENTIFIC_REASONING_H */
