/**
 * @file nimcp_lnn_gradient_dao.c
 * @brief GPU LNN Gradient Data Access Object (DAO) Implementation
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
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#include "gpu/lnn/nimcp_lnn_gradient_dao.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define LOG_MODULE "LNN_GRAD_DAO"

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(lnn_gradient_dao)

//=============================================================================
// Internal DAO Operations - Forward Declarations
//=============================================================================

static int dao_accumulate(nimcp_lnn_gradient_dao_t* self, float* new_grads);
static int dao_apply(nimcp_lnn_gradient_dao_t* self, float* weights, float lr);
static int dao_reset(nimcp_lnn_gradient_dao_t* self);
static int dao_sync_to_host(nimcp_lnn_gradient_dao_t* self);

//=============================================================================
// GPU Context Access (for CPU fallback)
//=============================================================================

//=============================================================================
// CPU Fallback Implementations (always available)
//=============================================================================

static void cpu_accumulate(float* accumulated, const float* new_grad, size_t n, float scale) {
    for (size_t i = 0; i < n; i++) {
        accumulated[i] += scale * new_grad[i];
    }
}

static void cpu_clip(float* grads, size_t n, float clip_value) {
    for (size_t i = 0; i < n; i++) {
        if (grads[i] > clip_value) grads[i] = clip_value;
        else if (grads[i] < -clip_value) grads[i] = -clip_value;
    }
}

static float cpu_norm(const float* grads, size_t n) {
    float sum_sq = 0.0f;
    for (size_t i = 0; i < n; i++) {
        sum_sq += grads[i] * grads[i];
    }
    return sqrtf(sum_sq);
}

static void cpu_normalize(float* grads, size_t n, float norm) {
    if (norm > 1e-8f) {
        float inv_norm = 1.0f / norm;
        for (size_t i = 0; i < n; i++) {
            grads[i] *= inv_norm;
        }
    }
}

static void cpu_apply(float* weights, const float* grads, size_t n, float lr) {
    for (size_t i = 0; i < n; i++) {
        weights[i] -= lr * grads[i];
    }
}

static void cpu_reset(float* grads, size_t n) {
    memset(grads, 0, n * sizeof(float));
}

//=============================================================================
// GPU Function Declarations (when CUDA enabled)
//=============================================================================

#ifdef NIMCP_ENABLE_CUDA
extern void nimcp_lnn_gradient_accumulate_gpu(void* d_accumulated, const void* d_new_grad,
                                               size_t n, float scale);
extern void nimcp_lnn_gradient_clip_gpu(void* d_grads, size_t n, float clip_value);
extern float nimcp_lnn_gradient_norm_gpu(const void* d_grads, size_t n);
extern void nimcp_lnn_gradient_normalize_gpu(void* d_grads, size_t n, float norm);
extern void nimcp_lnn_gradient_apply_gpu(void* d_weights, const void* d_grads,
                                          size_t n, float lr);
extern void nimcp_lnn_gradient_reset_gpu(void* d_grads, size_t n);
extern void nimcp_lnn_memcpy_d2h(void* dst, const void* src, size_t size);
#endif

//=============================================================================
// DAO Creation and Destruction
//=============================================================================

nimcp_lnn_gradient_dao_t* nimcp_lnn_gradient_dao_create(
    void* gpu_context,
    size_t grad_size,
    float clip_value,
    bool normalize)
{
    if (grad_size == 0) {
        LOG_ERROR("Gradient size must be > 0");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_lnn_gradient_dao_create: grad_size is zero");
        return NULL;
    }

    nimcp_lnn_gradient_dao_t* dao = (nimcp_lnn_gradient_dao_t*)nimcp_calloc(
        1, sizeof(nimcp_lnn_gradient_dao_t));
    if (!dao) {
        LOG_ERROR("Failed to allocate gradient DAO");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dao is NULL");

        return NULL;
    }

    dao->grad_size = grad_size;
    dao->accumulation_steps = 0;
    dao->clip_value = clip_value > 0 ? clip_value : 0.0f;
    dao->normalize = normalize;
    dao->gpu_context = gpu_context;

    // Bind operations
    dao->accumulate = dao_accumulate;
    dao->apply = dao_apply;
    dao->reset = dao_reset;
    dao->sync_to_host = dao_sync_to_host;

#ifdef NIMCP_ENABLE_CUDA
    if (gpu_context) {
        nimcp_gpu_context_t* ctx = (nimcp_gpu_context_t*)gpu_context;

        // Allocate GPU buffer
        size_t dims[] = {grad_size};
        nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
        if (!tensor) {
            LOG_ERROR("Failed to allocate GPU gradient buffer");
            nimcp_free(dao);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tensor is NULL");

            return NULL;
        }
        nimcp_gpu_zeros(ctx, tensor);
        dao->d_accumulated_grads = tensor->data;

        // Store tensor reference for cleanup (using a reserved field)
        dao->_internal_tensor = tensor;
    }
#endif

    // Allocate host cache
    dao->h_gradient_cache = (float*)nimcp_calloc(grad_size, sizeof(float));
    if (!dao->h_gradient_cache) {
        LOG_ERROR("Failed to allocate host gradient cache");
#ifdef NIMCP_ENABLE_CUDA
        if (dao->_internal_tensor) {
            nimcp_gpu_tensor_destroy((nimcp_gpu_tensor_t*)dao->_internal_tensor);
        }
#endif
        nimcp_free(dao);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lnn_gradient_dao_create: validation failed");
        return NULL;
    }

    LOG_DEBUG("Created gradient DAO: size=%zu, clip=%.4f, normalize=%d",
              grad_size, clip_value, normalize);
    return dao;
}

void nimcp_lnn_gradient_dao_destroy(nimcp_lnn_gradient_dao_t* dao)
{
    if (!dao) return;

#ifdef NIMCP_ENABLE_CUDA
    if (dao->_internal_tensor) {
        nimcp_gpu_tensor_destroy((nimcp_gpu_tensor_t*)dao->_internal_tensor);
    }
#endif

    if (dao->h_gradient_cache) {
        nimcp_free(dao->h_gradient_cache);
    }

    nimcp_free(dao);
    LOG_DEBUG("Destroyed gradient DAO");
}

//=============================================================================
// DAO Operations Implementation
//=============================================================================

/**
 * @brief Accumulate new gradients into the buffer
 *
 * @param self DAO instance
 * @param new_grads New gradients to accumulate (GPU pointer if CUDA enabled)
 * @return 0 on success, -1 on error
 */
