/**
 * @file nimcp_self_awareness_extended_thalamic_bridge.h
 * @brief Bridge between Extended Self-Awareness system and thalamic routing
 *
 * WHAT: Routes extended self-awareness signals through thalamic attention pathways
 * WHY: Extended self-reflection requires conscious attention for integration
 * HOW: Gates self-referential signals via attention; routes narrative updates
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal-parietal self-network communicates via thalamic relay
 * - Attention modulates depth of self-reflection
 * - Narrative identity updates require conscious processing
 * - Thalamic gating filters low-salience self-referential content
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_SELF_AWARENESS_EXTENDED_THALAMIC_BRIDGE_H
#define NIMCP_SELF_AWARENESS_EXTENDED_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SELF_EXT_SIGNAL_METACOGNITION    0x0E01
#define SELF_EXT_SIGNAL_TEMPORAL         0x0E02
#define SELF_EXT_SIGNAL_NARRATIVE        0x0E03
#define SELF_EXT_SIGNAL_FUTURE_SELF      0x0E04

typedef struct {
    uint32_t signal_type;
    float self_relevance;
    float temporal_depth;
    float urgency;
    void* content;
    uint32_t content_size;
    uint64_t timestamp_us;
} self_awareness_ext_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_depth_boost;
    float min_relevance_threshold;
    float depth_boost;
} self_awareness_ext_thalamic_config_t;

typedef struct {
    uint32_t metacognitions_routed;
    uint32_t temporal_updates;
    uint32_t narrative_integrations;
    uint32_t signals_gated;
    float avg_self_relevance;
} self_awareness_ext_thalamic_stats_t;

typedef struct self_awareness_ext_thalamic_bridge self_awareness_ext_thalamic_bridge_t;

self_awareness_ext_thalamic_config_t self_awareness_ext_thalamic_default_config(void);
self_awareness_ext_thalamic_bridge_t* self_awareness_ext_thalamic_bridge_create(void* self_awareness_ext, thalamic_router_t* router, const self_awareness_ext_thalamic_config_t* config);
void self_awareness_ext_thalamic_bridge_destroy(self_awareness_ext_thalamic_bridge_t* bridge);
int self_awareness_ext_thalamic_bridge_reset(self_awareness_ext_thalamic_bridge_t* bridge);
int self_awareness_ext_thalamic_route_metacognition(self_awareness_ext_thalamic_bridge_t* bridge, float relevance, float depth);
int self_awareness_ext_thalamic_route_temporal(self_awareness_ext_thalamic_bridge_t* bridge, float relevance, float span);
int self_awareness_ext_thalamic_route_signal(self_awareness_ext_thalamic_bridge_t* bridge, const self_awareness_ext_thalamic_signal_t* signal);
int self_awareness_ext_thalamic_set_attention(self_awareness_ext_thalamic_bridge_t* bridge, float attention);
int self_awareness_ext_thalamic_get_attention(self_awareness_ext_thalamic_bridge_t* bridge, float* attention);
int self_awareness_ext_thalamic_bridge_get_stats(self_awareness_ext_thalamic_bridge_t* bridge, self_awareness_ext_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SELF_AWARENESS_EXTENDED_THALAMIC_BRIDGE_H */
