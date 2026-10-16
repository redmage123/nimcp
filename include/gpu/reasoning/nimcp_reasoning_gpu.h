/**
 * @file nimcp_reasoning_gpu.h
 * @brief GPU-accelerated Reasoning and Inference Kernels
 *
 * WHAT: CUDA kernels for symbolic and neural-symbolic reasoning
 * WHY:  GPU acceleration for parallel rule application and inference
 * HOW:  Custom kernels for logic, constraint satisfaction, analogical reasoning
 *
 * ARCHITECTURE:
 * - Propositional Logic: Fast parallel truth value propagation
 * - Rule Engine: Parallel rule matching and firing
 * - Constraint Satisfaction: Parallel constraint propagation
 * - Analogical Reasoning: Structure mapping and similarity
 * - Causal Reasoning: Intervention and counterfactual computation
 *
 * All functions support both CUDA GPU and CPU fallback implementations.
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_REASONING_GPU_H
#define NIMCP_REASONING_GPU_H

#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "common/nimcp_export.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Logic Types and Enumerations
//=============================================================================

/**
 * @brief Logical operators
 */
typedef enum {
    NIMCP_LOGIC_AND = 0,
    NIMCP_LOGIC_OR = 1,
    NIMCP_LOGIC_NOT = 2,
    NIMCP_LOGIC_XOR = 3,
    NIMCP_LOGIC_IMPLIES = 4,
    NIMCP_LOGIC_IFF = 5,
    NIMCP_LOGIC_NAND = 6,
    NIMCP_LOGIC_NOR = 7
} nimcp_logic_op_t;

/**
 * @brief Rule types for inference
 */
typedef enum {
    NIMCP_RULE_MODUS_PONENS = 0,
    NIMCP_RULE_MODUS_TOLLENS = 1,
    NIMCP_RULE_CHAIN = 2,
    NIMCP_RULE_RESOLUTION = 3,
    NIMCP_RULE_UNIT_PROPAGATION = 4
} nimcp_rule_type_t;

//=============================================================================
// Propositional Logic Parameters
//=============================================================================

/**
 * @brief Propositional logic engine parameters
 */
typedef struct {
    float truth_threshold;        /**< Threshold for true (default 0.5) */
    float uncertainty_weight;     /**< Weight for uncertain values */
    int max_iterations;           /**< Max propagation iterations */
    float convergence_eps;        /**< Convergence epsilon */
    bool use_fuzzy;               /**< Use fuzzy logic (continuous) */
    float fuzzy_and_type;         /**< Fuzzy AND: 0=min, 1=product */
    float fuzzy_or_type;          /**< Fuzzy OR: 0=max, 1=probabilistic */
} nimcp_gpu_logic_params_t;

/**
 * @brief Logic formula representation
 */
typedef struct {
    nimcp_gpu_tensor_t* variables;    /**< Variable truth values */
    nimcp_gpu_tensor_t* operators;    /**< Operator indices */
    nimcp_gpu_tensor_t* operand1;     /**< First operand indices */
    nimcp_gpu_tensor_t* operand2;     /**< Second operand indices (for binary ops) */
    nimcp_gpu_tensor_t* output_idx;   /**< Output variable indices */
    size_t n_variables;               /**< Number of variables */
    size_t n_clauses;                 /**< Number of clauses */
} nimcp_gpu_logic_formula_t;

//=============================================================================
// Rule Engine Parameters
//=============================================================================

/**
 * @brief Rule engine parameters
 */
typedef struct {
    float activation_threshold;   /**< Rule activation threshold */
    float learning_rate;          /**< Rule strength learning rate */
    int max_chain_depth;          /**< Maximum inference chain depth */
    float decay_factor;           /**< Evidence decay per step */
    bool parallel_firing;         /**< Allow parallel rule firing */
    float conflict_resolution;    /**< Conflict resolution strategy */
} nimcp_gpu_rule_params_t;

/**
 * @brief Rule representation
 */
