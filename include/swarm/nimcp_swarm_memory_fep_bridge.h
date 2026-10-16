/**
 * @file nimcp_swarm_memory_fep_bridge.h
 * @brief FEP Bridge for Swarm Distributed Memory
 *
 * WHAT: Free Energy Principle integration for swarm distributed memory system
 * WHY:  Memory as generative model, recall as inference
 * HOW:  Bidirectional modulation between memory patterns and FEP beliefs
 *
 * BIOLOGICAL BASIS:
 * - Distributed memory as collective generative model
 * - Memory recall as predictive coding
 * - Pattern completion as free energy minimization
 * - Consolidation as model parameter update
 *
 * @author NIMCP Development Team
 * @date 2025-12-15
 */

#ifndef NIMCP_SWARM_MEMORY_FEP_BRIDGE_H
#define NIMCP_SWARM_MEMORY_FEP_BRIDGE_H

#include "swarm/nimcp_swarm_memory.h"
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
    float recall_precision_weight;   /**< Precision weight for recall */
    float consolidation_fe_threshold; /**< FE threshold for consolidation */
    float pattern_completion_gain;   /**< Pattern completion strength */
    bool enable_predictive_recall;   /**< Recall as inference */
} swarm_memory_fep_config_t;

typedef struct {
    float recall_confidence;         /**< Memory recall confidence boost */
    float consolidation_rate;        /**< Consolidation rate adjustment */
    float pattern_strength;          /**< Pattern strength modulation */
} swarm_memory_fep_effects_t;

typedef struct {
    float precision_from_recall;     /**< Precision from memory quality */
    float prior_strength_from_memory; /**< Prior strength from memory */
    uint32_t active_patterns;        /**< Number of active memory patterns */
} fep_swarm_memory_effects_t;

typedef struct {
    uint32_t recall_count;
    uint32_t consolidation_count;
    float last_recall_fe;
    uint64_t last_update_time;
} swarm_memory_fep_state_t;

typedef struct {
    uint64_t total_updates;
    uint64_t recalls_processed;
    uint64_t consolidations_triggered;
    float avg_recall_fe;
} swarm_memory_fep_stats_t;

typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    swarm_memory_fep_config_t config;
    fep_system_t* fep_system;
    void* memory_ctx;
    swarm_memory_fep_effects_t fep_effects;
    fep_swarm_memory_effects_t memory_effects;
    swarm_memory_fep_state_t state;
    swarm_memory_fep_stats_t stats;} swarm_memory_fep_bridge_t;

void swarm_memory_fep_default_config(swarm_memory_fep_config_t* config);
swarm_memory_fep_bridge_t* swarm_memory_fep_create(
    const swarm_memory_fep_config_t* config,
    void* memory_ctx,
    fep_system_t* fep_system
);
void swarm_memory_fep_destroy(swarm_memory_fep_bridge_t* bridge);
int swarm_memory_fep_update(swarm_memory_fep_bridge_t* bridge);
int swarm_memory_fep_apply_modulation(swarm_memory_fep_bridge_t* bridge);
int swarm_memory_fep_get_effects(const swarm_memory_fep_bridge_t* bridge, swarm_memory_fep_effects_t* effects);
int swarm_memory_fep_get_memory_effects(const swarm_memory_fep_bridge_t* bridge, fep_swarm_memory_effects_t* effects);
int swarm_memory_fep_get_stats(const swarm_memory_fep_bridge_t* bridge, swarm_memory_fep_stats_t* stats);
int swarm_memory_fep_connect_bio_async(swarm_memory_fep_bridge_t* bridge);
int swarm_memory_fep_disconnect_bio_async(swarm_memory_fep_bridge_t* bridge);
bool swarm_memory_fep_is_bio_async_connected(const swarm_memory_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SWARM_MEMORY_FEP_BRIDGE_H */
