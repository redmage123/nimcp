/**
 * @file nimcp_integration_superhuman_bridge.h
 * @brief Integration-Superhuman Inter-Layer Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Bridges global integration to superhuman capabilities
 * WHY:  Enhanced perception must integrate with consciousness
 * HOW:  Routes enhanced sensory data to global workspace
 *
 * KEY PATHWAYS:
 * =============
 * Bottom-Up (Integration → Superhuman):
 * - Conscious attention → enhanced perception focus
 * - Global binding → capability coordination
 * - Arousal state → enhancement activation
 *
 * Top-Down (Superhuman → Integration):
 * - Enhanced percepts → global workspace entry
 * - Capability blending → unified experience
 * - Extended sensing → expanded awareness
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_INTEGRATION_SUPERHUMAN_BRIDGE_H
#define NIMCP_INTEGRATION_SUPERHUMAN_BRIDGE_H

#include "integration/core/nimcp_layer_types.h"
#include "integration/core/nimcp_layer_registry.h"
#include "integration/intra/integration_layer/nimcp_integration_intra_coordinator.h"
#include "integration/intra/superhuman/nimcp_superhuman_intra_coordinator.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Bridge message types */
#define INTEG_SUPER_MSG_ENHANCE_FOCUS   (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x00B0)
#define INTEG_SUPER_MSG_CAP_COORDINATE  (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x00B1)
#define INTEG_SUPER_MSG_ENHANCE_ACTIVATE (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x00B2)
#define INTEG_SUPER_MSG_GW_PERCEPT      (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x00B3)
#define INTEG_SUPER_MSG_UNIFIED_EXP     (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x00B4)
#define INTEG_SUPER_MSG_EXPANDED_AWARE  (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x00B5)

typedef struct nimcp_integration_superhuman_bridge_struct* nimcp_integration_superhuman_bridge_t;

typedef struct {
    float enhance_focus_coupling;
    float capability_coordination_strength;
    float enhancement_activation_threshold;
    float gw_percept_coupling;
    float unified_experience_strength;
    float expanded_awareness_coupling;
    uint32_t update_interval_ms;
    bool enable_conscious_enhancement;
    bool enable_logging;
    bool enable_metrics;
} nimcp_integration_superhuman_config_t;

typedef struct {
    float enhance_focus_level;
    float capability_coordination;
    float enhancement_activation;
    float gw_percept_strength;
    float unified_experience;
    float expanded_awareness;
    float bridge_coherence;
    uint64_t bottom_up_messages;
    uint64_t top_down_messages;
} nimcp_integration_superhuman_state_t;

typedef struct {
    uint64_t enhance_focuses;
    uint64_t capability_coordinations;
    uint64_t enhancement_activations;
    uint64_t gw_percepts;
    uint64_t unified_experiences;
    uint64_t expanded_awarenesses;
    float avg_enhancement_level;
    float avg_unified_experience;
} nimcp_integration_superhuman_stats_t;

NIMCP_EXPORT nimcp_integration_superhuman_config_t nimcp_integration_superhuman_default_config(void);
NIMCP_EXPORT nimcp_integration_superhuman_bridge_t nimcp_integration_superhuman_create(const nimcp_integration_superhuman_config_t* config);
NIMCP_EXPORT void nimcp_integration_superhuman_destroy(nimcp_integration_superhuman_bridge_t bridge);
NIMCP_EXPORT nimcp_layer_error_t nimcp_integration_superhuman_init(
    nimcp_integration_superhuman_bridge_t bridge,
    nimcp_layer_registry_t registry,
    nimcp_integration_intra_t integration,
    nimcp_superhuman_intra_t superhuman
);
NIMCP_EXPORT nimcp_layer_error_t nimcp_integration_superhuman_shutdown(nimcp_integration_superhuman_bridge_t bridge);

NIMCP_EXPORT nimcp_layer_error_t nimcp_integration_superhuman_update(nimcp_integration_superhuman_bridge_t bridge, float dt);
NIMCP_EXPORT nimcp_layer_error_t nimcp_integration_superhuman_transfer_bottom_up(nimcp_integration_superhuman_bridge_t bridge, const nimcp_layer_msg_t* msg);
NIMCP_EXPORT nimcp_layer_error_t nimcp_integration_superhuman_transfer_top_down(nimcp_integration_superhuman_bridge_t bridge, const nimcp_layer_msg_t* msg);

NIMCP_EXPORT nimcp_layer_error_t nimcp_integration_superhuman_get_state(nimcp_integration_superhuman_bridge_t bridge, nimcp_integration_superhuman_state_t* state_out);
NIMCP_EXPORT nimcp_layer_error_t nimcp_integration_superhuman_get_stats(nimcp_integration_superhuman_bridge_t bridge, nimcp_integration_superhuman_stats_t* stats_out);
NIMCP_EXPORT float nimcp_integration_superhuman_get_coherence(nimcp_integration_superhuman_bridge_t bridge);
NIMCP_EXPORT nimcp_layer_error_t nimcp_integration_superhuman_reset_stats(nimcp_integration_superhuman_bridge_t bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_INTEGRATION_SUPERHUMAN_BRIDGE_H */
