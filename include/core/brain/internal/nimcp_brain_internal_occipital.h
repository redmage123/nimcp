//=============================================================================
// nimcp_brain_internal_occipital.h - Occipital Cortex Internal Fields
//=============================================================================
/**
 * @file nimcp_brain_internal_occipital.h
 * @brief Internal brain_struct fields for Occipital Cortex (visual processing)
 *
 * WHAT: Defines brain_struct fields for Occipital cortex and integration bridges
 * WHY:  Modularize brain_internal.h - separate Occipital fields for maintainability
 * HOW:  Included by nimcp_brain_internal.h to compose brain_struct
 *
 * BIOLOGICAL BASIS:
 * - Brodmann areas 17 (V1), 18 (V2), 19 (V3, V4, V5/MT)
 * - Primary visual cortex: edge detection, orientation selectivity
 * - Visual hierarchy: V1 -> V2 -> V3 -> (V4 | V5/MT)
 * - Dorsal "where" stream: motion and spatial processing
 * - Ventral "what" stream: object and color processing
 *
 * INTEGRATION BRIDGES:
 * - Substrate Bridge: ATP/fatigue modulation of visual processing
 * - Thalamic Bridge: LGN routing of visual input signals
 * - Quantum Bridge: Grover-accelerated visual search
 *
 * REFACTORING HISTORY:
 * - Created as part of Phase O1: Occipital Cortex Brain Integration
 * - Part of ongoing brain header modularization effort
 *
 * @version Phase O1: Occipital Cortex Modularization
 * @author NIMCP Development Team
 * @date 2025-12-30
 */

#ifndef NIMCP_BRAIN_INTERNAL_OCCIPITAL_H
#define NIMCP_BRAIN_INTERNAL_OCCIPITAL_H

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations for Occipital Types
//=============================================================================
// Full definitions are in core/brain/regions/occipital/ headers

struct occipital_adapter;           // Unified Occipital cortex adapter (V1-V5)
struct occipital_substrate_bridge;  // Metabolic modulation bridge
struct occipital_thalamic_bridge;   // Thalamic (LGN) signal routing bridge
struct occipital_quantum_bridge;    // Quantum-accelerated visual search bridge

//=============================================================================
// Occipital Cortex Fields for brain_struct
//=============================================================================
/**
 * OCCIPITAL CORTEX INTEGRATION (Visual Processing)
 *
 * Occipital Cortex (BA17/18/19) provides visual processing capabilities:
 * - V1 (Primary Visual Cortex): Edge detection, orientation selectivity
 * - V2 (Secondary Visual Cortex): Contour integration, texture processing
 * - V3 (Tertiary Visual Cortex): Dynamic form processing
 * - V4 (Color/Form Area): Color constancy, complex form processing
 * - V5/MT (Motion Area): Motion detection, optic flow
 *
 * The Occipital adapter unifies:
 * - V1 Processor: Gabor filter bank, contrast normalization
 * - V2 Processor: Association field, contour linking
 * - V4 Processor: Color constancy, complex shape primitives
 * - V5/MT Processor: Motion energy, optic flow computation
 *
 * Integrates with:
 * - Neural Substrate: Metabolic modulation of visual sensitivity
 * - Thalamic Router: LGN visual input routing
 * - Quantum Reasoner: Grover-accelerated visual search
 * - Parietal Cortex: Dorsal "where" stream (motion -> spatial)
 * - Temporal Cortex: Ventral "what" stream (form -> object)
 * - Dragonfly System: Target tracking and interception
 * - Brain Immune System: Inflammation affects visual acuity
 * - Training System: Visual learning and adaptation
 *
 * FIELD NAMING CONVENTION:
 * - occipital_*: Core Occipital cortex components
 * - occipital_*_bridge: Integration bridges to other subsystems
 * - occipital_enabled: Master enable flag
 * - last_occipital_update_us: Update timestamp for timing control
 */

/**
 * @brief Macro defining Occipital fields for brain_struct
 *
 * USAGE: Include this macro in brain_struct definition
 * WHY:   Enables modular composition of brain_struct from separate headers
 *
 * FIELDS:
 * - occipital: Core adapter for V1-V5 visual processing
 * - occipital_substrate_bridge: Metabolic state integration (ATP, fatigue)
 * - occipital_thalamic_bridge: Thalamic routing (LGN -> V1)
 * - occipital_quantum_bridge: Quantum-accelerated visual search
 * - occipital_enabled: Master enable flag for Occipital subsystem
 * - last_occipital_update_us: Timestamp for update rate limiting
 */
#define BRAIN_INTERNAL_FIELDS_OCCIPITAL                                         \
    /* === OCCIPITAL CORTEX INTEGRATION (Visual Processing) === */              \
    struct occipital_adapter* occipital;           /* Occipital cortex adapter (V1-V5) */ \
    struct occipital_substrate_bridge* occipital_substrate_bridge;  /* Substrate metabolic integration */ \
    struct occipital_thalamic_bridge* occipital_thalamic_bridge;    /* Thalamic (LGN) signal routing */ \
    struct occipital_quantum_bridge* occipital_quantum_bridge;      /* Quantum-accelerated visual search */ \
    bool occipital_enabled;                        /* Occipital cortex enabled for this brain */ \
    uint64_t last_occipital_update_us;             /* Last Occipital update timestamp */

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_INTERNAL_OCCIPITAL_H */
