//=============================================================================
// nimcp_fuzzy_gpu.h - GPU Fuzzy Logic Acceleration API
//=============================================================================
/**
 * @file nimcp_fuzzy_gpu.h
 * @brief Main public API for GPU-accelerated fuzzy logic operations
 *
 * WHAT: GPU acceleration for fuzzy inference, batch MF evaluation,
 *       defuzzification, relation composition, and ANFIS training
 * WHY:  Achieve 50-100x speedup for batch fuzzy operations in financial
 *       analysis, risk assessment, and real-time decision systems
 * HOW:  CUDA kernels for parallel MF evaluation, inference, and training
 *       with optimized memory access patterns
 *
 * ARCHITECTURE:
 *
 *   +------------------------------------------------------------------+
 *   |                    GPU FUZZY ACCELERATION                         |
 *   |                                                                  |
 *   |  +----------------+  +------------------+  +------------------+  |
 *   |  | MF Evaluation  |  | Batch Inference  |  | Defuzzification  |  |
 *   |  | (14 types +    |  | (Mamdani/Sugeno) |  | (7 methods)      |  |
 *   |  |  8 hedges)     |  |                  |  |                  |  |
 *   |  +----------------+  +------------------+  +------------------+  |
 *   |           |                  |                    |              |
 *   |  +----------------+  +------------------+  +------------------+  |
 *   |  | Relation       |  | ANFIS Training   |  | Operator Batch   |  |
 *   |  | Composition    |  | (GPU Backprop)   |  | (T-norm/conorm)  |  |
 *   |  +----------------+  +------------------+  +------------------+  |
 *   +------------------------------------------------------------------+
 *
 * EXPECTED PERFORMANCE:
 *   - Batch Inference (10K samples): 100ms CPU -> 2ms GPU (50x)
 *   - MF Discretization (256 pts):   5ms CPU -> 0.1ms GPU (50x)
 *   - ANFIS Training (1000 epochs): 10s CPU -> 100ms GPU (100x)
 *
 * THREAD SAFETY:
 *   - State creation/destruction: NOT thread-safe
 *   - Inference operations: Thread-safe with different states
 *   - Same state from multiple threads: NOT thread-safe
 *
 * CPU FALLBACK:
 *   - All functions return error codes when CUDA unavailable
 *   - Use CPU fuzzy_inference_* functions as fallback
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_FUZZY_GPU_H
#define NIMCP_FUZZY_GPU_H

#include "common/nimcp_export.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/fuzzy/nimcp_fuzzy_gpu_types.h"
#include "gpu/fuzzy/nimcp_fuzzy_gpu_params.h"
#include "utils/fuzzy/nimcp_fuzzy_inference.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief GPU fuzzy operation statistics
 */
typedef struct {
    uint64_t mf_evaluations;                    /**< Total MF evaluations on GPU */
    uint64_t batch_inferences;                  /**< Total batch inference calls */
    uint64_t samples_processed;                 /**< Total samples processed */
    uint64_t defuzzifications;                  /**< Total defuzzification ops */
    uint64_t relation_compositions;             /**< Total relation compositions */
    uint64_t anfis_epochs;                      /**< Total ANFIS training epochs */
    float total_kernel_time_ms;                 /**< Total GPU kernel time */
    float avg_inference_time_us;                /**< Average per-sample inference */
    size_t peak_memory_bytes;                   /**< Peak GPU memory used */
} nimcp_gpu_fuzzy_stats_t;

//=============================================================================
// State Lifecycle
//=============================================================================

/**
 * @brief Create GPU fuzzy inference state from CPU inference engine
 *
 * WHAT: Creates GPU state with all data uploaded to device
 * WHY:  One-time upload cost, then fast repeated inference
 * HOW:  Deep copy of variables, rules, and config to device memory
 *
 * @param ctx        GPU context (must be valid)
 * @param cpu_engine CPU inference engine to copy from
 * @return GPU state on success, NULL on failure
 *
 * THREAD SAFETY: NOT thread-safe
 *
 * EXAMPLE:
 *   fuzzy_inference_engine_t* cpu_eng = fuzzy_inference_create();
 *   // ... configure cpu_eng with variables and rules ...
 *   nimcp_gpu_fuzzy_inference_state_t* gpu_state =
 *       nimcp_gpu_fuzzy_state_create(ctx, cpu_eng);
 *   // ... run batch inference on gpu_state ...
 *   nimcp_gpu_fuzzy_state_destroy(gpu_state);
 */
