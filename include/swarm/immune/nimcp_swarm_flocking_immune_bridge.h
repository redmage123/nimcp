/**
 * @file nimcp_swarm_flocking_immune_bridge.h
 * @brief Swarm Flocking-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-18
 *
 * WHAT: Bidirectional integration between brain immune system and swarm flocking
 * WHY:  Inflammation affects flocking behavior; formation failures trigger immune
 * HOW:  Cytokines reduce alignment/cohesion; fragmentation triggers stress
 *
 * BIOLOGICAL BASIS:
 * - Pro-inflammatory cytokines reduce flocking cohesion and alignment
 * - Inflammation increases separation tendency (isolation behavior)
 * - Flock fragmentation triggers immune stress response
 * - Tight formations release anti-inflammatory signals
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SWARM_FLOCKING_IMMUNE_BRIDGE_H
#define NIMCP_SWARM_FLOCKING_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/immune/nimcp_brain_immune.h"
#include "async/nimcp_bio_router.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Cytokine flocking impact factors */
#define CYTOKINE_IL1_ALIGNMENT_IMPACT         -0.3f
#define CYTOKINE_IL6_ALIGNMENT_IMPACT         -0.2f
#define CYTOKINE_TNF_ALIGNMENT_IMPACT         -0.4f
#define CYTOKINE_IL10_COHESION_BOOST           0.2f

/* Cytokine separation increase */
#define CYTOKINE_IL1_SEPARATION_INCREASE       0.3f
#define CYTOKINE_TNF_SEPARATION_INCREASE       0.5f

/* Inflammation flocking factors */
#define INFLAMMATION_NONE_FLOCKING_FACTOR      1.0f
#define INFLAMMATION_LOCAL_FLOCKING_FACTOR     0.95f
#define INFLAMMATION_REGIONAL_FLOCKING_FACTOR  0.85f
#define INFLAMMATION_SYSTEMIC_FLOCKING_FACTOR  0.70f
#define INFLAMMATION_STORM_FLOCKING_FACTOR     0.50f

typedef struct {
    float alignment_reduction;
    float cohesion_reduction;
    float separation_increase;
    float formation_quality_impact;
} cytokine_flocking_effects_t;

typedef struct {
    brain_inflammation_level_t current_level;
    float flocking_factor;
    float fragmentation_risk;
    bool formation_degraded;
} inflammation_flocking_state_t;

typedef struct {
    float formation_quality;
    uint32_t fragmentation_events;
    bool stress_triggered;
    float il10_from_formation;
} flocking_immune_modulation_t;

typedef struct {
    bool enable_cytokine_effects;
    bool enable_inflammation_effects;
    bool enable_fragmentation_stress;
    bool enable_formation_boost;
    float cytokine_sensitivity;
} swarm_flocking_immune_config_t;

typedef struct swarm_flocking_immune_bridge_struct swarm_flocking_immune_bridge_t;

int swarm_flocking_immune_default_config(swarm_flocking_immune_config_t* config);
swarm_flocking_immune_bridge_t* swarm_flocking_immune_bridge_create(
    const swarm_flocking_immune_config_t* config,
    brain_immune_system_t* immune_system,
    void* swarm_flocking);
void swarm_flocking_immune_bridge_destroy(swarm_flocking_immune_bridge_t* bridge);

int swarm_flocking_immune_apply_cytokine_effects(swarm_flocking_immune_bridge_t* bridge);
int swarm_flocking_immune_apply_inflammation_effects(swarm_flocking_immune_bridge_t* bridge);

int swarm_flocking_immune_trigger_from_fragmentation(swarm_flocking_immune_bridge_t* bridge);
int swarm_flocking_immune_boost_from_formation(swarm_flocking_immune_bridge_t* bridge);

int swarm_flocking_immune_bridge_update(swarm_flocking_immune_bridge_t* bridge, uint64_t delta_ms);

float swarm_flocking_immune_get_alignment_factor(const swarm_flocking_immune_bridge_t* bridge);
float swarm_flocking_immune_get_cohesion_factor(const swarm_flocking_immune_bridge_t* bridge);
float swarm_flocking_immune_get_separation_factor(const swarm_flocking_immune_bridge_t* bridge);
bool swarm_flocking_immune_is_fragmented(const swarm_flocking_immune_bridge_t* bridge);

int swarm_flocking_immune_connect_bio_async(swarm_flocking_immune_bridge_t* bridge);
int swarm_flocking_immune_disconnect_bio_async(swarm_flocking_immune_bridge_t* bridge);
bool swarm_flocking_immune_is_bio_async_connected(const swarm_flocking_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SWARM_FLOCKING_IMMUNE_BRIDGE_H */
