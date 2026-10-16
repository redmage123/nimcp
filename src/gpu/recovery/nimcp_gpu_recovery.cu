/**
 * @file nimcp_gpu_recovery.cu
 * @brief GPU Self-Healing Recovery Implementation
 * @version 1.0.0
 * @date 2025-01-30
 *
 * WHAT: Implementation of GPU self-healing recovery system
 * WHY:  Enable automatic recovery from GPU errors without system halt
 * HOW:  Recovery strategies, parameter correction, CPU fallback, memory management
 */

#include "utils/memory/nimcp_memory.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/logging/nimcp_logging.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef NIMCP_ENABLE_CUDA
#include <cuda_runtime.h>
#endif

/* ============================================================================
 * Global State
 * ============================================================================ */

static bool g_recovery_initialized = false;
static nimcp_gpu_recovery_config_t g_default_config;
static nimcp_gpu_recovery_stats_t g_stats = {0};

/* Thread-local recovery context for operations without explicit context */
static __thread nimcp_gpu_recovery_context_t* tl_default_context = NULL;

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static void log_recovery_action(const char* action, const char* details)
{
    fprintf(stderr, "[GPU RECOVERY] %s: %s\n", action, details);
}

static uint64_t get_timestamp_ms(void)
{
#ifdef NIMCP_ENABLE_CUDA
    cudaEvent_t event;
    cudaEventCreate(&event);
    cudaEventRecord(event);
    cudaEventSynchronize(event);
    cudaEventDestroy(event);
#endif
    return 0; /* Simplified - in production use proper timing */
}

/* ============================================================================
 * Configuration API
 * ============================================================================ */

void nimcp_gpu_recovery_default_config(nimcp_gpu_recovery_config_t* config)
{
    if (!config) return;

    config->enable_cpu_fallback = true;
    config->enable_param_correction = true;
    config->enable_batch_reduction = true;
    config->enable_retry = true;
    config->max_retries = 3;
    config->retry_delay_ms = 10;
    config->batch_reduction_factor = 0.5f;
    config->memory_threshold = 0.9f;
}

int nimcp_gpu_recovery_init(const nimcp_gpu_recovery_config_t* config)
{
    if (g_recovery_initialized) {
        return 0;  /* Already initialized */
    }

    if (config) {
        g_default_config = *config;
    } else {
        nimcp_gpu_recovery_default_config(&g_default_config);
    }

    memset(&g_stats, 0, sizeof(g_stats));
    g_recovery_initialized = true;

    log_recovery_action("INIT", "GPU recovery system initialized");
    return 0;
}

void nimcp_gpu_recovery_shutdown(void)
{
    if (!g_recovery_initialized) return;

    if (tl_default_context) {
        nimcp_gpu_recovery_context_destroy(tl_default_context);
        tl_default_context = NULL;
    }

    g_recovery_initialized = false;
    log_recovery_action("SHUTDOWN", "GPU recovery system shutdown");
}

bool nimcp_gpu_recovery_is_initialized(void)
{
    return g_recovery_initialized;
}

/* ============================================================================
 * Context Management
 * ============================================================================ */

nimcp_gpu_recovery_context_t* nimcp_gpu_recovery_context_create(
    const nimcp_gpu_recovery_config_t* config)
{
    nimcp_gpu_recovery_context_t* ctx = (nimcp_gpu_recovery_context_t*)
        nimcp_calloc(1, sizeof(nimcp_gpu_recovery_context_t));

    if (!ctx) return NULL;

    if (config) {
        ctx->config = *config;
    } else if (g_recovery_initialized) {
        ctx->config = g_default_config;
    } else {
        nimcp_gpu_recovery_default_config(&ctx->config);
    }

    return ctx;
}

void nimcp_gpu_recovery_context_destroy(nimcp_gpu_recovery_context_t* ctx)
{
    if (!ctx) return;
    nimcp_free(ctx);
}

