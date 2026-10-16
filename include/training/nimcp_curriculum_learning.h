/**
 * @file nimcp_curriculum_learning.h
 * @brief Curriculum Learning for NIMCP Training
 *
 * WHAT: Progressive difficulty ordering for training samples
 * WHY:  Improve training efficiency and final performance by starting with
 *       easy examples and gradually introducing harder ones
 * HOW:  Multiple curriculum strategies (self-paced, teacher-guided, etc.)
 *       with automatic difficulty estimation and scheduling
 *
 * BIOLOGICAL GROUNDING:
 * - Models developmental learning (crawl before walk, walk before run)
 * - Inspired by zone of proximal development (Vygotsky)
 * - Scaffolding: appropriate support at each learning stage
 * - Error-driven attention: focus on learnable examples
 *
 * CURRICULUM STRATEGIES:
 * 1. Self-Paced: Model selects samples it can learn from
 * 2. Teacher-Guided: Fixed difficulty schedule
 * 3. Transfer: Use teacher model confidence as difficulty
 * 4. Uncertainty: Focus on samples with high uncertainty
 * 5. Anti-Curriculum: Occasionally introduce hard examples for robustness
 *
 * @author NIMCP Development Team
 * @date 2025-12-30
 * @version 1.0.0
 */

#ifndef NIMCP_CURRICULUM_LEARNING_H
#define NIMCP_CURRICULUM_LEARNING_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "utils/tensor/nimcp_tensor.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define CURRICULUM_MAX_DIFFICULTY_LEVELS   100  /**< Maximum difficulty bins */
#define CURRICULUM_DEFAULT_PACING_C        0.3f /**< Default self-paced C parameter */
#define CURRICULUM_DEFAULT_GROWTH_RATE     0.1f /**< Default difficulty growth per epoch */

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Curriculum learning strategies
 *
 * WHAT: Methods for determining sample ordering
 * WHY:  Different strategies suit different tasks
 * HOW:  Each implements different difficulty scoring/scheduling
 */
typedef enum {
    CURRICULUM_STRATEGY_NONE = 0,       /**< No curriculum (random sampling) */
    CURRICULUM_STRATEGY_SELF_PACED,     /**< Self-paced learning (SPL) */
    CURRICULUM_STRATEGY_TEACHER_GUIDED, /**< Fixed difficulty schedule */
    CURRICULUM_STRATEGY_TRANSFER,       /**< Use teacher model for difficulty */
    CURRICULUM_STRATEGY_UNCERTAINTY,    /**< Uncertainty-based curriculum */
    CURRICULUM_STRATEGY_LOSS_BASED,     /**< Use training loss as difficulty */
    CURRICULUM_STRATEGY_GRADIENT_NORM,  /**< Use gradient norm as difficulty */
    CURRICULUM_STRATEGY_CONFIDENCE,     /**< Use prediction confidence */
    CURRICULUM_STRATEGY_ANTI_CURRICULUM,/**< Reverse curriculum (hard first) */
    CURRICULUM_STRATEGY_HYBRID,         /**< Combine multiple strategies */
    CURRICULUM_STRATEGY_COUNT
} curriculum_strategy_t;

/**
 * @brief Difficulty metrics for automatic scoring
 */
typedef enum {
    DIFFICULTY_METRIC_LOSS = 0,         /**< Training loss */
    DIFFICULTY_METRIC_CONFIDENCE,       /**< Model confidence */
    DIFFICULTY_METRIC_GRADIENT_NORM,    /**< Gradient magnitude */
    DIFFICULTY_METRIC_ENTROPY,          /**< Prediction entropy */
    DIFFICULTY_METRIC_MARGIN,           /**< Classification margin */
    DIFFICULTY_METRIC_VARIANCE,         /**< Output variance */
    DIFFICULTY_METRIC_TEACHER,          /**< Teacher model score */
    DIFFICULTY_METRIC_CUSTOM,           /**< Custom metric function */
    DIFFICULTY_METRIC_COUNT
} difficulty_metric_t;

/**
 * @brief Pacing function types for self-paced learning
 */
typedef enum {
    PACING_LINEAR = 0,                  /**< Linear weighting */
    PACING_LOGARITHMIC,                 /**< Logarithmic weighting */
    PACING_MIXTURE,                     /**< Gaussian mixture */
    PACING_BINARY,                      /**< Hard threshold */
    PACING_SOFT_THRESHOLD,              /**< Soft threshold (sigmoid) */
    PACING_TYPE_COUNT
} pacing_function_t;

/**
 * @brief Curriculum scheduler state
 */
