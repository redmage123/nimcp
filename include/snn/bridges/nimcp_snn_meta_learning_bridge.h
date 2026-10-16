/**
 * @file nimcp_snn_meta_learning_bridge.h
 * @brief SNN-Meta Learning integration bridge
 *
 * WHAT: Bidirectional bridge between SNN and meta-learning modules
 * WHY:  Enable temporal integration of learning-to-learn via spike patterns
 * HOW:  Convert spike patterns to adaptation metrics, metrics to spike modulation
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal Cortex (PFC) modulates learning rates across brain regions
 * - Hippocampal replay optimizes learning strategies during consolidation
 * - Dopaminergic signals adjust learning rates based on prediction errors
 * - Theta-gamma coupling coordinates multi-timescale learning adaptation
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#ifndef NIMCP_SNN_META_LEARNING_BRIDGE_H
#define NIMCP_SNN_META_LEARNING_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "snn/nimcp_snn_types.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "snn/nimcp_snn_network.h"
#include "async/nimcp_bio_async.h"

typedef void* meta_learning_context_t;

typedef struct snn_meta_learning_config_s {
    float meta_learning_rate;
    float adaptation_threshold;
    float integration_time_window_ms;
    bool enable_efficiency_tracking;
    float efficiency_decay_rate;
    uint32_t meta_learning_population_id;
    float update_interval_ms;
    bool enable_bio_async;
    uint32_t n_adaptation_dims;
    float max_encoding_rate;
    float decoding_window_ms;
} snn_meta_learning_config_t;

typedef struct snn_meta_learning_state_s {
    float adaptation_level;
    float learning_efficiency;
    uint32_t meta_updates_count;
    bool adaptation_active;
    float avg_meta_rate;
    float last_adaptation_time;
} snn_meta_learning_state_t;

typedef struct snn_meta_learning_bridge_s {
    bridge_base_t base;                 /**< MUST be first: base bridge infrastructure */

    snn_network_t* snn;
    meta_learning_context_t meta_learning;
    snn_meta_learning_config_t config;
    snn_meta_learning_state_t state;
    snn_population_t* meta_learning_pop;
    snn_encoder_t* encoder;
    snn_decoder_t* decoder;
    float last_update_time;
    bool bio_async_enabled;
    bio_module_context_t bio_ctx;
    void* mutex;
} snn_meta_learning_bridge_t;

void snn_meta_learning_config_default(snn_meta_learning_config_t* config);

snn_meta_learning_bridge_t* snn_meta_learning_bridge_create(
    const snn_meta_learning_config_t* config,
    snn_network_t* snn,
    meta_learning_context_t meta_learning
);

void snn_meta_learning_bridge_destroy(snn_meta_learning_bridge_t* bridge);
int snn_meta_learning_bridge_connect_bio_async(snn_meta_learning_bridge_t* bridge);
int snn_meta_learning_bridge_disconnect_bio_async(snn_meta_learning_bridge_t* bridge);
bool snn_meta_learning_bridge_is_bio_async_connected(const snn_meta_learning_bridge_t* bridge);
int snn_meta_learning_bridge_update(snn_meta_learning_bridge_t* bridge, float dt);
float snn_meta_learning_compute_adaptation(snn_meta_learning_bridge_t* bridge, float spike_rate);
int snn_meta_learning_update_efficiency(snn_meta_learning_bridge_t* bridge, const float* spike_train, uint32_t length, float* efficiency);
bool snn_meta_learning_check_adaptation_active(const snn_meta_learning_bridge_t* bridge);
int snn_meta_learning_bridge_get_state(const snn_meta_learning_bridge_t* bridge, snn_meta_learning_state_t* state);
float snn_meta_learning_get_adaptation_level(const snn_meta_learning_bridge_t* bridge);
float snn_meta_learning_get_efficiency(const snn_meta_learning_bridge_t* bridge);
int snn_meta_learning_get_stats(const snn_meta_learning_bridge_t* bridge, uint32_t* update_count, uint32_t* adaptation_detections, float* avg_adaptation);
void snn_meta_learning_reset_stats(snn_meta_learning_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SNN_META_LEARNING_BRIDGE_H */