void nimcp_gpu_recovery_context_reset(nimcp_gpu_recovery_context_t* ctx)
{
    if (!ctx) return;

    ctx->retry_count = 0;
    ctx->batch_reductions = 0;
    ctx->cpu_fallback_active = false;
    ctx->last_error_category = GPU_ERROR_UNKNOWN;
    ctx->last_cuda_error = cudaSuccess;
    ctx->last_error_message[0] = '\0';
}

static nimcp_gpu_recovery_context_t* get_or_create_default_context(void)
{
    if (!tl_default_context) {
        tl_default_context = nimcp_gpu_recovery_context_create(NULL);
    }
    return tl_default_context;
}

/* ============================================================================
 * Error Categorization
 * ============================================================================ */

nimcp_gpu_error_category_t nimcp_gpu_categorize_cuda_error(cudaError_t err)
{
    switch (err) {
        case cudaSuccess:
            return GPU_ERROR_UNKNOWN;

        case cudaErrorMemoryAllocation:
        case cudaErrorLaunchOutOfResources:
            return GPU_ERROR_OUT_OF_MEMORY;

        case cudaErrorInvalidValue:
        case cudaErrorInvalidDevicePointer:
        case cudaErrorInvalidMemcpyDirection:
            return GPU_ERROR_INVALID_PARAMS;

        case cudaErrorLaunchFailure:
        case cudaErrorLaunchTimeout:
        case cudaErrorInvalidConfiguration:
            return GPU_ERROR_KERNEL_LAUNCH;

        case cudaErrorNoDevice:
        case cudaErrorInvalidDevice:
        case cudaErrorDeviceUninitialized:
            return GPU_ERROR_DEVICE_NOT_AVAILABLE;

        case cudaErrorCudartUnloading:
        case cudaErrorInitializationError:
        case cudaErrorInsufficientDriver:
            return GPU_ERROR_CUDA_RUNTIME;

        default:
            return GPU_ERROR_CUDA_RUNTIME;
    }
}

const char* nimcp_gpu_error_category_name(nimcp_gpu_error_category_t category)
{
    switch (category) {
        case GPU_ERROR_UNKNOWN:           return "UNKNOWN";
        case GPU_ERROR_INVALID_PARAMS:    return "INVALID_PARAMS";
        case GPU_ERROR_OUT_OF_MEMORY:     return "OUT_OF_MEMORY";
        case GPU_ERROR_CUDA_RUNTIME:      return "CUDA_RUNTIME";
        case GPU_ERROR_KERNEL_LAUNCH:     return "KERNEL_LAUNCH";
        case GPU_ERROR_NUMERICAL:         return "NUMERICAL";
        case GPU_ERROR_TIMEOUT:           return "TIMEOUT";
        case GPU_ERROR_CONTEXT_INVALID:   return "CONTEXT_INVALID";
        case GPU_ERROR_LIBRARY:           return "LIBRARY";
        case GPU_ERROR_DEVICE_NOT_AVAILABLE: return "DEVICE_NOT_AVAILABLE";
        default:                          return "UNKNOWN";
    }
}

void nimcp_gpu_recovery_report_error(
    nimcp_gpu_error_category_t error_category,
    cudaError_t cuda_error,
    const char* file,
    int line)
{
    g_stats.total_errors++;

    const char* category_name = nimcp_gpu_error_category_name(error_category);
#ifdef NIMCP_ENABLE_CUDA
    const char* cuda_error_str = cudaGetErrorString(cuda_error);
#else
    const char* cuda_error_str = "CUDA disabled";
    (void)cuda_error;
#endif

    fprintf(stderr, "[GPU ERROR] %s:%d - Category: %s, CUDA: %s\n",
            file ? file : "unknown", line, category_name, cuda_error_str);
}

