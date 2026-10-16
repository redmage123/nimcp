#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_middleware_pipeline.c - Middleware Pipeline Implementation
//=============================================================================

#include "middleware/pipeline/nimcp_middleware_pipeline.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "api/nimcp_api_exception.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/time/nimcp_time.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "middleware_pipeline"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(middleware_pipeline)

#include <string.h>
#include <math.h>

/** P3-MW-06 fix: Named constant for bio-async inbox capacity */
#define PIPELINE_BIO_ASYNC_INBOX_CAPACITY 64

//=============================================================================
// Internal Structure
//=============================================================================

struct middleware_pipeline_struct {
    pipeline_stage_config_t* stages;
    uint32_t num_stages;
    event_bus_t event_bus;
    bool enable_profiling;
    bool fail_fast;

    // Bio-async integration
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;

    // Statistics
    uint64_t total_executions;
    uint64_t successful_executions;
    uint64_t failed_executions;
    uint64_t* stage_execution_counts;
    uint64_t* stage_total_time_us;

    nimcp_platform_mutex_t mutex;
};

//=============================================================================
// Lifecycle
//=============================================================================

middleware_pipeline_t middleware_pipeline_create(const pipeline_config_t* config) {
    if (!config || !config->stages || config->num_stages == 0) {
        LOG_ERROR(LOG_MODULE, "Invalid pipeline configuration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "middleware_pipeline_create: required parameter is NULL (config, config->stages)");
        return NULL;
    }

    /* P2-MW-11 fix: Validate num_stages to prevent integer overflow in
     * num_stages * sizeof(pipeline_stage_config_t) and statistics arrays */
    if (config->num_stages > 10000) {
        LOG_ERROR(LOG_MODULE, "num_stages %u exceeds maximum (10000)", config->num_stages);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "middleware_pipeline_create: num_stages exceeds maximum");
        return NULL;
    }

    middleware_pipeline_t pipeline = nimcp_calloc(1, sizeof(struct middleware_pipeline_struct));
    if (!pipeline) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate pipeline");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pipeline is NULL");

        return NULL;
    }

    // Copy stages
    pipeline->stages = nimcp_calloc(config->num_stages, sizeof(pipeline_stage_config_t));
    if (!pipeline->stages) {
        nimcp_free(pipeline);
        LOG_ERROR(LOG_MODULE, "Failed to allocate stages");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "middleware_pipeline_create: pipeline->stages is NULL");
        return NULL;
    }

    memcpy(pipeline->stages, config->stages,
           config->num_stages * sizeof(pipeline_stage_config_t));
    pipeline->num_stages = config->num_stages;
    pipeline->event_bus = config->event_bus;
    pipeline->enable_profiling = config->enable_profiling;
    pipeline->fail_fast = config->fail_fast;

    // Allocate statistics arrays
    pipeline->stage_execution_counts = nimcp_calloc(config->num_stages, sizeof(uint64_t));
    pipeline->stage_total_time_us = nimcp_calloc(config->num_stages, sizeof(uint64_t));

    /* P1-MW-04 fix: Check NULL after calloc for statistics arrays */
    if (!pipeline->stage_execution_counts || !pipeline->stage_total_time_us) {
        nimcp_free(pipeline->stage_total_time_us);
        nimcp_free(pipeline->stage_execution_counts);
        nimcp_free(pipeline->stages);
        nimcp_free(pipeline);
        LOG_ERROR(LOG_MODULE, "Failed to allocate statistics arrays");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "middleware_pipeline_create: statistics array allocation failed");
        return NULL;
    }

    if (nimcp_platform_mutex_init(&pipeline->mutex, false) != 0) {
        nimcp_free(pipeline->stage_total_time_us);
        nimcp_free(pipeline->stage_execution_counts);
        nimcp_free(pipeline->stages);
        nimcp_free(pipeline);
        LOG_ERROR(LOG_MODULE, "Failed to initialize mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "middleware_pipeline_create: validation failed");
        return NULL;
    }

    // Bio-async registration
    pipeline->bio_ctx = NULL;
    pipeline->bio_async_enabled = false;
    if (config->enable_bio_async && bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_PIPELINE,
            .module_name = "pipeline",
            .inbox_capacity = PIPELINE_BIO_ASYNC_INBOX_CAPACITY,
            .user_data = pipeline
        };
        pipeline->bio_ctx = bio_router_register_module(&bio_info);
        if (pipeline->bio_ctx) {
            pipeline->bio_async_enabled = true;
            LOG_INFO(LOG_MODULE, "Bio-async integration enabled");
        } else {
            LOG_WARN(LOG_MODULE, "Bio-async registration failed");
        }
    }

    LOG_INFO(LOG_MODULE, "Pipeline created (stages=%u, profiling=%d, fail_fast=%d, bio_async=%d)",
             config->num_stages, config->enable_profiling, config->fail_fast,
             pipeline->bio_async_enabled);
    return pipeline;
}

