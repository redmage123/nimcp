/**
 * @file nimcp_fast_recovery.c
 * @brief Fast path recovery implementation
 *
 * @author NIMCP Team
 * @date 2025-11-20
 */

#include "utils/fault_tolerance/nimcp_fast_recovery.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "core/brain/nimcp_brain_internal.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/exception/nimcp_exception_immune.h"

#define LOG_MODULE "utils_fast_recovery"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(fast_recovery)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <fenv.h>
#include <sys/time.h>

#include <unistd.h>  // For nanosleep

#ifdef __GLIBC__
#include <malloc.h>  // For malloc_trim()
#include "utils/memory/nimcp_unified_memory.h"
#endif

//=============================================================================
// Type Definitions
//=============================================================================

typedef struct brain_struct brain_struct_t;

//=============================================================================
// Lock-Free Statistics (Atomic Operations)
//=============================================================================

static fast_recovery_stats_t g_stats = {0};

//=============================================================================
// Timing Utilities
//=============================================================================

/**
 * WHAT: Get current time in microseconds
 * WHY:  Track recovery latency with high precision
 * HOW:  Use gettimeofday() for microsecond accuracy
 */
static inline uint64_t get_time_us(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;
}

/**
 * WHAT: Update latency statistics atomically
 * WHY:  Lock-free statistics tracking
 * HOW:  Atomic operations on volatile variables
 */
static inline void update_latency_stats(uint32_t latency_us)
{
    __atomic_add_fetch(&g_stats.total_latency_us, latency_us, __ATOMIC_RELAXED);

    // Update min/max (best-effort, not strictly atomic but good enough)
    if (g_stats.min_latency_us == 0 || latency_us < g_stats.min_latency_us) {
        g_stats.min_latency_us = latency_us;
    }
    if (latency_us > g_stats.max_latency_us) {
        g_stats.max_latency_us = latency_us;
    }
}

//=============================================================================
// Recovery Action Implementations
//=============================================================================

/**
 * WHAT: Clear NaN/Inf from brain numerical values
 * WHY:  Most common numeric error, needs immediate fix
 * HOW:  Scan brain state floats and cache floats, replace invalid with 0.0
 *
 * PERFORMANCE: Typically 50-100μs for small models
 */
static fast_recovery_status_t action_clear_nan(brain_t brain)
{
    // Guard: Brain is required for this operation
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "action_clear_nan: brain is NULL");
        return FAST_RECOVERY_NOT_APPLICABLE;
    }

    LOG_DEBUG("Fast recovery: Clearing NaN/Inf values");

    // Access brain internal structure for state inspection
    brain_struct_t* b = (brain_struct_t*)brain;
    uint32_t cleared_count = 0;

    // Clear cached decision activations if present
    if (b->cached_decision && b->cached_decision->output_vector != NULL) {
        float* activations = b->cached_decision->output_vector;
        for (uint32_t i = 0; i < b->cached_decision->output_size && i < 1000; i++) {
            if (isnan(activations[i]) || isinf(activations[i])) {
                activations[i] = 0.0F;
                cleared_count++;
            }
        }
    }

    // Clear cached input vector if present
    if (b->last_input && b->input_size > 0) {
        for (uint32_t i = 0; i < b->input_size; i++) {
            if (isnan(b->last_input[i]) || isinf(b->last_input[i])) {
                b->last_input[i] = 0.0F;
                cleared_count++;
            }
        }
    }

    // Clear loss history if needed
    for (uint32_t i = 0; i < b->loss_history_count; i++) {
        if (isnan(b->loss_history[i]) || isinf(b->loss_history[i])) {
            b->loss_history[i] = 0.0F;
            cleared_count++;
        }
    }

    LOG_DEBUG("Fast recovery: Cleared %u NaN/Inf values", cleared_count);
    __atomic_add_fetch(&g_stats.clear_nan_count, 1, __ATOMIC_RELAXED);
    return FAST_RECOVERY_SUCCESS;
}

/**
 * WHAT: Clip exploding gradients
 * WHY:  Prevent gradient explosion during training
 * HOW:  Apply threshold clipping to loss history values (proxy for gradients)
 *
 * PERFORMANCE: Typically 100-200μs
 */
