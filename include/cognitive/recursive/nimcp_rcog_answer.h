/**
 * @file nimcp_rcog_answer.h
 * @brief Answer Refiner for Recursive Cognition - Diffusion-Style Refinement
 *
 * WHAT: Implements RLM's "answer generation via diffusion" pattern
 * WHY:  Answer iteratively refined across reasoning steps until ready
 * HOW:  Latent state updated with evidence, confidence tracks readiness
 *
 * RLM PATTERN:
 * answer = {"content": "", "ready": False}
 * # ... reasoning steps update answer["content"] ...
 * answer["ready"] = confidence > threshold
 *
 * BIOLOGICAL ANALOGY:
 * - Answer refinement is like hypothesis testing in prefrontal cortex
 * - Confidence accumulation mirrors evidence integration in decision-making
 * - Convergence detection is like reaching a decision threshold
 * - Latent state is like distributed representation across neural populations
 *
 * INTEGRATION POINTS:
 * - JEPA: Uses JEPA latent space for answer representation
 * - Executive: Threshold checking and early termination
 * - Bio-Async: Dopamine signals for refinement success
 * - Collective: CRDT-based distributed refinement
 *
 * @author NIMCP Development Team
 * @date 2026-01-03
 * @version 1.0.0
 */

#ifndef NIMCP_RCOG_ANSWER_H
#define NIMCP_RCOG_ANSWER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "cognitive/recursive/nimcp_rcog_types.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Answer refiner configuration
 */
typedef struct {
    size_t latent_dim;              /**< JEPA latent dimension (default: 256) */
    float learning_rate;            /**< Refinement step size (default: 0.1) */
    float momentum;                 /**< Momentum for smoother convergence (default: 0.9) */
    float ready_threshold;          /**< Confidence threshold for ready=true (default: 0.95) */
    uint32_t min_steps;             /**< Minimum refinement iterations (default: 1) */
    uint32_t max_steps;             /**< Maximum refinement iterations (default: 32) */
    bool enable_early_stopping;     /**< Stop when converged (default: true) */
    float convergence_epsilon;      /**< Convergence threshold (default: 0.001) */
    bool enable_history;            /**< Track refinement history (default: true) */
    size_t max_history_size;        /**< Maximum history entries (default: 64) */
} rcog_answer_config_t;

//=============================================================================
// History Tracking
//=============================================================================

/**
 * @brief Single history entry for refinement trajectory
 */
typedef struct {
    uint32_t step;                  /**< Refinement step number */
    float confidence;               /**< Confidence at this step */
    float delta;                    /**< Change from previous step */
    uint64_t timestamp_ms;          /**< Timestamp of this step */
    size_t evidence_count;          /**< Number of evidence items incorporated */
} rcog_answer_history_entry_t;

/**
 * @brief Answer refinement history
 */
typedef struct {
    rcog_answer_history_entry_t* entries;  /**< Array of history entries */
    size_t count;                   /**< Number of entries */
    size_t capacity;                /**< Capacity of entries array */
} rcog_answer_history_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Get default answer refiner configuration
 *
 * @return Configuration with sensible defaults
 */
rcog_answer_config_t rcog_answer_default_config(void);

/**
 * @brief Create answer refiner with configuration
 *
 * @param config Configuration (NULL for defaults)
 * @return Answer refiner handle or NULL on failure
 */
rcog_answer_refiner_t* rcog_answer_refiner_create(
    const rcog_answer_config_t* config
);

/**
 * @brief Create answer refiner with default configuration
 *
 * @return Answer refiner handle or NULL on failure
 */
rcog_answer_refiner_t* rcog_answer_refiner_create_default(void);

/**
 * @brief Destroy answer refiner and free resources
 *
 * @param refiner Answer refiner handle (NULL safe)
 */
void rcog_answer_refiner_destroy(rcog_answer_refiner_t* refiner);

//=============================================================================
// Answer State Management
//=============================================================================

/**
 * @brief Initialize answer state from goal
 *
 * WHAT: Creates initial answer state for a goal
 * WHY:  Starting point for diffusion-style refinement
 * HOW:  Initialize latent to zero/random, set confidence=0, ready=false
 *
 * @param refiner Answer refiner handle
 * @param goal Goal that generated this answer
 * @param state Output answer state (caller allocates)
 * @return RCOG_OK on success
 */
rcog_error_t rcog_answer_init(
    rcog_answer_refiner_t* refiner,
    const rcog_goal_t* goal,
    rcog_answer_state_t* state
);

/**
 * @brief Create new answer state (heap allocated)
 *
 * @param refiner Answer refiner handle
 * @param goal Goal that generated this answer
 * @return Answer state or NULL on failure (caller must free with rcog_answer_state_destroy)
 */
