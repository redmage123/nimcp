/**
 * @file nimcp_snn_joy_bridge.h
 * @brief SNN-Joy integration bridge
 *
 * WHAT: Bidirectional bridge between SNN and joy/reward processing system
 * WHY:  Enable spike-based reward detection and dopaminergic-like burst patterns
 * HOW:  Convert joy states to burst patterns, track reward prediction errors
 *
 * BIOLOGICAL BASIS:
 * - Dopamine neurons exhibit phasic burst firing for unexpected rewards
 * - Reward prediction errors encoded in burst magnitude and timing
 * - Ventral tegmental area (VTA) uses spike bursts for reward signaling
 * - Nucleus accumbens processes reward via synchronized population bursts
 *
 * INTEGRATION:
 * - SNN → Joy: Decode burst patterns into reward/joy magnitude
 * - Joy → SNN: Generate dopaminergic-like bursts for positive prediction errors
 * - Burst count tracks cumulative reward events
 * - RPE modulates burst amplitude and frequency
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#ifndef NIMCP_SNN_JOY_BRIDGE_H
#define NIMCP_SNN_JOY_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "snn/nimcp_snn_types.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "snn/nimcp_snn_network.h"
#include "async/nimcp_bio_async.h"

/* Forward declaration */
typedef struct joy_system_s joy_system_t;

//=============================================================================
// Configuration Types
//=============================================================================

typedef struct snn_joy_config_s {
    float joy_burst_threshold;         /**< Threshold for burst detection */
    float reward_prediction_gain;      /**< Gain for RPE computation */
    float burst_frequency_min;         /**< Min burst frequency (Hz) */
    float burst_frequency_max;         /**< Max burst frequency (Hz) */
    float burst_duration_ms;           /**< Duration of a single burst (ms) */
    float dopamine_baseline;           /**< Baseline dopamine-like activity */
    float dopamine_burst_peak;         /**< Peak dopamine during burst */
    bool enable_rpe_tracking;          /**< Track reward prediction errors */
    bool enable_burst_counting;        /**< Count burst events */
    uint32_t joy_pop_id;               /**< Population encoding joy/reward */
    uint32_t reward_pop_id;            /**< Population encoding reward signal */
    float update_interval_ms;          /**< How often to sync */
    bool enable_bio_async;             /**< Enable bio-async messaging */
} snn_joy_config_t;

typedef struct snn_burst_state_s {
    float frequency;                   /**< Current burst frequency (Hz) */
    float amplitude;                   /**< Current burst amplitude [0, 1] */
    float duration_ms;                 /**< Duration of current burst (ms) */
    uint32_t spike_count;              /**< Spikes in current burst */
    bool is_bursting;                  /**< Currently in burst state */
} snn_burst_state_t;

typedef struct snn_joy_state_s {
    float joy_level;                   /**< Current joy level [0, 1] */
    uint32_t burst_count;              /**< Total burst events detected */
    float reward_prediction_error;     /**< Current RPE [-1, 1] */
    snn_burst_state_t burst;           /**< Burst pattern state */
    float dopamine_level;              /**< Current dopamine-like signal [0, 1] */
    float predicted_reward;            /**< Predicted reward value */
    float actual_reward;               /**< Actual reward received */
    uint32_t sync_count;               /**< Number of syncs performed */
    float avg_joy_level;               /**< Average joy level */
    float avg_burst_frequency;         /**< Average burst frequency */
} snn_joy_state_t;

typedef struct snn_joy_bridge_s {
    bridge_base_t base;                 /**< MUST be first: base bridge infrastructure */

    snn_network_t* snn;
    joy_system_t* joy_system;
    snn_joy_config_t config;
    snn_joy_state_t state;
    snn_population_t* joy_pop;
    snn_population_t* reward_pop;
    snn_encoder_t* encoder;
    snn_decoder_t* decoder;
    float last_update_time;
    bool bio_async_enabled;
    bio_module_context_t bio_ctx;
    void* mutex;
} snn_joy_bridge_t;

//=============================================================================
// API Functions
//=============================================================================

void snn_joy_config_default(snn_joy_config_t* config);

snn_joy_bridge_t* snn_joy_bridge_create(
    const snn_joy_config_t* config,
    snn_network_t* snn,
    joy_system_t* joy_system
);

void snn_joy_bridge_destroy(snn_joy_bridge_t* bridge);

int snn_joy_bridge_connect_bio_async(snn_joy_bridge_t* bridge);
int snn_joy_bridge_disconnect_bio_async(snn_joy_bridge_t* bridge);
bool snn_joy_bridge_is_bio_async_connected(const snn_joy_bridge_t* bridge);

int snn_joy_bridge_update(snn_joy_bridge_t* bridge, float dt);

int snn_joy_decode_from_spikes(
    snn_joy_bridge_t* bridge,
    float* joy_level_out,
    float* rpe_out
);

int snn_joy_detect_burst(
    snn_joy_bridge_t* bridge,
    snn_burst_state_t* burst_state
);

int snn_joy_compute_rpe(
    snn_joy_bridge_t* bridge,
    float predicted_reward,
    float actual_reward,
    float* rpe_out
);

int snn_joy_modulate_populations(snn_joy_bridge_t* bridge);

int snn_joy_encode_to_spikes(
    snn_joy_bridge_t* bridge,
    float joy_level,
    float rpe
);

int snn_joy_bridge_get_state(
    const snn_joy_bridge_t* bridge,
    snn_joy_state_t* state
);

float snn_joy_get_level(const snn_joy_bridge_t* bridge);
float snn_joy_get_rpe(const snn_joy_bridge_t* bridge);
bool snn_joy_is_bursting(const snn_joy_bridge_t* bridge);

int snn_joy_get_stats(
    const snn_joy_bridge_t* bridge,
    uint32_t* sync_count,
    uint32_t* burst_count,
    float* avg_joy_level
);

void snn_joy_reset_stats(snn_joy_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SNN_JOY_BRIDGE_H */