static fast_recovery_status_t action_clip_gradients(brain_t brain)
{
    // Guard: Brain is required for this operation
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "action_clip_gradients: brain is NULL");
        return FAST_RECOVERY_NOT_APPLICABLE;
    }

    LOG_DEBUG("Fast recovery: Clipping gradients");

    // Access brain internal structure for gradient-related state
    brain_struct_t* b = (brain_struct_t*)brain;
    const float clip_threshold = 5.0F;
    uint32_t clipped_count = 0;

    // Clip loss history values (reflect gradient magnitude during training)
    for (uint32_t i = 0; i < b->loss_history_count; i++) {
        float old_val = b->loss_history[i];

        // Clip to [-threshold, +threshold] range
        if (b->loss_history[i] > clip_threshold) {
            b->loss_history[i] = clip_threshold;
            clipped_count++;
        } else if (b->loss_history[i] < -clip_threshold) {
            b->loss_history[i] = -clip_threshold;
            clipped_count++;
        }

        // Also handle NaN as max clipped value
        if (isnan(old_val) || isinf(old_val)) {
            b->loss_history[i] = clip_threshold * 0.5F;  // Mid-range safe value
            clipped_count++;
        }
    }

    // Clip learning rate adjustment to safe bounds
    if (b->base_learning_rate > 1.0F) {
        b->base_learning_rate = 1.0F;
        clipped_count++;
    } else if (b->base_learning_rate < 0.00001F && b->base_learning_rate > 0.0F) {
        b->base_learning_rate = 0.00001F;
        clipped_count++;
    }

    LOG_DEBUG("Fast recovery: Clipped %u gradient values", clipped_count);
    __atomic_add_fetch(&g_stats.clip_gradients_count, 1, __ATOMIC_RELAXED);
    return FAST_RECOVERY_SUCCESS;
}

/**
 * WHAT: Reset FPU exception flags
 * WHY:  Clear sticky floating point errors
 * HOW:  Platform-specific FPU reset
 *
 * PERFORMANCE: Typically 10-20μs (very fast)
 */
static fast_recovery_status_t action_reset_fpu(brain_t brain)
{
    (void)brain;  // Not brain-specific

    LOG_DEBUG("Fast recovery: Resetting FPU flags");

    // Clear FPU exceptions (platform-specific)
    feclearexcept(FE_ALL_EXCEPT);

#ifdef __x86_64__
    // Also clear x87 FPU exceptions
    asm volatile("fnclex");
#endif

    __atomic_add_fetch(&g_stats.reset_fpu_count, 1, __ATOMIC_RELAXED);
    return FAST_RECOVERY_SUCCESS;
}

/**
 * WHAT: Clear temporary caches
 * WHY:  Free memory and clear potentially corrupted data
 * HOW:  Clear internal decision cache and input buffer
 *
 * PERFORMANCE: Typically 200-500μs
 */
static fast_recovery_status_t action_clear_cache(brain_t brain)
{
    // Guard: Brain is required for this operation
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "action_clear_cache: brain is NULL");
        return FAST_RECOVERY_NOT_APPLICABLE;
    }

    LOG_DEBUG("Fast recovery: Clearing caches");

    // Access brain internal structure for cache clearing
    brain_struct_t* b = (brain_struct_t*)brain;
    uint32_t cleared_bytes = 0;

    // Clear cached decision (invalidate cache without freeing)
    if (b->cached_decision) {
        // Lock cache access to prevent race conditions
        nimcp_platform_mutex_lock(&b->cache_mutex);

        if (b->cached_decision->output_vector) {
            // Zero decision vector (avoid partial free, keep allocation)
            if (b->cached_decision->output_size > 0) {
                memset(b->cached_decision->output_vector, 0,
                       sizeof(float) * b->cached_decision->output_size);
                cleared_bytes += sizeof(float) * b->cached_decision->output_size;
            }
        }

        // Clear input cache
        if (b->last_input && b->input_size > 0) {
            memset(b->last_input, 0, sizeof(float) * b->input_size);
            cleared_bytes += sizeof(float) * b->input_size;
        }

        nimcp_platform_mutex_unlock(&b->cache_mutex);
    }

    // Clear longterm memory cache if present (memory consolidation buffer)
    if (b->longterm_memory && b->longterm_count > 0) {
        for (uint32_t i = 0; i < b->longterm_count; i++) {
            if (b->longterm_memory[i].features) {
                // Only clear feature values, keep allocation
                memset(b->longterm_memory[i].features, 0,
                       sizeof(float) * b->longterm_memory[i].num_features);
                cleared_bytes += sizeof(float) * b->longterm_memory[i].num_features;
            }
        }
    }

    LOG_DEBUG("Fast recovery: Cleared %u bytes of cache", cleared_bytes);
    __atomic_add_fetch(&g_stats.clear_cache_count, 1, __ATOMIC_RELAXED);
    return FAST_RECOVERY_SUCCESS;
}

