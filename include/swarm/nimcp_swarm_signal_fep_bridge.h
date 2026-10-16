/**
 * @file nimcp_swarm_signal_fep_bridge.h
 * @brief FEP Bridge for Swarm Signal Communication
 *
 * WHAT: Free Energy Principle integration for swarm signal/pheromone communication
 * WHY:  Signals as prediction error messages, routing as inference
 * HOW:  Bidirectional modulation between signal propagation and FEP beliefs
 *
 * BIOLOGICAL BASIS:
 * - Signals as prediction error broadcasts
 * - Signal strength as precision weighting
 * - Routing as active inference (minimize expected free energy)
 * - Pheromone gradients as belief gradients
 *
 * @author NIMCP Development Team
 * @date 2025-12-15
 */

#ifndef NIMCP_SWARM_SIGNAL_FEP_BRIDGE_H
#define NIMCP_SWARM_SIGNAL_FEP_BRIDGE_H

#include "swarm/nimcp_swarm_signal.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "async/nimcp_bio_router.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float signal_precision_weight;   /**< Precision weight for signals */
    float routing_fe_threshold;      /**< FE threshold for routing */
    float propagation_decay_rate;    /**< Signal decay as uncertainty */
    bool enable_precision_routing;   /**< Route by precision */
} swarm_signal_fep_config_t;

typedef struct {
    float signal_amplification;      /**< Signal strength boost */
    float routing_confidence;        /**< Routing confidence adjustment */
    float propagation_rate;          /**< Propagation speed adjustment */
} swarm_signal_fep_effects_t;

typedef struct {
    float precision_from_signal;     /**< Precision from signal quality */
    float belief_update_from_signal; /**< Belief update magnitude */
    uint32_t active_channels;        /**< Active communication channels */
} fep_swarm_signal_effects_t;

typedef struct {
    uint32_t signals_processed;
    float last_signal_fe;
    uint64_t last_update_time;
} swarm_signal_fep_state_t;

typedef struct {
    uint64_t total_updates;
    uint64_t signals_routed;
    float avg_signal_fe;
} swarm_signal_fep_stats_t;

typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    swarm_signal_fep_config_t config;
    fep_system_t* fep_system;
    void* signal_ctx;
    swarm_signal_fep_effects_t fep_effects;
    fep_swarm_signal_effects_t signal_effects;
    swarm_signal_fep_state_t state;
    swarm_signal_fep_stats_t stats;} swarm_signal_fep_bridge_t;

void swarm_signal_fep_default_config(swarm_signal_fep_config_t* config);
swarm_signal_fep_bridge_t* swarm_signal_fep_create(const swarm_signal_fep_config_t* config, void* signal_ctx, fep_system_t* fep_system);
void swarm_signal_fep_destroy(swarm_signal_fep_bridge_t* bridge);
int swarm_signal_fep_update(swarm_signal_fep_bridge_t* bridge);
int swarm_signal_fep_apply_modulation(swarm_signal_fep_bridge_t* bridge);
int swarm_signal_fep_get_effects(const swarm_signal_fep_bridge_t* bridge, swarm_signal_fep_effects_t* effects);
int swarm_signal_fep_get_signal_effects(const swarm_signal_fep_bridge_t* bridge, fep_swarm_signal_effects_t* effects);
int swarm_signal_fep_get_stats(const swarm_signal_fep_bridge_t* bridge, swarm_signal_fep_stats_t* stats);
int swarm_signal_fep_connect_bio_async(swarm_signal_fep_bridge_t* bridge);
int swarm_signal_fep_disconnect_bio_async(swarm_signal_fep_bridge_t* bridge);
bool swarm_signal_fep_is_bio_async_connected(const swarm_signal_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SWARM_SIGNAL_FEP_BRIDGE_H */