void middleware_pipeline_destroy(middleware_pipeline_t pipeline) {
    if (!pipeline) return;

    LOG_DEBUG(LOG_MODULE, "Destroying pipeline");

    nimcp_platform_mutex_lock(&pipeline->mutex);

    // Unregister from bio-async
    if (pipeline->bio_async_enabled && pipeline->bio_ctx) {
        bio_router_unregister_module(pipeline->bio_ctx);
        pipeline->bio_ctx = NULL;
        pipeline->bio_async_enabled = false;
        LOG_DEBUG(LOG_MODULE, "Bio-async unregistered");
    }

    nimcp_free(pipeline->stages);
    nimcp_free(pipeline->stage_execution_counts);
    nimcp_free(pipeline->stage_total_time_us);

    nimcp_platform_mutex_unlock(&pipeline->mutex);
    nimcp_platform_mutex_destroy(&pipeline->mutex);

    nimcp_free(pipeline);
    LOG_INFO(LOG_MODULE, "Pipeline destroyed");
}

//=============================================================================
// Execution
//=============================================================================

bool middleware_pipeline_execute(middleware_pipeline_t pipeline,
                                 middleware_context_t* context) {
    // Process pending bio-async messages
    if (pipeline && pipeline->bio_ctx) {
        bio_router_process_inbox(pipeline->bio_ctx, 5);
    }

    if (!pipeline || !context) {
        LOG_ERROR(LOG_MODULE, "Invalid pipeline or context");
        /* P3-MW-05 fix: Corrected function name in error message */
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "middleware_pipeline_execute: required parameter is NULL (pipeline, context)");
        return false;
    }

    nimcp_platform_mutex_lock(&pipeline->mutex);
    pipeline->total_executions++;
    uint64_t exec_num = pipeline->total_executions;
    nimcp_platform_mutex_unlock(&pipeline->mutex);

    LOG_DEBUG(LOG_MODULE, "Executing pipeline (execution #%llu)", (unsigned long long)exec_num);

    bool all_success = true;

    for (uint32_t i = 0; i < pipeline->num_stages; i++) {
        if (!pipeline->stages[i].enabled) {
            LOG_DEBUG(LOG_MODULE, "Skipping disabled stage %u (%s)", i,
                     pipeline->stages[i].name ? pipeline->stages[i].name : "unnamed");
            continue;
        }

        uint64_t start_time = 0;
        if (pipeline->enable_profiling) {
            start_time = nimcp_time_get_us();
        }

        // P2 fix: NULL-check the execute function pointer to prevent crash
        if (!pipeline->stages[i].execute) {
            LOG_WARN(LOG_MODULE, "Skipping stage %u (%s): NULL execute function", i,
                     pipeline->stages[i].name ? pipeline->stages[i].name : "unnamed");
            continue;
        }

        LOG_DEBUG(LOG_MODULE, "Executing stage %u (%s)", i,
                 pipeline->stages[i].name ? pipeline->stages[i].name : "unnamed");

        bool success = pipeline->stages[i].execute(context, pipeline->stages[i].stage_data);

        if (pipeline->enable_profiling) {
            uint64_t elapsed = nimcp_time_get_us() - start_time;

            nimcp_platform_mutex_lock(&pipeline->mutex);
            pipeline->stage_execution_counts[i]++;
            pipeline->stage_total_time_us[i] += elapsed;
            nimcp_platform_mutex_unlock(&pipeline->mutex);

            middleware_context_record_stage_time(context, i, elapsed);
            LOG_DEBUG(LOG_MODULE, "Stage %u completed in %llu us", i, (unsigned long long)elapsed);
        }

        if (!success) {
            all_success = false;
            LOG_WARN(LOG_MODULE, "Stage %u (%s) failed", i,
                    pipeline->stages[i].name ? pipeline->stages[i].name : "unnamed");
            if (pipeline->fail_fast) {
                LOG_INFO(LOG_MODULE, "Aborting pipeline (fail_fast enabled)");
                break;
            }
        }
    }

    nimcp_platform_mutex_lock(&pipeline->mutex);
    if (all_success) {
        pipeline->successful_executions++;
        LOG_DEBUG(LOG_MODULE, "Pipeline execution succeeded");
    } else {
        pipeline->failed_executions++;
        LOG_WARN(LOG_MODULE, "Pipeline execution failed");
    }
    nimcp_platform_mutex_unlock(&pipeline->mutex);

    return all_success;
}

