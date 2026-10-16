//=============================================================================
// nimcp_brain_init_plasticity_bridges.c - Plasticity Bridge Initialization
//=============================================================================
/**
 * @file nimcp_brain_init_plasticity_bridges.c
 * @brief Plasticity bridge subsystem initialization for brain factory
 *
 * WHAT: Initialization functions for Phase 7 plasticity bridges
 * WHY:  Connect plasticity mechanisms with higher-level cognitive systems
 * HOW:  Creates and configures bridges, wires to bio-async and immune
 *
 * BRIDGES INITIALIZED:
 * - STDP-Omni: STDP ↔ Omnidirectional inference (forward/backward PE)
 * - STDP-PR: STDP ↔ Prime Resonant memory (resonance/consolidation gating)
 * - Eligibility-PR: Eligibility traces ↔ PR memory (tag-and-capture)
 * - STDP-Quantum: Quantum-inspired STDP learning rate optimization
 *
 * BIOLOGICAL BASIS:
 * - STDP provides Hebbian learning at synaptic level
 * - Omnidirectional inference enables predictive coding
 * - PR memory provides multi-tier consolidation (Z0→Z3)
 * - Eligibility traces enable three-factor learning (pre×post×reward)
 * - Quantum annealing escapes local minima in parameter space
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2026-01-12
 */

//=============================================================================
// Includes
//=============================================================================

#include "core/brain/factory/init/nimcp_brain_init_subsystems.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "plasticity/stdp/nimcp_stdp_omni_bridge.h"
#include "plasticity/stdp/nimcp_stdp_pr_bridge.h"
#include "plasticity/eligibility/nimcp_eligibility_pr_bridge.h"
#include "plasticity/stdp/nimcp_stdp_quantum_bridge.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "BRAIN_INIT_PLASTICITY_BRIDGES"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_plasticity_bridges, MESH_ADAPTER_CATEGORY_SYSTEM)


//=============================================================================
// STDP-Omni Bridge Initialization
//=============================================================================

/**
 * @brief Initialize STDP-Omnidirectional bridge subsystem
 *
 * WHAT: Creates bidirectional bridge between STDP and omnidirectional inference
 * WHY:  PE from omni should modulate STDP, STDP changes should update world model
 * HOW:  Create bridge, connect to bio-async if enabled
 *
 * @param brain Brain instance to initialize
 * @return true on success or non-fatal failure, false on critical error
 */
bool nimcp_brain_factory_init_stdp_omni_bridge_subsystem(brain_t brain) {
    /* Guard clause */
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_brain_factory_init_stdp_omni_bridge_subsystem: brain is NULL");
        return false;
    }

    /* Initialize fields */
    brain->stdp_omni_bridge = NULL;
    brain->stdp_omni_bridge_enabled = false;

    /* Check dependencies - need plasticity coordinator or bio-async */
    bool should_enable = brain->plasticity_coordinator_enabled ||
                         brain->bio_async_enabled;

    if (!should_enable) {
        NIMCP_LOGGING_DEBUG("STDP-Omni bridge skipped (no dependencies)");
        return true;
    }

    /* Create configuration */
    stdp_omni_bridge_config_t config = stdp_omni_bridge_default_config();
    config.enable_bio_async = brain->bio_async_enabled;

    /* Create bridge */
    stdp_omni_bridge_t bridge = stdp_omni_bridge_create(&config);
    if (!bridge) {
        NIMCP_LOGGING_WARN("Failed to create STDP-Omni bridge - "
                          "continuing without STDP-Omni integration");
        return true; /* Non-fatal */
    }

    /* Store in brain */
    brain->stdp_omni_bridge = (struct stdp_omni_bridge*)bridge;
    brain->stdp_omni_bridge_enabled = true;

    NIMCP_LOGGING_INFO("STDP-Omni bridge initialized: "
                       "forward_pe=%s, backward_pe=%s, lateral_pe=%s, "
                       "bio_async=%s",
                       config.enable_forward_pe ? "enabled" : "disabled",
                       config.enable_backward_pe ? "enabled" : "disabled",
                       config.enable_lateral_pe ? "enabled" : "disabled",
                       config.enable_bio_async ? "enabled" : "disabled");

    return true;
}