static int dao_accumulate(nimcp_lnn_gradient_dao_t* self, float* new_grads)
{
    if (!self || !new_grads) {
        LOG_ERROR("Invalid arguments to dao_accumulate");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dao_accumulate: required parameter is NULL (self, new_grads)");
        return -1;
    }

    float scale = 1.0f;  // Could be parameterized for gradient scaling

#ifdef NIMCP_ENABLE_CUDA
    if (self->gpu_context && self->d_accumulated_grads) {
        nimcp_lnn_gradient_accumulate_gpu(self->d_accumulated_grads, new_grads,
                                           self->grad_size, scale);
    } else {
        cpu_accumulate(self->h_gradient_cache, new_grads, self->grad_size, scale);
    }
#else
    cpu_accumulate(self->h_gradient_cache, new_grads, self->grad_size, scale);
#endif

    self->accumulation_steps++;
    return 0;
}

/**
 * @brief Apply accumulated gradients to weights
 *
 * Applies gradient clipping and normalization if configured, then
 * subtracts lr * gradients from weights.
 *
 * @param self DAO instance
 * @param weights Weight buffer to update (GPU pointer if CUDA enabled)
 * @param lr Learning rate
 * @return 0 on success, -1 on error
 */
static int dao_apply(nimcp_lnn_gradient_dao_t* self, float* weights, float lr)
{
    if (!self || !weights) {
        LOG_ERROR("Invalid arguments to dao_apply");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dao_apply: required parameter is NULL (self, weights)");
        return -1;
    }

    if (self->accumulation_steps == 0) {
        LOG_WARN("No gradients accumulated, skipping apply");
        return 0;
    }

    // Scale by number of accumulation steps (average)
    float avg_scale = 1.0f / (float)self->accumulation_steps;

#ifdef NIMCP_ENABLE_CUDA
    if (self->gpu_context && self->d_accumulated_grads) {
        float* d_grads = self->d_accumulated_grads;

        // Apply gradient clipping
        if (self->clip_value > 0.0f) {
            nimcp_lnn_gradient_clip_gpu(d_grads, self->grad_size, self->clip_value);
        }

        // Apply gradient normalization
        if (self->normalize) {
            float norm = nimcp_lnn_gradient_norm_gpu(d_grads, self->grad_size);
            if (norm > 1.0f) {
                nimcp_lnn_gradient_normalize_gpu(d_grads, self->grad_size, norm);
            }
        }

        // Apply gradients with averaged learning rate
        nimcp_lnn_gradient_apply_gpu(weights, d_grads, self->grad_size, lr * avg_scale);
    } else {
        // CPU path
        float* grads = self->h_gradient_cache;

        if (self->clip_value > 0.0f) {
            cpu_clip(grads, self->grad_size, self->clip_value);
        }

        if (self->normalize) {
            float norm = cpu_norm(grads, self->grad_size);
            if (norm > 1.0f) {
                cpu_normalize(grads, self->grad_size, norm);
            }
        }

        cpu_apply(weights, grads, self->grad_size, lr * avg_scale);
    }
#else
    float* grads = self->h_gradient_cache;

    if (self->clip_value > 0.0f) {
        cpu_clip(grads, self->grad_size, self->clip_value);
    }

    if (self->normalize) {
        float norm = cpu_norm(grads, self->grad_size);
        if (norm > 1.0f) {
            cpu_normalize(grads, self->grad_size, norm);
        }
    }

    cpu_apply(weights, grads, self->grad_size, lr * avg_scale);
#endif

    LOG_DEBUG("Applied gradients: steps=%d, lr=%.6f, avg_scale=%.6f",
              self->accumulation_steps, lr, avg_scale);
    return 0;
}

