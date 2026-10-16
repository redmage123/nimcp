//=============================================================================
// nimcp_brain_internal_temporal.h - Temporal Cortex Internal Fields
//=============================================================================
/**
 * @file nimcp_brain_internal_temporal.h
 * @brief Internal brain_struct fields for temporal cortex
 *
 * WHAT: Defines brain_struct fields for temporal cortex and integration bridges
 * WHY:  Modularize brain_internal.h - separate temporal fields for maintainability
 * HOW:  Included by nimcp_brain_internal.h to compose brain_struct
 *
 * BIOLOGICAL BASIS:
 * - Superior Temporal Gyrus (STG) for auditory processing
 * - Brodmann areas 41/42 (A1/A2) for primary/secondary auditory cortex
 * - Inferotemporal cortex (IT) for object/face recognition
 * - Anterior temporal lobe for semantic memory/concepts
 *
 * INTEGRATION BRIDGES:
 * - Substrate Bridge: ATP/fatigue modulation of perception accuracy
 * - Thalamic Bridge: MGN routing of auditory signals
 * - Quantum Bridge: Grover-accelerated object/concept search
 *
 * REFACTORING HISTORY:
 * - Created for Phase T1: Temporal Cortex Brain Integration
 * - Part of ongoing brain header modularization effort
 *
 * @version Phase T1: Temporal Cortex Integration
 * @author NIMCP Development Team
 * @date 2025-12-30
 */

#ifndef NIMCP_BRAIN_INTERNAL_TEMPORAL_H
#define NIMCP_BRAIN_INTERNAL_TEMPORAL_H

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations for Temporal Types
//=============================================================================
// Full definitions are in core/brain/regions/temporal/ headers

struct temporal_adapter;               // Unified temporal cortex adapter
struct temporal_substrate_bridge;      // Metabolic modulation bridge
struct temporal_thalamic_bridge;       // Thalamic signal routing bridge
struct temporal_quantum_bridge;        // Quantum-accelerated perception bridge

//=============================================================================
// Temporal Cortex Fields for brain_struct
//=============================================================================
/**
 * TEMPORAL CORTEX INTEGRATION (Auditory, Object Recognition, Semantic Memory)
 *
 * Temporal Cortex provides perception and semantic memory capabilities:
 * - Auditory Processing: A1/A2 spectral analysis, speech detection
 * - Object Recognition: Inferotemporal cortex for object/face ID
 * - Semantic Memory: Concept storage, spreading activation, priming
 * - Multimodal Integration: Audio-visual binding
 *
 * The temporal adapter unifies:
 * - Auditory Processor: Spectral analysis, pitch tracking, onset detection
 * - Object Recognition: Prototype matching, face processing
 * - Semantic Memory Core: Concept network, spreading activation
 *
 * Integrates with:
 * - Neural Substrate: Metabolic modulation of perception accuracy
 * - Thalamic Router: MGN routing of auditory signals
 * - Quantum Reasoner: Grover-accelerated object/concept search
 * - Working Memory: Concept activation and manipulation
 * - Brain Immune System: Inflammation affects perception
 * - Training System: Object recognition and semantic learning
 * - Hippocampus: Semantic memory encoding/retrieval
 * - Frontal Cortex: Top-down attention and goal-directed retrieval
 *
 * FIELD NAMING CONVENTION:
 * - temporal_*: Core temporal cortex components
 * - temporal_*_bridge: Integration bridges to other subsystems
 * - temporal_enabled: Master enable flag
 * - last_temporal_update_us: Update timestamp for timing control
 */

/**
 * @brief Macro defining temporal fields for brain_struct
 *
 * USAGE: Include this macro in brain_struct definition
 * WHY:   Enables modular composition of brain_struct from separate headers
 *
 * FIELDS:
 * - temporal: Core adapter for auditory/object/semantic processing
 * - temporal_substrate_bridge: Metabolic state integration (ATP, fatigue)
 * - temporal_thalamic_bridge: Thalamic routing (MGN -> A1)
 * - temporal_quantum_bridge: Quantum-accelerated perception
 * - temporal_enabled: Master enable flag for temporal subsystem
 * - last_temporal_update_us: Timestamp for update rate limiting
 */
#define BRAIN_INTERNAL_FIELDS_TEMPORAL                                          \
    /* === TEMPORAL CORTEX INTEGRATION (Auditory, Object Recognition, Semantic) === */ \
    struct temporal_adapter* temporal;              /* Temporal cortex adapter */ \
    struct temporal_substrate_bridge* temporal_substrate_bridge;  /* Substrate metabolic integration */ \
    struct temporal_thalamic_bridge* temporal_thalamic_bridge;    /* Thalamic signal routing */ \
    struct temporal_quantum_bridge* temporal_quantum_bridge;      /* Quantum-accelerated perception */ \
    bool temporal_enabled;                          /* Temporal cortex enabled for this brain */ \
    uint64_t last_temporal_update_us;               /* Last temporal update timestamp */

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_INTERNAL_TEMPORAL_H */