//=============================================================================
// STDP-PR Bridge Initialization
//=============================================================================

/**
 * @brief Initialize STDP-Prime Resonant bridge subsystem
 *
 * WHAT: Creates bidirectional bridge between STDP and PR memory
 * WHY:  Memory resonance/consolidation should gate STDP plasticity
 * HOW:  Create bridge, configure tier-based modulation
 *
 * @param brain Brain instance to initialize
 * @return true on success or non-fatal failure, false on critical error
 */
bool nimcp_brain_factory_init_stdp_pr_bridge_subsystem(brain_t brain) {
    /* Guard clause */
    if (!brain) {
        NIMCP_LOGGING_ERROR("Null brain in init_stdp_pr_bridge_subsystem");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_stdp_pr_bridge_subsystem: brain is NULL");
        return false;
    }

    /* Initialize fields */
    brain->stdp_pr_bridge = NULL;
    brain->stdp_pr_bridge_enabled = false;

    /* Check dependencies - need PR memory or plasticity coordinator */
    bool should_enable = brain->pr_memory_enabled ||
                         brain->plasticity_coordinator_enabled;

    if (!should_enable) {
        NIMCP_LOGGING_DEBUG("STDP-PR bridge skipped (no dependencies)");
        return true;
    }

    /* Create configuration */
    stdp_pr_bridge_config_t config = stdp_pr_bridge_default_config();
    config.enable_bio_async = brain->bio_async_enabled;
    config.enable_consolidation_gate = true;
    config.enable_tier_modulation = true;
    config.enable_entangle_updates = brain->pr_memory_enabled;

    /* Create bridge */
    stdp_pr_bridge_t bridge = stdp_pr_bridge_create(&config);
    if (!bridge) {
        NIMCP_LOGGING_WARN("Failed to create STDP-PR bridge - "
                          "continuing without STDP-PR integration");
        return true; /* Non-fatal */
    }

    /* Store in brain */
    brain->stdp_pr_bridge = (struct stdp_pr_bridge*)bridge;
    brain->stdp_pr_bridge_enabled = true;

    NIMCP_LOGGING_INFO("STDP-PR bridge initialized: "
                       "consolidation_gate=%s, tier_modulation=%s, "
                       "entanglement=%s, bio_async=%s",
                       config.enable_consolidation_gate ? "enabled" : "disabled",
                       config.enable_tier_modulation ? "enabled" : "disabled",
                       config.enable_entangle_updates ? "enabled" : "disabled",
                       config.enable_bio_async ? "enabled" : "disabled");

    return true;
}

//=============================================================================
// Eligibility-PR Bridge Initialization
//=============================================================================

/**
 * @brief Initialize Eligibility-Prime Resonant bridge subsystem
 *
 * WHAT: Creates bidirectional bridge between eligibility traces and PR memory
 * WHY:  Eligibility traces should gate consolidation, consolidation affects decay
 * HOW:  Create bridge, configure tag-and-capture mechanism
 *
 * BIOLOGICAL BASIS:
 * - Eligibility traces tag synapses for potential consolidation
 * - Reward signal captures tagged synapses (Frey & Morris, 1997)
 * - Consolidated memories retain eligibility longer
 *
 * @param brain Brain instance to initialize
 * @return true on success or non-fatal failure, false on critical error
 */
