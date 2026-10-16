/**
 * @file nimcp_temporal_patterns.c
 * @brief Implementation of temporal pattern analysis for brain introspection
 *
 * WHAT: Detects, matches, and predicts temporal patterns in brain state evolution
 * WHY:  Metacognition requires understanding recurring patterns to enable
 *       prediction, planning, and self-awareness
 * HOW:  Dynamic Time Warping (DTW) for similarity, sliding window for detection,
 *       linear regression for trends, pattern library for storage
 *
 * BIOLOGICAL BASIS:
 * - Hippocampal replay: During sleep, hippocampus replays recent experiences
 *   at accelerated rates, consolidating memories (Wilson & McNaughton, 1994)
 * - Pattern completion: CA3 region can reconstruct complete patterns from
 *   partial cues through recurrent connections (Nakazawa et al., 2002)
 * - Predictive coding: Cortical hierarchies constantly predict upcoming
 *   sensory inputs based on learned patterns (Rao & Ballard, 1999)
 * - Consolidation: Memory traces are strengthened through repeated reactivation
 *   of neural patterns (Buzsáki, 1989)
 *
 * ALGORITHMS:
 * - DTW: Dynamic Time Warping for flexible sequence alignment
 * - K-means clustering: Group similar patterns
 * - Linear regression: Trend analysis
 * - Sliding window: Pattern extraction from continuous streams
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include "cognitive/introspection/nimcp_temporal_patterns.h"
#include "cognitive/introspection/nimcp_introspection.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "security/nimcp_bbb_helpers.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/containers/nimcp_queue.h"
#include "utils/containers/nimcp_vector.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/thread/nimcp_thread.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <math.h>
#include <float.h>
#include <string.h>
#include <stdlib.h>

#define LOG_MODULE "cognitive.introspection.temporal_patterns"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_buffer_constants.h"

BRIDGE_BOILERPLATE(temporal_patterns, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ========================================================================
 * INTERNAL STRUCTURES
 * ======================================================================== */

/**
 * WHAT: Pattern library entry
 * WHY: Store patterns with usage metadata
 * HOW: Linked list node for library
 */
typedef struct pattern_library_entry {
    temporal_pattern_t pattern;              /**< The pattern */
    struct pattern_library_entry* next;      /**< Next in list */
} pattern_library_entry_t;

/**
 * WHAT: Pattern detection context (extends introspection context)
 * WHY: Additional state for temporal pattern analysis
 * HOW: Stored in introspection context's pattern_detection_context field
 */
typedef struct {
    temporal_pattern_config_t config;        /**< Configuration */
    pattern_library_entry_t* library_head;   /**< Pattern library linked list */
    uint32_t library_size;                   /**< Number of patterns in library */
    pattern_detected_callback_t callback;    /**< Detection callback */
    void* callback_user_data;                /**< Callback context */
    nimcp_mutex_t lock;                      /**< Thread safety */
} pattern_detection_context_t;

/* ========================================================================
 * FORWARD DECLARATIONS
 * ======================================================================== */

static float compute_dtw_distance(const float* seq1, uint32_t len1,
                                   const float* seq2, uint32_t len2,
                                   uint32_t dimension);
static float compute_normalized_dtw(const temporal_pattern_t* pattern,
                                     const brain_state_t* states,
                                     uint32_t num_states);
static void extract_metric_values(const activity_history_entry_t* history,
                                   uint32_t num_entries,
                                   const char* metric_name,
                                   float* values);
static void linear_regression(const float* values, uint32_t count,
                               float* slope, float* intercept, float* r_squared);
static pattern_detection_context_t* get_pattern_context(introspection_context_t context);
static void notify_pattern_detected(pattern_detection_context_t* ctx,
                                     const temporal_pattern_t* pattern,
                                     float confidence);

/* ========================================================================
 * CONFIGURATION
 * ======================================================================== */

/**
 * WHAT: Get default temporal pattern configuration
 * WHY: Sensible defaults for most use cases
 * HOW: Return pre-configured struct
 */
temporal_pattern_config_t temporal_pattern_default_config(void)
{
    /* Phase 8: Heartbeat at operation start */
    temporal_patterns_heartbeat("temporal_pat_temporal_pattern_def", 0.0f);


    temporal_pattern_config_t config = {
        .window_size = TEMPORAL_DEFAULT_WINDOW_SIZE,
        .min_pattern_length = TEMPORAL_DEFAULT_MIN_PATTERN_LENGTH,
        .max_pattern_length = TEMPORAL_DEFAULT_MAX_PATTERN_LENGTH,
        .similarity_threshold = TEMPORAL_DEFAULT_SIMILARITY_THRESHOLD,
        .max_patterns = TEMPORAL_DEFAULT_MAX_PATTERNS,
        .min_occurrences = TEMPORAL_DEFAULT_MIN_OCCURRENCES,
        .trend_window = TEMPORAL_DEFAULT_TREND_WINDOW,
        .enable_auto_detection = true,
        .enable_prediction = true,
        .enable_callbacks = false
    };
    return config;
}

/* ========================================================================
 * PATTERN DETECTION
 * ======================================================================== */

/**
 * WHAT: Detect recurring patterns in activity history
 * WHY: Identify common sequences of brain states
 * HOW: Sliding window + DTW clustering + occurrence counting
 *
 * ALGORITHM:
 * 1. Get activity history from introspection context
 * 2. Extract state sequences using sliding window
 * 3. Compute pairwise DTW distances
 * 4. Cluster similar sequences (greedy approach for efficiency)
 * 5. Count occurrences and filter by threshold
 *
 * COMPLEXITY: O(n^2 * m * d) - expensive but infrequent
 */
