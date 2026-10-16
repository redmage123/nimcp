/**
 * @file nimcp_memory_thalamic_bridge.h
 * @brief Bridge between Memory system and thalamic router
 *
 * WHAT: Routes memory signals through thalamic attention pathways
 * WHY: Memory encoding/retrieval requires conscious attention
 * HOW: Packages memory signals, routes via anterior/MD thalamic pathways
 *
 * BIOLOGICAL BASIS:
 * - Anterior thalamus is key for episodic memory
 * - MD nucleus supports working memory integration
 * - Reuniens nucleus links hippocampus to PFC
 * - Pulvinar coordinates attention during retrieval
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_MEMORY_THALAMIC_BRIDGE_H
#define NIMCP_MEMORY_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MEMORY_SIGNAL_ENCODE         0x3101  /**< Encoding request */
#define MEMORY_SIGNAL_RETRIEVE       0x3102  /**< Retrieval request */
#define MEMORY_SIGNAL_CONSOLIDATE    0x3103  /**< Consolidation signal */
#define MEMORY_SIGNAL_UPDATE         0x3104  /**< Memory update */

typedef struct {
    uint32_t signal_type;
    float memory_urgency;
    float strength;
    float salience;
    float emotional_weight;
    void* content;
    uint32_t content_size;
    uint64_t timestamp_us;
} memory_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_emotional_boost;
    bool enable_consolidation_routing;
    float min_urgency_threshold;
    float emotional_boost_factor;
} memory_thalamic_config_t;

typedef struct {
    uint64_t encodings_routed;
    uint64_t retrievals_routed;
    uint64_t consolidations;
    uint64_t updates;
    uint64_t signals_gated;
    float avg_strength;
    float avg_emotional_weight;
} memory_thalamic_stats_t;

typedef struct memory_thalamic_bridge memory_thalamic_bridge_t;

memory_thalamic_config_t memory_thalamic_default_config(void);
memory_thalamic_bridge_t* memory_thalamic_bridge_create(
    void* memory, thalamic_router_t* router,
    const memory_thalamic_config_t* config);
void memory_thalamic_bridge_destroy(memory_thalamic_bridge_t* bridge);
int memory_thalamic_bridge_reset(memory_thalamic_bridge_t* bridge);

int memory_thalamic_route_signal(
    memory_thalamic_bridge_t* bridge,
    const memory_thalamic_signal_t* signal);
int memory_thalamic_route_encode(
    memory_thalamic_bridge_t* bridge,
    float strength, float emotional_weight);
int memory_thalamic_route_retrieve(
    memory_thalamic_bridge_t* bridge,
    float salience, float urgency);

int memory_thalamic_set_attention(memory_thalamic_bridge_t* bridge, float attention);
int memory_thalamic_get_attention(memory_thalamic_bridge_t* bridge, float* attention);
int memory_thalamic_bridge_get_stats(
    const memory_thalamic_bridge_t* bridge,
    memory_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MEMORY_THALAMIC_BRIDGE_H */
