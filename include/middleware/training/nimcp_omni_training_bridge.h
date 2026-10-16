/**
 * @file nimcp_omni_training_bridge.h
 * @brief Omnidirectional Inference to Training Module Bridge
 * @version 1.0.0
 * @date 2025-01-04
 *
 * WHAT: Bridge integrating omnidirectional inference with training pipeline
 * WHY:  Enable bidirectional gradient flow through predictive hierarchy,
 *       experience replay for training, and curriculum based on prediction difficulty
 * HOW:  Prediction errors → gradients, Hopfield patterns → training targets,
 *       Temporal replay → experience buffer, Pred hierarchy → layer-wise training
 *
 * THEORETICAL FOUNDATION:
 * =======================
 *
 * PREDICTIVE CODING AND GRADIENT FLOW:
 * ------------------------------------
 * Prediction errors in the hierarchy provide natural gradient signals:
 *
 *   1. Forward: Bottom-up errors drive weight updates
 *      Δw = -η × ε × ∂g/∂w  (prediction error × generative model gradient)
 *
 *   2. Backward: Top-down predictions guide learning targets
 *      Learning targets derived from higher-level predictions
 *
 *   3. Bidirectional: Both streams inform training
 *      Combined gradients for balanced learning
 *
 * CONTRASTIVE LEARNING WITH HOPFIELD:
 * -----------------------------------
 * Hopfield patterns provide natural contrastive targets:
 *
 *   - Positive: Correct pattern retrievals
 *   - Negative: Perturbed or incorrect retrievals
 *   - Training objective: Maximize similarity to positives, minimize to negatives
 *
 * EXPERIENCE REPLAY INTEGRATION:
 * ------------------------------
 * Temporal replay buffer provides training samples:
 *
 *   - Forward replay: Sequential training data
 *   - Backward replay: Credit assignment via temporal difference
 *   - Priority sampling: Focus on high-error transitions
 *
 * CURRICULUM LEARNING:
 * --------------------
 * Prediction difficulty informs curriculum:
 *
 *   - Easy: Low prediction error → standard learning rate
 *   - Medium: Moderate PE → focused attention
 *   - Hard: High PE → increased iterations, reduced LR
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_OMNI_TRAINING_BRIDGE_H
#define NIMCP_OMNI_TRAINING_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct omni_training_bridge omni_training_bridge_t;
typedef struct jepa_bidirectional jepa_bidirectional_t;
typedef struct hopfield_memory hopfield_memory_t;
typedef struct predictive_hierarchy predictive_hierarchy_t;
typedef struct temporal_replay temporal_replay_t;
typedef struct gradient_manager gradient_manager_t;
typedef struct nimcp_brain_training_ctx nimcp_brain_training_ctx_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Bio-async module ID for omni-training bridge */
#define BIO_MODULE_OMNI_TRAINING_BRIDGE        0x0E51

/** @brief Default batch size for replay training */
#define OMNI_TRAINING_DEFAULT_BATCH_SIZE       32

/** @brief Default replay ratio (replay samples per online sample) */
#define OMNI_TRAINING_DEFAULT_REPLAY_RATIO     4

/** @brief Default curriculum difficulty threshold */
#define OMNI_TRAINING_CURRICULUM_EASY_THRESHOLD    1.0f
#define OMNI_TRAINING_CURRICULUM_MEDIUM_THRESHOLD  3.0f

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Training mode for omni inference
 */
typedef enum {
    OMNI_TRAIN_MODE_INFERENCE = 0,   /**< Pure inference, no training */
    OMNI_TRAIN_MODE_ONLINE,          /**< Online learning from predictions */
    OMNI_TRAIN_MODE_REPLAY,          /**< Training from replay buffer */
    OMNI_TRAIN_MODE_CONTRASTIVE,     /**< Contrastive learning with Hopfield */
    OMNI_TRAIN_MODE_MIXED            /**< All training modes combined */
} omni_training_mode_t;

/**
 * @brief Curriculum difficulty level
 */
typedef enum {
    OMNI_DIFFICULTY_EASY = 0,        /**< Low prediction error */
    OMNI_DIFFICULTY_MEDIUM,          /**< Moderate prediction error */
    OMNI_DIFFICULTY_HARD,            /**< High prediction error */
    OMNI_DIFFICULTY_ADAPTIVE         /**< Dynamically adjusted */
} omni_difficulty_t;

