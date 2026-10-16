/**
 * @file nimcp_predictive_thalamic_bridge.h
 * @brief Bridge between Predictive Coding system and thalamic router
 *
 * WHAT: Routes prediction errors through attention-gated thalamic pathways
 * WHY: Prediction errors drive learning via thalamic surprise signals
 * HOW: Packages prediction signals, routes via thalamic attention mechanism
 *
 * BIOLOGICAL BASIS:
 * - Predictive coding involves hierarchical cortical computation
 * - Thalamus relays prediction errors between cortical levels
 * - First-order thalamus carries sensory predictions
 * - Higher-order thalamus carries cortical predictions
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_PREDICTIVE_THALAMIC_BRIDGE_H
#define NIMCP_PREDICTIVE_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PREDICTIVE_SIGNAL_PREDICTION    0x0801
#define PREDICTIVE_SIGNAL_ERROR         0x0802
#define PREDICTIVE_SIGNAL_UPDATE        0x0803
#define PREDICTIVE_SIGNAL_PRECISION     0x0804

typedef struct {
    uint32_t signal_type;
    float prediction_confidence;
    float error_magnitude;
    float precision_weight;
    uint32_t hierarchy_level;
    void* content;
    uint32_t content_size;
    uint64_t timestamp_us;
} predictive_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_error_amplification;
    float min_error_threshold;
    float precision_threshold;
} predictive_thalamic_config_t;

typedef struct predictive_thalamic_bridge predictive_thalamic_bridge_t;

predictive_thalamic_config_t predictive_thalamic_default_config(void);
predictive_thalamic_bridge_t* predictive_thalamic_bridge_create(void* predictive, thalamic_router_t* router, const predictive_thalamic_config_t* config);
void predictive_thalamic_bridge_destroy(predictive_thalamic_bridge_t* bridge);
int predictive_thalamic_bridge_reset(predictive_thalamic_bridge_t* bridge);
int predictive_thalamic_route_error(predictive_thalamic_bridge_t* bridge, const predictive_thalamic_signal_t* signal);
int predictive_thalamic_route_update(predictive_thalamic_bridge_t* bridge, const void* update, uint32_t level);
int predictive_thalamic_set_attention(predictive_thalamic_bridge_t* bridge, float attention);
int predictive_thalamic_get_attention(const predictive_thalamic_bridge_t* bridge, float* attention);

typedef struct {
    uint64_t predictions_routed;
    uint64_t errors_routed;
    uint64_t updates_triggered;
    float avg_error_magnitude;
} predictive_thalamic_stats_t;

int predictive_thalamic_bridge_get_stats(const predictive_thalamic_bridge_t* bridge, predictive_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PREDICTIVE_THALAMIC_BRIDGE_H */
