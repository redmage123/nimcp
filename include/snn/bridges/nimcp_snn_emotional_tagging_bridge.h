/**
 * @file nimcp_snn_emotional_tagging_bridge.h
 * @brief SNN-Emotional Tagging integration bridge
 *
 * WHAT: Bidirectional bridge between SNN and emotional tagging system
 * WHY:  Enable spike-based emotional memory tagging and consolidation
 * HOW:  Tag events with emotional significance via spike pattern intensity
 *
 * BIOLOGICAL BASIS:
 * - Amygdala tags emotionally salient events for enhanced consolidation
 * - Norepinephrine/cortisol release modulates hippocampal encoding
 * - Emotional arousal increases synaptic tagging probability
 * - Tagged memories show enhanced long-term potentiation (LTP)
 *
 * INTEGRATION:
 * - SNN → Tagging: High spike bursts trigger emotional tags
 * - Tagging → SNN: Tagged events boost synaptic weights
 * - Tag intensity proportional to spike synchrony and burst rate
 * - Emotional decay simulates memory consolidation over time
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#ifndef NIMCP_SNN_EMOTIONAL_TAGGING_BRIDGE_H
#define NIMCP_SNN_EMOTIONAL_TAGGING_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "snn/nimcp_snn_types.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "snn/nimcp_snn_network.h"
#include "async/nimcp_bio_async.h"

/* Forward declaration */
typedef struct emotional_tagging_system_s emotional_tagging_system_t;

//=============================================================================
// Configuration Types
//=============================================================================

typedef struct snn_emotional_tagging_config_s {
    float tagging_threshold;         /**< Spike burst threshold for tagging [Hz] */
    float emotional_decay_rate;      /**< Tag intensity decay rate [1/s] */
    float max_tag_intensity;         /**< Maximum tag intensity [0, 1] */
    float synchrony_weight;          /**< Weight of spike synchrony in tagging */
    float burst_weight;              /**< Weight of burst rate in tagging */
    float ltp_enhancement_factor;    /**< LTP boost for tagged events */
    uint32_t tagging_pop_id;         /**< Population responsible for tagging */
    uint32_t memory_pop_id;          /**< Population encoding memories */
    float consolidation_window_ms;   /**< Time window for consolidation */
    bool enable_synaptic_tagging;    /**< Enable synaptic tag marking */
    float update_interval_ms;        /**< How often to update tags */
    bool enable_bio_async;           /**< Enable bio-async messaging */
} snn_emotional_tagging_config_t;

typedef struct snn_emotional_tag_s {
    uint32_t event_id;               /**< Event/memory identifier */
    float intensity;                 /**< Current tag intensity [0, 1] */
    float creation_time;             /**< Time when tag was created (ms) */
    float last_update_time;          /**< Last decay update time (ms) */
    bool consolidated;               /**< Tag has been consolidated */
    float consolidation_strength;    /**< Strength of consolidation [0, 1] */
} snn_emotional_tag_t;

typedef struct snn_emotional_tagging_state_s {
    float current_tag_intensity;     /**< Current tagging activity [0, 1] */
    uint32_t tagged_events_count;    /**< Total number of tagged events */
    float avg_tag_intensity;         /**< Running average tag intensity */
    float burst_rate;                /**< Current burst rate [Hz] */
    float spike_synchrony;           /**< Current spike synchrony [0, 1] */
    float consolidation_rate;        /**< Rate of memory consolidation [events/s] */
    uint32_t active_tags;            /**< Number of active (non-decayed) tags */
    uint32_t consolidated_tags;      /**< Number of consolidated tags */
} snn_emotional_tagging_state_t;

typedef struct snn_emotional_tagging_bridge_s {
    bridge_base_t base;                 /**< MUST be first: base bridge infrastructure */

    snn_network_t* snn;
    emotional_tagging_system_t* tagging_system;
    snn_emotional_tagging_config_t config;
    snn_emotional_tagging_state_t state;
    snn_population_t* tagging_pop;
    snn_population_t* memory_pop;
    snn_emotional_tag_t* tags;
    uint32_t max_tags;
    uint32_t tag_count;
    snn_encoder_t* encoder;
    snn_decoder_t* decoder;
    float last_update_time;
    bool bio_async_enabled;
    bio_module_context_t bio_ctx;
    void* mutex;
} snn_emotional_tagging_bridge_t;

//=============================================================================
// API Functions
//=============================================================================

void snn_emotional_tagging_config_default(snn_emotional_tagging_config_t* config);

snn_emotional_tagging_bridge_t* snn_emotional_tagging_bridge_create(
    const snn_emotional_tagging_config_t* config,
    snn_network_t* snn,
    emotional_tagging_system_t* tagging_system
);

void snn_emotional_tagging_bridge_destroy(snn_emotional_tagging_bridge_t* bridge);

int snn_emotional_tagging_bridge_connect_bio_async(snn_emotional_tagging_bridge_t* bridge);
int snn_emotional_tagging_bridge_disconnect_bio_async(snn_emotional_tagging_bridge_t* bridge);
bool snn_emotional_tagging_bridge_is_bio_async_connected(const snn_emotional_tagging_bridge_t* bridge);

int snn_emotional_tagging_bridge_update(snn_emotional_tagging_bridge_t* bridge, float dt);

int snn_emotional_tagging_detect_burst(
    snn_emotional_tagging_bridge_t* bridge,
    float* burst_rate_out,
    float* synchrony_out
);

int snn_emotional_tagging_create_tag(
    snn_emotional_tagging_bridge_t* bridge,
    uint32_t event_id,
    float initial_intensity
);

int snn_emotional_tagging_decay_tags(
    snn_emotional_tagging_bridge_t* bridge,
    float dt
);

int snn_emotional_tagging_consolidate_memory(
    snn_emotional_tagging_bridge_t* bridge,
    uint32_t event_id
);

int snn_emotional_tagging_enhance_ltp(
    snn_emotional_tagging_bridge_t* bridge,
    uint32_t event_id,
    float* enhancement_factor
);

int snn_emotional_tagging_bridge_get_state(
    const snn_emotional_tagging_bridge_t* bridge,
    snn_emotional_tagging_state_t* state
);

float snn_emotional_tagging_get_current_intensity(const snn_emotional_tagging_bridge_t* bridge);
uint32_t snn_emotional_tagging_get_tagged_count(const snn_emotional_tagging_bridge_t* bridge);
uint32_t snn_emotional_tagging_get_active_tags(const snn_emotional_tagging_bridge_t* bridge);

int snn_emotional_tagging_get_tag(
    const snn_emotional_tagging_bridge_t* bridge,
    uint32_t event_id,
    snn_emotional_tag_t* tag_out
);

int snn_emotional_tagging_get_stats(
    const snn_emotional_tagging_bridge_t* bridge,
    uint32_t* tagged_count,
    float* avg_intensity,
    uint32_t* consolidated_count
);

void snn_emotional_tagging_reset_stats(snn_emotional_tagging_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SNN_EMOTIONAL_TAGGING_BRIDGE_H */
