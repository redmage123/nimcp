/**
 * @file nimcp_autobio_thalamic_bridge.h
 * @brief Bridge between Autobiographical Memory and thalamic router
 *
 * WHAT: Routes autobiographical memories through attention-gated thalamic pathways
 * WHY: Episodic recall requires conscious access via thalamic gating
 * HOW: Packages memory signals, routes via thalamic attention mechanism
 *
 * BIOLOGICAL BASIS:
 * - Autobiographical memory involves hippocampus and medial temporal lobe
 * - Thalamus gates episodic retrieval to consciousness
 * - Anterior thalamus supports episodic memory
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_AUTOBIO_THALAMIC_BRIDGE_H
#define NIMCP_AUTOBIO_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AUTOBIO_SIGNAL_RECALL        0x0C01
#define AUTOBIO_SIGNAL_ENCODING      0x0C02
#define AUTOBIO_SIGNAL_NARRATIVE     0x0C03
#define AUTOBIO_SIGNAL_IDENTITY      0x0C04

typedef struct {
    uint32_t signal_type;
    float vividness;
    float emotional_tone;
    float recency;
    void* content;
    uint32_t content_size;
    uint64_t timestamp_us;
} autobio_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_emotional_boost;
    float min_vividness_threshold;
    float emotional_threshold;
} autobio_thalamic_config_t;

typedef struct autobio_thalamic_bridge autobio_thalamic_bridge_t;

autobio_thalamic_config_t autobio_thalamic_default_config(void);
autobio_thalamic_bridge_t* autobio_thalamic_bridge_create(void* autobio, thalamic_router_t* router, const autobio_thalamic_config_t* config);
void autobio_thalamic_bridge_destroy(autobio_thalamic_bridge_t* bridge);
int autobio_thalamic_bridge_reset(autobio_thalamic_bridge_t* bridge);
int autobio_thalamic_route_recall(autobio_thalamic_bridge_t* bridge, const autobio_thalamic_signal_t* signal);
int autobio_thalamic_route_encoding(autobio_thalamic_bridge_t* bridge, const void* event, float importance);
int autobio_thalamic_set_attention(autobio_thalamic_bridge_t* bridge, float attention);
int autobio_thalamic_get_attention(const autobio_thalamic_bridge_t* bridge, float* attention);

typedef struct {
    uint64_t recalls_routed;
    uint64_t encodings_routed;
    uint64_t narrative_updates;
    float avg_vividness;
} autobio_thalamic_stats_t;

int autobio_thalamic_bridge_get_stats(const autobio_thalamic_bridge_t* bridge, autobio_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_AUTOBIO_THALAMIC_BRIDGE_H */
