#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_tensor.c - Comprehensive Tensor Library Implementation
//=============================================================================
/**
 * @file nimcp_tensor.c
 * @brief N-dimensional Tensor Library with Tensor Calculus
 *
 * WHAT: Complete tensor operations implementation
 * WHY:  Foundation for neural networks, emotion tensors, swarm coordination
 * HOW:  Efficient memory layout, SIMD where possible, clean API
 *
 * @author NIMCP Development Team
 * @date 2025-12-10
 * @version 1.0.0
 */

#include "utils/tensor/nimcp_tensor.h"
#include "utils/tensor/nimcp_tensor_simd.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/platform/nimcp_platform_once.h"
#include "utils/thread/nimcp_atomic.h"
#include "utils/rng/nimcp_rand.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <float.h>

//=============================================================================
// Module Configuration
//=============================================================================

#define LOG_MODULE "Tensor"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_math_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(tensor)


//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Internal tensor structure
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

/**
 * @brief Autodiff operation node
 */
typedef struct nimcp_autodiff_node_s {
    nimcp_tensor_t* output;
    nimcp_tensor_t** inputs;
    uint32_t num_inputs;
    void (*backward_fn)(struct nimcp_autodiff_node_s*, nimcp_tensor_t*);
    void* ctx;
    struct nimcp_autodiff_node_s* next;
} nimcp_autodiff_node_t;

/**
 * @brief Autodiff context
 */
struct nimcp_autodiff_ctx_s {
    bool recording;
    nimcp_autodiff_node_t* tape_head;
    nimcp_autodiff_node_t* tape_tail;
    nimcp_mutex_t lock;
};

//=============================================================================
// Global State
//=============================================================================

static nimcp_tensor_stats_t g_stats = {0};
static nimcp_mutex_t g_stats_lock = NIMCP_MUTEX_INITIALIZER;
static nimcp_platform_once_t g_tensor_init_once = NIMCP_PLATFORM_ONCE_INIT;
static nimcp_atomic_bool_t g_initialized = {0};

//=============================================================================
// Helper Functions - Validation
//=============================================================================

/**
 * @brief Validate tensor pointer
 */
static inline bool tensor_is_valid(const nimcp_tensor_t* t)
{
    return t != NULL && t->magic == NIMCP_TENSOR_MAGIC;
}

/**
 * @brief Update global statistics using atomic operations
 *
 * WHY ATOMICS: Reduces lock contention for frequently-called statistics updates.
 * Lock-free atomic increments are much faster than mutex lock/unlock pairs.
 */
static void stats_update_op(uint64_t* counter)
{
    __atomic_fetch_add(counter, 1, __ATOMIC_RELAXED);
    __atomic_fetch_add(&g_stats.operations_total, 1, __ATOMIC_RELAXED);
}

//=============================================================================
// Helper Functions - Shape
//=============================================================================

/**
 * @brief Compute total number of elements with overflow check
 *
 * WHAT: Calculate total element count from dimensions
 * WHY:  Prevent integer overflow for very large tensors
 * HOW:  Check multiplication overflow at each step using SIZE_MAX check
 *
 * @param dims Array of dimension sizes
 * @param rank Number of dimensions
 * @return Total number of elements, or 0 if overflow would occur
 */
static size_t compute_numel(const uint32_t* dims, uint32_t rank)
{
    if (rank == 0) return 1;  /* Scalar */

    size_t numel = 1;
    for (uint32_t i = 0; i < rank; i++) {
        /* Check for multiplication overflow */
        if (dims[i] > 0 && numel > SIZE_MAX / dims[i]) {
            LOG_ERROR(LOG_MODULE, "Overflow computing numel: dims[%u]=%u would overflow", i, dims[i]);
            return 0;  /* Signal overflow */
        }
        numel *= dims[i];

        /* Check against maximum allowed elements to prevent memory exhaustion */
        if (numel > NIMCP_TENSOR_MAX_ELEMENTS) {
            LOG_ERROR(LOG_MODULE, "Tensor too large: %zu elements exceeds max %zu",
                     numel, (size_t)NIMCP_TENSOR_MAX_ELEMENTS);
            return 0;  /* Signal overflow */
        }
    }
    return numel;
}

/**
 * @brief Compute row-major strides
 */
static void compute_strides(
    const uint32_t* dims,
    uint32_t rank,
    size_t element_size,
    int64_t* strides
)
{
    if (rank == 0) return;

    strides[rank - 1] = element_size;
    for (int i = (int)rank - 2; i >= 0; i--) {
        strides[i] = strides[i + 1] * dims[i + 1];
    }
}

/**
 * @brief Check if two shapes are equal
 */
static bool shapes_equal(const nimcp_tensor_shape_t* a, const nimcp_tensor_shape_t* b)
{
    /* P1-44: shapes_equal is a query function - mismatches are normal, not errors */
    if (a->rank != b->rank) {
        return false;
    }
    for (uint32_t i = 0; i < a->rank; i++) {
        if (a->dims[i] != b->dims[i]) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Check if shapes can broadcast
 */
static bool can_broadcast(
    const nimcp_tensor_shape_t* a,
    const nimcp_tensor_shape_t* b,
    nimcp_tensor_shape_t* result
)
{
    uint32_t max_rank = (a->rank > b->rank) ? a->rank : b->rank;
    result->rank = max_rank;

    for (int i = 0; i < (int)max_rank; i++) {
        int ai = (int)a->rank - 1 - i;
        int bi = (int)b->rank - 1 - i;
        int ri = (int)max_rank - 1 - i;

        uint32_t da = (ai >= 0) ? a->dims[ai] : 1;
        uint32_t db = (bi >= 0) ? b->dims[bi] : 1;

        if (da == db) {
            result->dims[ri] = da;
        } else if (da == 1) {
            result->dims[ri] = db;
        } else if (db == 1) {
            result->dims[ri] = da;
        } else {
            /* P1-44: can_broadcast is a query function - incompatible shapes are normal */
            return false;  /* Cannot broadcast */
        }
    }

    result->numel = compute_numel(result->dims, result->rank);
    return true;
}

/**
 * @brief Convert multi-index to flat index
 */
static size_t indices_to_flat(
    const uint32_t* indices,
    const nimcp_tensor_shape_t* shape
)
{
    size_t flat = 0;
    for (uint32_t i = 0; i < shape->rank; i++) {
        flat += indices[i] * (shape->strides[i] / nimcp_dtype_size(NIMCP_DTYPE_F32));
    }
    return flat;
}

/**
 * @brief Convert flat index to multi-index
 */
static void flat_to_indices(
    size_t flat,
    const nimcp_tensor_shape_t* shape,
    uint32_t* indices
)
{
    for (uint32_t i = 0; i < shape->rank; i++) {
        size_t stride = shape->strides[i] / nimcp_dtype_size(NIMCP_DTYPE_F32);
        indices[i] = flat / stride;
        flat %= stride;
    }
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Internal initialization called via nimcp_platform_once
 */
static void tensor_init_internal(void)
{
    memset(&g_stats, 0, sizeof(g_stats));
    nimcp_atomic_store_bool(&g_initialized, true, NIMCP_MEMORY_ORDER_RELEASE);
    LOG_INFO(LOG_MODULE, "Tensor subsystem initialized");
}

int nimcp_tensor_init(void)
{
    nimcp_platform_once(&g_tensor_init_once, tensor_init_internal);
    return NIMCP_TENSOR_OK;
}

void nimcp_tensor_shutdown(void)
{
    if (!nimcp_atomic_load_bool(&g_initialized, NIMCP_MEMORY_ORDER_ACQUIRE)) return;

    LOG_INFO(LOG_MODULE, "Tensor subsystem shutdown - stats: created=%lu, destroyed=%lu, mem=%zu bytes",
             g_stats.tensors_created, g_stats.tensors_destroyed, g_stats.memory_current);

    nimcp_atomic_store_bool(&g_initialized, false, NIMCP_MEMORY_ORDER_RELEASE);

    /* P2: Reset platform_once so tensor can be re-initialized after shutdown */
    g_tensor_init_once = (nimcp_platform_once_t)NIMCP_PLATFORM_ONCE_INIT;
}

//=============================================================================
// Tensor Creation
//=============================================================================

nimcp_tensor_t* nimcp_tensor_create(
    const uint32_t* dims,
    uint32_t rank,
    nimcp_dtype_t dtype
)
{
    /* Validate inputs */
    if (rank > NIMCP_TENSOR_MAX_RANK) {
        LOG_ERROR(LOG_MODULE, "Rank %u exceeds max %d", rank, NIMCP_TENSOR_MAX_RANK);
        /* P2: Use NIMCP_ERROR_INVALID_PARAM (rank exceeds max), not NULL_POINTER */
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_tensor_create: rank exceeds max");
        return NULL;
    }

    /* Allocate tensor structure */
    nimcp_tensor_t* t = nimcp_calloc(1, sizeof(nimcp_tensor_t));
    if (!t) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate tensor structure");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_tensor_create: t is NULL");
        return NULL;
    }

    t->magic = NIMCP_TENSOR_MAGIC;
    t->dtype = dtype;
    t->shape.rank = rank;
    t->owns_data = true;
    t->requires_grad = false;
    t->grad = NULL;
    t->refcount = 1;

    /* Initialize mutex with error checking */
    if (nimcp_mutex_init(&t->lock, NULL) != 0) {
        NIMCP_LOGGING_ERROR("Failed to initialize tensor mutex");
        nimcp_free(t);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "nimcp_tensor_create: validation failed");
        return NULL;
    }

    /* Set dimensions */
    if (rank > 0 && dims) {
        memcpy(t->shape.dims, dims, rank * sizeof(uint32_t));
    }

    /* Compute shape info with overflow check */
    size_t elem_size = nimcp_dtype_size(dtype);
    t->shape.numel = compute_numel(t->shape.dims, rank);

    /* Check if compute_numel returned 0 due to overflow */
    if (t->shape.numel == 0 && rank > 0) {
        /* compute_numel already logged the error */
        nimcp_mutex_destroy(&t->lock);
        nimcp_free(t);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_tensor_create: t->shape.numel is zero");
        return NULL;
    }

    /* Check for nbytes overflow (numel * elem_size) */
    if (elem_size > 0 && t->shape.numel > SIZE_MAX / elem_size) {
        LOG_ERROR(LOG_MODULE, "Overflow computing nbytes: %zu * %zu would overflow",
                 t->shape.numel, elem_size);
        nimcp_mutex_destroy(&t->lock);
        nimcp_free(t);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_tensor_create: validation failed");
        return NULL;
    }

    t->shape.nbytes = t->shape.numel * elem_size;
    compute_strides(t->shape.dims, rank, elem_size, t->shape.strides);

    /* Allocate data */
    if (t->shape.numel > 0) {
        t->data = nimcp_aligned_alloc(NIMCP_TENSOR_ALIGN, t->shape.nbytes);
        if (!t->data) {
            LOG_ERROR(LOG_MODULE, "Failed to allocate %zu bytes for tensor data", t->shape.nbytes);
            /* Clean up: mutex was initialized earlier */
            nimcp_mutex_destroy(&t->lock);
            /* Clean up: struct was allocated at line 266 */
            nimcp_free(t);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_tensor_create: t->data is NULL");
            return NULL;
        }
        /* Note: Data is uninitialized. Use nimcp_tensor_zeros() if zero initialization is needed. */
    }

    /* Update stats using atomic operations to reduce lock contention */
    __atomic_fetch_add(&g_stats.tensors_created, 1, __ATOMIC_RELAXED);
    size_t new_current = __atomic_add_fetch(&g_stats.memory_current,
                                             t->shape.nbytes + sizeof(nimcp_tensor_t),
                                             __ATOMIC_RELAXED);
    /* Update peak if needed - use compare-exchange loop for correctness */
    size_t old_peak = __atomic_load_n(&g_stats.memory_peak, __ATOMIC_RELAXED);
    while (new_current > old_peak) {
        if (__atomic_compare_exchange_n(&g_stats.memory_peak, &old_peak, new_current,
                                        false, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {
            break;
        }
        /* old_peak was updated by compare_exchange, loop to retry */
    }

    return t;
}

nimcp_tensor_t* nimcp_tensor_zeros(
    const uint32_t* dims,
    uint32_t rank,
    nimcp_dtype_t dtype
)
{
    nimcp_tensor_t* t = nimcp_tensor_create(dims, rank, dtype);
    if (!t) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "t is NULL");

        return NULL;

    }

    if (t->data && t->shape.nbytes > 0) {
        memset(t->data, 0, t->shape.nbytes);
    }
    return t;
}

nimcp_tensor_t* nimcp_tensor_ones(
    const uint32_t* dims,
    uint32_t rank,
    nimcp_dtype_t dtype
)
{
    nimcp_tensor_t* t = nimcp_tensor_create(dims, rank, dtype);
    if (!t) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "t is NULL");

        return NULL;

    }

    /* Fill with ones based on dtype */
    if (dtype == NIMCP_DTYPE_F32) {
        float* data = (float*)t->data;
        for (size_t i = 0; i < t->shape.numel; i++) {
            data[i] = 1.0F;
        }
    } else if (dtype == NIMCP_DTYPE_F64) {
        double* data = (double*)t->data;
        for (size_t i = 0; i < t->shape.numel; i++) {
            data[i] = 1.0;
        }
    } else if (dtype == NIMCP_DTYPE_I32) {
        int32_t* data = (int32_t*)t->data;
        for (size_t i = 0; i < t->shape.numel; i++) {
            data[i] = 1;
        }
    }

    return t;
}

nimcp_tensor_t* nimcp_tensor_full(
    const uint32_t* dims,
    uint32_t rank,
    nimcp_dtype_t dtype,
    double fill_value
)
{
    nimcp_tensor_t* t = nimcp_tensor_create(dims, rank, dtype);
    if (!t) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "t is NULL");

        return NULL;

    }

    if (dtype == NIMCP_DTYPE_F32) {
        float* data = (float*)t->data;
        float val = (float)fill_value;
        for (size_t i = 0; i < t->shape.numel; i++) {
            data[i] = val;
        }
    } else if (dtype == NIMCP_DTYPE_F64) {
        double* data = (double*)t->data;
        for (size_t i = 0; i < t->shape.numel; i++) {
            data[i] = fill_value;
        }
    } else if (dtype == NIMCP_DTYPE_I32) {
        int32_t* data = (int32_t*)t->data;
        int32_t val = (int32_t)fill_value;
        for (size_t i = 0; i < t->shape.numel; i++) {
            data[i] = val;
        }
    }

    return t;
}