rcog_answer_state_t* rcog_answer_state_create(
    rcog_answer_refiner_t* refiner,
    const rcog_goal_t* goal
);

/**
 * @brief Destroy answer state and free resources
 *
 * @param state Answer state (NULL safe)
 */
void rcog_answer_state_destroy(rcog_answer_state_t* state);

/**
 * @brief Reset answer state for reuse
 *
 * @param refiner Answer refiner handle
 * @param state Answer state to reset
 * @return RCOG_OK on success
 */
rcog_error_t rcog_answer_reset(
    rcog_answer_refiner_t* refiner,
    rcog_answer_state_t* state
);

//=============================================================================
// Refinement API (Core Diffusion Loop)
//=============================================================================

/**
 * @brief Single refinement step (one diffusion iteration)
 *
 * WHAT: Update answer state with new evidence
 * WHY:  Iteratively improve answer representation
 * HOW:  Incorporate evidence into latent, update confidence
 *
 * The diffusion formula:
 *   latent_new = latent + lr * (evidence_direction) + momentum * velocity
 *   velocity = momentum * velocity + lr * evidence_direction
 *   confidence = f(latent_coherence, evidence_strength)
 *
 * @param refiner Answer refiner handle
 * @param state Answer state to refine
 * @param evidence Array of subtask results as evidence
 * @param num_evidence Number of evidence items
 * @return RCOG_OK on success
 */
rcog_error_t rcog_answer_step(
    rcog_answer_refiner_t* refiner,
    rcog_answer_state_t* state,
    const rcog_subtask_result_t* evidence,
    size_t num_evidence
);

/**
 * @brief Refine until ready or max steps reached
 *
 * WHAT: Run refinement loop until termination condition
 * WHY:  Convenience wrapper for diffusion loop
 * HOW:  Call rcog_answer_step repeatedly
 *
 * @param refiner Answer refiner handle
 * @param state Answer state to refine
 * @param evidence_fn Function to get next batch of evidence
 * @param evidence_ctx Context for evidence function
 * @return RCOG_OK on success
 */
rcog_error_t rcog_answer_refine_until_ready(
    rcog_answer_refiner_t* refiner,
    rcog_answer_state_t* state,
    rcog_error_t (*evidence_fn)(void* ctx, rcog_subtask_result_t** evidence, size_t* count),
    void* evidence_ctx
);

/**
 * @brief Update answer with single piece of evidence
 *
 * @param refiner Answer refiner handle
 * @param state Answer state to update
 * @param evidence Single subtask result
 * @return RCOG_OK on success
 */
rcog_error_t rcog_answer_update(
    rcog_answer_refiner_t* refiner,
    rcog_answer_state_t* state,
    const rcog_subtask_result_t* evidence
);

//=============================================================================
// State Inspection
//=============================================================================

/**
 * @brief Check if answer is ready
 *
 * @param refiner Answer refiner handle
 * @param state Answer state to check
 * @return true if answer is ready (confidence >= threshold)
 */
bool rcog_answer_is_ready(
    const rcog_answer_refiner_t* refiner,
    const rcog_answer_state_t* state
);

/**
 * @brief Check if answer has converged
 *
 * @param refiner Answer refiner handle
 * @param state Answer state to check
 * @return true if delta < convergence_epsilon
 */
bool rcog_answer_has_converged(
    const rcog_answer_refiner_t* refiner,
    const rcog_answer_state_t* state
);

/**
 * @brief Check if answer has stalled (not improving)
 *
 * @param refiner Answer refiner handle
 * @param state Answer state to check
 * @param window Number of recent steps to consider
 * @return true if no improvement in last N steps
 */
bool rcog_answer_is_stalled(
    const rcog_answer_refiner_t* refiner,
    const rcog_answer_state_t* state,
    uint32_t window
);

/**
 * @brief Get current confidence level
 *
 * @param state Answer state
 * @return Confidence value [0, 1]
 */
float rcog_answer_get_confidence(const rcog_answer_state_t* state);

/**
 * @brief Get current refinement step number
 *
 * @param state Answer state
 * @return Step number (0 = initial)
 */
uint32_t rcog_answer_get_step(const rcog_answer_state_t* state);

/**
 * @brief Get last delta (change from previous step)
 *
 * @param state Answer state
 * @return Delta value (smaller = more converged)
 */
float rcog_answer_get_delta(const rcog_answer_state_t* state);

//=============================================================================
// Answer Extraction
//=============================================================================

/**
 * @brief Extract final answer as tensor
 *
 * WHAT: Convert latent representation to output tensor
 * WHY:  Get usable answer from refined state
 * HOW:  Copy or decode latent representation
 *
 * @param refiner Answer refiner handle
 * @param state Answer state
 * @param output Output tensor (allocated by this function, caller must free)
 * @return RCOG_OK on success
 */
