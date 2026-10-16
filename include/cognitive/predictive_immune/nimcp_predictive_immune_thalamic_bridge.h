/**
 * @file nimcp_predictive_immune_thalamic_bridge.h
 * @brief Bridge between Predictive Immune system and thalamic routing
 *
 * WHAT: Routes predictive-immune signals through thalamic attention pathways
 * WHY: Immune predictions require conscious awareness for behavioral response
 * HOW: Gates interoceptive signals via attention; routes cytokine updates
 *
 * BIOLOGICAL BASIS:
 * - Insular cortex integrates immune signals via thalamic relay
 * - Attention modulates awareness of immune state changes
 * - Sickness behavior requires conscious immune signal processing
 * - Thalamic gating filters low-urgency immune fluctuations
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_PREDICTIVE_IMMUNE_THALAMIC_BRIDGE_H
#define NIMCP_PREDICTIVE_IMMUNE_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PRED_IMMUNE_SIGNAL_INTEROCEPTION    0x0D01
#define PRED_IMMUNE_SIGNAL_CYTOKINE         0x0D02
#define PRED_IMMUNE_SIGNAL_SICKNESS         0x0D03
#define PRED_IMMUNE_SIGNAL_RECOVERY         0x0D04

typedef struct {
    uint32_t signal_type;
    float interoceptive_urgency;
    float immune_salience;
    float prediction_error;
    void* content;
    uint32_t content_size;
    uint64_t timestamp_us;
} predictive_immune_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_urgency_boost;
    float min_salience_threshold;
    float urgency_boost;
} predictive_immune_thalamic_config_t;

typedef struct {
    uint32_t interoceptions_routed;
    uint32_t cytokine_updates;
    uint32_t sickness_signals;
    uint32_t signals_gated;
    float avg_immune_salience;
} predictive_immune_thalamic_stats_t;

typedef struct predictive_immune_thalamic_bridge predictive_immune_thalamic_bridge_t;

predictive_immune_thalamic_config_t predictive_immune_thalamic_default_config(void);
predictive_immune_thalamic_bridge_t* predictive_immune_thalamic_bridge_create(void* predictive_immune, thalamic_router_t* router, const predictive_immune_thalamic_config_t* config);
void predictive_immune_thalamic_bridge_destroy(predictive_immune_thalamic_bridge_t* bridge);
int predictive_immune_thalamic_bridge_reset(predictive_immune_thalamic_bridge_t* bridge);
int predictive_immune_thalamic_route_interoception(predictive_immune_thalamic_bridge_t* bridge, float urgency, float salience);
int predictive_immune_thalamic_route_cytokine(predictive_immune_thalamic_bridge_t* bridge, float level, float importance);
int predictive_immune_thalamic_route_signal(predictive_immune_thalamic_bridge_t* bridge, const predictive_immune_thalamic_signal_t* signal);
int predictive_immune_thalamic_set_attention(predictive_immune_thalamic_bridge_t* bridge, float attention);
int predictive_immune_thalamic_get_attention(predictive_immune_thalamic_bridge_t* bridge, float* attention);
int predictive_immune_thalamic_bridge_get_stats(predictive_immune_thalamic_bridge_t* bridge, predictive_immune_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PREDICTIVE_IMMUNE_THALAMIC_BRIDGE_H */