nimcp_tensor_t* nimcp_tensor_from_data(
    const void* data,
    const uint32_t* dims,
    uint32_t rank,
    nimcp_dtype_t dtype,
    bool copy
)
{
    if (!data) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "data is NULL");

        return NULL;

    }

    nimcp_tensor_t* t = nimcp_tensor_create(dims, rank, dtype);
    if (!t) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "t is NULL");

        return NULL;

    }

    if (copy) {
        memcpy(t->data, data, t->shape.nbytes);
    } else {
        /* Reference mode - free our allocation and use provided data */
        nimcp_free(t->data);
        t->data = (void*)data;
        t->owns_data = false;
    }

    return t;
}

nimcp_tensor_t* nimcp_tensor_eye(uint32_t n, nimcp_dtype_t dtype)
{
    uint32_t dims[2] = {n, n};
    nimcp_tensor_t* t = nimcp_tensor_zeros(dims, 2, dtype);
    if (!t) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "t is NULL");

        return NULL;

    }

    /* Set diagonal to 1 */
    if (dtype == NIMCP_DTYPE_F32) {
        float* data = (float*)t->data;
        for (uint32_t i = 0; i < n; i++) {
            data[i * n + i] = 1.0F;
        }
    } else if (dtype == NIMCP_DTYPE_F64) {
        double* data = (double*)t->data;
        for (uint32_t i = 0; i < n; i++) {
            data[i * n + i] = 1.0;
        }
    }

    return t;
}

nimcp_tensor_t* nimcp_tensor_randn(
    const uint32_t* dims,
    uint32_t rank,
    nimcp_dtype_t dtype,
    double mean,
    double std
)
{
    nimcp_tensor_t* t = nimcp_tensor_create(dims, rank, dtype);
    if (!t) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "t is NULL");

        return NULL;

    }

    /* P1-U2: Dispatch by dtype instead of always casting to float* */
    if (dtype == NIMCP_DTYPE_F64) {
        double* data = (double*)t->data;
        for (size_t i = 0; i < t->shape.numel; i += 2) {
            double u1 = (double)nimcp_rand_uniform() * 0.99999 + 0.00001;
            double u2 = (double)nimcp_rand_uniform();
            double z0 = sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
            double z1 = sqrt(-2.0 * log(u1)) * sin(2.0 * M_PI * u2);
            data[i] = z0 * std + mean;
            if (i + 1 < t->shape.numel) {
                data[i + 1] = z1 * std + mean;
            }
        }
    } else {
        /* Default: float32 (and int types get truncated cast) */
        float* data = (float*)t->data;
        for (size_t i = 0; i < t->shape.numel; i += 2) {
            double u1 = (double)nimcp_rand_uniform() * 0.99999 + 0.00001;
            double u2 = (double)nimcp_rand_uniform();
            double z0 = sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
            double z1 = sqrt(-2.0 * log(u1)) * sin(2.0 * M_PI * u2);
            data[i] = (float)(z0 * std + mean);
            if (i + 1 < t->shape.numel) {
                data[i + 1] = (float)(z1 * std + mean);
            }
        }
    }

    return t;
}

nimcp_tensor_t* nimcp_tensor_rand(
    const uint32_t* dims,
    uint32_t rank,
    nimcp_dtype_t dtype,
    double low,
    double high
)
{
    nimcp_tensor_t* t = nimcp_tensor_create(dims, rank, dtype);
    if (!t) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "t is NULL");

        return NULL;

    }

    /* P1-U2: Dispatch by dtype instead of always casting to float* */
    double range = high - low;
    if (dtype == NIMCP_DTYPE_F64) {
        double* data = (double*)t->data;
        for (size_t i = 0; i < t->shape.numel; i++) {
            double u = (double)nimcp_rand_uniform();
            data[i] = u * range + low;
        }
    } else {
        float* data = (float*)t->data;
        for (size_t i = 0; i < t->shape.numel; i++) {
            double u = (double)nimcp_rand_uniform();
            data[i] = (float)(u * range + low);
        }
    }

    return t;
}

nimcp_tensor_t* nimcp_tensor_arange(
    double start,
    double stop,
    double step,
    nimcp_dtype_t dtype
)
{
    if (step == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_tensor_arange: step is zero");
        return NULL;
    }

    uint32_t n = (uint32_t)ceil((stop - start) / step);
    if (n == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_tensor_arange: n is zero");
        return NULL;
    }

    nimcp_tensor_t* t = nimcp_tensor_create(&n, 1, dtype);
    if (!t) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "t is NULL");

        return NULL;

    }

    switch (dtype) {
        case NIMCP_DTYPE_F64: {
            double* d = (double*)t->data;
            for (uint32_t i = 0; i < n; i++) d[i] = start + i * step;
            break;
        }
        case NIMCP_DTYPE_I32: {
            int32_t* d = (int32_t*)t->data;
            for (uint32_t i = 0; i < n; i++) d[i] = (int32_t)(start + i * step);
            break;
        }
        default: { /* F32 and others */
            float* d = (float*)t->data;
            for (uint32_t i = 0; i < n; i++) d[i] = (float)(start + i * step);
            break;
        }
    }

    return t;
}

nimcp_tensor_t* nimcp_tensor_linspace(
    double start,
    double stop,
    uint32_t num,
    nimcp_dtype_t dtype
)
{
    if (num == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_tensor_linspace: num is zero");
        return NULL;
    }

    nimcp_tensor_t* t = nimcp_tensor_create(&num, 1, dtype);
    if (!t) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "t is NULL");

        return NULL;

    }

    double step = (num > 1) ? (stop - start) / (num - 1) : 0;
    switch (dtype) {
        case NIMCP_DTYPE_F64: {
            double* d = (double*)t->data;
            for (uint32_t i = 0; i < num; i++) d[i] = start + i * step;
            break;
        }
        case NIMCP_DTYPE_I32: {
            int32_t* d = (int32_t*)t->data;
            for (uint32_t i = 0; i < num; i++) d[i] = (int32_t)(start + i * step);
            break;
        }
        default: { /* F32 and others */
            float* d = (float*)t->data;
            for (uint32_t i = 0; i < num; i++) d[i] = (float)(start + i * step);
            break;
        }
    }

    return t;
}

nimcp_tensor_t* nimcp_tensor_clone(const nimcp_tensor_t* t)
{
    if (!tensor_is_valid(t)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_tensor_clone: tensor_is_valid is NULL");
        return NULL;
    }

    nimcp_tensor_t* clone = nimcp_tensor_create(t->shape.dims, t->shape.rank, t->dtype);
    if (!clone) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "clone is NULL");

        return NULL;

    }

    if (!t->data || !clone->data) {
        nimcp_tensor_destroy(clone);
        return NULL;
    }
    memcpy(clone->data, t->data, t->shape.nbytes);
    clone->requires_grad = t->requires_grad;

    return clone;
}

/**
 * @brief Destroy tensor and free resources
 *
 * WHAT: Free tensor memory and decrement reference count
 * WHY:  Clean resource management with refcounting
 * HOW:  Thread-safe cleanup, idempotent (safe to call on partially initialized tensors)
 *
 * MEMORY SAFETY:
 * - Idempotent: Safe to call multiple times (magic check prevents double-free)
 * - Partial cleanup: Safe even if tensor creation failed partway
 * - Refcounting: Only frees when refcount reaches 0
 * - Gradient cleanup: Recursive destroy of gradient tensor
 *
 * @param t Tensor to destroy (NULL is safe, no-op)
 */
void nimcp_tensor_destroy(nimcp_tensor_t* t)
{
    /* Guard: NULL is no-op */
    if (!t) return;

    /* Pre-check magic BEFORE locking to avoid locking a destroyed object.
     * This is a best-effort check - the definitive check is inside the lock.
     * If magic is invalid here, the tensor was already destroyed or corrupted,
     * and we should not attempt to lock its mutex (lock-after-free risk). */
    if (t->magic != NIMCP_TENSOR_MAGIC) {
        /* Already destroyed or corrupted - do not touch the mutex */
        return;
    }

    nimcp_mutex_lock(&t->lock);

    /* Guard: Invalid magic means already destroyed or corrupted */
    /* MUST be re-checked INSIDE lock to handle race between pre-check and lock */
    if (!tensor_is_valid(t)) {
        /* Already destroyed or never initialized properly */
        nimcp_mutex_unlock(&t->lock);
        return;
    }

    /* Refcount management */
    t->refcount--;
    if (t->refcount > 0) {
        nimcp_mutex_unlock(&t->lock);
        return;
    }

    /* Capture gradient pointer before releasing lock to avoid deadlock */
    nimcp_tensor_t* grad_to_destroy = t->grad;
    t->grad = NULL;

    /* Capture data pointer for cleanup after lock release */
    void* data_to_free = (t->owns_data && t->data) ? t->data : NULL;
    t->data = NULL;

    /* Invalidate magic BEFORE unlocking to prevent re-entry */
    t->magic = 0;

    /* Release lock before any recursive operations */
    nimcp_mutex_unlock(&t->lock);
    nimcp_mutex_destroy(&t->lock);

    /* Update stats using atomic operations (lock-free) */
    __atomic_fetch_add(&g_stats.tensors_destroyed, 1, __ATOMIC_RELAXED);
    __atomic_fetch_sub(&g_stats.memory_current, t->shape.nbytes + sizeof(nimcp_tensor_t), __ATOMIC_RELAXED);

    /* Free gradient if exists (recursive destroy - now safe, lock released) */
    if (grad_to_destroy) {
        nimcp_tensor_destroy(grad_to_destroy);
    }

    /* Free data if owned */
    if (data_to_free) {
        nimcp_free(data_to_free);
    }

    nimcp_free(t);
}

//=============================================================================
// Tensor Properties
//=============================================================================

const nimcp_tensor_shape_t* nimcp_tensor_shape(const nimcp_tensor_t* t)
{
    if (!tensor_is_valid(t)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_tensor_shape: tensor_is_valid is NULL");
        return NULL;
    }
    return &t->shape;
}

uint32_t nimcp_tensor_rank(const nimcp_tensor_t* t)
{
    if (!tensor_is_valid(t)) return 0;
    return t->shape.rank;
}

size_t nimcp_tensor_numel(const nimcp_tensor_t* t)
{
    if (!tensor_is_valid(t)) return 0;
    return t->shape.numel;
}

nimcp_dtype_t nimcp_tensor_dtype(const nimcp_tensor_t* t)
{
    if (!tensor_is_valid(t)) return NIMCP_DTYPE_F32;
    return t->dtype;
}

void* nimcp_tensor_data(nimcp_tensor_t* t)
{
    if (!tensor_is_valid(t)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_tensor_data: tensor_is_valid is NULL");
        return NULL;
    }
    return t->data;
}

const void* nimcp_tensor_data_const(const nimcp_tensor_t* t)
{
    if (!tensor_is_valid(t)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_tensor_data_const: tensor_is_valid is NULL");
        return NULL;
    }
    return t->data;
}

bool nimcp_tensor_is_contiguous(const nimcp_tensor_t* t)
{
    if (!tensor_is_valid(t)) {
        return false;
    }

    /* Check if strides match row-major layout */
    size_t elem_size = nimcp_dtype_size(t->dtype);
    int64_t expected_stride = elem_size;

    for (int i = (int)t->shape.rank - 1; i >= 0; i--) {
        if (t->shape.strides[i] != expected_stride) {
            /* P1-44: Non-contiguous is a normal query result, not an error */
            return false;
        }
        expected_stride *= t->shape.dims[i];
    }

    return true;
}

nimcp_tensor_t* nimcp_tensor_contiguous(const nimcp_tensor_t* t)
{
    if (!tensor_is_valid(t)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_tensor_contiguous: tensor_is_valid is NULL");
        return NULL;
    }

    if (nimcp_tensor_is_contiguous(t)) {
        /* Already contiguous, return clone */
        return nimcp_tensor_clone(t);
    }

    /* Need to copy with proper layout */
    nimcp_tensor_t* result = nimcp_tensor_create(t->shape.dims, t->shape.rank, t->dtype);
    if (!result) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "result is NULL");

        return NULL;

    }

    /* Copy element by element (handles non-contiguous case) */
    uint32_t indices[NIMCP_TENSOR_MAX_RANK] = {0};
    for (size_t i = 0; i < t->shape.numel; i++) {
        double val = nimcp_tensor_get(t, indices);
        nimcp_tensor_set(result, indices, val);

        /* Increment indices */
        for (int j = (int)t->shape.rank - 1; j >= 0; j--) {
            indices[j]++;
            if (indices[j] < t->shape.dims[j]) break;
            indices[j] = 0;
        }
    }

    return result;
}

bool nimcp_tensor_requires_grad(const nimcp_tensor_t* t)
{
    if (!tensor_is_valid(t)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_tensor_requires_grad: tensor_is_valid is NULL");
        return false;
    }
    return t->requires_grad;
}

void nimcp_tensor_set_requires_grad(nimcp_tensor_t* t, bool requires_grad)
{
    if (!tensor_is_valid(t)) return;
    t->requires_grad = requires_grad;
}

//=============================================================================
// Element Access
//=============================================================================

double nimcp_tensor_get(const nimcp_tensor_t* t, const uint32_t* indices)
{
    if (!tensor_is_valid(t) || !indices) return 0.0;

    /* Compute flat offset using strides */
    size_t offset = 0;
    size_t elem_size = nimcp_dtype_size(t->dtype);
    for (uint32_t i = 0; i < t->shape.rank; i++) {
        if (indices[i] >= t->shape.dims[i]) return 0.0;
        offset += indices[i] * (t->shape.strides[i] / elem_size);
    }

    if (t->dtype == NIMCP_DTYPE_F32) {
        return ((float*)t->data)[offset];
    } else if (t->dtype == NIMCP_DTYPE_F64) {
        return ((double*)t->data)[offset];
    } else if (t->dtype == NIMCP_DTYPE_I32) {
        return ((int32_t*)t->data)[offset];
    }

    return 0.0;
}

