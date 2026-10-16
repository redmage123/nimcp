/**
 * @file nimcp_snn_introspection_bridge.h
 * @brief SNN-Introspection integration bridge
 *
 * WHAT: Bidirectional bridge between SNN and introspection modules
 * WHY:  Enable temporal integration of metacognitive insights via spike patterns
 * HOW:  Convert spike patterns to metacognitive metrics, metrics to spike modulation
 *
 * BIOLOGICAL BASIS:
 * - Default Mode Network (DMN) generates spontaneous activity for self-reflection
 * - Prefrontal cortex monitors neural states through recurrent connections
 * - Phi computation in consciousness correlates with gamma oscillations
 * - Temporal patterns in neural activity reflect metacognitive awareness
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#ifndef NIMCP_SNN_INTROSPECTION_BRIDGE_H
#define NIMCP_SNN_INTROSPECTION_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "snn/nimcp_snn_types.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "snn/nimcp_snn_network.h"
#include "async/nimcp_bio_async.h"

typedef void* introspection_context_t;

typedef struct snn_introspection_config_s {
    float phi_rate_min;
    float phi_rate_max;
    float consciousness_threshold;
    float integration_time_window_ms;
    bool enable_pattern_detection;
    float pattern_match_threshold;
    uint32_t introspection_population_id;
    float update_interval_ms;
    bool enable_bio_async;
    uint32_t n_metacog_dims;
    float max_encoding_rate;
    float decoding_window_ms;
} snn_introspection_config_t;

typedef struct snn_introspection_state_s {
    float phi_estimate;
    float uncertainty_level;
    float pattern_coherence;
    bool consciousness_detected;
    uint32_t pattern_matches;
    float avg_metacog_rate;
} snn_introspection_state_t;

typedef struct snn_introspection_bridge_s {
    bridge_base_t base;                 /**< MUST be first: base bridge infrastructure */

    snn_network_t* snn;
    introspection_context_t introspection;
    snn_introspection_config_t config;
    snn_introspection_state_t state;
    snn_population_t* introspection_pop;
    snn_encoder_t* encoder;
    snn_decoder_t* decoder;
    float last_update_time;
    bool bio_async_enabled;
    bio_module_context_t bio_ctx;
    void* mutex;
} snn_introspection_bridge_t;

void snn_introspection_config_default(snn_introspection_config_t* config);

snn_introspection_bridge_t* snn_introspection_bridge_create(
    const snn_introspection_config_t* config,
    snn_network_t* snn,
    introspection_context_t introspection
);

void snn_introspection_bridge_destroy(snn_introspection_bridge_t* bridge);
int snn_introspection_bridge_connect_bio_async(snn_introspection_bridge_t* bridge);
int snn_introspection_bridge_disconnect_bio_async(snn_introspection_bridge_t* bridge);
bool snn_introspection_bridge_is_bio_async_connected(const snn_introspection_bridge_t* bridge);
int snn_introspection_bridge_update(snn_introspection_bridge_t* bridge, float dt);
float snn_introspection_estimate_phi(snn_introspection_bridge_t* bridge, float spike_rate);
int snn_introspection_detect_patterns(snn_introspection_bridge_t* bridge, const float* spike_train, uint32_t length, float* coherence);
bool snn_introspection_check_consciousness(const snn_introspection_bridge_t* bridge);
int snn_introspection_bridge_get_state(const snn_introspection_bridge_t* bridge, snn_introspection_state_t* state);
float snn_introspection_get_phi(const snn_introspection_bridge_t* bridge);
float snn_introspection_get_uncertainty(const snn_introspection_bridge_t* bridge);
int snn_introspection_get_stats(const snn_introspection_bridge_t* bridge, uint32_t* pattern_matches, uint32_t* consciousness_detections, float* avg_phi);
void snn_introspection_reset_stats(snn_introspection_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SNN_INTROSPECTION_BRIDGE_H */