NIMCP_EXPORT nimcp_gpu_fuzzy_inference_state_t* nimcp_gpu_fuzzy_state_create(
    nimcp_gpu_context_t* ctx,
    const fuzzy_inference_engine_t* cpu_engine);

/**
 * @brief Create GPU fuzzy inference state with custom batch capacity
 *
 * @param ctx            GPU context
 * @param cpu_engine     CPU inference engine to copy
 * @param batch_capacity Maximum batch size for inference
 * @return GPU state on success, NULL on failure
 */
NIMCP_EXPORT nimcp_gpu_fuzzy_inference_state_t* nimcp_gpu_fuzzy_state_create_with_capacity(
    nimcp_gpu_context_t* ctx,
    const fuzzy_inference_engine_t* cpu_engine,
    uint32_t batch_capacity);

/**
 * @brief Destroy GPU fuzzy inference state
 *
 * @param state State to destroy (NULL-safe)
 */
NIMCP_EXPORT void nimcp_gpu_fuzzy_state_destroy(nimcp_gpu_fuzzy_inference_state_t* state);

/**
 * @brief Check if GPU state is valid for inference
 *
 * @param state State to check
 * @return true if valid
 */
NIMCP_EXPORT bool nimcp_gpu_fuzzy_state_is_valid(
    const nimcp_gpu_fuzzy_inference_state_t* state);

/**
 * @brief Synchronize GPU state with updated CPU engine
 *
 * Use after modifying rules or variables in CPU engine.
 *
 * @param ctx        GPU context
 * @param state      GPU state to update
 * @param cpu_engine Updated CPU engine
 * @return 0 on success, error code on failure
 */
NIMCP_EXPORT int nimcp_gpu_fuzzy_state_sync(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_fuzzy_inference_state_t* state,
    const fuzzy_inference_engine_t* cpu_engine);

//=============================================================================
// Batch Inference API
//=============================================================================

/**
 * @brief Run batch fuzzy inference on GPU
 *
 * WHAT: Parallel inference for many input samples
 * WHY:  50x speedup over sequential CPU inference
 * HOW:  One thread block per sample, parallel rule evaluation
 *
 * @param ctx     GPU context
 * @param state   GPU inference state
 * @param inputs  Input tensor [batch_size x num_inputs] on device
 * @param outputs Output tensor [batch_size x num_outputs] on device
 * @param params  Inference parameters (NULL for defaults)
 * @return true on success, false on failure
 *
 * THREAD SAFETY: Thread-safe with different states
 *
 * EXAMPLE:
 *   // Allocate device tensors
 *   nimcp_gpu_fuzzy_tensor_t inputs, outputs;
 *   // ... setup tensors with batch data ...
 *   nimcp_gpu_inference_params_t params = nimcp_gpu_inference_params_default();
 *   params.batch_size = 10000;
 *   bool ok = nimcp_gpu_fuzzy_inference_batch(ctx, state, &inputs, &outputs, &params);
 */
NIMCP_EXPORT bool nimcp_gpu_fuzzy_inference_batch(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_fuzzy_inference_state_t* state,
    const nimcp_gpu_fuzzy_tensor_t* inputs,
    nimcp_gpu_fuzzy_tensor_t* outputs,
    const nimcp_gpu_inference_params_t* params);