/**
 * @brief Gradient source
 */
typedef enum {
    OMNI_GRAD_FORWARD = 0,           /**< Forward (bottom-up) errors */
    OMNI_GRAD_BACKWARD,              /**< Backward (top-down) predictions */
    OMNI_GRAD_BIDIRECTIONAL,         /**< Combined bidirectional */
    OMNI_GRAD_CONTRASTIVE,           /**< Contrastive from Hopfield */
    OMNI_GRAD_TEMPORAL               /**< Temporal difference from replay */
} omni_gradient_source_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Omni inference effects on training
 */
typedef struct {
    float* pred_error_gradients;     /**< Gradients from prediction errors */
    uint32_t num_levels;             /**< Number of hierarchy levels */
    float contrastive_loss;          /**< Contrastive loss from Hopfield */
    float temporal_loss;             /**< TD loss from replay */
    float total_loss;                /**< Combined loss */
    omni_difficulty_t difficulty;    /**< Current difficulty level */
    float learning_rate_scale;       /**< Suggested LR scaling */
} omni_to_training_effects_t;

/**
 * @brief Training effects on omni inference
 */
typedef struct {
    float weight_update_magnitude;   /**< Magnitude of weight updates */
    float precision_learning_rate;   /**< LR for precision learning */
    bool enable_backward_training;   /**< Train backward predictor */
    bool enable_lateral_training;    /**< Train lateral predictor */
    uint32_t replay_batch_size;      /**< Batch size for replay */
    uint32_t replay_sequence_length; /**< Sequence length for replay */
} training_to_omni_effects_t;

/**
 * @brief Contrastive sample pair
 */
typedef struct {
    float* positive;                 /**< Positive example [dim] */
    float* negative;                 /**< Negative example [dim] */
    uint32_t dim;                    /**< Dimension */
    float margin;                    /**< Contrastive margin */
} omni_contrastive_pair_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Training mode */
    omni_training_mode_t mode;       /**< Training mode */
    bool enable_curriculum;          /**< Enable curriculum learning */

    /* Replay configuration */
    uint32_t replay_batch_size;      /**< Batch size for replay */
    uint32_t replay_ratio;           /**< Replay samples per online sample */
    bool use_priority_replay;        /**< Use prioritized replay */
    float priority_exponent;         /**< Priority exponent alpha */

    /* Contrastive configuration */
    float contrastive_margin;        /**< Margin for contrastive loss */
    float contrastive_temperature;   /**< Temperature for similarity */
    uint32_t num_negatives;          /**< Number of negative samples */

    /* Gradient configuration */
    omni_gradient_source_t grad_source; /**< Gradient source */
    float forward_grad_weight;       /**< Weight for forward gradients */
    float backward_grad_weight;      /**< Weight for backward gradients */
    bool enable_gradient_clipping;   /**< Enable gradient clipping */
    float gradient_clip_norm;        /**< Maximum gradient norm */

    /* Curriculum configuration */
    float easy_threshold;            /**< PE threshold for easy */
    float medium_threshold;          /**< PE threshold for medium */
    float hard_lr_scale;             /**< LR scale for hard samples */

    /* Integration */
    bool enable_bio_async;           /**< Enable bio-async messaging */
    bool enable_logging;             /**< Enable logging */
} omni_training_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;          /**< Total bridge updates */
    uint64_t training_steps;         /**< Total training steps */
    uint64_t online_steps;           /**< Online training steps */
    uint64_t replay_steps;           /**< Replay training steps */
    uint64_t contrastive_steps;      /**< Contrastive training steps */
    float avg_pred_error;            /**< Average prediction error */
    float avg_contrastive_loss;      /**< Average contrastive loss */
    float avg_temporal_loss;         /**< Average temporal loss */
    float avg_total_loss;            /**< Average total loss */
    uint64_t easy_samples;           /**< Easy sample count */
    uint64_t medium_samples;         /**< Medium sample count */
    uint64_t hard_samples;           /**< Hard sample count */
} omni_training_stats_t;

/**
 * @brief Omni-training bridge structure
 */
