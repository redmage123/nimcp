/**
 * @file nimcp_snn_grief_bridge.h
 * @brief SNN-Grief integration bridge
 *
 * WHAT: Bidirectional bridge between SNN and grief processing system
 * WHY:  Enable spike-based grief processing and bereavement-related neural patterns
 * HOW:  Convert grief states to slow oscillation patterns, track recovery dynamics
 *
 * BIOLOGICAL BASIS:
 * - Grief involves slow-wave activity in anterior cingulate cortex (ACC)
 * - Bereavement shows reduced delta/theta power during rumination
 * - Recovery associated with gradual normalization of oscillatory patterns
 * - Prolonged grief linked to sustained low-frequency oscillations (0.5-2 Hz)
 *
 * INTEGRATION:
 * - SNN → Grief: Decode slow oscillation patterns into grief intensity
 * - Grief → SNN: Modulate population activity with grief-related suppression
 * - Rumination suppression reduces repetitive spike patterns
 * - Recovery progress tracked via oscillation normalization
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#ifndef NIMCP_SNN_GRIEF_BRIDGE_H
#define NIMCP_SNN_GRIEF_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "snn/nimcp_snn_types.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "snn/nimcp_snn_network.h"
#include "async/nimcp_bio_async.h"

/* Forward declaration */
typedef struct grief_system_s grief_system_t;

//=============================================================================
// Configuration Types
//=============================================================================

typedef struct snn_grief_config_s {
    float grief_intensity_threshold;  /**< Threshold for grief state detection */
    float recovery_rate;               /**< Rate of grief recovery [0, 1] */
    float rumination_suppression;      /**< Suppression factor for rumination */
    float slow_wave_min_freq;          /**< Min frequency for slow waves (Hz) */
    float slow_wave_max_freq;          /**< Max frequency for slow waves (Hz) */
    float baseline_activity_level;     /**< Baseline activity during grief */
    bool enable_recovery_tracking;     /**< Track recovery progress */
    bool enable_rumination_detection;  /**< Detect rumination patterns */
    uint32_t grief_pop_id;             /**< Population encoding grief state */
    uint32_t recovery_pop_id;          /**< Population encoding recovery */
    float update_interval_ms;          /**< How often to sync */
    bool enable_bio_async;             /**< Enable bio-async messaging */
} snn_grief_config_t;

typedef struct snn_slow_wave_state_s {
    float frequency;                   /**< Current slow wave frequency (Hz) */
    float amplitude;                   /**< Current amplitude [0, 1] */
    float phase;                       /**< Current phase [0, 2π] */
    float power;                       /**< Spectral power in slow band */
    bool is_active;                    /**< Slow wave detected */
} snn_slow_wave_state_t;

typedef struct snn_grief_state_s {
    float grief_intensity;             /**< Current grief intensity [0, 1] */
    uint64_t grief_duration_ms;        /**< Duration of grief state (ms) */
    float recovery_progress;           /**< Recovery progress [0, 1] */
    snn_slow_wave_state_t slow_wave;   /**< Slow wave oscillation state */
    float rumination_level;            /**< Detected rumination level [0, 1] */
    float activity_suppression;        /**< Activity suppression factor */
    uint32_t sync_count;               /**< Number of syncs performed */
    float avg_grief_intensity;         /**< Average grief intensity */
    float avg_recovery_progress;       /**< Average recovery progress */
} snn_grief_state_t;

typedef struct snn_grief_bridge_s {
    bridge_base_t base;                 /**< MUST be first: base bridge infrastructure */

    snn_network_t* snn;
    grief_system_t* grief_system;
    snn_grief_config_t config;
    snn_grief_state_t state;
    snn_population_t* grief_pop;
    snn_population_t* recovery_pop;
    snn_encoder_t* encoder;
    snn_decoder_t* decoder;
    float last_update_time;
    bool bio_async_enabled;
    bio_module_context_t bio_ctx;
    void* mutex;
} snn_grief_bridge_t;

//=============================================================================
// API Functions
//=============================================================================

void snn_grief_config_default(snn_grief_config_t* config);

snn_grief_bridge_t* snn_grief_bridge_create(
    const snn_grief_config_t* config,
    snn_network_t* snn,
    grief_system_t* grief_system
);

void snn_grief_bridge_destroy(snn_grief_bridge_t* bridge);

int snn_grief_bridge_connect_bio_async(snn_grief_bridge_t* bridge);
int snn_grief_bridge_disconnect_bio_async(snn_grief_bridge_t* bridge);
bool snn_grief_bridge_is_bio_async_connected(const snn_grief_bridge_t* bridge);

int snn_grief_bridge_update(snn_grief_bridge_t* bridge, float dt);

int snn_grief_decode_from_spikes(
    snn_grief_bridge_t* bridge,
    float* grief_intensity_out,
    float* recovery_progress_out
);

int snn_grief_detect_slow_wave(
    snn_grief_bridge_t* bridge,
    snn_slow_wave_state_t* slow_wave_state
);

int snn_grief_detect_rumination(
    snn_grief_bridge_t* bridge,
    float* rumination_level
);

int snn_grief_modulate_populations(snn_grief_bridge_t* bridge);

int snn_grief_encode_to_spikes(
    snn_grief_bridge_t* bridge,
    float grief_intensity,
    float recovery_progress
);

int snn_grief_bridge_get_state(
    const snn_grief_bridge_t* bridge,
    snn_grief_state_t* state
);

float snn_grief_get_intensity(const snn_grief_bridge_t* bridge);
float snn_grief_get_recovery_progress(const snn_grief_bridge_t* bridge);
bool snn_grief_is_slow_wave_active(const snn_grief_bridge_t* bridge);

int snn_grief_get_stats(
    const snn_grief_bridge_t* bridge,
    uint32_t* sync_count,
    float* avg_grief_intensity,
    float* avg_recovery_progress
);

void snn_grief_reset_stats(snn_grief_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SNN_GRIEF_BRIDGE_H */