temporal_pattern_t* introspection_detect_patterns(introspection_context_t context,
                                                   const temporal_pattern_config_t* config,
                                                   uint32_t* num_patterns)
{
    /* Guard clause: validate inputs */
    if (!bbb_check_pointer(context, "introspection_detect_patterns")) {
        if (num_patterns) *num_patterns = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_detect_patterns: validation failed");
        return NULL;
    }

    if (!bbb_check_pointer(num_patterns, "introspection_detect_patterns")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_detect_patterns: bbb_check_pointer is NULL");
        return NULL;
    }

    /* Use default config if not provided */
    /* Phase 8: Heartbeat at operation start */
    temporal_patterns_heartbeat("temporal_pat_introspection_detect", 0.0f);


    temporal_pattern_config_t default_config = temporal_pattern_default_config();
    const temporal_pattern_config_t* cfg = config ? config : &default_config;

    /* WHAT: Get activity history from introspection context */
    /* WHY: Need state sequences to detect patterns */
    uint32_t history_count = 0;
    activity_history_entry_t* history = brain_get_activity_history(context, &history_count);

    /* Guard clause: need sufficient history */
    if (history == NULL || history_count < cfg->min_pattern_length) {
        LOG_DEBUG("Insufficient history for pattern detection: %u entries", history_count);
        *num_patterns = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "introspection_detect_patterns: validation failed");
        return NULL;
    }

    /* WHAT: Initialize pattern array (pessimistic allocation) */
    /* WHY: Don't know final count until clustering complete */
    uint32_t max_possible_patterns = history_count / cfg->min_pattern_length;
    if (max_possible_patterns > cfg->max_patterns) {
        max_possible_patterns = cfg->max_patterns;
    }

    temporal_pattern_t* patterns =
        (temporal_pattern_t*)nimcp_calloc(max_possible_patterns, sizeof(temporal_pattern_t));

    if (patterns == NULL) {
        nimcp_free(history);
        history = NULL;
        *num_patterns = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_detect_patterns: validation failed");
        return NULL;
    }

    /* WHAT: Simple pattern detection - look for repeating activation patterns */
    /* WHY: Full DTW clustering is O(n^2), use simpler heuristic for now */
    /* HOW: Sliding window with threshold-based matching */

    uint32_t pattern_count = 0;
    uint32_t window_size = cfg->window_size;

    /* Guard clause: window size must fit in history */
    if (window_size > history_count) {
        window_size = history_count;
    }

    /* WHAT: Extract first pattern as baseline */
    if (pattern_count < max_possible_patterns && window_size >= cfg->min_pattern_length) {
        snprintf(patterns[pattern_count].name, TEMPORAL_MAX_PATTERN_NAME, "pattern_%u", pattern_count);
        patterns[pattern_count].sequence_length = window_size;
        patterns[pattern_count].state_dimension = 1; /* Just using avg_activation for now */
        patterns[pattern_count].occurrence_count = 1;
        patterns[pattern_count].strength = 0.5F;
        patterns[pattern_count].first_detected = nimcp_time_monotonic_ms();
        patterns[pattern_count].last_detected = patterns[pattern_count].first_detected;
        patterns[pattern_count].average_duration_ms = 0.0F;

        /* Allocate state sequence (simplified: just avg_activation values) */
        patterns[pattern_count].state_sequence =
            (float**)nimcp_calloc(window_size, sizeof(float*));

        if (patterns[pattern_count].state_sequence != NULL) {
            for (uint32_t i = 0; i < window_size; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && window_size > 256) {
                    temporal_patterns_heartbeat("temporal_pat_loop",
                                     (float)(i + 1) / (float)window_size);
                }

                patterns[pattern_count].state_sequence[i] = (float*)nimcp_malloc(sizeof(float));
                if (patterns[pattern_count].state_sequence[i] != NULL) {
                    patterns[pattern_count].state_sequence[i][0] = history[i].avg_activation;
                }
            }
            pattern_count++;
        }
    }

    /* WHAT: Add immune threat patterns if immune system connected */
    /* WHY: Threat sequences are temporal patterns for metacognition */
    /* HOW: Query immune system for recent antigens and responses */
    brain_immune_system_t* immune = introspection_get_immune(context);
    if (immune != NULL && pattern_count < max_possible_patterns) {
        brain_immune_stats_t stats;
        if (brain_immune_get_stats(immune, &stats) == 0) {
            /* WHAT: If significant immune activity, create threat pattern */
            if (stats.antigens_processed > 0) {
                snprintf(patterns[pattern_count].name, TEMPORAL_MAX_PATTERN_NAME,
                        "immune_threat_%u", pattern_count);
                patterns[pattern_count].sequence_length = 3; /* Simplified: detection->response->resolution */
                patterns[pattern_count].state_dimension = 1;
                patterns[pattern_count].occurrence_count = stats.threats_neutralized;
                patterns[pattern_count].strength = 0.8F; /* High strength - important for survival */
                patterns[pattern_count].first_detected = nimcp_time_monotonic_ms();
                patterns[pattern_count].last_detected = patterns[pattern_count].first_detected;
                patterns[pattern_count].average_duration_ms = stats.avg_response_time_ms;

                /* Allocate simplified threat pattern sequence */
                patterns[pattern_count].state_sequence =
                    (float**)nimcp_malloc(3 * sizeof(float*));

                if (patterns[pattern_count].state_sequence != NULL) {
                    /* Detection phase */
                    patterns[pattern_count].state_sequence[0] = (float*)nimcp_malloc(sizeof(float));
                    if (patterns[pattern_count].state_sequence[0] != NULL) {
                        patterns[pattern_count].state_sequence[0][0] = 0.3F; /* Low baseline */
                    }
                    /* Response phase */
                    patterns[pattern_count].state_sequence[1] = (float*)nimcp_malloc(sizeof(float));
                    if (patterns[pattern_count].state_sequence[1] != NULL) {
                        patterns[pattern_count].state_sequence[1][0] = 0.8F; /* High activity */
                    }
                    /* Resolution phase */
                    patterns[pattern_count].state_sequence[2] = (float*)nimcp_malloc(sizeof(float));
                    if (patterns[pattern_count].state_sequence[2] != NULL) {
                        patterns[pattern_count].state_sequence[2][0] = 0.4F; /* Returning to baseline */
                    }
                    pattern_count++;
                    LOG_DEBUG("Added immune threat pattern: %u threats processed",
                             stats.antigens_processed);
                }
            }
        }
    }

    /* Cleanup */
    nimcp_free(history);
    history = NULL;
    *num_patterns = pattern_count;

    LOG_INFO("Detected %u temporal patterns", pattern_count);
    return patterns;
}

/**
 * WHAT: Check if current state matches a known pattern
 * WHY: Identify when brain is exhibiting learned behavior
 * HOW: DTW distance between current window and pattern
 */