typedef struct {
    nimcp_gpu_tensor_t* antecedents;  /**< Rule antecedent patterns */
    nimcp_gpu_tensor_t* consequents;  /**< Rule consequent patterns */
    nimcp_gpu_tensor_t* strengths;    /**< Rule strengths/weights */
    nimcp_gpu_tensor_t* conditions;   /**< Additional conditions */
    size_t n_rules;                   /**< Number of rules */
    size_t pattern_dim;               /**< Pattern dimensionality */
} nimcp_gpu_rule_base_t;

/**
 * @brief Working memory for inference
 */
typedef struct {
    nimcp_gpu_tensor_t* facts;        /**< Current facts */
    nimcp_gpu_tensor_t* activations;  /**< Fact activation levels */
    nimcp_gpu_tensor_t* sources;      /**< Fact sources (for explanation) */
    nimcp_gpu_tensor_t* timestamps;   /**< Fact timestamps */
    size_t n_facts;                   /**< Number of facts */
    size_t fact_dim;                  /**< Fact dimensionality */
} nimcp_gpu_working_memory_t;

//=============================================================================
// Constraint Satisfaction Parameters
//=============================================================================

/**
 * @brief Constraint satisfaction parameters
 */
typedef struct {
    int max_iterations;           /**< Max propagation iterations */
    float arc_consistency_level;  /**< Arc consistency level (1-3) */
    float constraint_weight;      /**< Default constraint weight */
    bool use_backtracking;        /**< Enable backtracking */
    float heuristic_strength;     /**< Variable/value ordering heuristic */
    float restart_threshold;      /**< Restart threshold */
} nimcp_gpu_csp_params_t;

/**
 * @brief CSP state
 */
typedef struct {
    nimcp_gpu_tensor_t* domains;      /**< Variable domains */
    nimcp_gpu_tensor_t* assignments;  /**< Current assignments */
    nimcp_gpu_tensor_t* constraints;  /**< Constraint matrix */
    nimcp_gpu_tensor_t* violated;     /**< Violation indicators */
    nimcp_gpu_tensor_t* pruned;       /**< Pruned domain values */
    size_t n_variables;               /**< Number of variables */
    size_t domain_size;               /**< Maximum domain size */
    size_t n_constraints;             /**< Number of constraints */
} nimcp_gpu_csp_state_t;

//=============================================================================
// Analogical Reasoning Parameters
//=============================================================================

/**
 * @brief Analogical reasoning parameters
 */
typedef struct {
    float structural_weight;      /**< Weight for structural similarity */
    float semantic_weight;        /**< Weight for semantic similarity */
    float relational_weight;      /**< Weight for relational matches */
    int max_mapping_size;         /**< Maximum mapping size */
    float consistency_threshold;  /**< Consistency threshold */
    float novelty_bonus;          /**< Bonus for novel inferences */
} nimcp_gpu_analogy_params_t;

/**
 * @brief Structure for analogical mapping
 */
typedef struct {
    nimcp_gpu_tensor_t* source_structure;  /**< Source domain structure */
    nimcp_gpu_tensor_t* target_structure;  /**< Target domain structure */
    nimcp_gpu_tensor_t* source_features;   /**< Source entity features */
    nimcp_gpu_tensor_t* target_features;   /**< Target entity features */
    nimcp_gpu_tensor_t* mapping;           /**< Current mapping */
    nimcp_gpu_tensor_t* mapping_scores;    /**< Mapping quality scores */
    size_t source_size;                    /**< Number of source entities */
    size_t target_size;                    /**< Number of target entities */
} nimcp_gpu_analogy_state_t;

//=============================================================================
// Causal Reasoning Parameters
//=============================================================================

/**
 * @brief Causal reasoning parameters
 */
typedef struct {
    float intervention_strength;  /**< Intervention effect strength */
    float confound_threshold;     /**< Confounder detection threshold */
    int max_path_length;          /**< Maximum causal path length */
    float noise_level;            /**< Causal noise level */
    bool use_backdoor;            /**< Use backdoor adjustment */
    float counterfactual_eps;     /**< Counterfactual epsilon */
} nimcp_gpu_causal_params_t;