const char* nimcp_gpu_recovery_action_name(nimcp_gpu_recovery_action_t action)
{
    switch (action) {
        case GPU_RECOVERY_NONE:             return "NONE";
        case GPU_RECOVERY_CLAMP_PARAMS:     return "CLAMP_PARAMS";
        case GPU_RECOVERY_SET_DEFAULTS:     return "SET_DEFAULTS";
        case GPU_RECOVERY_VALIDATE_FIX:     return "VALIDATE_FIX";
        case GPU_RECOVERY_REDUCE_BATCH:     return "REDUCE_BATCH";
        case GPU_RECOVERY_REDUCE_DIMENSIONS: return "REDUCE_DIMENSIONS";
        case GPU_RECOVERY_REDUCE_PRECISION: return "REDUCE_PRECISION";
        case GPU_RECOVERY_FREE_CACHE:       return "FREE_CACHE";
        case GPU_RECOVERY_RESET_DEVICE:     return "RESET_DEVICE";
        case GPU_RECOVERY_CPU_FALLBACK:     return "CPU_FALLBACK";
        case GPU_RECOVERY_ASYNC_SPLIT:      return "ASYNC_SPLIT";
        case GPU_RECOVERY_STREAM_SYNC:      return "STREAM_SYNC";
        case GPU_RECOVERY_RETRY_IMMEDIATE:  return "RETRY_IMMEDIATE";
        case GPU_RECOVERY_RETRY_BACKOFF:    return "RETRY_BACKOFF";
        case GPU_RECOVERY_RETRY_REDUCED:    return "RETRY_REDUCED";
        default:                            return "UNKNOWN";
    }
}

/* ============================================================================
 * Strategy Selection
 * ============================================================================ */

nimcp_gpu_recovery_action_t nimcp_gpu_select_recovery_strategy(
    nimcp_gpu_error_category_t error_category,
    cudaError_t cuda_error,
    uint32_t retry_count)
{
    /* Progressive strategy based on retry count and error type */

    switch (error_category) {
        case GPU_ERROR_INVALID_PARAMS:
            /* Try parameter correction first, then defaults */
            if (retry_count == 0) return GPU_RECOVERY_CLAMP_PARAMS;
            if (retry_count == 1) return GPU_RECOVERY_SET_DEFAULTS;
            return GPU_RECOVERY_CPU_FALLBACK;

        case GPU_ERROR_OUT_OF_MEMORY:
            /* Progressive memory recovery */
            if (retry_count == 0) return GPU_RECOVERY_FREE_CACHE;
            if (retry_count == 1) return GPU_RECOVERY_REDUCE_BATCH;
            if (retry_count == 2) return GPU_RECOVERY_REDUCE_DIMENSIONS;
            return GPU_RECOVERY_CPU_FALLBACK;

        case GPU_ERROR_KERNEL_LAUNCH:
            /* Try reducing workload, then CPU */
            if (retry_count == 0) return GPU_RECOVERY_STREAM_SYNC;
            if (retry_count == 1) return GPU_RECOVERY_REDUCE_BATCH;
            return GPU_RECOVERY_CPU_FALLBACK;

        case GPU_ERROR_CUDA_RUNTIME:
            /* Try reset, then fallback */
            if (retry_count == 0) return GPU_RECOVERY_RESET_DEVICE;
            if (retry_count == 1) return GPU_RECOVERY_RETRY_BACKOFF;
            return GPU_RECOVERY_CPU_FALLBACK;

        case GPU_ERROR_NUMERICAL:
            /* Numerical stability adjustments */
            if (retry_count == 0) return GPU_RECOVERY_REDUCE_PRECISION;
            if (retry_count == 1) return GPU_RECOVERY_CLAMP_PARAMS;
            return GPU_RECOVERY_CPU_FALLBACK;

        case GPU_ERROR_TIMEOUT:
            /* Split work or go async */
            if (retry_count == 0) return GPU_RECOVERY_ASYNC_SPLIT;
            if (retry_count == 1) return GPU_RECOVERY_REDUCE_BATCH;
            return GPU_RECOVERY_CPU_FALLBACK;

        case GPU_ERROR_DEVICE_NOT_AVAILABLE:
            /* Immediate CPU fallback */
            return GPU_RECOVERY_CPU_FALLBACK;

        case GPU_ERROR_CONTEXT_INVALID:
            /* Try reset, then fallback */
            if (retry_count == 0) return GPU_RECOVERY_RESET_DEVICE;
            return GPU_RECOVERY_CPU_FALLBACK;

        case GPU_ERROR_LIBRARY:
            /* cuBLAS/cuSOLVER errors - retry then fallback */
            if (retry_count == 0) return GPU_RECOVERY_RETRY_IMMEDIATE;
            if (retry_count == 1) return GPU_RECOVERY_RESET_DEVICE;
            return GPU_RECOVERY_CPU_FALLBACK;

        default:
            /* Unknown - try retry then fallback */
            if (retry_count < 2) return GPU_RECOVERY_RETRY_BACKOFF;
            return GPU_RECOVERY_CPU_FALLBACK;
    }
}

