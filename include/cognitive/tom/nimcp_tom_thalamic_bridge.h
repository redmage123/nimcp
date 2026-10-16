/**
 * @file nimcp_tom_thalamic_bridge.h
 * @brief Bridge between Theory of Mind system and thalamic router
 *
 * WHAT: Routes ToM signals through thalamic attention pathways
 * WHY: Mentalizing requires conscious attention for other-modeling
 * HOW: Packages ToM signals, routes via MD/temporal-parietal pathways
 *
 * BIOLOGICAL BASIS:
 * - ToM involves TPJ-mPFC-thalamic circuits
 * - MD nucleus supports meta-representation
 * - Pulvinar coordinates social attention
 * - Anterior thalamus links ToM to episodic social memory
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_TOM_THALAMIC_BRIDGE_H
#define NIMCP_TOM_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TOM_SIGNAL_BELIEF_ATTR      0x3601  /**< Belief attribution */
#define TOM_SIGNAL_INTENT_INFER     0x3602  /**< Intent inference */
#define TOM_SIGNAL_EMOTION_ATTR     0x3603  /**< Emotion attribution */
#define TOM_SIGNAL_PERSPECTIVE      0x3604  /**< Perspective taking */

typedef struct {
    uint32_t signal_type;
    float tom_urgency;
    float mentalizing_depth;
    float confidence;
    float social_relevance;
    void* content;
    uint32_t content_size;
    uint64_t timestamp_us;
} tom_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_social_boost;
    float min_urgency_threshold;
    float social_boost_factor;
} tom_thalamic_config_t;

typedef struct {
    uint64_t belief_attributions;
    uint64_t intent_inferences;
    uint64_t emotion_attributions;
    uint64_t perspective_takes;
    uint64_t signals_gated;
    float avg_mentalizing_depth;
    float avg_confidence;
} tom_thalamic_stats_t;

typedef struct tom_thalamic_bridge tom_thalamic_bridge_t;

tom_thalamic_config_t tom_thalamic_default_config(void);
tom_thalamic_bridge_t* tom_thalamic_bridge_create(
    void* tom, thalamic_router_t* router,
    const tom_thalamic_config_t* config);
void tom_thalamic_bridge_destroy(tom_thalamic_bridge_t* bridge);
int tom_thalamic_bridge_reset(tom_thalamic_bridge_t* bridge);

int tom_thalamic_route_signal(
    tom_thalamic_bridge_t* bridge,
    const tom_thalamic_signal_t* signal);
int tom_thalamic_route_belief_attribution(
    tom_thalamic_bridge_t* bridge,
    float depth, float confidence);
int tom_thalamic_route_perspective(
    tom_thalamic_bridge_t* bridge,
    float social_relevance, float urgency);

int tom_thalamic_set_attention(tom_thalamic_bridge_t* bridge, float attention);
int tom_thalamic_get_attention(const tom_thalamic_bridge_t* bridge, float* attention);
int tom_thalamic_bridge_get_stats(
    const tom_thalamic_bridge_t* bridge,
    tom_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TOM_THALAMIC_BRIDGE_H */
