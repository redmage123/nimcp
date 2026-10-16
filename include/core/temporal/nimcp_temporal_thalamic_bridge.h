/**
 * @file nimcp_temporal_thalamic_bridge.h
 * @brief Bridge between Temporal Cortex and thalamic router
 *
 * WHAT: Routes temporal lobe signals through thalamic relay
 * WHY: Temporal cortex connects with pulvinar and MGN
 * HOW: Packages language/object signals, routes via appropriate pathway
 *
 * BIOLOGICAL BASIS:
 * - Pulvinar connects with temporal association areas
 * - MGN relays auditory information
 * - Language and object recognition involve temporal-thalamic circuits
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_TEMPORAL_THALAMIC_BRIDGE_H
#define NIMCP_TEMPORAL_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TEMPORAL_SIGNAL_LANGUAGE     0x2901
#define TEMPORAL_SIGNAL_OBJECT       0x2902
#define TEMPORAL_SIGNAL_SEMANTIC     0x2903
#define TEMPORAL_SIGNAL_FACE         0x2904

typedef struct {
    uint32_t signal_type;
    float recognition_confidence;
    float semantic_activation;
    float attention_weight;
    void* content;
    uint32_t content_size;
    uint64_t timestamp_us;
} temporal_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_language_priority;
    float min_recognition_confidence;
    float semantic_threshold;
} temporal_thalamic_config_t;

typedef struct temporal_thalamic_bridge temporal_thalamic_bridge_t;

temporal_thalamic_config_t temporal_thalamic_default_config(void);
temporal_thalamic_bridge_t* temporal_thalamic_bridge_create(void* temporal, thalamic_router_t* router, const temporal_thalamic_config_t* config);
void temporal_thalamic_bridge_destroy(temporal_thalamic_bridge_t* bridge);
int temporal_thalamic_bridge_reset(temporal_thalamic_bridge_t* bridge);
int temporal_thalamic_route_signal(temporal_thalamic_bridge_t* bridge, const temporal_thalamic_signal_t* signal);
int temporal_thalamic_route_language(temporal_thalamic_bridge_t* bridge, const void* content, float priority);
int temporal_thalamic_set_attention(temporal_thalamic_bridge_t* bridge, float attention);
int temporal_thalamic_get_attention(const temporal_thalamic_bridge_t* bridge, float* attention);

typedef struct {
    uint64_t language_signals_routed;
    uint64_t objects_recognized;
    uint64_t semantic_activations;
    float avg_recognition_confidence;
} temporal_thalamic_stats_t;

int temporal_thalamic_bridge_get_stats(const temporal_thalamic_bridge_t* bridge, temporal_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TEMPORAL_THALAMIC_BRIDGE_H */
