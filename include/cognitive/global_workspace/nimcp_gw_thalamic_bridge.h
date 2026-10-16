/**
 * @file nimcp_gw_thalamic_bridge.h
 * @brief Bridge between Global Workspace and thalamic router
 *
 * WHAT: Routes global workspace broadcasts through attention-gated thalamic pathways
 * WHY: Conscious access requires thalamic gating for cortical distribution
 * HOW: Packages GW broadcasts as signals, routes via thalamic attention mechanism
 *
 * BIOLOGICAL BASIS:
 * - Thalamus gates access to global workspace (Dehaene & Changeux)
 * - Pulvinar coordinates attention during conscious access
 * - TRN modulates which content reaches awareness
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_GW_THALAMIC_BRIDGE_H
#define NIMCP_GW_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GW_SIGNAL_BROADCAST      0x0101
#define GW_SIGNAL_COMPETITION    0x0102
#define GW_SIGNAL_IGNITION       0x0103
#define GW_SIGNAL_CONTENT        0x0104

typedef struct {
    uint32_t signal_type;
    float salience;
    float attention_weight;
    float competition_strength;
    void* content;
    uint32_t content_size;
    uint64_t timestamp_us;
} gw_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_competition_routing;
    float min_salience_threshold;
    float ignition_threshold;
} gw_thalamic_config_t;

typedef struct gw_thalamic_bridge gw_thalamic_bridge_t;

gw_thalamic_config_t gw_thalamic_default_config(void);
gw_thalamic_bridge_t* gw_thalamic_bridge_create(void* gw, thalamic_router_t* router, const gw_thalamic_config_t* config);
void gw_thalamic_bridge_destroy(gw_thalamic_bridge_t* bridge);
int gw_thalamic_bridge_reset(gw_thalamic_bridge_t* bridge);
int gw_thalamic_route_broadcast(gw_thalamic_bridge_t* bridge, const gw_thalamic_signal_t* signal);
int gw_thalamic_route_ignition(gw_thalamic_bridge_t* bridge, const void* content, float strength);
int gw_thalamic_set_attention(gw_thalamic_bridge_t* bridge, float attention);
int gw_thalamic_get_attention(const gw_thalamic_bridge_t* bridge, float* attention);

typedef struct {
    uint64_t broadcasts_routed;
    uint64_t ignitions_triggered;
    uint64_t signals_gated;
    float avg_attention;
} gw_thalamic_stats_t;

int gw_thalamic_bridge_get_stats(const gw_thalamic_bridge_t* bridge, gw_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GW_THALAMIC_BRIDGE_H */
