/**
 * @file nimcp_jepa_bidirectional.h
 * @brief Bidirectional JEPA Predictor - Omnidirectional Inference
 * @version 1.0.0
 * @date 2025-01-04
 *
 * WHAT: Bidirectional JEPA predictor enabling prediction in any direction
 * WHY:  Support omnidirectional inference for retrodiction, lateral, and hierarchical prediction
 * HOW:  Multiple specialized predictors with FEP precision weighting per direction
 *
 * THEORETICAL FOUNDATION:
 * =======================
 *
 * OMNIDIRECTIONAL INFERENCE:
 * --------------------------
 * Traditional predictive coding is unidirectional (forward in time).
 * Omnidirectional inference extends this to:
 *
 *   1. FORWARD: x_t -> x_{t+1} (standard prediction)
 *   2. BACKWARD: x_t -> x_{t-1} (retrodiction/smoothing)
 *   3. LATERAL: x^A -> x^B (cross-modal inference)
 *   4. HIERARCHICAL: x^L -> x^{L±1} (top-down/bottom-up)
 *   5. MASKED: fill-in-the-blank prediction
 *
 * Each direction has its own precision weight learned from errors:
 *
 *   F_total = Σ_d π_d × ||z_pred^d - z_target^d||²
 *
 * BIOLOGICAL BASIS:
 * -----------------
 * - Hippocampal replay (forward and reverse)
 * - Associative cortex (lateral connections)
 * - Cortical hierarchy (feedforward and feedback)
 * - Predictive coding (top-down predictions)
 *
 * INTEGRATION POINTS:
 * -------------------
 * - Bio-async: BIO_MSG_OMNI_* messages
 * - Immune: Prediction error triggers surveillance
 * - Training: Bidirectional gradient flow
 * - Logic: Forward/backward chaining alignment
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_JEPA_BIDIRECTIONAL_H
#define NIMCP_JEPA_BIDIRECTIONAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/jepa/nimcp_jepa_predictor.h"
#include "cognitive/jepa/nimcp_jepa_latent.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "cognitive/free_energy/nimcp_fep_context.h"
#include "utils/tensor/nimcp_tensor.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef NIMCP_ENABLE_CUDA
#include "gpu/context/nimcp_gpu_context.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Bio-async module ID for bidirectional JEPA */
#define BIO_MODULE_JEPA_BIDIRECTIONAL           0x0E10

/** @brief Maximum number of prediction directions */
#define JEPA_BIDIR_MAX_DIRECTIONS               8

/** @brief Default precision for new directions */
#define JEPA_BIDIR_DEFAULT_PRECISION            1.0f

/** @brief Precision learning rate */
#define JEPA_BIDIR_PRECISION_LR                 0.01f

/** @brief Minimum precision value (prevent division by zero) */
#define JEPA_BIDIR_MIN_PRECISION                0.001f

/** @brief Maximum precision value (prevent numerical overflow) */
#define JEPA_BIDIR_MAX_PRECISION                1000.0f

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Prediction direction types
 *
 * WHAT: Defines the direction of inference
 * WHY:  Different directions require different predictors
 */
typedef enum {
    JEPA_DIR_FORWARD = 0,       /**< Forward in time: x_t -> x_{t+1} */
    JEPA_DIR_BACKWARD,          /**< Backward (retrodiction): x_t -> x_{t-1} */
    JEPA_DIR_LATERAL,           /**< Cross-modal: x^A -> x^B */
    JEPA_DIR_HIERARCHICAL_UP,   /**< Bottom-up: x^L -> x^{L+1} */
    JEPA_DIR_HIERARCHICAL_DOWN, /**< Top-down: x^L -> x^{L-1} */
    JEPA_DIR_MASKED,            /**< Fill-in-the-blank prediction */
    JEPA_DIR_ASSOCIATIVE,       /**< Content-addressable retrieval */
    JEPA_DIR_COUNT              /**< Number of direction types */
} jepa_direction_t;

/**
 * @brief Bidirectional predictor state
 */
