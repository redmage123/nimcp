/**
 * @file nimcp_visual_jepa_fep_bridge.c
 * @brief Visual JEPA-FEP Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-26
 *
 * WHAT: Implementation of JEPA-FEP precision-weighted integration
 * WHY:  Connect JEPA prediction errors to FEP belief updates
 * HOW:  Bidirectional error and precision propagation
 */

#include "perception/nimcp_visual_jepa_fep_bridge.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include <float.h>

/* ============================================================================
 * Module Constants
 * ============================================================================ */

#define LOG_MODULE "[JEPA_FEP]"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(visual_jepa_fep_bridge)

/* Default number of patches for buffer allocation */
#define DEFAULT_NUM_PATCHES     64

/* ============================================================================
 * Configuration API
 * ============================================================================ */

int visual_jepa_fep_bridge_default_config(visual_jepa_fep_config_t* config) {
    NIMCP_FEP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    memset(config, 0, sizeof(visual_jepa_fep_config_t));

    config->initial_precision = VISUAL_JEPA_FEP_DEFAULT_PRECISION;
    config->precision_learning_rate = VISUAL_JEPA_FEP_PRECISION_LR;
    config->precision_decay = 0.99f;

    config->enable_fep_belief_updates = true;
    config->enable_precision_weighting = true;
    config->enable_attention_precision = true;

    config->high_pe_threshold = 2.0f;
    config->novelty_threshold = 3.0f;

    config->enable_neuromod_precision = true;
    config->dopamine_precision_gain = 1.5f;
    config->norepinephrine_precision_gain = 1.2f;

    return 0;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

visual_jepa_fep_bridge_t* visual_jepa_fep_bridge_create(
    const visual_jepa_fep_config_t* config) {

    visual_jepa_fep_config_t default_config;
    if (!config) {
        visual_jepa_fep_bridge_default_config(&default_config);
        config = &default_config;
    }

    /* Allocate bridge */
    visual_jepa_fep_bridge_t* bridge = nimcp_malloc(sizeof(visual_jepa_fep_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR(LOG_MODULE " Failed to allocate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }
    memset(bridge, 0, sizeof(visual_jepa_fep_bridge_t));

    /* Initialize bridge base */
    if (bridge_base_init(&bridge->base, BIO_MODULE_VISUAL_JEPA_FEP,
                         "visual_jepa_fep") != 0) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_jepa_fep_bridge_default_config: operation failed");
        return NULL;
    }

    /* Store config */
    bridge->config = *config;

    /* Initialize precision tracking */
    bridge->precision.num_patches = DEFAULT_NUM_PATCHES;
    bridge->precision.patch_precision = nimcp_malloc(
        DEFAULT_NUM_PATCHES * sizeof(float));
    if (!bridge->precision.patch_precision) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "visual_jepa_fep_bridge_default_config: bridge->precision is NULL");
        return NULL;
    }

    /* Initialize all precisions to default */
    for (uint32_t i = 0; i < DEFAULT_NUM_PATCHES; i++) {
        bridge->precision.patch_precision[i] = config->initial_precision;
    }
    bridge->precision.global_precision = config->initial_precision;

    /* Initialize effects */
    bridge->effects.attention_weights = nimcp_malloc(
        DEFAULT_NUM_PATCHES * sizeof(float));
    if (!bridge->effects.attention_weights) {
        nimcp_free(bridge->precision.patch_precision);
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "visual_jepa_fep_bridge_default_config: bridge->effects is NULL");
        return NULL;
    }
    for (uint32_t i = 0; i < DEFAULT_NUM_PATCHES; i++) {
        bridge->effects.attention_weights[i] = 1.0f;
    }
    bridge->effects.precision_gain = 1.0f;
    bridge->effects.learning_rate_modifier = 1.0f;

    /* Initialize signals */
    bridge->buffer_size = NIMCP_JEPA_LATENT_DIM;
    bridge->error_buffer = nimcp_malloc(bridge->buffer_size * sizeof(float));
    bridge->signals.prediction_errors = nimcp_malloc(
        DEFAULT_NUM_PATCHES * sizeof(float));

    if (!bridge->error_buffer || !bridge->signals.prediction_errors) {
        nimcp_free(bridge->error_buffer);
        nimcp_free(bridge->signals.prediction_errors);
        nimcp_free(bridge->effects.attention_weights);
        nimcp_free(bridge->precision.patch_precision);
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_jepa_fep_bridge_default_config: required parameter is NULL (bridge->error_buffer, bridge->signals)");
        return NULL;
    }
    bridge->signals.error_dim = NIMCP_JEPA_LATENT_DIM;

    /* Initialize stats */
    bridge->stats.min_pe = FLT_MAX;
    bridge->stats.max_pe = 0.0f;

    NIMCP_LOGGING_INFO(LOG_MODULE " Created bridge: initial_precision=%.2f",
                      config->initial_precision);

    return bridge;
}

