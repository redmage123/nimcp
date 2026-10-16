/**
 * @file nimcp_mirror_thalamic_bridge.h
 * @brief Bridge between Mirror Neuron system and thalamic router
 *
 * WHAT: Routes mirror neuron signals through attention-gated thalamic pathways
 * WHY: Action understanding requires conscious access via thalamic gating
 * HOW: Packages mirroring signals, routes via thalamic attention mechanism
 *
 * BIOLOGICAL BASIS:
 * - Mirror neurons in premotor and parietal cortex
 * - Thalamus coordinates attention during action observation
 * - Empathic resonance gated through thalamic circuits
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_MIRROR_THALAMIC_BRIDGE_H
#define NIMCP_MIRROR_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MIRROR_SIGNAL_ACTION       0x0501
#define MIRROR_SIGNAL_INTENTION    0x0502
#define MIRROR_SIGNAL_EMPATHY      0x0503
#define MIRROR_SIGNAL_IMITATION    0x0504

typedef struct {
    uint32_t signal_type;
    float mirroring_strength;
    float empathic_resonance;
    float action_clarity;
    void* content;
    uint32_t content_size;
    uint64_t timestamp_us;
} mirror_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_empathy_boost;
    float min_mirroring_strength;
    float empathy_threshold;
    bool bio_async_enabled;              /**< Enable bio-async messaging */
} mirror_thalamic_config_t;

typedef struct mirror_thalamic_bridge mirror_thalamic_bridge_t;

mirror_thalamic_config_t mirror_thalamic_default_config(void);
mirror_thalamic_bridge_t* mirror_thalamic_bridge_create(void* mirror, thalamic_router_t* router, const mirror_thalamic_config_t* config);
void mirror_thalamic_bridge_destroy(mirror_thalamic_bridge_t* bridge);
int mirror_thalamic_bridge_reset(mirror_thalamic_bridge_t* bridge);
int mirror_thalamic_route_action(mirror_thalamic_bridge_t* bridge, const mirror_thalamic_signal_t* signal);
int mirror_thalamic_route_empathy(mirror_thalamic_bridge_t* bridge, const void* emotion, float resonance);
int mirror_thalamic_set_attention(mirror_thalamic_bridge_t* bridge, float attention);
int mirror_thalamic_get_attention(const mirror_thalamic_bridge_t* bridge, float* attention);

typedef struct {
    uint64_t actions_mirrored;
    uint64_t empathic_responses;
    uint64_t imitations_triggered;
    float avg_mirroring_strength;
} mirror_thalamic_stats_t;

int mirror_thalamic_bridge_get_stats(const mirror_thalamic_bridge_t* bridge, mirror_thalamic_stats_t* stats);

bool mirror_thalamic_register_bio_async(mirror_thalamic_bridge_t* bridge);
void mirror_thalamic_unregister_bio_async(mirror_thalamic_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MIRROR_THALAMIC_BRIDGE_H */