/**
 * @brief Reset accumulated gradients to zero
 *
 * @param self DAO instance
 * @return 0 on success, -1 on error
 */
static int dao_reset(nimcp_lnn_gradient_dao_t* self)
{
    if (!self) {
        LOG_ERROR("Invalid argument to dao_reset");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dao_reset: self is NULL");
        return -1;
    }

#ifdef NIMCP_ENABLE_CUDA
    if (self->gpu_context && self->d_accumulated_grads) {
        nimcp_lnn_gradient_reset_gpu(self->d_accumulated_grads, self->grad_size);
    }
#endif

    // Always reset host cache
    cpu_reset(self->h_gradient_cache, self->grad_size);
    self->accumulation_steps = 0;

    return 0;
}

/**
 * @brief Synchronize GPU gradients to host cache
 *
 * @param self DAO instance
 * @return 0 on success, -1 on error
 */
static int dao_sync_to_host(nimcp_lnn_gradient_dao_t* self)
{
    if (!self) {
        LOG_ERROR("Invalid argument to dao_sync_to_host");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dao_sync_to_host: self is NULL");
        return -1;
    }

#ifdef NIMCP_ENABLE_CUDA
    if (self->gpu_context && self->d_accumulated_grads) {
        nimcp_lnn_memcpy_d2h(self->h_gradient_cache, self->d_accumulated_grads,
                             self->grad_size * sizeof(float));
    }
#endif

    return 0;
}

//=============================================================================
// Utility Functions
//=============================================================================

int nimcp_lnn_gradient_dao_get_accumulation_steps(const nimcp_lnn_gradient_dao_t* dao)
{
    return dao ? dao->accumulation_steps : 0;
}

float nimcp_lnn_gradient_dao_get_clip_value(const nimcp_lnn_gradient_dao_t* dao)
{
    return dao ? dao->clip_value : 0.0f;
}

bool nimcp_lnn_gradient_dao_is_normalizing(const nimcp_lnn_gradient_dao_t* dao)
{
    return dao ? dao->normalize : false;
}

size_t nimcp_lnn_gradient_dao_get_size(const nimcp_lnn_gradient_dao_t* dao)
{
    return dao ? dao->grad_size : 0;
}

const float* nimcp_lnn_gradient_dao_get_host_cache(const nimcp_lnn_gradient_dao_t* dao)
{
    return dao ? dao->h_gradient_cache : NULL;
}
