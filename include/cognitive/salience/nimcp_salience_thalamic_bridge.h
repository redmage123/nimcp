/**
 * @file nimcp_salience_thalamic_bridge.h
 * @brief Bridge between Salience system and thalamic router
 *
 * WHAT: Routes salience signals through attention-gated thalamic pathways
 * WHY: Salient stimuli require priority processing via thalamic gating
 * HOW: Packages salience signals, routes via thalamic attention mechanism
 *
 * BIOLOGICAL BASIS:
 * - Salience network involves anterior insula and ACC
 * - Thalamus gates salient stimuli for priority processing
 * - Pulvinar coordinates attention to salient events
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_SALIENCE_THALAMIC_BRIDGE_H
#define NIMCP_SALIENCE_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SALIENCE_SIGNAL_DETECTION    0x0A01
#define SALIENCE_SIGNAL_PRIORITY     0x0A02
#define SALIENCE_SIGNAL_SWITCH       0x0A03
#define SALIENCE_SIGNAL_FILTER       0x0A04

typedef struct {
    uint32_t signal_type;
    float salience_value;
    float priority_level;
    float urgency;
    void* content;
    uint32_t content_size;
    uint64_t timestamp_us;
} salience_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_priority_override;
    float min_salience_threshold;
    float switch_threshold;
} salience_thalamic_config_t;

typedef struct salience_thalamic_bridge salience_thalamic_bridge_t;

salience_thalamic_config_t salience_thalamic_default_config(void);
salience_thalamic_bridge_t* salience_thalamic_bridge_create(void* salience, thalamic_router_t* router, const salience_thalamic_config_t* config);
void salience_thalamic_bridge_destroy(salience_thalamic_bridge_t* bridge);
int salience_thalamic_bridge_reset(salience_thalamic_bridge_t* bridge);
int salience_thalamic_route_detection(salience_thalamic_bridge_t* bridge, const salience_thalamic_signal_t* signal);
int salience_thalamic_route_priority(salience_thalamic_bridge_t* bridge, const void* stimulus, float priority);
int salience_thalamic_set_attention(salience_thalamic_bridge_t* bridge, float attention);
int salience_thalamic_get_attention(salience_thalamic_bridge_t* bridge, float* attention);

typedef struct {
    uint64_t detections_routed;
    uint64_t priority_overrides;
    uint64_t attention_switches;
    float avg_salience_value;
} salience_thalamic_stats_t;

int salience_thalamic_bridge_get_stats(salience_thalamic_bridge_t* bridge, salience_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SALIENCE_THALAMIC_BRIDGE_H */
