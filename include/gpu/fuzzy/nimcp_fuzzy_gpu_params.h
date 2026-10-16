//=============================================================================
// nimcp_fuzzy_gpu_params.h - GPU Fuzzy Logic Parameter Structures
//=============================================================================
/**
 * @file nimcp_fuzzy_gpu_params.h
 * @brief Parameter structures for GPU fuzzy logic kernel launches
 *
 * WHAT: Configuration structs for GPU fuzzy operations
 * WHY:  Decouple kernel configuration from implementation, enable
 *       flexible parameter passing without modifying kernel signatures
 * HOW:  Typed parameter structs for each operation category
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_FUZZY_GPU_PARAMS_H
#define NIMCP_FUZZY_GPU_PARAMS_H

#include <stdint.h>
#include <stdbool.h>
#include "gpu/fuzzy/nimcp_fuzzy_gpu_types.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Membership Function Evaluation Parameters
//=============================================================================

/**
 * @brief Parameters for batch MF evaluation
 */
typedef struct {
    uint32_t num_samples;                       /**< Number of input samples */
    uint32_t num_mfs;                           /**< Number of MFs to evaluate */
    bool apply_hedges;                          /**< Apply linguistic hedges */
    bool apply_alpha_cuts;                      /**< Apply alpha-cut thresholds */
    bool use_shared_memory;                     /**< Use shared memory optimization */
} nimcp_gpu_mf_eval_params_t;

/**
 * @brief Default MF evaluation parameters
 */
static inline nimcp_gpu_mf_eval_params_t nimcp_gpu_mf_eval_params_default(void)
{
    nimcp_gpu_mf_eval_params_t params = {
        .num_samples = 0,
        .num_mfs = 0,
        .apply_hedges = true,
        .apply_alpha_cuts = false,
        .use_shared_memory = true
    };
    return params;
}

//=============================================================================
// Inference Parameters
//=============================================================================

/**
 * @brief Parameters for GPU fuzzy inference
 */
typedef struct {
    uint32_t batch_size;                        /**< Number of samples to process */
    bool compute_rule_strengths;                /**< Output per-rule firing strengths */
    bool early_termination;                     /**< Skip rules with 0 strength */
    uint32_t defuzz_resolution;                 /**< Defuzzification resolution */
    float min_firing_strength;                  /**< Minimum strength to consider */
} nimcp_gpu_inference_params_t;

/**
 * @brief Default inference parameters
 */
static inline nimcp_gpu_inference_params_t nimcp_gpu_inference_params_default(void)
{
    nimcp_gpu_inference_params_t params = {
        .batch_size = 0,
        .compute_rule_strengths = false,
        .early_termination = true,
        .defuzz_resolution = FUZZY_GPU_DEFUZZ_RESOLUTION,
        .min_firing_strength = 1e-6f
    };
    return params;
}

//=============================================================================
// Defuzzification Parameters
//=============================================================================

/**
 * @brief Parameters for GPU defuzzification
 */
typedef struct {
    uint32_t method;                            /**< fuzzy_defuzz_type_t */
    uint32_t resolution;                        /**< Discretization resolution */
    float x_min;                                /**< Universe minimum */
    float x_max;                                /**< Universe maximum */
    bool use_parallel_reduction;                /**< Use parallel reduction */
    uint32_t num_samples;                       /**< Number of samples to defuzzify */
} nimcp_gpu_defuzz_params_t;

/**
 * @brief Default defuzzification parameters
 */
static inline nimcp_gpu_defuzz_params_t nimcp_gpu_defuzz_params_default(void)
{
    nimcp_gpu_defuzz_params_t params = {
        .method = 0,  // FUZZY_DEFUZZ_CENTROID
        .resolution = FUZZY_GPU_DEFUZZ_RESOLUTION,
        .x_min = 0.0f,
        .x_max = 1.0f,
        .use_parallel_reduction = true,
        .num_samples = 0
    };
    return params;
}

//=============================================================================
// Relation Composition Parameters
//=============================================================================

/**
 * @brief Parameters for fuzzy relation composition
 */
typedef struct {
    uint32_t tnorm;                             /**< fuzzy_tnorm_type_t for inner op */
    uint32_t tconorm;                           /**< fuzzy_tconorm_type_t for outer op */
    bool use_tiled;                             /**< Use tiled matrix multiplication */
    uint32_t tile_size;                         /**< Tile size for tiled mode */
} nimcp_gpu_relation_params_t;

/**
 * @brief Default relation parameters
 */
static inline nimcp_gpu_relation_params_t nimcp_gpu_relation_params_default(void)
{
    nimcp_gpu_relation_params_t params = {
        .tnorm = 0,     // FUZZY_TNORM_MIN
        .tconorm = 0,   // FUZZY_TCONORM_MAX
        .use_tiled = true,
        .tile_size = 16
    };
    return params;
}

//=============================================================================
// ANFIS Training Parameters
//=============================================================================