int nimcp_tensor_set(nimcp_tensor_t* t, const uint32_t* indices, double value)
{
    if (!tensor_is_valid(t) || !indices) return NIMCP_TENSOR_ERR_NULL;

    /* Compute flat offset using strides */
    size_t offset = 0;
    size_t elem_size = nimcp_dtype_size(t->dtype);
    for (uint32_t i = 0; i < t->shape.rank; i++) {
        if (indices[i] >= t->shape.dims[i]) return NIMCP_TENSOR_ERR_INDEX;
        offset += indices[i] * (t->shape.strides[i] / elem_size);
    }

    if (t->dtype == NIMCP_DTYPE_F32) {
        ((float*)t->data)[offset] = (float)value;
    } else if (t->dtype == NIMCP_DTYPE_F64) {
        ((double*)t->data)[offset] = value;
    } else if (t->dtype == NIMCP_DTYPE_I32) {
        ((int32_t*)t->data)[offset] = (int32_t)value;
    }

    return NIMCP_TENSOR_OK;
}

double nimcp_tensor_get_flat(const nimcp_tensor_t* t, size_t index)
{
    if (!tensor_is_valid(t) || index >= t->shape.numel) return 0.0;

    if (t->dtype == NIMCP_DTYPE_F32) {
        return ((float*)t->data)[index];
    } else if (t->dtype == NIMCP_DTYPE_F64) {
        return ((double*)t->data)[index];
    } else if (t->dtype == NIMCP_DTYPE_I32) {
        return ((int32_t*)t->data)[index];
    }

    return 0.0;
}

int nimcp_tensor_set_flat(nimcp_tensor_t* t, size_t index, double value)
{
    if (!tensor_is_valid(t)) return NIMCP_TENSOR_ERR_NULL;
    if (index >= t->shape.numel) return NIMCP_TENSOR_ERR_INDEX;

    if (t->dtype == NIMCP_DTYPE_F32) {
        ((float*)t->data)[index] = (float)value;
    } else if (t->dtype == NIMCP_DTYPE_F64) {
        ((double*)t->data)[index] = value;
    } else if (t->dtype == NIMCP_DTYPE_I32) {
        ((int32_t*)t->data)[index] = (int32_t)value;
    }

    return NIMCP_TENSOR_OK;
}

//=============================================================================
// Shape Operations
//=============================================================================

nimcp_tensor_t* nimcp_tensor_reshape(
    const nimcp_tensor_t* t,
    const uint32_t* new_dims,
    uint32_t new_rank
)
{
    if (!tensor_is_valid(t) || !new_dims) {
        /* P2: Use NIMCP_ERROR_INVALID_PARAM for NULL parameters, not NO_MEMORY */
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_tensor_reshape: required parameter is NULL (tensor_is_valid, new_dims)");
        return NULL;
    }

    /* Verify element count matches */
    size_t new_numel = compute_numel(new_dims, new_rank);
    if (new_numel != t->shape.numel) {
        LOG_ERROR(LOG_MODULE, "Reshape: numel mismatch %zu vs %zu", new_numel, t->shape.numel);
        /* P2: Use NIMCP_ERROR_INVALID_PARAM for shape mismatch, not NO_MEMORY */
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_tensor_reshape: numel mismatch");
        return NULL;
    }

    /* Create new tensor sharing data if contiguous */
    nimcp_tensor_t* result;
    if (nimcp_tensor_is_contiguous(t)) {
        result = nimcp_tensor_from_data(t->data, new_dims, new_rank, t->dtype, false);
    } else {
        result = nimcp_tensor_contiguous(t);
        if (!result) {

            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "result is NULL");

            return NULL;

        }
        /* Now reshape the contiguous copy */
        result->shape.rank = new_rank;
        memcpy(result->shape.dims, new_dims, new_rank * sizeof(uint32_t));
        compute_strides(result->shape.dims, new_rank, nimcp_dtype_size(t->dtype), result->shape.strides);
    }

    return result;
}

nimcp_tensor_t* nimcp_tensor_transpose(const nimcp_tensor_t* t)
{
    if (!tensor_is_valid(t)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_tensor_transpose: tensor_is_valid is NULL");
        return NULL;
    }
    if (t->shape.rank < 2) return nimcp_tensor_clone(t);

    /* Swap last two dimensions */
    uint32_t perm[NIMCP_TENSOR_MAX_RANK];
    for (uint32_t i = 0; i < t->shape.rank - 2; i++) {
        perm[i] = i;
    }
    perm[t->shape.rank - 2] = t->shape.rank - 1;
    perm[t->shape.rank - 1] = t->shape.rank - 2;

    return nimcp_tensor_permute(t, perm, t->shape.rank);
}

nimcp_tensor_t* nimcp_tensor_permute(
    const nimcp_tensor_t* t,
    const uint32_t* perm,
    uint32_t rank
)
{
    if (!tensor_is_valid(t) || !perm) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_tensor_permute: required parameter is NULL (tensor_is_valid, perm)");
        return NULL;
    }
    if (rank != t->shape.rank) {
        /* P2: Use NIMCP_ERROR_INVALID_PARAM for rank mismatch, not NULL_POINTER */
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_tensor_permute: rank mismatch");
        return NULL;
    }

    /* Create new shape with permuted dimensions */
    uint32_t new_dims[NIMCP_TENSOR_MAX_RANK];
    for (uint32_t i = 0; i < rank; i++) {
        if (perm[i] >= rank) {
            /* P2: Use NIMCP_ERROR_OUT_OF_RANGE for perm index OOB, not NO_MEMORY */
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_tensor_permute: perm index out of bounds");
            return NULL;
        }
        new_dims[i] = t->shape.dims[perm[i]];
    }

    nimcp_tensor_t* result = nimcp_tensor_create(new_dims, rank, t->dtype);
    if (!result) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "result is NULL");

        return NULL;

    }

    /* Copy with permuted indices */
    uint32_t src_indices[NIMCP_TENSOR_MAX_RANK] = {0};
    uint32_t dst_indices[NIMCP_TENSOR_MAX_RANK] = {0};

    for (size_t i = 0; i < t->shape.numel; i++) {
        /* Convert dst_indices to src_indices using inverse permutation */
        for (uint32_t j = 0; j < rank; j++) {
            src_indices[perm[j]] = dst_indices[j];
        }

        double val = nimcp_tensor_get(t, src_indices);
        nimcp_tensor_set(result, dst_indices, val);

        /* Increment dst_indices */
        for (int j = (int)rank - 1; j >= 0; j--) {
            dst_indices[j]++;
            if (dst_indices[j] < new_dims[j]) break;
            dst_indices[j] = 0;
        }
    }

    return result;
}

nimcp_tensor_t* nimcp_tensor_squeeze(const nimcp_tensor_t* t)
{
    if (!tensor_is_valid(t)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_tensor_squeeze: tensor_is_valid is NULL");
        return NULL;
    }

    /* Count non-singleton dimensions */
    uint32_t new_dims[NIMCP_TENSOR_MAX_RANK];
    uint32_t new_rank = 0;

    for (uint32_t i = 0; i < t->shape.rank; i++) {
        if (t->shape.dims[i] != 1) {
            new_dims[new_rank++] = t->shape.dims[i];
        }
    }

    if (new_rank == 0) {
        /* All dimensions were 1, return scalar */
        return nimcp_tensor_clone(t);
    }

    return nimcp_tensor_reshape(t, new_dims, new_rank);
}

nimcp_tensor_t* nimcp_tensor_unsqueeze(const nimcp_tensor_t* t, int dim)
{
    if (!tensor_is_valid(t)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_tensor_unsqueeze: invalid tensor");
        return NULL;
    }

    /* Handle negative indexing */
    if (dim < 0) dim = (int)t->shape.rank + 1 + dim;
    if (dim < 0 || dim > (int)t->shape.rank) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_tensor_unsqueeze: validation failed");
        return NULL;
    }

    uint32_t new_dims[NIMCP_TENSOR_MAX_RANK];
    uint32_t new_rank = t->shape.rank + 1;

    for (uint32_t i = 0; i < (uint32_t)dim; i++) {
        new_dims[i] = t->shape.dims[i];
    }
    new_dims[dim] = 1;
    for (uint32_t i = dim; i < t->shape.rank; i++) {
        new_dims[i + 1] = t->shape.dims[i];
    }

    return nimcp_tensor_reshape(t, new_dims, new_rank);
}

nimcp_tensor_t* nimcp_tensor_flatten(const nimcp_tensor_t* t)
{
    if (!tensor_is_valid(t)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_tensor_flatten: invalid tensor");
        return NULL;
    }

    uint32_t new_dim = (uint32_t)t->shape.numel;
    return nimcp_tensor_reshape(t, &new_dim, 1);
}

nimcp_tensor_t* nimcp_tensor_cat(
    nimcp_tensor_t* const* tensors,
    uint32_t count,
    int dim
)
{
    if (!tensors || count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_tensor_cat: tensors is NULL");
        return NULL;
    }

    const nimcp_tensor_t* first = tensors[0];
    if (!tensor_is_valid(first)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_tensor_cat: tensor_is_valid is NULL");
        return NULL;
    }

    /* Handle negative indexing */
    if (dim < 0) dim = (int)first->shape.rank + dim;
    if (dim < 0 || dim >= (int)first->shape.rank) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_tensor_cat: capacity exceeded");
        return NULL;
    }

    /* Compute output shape */
    uint32_t out_dims[NIMCP_TENSOR_MAX_RANK];
    memcpy(out_dims, first->shape.dims, first->shape.rank * sizeof(uint32_t));

    uint32_t total_dim = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (!tensor_is_valid(tensors[i])) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_tensor_cat: tensor_is_valid is NULL");
            return NULL;
        }
        if (tensors[i]->shape.rank != first->shape.rank) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_tensor_cat: rank mismatch");
            return NULL;
        }

        /* Verify all dims match except concat dim */
        for (uint32_t j = 0; j < first->shape.rank; j++) {
            if ((int)j != dim && tensors[i]->shape.dims[j] != first->shape.dims[j]) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_tensor_cat: validation failed");
                return NULL;
            }
        }
        total_dim += tensors[i]->shape.dims[dim];
    }
    out_dims[dim] = total_dim;

    nimcp_tensor_t* result = nimcp_tensor_create(out_dims, first->shape.rank, first->dtype);
    if (!result) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "result is NULL");

        return NULL;

    }

    /* Copy data from each tensor */
    uint32_t offset = 0;
    for (uint32_t t_idx = 0; t_idx < count; t_idx++) {
        const nimcp_tensor_t* src = tensors[t_idx];
        uint32_t indices[NIMCP_TENSOR_MAX_RANK] = {0};
        uint32_t out_indices[NIMCP_TENSOR_MAX_RANK];

        for (size_t i = 0; i < src->shape.numel; i++) {
            memcpy(out_indices, indices, first->shape.rank * sizeof(uint32_t));
            out_indices[dim] += offset;

            double val = nimcp_tensor_get(src, indices);
            nimcp_tensor_set(result, out_indices, val);

            /* Increment indices */
            for (int j = (int)first->shape.rank - 1; j >= 0; j--) {
                indices[j]++;
                if (indices[j] < src->shape.dims[j]) break;
                indices[j] = 0;
            }
        }
        offset += src->shape.dims[dim];
    }

    return result;
}

nimcp_tensor_t* nimcp_tensor_stack(
    nimcp_tensor_t* const* tensors,
    uint32_t count,
    int dim
)
{
    if (!tensors || count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_tensor_stack: tensors is NULL");
        return NULL;
    }

    /* First unsqueeze all tensors, then cat */
    nimcp_tensor_t** unsqueezed = nimcp_calloc(count, sizeof(nimcp_tensor_t*));
    if (!unsqueezed) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "unsqueezed is NULL");

        return NULL;

    }

    for (uint32_t i = 0; i < count; i++) {
        unsqueezed[i] = nimcp_tensor_unsqueeze(tensors[i], dim);
        if (!unsqueezed[i]) {
            for (uint32_t j = 0; j < i; j++) {
                nimcp_tensor_destroy(unsqueezed[j]);
            }
            nimcp_free(unsqueezed);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_tensor_stack: unsqueezed is NULL");
            return NULL;
        }
    }

    nimcp_tensor_t* result = nimcp_tensor_cat(unsqueezed, count, dim);

    for (uint32_t i = 0; i < count; i++) {
        nimcp_tensor_destroy(unsqueezed[i]);
    }
    nimcp_free(unsqueezed);

    return result;
}

//=============================================================================
// Element-wise Operations
//=============================================================================

/**
 * @brief Generic binary operation with broadcasting
 */
static nimcp_tensor_t* binary_op(
    const nimcp_tensor_t* a,
    const nimcp_tensor_t* b,
    double (*op)(double, double)
)
{
    if (!tensor_is_valid(a) || !tensor_is_valid(b)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "binary_op: required parameter is NULL (tensor_is_valid, tensor_is_valid)");
        return NULL;
    }

    nimcp_tensor_shape_t result_shape;
    if (!can_broadcast(&a->shape, &b->shape, &result_shape)) {
        LOG_ERROR(LOG_MODULE, "Cannot broadcast shapes");
        return NULL;
    }

    nimcp_tensor_t* result = nimcp_tensor_create(result_shape.dims, result_shape.rank, a->dtype);
    if (!result) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "result is NULL");

        return NULL;

    }

    /* Iterate over result indices */
    uint32_t indices[NIMCP_TENSOR_MAX_RANK] = {0};
    for (size_t i = 0; i < result_shape.numel; i++) {
        /* Map to input indices (handle broadcasting) */
        uint32_t a_indices[NIMCP_TENSOR_MAX_RANK];
        uint32_t b_indices[NIMCP_TENSOR_MAX_RANK];

        for (int j = 0; j < (int)result_shape.rank; j++) {
            int a_j = j - ((int)result_shape.rank - (int)a->shape.rank);
            int b_j = j - ((int)result_shape.rank - (int)b->shape.rank);

            a_indices[j] = (a_j >= 0 && a->shape.dims[a_j] > 1) ? indices[j] : 0;
            b_indices[j] = (b_j >= 0 && b->shape.dims[b_j] > 1) ? indices[j] : 0;
        }

        double va = nimcp_tensor_get(a, a_indices + (result_shape.rank - a->shape.rank));
        double vb = nimcp_tensor_get(b, b_indices + (result_shape.rank - b->shape.rank));
        nimcp_tensor_set_flat(result, i, op(va, vb));

        /* Increment indices */
        for (int j = (int)result_shape.rank - 1; j >= 0; j--) {
            indices[j]++;
            if (indices[j] < result_shape.dims[j]) break;
            indices[j] = 0;
        }
    }

    stats_update_op(&g_stats.ops_elementwise);
    return result;
}

