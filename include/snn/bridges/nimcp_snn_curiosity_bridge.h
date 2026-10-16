/**
 * @file nimcp_snn_curiosity_bridge.h
 * @brief SNN-Curiosity integration bridge
 *
 * WHAT: Bidirectional bridge between SNN and curiosity/exploration drive
 * WHY:  Enable spike-based novelty detection and exploration behavior
 * HOW:  Encode novelty as spike patterns, habituation via rate adaptation
 *
 * BIOLOGICAL BASIS:
 * - Curiosity driven by dopaminergic prediction error (VTA/SNc)
 * - Novelty detection via hippocampal mismatch signals
 * - Habituation through synaptic depression and adaptation
 * - Exploration drive modulated by noradrenergic arousal (LC)
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#ifndef NIMCP_SNN_CURIOSITY_BRIDGE_H
#define NIMCP_SNN_CURIOSITY_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "snn/nimcp_snn_types.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "snn/nimcp_snn_network.h"
#include "async/nimcp_bio_async.h"

typedef void* curiosity_system_t;

typedef struct snn_curiosity_config_s {
    float novelty_threshold;
    float exploration_drive_gain;
    float habituation_rate;
    uint32_t novelty_population_id;
    uint32_t exploration_population_id;
    float update_interval_ms;
    bool enable_bio_async;
} snn_curiosity_config_t;

typedef struct snn_curiosity_state_s {
    float curiosity_level;
    float novelty_response;
    uint32_t exploration_count;
    bool is_exploring;
    bool is_novel;
    uint32_t novelty_events;
    float accumulated_novelty;
} snn_curiosity_state_t;

typedef struct snn_curiosity_bridge_s {
    bridge_base_t base;                 /**< MUST be first: base bridge infrastructure */

    snn_network_t* snn;
    curiosity_system_t* curiosity_system;
    snn_curiosity_config_t config;
    snn_curiosity_state_t state;
    snn_population_t* novelty_pop;
    snn_population_t* exploration_pop;
    float* novelty_buffer;
    uint32_t buffer_capacity;
    float habituation_accumulator;
    float last_update_time;
    bool bio_async_enabled;
    bio_module_context_t bio_ctx;
    void* mutex;
} snn_curiosity_bridge_t;

void snn_curiosity_config_default(snn_curiosity_config_t* config);
snn_curiosity_bridge_t* snn_curiosity_bridge_create(const snn_curiosity_config_t* config, snn_network_t* snn, curiosity_system_t* curiosity_system);
void snn_curiosity_bridge_destroy(snn_curiosity_bridge_t* bridge);
int snn_curiosity_bridge_connect_bio_async(snn_curiosity_bridge_t* bridge);
int snn_curiosity_bridge_disconnect_bio_async(snn_curiosity_bridge_t* bridge);
bool snn_curiosity_bridge_is_bio_async_connected(const snn_curiosity_bridge_t* bridge);
int snn_curiosity_bridge_encode_novelty(snn_curiosity_bridge_t* bridge, float novelty);
int snn_curiosity_bridge_trigger_exploration(snn_curiosity_bridge_t* bridge);
int snn_curiosity_bridge_apply_habituation(snn_curiosity_bridge_t* bridge, float dt);
int snn_curiosity_bridge_update(snn_curiosity_bridge_t* bridge, float dt);
float snn_curiosity_compute_curiosity_level(const snn_curiosity_bridge_t* bridge, float novelty_rate, float habituation);
int snn_curiosity_bridge_get_state(const snn_curiosity_bridge_t* bridge, snn_curiosity_state_t* state);
float snn_curiosity_get_level(const snn_curiosity_bridge_t* bridge);
float snn_curiosity_get_novelty_response(const snn_curiosity_bridge_t* bridge);
uint32_t snn_curiosity_get_exploration_count(const snn_curiosity_bridge_t* bridge);
bool snn_curiosity_is_exploring(const snn_curiosity_bridge_t* bridge);
bool snn_curiosity_is_novel(const snn_curiosity_bridge_t* bridge);
int snn_curiosity_get_stats(const snn_curiosity_bridge_t* bridge, uint32_t* novelty_events, uint32_t* exploration_count, float* avg_novelty);
void snn_curiosity_reset_stats(snn_curiosity_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SNN_CURIOSITY_BRIDGE_H */
