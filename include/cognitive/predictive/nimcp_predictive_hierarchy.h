/**
 * @file nimcp_predictive_hierarchy.h
 * @brief Predictive Coding Hierarchy - Bidirectional Inference
 * @version 1.0.0
 * @date 2025-01-04
 *
 * WHAT: Hierarchical predictive coding with bidirectional message passing
 * WHY:  Enable top-down predictions and bottom-up error propagation
 * HOW:  Multi-level hierarchy with precision-weighted prediction errors
 *
 * THEORETICAL FOUNDATION:
 * =======================
 *
 * PREDICTIVE CODING (Rao & Ballard, 1999; Friston, 2005):
 * -------------------------------------------------------
 * The brain implements hierarchical Bayesian inference:
 *
 *   1. TOP-DOWN: Higher levels predict lower level states
 *      μ_{L-1} = g(μ_L)
 *
 *   2. BOTTOM-UP: Lower levels propagate prediction errors
 *      ε_L = x_L - μ_L
 *
 *   3. BELIEF UPDATE: Minimize precision-weighted errors
 *      Δμ_L = -κ × (Π_L × ε_L - ∂g/∂μ_L × Π_{L-1} × ε_{L-1})
 *
 * Where:
 * - μ_L is the belief state at level L
 * - ε_L is the prediction error at level L
 * - Π_L is the precision (inverse variance) at level L
 * - g() is the generative model (top-down predictions)
 *
 * FREE ENERGY MINIMIZATION:
 * -------------------------
 * The hierarchy minimizes variational free energy:
 *
 *   F = Σ_L Π_L × ||ε_L||² + complexity
 *
 * BIOLOGICAL BASIS:
 * -----------------
 * - Cortical hierarchy (V1 → V2 → V4 → IT → PFC)
 * - Superficial pyramidal: bottom-up errors
 * - Deep pyramidal: top-down predictions
 * - Precision: gain modulation via neuromodulators
 *
 * INTEGRATION POINTS:
 * -------------------
 * - JEPA bidirectional: Hierarchical direction
 * - Bio-async: Prediction/error messages
 * - Immune: Error triggers surveillance
 * - Attention: Precision modulation
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_PREDICTIVE_HIERARCHY_H
#define NIMCP_PREDICTIVE_HIERARCHY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

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

/** @brief Bio-async module ID for predictive hierarchy */
#define BIO_MODULE_PREDICTIVE_HIERARCHY         0x0E30

/** @brief Maximum hierarchy levels */
#define PRED_HIER_MAX_LEVELS                    16

/** @brief Default number of levels */
#define PRED_HIER_DEFAULT_LEVELS                4

/** @brief Default belief update rate */
#define PRED_HIER_DEFAULT_UPDATE_RATE           0.1f

/** @brief Default precision */
#define PRED_HIER_DEFAULT_PRECISION             1.0f

/** @brief Minimum precision */
#define PRED_HIER_MIN_PRECISION                 0.001f

/** @brief Maximum precision */
#define PRED_HIER_MAX_PRECISION                 1000.0f

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Hierarchy update mode
 */
typedef enum {
    PRED_HIER_UPDATE_SEQUENTIAL = 0, /**< Update levels sequentially */
    PRED_HIER_UPDATE_PARALLEL,       /**< Update all levels in parallel */
    PRED_HIER_UPDATE_INTERLEAVED     /**< Alternate bottom-up/top-down */
} pred_hier_update_mode_t;

/**
 * @brief Generative model type
 */
typedef enum {
    PRED_HIER_GEN_LINEAR = 0,       /**< Linear generative model */
    PRED_HIER_GEN_MLP,              /**< MLP generative model */
    PRED_HIER_GEN_CONV,             /**< Convolutional (for spatial data) */
    PRED_HIER_GEN_ATTENTION         /**< Attention-based */
} pred_hier_gen_model_t;

/**
 * @brief GPU acceleration mode
 */
