/**
 * @file nimcp_cerebellum_thalamic_bridge.h
 * @brief Bridge between Cerebellum and thalamic router
 *
 * WHAT: Routes cerebellar output through thalamic relay (VL)
 * WHY: Cerebellar output reaches cortex via ventral lateral thalamus
 * HOW: Packages cerebellar signals, routes via VL nucleus pathway
 *
 * BIOLOGICAL BASIS:
 * - Deep cerebellar nuclei project to VL thalamus
 * - VL relays to motor and premotor cortex
 * - Timing and coordination signals pass through
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_CEREBELLUM_THALAMIC_BRIDGE_H
#define NIMCP_CEREBELLUM_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CEREBELLUM_SIGNAL_TIMING      0x2501
#define CEREBELLUM_SIGNAL_CORRECTION  0x2502
#define CEREBELLUM_SIGNAL_LEARNING    0x2503
#define CEREBELLUM_SIGNAL_COORDINATE  0x2504

typedef struct {
    uint32_t signal_type;
    float timing_precision;
    float error_magnitude;
    float learning_signal;
    void* content;
    uint32_t content_size;
    uint64_t timestamp_us;
} cerebellum_thalamic_signal_t;

typedef struct {
    bool enable_timing_relay;
    bool enable_error_routing;
    float min_timing_precision;
    float error_threshold;
} cerebellum_thalamic_config_t;

typedef struct cerebellum_thalamic_bridge cerebellum_thalamic_bridge_t;

cerebellum_thalamic_config_t cerebellum_thalamic_default_config(void);
cerebellum_thalamic_bridge_t* cerebellum_thalamic_bridge_create(void* cerebellum, thalamic_router_t* router, const cerebellum_thalamic_config_t* config);
void cerebellum_thalamic_bridge_destroy(cerebellum_thalamic_bridge_t* bridge);
int cerebellum_thalamic_bridge_reset(cerebellum_thalamic_bridge_t* bridge);
int cerebellum_thalamic_route_signal(cerebellum_thalamic_bridge_t* bridge, const cerebellum_thalamic_signal_t* signal);
int cerebellum_thalamic_route_correction(cerebellum_thalamic_bridge_t* bridge, const void* correction, float magnitude);
int cerebellum_thalamic_set_attention(cerebellum_thalamic_bridge_t* bridge, float attention);
int cerebellum_thalamic_get_attention(const cerebellum_thalamic_bridge_t* bridge, float* attention);

typedef struct {
    uint64_t timing_signals_routed;
    uint64_t corrections_applied;
    uint64_t learning_signals;
    float avg_timing_precision;
} cerebellum_thalamic_stats_t;

int cerebellum_thalamic_bridge_get_stats(const cerebellum_thalamic_bridge_t* bridge, cerebellum_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CEREBELLUM_THALAMIC_BRIDGE_H */
