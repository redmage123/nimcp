/**
 * @file nimcp_omni_gpu.h
 * @brief GPU-accelerated Omnidirectional Inference CUDA Kernels
 * @version 1.0.0
 * @date 2025-01-04
 *
 * WHAT: CUDA kernels for omnidirectional inference operations
 * WHY:  GPU acceleration for bidirectional prediction, Hopfield retrieval,
 *       predictive coding, and temporal replay
 * HOW:  Custom CUDA kernels optimized for each inference direction
 *
 * HOT PATHS ACCELERATED:
 * ====================
 * 1. Bidirectional Prediction: Forward/backward latent prediction
 * 2. Hopfield Attention: Softmax attention over stored patterns
 * 3. Predictive Error: Parallel error computation across hierarchy levels
 * 4. Replay Sampling: GPU-accelerated priority-weighted sampling
 *
 * ARCHITECTURE:
 * =============
 *
 *   +----------------------------------------------------------+
 *   |              OMNIDIRECTIONAL GPU KERNELS                  |
 *   |                                                          |
 *   |  +--------------+  +--------------+  +--------------+    |
 *   |  | Bidirectional|  |   Hopfield   |  |  Predictive  |    |
 *   |  |  Prediction  |  |  Attention   |  |    Error     |    |
 *   |  +--------------+  +--------------+  +--------------+    |
 *   |                           |                              |
 *   |  +--------------+  +--------------+  +--------------+    |
 *   |  |   Temporal   |  |   Precision  |  |  Free Energy |    |
 *   |  |    Replay    |  |   Weighting  |  |  Aggregation |    |
 *   |  +--------------+  +--------------+  +--------------+    |
 *   +----------------------------------------------------------+
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_OMNI_GPU_H
#define NIMCP_OMNI_GPU_H

#include "gpu/context/nimcp_gpu_context.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "common/nimcp_export.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include <stdbool.h>
#include <stdint.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Maximum latent dimension for omni GPU operations */
#define NIMCP_OMNI_GPU_MAX_DIM              4096

/** @brief Maximum stored patterns for Hopfield GPU */
#define NIMCP_OMNI_GPU_MAX_PATTERNS         65536

/** @brief Maximum hierarchy levels */
#define NIMCP_OMNI_GPU_MAX_LEVELS           16

/** @brief Maximum replay sequence length */
#define NIMCP_OMNI_GPU_MAX_REPLAY_LEN       1024

/** @brief Default block size for omni kernels */
#define NIMCP_OMNI_GPU_BLOCK_SIZE           256

/** @brief Warp size for reduction operations */
#define NIMCP_OMNI_GPU_WARP_SIZE            32

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Inference direction for GPU kernels
 */
typedef enum {
    NIMCP_OMNI_GPU_DIR_FORWARD = 0,      /**< Forward prediction t -> t+1 */
    NIMCP_OMNI_GPU_DIR_BACKWARD,          /**< Backward inference t -> t-1 */
    NIMCP_OMNI_GPU_DIR_LATERAL,           /**< Cross-modal lateral */
    NIMCP_OMNI_GPU_DIR_HIER_UP,           /**< Bottom-up in hierarchy */
    NIMCP_OMNI_GPU_DIR_HIER_DOWN,         /**< Top-down in hierarchy */
    NIMCP_OMNI_GPU_DIR_MASKED,            /**< Fill-in-the-blank */
    NIMCP_OMNI_GPU_DIR_ASSOCIATIVE,       /**< Associative retrieval */
    NIMCP_OMNI_GPU_DIR_COUNT
} nimcp_omni_gpu_direction_t;

/**
 * @brief Hopfield attention mode
 */
typedef enum {
    NIMCP_HOPFIELD_GPU_SOFTMAX = 0,      /**< Standard softmax attention */
    NIMCP_HOPFIELD_GPU_EXPONENTIAL,       /**< Exponential (sparse) */
    NIMCP_HOPFIELD_GPU_POLYNOMIAL,        /**< Polynomial capacity */
    NIMCP_HOPFIELD_GPU_SPARSE_TOP_K       /**< Sparse top-k attention */
} nimcp_hopfield_gpu_mode_t;

