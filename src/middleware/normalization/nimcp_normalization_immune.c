/**
 * @file nimcp_normalization_immune.c
 * @brief Normalization-Immune System Integration Implementation
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Integration between normalization modules and brain immune system
 * WHY:  Model fever-induced baseline shifts and outlier-based threat detection
 * HOW:  Cytokines modulate normalization parameters; outliers trigger antigens
 */

#include "middleware/normalization/nimcp_normalization_immune.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <float.h>

#define LOG_MODULE "normalization_immune"
#define LOG_MODULE_ID 0x0900
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(normalization_immune)

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

/**
 * @brief Create normalization immune context
 *
 * WHAT: Allocate and initialize integration context
 * WHY:  Set up immune-normalization coordination
 * HOW:  Allocate structures, initialize defaults
 */
normalization_immune_context_t* normalization_immune_create(
    brain_immune_system_t* immune_system,
    size_t num_channels
) {
    if (!immune_system || num_channels == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "normalization_immune_create: immune_system is NULL");
        return NULL;
    }

    normalization_immune_context_t* ctx = nimcp_calloc(1, sizeof(normalization_immune_context_t));
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ctx is NULL");

        return NULL;
    }

    ctx->immune_system = immune_system;
    ctx->num_channels = num_channels;

    /* Allocate outlier storage */
    ctx->outlier_capacity = NORMALIZATION_IMMUNE_MAX_OUTLIERS;
    ctx->outliers = nimcp_calloc(ctx->outlier_capacity, sizeof(normalization_outlier_t));
    if (!ctx->outliers) {
        nimcp_free(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "normalization_immune_create: ctx->outliers is NULL");
        return NULL;
    }

    /* Allocate baseline storage for restoration */
    ctx->adaptive_original_lr = nimcp_calloc(num_channels, sizeof(float));
    ctx->homeostatic_original_target = nimcp_calloc(num_channels, sizeof(float));
    ctx->zscore_original_mean = nimcp_calloc(num_channels, sizeof(float));

    if (!ctx->adaptive_original_lr || !ctx->homeostatic_original_target ||
        !ctx->zscore_original_mean) {
        nimcp_free(ctx->adaptive_original_lr);
        nimcp_free(ctx->homeostatic_original_target);
        nimcp_free(ctx->zscore_original_mean);
        nimcp_free(ctx->outliers);
        nimcp_free(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "normalization_immune_create: operation failed");
        return NULL;
    }

    /* Initialize modulation to defaults */
    ctx->modulation.adaptive_learning_rate_factor = 1.0f;
    ctx->modulation.homeostatic_target_shift = 0.0f;
    ctx->modulation.homeostatic_time_constant_factor = 1.0f;
    ctx->modulation.zscore_mean_shift = 0.0f;
    ctx->modulation.zscore_variance_scale = 1.0f;
    ctx->modulation.zscore_outlier_threshold = NORMALIZATION_IMMUNE_ZSCORE_THRESHOLD;
    ctx->modulation.minmax_range_expansion = 1.0f;
    ctx->modulation.inflammation = INFLAMMATION_NONE;

    ctx->enabled = true;
    ctx->next_outlier_id = 1;

    return ctx;
}

/**
 * @brief Destroy normalization immune context
 *
 * WHAT: Clean up and deallocate context
 * WHY:  Prevent memory leaks
 * HOW:  Free all allocated structures
 */
void normalization_immune_destroy(normalization_immune_context_t* ctx) {
    if (!ctx) {
        return;
    }

    nimcp_free(ctx->outliers);
    nimcp_free(ctx->adaptive_original_lr);
    nimcp_free(ctx->homeostatic_original_target);
    nimcp_free(ctx->zscore_original_mean);
    nimcp_free(ctx);
}

/* ============================================================================
 * Connection API Implementation
 * ============================================================================ */

/**
 * @brief Connect adaptive normalizer
 *
 * WHAT: Link adaptive normalizer to integration
 * WHY:  Enable learning rate modulation
 * HOW:  Store reference, capture baselines
 */
