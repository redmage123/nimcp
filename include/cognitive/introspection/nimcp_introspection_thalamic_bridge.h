/**
 * @file nimcp_introspection_thalamic_bridge.h
 * @brief Bridge between Introspection system and thalamic router
 *
 * WHAT: Routes introspective signals through thalamic attention pathways
 * WHY: Introspection requires conscious attention for self-monitoring
 * HOW: Packages introspective signals, routes via MD/anterior thalamic pathways
 *
 * BIOLOGICAL BASIS:
 * - Introspection involves medial PFC-thalamic circuits
 * - Mediodorsal nucleus supports metacognitive awareness
 * - Anterior thalamus links to self-referential processing
 * - Intralaminar nuclei support arousal for introspection
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_INTROSPECTION_THALAMIC_BRIDGE_H
#define NIMCP_INTROSPECTION_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define INTROSPECTION_SIGNAL_MONITOR     0x3001  /**< Self-monitoring */
#define INTROSPECTION_SIGNAL_REFLECT     0x3002  /**< Reflection process */
#define INTROSPECTION_SIGNAL_META        0x3003  /**< Metacognitive insight */
#define INTROSPECTION_SIGNAL_AWARENESS   0x3004  /**< Awareness update */

typedef struct {
    uint32_t signal_type;
    float introspection_urgency;
    float depth;
    float clarity;
    float self_relevance;
    void* content;
    uint32_t content_size;
    uint64_t timestamp_us;
} introspection_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_depth_routing;
    float min_urgency_threshold;
    float depth_boost;
} introspection_thalamic_config_t;

typedef struct {
    uint64_t monitors_routed;
    uint64_t reflections;
    uint64_t meta_insights;
    uint64_t awareness_updates;
    uint64_t signals_gated;
    float avg_depth;
    float avg_clarity;
} introspection_thalamic_stats_t;

typedef struct introspection_thalamic_bridge introspection_thalamic_bridge_t;

introspection_thalamic_config_t introspection_thalamic_default_config(void);
introspection_thalamic_bridge_t* introspection_thalamic_bridge_create(
    void* introspection, thalamic_router_t* router,
    const introspection_thalamic_config_t* config);
void introspection_thalamic_bridge_destroy(introspection_thalamic_bridge_t* bridge);
int introspection_thalamic_bridge_reset(introspection_thalamic_bridge_t* bridge);

int introspection_thalamic_route_signal(
    introspection_thalamic_bridge_t* bridge,
    const introspection_thalamic_signal_t* signal);
int introspection_thalamic_route_monitor(
    introspection_thalamic_bridge_t* bridge,
    float depth, float urgency);
int introspection_thalamic_route_reflection(
    introspection_thalamic_bridge_t* bridge,
    float clarity, float urgency);

int introspection_thalamic_set_attention(introspection_thalamic_bridge_t* bridge, float attention);
int introspection_thalamic_get_attention(introspection_thalamic_bridge_t* bridge, float* attention);
int introspection_thalamic_bridge_get_stats(
    const introspection_thalamic_bridge_t* bridge,
    introspection_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_INTROSPECTION_THALAMIC_BRIDGE_H */
