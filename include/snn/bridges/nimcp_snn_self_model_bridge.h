/**
 * @file nimcp_snn_self_model_bridge.h
 * @brief SNN-Self Model integration bridge
 *
 * WHAT: Bidirectional bridge between SNN and self-model/self-concept modules
 * WHY:  Enable temporal integration of self-referential processing via spike patterns
 * HOW:  Convert spike patterns to self-representation metrics, metrics to spike modulation
 *
 * BIOLOGICAL BASIS:
 * - Medial Prefrontal Cortex (mPFC) maintains self-referential processing
 * - Posterior Cingulate Cortex (PCC) integrates self-knowledge across time
 * - Default Mode Network (DMN) shows heightened activity during self-reflection
 * - Gamma oscillations correlate with coherent self-concept representations
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#ifndef NIMCP_SNN_SELF_MODEL_BRIDGE_H
#define NIMCP_SNN_SELF_MODEL_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "snn/nimcp_snn_types.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "snn/nimcp_snn_network.h"
#include "async/nimcp_bio_async.h"

typedef void* self_model_context_t;

typedef struct snn_self_model_config_s {
    float self_reference_threshold;
    float identity_stability_gain;
    float integration_time_window_ms;
    bool enable_coherence_tracking;
    float coherence_decay_rate;
    uint32_t self_model_population_id;
    float update_interval_ms;
    bool enable_bio_async;
    uint32_t n_self_dims;
    float max_encoding_rate;
    float decoding_window_ms;
} snn_self_model_config_t;

typedef struct snn_self_model_state_s {
    float self_coherence;
    float identity_stability;
    uint32_t self_reference_count;
    bool coherent_self_detected;
    float avg_self_rate;
    float last_coherence_time;
} snn_self_model_state_t;

typedef struct snn_self_model_bridge_s {
    bridge_base_t base;                 /**< MUST be first: base bridge infrastructure */

    snn_network_t* snn;
    self_model_context_t self_model;
    snn_self_model_config_t config;
    snn_self_model_state_t state;
    snn_population_t* self_model_pop;
    snn_encoder_t* encoder;
    snn_decoder_t* decoder;
    float last_update_time;
    bool bio_async_enabled;
    bio_module_context_t bio_ctx;
    void* mutex;
} snn_self_model_bridge_t;

void snn_self_model_config_default(snn_self_model_config_t* config);

snn_self_model_bridge_t* snn_self_model_bridge_create(
    const snn_self_model_config_t* config,
    snn_network_t* snn,
    self_model_context_t self_model
);

void snn_self_model_bridge_destroy(snn_self_model_bridge_t* bridge);
int snn_self_model_bridge_connect_bio_async(snn_self_model_bridge_t* bridge);
int snn_self_model_bridge_disconnect_bio_async(snn_self_model_bridge_t* bridge);
bool snn_self_model_bridge_is_bio_async_connected(const snn_self_model_bridge_t* bridge);
int snn_self_model_bridge_update(snn_self_model_bridge_t* bridge, float dt);
float snn_self_model_compute_coherence(snn_self_model_bridge_t* bridge, float spike_rate);
int snn_self_model_update_stability(snn_self_model_bridge_t* bridge, const float* spike_train, uint32_t length, float* stability);
bool snn_self_model_check_coherent_self(const snn_self_model_bridge_t* bridge);
int snn_self_model_bridge_get_state(const snn_self_model_bridge_t* bridge, snn_self_model_state_t* state);
float snn_self_model_get_coherence(const snn_self_model_bridge_t* bridge);
float snn_self_model_get_stability(const snn_self_model_bridge_t* bridge);
int snn_self_model_get_stats(const snn_self_model_bridge_t* bridge, uint32_t* reference_count, uint32_t* coherent_detections, float* avg_coherence);
void snn_self_model_reset_stats(snn_self_model_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SNN_SELF_MODEL_BRIDGE_H */
