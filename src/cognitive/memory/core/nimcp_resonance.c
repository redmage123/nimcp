//=============================================================================
// nimcp_resonance.c - Resonance Scoring Engine Implementation
//=============================================================================
/**
 * @file nimcp_resonance.c
 * @brief Implementation of four-component resonance scoring for Prime Resonant
 *
 * WHAT: Computes multi-dimensional similarity/relevance between memories
 * WHY:  Memory retrieval requires combining content, semantic state, phase, and
 *       oscillator coherence for biologically realistic matching
 * HOW:  Weighted combination of Jaccard, Phase, Quaternion, and Kuramoto scores
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#include "cognitive/memory/core/nimcp_resonance.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"
#include <math.h>

BRIDGE_BOILERPLATE(resonance, MESH_ADAPTER_CATEGORY_MEMORY)


//=============================================================================
// Static Variables
//=============================================================================

/** Thread-local error message buffer */
static _Thread_local char s_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

/** Global statistics (consider making thread-local or atomic for production) */
static resonance_stats_t s_stats = {0};

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Set the last error message
 */
static void set_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(s_last_error, sizeof(s_last_error), format, args);
    va_end(args);
}

/**
 * @brief Clear the last error message
 */
static void clear_error(void) {
    s_last_error[0] = '\0';
}

/**
 * @brief Wrap phase to [0, 2*pi] range
 */
static inline float wrap_phase(float phase) {
    while (phase < 0.0f) phase += NIMCP_TWO_PI_F;
    while (phase >= NIMCP_TWO_PI_F) phase -= NIMCP_TWO_PI_F;
    return phase;
}

/**
 * @brief Fast absolute value for floats
 */
static inline float fabsf_fast(float x) {
    return (x < 0.0f) ? -x : x;
}

//=============================================================================
// Configuration Functions
//=============================================================================

resonance_config_t resonance_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    resonance_heartbeat("resonance_config_default", 0.0f);


    resonance_config_t config = {
        .weight_jaccard = RESONANCE_DEFAULT_WEIGHT_JACCARD,
        .weight_phase = RESONANCE_DEFAULT_WEIGHT_PHASE,
        .weight_quaternion = RESONANCE_DEFAULT_WEIGHT_QUATERNION,
        .weight_kuramoto = RESONANCE_DEFAULT_WEIGHT_KURAMOTO,
        .threshold = RESONANCE_DEFAULT_THRESHOLD,
        .normalize_weights = true
    };
    return config;
}

resonance_config_t resonance_config_content_focused(void) {
    /* Phase 8: Heartbeat at operation start */
    resonance_heartbeat("resonance_config_content_focus", 0.0f);


    resonance_config_t config = {
        .weight_jaccard = 0.6f,
        .weight_phase = 0.1f,
        .weight_quaternion = 0.2f,
        .weight_kuramoto = 0.1f,
        .threshold = RESONANCE_DEFAULT_THRESHOLD,
        .normalize_weights = false  // Already sums to 1.0
    };
    return config;
}

resonance_config_t resonance_config_emotion_focused(void) {
    /* Phase 8: Heartbeat at operation start */
    resonance_heartbeat("resonance_config_emotion_focus", 0.0f);


    resonance_config_t config = {
        .weight_jaccard = 0.2f,
        .weight_phase = 0.1f,
        .weight_quaternion = 0.6f,
        .weight_kuramoto = 0.1f,
        .threshold = RESONANCE_DEFAULT_THRESHOLD,
        .normalize_weights = false
    };
    return config;
}

resonance_config_t resonance_config_temporal_focused(void) {
    /* Phase 8: Heartbeat at operation start */
    resonance_heartbeat("resonance_config_temporal_focu", 0.0f);


    resonance_config_t config = {
        .weight_jaccard = 0.2f,
        .weight_phase = 0.5f,
        .weight_quaternion = 0.2f,
        .weight_kuramoto = 0.1f,
        .threshold = RESONANCE_DEFAULT_THRESHOLD,
        .normalize_weights = false
    };
    return config;
}