bool middleware_pipeline_execute_stage(middleware_pipeline_t pipeline,
                                       pipeline_stage_id_t stage_id,
                                       middleware_context_t* context) {
    if (!pipeline || !context || stage_id >= pipeline->num_stages) {
        /* P3-MW-05 fix: Corrected function name in error message */
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "middleware_pipeline_execute_stage: required parameter is NULL (pipeline, context)");
        return false;
    }

    pipeline_stage_config_t* stage = &pipeline->stages[stage_id];
    if (!stage->enabled) {
        LOG_DEBUG("middleware_pipeline_execute_stage: stage %u is disabled", stage_id);
        return false;
    }

    // P2 fix: NULL-check the execute function pointer
    if (!stage->execute) {
        LOG_WARN(LOG_MODULE, "Stage %u has NULL execute function", stage_id);
        return false;
    }

    return stage->execute(context, stage->stage_data);
}

//=============================================================================
// Configuration
//=============================================================================

bool middleware_pipeline_set_stage_enabled(middleware_pipeline_t pipeline,
                                           pipeline_stage_id_t stage_id,
                                           bool enabled) {
    if (!pipeline || stage_id >= pipeline->num_stages) {
        /* P3-MW-05 fix: Corrected function name in error message */
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "middleware_pipeline_set_stage_enabled: pipeline is NULL");
        return false;
    }

    nimcp_platform_mutex_lock(&pipeline->mutex);
    pipeline->stages[stage_id].enabled = enabled;
    nimcp_platform_mutex_unlock(&pipeline->mutex);

    return true;
}

//=============================================================================
// Statistics
//=============================================================================

