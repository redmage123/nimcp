/**
 * @file nimcp_hippocampus_thalamic_bridge.h
 * @brief Bridge between Hippocampus and thalamic router
 *
 * WHAT: Routes hippocampal memory signals through thalamic relay
 * WHY: Anterior thalamus is critical for episodic memory
 * HOW: Packages memory signals, routes via anterior thalamic nuclei
 *
 * BIOLOGICAL BASIS:
 * - Anterior thalamus supports episodic memory (Papez circuit)
 * - Midline thalamic nuclei involved in memory consolidation
 * - Hippocampus-thalamus-cortex loop for memory
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_HIPPOCAMPUS_THALAMIC_BRIDGE_H
#define NIMCP_HIPPOCAMPUS_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HIPPOCAMPUS_SIGNAL_ENCODE      0x2401
#define HIPPOCAMPUS_SIGNAL_RETRIEVE    0x2402
#define HIPPOCAMPUS_SIGNAL_CONSOLIDATE 0x2403
#define HIPPOCAMPUS_SIGNAL_SPATIAL     0x2404

typedef struct {
    uint32_t signal_type;
    float memory_strength;
    float novelty;
    float spatial_precision;
    void* content;
    uint32_t content_size;
    uint64_t timestamp_us;
} hippocampus_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_novelty_boost;
    float min_memory_strength;
    float novelty_threshold;
} hippocampus_thalamic_config_t;

typedef struct hippocampus_thalamic_bridge hippocampus_thalamic_bridge_t;

hippocampus_thalamic_config_t hippocampus_thalamic_default_config(void);
hippocampus_thalamic_bridge_t* hippocampus_thalamic_bridge_create(void* hippocampus, thalamic_router_t* router, const hippocampus_thalamic_config_t* config);
void hippocampus_thalamic_bridge_destroy(hippocampus_thalamic_bridge_t* bridge);
int hippocampus_thalamic_bridge_reset(hippocampus_thalamic_bridge_t* bridge);
int hippocampus_thalamic_route_signal(hippocampus_thalamic_bridge_t* bridge, const hippocampus_thalamic_signal_t* signal);
int hippocampus_thalamic_route_retrieval(hippocampus_thalamic_bridge_t* bridge, const void* cue, float strength);
int hippocampus_thalamic_set_attention(hippocampus_thalamic_bridge_t* bridge, float attention);
int hippocampus_thalamic_get_attention(const hippocampus_thalamic_bridge_t* bridge, float* attention);

typedef struct {
    uint64_t encodings_routed;
    uint64_t retrievals_routed;
    uint64_t consolidations_triggered;
    float avg_memory_strength;
} hippocampus_thalamic_stats_t;

int hippocampus_thalamic_bridge_get_stats(const hippocampus_thalamic_bridge_t* bridge, hippocampus_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HIPPOCAMPUS_THALAMIC_BRIDGE_H */