/**
 * @brief Run batch inference with rule strengths output
 *
 * Returns both defuzzified outputs and per-rule firing strengths.
 *
 * @param ctx             GPU context
 * @param state           GPU inference state
 * @param inputs          Input tensor [batch x inputs]
 * @param outputs         Output tensor [batch x outputs]
 * @param rule_strengths  Output tensor [batch x rules] for firing strengths
 * @param params          Inference parameters
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_fuzzy_inference_batch_with_strengths(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_fuzzy_inference_state_t* state,
    const nimcp_gpu_fuzzy_tensor_t* inputs,
    nimcp_gpu_fuzzy_tensor_t* outputs,
    nimcp_gpu_fuzzy_tensor_t* rule_strengths,
    const nimcp_gpu_inference_params_t* params);

//=============================================================================
// Membership Function Evaluation API
//=============================================================================

/**
 * @brief Batch evaluate membership functions on GPU
 *
 * Evaluates one or more MFs at many input points in parallel.
 *
 * @param ctx          GPU context
 * @param inputs       Input values [num_samples] on device
 * @param mfs          MF definitions [num_mfs] (host array)
 * @param num_mfs      Number of MFs to evaluate
 * @param memberships  Output [num_samples x num_mfs] on device
 * @param params       Evaluation parameters
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_fuzzy_mf_evaluate_batch(
    nimcp_gpu_context_t* ctx,
    const float* inputs,
    const fuzzy_gpu_mf_t* mfs,
    uint32_t num_mfs,
    float* memberships,
    const nimcp_gpu_mf_eval_params_t* params);

/**
 * @brief Batch discretize membership functions
 *
 * Samples each MF at uniform intervals for defuzzification.
 *
 * @param ctx          GPU context
 * @param mfs          MF definitions [num_mfs]
 * @param num_mfs      Number of MFs
 * @param discretized  Output [num_mfs x resolution] on device
 * @param params       Discretization parameters
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_fuzzy_mf_discretize_batch(
    nimcp_gpu_context_t* ctx,
    const fuzzy_gpu_mf_t* mfs,
    uint32_t num_mfs,
    float* discretized,
    const nimcp_gpu_discretize_params_t* params);

//=============================================================================
// Defuzzification API
//=============================================================================

/**
 * @brief Batch defuzzification on GPU
 *
 * Defuzzifies aggregated fuzzy outputs to crisp values.
 *
 * @param ctx        GPU context
 * @param aggregated Aggregated MF values [num_samples x resolution]
 * @param outputs    Crisp outputs [num_samples]
 * @param params     Defuzzification parameters
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_fuzzy_defuzzify_batch(
    nimcp_gpu_context_t* ctx,
    const float* aggregated,
    float* outputs,
    const nimcp_gpu_defuzz_params_t* params);

/**
 * @brief Batch defuzzify with multiple methods
 *
 * Applies multiple defuzzification methods to same input.
 *
 * @param ctx        GPU context
 * @param aggregated Aggregated values [num_samples x resolution]
 * @param methods    Array of method types [num_methods]
 * @param num_methods Number of methods
 * @param outputs    Outputs [num_samples x num_methods]
 * @param params     Base parameters (method field ignored)
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_fuzzy_defuzzify_multi_method(
    nimcp_gpu_context_t* ctx,
    const float* aggregated,
    const uint32_t* methods,
    uint32_t num_methods,
    float* outputs,
    const nimcp_gpu_defuzz_params_t* params);

//=============================================================================
// Fuzzy Relation API
//=============================================================================

/**
 * @brief Compose two fuzzy relations on GPU
 *
 * Max-min (or general t-norm/t-conorm) composition: R o S
 *
 * @param ctx      GPU context
 * @param rel_a    First relation [rows_a x cols_a]
 * @param rows_a   Rows of first relation
 * @param cols_a   Columns of first relation
 * @param rel_b    Second relation [cols_a x cols_b]
 * @param cols_b   Columns of second relation
 * @param rel_out  Output relation [rows_a x cols_b]
 * @param params   Composition parameters
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_fuzzy_relation_compose(
    nimcp_gpu_context_t* ctx,
    const float* rel_a, uint32_t rows_a, uint32_t cols_a,
    const float* rel_b, uint32_t cols_b,
    float* rel_out,
    const nimcp_gpu_relation_params_t* params);

/**
 * @brief Batch relation composition
 *
 * Compose multiple pairs of relations in parallel.
 *
 * @param ctx          GPU context
 * @param relations_a  Array of first relations [batch x rows_a x cols_a]
 * @param relations_b  Array of second relations [batch x cols_a x cols_b]
 * @param batch_size   Number of relation pairs
 * @param rows_a       Rows in each first relation
 * @param cols_a       Shared dimension
 * @param cols_b       Columns in each second relation
 * @param relations_out Output relations [batch x rows_a x cols_b]
 * @param params       Composition parameters
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_fuzzy_relation_compose_batch(
    nimcp_gpu_context_t* ctx,
    const float* relations_a,
    const float* relations_b,
    uint32_t batch_size,
    uint32_t rows_a, uint32_t cols_a, uint32_t cols_b,
    float* relations_out,
    const nimcp_gpu_relation_params_t* params);

//=============================================================================
// ANFIS Training API
//=============================================================================

/**
 * @brief Train ANFIS model on GPU
 *
 * WHAT: GPU-accelerated ANFIS training with backpropagation
 * WHY:  100x speedup for learning Sugeno consequent parameters
 * HOW:  Forward pass, error computation, gradient descent all on GPU
 *
 * @param ctx             GPU context
 * @param state           GPU inference state (Sugeno type required)
 * @param train_inputs    Training inputs [num_samples x num_inputs]
 * @param train_targets   Target outputs [num_samples x num_outputs]
 * @param num_samples     Number of training samples
 * @param out_final_error Output: final training error
 * @param params          ANFIS training parameters
 * @return true on success
 *
 * EXAMPLE:
 *   nimcp_gpu_anfis_params_t params = nimcp_gpu_anfis_params_default();
 *   params.max_epochs = 500;
 *   params.learning_rate = 0.01f;
 *   float final_error;
 *   bool ok = nimcp_gpu_anfis_train_raw(ctx, state, train_x, train_y,
 *                                    1000, &final_error, &params);
 */
