/**
 * @file nimcp_snn_executive_bridge.h
 * @brief SNN-Executive Control integration bridge
 *
 * WHAT: Bidirectional bridge between SNN and executive control system
 * WHY:  Enable inhibitory control via interneuron spikes and task switching
 * WHY:  Task switching implemented through network state changes
 *
 * BIOLOGICAL BASIS:
 * - PFC interneurons (PV+, SST+) provide inhibitory control
 * - Fast-spiking interneurons implement rapid response inhibition
 * - Task switching involves network state transitions in PFC
 * - Cognitive load correlates with overall PFC spike rate
 *
 * INTEGRATION:
 * - SNN → Executive: Interneuron spike rate controls inhibition
 * - SNN → Executive: Network state transitions represent task switches
 * - Executive → SNN: Task priority modulates population excitability
 * - Executive → SNN: Inhibitory signals activate interneuron populations
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#ifndef NIMCP_SNN_EXECUTIVE_BRIDGE_H
#define NIMCP_SNN_EXECUTIVE_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "snn/nimcp_snn_types.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "snn/nimcp_snn_network.h"
#include "cognitive/nimcp_executive.h"

//=============================================================================
// Configuration Types
//=============================================================================

typedef struct snn_executive_config_s {
    float inhibition_rate_threshold;    /**< Min interneuron rate for inhibition (Hz) */
    float task_switch_rate_change;      /**< Rate change indicating switch */
    float cognitive_load_max_rate;      /**< Max rate for full cognitive load */
    bool enable_interneuron_control;    /**< Enable interneuron inhibition */
    float interneuron_efficacy;         /**< Inhibitory synapse strength */
    uint32_t executive_population_id;   /**< Population for executive control */
    uint32_t interneuron_population_id; /**< Interneuron population */
    float update_interval_ms;           /**< Update interval */
    bool enable_bio_async;              /**< Enable bio-async */
} snn_executive_config_t;

typedef struct snn_executive_state_s {
    float inhibition_strength;          /**< Current inhibition [0, 1] */
    float cognitive_load;               /**< Current load [0, 1] */
    bool task_switching;                /**< Switch in progress */
    float interneuron_rate;             /**< Interneuron firing rate */
    float executive_rate;               /**< Executive population rate */
    uint32_t switch_count;              /**< Total switches detected */
} snn_executive_state_t;

typedef struct snn_executive_bridge_s {
    bridge_base_t base;                 /**< MUST be first: base bridge infrastructure */

    snn_network_t* snn;
    executive_controller_t* executive;
    snn_executive_config_t config;
    snn_executive_state_t state;
    snn_population_t* executive_pop;
    snn_population_t* interneuron_pop;
    float last_update_time;
    float last_executive_rate;
    bool bio_async_enabled;
    bio_module_context_t bio_ctx;
    void* mutex;
} snn_executive_bridge_t;

//=============================================================================
// API Functions
//=============================================================================

void snn_executive_config_default(snn_executive_config_t* config);

snn_executive_bridge_t* snn_executive_bridge_create(
    const snn_executive_config_t* config,
    snn_network_t* snn,
    executive_controller_t* executive
);

void snn_executive_bridge_destroy(snn_executive_bridge_t* bridge);

int snn_executive_bridge_connect_bio_async(snn_executive_bridge_t* bridge);
int snn_executive_bridge_disconnect_bio_async(snn_executive_bridge_t* bridge);
bool snn_executive_bridge_is_bio_async_connected(const snn_executive_bridge_t* bridge);

int snn_executive_bridge_process(
    snn_executive_bridge_t* bridge,
    const float* input,
    float* output
);

int snn_executive_bridge_update(snn_executive_bridge_t* bridge, float dt);

float snn_executive_compute_inhibition(
    const snn_executive_bridge_t* bridge,
    float interneuron_rate
);

bool snn_executive_detect_task_switch(
    snn_executive_bridge_t* bridge,
    float current_rate
);

float snn_executive_compute_cognitive_load(
    const snn_executive_bridge_t* bridge,
    float population_rate
);

int snn_executive_apply_inhibition(
    snn_executive_bridge_t* bridge,
    float inhibition_strength
);

int snn_executive_bridge_get_state(
    const snn_executive_bridge_t* bridge,
    snn_executive_state_t* state
);

float snn_executive_get_inhibition(const snn_executive_bridge_t* bridge);
float snn_executive_get_cognitive_load(const snn_executive_bridge_t* bridge);
bool snn_executive_is_task_switching(const snn_executive_bridge_t* bridge);

int snn_executive_get_stats(
    const snn_executive_bridge_t* bridge,
    uint32_t* switch_count,
    float* avg_inhibition,
    float* avg_load
);

void snn_executive_reset_stats(snn_executive_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SNN_EXECUTIVE_BRIDGE_H */