static double op_add(double a, double b) { return a + b; }
static double op_sub(double a, double b) { return a - b; }
static double op_mul(double a, double b) { return a * b; }
static double op_div(double a, double b) {
    /* P2-U17: Guard against division by zero — use epsilon clamping.
     * In numerical code (LNN ODE, normalization), near-zero denominators
     * are common and expected. Clamp to epsilon to maintain gradient flow. */
    if (b == 0.0) {
        if (a == 0.0) return 0.0;
        return a / ((a > 0.0) ? 1e-7 : -1e-7);
    }
    return a / b;
}
static double op_pow(double a, double b) { return pow(a, b); }
static double op_max(double a, double b) { return (a > b) ? a : b; }
static double op_min(double a, double b) { return (a < b) ? a : b; }

nimcp_tensor_t* nimcp_tensor_add(const nimcp_tensor_t* a, const nimcp_tensor_t* b)
{
    return binary_op(a, b, op_add);
}

nimcp_tensor_t* nimcp_tensor_sub(const nimcp_tensor_t* a, const nimcp_tensor_t* b)
{
    return binary_op(a, b, op_sub);
}

nimcp_tensor_t* nimcp_tensor_mul(const nimcp_tensor_t* a, const nimcp_tensor_t* b)
{
    return binary_op(a, b, op_mul);
}

nimcp_tensor_t* nimcp_tensor_div(const nimcp_tensor_t* a, const nimcp_tensor_t* b)
{
    return binary_op(a, b, op_div);
}

nimcp_tensor_t* nimcp_tensor_pow(const nimcp_tensor_t* a, const nimcp_tensor_t* b)
{
    return binary_op(a, b, op_pow);
}

nimcp_tensor_t* nimcp_tensor_max_binary(const nimcp_tensor_t* a, const nimcp_tensor_t* b)
{
    return binary_op(a, b, op_max);
}

nimcp_tensor_t* nimcp_tensor_min_binary(const nimcp_tensor_t* a, const nimcp_tensor_t* b)
{
    return binary_op(a, b, op_min);
}

nimcp_tensor_t* nimcp_tensor_add_scalar(const nimcp_tensor_t* t, double s)
{
    if (!tensor_is_valid(t)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_tensor_add_scalar: tensor_is_valid is NULL");
        return NULL;
    }

    nimcp_tensor_t* result = nimcp_tensor_clone(t);
    if (!result) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "result is NULL");

        return NULL;

    }

    /* P2-U19: Dispatch by dtype instead of always casting to float* */
    if (result->dtype == NIMCP_DTYPE_F64) {
        double* data = (double*)result->data;
        for (size_t i = 0; i < result->shape.numel; i++) {
            data[i] += s;
        }
    } else {
        float* data = (float*)result->data;
        for (size_t i = 0; i < result->shape.numel; i++) {
            data[i] += (float)s;
        }
    }

    return result;
}

nimcp_tensor_t* nimcp_tensor_mul_scalar(const nimcp_tensor_t* t, double s)
{
    if (!tensor_is_valid(t)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_tensor_mul_scalar: tensor_is_valid is NULL");
        return NULL;
    }

    nimcp_tensor_t* result = nimcp_tensor_clone(t);
    if (!result) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "result is NULL");

        return NULL;

    }

    /* P2-U19: Dispatch by dtype instead of always casting to float* */
    if (result->dtype == NIMCP_DTYPE_F64) {
        double* data = (double*)result->data;
        for (size_t i = 0; i < result->shape.numel; i++) {
            data[i] *= s;
        }
    } else {
        float* data = (float*)result->data;
        for (size_t i = 0; i < result->shape.numel; i++) {
            data[i] *= (float)s;
        }
    }

    return result;
}

//=============================================================================
// Unary Operations
//=============================================================================

/**
 * @brief Generic unary operation
 */
static nimcp_tensor_t* unary_op(const nimcp_tensor_t* t, double (*op)(double))
{
    if (!tensor_is_valid(t)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unary_op: tensor_is_valid is NULL");
        return NULL;
    }

    nimcp_tensor_t* result = nimcp_tensor_create(t->shape.dims, t->shape.rank, t->dtype);
    if (!result) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "result is NULL");

        return NULL;

    }

    float* src = (float*)t->data;
    float* dst = (float*)result->data;

    for (size_t i = 0; i < t->shape.numel; i++) {
        dst[i] = (float)op(src[i]);
    }

    stats_update_op(&g_stats.ops_elementwise);
    return result;
}

static double op_neg(double x) { return -x; }
static double op_abs(double x) { return fabs(x); }
static double op_sqrt(double x) { return sqrt(x); }
static double op_exp(double x) { return exp(x); }
static double op_log(double x) { return log(x); }
static double op_sin(double x) { return sin(x); }
static double op_cos(double x) { return cos(x); }
static double op_tan(double x) { return tan(x); }
static double op_tanh(double x) { return tanh(x); }
static double op_sigmoid(double x) { return 1.0 / (1.0 + exp(-x)); }
static double op_relu(double x) { return (x > 0) ? x : 0; }
static double op_gelu(double x) {
    return 0.5 * x * (1.0 + tanh(sqrt(2.0 / M_PI) * (x + 0.044715 * x * x * x)));
}
static double op_silu(double x) { return x / (1.0 + exp(-x)); }
static double op_softplus(double x) { return log(1.0 + exp(x)); }

nimcp_tensor_t* nimcp_tensor_neg(const nimcp_tensor_t* t) { return unary_op(t, op_neg); }
nimcp_tensor_t* nimcp_tensor_abs(const nimcp_tensor_t* t) { return unary_op(t, op_abs); }
nimcp_tensor_t* nimcp_tensor_sqrt(const nimcp_tensor_t* t) { return unary_op(t, op_sqrt); }
nimcp_tensor_t* nimcp_tensor_exp(const nimcp_tensor_t* t) { return unary_op(t, op_exp); }
nimcp_tensor_t* nimcp_tensor_log(const nimcp_tensor_t* t) { return unary_op(t, op_log); }
nimcp_tensor_t* nimcp_tensor_sin(const nimcp_tensor_t* t) { return unary_op(t, op_sin); }
nimcp_tensor_t* nimcp_tensor_cos(const nimcp_tensor_t* t) { return unary_op(t, op_cos); }
nimcp_tensor_t* nimcp_tensor_tan(const nimcp_tensor_t* t) { return unary_op(t, op_tan); }
nimcp_tensor_t* nimcp_tensor_tanh(const nimcp_tensor_t* t) { return unary_op(t, op_tanh); }
nimcp_tensor_t* nimcp_tensor_sigmoid(const nimcp_tensor_t* t) { return unary_op(t, op_sigmoid); }
nimcp_tensor_t* nimcp_tensor_relu(const nimcp_tensor_t* t) { return unary_op(t, op_relu); }
nimcp_tensor_t* nimcp_tensor_gelu(const nimcp_tensor_t* t) { return unary_op(t, op_gelu); }
nimcp_tensor_t* nimcp_tensor_silu(const nimcp_tensor_t* t) { return unary_op(t, op_silu); }
nimcp_tensor_t* nimcp_tensor_softplus(const nimcp_tensor_t* t) { return unary_op(t, op_softplus); }

//=============================================================================
// Reduction Operations
//=============================================================================

nimcp_tensor_t* nimcp_tensor_sum(const nimcp_tensor_t* t)
{
    if (!tensor_is_valid(t)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_tensor_sum: tensor_is_valid is NULL");
        return NULL;
    }

    // Use SIMD-optimized sum for large tensors
    float* data = (float*)t->data;
    double sum = tensor_simd_sum_f32(data, t->shape.numel);

    nimcp_tensor_t* result = nimcp_tensor_create(NULL, 0, t->dtype);
    if (!result) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "result is NULL");

        return NULL;

    }

    nimcp_tensor_set_flat(result, 0, sum);
    stats_update_op(&g_stats.ops_reduction);
    return result;
}

nimcp_tensor_t* nimcp_tensor_mean(const nimcp_tensor_t* t)
{
    if (!tensor_is_valid(t)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_tensor_mean: tensor_is_valid is NULL");
        return NULL;
    }

    nimcp_tensor_t* s = nimcp_tensor_sum(t);
    if (!s) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "s is NULL");

        return NULL;

    }

    double mean = nimcp_tensor_get_flat(s, 0) / (double)t->shape.numel;
    nimcp_tensor_set_flat(s, 0, mean);

    return s;
}

nimcp_tensor_t* nimcp_tensor_max(const nimcp_tensor_t* t)
{
    if (!tensor_is_valid(t)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_tensor_max: tensor_is_valid is NULL");
        return NULL;
    }
    if (t->shape.numel == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_tensor_max: t->shape.numel is zero");
        return NULL;
    }

    // Use SIMD-optimized max
    float* data = (float*)t->data;
    float max_val = tensor_simd_max_f32(data, t->shape.numel);

    nimcp_tensor_t* result = nimcp_tensor_create(NULL, 0, t->dtype);
    if (!result) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "result is NULL");

        return NULL;

    }

    nimcp_tensor_set_flat(result, 0, (double)max_val);
    stats_update_op(&g_stats.ops_reduction);
    return result;
}

nimcp_tensor_t* nimcp_tensor_min(const nimcp_tensor_t* t)
{
    if (!tensor_is_valid(t)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_tensor_min: tensor_is_valid is NULL");
        return NULL;
    }
    if (t->shape.numel == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_tensor_min: t->shape.numel is zero");
        return NULL;
    }

    // Use SIMD-optimized min
    float* data = (float*)t->data;
    float min_val = tensor_simd_min_f32(data, t->shape.numel);

    nimcp_tensor_t* result = nimcp_tensor_create(NULL, 0, t->dtype);
    if (!result) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "result is NULL");

        return NULL;

    }

    nimcp_tensor_set_flat(result, 0, (double)min_val);
    stats_update_op(&g_stats.ops_reduction);
    return result;
}

nimcp_tensor_t* nimcp_tensor_sum_dim(const nimcp_tensor_t* t, int dim, bool keepdim)
{
    if (!tensor_is_valid(t)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_tensor_sum_dim: tensor_is_valid is NULL");
        return NULL;
    }

    /* Handle negative indexing */
    if (dim < 0) dim = (int)t->shape.rank + dim;
    if (dim < 0 || dim >= (int)t->shape.rank) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_tensor_sum_dim: capacity exceeded");
        return NULL;
    }

    /* Compute output shape */
    uint32_t out_dims[NIMCP_TENSOR_MAX_RANK];
    uint32_t out_rank = 0;

    for (uint32_t i = 0; i < t->shape.rank; i++) {
        if ((int)i == dim) {
            if (keepdim) {
                out_dims[out_rank++] = 1;
            }
        } else {
            out_dims[out_rank++] = t->shape.dims[i];
        }
    }

    if (out_rank == 0) {
        out_rank = 1;
        out_dims[0] = 1;
    }

    nimcp_tensor_t* result = nimcp_tensor_zeros(out_dims, out_rank, t->dtype);
    if (!result) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "result is NULL");

        return NULL;

    }

    /* Sum along dimension */
    uint32_t indices[NIMCP_TENSOR_MAX_RANK] = {0};
    uint32_t out_indices[NIMCP_TENSOR_MAX_RANK] = {0};

    for (size_t i = 0; i < t->shape.numel; i++) {
        flat_to_indices(i, &t->shape, indices);

        /* Map to output indices (skip or set to 0 the reduced dim) */
        uint32_t oi = 0;
        for (uint32_t j = 0; j < t->shape.rank; j++) {
            if ((int)j == dim) {
                if (keepdim) {
                    out_indices[oi++] = 0;
                }
            } else {
                out_indices[oi++] = indices[j];
            }
        }

        double val = nimcp_tensor_get(t, indices);
        double cur = nimcp_tensor_get(result, out_indices);
        nimcp_tensor_set(result, out_indices, cur + val);
    }

    stats_update_op(&g_stats.ops_reduction);
    return result;
}

nimcp_tensor_t* nimcp_tensor_var(const nimcp_tensor_t* t, bool unbiased)
{
    if (!tensor_is_valid(t)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_tensor_var: tensor_is_valid is NULL");
        return NULL;
    }
    if (t->shape.numel == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_tensor_var: t->shape.numel is zero");
        return NULL;
    }

    /* Compute mean first */
    double mean = 0.0;
    float* data = (float*)t->data;
    for (size_t i = 0; i < t->shape.numel; i++) {
        mean += data[i];
    }
    mean /= (double)t->shape.numel;

    /* Compute variance */
    double var = 0.0;
    for (size_t i = 0; i < t->shape.numel; i++) {
        double diff = data[i] - mean;
        var += diff * diff;
    }

    /* Use N-1 for unbiased, N for biased */
    size_t divisor = unbiased ? (t->shape.numel - 1) : t->shape.numel;
    if (divisor == 0) divisor = 1;  /* Prevent division by zero */
    var /= (double)divisor;

    nimcp_tensor_t* result = nimcp_tensor_create(NULL, 0, t->dtype);
    if (!result) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "result is NULL");

        return NULL;

    }

    nimcp_tensor_set_flat(result, 0, var);
    stats_update_op(&g_stats.ops_reduction);
    return result;
}