bool resonance_config_validate(resonance_config_t* config) {
    if (!config) {
        set_error("NULL config pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "resonance_config_validate: config is NULL");
        return false;
    }

    // Check for negative weights
    /* Phase 8: Heartbeat at operation start */
    resonance_heartbeat("resonance_config_validate", 0.0f);


    if (config->weight_jaccard < 0.0f ||
        config->weight_phase < 0.0f ||
        config->weight_quaternion < 0.0f ||
        config->weight_kuramoto < 0.0f) {
        set_error("Weights must be non-negative");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "resonance_config_validate: operation failed");
        return false;
    }

    // Check that at least one weight is positive
    float sum = config->weight_jaccard + config->weight_phase +
                config->weight_quaternion + config->weight_kuramoto;
    if (sum < RESONANCE_EPSILON) {
        set_error("At least one weight must be positive");
        return false;
    }

    // Check threshold range
    if (config->threshold < 0.0f || config->threshold > 1.0f) {
        set_error("Threshold must be in [0, 1]");
        return false;
    }

    // Normalize weights if requested
    if (config->normalize_weights && fabsf_fast(sum - 1.0f) > RESONANCE_EPSILON) {
        config->weight_jaccard /= sum;
        config->weight_phase /= sum;
        config->weight_quaternion /= sum;
        config->weight_kuramoto /= sum;
    }

    clear_error();
    return true;
}

//=============================================================================
// Component Functions
//=============================================================================

float resonance_jaccard(const prime_signature_t* sig1, const prime_signature_t* sig2) {
    if (!sig1 || !sig2) {
        set_error("NULL signature pointer");
        return -1.0f;
    }

    // Delegate to prime_sig_jaccard from nimcp_prime_signature module
    // This ensures consistency and avoids code duplication
    /* Phase 8: Heartbeat at operation start */
    resonance_heartbeat("resonance_jaccard", 0.0f);


    float result = prime_sig_jaccard(sig1, sig2);

    if (result < 0.0f) {
        set_error("prime_sig_jaccard failed");
        return -1.0f;
    }

    clear_error();
    return result;
}

float resonance_phase_coherence(float phase1, float phase2) {
    // Wrap phases to [0, 2*pi]
    /* Phase 8: Heartbeat at operation start */
    resonance_heartbeat("resonance_phase_coherence", 0.0f);


    phase1 = wrap_phase(phase1);
    phase2 = wrap_phase(phase2);

    // Compute phase difference
    float diff = phase1 - phase2;

    // Wrap difference to [-pi, pi]
    while (diff > M_PI) diff -= NIMCP_TWO_PI_F;
    while (diff < -M_PI) diff += NIMCP_TWO_PI_F;

    // PLV = cos(diff), map from [-1, +1] to [0, 1]
    float plv = cosf(diff);
    return (plv + 1.0f) / 2.0f;
}

float resonance_quaternion_similarity(nimcp_quaternion_t q1, nimcp_quaternion_t q2) {
    // Normalize quaternions
    /* Phase 8: Heartbeat at operation start */
    resonance_heartbeat("resonance_quaternion_similarit", 0.0f);


    q1 = quat_normalize(q1);
    q2 = quat_normalize(q2);

    // Compute geodesic distance: arccos(|q1.q2|) / pi
    // Similarity = 1 - distance
    float geodesic = quat_geodesic_distance(q1, q2);

    // Geodesic is in [0, pi], normalize to [0, 1] and invert for similarity
    float similarity = 1.0f - (geodesic / M_PI);

    return nimcp_clamp01(similarity);
}

