/**
 * @file nimcp_creative_language_bridge.c
 * @brief Creative language bridge implementation
 *
 * Routes creative cognition output through grounded language production.
 * Connects conceptual blending and counterfactual reasoning to the
 * grounded language system for natural-language narration of creative thought.
 */

#include "language/nimcp_creative_language_bridge.h"
#include "language/nimcp_grounded_language.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdio.h>

struct creative_language_bridge {
    clb_config_t config;
    clb_stats_t stats;
    float inflammation;
    float fatigue;

    /* Attached subsystems (opaque pointers to avoid circular deps) */
    void* gl_context;          /* grounded_language_t* */
    void* blending_engine;     /* blending_engine_t* */
    void* cf_engine;           /* counterfactual_engine_t* */
};

/* ---------------------------------------------------------------------------
 * Internal helpers
 * ------------------------------------------------------------------------- */

/**
 * Apply inflammation/fatigue modulation to a value.
 * Returns the modulated value, clamped to [0.4, 1.0] of the original.
 */
static float clb_apply_mod(const creative_language_bridge_t* b, float v) {
    float factor = 1.0f
        - b->inflammation * b->config.inflammation_sensitivity * 0.2f
        - b->fatigue      * b->config.fatigue_sensitivity      * 0.3f;
    return v * fmaxf(0.4f, factor);
}

/**
 * Copy production result text into caller's output buffer, then clean up.
 * If the production result has no text, output is left unchanged.
 */
static void clb_copy_result(const gl_production_result_t* result,
                            char* output, uint32_t output_size)
{
    if (result->text && result->text[0] != '\0') {
        strncpy(output, result->text, output_size - 1);
        output[output_size - 1] = '\0';
    }
}

/**
 * Prepare a CLB_SEMANTIC_DIM intent vector from an input vector of arbitrary
 * dimension, applying the biological modulation scale factor.
 */
static void clb_prepare_intent(const creative_language_bridge_t* b,
                               const float* src, uint32_t src_dim,
                               float scale_base,
                               float intent[CLB_SEMANTIC_DIM])
{
    memset(intent, 0, CLB_SEMANTIC_DIM * sizeof(float));
    uint32_t copy_dim = src_dim < (uint32_t)CLB_SEMANTIC_DIM
                      ? src_dim : (uint32_t)CLB_SEMANTIC_DIM;
    memcpy(intent, src, copy_dim * sizeof(float));

    float scale = clb_apply_mod(b, scale_base);
    for (uint32_t i = 0; i < (uint32_t)CLB_SEMANTIC_DIM; i++) {
        intent[i] *= scale;
    }
}

/* ---------------------------------------------------------------------------
 * Lifecycle
 * ------------------------------------------------------------------------- */

clb_config_t creative_language_bridge_default_config(void) {
    return (clb_config_t){
        .min_novelty              = 0.3f,
        .min_plausibility         = 0.2f,
        .enable_blend_narration   = true,
        .enable_cf_narration      = true,
        .inflammation_sensitivity = 0.5f,
        .fatigue_sensitivity      = 0.5f
    };
}

creative_language_bridge_t* creative_language_bridge_create(const clb_config_t* config) {
    creative_language_bridge_t* b = nimcp_calloc(1, sizeof(creative_language_bridge_t));
    if (!b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate creative_language_bridge");
        return NULL;
    }
    b->config = config ? *config : creative_language_bridge_default_config();
    return b;
}

void creative_language_bridge_destroy(creative_language_bridge_t* bridge) {
    if (bridge) {
        nimcp_free(bridge);
    }
}

/* ---------------------------------------------------------------------------
 * Attach subsystems
 * ------------------------------------------------------------------------- */

int clb_attach_language(creative_language_bridge_t* bridge, void* gl_context) {
    if (!bridge) return -1;
    bridge->gl_context = gl_context;
    return 0;
}

int clb_attach_blending(creative_language_bridge_t* bridge, void* blending_engine) {
    if (!bridge) return -1;
    bridge->blending_engine = blending_engine;
    return 0;
}

int clb_attach_counterfactual(creative_language_bridge_t* bridge, void* cf_engine) {
    if (!bridge) return -1;
    bridge->cf_engine = cf_engine;
    return 0;
}

/* ---------------------------------------------------------------------------
 * Production: narrate blend
 * ------------------------------------------------------------------------- */

