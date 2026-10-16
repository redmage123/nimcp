//=============================================================================
// nimcp_tensor_internal.h - Internal Tensor Structure Definition
//=============================================================================
/**
 * @file nimcp_tensor_internal.h
 * @brief Internal header exposing tensor struct for performance-critical code
 *
 * WHAT: Full definition of nimcp_tensor_t struct
 * WHY:  Some modules (VAE, SNN) need direct member access for performance
 * HOW:  Include this header instead of nimcp_tensor.h when direct access needed
 *
 * WARNING: This header breaks encapsulation. Only use when absolutely necessary.
 *          Prefer accessor functions from nimcp_tensor.h for normal usage.
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2026-01-30
 */

#ifndef NIMCP_TENSOR_INTERNAL_H
#define NIMCP_TENSOR_INTERNAL_H

#include "utils/tensor/nimcp_tensor.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Internal Tensor Structure
//=============================================================================

/**
 * @brief Full tensor structure (internal use only)
 *
 * This exposes the internal structure for modules requiring direct access.
 * Normal code should use accessor functions from nimcp_tensor.h instead.
 */
struct nimcp_tensor_s {
    uint32_t magic;                /**< Validation magic number */
    nimcp_tensor_shape_t shape;    /**< Shape information */
    nimcp_dtype_t dtype;           /**< Data type */
    void* data;                    /**< Pointer to data */
    bool owns_data;                /**< Whether tensor owns the data */
    bool requires_grad;            /**< Track gradients */
    nimcp_tensor_t* grad;          /**< Accumulated gradient */
    uint32_t refcount;             /**< Reference count */
    nimcp_mutex_t lock;          /**< Thread safety lock */
};

//=============================================================================
// Internal Helper Macros
//=============================================================================

/** Quick access to tensor dimensions */
#define TENSOR_DIM(t, i) ((t)->shape.dims[i])

/** Quick access to tensor strides */
#define TENSOR_STRIDE(t, i) ((t)->shape.strides[i])

/** Quick access to number of elements */
#define TENSOR_NUMEL(t) ((t)->shape.numel)

/** Quick access to rank */
#define TENSOR_RANK(t) ((t)->shape.rank)

/** Get data as float pointer */
#define TENSOR_DATA_F32(t) ((float*)((t)->data))

/** Get data as double pointer */
#define TENSOR_DATA_F64(t) ((double*)((t)->data))

/** Validate tensor magic */
#define TENSOR_VALID(t) ((t) && (t)->magic == NIMCP_TENSOR_MAGIC)

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TENSOR_INTERNAL_H */