float resonance_kuramoto_coherence(
    uint32_t module1,
    uint32_t module2,
    const kuramoto_state_t* kuramoto_state)
{
    // If no Kuramoto state, return neutral value
    if (!kuramoto_state) {
        return 0.5f;
    }

    // If same module, perfect coherence
    /* Phase 8: Heartbeat at operation start */
    resonance_heartbeat("resonance_kuramoto_coherence", 0.0f);


    if (module1 == module2) {
        return 1.0f;
    }

    // TODO: Implement actual Kuramoto coherence computation when
    // nimcp_kuramoto.h is available. For now, return a placeholder
    // based on module distance heuristic.
    //
    // Real implementation would:
    // 1. Get oscillator phases for both modules from kuramoto_state
    // 2. Compute order parameter r = |mean(exp(i*theta_j))| for joint set
    // 3. Return r as coherence measure

    // Placeholder: Assume nearby modules are more coherent
    // This is a stub until Kuramoto integration is complete
    uint32_t diff = (module1 > module2) ? (module1 - module2) : (module2 - module1);
    float coherence = 1.0f / (1.0f + 0.1f * (float)diff);

    return nimcp_clamp01(coherence);
}

//=============================================================================
// Single Computation Functions
//=============================================================================

bool resonance_compute(
    const resonance_query_t* query,
    const resonance_target_t* target,
    const resonance_config_t* config,
    const kuramoto_state_t* kuramoto_state,
    resonance_result_t* result)
{
    // Validate inputs
    if (!query || !target || !config || !result) {
        set_error("NULL pointer: query=%p, target=%p, config=%p, result=%p",
                  (void*)query, (void*)target, (void*)config, (void*)result);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "resonance_compute: required parameter is NULL (query, target, config, result)");
        return false;
    }

    // Initialize result
    /* Phase 8: Heartbeat at operation start */
    resonance_heartbeat("resonance_compute", 0.0f);


    memset(result, 0, sizeof(*result));

    // Create working copy of config for validation
    resonance_config_t cfg = *config;
    if (!resonance_config_validate(&cfg)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "resonance_compute: resonance_config_validate is NULL");
        return false;  // Error already set
    }

    // Compute Jaccard component
    float jaccard = 0.0f;
    if (query->signature && target->signature && cfg.weight_jaccard > RESONANCE_EPSILON) {
        jaccard = resonance_jaccard(query->signature, target->signature);
        if (jaccard < 0.0f) jaccard = 0.0f;  // Error case
    }
    result->jaccard_component = cfg.weight_jaccard * jaccard;

    // Compute Phase coherence component
    float phase = 0.0f;
    if (cfg.weight_phase > RESONANCE_EPSILON) {
        phase = resonance_phase_coherence(query->phase, target->phase);
    }
    result->phase_component = cfg.weight_phase * phase;

    // Compute Quaternion similarity component
    float quat = 0.0f;
    if (cfg.weight_quaternion > RESONANCE_EPSILON) {
        quat = resonance_quaternion_similarity(query->quaternion, target->quaternion);
    }
    result->quat_component = cfg.weight_quaternion * quat;

    // Compute Kuramoto coherence component
    float kuramoto = 0.0f;
    if (cfg.weight_kuramoto > RESONANCE_EPSILON) {
        kuramoto = resonance_kuramoto_coherence(query->module_id, target->module_id,
                                                 kuramoto_state);
    }
    result->kuramoto_component = cfg.weight_kuramoto * kuramoto;

    // Compute total resonance
    result->total = result->jaccard_component +
                    result->phase_component +
                    result->quat_component +
                    result->kuramoto_component;

    // Clamp total to [0, 1] for safety
    result->total = nimcp_clamp01(result->total);

    // Check threshold
    result->above_threshold = (result->total >= cfg.threshold);

    // Update statistics
    s_stats.total_computations++;
    if (result->above_threshold) {
        s_stats.above_threshold_count++;
    }
    // Running mean update (simple moving average)
    float n = (float)s_stats.total_computations;
    s_stats.mean_resonance = ((n - 1.0f) * s_stats.mean_resonance + result->total) / (fabsf(n) > 1e-7f ? n : 1e-7f);

    if (result->total > s_stats.max_resonance) {
        s_stats.max_resonance = result->total;
    }
    if (s_stats.min_resonance == 0.0f || result->total < s_stats.min_resonance) {
        if (result->total > RESONANCE_EPSILON) {
            s_stats.min_resonance = result->total;
        }
    }

    clear_error();
    return true;
}

