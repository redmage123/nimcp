/**
 * @file nimcp_snn_scaling_bridge.h
 * @brief SNN-Synaptic Scaling Integration Bridge
 *
 * WHAT: Bidirectional integration between SNN and synaptic scaling plasticity
 * WHY:  Multiplicative scaling of all synapses to maintain network stability
 * HOW:  Activity-dependent weight normalization based on firing rates
 *
 * BIOLOGICAL BASIS:
 * - Synaptic scaling maintains stable firing rates (Turrigiano et al., 1998)
 * - Multiplicative adjustment preserves relative weight differences
 * - Operates on slow timescale (hours to days)
 * - Global homeostatic mechanism complementing local Hebbian rules
 *
 * @author NIMCP Team
 * @date 2024
 */

#ifndef NIMCP_SNN_SCALING_BRIDGE_H
#define NIMCP_SNN_SCALING_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "snn/nimcp_snn_types.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "snn/nimcp_snn_network.h"
#include "plasticity/homeostatic/nimcp_homeostatic.h"
#include "async/nimcp_bio_async.h"

typedef struct snn_scaling_bridge_config_s {
    float target_rate_hz;
    float scaling_exponent;
    float min_scaling_factor;
    float max_scaling_factor;
    float update_interval_ms;
    bool bidirectional_updates;
    bool enable_bio_async;
} snn_scaling_bridge_config_t;

typedef struct snn_scaling_effects_s {
    float avg_scaling_factor;
    float scaling_variance;
    uint32_t scaled_synapses;
    bool convergence_achieved;
} snn_scaling_effects_t;

typedef struct snn_scaling_bridge_s {
    bridge_base_t base;                 /**< MUST be first: base bridge infrastructure */

    snn_network_t* network;
    synaptic_scaling_state_t* scaling_states;
    uint32_t n_neurons;
    uint32_t n_synapses;

    snn_scaling_bridge_config_t config;
    snn_scaling_effects_t effects;

    bool connected;
    float last_update_time_ms;
    uint32_t scaling_events;

    bool bio_async_enabled;
    bio_module_context_t bio_ctx;
    void* mutex;
} snn_scaling_bridge_t;

void snn_scaling_bridge_config_default(snn_scaling_bridge_config_t* config);

snn_scaling_bridge_t* snn_scaling_bridge_create(
    const snn_scaling_bridge_config_t* config,
    snn_network_t* network,
    synaptic_scaling_state_t* scaling_states,
    uint32_t n_neurons,
    uint32_t n_synapses
);

void snn_scaling_bridge_destroy(snn_scaling_bridge_t* bridge);

int snn_scaling_bridge_connect_bio_async(snn_scaling_bridge_t* bridge);
int snn_scaling_bridge_disconnect_bio_async(snn_scaling_bridge_t* bridge);
bool snn_scaling_bridge_is_bio_async_connected(const snn_scaling_bridge_t* bridge);

int snn_scaling_bridge_compute_factors(snn_scaling_bridge_t* bridge, float dt);
int snn_scaling_bridge_apply_plasticity(snn_scaling_bridge_t* bridge, float dt);
int snn_scaling_bridge_update(snn_scaling_bridge_t* bridge, float dt);

int snn_scaling_bridge_get_weight_changes(
    const snn_scaling_bridge_t* bridge,
    uint32_t* synapse_ids,
    float* scaling_factors,
    uint32_t max_changes,
    uint32_t* n_changes
);

int snn_scaling_bridge_get_effects(
    const snn_scaling_bridge_t* bridge,
    snn_scaling_effects_t* effects
);

float snn_scaling_bridge_get_avg_factor(const snn_scaling_bridge_t* bridge);

int snn_scaling_bridge_get_stats(
    const snn_scaling_bridge_t* bridge,
    uint32_t* scaling_events,
    uint32_t* updates
);

void snn_scaling_bridge_reset_stats(snn_scaling_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SNN_SCALING_BRIDGE_H */