void visual_jepa_fep_bridge_destroy(visual_jepa_fep_bridge_t* bridge) {
    if (!bridge) return;

    nimcp_free(bridge->precision.patch_precision);
    nimcp_free(bridge->effects.attention_weights);
    nimcp_free(bridge->signals.prediction_errors);
    nimcp_free(bridge->error_buffer);

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int visual_jepa_fep_bridge_reset(visual_jepa_fep_bridge_t* bridge) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    /* Reset precision to initial values */
    for (uint32_t i = 0; i < bridge->precision.num_patches; i++) {
        bridge->precision.patch_precision[i] = bridge->config.initial_precision;
    }
    bridge->precision.global_precision = bridge->config.initial_precision;

    /* Reset effects */
    for (uint32_t i = 0; i < bridge->precision.num_patches; i++) {
        bridge->effects.attention_weights[i] = 1.0f;
    }
    bridge->effects.precision_gain = 1.0f;
    bridge->effects.learning_rate_modifier = 1.0f;
    bridge->effects.novelty_boost = 0.0f;

    /* Reset signals */
    memset(bridge->signals.prediction_errors, 0,
           bridge->precision.num_patches * sizeof(float));
    bridge->signals.total_prediction_error = 0.0f;
    bridge->signals.novelty_detected = false;

    /* Reset state and stats */
    memset(&bridge->state, 0, sizeof(visual_jepa_fep_state_t));
    memset(&bridge->stats, 0, sizeof(visual_jepa_fep_stats_t));
    bridge->stats.min_pe = FLT_MAX;

    return 0;
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int visual_jepa_fep_bridge_connect_jepa(
    visual_jepa_fep_bridge_t* bridge,
    visual_jepa_bridge_t* visual_jepa) {

    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_FEP_CHECK_THROW(visual_jepa, NIMCP_ERROR_NULL_POINTER, "visual_jepa is NULL");

    bridge->visual_jepa = visual_jepa;
    bridge->base.system_a = visual_jepa;
    bridge->base.system_a_connected = true;

    /* Update patch count to match JEPA config */
    uint32_t num_patches = visual_jepa->config.patch.num_patches_x *
                           visual_jepa->config.patch.num_patches_y;

    if (num_patches != bridge->precision.num_patches) {
        /* Reallocate precision arrays */
        float* new_prec = nimcp_malloc(num_patches * sizeof(float));
        float* new_attn = nimcp_malloc(num_patches * sizeof(float));
        float* new_errs = nimcp_malloc(num_patches * sizeof(float));

        if (!new_prec || !new_attn || !new_errs) {
            nimcp_free(new_prec);
            nimcp_free(new_attn);
            nimcp_free(new_errs);
            return -1;
        }

        nimcp_free(bridge->precision.patch_precision);
        nimcp_free(bridge->effects.attention_weights);
        nimcp_free(bridge->signals.prediction_errors);

        bridge->precision.patch_precision = new_prec;
        bridge->effects.attention_weights = new_attn;
        bridge->signals.prediction_errors = new_errs;
        bridge->precision.num_patches = num_patches;

        /* Initialize new arrays */
        for (uint32_t i = 0; i < num_patches; i++) {
            bridge->precision.patch_precision[i] = bridge->config.initial_precision;
            bridge->effects.attention_weights[i] = 1.0f;
            bridge->signals.prediction_errors[i] = 0.0f;
        }
    }

    bridge->base.bridge_active = (bridge->fep_system != NULL);

    NIMCP_LOGGING_INFO(LOG_MODULE " Connected to Visual JEPA (%u patches)",
                      num_patches);
    return 0;
}

int visual_jepa_fep_bridge_connect_fep(
    visual_jepa_fep_bridge_t* bridge,
    fep_system_t* fep) {

    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_FEP_CHECK_THROW(fep, NIMCP_ERROR_NULL_POINTER, "fep system is NULL");

    bridge->fep_system = fep;
    bridge->base.system_b = fep;
    bridge->base.system_b_connected = true;
    bridge->base.bridge_active = (bridge->visual_jepa != NULL);

    NIMCP_LOGGING_INFO(LOG_MODULE " Connected to FEP system");
    return 0;
}

bool visual_jepa_fep_bridge_is_connected(const visual_jepa_fep_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->visual_jepa != NULL && bridge->fep_system != NULL;
}

/* ============================================================================
 * FEP → JEPA Direction
 * ============================================================================ */

int visual_jepa_fep_get_precision_weights(
    visual_jepa_fep_bridge_t* bridge,
    float* patch_weights,
    uint32_t num_patches) {

    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_FEP_CHECK_THROW(patch_weights, NIMCP_ERROR_NULL_POINTER, "patch_weights is NULL");

    uint32_t count = (num_patches < bridge->precision.num_patches) ?
                     num_patches : bridge->precision.num_patches;

    for (uint32_t i = 0; i < count; i++) {
        /* Combine patch precision with attention weight */
        float prec = bridge->precision.patch_precision[i];
        float attn = bridge->effects.attention_weights[i];

        /* Apply global precision gain */
        float weight = prec * attn * bridge->effects.precision_gain;

        /* Clamp to valid range */
        if (weight < VISUAL_JEPA_FEP_MIN_PRECISION) {
            weight = VISUAL_JEPA_FEP_MIN_PRECISION;
        }
        if (weight > VISUAL_JEPA_FEP_MAX_PRECISION) {
            weight = VISUAL_JEPA_FEP_MAX_PRECISION;
        }

        patch_weights[i] = weight;
    }

    /* Fill remaining with default */
    for (uint32_t i = count; i < num_patches; i++) {
        patch_weights[i] = bridge->precision.global_precision;
    }

    return 0;
}

int visual_jepa_fep_apply_attention_precision(
    visual_jepa_fep_bridge_t* bridge,
    const float* attention,
    uint32_t width,
    uint32_t height) {

    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_FEP_CHECK_THROW(attention, NIMCP_ERROR_NULL_POINTER, "attention is NULL");

    if (!bridge->config.enable_attention_precision) {
        return 0;  /* Disabled */
    }

    /* Map attention to patches */
    uint32_t total_attn = width * height;
    uint32_t attn_per_patch = total_attn / bridge->precision.num_patches;
    if (attn_per_patch == 0) attn_per_patch = 1;

    for (uint32_t p = 0; p < bridge->precision.num_patches; p++) {
        /* Average attention in this patch region */
        double sum = 0.0;
        uint32_t start = p * attn_per_patch;
        uint32_t end = start + attn_per_patch;
        if (end > total_attn) end = total_attn;

        for (uint32_t i = start; i < end; i++) {
            sum += attention[i];
        }

        float avg_attn = (float)(sum / (end - start));
        bridge->effects.attention_weights[p] = avg_attn;

        /* Boost precision based on attention */
        float boost = 1.0f + avg_attn;  /* Attention in [0,1] → boost in [1,2] */
        bridge->precision.patch_precision[p] *= boost;

        /* Clamp */
        if (bridge->precision.patch_precision[p] > VISUAL_JEPA_FEP_MAX_PRECISION) {
            bridge->precision.patch_precision[p] = VISUAL_JEPA_FEP_MAX_PRECISION;
        }
    }

    return 0;
}

float visual_jepa_fep_get_lr_modifier(const visual_jepa_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_jepa_fep_get_lr_modifier: bridge is NULL");
        return 1.0f;
    }

    /* Higher precision → higher learning rate */
    float modifier = sqrtf(bridge->precision.global_precision);

    /* Apply novelty boost if active */
    modifier *= (1.0f + bridge->effects.novelty_boost);

    /* Clamp to reasonable range */
    if (modifier < 0.1f) modifier = 0.1f;
    if (modifier > 5.0f) modifier = 5.0f;

    return modifier;
}