bool resonance_compute_fast(
    const resonance_query_t* query,
    const resonance_target_t* target,
    const resonance_config_t* config,
    resonance_result_t* result)
{
    if (!query || !target || !config || !result) {
        set_error("NULL pointer in resonance_compute_fast");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "resonance_compute_fast: required parameter is NULL (query, target, config, result)");
        return false;
    }

    // Create config copy and redistribute Kuramoto weight to other components
    /* Phase 8: Heartbeat at operation start */
    resonance_heartbeat("resonance_compute_fast", 0.0f);


    resonance_config_t cfg = *config;
    float kuramoto_weight = cfg.weight_kuramoto;
    cfg.weight_kuramoto = 0.0f;

    // Redistribute weight proportionally to other components
    float other_sum = cfg.weight_jaccard + cfg.weight_phase + cfg.weight_quaternion;
    if (other_sum > RESONANCE_EPSILON) {
        float scale = (other_sum + kuramoto_weight) / (fabsf(other_sum) > 1e-7f ? other_sum : 1e-7f);
        cfg.weight_jaccard *= scale;
        cfg.weight_phase *= scale;
        cfg.weight_quaternion *= scale;
    }

    // Use main compute function with NULL Kuramoto state
    return resonance_compute(query, target, &cfg, NULL, result);
}

float resonance_compute_jaccard_only(
    const resonance_query_t* query,
    const resonance_target_t* target)
{
    if (!query || !target) {
        set_error("NULL pointer in resonance_compute_jaccard_only");
        return -1.0f;
    }

    if (!query->signature || !target->signature) {
        set_error("NULL signature in query or target");
        return -1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    resonance_heartbeat("resonance_compute_jaccard_only", 0.0f);


    s_stats.total_computations++;
    return resonance_jaccard(query->signature, target->signature);
}

//=============================================================================
// Batch Computation Functions
//=============================================================================

int resonance_compute_batch(
    const resonance_query_t* query,
    const resonance_target_t* targets,
    size_t count,
    const resonance_config_t* config,
    const kuramoto_state_t* kuramoto_state,
    resonance_result_t* results)
{
    if (!query || !targets || !config || !results) {
        set_error("NULL pointer in batch computation");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "resonance_compute_batch: required parameter is NULL (query, targets, config, results)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    resonance_heartbeat("resonance_compute_batch", 0.0f);


    if (count == 0) {
        return 0;
    }

    if (count > RESONANCE_MAX_BATCH_SIZE) {
        set_error("Batch size %zu exceeds maximum %d", count, RESONANCE_MAX_BATCH_SIZE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "resonance_compute_batch: validation failed");
        return -1;
    }

    // Validate config once
    resonance_config_t cfg = *config;
    if (!resonance_config_validate(&cfg)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "resonance_compute_batch: resonance_config_validate is NULL");
        return -1;
    }

    int success_count = 0;
    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            resonance_heartbeat("resonance_loop",
                             (float)(i + 1) / (float)count);
        }

        if (resonance_compute(query, &targets[i], &cfg, kuramoto_state, &results[i])) {
            success_count++;
        }
    }

    s_stats.batch_computations++;
    clear_error();
    return success_count;
}

/**
 * @brief Comparison function for sorting batch results by total resonance (descending)
 */
static int compare_batch_results_desc(const void* a, const void* b) {
    const resonance_batch_result_t* ra = (const resonance_batch_result_t*)a;
    const resonance_batch_result_t* rb = (const resonance_batch_result_t*)b;

    if (rb->result.total > ra->result.total) return 1;
    if (rb->result.total < ra->result.total) return -1;
    return 0;
}