/* ============================================================================
 * Recovery Action Execution
 * ============================================================================ */

bool nimcp_gpu_execute_recovery_action(
    nimcp_gpu_recovery_context_t* ctx,
    nimcp_gpu_recovery_action_t action)
{
    if (!ctx) ctx = get_or_create_default_context();
    if (!ctx) return false;

    char details[128];
    snprintf(details, sizeof(details), "Executing recovery action: %s",
             nimcp_gpu_recovery_action_name(action));
    log_recovery_action("ACTION", details);

    switch (action) {
        case GPU_RECOVERY_NONE:
            return true;

        case GPU_RECOVERY_CLAMP_PARAMS:
        case GPU_RECOVERY_SET_DEFAULTS:
        case GPU_RECOVERY_VALIDATE_FIX:
            /* These are handled at the call site with parameter correction */
            g_stats.param_corrections_applied++;
            return true;

        case GPU_RECOVERY_REDUCE_BATCH:
            ctx->batch_reductions++;
            g_stats.batch_reductions_applied++;
            return true;

        case GPU_RECOVERY_REDUCE_DIMENSIONS:
            /* Flag for caller to handle */
            return true;

        case GPU_RECOVERY_REDUCE_PRECISION:
            /* Flag for caller to handle precision reduction */
            return true;

        case GPU_RECOVERY_FREE_CACHE:
            {
                size_t freed = nimcp_gpu_free_caches();
                g_stats.memory_freed_bytes += freed;
                snprintf(details, sizeof(details), "Freed %zu bytes from GPU caches", freed);
                log_recovery_action("MEMORY", details);
                return freed > 0;
            }

        case GPU_RECOVERY_RESET_DEVICE:
#ifdef NIMCP_ENABLE_CUDA
            {
                cudaError_t err = cudaDeviceReset();
                if (err == cudaSuccess) {
                    log_recovery_action("DEVICE", "GPU device reset successful");
                    return true;
                }
                snprintf(details, sizeof(details), "Device reset failed: %s",
                         cudaGetErrorString(err));
                log_recovery_action("DEVICE", details);
                return false;
            }
#else
            return false;
#endif

        case GPU_RECOVERY_CPU_FALLBACK:
            if (ctx->config.enable_cpu_fallback && ctx->cpu_fallback_fn) {
                ctx->cpu_fallback_active = true;
                g_stats.cpu_fallbacks_used++;
                log_recovery_action("FALLBACK", "Switching to CPU execution");
                return true;
            }
            log_recovery_action("FALLBACK", "CPU fallback not available");
            return false;

        case GPU_RECOVERY_ASYNC_SPLIT:
        case GPU_RECOVERY_STREAM_SYNC:
            /* Flag for caller to handle execution mode */
            return true;

        case GPU_RECOVERY_RETRY_IMMEDIATE:
            ctx->retry_count++;
            return true;

        case GPU_RECOVERY_RETRY_BACKOFF:
            ctx->retry_count++;
            /* Simple backoff delay */
#ifdef _WIN32
            Sleep(ctx->config.retry_delay_ms * (1 << ctx->retry_count));
#else
            usleep(ctx->config.retry_delay_ms * 1000 * (1 << ctx->retry_count));
#endif
            return true;

        case GPU_RECOVERY_RETRY_REDUCED:
            ctx->retry_count++;
            ctx->batch_reductions++;
            return true;

        default:
            return false;
    }
}