/**
 * @brief Replay sweep direction
 */
typedef enum {
    NIMCP_REPLAY_GPU_FORWARD = 0,        /**< Forward temporal sweep */
    NIMCP_REPLAY_GPU_BACKWARD,            /**< Reverse temporal sweep */
    NIMCP_REPLAY_GPU_PRIORITY             /**< Priority-based sampling */
} nimcp_replay_gpu_mode_t;

/**
 * @brief Precision update mode
 */
typedef enum {
    NIMCP_PRECISION_GPU_FIXED = 0,       /**< Fixed precision */
    NIMCP_PRECISION_GPU_ADAPTIVE,         /**< Adaptive from errors */
    NIMCP_PRECISION_GPU_LEARNED           /**< Learned precision */
} nimcp_precision_gpu_mode_t;

/* ============================================================================
 * GPU State Structures
 * ============================================================================ */

/**
 * @brief Bidirectional predictor GPU state
 */
typedef struct {
    nimcp_gpu_tensor_t* forward_weights;   /**< Forward prediction weights */
    nimcp_gpu_tensor_t* forward_bias;      /**< Forward prediction bias */
    nimcp_gpu_tensor_t* backward_weights;  /**< Backward inference weights */
    nimcp_gpu_tensor_t* backward_bias;     /**< Backward inference bias */
    nimcp_gpu_tensor_t* lateral_weights;   /**< Lateral prediction weights */
    nimcp_gpu_tensor_t* lateral_bias;      /**< Lateral prediction bias */

    /* Workspace buffers */
    nimcp_gpu_tensor_t* hidden_buffer;     /**< Hidden layer activations */
    nimcp_gpu_tensor_t* output_buffer;     /**< Output buffer */

    /* Configuration */
    uint32_t input_dim;                    /**< Input dimension */
    uint32_t hidden_dim;                   /**< Hidden dimension */
    uint32_t output_dim;                   /**< Output dimension */
    uint32_t num_layers;                   /**< Number of MLP layers */

    /* GPU context */
    nimcp_gpu_context_t* ctx;
} nimcp_omni_gpu_bidirectional_t;

/**
 * @brief Hopfield memory GPU state
 */
typedef struct {
    nimcp_gpu_tensor_t* patterns;          /**< Stored patterns [capacity x dim] */
    nimcp_gpu_tensor_t* similarities;      /**< Similarity scores [capacity] */
    nimcp_gpu_tensor_t* attention;         /**< Attention weights [capacity] */
    nimcp_gpu_tensor_t* output;            /**< Retrieved pattern [dim] */

    /* Parameters */
    uint32_t pattern_dim;                  /**< Pattern dimension */
    uint32_t capacity;                     /**< Maximum patterns */
    uint32_t pattern_count;                /**< Current pattern count */
    float beta;                            /**< Inverse temperature */
    nimcp_hopfield_gpu_mode_t mode;        /**< Attention mode */

    /* GPU context */
    nimcp_gpu_context_t* ctx;
} nimcp_omni_gpu_hopfield_t;

/**
 * @brief Predictive hierarchy GPU state
 */
typedef struct {
    nimcp_gpu_tensor_t** predictions;      /**< Top-down predictions per level */
    nimcp_gpu_tensor_t** errors;           /**< Bottom-up errors per level */
    nimcp_gpu_tensor_t** precisions;       /**< Precision weights per level */
    nimcp_gpu_tensor_t** states;           /**< Belief states per level */

    /* Inter-level weights */
    nimcp_gpu_tensor_t** up_weights;       /**< Bottom-up weights */
    nimcp_gpu_tensor_t** down_weights;     /**< Top-down weights */

    /* Configuration */
    uint32_t num_levels;                   /**< Number of hierarchy levels */
    uint32_t* level_dims;                  /**< Dimension per level */
    nimcp_precision_gpu_mode_t precision_mode;

    /* GPU context */
    nimcp_gpu_context_t* ctx;
} nimcp_omni_gpu_hierarchy_t;

/**
 * @brief Temporal replay GPU state
 */