rcog_error_t rcog_answer_extract(
    rcog_answer_refiner_t* refiner,
    const rcog_answer_state_t* state,
    float** output,
    size_t* output_size
);

/**
 * @brief Extract answer content directly
 *
 * @param state Answer state
 * @param content Output pointer to content (do not free)
 * @param size Output size of content
 * @return RCOG_OK on success
 */
rcog_error_t rcog_answer_get_content(
    const rcog_answer_state_t* state,
    void** content,
    size_t* size
);

/**
 * @brief Set answer content directly
 *
 * @param state Answer state
 * @param content Content to set (copied)
 * @param size Size of content
 * @return RCOG_OK on success
 */
rcog_error_t rcog_answer_set_content(
    rcog_answer_state_t* state,
    const void* content,
    size_t size
);

//=============================================================================
// Latent Space Operations
//=============================================================================

/**
 * @brief Get latent representation
 *
 * @param state Answer state
 * @param latent Output pointer to latent array (do not free)
 * @param dim Output dimension of latent
 * @return RCOG_OK on success
 */
rcog_error_t rcog_answer_get_latent(
    const rcog_answer_state_t* state,
    float** latent,
    size_t* dim
);

/**
 * @brief Set latent representation directly
 *
 * @param state Answer state
 * @param latent Latent array to set (copied)
 * @param dim Dimension of latent
 * @return RCOG_OK on success
 */
rcog_error_t rcog_answer_set_latent(
    rcog_answer_state_t* state,
    const float* latent,
    size_t dim
);

/**
 * @brief Blend two answer states
 *
 * @param refiner Answer refiner handle
 * @param a First answer state
 * @param b Second answer state
 * @param alpha Blend factor (0 = all a, 1 = all b)
 * @param result Output blended state (caller allocates)
 * @return RCOG_OK on success
 */
rcog_error_t rcog_answer_blend(
    rcog_answer_refiner_t* refiner,
    const rcog_answer_state_t* a,
    const rcog_answer_state_t* b,
    float alpha,
    rcog_answer_state_t* result
);

//=============================================================================
// History and Statistics
//=============================================================================

/**
 * @brief Get refinement history
 *
 * @param refiner Answer refiner handle
 * @param state Answer state
 * @param history Output history structure
 * @return RCOG_OK on success
 */
rcog_error_t rcog_answer_get_history(
    const rcog_answer_refiner_t* refiner,
    const rcog_answer_state_t* state,
    rcog_answer_history_t** history
);

/**
 * @brief Free history structure
 *
 * @param history History to free (NULL safe)
 */
void rcog_answer_history_free(rcog_answer_history_t* history);

/**
 * @brief Answer refiner statistics
 */
typedef struct {
    uint64_t total_refinements;     /**< Total refinement steps across all answers */
    uint64_t total_answers;         /**< Total answers processed */
    uint64_t answers_converged;     /**< Answers that converged early */
    uint64_t answers_max_steps;     /**< Answers that hit max steps */
    uint64_t answers_stalled;       /**< Answers that stalled */
    float avg_steps_to_ready;       /**< Average steps to reach ready */
    float avg_final_confidence;     /**< Average final confidence */
} rcog_answer_stats_t;

/**
 * @brief Get refiner statistics
 *
 * @param refiner Answer refiner handle
 * @param stats Output statistics structure
 * @return RCOG_OK on success
 */
rcog_error_t rcog_answer_refiner_get_stats(
    const rcog_answer_refiner_t* refiner,
    rcog_answer_stats_t* stats
);

/**
 * @brief Reset refiner statistics
 *
 * @param refiner Answer refiner handle
 */
void rcog_answer_refiner_reset_stats(rcog_answer_refiner_t* refiner);

//=============================================================================
// Configuration Adjustment
//=============================================================================

/**
 * @brief Adjust ready threshold dynamically
 *
 * @param refiner Answer refiner handle
 * @param threshold New threshold [0, 1]
 * @return RCOG_OK on success
 */
rcog_error_t rcog_answer_set_threshold(
    rcog_answer_refiner_t* refiner,
    float threshold
);

/**
 * @brief Adjust learning rate dynamically
 *
 * @param refiner Answer refiner handle
 * @param learning_rate New learning rate
 * @return RCOG_OK on success
 */
rcog_error_t rcog_answer_set_learning_rate(
    rcog_answer_refiner_t* refiner,
    float learning_rate
);

/**
 * @brief Get current configuration
 *
 * @param refiner Answer refiner handle
 * @param config Output configuration structure
 * @return RCOG_OK on success
 */
rcog_error_t rcog_answer_refiner_get_config(
    const rcog_answer_refiner_t* refiner,
    rcog_answer_config_t* config
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RCOG_ANSWER_H */