int resonance_compute_top_k(
    const resonance_query_t* query,
    const resonance_target_t* targets,
    size_t count,
    size_t k,
    const resonance_config_t* config,
    const kuramoto_state_t* kuramoto_state,
    resonance_batch_result_t* results)
{
    if (!query || !targets || !config || !results) {
        set_error("NULL pointer in top-K computation");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "resonance_compute_top_k: required parameter is NULL (query, targets, config, results)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    resonance_heartbeat("resonance_compute_top_k", 0.0f);


    if (count == 0 || k == 0) {
        return 0;
    }

    if (count > RESONANCE_MAX_BATCH_SIZE) {
        set_error("Batch size %zu exceeds maximum %d", count, RESONANCE_MAX_BATCH_SIZE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "resonance_compute_top_k: validation failed");
        return -1;
    }

    // Validate config
    resonance_config_t cfg = *config;
    if (!resonance_config_validate(&cfg)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "resonance_compute_top_k: resonance_config_validate is NULL");
        return -1;
    }

    // For small k or count, just compute all and sort
    // For larger datasets, a heap-based approach would be more efficient
    // TODO: Implement heap-based selection for O(N log K) when K << N

    // Allocate temporary array for all results
    resonance_batch_result_t* all_results =
        (resonance_batch_result_t*)nimcp_calloc(count, sizeof(resonance_batch_result_t));
    if (!all_results) {
        set_error("Failed to allocate memory for batch results");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "resonance_compute_top_k: all_results is NULL");
        return -1;
    }

    // Compute all resonances
    int success_count = 0;
    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            resonance_heartbeat("resonance_loop",
                             (float)(i + 1) / (float)count);
        }

        if (resonance_compute(query, &targets[i], &cfg, kuramoto_state,
                              &all_results[i].result)) {
            all_results[i].memory_id = targets[i].memory_id;
            success_count++;
        } else {
            // Mark failed computations with zero resonance
            all_results[i].memory_id = targets[i].memory_id;
            all_results[i].result.total = 0.0f;
        }
    }

    // Sort by total resonance (descending)
    qsort(all_results, count, sizeof(resonance_batch_result_t), compare_batch_results_desc);

    // Copy top-K to output
    size_t result_count = (k < count) ? k : count;
    memcpy(results, all_results, result_count * sizeof(resonance_batch_result_t));

    nimcp_free(all_results);
    all_results = NULL;
    s_stats.batch_computations++;
    clear_error();

    return (int)result_count;
}

bool resonance_compute_above_threshold(
    const resonance_query_t* query,
    const resonance_target_t* targets,
    size_t count,
    const resonance_config_t* config,
    const kuramoto_state_t* kuramoto_state,
    resonance_batch_result_t* results,
    size_t* result_count)
{
    if (!query || !targets || !config || !results || !result_count) {
        set_error("NULL pointer in threshold computation");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "resonance_compute_above_threshold: required parameter is NULL (query, targets, config, results, result_count)");
        return false;
    }

    *result_count = 0;

    /* Phase 8: Heartbeat at operation start */
    resonance_heartbeat("resonance_compute_above_thresh", 0.0f);


    if (count == 0) {
        clear_error();
        return true;
    }

    if (count > RESONANCE_MAX_BATCH_SIZE) {
        set_error("Batch size %zu exceeds maximum %d", count, RESONANCE_MAX_BATCH_SIZE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "resonance_compute_above_threshold: validation failed");
        return false;
    }

    // Validate config
    resonance_config_t cfg = *config;
    if (!resonance_config_validate(&cfg)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "resonance_compute_above_threshold: resonance_config_validate is NULL");
        return false;
    }

    // Linear scan with threshold filtering
    size_t found = 0;
    resonance_result_t temp_result;

    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            resonance_heartbeat("resonance_loop",
                             (float)(i + 1) / (float)count);
        }

        if (resonance_compute(query, &targets[i], &cfg, kuramoto_state, &temp_result)) {
            if (temp_result.above_threshold) {
                results[found].memory_id = targets[i].memory_id;
                results[found].result = temp_result;
                found++;
            }
        }
    }

    *result_count = found;
    s_stats.batch_computations++;
    clear_error();
    return true;
}

//=============================================================================
// Pink Noise Modulation Functions
//=============================================================================

float resonance_modulate(float base_score, float pink_sample, float depth) {
    // Clamp inputs
    /* Phase 8: Heartbeat at operation start */
    resonance_heartbeat("resonance_modulate", 0.0f);


    base_score = nimcp_clamp01(base_score);
    depth = nimcp_clamp01(depth);

    // Modulation: base * (1 + depth * pink_sample)
    // Pink sample is in [-1, +1], so modulation range is [1-depth, 1+depth]
    float modulated = base_score * (1.0f + depth * pink_sample);

    return nimcp_clamp01(modulated);
}

