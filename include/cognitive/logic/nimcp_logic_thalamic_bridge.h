/**
 * @file nimcp_logic_thalamic_bridge.h
 * @brief Bridge between Logic system and thalamic router
 *
 * WHAT: Routes logical inferences through attention-gated thalamic pathways
 * WHY: Explicit reasoning requires conscious access via thalamic gating
 * HOW: Packages logic signals, routes via thalamic attention mechanism
 *
 * BIOLOGICAL BASIS:
 * - Logical reasoning involves prefrontal-parietal networks
 * - Thalamus coordinates attention during inference chains
 * - Working memory gated through thalamic circuits
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_LOGIC_THALAMIC_BRIDGE_H
#define NIMCP_LOGIC_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LOGIC_SIGNAL_PREMISE      0x0401
#define LOGIC_SIGNAL_INFERENCE    0x0402
#define LOGIC_SIGNAL_CONCLUSION   0x0403
#define LOGIC_SIGNAL_CONTRADICTION 0x0404

typedef struct {
    uint32_t signal_type;
    float logical_strength;
    float confidence;
    uint32_t inference_depth;
    void* content;
    uint32_t content_size;
    uint64_t timestamp_us;
} logic_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_contradiction_alert;
    float min_logical_strength;
    uint32_t max_inference_depth;
} logic_thalamic_config_t;

typedef struct logic_thalamic_bridge logic_thalamic_bridge_t;

logic_thalamic_config_t logic_thalamic_default_config(void);
logic_thalamic_bridge_t* logic_thalamic_bridge_create(void* logic, thalamic_router_t* router, const logic_thalamic_config_t* config);
void logic_thalamic_bridge_destroy(logic_thalamic_bridge_t* bridge);
int logic_thalamic_bridge_reset(logic_thalamic_bridge_t* bridge);
int logic_thalamic_route_inference(logic_thalamic_bridge_t* bridge, const logic_thalamic_signal_t* signal);
int logic_thalamic_route_conclusion(logic_thalamic_bridge_t* bridge, const void* conclusion, float confidence);
int logic_thalamic_set_attention(logic_thalamic_bridge_t* bridge, float attention);
int logic_thalamic_get_attention(const logic_thalamic_bridge_t* bridge, float* attention);

typedef struct {
    uint64_t inferences_routed;
    uint64_t conclusions_routed;
    uint64_t contradictions_flagged;
    float avg_inference_depth;
} logic_thalamic_stats_t;

int logic_thalamic_bridge_get_stats(const logic_thalamic_bridge_t* bridge, logic_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LOGIC_THALAMIC_BRIDGE_H */