/* ============================================================================
 * JEPA → FEP Direction
 * ============================================================================ */

int visual_jepa_fep_report_prediction_error(
    visual_jepa_fep_bridge_t* bridge,
    const jepa_latent_t* prediction,
    const jepa_latent_t* target) {

    NIMCP_FEP_CHECK_THROW(bridge && prediction && target, NIMCP_ERROR_NULL_POINTER,
        "NULL parameter in visual_jepa_fep_report_prediction_error");
    NIMCP_FEP_CHECK_THROW(prediction->latent_dim == target->latent_dim, NIMCP_ERROR_INVALID_PARAM,
        "latent_dim mismatch in visual_jepa_fep_report_prediction_error");

    /* Compute prediction error - only store up to buffer capacity */
    double sum_sq = 0.0;
    uint32_t store_count = (prediction->latent_dim < bridge->buffer_size) ?
                           prediction->latent_dim : bridge->buffer_size;
    for (uint32_t i = 0; i < prediction->latent_dim; i++) {
        float diff = prediction->embedding[i] - target->embedding[i];
        if (i < store_count) {
            bridge->error_buffer[i] = diff;
        }
        sum_sq += diff * diff;
    }

    float pe_magnitude = (float)sqrt(sum_sq / prediction->latent_dim);
    bridge->signals.total_prediction_error = pe_magnitude;

    /* Update stats */
    float new_avg_pe = 0.9f * bridge->state.avg_prediction_error + 0.1f * pe_magnitude;
    if (isfinite(new_avg_pe)) bridge->state.avg_prediction_error = new_avg_pe;

    if (pe_magnitude < bridge->stats.min_pe) {
        bridge->stats.min_pe = pe_magnitude;
    }
    if (pe_magnitude > bridge->stats.max_pe) {
        bridge->stats.max_pe = pe_magnitude;
    }

    /* Check for high PE */
    if (pe_magnitude > bridge->config.high_pe_threshold) {
        bridge->state.high_pe_events++;
    }

    /* Update FEP if enabled and connected */
    if (bridge->config.enable_fep_belief_updates && bridge->fep_system) {
        /* Convert to FEP prediction error format */
        /* This would integrate with fep_system_report_observation() */
        /* For now, just track the signal */
    }

    return 0;
}