bool resonance_modulate_batch(
    float* scores,
    size_t count,
    const float* pink_buffer,
    float depth)
{
    if (!scores || !pink_buffer) {
        set_error("NULL pointer in batch modulation");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "resonance_modulate_batch: required parameter is NULL (scores, pink_buffer)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    resonance_heartbeat("resonance_modulate_batch", 0.0f);


    depth = nimcp_clamp01(depth);

    // Vectorizable loop
    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            resonance_heartbeat("resonance_loop",
                             (float)(i + 1) / (float)count);
        }

        scores[i] = nimcp_clamp01(scores[i] * (1.0f + depth * pink_buffer[i]));
    }

    clear_error();
    return true;
}

//=============================================================================
// Utility Functions
//=============================================================================

void resonance_result_print(const resonance_result_t* result) {
    if (!result) {
        printf("Resonance: (null result)\n");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    resonance_heartbeat("resonance_result_print", 0.0f);


    printf("Resonance: %.3f [Jaccard: %.3f, Phase: %.3f, Quat: %.3f, Kuramoto: %.3f] %s\n",
           result->total,
           result->jaccard_component,
           result->phase_component,
           result->quat_component,
           result->kuramoto_component,
           result->above_threshold ? "ABOVE" : "below");
}

size_t resonance_explain(const resonance_result_t* result, char* buf, size_t size) {
    if (!result || !buf || size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "resonance_explain: invalid parameters");

            return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    resonance_heartbeat("resonance_explain", 0.0f);


    const char* strength_total;
    if (result->total >= 0.8f) strength_total = "Very strong";
    else if (result->total >= 0.6f) strength_total = "Strong";
    else if (result->total >= 0.4f) strength_total = "Moderate";
    else if (result->total >= 0.2f) strength_total = "Weak";
    else strength_total = "Very weak";

    // Determine which component contributed most
    float max_comp = result->jaccard_component;
    const char* max_name = "content";
    if (result->phase_component > max_comp) {
        max_comp = result->phase_component;
        max_name = "phase coherence";
    }
    if (result->quat_component > max_comp) {
        max_comp = result->quat_component;
        max_name = "semantic state";
    }
    if (result->kuramoto_component > max_comp) {
        max_name = "oscillator sync";
    }

    int written = snprintf(buf, size,
        "%s resonance (%.2f): "
        "content=%.2f, phase=%.2f, semantic=%.2f, sync=%.2f. "
        "Primary factor: %s. %s threshold.",
        strength_total,
        result->total,
        result->jaccard_component,
        result->phase_component,
        result->quat_component,
        result->kuramoto_component,
        max_name,
        result->above_threshold ? "Above" : "Below");

    return (written > 0) ? (size_t)written : 0;
}

void resonance_query_init(resonance_query_t* query) {
    if (!query) return;

    /* Phase 8: Heartbeat at operation start */
    resonance_heartbeat("resonance_query_init", 0.0f);


    memset(query, 0, sizeof(*query));
    query->quaternion = quat_identity();
    query->phase = 0.0f;
    query->module_id = 0;
}

void resonance_target_init(resonance_target_t* target) {
    if (!target) return;

    /* Phase 8: Heartbeat at operation start */
    resonance_heartbeat("resonance_target_init", 0.0f);


    memset(target, 0, sizeof(*target));
    target->quaternion = quat_identity();
    target->phase = 0.0f;
    target->module_id = 0;
    target->memory_id = 0;
}

void resonance_get_stats(resonance_stats_t* stats) {
    if (!stats) return;
    *stats = s_stats;
    /* Phase 8: Heartbeat at operation start */
    resonance_heartbeat("resonance_get_stats", 0.0f);


}

void resonance_reset_stats(void) {
    /* Phase 8: Heartbeat at operation start */
    resonance_heartbeat("resonance_reset_stats", 0.0f);


    memset(&s_stats, 0, sizeof(s_stats));
}

const char* resonance_get_last_error(void) {
    return s_last_error[0] ? s_last_error : NULL;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void resonance_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_resonance_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration
 * ============================================================================ */

int resonance_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "resonance_training_begin: NULL argument");
        return -1;
    }
    resonance_heartbeat_instance(NULL, "resonance_training_begin", 0.0f);

    resonance_config_t* cfg = (resonance_config_t*)instance;

    /* Reset stats for training baseline measurement */
    memset(&s_stats, 0, sizeof(s_stats));

    /* Clear error state */
    clear_error();

    /* Initialize weights to balanced starting point for training */
    cfg->weight_jaccard = RESONANCE_DEFAULT_WEIGHT_JACCARD;
    cfg->weight_phase = RESONANCE_DEFAULT_WEIGHT_PHASE;
    cfg->weight_quaternion = RESONANCE_DEFAULT_WEIGHT_QUATERNION;
    cfg->weight_kuramoto = RESONANCE_DEFAULT_WEIGHT_KURAMOTO;

    resonance_heartbeat_instance(NULL, "resonance_training_begin", 1.0f);
    return 0;
}