struct omni_training_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge */

    omni_training_config_t config;   /**< Configuration */

    /* Connected systems */
    jepa_bidirectional_t* jepa;      /**< Bidirectional JEPA */
    hopfield_memory_t* hopfield;     /**< Hopfield memory */
    predictive_hierarchy_t* pred_hier; /**< Predictive hierarchy */
    temporal_replay_t* replay;       /**< Temporal replay */
    gradient_manager_t* grad_mgr;    /**< Gradient manager */
    nimcp_brain_training_ctx_t* train_ctx; /**< Brain training context */

    /* Computed effects */
    omni_to_training_effects_t omni_effects;    /**< Omni → training */
    training_to_omni_effects_t training_effects; /**< Training → omni */

    /* Workspace */
    float* gradient_buffer;          /**< Gradient accumulation buffer */
    uint32_t gradient_buffer_size;   /**< Gradient buffer size */

    /* Statistics */
    omni_training_stats_t stats;

    /* Bio-async integration */
    void* bio_context;               /**< Bio-async module context */
    bool bio_async_connected;        /**< Bio-async connection state */

    /* Thread safety: uses base.mutex from bridge_base_t */
};

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * @param config Output configuration
 * @return NIMCP_SUCCESS on success
 */
int omni_training_default_config(omni_training_config_t* config);

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Create omni-training bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 */
omni_training_bridge_t* omni_training_bridge_create(
    const omni_training_config_t* config);

/**
 * @brief Destroy bridge
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void omni_training_bridge_destroy(omni_training_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect to bidirectional JEPA
 *
 * @param bridge Bridge
 * @param jepa Bidirectional JEPA (NULL to disconnect)
 * @return NIMCP_SUCCESS on success
 */
int omni_training_connect_jepa(omni_training_bridge_t* bridge,
                                jepa_bidirectional_t* jepa);

/**
 * @brief Connect to Hopfield memory
 *
 * @param bridge Bridge
 * @param hopfield Hopfield memory (NULL to disconnect)
 * @return NIMCP_SUCCESS on success
 */
int omni_training_connect_hopfield(omni_training_bridge_t* bridge,
                                    hopfield_memory_t* hopfield);

/**
 * @brief Connect to predictive hierarchy
 *
 * @param bridge Bridge
 * @param pred_hier Predictive hierarchy (NULL to disconnect)
 * @return NIMCP_SUCCESS on success
 */
int omni_training_connect_pred_hier(omni_training_bridge_t* bridge,
                                     predictive_hierarchy_t* pred_hier);

/**
 * @brief Connect to temporal replay
 *
 * @param bridge Bridge
 * @param replay Temporal replay (NULL to disconnect)
 * @return NIMCP_SUCCESS on success
 */
int omni_training_connect_replay(omni_training_bridge_t* bridge,
                                  temporal_replay_t* replay);

/**
 * @brief Connect to gradient manager
 *
 * @param bridge Bridge
 * @param grad_mgr Gradient manager (NULL to disconnect)
 * @return NIMCP_SUCCESS on success
 */
int omni_training_connect_gradient_manager(omni_training_bridge_t* bridge,
                                            gradient_manager_t* grad_mgr);

/**
 * @brief Connect to brain training context
 *
 * @param bridge Bridge
 * @param train_ctx Training context (NULL to disconnect)
 * @return NIMCP_SUCCESS on success
 */
int omni_training_connect_training_ctx(omni_training_bridge_t* bridge,
                                        nimcp_brain_training_ctx_t* train_ctx);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Update bridge (compute bidirectional effects)
 *
 * @param bridge Bridge
 * @return NIMCP_SUCCESS on success
 */
int omni_training_update(omni_training_bridge_t* bridge);

/**
 * @brief Perform training step
 *
 * WHAT: Execute one training step using omni inference
 * WHY:  Update model weights based on prediction errors
 * HOW:  Collect gradients from PE, apply via gradient manager
 *
 * @param bridge Bridge
 * @param loss Output loss (can be NULL)
 * @return NIMCP_SUCCESS on success
 */
int omni_training_step(omni_training_bridge_t* bridge, float* loss);

/**
 * @brief Perform replay training step
 *
 * WHAT: Train from temporal replay buffer
 * WHY:  Experience replay for off-policy learning
 * HOW:  Sample batch, compute TD gradients, apply updates
 *
 * @param bridge Bridge
 * @param loss Output loss (can be NULL)
 * @return NIMCP_SUCCESS on success
 */
int omni_training_replay_step(omni_training_bridge_t* bridge, float* loss);

