//=============================================================================
// nimcp_fuzzy_inference.h - Fuzzy Inference Systems
//=============================================================================
/**
 * @file nimcp_fuzzy_inference.h
 * @brief Mamdani, Sugeno, and Tsukamoto fuzzy inference engines with
 *        7 defuzzification methods, rule management, and ANFIS learning
 *
 * WHAT: Three inference engine types with full rule management, batch
 *       inference, and ANFIS (Adaptive Neuro-Fuzzy Inference System)
 * WHY:  Map fuzzy inputs through IF-THEN rules to fuzzy/crisp outputs
 *       for risk assessment, market classification, decision-making
 * HOW:  Fuzzify inputs -> fire matching rules -> aggregate outputs ->
 *       defuzzify to crisp value (Mamdani) or compute weighted average (Sugeno)
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#ifndef NIMCP_FUZZY_INFERENCE_H
#define NIMCP_FUZZY_INFERENCE_H

#include "utils/fuzzy/nimcp_fuzzy_types.h"
#include "utils/fuzzy/nimcp_fuzzy_operators.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define BIO_MODULE_FUZZY_INFERENCE      0x0282

/** Maximum number of rules in a fuzzy inference system */
#define FUZZY_MAX_RULES                 256

/** Maximum antecedents per rule */
#define FUZZY_MAX_ANTECEDENTS           16

/** Maximum input variables */
#define FUZZY_MAX_INPUTS                32

/** Maximum output variables */
#define FUZZY_MAX_OUTPUTS               8

/** Maximum Sugeno polynomial coefficients */
#define FUZZY_SUGENO_MAX_COEFFS         (FUZZY_MAX_INPUTS + 1)

//=============================================================================
// Error Codes
//=============================================================================

#define FUZZY_INFERENCE_ERROR_BASE      28100

#define FUZZY_INF_ERR_OK                0
#define FUZZY_INF_ERR_NULL              (FUZZY_INFERENCE_ERROR_BASE + 1)
#define FUZZY_INF_ERR_MAX_RULES         (FUZZY_INFERENCE_ERROR_BASE + 2)
#define FUZZY_INF_ERR_MAX_INPUTS        (FUZZY_INFERENCE_ERROR_BASE + 3)
#define FUZZY_INF_ERR_MAX_OUTPUTS       (FUZZY_INFERENCE_ERROR_BASE + 4)
#define FUZZY_INF_ERR_INVALID_FIS       (FUZZY_INFERENCE_ERROR_BASE + 5)
#define FUZZY_INF_ERR_INVALID_RULE      (FUZZY_INFERENCE_ERROR_BASE + 6)
#define FUZZY_INF_ERR_NO_RULES          (FUZZY_INFERENCE_ERROR_BASE + 7)
#define FUZZY_INF_ERR_INPUT_MISMATCH    (FUZZY_INFERENCE_ERROR_BASE + 8)
#define FUZZY_INF_ERR_ALLOC             (FUZZY_INFERENCE_ERROR_BASE + 9)
#define FUZZY_INF_ERR_CONVERGENCE       (FUZZY_INFERENCE_ERROR_BASE + 10)

//=============================================================================
// Inference System Types
//=============================================================================

typedef enum {
    FUZZY_FIS_MAMDANI,              /**< Mamdani: fuzzy output aggregation + defuzzification */
    FUZZY_FIS_SUGENO,               /**< Sugeno: weighted average of polynomial outputs */
    FUZZY_FIS_TSUKAMOTO,            /**< Tsukamoto: monotonic MFs -> inverse -> weighted avg */
    FUZZY_FIS_TYPE_COUNT
} fuzzy_fis_type_t;

//=============================================================================
// Defuzzification Methods (for Mamdani)
//=============================================================================

