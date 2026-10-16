//=============================================================================
// nimcp_brain_internal_hippocampus.h - Hippocampus Internal Fields
//=============================================================================
/**
 * @file nimcp_brain_internal_hippocampus.h
 * @brief Internal brain_struct fields for hippocampus (memory and navigation)
 *
 * WHAT: Defines brain_struct fields for hippocampus and integration bridges
 * WHY:  Modularize brain_internal.h - separate hippocampus fields for maintainability
 * HOW:  Included by nimcp_brain_internal.h to compose brain_struct
 *
 * BIOLOGICAL BASIS:
 * - CA1, CA3: Pyramidal cells for memory encoding/retrieval
 * - Dentate Gyrus: Pattern separation (sparse coding)
 * - Entorhinal Cortex: Grid cells for path integration
 * - Place cells: Location-specific firing for spatial map
 *
 * INTEGRATION BRIDGES:
 * - Substrate Bridge: ATP/fatigue modulation of memory consolidation
 * - Thalamic Bridge: Anterior nucleus routing of memory signals
 * - Quantum Bridge: Grover-accelerated memory search
 *
 * REFACTORING HISTORY:
 * - Created for Phase H1: Hippocampus Brain Integration
 * - Part of brain header modularization effort
 *
 * @version Phase H1: Hippocampus Brain Integration
 * @author NIMCP Development Team
 * @date 2025-12-30
 */

#ifndef NIMCP_BRAIN_INTERNAL_HIPPOCAMPUS_H
#define NIMCP_BRAIN_INTERNAL_HIPPOCAMPUS_H

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations for Hippocampus Types
//=============================================================================
// Full definitions are in core/brain/regions/hippocampus/ headers

struct hippocampus_adapter;          // Unified hippocampus adapter
struct hippocampus_substrate_bridge; // Metabolic modulation bridge
struct hippocampus_thalamic_bridge;  // Thalamic signal routing bridge
struct hippocampus_quantum_bridge;   // Quantum-accelerated memory bridge

//=============================================================================
// Hippocampus Fields for brain_struct
//=============================================================================
/**
 * HIPPOCAMPUS INTEGRATION (Episodic Memory and Spatial Navigation)
 *
 * The hippocampus provides core memory and navigation capabilities:
 * - Memory Encoding: Convert experiences to storable representations
 * - Memory Retrieval: Content-addressable recall from partial cues
 * - Pattern Separation: Reduce interference between similar memories (DG)
 * - Pattern Completion: Reconstruct full patterns from partial input (CA3)
 * - Spatial Navigation: Place cells and grid cells for cognitive map
 *
 * The hippocampus adapter unifies:
 * - Place Cell Network: Location-specific neural activity
 * - Grid Cell Network: Metric spatial representation
 * - Pattern Separator (DG): Sparse encoding for orthogonalization
 * - Memory Encoder (CA3/CA1): Autoassociative memory storage
 *
 * Integrates with:
 * - Neural Substrate: Metabolic modulation of consolidation
 * - Thalamic Router: Memory signal routing through anterior nucleus
 * - Quantum Reasoner: Grover-accelerated memory search
 * - Cortical Areas: Systems consolidation to neocortex
 * - Amygdala: Emotional memory tagging
 * - Sleep System: Consolidation during sleep stages
 * - Brain Immune System: Neuroinflammation affects plasticity
 * - Training System: Experience-dependent learning
 *
 * FIELD NAMING CONVENTION:
 * - hippocampus_*: Core hippocampus components
 * - hippocampus_*_bridge: Integration bridges to other subsystems
 * - hippocampus_enabled: Master enable flag
 * - last_hippocampus_update_us: Update timestamp for timing control
 */

/**
 * @brief Macro defining hippocampus fields for brain_struct
 *
 * USAGE: Include this macro in brain_struct definition
 * WHY:   Enables modular composition of brain_struct from separate headers
 *
 * FIELDS:
 * - hippocampus: Core adapter for memory encoding/retrieval/navigation
 * - hippocampus_substrate_bridge: Metabolic state integration (ATP, sleep)
 * - hippocampus_thalamic_bridge: Thalamic routing (anterior nucleus -> cortex)
 * - hippocampus_quantum_bridge: Quantum-accelerated memory access
 * - hippocampus_enabled: Master enable flag for hippocampus subsystem
 * - last_hippocampus_update_us: Timestamp for update rate limiting
 */
#define BRAIN_INTERNAL_FIELDS_HIPPOCAMPUS                                       \
    /* === HIPPOCAMPUS INTEGRATION (Episodic Memory and Spatial Navigation) === */ \
    struct hippocampus_adapter* hippocampus;        /* Hippocampus adapter */ \
    struct hippocampus_substrate_bridge* hippocampus_substrate_bridge; /* Substrate metabolic integration */ \
    struct hippocampus_thalamic_bridge* hippocampus_thalamic_bridge;   /* Thalamic signal routing */ \
    struct hippocampus_quantum_bridge* hippocampus_quantum_bridge;     /* Quantum-accelerated memory */ \
    bool hippocampus_enabled;                       /* Hippocampus enabled for this brain */ \
    uint64_t last_hippocampus_update_us;            /* Last hippocampus update timestamp */

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_INTERNAL_HIPPOCAMPUS_H */
