//=============================================================================
// nimcp_fuzzy_anfis_gpu.h - GPU ANFIS Training API
//=============================================================================
/**
 * @file nimcp_fuzzy_anfis_gpu.h
 * @brief GPU-accelerated ANFIS (Adaptive Neuro-Fuzzy Inference System) training
 *
 * WHAT: GPU acceleration for ANFIS network training and inference
 * WHY:  100x speedup for ANFIS parameter optimization via backpropagation
 * HOW:  CUDA kernels for forward pass, backward pass, and parameter updates
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_FUZZY_ANFIS_GPU_H
#define NIMCP_FUZZY_ANFIS_GPU_H

#include "common/nimcp_export.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/fuzzy/nimcp_fuzzy_gpu_types.h"
#include "gpu/fuzzy/nimcp_fuzzy_gpu_params.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "utils/fuzzy/nimcp_fuzzy_inference.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Extended ANFIS Creation Parameters
//=============================================================================

/**
 * @brief Extended parameters for creating ANFIS with more control
 */
typedef struct nimcp_gpu_anfis_create_params {
    uint32_t num_inputs;                    /**< Number of input variables */
    uint32_t num_outputs;                   /**< Number of outputs */
    uint32_t num_mfs_per_input;             /**< MFs per input */
    fuzzy_mf_type_t mf_type;                /**< Type of membership functions */
    float learning_rate;                    /**< Initial learning rate */
    float momentum;                         /**< Momentum for SGD */
} nimcp_gpu_anfis_create_params_t;

/**
 * @brief Default create params
 */
static inline nimcp_gpu_anfis_create_params_t nimcp_gpu_anfis_create_params_default(void)
{
    nimcp_gpu_anfis_create_params_t params = {
        .num_inputs = 2,
        .num_outputs = 1,
        .num_mfs_per_input = 3,
        .mf_type = FUZZY_MF_GAUSSIAN,
        .learning_rate = 0.01f,
        .momentum = 0.9f
    };
    return params;
}

//=============================================================================
// ANFIS Training Parameters
//=============================================================================

/**
 * @brief Parameters for ANFIS training (extended version)
 */
typedef struct nimcp_gpu_anfis_train_params {
    uint32_t num_epochs;                    /**< Number of training epochs */
    uint32_t batch_size;                    /**< Mini-batch size */
    float early_stop_threshold;             /**< Error threshold for early stop */
    bool shuffle;                           /**< Shuffle data each epoch */
    bool hybrid_learning;                   /**< Use hybrid learning algorithm */
    float lse_lambda;                       /**< LSE regularization */
} nimcp_gpu_anfis_train_params_t;

/**
 * @brief Default training params
 */
static inline nimcp_gpu_anfis_train_params_t nimcp_gpu_anfis_train_params_default(void)
{
    nimcp_gpu_anfis_train_params_t params = {
        .num_epochs = 100,
        .batch_size = 64,
        .early_stop_threshold = 0.001f,
        .shuffle = true,
        .hybrid_learning = true,
        .lse_lambda = 0.0001f
    };
    return params;
}

//=============================================================================
// ANFIS Lifecycle Functions
//=============================================================================

/**
 * @brief Create a new ANFIS network on GPU
 *
 * @param ctx     GPU context (must be valid)
 * @param params  Creation parameters (can use nimcp_gpu_anfis_create_params_t* cast)
 * @return ANFIS state on success, NULL on failure
 */
NIMCP_EXPORT nimcp_gpu_anfis_state_t* nimcp_gpu_anfis_create(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_anfis_create_params_t* params);

/**
 * @brief Destroy ANFIS network and free GPU memory
 *
 * @param anfis ANFIS state to destroy (can be NULL)
 */
NIMCP_EXPORT void nimcp_gpu_anfis_destroy(nimcp_gpu_anfis_state_t* anfis);

//=============================================================================
// ANFIS Training Functions
//=============================================================================

/**
 * @brief Train ANFIS network on GPU
 *
 * @param ctx            GPU context
 * @param anfis          ANFIS state to train
 * @param train_inputs   Training input tensor [n_samples x n_inputs]
 * @param train_targets  Training target tensor [n_samples x n_outputs]
 * @param params         Training parameters
 * @param out_initial_error Output: initial error (can be NULL)
 * @param out_final_error   Output: final error (can be NULL)
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_anfis_train(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_anfis_state_t* anfis,
    const nimcp_gpu_tensor_t* train_inputs,
    const nimcp_gpu_tensor_t* train_targets,
    const nimcp_gpu_anfis_train_params_t* params,
    float* out_initial_error,
    float* out_final_error);

//=============================================================================
// ANFIS Inference Functions
//=============================================================================

/**
 * @brief Forward pass through ANFIS network
 *
 * @param ctx      GPU context
 * @param anfis    Trained ANFIS state
 * @param inputs   Input tensor [n_samples x n_inputs]
 * @param outputs  Output tensor [n_samples x n_outputs] (pre-allocated)
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_anfis_forward(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_anfis_state_t* anfis,
    const nimcp_gpu_tensor_t* inputs,
    nimcp_gpu_tensor_t* outputs);

/**
 * @brief Get last ANFIS error message
 */
NIMCP_EXPORT const char* nimcp_gpu_anfis_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_FUZZY_ANFIS_GPU_H
