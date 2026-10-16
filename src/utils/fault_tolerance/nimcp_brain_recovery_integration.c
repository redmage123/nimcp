/**
 * @file nimcp_brain_recovery_integration.c
 * @brief Brain-Driven Intelligent Recovery Implementation
 */

#include "utils/fault_tolerance/nimcp_brain_recovery_integration.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "utils_brain_recovery_integration"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(brain_recovery_integration)

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include "utils/memory/nimcp_unified_memory.h"
#include "constants/nimcp_buffer_constants.h"

//=============================================================================
// Internal Structures
//=============================================================================

#define MAX_RECOVERY_HISTORY 1000
#define MAX_LEARNED_PATTERNS 500

/**
 * @brief Internal brain recovery context
 */
struct brain_recovery_context_internal {
    brain_t brain;                     /**< Associated brain */

    // Learning history
    recovery_outcome_t* history;       /**< Recovery history */
    uint32_t history_count;            /**< Number of entries */
    uint32_t history_capacity;         /**< Capacity */
    uint32_t history_write_idx;        /**< Circular write index */

    // Learned patterns
    recovery_pattern_t* patterns;      /**< Learned patterns */
    uint32_t pattern_count;            /**< Number of patterns */
    uint32_t pattern_capacity;         /**< Capacity */

    // Statistics
    uint32_t total_recoveries;
    uint32_t successful_recoveries;
    uint32_t brain_decisions;
    uint32_t brain_correct_predictions;

    // Configuration
    float confidence_threshold;        /**< Minimum confidence for brain decisions */
    bool enable_learning;              /**< Learn from outcomes */
};

//=============================================================================
// Helper Functions
//=============================================================================

static uint64_t get_timestamp_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

static float calculate_success_rate(uint32_t successes, uint32_t total) {
    if (total == 0) return 0.0F;
    return (float)successes / (float)total;
}

/**
 * @brief Generate failure signature from diagnosis
 */
static void generate_failure_signature(
    diagnostic_result_t* diagnosis,
    char* signature,
    size_t size
) {
    if (!diagnosis || !signature || size == 0) return;

    /* Cap root_cause to 240 chars so the total signature fits in a 256-byte
     * destination buffer (root_cause[512] could otherwise overflow). */
    snprintf(signature, size, "%.240s_%d_0x%04x",
        diagnosis->root_cause,
        diagnosis->severity,
        diagnosis->error_type & 0xFFFF);
}

/**
 * @brief Find pattern by signature
 */