typedef enum {
    JEPA_BIDIR_STATE_IDLE = 0,      /**< No active prediction */
    JEPA_BIDIR_STATE_PREDICTING,    /**< Prediction in progress */
    JEPA_BIDIR_STATE_TRAINING,      /**< Training mode active */
    JEPA_BIDIR_STATE_ERROR          /**< Error state */
} jepa_bidir_state_t;

/**
 * @brief GPU acceleration mode
 */
typedef enum {
    JEPA_BIDIR_GPU_DISABLED = 0,    /**< CPU only */
    JEPA_BIDIR_GPU_AUTO,            /**< Auto-select based on workload */
    JEPA_BIDIR_GPU_PREFERRED,       /**< Prefer GPU if available */
    JEPA_BIDIR_GPU_REQUIRED         /**< Require GPU (fail if unavailable) */
} jepa_bidir_gpu_mode_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Per-direction predictor state
 */
typedef struct {
    jepa_predictor_t* predictor;    /**< Underlying JEPA predictor */
    jepa_direction_t direction;     /**< Direction type */
    float precision;                /**< Current precision weight (π) */
    float precision_ema;            /**< EMA of precision for stability */
    uint64_t prediction_count;      /**< Number of predictions made */
    float cumulative_error;         /**< Running sum of squared errors */
    bool enabled;                   /**< Is this direction active? */
} jepa_direction_state_t;

/**
 * @brief Bidirectional prediction result
 */
typedef struct {
    jepa_latent_t* prediction;      /**< Predicted embedding */
    jepa_direction_t direction;     /**< Direction used */
    float confidence;               /**< Prediction confidence [0,1] */
    float free_energy;              /**< Prediction free energy */
    float precision;                /**< Applied precision weight */
    uint64_t timestamp_us;          /**< Prediction timestamp */
} jepa_bidir_result_t;

/**
 * @brief Multi-direction prediction request
 */
typedef struct {
    jepa_direction_t* directions;   /**< Array of directions to predict */
    uint32_t num_directions;        /**< Number of directions */
    jepa_latent_t** inputs;         /**< Input embeddings per direction */
    bool parallel;                  /**< Enable parallel prediction */
} jepa_bidir_multi_request_t;

/**
 * @brief Multi-direction prediction results
 */
typedef struct {
    jepa_bidir_result_t* results;   /**< Array of results per direction */
    uint32_t num_results;           /**< Number of results */
    float total_free_energy;        /**< Sum of precision-weighted FE */
    float avg_confidence;           /**< Average confidence across directions */
} jepa_bidir_multi_result_t;

/**
 * @brief Bidirectional JEPA configuration
 */
typedef struct {
    /* Architecture */
    uint32_t embedding_dim;         /**< Latent embedding dimension */
    uint32_t hidden_dim;            /**< Hidden layer dimension */
    uint32_t num_layers;            /**< MLP layers per predictor */
    jepa_activation_t activation;   /**< Activation function */

    /* Direction configuration */
    bool enable_forward;            /**< Enable forward prediction */
    bool enable_backward;           /**< Enable backward prediction */
    bool enable_lateral;            /**< Enable lateral prediction */
    bool enable_hierarchical;       /**< Enable hierarchical prediction */
    bool enable_masked;             /**< Enable masked prediction */
    bool enable_associative;        /**< Enable associative prediction */

    /* Training parameters */
    float learning_rate;            /**< Base learning rate */
    float weight_decay;             /**< L2 regularization */
    float precision_lr;             /**< Precision learning rate */
    float dropout_rate;             /**< Dropout probability */

    /* FEP integration */
    bool enable_fep;                /**< Enable FEP precision weighting */
    float initial_precision;        /**< Initial precision for all directions */

    /* GPU configuration */
    jepa_bidir_gpu_mode_t gpu_mode; /**< GPU acceleration mode */
    uint32_t min_batch_for_gpu;     /**< Minimum batch size for GPU */

    /* Bio-async */
    bool enable_bio_async;          /**< Enable bio-async messaging */
} jepa_bidir_config_t;

/**
 * @brief Bidirectional JEPA statistics
 */
