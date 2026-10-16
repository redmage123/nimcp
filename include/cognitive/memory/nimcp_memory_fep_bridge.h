/**
 * @file nimcp_memory_fep_bridge.h
 * @brief Free Energy Principle - Memory Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between FEP and working memory system
 * WHY:  Working memory maintains variational density (belief buffer). Memory is
 *       active inference over temporal sequences.
 * HOW:  FEP beliefs stored in WM; WM content biases FEP priors; consolidation
 *       minimizes long-term free energy.
 *
 * BIOLOGICAL BASIS:
 * - WM as belief buffer (PFC maintains posterior beliefs)
 * - Memory retrieval = active inference (recall minimizes F)
 * - Consolidation = offline replay minimizing surprise
 * - Friston & Buzsáki (2016): "The functional anatomy of time"
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_MEMORY_FEP_BRIDGE_H
#define NIMCP_MEMORY_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/free_energy/nimcp_free_energy.h"
#include "cognitive/memory/nimcp_semantic_memory.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MEMORY_FEP_WM_CAPACITY            7  /**< 7±2 items */
#define MEMORY_FEP_CONSOLIDATION_THRESHOLD 0.8f

typedef struct memory_fep_bridge memory_fep_bridge_t;

typedef struct {
    float wm_capacity_factor;
    float consolidation_threshold;
    float retrieval_precision_boost;
    bool enable_wm_belief_buffer;
    bool enable_consolidation_replay;
    bool enable_retrieval_active_inference;
    float belief_prior_strength;
    float memory_trace_persistence;
    bool enable_belief_priors;
    bool enable_trace_persistence;
    float fe_sensitivity;
    float memory_sensitivity;
} memory_fep_config_t;

typedef struct {
    float wm_load;
    float wm_capacity_remaining;
    float consolidation_pressure;
    bool consolidation_triggered;
    float retrieval_precision;
} memory_fep_effects_t;

typedef struct {
    float belief_prior_bias;
    bool priors_active;
    float trace_persistence_factor;
} fep_memory_effects_t;

typedef struct {
    float current_wm_load;
    float current_precision;
    bool consolidation_active;
    uint64_t last_consolidation_time;
} memory_fep_state_t;

typedef struct {
    uint64_t wm_buffer_events;
    uint64_t consolidation_events;
    uint64_t retrieval_events;
    float avg_wm_load;
    uint64_t belief_prior_applications;
    float avg_free_energy;
} memory_fep_stats_t;

struct memory_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    memory_fep_config_t config;
    fep_system_t* fep_system;
    semantic_memory_system_t* memory_system;
    memory_fep_effects_t fep_effects;
    fep_memory_effects_t memory_effects;
    memory_fep_state_t state;
    memory_fep_stats_t stats;
};

int memory_fep_bridge_default_config(memory_fep_config_t* config);
memory_fep_bridge_t* memory_fep_bridge_create(const memory_fep_config_t* config);
void memory_fep_bridge_destroy(memory_fep_bridge_t* bridge);

int memory_fep_bridge_connect_fep(memory_fep_bridge_t* bridge, fep_system_t* fep);
int memory_fep_bridge_connect_memory(memory_fep_bridge_t* bridge, semantic_memory_system_t* memory);
int memory_fep_bridge_disconnect(memory_fep_bridge_t* bridge);

int memory_fep_maintain_wm_beliefs(memory_fep_bridge_t* bridge);
int memory_fep_trigger_consolidation(memory_fep_bridge_t* bridge);
int memory_fep_boost_retrieval_precision(memory_fep_bridge_t* bridge);

int memory_fep_apply_belief_priors(memory_fep_bridge_t* bridge);
int memory_fep_apply_trace_persistence(memory_fep_bridge_t* bridge);

int memory_fep_bridge_update(memory_fep_bridge_t* bridge, uint64_t delta_ms);

int memory_fep_bridge_get_state(memory_fep_bridge_t* bridge, memory_fep_state_t* state);
int memory_fep_bridge_get_stats(memory_fep_bridge_t* bridge, memory_fep_stats_t* stats);

int memory_fep_bridge_connect_bio_async(memory_fep_bridge_t* bridge);
int memory_fep_bridge_disconnect_bio_async(memory_fep_bridge_t* bridge);
bool memory_fep_bridge_is_bio_async_connected(const memory_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MEMORY_FEP_BRIDGE_H */