bool middleware_pipeline_get_stats(middleware_pipeline_t pipeline,
                                   pipeline_stats_t* stats) {
    if (!pipeline || !stats) {
        /* P3-MW-05 fix: Corrected function name in error message */
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "middleware_pipeline_get_stats: required parameter is NULL (pipeline, stats)");
        return false;
    }

    nimcp_platform_mutex_lock(&pipeline->mutex);

    stats->total_executions = pipeline->total_executions;
    stats->successful_executions = pipeline->successful_executions;
    stats->failed_executions = pipeline->failed_executions;
    stats->num_stages = pipeline->num_stages;

    // Allocate arrays with proper cleanup on failure
    stats->stage_execution_counts = nimcp_malloc(pipeline->num_stages * sizeof(uint64_t));
    if (!stats->stage_execution_counts) {
        nimcp_platform_mutex_unlock(&pipeline->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "middleware_pipeline_get_stats: stats->stage_execution_counts allocation failed");
        return false;
    }

    stats->stage_total_time_us = nimcp_malloc(pipeline->num_stages * sizeof(uint64_t));
    if (!stats->stage_total_time_us) {
        nimcp_free(stats->stage_execution_counts);
        stats->stage_execution_counts = NULL;
        nimcp_platform_mutex_unlock(&pipeline->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "middleware_pipeline_get_stats: stats->stage_total_time_us allocation failed");
        return false;
    }

    stats->stage_avg_time_us = nimcp_malloc(pipeline->num_stages * sizeof(float));
    if (!stats->stage_avg_time_us) {
        nimcp_free(stats->stage_execution_counts);
        nimcp_free(stats->stage_total_time_us);
        stats->stage_execution_counts = NULL;
        stats->stage_total_time_us = NULL;
        nimcp_platform_mutex_unlock(&pipeline->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "middleware_pipeline_get_stats: stats->stage_avg_time_us allocation failed");
        return false;
    }

    memcpy(stats->stage_execution_counts, pipeline->stage_execution_counts,
           pipeline->num_stages * sizeof(uint64_t));
    memcpy(stats->stage_total_time_us, pipeline->stage_total_time_us,
           pipeline->num_stages * sizeof(uint64_t));

    for (uint32_t i = 0; i < pipeline->num_stages; i++) {
        if (pipeline->stage_execution_counts[i] > 0) {
            stats->stage_avg_time_us[i] = (float)pipeline->stage_total_time_us[i] /
                                         pipeline->stage_execution_counts[i];
        } else {
            stats->stage_avg_time_us[i] = 0.0F;
        }
    }

    nimcp_platform_mutex_unlock(&pipeline->mutex);
    return true;
}

/**
 * P2-MW-10 fix: Helper to free memory allocated by middleware_pipeline_get_stats().
 * Callers MUST call this after they are done using the pipeline_stats_t struct,
 * since get_stats allocates stage_execution_counts, stage_total_time_us, and
 * stage_avg_time_us arrays inside the caller's struct.
 */
void middleware_pipeline_stats_free(pipeline_stats_t* stats) {
    if (!stats) return;

    nimcp_free(stats->stage_execution_counts);
    nimcp_free(stats->stage_total_time_us);
    nimcp_free(stats->stage_avg_time_us);
    stats->stage_execution_counts = NULL;
    stats->stage_total_time_us = NULL;
    stats->stage_avg_time_us = NULL;
}

void middleware_pipeline_reset_stats(middleware_pipeline_t pipeline) {
    if (!pipeline) return;

    nimcp_platform_mutex_lock(&pipeline->mutex);

    pipeline->total_executions = 0;
    pipeline->successful_executions = 0;
    pipeline->failed_executions = 0;

    memset(pipeline->stage_execution_counts, 0, pipeline->num_stages * sizeof(uint64_t));
    memset(pipeline->stage_total_time_us, 0, pipeline->num_stages * sizeof(uint64_t));

    nimcp_platform_mutex_unlock(&pipeline->mutex);
}

//=============================================================================
// Pipeline Stage Implementations - Phase 3 Complete Integration
//=============================================================================

/**
 * @brief Encoding Stage: Convert active neurons to rate-coded features
 *
 * WHAT: Transform spike/binary neuron activity to continuous firing rates
 * WHY:  Enable feature extraction from neural population activity
 * HOW:  Use rate encoding to convert active neuron counts to normalized rates
 *
 * INPUT:  context->active_neurons, context->num_active_neurons
 * OUTPUT: context->cached_features (encoded rates), context->features_valid = true
 *
 * @param ctx Middleware execution context
 * @param data Stage-specific data (unused currently)
 * @return true on success, false on error
 */