/* ============================================================================
 * Main Recovery Function
 * ============================================================================ */

bool nimcp_gpu_try_recover(
    nimcp_gpu_recovery_context_t* ctx,
    nimcp_gpu_error_category_t error_category,
    cudaError_t cuda_error,
    nimcp_gpu_recovery_result_t* result)
{
    if (!ctx) ctx = get_or_create_default_context();
    if (!ctx) {
        if (result) {
            result->success = false;
            snprintf(result->message, sizeof(result->message), "No recovery context");
        }
        return false;
    }

    /* Initialize if needed */
    if (!g_recovery_initialized) {
        nimcp_gpu_recovery_init(NULL);
    }

    g_stats.total_errors++;
    g_stats.recoveries_attempted++;

    /* Store error info */
    ctx->last_error_category = error_category;
    ctx->last_cuda_error = cuda_error;

#ifdef NIMCP_ENABLE_CUDA
    if (cuda_error != cudaSuccess) {
        snprintf(ctx->last_error_message, sizeof(ctx->last_error_message),
                 "%s", cudaGetErrorString(cuda_error));
    }
#endif

    /* Log the error */
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Error: %s (CUDA: %s, retry: %u)",
             nimcp_gpu_error_category_name(error_category),
             ctx->last_error_message,
             ctx->retry_count);
    log_recovery_action("ERROR", log_msg);

    /* Check retry limit */
    if (ctx->retry_count >= ctx->config.max_retries) {
        /* Try CPU fallback as last resort */
        if (ctx->config.enable_cpu_fallback && ctx->cpu_fallback_fn) {
            if (nimcp_gpu_execute_recovery_action(ctx, GPU_RECOVERY_CPU_FALLBACK)) {
                if (result) {
                    result->success = true;
                    result->action_taken = GPU_RECOVERY_CPU_FALLBACK;
                    result->using_fallback = true;
                    result->retries_used = ctx->retry_count;
                    snprintf(result->message, sizeof(result->message),
                             "Switched to CPU fallback after %u retries", ctx->retry_count);
                }
                g_stats.recoveries_succeeded++;
                return true;
            }
        }

        log_recovery_action("FAILED", "Max retries exceeded, no recovery available");
        if (result) {
            result->success = false;
            result->retries_used = ctx->retry_count;
            snprintf(result->message, sizeof(result->message),
                     "Recovery failed: max retries exceeded");
        }
        return false;
    }

    /* Select recovery strategy */
    nimcp_gpu_recovery_action_t action = nimcp_gpu_select_recovery_strategy(
        error_category, cuda_error, ctx->retry_count);

    /* Execute recovery action */
    bool action_success = nimcp_gpu_execute_recovery_action(ctx, action);

    if (result) {
        result->success = action_success;
        result->action_taken = action;
        result->retries_used = ctx->retry_count;
        result->using_fallback = ctx->cpu_fallback_active;
        result->adjusted_batch_factor = powf(ctx->config.batch_reduction_factor,
                                             (float)ctx->batch_reductions);
        snprintf(result->message, sizeof(result->message),
                 "Recovery %s: %s (retry %u)",
                 action_success ? "succeeded" : "failed",
                 nimcp_gpu_recovery_action_name(action),
                 ctx->retry_count);
    }

    if (action_success) {
        g_stats.recoveries_succeeded++;
        ctx->recoveries_succeeded++;
    }
    ctx->recoveries_attempted++;

    return action_success;
}

/* ============================================================================
 * Parameter Correction
 * ============================================================================ */