int visual_jepa_fep_report_novelty(
    visual_jepa_fep_bridge_t* bridge,
    float prediction_error) {

    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    bool is_novel = (prediction_error > bridge->config.novelty_threshold);
    bridge->signals.novelty_detected = is_novel;

    if (is_novel) {
        bridge->state.novelty_events++;

        /* Boost learning temporarily */
        bridge->effects.novelty_boost = fminf(prediction_error /
                                               bridge->config.novelty_threshold, 2.0f);

        NIMCP_LOGGING_DEBUG(LOG_MODULE " Novelty detected: PE=%.3f", prediction_error);
    } else {
        /* Decay novelty boost */
        bridge->effects.novelty_boost *= 0.9f;
    }

    return 0;
}

int visual_jepa_fep_update_beliefs(
    visual_jepa_fep_bridge_t* bridge,
    const jepa_latent_t* latent) {

    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_FEP_CHECK_THROW(latent, NIMCP_ERROR_NULL_POINTER, "latent is NULL");

    if (!bridge->config.enable_fep_belief_updates || !bridge->fep_system) {
        return 0;  /* Disabled or not connected */
    }

    /* In full implementation, this would:
     * 1. Map JEPA latent to FEP state space
     * 2. Update FEP visual hierarchy beliefs
     * 3. Propagate updates through FEP
     *
     * For now, track that we would update
     */

    bridge->state.updates_processed++;
    return 0;
}

