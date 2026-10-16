/**
 * @file nimcp_swarm_pheromone_fep_bridge.h
 * @brief FEP Bridge for Swarm Pheromone System
 *
 * WHAT: Free Energy Principle integration for pheromone-based coordination
 * WHY:  Pheromone gradients as belief gradients, following as inference
 * HOW:  Bidirectional modulation between pheromone fields and FEP
 *
 * BIOLOGICAL BASIS:
 * - Pheromone concentration as belief strength
 * - Gradient following as gradient descent on free energy
 * - Trail formation as belief consolidation
 * - Evaporation as uncertainty increase
 *
 * @author NIMCP Development Team
 * @date 2025-12-15
 */

#ifndef NIMCP_SWARM_PHEROMONE_FEP_BRIDGE_H
#define NIMCP_SWARM_PHEROMONE_FEP_BRIDGE_H

#include "swarm/nimcp_swarm_pheromone.h"
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
    float gradient_precision_weight; /**< Gradient strength as precision */
    float evaporation_fe_coupling;   /**< Evaporation rate from FE */
    float trail_strength_gain;       /**< Trail formation strength */
    bool enable_fe_gradient_descent; /**< Gradient following as FE minimization */
} swarm_pheromone_fep_config_t;

typedef struct {
    float deposition_rate;           /**< Pheromone deposition boost */
    float evaporation_adjustment;    /**< Evaporation rate adjustment */
    float gradient_following_bias;   /**< Bias toward gradient following */
} swarm_pheromone_fep_effects_t;

typedef struct {
    float precision_from_gradient;   /**< Precision from gradient strength */
    float action_bias_from_trail;    /**< Action bias from pheromone trail */
    float uncertainty_from_evaporation; /**< Uncertainty from evaporation */
} fep_swarm_pheromone_effects_t;

typedef struct {
    float last_gradient_magnitude;
    uint32_t trails_active;
    uint64_t last_update_time;
} swarm_pheromone_fep_state_t;

typedef struct {
    uint64_t total_updates;
    uint64_t trails_formed;
    float avg_gradient_fe;
} swarm_pheromone_fep_stats_t;

typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    swarm_pheromone_fep_config_t config;
    fep_system_t* fep_system;
    void* pheromone_ctx;
    swarm_pheromone_fep_effects_t fep_effects;
    fep_swarm_pheromone_effects_t pheromone_effects;
    swarm_pheromone_fep_state_t state;
    swarm_pheromone_fep_stats_t stats;} swarm_pheromone_fep_bridge_t;

void swarm_pheromone_fep_default_config(swarm_pheromone_fep_config_t* config);
swarm_pheromone_fep_bridge_t* swarm_pheromone_fep_create(const swarm_pheromone_fep_config_t* config, void* pheromone_ctx, fep_system_t* fep_system);
void swarm_pheromone_fep_destroy(swarm_pheromone_fep_bridge_t* bridge);
int swarm_pheromone_fep_update(swarm_pheromone_fep_bridge_t* bridge);
int swarm_pheromone_fep_apply_modulation(swarm_pheromone_fep_bridge_t* bridge);
int swarm_pheromone_fep_get_effects(const swarm_pheromone_fep_bridge_t* bridge, swarm_pheromone_fep_effects_t* effects);
int swarm_pheromone_fep_get_pheromone_effects(const swarm_pheromone_fep_bridge_t* bridge, fep_swarm_pheromone_effects_t* effects);
int swarm_pheromone_fep_get_stats(const swarm_pheromone_fep_bridge_t* bridge, swarm_pheromone_fep_stats_t* stats);
int swarm_pheromone_fep_connect_bio_async(swarm_pheromone_fep_bridge_t* bridge);
int swarm_pheromone_fep_disconnect_bio_async(swarm_pheromone_fep_bridge_t* bridge);
bool swarm_pheromone_fep_is_bio_async_connected(const swarm_pheromone_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SWARM_PHEROMONE_FEP_BRIDGE_H */