void nimcp_gpu_default_param_constraints(nimcp_gpu_param_constraints_t* constraints)
{
    if (!constraints) return;

    constraints->batch_size = (nimcp_gpu_param_range_t){1, 65536, 64, true};
    constraints->learning_rate = (nimcp_gpu_param_range_t){1e-8f, 10.0f, 0.01f, true};
    constraints->num_inputs = (nimcp_gpu_param_range_t){1, 65536, 2, true};
    constraints->num_outputs = (nimcp_gpu_param_range_t){1, 65536, 1, true};
    constraints->num_iterations = (nimcp_gpu_param_range_t){1, 1000000, 100, true};
    constraints->tolerance = (nimcp_gpu_param_range_t){1e-12f, 1.0f, 1e-6f, true};
    constraints->max_memory_bytes = 0;  /* 0 = auto-detect */
    constraints->require_power_of_two_batch = false;
}

bool nimcp_gpu_correct_param_float(
    float* value,
    const nimcp_gpu_param_range_t* range,
    const char* param_name)
{
    if (!value || !range) return false;

    float original = *value;

    /* Check for NaN/Inf */
    if (isnan(*value) || isinf(*value)) {
        *value = range->default_value;
        fprintf(stderr, "[GPU RECOVERY] Corrected %s: NaN/Inf -> %.6f\n",
                param_name, *value);
        return true;
    }

    /* Check range */
    if (*value < range->min_value || *value > range->max_value) {
        if (range->clamp_to_range) {
            if (*value < range->min_value) *value = range->min_value;
            if (*value > range->max_value) *value = range->max_value;
        } else {
            *value = range->default_value;
        }
        fprintf(stderr, "[GPU RECOVERY] Corrected %s: %.6f -> %.6f (range: [%.6f, %.6f])\n",
                param_name, original, *value, range->min_value, range->max_value);
        return true;
    }

    return false;
}

bool nimcp_gpu_correct_param_int(
    int* value,
    int min_val,
    int max_val,
    int default_val,
    const char* param_name)
{
    if (!value) return false;

    int original = *value;

    if (*value < min_val) {
        *value = min_val;
    } else if (*value > max_val) {
        *value = max_val;
    } else {
        return false;  /* No correction needed */
    }

    fprintf(stderr, "[GPU RECOVERY] Corrected %s: %d -> %d (range: [%d, %d])\n",
            param_name, original, *value, min_val, max_val);
    return true;
}

bool nimcp_gpu_correct_param_size(
    size_t* value,
    size_t min_val,
    size_t max_val,
    size_t default_val,
    const char* param_name)
{
    if (!value) return false;

    size_t original = *value;

    if (*value < min_val) {
        *value = min_val;
    } else if (*value > max_val) {
        *value = max_val;
    } else if (*value == 0 && min_val > 0) {
        *value = default_val;
    } else {
        return false;
    }

    fprintf(stderr, "[GPU RECOVERY] Corrected %s: %zu -> %zu (range: [%zu, %zu])\n",
            param_name, original, *value, min_val, max_val);
    return true;
}

bool nimcp_gpu_correct_batch_for_memory(
    size_t* batch_size,
    size_t element_size,
    size_t memory_per_element)
{
    if (!batch_size || *batch_size == 0) return false;

#ifdef NIMCP_ENABLE_CUDA
    size_t free_mem, total_mem;
    if (!nimcp_gpu_get_memory_info(&free_mem, &total_mem)) {
        return false;
    }

    /* Calculate required memory */
    size_t required = (*batch_size) * (element_size + memory_per_element);

    /* Leave 20% buffer */
    size_t available = (size_t)(free_mem * 0.8);

    if (required > available) {
        /* Calculate max batch that fits */
        size_t per_element_total = element_size + memory_per_element;
        size_t max_batch = available / per_element_total;

        if (max_batch < 1) max_batch = 1;

        fprintf(stderr, "[GPU RECOVERY] Reducing batch for memory: %zu -> %zu "
                "(required: %zu MB, available: %zu MB)\n",
                *batch_size, max_batch,
                required / (1024 * 1024), available / (1024 * 1024));

        *batch_size = max_batch;
        return true;
    }
#endif

    return false;
}

/* ============================================================================
 * CPU Fallback
 * ============================================================================ */