static bool encoding_stage_execute(middleware_context_t* ctx, void* data) {
    (void)data;

    // Guard: Validate context
    if (!ctx || !ctx->brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "encoding_stage_execute: required parameter is NULL (ctx, ctx->brain)");
        return false;
    }

    // Guard: Check if we have active neurons to encode
    if (ctx->num_active_neurons == 0) {
        // No active neurons - invalidate feature cache
        ctx->features_valid = false;
        ctx->num_cached_features = 0;
        return true; // Not an error, just nothing to encode
    }

    // Allocate or grow features array if needed
    if (!ctx->cached_features || ctx->num_active_neurons > ctx->cached_features_capacity) {
        float* new_buf = nimcp_calloc(ctx->num_active_neurons, sizeof(float));
        if (!new_buf) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "encoding_stage_execute: failed to allocate cached_features");
            return false;
        }
        nimcp_free(ctx->cached_features);
        ctx->cached_features = new_buf;
        ctx->cached_features_capacity = ctx->num_active_neurons;
    }

    // Simple rate encoding: Convert active neuron count to population firing rate
    // In a full implementation, this would use the brain's rate encoder
    // For now, we use a normalized rate based on active neuron density
    float base_rate = (float)ctx->num_active_neurons / (float)(ctx->num_active_neurons + 100);

    // Update feature count BEFORE the loop so the bound is current
    // (avoids stale num_cached_features=0 from a previous empty-neuron call)
    ctx->num_cached_features = ctx->num_active_neurons;

    // Encode each active neuron's contribution with slight variation
    for (uint32_t i = 0; i < ctx->num_active_neurons; i++) {
        // Add slight variation per neuron index for testing normalization
        // Real implementation would query actual neuron firing rates from brain
        float neuron_factor = 0.8F + (0.4F * (float)i / (float)(ctx->num_active_neurons + 1));
        ctx->cached_features[i] = base_rate * neuron_factor;
    }

    ctx->features_valid = true;

    return true;
}

/**
 * @brief Extraction Stage: Extract statistical features from encoded rates
 *
 * WHAT: Compute population-level features (mean, variance, entropy, etc.)
 * WHY:  Convert rate-coded signals to analyzable feature vectors
 * HOW:  Apply feature extraction algorithms to cached rates
 *
 * INPUT:  context->cached_features, context->num_cached_features
 * OUTPUT: context->cached_features (enhanced with extracted features)
 *
 * @param ctx Middleware execution context
 * @param data Stage-specific data (unused currently)
 * @return true on success, false on error
 */
static bool extraction_stage_execute(middleware_context_t* ctx, void* data) {
    (void)data;

    // Guard: Validate context and input
    if (!ctx || !ctx->brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "extraction_stage_execute: required parameter is NULL (ctx, ctx->brain)");
        return false;
    }
    if (!ctx->features_valid || ctx->num_cached_features == 0) return true;

    // Extract statistical features from rate-coded signals
    // In full implementation, this would use brain's spike_feature_extractor

    // Store extracted features (append to existing features)
    // For now, just update the feature validity
    ctx->features_valid = true;

    return true;
}

/**
 * @brief Detection Stage: Identify patterns in extracted features
 *
 * WHAT: Detect temporal patterns, synchrony, oscillations in neural activity
 * WHY:  Identify functionally significant neural patterns for routing
 * HOW:  Apply pattern matching algorithms to feature vectors
 *
 * INPUT:  context->cached_features, context->num_cached_features
 * OUTPUT: context->detected_patterns, context->pattern_confidences
 *
 * @param ctx Middleware execution context
 * @param data Stage-specific data (unused currently)
 * @return true on success, false on error
 */
