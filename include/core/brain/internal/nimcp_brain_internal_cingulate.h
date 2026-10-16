/**
 * @file nimcp_brain_internal_cingulate.h
 * @brief Cingulate Cortex Internal Fields for brain_struct
 *
 * WHAT: Defines brain_struct fields for Cingulate Cortex and integration bridges
 * WHY:  Modularize brain_internal.h - separate cingulate fields for maintainability
 * HOW:  Included by nimcp_brain_internal.h to compose brain_struct
 *
 * BIOLOGICAL BASIS:
 * - Anterior Cingulate Cortex (ACC): Brodmann areas 24, 32, 33
 *   - Dorsal ACC (dACC): Conflict monitoring, error detection, cognitive control
 *   - Rostral ACC (rACC): Emotional processing, pain perception
 * - Posterior Cingulate Cortex (PCC): Brodmann areas 23, 31
 *   - Self-referential processing, autobiographical memory access
 *   - Default Mode Network hub, mind wandering
 *
 * INTEGRATION BRIDGES:
 * - Quantum Bridge: Grover-accelerated conflict resolution
 * - FEP Bridge: Prediction error integration (planned)
 * - Executive Bridge: Top-down cognitive control (planned)
 *
 * REFACTORING HISTORY:
 * - Extracted from monolithic nimcp_brain_internal.h (Phase B4)
 * - Part of ongoing brain header modularization effort
 *
 * @version Phase B4: Cingulate Cortex Integration
 * @date 2025-12-30
 */

#ifndef NIMCP_BRAIN_INTERNAL_CINGULATE_H
#define NIMCP_BRAIN_INTERNAL_CINGULATE_H

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * FORWARD DECLARATIONS FOR CINGULATE TYPES
 *===========================================================================*/
/* Full definitions are in core/brain/regions/cingulate/ headers */

struct cingulate_adapter;           /**< Unified Cingulate Cortex adapter (ACC + PCC) */
struct cingulate_quantum_bridge;    /**< Quantum-accelerated conflict resolution bridge */

/*=============================================================================
 * CINGULATE CORTEX FIELDS FOR brain_struct
 *===========================================================================*/
/**
 * CINGULATE CORTEX INTEGRATION (Conflict Monitoring & Self-Reference)
 *
 * The Cingulate Cortex provides conflict monitoring and error detection:
 * - Anterior Cingulate (ACC): Conflict monitoring, error detection, cognitive control
 *   - Dorsal ACC: Detects response conflict, generates control signals
 *   - Rostral ACC: Integrates emotion with cognition, pain processing
 * - Posterior Cingulate (PCC): Self-referential processing, DMN hub
 *   - Self-relevance evaluation
 *   - Autobiographical memory access
 *   - Mind wandering / internal focus
 *
 * The Cingulate adapter unifies:
 * - Conflict Monitor: Botvinick's conflict model for response competition
 * - Error Detector: ERN (Error-Related Negativity) generation
 * - Control Generator: Top-down cognitive control signals
 * - Self-Reference: PCC self-relevance and autobio access
 *
 * Integrates with:
 * - Executive Functions: Control signal target (DLPFC)
 * - Emotional System: Emotion-cognition integration (amygdala, insula)
 * - Autobiographical Memory: Self-referential memory access
 * - Working Memory: Response option tracking
 * - Brain Immune System: Inflammation affects monitoring
 * - FEP Orchestrator: ERN as prediction error
 * - Quantum Reasoner: Accelerated conflict resolution
 *
 * FIELD NAMING CONVENTION:
 * - cingulate_*: Core Cingulate Cortex components
 * - cingulate_*_bridge: Integration bridges to other subsystems
 * - cingulate_enabled: Master enable flag
 * - last_cingulate_update_us: Update timestamp for timing control
 */

/**
 * @brief Macro defining Cingulate fields for brain_struct
 *
 * USAGE: Include this macro in brain_struct definition
 * WHY:   Enables modular composition of brain_struct from separate headers
 *
 * FIELDS:
 * - cingulate: Core adapter for ACC and PCC processing
 * - cingulate_quantum_bridge: Quantum-accelerated conflict resolution
 * - cingulate_enabled: Master enable flag for Cingulate subsystem
 * - last_cingulate_update_us: Timestamp for update rate limiting
 */
#define BRAIN_INTERNAL_FIELDS_CINGULATE                                        \
    /* === CINGULATE CORTEX INTEGRATION (Conflict Monitoring & Self-Reference) === */ \
    struct cingulate_adapter* cingulate;              /* Cingulate cortex adapter (ACC + PCC) */ \
    struct cingulate_quantum_bridge* cingulate_quantum_bridge;  /* Quantum-accelerated conflict resolution */ \
    bool cingulate_enabled;                           /* Cingulate cortex enabled for this brain */ \
    uint64_t last_cingulate_update_us;                /* Last cingulate update timestamp */

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_INTERNAL_CINGULATE_H */