typedef struct {
    uint64_t total_predictions;     /**< Total predictions across all directions */
    uint64_t forward_predictions;   /**< Forward predictions */
    uint64_t backward_predictions;  /**< Backward predictions */
    uint64_t lateral_predictions;   /**< Lateral predictions */
    uint64_t hierarchical_predictions; /**< Hierarchical predictions */
    float avg_free_energy;          /**< Running average free energy */
    float avg_precision[JEPA_DIR_COUNT]; /**< Precision per direction */
    float avg_confidence;           /**< Running average confidence */
    uint64_t gpu_predictions;       /**< Predictions on GPU */
    uint64_t cpu_predictions;       /**< Predictions on CPU */
} jepa_bidir_stats_t;

/**
 * @brief Bidirectional JEPA predictor
 *
 * Main structure containing all direction-specific predictors
 * and managing omnidirectional inference.
 */
typedef struct jepa_bidirectional {
    /* Configuration */
    jepa_bidir_config_t config;

    /* Direction-specific predictors */
    jepa_direction_state_t directions[JEPA_DIR_COUNT];
    uint32_t active_direction_count;

    /* FEP integration */
    fep_context_t* fep_ctx;         /**< FEP context for precision */
    float total_free_energy;        /**< Accumulated free energy */

    /* State */
    jepa_bidir_state_t state;
    bool training_mode;
    uint64_t step_count;

    /* GPU context */
#ifdef NIMCP_ENABLE_CUDA
    struct nimcp_gpu_context_s* gpu_ctx;
    bool gpu_initialized;
#endif

    /* Statistics */
    jepa_bidir_stats_t stats;

    /* Thread safety */
    void* mutex;                    /**< Mutex for thread safety */
} jepa_bidirectional_t;

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Get default bidirectional configuration
 *
 * @param config Output configuration
 * @return NIMCP_SUCCESS on success
 */
int jepa_bidir_default_config(jepa_bidir_config_t* config);

/**
 * @brief Validate configuration
 *
 * @param config Configuration to validate
 * @return NIMCP_SUCCESS if valid
 */
int jepa_bidir_validate_config(const jepa_bidir_config_t* config);

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Create bidirectional JEPA predictor
 *
 * WHAT: Initialize omnidirectional inference system
 * WHY:  Enable prediction in any direction
 * HOW:  Create direction-specific predictors, initialize FEP
 *
 * @param config Configuration (NULL for defaults)
 * @return New predictor or NULL on failure
 */
jepa_bidirectional_t* jepa_bidirectional_create(const jepa_bidir_config_t* config);

/**
 * @brief Destroy bidirectional predictor
 *
 * @param bidir Predictor to destroy (NULL safe)
 */
void jepa_bidirectional_destroy(jepa_bidirectional_t* bidir);

/**
 * @brief Reset predictor to initial state
 *
 * @param bidir Predictor to reset
 * @return NIMCP_SUCCESS on success
 */
int jepa_bidirectional_reset(jepa_bidirectional_t* bidir);

/* ============================================================================
 * Prediction API
 * ============================================================================ */

/**
 * @brief Predict in a specific direction
 *
 * WHAT: Make prediction using specified direction
 * WHY:  Core omnidirectional inference operation
 * HOW:  Route to appropriate predictor, apply precision weighting
 *
 * @param bidir Bidirectional predictor
 * @param direction Prediction direction
 * @param input Input embedding
 * @param result Output result structure
 * @return NIMCP_SUCCESS on success
 */
int jepa_bidirectional_predict(jepa_bidirectional_t* bidir,
                                jepa_direction_t direction,
                                const jepa_latent_t* input,
                                jepa_bidir_result_t* result);

/**
 * @brief Predict in multiple directions simultaneously
 *
 * WHAT: Make predictions in multiple directions
 * WHY:  Efficient batch processing for omnidirectional inference
 * HOW:  Parallel prediction when GPU available
 *
 * @param bidir Bidirectional predictor
 * @param request Multi-direction request
 * @param result Multi-direction results
 * @return NIMCP_SUCCESS on success
 */
int jepa_bidirectional_predict_multi(jepa_bidirectional_t* bidir,
                                      const jepa_bidir_multi_request_t* request,
                                      jepa_bidir_multi_result_t* result);