int resonance_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "resonance_training_end: NULL argument");
        return -1;
    }
    resonance_heartbeat_instance(NULL, "resonance_training_end", 0.0f);

    resonance_config_t* cfg = (resonance_config_t*)instance;

    /* Normalize weights after training to ensure they sum to 1.0 */
    float weight_sum = cfg->weight_jaccard + cfg->weight_phase +
                       cfg->weight_quaternion + cfg->weight_kuramoto;
    if (weight_sum > 1e-6f) {
        cfg->weight_jaccard /= weight_sum;
        cfg->weight_phase /= weight_sum;
        cfg->weight_quaternion /= weight_sum;
        cfg->weight_kuramoto /= weight_sum;
    }

    /* Capture training quality metrics */
    uint64_t total_comps = s_stats.total_computations;
    float mean_res = s_stats.mean_resonance;
    (void)total_comps;
    (void)mean_res;

    resonance_heartbeat_instance(NULL, "resonance_training_end", 1.0f);
    return 0;
}

int resonance_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "resonance_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    resonance_heartbeat_instance(NULL, "resonance_training_step", progress);

    resonance_config_t* cfg = (resonance_config_t*)instance;

    /*
     * Adapt resonance weights during training based on progress:
     * - Early: emphasize content (Jaccard) for establishing base associations
     * - Mid: balanced across all components
     * - Late: emphasize quaternion (emotional/state) for fine-tuning
     */
    float lr = 0.01f * (1.0f - 0.8f * progress);  /* Anneal learning rate */

    /* Shift toward quaternion/phase emphasis as training progresses */
    float jaccard_target = 0.35f - 0.1f * progress;    /* 0.35 -> 0.25 */
    float phase_target = 0.15f + 0.05f * progress;     /* 0.15 -> 0.20 */
    float quat_target = 0.25f + 0.1f * progress;       /* 0.25 -> 0.35 */
    float kuramoto_target = 0.25f - 0.05f * progress;  /* 0.25 -> 0.20 */

    cfg->weight_jaccard += lr * (jaccard_target - cfg->weight_jaccard);
    cfg->weight_phase += lr * (phase_target - cfg->weight_phase);
    cfg->weight_quaternion += lr * (quat_target - cfg->weight_quaternion);
    cfg->weight_kuramoto += lr * (kuramoto_target - cfg->weight_kuramoto);

    /* Ensure weights stay positive */
    if (cfg->weight_jaccard < 0.01f) cfg->weight_jaccard = 0.01f;
    if (cfg->weight_phase < 0.01f) cfg->weight_phase = 0.01f;
    if (cfg->weight_quaternion < 0.01f) cfg->weight_quaternion = 0.01f;
    if (cfg->weight_kuramoto < 0.01f) cfg->weight_kuramoto = 0.01f;

    /* Adapt threshold: lower early for exploration, higher late for precision */
    cfg->threshold = 0.15f + 0.2f * progress;

    return 0;
}
