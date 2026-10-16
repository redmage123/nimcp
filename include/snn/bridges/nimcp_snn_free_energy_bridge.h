/**
 * @file nimcp_snn_free_energy_bridge.h
 * @brief SNN-Free Energy integration bridge
 *
 * WHAT: Bidirectional bridge between SNN and free energy minimization
 * WHY:  Enable spike-based predictive coding and surprise minimization
 * HOW:  Encode prediction errors as spike patterns, compute free energy from population activity
 *
 * BIOLOGICAL BASIS:
 * - Free energy = thermodynamic brain principle (Friston)
 * - Prediction errors encoded in spike timing and rate
 * - Precision weighting via gain modulation of spike responses
 * - Surprise detection from unexpected spike patterns
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#ifndef NIMCP_SNN_FREE_ENERGY_BRIDGE_H
#define NIMCP_SNN_FREE_ENERGY_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "snn/nimcp_snn_types.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "snn/nimcp_snn_network.h"
#include "async/nimcp_bio_async.h"

typedef void* free_energy_system_t;

typedef struct snn_free_energy_config_s {
    float prediction_error_gain;
    float surprise_threshold;
    float precision_weighting;
    uint32_t prediction_population_id;
    uint32_t error_population_id;
    float update_interval_ms;
    bool enable_bio_async;
} snn_free_energy_config_t;

typedef struct snn_free_energy_state_s {
    float free_energy_estimate;
    float prediction_error;
    float surprise_level;
    bool is_surprised;
    uint32_t prediction_error_count;
    uint32_t surprise_events;
    float accumulated_free_energy;
} snn_free_energy_state_t;

typedef struct snn_free_energy_bridge_s {
    bridge_base_t base;                 /**< MUST be first: base bridge infrastructure */

    snn_network_t* snn;
    free_energy_system_t* free_energy_system;
    snn_free_energy_config_t config;
    snn_free_energy_state_t state;
    snn_population_t* prediction_pop;
    snn_population_t* error_pop;
    float* prediction_buffer;
    uint32_t buffer_capacity;
    float last_update_time;
    bool bio_async_enabled;
    bio_module_context_t bio_ctx;
    void* mutex;
} snn_free_energy_bridge_t;

void snn_free_energy_config_default(snn_free_energy_config_t* config);
snn_free_energy_bridge_t* snn_free_energy_bridge_create(const snn_free_energy_config_t* config, snn_network_t* snn, free_energy_system_t* free_energy_system);
void snn_free_energy_bridge_destroy(snn_free_energy_bridge_t* bridge);
int snn_free_energy_bridge_connect_bio_async(snn_free_energy_bridge_t* bridge);
int snn_free_energy_bridge_disconnect_bio_async(snn_free_energy_bridge_t* bridge);
bool snn_free_energy_bridge_is_bio_async_connected(const snn_free_energy_bridge_t* bridge);
int snn_free_energy_bridge_encode_prediction_error(snn_free_energy_bridge_t* bridge, float error);
int snn_free_energy_bridge_update_precision(snn_free_energy_bridge_t* bridge, float precision);
int snn_free_energy_bridge_update(snn_free_energy_bridge_t* bridge, float dt);
float snn_free_energy_compute_free_energy(const snn_free_energy_bridge_t* bridge, float prediction_rate, float error_rate);
int snn_free_energy_bridge_get_state(const snn_free_energy_bridge_t* bridge, snn_free_energy_state_t* state);
float snn_free_energy_get_estimate(const snn_free_energy_bridge_t* bridge);
float snn_free_energy_get_prediction_error(const snn_free_energy_bridge_t* bridge);
float snn_free_energy_get_surprise_level(const snn_free_energy_bridge_t* bridge);
bool snn_free_energy_is_surprised(const snn_free_energy_bridge_t* bridge);
int snn_free_energy_get_stats(const snn_free_energy_bridge_t* bridge, uint32_t* error_count, uint32_t* surprise_events, float* avg_free_energy);
void snn_free_energy_reset_stats(snn_free_energy_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SNN_FREE_ENERGY_BRIDGE_H */