typedef enum {
    FUZZY_DEFUZZ_CENTROID,          /**< Center of gravity (area) */
    FUZZY_DEFUZZ_BISECTOR,          /**< Bisector of area */
    FUZZY_DEFUZZ_MOM,               /**< Mean of maximum */
    FUZZY_DEFUZZ_SOM,               /**< Smallest of maximum */
    FUZZY_DEFUZZ_LOM,               /**< Largest of maximum */
    FUZZY_DEFUZZ_WEIGHTED_AVG,      /**< Weighted average of term centroids */
    FUZZY_DEFUZZ_WEIGHTED_SUM,      /**< Weighted sum (unnormalized) */
    FUZZY_DEFUZZ_TYPE_COUNT
} fuzzy_defuzz_type_t;

//=============================================================================
// Rule Structures
//=============================================================================

/**
 * @brief Single antecedent clause: "variable[var_index] IS term[term_index]"
 */
typedef struct {
    uint32_t var_index;             /**< Index into input variable array */
    uint32_t term_index;            /**< Index into that variable's terms */
    bool negated;                   /**< If true, apply NOT to membership */
    fuzzy_hedge_t hedge;            /**< Optional hedge modifier */
} fuzzy_antecedent_t;

/**
 * @brief Rule consequent for Mamdani: "output IS term"
 */
typedef struct {
    uint32_t var_index;             /**< Index into output variable array */
    uint32_t term_index;            /**< Index into that variable's terms */
} fuzzy_consequent_mamdani_t;

/**
 * @brief Rule consequent for Sugeno: output = c0 + c1*x1 + c2*x2 + ...
 */
typedef struct {
    float coefficients[FUZZY_SUGENO_MAX_COEFFS];  /**< Polynomial coefficients */
    uint32_t num_coeffs;            /**< Number of coefficients */
} fuzzy_consequent_sugeno_t;

/**
 * @brief Fuzzy inference rule
 */
typedef struct {
    fuzzy_antecedent_t antecedents[FUZZY_MAX_ANTECEDENTS];
    uint32_t num_antecedents;
    fuzzy_tnorm_type_t connector;   /**< AND/OR connector between antecedents */
    float weight;                   /**< Rule weight [0,1] (default 1.0) */
    bool use_or;                    /**< True = OR (t-conorm), false = AND (t-norm) */

    /* Output: union of Mamdani and Sugeno consequents */
    fuzzy_consequent_mamdani_t mamdani;
    fuzzy_consequent_sugeno_t sugeno;
} fuzzy_rule_t;

//=============================================================================
// Inference Result
//=============================================================================

typedef struct {
    float crisp_outputs[FUZZY_MAX_OUTPUTS];     /**< Defuzzified output values */
    uint32_t num_outputs;
    float rule_firing_strengths[FUZZY_MAX_RULES]; /**< Per-rule activation */
    uint32_t num_rules_fired;                     /**< Rules with strength > 0 */
    float total_firing_strength;                  /**< Sum of all strengths */
} fuzzy_inference_result_t;

//=============================================================================
// Inference System Configuration
//=============================================================================

typedef struct {
    fuzzy_fis_type_t fis_type;
    fuzzy_defuzz_type_t defuzz_method;
    fuzzy_tnorm_type_t and_method;
    fuzzy_tconorm_type_t or_method;
    fuzzy_implication_type_t implication;
    fuzzy_aggregation_type_t aggregation;
    uint32_t defuzz_resolution;
    bool enable_anfis;
    float anfis_learning_rate;
    uint32_t anfis_max_epochs;
    float anfis_convergence_tol;
    float inflammation_sensitivity;
    float fatigue_sensitivity;
} fuzzy_inference_config_t;

typedef struct {
    uint64_t inferences_run;
    uint64_t rules_evaluated;
    uint64_t defuzzifications;
    uint64_t anfis_training_steps;
    float avg_inference_time_us;
    float anfis_last_error;
} fuzzy_inference_stats_t;

//=============================================================================
// Opaque Handle
//=============================================================================

typedef struct fuzzy_inference_engine fuzzy_inference_engine_t;

//=============================================================================
// Lifecycle
//=============================================================================

fuzzy_inference_engine_t* fuzzy_inference_create(void);
fuzzy_inference_engine_t* fuzzy_inference_create_custom(const fuzzy_inference_config_t* config);
void fuzzy_inference_destroy(fuzzy_inference_engine_t* engine);
fuzzy_inference_config_t fuzzy_inference_default_config(void);