/**
 * @brief Causal graph state
 */
typedef struct {
    nimcp_gpu_tensor_t* adjacency;        /**< Causal adjacency matrix */
    nimcp_gpu_tensor_t* edge_weights;     /**< Causal edge strengths */
    nimcp_gpu_tensor_t* node_values;      /**< Current node values */
    nimcp_gpu_tensor_t* interventions;    /**< Intervention indicators */
    nimcp_gpu_tensor_t* confounders;      /**< Confounder indicators */
    size_t n_nodes;                       /**< Number of causal nodes */
} nimcp_gpu_causal_state_t;

//=============================================================================
// Default Parameter Functions
//=============================================================================

NIMCP_EXPORT nimcp_gpu_logic_params_t nimcp_gpu_logic_params_default(void);
NIMCP_EXPORT nimcp_gpu_rule_params_t nimcp_gpu_rule_params_default(void);
NIMCP_EXPORT nimcp_gpu_csp_params_t nimcp_gpu_csp_params_default(void);
NIMCP_EXPORT nimcp_gpu_analogy_params_t nimcp_gpu_analogy_params_default(void);
NIMCP_EXPORT nimcp_gpu_causal_params_t nimcp_gpu_causal_params_default(void);

//=============================================================================
// Propositional Logic Functions
//=============================================================================

/**
 * @brief Evaluate logical formula on GPU
 */
NIMCP_EXPORT bool nimcp_gpu_logic_evaluate(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_logic_formula_t* formula,
    nimcp_gpu_tensor_t* results,
    const nimcp_gpu_logic_params_t* params);

/**
 * @brief Propagate truth values through formula
 */
NIMCP_EXPORT bool nimcp_gpu_logic_propagate(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_logic_formula_t* formula,
    int max_iterations,
    const nimcp_gpu_logic_params_t* params);

/**
 * @brief Apply fuzzy logic operators
 */
NIMCP_EXPORT bool nimcp_gpu_fuzzy_logic(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input1,
    const nimcp_gpu_tensor_t* input2,
    nimcp_gpu_tensor_t* output,
    nimcp_logic_op_t op,
    const nimcp_gpu_logic_params_t* params);

/**
 * @brief SAT solving step (unit propagation + conflict detection)
 */
NIMCP_EXPORT bool nimcp_gpu_sat_step(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_logic_formula_t* formula,
    nimcp_gpu_tensor_t* conflict,
    const nimcp_gpu_logic_params_t* params);

//=============================================================================
// Rule Engine Functions
//=============================================================================

/**
 * @brief Match rules against working memory
 */
NIMCP_EXPORT bool nimcp_gpu_rule_match(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_rule_base_t* rules,
    const nimcp_gpu_working_memory_t* wm,
    nimcp_gpu_tensor_t* match_scores,
    const nimcp_gpu_rule_params_t* params);

/**
 * @brief Fire matched rules and update working memory
 */
NIMCP_EXPORT bool nimcp_gpu_rule_fire(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_rule_base_t* rules,
    nimcp_gpu_working_memory_t* wm,
    const nimcp_gpu_tensor_t* match_scores,
    const nimcp_gpu_rule_params_t* params);

/**
 * @brief Forward chaining inference
 */
NIMCP_EXPORT bool nimcp_gpu_forward_chain(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_rule_base_t* rules,
    nimcp_gpu_working_memory_t* wm,
    int max_steps,
    const nimcp_gpu_rule_params_t* params);

/**
 * @brief Backward chaining inference
 */
NIMCP_EXPORT bool nimcp_gpu_backward_chain(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_rule_base_t* rules,
    nimcp_gpu_working_memory_t* wm,
    const nimcp_gpu_tensor_t* goal,
    nimcp_gpu_tensor_t* proof,
    const nimcp_gpu_rule_params_t* params);

/**
 * @brief Update rule strengths based on success
 */
NIMCP_EXPORT bool nimcp_gpu_rule_learning(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_rule_base_t* rules,
    const nimcp_gpu_tensor_t* feedback,
    float learning_rate,
    const nimcp_gpu_rule_params_t* params);