int clb_narrate_blend(creative_language_bridge_t* bridge,
                      const float* blend_vector, uint32_t dim,
                      float novelty, float integration,
                      char* output, uint32_t output_size)
{
    if (!bridge || !output || output_size == 0) return -1;
    if (!bridge->config.enable_blend_narration) return -1;

    output[0] = '\0';

    /* Route through grounded language if available */
    if (bridge->gl_context && blend_vector && dim > 0) {
        /* Use GL_PRODUCE_CREATE for high novelty, GL_PRODUCE_ANALOGIZE otherwise */
        gl_production_mode_t mode = (novelty > 0.7f)
            ? GL_PRODUCE_CREATE : GL_PRODUCE_ANALOGIZE;

        float intent[CLB_SEMANTIC_DIM];
        clb_prepare_intent(bridge, blend_vector, dim,
                           0.5f + novelty * 0.5f, intent);

        gl_production_result_t result;
        memset(&result, 0, sizeof(result));

        int rc = grounded_language_produce(
            (grounded_language_t*)bridge->gl_context,
            intent, CLB_SEMANTIC_DIM, mode, &result);

        if (rc == 0) {
            clb_copy_result(&result, output, output_size);
        }
        gl_production_result_cleanup(&result);
    }

    /* If grounded language didn't produce output, generate template */
    if (output[0] == '\0') {
        if (novelty > 0.7f) {
            snprintf(output, output_size,
                "A novel concept emerges (novelty=%.2f, integration=%.2f)",
                novelty, integration);
        } else {
            snprintf(output, output_size,
                "Concepts blend together (novelty=%.2f, integration=%.2f)",
                novelty, integration);
        }
    }

    bridge->stats.blends_narrated++;
    bridge->stats.productions_generated++;
    bridge->stats.avg_novelty = bridge->stats.avg_novelty * 0.9f + novelty * 0.1f;

    return 0;
}

/* ---------------------------------------------------------------------------
 * Production: narrate counterfactual
 * ------------------------------------------------------------------------- */

int clb_narrate_counterfactual(creative_language_bridge_t* bridge,
                               const float* cf_vector, uint32_t dim,
                               float plausibility,
                               char* output, uint32_t output_size)
{
    if (!bridge || !output || output_size == 0) return -1;
    if (!bridge->config.enable_cf_narration) return -1;
    if (plausibility < bridge->config.min_plausibility) return -1;

    output[0] = '\0';

    if (bridge->gl_context && cf_vector && dim > 0) {
        float intent[CLB_SEMANTIC_DIM];
        clb_prepare_intent(bridge, cf_vector, dim, plausibility, intent);

        gl_production_result_t result;
        memset(&result, 0, sizeof(result));

        int rc = grounded_language_produce(
            (grounded_language_t*)bridge->gl_context,
            intent, CLB_SEMANTIC_DIM, GL_PRODUCE_NARRATE, &result);

        if (rc == 0) {
            clb_copy_result(&result, output, output_size);
        }
        gl_production_result_cleanup(&result);
    }

    if (output[0] == '\0') {
        snprintf(output, output_size,
            "If things were different (plausibility=%.2f)...",
            plausibility);
    }

    bridge->stats.counterfactuals_narrated++;
    bridge->stats.productions_generated++;
    bridge->stats.avg_plausibility =
        bridge->stats.avg_plausibility * 0.9f + plausibility * 0.1f;

    return 0;
}

/* ---------------------------------------------------------------------------
 * Production: describe emergent property
 * ------------------------------------------------------------------------- */

int clb_describe_emergence(creative_language_bridge_t* bridge,
                           const char* property_name, float strength,
                           const float* context_vector, uint32_t dim,
                           char* output, uint32_t output_size)
{
    if (!bridge || !output || output_size == 0) return -1;

    output[0] = '\0';

    if (bridge->gl_context && context_vector && dim > 0) {
        float intent[CLB_SEMANTIC_DIM];
        clb_prepare_intent(bridge, context_vector, dim, strength, intent);

        gl_production_result_t result;
        memset(&result, 0, sizeof(result));

        int rc = grounded_language_produce(
            (grounded_language_t*)bridge->gl_context,
            intent, CLB_SEMANTIC_DIM, GL_PRODUCE_DESCRIBE, &result);

        if (rc == 0) {
            clb_copy_result(&result, output, output_size);
        }
        gl_production_result_cleanup(&result);
    }

    if (output[0] == '\0') {
        snprintf(output, output_size,
            "An emergent property '%s' (strength=%.2f) manifests from the blend",
            property_name ? property_name : "unknown", strength);
    }

    bridge->stats.productions_generated++;

    return 0;
}

/* ---------------------------------------------------------------------------
 * Modulation
 * ------------------------------------------------------------------------- */

int clb_set_inflammation(creative_language_bridge_t* bridge, float level) {
    if (!bridge) return -1;
    bridge->inflammation = fmaxf(0.0f, fminf(1.0f, level));
    return 0;
}

int clb_set_fatigue(creative_language_bridge_t* bridge, float level) {
    if (!bridge) return -1;
    bridge->fatigue = fmaxf(0.0f, fminf(1.0f, level));
    return 0;
}

/* ---------------------------------------------------------------------------
 * Stats
 * ------------------------------------------------------------------------- */

int clb_get_stats(const creative_language_bridge_t* bridge, clb_stats_t* stats) {
    if (!bridge || !stats) return -1;
    *stats = bridge->stats;
    return 0;
}

void clb_reset_stats(creative_language_bridge_t* bridge) {
    if (bridge) {
        memset(&bridge->stats, 0, sizeof(bridge->stats));
    }
}