/* ============================================================================
 * Update Cycle
 * ============================================================================ */

int visual_jepa_fep_bridge_update(
    visual_jepa_fep_bridge_t* bridge,
    uint64_t delta_ms) {

    (void)delta_ms;  /* May use for time-based decay */

    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    /* Update global precision from patch precisions */
    double sum_prec = 0.0;
    for (uint32_t i = 0; i < bridge->precision.num_patches; i++) {
        sum_prec += bridge->precision.patch_precision[i];
    }
    bridge->precision.global_precision = (float)(sum_prec / bridge->precision.num_patches);

    /* Update running average precision */
    float new_avg_prec = 0.99f * bridge->state.avg_precision +
                         0.01f * bridge->precision.global_precision;
    if (isfinite(new_avg_prec)) bridge->state.avg_precision = new_avg_prec;

    /* Decay precision slightly (regression to mean) */
    float decay = bridge->config.precision_decay;
    float target = bridge->config.initial_precision;

    for (uint32_t i = 0; i < bridge->precision.num_patches; i++) {
        bridge->precision.patch_precision[i] =
            decay * bridge->precision.patch_precision[i] +
            (1.0f - decay) * target;
    }

    /* Update learning rate modifier */
    bridge->effects.learning_rate_modifier = visual_jepa_fep_get_lr_modifier(bridge);

    /* Update stats */
    bridge->stats.total_updates++;
    bridge->stats.avg_pe = bridge->state.avg_prediction_error;

    /* Record update */
    bridge_base_record_update(&bridge->base);

    return 0;
}

int visual_jepa_fep_update_precision(
    visual_jepa_fep_bridge_t* bridge,
    const float* prediction_errors,
    uint32_t num_patches) {

    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_FEP_CHECK_THROW(prediction_errors, NIMCP_ERROR_NULL_POINTER, "prediction_errors is NULL");

    if (!bridge->config.enable_precision_weighting) {
        return 0;
    }

    float lr = bridge->config.precision_learning_rate;
    uint32_t count = (num_patches < bridge->precision.num_patches) ?
                     num_patches : bridge->precision.num_patches;

    for (uint32_t i = 0; i < count; i++) {
        float pe = prediction_errors[i];
        if (!isfinite(pe)) continue;  /* Skip NaN/Inf prediction errors */
        bridge->signals.prediction_errors[i] = pe;

        /* Precision = inverse variance of prediction errors */
        /* Use exponential moving average */
        float error_sq = pe * pe;
        float new_precision = 1.0f / (error_sq + 0.01f);  /* Add small epsilon */

        /* Clamp new precision */
        if (new_precision < VISUAL_JEPA_FEP_MIN_PRECISION) {
            new_precision = VISUAL_JEPA_FEP_MIN_PRECISION;
        }
        if (new_precision > VISUAL_JEPA_FEP_MAX_PRECISION) {
            new_precision = VISUAL_JEPA_FEP_MAX_PRECISION;
        }

        /* EMA update */
        float updated = (1.0f - lr) * bridge->precision.patch_precision[i] +
            lr * new_precision;
        if (isfinite(updated)) {
            bridge->precision.patch_precision[i] = updated;
        }
    }

    return 0;
}

/* ============================================================================
 * State/Stats API
 * ============================================================================ */

int visual_jepa_fep_bridge_get_state(
    const visual_jepa_fep_bridge_t* bridge,
    visual_jepa_fep_state_t* state) {

    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_FEP_CHECK_THROW(state, NIMCP_ERROR_NULL_POINTER, "state is NULL");
    *state = bridge->state;
    return 0;
}

int visual_jepa_fep_bridge_get_stats(
    const visual_jepa_fep_bridge_t* bridge,
    visual_jepa_fep_stats_t* stats) {

    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_FEP_CHECK_THROW(stats, NIMCP_ERROR_NULL_POINTER, "stats is NULL");
    *stats = bridge->stats;
    return 0;
}

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

BRIDGE_DEFINE_BIO_ASYNC_FUNCS(visual_jepa_fep_bridge)
