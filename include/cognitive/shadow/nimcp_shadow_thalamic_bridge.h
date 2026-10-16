/**
 * @file nimcp_shadow_thalamic_bridge.h
 * @brief Bridge between Shadow (unconscious) system and thalamic router
 *
 * WHAT: Routes shadow content through thalamic pathways
 * WHY: Shadow integration requires controlled conscious access via thalamic gating
 * HOW: Packages shadow signals, routes via thalamic attention mechanism
 *
 * BIOLOGICAL BASIS:
 * - Shadow content involves subcortical and limbic circuits
 * - Thalamus gates emergence of unconscious content
 * - Integration requires controlled conscious access
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_SHADOW_THALAMIC_BRIDGE_H
#define NIMCP_SHADOW_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SHADOW_SIGNAL_EMERGENCE    0x1501
#define SHADOW_SIGNAL_PROJECTION   0x1502
#define SHADOW_SIGNAL_INTEGRATION  0x1503
#define SHADOW_SIGNAL_AWARENESS    0x1504

typedef struct {
    uint32_t signal_type;
    float emergence_strength;
    float integration_potential;
    float awareness_level;
    void* content;
    uint32_t content_size;
    uint64_t timestamp_us;
} shadow_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_gradual_emergence;
    float min_emergence_threshold;
    float integration_threshold;
} shadow_thalamic_config_t;

typedef struct shadow_thalamic_bridge shadow_thalamic_bridge_t;

shadow_thalamic_config_t shadow_thalamic_default_config(void);
shadow_thalamic_bridge_t* shadow_thalamic_bridge_create(void* shadow, thalamic_router_t* router, const shadow_thalamic_config_t* config);
void shadow_thalamic_bridge_destroy(shadow_thalamic_bridge_t* bridge);
int shadow_thalamic_bridge_reset(shadow_thalamic_bridge_t* bridge);
int shadow_thalamic_route_emergence(shadow_thalamic_bridge_t* bridge, const shadow_thalamic_signal_t* signal);
int shadow_thalamic_route_integration(shadow_thalamic_bridge_t* bridge, const void* content, float readiness);
int shadow_thalamic_set_attention(shadow_thalamic_bridge_t* bridge, float attention);
int shadow_thalamic_get_attention(shadow_thalamic_bridge_t* bridge, float* attention);

typedef struct {
    uint64_t emergences_routed;
    uint64_t projections_detected;
    uint64_t integrations_achieved;
    float avg_emergence_strength;
} shadow_thalamic_stats_t;

int shadow_thalamic_bridge_get_stats(shadow_thalamic_bridge_t* bridge, shadow_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SHADOW_THALAMIC_BRIDGE_H */