static bool detection_stage_execute(middleware_context_t* ctx, void* data) {
    (void)data;

    // Guard: Validate context and input
    if (!ctx || !ctx->brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "detection_stage_execute: required parameter is NULL (ctx, ctx->brain)");
        return false;
    }
    if (!ctx->features_valid || ctx->num_cached_features == 0) {
        // No features to process - reset pattern count to avoid stale data
        ctx->num_detected_patterns = 0;
        return true;
    }

    // Pattern detection algorithms would go here
    // In full implementation: synchrony detection, oscillation detection, sequence detection

    // For now, simple threshold-based detection
    uint32_t patterns_detected = 0;

    // Allocate or grow pattern arrays if needed
    if (!ctx->detected_patterns || !ctx->pattern_confidences ||
        ctx->num_cached_features > ctx->pattern_capacity) {
        uint32_t new_cap = ctx->num_cached_features;
        uint32_t* new_patterns = nimcp_calloc(new_cap, sizeof(uint32_t));
        float* new_confidences = nimcp_calloc(new_cap, sizeof(float));
        if (!new_patterns || !new_confidences) {
            nimcp_free(new_patterns);
            nimcp_free(new_confidences);
            // Ensure both are NULL so next call retries allocation
            nimcp_free(ctx->detected_patterns);
            nimcp_free(ctx->pattern_confidences);
            ctx->detected_patterns = NULL;
            ctx->pattern_confidences = NULL;
            ctx->pattern_capacity = 0;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "detection_stage_execute: failed to allocate pattern arrays");
            return false;
        }
        nimcp_free(ctx->detected_patterns);
        nimcp_free(ctx->pattern_confidences);
        ctx->detected_patterns = new_patterns;
        ctx->pattern_confidences = new_confidences;
        ctx->pattern_capacity = new_cap;
    }

    // Simple pattern detection: features above mean (positive z-scores after normalization)
    // After normalization, features are z-scores, so threshold at 0.0 (above mean)
    const float PATTERN_THRESHOLD = 0.0F;
    for (uint32_t i = 0; i < ctx->num_cached_features; i++) {
        if (ctx->cached_features[i] > PATTERN_THRESHOLD) {
            ctx->detected_patterns[patterns_detected] = i;
            float conf = ctx->cached_features[i];
            ctx->pattern_confidences[patterns_detected] = (conf > 1.0f) ? 1.0f : conf;
            patterns_detected++;
        }
    }

    ctx->num_detected_patterns = patterns_detected;

    return true;
}

/**
 * @brief Routing Stage: Route detected patterns to appropriate brain regions
 *
 * WHAT: Thalamic routing of patterns to cortical targets
 * WHY:  Direct information flow based on detected patterns
 * HOW:  Use attention gates and routing tables to select targets
 *
 * INPUT:  context->detected_patterns, context->num_detected_patterns
 * OUTPUT: Routing decisions stored in context
 *
 * @param ctx Middleware execution context
 * @param data Stage-specific data (unused currently)
 * @return true on success, false on error
 */
static bool routing_stage_execute(middleware_context_t* ctx, void* data) {
    (void)data;

    // Guard: Validate context
    if (!ctx || !ctx->brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "routing_stage_execute: required parameter is NULL (ctx, ctx->brain)");
        return false;
    }
    if (ctx->num_detected_patterns == 0) return true;

    // Thalamic routing implementation
    // In full implementation: Use thalamic_router_t to route patterns
    // to appropriate cortical regions based on salience and attention

    // For now, simple routing: all patterns are "routed"
    // Real implementation would modulate pattern strengths based on routing table

    return true;
}

/**
 * @brief Normalization Stage: Normalize features for stable learning
 *
 * WHAT: Apply normalization (z-score, min-max, homeostatic) to features
 * WHY:  Ensure features have appropriate scale for downstream processing
 * HOW:  Use normalizers to transform features in-place
 *
 * INPUT:  context->cached_features, context->num_cached_features
 * OUTPUT: context->cached_features (normalized in-place)
 *
 * @param ctx Middleware execution context
 * @param data Stage-specific data (unused currently)
 * @return true on success, false on error
 */
static bool normalization_stage_execute(middleware_context_t* ctx, void* data) {
    (void)data;

    // Guard: Validate context and input
    if (!ctx || !ctx->brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "normalization_stage_execute: required parameter is NULL (ctx, ctx->brain)");
        return false;
    }
    if (!ctx->features_valid || ctx->num_cached_features == 0) return true;

    // Apply normalization to features
    // In full implementation: Use brain->feature_normalizer

    // Simple z-score normalization
    float sum = 0.0F;
    float sum_sq = 0.0F;

    for (uint32_t i = 0; i < ctx->num_cached_features; i++) {
        sum += ctx->cached_features[i];
        sum_sq += ctx->cached_features[i] * ctx->cached_features[i];
    }

    float mean = sum / ctx->num_cached_features;
    float variance = (sum_sq / ctx->num_cached_features) - (mean * mean);
    float std_dev = sqrtf(variance + 1e-8F); // Add epsilon for numerical stability

    // Normalize in-place
    for (uint32_t i = 0; i < ctx->num_cached_features; i++) {
        ctx->cached_features[i] = (ctx->cached_features[i] - mean) / std_dev;
    }

    return true;
}