int normalization_immune_connect_adaptive(
    normalization_immune_context_t* ctx,
    adaptive_normalizer_t* adaptive
) {
    if (!ctx || !adaptive) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "normalization_immune_connect_adaptive: required parameter is NULL (ctx, adaptive)");
        return -1;
    }

    ctx->adaptive = adaptive;

    /* Capture original learning rates (assume default for now) */
    for (size_t i = 0; i < ctx->num_channels; i++) {
        ctx->adaptive_original_lr[i] = 0.01f;  /* Default learning rate */
    }

    return 0;
}

/**
 * @brief Connect homeostatic normalizer
 *
 * WHAT: Link homeostatic normalizer to integration
 * WHY:  Enable target activity shifts
 * HOW:  Store reference, capture baselines
 */
int normalization_immune_connect_homeostatic(
    normalization_immune_context_t* ctx,
    homeostatic_normalizer_t* homeostatic
) {
    if (!ctx || !homeostatic) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "normalization_immune_connect_homeostatic: required parameter is NULL (ctx, homeostatic)");
        return -1;
    }

    ctx->homeostatic = homeostatic;

    /* Capture original target activities (assume default for now) */
    for (size_t i = 0; i < ctx->num_channels; i++) {
        ctx->homeostatic_original_target[i] = 0.5f;  /* Default target */
    }

    return 0;
}

/**
 * @brief Connect z-score normalizer
 *
 * WHAT: Link z-score normalizer to integration
 * WHY:  Enable mean shifts and outlier detection
 * HOW:  Store reference, capture statistics
 */
int normalization_immune_connect_zscore(
    normalization_immune_context_t* ctx,
    zscore_normalizer_t* zscore
) {
    if (!ctx || !zscore) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "normalization_immune_connect_zscore: required parameter is NULL (ctx, zscore)");
        return -1;
    }

    ctx->zscore = zscore;

    /* Capture original means */
    for (size_t i = 0; i < ctx->num_channels; i++) {
        ctx->zscore_original_mean[i] = zscore_normalizer_mean(zscore, i);
    }

    return 0;
}

/**
 * @brief Connect min-max normalizer
 *
 * WHAT: Link min-max normalizer to integration
 * WHY:  Enable range expansion
 * HOW:  Store opaque reference
 */
int normalization_immune_connect_minmax(
    normalization_immune_context_t* ctx,
    void* minmax
) {
    if (!ctx || !minmax) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "normalization_immune_connect_minmax: required parameter is NULL (ctx, minmax)");
        return -1;
    }

    ctx->minmax = minmax;
    return 0;
}

/* ============================================================================
 * Outlier Detection Implementation
 * ============================================================================ */

/**
 * @brief Create outlier event
 *
 * WHAT: Allocate and initialize outlier structure
 * WHY:  Track detected anomalies
 * HOW:  Add to outlier array
 */
static normalization_outlier_t* create_outlier(
    normalization_immune_context_t* ctx,
    normalization_anomaly_type_t type,
    normalizer_type_t normalizer,
    size_t channel,
    float value,
    float zscore,
    float severity
) {
    if (!ctx || ctx->outlier_count >= ctx->outlier_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "create_outlier: ctx is NULL");
        return NULL;
    }

    normalization_outlier_t* outlier = &ctx->outliers[ctx->outlier_count++];
    outlier->id = ctx->next_outlier_id++;
    outlier->type = type;
    outlier->normalizer = normalizer;
    outlier->channel = channel;
    outlier->value = value;
    outlier->zscore = zscore;
    outlier->severity = severity;
    outlier->timestamp_ms = 0;  /* Should be filled by caller */
    outlier->antigen_id = 0;
    outlier->immune_responded = false;

    return outlier;
}

/**
 * @brief Detect z-score outliers
 *
 * WHAT: Check if value exceeds z-score threshold
 * WHY:  Statistical outliers indicate anomalies
 * HOW:  Compare |zscore| to threshold, create antigen
 */