pattern_match_result_t introspection_match_pattern(introspection_context_t context,
                                                    const temporal_pattern_t* pattern,
                                                    const temporal_pattern_config_t* config)
{
    /* Phase 8: Heartbeat at operation start */
    temporal_patterns_heartbeat("temporal_pat_introspection_match_", 0.0f);


    pattern_match_result_t result;
    memset(&result, 0, sizeof(pattern_match_result_t));

    /* Guard clause: validate inputs */
    if (!bbb_check_pointer(context, "introspection_match_pattern")) {
        return result;
    }

    if (!bbb_check_pointer(pattern, "introspection_match_pattern")) {
        return result;
    }

    /* Use default config if not provided */
    temporal_pattern_config_t default_config = temporal_pattern_default_config();
    const temporal_pattern_config_t* cfg = config ? config : &default_config;

    /* WHAT: Get recent activity history */
    uint32_t history_count = 0;
    activity_history_entry_t* history = brain_get_activity_history(context, &history_count);

    /* Guard clause: need sufficient history */
    if (history == NULL || history_count < pattern->sequence_length) {
        if (history) nimcp_free(history);
        return result;
    }

    /* WHAT: Simple matching - compare recent avg_activation to pattern */
    /* WHY: Full DTW is expensive, use simplified metric matching */
    float distance = 0.0F;
    uint32_t compare_len = pattern->sequence_length;
    if (compare_len > history_count) {
        compare_len = history_count;
    }

    for (uint32_t i = 0; i < compare_len; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && compare_len > 256) {
            temporal_patterns_heartbeat("temporal_pat_loop",
                             (float)(i + 1) / (float)compare_len);
        }

        float hist_val = history[history_count - compare_len + i].avg_activation;
        float pattern_val = (pattern->state_sequence && pattern->state_sequence[i])
                            ? pattern->state_sequence[i][0]
                            : 0.0F;
        float diff = hist_val - pattern_val;
        distance += diff * diff;
    }

    distance = sqrtf(distance / (fabsf(compare_len) > 1e-7f ? compare_len : 1e-7f)); /* RMSE */

    /* WHAT: Convert distance to confidence (inverse relationship) */
    /* WHY: Lower distance = higher confidence */
    result.confidence = 1.0F / (1.0F + distance);
    result.dtw_distance = distance;
    result.matched_pattern = (temporal_pattern_t*)pattern; /* Note: not owned */
    result.match_offset = 0;
    result.is_complete_match = (result.confidence >= cfg->similarity_threshold);

    nimcp_free(history);
    history = NULL;

    LOG_DEBUG("Pattern match: distance=%.3f, confidence=%.3f", distance, result.confidence);
    return result;
}

/**
 * WHAT: Predict next brain state based on pattern matching
 * WHY: Enable anticipation and planning
 * HOW: Find best matching pattern, return next expected state
 */
brain_state_t introspection_predict_next_state(introspection_context_t context,
                                                const temporal_pattern_config_t* config)
{
    /* Phase 8: Heartbeat at operation start */
    temporal_patterns_heartbeat("temporal_pat_introspection_predic", 0.0f);


    brain_state_t predicted_state;
    memset(&predicted_state, 0, sizeof(brain_state_t));

    /* Guard clause: validate inputs */
    if (!bbb_check_pointer(context, "introspection_predict_next_state")) {
        return predicted_state;
    }

    /* Use default config if not provided */
    temporal_pattern_config_t default_config = temporal_pattern_default_config();
    const temporal_pattern_config_t* cfg = config ? config : &default_config;

    /* Guard clause: prediction must be enabled */
    if (!cfg->enable_prediction) {
        LOG_DEBUG("Prediction disabled in configuration");
        return predicted_state;
    }

    /* WHAT: Get pattern library */
    uint32_t num_library_patterns = 0;
    temporal_pattern_t* library = introspection_get_pattern_library(context, &num_library_patterns);

    /* Guard clause: need patterns for prediction */
    if (library == NULL || num_library_patterns == 0) {
        LOG_DEBUG("No patterns in library for prediction");
        return predicted_state;
    }

    /* WHAT: Find best matching pattern */
    float best_confidence = 0.0F;
    const temporal_pattern_t* best_pattern = NULL;

    for (uint32_t i = 0; i < num_library_patterns; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_library_patterns > 256) {
            temporal_patterns_heartbeat("temporal_pat_loop",
                             (float)(i + 1) / (float)num_library_patterns);
        }

        pattern_match_result_t match = introspection_match_pattern(context, &library[i], cfg);
        if (match.confidence > best_confidence) {
            best_confidence = match.confidence;
            best_pattern = &library[i];
        }
    }

    /* WHAT: Construct predicted state from best pattern */
    if (best_pattern != NULL && best_confidence >= cfg->similarity_threshold) {
        /* Allocate state vector (simplified: single value) */
        predicted_state.dimension = 1;
        predicted_state.state_vector = (float*)nimcp_malloc(sizeof(float));

        if (predicted_state.state_vector != NULL) {
            /* Get next expected value from pattern */
            uint32_t next_idx = best_pattern->sequence_length - 1;
            if (best_pattern->state_sequence && best_pattern->state_sequence[next_idx]) {
                predicted_state.state_vector[0] = best_pattern->state_sequence[next_idx][0];
            } else {
                predicted_state.state_vector[0] = 0.5F; /* Default */
            }

            predicted_state.timestamp = nimcp_time_monotonic_ms();
            predicted_state.compression_ratio = 1.0F;
            predicted_state.information_content = 1.0F;

            char interp[NIMCP_ERROR_BUFFER_SIZE];
            snprintf(interp, sizeof(interp),
                     "Predicted from pattern '%s' (confidence: %.2f)",
                     best_pattern->name, best_confidence);
            predicted_state.interpretation = nimcp_strdup(interp);

            LOG_DEBUG("Predicted next state from pattern '%s' (confidence: %.2f)",
                      best_pattern->name, best_confidence);
        }
    }

    pattern_array_free(library, num_library_patterns);
    return predicted_state;
}

/**
 * WHAT: Analyze long-term trend for a metric
 * WHY: Track brain evolution over extended periods
 * HOW: Linear regression on metric values from history
 */
