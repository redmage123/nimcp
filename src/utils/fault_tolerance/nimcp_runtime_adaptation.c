/**
 * @file nimcp_runtime_adaptation.c
 * @brief Runtime Parameter Adaptation System Implementation
 */

#include "utils/fault_tolerance/nimcp_runtime_adaptation.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "utils_runtime_adaptation"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(runtime_adaptation)

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/time.h>
#include "utils/memory/nimcp_unified_memory.h"

//=============================================================================
// Parameter Metadata Registry
//=============================================================================

static const parameter_info_t PARAMETER_REGISTRY[] = {
    {RUNTIME_PARAM_LEARNING_RATE, "learning_rate", "Learning rate", 0.0001F, 1.0F, 0.01F, 0.01F, true, ""},
    {RUNTIME_PARAM_BATCH_SIZE, "batch_size", "Batch size", 1.0F, 1000.0F, 32.0F, 32.0F, false, ""},
    {RUNTIME_PARAM_DROPOUT_RATE, "dropout_rate", "Dropout probability", 0.0F, 0.9F, 0.2F, 0.2F, false, ""},
    {RUNTIME_PARAM_GRADIENT_CLIP_VALUE, "gradient_clip", "Gradient clipping", 0.1F, 10.0F, 0.0F, 0.0F, false, ""},
    {RUNTIME_PARAM_TEMPERATURE, "temperature", "Neuron temperature", 0.1F, 10.0F, 1.0F, 1.0F, false, ""},
    // ... more parameters would be added here
};

#define PARAMETER_REGISTRY_SIZE (sizeof(PARAMETER_REGISTRY) / sizeof(parameter_info_t))

//=============================================================================
// Internal Structures
//=============================================================================

#define MAX_ADAPTATION_HISTORY 500

struct runtime_adaptation_context_internal {
    brain_t brain;                     /**< Associated brain */

    // Parameter values
    float parameters[RUNTIME_PARAM_COUNT];

    // Feature flags
    bool features[RUNTIME_FEATURE_COUNT];

    // History
    adaptation_history_t* history;
    uint32_t history_count;
    uint32_t history_capacity;
    uint32_t history_write_idx;

    // Statistics
    uint32_t total_adjustments;
    uint32_t successful_adjustments;
};

//=============================================================================
// Helper Functions
//=============================================================================

static uint64_t get_timestamp_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

static void initialize_default_parameters(runtime_adaptation_context_t ctx) {
    for (size_t i = 0; i < PARAMETER_REGISTRY_SIZE; i++) {
        runtime_parameter_t param = PARAMETER_REGISTRY[i].param_type;
        if (param < RUNTIME_PARAM_COUNT) {
            ctx->parameters[param] = PARAMETER_REGISTRY[i].default_value;
        }
    }
}

static void record_adaptation(
    runtime_adaptation_context_t ctx,
    runtime_parameter_t parameter,
    float old_value,
    float new_value,
    const char* reason
) {
    if (ctx->history_count < ctx->history_capacity) {
        adaptation_history_t* entry = &ctx->history[ctx->history_write_idx];

        entry->parameter = parameter;
        entry->old_value = old_value;
        entry->new_value = new_value;
        entry->timestamp_us = get_timestamp_us();
        strncpy(entry->reason, reason ? reason : "Unknown", sizeof(entry->reason) - 1);
        entry->was_successful = true;  // Assume success, update later if needed

        ctx->history_write_idx = (ctx->history_write_idx + 1) % ctx->history_capacity;
        if (ctx->history_count < ctx->history_capacity) {
            ctx->history_count++;
        }
    }
}

//=============================================================================
// Initialization & Lifecycle
//=============================================================================