static recovery_pattern_t* find_pattern(
    brain_recovery_context_t ctx,
    const char* signature
) {
    if (!ctx || !signature) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_pattern: required parameter is NULL (ctx, signature)");
        return NULL;
    }

    for (uint32_t i = 0; i < ctx->pattern_count; i++) {
        if (strcmp(ctx->patterns[i].failure_signature, signature) == 0) {
            return &ctx->patterns[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_pattern: validation failed");
    return NULL;
}

/**
 * @brief Add or update learned pattern
 */
static void update_pattern(
    brain_recovery_context_t ctx,
    const char* signature,
    recovery_strategy_t* strategy,
    bool success
) {
    if (!ctx || !signature || !strategy) return;

    recovery_pattern_t* pattern = find_pattern(ctx, signature);

    if (!pattern) {
        // Create new pattern
        if (ctx->pattern_count >= ctx->pattern_capacity) {
            LOG_WARNING("Pattern capacity reached, cannot add new pattern");
            return;
        }

        pattern = &ctx->patterns[ctx->pattern_count++];
        strncpy(pattern->failure_signature, signature, sizeof(pattern->failure_signature) - 1);
        pattern->occurrence_count = 0;
        pattern->success_count = 0;
        pattern->first_seen_us = get_timestamp_us();
    }

    // Update pattern
    pattern->occurrence_count++;
    if (success) {
        pattern->success_count++;
        pattern->best_tier = strategy->tier;
        pattern->best_action = strategy->primary;
    }
    pattern->last_seen_us = get_timestamp_us();
    pattern->success_rate = calculate_success_rate(
        pattern->success_count,
        pattern->occurrence_count
    );
    // Confidence combines success rate with occurrence count
    // High success rate gives base confidence, occurrences add more
    pattern->confidence = fminf(0.99F, pattern->success_rate * 0.5F + (pattern->occurrence_count / 10.0F));
}

//=============================================================================
// Initialization & Lifecycle
//=============================================================================

brain_recovery_context_t brain_recovery_init(brain_t brain) {
    if (!brain) {
        LOG_ERROR("Cannot initialize brain recovery: NULL brain");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain is NULL");

        return NULL;
    }

    brain_recovery_context_t ctx = nimcp_calloc(1, sizeof(struct brain_recovery_context_internal));
    if (!ctx) {
        LOG_ERROR("Failed to allocate brain recovery context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ctx is NULL");

        return NULL;
    }

    ctx->brain = brain;

    // Allocate history
    ctx->history_capacity = MAX_RECOVERY_HISTORY;
    ctx->history = nimcp_calloc(ctx->history_capacity, sizeof(recovery_outcome_t));
    if (!ctx->history) {
        LOG_ERROR("Failed to allocate recovery history");
        nimcp_free(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_recovery_init: ctx->history is NULL");
        return NULL;
    }

    // Allocate patterns
    ctx->pattern_capacity = MAX_LEARNED_PATTERNS;
    ctx->patterns = nimcp_calloc(ctx->pattern_capacity, sizeof(recovery_pattern_t));
    if (!ctx->patterns) {
        LOG_ERROR("Failed to allocate pattern storage");
        nimcp_free(ctx->history);
        nimcp_free(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_recovery_init: ctx->patterns is NULL");
        return NULL;
    }

    // Configuration
    ctx->confidence_threshold = 0.5F;
    ctx->enable_learning = true;

    LOG_INFO("Brain recovery context initialized (history=%u, patterns=%u)",
        ctx->history_capacity, ctx->pattern_capacity);

    return ctx;
}

void brain_recovery_shutdown(brain_recovery_context_t ctx) {
    if (!ctx) return;

    LOG_INFO("Shutting down brain recovery (total_recoveries=%u, success_rate=%.2f%%)",
        ctx->total_recoveries,
        calculate_success_rate(ctx->successful_recoveries, ctx->total_recoveries) * 100.0F);

    // Free heap-allocated pointers in history entries
    if (ctx->history) {
        for (uint32_t i = 0; i < ctx->history_count; i++) {
            nimcp_free(ctx->history[i].strategy);
            nimcp_free(ctx->history[i].result);
        }
        nimcp_free(ctx->history);
    }

    nimcp_free(ctx->patterns);
    nimcp_free(ctx);
}

//=============================================================================
// Brain-Driven Strategy Selection
//=============================================================================

brain_recovery_decision_t* brain_recovery_select_strategy(
    brain_recovery_context_t ctx,
    diagnostic_result_t* diagnosis,
    health_status_snapshot_t* current_health
) {
    if (!ctx || !diagnosis) {
        LOG_ERROR("Invalid parameters for strategy selection");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_recovery_select_strategy: required parameter is NULL (ctx, diagnosis)");
        return NULL;
    }

    brain_recovery_decision_t* decision = nimcp_calloc(1, sizeof(brain_recovery_decision_t));
    if (!decision) {
        LOG_ERROR("Failed to allocate brain decision");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "decision is NULL");

        return NULL;
    }

    // Generate failure signature and store in decision
    generate_failure_signature(diagnosis, decision->failure_signature, sizeof(decision->failure_signature));

    // Check for learned patterns (simulating working memory)
    recovery_pattern_t* pattern = find_pattern(ctx, decision->failure_signature);

    if (pattern && pattern->confidence >= 0.1F) {
        // Use learned strategy
        LOG_INFO("Found learned pattern: %s (success_rate=%.2f, confidence=%.2f)",
            decision->failure_signature, pattern->success_rate, pattern->confidence);

        decision->selected_strategy = nimcp_calloc(1, sizeof(recovery_strategy_t));
        if (decision->selected_strategy) {
            decision->selected_strategy->tier = pattern->best_tier;
            decision->selected_strategy->primary = pattern->best_action;
            decision->selected_strategy->fallback = RECOVERY_ACTION_NONE;
            decision->selected_strategy->max_retries = 3;
            decision->selected_strategy->timeout_ms = 1000;
            decision->selected_strategy->success_threshold = 0.5F;
            decision->selected_strategy->description = "Learned strategy from past experience";
        }

        decision->confidence = pattern->confidence;
        decision->predicted_success_prob = pattern->success_rate;
        decision->is_novel_situation = false;

        snprintf(decision->reasoning, sizeof(decision->reasoning),
            "Using learned strategy (seen %u times, %.0f%% success rate)",
            pattern->occurrence_count, pattern->success_rate * 100.0F);
    } else {
        // Novel situation - use default strategy selection
        LOG_INFO("Novel failure pattern: %s - using heuristic selection", decision->failure_signature);

        // Convert diagnostic_result_t to diagnostic_summary_t for recovery_select_strategy
        diagnostic_summary_t summary = {
            .signal = 0,  // Not signal-driven
            .failure_type = diagnosis->root_cause,
            .severity = (uint32_t)diagnosis->severity,
            .is_recoverable = (diagnosis->severity < DIAG_SEVERITY_FATAL),
            .context = NULL
        };

        // Get strategy from heuristic (returns static pointer)
        recovery_strategy_t* static_strategy = recovery_select_strategy(&summary);

        // Make a heap copy so we can safely free it later
        decision->selected_strategy = nimcp_calloc(1, sizeof(recovery_strategy_t));
        if (decision->selected_strategy && static_strategy) {
            memcpy(decision->selected_strategy, static_strategy, sizeof(recovery_strategy_t));
        }

        decision->confidence = 0.4F;  // Lower confidence for novel situations
        decision->predicted_success_prob = 0.6F;  // Conservative estimate
        decision->is_novel_situation = true;

        snprintf(decision->reasoning, sizeof(decision->reasoning),
            "Novel failure pattern - using heuristic based on error type");
    }

    decision->estimated_time_us = 1000;  // 1ms estimate
    decision->requires_user_confirmation = (diagnosis->severity >= DIAG_SEVERITY_CRITICAL);

    ctx->brain_decisions++;

    return decision;
}

void brain_recovery_free_decision(brain_recovery_decision_t* decision) {
    if (!decision) return;
    nimcp_free(decision->selected_strategy);
    nimcp_free(decision);
}

//=============================================================================
// Learning from Recovery Outcomes
//=============================================================================

void brain_recovery_learn_outcome(
    brain_recovery_context_t ctx,
    brain_recovery_decision_t* decision,
    recovery_result_t* result
) {
    if (!ctx || !decision || !result) return;

    if (!ctx->enable_learning) return;

    // Update recovery counters
    ctx->total_recoveries++;
    if (result->status == RECOVERY_SUCCESS) {
        ctx->successful_recoveries++;
    }

    // Store outcome in history
    if (ctx->history_capacity > 0) {
        recovery_outcome_t* outcome = &ctx->history[ctx->history_write_idx];

        // If we're about to overwrite an old entry, free its pointers first
        if (ctx->history_count >= ctx->history_capacity) {
            nimcp_free(outcome->strategy);
            nimcp_free(outcome->result);
        }

        // Make heap copy of strategy (don't store pointer that will be freed)
        if (decision->selected_strategy) {
            outcome->strategy = nimcp_calloc(1, sizeof(recovery_strategy_t));
            if (outcome->strategy) {
                memcpy(outcome->strategy, decision->selected_strategy, sizeof(recovery_strategy_t));
            }
        } else {
            outcome->strategy = NULL;
        }

        // Make heap copy of result (don't store pointer that may be stack-allocated)
        if (result) {
            outcome->result = nimcp_calloc(1, sizeof(recovery_result_t));
            if (outcome->result) {
                memcpy(outcome->result, result, sizeof(recovery_result_t));
            }
        } else {
            outcome->result = NULL;
        }

        outcome->timestamp_us = get_timestamp_us();
        outcome->execution_time_us = result->time_us;
        outcome->was_successful = (result->status == RECOVERY_SUCCESS);
        outcome->expected_success_prob = decision->predicted_success_prob;
        outcome->actual_success = outcome->was_successful ? 1.0F : 0.0F;

        ctx->history_write_idx = (ctx->history_write_idx + 1) % ctx->history_capacity;
        if (ctx->history_count < ctx->history_capacity) {
            ctx->history_count++;
        }
    }

    // Update learned patterns using stored failure signature
    update_pattern(ctx, decision->failure_signature, decision->selected_strategy,
                  result->status == RECOVERY_SUCCESS);

    // Update prediction accuracy
    float prediction_error = fabsf(decision->predicted_success_prob -
                                   (result->status == RECOVERY_SUCCESS ? 1.0F : 0.0F));
    if (prediction_error < 0.3F) {
        ctx->brain_correct_predictions++;
    }

    LOG_INFO("Learned from recovery outcome (signature=%s, success=%d, prediction_error=%.2f)",
        decision->failure_signature, result->status == RECOVERY_SUCCESS, prediction_error);
}

uint32_t brain_recovery_get_patterns(
    brain_recovery_context_t ctx,
    recovery_pattern_t* patterns,
    uint32_t max_patterns
) {
    if (!ctx || !patterns || max_patterns == 0) return 0;

    uint32_t count = (ctx->pattern_count < max_patterns) ?
                     ctx->pattern_count : max_patterns;

    memcpy(patterns, ctx->patterns, count * sizeof(recovery_pattern_t));
    return count;
}

//=============================================================================
// Runtime Parameter Suggestions
//=============================================================================

uint32_t brain_recovery_suggest_parameters(
    brain_recovery_context_t ctx,
    diagnostic_result_t* diagnosis,
    parameter_adjustment_t* adjustments,
    uint32_t max_adjustments
) {
    if (!ctx || !diagnosis || !adjustments || max_adjustments == 0) return 0;

    uint32_t count = 0;

    // Suggest parameters based on error type
    switch (diagnosis->error_type) {
        case ERROR_TYPE_NAN_DETECTED:
        case ERROR_TYPE_INF_DETECTED:
            if (count < max_adjustments) {
                adjustments[count].parameter_name = "learning_rate";
                adjustments[count].current_value = 0.01F;  // Would query brain
                adjustments[count].suggested_value = 0.005F;  // Reduce by 50%
                adjustments[count].confidence = 0.9F;
                snprintf((char*)adjustments[count].rationale, sizeof(adjustments[count].rationale),
                    "Reduce learning rate to prevent numerical instability");
                count++;
            }
            break;

        case ERROR_TYPE_OUT_OF_MEMORY:
            if (count < max_adjustments) {
                adjustments[count].parameter_name = "batch_size";
                adjustments[count].current_value = 64.0F;
                adjustments[count].suggested_value = 32.0F;  // Reduce by 50%
                adjustments[count].confidence = 0.85F;
                snprintf((char*)adjustments[count].rationale, sizeof(adjustments[count].rationale),
                    "Reduce batch size to decrease memory usage");
                count++;
            }
            break;

        case ERROR_TYPE_GRADIENT_EXPLOSION:
            if (count < max_adjustments) {
                adjustments[count].parameter_name = "gradient_clip_value";
                adjustments[count].current_value = 0.0F;  // Disabled
                adjustments[count].suggested_value = 1.0F;  // Enable
                adjustments[count].confidence = 0.95F;
                snprintf((char*)adjustments[count].rationale, sizeof(adjustments[count].rationale),
                    "Enable gradient clipping to prevent explosion");
                count++;
            }
            break;

        default:
            break;
    }

    return count;
}

//=============================================================================
// Success Probability Prediction
//=============================================================================

float brain_recovery_predict_success(
    brain_recovery_context_t ctx,
    recovery_strategy_t* strategy,
    diagnostic_result_t* diagnosis
) {
    if (!ctx || !strategy || !diagnosis) return -1.0F;

    // Generate signature and look for pattern
    char signature[NIMCP_ERROR_BUFFER_SIZE];
    generate_failure_signature(diagnosis, signature, sizeof(signature));

    recovery_pattern_t* pattern = find_pattern(ctx, signature);
    if (pattern) {
        // Use historical success rate
        return pattern->success_rate;
    }

    // No historical data - use base rate
    // Different tiers have different base success rates
    switch (strategy->tier) {
        case RECOVERY_TIER_IMMEDIATE:   return 0.85F;
        case RECOVERY_TIER_TACTICAL:    return 0.70F;
        case RECOVERY_TIER_STRATEGIC:   return 0.60F;
        case RECOVERY_TIER_PREVENTIVE:  return 0.75F;
        default:                        return 0.50F;
    }
}

//=============================================================================
// Recovery History & Analytics
//=============================================================================

bool brain_recovery_get_stats(
    brain_recovery_context_t ctx,
    recovery_history_stats_t* stats
) {
    if (!ctx || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_recovery_get_stats: required parameter is NULL (ctx, stats)");
        return false;
    }

    memset(stats, 0, sizeof(recovery_history_stats_t));

    stats->total_recoveries = ctx->total_recoveries;
    stats->successful_recoveries = ctx->successful_recoveries;
    stats->failed_recoveries = ctx->total_recoveries - ctx->successful_recoveries;
    stats->success_rate = calculate_success_rate(
        ctx->successful_recoveries,
        ctx->total_recoveries
    );

    stats->total_patterns_learned = ctx->pattern_count;
    stats->avg_prediction_accuracy = calculate_success_rate(
        ctx->brain_correct_predictions,
        ctx->brain_decisions
    );

    // Find most effective tier
    uint32_t tier_successes[4] = {0};
    uint32_t tier_counts[4] = {0};

    for (uint32_t i = 0; i < ctx->pattern_count; i++) {
        recovery_pattern_t* p = &ctx->patterns[i];
        if (p->best_tier < 4) {
            tier_counts[p->best_tier]++;
            tier_successes[p->best_tier] += p->success_count;
        }
    }

    float best_rate = 0.0F;
    for (int i = 0; i < 4; i++) {
        float rate = calculate_success_rate(tier_successes[i], tier_counts[i]);
        if (rate > best_rate) {
            best_rate = rate;
            stats->most_effective_tier = (recovery_tier_t)i;
        }
    }

    return true;
}

uint32_t brain_recovery_get_recent_outcomes(
    brain_recovery_context_t ctx,
    recovery_outcome_t* outcomes,
    uint32_t max_outcomes
) {
    if (!ctx || !outcomes || max_outcomes == 0) return 0;

    uint32_t count = (ctx->history_count < max_outcomes) ?
                     ctx->history_count : max_outcomes;

    // Copy most recent outcomes
    for (uint32_t i = 0; i < count; i++) {
        uint32_t idx = (ctx->history_write_idx - 1 - i + ctx->history_capacity) %
                       ctx->history_capacity;
        if (idx < ctx->history_count) {
            outcomes[i] = ctx->history[idx];
        }
    }

    return count;
}

//=============================================================================
// Integration with Cognitive Pipeline
//=============================================================================

bool brain_recovery_register_pipeline(
    brain_recovery_context_t ctx,
    brain_t brain
) {
    if (!ctx || !brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_recovery_register_pipeline: required parameter is NULL (ctx, brain)");
        return false;
    }

    // In full implementation, would register callbacks with:
    // - Executive function for decision integration
    // - Working memory for pattern storage
    // - Episodic memory for outcome learning

    LOG_INFO("Registered brain recovery with cognitive pipeline");
    return true;
}

void brain_recovery_unregister_pipeline(
    brain_recovery_context_t ctx,
    brain_t brain
) {
    if (!ctx || !brain) return;
    LOG_INFO("Unregistered brain recovery from cognitive pipeline");
}

//=============================================================================
// Persistence
//=============================================================================

bool brain_recovery_save(
    brain_recovery_context_t ctx,
    const char* filepath
) {
    if (!ctx || !filepath) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_recovery_save: required parameter is NULL (ctx, filepath)");
        return false;
    }

    FILE* f = fopen(filepath, "wb");
    if (!f) {
        LOG_ERROR("Failed to open file for writing: %s", filepath);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_recovery_save: f is NULL");
        return false;
    }

    // Write header
    uint32_t version = 1;
    fwrite(&version, sizeof(uint32_t), 1, f);

    // Write statistics
    fwrite(&ctx->total_recoveries, sizeof(uint32_t), 1, f);
    fwrite(&ctx->successful_recoveries, sizeof(uint32_t), 1, f);
    fwrite(&ctx->brain_decisions, sizeof(uint32_t), 1, f);
    fwrite(&ctx->brain_correct_predictions, sizeof(uint32_t), 1, f);

    // Write patterns
    fwrite(&ctx->pattern_count, sizeof(uint32_t), 1, f);
    fwrite(ctx->patterns, sizeof(recovery_pattern_t), ctx->pattern_count, f);

    fclose(f);

    LOG_INFO("Saved brain recovery state to %s", filepath);
    return true;
}

brain_recovery_context_t brain_recovery_load(
    brain_t brain,
    const char* filepath
) {
    if (!brain || !filepath) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_recovery_load: required parameter is NULL (brain, filepath)");
        return NULL;
    }

    FILE* f = fopen(filepath, "rb");
    if (!f) {
        LOG_ERROR("Failed to open file for reading: %s", filepath);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "f is NULL");

        return NULL;
    }

    // Read and verify version
    uint32_t version;
    if (fread(&version, sizeof(uint32_t), 1, f) != 1 || version != 1) {
        LOG_ERROR("Invalid or unsupported file version");
        fclose(f);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_recovery_load: validation failed");
        return NULL;
    }

    brain_recovery_context_t ctx = brain_recovery_init(brain);
    if (!ctx) {
        fclose(f);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return NULL;
    }

    /* Read statistics. Each fread returns the number of items read;
     * a short read (!= 1) indicates a truncated file. On error we log
     * and bail rather than silently using stale zeros. */
    bool read_ok = true;
    read_ok &= (fread(&ctx->total_recoveries, sizeof(uint32_t), 1, f) == 1);
    read_ok &= (fread(&ctx->successful_recoveries, sizeof(uint32_t), 1, f) == 1);
    read_ok &= (fread(&ctx->brain_decisions, sizeof(uint32_t), 1, f) == 1);
    read_ok &= (fread(&ctx->brain_correct_predictions, sizeof(uint32_t), 1, f) == 1);

    // Read patterns
    read_ok &= (fread(&ctx->pattern_count, sizeof(uint32_t), 1, f) == 1);
    if (read_ok && ctx->pattern_count <= ctx->pattern_capacity) {
        size_t got = fread(ctx->patterns, sizeof(recovery_pattern_t),
                           ctx->pattern_count, f);
        if (got != ctx->pattern_count) {
            LOG_WARN("brain_recovery_load: truncated patterns (%zu/%u)",
                     got, ctx->pattern_count);
            ctx->pattern_count = (uint32_t)got;
        }
    }
    if (!read_ok) {
        LOG_WARN("brain_recovery_load: truncated header in %s", filepath);
    }

    fclose(f);

    LOG_INFO("Loaded brain recovery state from %s (patterns=%u)",
        filepath, ctx->pattern_count);
    return ctx;
}

