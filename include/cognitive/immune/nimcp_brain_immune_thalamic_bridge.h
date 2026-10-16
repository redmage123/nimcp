/**
 * @file nimcp_brain_immune_thalamic_bridge.h
 * @brief Bridge between Brain Immune system and thalamic router
 *
 * WHAT: Routes immune signals through attention-gated thalamic pathways
 * WHY: Critical immune events require conscious awareness via thalamic gating
 * HOW: Packages immune signals, routes via thalamic attention mechanism
 *
 * BIOLOGICAL BASIS:
 * - Immune-brain communication occurs via cytokine signaling to hypothalamus
 * - High-severity immune events get enhanced routing priority
 * - Attention modulates which immune signals reach consciousness (sickness behavior)
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_BRAIN_IMMUNE_THALAMIC_BRIDGE_H
#define NIMCP_BRAIN_IMMUNE_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IMMUNE_SIGNAL_THREAT        0x0A01
#define IMMUNE_SIGNAL_RESPONSE      0x0A02
#define IMMUNE_SIGNAL_RECOVERY      0x0A03
#define IMMUNE_SIGNAL_MEMORY        0x0A04

typedef struct {
    uint32_t signal_type;
    float threat_severity;
    float inflammation_level;
    float urgency;
    void* immune_data;
    uint32_t data_size;
    uint64_t timestamp_us;
} brain_immune_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_severity_boost;
    float min_threat_threshold;
    float inflammation_boost;
} brain_immune_thalamic_config_t;

typedef struct brain_immune_thalamic_bridge brain_immune_thalamic_bridge_t;

brain_immune_thalamic_config_t brain_immune_thalamic_default_config(void);
brain_immune_thalamic_bridge_t* brain_immune_thalamic_bridge_create(void* brain_immune, thalamic_router_t* router, const brain_immune_thalamic_config_t* config);
void brain_immune_thalamic_bridge_destroy(brain_immune_thalamic_bridge_t* bridge);
int brain_immune_thalamic_bridge_reset(brain_immune_thalamic_bridge_t* bridge);
int brain_immune_thalamic_route_threat(brain_immune_thalamic_bridge_t* bridge, const brain_immune_thalamic_signal_t* signal);
int brain_immune_thalamic_route_response(brain_immune_thalamic_bridge_t* bridge, const void* response, float intensity);
int brain_immune_thalamic_set_attention(brain_immune_thalamic_bridge_t* bridge, float attention);
int brain_immune_thalamic_get_attention(const brain_immune_thalamic_bridge_t* bridge, float* attention);

typedef struct {
    uint64_t threats_routed;
    uint64_t responses_triggered;
    uint64_t recoveries_completed;
    float avg_threat_severity;
} brain_immune_thalamic_stats_t;

int brain_immune_thalamic_bridge_get_stats(const brain_immune_thalamic_bridge_t* bridge, brain_immune_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_IMMUNE_THALAMIC_BRIDGE_H */