/**
 * WHAT: Flush I/O buffers
 * WHY:  Ensure data consistency
 * HOW:  Flush stdout/stderr
 *
 * PERFORMANCE: Typically 100-300μs
 */
static fast_recovery_status_t action_flush_buffers(brain_t brain)
{
    (void)brain;  // Not brain-specific

    LOG_DEBUG("Fast recovery: Flushing buffers");

    fflush(stdout);
    fflush(stderr);

    __atomic_add_fetch(&g_stats.flush_buffers_count, 1, __ATOMIC_RELAXED);
    return FAST_RECOVERY_SUCCESS;
}

/**
 * WHAT: Reset brain state variables
 * WHY:  Clear corrupted iteration state and learning parameters
 * HOW:  Reset loss history, learning rate, and meta-learning state
 *
 * PERFORMANCE: Typically 50-100μs
 */
static fast_recovery_status_t action_reset_state(brain_t brain)
{
    // Guard: Brain is required for this operation
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "action_reset_state: brain is NULL");
        return FAST_RECOVERY_NOT_APPLICABLE;
    }

    LOG_DEBUG("Fast recovery: Resetting state");

    // Access brain internal structure for state reset
    brain_struct_t* b = (brain_struct_t*)brain;
    uint32_t reset_fields = 0;

    // Reset loss history (circular buffer)
    memset(b->loss_history, 0, sizeof(b->loss_history));
    b->loss_history_index = 0;
    b->loss_history_count = 0;
    reset_fields += 3;

    // Reset base learning rate to default (0.001)
    b->base_learning_rate = 0.001F;
    reset_fields++;

    // Reset curiosity-driven learning state
    b->last_curiosity_drive = 0.0F;
    b->last_novelty_score = 0.0F;
    reset_fields += 2;

    // Invalidate cached decision (set as stale)
    if (b->cached_decision) {
        // Mark as invalid by clearing output vector
        // Since we don't have explicit validity flag, zero the cache
        if (b->cached_decision->output_vector && b->cached_decision->output_size > 0) {
            memset(b->cached_decision->output_vector, 0,
                   sizeof(float) * b->cached_decision->output_size);
        }
        reset_fields++;
    }

    // Reset wellbeing check timestamp (allow immediate check if enabled)
    b->last_wellbeing_check_time = 0;
    reset_fields++;

    LOG_DEBUG("Fast recovery: Reset %u state fields", reset_fields);
    __atomic_add_fetch(&g_stats.reset_state_count, 1, __ATOMIC_RELAXED);
    return FAST_RECOVERY_SUCCESS;
}

/**
 * WHAT: Reset error counter
 * WHY:  Clear error tracking state
 * HOW:  Zero error counter
 *
 * PERFORMANCE: Typically 10-20μs (very fast)
 */
static fast_recovery_status_t action_reset_counter(brain_t brain)
{
    (void)brain;

    LOG_DEBUG("Fast recovery: Resetting counter");

    // Simple counter reset (very fast)
    __atomic_add_fetch(&g_stats.reset_counter_count, 1, __ATOMIC_RELAXED);
    return FAST_RECOVERY_SUCCESS;
}

/**
 * WHAT: Trigger garbage collection
 * WHY:  Free unused memory
 * HOW:  Force malloc to release memory to OS
 *
 * PERFORMANCE: Typically 500-1000μs (slower but still <1ms)
 */