NIMCP_EXPORT bool nimcp_gpu_anfis_train_raw(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_fuzzy_inference_state_t* state,
    const float* train_inputs,
    const float* train_targets,
    uint32_t num_samples,
    float* out_final_error,
    const nimcp_gpu_anfis_params_t* params);

/**
 * @brief Single ANFIS training epoch on GPU
 *
 * Run one epoch of training, useful for custom training loops.
 *
 * @param ctx        GPU context
 * @param state      GPU inference state
 * @param inputs     Training inputs (device pointer)
 * @param targets    Target outputs (device pointer)
 * @param num_samples Number of samples
 * @param params     Training parameters
 * @param out_epoch_error Output: error for this epoch
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_anfis_train_epoch(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_fuzzy_inference_state_t* state,
    const float* inputs,
    const float* targets,
    uint32_t num_samples,
    const nimcp_gpu_anfis_params_t* params,
    float* out_epoch_error);

/**
 * @brief Download trained ANFIS parameters back to CPU engine
 *
 * After GPU training, sync learned parameters to CPU inference engine.
 *
 * @param ctx        GPU context
 * @param state      GPU state with trained parameters
 * @param cpu_engine CPU engine to update
 * @return 0 on success, error code on failure
 */
NIMCP_EXPORT int nimcp_gpu_anfis_download_params(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_fuzzy_inference_state_t* state,
    fuzzy_inference_engine_t* cpu_engine);

//=============================================================================
// Operator Batch API
//=============================================================================

/**
 * @brief Batch T-norm evaluation on GPU
 *
 * @param ctx      GPU context
 * @param a        First operands [num_elements]
 * @param b        Second operands [num_elements]
 * @param result   Results [num_elements]
 * @param tnorm    T-norm type
 * @param num_elements Number of operations
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_fuzzy_tnorm_batch(
    nimcp_gpu_context_t* ctx,
    const float* a, const float* b, float* result,
    uint32_t tnorm, uint32_t num_elements);

/**
 * @brief Batch T-conorm evaluation on GPU
 */
NIMCP_EXPORT bool nimcp_gpu_fuzzy_tconorm_batch(
    nimcp_gpu_context_t* ctx,
    const float* a, const float* b, float* result,
    uint32_t tconorm, uint32_t num_elements);

/**
 * @brief Batch complement evaluation on GPU
 */
NIMCP_EXPORT bool nimcp_gpu_fuzzy_complement_batch(
    nimcp_gpu_context_t* ctx,
    const float* a, float* result,
    uint32_t complement, float param, uint32_t num_elements);

/**
 * @brief Batch implication evaluation on GPU
 */