/**
 * @brief Parameters for GPU ANFIS training
 */
typedef struct {
    float learning_rate;                        /**< Initial learning rate */
    float learning_rate_decay;                  /**< LR decay per epoch */
    float min_learning_rate;                    /**< Minimum learning rate */
    float momentum;                             /**< Momentum coefficient */
    float convergence_tolerance;                /**< Convergence threshold */
    uint32_t max_epochs;                        /**< Maximum training epochs */
    uint32_t batch_size;                        /**< Training batch size */
    bool update_mf_params;                      /**< Update MF parameters (premise) */
    bool update_consequent_params;              /**< Update Sugeno coefficients */
    bool use_adam;                              /**< Use Adam optimizer */
    float adam_beta1;                           /**< Adam beta1 */
    float adam_beta2;                           /**< Adam beta2 */
    float adam_epsilon;                         /**< Adam epsilon */
    bool verbose;                               /**< Print training progress */
    uint32_t print_interval;                    /**< Epochs between progress prints */
} nimcp_gpu_anfis_params_t;

/**
 * @brief Default ANFIS parameters
 */
static inline nimcp_gpu_anfis_params_t nimcp_gpu_anfis_params_default(void)
{
    nimcp_gpu_anfis_params_t params = {
        .learning_rate = 0.01f,
        .learning_rate_decay = 0.99f,
        .min_learning_rate = 1e-6f,
        .momentum = 0.9f,
        .convergence_tolerance = 1e-5f,
        .max_epochs = 1000,
        .batch_size = 256,
        .update_mf_params = true,
        .update_consequent_params = true,
        .use_adam = true,
        .adam_beta1 = 0.9f,
        .adam_beta2 = 0.999f,
        .adam_epsilon = 1e-8f,
        .verbose = false,
        .print_interval = 100
    };
    return params;
}

//=============================================================================
// Operator Batch Parameters
//=============================================================================

/**
 * @brief Parameters for batch fuzzy operator evaluation
 */
typedef struct {
    uint32_t tnorm;                             /**< T-norm type */
    uint32_t tconorm;                           /**< T-conorm type */
    uint32_t complement;                        /**< Complement type */
    float complement_param;                     /**< Parameter for Sugeno/Yager */
    uint32_t implication;                       /**< Implication type */
    uint32_t aggregation;                       /**< Aggregation type */
    uint32_t num_elements;                      /**< Number of elements */
} nimcp_gpu_operator_params_t;

/**
 * @brief Default operator parameters
 */
static inline nimcp_gpu_operator_params_t nimcp_gpu_operator_params_default(void)
{
    nimcp_gpu_operator_params_t params = {
        .tnorm = 0,         // FUZZY_TNORM_MIN
        .tconorm = 0,       // FUZZY_TCONORM_MAX
        .complement = 0,    // FUZZY_COMPLEMENT_STANDARD
        .complement_param = 0.0f,
        .implication = 0,   // FUZZY_IMPL_MAMDANI
        .aggregation = 0,   // FUZZY_AGG_MAX
        .num_elements = 0
    };
    return params;
}

//=============================================================================
// Fuzzification Parameters
//=============================================================================

/**
 * @brief Parameters for batch fuzzification
 */
typedef struct {
    uint32_t num_samples;                       /**< Number of input samples */
    uint32_t num_variables;                     /**< Number of input variables */
    bool normalize_inputs;                      /**< Normalize inputs to [0,1] */
    bool compute_entropy;                       /**< Compute entropy per sample */
    bool compute_dominant;                      /**< Find dominant term per sample */
} nimcp_gpu_fuzzify_params_t;

/**
 * @brief Default fuzzification parameters
 */
static inline nimcp_gpu_fuzzify_params_t nimcp_gpu_fuzzify_params_default(void)
{
    nimcp_gpu_fuzzify_params_t params = {
        .num_samples = 0,
        .num_variables = 0,
        .normalize_inputs = false,
        .compute_entropy = false,
        .compute_dominant = true
    };
    return params;
}

//=============================================================================
// Discretization Parameters
//=============================================================================

/**
 * @brief Parameters for MF discretization
 */
typedef struct {
    uint32_t resolution;                        /**< Number of sample points */
    float x_min;                                /**< Start of range */
    float x_max;                                /**< End of range */
    bool apply_hedge;                           /**< Apply hedge to discretized values */
    uint32_t hedge;                             /**< Hedge type if applied */
} nimcp_gpu_discretize_params_t;

/**
 * @brief Default discretization parameters
 */
static inline nimcp_gpu_discretize_params_t nimcp_gpu_discretize_params_default(void)
{
    nimcp_gpu_discretize_params_t params = {
        .resolution = FUZZY_GPU_DEFUZZ_RESOLUTION,
        .x_min = 0.0f,
        .x_max = 1.0f,
        .apply_hedge = false,
        .hedge = 0  // FUZZY_HEDGE_NONE
    };
    return params;
}

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FUZZY_GPU_PARAMS_H */