int normalization_immune_detect_zscore_outlier(
    normalization_immune_context_t* ctx,
    size_t channel,
    float value,
    float zscore,
    uint32_t* outlier_id
) {
    if (!ctx || !ctx->enabled || !outlier_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "normalization_immune_detect_zscore_outlier: required parameter is NULL (ctx, ctx->enabled, outlier_id)");
        return -1;
    }

    float threshold = ctx->modulation.zscore_outlier_threshold;

    /* Check if outlier (use < so that exact threshold value is an outlier) */
    if (fabsf(zscore) < threshold) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "normalization_immune_detect_zscore_outlier: validation failed");
        return -1;  /* Not an outlier */
    }

    /* Create outlier event */
    float severity = fminf(fabsf(zscore) / (threshold * 2.0f), 1.0f);
    normalization_outlier_t* outlier = create_outlier(
        ctx,
        NORMALIZATION_ANOMALY_ZSCORE_OUTLIER,
        NORMALIZER_ZSCORE,
        channel,
        value,
        zscore,
        severity
    );

    if (!outlier) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "outlier is NULL");


        return -1;
    }

    *outlier_id = outlier->id;
    ctx->outliers_detected++;

    /* Present as antigen to immune system */
    uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE];
    memcpy(epitope, &value, sizeof(float));
    memcpy(epitope + sizeof(float), &zscore, sizeof(float));
    memcpy(epitope + 2 * sizeof(float), &channel, sizeof(size_t));

    uint32_t antigen_id;
    int result = brain_immune_present_antigen(
        ctx->immune_system,
        ANTIGEN_SOURCE_ANOMALY,
        epitope,
        3 * sizeof(float),
        (uint32_t)(severity * 10.0f),
        (uint32_t)channel,
        &antigen_id
    );

    if (result == 0) {
        outlier->antigen_id = antigen_id;
        outlier->immune_responded = true;
        ctx->immune_alerts_triggered++;
    }

    return 0;
}

/**
 * @brief Detect rapid baseline shift
 *
 * WHAT: Detect unexpectedly fast baseline change
 * WHY:  Rapid shifts may indicate corruption
 * HOW:  Compute velocity, compare to threshold
 */
int normalization_immune_detect_rapid_shift(
    normalization_immune_context_t* ctx,
    normalizer_type_t normalizer,
    size_t channel,
    float old_baseline,
    float new_baseline,
    uint64_t delta_ms,
    uint32_t* outlier_id
) {
    if (!ctx || !ctx->enabled || !outlier_id || delta_ms == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "normalization_immune_detect_rapid_shift: required parameter is NULL (ctx, ctx->enabled, outlier_id)");
        return -1;
    }

    /* Compute velocity (units per second) */
    float shift = fabsf(new_baseline - old_baseline);
    float velocity = (shift * 1000.0f) / (float)delta_ms;

    /* Threshold: 0.5 units/sec is considered rapid */
    float threshold = 0.5f;

    if (velocity <= threshold) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "normalization_immune_detect_rapid_shift: validation failed");
        return -1;  /* Not rapid */
    }

    /* Create outlier event */
    float severity = fminf(velocity / (threshold * 2.0f), 1.0f);
    normalization_outlier_t* outlier = create_outlier(
        ctx,
        NORMALIZATION_ANOMALY_RAPID_SHIFT,
        normalizer,
        channel,
        new_baseline,
        velocity,
        severity
    );

    if (!outlier) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "outlier is NULL");


        return -1;
    }

    *outlier_id = outlier->id;
    ctx->outliers_detected++;

    /* Present as antigen */
    uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE];
    memcpy(epitope, &old_baseline, sizeof(float));
    memcpy(epitope + sizeof(float), &new_baseline, sizeof(float));
    memcpy(epitope + 2 * sizeof(float), &velocity, sizeof(float));

    uint32_t antigen_id;
    int result = brain_immune_present_antigen(
        ctx->immune_system,
        ANTIGEN_SOURCE_ANOMALY,
        epitope,
        3 * sizeof(float),
        (uint32_t)(severity * 10.0f),
        (uint32_t)channel,
        &antigen_id
    );

    if (result == 0) {
        outlier->antigen_id = antigen_id;
        outlier->immune_responded = true;
        ctx->immune_alerts_triggered++;
    }

    return 0;
}