typedef enum {
    PRED_HIER_GPU_DISABLED = 0,     /**< CPU only */
    PRED_HIER_GPU_AUTO,             /**< Auto-select based on workload */
    PRED_HIER_GPU_PREFERRED,        /**< Prefer GPU if available */
    PRED_HIER_GPU_REQUIRED          /**< Require GPU */
} pred_hier_gpu_mode_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Single level in the hierarchy
 */
typedef struct pred_level {
    /* State */
    float* state;                   /**< Current belief state μ [dim] */
    float* prediction;              /**< Top-down prediction [dim] */
    float* prediction_error;        /**< Bottom-up error ε [dim] */
    float* precision;               /**< Precision weights Π [dim] */
    uint32_t dim;                   /**< Dimension of this level */

    /* Generative model (top-down) */
    float* gen_weights;             /**< Weights for g(μ_L) */
    float* gen_bias;                /**< Bias for g(μ_L) */
    uint32_t gen_hidden_dim;        /**< Hidden dimension if MLP */

    /* Recognition model (bottom-up, optional) */
    float* rec_weights;             /**< Recognition model weights */
    float* rec_bias;                /**< Recognition model bias */

    /* Gradient storage for learning */
    float* grad_gen_weights;        /**< Gradients for gen weights */
    float* grad_gen_bias;           /**< Gradients for gen bias */

    /* Hierarchy links */
    struct pred_level* above;       /**< Higher level (NULL if top) */
    struct pred_level* below;       /**< Lower level (NULL if bottom) */
    uint32_t level_index;           /**< Index in hierarchy (0 = bottom) */

    /* Statistics */
    float avg_error;                /**< Running average error */
    float avg_precision;            /**< Running average precision */
    uint64_t update_count;          /**< Number of updates */
} pred_level_t;

/**
 * @brief Prediction result for a level
 */
typedef struct {
    float* prediction;              /**< Predicted state [dim] */
    float* error;                   /**< Prediction error [dim] */
    float precision_weighted_error; /**< Σ Π × ε² */
    float free_energy;              /**< Level free energy */
    uint32_t level_index;           /**< Level index */
} pred_level_result_t;

/**
 * @brief Full hierarchy result
 */
typedef struct {
    pred_level_result_t* level_results; /**< Per-level results */
    uint32_t num_levels;            /**< Number of levels */
    float total_free_energy;        /**< Total hierarchy FE */
    float complexity;               /**< Model complexity term */
    float accuracy;                 /**< Data fit term */
    uint64_t timestamp_us;          /**< Timestamp */
} pred_hier_result_t;

/**
 * @brief Level configuration
 */
typedef struct {
    uint32_t dim;                   /**< State dimension */
    uint32_t gen_hidden_dim;        /**< Generative model hidden dim */
    pred_hier_gen_model_t gen_type; /**< Generative model type */
    float initial_precision;        /**< Initial precision */
    float precision_lr;             /**< Precision learning rate */
    bool learnable_precision;       /**< Learn precision from errors */
} pred_level_config_t;

/**
 * @brief Hierarchy configuration
 */
typedef struct {
    /* Architecture */
    uint32_t num_levels;            /**< Number of hierarchy levels */
    pred_level_config_t* level_configs; /**< Per-level configuration */
    pred_hier_update_mode_t update_mode; /**< Update mode */

    /* Learning parameters */
    float state_update_rate;        /**< Belief update rate κ */
    float weight_lr;                /**< Weight learning rate */
    float precision_lr;             /**< Precision learning rate */
    bool enable_learning;           /**< Enable weight learning */
    bool enable_lateral;            /**< Enable lateral connections */

    /* FEP integration */
    bool enable_fep;                /**< Enable FEP integration */
    float complexity_weight;        /**< Weight on complexity term */

    /* GPU configuration */
    pred_hier_gpu_mode_t gpu_mode;  /**< GPU acceleration mode */

    /* Bio-async */
    bool enable_bio_async;          /**< Enable bio-async messaging */
} pred_hier_config_t;

