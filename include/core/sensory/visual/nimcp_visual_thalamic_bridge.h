/**
 * @file nimcp_visual_thalamic_bridge.h
 * @brief Bridge between Visual processing and thalamic router
 *
 * WHAT: Routes visual signals through thalamic relay (LGN)
 * WHY: All visual information passes through LGN to cortex
 * HOW: Packages visual signals, routes via lateral geniculate nucleus pathway
 *
 * BIOLOGICAL BASIS:
 * - LGN (lateral geniculate nucleus) is primary visual relay
 * - Pulvinar coordinates visual attention
 * - Thalamic reticular nucleus modulates visual gating
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_VISUAL_THALAMIC_BRIDGE_H
#define NIMCP_VISUAL_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VISUAL_SIGNAL_PRIMARY      0x2001
#define VISUAL_SIGNAL_FEATURE      0x2002
#define VISUAL_SIGNAL_MOTION       0x2003
#define VISUAL_SIGNAL_ATTENTION    0x2004

typedef struct {
    uint32_t signal_type;
    float visual_salience;
    float attention_weight;
    uint32_t retinotopic_x;
    uint32_t retinotopic_y;
    void* content;
    uint32_t content_size;
    uint64_t timestamp_us;
} visual_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_pulvinar_modulation;
    float min_salience_threshold;
    float attention_boost;
} visual_thalamic_config_t;

typedef struct visual_thalamic_bridge visual_thalamic_bridge_t;

visual_thalamic_config_t visual_thalamic_default_config(void);
visual_thalamic_bridge_t* visual_thalamic_bridge_create(void* visual, thalamic_router_t* router, const visual_thalamic_config_t* config);
void visual_thalamic_bridge_destroy(visual_thalamic_bridge_t* bridge);
int visual_thalamic_bridge_reset(visual_thalamic_bridge_t* bridge);
int visual_thalamic_route_signal(visual_thalamic_bridge_t* bridge, const visual_thalamic_signal_t* signal);
int visual_thalamic_route_attention(visual_thalamic_bridge_t* bridge, uint32_t x, uint32_t y, float weight);
int visual_thalamic_set_attention(visual_thalamic_bridge_t* bridge, float attention);
int visual_thalamic_get_attention(const visual_thalamic_bridge_t* bridge, float* attention);

typedef struct {
    uint64_t signals_relayed;
    uint64_t attention_shifts;
    uint64_t features_routed;
    float avg_visual_salience;
} visual_thalamic_stats_t;

int visual_thalamic_bridge_get_stats(const visual_thalamic_bridge_t* bridge, visual_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_VISUAL_THALAMIC_BRIDGE_H */
