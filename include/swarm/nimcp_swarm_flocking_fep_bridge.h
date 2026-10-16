/**
 * @file nimcp_swarm_flocking_fep_bridge.h
 * @brief FEP Bridge for Swarm Flocking Behavior
 *
 * WHAT: Free Energy Principle integration for Reynolds flocking dynamics
 * WHY:  Flocking as collective active inference, coordinated precision
 * HOW:  Separation/alignment/cohesion as free energy minimization
 *
 * BIOLOGICAL BASIS:
 * - Flocking rules as free energy gradients
 * - Neighbor positions as sensory observations
 * - Desired spacing as generative model
 * - Formation as low free energy attractor state
 * - Boid steering as active inference (minimize expected FE)
 *
 * @author NIMCP Development Team
 * @date 2025-12-15
 */

#ifndef NIMCP_SWARM_FLOCKING_FEP_BRIDGE_H
#define NIMCP_SWARM_FLOCKING_FEP_BRIDGE_H

#include "swarm/nimcp_swarm_flocking.h"
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
    float alignment_precision_weight; /**< Alignment as precision coordination */
    float cohesion_fe_coupling;      /**< Cohesion from FE minimization */
    float formation_prior_strength;  /**< Formation as generative model */
    bool enable_active_inference_steering; /**< Steering as active inference */
} swarm_flocking_fep_config_t;

typedef struct {
    float separation_adjustment;     /**< Separation weight modulation */
    float alignment_adjustment;      /**< Alignment weight modulation */
    float cohesion_adjustment;       /**< Cohesion weight modulation */
    float formation_tightness;       /**< Formation tightness modulation */
} swarm_flocking_fep_effects_t;

typedef struct {
    float precision_from_alignment;  /**< Precision from flock alignment */
    float uncertainty_from_dispersion; /**< Uncertainty from flock spread */
    float formation_confidence;      /**< Confidence in formation state */
} fep_swarm_flocking_effects_t;

typedef struct {
    float last_alignment_metric;
    float last_cohesion_metric;
    uint32_t formation_updates;
    uint64_t last_update_time;
} swarm_flocking_fep_state_t;

typedef struct {
    uint64_t total_updates;
    float avg_alignment_fe;
    float avg_formation_quality;
} swarm_flocking_fep_stats_t;

typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    swarm_flocking_fep_config_t config;
    fep_system_t* fep_system;
    nimcp_flocking_engine_t* flocking_engine;
    swarm_flocking_fep_effects_t fep_effects;
    fep_swarm_flocking_effects_t flocking_effects;
    swarm_flocking_fep_state_t state;
    swarm_flocking_fep_stats_t stats;} swarm_flocking_fep_bridge_t;

void swarm_flocking_fep_default_config(swarm_flocking_fep_config_t* config);
swarm_flocking_fep_bridge_t* swarm_flocking_fep_create(const swarm_flocking_fep_config_t* config, nimcp_flocking_engine_t* flocking_engine, fep_system_t* fep_system);
void swarm_flocking_fep_destroy(swarm_flocking_fep_bridge_t* bridge);
int swarm_flocking_fep_update(swarm_flocking_fep_bridge_t* bridge);
int swarm_flocking_fep_apply_modulation(swarm_flocking_fep_bridge_t* bridge);
int swarm_flocking_fep_get_effects(const swarm_flocking_fep_bridge_t* bridge, swarm_flocking_fep_effects_t* effects);
int swarm_flocking_fep_get_flocking_effects(const swarm_flocking_fep_bridge_t* bridge, fep_swarm_flocking_effects_t* effects);
int swarm_flocking_fep_get_stats(const swarm_flocking_fep_bridge_t* bridge, swarm_flocking_fep_stats_t* stats);
int swarm_flocking_fep_connect_bio_async(swarm_flocking_fep_bridge_t* bridge);
int swarm_flocking_fep_disconnect_bio_async(swarm_flocking_fep_bridge_t* bridge);
bool swarm_flocking_fep_is_bio_async_connected(const swarm_flocking_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SWARM_FLOCKING_FEP_BRIDGE_H */
