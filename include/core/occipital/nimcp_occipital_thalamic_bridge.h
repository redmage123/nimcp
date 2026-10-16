/**
 * @file nimcp_occipital_thalamic_bridge.h
 * @brief Bridge between Occipital Cortex and thalamic router
 *
 * WHAT: Routes occipital visual signals through thalamic relay (LGN)
 * WHY: All primary visual information passes through LGN
 * HOW: Packages visual signals, routes via lateral geniculate pathway
 *
 * BIOLOGICAL BASIS:
 * - LGN (lateral geniculate nucleus) relays to V1
 * - Pulvinar involved in higher visual processing
 * - Retinotopic organization preserved through relay
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_OCCIPITAL_THALAMIC_BRIDGE_H
#define NIMCP_OCCIPITAL_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OCCIPITAL_SIGNAL_V1          0x2D01
#define OCCIPITAL_SIGNAL_V2          0x2D02
#define OCCIPITAL_SIGNAL_DORSAL      0x2D03
#define OCCIPITAL_SIGNAL_VENTRAL     0x2D04

typedef struct {
    uint32_t signal_type;
    float visual_intensity;
    float contrast;
    float spatial_frequency;
    void* content;
    uint32_t content_size;
    uint64_t timestamp_us;
} occipital_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_contrast_boost;
    float min_visual_intensity;
    float contrast_threshold;
} occipital_thalamic_config_t;

typedef struct occipital_thalamic_bridge occipital_thalamic_bridge_t;

occipital_thalamic_config_t occipital_thalamic_default_config(void);
occipital_thalamic_bridge_t* occipital_thalamic_bridge_create(void* occipital, thalamic_router_t* router, const occipital_thalamic_config_t* config);
void occipital_thalamic_bridge_destroy(occipital_thalamic_bridge_t* bridge);
int occipital_thalamic_bridge_reset(occipital_thalamic_bridge_t* bridge);
int occipital_thalamic_route_signal(occipital_thalamic_bridge_t* bridge, const occipital_thalamic_signal_t* signal);
int occipital_thalamic_route_v1(occipital_thalamic_bridge_t* bridge, const void* visual_data, float intensity);
int occipital_thalamic_set_attention(occipital_thalamic_bridge_t* bridge, float attention);
int occipital_thalamic_get_attention(const occipital_thalamic_bridge_t* bridge, float* attention);

typedef struct {
    uint64_t v1_signals_routed;
    uint64_t dorsal_signals;
    uint64_t ventral_signals;
    float avg_visual_intensity;
} occipital_thalamic_stats_t;

int occipital_thalamic_bridge_get_stats(const occipital_thalamic_bridge_t* bridge, occipital_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OCCIPITAL_THALAMIC_BRIDGE_H */