typedef enum {
    CURRICULUM_STATE_WARMUP = 0,        /**< Initial warmup phase */
    CURRICULUM_STATE_EASY,              /**< Easy examples only */
    CURRICULUM_STATE_MEDIUM,            /**< Easy + medium examples */
    CURRICULUM_STATE_FULL,              /**< All examples available */
    CURRICULUM_STATE_COUNT
} curriculum_state_t;

//=============================================================================
// Configuration Structures
//=============================================================================

/**
 * @brief Self-paced learning configuration
 *
 * BIOLOGICAL BASIS:
 * Models intrinsic motivation and learning from zone of proximal development
 */
typedef struct {
    pacing_function_t pacing_type;      /**< Pacing function type */
    float initial_pace;                 /**< Starting pace parameter (lambda_0) */
    float pace_increment;               /**< Pace increase per epoch (delta) */
    float pace_max;                     /**< Maximum pace parameter */
    float threshold_c;                  /**< Soft threshold parameter (C) */
    bool use_hard_samples;              /**< Include hard samples with low weight */
} spl_config_t;

/**
 * @brief Teacher-guided curriculum configuration
 */
typedef struct {
    float* difficulty_schedule;         /**< Difficulty thresholds per epoch */
    uint32_t schedule_length;           /**< Length of schedule */
    float initial_fraction;             /**< Fraction of easiest samples initially */
    float growth_rate;                  /**< Fraction growth per epoch */
    bool smooth_transition;             /**< Smooth probability vs. hard cutoff */
} teacher_config_t;

/**
 * @brief Transfer curriculum configuration (using teacher model)
 */
typedef struct {
    void* teacher_model;                /**< Teacher model for scoring */
    float temperature;                  /**< Temperature for confidence */
    bool use_soft_labels;               /**< Use teacher soft labels */
} transfer_config_t;

/**
 * @brief Uncertainty-based curriculum configuration
 *
 * BIOLOGICAL BASIS:
 * Models attention allocation - focus on uncertain but learnable examples
 */
typedef struct {
    float uncertainty_threshold;        /**< Maximum uncertainty to include */
    float threshold_decay;              /**< Decay rate for threshold */
    uint32_t mc_samples;                /**< MC dropout samples for uncertainty */
    bool exclude_outliers;              /**< Exclude extremely uncertain samples */
} uncertainty_config_t;

/**
 * @brief Custom difficulty function signature
 *
 * @param sample_idx Sample index
 * @param prediction Model prediction tensor
 * @param target Target tensor
 * @param loss Sample loss value
 * @param user_data User-provided context
 * @return Difficulty score (higher = harder)
 */
typedef float (*custom_difficulty_fn)(
    uint32_t sample_idx,
    const nimcp_tensor_t* prediction,
    const nimcp_tensor_t* target,
    float loss,
    void* user_data
);

/**
 * @brief Main curriculum learning configuration
 */
typedef struct {
    /* Strategy selection */
    curriculum_strategy_t strategy;      /**< Curriculum strategy */
    difficulty_metric_t metric;          /**< Difficulty metric */

    /* Strategy-specific configs */
    spl_config_t spl;                    /**< Self-paced config */
    teacher_config_t teacher;            /**< Teacher-guided config */
    transfer_config_t transfer;          /**< Transfer curriculum config */
    uncertainty_config_t uncertainty;    /**< Uncertainty-based config */

    /* General parameters */
    uint32_t warmup_epochs;              /**< Epochs before curriculum starts */
    uint32_t num_difficulty_bins;        /**< Number of difficulty quantiles */
    bool cache_difficulties;             /**< Cache difficulty scores */
    uint32_t update_frequency;           /**< Re-score every N epochs */

    /* Anti-curriculum injection */
    bool inject_hard_samples;            /**< Randomly inject hard samples */
    float hard_sample_ratio;             /**< Fraction of hard samples to inject */

    /* Custom difficulty */
    custom_difficulty_fn custom_fn;      /**< Custom difficulty function */
    void* custom_data;                   /**< User data for custom function */

    /* Logging */
    bool verbose;                        /**< Print curriculum progress */
} curriculum_config_t;

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Curriculum learning statistics
 */
typedef struct {
    uint64_t total_samples_scored;       /**< Total samples scored */
    uint64_t samples_selected;           /**< Samples selected for training */
    uint64_t samples_excluded;           /**< Samples excluded by curriculum */

    /* Difficulty distribution */
    float avg_difficulty;                /**< Average difficulty of selected */
    float min_difficulty;                /**< Minimum difficulty */
    float max_difficulty;                /**< Maximum difficulty */
    float difficulty_std;                /**< Difficulty standard deviation */

    /* Progress tracking */
    float current_difficulty_threshold;  /**< Current difficulty cutoff */
    curriculum_state_t current_state;    /**< Current curriculum state */
    uint32_t epochs_completed;           /**< Epochs completed */

    /* Selection statistics */
    float* bin_counts;                   /**< Samples per difficulty bin */
    uint32_t num_bins;                   /**< Number of bins */

    /* Performance correlation */
    float difficulty_loss_correlation;   /**< Correlation: difficulty vs loss */
} curriculum_stats_t;