typedef struct {
    nimcp_gpu_tensor_t* sequences;         /**< Replay buffer [capacity x seq_len x dim] */
    nimcp_gpu_tensor_t* priorities;        /**< Priority weights [capacity] */
    nimcp_gpu_tensor_t* timestamps;        /**< Timestamps [capacity] */
    nimcp_gpu_tensor_t* sampled;           /**< Sampled sequences buffer */

    /* Parameters */
    uint32_t state_dim;                    /**< State dimension */
    uint32_t capacity;                     /**< Buffer capacity */
    uint32_t sequence_len;                 /**< Sequence length */
    uint32_t head;                         /**< Circular buffer head */
    uint32_t count;                        /**< Current sequence count */

    /* GPU context */
    nimcp_gpu_context_t* ctx;
} nimcp_omni_gpu_replay_t;

/**
 * @brief Combined omnidirectional GPU state
 */
typedef struct {
    nimcp_omni_gpu_bidirectional_t* bidirectional;
    nimcp_omni_gpu_hopfield_t* hopfield;
    nimcp_omni_gpu_hierarchy_t* hierarchy;
    nimcp_omni_gpu_replay_t* replay;

    /* Shared precision tensor */
    nimcp_gpu_tensor_t* global_precision;

    /* Free energy buffer */
    nimcp_gpu_tensor_t* free_energy;

    /* GPU context */
    nimcp_gpu_context_t* ctx;
    bool initialized;
} nimcp_omni_gpu_state_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Create omnidirectional GPU state
 *
 * WHAT: Initialize GPU resources for omni inference
 * WHY:  Unified GPU state for all inference directions
 *
 * @param ctx GPU context
 * @param latent_dim Latent space dimension
 * @param hidden_dim Hidden layer dimension
 * @param num_patterns Max Hopfield patterns
 * @param num_levels Hierarchy levels
 * @return GPU state or NULL on failure
 */
NIMCP_EXPORT nimcp_omni_gpu_state_t* nimcp_omni_gpu_create(
    nimcp_gpu_context_t* ctx,
    uint32_t latent_dim,
    uint32_t hidden_dim,
    uint32_t num_patterns,
    uint32_t num_levels
);

/**
 * @brief Destroy omnidirectional GPU state
 *
 * @param state GPU state to destroy
 */
NIMCP_EXPORT void nimcp_omni_gpu_destroy(nimcp_omni_gpu_state_t* state);

/**
 * @brief Check if GPU state is valid
 *
 * @param state GPU state
 * @return true if valid and initialized
 */
NIMCP_EXPORT bool nimcp_omni_gpu_is_valid(const nimcp_omni_gpu_state_t* state);

/* ============================================================================
 * Bidirectional Prediction Kernels
 * ============================================================================ */

/**
 * @brief Bidirectional prediction on GPU
 *
 * WHAT: Predict in any direction using GPU
 * WHY:  Core omnidirectional inference operation
 * HOW:  Select appropriate weight matrix and run MLP
 *
 * @param state GPU state
 * @param input Input latent [batch_size x dim]
 * @param output Output prediction [batch_size x dim]
 * @param direction Prediction direction
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_omni_gpu_predict(
    nimcp_omni_gpu_state_t* state,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output,
    nimcp_omni_gpu_direction_t direction
);

/**
 * @brief Multi-direction prediction on GPU
 *
 * WHAT: Run multiple inference directions in parallel
 * WHY:  Efficient batch processing of different directions
 *
 * @param state GPU state
 * @param input Input latent [batch_size x dim]
 * @param outputs Array of output tensors per direction
 * @param directions Array of directions
 * @param num_directions Number of directions
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_omni_gpu_predict_multi(
    nimcp_omni_gpu_state_t* state,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t** outputs,
    const nimcp_omni_gpu_direction_t* directions,
    uint32_t num_directions
);

/**
 * @brief Precision-weighted prediction on GPU
 *
 * WHAT: Predict with per-dimension precision weighting
 * WHY:  FEP integration - weight by confidence
 *
 * @param state GPU state
 * @param input Input latent
 * @param precision Per-dimension precision
 * @param output Output prediction
 * @param direction Prediction direction
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_omni_gpu_predict_precision(
    nimcp_omni_gpu_state_t* state,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* precision,
    nimcp_gpu_tensor_t* output,
    nimcp_omni_gpu_direction_t direction
);

/* ============================================================================
 * Hopfield Attention Kernels
 * ============================================================================ */

