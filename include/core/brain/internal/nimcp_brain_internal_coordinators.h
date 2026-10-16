//=============================================================================
// nimcp_brain_internal_coordinators.h - System Coordinators Internal Fields
//=============================================================================
/**
 * @file nimcp_brain_internal_coordinators.h
 * @brief Internal brain_struct fields for system-wide coordinators
 *
 * WHAT: Defines brain_struct fields for cross-module coordinators
 * WHY:  Modularize brain_internal.h - separate coordinator fields
 * HOW:  Included by nimcp_brain_internal.h to compose brain_struct
 *
 * COORDINATORS:
 * - Plasticity Coordinator: Unified manager for all plasticity mechanisms
 * - Immune Bridge Coordinator: Central registry for 27+ immune bridges
 * - Cognitive Meta-Controller: Arbitrator for cognitive subsystem resources
 * - Security-Perception Bridge: Sensory threat analysis and defense
 * - Swarm Module Registry: Plugin architecture for swarm behaviors
 *
 * INITIALIZATION ORDER (dependencies):
 * 1. Bio-Async Orchestrator (foundation for messaging) - in bio_async.h
 * 2. Plasticity Coordinator (depends on bio-async)
 * 3. Immune Bridge Coordinator (depends on bio-async, brain immune)
 * 4. Cognitive Meta-Controller (depends on plasticity, working memory, executive)
 * 5. Security-Perception Bridge (depends on BBB, immune, perception cortices)
 * 6. Swarm Module Registry (depends on all above, swarm_brain)
 *
 * REFACTORING HISTORY:
 * - Extracted from monolithic nimcp_brain_internal.h (Phase B3.1)
 *
 * @version Phase B3.1: Coordinators Modularization
 * @author NIMCP Development Team
 * @date 2025-12-30
 */

#ifndef NIMCP_BRAIN_INTERNAL_COORDINATORS_H
#define NIMCP_BRAIN_INTERNAL_COORDINATORS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations for Coordinator Types
//=============================================================================

struct plasticity_coordinator;
struct immune_bridge_coordinator;
struct cognitive_meta_controller;
struct security_perception_bridge;
struct swarm_module_registry;

//=============================================================================
// Coordinator Fields for brain_struct
//=============================================================================

/**
 * @brief Macro defining coordinator fields for brain_struct
 *
 * USAGE: Include this macro in brain_struct definition
 *
 * SUBSYSTEMS:
 * 1. PLASTICITY COORDINATOR
 *    - Unified manager for STDP, BCM, eligibility traces
 *    - Cross-mechanism coordination (Hebbian, anti-Hebbian)
 *    - Bio-async integration for plasticity signals
 *
 * 2. IMMUNE BRIDGE COORDINATOR
 *    - Central registry for 27+ immune bridges
 *    - Cytokine routing across subsystems
 *    - Inflammation cascade management
 *
 * 3. COGNITIVE META-CONTROLLER
 *    - Resource arbitration for cognitive subsystems
 *    - Attention allocation across modules
 *    - Working memory slot management
 *
 * 4. SECURITY-PERCEPTION BRIDGE
 *    - Sensory threat analysis
 *    - Visual/auditory threat detection
 *    - Immune response triggering
 *
 * 5. SWARM MODULE REGISTRY
 *    - Plugin architecture for swarm behaviors
 *    - Dynamic module loading/unloading
 *    - Inter-swarm communication protocols
 */
#define BRAIN_INTERNAL_FIELDS_COORDINATORS                                     \
    /* === PLASTICITY COORDINATOR === */                                       \
    struct plasticity_coordinator* plasticity_coordinator; /* Plasticity manager */ \
    bool plasticity_coordinator_enabled;        /* Plasticity coordinator enabled */ \
                                                                               \
    /* === IMMUNE BRIDGE COORDINATOR === */                                    \
    struct immune_bridge_coordinator* immune_bridge_coordinator; /* Immune bridges */ \
    bool immune_bridge_coordinator_enabled;     /* Immune coordinator enabled */ \
                                                                               \
    /* === COGNITIVE META-CONTROLLER === */                                    \
    struct cognitive_meta_controller* cognitive_meta_controller; /* Resource arbiter */ \
    bool cognitive_meta_controller_enabled;     /* Meta-controller enabled */  \
                                                                               \
    /* === SECURITY-PERCEPTION BRIDGE === */                                   \
    struct security_perception_bridge* security_perception_bridge; /* Threat defense */ \
    bool security_perception_bridge_enabled;    /* Security-perception enabled */ \
                                                                               \
    /* === SWARM MODULE REGISTRY === */                                        \
    struct swarm_module_registry* swarm_module_registry; /* Swarm plugins */   \
    bool swarm_module_registry_enabled;         /* Swarm registry enabled */

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_INTERNAL_COORDINATORS_H */