/**
 * @brief Hierarchy statistics
 */
typedef struct {
    uint64_t forward_passes;        /**< Bottom-up passes */
    uint64_t backward_passes;       /**< Top-down passes */
    uint64_t full_updates;          /**< Full belief updates */
    float avg_free_energy;          /**< Average total free energy */
    float* avg_level_error;         /**< Average error per level */
    float* avg_level_precision;     /**< Average precision per level */
    uint64_t gpu_updates;           /**< GPU update count */
    uint64_t cpu_updates;           /**< CPU update count */
} pred_hier_stats_t;

/**
 * @brief Predictive hierarchy state
 */
typedef struct predictive_hierarchy {
    /* Configuration */
    pred_hier_config_t config;

    /* Hierarchy structure */
    pred_level_t** levels;          /**< Array of levels */
    uint32_t num_levels;            /**< Number of levels */
    pred_level_t* bottom;           /**< Pointer to bottom level */
    pred_level_t* top;              /**< Pointer to top level */

    /* FEP integration */
    float total_free_energy;        /**< Current total free energy */
    float complexity;               /**< Complexity term */
    float accuracy;                 /**< Accuracy term */

    /* State */
    bool training_mode;             /**< Training vs inference */
    uint64_t step_count;            /**< Update step counter */

    /* GPU resources */
#ifdef NIMCP_ENABLE_CUDA
    struct nimcp_gpu_context_s* gpu_ctx;
    bool gpu_initialized;
#endif

    /* Statistics */
    pred_hier_stats_t stats;

    /* Thread safety */
    void* mutex;
} predictive_hierarchy_t;

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Get default hierarchy configuration
 *
 * @param config Output configuration
 * @return NIMCP_SUCCESS on success
 */
int pred_hier_default_config(pred_hier_config_t* config);

/**
 * @brief Create simple hierarchy with uniform dimensions
 *
 * @param config Output configuration
 * @param num_levels Number of levels
 * @param dims Dimension per level [num_levels]
 * @return NIMCP_SUCCESS on success
 */
int pred_hier_simple_config(pred_hier_config_t* config,
                             uint32_t num_levels,
                             const uint32_t* dims);

/**
 * @brief Free level configs in configuration
 *
 * @param config Configuration to clean up
 */
void pred_hier_free_config(pred_hier_config_t* config);

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Create predictive hierarchy
 *
 * WHAT: Initialize hierarchical predictive coding system
 * WHY:  Enable bidirectional inference in cortical-like hierarchy
 * HOW:  Create levels, link together, initialize generative models
 *
 * @param config Configuration
 * @return New hierarchy or NULL on failure
 */
predictive_hierarchy_t* pred_hier_create(const pred_hier_config_t* config);

/**
 * @brief Destroy hierarchy
 *
 * @param hier Hierarchy to destroy (NULL safe)
 */
void pred_hier_destroy(predictive_hierarchy_t* hier);

/**
 * @brief Reset hierarchy to initial state
 *
 * @param hier Hierarchy to reset
 * @return NIMCP_SUCCESS on success
 */
int pred_hier_reset(predictive_hierarchy_t* hier);

/* ============================================================================
 * Inference API
 * ============================================================================ */

/**
 * @brief Forward pass (bottom-up)
 *
 * WHAT: Propagate input up through hierarchy
 * WHY:  Feed sensory data, compute prediction errors
 * HOW:  Set bottom state, compute errors at each level
 *
 * @param hier Predictive hierarchy
 * @param input Input to bottom level [bottom_dim]
 * @return NIMCP_SUCCESS on success
 */
int pred_hier_forward(predictive_hierarchy_t* hier,
                       const float* input);

/**
 * @brief Backward pass (top-down)
 *
 * WHAT: Generate predictions from top to bottom
 * WHY:  Top-down predictions for comparison with bottom-up
 * HOW:  Apply generative model at each level
 *
 * @param hier Predictive hierarchy
 * @return NIMCP_SUCCESS on success
 */