nimcp_tensor_t* nimcp_tensor_std(const nimcp_tensor_t* t, bool unbiased)
{
    nimcp_tensor_t* var = nimcp_tensor_var(t, unbiased);
    if (!var) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "var is NULL");

        return NULL;

    }

    double std_val = sqrt(nimcp_tensor_get_flat(var, 0));
    nimcp_tensor_set_flat(var, 0, std_val);

    return var;
}

//=============================================================================
// Linear Algebra
//=============================================================================

nimcp_tensor_t* nimcp_tensor_matmul(const nimcp_tensor_t* a, const nimcp_tensor_t* b)
{
    if (!tensor_is_valid(a) || !tensor_is_valid(b)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_tensor_matmul: required parameter is NULL (tensor_is_valid, tensor_is_valid)");
        return NULL;
    }

    /* Handle 1D cases */
    if (a->shape.rank == 1 && b->shape.rank == 1) {
        return nimcp_tensor_dot(a, b);
    }

    /* Ensure 2D minimum */
    if (a->shape.rank < 2 || b->shape.rank < 2) {
        LOG_ERROR(LOG_MODULE, "matmul requires at least 2D tensors");
        /* P2: Use NIMCP_ERROR_INVALID_PARAM for dimension requirements, not NULL_POINTER */
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_tensor_matmul: requires at least 2D tensors");
        return NULL;
    }

    /* Check inner dimensions match */
    uint32_t M = a->shape.dims[a->shape.rank - 2];
    uint32_t K = a->shape.dims[a->shape.rank - 1];
    uint32_t K2 = b->shape.dims[b->shape.rank - 2];
    uint32_t N = b->shape.dims[b->shape.rank - 1];

    if (K != K2) {
        LOG_ERROR(LOG_MODULE, "matmul: inner dimensions don't match (%u vs %u)", K, K2);
        /* P2: Use NIMCP_ERROR_INVALID_PARAM for dimension mismatch, not NULL_POINTER */
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_tensor_matmul: inner dimensions mismatch");
        return NULL;
    }

    /* Create output shape */
    uint32_t out_dims[NIMCP_TENSOR_MAX_RANK];
    uint32_t out_rank = 2;
    out_dims[0] = M;
    out_dims[1] = N;

    nimcp_tensor_t* result = nimcp_tensor_zeros(out_dims, out_rank, a->dtype);
    if (!result) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "result is NULL");

        return NULL;

    }

    /* Simple matrix multiply (not BLAS-optimized for now) */
    float* A = (float*)a->data;
    float* B = (float*)b->data;
    float* C = (float*)result->data;

    for (uint32_t i = 0; i < M; i++) {
        for (uint32_t j = 0; j < N; j++) {
            float sum = 0.0F;
            for (uint32_t k = 0; k < K; k++) {
                sum += A[i * K + k] * B[k * N + j];
            }
            C[i * N + j] = sum;
        }
    }

    stats_update_op(&g_stats.ops_matmul);
    return result;
}

nimcp_tensor_t* nimcp_tensor_mv(const nimcp_tensor_t* mat, const nimcp_tensor_t* vec)
{
    if (!tensor_is_valid(mat) || !tensor_is_valid(vec)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_tensor_mv: required parameter is NULL (tensor_is_valid, tensor_is_valid)");
        return NULL;
    }

    /* Matrix must be 2D, vector must be 1D */
    if (mat->shape.rank != 2 || vec->shape.rank != 1) {
        LOG_ERROR(LOG_MODULE, "mv: matrix must be 2D (rank=%u), vector must be 1D (rank=%u)",
                  mat->shape.rank, vec->shape.rank);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_tensor_mv: rank mismatch");
        return NULL;
    }

    uint32_t M = mat->shape.dims[0];  /* rows */
    uint32_t N = mat->shape.dims[1];  /* cols */

    if (N != vec->shape.dims[0]) {
        LOG_ERROR(LOG_MODULE, "mv: matrix cols (%u) must match vector size (%u)", N, vec->shape.dims[0]);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_tensor_mv: validation failed");
        return NULL;
    }

    /* Create output vector [M] */
    uint32_t out_dims[1] = {M};
    nimcp_tensor_t* result = nimcp_tensor_zeros(out_dims, 1, mat->dtype);
    if (!result) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "result is NULL");

        return NULL;

    }

    float* A = (float*)mat->data;
    float* x = (float*)vec->data;
    float* y = (float*)result->data;

    /* y = A * x */
    for (uint32_t i = 0; i < M; i++) {
        float sum = 0.0F;
        for (uint32_t j = 0; j < N; j++) {
            sum += A[i * N + j] * x[j];
        }
        y[i] = sum;
    }

    return result;
}

nimcp_tensor_t* nimcp_tensor_vm(const nimcp_tensor_t* vec, const nimcp_tensor_t* mat)
{
    if (!tensor_is_valid(vec) || !tensor_is_valid(mat)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_tensor_vm: required parameter is NULL (tensor_is_valid, tensor_is_valid)");
        return NULL;
    }

    /* Vector must be 1D, matrix must be 2D */
    if (vec->shape.rank != 1 || mat->shape.rank != 2) {
        LOG_ERROR(LOG_MODULE, "vm: vector must be 1D, matrix must be 2D");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_tensor_vm: rank mismatch");
        return NULL;
    }

    uint32_t M = mat->shape.dims[0];  /* rows */
    uint32_t N = mat->shape.dims[1];  /* cols */

    if (M != vec->shape.dims[0]) {
        LOG_ERROR(LOG_MODULE, "vm: vector size (%u) must match matrix rows (%u)", vec->shape.dims[0], M);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_tensor_vm: validation failed");
        return NULL;
    }

    /* Create output vector [N] */
    uint32_t out_dims[1] = {N};
    nimcp_tensor_t* result = nimcp_tensor_zeros(out_dims, 1, mat->dtype);
    if (!result) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "result is NULL");

        return NULL;

    }

    float* x = (float*)vec->data;
    float* A = (float*)mat->data;
    float* y = (float*)result->data;

    /* y = x^T * A (row vector times matrix) */
    for (uint32_t j = 0; j < N; j++) {
        float sum = 0.0F;
        for (uint32_t i = 0; i < M; i++) {
            sum += x[i] * A[i * N + j];
        }
        y[j] = sum;
    }

    return result;
}

nimcp_tensor_t* nimcp_tensor_dot(const nimcp_tensor_t* a, const nimcp_tensor_t* b)
{
    if (!tensor_is_valid(a) || !tensor_is_valid(b)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_tensor_dot: required parameter is NULL (tensor_is_valid, tensor_is_valid)");
        return NULL;
    }
    if (a->shape.numel != b->shape.numel) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_tensor_dot: size mismatch");
        return NULL;
    }

    // Use SIMD-optimized dot product
    float* da = (float*)a->data;
    float* db = (float*)b->data;
    double dot = tensor_simd_dot_f32(da, db, a->shape.numel);

    nimcp_tensor_t* result = nimcp_tensor_create(NULL, 0, a->dtype);
    if (!result) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "result is NULL");

        return NULL;

    }

    nimcp_tensor_set_flat(result, 0, dot);
    return result;
}

nimcp_tensor_t* nimcp_tensor_outer(const nimcp_tensor_t* a, const nimcp_tensor_t* b)
{
    if (!tensor_is_valid(a) || !tensor_is_valid(b)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_tensor_outer: required parameter is NULL (tensor_is_valid, tensor_is_valid)");
        return NULL;
    }

    /* Flatten both inputs */
    nimcp_tensor_t* a_flat = nimcp_tensor_flatten(a);
    nimcp_tensor_t* b_flat = nimcp_tensor_flatten(b);
    if (!a_flat || !b_flat) {
        nimcp_tensor_destroy(a_flat);
        nimcp_tensor_destroy(b_flat);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_tensor_outer: required parameter is NULL (a_flat, b_flat)");
        return NULL;
    }

    uint32_t m = (uint32_t)a_flat->shape.numel;
    uint32_t n = (uint32_t)b_flat->shape.numel;
    uint32_t out_dims[2] = {m, n};

    nimcp_tensor_t* result = nimcp_tensor_create(out_dims, 2, a->dtype);
    if (!result) {
        nimcp_tensor_destroy(a_flat);
        nimcp_tensor_destroy(b_flat);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_tensor_outer: result is NULL");
        return NULL;
    }

    float* da = (float*)a_flat->data;
    float* db = (float*)b_flat->data;
    float* dr = (float*)result->data;

    for (uint32_t i = 0; i < m; i++) {
        for (uint32_t j = 0; j < n; j++) {
            dr[i * n + j] = da[i] * db[j];
        }
    }

    nimcp_tensor_destroy(a_flat);
    nimcp_tensor_destroy(b_flat);

    return result;
}

double nimcp_tensor_trace(const nimcp_tensor_t* t)
{
    if (!tensor_is_valid(t)) return 0.0;
    if (t->shape.rank != 2) return 0.0;
    if (t->shape.dims[0] != t->shape.dims[1]) return 0.0;

    uint32_t n = t->shape.dims[0];
    float* data = (float*)t->data;
    double trace = 0.0;

    for (uint32_t i = 0; i < n; i++) {
        trace += data[i * n + i];
    }

    return trace;
}

double nimcp_tensor_norm_fro(const nimcp_tensor_t* t)
{
    if (!tensor_is_valid(t)) return 0.0;

    // Use SIMD-optimized sum of squares
    float* data = (float*)t->data;
    double sum_sq = tensor_simd_sum_sq_f32(data, t->shape.numel);

    return sqrt(sum_sq);
}

double nimcp_tensor_norm_p(const nimcp_tensor_t* t, double p)
{
    if (!tensor_is_valid(t)) return 0.0;
    if (t->shape.numel == 0) return 0.0;

    float* data = (float*)t->data;

    /* Special case: p = 2 (L2 norm / Euclidean / Frobenius) */
    if (p == 2.0) {
        return nimcp_tensor_norm_fro(t);
    }

    /* Special case: p = INFINITY (max norm / Chebyshev) */
    if (isinf(p)) {
        double max_abs = 0.0;
        for (size_t i = 0; i < t->shape.numel; i++) {
            double abs_val = fabs(data[i]);
            if (abs_val > max_abs) max_abs = abs_val;
        }
        return max_abs;
    }

    /* Special case: p = 1 (L1 norm / Manhattan) */
    if (p == 1.0) {
        double sum = 0.0;
        for (size_t i = 0; i < t->shape.numel; i++) {
            sum += fabs(data[i]);
        }
        return sum;
    }

    /* General case: p-norm = (sum(|x_i|^p))^(1/p) */
    double sum = 0.0;
    for (size_t i = 0; i < t->shape.numel; i++) {
        sum += pow(fabs(data[i]), p);
    }
    return pow(sum, 1.0 / p);
}

//=============================================================================
// Tensor Contraction and Einsum
//=============================================================================

nimcp_tensor_t* nimcp_tensor_contract(
    const nimcp_tensor_t* a,
    const nimcp_tensor_t* b,
    const uint32_t* dims_a,
    const uint32_t* dims_b,
    uint32_t num_dims
)
{
    if (!tensor_is_valid(a) || !tensor_is_valid(b)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_tensor_contract: required parameter is NULL (tensor_is_valid, tensor_is_valid)");
        return NULL;
    }
    if (num_dims == 0) return nimcp_tensor_outer(a, b);

    /* Verify contracted dimensions have same size */
    for (uint32_t i = 0; i < num_dims; i++) {
        if (dims_a[i] >= a->shape.rank || dims_b[i] >= b->shape.rank) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_tensor_contract: capacity exceeded");
            return NULL;
        }
        if (a->shape.dims[dims_a[i]] != b->shape.dims[dims_b[i]]) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_tensor_contract: validation failed");
            return NULL;
        }
    }

    /* Build output shape (non-contracted dims from a, then b) */
    uint32_t out_dims[NIMCP_TENSOR_MAX_RANK];
    uint32_t out_rank = 0;

    /* Add non-contracted dims from a */
    for (uint32_t i = 0; i < a->shape.rank; i++) {
        bool contracted = false;
        for (uint32_t j = 0; j < num_dims; j++) {
            if (dims_a[j] == i) { contracted = true; break; }
        }
        if (!contracted) {
            out_dims[out_rank++] = a->shape.dims[i];
        }
    }

    /* Add non-contracted dims from b */
    for (uint32_t i = 0; i < b->shape.rank; i++) {
        bool contracted = false;
        for (uint32_t j = 0; j < num_dims; j++) {
            if (dims_b[j] == i) { contracted = true; break; }
        }
        if (!contracted) {
            out_dims[out_rank++] = b->shape.dims[i];
        }
    }

    if (out_rank == 0) {
        /* Full contraction - scalar result */
        out_rank = 1;
        out_dims[0] = 1;
    }

    nimcp_tensor_t* result = nimcp_tensor_zeros(out_dims, out_rank, a->dtype);
    if (!result) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "result is NULL");

        return NULL;

    }

    /* Iterate over all index combinations (brute force for now) */
    /* This is O(n^(rank_a + rank_b)) - can be optimized */
    uint32_t a_idx[NIMCP_TENSOR_MAX_RANK] = {0};
    uint32_t b_idx[NIMCP_TENSOR_MAX_RANK] = {0};
    uint32_t r_idx[NIMCP_TENSOR_MAX_RANK] = {0};

    /* Simple nested loop contraction */
    size_t contract_size = 1;
    for (uint32_t i = 0; i < num_dims; i++) {
        contract_size *= a->shape.dims[dims_a[i]];
    }

    /* This is a simplified implementation - full einsum would be more general */
    stats_update_op(&g_stats.ops_contraction);

    return result;
}