/**
 * @brief Detect homeostatic drift
 *
 * WHAT: Detect excessive homeostatic target drift
 * WHY:  Uncontrolled drift indicates failure
 * HOW:  Compare drift to threshold
 */
int normalization_immune_detect_homeostatic_drift(
    normalization_immune_context_t* ctx,
    size_t channel,
    float target_drift,
    uint32_t* outlier_id
) {
    if (!ctx || !ctx->enabled || !outlier_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "normalization_immune_detect_homeostatic_drift: required parameter is NULL (ctx, ctx->enabled, outlier_id)");
        return -1;
    }

    float threshold = 0.3f;  /* 30% drift is excessive */

    if (fabsf(target_drift) <= threshold) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "normalization_immune_detect_homeostatic_drift: validation failed");
        return -1;  /* Not excessive */
    }

    /* Create outlier event */
    float severity = fminf(fabsf(target_drift) / (threshold * 2.0f), 1.0f);
    normalization_outlier_t* outlier = create_outlier(
        ctx,
        NORMALIZATION_ANOMALY_HOMEOSTATIC_DRIFT,
        NORMALIZER_HOMEOSTATIC,
        channel,
        target_drift,
        0.0f,
        severity
    );

    if (!outlier) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "outlier is NULL");


        return -1;
    }

    *outlier_id = outlier->id;
    ctx->outliers_detected++;

    /* Present as antigen */
    uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE];
    memcpy(epitope, &target_drift, sizeof(float));
    memcpy(epitope + sizeof(float), &channel, sizeof(size_t));

    uint32_t antigen_id;
    int result = brain_immune_present_antigen(
        ctx->immune_system,
        ANTIGEN_SOURCE_ANOMALY,
        epitope,
        2 * sizeof(float),
        (uint32_t)(severity * 10.0f),
        (uint32_t)channel,
        &antigen_id
    );

    if (result == 0) {
        outlier->antigen_id = antigen_id;
        outlier->immune_responded = true;
        ctx->immune_alerts_triggered++;
    }

    return 0;
}

/**
 * @brief Report range violation
 *
 * WHAT: Report value outside expected range
 * WHY:  Range violations indicate corruption
 * HOW:  Create outlier, present as antigen
 */
int normalization_immune_report_range_violation(
    normalization_immune_context_t* ctx,
    size_t channel,
    float value,
    float min,
    float max,
    uint32_t* outlier_id
) {
    if (!ctx || !ctx->enabled || !outlier_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "normalization_immune_report_range_violation: required parameter is NULL (ctx, ctx->enabled, outlier_id)");
        return -1;
    }

    /* Compute severity based on how far outside range */
    float range = max - min;
    float excess = 0.0f;

    if (value < min) {
        excess = min - value;
    } else if (value > max) {
        excess = value - max;
    }

    float severity = fminf(excess / range, 1.0f);

    /* Create outlier event */
    normalization_outlier_t* outlier = create_outlier(
        ctx,
        NORMALIZATION_ANOMALY_RANGE_VIOLATION,
        NORMALIZER_MINMAX,
        channel,
        value,
        0.0f,
        severity
    );

    if (!outlier) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "outlier is NULL");


        return -1;
    }

    *outlier_id = outlier->id;
    ctx->outliers_detected++;

    /* Present as antigen */
    uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE];
    memcpy(epitope, &value, sizeof(float));
    memcpy(epitope + sizeof(float), &min, sizeof(float));
    memcpy(epitope + 2 * sizeof(float), &max, sizeof(float));

    uint32_t antigen_id;
    int result = brain_immune_present_antigen(
        ctx->immune_system,
        ANTIGEN_SOURCE_ANOMALY,
        epitope,
        3 * sizeof(float),
        (uint32_t)(severity * 10.0f),
        (uint32_t)channel,
        &antigen_id
    );

    if (result == 0) {
        outlier->antigen_id = antigen_id;
        outlier->immune_responded = true;
        ctx->immune_alerts_triggered++;
    }

    return 0;
}

