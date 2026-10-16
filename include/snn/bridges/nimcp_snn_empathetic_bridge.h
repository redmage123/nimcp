/**
 * @file nimcp_snn_empathetic_bridge.h
 * @brief SNN-Empathy integration bridge
 *
 * WHAT: Bidirectional bridge between SNN and empathy/mirroring system
 * WHY:  Enable spike-based social cognition and mirror neuron activity
 * HOW:  Decode observed actions into spike patterns, trigger empathic responses
 *
 * BIOLOGICAL BASIS:
 * - Mirror neurons fire during both action execution and observation
 * - Superior temporal sulcus (STS) integrates sensory input for action understanding
 * - Inferior parietal lobule (IPL) encodes motor representations
 * - Anterior cingulate cortex (ACC) processes empathic distress/pain
 *
 * INTEGRATION:
 * - SNN → Empathy: Decode observed spike patterns into action representations
 * - Empathy → SNN: Trigger mirrored spike activity in motor/premotor populations
 * - Activation threshold models attention modulation of mirroring
 * - Empathy gain scales resonance strength between self and other
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#ifndef NIMCP_SNN_EMPATHETIC_BRIDGE_H
#define NIMCP_SNN_EMPATHETIC_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "snn/nimcp_snn_types.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "snn/nimcp_snn_network.h"
#include "async/nimcp_bio_async.h"

/* Forward declaration */
typedef struct empathetic_system_s empathetic_system_t;

//=============================================================================
// Configuration Types
//=============================================================================

typedef struct snn_empathetic_config_s {
    float mirror_activation_threshold;  /**< Spike rate threshold for mirroring [Hz] */
    float empathy_gain;                  /**< Gain factor for empathic response [0, 2] */
    float resonance_decay_rate;          /**< Decay rate for mirror activation [1/s] */
    float action_observation_weight;     /**< Weight of observed action encoding */
    float self_other_discrimination;     /**< Self vs. other discrimination threshold */
    uint32_t mirror_neuron_pop_id;       /**< Population with mirror neurons */
    uint32_t sts_pop_id;                 /**< Superior temporal sulcus population */
    uint32_t ipl_pop_id;                 /**< Inferior parietal lobule population */
    uint32_t acc_pop_id;                 /**< Anterior cingulate cortex population */
    bool enable_emotional_contagion;     /**< Enable emotion mirroring */
    bool enable_perspective_taking;      /**< Enable cognitive empathy */
    float update_interval_ms;            /**< How often to update mirroring */
    bool enable_bio_async;               /**< Enable bio-async messaging */
} snn_empathetic_config_t;

typedef struct snn_empathy_response_s {
    uint32_t response_id;                /**< Unique response identifier */
    float mirror_activation;             /**< Mirror neuron activation [0, 1] */
    float emotional_resonance;           /**< Emotional contagion strength [0, 1] */
    float perspective_accuracy;          /**< Accuracy of perspective taking [0, 1] */
    float self_other_distinction;        /**< Clarity of self/other boundary [0, 1] */
    float response_time_ms;              /**< Time to generate response */
    bool action_recognized;              /**< Observed action recognized */
} snn_empathy_response_t;

typedef struct snn_empathetic_state_s {
    float mirror_activation_level;       /**< Current mirror activation [0, 1] */
    uint32_t empathy_response_count;     /**< Total empathy responses triggered */
    float avg_mirror_activation;         /**< Running average mirror activation */
    float avg_emotional_resonance;       /**< Average emotional contagion */
    float current_observed_action_rate;  /**< Spike rate of observed action [Hz] */
    float current_mirrored_action_rate;  /**< Spike rate of mirrored response [Hz] */
    float resonance_coherence;           /**< Coherence between observation and mirror [0, 1] */
    uint32_t action_recognition_count;   /**< Number of actions recognized */
    uint32_t failed_mirror_count;        /**< Failed mirroring attempts */
} snn_empathetic_state_t;

typedef struct snn_empathetic_bridge_s {
    bridge_base_t base;                 /**< MUST be first: base bridge infrastructure */

    snn_network_t* snn;
    empathetic_system_t* empathetic_system;
    snn_empathetic_config_t config;
    snn_empathetic_state_t state;
    snn_population_t* mirror_pop;
    snn_population_t* sts_pop;
    snn_population_t* ipl_pop;
    snn_population_t* acc_pop;
    snn_empathy_response_t* responses;
    uint32_t max_responses;
    uint32_t response_count;
    snn_encoder_t* encoder;
    snn_decoder_t* decoder;
    float last_update_time;
    bool bio_async_enabled;
    bio_module_context_t bio_ctx;
    void* mutex;
} snn_empathetic_bridge_t;

//=============================================================================
// API Functions
//=============================================================================

void snn_empathetic_config_default(snn_empathetic_config_t* config);

snn_empathetic_bridge_t* snn_empathetic_bridge_create(
    const snn_empathetic_config_t* config,
    snn_network_t* snn,
    empathetic_system_t* empathetic_system
);

void snn_empathetic_bridge_destroy(snn_empathetic_bridge_t* bridge);

int snn_empathetic_bridge_connect_bio_async(snn_empathetic_bridge_t* bridge);
int snn_empathetic_bridge_disconnect_bio_async(snn_empathetic_bridge_t* bridge);
bool snn_empathetic_bridge_is_bio_async_connected(const snn_empathetic_bridge_t* bridge);

int snn_empathetic_bridge_update(snn_empathetic_bridge_t* bridge, float dt);

int snn_empathetic_observe_action(
    snn_empathetic_bridge_t* bridge,
    const float* action_spikes,
    uint32_t n_spikes,
    float* observation_rate_out
);

int snn_empathetic_trigger_mirror_response(
    snn_empathetic_bridge_t* bridge,
    float observation_rate,
    snn_empathy_response_t* response_out
);

int snn_empathetic_compute_resonance(
    snn_empathetic_bridge_t* bridge,
    float observation_rate,
    float mirror_rate,
    float* coherence_out
);

int snn_empathetic_recognize_action(
    snn_empathetic_bridge_t* bridge,
    const float* spike_pattern,
    uint32_t n_spikes,
    bool* recognized_out
);

int snn_empathetic_decay_activation(
    snn_empathetic_bridge_t* bridge,
    float dt
);

int snn_empathetic_emotional_contagion(
    snn_empathetic_bridge_t* bridge,
    float observed_emotion_intensity,
    float* mirrored_emotion_out
);

int snn_empathetic_bridge_get_state(
    const snn_empathetic_bridge_t* bridge,
    snn_empathetic_state_t* state
);

float snn_empathetic_get_mirror_activation(const snn_empathetic_bridge_t* bridge);
uint32_t snn_empathetic_get_response_count(const snn_empathetic_bridge_t* bridge);
float snn_empathetic_get_resonance_coherence(const snn_empathetic_bridge_t* bridge);

int snn_empathetic_get_response(
    const snn_empathetic_bridge_t* bridge,
    uint32_t response_id,
    snn_empathy_response_t* response_out
);

int snn_empathetic_get_stats(
    const snn_empathetic_bridge_t* bridge,
    uint32_t* response_count,
    float* avg_activation,
    float* avg_resonance
);

void snn_empathetic_reset_stats(snn_empathetic_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SNN_EMPATHETIC_BRIDGE_H */