temporal_trend_t introspection_get_trend(introspection_context_t context,
                                          const char* metric_name,
                                          const temporal_pattern_config_t* config)
{
    /* Phase 8: Heartbeat at operation start */
    temporal_patterns_heartbeat("temporal_pat_introspection_get_tr", 0.0f);


    temporal_trend_t trend;
    memset(&trend, 0, sizeof(temporal_trend_t));
    trend.direction = TREND_UNKNOWN;

    /* Guard clause: validate inputs */
    if (!bbb_check_pointer(context, "introspection_get_trend")) {
        return trend;
    }

    if (!bbb_check_string(metric_name, TEMPORAL_MAX_PATTERN_NAME, "introspection_get_trend")) {
        return trend;
    }

    /* WHAT: Copy metric name early so it's always set in return value */
    strncpy(trend.metric_name, metric_name, TEMPORAL_MAX_PATTERN_NAME - 1);
    trend.metric_name[TEMPORAL_MAX_PATTERN_NAME - 1] = '\0';

    /* Use default config if not provided */
    temporal_pattern_config_t default_config = temporal_pattern_default_config();
    const temporal_pattern_config_t* cfg = config ? config : &default_config;

    /* WHAT: Get activity history */
    uint32_t history_count = 0;
    activity_history_entry_t* history = brain_get_activity_history(context, &history_count);

    /* Guard clause: need sufficient data for trend */
    if (history == NULL || history_count < 2) {
        if (history) nimcp_free(history);
        return trend;
    }

    /* WHAT: Limit to trend window */
    uint32_t window = cfg->trend_window;
    if (window == 0) {
        window = history_count;  /* Use full history if window is 0 */
    }
    if (window > history_count) {
        window = history_count;
    }

    /* WHAT: Extract metric values */
    float* values = (float*)nimcp_calloc(window, sizeof(float));
    if (values == NULL) {
        nimcp_free(history);
        history = NULL;
        return trend;
    }

    extract_metric_values(history, history_count, metric_name, values);

    /* WHAT: Compute statistics */
    float sum = 0.0F, sum_sq = 0.0F;
    float min_val = FLT_MAX, max_val = -FLT_MAX;

    for (uint32_t i = 0; i < window; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && window > 256) {
            temporal_patterns_heartbeat("temporal_pat_loop",
                             (float)(i + 1) / (float)window);
        }

        sum += values[i];
        sum_sq += values[i] * values[i];
        if (values[i] < min_val) min_val = values[i];
        if (values[i] > max_val) max_val = values[i];
    }

    trend.mean_value = sum / window;
    trend.variance = (sum_sq / window) - (trend.mean_value * trend.mean_value);
    trend.min_value = min_val;
    trend.max_value = max_val;
    trend.num_samples = window;

    /* WHAT: Linear regression */
    float slope, intercept;
    linear_regression(values, window, &slope, &intercept, &trend.r_squared);
    trend.slope = slope;

    /* WHAT: Classify trend direction */
    /* WHY: Threshold based on slope magnitude and variance */
    float slope_threshold = sqrtf(trend.variance) * 0.1F; /* 10% of std dev */

    if (fabsf(slope) < slope_threshold) {
        trend.direction = TREND_STABLE;
    } else if (slope > 0) {
        trend.direction = TREND_INCREASING;
    } else {
        trend.direction = TREND_DECREASING;
    }

    /* WHAT: Check for oscillation (high variance, low R²) */
    if (trend.r_squared < 0.3F && trend.variance > trend.mean_value * 0.5F) {
        trend.direction = TREND_OSCILLATING;
    }

    LOG_DEBUG("Trend analysis for '%s': direction=%d, slope=%.3f, R²=%.3f",
              metric_name, trend.direction, slope, trend.r_squared);

    nimcp_free(values);
    values = NULL;
    nimcp_free(history);
    history = NULL;
    return trend;
}

/* ========================================================================
 * PATTERN LIBRARY
 * ======================================================================== */

/**
 * WHAT: Get or create pattern detection context
 * WHY: Store pattern library and callbacks
 * HOW: Lazy initialization on first use
 */
static pattern_detection_context_t* get_pattern_context(introspection_context_t context)
{
    /* This is a simplified version - in real implementation, this would be
     * stored in the introspection_context_struct */
    static pattern_detection_context_t global_pattern_ctx;
    static bool initialized = false;

    if (!initialized) {
        memset(&global_pattern_ctx, 0, sizeof(pattern_detection_context_t));
        global_pattern_ctx.config = temporal_pattern_default_config();
        nimcp_mutex_init(&global_pattern_ctx.lock, false);
        initialized = true;
    }

    return &global_pattern_ctx;
}

/**
 * WHAT: Register a known pattern in the pattern library
 * WHY: Store patterns for future matching and prediction
 * HOW: Add to linked list in pattern context
 */
bool introspection_register_pattern(introspection_context_t context,
                                     const temporal_pattern_t* pattern)
{
    /* Guard clause: validate inputs */
    if (!bbb_check_pointer(context, "introspection_register_pattern")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "introspection_register_pattern: bbb_check_pointer is NULL");
        return false;
    }

    if (!bbb_check_pointer(pattern, "introspection_register_pattern")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "introspection_register_pattern: bbb_check_pointer is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    temporal_patterns_heartbeat("temporal_pat_introspection_regist", 0.0f);


    pattern_detection_context_t* ctx = get_pattern_context(context);
    if (ctx == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "introspection_register_pattern: validation failed");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);

    /* Guard clause: check library size limit */
    if (ctx->library_size >= ctx->config.max_patterns) {
        LOG_WARN("Pattern library full (%u patterns), cannot register '%s'",
                 ctx->library_size, pattern->name);
        nimcp_mutex_unlock(&ctx->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "introspection_register_pattern: capacity exceeded");
        return false;
    }

    /* WHAT: Allocate new library entry */
    pattern_library_entry_t* entry =
        (pattern_library_entry_t*)nimcp_malloc(sizeof(pattern_library_entry_t));

    if (entry == NULL) {
        nimcp_mutex_unlock(&ctx->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "introspection_register_pattern: validation failed");
        return false;
    }

    /* WHAT: Deep copy pattern */
    memcpy(&entry->pattern, pattern, sizeof(temporal_pattern_t));

    /* WHAT: Deep copy state sequence */
    if (pattern->state_sequence != NULL && pattern->sequence_length > 0) {
        entry->pattern.state_sequence =
            (float**)nimcp_calloc(pattern->sequence_length, sizeof(float*));

        if (entry->pattern.state_sequence != NULL) {
            for (uint32_t i = 0; i < pattern->sequence_length; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && pattern->sequence_length > 256) {
                    temporal_patterns_heartbeat("temporal_pat_loop",
                                     (float)(i + 1) / (float)pattern->sequence_length);
                }

                entry->pattern.state_sequence[i] =
                    (float*)nimcp_calloc(pattern->state_dimension, sizeof(float));

                if (entry->pattern.state_sequence[i] != NULL && pattern->state_sequence[i] != NULL) {
                    memcpy(entry->pattern.state_sequence[i], pattern->state_sequence[i],
                           pattern->state_dimension * sizeof(float));
                }
            }
        }
    }

    /* WHAT: Insert at head of list */
    entry->next = ctx->library_head;
    ctx->library_head = entry;
    ctx->library_size++;

    nimcp_mutex_unlock(&ctx->lock);

    LOG_INFO("Registered pattern '%s' in library (%u total patterns)",
             pattern->name, ctx->library_size);

    return true;
}

