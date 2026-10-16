/**
 * @file nimcp_empathetic_response_thalamic_bridge.h
 * @brief Bridge between Empathetic Response system and thalamic router
 *
 * WHAT: Routes empathetic response signals through attention-gated thalamic pathways
 * WHY: Empathetic responses require conscious awareness and attention allocation
 * HOW: Packages empathy signals, routes via thalamic attention mechanism
 *
 * BIOLOGICAL BASIS:
 * - Empathetic responses involve conscious processing via thalamo-cortical loops
 * - High-distress signals from others get enhanced routing priority
 * - Attention modulates which empathetic responses reach consciousness
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_EMPATHETIC_RESPONSE_THALAMIC_BRIDGE_H
#define NIMCP_EMPATHETIC_RESPONSE_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EMPATHETIC_SIGNAL_RECOGNIZE     0x0501
#define EMPATHETIC_SIGNAL_RESPOND       0x0502
#define EMPATHETIC_SIGNAL_COMFORT       0x0503
#define EMPATHETIC_SIGNAL_MIRROR        0x0504

typedef struct {
    uint32_t signal_type;
    float distress_level;
    float emotional_intensity;
    float urgency;
    void* content;
    uint32_t content_size;
    uint64_t timestamp_us;
} empathetic_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_distress_boost;
    float min_distress_threshold;
    float mirror_boost;
} empathetic_response_thalamic_config_t;

typedef struct empathetic_response_thalamic_bridge empathetic_response_thalamic_bridge_t;

empathetic_response_thalamic_config_t empathetic_response_thalamic_default_config(void);
empathetic_response_thalamic_bridge_t* empathetic_response_thalamic_bridge_create(void* empathetic_response, thalamic_router_t* router, const empathetic_response_thalamic_config_t* config);
void empathetic_response_thalamic_bridge_destroy(empathetic_response_thalamic_bridge_t* bridge);
int empathetic_response_thalamic_bridge_reset(empathetic_response_thalamic_bridge_t* bridge);
int empathetic_response_thalamic_route_recognition(empathetic_response_thalamic_bridge_t* bridge, const empathetic_thalamic_signal_t* signal);
int empathetic_response_thalamic_route_response(empathetic_response_thalamic_bridge_t* bridge, const void* response, float intensity);
int empathetic_response_thalamic_set_attention(empathetic_response_thalamic_bridge_t* bridge, float attention);
int empathetic_response_thalamic_get_attention(empathetic_response_thalamic_bridge_t* bridge, float* attention);

typedef struct {
    uint64_t recognitions_routed;
    uint64_t responses_generated;
    uint64_t comforts_provided;
    float avg_distress_level;
} empathetic_response_thalamic_stats_t;

int empathetic_response_thalamic_bridge_get_stats(empathetic_response_thalamic_bridge_t* bridge, empathetic_response_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EMPATHETIC_RESPONSE_THALAMIC_BRIDGE_H */
