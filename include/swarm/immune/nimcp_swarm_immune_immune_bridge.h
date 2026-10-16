/**
 * @file nimcp_swarm_immune_immune_bridge.h
 * @brief Swarm Immune-Brain Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-18
 *
 * WHAT: Bidirectional integration between brain immune system and swarm immune system
 * WHY:  Coordinate immune responses across swarm and brain layers
 * HOW:  Brain inflammation affects swarm threat detection; swarm threats trigger brain immune
 *
 * BIOLOGICAL BASIS:
 * - Brain inflammation modulates swarm threat detection sensitivity
 * - Swarm-detected threats trigger brain immune antigen presentation
 * - Coordinated immune memory across both systems
 * - Cross-reactive immunity between swarm and brain patterns
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SWARM_IMMUNE_IMMUNE_BRIDGE_H
#define NIMCP_SWARM_IMMUNE_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/immune/nimcp_brain_immune.h"
#include "swarm/nimcp_swarm_immune.h"
#include "async/nimcp_bio_router.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Brain inflammation swarm immune factors */
#define BRAIN_INFLAMMATION_NONE_DETECTION     1.0f
#define BRAIN_INFLAMMATION_LOCAL_DETECTION    0.95f
#define BRAIN_INFLAMMATION_REGIONAL_DETECTION 0.85f
#define BRAIN_INFLAMMATION_SYSTEMIC_DETECTION 0.70f
#define BRAIN_INFLAMMATION_STORM_DETECTION    0.50f

/* Swarm threat brain immune trigger thresholds */
#define SWARM_THREAT_MINOR_SEVERITY           0.3f
#define SWARM_THREAT_MODERATE_SEVERITY        0.5f
#define SWARM_THREAT_SEVERE_SEVERITY          0.7f
#define SWARM_THREAT_CRITICAL_SEVERITY        0.9f

typedef struct {
    float detection_sensitivity_factor;
    float response_intensity_factor;
    float memory_consolidation_factor;
} brain_to_swarm_immune_effects_t;

typedef struct {
    float threat_presentation_rate;
    float antigen_severity;
    uint32_t cross_reactive_matches;
} swarm_to_brain_immune_effects_t;

typedef struct {
    brain_inflammation_level_t brain_level;
    float swarm_threat_level;
    bool coordinated_response_active;
    float cross_immunity_strength;
} immune_coordination_state_t;

typedef struct {
    bool enable_brain_to_swarm;
    bool enable_swarm_to_brain;
    bool enable_cross_immunity;
    bool enable_coordinated_response;
    float coordination_strength;
} swarm_immune_immune_config_t;

typedef struct swarm_immune_immune_bridge_struct swarm_immune_immune_bridge_t;

int swarm_immune_immune_default_config(swarm_immune_immune_config_t* config);
swarm_immune_immune_bridge_t* swarm_immune_immune_bridge_create(
    const swarm_immune_immune_config_t* config,
    brain_immune_system_t* brain_immune,
    NimcpSwarmImmuneSystem* swarm_immune);
void swarm_immune_immune_bridge_destroy(swarm_immune_immune_bridge_t* bridge);

int swarm_immune_immune_apply_brain_effects(swarm_immune_immune_bridge_t* bridge);
int swarm_immune_immune_apply_swarm_effects(swarm_immune_immune_bridge_t* bridge);

int swarm_immune_immune_present_swarm_threat(swarm_immune_immune_bridge_t* bridge,
                                              const uint8_t* pattern, size_t len, float severity);
int swarm_immune_immune_coordinate_response(swarm_immune_immune_bridge_t* bridge);

int swarm_immune_immune_bridge_update(swarm_immune_immune_bridge_t* bridge, uint64_t delta_ms);

float swarm_immune_immune_get_detection_factor(const swarm_immune_immune_bridge_t* bridge);
bool swarm_immune_immune_is_coordinated(const swarm_immune_immune_bridge_t* bridge);

int swarm_immune_immune_connect_bio_async(swarm_immune_immune_bridge_t* bridge);
int swarm_immune_immune_disconnect_bio_async(swarm_immune_immune_bridge_t* bridge);
bool swarm_immune_immune_is_bio_async_connected(const swarm_immune_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SWARM_IMMUNE_IMMUNE_BRIDGE_H */