NIMCP_EXPORT bool nimcp_gpu_fuzzy_implication_batch(
    nimcp_gpu_context_t* ctx,
    const float* antecedent, const float* consequent, float* result,
    uint32_t implication, uint32_t num_elements);

/**
 * @brief Batch aggregation on GPU (reduce array)
 */
NIMCP_EXPORT bool nimcp_gpu_fuzzy_aggregate_batch(
    nimcp_gpu_context_t* ctx,
    const float* values, uint32_t* lengths, float* results,
    uint32_t aggregation, uint32_t num_arrays);

//=============================================================================
// Tensor Utilities
//=============================================================================

/**
 * @brief Create GPU fuzzy tensor
 *
 * @param ctx   GPU context
 * @param dims  Dimension sizes [rank]
 * @param rank  Number of dimensions (1-4)
 * @return Tensor on success, {NULL,0,0,0,false} on failure
 */
NIMCP_EXPORT nimcp_gpu_fuzzy_tensor_t nimcp_gpu_fuzzy_tensor_create(
    nimcp_gpu_context_t* ctx,
    const uint32_t* dims,
    uint32_t rank);

/**
 * @brief Destroy GPU fuzzy tensor
 */
NIMCP_EXPORT void nimcp_gpu_fuzzy_tensor_destroy(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_fuzzy_tensor_t* tensor);

/**
 * @brief Upload data to GPU tensor
 */
NIMCP_EXPORT bool nimcp_gpu_fuzzy_tensor_upload(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_fuzzy_tensor_t* tensor,
    const float* host_data);

/**
 * @brief Download data from GPU tensor
 */
NIMCP_EXPORT bool nimcp_gpu_fuzzy_tensor_download(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_fuzzy_tensor_t* tensor,
    float* host_data);

//=============================================================================
// Statistics and Utilities
//=============================================================================

/**
 * @brief Get GPU fuzzy operation statistics
 */
NIMCP_EXPORT int nimcp_gpu_fuzzy_get_stats(nimcp_gpu_fuzzy_stats_t* stats);

/**
 * @brief Reset GPU fuzzy statistics
 */
NIMCP_EXPORT void nimcp_gpu_fuzzy_reset_stats(void);

/**
 * @brief Get last GPU fuzzy error message
 */
NIMCP_EXPORT const char* nimcp_gpu_fuzzy_get_last_error(void);

/**
 * @brief Check if GPU fuzzy acceleration is available
 */
NIMCP_EXPORT bool nimcp_gpu_fuzzy_is_available(void);

/**
 * @brief Get recommended batch size for current GPU
 *
 * @param ctx   GPU context
 * @param state Inference state (for memory estimation)
 * @return Recommended batch size
 */
NIMCP_EXPORT uint32_t nimcp_gpu_fuzzy_recommended_batch_size(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_fuzzy_inference_state_t* state);

//=============================================================================
// CPU Type Conversion Utilities
//=============================================================================

/**
 * @brief Convert CPU MF to GPU MF format
 *
 * @param cpu_mf Source CPU MF
 * @param hedge  Hedge to apply
 * @param alpha_cut Alpha-cut threshold
 * @return GPU MF structure
 */
NIMCP_EXPORT fuzzy_gpu_mf_t nimcp_gpu_fuzzy_mf_from_cpu(
    const fuzzy_mf_t* cpu_mf,
    fuzzy_hedge_t hedge,
    float alpha_cut);

/**
 * @brief Convert CPU variable to GPU variable format
 *
 * @param cpu_var Source CPU variable
 * @param out_var Output GPU variable
 * @return 0 on success, error code on failure
 */
NIMCP_EXPORT int nimcp_gpu_fuzzy_variable_from_cpu(
    const fuzzy_variable_t* cpu_var,
    fuzzy_gpu_variable_t* out_var);

/**
 * @brief Convert CPU rule to GPU rule format
 *
 * @param cpu_rule Source CPU rule
 * @param out_rule Output GPU rule
 * @return 0 on success, error code on failure
 */
NIMCP_EXPORT int nimcp_gpu_fuzzy_rule_from_cpu(
    const fuzzy_rule_t* cpu_rule,
    fuzzy_gpu_rule_t* out_rule);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FUZZY_GPU_H */
