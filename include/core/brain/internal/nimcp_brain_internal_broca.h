//=============================================================================
// nimcp_brain_internal_broca.h - Broca's Region Internal Fields
//=============================================================================
/**
 * @file nimcp_brain_internal_broca.h
 * @brief Internal brain_struct fields for Broca's region (language production)
 *
 * WHAT: Defines brain_struct fields for Broca's area and integration bridges
 * WHY:  Modularize brain_internal.h - separate Broca fields for maintainability
 * HOW:  Included by nimcp_brain_internal.h to compose brain_struct
 *
 * BIOLOGICAL BASIS:
 * - Brodmann areas 44 (pars opercularis) and 45 (pars triangularis)
 * - Critical for speech production, syntax processing, and motor planning
 * - Damage causes Broca's aphasia (non-fluent, telegraphic speech)
 *
 * INTEGRATION BRIDGES:
 * - Substrate Bridge: ATP/fatigue modulation of speech fluency
 * - Thalamic Bridge: VA/VL routing of motor speech commands
 * - Quantum Bridge: Grover-accelerated lexical search
 *
 * REFACTORING HISTORY:
 * - Extracted from monolithic nimcp_brain_internal.h (Phase B3.1)
 * - Part of ongoing brain header modularization effort
 *
 * @version Phase B3.1: Broca Modularization
 * @author NIMCP Development Team
 * @date 2025-12-30
 */

#ifndef NIMCP_BRAIN_INTERNAL_BROCA_H
#define NIMCP_BRAIN_INTERNAL_BROCA_H

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations for Broca Types
//=============================================================================
// Full definitions are in core/brain/regions/broca/ headers

struct broca_adapter;              // Unified Broca's region adapter
struct broca_substrate_bridge;     // Metabolic modulation bridge
struct broca_thalamic_bridge;      // Thalamic signal routing bridge
struct broca_quantum_bridge;       // Quantum-accelerated language bridge

//=============================================================================
// Broca's Region Fields for brain_struct
//=============================================================================
/**
 * BROCA'S REGION INTEGRATION (Language Production)
 *
 * Broca's Region (BA44/45) provides language production capabilities:
 * - Syntax Processing: Hierarchical phrase structure generation
 * - Phonological Processing: Sound sequence planning
 * - Speech Motor Planning: Articulatory trajectory generation
 * - Working Memory Integration: Lexical access and retrieval
 *
 * The Broca adapter unifies:
 * - Syntax Processor: Grammatical structure generation
 * - Phonological Processor: Phoneme sequence optimization
 * - Speech Motor Planner: Motor command sequencing
 *
 * Integrates with:
 * - Neural Substrate: Metabolic modulation of speech fluency
 * - Thalamic Router: Motor speech routing through VA/VL nuclei
 * - Quantum Reasoner: Grover-accelerated lexical search
 * - Working Memory: Lexical access buffer
 * - Brain Immune System: Inflammation affects fluency
 * - Training System: Language production learning
 *
 * FIELD NAMING CONVENTION:
 * - broca_*: Core Broca's region components
 * - broca_*_bridge: Integration bridges to other subsystems
 * - broca_enabled: Master enable flag
 * - last_broca_update_us: Update timestamp for timing control
 */

/**
 * @brief Macro defining Broca fields for brain_struct
 *
 * USAGE: Include this macro in brain_struct definition
 * WHY:   Enables modular composition of brain_struct from separate headers
 *
 * FIELDS:
 * - broca: Core adapter for syntax/phonological/motor processing
 * - broca_substrate_bridge: Metabolic state integration (ATP, fatigue)
 * - broca_thalamic_bridge: Thalamic routing (VA/VL → motor cortex)
 * - broca_quantum_bridge: Quantum-accelerated lexical access
 * - broca_enabled: Master enable flag for Broca subsystem
 * - last_broca_update_us: Timestamp for update rate limiting
 */
#define BRAIN_INTERNAL_FIELDS_BROCA                                            \
    /* === BROCA'S REGION INTEGRATION (Language Production) === */             \
    struct broca_adapter* broca;                /* Broca's region adapter */   \
    struct broca_substrate_bridge* broca_substrate_bridge;  /* Substrate metabolic integration */ \
    struct broca_thalamic_bridge* broca_thalamic_bridge;    /* Thalamic signal routing */ \
    struct broca_quantum_bridge* broca_quantum_bridge;      /* Quantum-accelerated language */ \
    bool broca_enabled;                         /* Broca's region enabled for this brain */ \
    uint64_t last_broca_update_us;              /* Last Broca update timestamp */

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_INTERNAL_BROCA_H */