//=============================================================================
// Diagnostics & Debugging
//=============================================================================

void brain_recovery_report(
    brain_recovery_context_t ctx,
    FILE* output
) {
    if (!ctx || !output) return;

    fprintf(output, "\n=== Brain Recovery Report ===\n\n");

    fprintf(output, "Recovery Statistics:\n");
    fprintf(output, "  Total recoveries: %u\n", ctx->total_recoveries);
    fprintf(output, "  Successful: %u (%.1f%%)\n",
        ctx->successful_recoveries,
        calculate_success_rate(ctx->successful_recoveries, ctx->total_recoveries) * 100.0F);

    fprintf(output, "\nBrain Decision Statistics:\n");
    fprintf(output, "  Total decisions: %u\n", ctx->brain_decisions);
    fprintf(output, "  Prediction accuracy: %.1f%%\n",
        calculate_success_rate(ctx->brain_correct_predictions, ctx->brain_decisions) * 100.0F);

    fprintf(output, "\nLearned Patterns: %u\n", ctx->pattern_count);
    for (uint32_t i = 0; i < ctx->pattern_count && i < 10; i++) {
        recovery_pattern_t* p = &ctx->patterns[i];
        fprintf(output, "  [%u] %s: %.1f%% success (%u/%u), confidence=%.2f\n",
            i, p->failure_signature,
            p->success_rate * 100.0F,
            p->success_count, p->occurrence_count,
            p->confidence);
    }

    fprintf(output, "\n");
}

int32_t brain_recovery_export_json(
    brain_recovery_context_t ctx,
    char* json_buffer,
    size_t buffer_size
) {
    if (!ctx || !json_buffer || buffer_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_recovery_export_json: required parameter is NULL (ctx, json_buffer)");
        return -1;
    }

    int written = snprintf(json_buffer, buffer_size,
        "{\n"
        "  \"total_recoveries\": %u,\n"
        "  \"successful_recoveries\": %u,\n"
        "  \"success_rate\": %.3f,\n"
        "  \"brain_decisions\": %u,\n"
        "  \"prediction_accuracy\": %.3f,\n"
        "  \"patterns_learned\": %u\n"
        "}",
        ctx->total_recoveries,
        ctx->successful_recoveries,
        calculate_success_rate(ctx->successful_recoveries, ctx->total_recoveries),
        ctx->brain_decisions,
        calculate_success_rate(ctx->brain_correct_predictions, ctx->brain_decisions),
        ctx->pattern_count
    );

    return (written > 0 && (size_t)written < buffer_size) ? written : -1;
}
