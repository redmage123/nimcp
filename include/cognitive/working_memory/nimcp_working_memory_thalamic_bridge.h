/**
 * @file nimcp_working_memory_thalamic_bridge.h
 * @brief Bridge between Working Memory system and thalamic router
 *
 * WHAT: Routes working memory signals through thalamic attention pathways
 * WHY: Working memory requires attention for maintenance and updating
 * HOW: Packages WM signals, routes via MD nucleus pathway
 *
 * BIOLOGICAL BASIS:
 * - WM involves DLPFC-MD thalamic circuits
 * - MD nucleus is critical for WM maintenance
 * - Pulvinar coordinates attention during WM operations
 * - Reuniens nucleus supports hippocampal-PFC WM integration
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_WORKING_MEMORY_THALAMIC_BRIDGE_H
#define NIMCP_WORKING_MEMORY_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WM_SIGNAL_ENCODE          0x3801  /**< WM encoding */
#define WM_SIGNAL_MAINTAIN        0x3802  /**< Maintenance signal */
#define WM_SIGNAL_UPDATE          0x3803  /**< WM update */
#define WM_SIGNAL_RETRIEVE        0x3804  /**< WM retrieval */
#define WM_SIGNAL_CLEAR           0x3805  /**< Clear WM slot */

typedef struct {
    uint32_t signal_type;
    float wm_urgency;
    float capacity_used;
    float item_priority;
    float decay_rate;
    void* content;
    uint32_t content_size;
    uint64_t timestamp_us;
} working_memory_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_priority_routing;
    bool enable_capacity_check;
    float min_urgency_threshold;
    float priority_boost;
} working_memory_thalamic_config_t;

typedef struct {
    uint64_t encodings;
    uint64_t maintenances;
    uint64_t updates;
    uint64_t retrievals;
    uint64_t clears;
    uint64_t signals_gated;
    float avg_capacity_used;
    float avg_item_priority;
} working_memory_thalamic_stats_t;

typedef struct working_memory_thalamic_bridge working_memory_thalamic_bridge_t;

working_memory_thalamic_config_t working_memory_thalamic_default_config(void);
working_memory_thalamic_bridge_t* working_memory_thalamic_bridge_create(
    void* working_memory, thalamic_router_t* router,
    const working_memory_thalamic_config_t* config);
void working_memory_thalamic_bridge_destroy(working_memory_thalamic_bridge_t* bridge);
int working_memory_thalamic_bridge_reset(working_memory_thalamic_bridge_t* bridge);

int working_memory_thalamic_route_signal(
    working_memory_thalamic_bridge_t* bridge,
    const working_memory_thalamic_signal_t* signal);
int working_memory_thalamic_route_encode(
    working_memory_thalamic_bridge_t* bridge,
    float priority, float urgency);
int working_memory_thalamic_route_update(
    working_memory_thalamic_bridge_t* bridge,
    float priority, float urgency);

int working_memory_thalamic_set_attention(working_memory_thalamic_bridge_t* bridge, float attention);
int working_memory_thalamic_get_attention(const working_memory_thalamic_bridge_t* bridge, float* attention);
int working_memory_thalamic_bridge_get_stats(
    const working_memory_thalamic_bridge_t* bridge,
    working_memory_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_WORKING_MEMORY_THALAMIC_BRIDGE_H */