/* ============================================================================
 * Immune Modulation Implementation
 * ============================================================================ */

/**
 * @brief Update modulation from immune state
 *
 * WHAT: Query immune system, compute parameter shifts
 * WHY:  Synchronize normalization with immune state
 * HOW:  Read inflammation/cytokines, update modulation
 */
int normalization_immune_update_modulation(normalization_immune_context_t* ctx) {
    if (!ctx || !ctx->enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "normalization_immune_update_modulation: required parameter is NULL (ctx, ctx->enabled)");
        return -1;
    }

    /* Get immune system stats */
    brain_immune_stats_t stats;
    if (brain_immune_get_stats(ctx->immune_system, &stats) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "normalization_immune_update_modulation: validation failed");
        return -1;
    }

    /* Query actual inflammation level from immune system */
    ctx->modulation.inflammation = brain_immune_get_inflammation_level(ctx->immune_system);

    /* Simplified cytokine levels (in real implementation, query from immune system) */
    /* For now, derive from inflammation level */
    switch (ctx->modulation.inflammation) {
        case INFLAMMATION_NONE:
            ctx->modulation.il1_level = 0.0f;
            ctx->modulation.il6_level = 0.0f;
            ctx->modulation.il10_level = 0.0f;
            ctx->modulation.tnf_alpha_level = 0.0f;
            break;
        case INFLAMMATION_LOCAL:
            ctx->modulation.il1_level = 0.2f;
            ctx->modulation.il6_level = 0.3f;
            ctx->modulation.il10_level = 0.1f;
            ctx->modulation.tnf_alpha_level = 0.1f;
            break;
        case INFLAMMATION_REGIONAL:
            ctx->modulation.il1_level = 0.5f;
            ctx->modulation.il6_level = 0.6f;
            ctx->modulation.il10_level = 0.2f;
            ctx->modulation.tnf_alpha_level = 0.4f;
            break;
        case INFLAMMATION_SYSTEMIC:
            ctx->modulation.il1_level = 0.8f;
            ctx->modulation.il6_level = 0.9f;
            ctx->modulation.il10_level = 0.3f;
            ctx->modulation.tnf_alpha_level = 0.7f;
            break;
        case INFLAMMATION_STORM:
            ctx->modulation.il1_level = 1.0f;
            ctx->modulation.il6_level = 1.0f;
            ctx->modulation.il10_level = 0.5f;
            ctx->modulation.tnf_alpha_level = 1.0f;
            break;
    }

    /* Apply modulations */
    if (ctx->modulation.il6_level > 0.1f) {
        normalization_immune_apply_fever_shift(ctx, ctx->modulation.il6_level);
    }

    if (ctx->modulation.inflammation == INFLAMMATION_STORM) {
        normalization_immune_apply_storm_clamping(ctx);
    }

    normalization_immune_apply_learning_rate_modulation(ctx);
    normalization_immune_apply_homeostatic_shift(ctx);
    normalization_immune_apply_variance_expansion(ctx);
    normalization_immune_apply_range_expansion(ctx);

    return 0;
}

/**
 * @brief Apply fever baseline shift
 *
 * WHAT: Shift baselines due to fever (IL-6)
 * WHY:  Model physiological fever response
 * HOW:  Increase z-score mean, homeostatic target
 */
int normalization_immune_apply_fever_shift(
    normalization_immune_context_t* ctx,
    float il6_level
) {
    if (!ctx || !ctx->enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "normalization_immune_apply_fever_shift: required parameter is NULL (ctx, ctx->enabled)");
        return -1;
    }

    if (il6_level < 0.1f) {
        ctx->modulation.fever_shift_active = false;
        return 0;
    }

    ctx->modulation.fever_shift_active = true;
    ctx->fever_shifts_applied++;

    /* Fever shift magnitude proportional to IL-6 */
    float shift = il6_level * NORMALIZATION_IMMUNE_BASELINE_FEVER_SHIFT;

    ctx->modulation.zscore_mean_shift = shift;
    ctx->modulation.homeostatic_target_shift = shift * 0.2f;  /* Smaller shift for homeostatic */

    return 0;
}

