#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_middleware_context.c - Middleware Context Implementation
//=============================================================================

#include "middleware/pipeline/nimcp_middleware_context.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "api/nimcp_api_exception.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "middleware_context"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(middleware_context)

#include <string.h>

//=============================================================================
// Internal Event Helpers
//=============================================================================

/**
 * WHAT: Deep copy brain event
 * WHY:  Store events in history with owned data
 * HOW:  Copy entire structure (data is embedded array)
 */
static bool brain_event_copy(brain_event_t* dest, const brain_event_t* src) {
    if (!dest || !src) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_event_copy: required parameter is NULL (dest, src)");
        return false;
    }

    // Simple struct copy - data array is embedded, not dynamically allocated
    *dest = *src;

    return true;
}

/**
 * WHAT: Free brain event resources
 * WHY:  Clean up event data
 * HOW:  No-op - data array is embedded in struct, not dynamically allocated
 */
static void brain_event_free(brain_event_t* event) {
    // No-op: brain_event_t uses embedded data array, not dynamic allocation
    (void)event;
}

//=============================================================================
// Context Lifecycle
//=============================================================================

middleware_context_t* middleware_context_create(brain_t brain,
                                                 uint32_t max_features,
                                                 uint32_t max_patterns,
                                                 uint32_t event_history_size,
                                                 uint32_t num_stages) {
    // VALIDATION FIX: Check for integer overflow in size calculations
    // Each allocation multiplies count * element_size, check for overflow
    if (max_features > SIZE_MAX / sizeof(float) ||
        max_patterns > SIZE_MAX / sizeof(uint32_t) ||
        max_patterns > SIZE_MAX / sizeof(float) ||
        event_history_size > SIZE_MAX / sizeof(brain_event_t) ||
        num_stages > SIZE_MAX / sizeof(uint64_t)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "middleware_context_create: size overflow detected");
        return NULL;  // Overflow would occur
    }

    middleware_context_t* ctx = nimcp_calloc(1, sizeof(middleware_context_t));
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "middleware_context_create: failed to allocate context");
        return NULL;
    }

    ctx->brain = brain;

    // ALLOCATION FIX: Check each allocation and clean up on failure
    ctx->cached_features = nimcp_calloc(max_features, sizeof(float));
    if (max_features > 0 && !ctx->cached_features) goto cleanup;
    ctx->num_cached_features = max_features;
    ctx->cached_features_capacity = max_features;

    ctx->detected_patterns = nimcp_calloc(max_patterns, sizeof(uint32_t));
    if (max_patterns > 0 && !ctx->detected_patterns) goto cleanup;

    ctx->pattern_confidences = nimcp_calloc(max_patterns, sizeof(float));
    if (max_patterns > 0 && !ctx->pattern_confidences) goto cleanup;
    ctx->num_detected_patterns = max_patterns;
    ctx->pattern_capacity = max_patterns;

    ctx->recent_events = nimcp_calloc(event_history_size, sizeof(brain_event_t));
    if (event_history_size > 0 && !ctx->recent_events) goto cleanup;
    ctx->recent_event_capacity = event_history_size;

    ctx->stage_timings_us = nimcp_calloc(num_stages, sizeof(uint64_t));
    if (num_stages > 0 && !ctx->stage_timings_us) goto cleanup;
    ctx->num_stages = num_stages;

    return ctx;

cleanup:
    // Clean up any successful allocations before returning NULL
    nimcp_free(ctx->cached_features);
    nimcp_free(ctx->detected_patterns);
    nimcp_free(ctx->pattern_confidences);
    nimcp_free(ctx->recent_events);
    nimcp_free(ctx->stage_timings_us);
    nimcp_free(ctx);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_event_free: operation failed");
    return NULL;
}

void middleware_context_destroy(middleware_context_t* context) {
    if (!context) return;

    // active_neurons is allocated on-demand by middleware_context_set_active_neurons()
    // (not in middleware_context_create), but may be NULL - freeing NULL is safe
    nimcp_free(context->active_neurons);
    nimcp_free(context->cached_features);
    nimcp_free(context->detected_patterns);
    nimcp_free(context->pattern_confidences);

    // Free events in history
    for (uint32_t i = 0; i < context->recent_event_count; i++) {
        brain_event_free(&context->recent_events[i]);
    }
    nimcp_free(context->recent_events);

    nimcp_free(context->stage_timings_us);
    nimcp_free(context);
}

//=============================================================================
// Context Operations
//=============================================================================

void middleware_context_set_active_neurons(middleware_context_t* context,
                                           uint32_t* neurons, uint32_t count) {
    if (!context) return;

    nimcp_free(context->active_neurons);
    context->active_neurons = NULL;
    context->num_active_neurons = 0;

    // Handle empty neuron list
    if (count == 0 || !neurons) return;

    context->active_neurons = nimcp_malloc(count * sizeof(uint32_t));
    if (context->active_neurons) {
        memcpy(context->active_neurons, neurons, count * sizeof(uint32_t));
        context->num_active_neurons = count;
    }
}

void middleware_context_cache_features(middleware_context_t* context,
                                       float* features, uint32_t count) {
    if (!context || count > context->num_cached_features) return;

    memcpy(context->cached_features, features, count * sizeof(float));
    context->features_valid = true;
}

bool middleware_context_get_cached_features(middleware_context_t* context,
                                            float** features, uint32_t* count) {
    if (!context || !context->features_valid) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "middleware_context_destroy: required parameter is NULL (context, context->features_valid)");
        return false;
    }

    *features = context->cached_features;
    *count = context->num_cached_features;
    return true;
}

void middleware_context_invalidate_cache(middleware_context_t* context) {
    if (context) context->features_valid = false;
}

void middleware_context_add_event(middleware_context_t* context, const brain_event_t* event) {
    if (!context || !event) return;

    // Free old event at write position
    if (context->recent_event_count >= context->recent_event_capacity) {
        brain_event_free(&context->recent_events[context->recent_event_head]);
    }

    // Copy new event
    brain_event_copy(&context->recent_events[context->recent_event_head], event);

    // Update counters
    context->recent_event_head = (context->recent_event_head + 1) % context->recent_event_capacity;
    if (context->recent_event_count < context->recent_event_capacity) {
        context->recent_event_count++;
    }
}

uint32_t middleware_context_get_recent_events(middleware_context_t* context, brain_event_t** events) {
    if (!context) return 0;

    *events = context->recent_events;
    return context->recent_event_count;
}

void middleware_context_record_stage_time(middleware_context_t* context,
                                          uint32_t stage_index, uint64_t time_us) {
    if (context && stage_index < context->num_stages) {
        context->stage_timings_us[stage_index] = time_us;
    }
}

bool middleware_context_get_stage_timings(middleware_context_t* context,
                                          uint64_t** timings, uint32_t* count) {
    if (!context) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "middleware_context_get_recent_events: context is NULL");
        return false;
    }

    *timings = context->stage_timings_us;
    *count = context->num_stages;
    return true;
}
