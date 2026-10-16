/**
 * @file nimcp_lnn_gradient_dao.h
 * @brief GPU LNN Gradient Data Access Object (DAO) API
 *
 * WHAT: Data Access Object pattern for GPU gradient accumulation
 * WHY:  Enables efficient gradient accumulation across mini-batches
 * HOW:  Maintains GPU buffers for accumulated gradients with host sync
 *
 * FEATURES:
 * - Gradient accumulation across multiple backward passes
 * - Gradient clipping (by value)
 * - Gradient normalization (by L2 norm)
 * - Host synchronization for logging/checkpointing
 *
 * USAGE:
 * @code
 * // Create DAO with gradient clipping at 1.0
 * nimcp_lnn_gradient_dao_t* dao = nimcp_lnn_gradient_dao_create(
 *     gpu_ctx, param_count, 1.0f, true);
 *
 * // Accumulate gradients from mini-batches
 * for (int batch = 0; batch < accumulation_steps; batch++) {
 *     compute_gradients(&grads);
 *     dao->accumulate(dao, grads);
 * }
 *
 * // Apply to weights (averages accumulated gradients)
 * dao->apply(dao, weights, learning_rate);
 *
 * // Reset for next accumulation cycle
 * dao->reset(dao);
 *
 * nimcp_lnn_gradient_dao_destroy(dao);
 * @endcode
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_LNN_GRADIENT_DAO_H
#define NIMCP_LNN_GRADIENT_DAO_H

#include "common/nimcp_export.h"
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Gradient DAO Structure
//=============================================================================

/**
 * @brief Gradient Data Access Object for GPU LNN training
 *
 * Provides an encapsulated interface for gradient accumulation, clipping,
 * normalization, and application. Uses the DAO pattern with function pointers
 * for operations.
 */
typedef struct nimcp_lnn_gradient_dao {
    // Data buffers
    float* d_accumulated_grads;     /**< GPU accumulated gradients */
    float* h_gradient_cache;        /**< Host cache for sync/logging */
    size_t grad_size;               /**< Number of gradient elements */

    // Configuration
    int accumulation_steps;         /**< Steps accumulated since last apply */
    float clip_value;               /**< Gradient clipping threshold (0 = disabled) */
    bool normalize;                 /**< Whether to normalize gradients */
    void* gpu_context;              /**< GPU context reference */

    // Internal state (do not access directly)
    void* _internal_tensor;         /**< Internal tensor reference */

    // DAO operations (function pointers)
    /**
     * @brief Accumulate new gradients
     * @param self DAO instance
     * @param new_grads New gradients (GPU pointer if CUDA enabled)
     * @return 0 on success, -1 on error
     */
    int (*accumulate)(struct nimcp_lnn_gradient_dao* self, float* new_grads);

    /**
     * @brief Apply accumulated gradients to weights
     *
     * Applies clipping/normalization if configured, then updates weights:
     * weights = weights - lr * (avg_grads)
     *
     * @param self DAO instance
     * @param weights Weights to update (GPU pointer if CUDA enabled)
     * @param lr Learning rate
     * @return 0 on success, -1 on error
     */
    int (*apply)(struct nimcp_lnn_gradient_dao* self, float* weights, float lr);

    /**
     * @brief Reset accumulated gradients to zero
     * @param self DAO instance
     * @return 0 on success, -1 on error
     */
    int (*reset)(struct nimcp_lnn_gradient_dao* self);

    /**
     * @brief Synchronize GPU gradients to host cache
     * @param self DAO instance
     * @return 0 on success, -1 on error
     */
    int (*sync_to_host)(struct nimcp_lnn_gradient_dao* self);
} nimcp_lnn_gradient_dao_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create a gradient DAO
 *
 * @param gpu_context GPU context (can be NULL for CPU-only)
 * @param grad_size Number of gradient elements
 * @param clip_value Gradient clipping threshold (0 or negative to disable)
 * @param normalize Whether to normalize gradients by L2 norm
 * @return New DAO instance, or NULL on failure
 */
NIMCP_EXPORT nimcp_lnn_gradient_dao_t* nimcp_lnn_gradient_dao_create(
    void* gpu_context,
    size_t grad_size,
    float clip_value,
    bool normalize
);

/**
 * @brief Destroy a gradient DAO
 *
 * Frees all GPU and host memory associated with the DAO.
 *
 * @param dao DAO to destroy
 */
NIMCP_EXPORT void nimcp_lnn_gradient_dao_destroy(nimcp_lnn_gradient_dao_t* dao);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get number of accumulation steps since last apply/reset
 */
NIMCP_EXPORT int nimcp_lnn_gradient_dao_get_accumulation_steps(const nimcp_lnn_gradient_dao_t* dao);

/**
 * @brief Get configured gradient clip value
 */
NIMCP_EXPORT float nimcp_lnn_gradient_dao_get_clip_value(const nimcp_lnn_gradient_dao_t* dao);

/**
 * @brief Check if gradient normalization is enabled
 */
NIMCP_EXPORT bool nimcp_lnn_gradient_dao_is_normalizing(const nimcp_lnn_gradient_dao_t* dao);

/**
 * @brief Get gradient buffer size
 */
NIMCP_EXPORT size_t nimcp_lnn_gradient_dao_get_size(const nimcp_lnn_gradient_dao_t* dao);

/**
 * @brief Get host gradient cache (call sync_to_host first for latest GPU values)
 */
NIMCP_EXPORT const float* nimcp_lnn_gradient_dao_get_host_cache(const nimcp_lnn_gradient_dao_t* dao);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_LNN_GRADIENT_DAO_H