int pred_hier_backward(predictive_hierarchy_t* hier);

/**
 * @brief Full belief update
 *
 * WHAT: Update all beliefs to minimize free energy
 * WHY:  Core predictive coding inference
 * HOW:  Combine top-down and bottom-up to update μ
 *
 * @param hier Predictive hierarchy
 * @param input Sensory input [bottom_dim]
 * @param result Output result (can be NULL)
 * @return NIMCP_SUCCESS on success
 */
int pred_hier_update(predictive_hierarchy_t* hier,
                      const float* input,
                      pred_hier_result_t* result);

/**
 * @brief Get prediction at a level
 *
 * @param hier Predictive hierarchy
 * @param level_index Level index (0 = bottom)
 * @param prediction Output prediction [level_dim]
 * @return NIMCP_SUCCESS on success
 */
int pred_hier_get_prediction(const predictive_hierarchy_t* hier,
                              uint32_t level_index,
                              float* prediction);

/**
 * @brief Get prediction error at a level
 *
 * @param hier Predictive hierarchy
 * @param level_index Level index
 * @param error Output error [level_dim]
 * @return NIMCP_SUCCESS on success
 */
int pred_hier_get_error(const predictive_hierarchy_t* hier,
                         uint32_t level_index,
                         float* error);

/**
 * @brief Get state at a level
 *
 * @param hier Predictive hierarchy
 * @param level_index Level index
 * @param state Output state [level_dim]
 * @return NIMCP_SUCCESS on success
 */
int pred_hier_get_state(const predictive_hierarchy_t* hier,
                         uint32_t level_index,
                         float* state);

/**
 * @brief Set state at a level
 *
 * @param hier Predictive hierarchy
 * @param level_index Level index
 * @param state New state [level_dim]
 * @return NIMCP_SUCCESS on success
 */
int pred_hier_set_state(predictive_hierarchy_t* hier,
                         uint32_t level_index,
                         const float* state);

/* ============================================================================
 * Free Energy API
 * ============================================================================ */

/**
 * @brief Compute total free energy
 *
 * WHAT: Calculate variational free energy across hierarchy
 * WHY:  Free energy is the objective being minimized
 * HOW:  F = Σ_L Π_L × ||ε_L||² + complexity
 *
 * @param hier Predictive hierarchy
 * @return Total free energy, NAN on error
 */
float pred_hier_compute_free_energy(predictive_hierarchy_t* hier);

/**
 * @brief Get free energy at a level
 *
 * @param hier Predictive hierarchy
 * @param level_index Level index
 * @return Level free energy, NAN on error
 */
float pred_hier_get_level_free_energy(const predictive_hierarchy_t* hier,
                                       uint32_t level_index);

/**
 * @brief Update precision at a level
 *
 * @param hier Predictive hierarchy
 * @param level_index Level index
 * @param precision New precision [level_dim]
 * @return NIMCP_SUCCESS on success
 */
int pred_hier_set_precision(predictive_hierarchy_t* hier,
                             uint32_t level_index,
                             const float* precision);

/**
 * @brief Get precision at a level
 *
 * @param hier Predictive hierarchy
 * @param level_index Level index
 * @param precision Output precision [level_dim]
 * @return NIMCP_SUCCESS on success
 */
int pred_hier_get_precision(const predictive_hierarchy_t* hier,
                             uint32_t level_index,
                             float* precision);

/* ============================================================================
 * Training API
 * ============================================================================ */

/**
 * @brief Set training mode
 *
 * @param hier Predictive hierarchy
 * @param training true for training, false for inference
 * @return NIMCP_SUCCESS on success
 */
int pred_hier_set_training(predictive_hierarchy_t* hier, bool training);

