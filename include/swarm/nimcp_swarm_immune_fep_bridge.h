/**
 * @file nimcp_swarm_immune_fep_bridge.h
 * @brief FEP Bridge for Swarm Immune System
 *
 * WHAT: Free Energy Principle integration for swarm immune response
 * WHY:  Immune response as active inference against threats
 * HOW:  Threat detection as prediction error, response as action
 *
 * BIOLOGICAL BASIS:
 * - Threat detection as prediction error (unexpected patterns)
 * - Immune response as active inference (minimize expected harm)
 * - Antibody production as model parameter learning
 * - Inflammation as precision modulation
 *
 * @author NIMCP Development Team
 * @date 2025-12-15
 */

#ifndef NIMCP_SWARM_IMMUNE_FEP_BRIDGE_H
#define NIMCP_SWARM_IMMUNE_FEP_BRIDGE_H

#include "swarm/nimcp_swarm_immune.h"
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
    float threat_pe_weight;          /**< Threat as prediction error */
    float response_action_gain;      /**< Response as action selection */
    float inflammation_precision_mod; /**< Inflammation affects precision */
    bool enable_adaptive_response;   /**< FEP-driven response adaptation */
} swarm_immune_fep_config_t;

typedef struct {
    float threat_sensitivity;        /**< Threat detection sensitivity */
    float response_urgency;          /**< Response speed boost */
    float inflammation_level;        /**< Inflammation modulation */
} swarm_immune_fep_effects_t;

typedef struct {
    float precision_from_threat;     /**< Precision from threat level */
    float action_bias_from_immune;   /**< Action bias from immune state */
    float learning_suppression;      /**< Learning suppression during threat */
} fep_swarm_immune_effects_t;

typedef struct {
    uint32_t threats_detected;
    uint32_t responses_triggered;
    float last_threat_fe;
    uint64_t last_update_time;
} swarm_immune_fep_state_t;

typedef struct {
    uint64_t total_updates;
    uint64_t threats_processed;
    float avg_threat_fe;
} swarm_immune_fep_stats_t;

typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    swarm_immune_fep_config_t config;
    fep_system_t* fep_system;
    void* immune_system;
    swarm_immune_fep_effects_t fep_effects;
    fep_swarm_immune_effects_t immune_effects;
    swarm_immune_fep_state_t state;
    swarm_immune_fep_stats_t stats;} swarm_immune_fep_bridge_t;

void swarm_immune_fep_default_config(swarm_immune_fep_config_t* config);
swarm_immune_fep_bridge_t* swarm_immune_fep_create(const swarm_immune_fep_config_t* config, void* immune_system, fep_system_t* fep_system);
void swarm_immune_fep_destroy(swarm_immune_fep_bridge_t* bridge);
int swarm_immune_fep_update(swarm_immune_fep_bridge_t* bridge);
int swarm_immune_fep_apply_modulation(swarm_immune_fep_bridge_t* bridge);
int swarm_immune_fep_get_effects(const swarm_immune_fep_bridge_t* bridge, swarm_immune_fep_effects_t* effects);
int swarm_immune_fep_get_immune_effects(const swarm_immune_fep_bridge_t* bridge, fep_swarm_immune_effects_t* effects);
int swarm_immune_fep_get_stats(const swarm_immune_fep_bridge_t* bridge, swarm_immune_fep_stats_t* stats);
int swarm_immune_fep_connect_bio_async(swarm_immune_fep_bridge_t* bridge);
int swarm_immune_fep_disconnect_bio_async(swarm_immune_fep_bridge_t* bridge);
bool swarm_immune_fep_is_bio_async_connected(const swarm_immune_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SWARM_IMMUNE_FEP_BRIDGE_H */