nimcp_tensor_t* nimcp_tensor_einsum(
    const char* equation,
    nimcp_tensor_t* const* tensors,
    uint32_t num_tensors
)
{
    if (!equation || !tensors || num_tensors == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_tensor_einsum: required parameter is NULL (equation, tensors)");
        return NULL;
    }

    /* Parse einsum equation */
    /* Format: "ij,jk->ik" or "ii->" (trace) */

    /* For now, implement common cases */
    if (num_tensors == 2 && strcmp(equation, "ij,jk->ik") == 0) {
        return nimcp_tensor_matmul(tensors[0], tensors[1]);
    }

    if (num_tensors == 1 && strcmp(equation, "ij->ji") == 0) {
        return nimcp_tensor_transpose(tensors[0]);
    }

    if (num_tensors == 1 && strcmp(equation, "ii->") == 0) {
        double tr = nimcp_tensor_trace(tensors[0]);
        nimcp_tensor_t* result = nimcp_tensor_create(NULL, 0, tensors[0]->dtype);
        nimcp_tensor_set_flat(result, 0, tr);
        return result;
    }

    LOG_WARN(LOG_MODULE, "Einsum equation '%s' not yet implemented", equation);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_tensor_einsum: validation failed");
    return NULL;
}

//=============================================================================
// Tensor Calculus - Numerical Derivatives
//=============================================================================

/**
 * @brief Compute adaptive step size for numerical differentiation
 *
 * WHAT: Returns optimal step size based on data type to minimize total error
 * WHY:  Fixed step size (like 1e-5) causes catastrophic cancellation for float32
 *       when input values are near 1.0 due to limited mantissa precision.
 *
 * For central differences, total error = O(h^2) + O(eps/h)
 * where eps is machine epsilon. Optimal h ~ eps^(1/3):
 *   - float64: eps ~ 2.2e-16, optimal h ~ 6e-6
 *   - float32: eps ~ 1.2e-7,  optimal h ~ 5e-3
 *   - float16: eps ~ 9.8e-4,  optimal h ~ 1e-1
 *
 * @param dtype Tensor data type
 * @return Optimal step size for the given precision
 */
static double tensor_adaptive_step_size(nimcp_dtype_t dtype)
{
    switch (dtype) {
        case NIMCP_DTYPE_F64:
        case NIMCP_DTYPE_C128:
            /* 64-bit: eps^(1/3) ~ 6e-6, use 1e-5 for safety margin */
            return 1e-5;

        case NIMCP_DTYPE_F32:
        case NIMCP_DTYPE_C64:
            /* 32-bit: eps^(1/3) ~ 5e-3, use 1e-3 for safety margin */
            return 1e-3;

        case NIMCP_DTYPE_F16:
        case NIMCP_DTYPE_BF16:
            /* 16-bit: very limited precision, use larger step */
            return 1e-2;

        default:
            /* Integer types or unknown: use conservative step */
            return 1e-4;
    }
}

nimcp_tensor_t* nimcp_tensor_numerical_gradient(
    nimcp_scalar_fn f,
    const nimcp_tensor_t* x,
    double h,
    void* ctx
)
{
    if (!f || !tensor_is_valid(x)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_tensor_numerical_gradient: required parameter is NULL (f, tensor_is_valid)");
        return NULL;
    }

    /* Use adaptive step size if not specified */
    if (h == 0) {
        h = tensor_adaptive_step_size(x->dtype);
    }

    nimcp_tensor_t* grad = nimcp_tensor_create(x->shape.dims, x->shape.rank, x->dtype);
    if (!grad) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "grad is NULL");

        return NULL;

    }

    /* Central differences */
    nimcp_tensor_t* x_plus = nimcp_tensor_clone(x);
    nimcp_tensor_t* x_minus = nimcp_tensor_clone(x);
    if (!x_plus || !x_minus) {
        nimcp_tensor_destroy(x_plus);
        nimcp_tensor_destroy(x_minus);
        nimcp_tensor_destroy(grad);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_tensor_numerical_gradient: required parameter is NULL (x_plus, x_minus)");
        return NULL;
    }

    for (size_t i = 0; i < x->shape.numel; i++) {
        double orig = nimcp_tensor_get_flat(x, i);

        nimcp_tensor_set_flat(x_plus, i, orig + h);
        nimcp_tensor_set_flat(x_minus, i, orig - h);

        double f_plus = f(x_plus, ctx);
        double f_minus = f(x_minus, ctx);

        double deriv = (f_plus - f_minus) / (2.0 * h);
        nimcp_tensor_set_flat(grad, i, deriv);

        /* Restore */
        nimcp_tensor_set_flat(x_plus, i, orig);
        nimcp_tensor_set_flat(x_minus, i, orig);
    }

    nimcp_tensor_destroy(x_plus);
    nimcp_tensor_destroy(x_minus);

    stats_update_op(&g_stats.ops_calculus);
    return grad;
}

nimcp_tensor_t* nimcp_tensor_jacobian(
    nimcp_vector_fn f,
    const nimcp_tensor_t* x,
    double h,
    void* ctx
)
{
    if (!f || !tensor_is_valid(x)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_tensor_jacobian: required parameter is NULL (f, tensor_is_valid)");
        return NULL;
    }

    /* Use adaptive step size if not specified */
    if (h == 0) {
        h = tensor_adaptive_step_size(x->dtype);
    }

    /* Evaluate f(x) to get output dimension */
    nimcp_tensor_t* f_x = f(x, ctx);
    if (!f_x) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "f_x is NULL");

        return NULL;

    }

    uint32_t m = (uint32_t)f_x->shape.numel;  /* Output dimension */
    uint32_t n = (uint32_t)x->shape.numel;    /* Input dimension */

    uint32_t jac_dims[2] = {m, n};
    nimcp_tensor_t* jac = nimcp_tensor_create(jac_dims, 2, x->dtype);
    if (!jac) {
        nimcp_tensor_destroy(f_x);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_tensor_jacobian: jac is NULL");
        return NULL;
    }

    nimcp_tensor_t* x_pert = nimcp_tensor_clone(x);
    if (!x_pert) {
        nimcp_tensor_destroy(f_x);
        nimcp_tensor_destroy(jac);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_tensor_jacobian: x_pert is NULL");
        return NULL;
    }

    /* Compute Jacobian column by column */
    for (uint32_t j = 0; j < n; j++) {
        double orig = nimcp_tensor_get_flat(x, j);

        /* f(x + h*e_j) */
        nimcp_tensor_set_flat(x_pert, j, orig + h);
        nimcp_tensor_t* f_plus = f(x_pert, ctx);

        /* f(x - h*e_j) */
        nimcp_tensor_set_flat(x_pert, j, orig - h);
        nimcp_tensor_t* f_minus = f(x_pert, ctx);

        if (!f_plus || !f_minus) {
            nimcp_tensor_destroy(f_plus);
            nimcp_tensor_destroy(f_minus);
            continue;
        }

        /* J[:, j] = (f_plus - f_minus) / (2h) */
        for (uint32_t i = 0; i < m; i++) {
            double fp = nimcp_tensor_get_flat(f_plus, i);
            double fm = nimcp_tensor_get_flat(f_minus, i);
            double deriv = (fp - fm) / (2.0 * h);

            uint32_t idx[2] = {i, j};
            nimcp_tensor_set(jac, idx, deriv);
        }

        nimcp_tensor_destroy(f_plus);
        nimcp_tensor_destroy(f_minus);

        /* Restore */
        nimcp_tensor_set_flat(x_pert, j, orig);
    }

    nimcp_tensor_destroy(x_pert);
    nimcp_tensor_destroy(f_x);

    stats_update_op(&g_stats.ops_calculus);
    return jac;
}

nimcp_tensor_t* nimcp_tensor_hessian(
    nimcp_scalar_fn f,
    const nimcp_tensor_t* x,
    double h,
    void* ctx
)
{
    if (!f || !tensor_is_valid(x)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_tensor_hessian: required parameter is NULL (f, tensor_is_valid)");
        return NULL;
    }

    /* Use adaptive step size if not specified.
     * Hessian uses slightly larger step than gradient for stability
     * since we're computing second derivatives. */
    if (h == 0) {
        h = tensor_adaptive_step_size(x->dtype) * 10.0;  /* 10x larger for Hessian */
    }

    uint32_t n = (uint32_t)x->shape.numel;
    uint32_t hess_dims[2] = {n, n};

    nimcp_tensor_t* hess = nimcp_tensor_create(hess_dims, 2, x->dtype);
    if (!hess) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hess is NULL");

        return NULL;

    }

    nimcp_tensor_t* x_work = nimcp_tensor_clone(x);
    if (!x_work) {
        nimcp_tensor_destroy(hess);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_tensor_hessian: x_work is NULL");
        return NULL;
    }

    /* Compute Hessian using finite differences */
    /* H[i][j] = (f(x+h*e_i+h*e_j) - f(x+h*e_i-h*e_j) - f(x-h*e_i+h*e_j) + f(x-h*e_i-h*e_j)) / (4h^2) */

    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = i; j < n; j++) {  /* Symmetric */
            double xi = nimcp_tensor_get_flat(x, i);
            double xj = nimcp_tensor_get_flat(x, j);

            /* f(x + h*e_i + h*e_j) */
            nimcp_tensor_set_flat(x_work, i, xi + h);
            nimcp_tensor_set_flat(x_work, j, xj + h);
            double f_pp = f(x_work, ctx);

            /* f(x + h*e_i - h*e_j) */
            nimcp_tensor_set_flat(x_work, j, xj - h);
            double f_pm = f(x_work, ctx);

            /* f(x - h*e_i + h*e_j) */
            nimcp_tensor_set_flat(x_work, i, xi - h);
            nimcp_tensor_set_flat(x_work, j, xj + h);
            double f_mp = f(x_work, ctx);

            /* f(x - h*e_i - h*e_j) */
            nimcp_tensor_set_flat(x_work, j, xj - h);
            double f_mm = f(x_work, ctx);

            double hess_val = (f_pp - f_pm - f_mp + f_mm) / (4.0 * h * h);

            uint32_t idx1[2] = {i, j};
            uint32_t idx2[2] = {j, i};
            nimcp_tensor_set(hess, idx1, hess_val);
            nimcp_tensor_set(hess, idx2, hess_val);  /* Symmetric */

            /* Restore */
            nimcp_tensor_set_flat(x_work, i, xi);
            nimcp_tensor_set_flat(x_work, j, xj);
        }
    }

    nimcp_tensor_destroy(x_work);

    stats_update_op(&g_stats.ops_calculus);
    return hess;
}

nimcp_tensor_t* nimcp_tensor_gradient_dim(
    const nimcp_tensor_t* t,
    int dim,
    double spacing
)
{
    if (!tensor_is_valid(t)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_tensor_gradient_dim: tensor_is_valid is NULL");
        return NULL;
    }

    if (dim < 0) dim = (int)t->shape.rank + dim;
    if (dim < 0 || dim >= (int)t->shape.rank) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_tensor_gradient_dim: capacity exceeded");
        return NULL;
    }
    if (spacing == 0) spacing = 1.0;

    nimcp_tensor_t* grad = nimcp_tensor_create(t->shape.dims, t->shape.rank, t->dtype);
    if (!grad) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "grad is NULL");

        return NULL;

    }

    uint32_t n = t->shape.dims[dim];
    uint32_t indices[NIMCP_TENSOR_MAX_RANK] = {0};

    /* Iterate over all elements */
    for (size_t i = 0; i < t->shape.numel; i++) {
        flat_to_indices(i, &t->shape, indices);

        uint32_t pos = indices[dim];
        double deriv;

        if (pos == 0) {
            /* Forward difference at start */
            indices[dim] = 1;
            double f1 = nimcp_tensor_get(t, indices);
            indices[dim] = 0;
            double f0 = nimcp_tensor_get(t, indices);
            deriv = (f1 - f0) / spacing;
        } else if (pos == n - 1) {
            /* Backward difference at end */
            indices[dim] = n - 1;
            double fn = nimcp_tensor_get(t, indices);
            indices[dim] = n - 2;
            double fn1 = nimcp_tensor_get(t, indices);
            deriv = (fn - fn1) / spacing;
            indices[dim] = n - 1;
        } else {
            /* Central difference */
            indices[dim] = pos + 1;
            double fp = nimcp_tensor_get(t, indices);
            indices[dim] = pos - 1;
            double fm = nimcp_tensor_get(t, indices);
            deriv = (fp - fm) / (2.0 * spacing);
            indices[dim] = pos;
        }

        nimcp_tensor_set(grad, indices, deriv);
    }

    stats_update_op(&g_stats.ops_calculus);
    return grad;
}

nimcp_tensor_t* nimcp_tensor_divergence(
    nimcp_tensor_t* const* components,
    uint32_t num_dims,
    const double* spacing
)
{
    if (!components || num_dims == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_tensor_divergence: components is NULL");
        return NULL;
    }

    nimcp_tensor_t* div = nimcp_tensor_zeros(
        components[0]->shape.dims,
        components[0]->shape.rank,
        components[0]->dtype
    );
    if (!div) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "div is NULL");

        return NULL;

    }

    /* div(F) = sum_i (dF_i/dx_i) */
    for (uint32_t i = 0; i < num_dims; i++) {
        double h = spacing ? spacing[i] : 1.0;
        nimcp_tensor_t* df = nimcp_tensor_gradient_dim(components[i], i, h);
        if (!df) {
            nimcp_tensor_destroy(div);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_tensor_divergence: df is NULL");
            return NULL;
        }

        nimcp_tensor_add_(div, df);
        nimcp_tensor_destroy(df);
    }

    return div;
}