/**
 * @brief Initialize Hopfield GPU memory
 *
 * @param state GPU state
 * @param pattern_dim Pattern dimension
 * @param capacity Maximum patterns
 * @param beta Inverse temperature
 * @param mode Attention mode
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_omni_gpu_hopfield_init(
    nimcp_omni_gpu_state_t* state,
    uint32_t pattern_dim,
    uint32_t capacity,
    float beta,
    nimcp_hopfield_gpu_mode_t mode
);

/**
 * @brief Store pattern in Hopfield GPU memory
 *
 * @param state GPU state
 * @param pattern Pattern to store [dim]
 * @return Pattern index or -1 on failure
 */
NIMCP_EXPORT int nimcp_omni_gpu_hopfield_store(
    nimcp_omni_gpu_state_t* state,
    const nimcp_gpu_tensor_t* pattern
);

/**
 * @brief Store batch of patterns
 *
 * @param state GPU state
 * @param patterns Patterns to store [num_patterns x dim]
 * @param num_patterns Number of patterns
 * @return Number stored or -1 on failure
 */
NIMCP_EXPORT int nimcp_omni_gpu_hopfield_store_batch(
    nimcp_omni_gpu_state_t* state,
    const nimcp_gpu_tensor_t* patterns,
    uint32_t num_patterns
);

/**
 * @brief Retrieve from Hopfield GPU memory
 *
 * WHAT: Content-addressable retrieval using attention
 * WHY:  Associative memory for omnidirectional inference
 * HOW:  Softmax attention over stored patterns
 *
 * @param state GPU state
 * @param query Query pattern [batch_size x dim]
 * @param output Retrieved pattern [batch_size x dim]
 * @param max_iterations Maximum convergence iterations
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_omni_gpu_hopfield_retrieve(
    nimcp_omni_gpu_state_t* state,
    const nimcp_gpu_tensor_t* query,
    nimcp_gpu_tensor_t* output,
    uint32_t max_iterations
);

/**
 * @brief Get top-k similar patterns on GPU
 *
 * @param state GPU state
 * @param query Query pattern
 * @param k Number of patterns to return
 * @param indices Output indices [k]
 * @param similarities Output similarities [k]
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_omni_gpu_hopfield_top_k(
    nimcp_omni_gpu_state_t* state,
    const nimcp_gpu_tensor_t* query,
    uint32_t k,
    uint32_t* indices,
    float* similarities
);

/**
 * @brief Compute Hopfield energy on GPU
 *
 * @param state GPU state
 * @param pattern Pattern to evaluate
 * @param energy Output energy value
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_omni_gpu_hopfield_energy(
    nimcp_omni_gpu_state_t* state,
    const nimcp_gpu_tensor_t* pattern,
    float* energy
);

/* ============================================================================
 * Predictive Hierarchy Kernels
 * ============================================================================ */