runtime_adaptation_context_t runtime_adaptation_create(brain_t brain) {
    if (!brain) {
        LOG_ERROR("Cannot create runtime adaptation: NULL brain");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain is NULL");

        return NULL;
    }

    runtime_adaptation_context_t ctx = nimcp_calloc(1, sizeof(struct runtime_adaptation_context_internal));
    if (!ctx) {
        LOG_ERROR("Failed to allocate runtime adaptation context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ctx is NULL");

        return NULL;
    }

    ctx->brain = brain;

    // Initialize parameters to defaults
    initialize_default_parameters(ctx);

    // Initialize features (all disabled by default)
    memset(ctx->features, 0, sizeof(ctx->features));

    // Allocate history
    ctx->history_capacity = MAX_ADAPTATION_HISTORY;
    ctx->history = nimcp_calloc(ctx->history_capacity, sizeof(adaptation_history_t));
    if (!ctx->history) {
        LOG_ERROR("Failed to allocate adaptation history");
        nimcp_free(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "runtime_adaptation_create: ctx->history is NULL");
        return NULL;
    }

    LOG_INFO("Runtime adaptation context created");
    return ctx;
}

void runtime_adaptation_destroy(runtime_adaptation_context_t ctx) {
    if (!ctx) return;

    LOG_INFO("Destroying runtime adaptation (total_adjustments=%u)",
        ctx->total_adjustments);

    nimcp_free(ctx->history);
    nimcp_free(ctx);
}

//=============================================================================
// Parameter Adjustment API
//=============================================================================

bool runtime_adaptation_set_parameter(
    runtime_adaptation_context_t ctx,
    runtime_parameter_t param,
    float new_value,
    const char* reason
) {
    if (!ctx || param >= RUNTIME_PARAM_COUNT) {
        LOG_ERROR("Invalid parameters for set_parameter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "runtime_adaptation_set_parameter: ctx is NULL");
        return false;
    }

    // Find parameter info for validation
    const parameter_info_t* info = NULL;
    for (size_t i = 0; i < PARAMETER_REGISTRY_SIZE; i++) {
        if (PARAMETER_REGISTRY[i].param_type == param) {
            info = &PARAMETER_REGISTRY[i];
            break;
        }
    }

    if (info) {
        // Validate bounds
        if (new_value < info->min_value || new_value > info->max_value) {
            LOG_ERROR("Parameter value %.3f out of bounds [%.3f, %.3f]",
                new_value, info->min_value, info->max_value);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "runtime_adaptation_set_parameter: validation failed");
            return false;
        }
    }

    float old_value = ctx->parameters[param];
    ctx->parameters[param] = new_value;

    record_adaptation(ctx, param, old_value, new_value, reason);
    ctx->total_adjustments++;

    LOG_INFO("Adjusted parameter %d: %.3f -> %.3f (%s)",
        param, old_value, new_value, reason ? reason : "no reason");

    return true;
}

float runtime_adaptation_get_parameter(
    runtime_adaptation_context_t ctx,
    runtime_parameter_t param
) {
    if (!ctx || param >= RUNTIME_PARAM_COUNT) return -1.0F;
    return ctx->parameters[param];
}

bool runtime_adaptation_reset_parameter(
    runtime_adaptation_context_t ctx,
    runtime_parameter_t param
) {
    if (!ctx || param >= RUNTIME_PARAM_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "runtime_adaptation_reset_parameter: ctx is NULL");
        return false;
    }

    // Find default value
    for (size_t i = 0; i < PARAMETER_REGISTRY_SIZE; i++) {
        if (PARAMETER_REGISTRY[i].param_type == param) {
            ctx->parameters[param] = PARAMETER_REGISTRY[i].default_value;
            LOG_INFO("Reset parameter %d to default %.3f", param,
                PARAMETER_REGISTRY[i].default_value);
            return true;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "runtime_adaptation_reset_parameter: validation failed");
    return false;
}

uint32_t runtime_adaptation_reset_all(runtime_adaptation_context_t ctx) {
    if (!ctx) return 0;

    initialize_default_parameters(ctx);
    LOG_INFO("Reset all parameters to defaults");
    return RUNTIME_PARAM_COUNT;
}