/**
 * @brief Forward prediction (convenience wrapper)
 *
 * @param bidir Bidirectional predictor
 * @param input Input embedding
 * @param prediction Output prediction
 * @return NIMCP_SUCCESS on success
 */
int jepa_bidirectional_predict_forward(jepa_bidirectional_t* bidir,
                                        const jepa_latent_t* input,
                                        jepa_latent_t* prediction);

/**
 * @brief Backward prediction (retrodiction)
 *
 * @param bidir Bidirectional predictor
 * @param input Input embedding
 * @param prediction Output prediction
 * @return NIMCP_SUCCESS on success
 */
int jepa_bidirectional_predict_backward(jepa_bidirectional_t* bidir,
                                         const jepa_latent_t* input,
                                         jepa_latent_t* prediction);

/**
 * @brief Lateral prediction (cross-modal)
 *
 * @param bidir Bidirectional predictor
 * @param input Input embedding from modality A
 * @param prediction Output prediction for modality B
 * @return NIMCP_SUCCESS on success
 */
int jepa_bidirectional_predict_lateral(jepa_bidirectional_t* bidir,
                                        const jepa_latent_t* input,
                                        jepa_latent_t* prediction);

/**
 * @brief Hierarchical prediction (up or down)
 *
 * @param bidir Bidirectional predictor
 * @param input Input embedding
 * @param up If true, predict higher level; if false, predict lower
 * @param prediction Output prediction
 * @return NIMCP_SUCCESS on success
 */
int jepa_bidirectional_predict_hierarchical(jepa_bidirectional_t* bidir,
                                             const jepa_latent_t* input,
                                             bool up,
                                             jepa_latent_t* prediction);

/* ============================================================================
 * Free Energy API
 * ============================================================================ */

/**
 * @brief Compute total free energy across directions
 *
 * WHAT: Calculate precision-weighted sum of prediction errors
 * WHY:  FEP integration for belief updating
 * HOW:  F = Σ_d π_d × ||ε_d||²
 *
 * @param bidir Bidirectional predictor
 * @return Total free energy, NAN on error
 */
float jepa_bidirectional_compute_free_energy(jepa_bidirectional_t* bidir);

/**
 * @brief Update precision weights from errors
 *
 * WHAT: Learn precision for each direction
 * WHY:  Precision encodes inverse variance of errors
 * HOW:  π_d = 1 / E[ε_d²] with smoothing
 *
 * @param bidir Bidirectional predictor
 * @param direction Direction to update
 * @param error Prediction error
 * @return NIMCP_SUCCESS on success
 */
int jepa_bidirectional_update_precision(jepa_bidirectional_t* bidir,
                                         jepa_direction_t direction,
                                         float error);

/**
 * @brief Get precision for a direction
 *
 * @param bidir Bidirectional predictor
 * @param direction Direction
 * @return Precision value, NAN on error
 */
float jepa_bidirectional_get_precision(const jepa_bidirectional_t* bidir,
                                        jepa_direction_t direction);

/**
 * @brief Set precision for a direction
 *
 * @param bidir Bidirectional predictor
 * @param direction Direction
 * @param precision New precision value
 * @return NIMCP_SUCCESS on success
 */
int jepa_bidirectional_set_precision(jepa_bidirectional_t* bidir,
                                      jepa_direction_t direction,
                                      float precision);

/* ============================================================================
 * Training API
 * ============================================================================ */

/**
 * @brief Set training mode
 *
 * @param bidir Bidirectional predictor
 * @param training true for training, false for inference
 * @return NIMCP_SUCCESS on success
 */
int jepa_bidirectional_set_training(jepa_bidirectional_t* bidir, bool training);

/**
 * @brief Training step for a direction
 *
 * WHAT: Complete forward-backward-update cycle
 * WHY:  Learn to predict in specified direction
 * HOW:  Predict, compute error, backprop, update weights + precision
 *
 * @param bidir Bidirectional predictor
 * @param direction Direction to train
 * @param input Input embedding
 * @param target Target embedding
 * @param loss Output loss value (can be NULL)
 * @return NIMCP_SUCCESS on success
 */