/**
 * @brief Initialize predictive hierarchy on GPU
 *
 * @param state GPU state
 * @param level_dims Dimension per level [num_levels]
 * @param num_levels Number of hierarchy levels
 * @param precision_mode Precision update mode
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_omni_gpu_hierarchy_init(
    nimcp_omni_gpu_state_t* state,
    const uint32_t* level_dims,
    uint32_t num_levels,
    nimcp_precision_gpu_mode_t precision_mode
);

/**
 * @brief Forward pass through hierarchy (bottom-up)
 *
 * WHAT: Propagate input up through hierarchy
 * WHY:  Bottom-up inference in predictive coding
 *
 * @param state GPU state
 * @param input Sensory input [batch_size x input_dim]
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_omni_gpu_hierarchy_forward(
    nimcp_omni_gpu_state_t* state,
    const nimcp_gpu_tensor_t* input
);

/**
 * @brief Backward pass through hierarchy (top-down)
 *
 * WHAT: Generate predictions from top to bottom
 * WHY:  Top-down predictions in predictive coding
 *
 * @param state GPU state
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_omni_gpu_hierarchy_backward(
    nimcp_omni_gpu_state_t* state
);

/**
 * @brief Compute prediction errors across all levels
 *
 * WHAT: Parallel error computation on GPU
 * WHY:  Core predictive coding operation
 * HOW:  ε = Π * (μ - x) at each level
 *
 * @param state GPU state
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_omni_gpu_hierarchy_compute_errors(
    nimcp_omni_gpu_state_t* state
);

/**
 * @brief Update belief states from errors
 *
 * @param state GPU state
 * @param learning_rate State update rate
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_omni_gpu_hierarchy_update_states(
    nimcp_omni_gpu_state_t* state,
    float learning_rate
);

/**
 * @brief Update precision weights
 *
 * @param state GPU state
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_omni_gpu_hierarchy_update_precision(
    nimcp_omni_gpu_state_t* state
);

/**
 * @brief Compute free energy across hierarchy
 *
 * WHAT: Sum of precision-weighted prediction errors
 * WHY:  FEP minimization objective
 *
 * @param state GPU state
 * @param free_energy Output total free energy
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_omni_gpu_hierarchy_free_energy(
    nimcp_omni_gpu_state_t* state,
    float* free_energy
);

/**
 * @brief Get level state from hierarchy
 *
 * @param state GPU state
 * @param level Level index
 * @param level_state Output state tensor
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_omni_gpu_hierarchy_get_level(
    const nimcp_omni_gpu_state_t* state,
    uint32_t level,
    nimcp_gpu_tensor_t* level_state
);

/* ============================================================================
 * Temporal Replay Kernels
 * ============================================================================ */

/**
 * @brief Initialize temporal replay buffer on GPU
 *
 * @param state GPU state
 * @param state_dim State dimension
 * @param capacity Buffer capacity
 * @param sequence_len Sequence length
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_omni_gpu_replay_init(
    nimcp_omni_gpu_state_t* state,
    uint32_t state_dim,
    uint32_t capacity,
    uint32_t sequence_len
);

/**
 * @brief Store sequence in replay buffer
 *
 * @param state GPU state
 * @param sequence State sequence [seq_len x dim]
 * @param priority Priority weight
 * @return Sequence index or -1 on failure
 */
NIMCP_EXPORT int nimcp_omni_gpu_replay_store(
    nimcp_omni_gpu_state_t* state,
    const nimcp_gpu_tensor_t* sequence,
    float priority
);

/**
 * @brief Sample sequences from replay buffer
 *
 * WHAT: GPU-accelerated priority sampling
 * WHY:  Efficient experience replay
 *
 * @param state GPU state
 * @param batch_size Number of sequences to sample
 * @param mode Sampling mode
 * @param sequences Output sampled sequences
 * @param indices Output sequence indices (can be NULL)
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_omni_gpu_replay_sample(
    nimcp_omni_gpu_state_t* state,
    uint32_t batch_size,
    nimcp_replay_gpu_mode_t mode,
    nimcp_gpu_tensor_t* sequences,
    uint32_t* indices
);

/**
 * @brief Forward sweep through replay sequence
 *
 * WHAT: Play sequence forward in time
 * WHY:  Forward temporal inference
 *
 * @param state GPU state
 * @param sequence_idx Sequence index to sweep
 * @param start Start position in sequence
 * @param length Sweep length
 * @param sweep_output Output swept states
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_omni_gpu_replay_forward_sweep(
    nimcp_omni_gpu_state_t* state,
    uint32_t sequence_idx,
    uint32_t start,
    uint32_t length,
    nimcp_gpu_tensor_t* sweep_output
);

/**
 * @brief Backward sweep through replay sequence
 *
 * WHAT: Play sequence backward in time
 * WHY:  Reverse temporal inference for planning
 *
 * @param state GPU state
 * @param sequence_idx Sequence index
 * @param end End position in sequence
 * @param length Sweep length
 * @param sweep_output Output swept states
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_omni_gpu_replay_backward_sweep(
    nimcp_omni_gpu_state_t* state,
    uint32_t sequence_idx,
    uint32_t end,
    uint32_t length,
    nimcp_gpu_tensor_t* sweep_output
);

/**
 * @brief Update priorities in replay buffer
 *
 * @param state GPU state
 * @param indices Sequence indices to update
 * @param priorities New priorities
 * @param count Number of updates
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_omni_gpu_replay_update_priorities(
    nimcp_omni_gpu_state_t* state,
    const uint32_t* indices,
    const float* priorities,
    uint32_t count
);

/* ============================================================================
 * Free Energy Aggregation Kernels
 * ============================================================================ */

