/**
 * @file nimcp_parietal_thalamic_bridge.h
 * @brief Bridge between Parietal Cortex and thalamic router
 *
 * WHAT: Routes parietal signals through thalamic relay
 * WHY: Parietal cortex connects with pulvinar and VPL
 * HOW: Packages spatial/attention signals, routes via appropriate pathway
 *
 * BIOLOGICAL BASIS:
 * - Pulvinar coordinates parietal attention networks
 * - VPL relays somatosensory to parietal
 * - Spatial attention involves parietal-pulvinar circuits
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_PARIETAL_THALAMIC_BRIDGE_H
#define NIMCP_PARIETAL_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PARIETAL_SIGNAL_SPATIAL      0x2A01
#define PARIETAL_SIGNAL_ATTENTION    0x2A02
#define PARIETAL_SIGNAL_NUMERICAL    0x2A03
#define PARIETAL_SIGNAL_INTEGRATION  0x2A04

typedef struct {
    uint32_t signal_type;
    float spatial_precision;
    float attention_weight;
    float integration_quality;
    void* content;
    uint32_t content_size;
    uint64_t timestamp_us;
} parietal_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_spatial_priority;
    float min_spatial_precision;
    float attention_threshold;
} parietal_thalamic_config_t;

typedef struct parietal_thalamic_bridge parietal_thalamic_bridge_t;

parietal_thalamic_config_t parietal_thalamic_default_config(void);
parietal_thalamic_bridge_t* parietal_thalamic_bridge_create(void* parietal, thalamic_router_t* router, const parietal_thalamic_config_t* config);
void parietal_thalamic_bridge_destroy(parietal_thalamic_bridge_t* bridge);
int parietal_thalamic_bridge_reset(parietal_thalamic_bridge_t* bridge);
int parietal_thalamic_route_signal(parietal_thalamic_bridge_t* bridge, const parietal_thalamic_signal_t* signal);
int parietal_thalamic_route_attention(parietal_thalamic_bridge_t* bridge, const void* target, float weight);
int parietal_thalamic_set_attention(parietal_thalamic_bridge_t* bridge, float attention);
int parietal_thalamic_get_attention(const parietal_thalamic_bridge_t* bridge, float* attention);

typedef struct {
    uint64_t spatial_signals_routed;
    uint64_t attention_shifts;
    uint64_t integrations_completed;
    float avg_spatial_precision;
} parietal_thalamic_stats_t;

int parietal_thalamic_bridge_get_stats(const parietal_thalamic_bridge_t* bridge, parietal_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PARIETAL_THALAMIC_BRIDGE_H */