//=============================================================================
// Constraint Satisfaction Functions
//=============================================================================

/**
 * @brief Initialize CSP domains
 */
NIMCP_EXPORT bool nimcp_gpu_csp_init(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_csp_state_t* state,
    const nimcp_gpu_tensor_t* initial_domains);

/**
 * @brief Arc consistency propagation (AC-3)
 */
NIMCP_EXPORT bool nimcp_gpu_csp_arc_consistency(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_csp_state_t* state,
    const nimcp_gpu_csp_params_t* params);

/**
 * @brief Check constraint violations
 */
NIMCP_EXPORT bool nimcp_gpu_csp_check_constraints(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_csp_state_t* state,
    nimcp_gpu_tensor_t* n_violations,
    const nimcp_gpu_csp_params_t* params);

/**
 * @brief Select variable and value for assignment
 */
NIMCP_EXPORT bool nimcp_gpu_csp_select(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_csp_state_t* state,
    size_t* variable_idx,
    size_t* value_idx,
    const nimcp_gpu_csp_params_t* params);

/**
 * @brief CSP solving step
 */
NIMCP_EXPORT bool nimcp_gpu_csp_step(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_csp_state_t* state,
    bool* solved,
    bool* failed,
    const nimcp_gpu_csp_params_t* params);

//=============================================================================
// Analogical Reasoning Functions
//=============================================================================

/**
 * @brief Compute structural similarity between domains
 */
NIMCP_EXPORT bool nimcp_gpu_analogy_structural_similarity(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_analogy_state_t* state,
    nimcp_gpu_tensor_t* similarity_matrix,
    const nimcp_gpu_analogy_params_t* params);

/**
 * @brief Find optimal structure mapping
 */
NIMCP_EXPORT bool nimcp_gpu_analogy_find_mapping(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_analogy_state_t* state,
    const nimcp_gpu_analogy_params_t* params);

/**
 * @brief Transfer inferences from source to target
 */
NIMCP_EXPORT bool nimcp_gpu_analogy_transfer(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_analogy_state_t* state,
    const nimcp_gpu_tensor_t* source_inferences,
    nimcp_gpu_tensor_t* target_inferences,
    const nimcp_gpu_analogy_params_t* params);

/**
 * @brief Evaluate analogy quality
 */
NIMCP_EXPORT bool nimcp_gpu_analogy_evaluate(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_analogy_state_t* state,
    float* quality_score,
    const nimcp_gpu_analogy_params_t* params);

//=============================================================================
// Causal Reasoning Functions
//=============================================================================

/**
 * @brief Propagate causal effects through graph
 */
NIMCP_EXPORT bool nimcp_gpu_causal_propagate(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_causal_state_t* state,
    const nimcp_gpu_causal_params_t* params);

/**
 * @brief Apply intervention (do-calculus)
 */
NIMCP_EXPORT bool nimcp_gpu_causal_intervene(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_causal_state_t* state,
    size_t node_idx,
    float intervention_value,
    const nimcp_gpu_causal_params_t* params);

/**
 * @brief Compute counterfactual
 */
NIMCP_EXPORT bool nimcp_gpu_causal_counterfactual(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_causal_state_t* state,
    const nimcp_gpu_tensor_t* factual_values,
    size_t intervention_node,
    float counterfactual_value,
    nimcp_gpu_tensor_t* counterfactual_outcome,
    const nimcp_gpu_causal_params_t* params);

/**
 * @brief Identify causal effects (backdoor adjustment)
 */
NIMCP_EXPORT bool nimcp_gpu_causal_identify_effect(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_causal_state_t* state,
    size_t cause_node,
    size_t effect_node,
    float* causal_effect,
    const nimcp_gpu_causal_params_t* params);

/**
 * @brief Discover causal structure from data
 */
NIMCP_EXPORT bool nimcp_gpu_causal_discover(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* observational_data,
    nimcp_gpu_causal_state_t* state,
    const nimcp_gpu_causal_params_t* params);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_REASONING_GPU_H