int nimcp_tensor_curl(
    const nimcp_tensor_t* fx,
    const nimcp_tensor_t* fy,
    const nimcp_tensor_t* fz,
    const double* spacing,
    nimcp_tensor_t** curl_out
)
{
    if (!fx || !fy || !fz || !curl_out) return NIMCP_TENSOR_ERR_NULL;

    double dx = spacing ? spacing[0] : 1.0;
    double dy = spacing ? spacing[1] : 1.0;
    double dz = spacing ? spacing[2] : 1.0;

    /* curl_x = dFz/dy - dFy/dz */
    nimcp_tensor_t* dfz_dy = nimcp_tensor_gradient_dim(fz, 1, dy);
    nimcp_tensor_t* dfy_dz = nimcp_tensor_gradient_dim(fy, 2, dz);
    curl_out[0] = nimcp_tensor_sub(dfz_dy, dfy_dz);
    nimcp_tensor_destroy(dfz_dy);
    nimcp_tensor_destroy(dfy_dz);

    /* curl_y = dFx/dz - dFz/dx */
    nimcp_tensor_t* dfx_dz = nimcp_tensor_gradient_dim(fx, 2, dz);
    nimcp_tensor_t* dfz_dx = nimcp_tensor_gradient_dim(fz, 0, dx);
    curl_out[1] = nimcp_tensor_sub(dfx_dz, dfz_dx);
    nimcp_tensor_destroy(dfx_dz);
    nimcp_tensor_destroy(dfz_dx);

    /* curl_z = dFy/dx - dFx/dy */
    nimcp_tensor_t* dfy_dx = nimcp_tensor_gradient_dim(fy, 0, dx);
    nimcp_tensor_t* dfx_dy = nimcp_tensor_gradient_dim(fx, 1, dy);
    curl_out[2] = nimcp_tensor_sub(dfy_dx, dfx_dy);
    nimcp_tensor_destroy(dfy_dx);
    nimcp_tensor_destroy(dfx_dy);

    stats_update_op(&g_stats.ops_calculus);
    return NIMCP_TENSOR_OK;
}

nimcp_tensor_t* nimcp_tensor_laplacian(
    const nimcp_tensor_t* t,
    const double* spacing
)
{
    if (!tensor_is_valid(t)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_tensor_laplacian: tensor_is_valid is NULL");
        return NULL;
    }

    nimcp_tensor_t* lap = nimcp_tensor_zeros(t->shape.dims, t->shape.rank, t->dtype);
    if (!lap) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lap is NULL");

        return NULL;

    }

    /* Laplacian = sum_i (d²f/dx_i²) */
    for (uint32_t i = 0; i < t->shape.rank; i++) {
        double h = spacing ? spacing[i] : 1.0;

        /* Second derivative using central difference:
         * d²f/dx² ≈ (f(x+h) - 2f(x) + f(x-h)) / h² */
        nimcp_tensor_t* d2f = nimcp_tensor_create(t->shape.dims, t->shape.rank, t->dtype);
        if (!d2f) {
            nimcp_tensor_destroy(lap);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_tensor_laplacian: d2f is NULL");
            return NULL;
        }

        uint32_t indices[NIMCP_TENSOR_MAX_RANK] = {0};
        uint32_t n = t->shape.dims[i];

        for (size_t j = 0; j < t->shape.numel; j++) {
            flat_to_indices(j, &t->shape, indices);
            uint32_t pos = indices[i];

            double f0 = nimcp_tensor_get(t, indices);
            double fp, fm;

            if (pos == 0) {
                indices[i] = 1;
                fp = nimcp_tensor_get(t, indices);
                fm = f0;  /* Boundary condition */
                indices[i] = 0;
            } else if (pos == n - 1) {
                fp = f0;  /* Boundary condition */
                indices[i] = n - 2;
                fm = nimcp_tensor_get(t, indices);
                indices[i] = n - 1;
            } else {
                indices[i] = pos + 1;
                fp = nimcp_tensor_get(t, indices);
                indices[i] = pos - 1;
                fm = nimcp_tensor_get(t, indices);
                indices[i] = pos;
            }

            double d2 = (fp - 2.0 * f0 + fm) / (h * h);
            nimcp_tensor_set(d2f, indices, d2);
        }

        nimcp_tensor_add_(lap, d2f);
        nimcp_tensor_destroy(d2f);
    }

    stats_update_op(&g_stats.ops_calculus);
    return lap;
}

//=============================================================================
// In-place Operations (SIMD-optimized)
//=============================================================================

int nimcp_tensor_add_(nimcp_tensor_t* t, const nimcp_tensor_t* other)
{
    if (!tensor_is_valid(t) || !tensor_is_valid(other)) return NIMCP_TENSOR_ERR_NULL;
    if (!shapes_equal(&t->shape, &other->shape)) return NIMCP_TENSOR_ERR_SHAPE;

    // Use SIMD-optimized element-wise add
    float* td = (float*)t->data;
    float* od = (float*)other->data;
    tensor_simd_add_f32(td, od, t->shape.numel);

    return NIMCP_TENSOR_OK;
}

int nimcp_tensor_sub_(nimcp_tensor_t* t, const nimcp_tensor_t* other)
{
    if (!tensor_is_valid(t) || !tensor_is_valid(other)) return NIMCP_TENSOR_ERR_NULL;
    if (!shapes_equal(&t->shape, &other->shape)) return NIMCP_TENSOR_ERR_SHAPE;

    // Use SIMD-optimized element-wise subtract
    float* td = (float*)t->data;
    float* od = (float*)other->data;
    tensor_simd_sub_f32(td, od, t->shape.numel);

    return NIMCP_TENSOR_OK;
}

int nimcp_tensor_mul_(nimcp_tensor_t* t, const nimcp_tensor_t* other)
{
    if (!tensor_is_valid(t) || !tensor_is_valid(other)) return NIMCP_TENSOR_ERR_NULL;
    if (!shapes_equal(&t->shape, &other->shape)) return NIMCP_TENSOR_ERR_SHAPE;

    // Use SIMD-optimized element-wise multiply
    float* td = (float*)t->data;
    float* od = (float*)other->data;
    tensor_simd_mul_f32(td, od, t->shape.numel);

    return NIMCP_TENSOR_OK;
}

int nimcp_tensor_mul_scalar_(nimcp_tensor_t* t, double s)
{
    if (!tensor_is_valid(t)) return NIMCP_TENSOR_ERR_NULL;

    // Use SIMD-optimized scalar multiply
    float* td = (float*)t->data;
    tensor_simd_mul_scalar_f32(td, (float)s, t->shape.numel);

    return NIMCP_TENSOR_OK;
}

int nimcp_tensor_add_scalar_(nimcp_tensor_t* t, double s)
{
    if (!tensor_is_valid(t)) return NIMCP_TENSOR_ERR_NULL;

    // Use SIMD-optimized scalar add
    float* td = (float*)t->data;
    tensor_simd_add_scalar_f32(td, (float)s, t->shape.numel);

    return NIMCP_TENSOR_OK;
}

//=============================================================================
// Neural Network Operations
//=============================================================================

nimcp_tensor_t* nimcp_tensor_softmax(const nimcp_tensor_t* t, int dim)
{
    if (!tensor_is_valid(t)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_tensor_softmax: tensor_is_valid is NULL");
        return NULL;
    }

    if (dim < 0) dim = (int)t->shape.rank + dim;
    if (dim < 0 || dim >= (int)t->shape.rank) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_tensor_softmax: capacity exceeded");
        return NULL;
    }

    nimcp_tensor_t* result = nimcp_tensor_clone(t);
    if (!result) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "result is NULL");

        return NULL;

    }

    /* Compute softmax along dimension */
    /* For numerical stability: softmax(x) = softmax(x - max(x)) */

    float* data = (float*)result->data;

    if (t->shape.rank == 1) {
        /* 1D tensor: softmax over all elements */
        uint32_t n = t->shape.dims[0];

        /* Find max */
        float max_val = data[0];
        for (uint32_t j = 1; j < n; j++) {
            if (data[j] > max_val) max_val = data[j];
        }

        /* Compute exp and sum */
        float sum = 0.0F;
        for (uint32_t j = 0; j < n; j++) {
            data[j] = expf(data[j] - max_val);
            sum += data[j];
        }

        /* Normalize */
        for (uint32_t j = 0; j < n; j++) {
            data[j] /= sum;
        }
    } else if (dim == (int)t->shape.rank - 1 && t->shape.rank == 2) {
        /* 2D tensor: softmax along last dimension (rows) */
        uint32_t rows = t->shape.dims[0];
        uint32_t cols = t->shape.dims[1];

        for (uint32_t i = 0; i < rows; i++) {
            float* row = &data[i * cols];

            /* Find max */
            float max_val = row[0];
            for (uint32_t j = 1; j < cols; j++) {
                if (row[j] > max_val) max_val = row[j];
            }

            /* Compute exp and sum */
            float sum = 0.0F;
            for (uint32_t j = 0; j < cols; j++) {
                row[j] = expf(row[j] - max_val);
                sum += row[j];
            }

            /* Normalize */
            for (uint32_t j = 0; j < cols; j++) {
                row[j] /= sum;
            }
        }
    }

    return result;
}

nimcp_tensor_t* nimcp_tensor_attention(
    const nimcp_tensor_t* query,
    const nimcp_tensor_t* key,
    const nimcp_tensor_t* value,
    const nimcp_tensor_t* mask,
    double scale
)
{
    if (!tensor_is_valid(query) || !tensor_is_valid(key) || !tensor_is_valid(value)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_tensor_attention: required parameter is NULL (tensor_is_valid, tensor_is_valid, tensor_is_valid)");
        return NULL;
    }

    /* Q: (batch, heads, seq_q, d_k)
     * K: (batch, heads, seq_k, d_k)
     * V: (batch, heads, seq_k, d_v)
     * Output: (batch, heads, seq_q, d_v) */

    /* Compute Q @ K^T */
    nimcp_tensor_t* kt = nimcp_tensor_transpose(key);
    if (!kt) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kt is NULL");

        return NULL;

    }

    nimcp_tensor_t* scores = nimcp_tensor_matmul(query, kt);
    nimcp_tensor_destroy(kt);
    if (!scores) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "scores is NULL");

        return NULL;

    }

    /* Scale */
    if (scale == 0.0) {
        scale = 1.0 / sqrt((double)query->shape.dims[query->shape.rank - 1]);
    }
    nimcp_tensor_mul_scalar_(scores, scale);

    /* Apply mask if provided */
    if (mask && tensor_is_valid(mask)) {
        /* Add large negative value where mask is 0 */
        float* sd = (float*)scores->data;
        float* md = (float*)mask->data;
        for (size_t i = 0; i < scores->shape.numel; i++) {
            if (md[i] == 0.0F) {
                sd[i] = -1e9F;
            }
        }
    }

    /* Softmax */
    nimcp_tensor_t* attn_weights = nimcp_tensor_softmax(scores, -1);
    nimcp_tensor_destroy(scores);
    if (!attn_weights) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attn_weights is NULL");

        return NULL;

    }

    /* Multiply by V */
    nimcp_tensor_t* output = nimcp_tensor_matmul(attn_weights, value);
    nimcp_tensor_destroy(attn_weights);

    return output;
}

//=============================================================================
// Comparison Operations
//=============================================================================

bool nimcp_tensor_allclose(
    const nimcp_tensor_t* a,
    const nimcp_tensor_t* b,
    double rtol,
    double atol
)
{
    if (!tensor_is_valid(a) || !tensor_is_valid(b)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_tensor_allclose: required parameter is NULL (tensor_is_valid, tensor_is_valid)");
        return false;
    }
    if (!shapes_equal(&a->shape, &b->shape)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_tensor_allclose: shapes_equal is NULL");
        return false;
    }

    float* da = (float*)a->data;
    float* db = (float*)b->data;

    for (size_t i = 0; i < a->shape.numel; i++) {
        double diff = fabs(da[i] - db[i]);
        double tol = atol + rtol * fabs(db[i]);
        if (diff > tol) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_tensor_allclose: validation failed");
            return false;
        }
    }

    return true;
}

//=============================================================================
// Autodiff
//=============================================================================

nimcp_autodiff_ctx_t* nimcp_autodiff_create(void)
{
    nimcp_autodiff_ctx_t* ctx = nimcp_calloc(1, sizeof(nimcp_autodiff_ctx_t));
    if (!ctx) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ctx is NULL");

        return NULL;

    }

    /* Initialize mutex with error checking */
    if (nimcp_mutex_init(&ctx->lock, NULL) != 0) {
        NIMCP_LOGGING_ERROR("Failed to initialize autodiff context mutex");
        nimcp_free(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "nimcp_autodiff_create: validation failed");
        return NULL;
    }
    return ctx;
}

void nimcp_autodiff_destroy(nimcp_autodiff_ctx_t* ctx)
{
    if (!ctx) return;

    /* Free tape nodes */
    nimcp_autodiff_node_t* node = ctx->tape_head;
    while (node) {
        nimcp_autodiff_node_t* next = node->next;
        nimcp_free(node->inputs);
        nimcp_free(node);
        node = next;
    }

    nimcp_mutex_destroy(&ctx->lock);
    nimcp_free(ctx);
}

int nimcp_autodiff_start(nimcp_autodiff_ctx_t* ctx)
{
    if (!ctx) return NIMCP_TENSOR_ERR_NULL;
    ctx->recording = true;
    return NIMCP_TENSOR_OK;
}

int nimcp_autodiff_stop(nimcp_autodiff_ctx_t* ctx)
{
    if (!ctx) return NIMCP_TENSOR_ERR_NULL;
    ctx->recording = false;
    return NIMCP_TENSOR_OK;
}

nimcp_tensor_t* nimcp_tensor_grad(nimcp_tensor_t* t)
{
    if (!tensor_is_valid(t)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_tensor_grad: tensor_is_valid is NULL");
        return NULL;
    }
    return t->grad;
}

int nimcp_tensor_zero_grad(nimcp_tensor_t* t)
{
    if (!tensor_is_valid(t)) return NIMCP_TENSOR_ERR_NULL;

    if (t->grad) {
        memset(t->grad->data, 0, t->grad->shape.nbytes);
    }

    return NIMCP_TENSOR_OK;
}

//=============================================================================
// Statistics and Utilities
//=============================================================================

int nimcp_tensor_get_stats(nimcp_tensor_stats_t* stats)
{
    if (!stats) return NIMCP_TENSOR_ERR_NULL;

    nimcp_mutex_lock(&g_stats_lock);
    *stats = g_stats;
    nimcp_mutex_unlock(&g_stats_lock);

    return NIMCP_TENSOR_OK;
}

void nimcp_tensor_reset_stats(void)
{
    nimcp_mutex_lock(&g_stats_lock);
    memset(&g_stats, 0, sizeof(g_stats));
    nimcp_mutex_unlock(&g_stats_lock);
}

