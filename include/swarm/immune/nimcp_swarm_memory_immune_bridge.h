/**
 * @file nimcp_swarm_memory_immune_bridge.h
 * @brief Swarm Memory-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-18
 *
 * WHAT: Bidirectional integration between brain immune system and swarm memory
 * WHY:  Inflammation impairs memory consolidation; memory overload triggers immune
 * HOW:  Cytokines reduce consolidation; high load triggers stress response
 *
 * BIOLOGICAL BASIS:
 * - Pro-inflammatory cytokines impair memory consolidation
 * - Inflammation increases forgetting rate
 * - Memory overload triggers cortisol-like stress response
 * - Memory corruption triggers immune cleanup
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SWARM_MEMORY_IMMUNE_BRIDGE_H
#define NIMCP_SWARM_MEMORY_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/immune/nimcp_brain_immune.h"
#include "async/nimcp_bio_router.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Cytokine memory impact factors */
#define CYTOKINE_IL1_CONSOLIDATION_IMPACT     -0.4f
#define CYTOKINE_IL6_CONSOLIDATION_IMPACT     -0.3f
#define CYTOKINE_TNF_CONSOLIDATION_IMPACT     -0.5f
#define CYTOKINE_IL10_CONSOLIDATION_IMPACT     0.15f

/* Inflammation memory capacity factors */
#define INFLAMMATION_NONE_MEMORY_CAPACITY     1.0f
#define INFLAMMATION_LOCAL_MEMORY_CAPACITY    0.95f
#define INFLAMMATION_REGIONAL_MEMORY_CAPACITY 0.80f
#define INFLAMMATION_SYSTEMIC_MEMORY_CAPACITY 0.60f
#define INFLAMMATION_STORM_MEMORY_CAPACITY    0.30f

/* Inflammation forgetting multipliers */
#define INFLAMMATION_NONE_FORGETTING_MULT     1.0f
#define INFLAMMATION_LOCAL_FORGETTING_MULT    1.1f
#define INFLAMMATION_REGIONAL_FORGETTING_MULT 1.3f
#define INFLAMMATION_SYSTEMIC_FORGETTING_MULT 1.6f
#define INFLAMMATION_STORM_FORGETTING_MULT    2.5f

typedef struct {
    float consolidation_deficit;
    float forgetting_rate_multiplier;
    float replay_strength_reduction;
} cytokine_memory_effects_t;

typedef struct {
    brain_inflammation_level_t current_level;
    float capacity_factor;
    float consolidation_efficiency;
    float forgetting_multiplier;
} inflammation_memory_state_t;

typedef struct {
    float memory_load;
    uint32_t corruptions;
    bool stress_triggered;
    float cleanup_signal;
} memory_immune_modulation_t;

typedef struct {
    bool enable_cytokine_effects;
    bool enable_inflammation_effects;
    bool enable_load_stress;
    bool enable_corruption_cleanup;
    float cytokine_sensitivity;
} swarm_memory_immune_config_t;

typedef struct swarm_memory_immune_bridge_struct swarm_memory_immune_bridge_t;

int swarm_memory_immune_default_config(swarm_memory_immune_config_t* config);
swarm_memory_immune_bridge_t* swarm_memory_immune_create(
    const swarm_memory_immune_config_t* config,
    brain_immune_system_t* immune_system,
    void* swarm_memory);
void swarm_memory_immune_destroy(swarm_memory_immune_bridge_t* bridge);

int swarm_memory_immune_apply_cytokine_effects(swarm_memory_immune_bridge_t* bridge);
int swarm_memory_immune_apply_inflammation_effects(swarm_memory_immune_bridge_t* bridge);

int swarm_memory_immune_trigger_stress_from_load(swarm_memory_immune_bridge_t* bridge);
int swarm_memory_immune_activate_cleanup(swarm_memory_immune_bridge_t* bridge, uint32_t corruptions);

int swarm_memory_immune_update(swarm_memory_immune_bridge_t* bridge, uint64_t delta_ms);

float swarm_memory_immune_get_capacity_factor(const swarm_memory_immune_bridge_t* bridge);
float swarm_memory_immune_get_consolidation_efficiency(const swarm_memory_immune_bridge_t* bridge);
float swarm_memory_immune_compute_forgetting_multiplier(const swarm_memory_immune_bridge_t* bridge);
bool swarm_memory_immune_has_memory_impairment(const swarm_memory_immune_bridge_t* bridge);

int swarm_memory_immune_connect_bio_async(swarm_memory_immune_bridge_t* bridge);
int swarm_memory_immune_disconnect_bio_async(swarm_memory_immune_bridge_t* bridge);
bool swarm_memory_immune_is_bio_async_connected(const swarm_memory_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SWARM_MEMORY_IMMUNE_BRIDGE_H */
