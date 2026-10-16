//=============================================================================
// nimcp_fuzzy_gpu_types.h - GPU-Specific Fuzzy Logic Types
//=============================================================================
/**
 * @file nimcp_fuzzy_gpu_types.h
 * @brief GPU-specific types for fuzzy logic acceleration
 *
 * WHAT: Device-side structures and GPU-optimized type definitions for
 *       parallel fuzzy logic operations
 * WHY:  Enable efficient batch processing of membership functions and
 *       inference on GPU with aligned, coalesced memory access
 * HOW:  Flat parameter arrays, AoS/SoA layouts for GPU efficiency,
 *       device-compatible data structures
 *
 * ARCHITECTURE:
 *
 *   CPU Fuzzy Types                    GPU Fuzzy Types
 *   +------------------+               +------------------+
 *   | fuzzy_mf_t       |  ---copy--->  | fuzzy_gpu_mf_t   |
 *   | (flexible)       |               | (flat params)    |
 *   +------------------+               +------------------+
 *   | inference_engine |  ---upload--> | gpu_inference_   |
 *   | (rules, vars)    |               | state_t          |
 *   +------------------+               +------------------+
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_FUZZY_GPU_TYPES_H
#define NIMCP_FUZZY_GPU_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include "common/nimcp_export.h"
#include "utils/fuzzy/nimcp_fuzzy_types.h"
#include "utils/fuzzy/nimcp_fuzzy_operators.h"
#include "utils/fuzzy/nimcp_fuzzy_inference.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Bio-async module ID for GPU fuzzy acceleration */
#define BIO_MODULE_FUZZY_GPU                0x02A0

/** Maximum MF parameters for GPU (aligned for coalesced access) */
#define FUZZY_GPU_MAX_PARAMS                8

/** Maximum terms per variable on GPU */
#define FUZZY_GPU_MAX_TERMS                 32

/** Maximum input variables for GPU inference */
#define FUZZY_GPU_MAX_INPUTS                32

/** Maximum output variables for GPU inference */
#define FUZZY_GPU_MAX_OUTPUTS               8

/** Maximum rules for GPU inference */
#define FUZZY_GPU_MAX_RULES                 256

/** Maximum antecedents per rule */
#define FUZZY_GPU_MAX_ANTECEDENTS           16

/** Default defuzzification resolution */
#define FUZZY_GPU_DEFUZZ_RESOLUTION         256

/** Default CUDA block size for fuzzy kernels */
#define FUZZY_GPU_BLOCK_SIZE                256

/** Warp size for shuffle operations */
#define FUZZY_GPU_WARP_SIZE                 32

//=============================================================================
// GPU Membership Function Types
//=============================================================================

/**
 * @brief GPU-optimized membership function representation
 *
 * Flat structure with all parameters inline for efficient device access.
 * No pointers or callbacks - all data is inline.
 */
typedef struct {
    uint32_t type;                              /**< fuzzy_mf_type_t cast to uint32 */
    uint32_t hedge;                             /**< fuzzy_hedge_t cast to uint32 */
    float params[FUZZY_GPU_MAX_PARAMS];         /**< Parameter values */
    uint32_t num_params;                        /**< Active parameter count */
    float alpha_cut;                            /**< Minimum activation threshold */
} fuzzy_gpu_mf_t;

/**
 * @brief Packed membership function for SoA layout
 *
 * Used when processing many MFs in parallel with coalesced access.
 */
typedef struct {
    uint32_t* types;                            /**< Array of MF types [num_mfs] */
    uint32_t* hedges;                           /**< Array of hedges [num_mfs] */
    float* params;                              /**< Flattened params [num_mfs * MAX_PARAMS] */
    uint32_t* num_params;                       /**< Param counts [num_mfs] */
    float* alpha_cuts;                          /**< Alpha cuts [num_mfs] */
    uint32_t num_mfs;                           /**< Total number of MFs */
} fuzzy_gpu_mf_soa_t;

//=============================================================================
// GPU Linguistic Variable Types
//=============================================================================