//=============================================================================
// Opaque Types
//=============================================================================

/**
 * @brief Curriculum learning context (opaque)
 */
typedef struct curriculum_ctx_s curriculum_ctx_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default curriculum configuration
 *
 * WHAT: Initialize config with sensible defaults
 * WHY:  Simplify setup
 * HOW:  Self-paced with linear pacing
 *
 * @param config Configuration to initialize
 * @return 0 on success, negative on error
 */
int curriculum_default_config(curriculum_config_t* config);

/**
 * @brief Create curriculum learning context
 *
 * WHAT: Allocate and initialize curriculum tracker
 * WHY:  Set up difficulty scoring and sample selection
 * HOW:  Create buffers for scores, initialize strategy
 *
 * @param num_samples Total number of training samples
 * @param config Curriculum configuration
 * @return Curriculum context or NULL on failure
 */
curriculum_ctx_t* curriculum_create(
    uint32_t num_samples,
    const curriculum_config_t* config
);

/**
 * @brief Destroy curriculum context
 *
 * @param ctx Context to destroy (NULL-safe)
 */
void curriculum_destroy(curriculum_ctx_t* ctx);

//=============================================================================
// Difficulty Scoring API
//=============================================================================

/**
 * @brief Set pre-computed difficulty scores
 *
 * WHAT: Provide difficulty scores for all samples
 * WHY:  Use external difficulty metric (e.g., human annotation)
 * HOW:  Copies scores array
 *
 * @param ctx Curriculum context
 * @param scores Difficulty scores [num_samples]
 * @return 0 on success, negative on error
 */
int curriculum_set_difficulties(
    curriculum_ctx_t* ctx,
    const float* scores
);

/**
 * @brief Update difficulty score for single sample
 *
 * WHAT: Update difficulty based on training feedback
 * WHY:  Online difficulty estimation
 * HOW:  Uses exponential moving average
 *
 * @param ctx Curriculum context
 * @param sample_idx Sample index
 * @param loss Sample training loss
 * @param prediction Model prediction (optional, for confidence)
 * @param target Target value (optional)
 * @return 0 on success, negative on error
 */
int curriculum_update_difficulty(
    curriculum_ctx_t* ctx,
    uint32_t sample_idx,
    float loss,
    const nimcp_tensor_t* prediction,
    const nimcp_tensor_t* target
);

/**
 * @brief Batch update difficulties
 *
 * WHAT: Update difficulties for batch of samples
 * WHY:  Efficient batch processing
 * HOW:  Vectorized difficulty computation
 *
 * @param ctx Curriculum context
 * @param sample_indices Sample indices in batch
 * @param losses Per-sample losses
 * @param batch_size Batch size
 * @return 0 on success, negative on error
 */
int curriculum_update_difficulties_batch(
    curriculum_ctx_t* ctx,
    const uint32_t* sample_indices,
    const float* losses,
    uint32_t batch_size
);

/**
 * @brief Get difficulty score for sample
 *
 * @param ctx Curriculum context
 * @param sample_idx Sample index
 * @return Difficulty score (or -1 if not available)
 */
float curriculum_get_difficulty(
    const curriculum_ctx_t* ctx,
    uint32_t sample_idx
);

/**
 * @brief Compute difficulty percentile for sample
 *
 * @param ctx Curriculum context
 * @param sample_idx Sample index
 * @return Percentile [0, 100] or -1 if not available
 */
float curriculum_get_percentile(
    const curriculum_ctx_t* ctx,
    uint32_t sample_idx
);

//=============================================================================
// Sample Selection API
//=============================================================================

/**
 * @brief Get sampling weights for current curriculum state
 *
 * WHAT: Compute per-sample weights for weighted sampling
 * WHY:  Main interface for curriculum-guided training
 * HOW:  Apply pacing function to difficulties
 *
 * @param ctx Curriculum context
 * @param weights Output weight array [num_samples]
 * @return 0 on success, negative on error
 */
int curriculum_get_sample_weights(
    curriculum_ctx_t* ctx,
    float* weights
);

/**
 * @brief Check if sample should be included in training
 *
 * WHAT: Binary inclusion decision for sample
 * WHY:  Simple curriculum without weighting
 * HOW:  Compare difficulty to current threshold
 *
 * @param ctx Curriculum context
 * @param sample_idx Sample index
 * @return true if sample should be included
 */
bool curriculum_should_include(
    const curriculum_ctx_t* ctx,
    uint32_t sample_idx
);

