/**
 * @file nimcp_jepa_thalamic_bridge.h
 * @brief Bridge between JEPA (Joint Embedding Predictive Architecture) and thalamic router
 *
 * WHAT: Routes world model predictions through thalamic pathways
 * WHY: World model updates require cortical coordination via thalamic relay
 * HOW: Packages prediction signals, routes via thalamic attention mechanism
 *
 * BIOLOGICAL BASIS:
 * - World models involve distributed cortical prediction
 * - Thalamus coordinates cortical prediction hierarchies
 * - Higher-order thalamus relays cortical predictions
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_JEPA_THALAMIC_BRIDGE_H
#define NIMCP_JEPA_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define JEPA_SIGNAL_PREDICTION      0x0F01
#define JEPA_SIGNAL_EMBEDDING       0x0F02
#define JEPA_SIGNAL_UPDATE          0x0F03
#define JEPA_SIGNAL_ERROR           0x0F04

typedef struct {
    uint32_t signal_type;
    float prediction_confidence;
    float embedding_quality;
    uint32_t temporal_horizon;
    void* content;
    uint32_t content_size;
    uint64_t timestamp_us;
} jepa_thalamic_signal_t;

typedef struct {
    bool enable_attention_gating;
    bool enable_error_amplification;
    float min_prediction_confidence;
    uint32_t max_temporal_horizon;
    bool bio_async_enabled;
} jepa_thalamic_config_t;

typedef struct jepa_thalamic_bridge jepa_thalamic_bridge_t;

jepa_thalamic_config_t jepa_thalamic_default_config(void);
jepa_thalamic_bridge_t* jepa_thalamic_bridge_create(void* jepa, thalamic_router_t* router, const jepa_thalamic_config_t* config);
void jepa_thalamic_bridge_destroy(jepa_thalamic_bridge_t* bridge);
int jepa_thalamic_bridge_reset(jepa_thalamic_bridge_t* bridge);
int jepa_thalamic_route_prediction(jepa_thalamic_bridge_t* bridge, const jepa_thalamic_signal_t* signal);
int jepa_thalamic_route_error(jepa_thalamic_bridge_t* bridge, const void* error, float magnitude);
int jepa_thalamic_set_attention(jepa_thalamic_bridge_t* bridge, float attention);
int jepa_thalamic_get_attention(const jepa_thalamic_bridge_t* bridge, float* attention);

typedef struct {
    uint64_t predictions_routed;
    uint64_t embeddings_updated;
    uint64_t errors_propagated;
    float avg_prediction_confidence;
} jepa_thalamic_stats_t;

int jepa_thalamic_bridge_get_stats(const jepa_thalamic_bridge_t* bridge, jepa_thalamic_stats_t* stats);

/* Bio-async integration */
bool jepa_thalamic_bridge_register_bio_async(jepa_thalamic_bridge_t* bridge);
void jepa_thalamic_bridge_unregister_bio_async(jepa_thalamic_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_JEPA_THALAMIC_BRIDGE_H */
