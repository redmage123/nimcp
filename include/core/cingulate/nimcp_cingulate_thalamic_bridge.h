/**
 * @file nimcp_cingulate_thalamic_bridge.h
 * @brief Bridge between Cingulate Cortex and thalamic router
 *
 * WHAT: Routes cingulate signals through thalamic relay
 * WHY: Anterior thalamus connects with cingulate (Papez circuit)
 * HOW: Packages error/conflict signals, routes via anterior thalamic pathway
 *
 * BIOLOGICAL BASIS:
 * - Anterior thalamic nuclei connect with cingulate
 * - Part of limbic thalamus for emotional processing
 * - Error monitoring and conflict signals relayed
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_CINGULATE_THALAMIC_BRIDGE_H
#define NIMCP_CINGULATE_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CINGULATE_SIGNAL_ERROR       0x2701
#define CINGULATE_SIGNAL_CONFLICT    0x2702
#define CINGULATE_SIGNAL_PAIN        0x2703
#define CINGULATE_SIGNAL_MOTIVATION  0x2704

typedef struct {
    uint32_t signal_type;
    float error_magnitude;
    float conflict_level;
    float urgency;
    void* content;
    uint32_t content_size;
    uint64_t timestamp_us;
} cingulate_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_error_priority;
    float min_error_threshold;
    float conflict_threshold;
} cingulate_thalamic_config_t;

typedef struct cingulate_thalamic_bridge cingulate_thalamic_bridge_t;

cingulate_thalamic_config_t cingulate_thalamic_default_config(void);
cingulate_thalamic_bridge_t* cingulate_thalamic_bridge_create(void* cingulate, thalamic_router_t* router, const cingulate_thalamic_config_t* config);
void cingulate_thalamic_bridge_destroy(cingulate_thalamic_bridge_t* bridge);
int cingulate_thalamic_bridge_reset(cingulate_thalamic_bridge_t* bridge);
int cingulate_thalamic_route_signal(cingulate_thalamic_bridge_t* bridge, const cingulate_thalamic_signal_t* signal);
int cingulate_thalamic_route_error(cingulate_thalamic_bridge_t* bridge, const void* error, float magnitude);
int cingulate_thalamic_set_attention(cingulate_thalamic_bridge_t* bridge, float attention);
int cingulate_thalamic_get_attention(const cingulate_thalamic_bridge_t* bridge, float* attention);

typedef struct {
    uint64_t errors_routed;
    uint64_t conflicts_detected;
    uint64_t pain_signals;
    float avg_error_magnitude;
} cingulate_thalamic_stats_t;

int cingulate_thalamic_bridge_get_stats(const cingulate_thalamic_bridge_t* bridge, cingulate_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CINGULATE_THALAMIC_BRIDGE_H */
