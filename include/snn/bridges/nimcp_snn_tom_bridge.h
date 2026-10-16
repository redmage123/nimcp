/**
 * @file nimcp_snn_tom_bridge.h
 * @brief SNN-Theory of Mind integration bridge
 *
 * WHAT: Bidirectional bridge between SNN and Theory of Mind (ToM) modules
 * WHY:  Enable temporal integration of mental state attribution via spike patterns
 * HOW:  Convert spike patterns to mentalizing metrics, metrics to spike modulation
 *
 * BIOLOGICAL BASIS:
 * - Temporoparietal Junction (TPJ) specialized for perspective-taking
 * - Medial Prefrontal Cortex (mPFC) represents others' mental states
 * - Superior Temporal Sulcus (STS) processes social perception cues
 * - Beta oscillations coordinate mentalizing network synchronization
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#ifndef NIMCP_SNN_TOM_BRIDGE_H
#define NIMCP_SNN_TOM_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "snn/nimcp_snn_types.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "snn/nimcp_snn_network.h"
#include "async/nimcp_bio_async.h"

typedef void* tom_context_t;

typedef struct snn_tom_config_s {
    float mentalizing_threshold;
    float perspective_shift_rate;
    float integration_time_window_ms;
    bool enable_attribution_tracking;
    float attribution_confidence_min;
    uint32_t tom_population_id;
    float update_interval_ms;
    bool enable_bio_async;
    uint32_t n_mental_state_dims;
    float max_encoding_rate;
    float decoding_window_ms;
} snn_tom_config_t;

typedef struct snn_tom_state_s {
    float mentalizing_activity;
    float perspective_accuracy;
    uint32_t attribution_count;
    bool mentalizing_active;
    float avg_tom_rate;
    float last_perspective_shift;
} snn_tom_state_t;

typedef struct snn_tom_bridge_s {
    bridge_base_t base;                 /**< MUST be first: base bridge infrastructure */

    snn_network_t* snn;
    tom_context_t tom;
    snn_tom_config_t config;
    snn_tom_state_t state;
    snn_population_t* tom_pop;
    snn_encoder_t* encoder;
    snn_decoder_t* decoder;
    float last_update_time;
    bool bio_async_enabled;
    bio_module_context_t bio_ctx;
    void* mutex;
} snn_tom_bridge_t;

void snn_tom_config_default(snn_tom_config_t* config);

snn_tom_bridge_t* snn_tom_bridge_create(
    const snn_tom_config_t* config,
    snn_network_t* snn,
    tom_context_t tom
);

void snn_tom_bridge_destroy(snn_tom_bridge_t* bridge);
int snn_tom_bridge_connect_bio_async(snn_tom_bridge_t* bridge);
int snn_tom_bridge_disconnect_bio_async(snn_tom_bridge_t* bridge);
bool snn_tom_bridge_is_bio_async_connected(const snn_tom_bridge_t* bridge);
int snn_tom_bridge_update(snn_tom_bridge_t* bridge, float dt);
float snn_tom_compute_mentalizing(snn_tom_bridge_t* bridge, float spike_rate);
int snn_tom_update_perspective(snn_tom_bridge_t* bridge, const float* spike_train, uint32_t length, float* accuracy);
bool snn_tom_check_mentalizing_active(const snn_tom_bridge_t* bridge);
int snn_tom_bridge_get_state(const snn_tom_bridge_t* bridge, snn_tom_state_t* state);
float snn_tom_get_mentalizing_activity(const snn_tom_bridge_t* bridge);
float snn_tom_get_perspective_accuracy(const snn_tom_bridge_t* bridge);
int snn_tom_get_stats(const snn_tom_bridge_t* bridge, uint32_t* attribution_count, uint32_t* mentalizing_detections, float* avg_activity);
void snn_tom_reset_stats(snn_tom_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SNN_TOM_BRIDGE_H */