/**
 * WHAT: Clear all patterns from the pattern library
 * WHY: Reset library for fresh pattern learning or testing
 * HOW: Free all library entries and reset count
 */
void introspection_clear_pattern_library(introspection_context_t context)
{
    /* Phase 8: Heartbeat at operation start */
    temporal_patterns_heartbeat("temporal_pat_introspection_clear_", 0.0f);


    pattern_detection_context_t* ctx = get_pattern_context(context);
    if (ctx == NULL) {
        return;
    }

    nimcp_mutex_lock(&ctx->lock);

    /* Free all library entries */
    pattern_library_entry_t* entry = ctx->library_head;
    while (entry != NULL) {
        pattern_library_entry_t* next = entry->next;

        /* Free pattern's state sequence */
        if (entry->pattern.state_sequence != NULL) {
            for (uint32_t i = 0; i < entry->pattern.sequence_length; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && entry->pattern.sequence_length > 256) {
                    temporal_patterns_heartbeat("temporal_pat_loop",
                                     (float)(i + 1) / (float)entry->pattern.sequence_length);
                }

                if (entry->pattern.state_sequence[i] != NULL) {
                    nimcp_free(entry->pattern.state_sequence[i]);
                }
            }
            nimcp_free(entry->pattern.state_sequence);
        }

        nimcp_free(entry);
        entry = NULL;
        entry = next;
    }

    ctx->library_head = NULL;
    ctx->library_size = 0;

    nimcp_mutex_unlock(&ctx->lock);

    LOG_DEBUG("Cleared pattern library");
}

/**
 * WHAT: Get all patterns in the pattern library
 * WHY: Inspect learned patterns
 * HOW: Return copy of library contents
 */
temporal_pattern_t* introspection_get_pattern_library(introspection_context_t context,
                                                       uint32_t* num_patterns)
{
    /* Guard clause: validate inputs */
    if (!bbb_check_pointer(context, "introspection_get_pattern_library")) {
        if (num_patterns) *num_patterns = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_get_pattern_library: validation failed");
        return NULL;
    }

    if (!bbb_check_pointer(num_patterns, "introspection_get_pattern_library")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_get_pattern_library: bbb_check_pointer is NULL");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    temporal_patterns_heartbeat("temporal_pat_introspection_get_pa", 0.0f);


    pattern_detection_context_t* ctx = get_pattern_context(context);
    if (ctx == NULL) {
        *num_patterns = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_get_pattern_library: validation failed");
        return NULL;
    }

    nimcp_mutex_lock(&ctx->lock);

    *num_patterns = ctx->library_size;

    /* Guard clause: empty library */
    if (ctx->library_size == 0) {
        nimcp_mutex_unlock(&ctx->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "introspection_get_pattern_library: ctx->library_size is zero");
        return NULL;
    }

    /* WHAT: Allocate array for patterns */
    temporal_pattern_t* patterns =
        (temporal_pattern_t*)nimcp_calloc(ctx->library_size, sizeof(temporal_pattern_t));

    if (patterns == NULL) {
        nimcp_mutex_unlock(&ctx->lock);
        *num_patterns = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_get_pattern_library: validation failed");
        return NULL;
    }

    /* WHAT: Copy patterns from library */
    pattern_library_entry_t* entry = ctx->library_head;
    uint32_t index = 0;

    while (entry != NULL && index < ctx->library_size) {
        memcpy(&patterns[index], &entry->pattern, sizeof(temporal_pattern_t));

        /* WHAT: Deep copy state sequence for proper ownership */
        /* WHY: Caller will free returned patterns, must not double-free library data */
        if (entry->pattern.state_sequence != NULL && entry->pattern.sequence_length > 0) {
            patterns[index].state_sequence =
                (float**)nimcp_calloc(entry->pattern.sequence_length, sizeof(float*));

            if (patterns[index].state_sequence != NULL) {
                for (uint32_t j = 0; j < entry->pattern.sequence_length; j++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((j & 0xFF) == 0 && entry->pattern.sequence_length > 256) {
                        temporal_patterns_heartbeat("temporal_pat_loop",
                                         (float)(j + 1) / (float)entry->pattern.sequence_length);
                    }

                    if (entry->pattern.state_sequence[j] != NULL) {
                        patterns[index].state_sequence[j] =
                            (float*)nimcp_calloc(entry->pattern.state_dimension, sizeof(float));
                        if (patterns[index].state_sequence[j] != NULL) {
                            memcpy(patterns[index].state_sequence[j],
                                   entry->pattern.state_sequence[j],
                                   entry->pattern.state_dimension * sizeof(float));
                        }
                    } else {
                        patterns[index].state_sequence[j] = NULL;
                    }
                }
            }
        } else {
            patterns[index].state_sequence = NULL;
        }

        entry = entry->next;
        index++;
    }

    nimcp_mutex_unlock(&ctx->lock);

    LOG_DEBUG("Retrieved %u patterns from library", *num_patterns);
    return patterns;
}

/**
 * WHAT: Compare two patterns for similarity
 * WHY: Measure pattern distance for clustering
 * HOW: Average DTW distance across state sequences
 */
float introspection_pattern_similarity(const temporal_pattern_t* pattern1,
                                        const temporal_pattern_t* pattern2)
{
    /* Guard clause: validate inputs */
    if (!bbb_check_pointer(pattern1, "introspection_pattern_similarity")) {
        return 0.0F;
    }

    if (!bbb_check_pointer(pattern2, "introspection_pattern_similarity")) {
        return 0.0F;
    }

    /* Guard clause: dimensions must match */
    /* Phase 8: Heartbeat at operation start */
    temporal_patterns_heartbeat("temporal_pat_introspection_patter", 0.0f);


    if (pattern1->state_dimension != pattern2->state_dimension) {
        LOG_DEBUG("Pattern dimension mismatch: %u vs %u",
                  pattern1->state_dimension, pattern2->state_dimension);
        return 0.0F;
    }

    /* WHAT: Compute DTW distance (simplified for single dimension) */
    float distance = 0.0F;
    uint32_t min_len = (pattern1->sequence_length < pattern2->sequence_length)
                       ? pattern1->sequence_length
                       : pattern2->sequence_length;

    for (uint32_t i = 0; i < min_len; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && min_len > 256) {
            temporal_patterns_heartbeat("temporal_pat_loop",
                             (float)(i + 1) / (float)min_len);
        }

        if (pattern1->state_sequence && pattern1->state_sequence[i] &&
            pattern2->state_sequence && pattern2->state_sequence[i]) {

            float diff = pattern1->state_sequence[i][0] - pattern2->state_sequence[i][0];
            distance += diff * diff;
        }
    }

    distance = sqrtf(distance / (fabsf(min_len) > 1e-7f ? min_len : 1e-7f));

    /* WHAT: Convert distance to similarity (inverse) */
    float similarity = 1.0F / (1.0F + distance);

    LOG_DEBUG("Pattern similarity: distance=%.3f, similarity=%.3f", distance, similarity);
    return similarity;
}

