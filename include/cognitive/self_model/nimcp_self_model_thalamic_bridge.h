/**
 * @file nimcp_self_model_thalamic_bridge.h
 * @brief Bridge between Self-Model system and thalamic router
 *
 * WHAT: Routes self-model signals through thalamic attention pathways
 * WHY: Self-modeling requires conscious attention for integration
 * HOW: Packages self-model signals, routes via MD/anterior thalamic pathways
 *
 * BIOLOGICAL BASIS:
 * - Self-referential processing involves medial PFC-thalamic circuits
 * - MD nucleus supports self-model maintenance
 * - Anterior thalamus links self-model to autobiographical memory
 * - Intralaminar nuclei support self-awareness
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_SELF_MODEL_THALAMIC_BRIDGE_H
#define NIMCP_SELF_MODEL_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SELF_MODEL_SIGNAL_UPDATE      0x3301  /**< Model update */
#define SELF_MODEL_SIGNAL_PREDICTION  0x3302  /**< Self-prediction */
#define SELF_MODEL_SIGNAL_CONFLICT    0x3303  /**< Model conflict */
#define SELF_MODEL_SIGNAL_INTEGRATION 0x3304  /**< Integration event */

typedef struct {
    uint32_t signal_type;
    float model_urgency;
    float coherence;
    float prediction_error;
    float self_relevance;
    void* content;
    uint32_t content_size;
    uint64_t timestamp_us;
} self_model_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_conflict_priority;
    float min_urgency_threshold;
    float conflict_boost;
} self_model_thalamic_config_t;

typedef struct {
    uint64_t updates_routed;
    uint64_t predictions;
    uint64_t conflicts_detected;
    uint64_t integrations;
    uint64_t signals_gated;
    float avg_coherence;
    float avg_prediction_error;
} self_model_thalamic_stats_t;

typedef struct self_model_thalamic_bridge self_model_thalamic_bridge_t;

self_model_thalamic_config_t self_model_thalamic_default_config(void);
self_model_thalamic_bridge_t* self_model_thalamic_bridge_create(
    void* self_model, thalamic_router_t* router,
    const self_model_thalamic_config_t* config);
void self_model_thalamic_bridge_destroy(self_model_thalamic_bridge_t* bridge);
int self_model_thalamic_bridge_reset(self_model_thalamic_bridge_t* bridge);

int self_model_thalamic_route_signal(
    self_model_thalamic_bridge_t* bridge,
    const self_model_thalamic_signal_t* signal);
int self_model_thalamic_route_update(
    self_model_thalamic_bridge_t* bridge,
    float coherence, float urgency);
int self_model_thalamic_route_conflict(
    self_model_thalamic_bridge_t* bridge,
    float prediction_error, float urgency);

int self_model_thalamic_set_attention(self_model_thalamic_bridge_t* bridge, float attention);
int self_model_thalamic_get_attention(const self_model_thalamic_bridge_t* bridge, float* attention);
int self_model_thalamic_bridge_get_stats(
    const self_model_thalamic_bridge_t* bridge,
    self_model_thalamic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SELF_MODEL_THALAMIC_BRIDGE_H */
