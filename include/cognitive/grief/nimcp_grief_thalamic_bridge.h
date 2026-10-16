/**
 * @file nimcp_grief_thalamic_bridge.h
 * @brief Bridge between Grief system and thalamic router
 *
 * WHAT: Routes grief signals through attention-gated thalamic pathways
 * WHY: Grief processing requires conscious integration via thalamic gating
 * HOW: Packages grief signals, routes via thalamic attention mechanism
 *
 * BIOLOGICAL BASIS:
 * - Grief involves anterior cingulate and limbic circuits
 * - Thalamus gates emotional pain signals
 * - Attention to grief facilitates processing
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_GRIEF_THALAMIC_BRIDGE_H
#define NIMCP_GRIEF_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GRIEF_SIGNAL_LOSS         0x1101
#define GRIEF_SIGNAL_PROCESSING   0x1102
#define GRIEF_SIGNAL_ADAPTATION   0x1103
#define GRIEF_SIGNAL_RESOLUTION   0x1104

typedef struct {
    uint32_t signal_type;
    float grief_intensity;
    float processing_stage;
    float adaptation_progress;
    void* content;
    uint32_t content_size;
    uint64_t timestamp_us;
} grief_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_gradual_processing;
    float min_intensity_threshold;
    float adaptation_threshold;
} grief_thalamic_config_t;

typedef struct grief_thalamic_bridge grief_thalamic_bridge_t;

grief_thalamic_config_t grief_thalamic_default_config(void);
grief_thalamic_bridge_t* grief_thalamic_bridge_create(void* grief, thalamic_router_t* router, const grief_thalamic_config_t* config);
void grief_thalamic_bridge_destroy(grief_thalamic_bridge_t* bridge);
int grief_thalamic_bridge_reset(grief_thalamic_bridge_t* bridge);
int grief_thalamic_route_loss(grief_thalamic_bridge_t* bridge, const grief_thalamic_signal_t* signal);
int grief_thalamic_route_processing(grief_thalamic_bridge_t* bridge, const void* stage, float progress);
int grief_thalamic_set_attention(grief_thalamic_bridge_t* bridge, float attention);
int grief_thalamic_get_attention(grief_thalamic_bridge_t* bridge, float* attention);

typedef struct {
    uint64_t losses_processed;
    uint64_t processing_stages;
    uint64_t adaptations_achieved;
    float avg_grief_intensity;
} grief_thalamic_stats_t;

int grief_thalamic_bridge_get_stats(grief_thalamic_bridge_t* bridge, grief_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GRIEF_THALAMIC_BRIDGE_H */