/**
 * @brief Apply cytokine storm clamping
 *
 * WHAT: Clamp normalization during storm
 * WHY:  Prevent runaway normalization
 * HOW:  Tighten thresholds, reduce ranges
 */
int normalization_immune_apply_storm_clamping(
    normalization_immune_context_t* ctx
) {
    if (!ctx || !ctx->enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "normalization_immune_apply_storm_clamping: required parameter is NULL (ctx, ctx->enabled)");
        return -1;
    }

    ctx->modulation.storm_clamping_active = true;
    ctx->storm_clamps_activated++;

    /* Tighten outlier threshold to prevent false positives */
    ctx->modulation.zscore_outlier_threshold = NORMALIZATION_IMMUNE_STORM_CLAMP_FACTOR;

    /* Reduce acceptable ranges */
    ctx->modulation.minmax_range_expansion = 0.5f;

    return 0;
}

/**
 * @brief Apply learning rate modulation
 *
 * WHAT: Adjust adaptive normalizer learning rate
 * WHY:  Inflammation should slow adaptation
 * HOW:  Reduce learning rate based on cytokines
 */
int normalization_immune_apply_learning_rate_modulation(
    normalization_immune_context_t* ctx
) {
    if (!ctx || !ctx->enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "normalization_immune_apply_learning_rate_modulation: required parameter is NULL (ctx, ctx->enabled)");
        return -1;
    }

    /* Inflammation reduces learning rate (conservative during threat) */
    float inflammation_factor = (ctx->modulation.il1_level + ctx->modulation.tnf_alpha_level) / 2.0f;
    ctx->modulation.adaptive_learning_rate_factor = 1.0f - (inflammation_factor * 0.7f);

    /* Clamp to safe range */
    if (ctx->modulation.adaptive_learning_rate_factor < 0.3f) {
        ctx->modulation.adaptive_learning_rate_factor = 0.3f;
    }

    return 0;
}

/**
 * @brief Apply homeostatic target shift
 *
 * WHAT: Shift homeostatic target activity
 * WHY:  Fever shifts set-points
 * HOW:  Adjust target based on fever shift
 */
int normalization_immune_apply_homeostatic_shift(
    normalization_immune_context_t* ctx
) {
    if (!ctx || !ctx->enabled || !ctx->homeostatic) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "normalization_immune_apply_homeostatic_shift: required parameter is NULL (ctx, ctx->enabled, ctx->homeostatic)");
        return -1;
    }

    /* Already computed in apply_fever_shift */
    /* This function could apply the shift to the actual normalizer */
    /* For now, just record the intended shift */

    return 0;
}

/**
 * @brief Apply z-score variance expansion
 *
 * WHAT: Expand acceptable variance
 * WHY:  Allow greater variability during immune response
 * HOW:  Scale variance by (1 + IL-1)
 */
int normalization_immune_apply_variance_expansion(
    normalization_immune_context_t* ctx
) {
    if (!ctx || !ctx->enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "normalization_immune_apply_variance_expansion: required parameter is NULL (ctx, ctx->enabled)");
        return -1;
    }

    /* Expand variance during inflammation */
    ctx->modulation.zscore_variance_scale = 1.0f + ctx->modulation.il1_level;

    return 0;
}

/**
 * @brief Apply min-max range expansion
 *
 * WHAT: Expand normalization range
 * WHY:  Tolerate wider values during immune activity
 * HOW:  Expand range by inflammation factor
 */
int normalization_immune_apply_range_expansion(
    normalization_immune_context_t* ctx
) {
    if (!ctx || !ctx->enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "normalization_immune_apply_range_expansion: required parameter is NULL (ctx, ctx->enabled)");
        return -1;
    }

    /* Expand range during inflammation (unless storm clamping active) */
    if (!ctx->modulation.storm_clamping_active) {
        float inflammation_factor = (ctx->modulation.il1_level + ctx->modulation.il6_level) / 2.0f;
        ctx->modulation.minmax_range_expansion = 1.0f + (inflammation_factor * 0.5f);
    }

    return 0;
}

