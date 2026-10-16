//=============================================================================
// nimcp_gt_hemispheric.h - Game-Theoretic Hemispheric Brain Integration
//=============================================================================
/**
 * @file nimcp_gt_hemispheric.h
 * @brief Nash bargaining and Shapley credit for hemispheric brain
 *
 * WHAT: Game-theoretic hemisphere coordination
 * WHY:  Fair resource allocation and credit assignment
 * HOW:  Nash bargaining for resources, Shapley for credit
 *
 * BIOLOGICAL INSPIRATION:
 * - Inter-hemispheric negotiation via corpus callosum
 * - Specialized processing as division of labor game
 * - Credit assignment for joint cognitive outcomes
 *
 * INTEGRATION: Adds HEMISPHERIC_MODE_BARGAINING
 *
 * @author NIMCP Development Team
 * @date 2024-12-27
 * @version 1.0.0
 */

#ifndef NIMCP_GT_HEMISPHERIC_H
#define NIMCP_GT_HEMISPHERIC_H

#include "cognitive/game_theory/nimcp_bargaining.h"
#include "cognitive/game_theory/nimcp_credit_assignment.h"
#include "core/brain/hemispheric/nimcp_hemispheric_brain.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Extended Hemispheric Modes
//=============================================================================

/**
 * @brief Game-theoretic hemispheric modes
 *
 * Extends hemispheric_mode_t with bargaining capabilities.
 */
typedef enum {
    GT_HEMI_MODE_BARGAINING = 100,    /**< Nash bargaining for resources */
    GT_HEMI_MODE_COALITION,           /**< Coalition formation */
    GT_HEMI_MODE_AUCTION              /**< Auction for task assignment */
} gt_hemispheric_mode_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Hemispheric bargaining configuration
 */
typedef struct {
    float left_bargaining_power;      /**< Left hemisphere power (0-1) */
    float right_bargaining_power;     /**< Right = 1 - left */
    float disagreement_left;          /**< Left disagreement payoff */
    float disagreement_right;         /**< Right disagreement payoff */
    float discount_factor;            /**< Patience in negotiation */
    uint32_t max_rounds;              /**< Max negotiation rounds */
    bool use_shapley_credit;          /**< Credit via Shapley value? */
    nimcp_bargaining_type_t bargain_type; /**< Bargaining solution type */
} gt_hemi_config_t;

/**
 * @brief Hemispheric credit assignment result
 */
typedef struct {
    float left_credit;                /**< Left hemisphere credit */
    float right_credit;               /**< Right hemisphere credit */
    float synergy_bonus;              /**< Cooperation value - sum of parts */
    float total_value;                /**< Combined output value */
    bool is_superadditive;            /**< Cooperation adds value? */
} gt_hemi_credit_t;

/**
 * @brief Hemispheric bargaining outcome
 */
typedef struct {
    float left_allocation;            /**< Resources to left */
    float right_allocation;           /**< Resources to right */
    float left_utility;               /**< Left achieved utility */
    float right_utility;              /**< Right achieved utility */
    uint32_t rounds_taken;            /**< Negotiation rounds */
    bool agreement_reached;           /**< Bargaining succeeded? */
    float nash_product;               /**< Nash objective value */
} gt_hemi_outcome_t;

/**
 * @brief Opaque handle to hemispheric bargaining context
 */
typedef struct gt_hemi_bargaining_ctx_struct* gt_hemi_bargaining_ctx_t;

//=============================================================================
// Lifecycle
//=============================================================================

/**
 * @brief Get default hemispheric bargaining configuration
 */
gt_hemi_config_t gt_hemi_default_config(void);

/**
 * @brief Create hemispheric bargaining context
 *
 * @param brain Hemispheric brain to augment
 * @param config Bargaining configuration
 * @return Context handle or NULL
 */
gt_hemi_bargaining_ctx_t gt_hemi_create(
    hemispheric_brain_t* brain,
    const gt_hemi_config_t* config
);

/**
 * @brief Destroy hemispheric bargaining context
 */
void gt_hemi_destroy(gt_hemi_bargaining_ctx_t ctx);

//=============================================================================
// Core Operations
//=============================================================================

/**
 * @brief Process input via hemispheric bargaining
 *
 * WHAT: Hemispheres negotiate resource allocation
 * WHY:  Fair, efficient resource sharing
 * HOW:  Nash bargaining solution for resource split
 *
 * @param ctx Bargaining context
 * @param input Input data
 * @param input_size Input size
 * @param output Output buffer
 * @param output_size Output size
 * @param outcome Output bargaining outcome
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t gt_hemi_process_bargaining(
    gt_hemi_bargaining_ctx_t ctx,
    const float* input,
    uint32_t input_size,
    float* output,
    uint32_t output_size,
    gt_hemi_outcome_t* outcome
);

/**
 * @brief Negotiate resource allocation between hemispheres
 *
 * @param ctx Bargaining context
 * @param total_resources Total resources to allocate
 * @param outcome Output allocation outcome
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t gt_hemi_negotiate_resources(
    gt_hemi_bargaining_ctx_t ctx,
    float total_resources,
    gt_hemi_outcome_t* outcome
);

/**
 * @brief Compute Shapley credit for hemispheric contribution
 *
 * WHAT: Fair credit for joint processing outcome
 * WHY:  Attribution based on marginal contributions
 * HOW:  Shapley value computation for 2-player game
 *
 * @param ctx Bargaining context
 * @param combined_output Combined processing result
 * @param output_size Output size
 * @param credit Output credit assignment
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t gt_hemi_compute_credit(
    gt_hemi_bargaining_ctx_t ctx,
    const float* combined_output,
    uint32_t output_size,
    gt_hemi_credit_t* credit
);

/**
 * @brief Set bargaining power (influences Nash solution)
 *
 * @param ctx Bargaining context
 * @param left_power Left hemisphere power (0-1, right = 1 - left)
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t gt_hemi_set_bargaining_power(
    gt_hemi_bargaining_ctx_t ctx,
    float left_power
);

/**
 * @brief Set disagreement point
 *
 * @param ctx Bargaining context
 * @param left_disagree Left disagreement payoff
 * @param right_disagree Right disagreement payoff
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t gt_hemi_set_disagreement(
    gt_hemi_bargaining_ctx_t ctx,
    float left_disagree,
    float right_disagree
);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get underlying hemispheric brain
 */
hemispheric_brain_t* gt_hemi_get_brain(const gt_hemi_bargaining_ctx_t ctx);

/**
 * @brief Get last bargaining outcome
 */
nimcp_error_t gt_hemi_get_last_outcome(
    const gt_hemi_bargaining_ctx_t ctx,
    gt_hemi_outcome_t* outcome
);

/**
 * @brief Get last credit assignment
 */
nimcp_error_t gt_hemi_get_last_credit(
    const gt_hemi_bargaining_ctx_t ctx,
    gt_hemi_credit_t* credit
);

/**
 * @brief Get cumulative statistics
 */
nimcp_error_t gt_hemi_get_stats(
    const gt_hemi_bargaining_ctx_t ctx,
    nimcp_game_stats_t* stats
);

/**
 * @brief Check if bargaining mode is active
 */
bool gt_hemi_is_active(const gt_hemi_bargaining_ctx_t ctx);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_GT_HEMISPHERIC_H
