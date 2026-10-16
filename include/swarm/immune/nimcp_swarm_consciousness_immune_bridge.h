/**
 * @file nimcp_swarm_consciousness_immune_bridge.h
 * @brief Swarm Consciousness-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-18
 *
 * WHAT: Bidirectional integration between brain immune system and swarm consciousness
 * WHY:  Inflammation reduces collective phi; low consciousness triggers immune
 * HOW:  Cytokines reduce phi integration; consciousness fragmentation triggers stress
 *
 * BIOLOGICAL BASIS:
 * - Pro-inflammatory cytokines reduce collective phi and integration
 * - Inflammation narrows awareness and reduces network coherence
 * - Low phi state triggers stress-induced inflammation
 * - High phi (transcendent state) releases anti-inflammatory signals
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SWARM_CONSCIOUSNESS_IMMUNE_BRIDGE_H
#define NIMCP_SWARM_CONSCIOUSNESS_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/immune/nimcp_brain_immune.h"
#include "async/nimcp_bio_router.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Cytokine consciousness impact factors */
#define CYTOKINE_IL1_PHI_SUPPRESSION          -0.25f
#define CYTOKINE_IL6_PHI_SUPPRESSION          -0.15f
#define CYTOKINE_TNF_PHI_SUPPRESSION          -0.35f
#define CYTOKINE_IL10_PHI_BOOST                0.2f

/* Inflammation consciousness factors */
#define INFLAMMATION_NONE_PHI_FACTOR          1.0f
#define INFLAMMATION_LOCAL_PHI_FACTOR         0.95f
#define INFLAMMATION_REGIONAL_PHI_FACTOR      0.85f
#define INFLAMMATION_SYSTEMIC_PHI_FACTOR      0.65f
#define INFLAMMATION_STORM_PHI_FACTOR         0.40f

typedef struct {
    float phi_suppression;
    float integration_reduction;
    float awareness_narrowing;
} cytokine_consciousness_effects_t;

typedef struct {
    brain_inflammation_level_t current_level;
    float phi_factor;
    float integration_factor;
    bool awareness_fragmented;
} inflammation_consciousness_state_t;

typedef struct {
    float current_phi;
    bool low_phi_triggered;
    float il10_from_phi;
} consciousness_immune_modulation_t;

typedef struct {
    bool enable_cytokine_effects;
    bool enable_inflammation_effects;
    bool enable_low_phi_stress;
    bool enable_high_phi_boost;
    float cytokine_sensitivity;
} swarm_consciousness_immune_config_t;

typedef struct swarm_consciousness_immune_bridge_struct swarm_consciousness_immune_bridge_t;

int swarm_consciousness_immune_default_config(swarm_consciousness_immune_config_t* config);
swarm_consciousness_immune_bridge_t* swarm_consciousness_immune_bridge_create(
    const swarm_consciousness_immune_config_t* config,
    brain_immune_system_t* immune_system,
    void* swarm_consciousness);
void swarm_consciousness_immune_bridge_destroy(swarm_consciousness_immune_bridge_t* bridge);

int swarm_consciousness_immune_apply_cytokine_effects(swarm_consciousness_immune_bridge_t* bridge);
int swarm_consciousness_immune_apply_inflammation_effects(swarm_consciousness_immune_bridge_t* bridge);

int swarm_consciousness_immune_trigger_from_low_phi(swarm_consciousness_immune_bridge_t* bridge);
int swarm_consciousness_immune_boost_from_high_phi(swarm_consciousness_immune_bridge_t* bridge);

int swarm_consciousness_immune_bridge_update(swarm_consciousness_immune_bridge_t* bridge, uint64_t delta_ms);

float swarm_consciousness_immune_get_phi_factor(const swarm_consciousness_immune_bridge_t* bridge);
float swarm_consciousness_immune_get_integration_factor(const swarm_consciousness_immune_bridge_t* bridge);
bool swarm_consciousness_immune_is_fragmented(const swarm_consciousness_immune_bridge_t* bridge);

int swarm_consciousness_immune_connect_bio_async(swarm_consciousness_immune_bridge_t* bridge);
int swarm_consciousness_immune_disconnect_bio_async(swarm_consciousness_immune_bridge_t* bridge);
bool swarm_consciousness_immune_is_bio_async_connected(const swarm_consciousness_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SWARM_CONSCIOUSNESS_IMMUNE_BRIDGE_H */