//=============================================================================
// Feature Toggle API
//=============================================================================

bool runtime_adaptation_enable_feature(
    runtime_adaptation_context_t ctx,
    runtime_feature_t feature,
    const char* reason
) {
    if (!ctx || feature >= RUNTIME_FEATURE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "runtime_adaptation_enable_feature: ctx is NULL");
        return false;
    }

    ctx->features[feature] = true;
    LOG_INFO("Enabled feature %d: %s", feature, reason ? reason : "no reason");
    return true;
}

bool runtime_adaptation_disable_feature(
    runtime_adaptation_context_t ctx,
    runtime_feature_t feature,
    const char* reason
) {
    if (!ctx || feature >= RUNTIME_FEATURE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "runtime_adaptation_disable_feature: ctx is NULL");
        return false;
    }

    ctx->features[feature] = false;
    LOG_INFO("Disabled feature %d: %s", feature, reason ? reason : "no reason");
    return true;
}

bool runtime_adaptation_is_feature_enabled(
    runtime_adaptation_context_t ctx,
    runtime_feature_t feature
) {
    if (!ctx || feature >= RUNTIME_FEATURE_COUNT) {
        return false;
    }
    return ctx->features[feature];
}

//=============================================================================
// Batch Adjustment API
//=============================================================================

bool runtime_adaptation_apply_batch(
    runtime_adaptation_context_t ctx,
    parameter_change_t* changes,
    uint32_t num_changes,
    const char* reason
) {
    if (!ctx || !changes || num_changes == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "runtime_adaptation_apply_batch: required parameter is NULL (ctx, changes)");
        return false;
    }

    // Validate all changes first
    for (uint32_t i = 0; i < num_changes; i++) {
        if (changes[i].param >= RUNTIME_PARAM_COUNT) {
            LOG_ERROR("Invalid parameter in batch change");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "runtime_adaptation_apply_batch: capacity exceeded");
            return false;
        }
    }

    // Apply all changes
    for (uint32_t i = 0; i < num_changes; i++) {
        if (!runtime_adaptation_set_parameter(ctx, changes[i].param,
                                             changes[i].value, reason)) {
            LOG_WARNING("Failed to apply batch change %u", i);
        }
    }

    LOG_INFO("Applied batch of %u parameter changes", num_changes);
    return true;
}

//=============================================================================
// Automated Adaptation Policies
//=============================================================================

bool runtime_adaptation_policy_nan_detected(runtime_adaptation_context_t ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "runtime_adaptation_policy_nan_detected: ctx is NULL");
        return false;
    }

    LOG_INFO("Applying NaN detection policy");

    parameter_change_t changes[] = {
        {RUNTIME_PARAM_LEARNING_RATE, ctx->parameters[RUNTIME_PARAM_LEARNING_RATE] * 0.5F},
        {RUNTIME_PARAM_GRADIENT_CLIP_VALUE, 1.0F},
    };

    bool success = runtime_adaptation_apply_batch(ctx, changes, 2,
        "NaN detected - reducing LR and enabling clipping");

    runtime_adaptation_enable_feature(ctx, RUNTIME_FEATURE_NAN_DETECTION,
        "Enable NaN checks");

    return success;
}

bool runtime_adaptation_policy_memory_pressure(runtime_adaptation_context_t ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "runtime_adaptation_policy_memory_pressure: ctx is NULL");
        return false;
    }

    LOG_INFO("Applying memory pressure policy");

    parameter_change_t changes[] = {
        {RUNTIME_PARAM_BATCH_SIZE, ctx->parameters[RUNTIME_PARAM_BATCH_SIZE] * 0.5F},
    };

    bool success = runtime_adaptation_apply_batch(ctx, changes, 1,
        "Memory pressure - reducing batch size");

    runtime_adaptation_enable_feature(ctx, RUNTIME_FEATURE_MEMORY_COMPACTION,
        "Enable memory compaction");

    return success;
}

