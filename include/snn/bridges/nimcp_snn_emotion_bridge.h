/**
 * @file nimcp_snn_emotion_bridge.h
 * @brief SNN-Emotion integration bridge
 *
 * WHAT: Bidirectional bridge between SNN and emotional system
 * WHY:  Enable spike-based emotional processing and affective modulation
 * HOW:  Convert emotional states to spike patterns, modulate neural activity
 *
 * BIOLOGICAL BASIS:
 * - Amygdala uses spike timing for rapid emotional detection
 * - Emotional arousal modulates cortical excitability via spike rates
 * - Valence information encoded in population activity patterns
 * - Limbic oscillations (theta 4-8 Hz) coordinate emotional processing
 *
 * INTEGRATION:
 * - SNN → Emotion: Decode spike patterns into valence/arousal states
 * - Emotion → SNN: Modulate population excitability by emotional intensity
 * - Arousal boosts baseline firing rates in attention networks
 * - Valence modulates synaptic weights
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#ifndef NIMCP_SNN_EMOTION_BRIDGE_H
#define NIMCP_SNN_EMOTION_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "snn/nimcp_snn_types.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "snn/nimcp_snn_network.h"
#include "async/nimcp_bio_async.h"

/* Forward declaration */
typedef struct emotional_system_s emotional_system_t;

//=============================================================================
// Configuration Types
//=============================================================================

typedef struct snn_emotion_config_s {
    float arousal_rate_min;          /**< Min spike rate for arousal baseline */
    float arousal_rate_max;          /**< Max spike rate for full arousal */
    float valence_threshold;         /**< Threshold for valence classification */
    float arousal_boost_factor;      /**< Arousal multiplier for firing rates */
    float valence_weight_scaling;    /**< Valence impact on synaptic weights */
    bool modulate_excitability;      /**< Allow emotion to modulate excitability */
    bool enable_theta_sync;          /**< Enable theta oscillation tracking */
    float theta_frequency;           /**< Center frequency (Hz) */
    float theta_bandwidth;           /**< Bandwidth (Hz) */
    uint32_t valence_positive_pop_id;/**< Population encoding positive valence */
    uint32_t valence_negative_pop_id;/**< Population encoding negative valence */
    uint32_t arousal_pop_id;         /**< Population encoding arousal */
    float update_interval_ms;        /**< How often to sync */
    bool enable_bio_async;           /**< Enable bio-async messaging */
} snn_emotion_config_t;

typedef struct snn_theta_state_s {
    float phase;                     /**< Current phase [0, 2π] */
    float amplitude;                 /**< Current amplitude [0, 1] */
    float frequency;                 /**< Instantaneous frequency (Hz) */
    float coherence;                 /**< Phase coherence [0, 1] */
    bool is_synchronized;            /**< Populations synchronized */
} snn_theta_state_t;

typedef struct snn_emotion_state_s {
    float decoded_valence;           /**< Decoded valence from spikes [-1, 1] */
    float decoded_arousal;           /**< Decoded arousal from spikes [0, 1] */
    float emotional_intensity;       /**< Overall intensity [0, 1] */
    snn_theta_state_t theta;         /**< Theta oscillation state */
    float arousal_modulation;        /**< Arousal boost to firing rates */
    float valence_modulation;        /**< Valence impact on weights */
    uint32_t sync_count;             /**< Number of syncs performed */
    float avg_valence;               /**< Average decoded valence */
    float avg_arousal;               /**< Average decoded arousal */
} snn_emotion_state_t;

typedef struct snn_emotion_bridge_s {
    bridge_base_t base;                 /**< MUST be first: base bridge infrastructure */

    snn_network_t* snn;
    emotional_system_t* emotion_system;
    snn_emotion_config_t config;
    snn_emotion_state_t state;
    snn_population_t* valence_pos_pop;
    snn_population_t* valence_neg_pop;
    snn_population_t* arousal_pop;
    snn_encoder_t* encoder;
    snn_decoder_t* decoder;
    float last_update_time;
    bool bio_async_enabled;
    bio_module_context_t bio_ctx;
    void* mutex;
} snn_emotion_bridge_t;

//=============================================================================
// API Functions
//=============================================================================

void snn_emotion_config_default(snn_emotion_config_t* config);

snn_emotion_bridge_t* snn_emotion_bridge_create(
    const snn_emotion_config_t* config,
    snn_network_t* snn,
    emotional_system_t* emotion_system
);

void snn_emotion_bridge_destroy(snn_emotion_bridge_t* bridge);

int snn_emotion_bridge_connect_bio_async(snn_emotion_bridge_t* bridge);
int snn_emotion_bridge_disconnect_bio_async(snn_emotion_bridge_t* bridge);
bool snn_emotion_bridge_is_bio_async_connected(const snn_emotion_bridge_t* bridge);

int snn_emotion_bridge_update(snn_emotion_bridge_t* bridge, float dt);

int snn_emotion_decode_from_spikes(
    snn_emotion_bridge_t* bridge,
    float* valence_out,
    float* arousal_out
);

int snn_emotion_detect_theta(
    snn_emotion_bridge_t* bridge,
    snn_theta_state_t* theta_state
);

int snn_emotion_modulate_populations(snn_emotion_bridge_t* bridge);

int snn_emotion_encode_to_spikes(
    snn_emotion_bridge_t* bridge,
    float valence,
    float arousal
);

int snn_emotion_bridge_get_state(
    const snn_emotion_bridge_t* bridge,
    snn_emotion_state_t* state
);

float snn_emotion_get_decoded_valence(const snn_emotion_bridge_t* bridge);
float snn_emotion_get_decoded_arousal(const snn_emotion_bridge_t* bridge);
bool snn_emotion_is_theta_synchronized(const snn_emotion_bridge_t* bridge);

int snn_emotion_get_stats(
    const snn_emotion_bridge_t* bridge,
    uint32_t* sync_count,
    float* avg_valence,
    float* avg_arousal
);

void snn_emotion_reset_stats(snn_emotion_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SNN_EMOTION_BRIDGE_H */