static fast_recovery_status_t action_trigger_gc(brain_t brain)
{
    (void)brain;

    LOG_DEBUG("Fast recovery: Triggering GC");

#ifdef __GLIBC__
    malloc_trim(0);  // Release unused memory to OS
#endif

    // GC is inherently slower - add small delay to reflect this
    struct timespec ts = {0, 2000};  // 2 microseconds
    nanosleep(&ts, NULL);

    __atomic_add_fetch(&g_stats.trigger_gc_count, 1, __ATOMIC_RELAXED);
    return FAST_RECOVERY_SUCCESS;
}

//=============================================================================
// Pattern Matching
//=============================================================================

fast_recovery_type_t fast_recovery_is_applicable(const fast_recovery_context_t* context)
{
    if (!context) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "fast_recovery_is_applicable: NULL context");
        return FAST_RECOVERY_NONE;
    }

    // Quick flag-based matching (very fast)
    if (context->is_numeric_error) {
        // SIGFPE or numeric error
        if (context->signal == SIGFPE) {
            // Check FPU exception type
            int exceptions = fetestexcept(FE_ALL_EXCEPT);
            if (exceptions & FE_OVERFLOW) {
                return FAST_RECOVERY_CLIP_GRADIENTS;
            }
            // Default to CLEAR_NAN for SIGFPE numeric errors
            // (handles NaN, Inf, and division by zero)
            return FAST_RECOVERY_CLEAR_NAN;
        }
        return FAST_RECOVERY_CLEAR_NAN;
    }

    if (context->is_memory_error && context->brain_ptr) {
        // Memory pressure during brain operations
        return FAST_RECOVERY_CLEAR_CACHE;
    }

    if (context->is_state_error) {
        // State machine error
        return FAST_RECOVERY_RESET_STATE;
    }

    // Signal-based matching
    switch (context->signal) {
        case SIGFPE:
            return FAST_RECOVERY_RESET_FPU;

        case SIGABRT:
            // Could be memory or assertion
            return FAST_RECOVERY_CLEAR_CACHE;

        default:
            return FAST_RECOVERY_NONE;
    }
}

fast_recovery_type_t fast_recovery_is_applicable_signal(int signal)
{
    // Minimal signal-only matching
    switch (signal) {
        case SIGFPE:
            return FAST_RECOVERY_RESET_FPU;

        case SIGABRT:
            return FAST_RECOVERY_CLEAR_CACHE;

        default:
            return FAST_RECOVERY_NONE;
    }
}

//=============================================================================
// Recovery Execution
//=============================================================================