/**
 * @brief Learning step (update weights)
 *
 * WHAT: Update generative model weights
 * WHY:  Learn better predictions
 * HOW:  Gradient descent on prediction errors
 *
 * @param hier Predictive hierarchy
 * @param input Sensory input
 * @param loss Output loss (can be NULL)
 * @return NIMCP_SUCCESS on success
 */
int pred_hier_learn_step(predictive_hierarchy_t* hier,
                          const float* input,
                          float* loss);

/**
 * @brief Update precision from errors
 *
 * WHAT: Learn precision (inverse variance) from errors
 * WHY:  Precision encodes reliability/confidence
 * HOW:  π = 1 / E[ε²] with smoothing
 *
 * @param hier Predictive hierarchy
 * @return NIMCP_SUCCESS on success
 */
int pred_hier_update_precision(predictive_hierarchy_t* hier);

/* ============================================================================
 * GPU API
 * ============================================================================ */

#ifdef NIMCP_ENABLE_CUDA
/**
 * @brief Initialize GPU acceleration
 *
 * @param hier Predictive hierarchy
 * @param gpu_ctx GPU context (NULL for auto-create)
 * @return NIMCP_SUCCESS on success
 */
int pred_hier_init_gpu(predictive_hierarchy_t* hier,
                        struct nimcp_gpu_context_s* gpu_ctx);

/**
 * @brief Check if GPU is available
 *
 * @param hier Predictive hierarchy
 * @return true if GPU initialized
 */
bool pred_hier_has_gpu(const predictive_hierarchy_t* hier);
#endif

/* ============================================================================
 * Statistics API
 * ============================================================================ */

/**
 * @brief Get statistics
 *
 * @param hier Predictive hierarchy
 * @param stats Output statistics
 * @return NIMCP_SUCCESS on success
 */
int pred_hier_get_stats(const predictive_hierarchy_t* hier,
                         pred_hier_stats_t* stats);

/**
 * @brief Reset statistics
 *
 * @param hier Predictive hierarchy
 * @return NIMCP_SUCCESS on success
 */
int pred_hier_reset_stats(predictive_hierarchy_t* hier);

/**
 * @brief Get number of levels
 *
 * @param hier Predictive hierarchy
 * @return Number of levels
 */
uint32_t pred_hier_num_levels(const predictive_hierarchy_t* hier);

/**
 * @brief Get level dimension
 *
 * @param hier Predictive hierarchy
 * @param level_index Level index
 * @return Level dimension, 0 on error
 */
uint32_t pred_hier_level_dim(const predictive_hierarchy_t* hier,
                              uint32_t level_index);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * @param hier Predictive hierarchy
 * @return NIMCP_SUCCESS on success
 */
int pred_hier_connect_bio_async(predictive_hierarchy_t* hier);

/**
 * @brief Disconnect from bio-async router
 *
 * @param hier Predictive hierarchy
 * @return NIMCP_SUCCESS on success
 */
int pred_hier_disconnect_bio_async(predictive_hierarchy_t* hier);

/* ============================================================================
 * Result Management API
 * ============================================================================ */

/**
 * @brief Create hierarchy result
 *
 * @param num_levels Number of levels
 * @param dims Dimension per level [num_levels]
 * @return New result or NULL
 */
pred_hier_result_t* pred_hier_result_create(uint32_t num_levels,
                                             const uint32_t* dims);

/**
 * @brief Destroy hierarchy result
 *
 * @param result Result to destroy (NULL safe)
 */
void pred_hier_result_destroy(pred_hier_result_t* result);

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

/**
 * @brief Convert update mode to string
 *
 * @param mode Update mode
 * @return Human-readable string
 */
const char* pred_hier_update_mode_to_string(pred_hier_update_mode_t mode);

/**
 * @brief Convert generative model type to string
 *
 * @param type Generative model type
 * @return Human-readable string
 */
const char* pred_hier_gen_model_to_string(pred_hier_gen_model_t type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PREDICTIVE_HIERARCHY_H */
