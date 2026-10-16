/**
 * @file nimcp_snn_shadow_bridge.h
 * @brief SNN-Shadow integration bridge
 *
 * WHAT: Bidirectional bridge between SNN and shadow/unconscious processing
 * WHY:  Enable spike-based unconscious processing and background pattern detection
 * HOW:  Convert shadow states to low-frequency patterns, track integration events
 *
 * BIOLOGICAL BASIS:
 * - Default mode network (DMN) operates at low frequencies during rest
 * - Unconscious processing involves slow cortical potentials (< 1 Hz)
 * - Shadow integration linked to slow-wave sleep delta oscillations (0.5-4 Hz)
 * - Subliminal processing uses sustained background activity patterns
 *
 * INTEGRATION:
 * - SNN → Shadow: Decode low-frequency background patterns into shadow activity
 * - Shadow → SNN: Maintain sustained low-amplitude background activity
 * - Integration events detected via coherence increases
 * - Shadow processing modulates resting-state network dynamics
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#ifndef NIMCP_SNN_SHADOW_BRIDGE_H
#define NIMCP_SNN_SHADOW_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "snn/nimcp_snn_types.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "snn/nimcp_snn_network.h"
#include "async/nimcp_bio_async.h"

/* Forward declaration */
typedef struct shadow_system_s shadow_system_t;

//=============================================================================
// Configuration Types
//=============================================================================

typedef struct snn_shadow_config_s {
    float shadow_activation_threshold; /**< Threshold for shadow activation */
    float integration_rate;            /**< Rate of shadow-conscious integration */
    float background_frequency_min;    /**< Min background frequency (Hz) */
    float background_frequency_max;    /**< Max background frequency (Hz) */
    float background_amplitude_max;    /**< Max background amplitude [0, 1] */
    float resting_state_baseline;      /**< Baseline resting state activity */
    float coherence_threshold;         /**< Threshold for integration events */
    bool enable_integration_tracking;  /**< Track integration events */
    bool enable_dmn_simulation;        /**< Simulate default mode network */
    uint32_t shadow_pop_id;            /**< Population encoding shadow activity */
    uint32_t dmn_pop_id;               /**< Population encoding DMN activity */
    float update_interval_ms;          /**< How often to sync */
    bool enable_bio_async;             /**< Enable bio-async messaging */
} snn_shadow_config_t;

typedef struct snn_background_pattern_state_s {
    float frequency;                   /**< Current background frequency (Hz) */
    float amplitude;                   /**< Current amplitude [0, 1] */
    float phase;                       /**< Current phase [0, 2π] */
    float coherence;                   /**< Background coherence [0, 1] */
    bool is_active;                    /**< Background pattern active */
} snn_background_pattern_state_t;

typedef struct snn_shadow_state_s {
    float shadow_activity_level;       /**< Current shadow activity [0, 1] */
    uint32_t integration_events;       /**< Total integration events */
    snn_background_pattern_state_t background; /**< Background pattern state */
    float dmn_activity;                /**< Default mode network activity [0, 1] */
    float integration_progress;        /**< Current integration progress [0, 1] */
    float resting_state_level;         /**< Resting state activity level */
    float shadow_conscious_coupling;   /**< Shadow-conscious coupling strength */
    uint32_t sync_count;               /**< Number of syncs performed */
    float avg_shadow_activity;         /**< Average shadow activity */
    float avg_dmn_activity;            /**< Average DMN activity */
} snn_shadow_state_t;

typedef struct snn_shadow_bridge_s {
    bridge_base_t base;                 /**< MUST be first: base bridge infrastructure */

    snn_network_t* snn;
    shadow_system_t* shadow_system;
    snn_shadow_config_t config;
    snn_shadow_state_t state;
    snn_population_t* shadow_pop;
    snn_population_t* dmn_pop;
    snn_encoder_t* encoder;
    snn_decoder_t* decoder;
    float last_update_time;
    bool bio_async_enabled;
    bio_module_context_t bio_ctx;
    void* mutex;
} snn_shadow_bridge_t;

//=============================================================================
// API Functions
//=============================================================================

void snn_shadow_config_default(snn_shadow_config_t* config);

snn_shadow_bridge_t* snn_shadow_bridge_create(
    const snn_shadow_config_t* config,
    snn_network_t* snn,
    shadow_system_t* shadow_system
);

void snn_shadow_bridge_destroy(snn_shadow_bridge_t* bridge);

int snn_shadow_bridge_connect_bio_async(snn_shadow_bridge_t* bridge);
int snn_shadow_bridge_disconnect_bio_async(snn_shadow_bridge_t* bridge);
bool snn_shadow_bridge_is_bio_async_connected(const snn_shadow_bridge_t* bridge);

int snn_shadow_bridge_update(snn_shadow_bridge_t* bridge, float dt);

int snn_shadow_decode_from_spikes(
    snn_shadow_bridge_t* bridge,
    float* shadow_activity_out,
    float* dmn_activity_out
);

int snn_shadow_detect_background_pattern(
    snn_shadow_bridge_t* bridge,
    snn_background_pattern_state_t* background_state
);

int snn_shadow_detect_integration_event(
    snn_shadow_bridge_t* bridge,
    bool* integration_detected
);

int snn_shadow_modulate_populations(snn_shadow_bridge_t* bridge);

int snn_shadow_encode_to_spikes(
    snn_shadow_bridge_t* bridge,
    float shadow_activity,
    float integration_progress
);

int snn_shadow_bridge_get_state(
    const snn_shadow_bridge_t* bridge,
    snn_shadow_state_t* state
);

float snn_shadow_get_activity_level(const snn_shadow_bridge_t* bridge);
float snn_shadow_get_dmn_activity(const snn_shadow_bridge_t* bridge);
bool snn_shadow_is_background_active(const snn_shadow_bridge_t* bridge);

int snn_shadow_get_stats(
    const snn_shadow_bridge_t* bridge,
    uint32_t* sync_count,
    uint32_t* integration_events,
    float* avg_shadow_activity
);

void snn_shadow_reset_stats(snn_shadow_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SNN_SHADOW_BRIDGE_H */