bool nimcp_gpu_cpu_fallback_available(void)
{
    /* CPU is always available as fallback */
    return true;
}

void nimcp_gpu_set_cpu_fallback(
    nimcp_gpu_recovery_context_t* ctx,
    bool (*fallback_fn)(void* context, void* params, void* result),
    void* user_context)
{
    if (!ctx) return;

    ctx->cpu_fallback_fn = fallback_fn;
    ctx->cpu_fallback_context = user_context;
}

bool nimcp_gpu_execute_cpu_fallback(
    nimcp_gpu_recovery_context_t* ctx,
    void* params,
    void* result)
{
    if (!ctx || !ctx->cpu_fallback_fn) {
        return false;
    }

    log_recovery_action("CPU", "Executing CPU fallback");
    return ctx->cpu_fallback_fn(ctx->cpu_fallback_context, params, result);
}

/* ============================================================================
 * Memory Management
 * ============================================================================ */

size_t nimcp_gpu_free_caches(void)
{
    size_t freed = 0;

#ifdef NIMCP_ENABLE_CUDA
    size_t before_free, before_total;
    nimcp_gpu_get_memory_info(&before_free, &before_total);

    /* Clear device memory caches */
    cudaDeviceSynchronize();

    /* Trim memory pools (if using CUDA 11.2+) */
#if CUDART_VERSION >= 11020
    cudaMemPool_t mempool;
    if (cudaDeviceGetDefaultMemPool(&mempool, 0) == cudaSuccess) {
        cudaMemPoolTrimTo(mempool, 0);
    }
#endif

    size_t after_free, after_total;
    nimcp_gpu_get_memory_info(&after_free, &after_total);

    freed = (after_free > before_free) ? (after_free - before_free) : 0;
#endif

    return freed;
}

bool nimcp_gpu_get_memory_info(size_t* free_bytes, size_t* total_bytes)
{
    if (!free_bytes || !total_bytes) return false;

#ifdef NIMCP_ENABLE_CUDA
    cudaError_t err = cudaMemGetInfo(free_bytes, total_bytes);
    return (err == cudaSuccess);
#else
    *free_bytes = 0;
    *total_bytes = 0;
    return false;
#endif
}

bool nimcp_gpu_memory_critical(float threshold)
{
    size_t free_bytes, total_bytes;
    if (!nimcp_gpu_get_memory_info(&free_bytes, &total_bytes)) {
        return true;  /* Assume critical if can't check */
    }

    float usage = 1.0f - ((float)free_bytes / (float)total_bytes);
    return usage >= threshold;
}

bool nimcp_gpu_ensure_memory_available(size_t required_bytes)
{
    size_t free_bytes, total_bytes;
    if (!nimcp_gpu_get_memory_info(&free_bytes, &total_bytes)) {
        return false;
    }

    if (free_bytes >= required_bytes) {
        return true;  /* Already available */
    }

    /* Strategy 1: Free caches */
    nimcp_gpu_free_caches();

    if (!nimcp_gpu_get_memory_info(&free_bytes, &total_bytes)) {
        return false;
    }
    if (free_bytes >= required_bytes) {
        return true;
    }

    /* Strategy 2: Force synchronization (releases temp buffers) */
#ifdef NIMCP_ENABLE_CUDA
    cudaDeviceSynchronize();
#endif

    if (!nimcp_gpu_get_memory_info(&free_bytes, &total_bytes)) {
        return false;
    }

    return free_bytes >= required_bytes;
}

/* ============================================================================
 * Statistics
 * ============================================================================ */

void nimcp_gpu_recovery_get_stats(nimcp_gpu_recovery_stats_t* stats)
{
    if (!stats) return;

    *stats = g_stats;

    /* Calculate derived stats */
    if (stats->recoveries_attempted > 0) {
        stats->success_rate = (float)stats->recoveries_succeeded /
                             (float)stats->recoveries_attempted;
    }
}

void nimcp_gpu_recovery_reset_stats(void)
{
    memset(&g_stats, 0, sizeof(g_stats));
}