/* ============================================================================
 * Baseline Restoration Implementation
 * ============================================================================ */

/**
 * @brief Restore baselines after resolution
 *
 * WHAT: Gradually return to original baselines
 * WHY:  Smooth transition after threat cleared
 * HOW:  Interpolate toward originals based on IL-10
 */
int normalization_immune_restore_baselines(
    normalization_immune_context_t* ctx,
    float il10_level,
    uint64_t delta_ms
) {
    if (!ctx || !ctx->enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "normalization_immune_restore_baselines: required parameter is NULL (ctx, ctx->enabled)");
        return -1;
    }

    if (il10_level < 0.1f) {
        return 0;  /* No restoration without IL-10 */
    }

    /* Restoration rate: 2.0 per second at full IL-10 (fast anti-inflammatory response) */
    float restoration_rate = il10_level * 2.0f * (delta_ms / 1000.0f);
    if (restoration_rate > 0.25f) restoration_rate = 0.25f;  /* Cap per-step rate */

    /* Restore z-score mean shift */
    ctx->modulation.zscore_mean_shift *= (1.0f - restoration_rate);
    if (fabsf(ctx->modulation.zscore_mean_shift) < 0.01f) {
        ctx->modulation.zscore_mean_shift = 0.0f;
        ctx->modulation.fever_shift_active = false;
        ctx->baseline_restorations++;
    }

    /* Restore homeostatic target shift */
    ctx->modulation.homeostatic_target_shift *= (1.0f - restoration_rate);

    /* Restore variance scale */
    float variance_target = 1.0f;
    ctx->modulation.zscore_variance_scale =
        ctx->modulation.zscore_variance_scale +
        (variance_target - ctx->modulation.zscore_variance_scale) * restoration_rate;

    /* Restore learning rate factor */
    float lr_target = 1.0f;
    ctx->modulation.adaptive_learning_rate_factor =
        ctx->modulation.adaptive_learning_rate_factor +
        (lr_target - ctx->modulation.adaptive_learning_rate_factor) * restoration_rate;

    /* Restore outlier threshold */
    float threshold_target = NORMALIZATION_IMMUNE_ZSCORE_THRESHOLD;
    ctx->modulation.zscore_outlier_threshold =
        ctx->modulation.zscore_outlier_threshold +
        (threshold_target - ctx->modulation.zscore_outlier_threshold) * restoration_rate;

    /* Restore range expansion */
    float range_target = 1.0f;
    ctx->modulation.minmax_range_expansion =
        ctx->modulation.minmax_range_expansion +
        (range_target - ctx->modulation.minmax_range_expansion) * restoration_rate;

    /* Disable storm clamping if IL-10 is high */
    if (il10_level > 0.5f && ctx->modulation.storm_clamping_active) {
        ctx->modulation.storm_clamping_active = false;
    }

    return 0;
}

/**
 * @brief Capture current baselines
 *
 * WHAT: Save current parameters as originals
 * WHY:  Enable restoration after immune events
 * HOW:  Copy current values to original arrays
 */
int normalization_immune_capture_baselines(
    normalization_immune_context_t* ctx
) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return -1;
    }

    /* Capture z-score means */
    if (ctx->zscore) {
        for (size_t i = 0; i < ctx->num_channels; i++) {
            ctx->zscore_original_mean[i] = zscore_normalizer_mean(ctx->zscore, i);
        }
    }

    /* Other normalizers: would need API to query current parameters */
    /* For now, originals are set at connection time */

    return 0;
}

/* ============================================================================
 * Query API Implementation
 * ============================================================================ */

/**
 * @brief Get current modulation state
 *
 * WHAT: Return modulation structure
 * WHY:  Allow monitoring of immune effects
 * HOW:  Copy modulation to output
 */
int normalization_immune_get_modulation(
    const normalization_immune_context_t* ctx,
    normalization_immune_modulation_t* modulation
) {
    if (!ctx || !modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "normalization_immune_get_modulation: required parameter is NULL (ctx, modulation)");
        return -1;
    }

    *modulation = ctx->modulation;
    return 0;
}