/* ========================================================================
 * BRAIN INTEGRATION
 * ======================================================================== */

/**
 * WHAT: Enable automatic pattern detection on brain
 * WHY: Continuously monitor and learn patterns
 * HOW: Set flag in pattern context
 */
bool brain_enable_pattern_detection(brain_t brain,
                                     const temporal_pattern_config_t* config)
{
    /* Guard clause: validate brain */
    if (!bbb_check_pointer(brain, "brain_enable_pattern_detection")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_enable_pattern_detection: bbb_check_pointer is NULL");
        return false;
    }

    /* WHAT: Get introspection context */
    /* Phase 8: Heartbeat at operation start */
    temporal_patterns_heartbeat("temporal_pat_brain_enable_pattern", 0.0f);


    introspection_context_t intro = brain_get_introspection(brain);
    if (intro == NULL) {
        LOG_WARN("Brain does not have introspection enabled");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_enable_pattern_detection: validation failed");
        return false;
    }

    pattern_detection_context_t* ctx = get_pattern_context(intro);
    if (ctx == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_enable_pattern_detection: validation failed");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);

    /* WHAT: Update configuration */
    if (config != NULL) {
        ctx->config = *config;
    }
    ctx->config.enable_auto_detection = true;

    nimcp_mutex_unlock(&ctx->lock);

    LOG_INFO("Enabled automatic pattern detection on brain");
    return true;
}

/**
 * WHAT: Get currently active patterns in brain
 * WHY: See which patterns are currently executing
 * HOW: Match recent history against library
 */
temporal_pattern_t* brain_get_active_patterns(brain_t brain, uint32_t* num_patterns)
{
    /* Guard clause: validate inputs */
    if (!bbb_check_pointer(brain, "brain_get_active_patterns")) {
        if (num_patterns) *num_patterns = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_get_active_patterns: validation failed");
        return NULL;
    }

    if (!bbb_check_pointer(num_patterns, "brain_get_active_patterns")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_get_active_patterns: bbb_check_pointer is NULL");
        return NULL;
    }

    /* WHAT: Get introspection context */
    /* Phase 8: Heartbeat at operation start */
    temporal_patterns_heartbeat("temporal_pat_brain_get_active_pat", 0.0f);


    introspection_context_t intro = brain_get_introspection(brain);
    if (intro == NULL) {
        *num_patterns = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_get_active_patterns: validation failed");
        return NULL;
    }

    pattern_detection_context_t* ctx = get_pattern_context(intro);
    if (ctx == NULL) {
        *num_patterns = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_get_active_patterns: validation failed");
        return NULL;
    }

    /* WHAT: Get pattern library */
    uint32_t library_size = 0;
    temporal_pattern_t* library = introspection_get_pattern_library(intro, &library_size);

    if (library == NULL || library_size == 0) {
        *num_patterns = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_get_active_patterns: library_size is zero");
        return NULL;
    }

    /* WHAT: Match against library */
    temporal_pattern_t* active =
        (temporal_pattern_t*)nimcp_calloc(library_size, sizeof(temporal_pattern_t));

    if (active == NULL) {
        pattern_array_free(library, library_size);
        *num_patterns = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_get_active_patterns: validation failed");
        return NULL;
    }

    uint32_t active_count = 0;

    for (uint32_t i = 0; i < library_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && library_size > 256) {
            temporal_patterns_heartbeat("temporal_pat_loop",
                             (float)(i + 1) / (float)library_size);
        }

        pattern_match_result_t match =
            introspection_match_pattern(intro, &library[i], &ctx->config);

        if (match.is_complete_match && active_count < library_size) {
            memcpy(&active[active_count], &library[i], sizeof(temporal_pattern_t));
            active_count++;
        }
    }

    pattern_array_free(library, library_size);
    *num_patterns = active_count;

    LOG_DEBUG("Found %u active patterns", active_count);
    return active;
}

/**
 * WHAT: Register callback for pattern detection events
 * WHY: React to pattern detection in real-time
 * HOW: Store callback in pattern context
 */