/**
 * @brief Compute total free energy on GPU
 *
 * WHAT: Aggregate free energy from all sources
 * WHY:  FEP optimization objective
 *
 * @param state GPU state
 * @param prediction_error Prediction error tensor
 * @param precision Precision weights
 * @param total_fe Output total free energy
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_omni_gpu_compute_free_energy(
    nimcp_omni_gpu_state_t* state,
    const nimcp_gpu_tensor_t* prediction_error,
    const nimcp_gpu_tensor_t* precision,
    float* total_fe
);

/**
 * @brief Compute free energy gradient on GPU
 *
 * @param state GPU state
 * @param prediction_error Prediction error
 * @param precision Precision weights
 * @param gradient Output gradient
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_omni_gpu_compute_fe_gradient(
    nimcp_omni_gpu_state_t* state,
    const nimcp_gpu_tensor_t* prediction_error,
    const nimcp_gpu_tensor_t* precision,
    nimcp_gpu_tensor_t* gradient
);

/* ============================================================================
 * Weight Upload/Download API
 * ============================================================================ */

/**
 * @brief Upload bidirectional weights to GPU
 *
 * @param state GPU state
 * @param direction Which direction's weights
 * @param weights Weight data
 * @param bias Bias data
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_omni_gpu_upload_weights(
    nimcp_omni_gpu_state_t* state,
    nimcp_omni_gpu_direction_t direction,
    const float* weights,
    const float* bias
);

/**
 * @brief Download weights from GPU
 *
 * @param state GPU state
 * @param direction Which direction's weights
 * @param weights Output weight data
 * @param bias Output bias data
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_omni_gpu_download_weights(
    const nimcp_omni_gpu_state_t* state,
    nimcp_omni_gpu_direction_t direction,
    float* weights,
    float* bias
);

/**
 * @brief Upload hierarchy weights to GPU
 *
 * @param state GPU state
 * @param level Level index
 * @param up_weights Bottom-up weights
 * @param down_weights Top-down weights
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_omni_gpu_upload_hierarchy_weights(
    nimcp_omni_gpu_state_t* state,
    uint32_t level,
    const float* up_weights,
    const float* down_weights
);

/* ============================================================================
 * Utility API
 * ============================================================================ */

/**
 * @brief Synchronize GPU operations
 *
 * @param state GPU state
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_omni_gpu_synchronize(nimcp_omni_gpu_state_t* state);

/**
 * @brief Get GPU memory usage
 *
 * @param state GPU state
 * @param allocated_bytes Output allocated bytes
 * @param peak_bytes Output peak bytes
 */
NIMCP_EXPORT void nimcp_omni_gpu_memory_usage(
    const nimcp_omni_gpu_state_t* state,
    size_t* allocated_bytes,
    size_t* peak_bytes
);

/**
 * @brief Direction to string
 *
 * @param dir Direction
 * @return Human-readable string
 */
NIMCP_EXPORT const char* nimcp_omni_gpu_direction_to_string(
    nimcp_omni_gpu_direction_t dir
);

/**
 * @brief Hopfield mode to string
 *
 * @param mode Hopfield mode
 * @return Human-readable string
 */
NIMCP_EXPORT const char* nimcp_hopfield_gpu_mode_to_string(
    nimcp_hopfield_gpu_mode_t mode
);

/**
 * @brief Replay mode to string
 *
 * @param mode Replay mode
 * @return Human-readable string
 */
NIMCP_EXPORT const char* nimcp_replay_gpu_mode_to_string(
    nimcp_replay_gpu_mode_t mode
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OMNI_GPU_H */