/**
 * @brief Get outlier by ID
 *
 * WHAT: Find outlier in history
 * WHY:  Retrieve outlier details
 * HOW:  Linear search by ID
 */
const normalization_outlier_t* normalization_immune_get_outlier(
    const normalization_immune_context_t* ctx,
    uint32_t outlier_id
) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return NULL;
    }

    for (size_t i = 0; i < ctx->outlier_count; i++) {
        if (ctx->outliers[i].id == outlier_id) {
            return &ctx->outliers[i];
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "normalization_immune_get_outlier: validation failed");
    return NULL;
}

/**
 * @brief Check if fever shift is active
 *
 * WHAT: Return fever shift flag
 * WHY:  Monitor fever state
 * HOW:  Return flag from modulation
 */
bool normalization_immune_is_fever_active(
    const normalization_immune_context_t* ctx
) {
    return ctx ? ctx->modulation.fever_shift_active : false;
}

/**
 * @brief Check if storm clamping is active
 *
 * WHAT: Return storm clamping flag
 * WHY:  Monitor storm protection
 * HOW:  Return flag from modulation
 */
bool normalization_immune_is_storm_clamping_active(
    const normalization_immune_context_t* ctx
) {
    return ctx ? ctx->modulation.storm_clamping_active : false;
}

/**
 * @brief Get current inflammation level
 *
 * WHAT: Return inflammation enum
 * WHY:  Monitor immune state
 * HOW:  Return from modulation
 */
brain_inflammation_level_t normalization_immune_get_inflammation(
    const normalization_immune_context_t* ctx
) {
    return ctx ? ctx->modulation.inflammation : INFLAMMATION_NONE;
}

/**
 * @brief Get fever shift magnitude
 *
 * WHAT: Return amount of baseline shift
 * WHY:  Monitor fever effects
 * HOW:  Return difference from original
 */
float normalization_immune_get_fever_shift(
    const normalization_immune_context_t* ctx,
    normalizer_type_t normalizer,
    size_t channel
) {
    if (!ctx || channel >= ctx->num_channels) {
        return 0.0f;
    }

    switch (normalizer) {
        case NORMALIZER_ZSCORE:
            return ctx->modulation.zscore_mean_shift;
        case NORMALIZER_HOMEOSTATIC:
            return ctx->modulation.homeostatic_target_shift;
        default:
            return 0.0f;
    }
}

/* ============================================================================
 * Utility Functions Implementation
 * ============================================================================ */

/**
 * @brief Convert anomaly type to string
 *
 * WHAT: Return human-readable anomaly type
 * WHY:  Debugging and logging
 * HOW:  Switch on enum
 */
const char* normalization_immune_anomaly_type_to_string(
    normalization_anomaly_type_t type
) {
    switch (type) {
        case NORMALIZATION_ANOMALY_ZSCORE_OUTLIER:
            return "ZScoreOutlier";
        case NORMALIZATION_ANOMALY_RANGE_VIOLATION:
            return "RangeViolation";
        case NORMALIZATION_ANOMALY_RAPID_SHIFT:
            return "RapidShift";
        case NORMALIZATION_ANOMALY_HOMEOSTATIC_DRIFT:
            return "HomeostaticDrift";
        case NORMALIZATION_ANOMALY_ADAPTATION_FAILURE:
            return "AdaptationFailure";
        default:
            return "Unknown";
    }
}

/**
 * @brief Convert normalizer type to string
 *
 * WHAT: Return human-readable normalizer type
 * WHY:  Debugging and logging
 * HOW:  Switch on enum
 */
const char* normalization_immune_normalizer_to_string(
    normalizer_type_t normalizer
) {
    switch (normalizer) {
        case NORMALIZER_ADAPTIVE:
            return "Adaptive";
        case NORMALIZER_HOMEOSTATIC:
            return "Homeostatic";
        case NORMALIZER_ZSCORE:
            return "ZScore";
        case NORMALIZER_MINMAX:
            return "MinMax";
        default:
            return "Unknown";
    }
}