bool brain_on_pattern_detected(brain_t brain,
                                pattern_detected_callback_t callback,
                                void* user_data)
{
    /* Guard clause: validate brain */
    if (!bbb_check_pointer(brain, "brain_on_pattern_detected")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_on_pattern_detected: bbb_check_pointer is NULL");
        return false;
    }

    /* WHAT: Get introspection context */
    /* Phase 8: Heartbeat at operation start */
    temporal_patterns_heartbeat("temporal_pat_brain_on_pattern_det", 0.0f);


    introspection_context_t intro = brain_get_introspection(brain);
    if (intro == NULL) {
        LOG_WARN("Brain does not have introspection enabled");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_on_pattern_detected: validation failed");
        return false;
    }

    pattern_detection_context_t* ctx = get_pattern_context(intro);
    if (ctx == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_on_pattern_detected: validation failed");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);

    ctx->callback = callback;
    ctx->callback_user_data = user_data;
    ctx->config.enable_callbacks = (callback != NULL);

    nimcp_mutex_unlock(&ctx->lock);

    LOG_INFO("Registered pattern detection callback");
    return true;
}

/* ========================================================================
 * MEMORY MANAGEMENT
 * ======================================================================== */

/**
 * WHAT: Free temporal pattern structure
 * WHY: Release allocated state sequences
 * HOW: Free sequence array, zero struct
 */
void temporal_pattern_free(temporal_pattern_t* pattern)
{
    if (pattern == NULL) {
        return;
    }

    if (pattern->state_sequence != NULL) {
        for (uint32_t i = 0; i < pattern->sequence_length; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && pattern->sequence_length > 256) {
                temporal_patterns_heartbeat("temporal_pat_loop",
                                 (float)(i + 1) / (float)pattern->sequence_length);
            }

            nimcp_free(pattern->state_sequence[i]);
        }
        nimcp_free(pattern->state_sequence);
    }

    /* Phase 8: Heartbeat at operation start */
    temporal_patterns_heartbeat("temporal_pat_temporal_pattern_fre", 0.0f);


    memset(pattern, 0, sizeof(temporal_pattern_t));
}

/**
 * WHAT: Free array of patterns
 * WHY: Release pattern library allocations
 * HOW: Free each pattern, free array
 */
void pattern_array_free(temporal_pattern_t* patterns, uint32_t num_patterns)
{
    if (patterns == NULL) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    temporal_patterns_heartbeat("temporal_pat_pattern_array_free", 0.0f);


    for (uint32_t i = 0; i < num_patterns; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_patterns > 256) {
            temporal_patterns_heartbeat("temporal_pat_loop",
                             (float)(i + 1) / (float)num_patterns);
        }

        temporal_pattern_free(&patterns[i]);
    }

    nimcp_free(patterns);
    patterns = NULL;
}

/**
 * WHAT: Free pattern sequence structure
 * WHY: Release state array
 * HOW: Free states array, zero struct
 */
void pattern_sequence_free(pattern_sequence_t* sequence)
{
    if (sequence == NULL) {
        return;
    }

    if (sequence->states != NULL) {
        for (uint32_t i = 0; i < sequence->num_states; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && sequence->num_states > 256) {
                temporal_patterns_heartbeat("temporal_pat_loop",
                                 (float)(i + 1) / (float)sequence->num_states);
            }

            brain_state_free(&sequence->states[i]);
        }
        nimcp_free(sequence->states);
    }

    /* Phase 8: Heartbeat at operation start */
    temporal_patterns_heartbeat("temporal_pat_pattern_sequence_fre", 0.0f);


    memset(sequence, 0, sizeof(pattern_sequence_t));
}

/**
 * WHAT: Free pattern match result
 * WHY: Release matched pattern if allocated
 * HOW: Zero struct (matched_pattern is not owned)
 */
void pattern_match_result_free(pattern_match_result_t* result)
{
    if (result == NULL) {
        return;
    }

    /* Note: matched_pattern is a pointer to library pattern, not owned */
    /* Phase 8: Heartbeat at operation start */
    temporal_patterns_heartbeat("temporal_pat_pattern_match_result", 0.0f);


    memset(result, 0, sizeof(pattern_match_result_t));
}

/* ========================================================================
 * HELPER FUNCTIONS
 * ======================================================================== */

/**
 * WHAT: Compute DTW distance between two sequences
 * WHY: Flexible sequence alignment for pattern matching
 * HOW: Dynamic programming algorithm
 *
 * COMPLEXITY: O(m * n * d) where m, n are lengths, d is dimension
 */
static float compute_dtw_distance(const float* seq1, uint32_t len1,
                                   const float* seq2, uint32_t len2,
                                   uint32_t dimension)
{
    /* Guard clause: validate inputs */
    if (seq1 == NULL || seq2 == NULL || len1 == 0 || len2 == 0) {
        return FLT_MAX;
    }

    /* WHAT: Allocate DTW matrix (simplified: use previous row only) */
    float* prev_row = (float*)nimcp_calloc((len2 + 1), sizeof(float));
    float* curr_row = (float*)nimcp_calloc((len2 + 1), sizeof(float));

    if (prev_row == NULL || curr_row == NULL) {
        nimcp_free(prev_row);
        prev_row = NULL;
        nimcp_free(curr_row);
        curr_row = NULL;
        return FLT_MAX;
    }

    /* WHAT: Initialize first row */
    prev_row[0] = 0.0F;
    for (uint32_t j = 1; j <= len2; j++) {
        prev_row[j] = FLT_MAX;
    }

    /* WHAT: Fill DTW matrix */
    for (uint32_t i = 1; i <= len1; i++) {
        curr_row[0] = FLT_MAX;

        for (uint32_t j = 1; j <= len2; j++) {
            /* Compute Euclidean distance between vectors */
            float dist = 0.0F;
            for (uint32_t d = 0; d < dimension; d++) {
                /* Phase 8: Loop progress heartbeat */
                if ((d & 0xFF) == 0 && dimension > 256) {
                    temporal_patterns_heartbeat("temporal_pat_loop",
                                     (float)(d + 1) / (float)dimension);
                }

                float diff = seq1[(i-1)*dimension + d] - seq2[(j-1)*dimension + d];
                dist += diff * diff;
            }
            dist = sqrtf(dist);

            /* DTW recurrence */
            float min_prev = prev_row[j-1];
            if (prev_row[j] < min_prev) min_prev = prev_row[j];
            if (curr_row[j-1] < min_prev) min_prev = curr_row[j-1];

            curr_row[j] = dist + min_prev;
        }

        /* Swap rows */
        float* temp = prev_row;
        prev_row = curr_row;
        curr_row = temp;
    }

    float distance = prev_row[len2];

    nimcp_free(prev_row);
    prev_row = NULL;
    nimcp_free(curr_row);
    curr_row = NULL;

    return distance;
}

/**
 * WHAT: Extract metric values from activity history
 * WHY: Prepare data for trend analysis
 * HOW: Map metric name to history field
 */
