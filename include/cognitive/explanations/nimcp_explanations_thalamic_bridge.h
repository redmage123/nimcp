/**
 * @file nimcp_explanations_thalamic_bridge.h
 * @brief Bridge between Explanations system and thalamic router
 *
 * WHAT: Routes explanation generation through thalamic attention pathways
 * WHY: Generating explanations requires conscious attention for coherence
 * HOW: Packages explanation signals, routes via MD/pulvinar pathways
 *
 * BIOLOGICAL BASIS:
 * - Explanation generation involves prefrontal-temporal integration
 * - Mediodorsal nucleus supports causal reasoning
 * - Pulvinar coordinates attention during explanation construction
 * - Anterior thalamus links explanations to episodic context
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_EXPLANATIONS_THALAMIC_BRIDGE_H
#define NIMCP_EXPLANATIONS_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EXPLANATION_SIGNAL_CAUSAL        0x2F01  /**< Causal explanation */
#define EXPLANATION_SIGNAL_MECHANISTIC   0x2F02  /**< Mechanistic explanation */
#define EXPLANATION_SIGNAL_CONTRASTIVE   0x2F03  /**< Contrastive explanation */
#define EXPLANATION_SIGNAL_COMPLETION    0x2F04  /**< Explanation complete */

typedef struct {
    uint32_t signal_type;
    float explanation_urgency;
    float complexity;
    float coherence;
    float completeness;
    void* content;
    uint32_t content_size;
    uint64_t timestamp_us;
} explanations_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_complexity_routing;
    float min_urgency_threshold;
    float complexity_boost;
} explanations_thalamic_config_t;

typedef struct {
    uint64_t causal_explanations;
    uint64_t mechanistic_explanations;
    uint64_t contrastive_explanations;
    uint64_t completions;
    uint64_t signals_gated;
    float avg_complexity;
    float avg_coherence;
} explanations_thalamic_stats_t;

typedef struct explanations_thalamic_bridge explanations_thalamic_bridge_t;

explanations_thalamic_config_t explanations_thalamic_default_config(void);
explanations_thalamic_bridge_t* explanations_thalamic_bridge_create(
    void* explanations, thalamic_router_t* router,
    const explanations_thalamic_config_t* config);
void explanations_thalamic_bridge_destroy(explanations_thalamic_bridge_t* bridge);
int explanations_thalamic_bridge_reset(explanations_thalamic_bridge_t* bridge);

int explanations_thalamic_route_signal(
    explanations_thalamic_bridge_t* bridge,
    const explanations_thalamic_signal_t* signal);
int explanations_thalamic_route_causal(
    explanations_thalamic_bridge_t* bridge,
    float complexity, float urgency);

int explanations_thalamic_set_attention(explanations_thalamic_bridge_t* bridge, float attention);
int explanations_thalamic_get_attention(explanations_thalamic_bridge_t* bridge, float* attention);
int explanations_thalamic_bridge_get_stats(
    const explanations_thalamic_bridge_t* bridge,
    explanations_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EXPLANATIONS_THALAMIC_BRIDGE_H */
