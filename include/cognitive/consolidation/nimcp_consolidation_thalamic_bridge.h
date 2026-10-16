/**
 * @file nimcp_consolidation_thalamic_bridge.h
 * @brief Bridge between Memory Consolidation system and thalamic router
 *
 * WHAT: Routes consolidation signals through attention-gated thalamic pathways
 * WHY: Memory consolidation requires thalamic gating during sleep/wake transitions
 * HOW: Packages consolidation signals, routes via thalamic attention mechanism
 *
 * BIOLOGICAL BASIS:
 * - Sleep spindles involve thalamo-cortical loops during consolidation
 * - Thalamus gates hippocampal reactivation during NREM sleep
 * - High-salience memories get enhanced thalamic routing priority
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_CONSOLIDATION_THALAMIC_BRIDGE_H
#define NIMCP_CONSOLIDATION_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CONSOLIDATION_SIGNAL_ENCODE     0x0301
#define CONSOLIDATION_SIGNAL_REPLAY     0x0302
#define CONSOLIDATION_SIGNAL_TRANSFER   0x0303
#define CONSOLIDATION_SIGNAL_COMPLETE   0x0304

typedef struct {
    uint32_t signal_type;
    float memory_salience;
    float emotional_weight;
    float urgency;
    void* content;
    uint32_t content_size;
    uint64_t timestamp_us;
} consolidation_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_emotional_boost;
    float min_memory_salience;
    float replay_boost;
} consolidation_thalamic_config_t;

typedef struct consolidation_thalamic_bridge consolidation_thalamic_bridge_t;

consolidation_thalamic_config_t consolidation_thalamic_default_config(void);
consolidation_thalamic_bridge_t* consolidation_thalamic_bridge_create(void* consolidation, thalamic_router_t* router, const consolidation_thalamic_config_t* config);
void consolidation_thalamic_bridge_destroy(consolidation_thalamic_bridge_t* bridge);
int consolidation_thalamic_bridge_reset(consolidation_thalamic_bridge_t* bridge);
int consolidation_thalamic_route_encode(consolidation_thalamic_bridge_t* bridge, const consolidation_thalamic_signal_t* signal);
int consolidation_thalamic_route_replay(consolidation_thalamic_bridge_t* bridge, const void* memory, float importance);
int consolidation_thalamic_set_attention(consolidation_thalamic_bridge_t* bridge, float attention);
int consolidation_thalamic_get_attention(consolidation_thalamic_bridge_t* bridge, float* attention);

typedef struct {
    uint64_t encodes_routed;
    uint64_t replays_triggered;
    uint64_t transfers_completed;
    float avg_memory_salience;
} consolidation_thalamic_stats_t;

int consolidation_thalamic_bridge_get_stats(consolidation_thalamic_bridge_t* bridge, consolidation_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CONSOLIDATION_THALAMIC_BRIDGE_H */
