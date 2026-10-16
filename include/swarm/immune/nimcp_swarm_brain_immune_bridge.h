/**
 * @file nimcp_swarm_brain_immune_bridge.h
 * @brief Swarm Brain-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-18
 *
 * WHAT: Bidirectional integration between brain immune system and swarm brain coordination
 * WHY:  Inflammation affects swarm coordination; swarm stress triggers immune responses
 * HOW:  Cytokines modulate coordination; low coherence triggers inflammation
 *
 * BIOLOGICAL BASIS:
 * - Pro-inflammatory cytokines (IL-1, IL-6, TNF) reduce swarm coordination
 * - Anti-inflammatory IL-10 enhances coordination recovery
 * - Low swarm coherence triggers stress-induced inflammation
 * - High emergence tier releases anti-inflammatory signals
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SWARM_BRAIN_IMMUNE_BRIDGE_H
#define NIMCP_SWARM_BRAIN_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/immune/nimcp_brain_immune.h"
#include "async/nimcp_bio_router.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Cytokine coordination impact factors */
#define CYTOKINE_IL1_COORDINATION_IMPACT      -0.3f
#define CYTOKINE_IL6_COORDINATION_IMPACT      -0.2f
#define CYTOKINE_TNF_COORDINATION_IMPACT      -0.4f
#define CYTOKINE_IL10_COORDINATION_IMPACT      0.3f

/* Inflammation coherence factors */
#define INFLAMMATION_NONE_COHERENCE_FACTOR     1.0f
#define INFLAMMATION_LOCAL_COHERENCE_FACTOR    0.95f
#define INFLAMMATION_REGIONAL_COHERENCE_FACTOR 0.85f
#define INFLAMMATION_SYSTEMIC_COHERENCE_FACTOR 0.70f
#define INFLAMMATION_STORM_COHERENCE_FACTOR    0.50f

/* Swarm stress thresholds */
#define SWARM_STRESS_TRIGGER_THRESHOLD         0.7f
#define SWARM_COHERENCE_BOOST_THRESHOLD        0.8f

typedef struct {
    float il1_deficit;
    float il6_deficit;
    float tnf_deficit;
    float il10_recovery;
    float total_coherence_reduction;
    float consensus_delay_factor;
} cytokine_swarm_effects_t;

typedef struct {
    brain_inflammation_level_t current_level;
    float coherence_factor;
    float fragmentation_risk;
    float consensus_impairment;
} inflammation_swarm_state_t;

typedef struct {
    float coherence_level;
    uint32_t peer_count;
    bool immune_triggered;
    float il10_release;
} swarm_immune_modulation_t;

typedef struct {
    bool enable_cytokine_impairment;
    bool enable_inflammation_effects;
    bool enable_stress_trigger;
    bool enable_cohesion_boost;
    float cytokine_sensitivity;
    float inflammation_sensitivity;
} swarm_brain_immune_config_t;

typedef struct swarm_brain_immune_bridge_struct swarm_brain_immune_bridge_t;

int swarm_brain_immune_default_config(swarm_brain_immune_config_t* config);
swarm_brain_immune_bridge_t* swarm_brain_immune_bridge_create(
    const swarm_brain_immune_config_t* config,
    brain_immune_system_t* immune_system,
    void* swarm_brain);
void swarm_brain_immune_bridge_destroy(swarm_brain_immune_bridge_t* bridge);

int swarm_brain_immune_apply_cytokine_effects(swarm_brain_immune_bridge_t* bridge);
int swarm_brain_immune_apply_inflammation_effects(swarm_brain_immune_bridge_t* bridge);
float swarm_brain_immune_compute_coherence(const swarm_brain_immune_bridge_t* bridge);

int swarm_brain_immune_trigger_from_stress(swarm_brain_immune_bridge_t* bridge);
int swarm_brain_immune_boost_from_cohesion(swarm_brain_immune_bridge_t* bridge);

int swarm_brain_immune_bridge_update(swarm_brain_immune_bridge_t* bridge, uint64_t delta_ms);

int swarm_brain_immune_get_cytokine_effects(const swarm_brain_immune_bridge_t* bridge,
                                             cytokine_swarm_effects_t* effects);
int swarm_brain_immune_get_inflammation_state(const swarm_brain_immune_bridge_t* bridge,
                                               inflammation_swarm_state_t* state);
bool swarm_brain_immune_is_fragmented(const swarm_brain_immune_bridge_t* bridge);
float swarm_brain_immune_get_coherence_factor(const swarm_brain_immune_bridge_t* bridge);

int swarm_brain_immune_connect_bio_async(swarm_brain_immune_bridge_t* bridge);
int swarm_brain_immune_disconnect_bio_async(swarm_brain_immune_bridge_t* bridge);
bool swarm_brain_immune_is_bio_async_connected(const swarm_brain_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SWARM_BRAIN_IMMUNE_BRIDGE_H */