/**
 * @brief Get ordered sample indices by difficulty
 *
 * WHAT: Return indices sorted by difficulty (easy first)
 * WHY:  Teacher-guided curriculum needs ordered samples
 * HOW:  Sort by cached difficulty scores
 *
 * @param ctx Curriculum context
 * @param indices Output index array [num_samples]
 * @param ascending true for easy-first, false for hard-first
 * @return 0 on success, negative on error
 */
int curriculum_get_ordered_indices(
    const curriculum_ctx_t* ctx,
    uint32_t* indices,
    bool ascending
);

/**
 * @brief Select samples for current epoch
 *
 * WHAT: Get sample indices to use for current epoch
 * WHY:  Dynamic subset selection based on curriculum state
 * HOW:  Apply difficulty threshold and sampling strategy
 *
 * @param ctx Curriculum context
 * @param indices Output selected indices
 * @param max_samples Maximum samples to select
 * @param num_selected Output: actual number selected
 * @return 0 on success, negative on error
 */
int curriculum_select_samples(
    curriculum_ctx_t* ctx,
    uint32_t* indices,
    uint32_t max_samples,
    uint32_t* num_selected
);

//=============================================================================
// Epoch Management API
//=============================================================================

/**
 * @brief Signal start of new epoch
 *
 * WHAT: Update curriculum state for new epoch
 * WHY:  Advance difficulty threshold, update pacing
 * HOW:  Apply schedule/growth parameters
 *
 * @param ctx Curriculum context
 * @return 0 on success, negative on error
 */
int curriculum_start_epoch(curriculum_ctx_t* ctx);

/**
 * @brief Signal end of epoch with performance metrics
 *
 * WHAT: Update curriculum based on epoch performance
 * WHY:  Adaptive pacing based on learning progress
 * HOW:  Adjust threshold if learning stagnates
 *
 * @param ctx Curriculum context
 * @param epoch_loss Average epoch loss
 * @param epoch_accuracy Epoch accuracy (if applicable, -1 to ignore)
 * @return 0 on success, negative on error
 */
int curriculum_end_epoch(
    curriculum_ctx_t* ctx,
    float epoch_loss,
    float epoch_accuracy
);

/**
 * @brief Get current curriculum state
 *
 * @param ctx Curriculum context
 * @return Current state (WARMUP, EASY, MEDIUM, FULL)
 */
curriculum_state_t curriculum_get_state(const curriculum_ctx_t* ctx);

/**
 * @brief Get current difficulty threshold
 *
 * @param ctx Curriculum context
 * @return Current difficulty threshold
 */
float curriculum_get_threshold(const curriculum_ctx_t* ctx);

/**
 * @brief Force curriculum to specific state
 *
 * WHAT: Override automatic progression
 * WHY:  Manual control or experimentation
 * HOW:  Sets threshold based on state
 *
 * @param ctx Curriculum context
 * @param state Target state
 * @return 0 on success, negative on error
 */
int curriculum_set_state(curriculum_ctx_t* ctx, curriculum_state_t state);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get curriculum statistics
 *
 * @param ctx Curriculum context
 * @param stats Output statistics
 * @return 0 on success, negative on error
 */
int curriculum_get_stats(
    const curriculum_ctx_t* ctx,
    curriculum_stats_t* stats
);

/**
 * @brief Reset curriculum statistics
 *
 * @param ctx Curriculum context
 */
void curriculum_reset_stats(curriculum_ctx_t* ctx);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get strategy name
 *
 * @param strategy Strategy enum
 * @return String name
 */
const char* curriculum_strategy_name(curriculum_strategy_t strategy);

/**
 * @brief Get metric name
 *
 * @param metric Metric enum
 * @return String name
 */
const char* curriculum_metric_name(difficulty_metric_t metric);

/**
 * @brief Get pacing function name
 *
 * @param pacing Pacing type enum
 * @return String name
 */
const char* curriculum_pacing_name(pacing_function_t pacing);

/**
 * @brief Get state name
 *
 * @param state State enum
 * @return String name
 */
const char* curriculum_state_name(curriculum_state_t state);

/**
 * @brief Compute difficulty from loss values
 *
 * WHAT: Estimate difficulty scores from training losses
 * WHY:  Bootstrap curriculum from initial training run
 * HOW:  Normalize losses to [0, 1] difficulty range
 *
 * @param losses Per-sample losses [num_samples]
 * @param num_samples Number of samples
 * @param difficulties Output difficulty scores [num_samples]
 * @return 0 on success, negative on error
 */
int curriculum_compute_difficulty_from_loss(
    const float* losses,
    uint32_t num_samples,
    float* difficulties
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CURRICULUM_LEARNING_H */