bool runtime_adaptation_policy_gradient_explosion(runtime_adaptation_context_t ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "runtime_adaptation_policy_gradient_explosion: ctx is NULL");
        return false;
    }

    LOG_INFO("Applying gradient explosion policy");

    parameter_change_t changes[] = {
        {RUNTIME_PARAM_LEARNING_RATE, ctx->parameters[RUNTIME_PARAM_LEARNING_RATE] * 0.25F},
        {RUNTIME_PARAM_GRADIENT_CLIP_VALUE, 1.0F},
    };

    return runtime_adaptation_apply_batch(ctx, changes, 2,
        "Gradient explosion - aggressive LR reduction and clipping");
}

bool runtime_adaptation_policy_slow_convergence(runtime_adaptation_context_t ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "runtime_adaptation_policy_slow_convergence: ctx is NULL");
        return false;
    }

    LOG_INFO("Applying slow convergence policy");

    parameter_change_t changes[] = {
        {RUNTIME_PARAM_LEARNING_RATE, ctx->parameters[RUNTIME_PARAM_LEARNING_RATE] * 1.2F},
    };

    return runtime_adaptation_apply_batch(ctx, changes, 1,
        "Slow convergence - increasing learning rate");
}

bool runtime_adaptation_policy_overfitting(runtime_adaptation_context_t ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "runtime_adaptation_policy_overfitting: ctx is NULL");
        return false;
    }

    LOG_INFO("Applying overfitting policy");

    parameter_change_t changes[] = {
        {RUNTIME_PARAM_DROPOUT_RATE, 0.3F},
        {RUNTIME_PARAM_WEIGHT_DECAY, 0.001F},
    };

    return runtime_adaptation_apply_batch(ctx, changes, 2,
        "Overfitting - enabling regularization");
}

//=============================================================================
// History & Analytics
//=============================================================================

bool runtime_adaptation_get_param_info(
    runtime_parameter_t param,
    parameter_info_t* info
) {
    if (!info || param >= RUNTIME_PARAM_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "runtime_adaptation_get_param_info: info is NULL");
        return false;
    }

    for (size_t i = 0; i < PARAMETER_REGISTRY_SIZE; i++) {
        if (PARAMETER_REGISTRY[i].param_type == param) {
            *info = PARAMETER_REGISTRY[i];
            return true;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "runtime_adaptation_get_param_info: validation failed");
    return false;
}

uint32_t runtime_adaptation_get_history(
    runtime_adaptation_context_t ctx,
    adaptation_history_t* history,
    uint32_t max_entries
) {
    if (!ctx || !history || max_entries == 0) return 0;

    uint32_t count = (ctx->history_count < max_entries) ?
                     ctx->history_count : max_entries;

    memcpy(history, ctx->history, count * sizeof(adaptation_history_t));
    return count;
}

const char* runtime_adaptation_param_name(runtime_parameter_t param) {
    for (size_t i = 0; i < PARAMETER_REGISTRY_SIZE; i++) {
        if (PARAMETER_REGISTRY[i].param_type == param) {
            return PARAMETER_REGISTRY[i].name;
        }
    }
    return "unknown";
}

const char* runtime_adaptation_feature_name(runtime_feature_t feature) {
    static const char* names[] = {
        "dropout", "batch_norm", "gradient_clipping", "weight_clipping",
        "layer_freezing", "plasticity", "homeostasis", "neuromodulation",
        "memory_compaction", "prefetching", "caching", "checkpointing",
        "debug_logging", "nan_detection", "bounds_checking"
    };

    if (feature < RUNTIME_FEATURE_COUNT) {
        return names[feature];
    }
    return "unknown";
}

//=============================================================================
// Persistence
//=============================================================================