bool nimcp_brain_factory_init_eligibility_pr_bridge_subsystem(brain_t brain) {
    /* Guard clause */
    if (!brain) {
        NIMCP_LOGGING_ERROR("Null brain in init_eligibility_pr_bridge_subsystem");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_eligibility_pr_bridge_subsystem: brain is NULL");
        return false;
    }

    /* Initialize fields */
    brain->eligibility_pr_bridge = NULL;
    brain->eligibility_pr_bridge_enabled = false;

    /* Check dependencies - need PR memory or plasticity */
    bool should_enable = brain->pr_memory_enabled ||
                         brain->plasticity_coordinator_enabled ||
                         brain->enable_event_driven_plasticity;

    if (!should_enable) {
        NIMCP_LOGGING_DEBUG("Eligibility-PR bridge skipped (no dependencies)");
        return true;
    }

    /* Create configuration */
    elig_pr_bridge_config_t config = elig_pr_bridge_default_config();
    config.enable_bio_async = brain->bio_async_enabled;
    config.enable_consolidation_gate = true;
    config.enable_decay_modulation = true;
    config.enable_tier_modulation = true;
    config.enable_resonance_boost = brain->pr_memory_enabled;

    /* Create bridge */
    elig_pr_bridge_t bridge = elig_pr_bridge_create(&config);
    if (!bridge) {
        NIMCP_LOGGING_WARN("Failed to create Eligibility-PR bridge - "
                          "continuing without eligibility-PR integration");
        return true; /* Non-fatal */
    }

    /* Store in brain */
    brain->eligibility_pr_bridge = (struct elig_pr_bridge_struct*)bridge;
    brain->eligibility_pr_bridge_enabled = true;

    NIMCP_LOGGING_INFO("Eligibility-PR bridge initialized: "
                       "consol_gate=%s, decay_mod=%s, tier_mod=%s, "
                       "resonance_boost=%s, bio_async=%s",
                       config.enable_consolidation_gate ? "enabled" : "disabled",
                       config.enable_decay_modulation ? "enabled" : "disabled",
                       config.enable_tier_modulation ? "enabled" : "disabled",
                       config.enable_resonance_boost ? "enabled" : "disabled",
                       config.enable_bio_async ? "enabled" : "disabled");

    return true;
}

//=============================================================================
// STDP-Quantum Bridge Initialization
//=============================================================================

/**
 * @brief Initialize STDP-Quantum optimization bridge subsystem
 *
 * WHAT: Creates quantum-inspired optimizer for STDP learning rates
 * WHY:  Escape local minima in learning rate space via quantum tunneling
 * HOW:  Create bridge with quantum annealing optimizer
 *
 * QUANTUM CONCEPTS:
 * - Simulated quantum annealing for parameter optimization
 * - Ensemble of candidate parameters with amplitude weighting
 * - Tunneling probability decreases with temperature
 *
 * @param brain Brain instance to initialize
 * @return true on success or non-fatal failure, false on critical error
 */
bool nimcp_brain_factory_init_stdp_quantum_bridge_subsystem(brain_t brain) {
    /* Guard clause */
    if (!brain) {
        NIMCP_LOGGING_ERROR("Null brain in init_stdp_quantum_bridge_subsystem");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_stdp_quantum_bridge_subsystem: brain is NULL");
        return false;
    }

    /* Initialize fields */
    brain->stdp_quantum_bridge = NULL;
    brain->stdp_quantum_bridge_enabled = false;

    /* Check dependencies - need plasticity or training */
    bool should_enable = brain->plasticity_coordinator_enabled ||
                         brain->enable_training_integration ||
                         brain->enable_event_driven_plasticity;

    if (!should_enable) {
        NIMCP_LOGGING_DEBUG("STDP-Quantum bridge skipped (no dependencies)");
        return true;
    }

    /* Create configuration */
    stdp_quantum_config_t config = stdp_quantum_default_config();
    config.enabled = true;
    config.objective = QSTDP_OBJ_STABILITY;  /* Default: minimize weight variance */
    config.schedule = QSTDP_SCHEDULE_EXPONENTIAL;

    /* Create bridge */
    stdp_quantum_bridge_t* bridge = stdp_quantum_bridge_create(&config);
    if (!bridge) {
        NIMCP_LOGGING_WARN("Failed to create STDP-Quantum bridge - "
                          "continuing without quantum optimization");
        return true; /* Non-fatal */
    }

    /* Store in brain */
    brain->stdp_quantum_bridge = (struct stdp_quantum_bridge*)bridge;
    brain->stdp_quantum_bridge_enabled = true;

    NIMCP_LOGGING_INFO("STDP-Quantum bridge initialized: "
                       "objective=%s, schedule=%s",
                       config.objective == QSTDP_OBJ_STABILITY ? "stability" :
                       config.objective == QSTDP_OBJ_BALANCE ? "balance" :
                       config.objective == QSTDP_OBJ_HOMEOSTASIS ? "homeostasis" :
                       "other",
                       config.schedule == QSTDP_SCHEDULE_EXPONENTIAL ? "exponential" :
                       config.schedule == QSTDP_SCHEDULE_LINEAR ? "linear" :
                       "adaptive");

    return true;
}