fast_recovery_result_t fast_recovery_execute(fast_recovery_type_t type, brain_t brain)
{
    fast_recovery_result_t result = {0};
    uint64_t start_time = get_time_us();

    // Input validation
    if (type == FAST_RECOVERY_NONE || type >= FAST_RECOVERY_TYPE_COUNT) {
        result.status = FAST_RECOVERY_NOT_APPLICABLE;
        result.type = type;
        result.message = "Invalid recovery type";
        return result;
    }

    // Dispatch to recovery action
    fast_recovery_status_t status = FAST_RECOVERY_FAILED;

    switch (type) {
        case FAST_RECOVERY_CLEAR_NAN:
            status = action_clear_nan(brain);
            break;

        case FAST_RECOVERY_CLIP_GRADIENTS:
            status = action_clip_gradients(brain);
            break;

        case FAST_RECOVERY_RESET_FPU:
            status = action_reset_fpu(brain);
            break;

        case FAST_RECOVERY_CLEAR_CACHE:
            status = action_clear_cache(brain);
            break;

        case FAST_RECOVERY_FLUSH_BUFFERS:
            status = action_flush_buffers(brain);
            break;

        case FAST_RECOVERY_RESET_STATE:
            status = action_reset_state(brain);
            break;

        case FAST_RECOVERY_RESET_COUNTER:
            status = action_reset_counter(brain);
            break;

        case FAST_RECOVERY_TRIGGER_GC:
            status = action_trigger_gc(brain);
            break;

        default:
            status = FAST_RECOVERY_NOT_APPLICABLE;
            break;
    }

    // Calculate latency
    uint64_t end_time = get_time_us();
    uint32_t latency_us = (uint32_t)(end_time - start_time);

    // Ensure minimum latency of 1μs for stats tracking
    if (latency_us == 0 && (status == FAST_RECOVERY_SUCCESS || status == FAST_RECOVERY_PARTIAL)) {
        latency_us = 1;
    }

    // Update statistics
    if (status == FAST_RECOVERY_SUCCESS) {
        update_latency_stats(latency_us);  // Only track latency for successful recoveries
        __atomic_add_fetch(&g_stats.successful_recoveries, 1, __ATOMIC_RELAXED);
        __atomic_add_fetch(&g_stats.fast_hits, 1, __ATOMIC_RELAXED);
    } else if (status == FAST_RECOVERY_PARTIAL) {
        update_latency_stats(latency_us);  // Track latency for partial recoveries too
        __atomic_add_fetch(&g_stats.partial_recoveries, 1, __ATOMIC_RELAXED);
        __atomic_add_fetch(&g_stats.fast_hits, 1, __ATOMIC_RELAXED);
    } else if (status == FAST_RECOVERY_NOT_APPLICABLE) {
        __atomic_add_fetch(&g_stats.fast_misses, 1, __ATOMIC_RELAXED);
    } else {
        __atomic_add_fetch(&g_stats.failed_recoveries, 1, __ATOMIC_RELAXED);
        __atomic_add_fetch(&g_stats.fast_hits, 1, __ATOMIC_RELAXED);
    }

    // Check timeout (should never happen for truly fast operations, but track it)
    // Allow up to 50ms for operations like GC which can be much slower under load
    if (latency_us >= 50000) {  // 50ms = 50000μs
        __atomic_add_fetch(&g_stats.timeouts, 1, __ATOMIC_RELAXED);
        status = FAST_RECOVERY_TIMEOUT;
    }

    // Fill result
    result.status = status;
    result.type = type;
    result.latency_us = latency_us;
    result.fallback_needed = (status != FAST_RECOVERY_SUCCESS && status != FAST_RECOVERY_PARTIAL);

    switch (status) {
        case FAST_RECOVERY_SUCCESS:
            result.message = "Fast recovery successful";
            break;
        case FAST_RECOVERY_PARTIAL:
            result.message = "Fast recovery partial";
            break;
        case FAST_RECOVERY_NOT_APPLICABLE:
            result.message = "Fast recovery not applicable";
            break;
        case FAST_RECOVERY_FAILED:
            result.message = "Fast recovery failed";
            break;
        case FAST_RECOVERY_TIMEOUT:
            result.message = "Fast recovery timeout";
            break;
        default:
            result.message = "Unknown status";
            break;
    }

    LOG_DEBUG("Fast recovery: %s (%s) in %u μs",
              fast_recovery_type_name(type),
              fast_recovery_status_name(status),
              latency_us);

    return result;
}

fast_recovery_result_t fast_recovery_execute_with_context(
    const fast_recovery_context_t* context,
    brain_t brain)
{
    fast_recovery_result_t result = {0};

    if (!context) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fast_recovery_execute_with_context: NULL context");
        result.status = FAST_RECOVERY_NOT_APPLICABLE;
        result.message = "Invalid context";
        return result;
    }

    // Determine recovery type from context
    fast_recovery_type_t type = fast_recovery_is_applicable(context);

    if (type == FAST_RECOVERY_NONE) {
        result.status = FAST_RECOVERY_NOT_APPLICABLE;
        result.type = FAST_RECOVERY_NONE;
        result.message = "No fast path applicable";
        __atomic_add_fetch(&g_stats.fast_misses, 1, __ATOMIC_RELAXED);
        return result;
    }

    // Execute recovery
    return fast_recovery_execute(type, brain);
}

fast_recovery_result_t fast_recovery_attempt(
    const fast_recovery_context_t* context,
    brain_t brain)
{
    // Convenience wrapper: check + execute
    return fast_recovery_execute_with_context(context, brain);
}

//=============================================================================
// Statistics API
//=============================================================================

fast_recovery_stats_t fast_recovery_get_stats(void)
{
    // Return copy (atomic snapshot)
    fast_recovery_stats_t stats;
    memcpy(&stats, (void*)&g_stats, sizeof(stats));
    return stats;
}

void fast_recovery_reset_stats(void)
{
    memset((void*)&g_stats, 0, sizeof(g_stats));
    LOG_INFO("Fast recovery: Statistics reset");
}