bool runtime_adaptation_save_config(
    runtime_adaptation_context_t ctx,
    const char* filepath
) {
    if (!ctx || !filepath) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "runtime_adaptation_save_config: required parameter is NULL (ctx, filepath)");
        return false;
    }

    FILE* f = fopen(filepath, "wb");
    if (!f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "runtime_adaptation_save_config: f is NULL");
        return false;
    }

    fwrite(&ctx->parameters, sizeof(float), RUNTIME_PARAM_COUNT, f);
    fwrite(&ctx->features, sizeof(bool), RUNTIME_FEATURE_COUNT, f);

    fclose(f);
    LOG_INFO("Saved runtime configuration to %s", filepath);
    return true;
}

bool runtime_adaptation_load_config(
    runtime_adaptation_context_t ctx,
    const char* filepath
) {
    if (!ctx || !filepath) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "runtime_adaptation_load_config: required parameter is NULL (ctx, filepath)");
        return false;
    }

    FILE* f = fopen(filepath, "rb");
    if (!f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "runtime_adaptation_load_config: f is NULL");
        return false;
    }

    /* Track short reads so a truncated file is logged rather than silently
     * restoring partial state. */
    size_t got_params = fread(&ctx->parameters, sizeof(float),
                               RUNTIME_PARAM_COUNT, f);
    size_t got_features = fread(&ctx->features, sizeof(bool),
                                 RUNTIME_FEATURE_COUNT, f);
    if (got_params != RUNTIME_PARAM_COUNT ||
        got_features != RUNTIME_FEATURE_COUNT) {
        LOG_WARN("runtime_adaptation_load_config: truncated file %s "
                 "(params %zu/%d, features %zu/%d)",
                 filepath, got_params, RUNTIME_PARAM_COUNT,
                 got_features, RUNTIME_FEATURE_COUNT);
    }

    fclose(f);
    LOG_INFO("Loaded runtime configuration from %s", filepath);
    return true;
}

//=============================================================================
// Reporting & Debugging
//=============================================================================

void runtime_adaptation_report(
    runtime_adaptation_context_t ctx,
    FILE* output
) {
    if (!ctx || !output) return;

    fprintf(output, "\n=== Runtime Adaptation Report ===\n\n");
    fprintf(output, "Total adjustments: %u\n\n", ctx->total_adjustments);

    fprintf(output, "Active Parameters:\n");
    for (size_t i = 0; i < PARAMETER_REGISTRY_SIZE && i < 10; i++) {
        runtime_parameter_t param = PARAMETER_REGISTRY[i].param_type;
        fprintf(output, "  %s: %.4f\n",
            PARAMETER_REGISTRY[i].name,
            ctx->parameters[param]);
    }

    fprintf(output, "\nEnabled Features:\n");
    for (runtime_feature_t f = 0; f < RUNTIME_FEATURE_COUNT; f++) {
        if (ctx->features[f]) {
            fprintf(output, "  - %s\n", runtime_adaptation_feature_name(f));
        }
    }

    fprintf(output, "\n");
}

int32_t runtime_adaptation_export_json(
    runtime_adaptation_context_t ctx,
    char* json_buffer,
    size_t buffer_size
) {
    if (!ctx || !json_buffer || buffer_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "runtime_adaptation_export_json: required parameter is NULL (ctx, json_buffer)");
        return -1;
    }

    int written = snprintf(json_buffer, buffer_size,
        "{\n"
        "  \"total_adjustments\": %u,\n"
        "  \"learning_rate\": %.6f,\n"
        "  \"batch_size\": %.0f,\n"
        "  \"dropout_rate\": %.3f\n"
        "}",
        ctx->total_adjustments,
        ctx->parameters[RUNTIME_PARAM_LEARNING_RATE],
        ctx->parameters[RUNTIME_PARAM_BATCH_SIZE],
        ctx->parameters[RUNTIME_PARAM_DROPOUT_RATE]
    );

    return (written > 0 && (size_t)written < buffer_size) ? written : -1;
}