/**
 * @brief GPU-optimized linguistic variable
 *
 * Contains universe bounds and embedded MFs for all terms.
 */
typedef struct {
    float universe_min;                         /**< Start of universe */
    float universe_max;                         /**< End of universe */
    fuzzy_gpu_mf_t terms[FUZZY_GPU_MAX_TERMS];  /**< Term MFs */
    uint32_t num_terms;                         /**< Number of active terms */
    uint32_t _padding;                          /**< Alignment padding */
} fuzzy_gpu_variable_t;

//=============================================================================
// GPU Fuzzy Rule Types
//=============================================================================

/**
 * @brief GPU-optimized antecedent
 */
typedef struct {
    uint32_t var_index;                         /**< Input variable index */
    uint32_t term_index;                        /**< Term index within variable */
    uint32_t negated;                           /**< Boolean: apply NOT */
    uint32_t hedge;                             /**< Additional hedge (fuzzy_hedge_t) */
} fuzzy_gpu_antecedent_t;

/**
 * @brief GPU-optimized Sugeno consequent (polynomial)
 */
typedef struct {
    float coefficients[FUZZY_GPU_MAX_INPUTS + 1];  /**< Polynomial: c0 + c1*x1 + ... */
    uint32_t num_coeffs;                           /**< Number of coefficients */
    uint32_t _padding[3];                          /**< Alignment */
} fuzzy_gpu_consequent_sugeno_t;

/**
 * @brief GPU-optimized fuzzy rule
 */
typedef struct {
    fuzzy_gpu_antecedent_t antecedents[FUZZY_GPU_MAX_ANTECEDENTS];
    uint32_t num_antecedents;                   /**< Active antecedent count */
    uint32_t connector;                         /**< T-norm type for AND */
    uint32_t use_or;                            /**< Boolean: use T-conorm */
    float weight;                               /**< Rule weight [0,1] */

    // Mamdani consequent
    uint32_t out_var_index;                     /**< Output variable index */
    uint32_t out_term_index;                    /**< Output term index */

    // Sugeno consequent (union - either Mamdani or Sugeno used)
    fuzzy_gpu_consequent_sugeno_t sugeno;
} fuzzy_gpu_rule_t;

//=============================================================================
// GPU Inference State
//=============================================================================

/**
 * @brief GPU inference state containing all data for batch inference
 *
 * This structure holds device pointers to all data needed for parallel
 * fuzzy inference on the GPU.
 */
typedef struct {
    // Device pointers to variables
    fuzzy_gpu_variable_t* d_input_vars;         /**< Input variables on device */
    fuzzy_gpu_variable_t* d_output_vars;        /**< Output variables on device */
    uint32_t num_inputs;                        /**< Number of input variables */
    uint32_t num_outputs;                       /**< Number of output variables */

    // Device pointers to rules
    fuzzy_gpu_rule_t* d_rules;                  /**< Rules on device */
    uint32_t num_rules;                         /**< Number of rules */

    // Inference configuration
    uint32_t fis_type;                          /**< fuzzy_fis_type_t */
    uint32_t defuzz_method;                     /**< fuzzy_defuzz_type_t */
    uint32_t and_method;                        /**< fuzzy_tnorm_type_t */
    uint32_t or_method;                         /**< fuzzy_tconorm_type_t */
    uint32_t implication;                       /**< fuzzy_implication_type_t */
    uint32_t aggregation;                       /**< fuzzy_aggregation_type_t */
    uint32_t defuzz_resolution;                 /**< Discretization resolution */

    // Temporary device buffers for inference
    float* d_fuzzified;                         /**< [batch x inputs x terms] */
    float* d_rule_strengths;                    /**< [batch x rules] */
    float* d_aggregated;                        /**< [batch x outputs x resolution] */
    float* d_outputs;                           /**< [batch x outputs] */

    // Capacity tracking
    uint32_t batch_capacity;                    /**< Maximum batch size allocated */
    bool is_valid;                              /**< State is properly initialized */
} nimcp_gpu_fuzzy_inference_state_t;

