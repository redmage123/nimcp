/**
 * @file nimcp_free_energy_thalamic_bridge.h
 * @brief Bridge between Free Energy Principle system and thalamic router
 *
 * WHAT: Routes FEP prediction error signals through attention-gated thalamic pathways
 * WHY: Prediction errors require thalamic gating for conscious processing
 * HOW: Packages FEP signals, routes via thalamic attention with precision weighting
 *
 * BIOLOGICAL BASIS:
 * - Thalamus serves as the relay for prediction errors in predictive coding
 * - High-precision prediction errors get enhanced routing priority
 * - Attention modulates which prediction errors reach consciousness
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_FREE_ENERGY_THALAMIC_BRIDGE_H
#define NIMCP_FREE_ENERGY_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FEP_SIGNAL_PREDICTION_ERROR 0x0801
#define FEP_SIGNAL_PRECISION_UPDATE 0x0802
#define FEP_SIGNAL_MODEL_UPDATE     0x0803
#define FEP_SIGNAL_ACTIVE_INFERENCE 0x0804

typedef struct {
    uint32_t signal_type;
    float prediction_error;
    float precision;
    float urgency;
    void* content;
    uint32_t content_size;
    uint64_t timestamp_us;
} free_energy_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_precision_boost;
    float min_error_threshold;
    float precision_boost;
} free_energy_thalamic_config_t;

typedef struct free_energy_thalamic_bridge free_energy_thalamic_bridge_t;

free_energy_thalamic_config_t free_energy_thalamic_default_config(void);
free_energy_thalamic_bridge_t* free_energy_thalamic_bridge_create(void* free_energy, thalamic_router_t* router, const free_energy_thalamic_config_t* config);
void free_energy_thalamic_bridge_destroy(free_energy_thalamic_bridge_t* bridge);
int free_energy_thalamic_bridge_reset(free_energy_thalamic_bridge_t* bridge);
int free_energy_thalamic_route_prediction_error(free_energy_thalamic_bridge_t* bridge, const free_energy_thalamic_signal_t* signal);
int free_energy_thalamic_route_model_update(free_energy_thalamic_bridge_t* bridge, const void* model, float importance);
int free_energy_thalamic_set_attention(free_energy_thalamic_bridge_t* bridge, float attention);
int free_energy_thalamic_get_attention(free_energy_thalamic_bridge_t* bridge, float* attention);

typedef struct {
    uint64_t errors_routed;
    uint64_t model_updates;
    uint64_t inference_actions;
    float avg_precision;
} free_energy_thalamic_stats_t;

int free_energy_thalamic_bridge_get_stats(free_energy_thalamic_bridge_t* bridge, free_energy_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FREE_ENERGY_THALAMIC_BRIDGE_H */
