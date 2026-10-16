/**
 * @file nimcp_remorse_thalamic_bridge.h
 * @brief Bridge between Remorse system and thalamic router
 *
 * WHAT: Routes remorse/guilt signals through attention-gated thalamic pathways
 * WHY: Moral emotions require conscious processing via thalamic gating
 * HOW: Packages remorse signals, routes via thalamic attention mechanism
 *
 * BIOLOGICAL BASIS:
 * - Remorse involves anterior cingulate and insula
 * - Thalamus gates moral emotion signals
 * - Conscious guilt drives repair behavior
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_REMORSE_THALAMIC_BRIDGE_H
#define NIMCP_REMORSE_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define REMORSE_SIGNAL_GUILT       0x1401
#define REMORSE_SIGNAL_REPAIR      0x1402
#define REMORSE_SIGNAL_ATONEMENT   0x1403
#define REMORSE_SIGNAL_FORGIVENESS 0x1404

typedef struct {
    uint32_t signal_type;
    float guilt_intensity;
    float repair_motivation;
    float transgression_severity;
    void* content;
    uint32_t content_size;
    uint64_t timestamp_us;
} remorse_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_repair_routing;
    float min_guilt_threshold;
    float repair_threshold;
} remorse_thalamic_config_t;

typedef struct remorse_thalamic_bridge remorse_thalamic_bridge_t;

remorse_thalamic_config_t remorse_thalamic_default_config(void);
remorse_thalamic_bridge_t* remorse_thalamic_bridge_create(void* remorse, thalamic_router_t* router, const remorse_thalamic_config_t* config);
void remorse_thalamic_bridge_destroy(remorse_thalamic_bridge_t* bridge);
int remorse_thalamic_bridge_reset(remorse_thalamic_bridge_t* bridge);
int remorse_thalamic_route_guilt(remorse_thalamic_bridge_t* bridge, const remorse_thalamic_signal_t* signal);
int remorse_thalamic_route_repair(remorse_thalamic_bridge_t* bridge, const void* action, float motivation);
int remorse_thalamic_set_attention(remorse_thalamic_bridge_t* bridge, float attention);
int remorse_thalamic_get_attention(const remorse_thalamic_bridge_t* bridge, float* attention);

typedef struct {
    uint64_t guilt_signals_routed;
    uint64_t repair_actions;
    uint64_t forgiveness_achieved;
    float avg_guilt_intensity;
} remorse_thalamic_stats_t;

int remorse_thalamic_bridge_get_stats(const remorse_thalamic_bridge_t* bridge, remorse_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_REMORSE_THALAMIC_BRIDGE_H */