int jepa_bidirectional_train_step(jepa_bidirectional_t* bidir,
                                   jepa_direction_t direction,
                                   const jepa_latent_t* input,
                                   const jepa_latent_t* target,
                                   float* loss);

/**
 * @brief Enable/disable a prediction direction
 *
 * @param bidir Bidirectional predictor
 * @param direction Direction to modify
 * @param enabled New state
 * @return NIMCP_SUCCESS on success
 */
int jepa_bidirectional_set_direction_enabled(jepa_bidirectional_t* bidir,
                                              jepa_direction_t direction,
                                              bool enabled);

/**
 * @brief Check if direction is enabled
 *
 * @param bidir Bidirectional predictor
 * @param direction Direction to check
 * @return true if enabled
 */
bool jepa_bidirectional_is_direction_enabled(const jepa_bidirectional_t* bidir,
                                              jepa_direction_t direction);

/* ============================================================================
 * GPU API
 * ============================================================================ */

#ifdef NIMCP_ENABLE_CUDA
/**
 * @brief Initialize GPU acceleration
 *
 * @param bidir Bidirectional predictor
 * @param gpu_ctx GPU context (NULL for auto-create)
 * @return NIMCP_SUCCESS on success
 */
int jepa_bidirectional_init_gpu(jepa_bidirectional_t* bidir,
                                 struct nimcp_gpu_context_s* gpu_ctx);

/**
 * @brief Check if GPU is available
 *
 * @param bidir Bidirectional predictor
 * @return true if GPU initialized and ready
 */
bool jepa_bidirectional_has_gpu(const jepa_bidirectional_t* bidir);

/**
 * @brief Get GPU context
 *
 * @param bidir Bidirectional predictor
 * @return GPU context or NULL
 */
struct nimcp_gpu_context_s* jepa_bidirectional_get_gpu_ctx(
    const jepa_bidirectional_t* bidir);
#endif

/* ============================================================================
 * Statistics API
 * ============================================================================ */

/**
 * @brief Get statistics
 *
 * @param bidir Bidirectional predictor
 * @param stats Output statistics
 * @return NIMCP_SUCCESS on success
 */
int jepa_bidirectional_get_stats(const jepa_bidirectional_t* bidir,
                                  jepa_bidir_stats_t* stats);

/**
 * @brief Reset statistics
 *
 * @param bidir Bidirectional predictor
 * @return NIMCP_SUCCESS on success
 */
int jepa_bidirectional_reset_stats(jepa_bidirectional_t* bidir);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * @param bidir Bidirectional predictor
 * @return NIMCP_SUCCESS on success
 */
int jepa_bidirectional_connect_bio_async(jepa_bidirectional_t* bidir);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bidir Bidirectional predictor
 * @return NIMCP_SUCCESS on success
 */
int jepa_bidirectional_disconnect_bio_async(jepa_bidirectional_t* bidir);

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

/**
 * @brief Convert direction to string
 *
 * @param direction Direction type
 * @return Human-readable string
 */
const char* jepa_direction_to_string(jepa_direction_t direction);

/**
 * @brief Convert state to string
 *
 * @param state State type
 * @return Human-readable string
 */
const char* jepa_bidir_state_to_string(jepa_bidir_state_t state);

/* ============================================================================
 * Result Management API
 * ============================================================================ */

/**
 * @brief Create prediction result
 *
 * @param dim Embedding dimension
 * @return New result or NULL
 */
jepa_bidir_result_t* jepa_bidir_result_create(uint32_t dim);

/**
 * @brief Destroy prediction result
 *
 * @param result Result to destroy (NULL safe)
 */
void jepa_bidir_result_destroy(jepa_bidir_result_t* result);

/**
 * @brief Create multi-result structure
 *
 * @param num_directions Number of directions
 * @param dim Embedding dimension
 * @return New multi-result or NULL
 */
jepa_bidir_multi_result_t* jepa_bidir_multi_result_create(uint32_t num_directions,
                                                           uint32_t dim);

/**
 * @brief Destroy multi-result structure
 *
 * @param result Multi-result to destroy (NULL safe)
 */
void jepa_bidir_multi_result_destroy(jepa_bidir_multi_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_JEPA_BIDIRECTIONAL_H */
