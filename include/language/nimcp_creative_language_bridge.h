/**
 * @file nimcp_creative_language_bridge.h
 * @brief Bridge between creative cognition (blending/counterfactual) and grounded language
 *
 * Routes output from conceptual blending and counterfactual reasoning
 * through the grounded language production system.
 */

#ifndef NIMCP_CREATIVE_LANGUAGE_BRIDGE_H
#define NIMCP_CREATIVE_LANGUAGE_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CLB_MAX_OUTPUT 512
#define CLB_SEMANTIC_DIM 128

typedef struct creative_language_bridge creative_language_bridge_t;

typedef struct {
    float min_novelty;          /* Min novelty score to trigger creative production */
    float min_plausibility;     /* Min plausibility for counterfactual narration */
    bool enable_blend_narration;
    bool enable_cf_narration;
    float inflammation_sensitivity;
    float fatigue_sensitivity;
} clb_config_t;

typedef struct {
    uint64_t blends_narrated;
    uint64_t counterfactuals_narrated;
    uint64_t productions_generated;
    float avg_novelty;
    float avg_plausibility;
} clb_stats_t;

/* Lifecycle */
clb_config_t creative_language_bridge_default_config(void);
creative_language_bridge_t* creative_language_bridge_create(const clb_config_t* config);
void creative_language_bridge_destroy(creative_language_bridge_t* bridge);

/* Attach subsystems */
int clb_attach_language(creative_language_bridge_t* bridge, void* gl_context);
int clb_attach_blending(creative_language_bridge_t* bridge, void* blending_engine);
int clb_attach_counterfactual(creative_language_bridge_t* bridge, void* cf_engine);

/* Production */
int clb_narrate_blend(creative_language_bridge_t* bridge,
    const float* blend_vector, uint32_t dim,
    float novelty, float integration,
    char* output, uint32_t output_size);

int clb_narrate_counterfactual(creative_language_bridge_t* bridge,
    const float* cf_vector, uint32_t dim,
    float plausibility,
    char* output, uint32_t output_size);

int clb_describe_emergence(creative_language_bridge_t* bridge,
    const char* property_name, float strength,
    const float* context_vector, uint32_t dim,
    char* output, uint32_t output_size);

/* Modulation */
int clb_set_inflammation(creative_language_bridge_t* bridge, float level);
int clb_set_fatigue(creative_language_bridge_t* bridge, float level);

/* Stats */
int clb_get_stats(const creative_language_bridge_t* bridge, clb_stats_t* stats);
void clb_reset_stats(creative_language_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CREATIVE_LANGUAGE_BRIDGE_H */
