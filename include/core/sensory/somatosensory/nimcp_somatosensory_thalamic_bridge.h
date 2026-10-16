/**
 * @file nimcp_somatosensory_thalamic_bridge.h
 * @brief Bridge between Somatosensory processing and thalamic router
 *
 * WHAT: Routes touch/proprioception through thalamic relay (VPL/VPM)
 * WHY: All somatosensory information passes through VPL/VPM to cortex
 * HOW: Packages sensory signals, routes via ventral posterior nuclei pathway
 *
 * BIOLOGICAL BASIS:
 * - VPL (body) and VPM (face) are somatosensory relays
 * - Somatotopic organization preserved
 * - Pain signals also relayed through thalamus
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_SOMATOSENSORY_THALAMIC_BRIDGE_H
#define NIMCP_SOMATOSENSORY_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SOMATOSENSORY_SIGNAL_TOUCH       0x2201
#define SOMATOSENSORY_SIGNAL_PROPRIOCEPT 0x2202
#define SOMATOSENSORY_SIGNAL_PAIN        0x2203
#define SOMATOSENSORY_SIGNAL_TEMPERATURE 0x2204

typedef struct {
    uint32_t signal_type;
    float somatosensory_salience;
    float attention_weight;
    uint32_t body_region;
    void* content;
    uint32_t content_size;
    uint64_t timestamp_us;
} somatosensory_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_pain_priority;
    float min_salience_threshold;
    float pain_boost;
} somatosensory_thalamic_config_t;

typedef struct somatosensory_thalamic_bridge somatosensory_thalamic_bridge_t;

somatosensory_thalamic_config_t somatosensory_thalamic_default_config(void);
somatosensory_thalamic_bridge_t* somatosensory_thalamic_bridge_create(void* somatosensory, thalamic_router_t* router, const somatosensory_thalamic_config_t* config);
void somatosensory_thalamic_bridge_destroy(somatosensory_thalamic_bridge_t* bridge);
int somatosensory_thalamic_bridge_reset(somatosensory_thalamic_bridge_t* bridge);
int somatosensory_thalamic_route_signal(somatosensory_thalamic_bridge_t* bridge, const somatosensory_thalamic_signal_t* signal);
int somatosensory_thalamic_route_pain(somatosensory_thalamic_bridge_t* bridge, uint32_t region, float intensity);
int somatosensory_thalamic_set_attention(somatosensory_thalamic_bridge_t* bridge, float attention);
int somatosensory_thalamic_get_attention(const somatosensory_thalamic_bridge_t* bridge, float* attention);

typedef struct {
    uint64_t signals_relayed;
    uint64_t pain_signals;
    uint64_t proprioceptive_updates;
    float avg_somatosensory_salience;
} somatosensory_thalamic_stats_t;

int somatosensory_thalamic_bridge_get_stats(const somatosensory_thalamic_bridge_t* bridge, somatosensory_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SOMATOSENSORY_THALAMIC_BRIDGE_H */