//=============================================================================
// GPU Tensor Type (for fuzzy operations)
//=============================================================================

/**
 * @brief GPU tensor for fuzzy batch operations
 *
 * Simple contiguous tensor with row-major layout.
 */
typedef struct {
    float* d_data;                              /**< Device data pointer */
    uint32_t dims[4];                           /**< Dimensions [batch, d1, d2, d3] */
    uint32_t rank;                              /**< Number of dimensions (1-4) */
    uint32_t total_elements;                    /**< Product of dimensions */
    bool owns_data;                             /**< True if we should free d_data */
} nimcp_gpu_fuzzy_tensor_t;

//=============================================================================
// GPU ANFIS Types
//=============================================================================

/**
 * @brief ANFIS training state on GPU
 */
typedef struct {
    // Layer outputs (device pointers)
    float* d_fuzzified;                         /**< Layer 1: MF outputs */
    float* d_rule_strengths;                    /**< Layer 2: Rule firing */
    float* d_normalized_strengths;              /**< Layer 3: Normalized strengths */
    float* d_weighted_outputs;                  /**< Layer 4: Weighted consequent outputs */
    float* d_output;                            /**< Layer 5: Summed output */

    // Gradients
    float* d_consequent_grad;                   /**< Gradients for Sugeno coefficients */
    float* d_mf_param_grad;                     /**< Gradients for MF parameters */

    // Training parameters
    float learning_rate;                        /**< Current learning rate */
    float momentum;                             /**< Momentum coefficient */
    float* d_velocity;                          /**< Momentum velocity */

    uint32_t num_samples;                       /**< Samples in current batch */
    uint32_t epoch;                             /**< Current epoch */
} nimcp_gpu_anfis_state_t;

//=============================================================================
// GPU Fuzzy Relation Types
//=============================================================================

/**
 * @brief GPU fuzzy relation matrix
 */
typedef struct {
    float* d_data;                              /**< Device data pointer */
    uint32_t rows;                              /**< Number of rows */
    uint32_t cols;                              /**< Number of columns */
    bool owns_data;                             /**< True if we should free */
} nimcp_gpu_fuzzy_relation_t;

//=============================================================================
// Error Codes
//=============================================================================

#define FUZZY_GPU_ERR_BASE                  28500

#define FUZZY_GPU_ERR_OK                    0
#define FUZZY_GPU_ERR_NULL_CTX              (FUZZY_GPU_ERR_BASE + 1)
#define FUZZY_GPU_ERR_NULL_STATE            (FUZZY_GPU_ERR_BASE + 2)
#define FUZZY_GPU_ERR_NULL_INPUT            (FUZZY_GPU_ERR_BASE + 3)
#define FUZZY_GPU_ERR_NULL_OUTPUT           (FUZZY_GPU_ERR_BASE + 4)
#define FUZZY_GPU_ERR_ALLOC                 (FUZZY_GPU_ERR_BASE + 5)
#define FUZZY_GPU_ERR_KERNEL_LAUNCH         (FUZZY_GPU_ERR_BASE + 6)
#define FUZZY_GPU_ERR_CUDA                  (FUZZY_GPU_ERR_BASE + 7)
#define FUZZY_GPU_ERR_INVALID_STATE         (FUZZY_GPU_ERR_BASE + 8)
#define FUZZY_GPU_ERR_BATCH_SIZE            (FUZZY_GPU_ERR_BASE + 9)
#define FUZZY_GPU_ERR_DIMENSION_MISMATCH    (FUZZY_GPU_ERR_BASE + 10)
#define FUZZY_GPU_ERR_NO_RULES              (FUZZY_GPU_ERR_BASE + 11)
#define FUZZY_GPU_ERR_INVALID_MF_TYPE       (FUZZY_GPU_ERR_BASE + 12)
#define FUZZY_GPU_ERR_INVALID_FIS_TYPE      (FUZZY_GPU_ERR_BASE + 13)
#define FUZZY_GPU_ERR_CONVERGENCE           (FUZZY_GPU_ERR_BASE + 14)

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FUZZY_GPU_TYPES_H */