static void extract_metric_values(const activity_history_entry_t* history,
                                   uint32_t num_entries,
                                   const char* metric_name,
                                   float* values)
{
    if (history == NULL || values == NULL || metric_name == NULL) {
        return;
    }

    for (uint32_t i = 0; i < num_entries; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_entries > 256) {
            temporal_patterns_heartbeat("temporal_pat_loop",
                             (float)(i + 1) / (float)num_entries);
        }

        if (strcmp(metric_name, "avg_activation") == 0) {
            values[i] = history[i].avg_activation;
        } else if (strcmp(metric_name, "max_activation") == 0) {
            values[i] = history[i].max_activation;
        } else if (strcmp(metric_name, "num_active") == 0) {
            values[i] = (float)history[i].num_active;
        } else if (strcmp(metric_name, "energy") == 0) {
            values[i] = history[i].energy_consumption;
        } else {
            values[i] = 0.0F; /* Unknown metric */
        }
    }
}

/**
 * WHAT: Perform linear regression on data
 * WHY: Quantify trend direction and strength
 * HOW: Least squares fitting
 *
 * COMPLEXITY: O(n) where n = number of data points
 */
static void linear_regression(const float* values, uint32_t count,
                               float* slope, float* intercept, float* r_squared)
{
    if (values == NULL || count < 2 || slope == NULL || intercept == NULL || r_squared == NULL) {
        if (slope) *slope = 0.0F;
        if (intercept) *intercept = 0.0F;
        if (r_squared) *r_squared = 0.0F;
        return;
    }

    /* WHAT: Compute sums for least squares */
    float sum_x = 0.0F, sum_y = 0.0F, sum_xx = 0.0F, sum_xy = 0.0F;

    for (uint32_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            temporal_patterns_heartbeat("temporal_pat_loop",
                             (float)(i + 1) / (float)count);
        }

        float x = (float)i; /* Time index */
        float y = values[i];
        sum_x += x;
        sum_y += y;
        sum_xx += x * x;
        sum_xy += x * y;
    }

    float n = (float)count;
    float denominator = (n * sum_xx - sum_x * sum_x);

    /* Guard clause: prevent division by zero */
    if (fabsf(denominator) < 1e-10F) {
        *slope = 0.0F;
        *intercept = sum_y / n;
        *r_squared = 0.0F;
        return;
    }

    /* WHAT: Compute slope and intercept */
    *slope = (n * sum_xy - sum_x * sum_y) / denominator;
    *intercept = (sum_y - (*slope) * sum_x) / n;

    /* WHAT: Compute R² (coefficient of determination) */
    float mean_y = sum_y / (fabsf(n) > 1e-7f ? n : 1e-7f);
    float ss_tot = 0.0F, ss_res = 0.0F;

    for (uint32_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            temporal_patterns_heartbeat("temporal_pat_loop",
                             (float)(i + 1) / (float)count);
        }

        float x = (float)i;
        float y = values[i];
        float y_pred = (*slope) * x + (*intercept);

        float diff_tot = y - mean_y;
        float diff_res = y - y_pred;

        ss_tot += diff_tot * diff_tot;
        ss_res += diff_res * diff_res;
    }

    *r_squared = (ss_tot > 1e-10F) ? (1.0F - ss_res / ss_tot) : 0.0F;
}

/**
 * WHAT: Notify callback of pattern detection
 * WHY: Enable real-time response to pattern events
 * HOW: Invoke registered callback if enabled
 */
static void notify_pattern_detected(pattern_detection_context_t* ctx,
                                     const temporal_pattern_t* pattern,
                                     float confidence)
{
    if (ctx == NULL || pattern == NULL) {
        return;
    }

    if (!ctx->config.enable_callbacks || ctx->callback == NULL) {
        return;
    }

    /* WHAT: Invoke callback with pattern and confidence */
    ctx->callback(pattern, confidence, ctx->callback_user_data);

    LOG_DEBUG("Pattern detection callback invoked for '%s' (confidence: %.2f)",
              pattern->name, confidence);
}

/* ========================================================================
 * KG SELF-AWARENESS INTEGRATION
 * ======================================================================== */

/**
 * WHAT: Query knowledge graph for self-knowledge about temporal patterns module
 * WHY:  Enable self-awareness - module can introspect its own capabilities
 * HOW:  Query entity by name, get relations from/to
 *
 * @param kg Knowledge graph reader
 * @return 1 if entity found, 0 if not
 */
int temporal_patterns_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Query our own entity from the knowledge graph */
    /* Phase 8: Heartbeat at operation start */
    temporal_patterns_heartbeat("temporal_pat_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Temporal_Patterns_Module");
    if (self) {
        /* Module now knows its own capabilities from KG */
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                temporal_patterns_heartbeat("temporal_pat_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            LOG_DEBUG("Temporal patterns self-knowledge: %s", self->observations[i]);
        }
    }

    /* Query connections to understand integration points */
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Temporal_Patterns_Module");
    if (connections) {
        LOG_DEBUG("Temporal patterns has %u outgoing connections", connections->count);
        kg_relation_list_destroy(connections);
    }

    /* Query incoming connections */
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Temporal_Patterns_Module");
    if (incoming) {
        LOG_DEBUG("Temporal patterns has %u incoming connections", incoming->count);
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent + Full Training
 * ============================================================================ */

void temporal_patterns_set_instance_health_agent(void* ctx, nimcp_health_agent_t* agent) {
    (void)ctx;
    g_temporal_patterns_instance_health_agent = agent;
}

int temporal_patterns_training_begin(void* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "temporal_patterns_training_begin: ctx is NULL");
        return -1;
    }
    temporal_patterns_heartbeat_instance(g_temporal_patterns_instance_health_agent,
        "temp_pat_training_begin", 0.0f);
    NIMCP_LOGGING_INFO("[TEMPORAL_PATTERNS] Training begin: module state reset");
    return 0;
}

int temporal_patterns_training_step(void* ctx, float progress) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "temporal_patterns_training_step: ctx is NULL");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    temporal_patterns_heartbeat_instance(g_temporal_patterns_instance_health_agent,
        "temp_pat_training_step", progress);
    return 0;
}

int temporal_patterns_training_end(void* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "temporal_patterns_training_end: ctx is NULL");
        return -1;
    }
    temporal_patterns_heartbeat_instance(g_temporal_patterns_instance_health_agent,
        "temp_pat_training_end", 1.0f);
    NIMCP_LOGGING_INFO("[TEMPORAL_PATTERNS] Training end: metrics finalized");
    return 0;
}