//=============================================================================
// Variable Registration
//=============================================================================

int fuzzy_inference_add_input(fuzzy_inference_engine_t* engine,
                               const fuzzy_variable_t* var);
int fuzzy_inference_add_output(fuzzy_inference_engine_t* engine,
                                const fuzzy_variable_t* var);

//=============================================================================
// Rule Management
//=============================================================================

int fuzzy_inference_add_rule(fuzzy_inference_engine_t* engine,
                              const fuzzy_rule_t* rule);
int fuzzy_inference_clear_rules(fuzzy_inference_engine_t* engine);
int fuzzy_inference_get_rule_count(const fuzzy_inference_engine_t* engine);

/** Convenience: build a simple 2-antecedent Mamdani rule */
fuzzy_rule_t fuzzy_rule_mamdani(uint32_t in_var1, uint32_t in_term1,
                                 uint32_t in_var2, uint32_t in_term2,
                                 uint32_t out_var, uint32_t out_term,
                                 float weight);

/** Convenience: build a simple 2-input Sugeno rule */
fuzzy_rule_t fuzzy_rule_sugeno(uint32_t in_var1, uint32_t in_term1,
                                uint32_t in_var2, uint32_t in_term2,
                                const float* coefficients, uint32_t num_coeffs,
                                float weight);

//=============================================================================
// Inference Evaluation
//=============================================================================

/**
 * @brief Evaluate the FIS with given crisp inputs
 * @param engine      Inference engine
 * @param inputs      Array of crisp input values (one per input variable)
 * @param num_inputs  Number of inputs
 * @param out_result  Output inference result
 * @return 0 on success
 */
int fuzzy_inference_evaluate(fuzzy_inference_engine_t* engine,
                              const float* inputs, uint32_t num_inputs,
                              fuzzy_inference_result_t* out_result);

/**
 * @brief Batch inference: evaluate multiple input vectors
 * @param engine       Inference engine
 * @param inputs       2D array: inputs[sample][variable]
 * @param num_samples  Number of samples
 * @param num_inputs   Number of input variables per sample
 * @param out_results  Output array of results (pre-allocated, size num_samples)
 * @return 0 on success
 */
int fuzzy_inference_evaluate_batch(fuzzy_inference_engine_t* engine,
                                    const float* inputs, uint32_t num_samples,
                                    uint32_t num_inputs,
                                    fuzzy_inference_result_t* out_results);

//=============================================================================
// Defuzzification (standalone)
//=============================================================================

/**
 * @brief Defuzzify a discrete fuzzy set
 * @param set    Discretized output MF (aggregated)
 * @param method Defuzzification method
 * @return Crisp output value
 */
float fuzzy_defuzzify(const fuzzy_discrete_set_t* set, fuzzy_defuzz_type_t method);

//=============================================================================
// ANFIS (Adaptive Neuro-Fuzzy Inference System)
//=============================================================================

/**
 * @brief Train ANFIS to learn Sugeno consequent parameters from data
 * @param engine       Inference engine (must be SUGENO type with rules defined)
 * @param input_data   Training inputs [num_samples x num_inputs]
 * @param target_data  Target outputs [num_samples x num_outputs]
 * @param num_samples  Number of training samples
 * @param num_epochs   Max training epochs
 * @param out_final_error Final training error
 * @return 0 on success, error code on failure
 */
int fuzzy_anfis_train(fuzzy_inference_engine_t* engine,
                       const float* input_data, const float* target_data,
                       uint32_t num_samples, uint32_t num_epochs,
                       float* out_final_error);

//=============================================================================
// Modulation & Statistics
//=============================================================================

int fuzzy_inference_set_inflammation(fuzzy_inference_engine_t* engine, float level);
int fuzzy_inference_set_fatigue(fuzzy_inference_engine_t* engine, float level);
int fuzzy_inference_get_stats(const fuzzy_inference_engine_t* engine,
                               fuzzy_inference_stats_t* stats);
void fuzzy_inference_reset_stats(fuzzy_inference_engine_t* engine);
const char* fuzzy_inference_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FUZZY_INFERENCE_H */
