/**
 * @file nimcp_swarm_emergence_fep_bridge.h
 * @brief FEP Bridge for Swarm Emergence System
 *
 * WHAT: Free Energy Principle integration for tier-based emergence
 * WHY:  Emergence as hierarchical generative model depth
 * HOW:  Tier advancement as model complexity increase from reduced FE
 *
 * BIOLOGICAL BASIS:
 * - Emergence tiers as hierarchical model levels
 * - Coherence as model agreement (low collective surprise)
 * - Tier advancement as phase transition in free energy landscape
 * - Capabilities as emergent inference properties
 *
 * @author NIMCP Development Team
 * @date 2025-12-15
 */

#ifndef NIMCP_SWARM_EMERGENCE_FEP_BRIDGE_H
#define NIMCP_SWARM_EMERGENCE_FEP_BRIDGE_H

#include "swarm/nimcp_swarm_emergence.h"
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
    float tier_fe_threshold;         /**< FE threshold for tier advancement */
    float coherence_precision_gain;  /**< Precision from coherence */
    float hierarchy_depth_scaling;   /**< Tier as hierarchy depth */
    bool enable_emergence_detection; /**< Detect emergence transitions */
} swarm_emergence_fep_config_t;

typedef struct {
    float tier_advancement_bias;     /**< Bias toward tier advancement */
    float coherence_boost;           /**< Coherence enhancement */
    float capability_activation_threshold; /**< Capability activation threshold */
} swarm_emergence_fep_effects_t;

typedef struct {
    float precision_from_tier;       /**< Precision from emergence tier */
    uint32_t hierarchy_depth;        /**< Effective hierarchy depth */
    float model_complexity;          /**< Model complexity from tier */
} fep_swarm_emergence_effects_t;

typedef struct {
    swarm_emergence_tier_t last_tier;
    float last_coherence;
    uint32_t tier_transitions;
    uint64_t last_update_time;
} swarm_emergence_fep_state_t;

typedef struct {
    uint64_t total_updates;
    uint32_t emergence_events;
    float avg_tier_fe;
} swarm_emergence_fep_stats_t;

typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    swarm_emergence_fep_config_t config;
    fep_system_t* fep_system;
    swarm_emergence_ctx_t* emergence_ctx;
    swarm_emergence_fep_effects_t fep_effects;
    fep_swarm_emergence_effects_t emergence_effects;
    swarm_emergence_fep_state_t state;
    swarm_emergence_fep_stats_t stats;} swarm_emergence_fep_bridge_t;

void swarm_emergence_fep_default_config(swarm_emergence_fep_config_t* config);
swarm_emergence_fep_bridge_t* swarm_emergence_fep_create(const swarm_emergence_fep_config_t* config, swarm_emergence_ctx_t* emergence_ctx, fep_system_t* fep_system);
void swarm_emergence_fep_destroy(swarm_emergence_fep_bridge_t* bridge);
int swarm_emergence_fep_update(swarm_emergence_fep_bridge_t* bridge);
int swarm_emergence_fep_apply_modulation(swarm_emergence_fep_bridge_t* bridge);
int swarm_emergence_fep_get_effects(const swarm_emergence_fep_bridge_t* bridge, swarm_emergence_fep_effects_t* effects);
int swarm_emergence_fep_get_emergence_effects(const swarm_emergence_fep_bridge_t* bridge, fep_swarm_emergence_effects_t* effects);
int swarm_emergence_fep_get_stats(const swarm_emergence_fep_bridge_t* bridge, swarm_emergence_fep_stats_t* stats);
int swarm_emergence_fep_connect_bio_async(swarm_emergence_fep_bridge_t* bridge);
int swarm_emergence_fep_disconnect_bio_async(swarm_emergence_fep_bridge_t* bridge);
bool swarm_emergence_fep_is_bio_async_connected(const swarm_emergence_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SWARM_EMERGENCE_FEP_BRIDGE_H */
