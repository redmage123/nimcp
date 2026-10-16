/**
 * @file nimcp_self_awareness_thalamic_bridge.h
 * @brief Bridge between Self-Awareness system and thalamic router
 *
 * WHAT: Routes self-awareness signals through attention-gated thalamic pathways
 * WHY: Metacognition requires conscious access via thalamic gating
 * HOW: Packages self-model signals, routes via thalamic attention mechanism
 *
 * BIOLOGICAL BASIS:
 * - Self-awareness involves medial PFC and posterior cingulate
 * - Thalamus gates self-referential processing
 * - Default mode network interacts with thalamic circuits
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_SELF_AWARENESS_THALAMIC_BRIDGE_H
#define NIMCP_SELF_AWARENESS_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SELF_SIGNAL_INTROSPECTION    0x0701
#define SELF_SIGNAL_METACOGNITION    0x0702
#define SELF_SIGNAL_IDENTITY         0x0703
#define SELF_SIGNAL_MONITORING       0x0704

typedef struct {
    uint32_t signal_type;
    float introspection_depth;
    float metacognitive_accuracy;
    float identity_coherence;
    void* content;
    uint32_t content_size;
    uint64_t timestamp_us;
} self_awareness_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_metacognitive_boost;
    float min_introspection_depth;
    float identity_threshold;
} self_awareness_thalamic_config_t;

typedef struct self_awareness_thalamic_bridge self_awareness_thalamic_bridge_t;

self_awareness_thalamic_config_t self_awareness_thalamic_default_config(void);
self_awareness_thalamic_bridge_t* self_awareness_thalamic_bridge_create(void* self_awareness, thalamic_router_t* router, const self_awareness_thalamic_config_t* config);
void self_awareness_thalamic_bridge_destroy(self_awareness_thalamic_bridge_t* bridge);
int self_awareness_thalamic_bridge_reset(self_awareness_thalamic_bridge_t* bridge);
int self_awareness_thalamic_route_introspection(self_awareness_thalamic_bridge_t* bridge, const self_awareness_thalamic_signal_t* signal);
int self_awareness_thalamic_route_metacognition(self_awareness_thalamic_bridge_t* bridge, const void* metacog, float accuracy);
int self_awareness_thalamic_set_attention(self_awareness_thalamic_bridge_t* bridge, float attention);
int self_awareness_thalamic_get_attention(const self_awareness_thalamic_bridge_t* bridge, float* attention);

typedef struct {
    uint64_t introspections_routed;
    uint64_t metacognitions_routed;
    uint64_t identity_updates;
    float avg_introspection_depth;
} self_awareness_thalamic_stats_t;

int self_awareness_thalamic_bridge_get_stats(const self_awareness_thalamic_bridge_t* bridge, self_awareness_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SELF_AWARENESS_THALAMIC_BRIDGE_H */
