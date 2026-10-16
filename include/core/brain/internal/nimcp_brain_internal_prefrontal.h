//=============================================================================
// nimcp_brain_internal_prefrontal.h - Prefrontal Cortex Internal Fields
//=============================================================================
/**
 * @file nimcp_brain_internal_prefrontal.h
 * @brief Internal brain_struct fields for Prefrontal Cortex (executive functions)
 *
 * WHAT: Defines brain_struct fields for prefrontal cortex and integration bridges
 * WHY:  Modularize brain_internal.h - separate prefrontal fields for maintainability
 * HOW:  Defines macro BRAIN_INTERNAL_FIELDS_PREFRONTAL for inclusion in brain_struct
 *
 * BIOLOGICAL BASIS:
 * - Brodmann areas 9, 10, 11, 44, 45, 46, 47 (Prefrontal Cortex)
 * - Dorsolateral PFC: Working memory, cognitive control
 * - Ventromedial PFC: Decision-making, value-based choice
 * - Orbitofrontal Cortex: Reward processing, impulse control
 * - Anterior Cingulate: Conflict monitoring, error detection
 *
 * EXECUTIVE FUNCTIONS:
 * - Goal maintenance and monitoring
 * - Planning and sequencing
 * - Decision-making
 * - Inhibitory control
 * - Cognitive flexibility
 *
 * @version Phase PFC-1: Prefrontal Cortex Integration
 * @author NIMCP Development Team
 * @date 2025-12-30
 */

#ifndef NIMCP_BRAIN_INTERNAL_PREFRONTAL_H
#define NIMCP_BRAIN_INTERNAL_PREFRONTAL_H

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations for Prefrontal Types
//=============================================================================
// Full definitions are in core/brain/regions/prefrontal/ headers

struct prefrontal_adapter;              // Unified prefrontal cortex adapter
struct prefrontal_substrate_bridge;     // Metabolic modulation bridge
struct prefrontal_thalamic_bridge;      // Thalamic signal routing bridge
struct prefrontal_quantum_bridge;       // Quantum-accelerated decision bridge

//=============================================================================
// Prefrontal Cortex Fields for brain_struct
//=============================================================================
/**
 * PREFRONTAL CORTEX INTEGRATION (Executive Functions)
 *
 * The Prefrontal Cortex (BA9/10/11/44/45/46/47) provides executive capabilities:
 * - Goal Maintenance: Active representation of objectives
 * - Planning: Multi-step action sequence generation
 * - Decision-Making: Value-based choice between alternatives
 * - Inhibitory Control: Suppression of inappropriate responses
 * - Cognitive Flexibility: Task switching and rule adaptation
 *
 * The Prefrontal adapter unifies:
 * - Dorsolateral PFC: Working memory and cognitive control
 * - Ventromedial PFC: Value-based decision-making
 * - Orbitofrontal Cortex: Reward processing and impulse control
 *
 * Integration bridges connect to:
 * - Neural Substrate: Metabolic modulation of executive capacity
 * - Thalamic Router: Signal routing through MD nucleus
 * - Quantum Reasoner: Accelerated decision-making and planning
 * - Working Memory: Goal and context maintenance
 * - Basal Ganglia: Action selection and motor planning
 * - Immune System: Inflammation modulates cognition
 *
 * FIELD NAMING CONVENTION:
 * - prefrontal_*: Core prefrontal cortex components
 * - prefrontal_*_bridge: Integration bridges to other subsystems
 * - prefrontal_enabled: Master enable flag
 * - last_prefrontal_update_us: Update timestamp for timing control
 */

/**
 * @brief Macro defining Prefrontal fields for brain_struct
 *
 * This macro can be used to include prefrontal fields in brain_struct.
 * Currently, the fields are also defined directly in brain_internal.h
 * for backward compatibility.
 *
 * FIELDS:
 * - prefrontal: Core adapter for executive functions
 * - prefrontal_substrate_bridge: Metabolic state integration (ATP, fatigue)
 * - prefrontal_thalamic_bridge: Thalamic routing (MD -> cortex loop)
 * - prefrontal_quantum_bridge: Quantum-accelerated decision-making
 * - prefrontal_enabled: Master enable flag for prefrontal subsystem
 * - last_prefrontal_update_us: Timestamp for update rate limiting
 */
#define BRAIN_INTERNAL_FIELDS_PREFRONTAL                                        \
    /* === PREFRONTAL CORTEX INTEGRATION (Executive Functions) === */           \
    struct prefrontal_adapter* prefrontal;                /* Prefrontal cortex adapter */ \
    struct prefrontal_substrate_bridge* prefrontal_substrate_bridge;  /* Substrate metabolic integration */ \
    struct prefrontal_thalamic_bridge* prefrontal_thalamic_bridge;    /* Thalamic signal routing */ \
    struct prefrontal_quantum_bridge* prefrontal_quantum_bridge;      /* Quantum-accelerated decisions */ \
    bool prefrontal_enabled;                              /* Prefrontal enabled for this brain */ \
    uint64_t last_prefrontal_update_us;                   /* Last prefrontal update timestamp */

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_INTERNAL_PREFRONTAL_H */