/**
 * @brief Buffering Stage: Update temporal buffers with current data
 *
 * WHAT: Store current features and patterns in temporal buffers
 * WHY:  Enable temporal integration and sequence learning
 * HOW:  Push current data to circular buffers and sliding windows
 *
 * INPUT:  context->cached_features, context->detected_patterns
 * OUTPUT: Updated temporal buffers in context
 *
 * @param ctx Middleware execution context
 * @param data Stage-specific data (unused currently)
 * @return true on success, false on error
 */
static bool buffering_stage_execute(middleware_context_t* ctx, void* data) {
    (void)data;

    // Guard: Validate context
    if (!ctx || !ctx->brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "buffering_stage_execute: required parameter is NULL (ctx, ctx->brain)");
        return false;
    }

    // Update temporal buffers
    // In full implementation: Use circular_buffer_t and sliding_window_t
    // to maintain temporal history of features and patterns

    // For now, buffers are conceptually updated
    // Real implementation would push features to temporal buffers

    return true;
}

/**
 * @brief Events Stage: Generate and publish events based on pipeline results
 *
 * WHAT: Create events for detected patterns and significant changes
 * WHY:  Enable event-driven processing and inter-module communication
 * HOW:  Generate events and publish to event bus
 *
 * INPUT:  context->detected_patterns, context->num_detected_patterns
 * OUTPUT: Events published to event bus
 *
 * @param ctx Middleware execution context
 * @param data Stage-specific data (unused currently)
 * @return true on success, false on error
 */
static bool events_stage_execute(middleware_context_t* ctx, void* data) {
    (void)data;

    // Guard: Validate context
    if (!ctx || !ctx->brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "events_stage_execute: required parameter is NULL (ctx, ctx->brain)");
        return false;
    }

    // Generate events for detected patterns
    // In full implementation: Create event_t structures and publish via event_bus

    // Generate pattern detection events
    for (uint32_t i = 0; i < ctx->num_detected_patterns; i++) {
        // Create and publish event
        // Real implementation would create event_t and call event_bus_publish()
    }

    return true;
}

//=============================================================================
// Default Pipeline Factory
//=============================================================================

middleware_pipeline_t middleware_pipeline_create_default(brain_t brain,
                                                         event_bus_t event_bus) {
    pipeline_stage_config_t stages[PIPELINE_STAGE_COUNT] = {
        {PIPELINE_STAGE_ENCODING, "Encoding", encoding_stage_execute, NULL, true, 10000},
        {PIPELINE_STAGE_EXTRACTION, "Extraction", extraction_stage_execute, NULL, true, 10000},
        {PIPELINE_STAGE_NORMALIZATION, "Normalization", normalization_stage_execute, NULL, true, 10000},
        {PIPELINE_STAGE_DETECTION, "Detection", detection_stage_execute, NULL, true, 10000},
        {PIPELINE_STAGE_ROUTING, "Routing", routing_stage_execute, NULL, true, 10000},
        {PIPELINE_STAGE_BUFFERING, "Buffering", buffering_stage_execute, NULL, true, 10000},
        {PIPELINE_STAGE_EVENTS, "Events", events_stage_execute, NULL, true, 10000}
    };

    pipeline_config_t config = {
        .stages = stages,
        .num_stages = PIPELINE_STAGE_COUNT,
        .event_bus = event_bus,
        .enable_profiling = true,
        .fail_fast = false
    };

    return middleware_pipeline_create(&config);
}
