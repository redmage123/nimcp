/**
 * @file nimcp_snn_wellbeing_bridge.h
 * @brief SNN-Wellbeing integration bridge
 *
 * WHAT: Bidirectional bridge between SNN and wellbeing/homeostasis
 * WHY:  Enable spike-based allostatic regulation and homeostatic control
 * HOW:  Encode wellbeing as population activity, regulate via spike pattern modulation
 *
 * BIOLOGICAL BASIS:
 * - Allostasis = predictive regulation anticipating needs (vs reactive homeostasis)
 * - Homeostatic setpoints encoded in baseline firing rates
 * - Allostatic load accumulates from sustained regulatory demands
 * - Recovery via parasympathetic downregulation and sleep
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#ifndef NIMCP_SNN_WELLBEING_BRIDGE_H
#define NIMCP_SNN_WELLBEING_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "snn/nimcp_snn_types.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "snn/nimcp_snn_network.h"
#include "async/nimcp_bio_async.h"

typedef void* wellbeing_system_t;

typedef struct snn_wellbeing_config_s {
    float homeostasis_setpoint;
    float allostatic_load_threshold;
    float recovery_rate;
    uint32_t wellbeing_population_id;
    uint32_t regulation_population_id;
    float update_interval_ms;
    bool enable_bio_async;
} snn_wellbeing_config_t;

typedef struct snn_wellbeing_state_s {
    float wellbeing_index;
    float allostatic_load;
    uint32_t regulation_events;
    bool is_regulating;
    bool is_overloaded;
    uint32_t overload_events;
    float accumulated_load;
} snn_wellbeing_state_t;

typedef struct snn_wellbeing_bridge_s {
    bridge_base_t base;                 /**< MUST be first: base bridge infrastructure */

    snn_network_t* snn;
    wellbeing_system_t* wellbeing_system;
    snn_wellbeing_config_t config;
    snn_wellbeing_state_t state;
    snn_population_t* wellbeing_pop;
    snn_population_t* regulation_pop;
    float* wellbeing_buffer;
    uint32_t buffer_capacity;
    float last_update_time;
    bool bio_async_enabled;
    bio_module_context_t bio_ctx;
    void* mutex;
} snn_wellbeing_bridge_t;

void snn_wellbeing_config_default(snn_wellbeing_config_t* config);
snn_wellbeing_bridge_t* snn_wellbeing_bridge_create(const snn_wellbeing_config_t* config, snn_network_t* snn, wellbeing_system_t* wellbeing_system);
void snn_wellbeing_bridge_destroy(snn_wellbeing_bridge_t* bridge);
int snn_wellbeing_bridge_connect_bio_async(snn_wellbeing_bridge_t* bridge);
int snn_wellbeing_bridge_disconnect_bio_async(snn_wellbeing_bridge_t* bridge);
bool snn_wellbeing_bridge_is_bio_async_connected(const snn_wellbeing_bridge_t* bridge);
int snn_wellbeing_bridge_encode_wellbeing(snn_wellbeing_bridge_t* bridge, float wellbeing);
int snn_wellbeing_bridge_trigger_regulation(snn_wellbeing_bridge_t* bridge);
int snn_wellbeing_bridge_apply_recovery(snn_wellbeing_bridge_t* bridge, float dt);
int snn_wellbeing_bridge_update(snn_wellbeing_bridge_t* bridge, float dt);
float snn_wellbeing_compute_wellbeing_index(const snn_wellbeing_bridge_t* bridge, float population_rate, float allostatic_load);
float snn_wellbeing_compute_allostatic_load(const snn_wellbeing_bridge_t* bridge, float regulation_rate, float current_load, float dt);
int snn_wellbeing_bridge_get_state(const snn_wellbeing_bridge_t* bridge, snn_wellbeing_state_t* state);
float snn_wellbeing_get_index(const snn_wellbeing_bridge_t* bridge);
float snn_wellbeing_get_allostatic_load(const snn_wellbeing_bridge_t* bridge);
uint32_t snn_wellbeing_get_regulation_events(const snn_wellbeing_bridge_t* bridge);
bool snn_wellbeing_is_regulating(const snn_wellbeing_bridge_t* bridge);
bool snn_wellbeing_is_overloaded(const snn_wellbeing_bridge_t* bridge);
int snn_wellbeing_get_stats(const snn_wellbeing_bridge_t* bridge, uint32_t* regulation_events, uint32_t* overload_events, float* avg_load);
void snn_wellbeing_reset_stats(snn_wellbeing_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SNN_WELLBEING_BRIDGE_H */