/**
 * @brief Perform contrastive training step
 *
 * WHAT: Train with contrastive loss using Hopfield patterns
 * WHY:  Learn discriminative representations
 * HOW:  Sample positive/negative pairs, compute contrastive gradients
 *
 * @param bridge Bridge
 * @param loss Output loss (can be NULL)
 * @return NIMCP_SUCCESS on success
 */
int omni_training_contrastive_step(omni_training_bridge_t* bridge, float* loss);

/* ============================================================================
 * Gradient API
 * ============================================================================ */

/**
 * @brief Get prediction error gradients
 *
 * @param bridge Bridge
 * @param level Hierarchy level
 * @param gradients Output gradients [level_dim]
 * @return NIMCP_SUCCESS on success
 */
int omni_training_get_pe_gradients(const omni_training_bridge_t* bridge,
                                    uint32_t level,
                                    float* gradients);

/**
 * @brief Accumulate gradients from all sources
 *
 * @param bridge Bridge
 * @return NIMCP_SUCCESS on success
 */
int omni_training_accumulate_gradients(omni_training_bridge_t* bridge);

/**
 * @brief Apply accumulated gradients
 *
 * @param bridge Bridge
 * @return NIMCP_SUCCESS on success
 */
int omni_training_apply_gradients(omni_training_bridge_t* bridge);

/**
 * @brief Zero gradient buffer
 *
 * @param bridge Bridge
 * @return NIMCP_SUCCESS on success
 */
int omni_training_zero_gradients(omni_training_bridge_t* bridge);

/* ============================================================================
 * Curriculum API
 * ============================================================================ */

/**
 * @brief Get current difficulty level
 *
 * @param bridge Bridge
 * @return Current difficulty
 */
omni_difficulty_t omni_training_get_difficulty(
    const omni_training_bridge_t* bridge);

/**
 * @brief Set difficulty level
 *
 * @param bridge Bridge
 * @param difficulty Difficulty level
 * @return NIMCP_SUCCESS on success
 */
int omni_training_set_difficulty(omni_training_bridge_t* bridge,
                                  omni_difficulty_t difficulty);

/**
 * @brief Get suggested learning rate scale
 *
 * @param bridge Bridge
 * @return LR scale factor based on difficulty
 */
float omni_training_get_lr_scale(const omni_training_bridge_t* bridge);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current omni-to-training effects
 *
 * @param bridge Bridge
 * @param effects Output effects
 * @return NIMCP_SUCCESS on success
 */
int omni_training_get_omni_effects(const omni_training_bridge_t* bridge,
                                    omni_to_training_effects_t* effects);

/**
 * @brief Get current training-to-omni effects
 *
 * @param bridge Bridge
 * @param effects Output effects
 * @return NIMCP_SUCCESS on success
 */
int omni_training_get_training_effects(const omni_training_bridge_t* bridge,
                                        training_to_omni_effects_t* effects);

/**
 * @brief Get statistics
 *
 * @param bridge Bridge
 * @param stats Output statistics
 * @return NIMCP_SUCCESS on success
 */
int omni_training_get_stats(const omni_training_bridge_t* bridge,
                             omni_training_stats_t* stats);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge
 * @return NIMCP_SUCCESS on success
 */
int omni_training_reset_stats(omni_training_bridge_t* bridge);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * @param bridge Bridge
 * @return NIMCP_SUCCESS on success
 */
int omni_training_connect_bio_async(omni_training_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Bridge
 * @return NIMCP_SUCCESS on success
 */
int omni_training_disconnect_bio_async(omni_training_bridge_t* bridge);

/**
 * @brief Check if bio-async connected
 *
 * @param bridge Bridge
 * @return true if connected
 */
bool omni_training_is_bio_async_connected(const omni_training_bridge_t* bridge);

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

/**
 * @brief Convert training mode to string
 *
 * @param mode Training mode
 * @return String representation
 */
const char* omni_training_mode_to_string(omni_training_mode_t mode);

/**
 * @brief Convert difficulty to string
 *
 * @param difficulty Difficulty level
 * @return String representation
 */
const char* omni_training_difficulty_to_string(omni_difficulty_t difficulty);

/**
 * @brief Convert gradient source to string
 *
 * @param source Gradient source
 * @return String representation
 */
const char* omni_training_grad_source_to_string(omni_gradient_source_t source);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OMNI_TRAINING_BRIDGE_H */