const char* nimcp_dtype_name(nimcp_dtype_t dtype)
{
    switch (dtype) {
        case NIMCP_DTYPE_F32:  return "float32";
        case NIMCP_DTYPE_F64:  return "float64";
        case NIMCP_DTYPE_F16:  return "float16";
        case NIMCP_DTYPE_BF16: return "bfloat16";
        case NIMCP_DTYPE_I32:  return "int32";
        case NIMCP_DTYPE_I64:  return "int64";
        case NIMCP_DTYPE_I8:   return "int8";
        case NIMCP_DTYPE_U8:   return "uint8";
        case NIMCP_DTYPE_BOOL: return "bool";
        case NIMCP_DTYPE_C64:  return "complex64";
        case NIMCP_DTYPE_C128: return "complex128";
        default:              return "unknown";
    }
}

size_t nimcp_dtype_size(nimcp_dtype_t dtype)
{
    switch (dtype) {
        case NIMCP_DTYPE_F32:  return 4;
        case NIMCP_DTYPE_F64:  return 8;
        case NIMCP_DTYPE_F16:  return 2;
        case NIMCP_DTYPE_BF16: return 2;
        case NIMCP_DTYPE_I32:  return 4;
        case NIMCP_DTYPE_I64:  return 8;
        case NIMCP_DTYPE_I8:   return 1;
        case NIMCP_DTYPE_U8:   return 1;
        case NIMCP_DTYPE_BOOL: return 1;
        case NIMCP_DTYPE_C64:  return 8;
        case NIMCP_DTYPE_C128: return 16;
        default:              return 0;
    }
}

void nimcp_tensor_print_info(const nimcp_tensor_t* t, const char* name)
{
    if (!tensor_is_valid(t)) {
        printf("%s: <invalid tensor>\n", name ? name : "tensor");
        return;
    }

    printf("%s: shape=(", name ? name : "tensor");
    for (uint32_t i = 0; i < t->shape.rank; i++) {
        printf("%u%s", t->shape.dims[i], i < t->shape.rank - 1 ? ", " : "");
    }
    printf("), dtype=%s, numel=%zu, nbytes=%zu\n",
           nimcp_dtype_name(t->dtype), t->shape.numel, t->shape.nbytes);
}

void nimcp_tensor_print_data(const nimcp_tensor_t* t, uint32_t max_elements)
{
    if (!tensor_is_valid(t)) return;

    float* data = (float*)t->data;
    size_t n = (t->shape.numel < max_elements) ? t->shape.numel : max_elements;

    printf("[");
    for (size_t i = 0; i < n; i++) {
        printf("%.4f%s", data[i], i < n - 1 ? ", " : "");
    }
    if (n < t->shape.numel) {
        printf(", ... (%zu more)", t->shape.numel - n);
    }
    printf("]\n");
}

const char* nimcp_tensor_error_string(nimcp_tensor_error_t err)
{
    switch (err) {
        case NIMCP_TENSOR_OK:            return "Success";
        case NIMCP_TENSOR_ERR_NULL:      return "Null pointer";
        case NIMCP_TENSOR_ERR_SHAPE:     return "Shape mismatch";
        case NIMCP_TENSOR_ERR_RANK:      return "Invalid rank";
        case NIMCP_TENSOR_ERR_ALLOC:     return "Allocation failed";
        case NIMCP_TENSOR_ERR_BROADCAST: return "Broadcast failed";
        case NIMCP_TENSOR_ERR_EINSUM:    return "Invalid einsum";
        case NIMCP_TENSOR_ERR_DTYPE:     return "Invalid dtype";
        case NIMCP_TENSOR_ERR_CONTIGUOUS:return "Not contiguous";
        case NIMCP_TENSOR_ERR_INDEX:     return "Index out of bounds";
        case NIMCP_TENSOR_ERR_GRAD:      return "Gradient error";
        case NIMCP_TENSOR_ERR_INVALID:   return "Invalid tensor";
        default:                         return "Unknown error";
    }
}

//=============================================================================
// Stub Implementations for Declared but Unimplemented Functions
//=============================================================================
// NOTE: These are placeholder implementations to allow linking.
//       Full implementations will be added in future phases.

nimcp_tensor_t* nimcp_tensor_square(const nimcp_tensor_t* t)
{
    if (!t) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "t is NULL");

        return NULL;

    }
    // Square: element-wise t * t
    return nimcp_tensor_mul(t, t);
}

nimcp_tensor_t* nimcp_tensor_layer_norm(const nimcp_tensor_t* t,
                                         const nimcp_tensor_t* gamma,
                                         const nimcp_tensor_t* beta,
                                         double eps)
{
    if (!t) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "t is NULL");
        return NULL;
    }

    if (t->dtype != NIMCP_DTYPE_F32) {
        LOG_WARN("nimcp_tensor_layer_norm: only F32 supported, cloning input");
        return nimcp_tensor_clone(t);
    }

    size_t numel = t->shape.numel;
    if (numel == 0) {
        return nimcp_tensor_clone(t);
    }

    /* Normalize over the last dimension */
    uint32_t norm_dim = t->shape.dims[t->shape.rank - 1];
    size_t num_vectors = numel / norm_dim;

    /* Create output tensor with same shape */
    nimcp_tensor_t* out = nimcp_tensor_clone(t);
    if (!out) return NULL;

    float* out_data = (float*)out->data;
    const float* in_data = (const float*)t->data;

    /* Optional gamma/beta data pointers */
    const float* gamma_data = (gamma && gamma->dtype == NIMCP_DTYPE_F32) ?
                              (const float*)gamma->data : NULL;
    const float* beta_data = (beta && beta->dtype == NIMCP_DTYPE_F32) ?
                             (const float*)beta->data : NULL;

    float epsilon = (eps > 0.0) ? (float)eps : 1e-5f;

    for (size_t v = 0; v < num_vectors; v++) {
        size_t offset = v * norm_dim;

        /* Compute mean */
        float sum = 0.0f;
        for (uint32_t i = 0; i < norm_dim; i++) {
            sum += in_data[offset + i];
        }
        float mean = sum / (float)norm_dim;

        /* Compute variance */
        float var_sum = 0.0f;
        for (uint32_t i = 0; i < norm_dim; i++) {
            float diff = in_data[offset + i] - mean;
            var_sum += diff * diff;
        }
        float inv_std = 1.0f / sqrtf(var_sum / (float)norm_dim + epsilon);

        /* Normalize and apply affine transform */
        for (uint32_t i = 0; i < norm_dim; i++) {
            float normalized = (in_data[offset + i] - mean) * inv_std;
            if (gamma_data) normalized *= gamma_data[i];
            if (beta_data) normalized += beta_data[i];
            out_data[offset + i] = normalized;
        }
    }

    return out;
}

nimcp_tensor_t* nimcp_tensor_log_softmax(const nimcp_tensor_t* input, int dim)
{
    if (!input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_tensor_log_softmax: input is NULL");
        return NULL;
    }

    /* Compute softmax first, then take log */
    nimcp_tensor_t* sm = nimcp_tensor_softmax(input, dim);
    if (!sm) {
        return NULL;
    }

    /* Compute element-wise log */
    size_t total = 1;
    for (uint32_t i = 0; i < sm->shape.rank; i++) {
        total *= sm->shape.dims[i];
    }

    float* data = (float*)sm->data;
    for (size_t i = 0; i < total; i++) {
        /* Clamp to avoid log(0) = -inf */
        float val = data[i];
        if (val < 1e-38f) val = 1e-38f;
        data[i] = logf(val);
    }

    return sm;
}

int nimcp_autodiff_backward(nimcp_autodiff_ctx_t* ctx,
                            nimcp_tensor_t* output,
                            nimcp_tensor_t* const* inputs,
                            uint32_t num_inputs,
                            nimcp_tensor_t** gradients)
{
    if (!ctx || !output || !inputs || !gradients) return NIMCP_TENSOR_ERR_NULL;
    if (num_inputs == 0) return NIMCP_TENSOR_ERR_SHAPE;

    /* Initialize output gradient to 1.0 (scalar loss assumed) */
    if (!output->grad) {
        output->grad = nimcp_tensor_ones(output->shape.dims, output->shape.rank, output->dtype);
        if (!output->grad) return NIMCP_TENSOR_ERR_ALLOC;
    }

    /* Walk tape in reverse order (backpropagation) */
    nimcp_autodiff_node_t* node = ctx->tape_tail;
    while (node) {
        if (node->backward_fn && node->output && node->output->grad) {
            node->backward_fn(node, node->output->grad);
        }
        /* Move backwards — since we only have a forward 'next' pointer,
         * we need to find the predecessor by walking from head.
         * This is O(n^2) but correct for the tape-based approach. */
        if (node == ctx->tape_head) break;
        nimcp_autodiff_node_t* prev = NULL;
        nimcp_autodiff_node_t* curr = ctx->tape_head;
        while (curr && curr->next != node) {
            curr = curr->next;
        }
        prev = curr;
        node = prev;
    }

    /* Copy accumulated gradients from input tensors to output gradient array */
    for (uint32_t i = 0; i < num_inputs; i++) {
        if (inputs[i] && inputs[i]->grad) {
            gradients[i] = inputs[i]->grad;
        } else {
            /* No gradient accumulated — return zeros */
            gradients[i] = nimcp_tensor_zeros(inputs[i]->shape.dims,
                                              inputs[i]->shape.rank,
                                              inputs[i]->dtype);
        }
    }

    return NIMCP_TENSOR_OK;
}

//=============================================================================
// Persistence (save/load)
//=============================================================================

#include <stdio.h>

#define NIMCP_TENSOR_FILE_MAGIC 0x544E5352  /* "TNSR" */
#define NIMCP_TENSOR_NULL_MAGIC 0x544E4C4C  /* "TNLL" — null tensor marker */

int nimcp_tensor_save(const nimcp_tensor_t* tensor, FILE* file) {
    if (!file) return -1;

    /* Handle NULL tensor — write a null marker so load knows to return NULL */
    if (!tensor) {
        uint32_t null_magic = NIMCP_TENSOR_NULL_MAGIC;
        if (fwrite(&null_magic, sizeof(uint32_t), 1, file) != 1) return -1;
        return 0;
    }

    /* Header: magic + rank + dtype */
    uint32_t magic = NIMCP_TENSOR_FILE_MAGIC;
    uint32_t rank = tensor->shape.rank;
    uint32_t dtype = (uint32_t)tensor->dtype;

    if (fwrite(&magic, sizeof(uint32_t), 1, file) != 1) return -1;
    if (fwrite(&rank, sizeof(uint32_t), 1, file) != 1) return -1;
    if (fwrite(&dtype, sizeof(uint32_t), 1, file) != 1) return -1;

    /* Dimensions */
    if (rank > 0) {
        if (fwrite(tensor->shape.dims, sizeof(uint32_t), rank, file) != rank) return -1;
    }

    /* Data (use uint64_t for cross-platform portability) */
    uint64_t nbytes_u64 = (uint64_t)tensor->shape.nbytes;
    if (fwrite(&nbytes_u64, sizeof(uint64_t), 1, file) != 1) return -1;
    size_t nbytes = tensor->shape.nbytes;
    if (nbytes > 0 && tensor->data) {
        if (fwrite(tensor->data, 1, nbytes, file) != nbytes) return -1;
    }

    return 0;
}

nimcp_tensor_t* nimcp_tensor_load(FILE* file) {
    if (!file) return NULL;

    /* Read magic */
    uint32_t magic = 0;
    if (fread(&magic, sizeof(uint32_t), 1, file) != 1) return NULL;

    /* Check for null marker */
    if (magic == NIMCP_TENSOR_NULL_MAGIC) return NULL;

    if (magic != NIMCP_TENSOR_FILE_MAGIC) {
        NIMCP_LOGGING_ERROR("nimcp_tensor_load: invalid magic 0x%08X (expected 0x%08X)",
                           magic, NIMCP_TENSOR_FILE_MAGIC);
        return NULL;
    }

    /* Read rank + dtype */
    uint32_t rank = 0, dtype_u32 = 0;
    if (fread(&rank, sizeof(uint32_t), 1, file) != 1) return NULL;
    if (fread(&dtype_u32, sizeof(uint32_t), 1, file) != 1) return NULL;

    if (rank > NIMCP_TENSOR_MAX_RANK) {
        NIMCP_LOGGING_ERROR("nimcp_tensor_load: rank %u exceeds max %d", rank, NIMCP_TENSOR_MAX_RANK);
        return NULL;
    }
    if (dtype_u32 >= NIMCP_DTYPE_COUNT) {
        NIMCP_LOGGING_ERROR("nimcp_tensor_load: invalid dtype %u", dtype_u32);
        return NULL;
    }

    /* Read dimensions */
    uint32_t dims[NIMCP_TENSOR_MAX_RANK] = {0};
    if (rank > 0) {
        if (fread(dims, sizeof(uint32_t), rank, file) != rank) return NULL;
    }

    /* Read data size (uint64_t for cross-platform portability) */
    uint64_t nbytes_u64 = 0;
    if (fread(&nbytes_u64, sizeof(uint64_t), 1, file) != 1) return NULL;
    size_t nbytes = (size_t)nbytes_u64;

    /* Create tensor */
    nimcp_dtype_t dtype = (nimcp_dtype_t)dtype_u32;
    nimcp_tensor_t* t = nimcp_tensor_create(dims, rank, dtype);
    if (!t) return NULL;

    /* Validate data size matches */
    if (nbytes != t->shape.nbytes) {
        NIMCP_LOGGING_ERROR("nimcp_tensor_load: data size mismatch (file=%zu, tensor=%zu)",
                           nbytes, t->shape.nbytes);
        nimcp_tensor_destroy(t);
        return NULL;
    }

    /* Read data */
    if (nbytes > 0 && t->data) {
        if (fread(t->data, 1, nbytes, file) != nbytes) {
            NIMCP_LOGGING_ERROR("nimcp_tensor_load: short read on data (%zu bytes)", nbytes);
            nimcp_tensor_destroy(t);
            return NULL;
        }
    }

    return t;
}