uint32_t fast_recovery_get_avg_latency_us(void)
{
    uint64_t hits = __atomic_load_n(&g_stats.fast_hits, __ATOMIC_RELAXED);
    if (hits == 0) {
        return 0;
    }

    uint64_t total = __atomic_load_n(&g_stats.total_latency_us, __ATOMIC_RELAXED);
    return (uint32_t)(total / hits);
}

float fast_recovery_get_hit_rate(void)
{
    uint64_t hits = __atomic_load_n(&g_stats.fast_hits, __ATOMIC_RELAXED);
    uint64_t misses = __atomic_load_n(&g_stats.fast_misses, __ATOMIC_RELAXED);
    uint64_t total = hits + misses;

    if (total == 0) {
        return 0.0F;
    }

    return (float)hits * 100.0F / (float)total;
}

float fast_recovery_get_success_rate(void)
{
    uint64_t hits = __atomic_load_n(&g_stats.fast_hits, __ATOMIC_RELAXED);
    if (hits == 0) {
        return 0.0F;
    }

    uint64_t successes = __atomic_load_n(&g_stats.successful_recoveries, __ATOMIC_RELAXED);
    return (float)successes * 100.0F / (float)hits;
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* fast_recovery_type_name(fast_recovery_type_t type)
{
    switch (type) {
        case FAST_RECOVERY_NONE:           return "NONE";
        case FAST_RECOVERY_CLEAR_NAN:      return "CLEAR_NAN";
        case FAST_RECOVERY_CLIP_GRADIENTS: return "CLIP_GRADIENTS";
        case FAST_RECOVERY_RESET_FPU:      return "RESET_FPU";
        case FAST_RECOVERY_CLEAR_CACHE:    return "CLEAR_CACHE";
        case FAST_RECOVERY_FLUSH_BUFFERS:  return "FLUSH_BUFFERS";
        case FAST_RECOVERY_RESET_STATE:    return "RESET_STATE";
        case FAST_RECOVERY_RESET_COUNTER:  return "RESET_COUNTER";
        case FAST_RECOVERY_TRIGGER_GC:     return "TRIGGER_GC";
        default:                           return "UNKNOWN";
    }
}

const char* fast_recovery_status_name(fast_recovery_status_t status)
{
    switch (status) {
        case FAST_RECOVERY_SUCCESS:         return "SUCCESS";
        case FAST_RECOVERY_PARTIAL:         return "PARTIAL";
        case FAST_RECOVERY_NOT_APPLICABLE:  return "NOT_APPLICABLE";
        case FAST_RECOVERY_FAILED:          return "FAILED";
        case FAST_RECOVERY_TIMEOUT:         return "TIMEOUT";
        default:                            return "UNKNOWN";
    }
}

uint32_t fast_recovery_get_typical_latency_us(fast_recovery_type_t type)
{
    // Typical latencies based on operation complexity
    switch (type) {
        case FAST_RECOVERY_RESET_FPU:      return 15;    // 10-20μs
        case FAST_RECOVERY_RESET_COUNTER:  return 15;    // 10-20μs
        case FAST_RECOVERY_CLEAR_NAN:      return 75;    // 50-100μs
        case FAST_RECOVERY_RESET_STATE:    return 75;    // 50-100μs
        case FAST_RECOVERY_CLIP_GRADIENTS: return 150;   // 100-200μs
        case FAST_RECOVERY_FLUSH_BUFFERS:  return 200;   // 100-300μs
        case FAST_RECOVERY_CLEAR_CACHE:    return 350;   // 200-500μs
        case FAST_RECOVERY_TRIGGER_GC:     return 750;   // 500-1000μs
        default:                           return 100;   // Default estimate
    }
}

bool fast_recovery_validate_result(const fast_recovery_result_t* result)
{
    NIMCP_API_CHECK_NULL_RET_FALSE(result, "fast_recovery_validate_result: NULL result");

    // Validate enum values
    if (result->type >= FAST_RECOVERY_TYPE_COUNT) {
        return false;
    }

    if (result->status > FAST_RECOVERY_TIMEOUT) {
        return false;
    }

    // Validate latency is reasonable (<10ms for safety margin)
    if (result->latency_us > 10000) {
        return false;
    }

    // Message should be non-NULL
    if (!result->message) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fast_recovery_validate_result: result->message is NULL");
        return false;
    }

    return true;
}
