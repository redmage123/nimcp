/**
 * @file nimcp_brain_internal_parietal_cortex.h
 * @brief Parietal Cortex Internal Fields for brain_struct
 *
 * WHAT: Defines brain_struct fields for parietal cortex region (spatial/sensorimotor)
 * WHY:  Modularize brain_internal.h - separate parietal cortex fields for maintainability
 * HOW:  Included by nimcp_brain_internal.h to compose brain_struct
 *
 * BIOLOGICAL BASIS:
 * - Posterior parietal cortex (Brodmann areas 5, 7, 39, 40)
 * - Primary somatosensory cortex (S1) - areas 3a, 3b, 1, 2
 * - Secondary somatosensory cortex (S2) - area 40
 * - Superior parietal lobule (SPL) - spatial attention
 * - Inferior parietal lobule (IPL) - sensorimotor integration
 * - Intraparietal sulcus (IPS) - reaching, grasping, attention
 *
 * INTEGRATION BRIDGES:
 * - Substrate Bridge: ATP/fatigue modulation of spatial processing
 * - Thalamic Bridge: Pulvinar/LP routing of spatial signals
 * - Quantum Bridge: Superposition for attention allocation
 *
 * NOTE: This is separate from the cognitive parietal_lobe (math/science reasoning).
 * This module provides spatial/sensorimotor processing in the brain regions architecture.
 *
 * @version Phase PC1: Parietal Cortex Brain Integration
 * @author NIMCP Development Team
 * @date 2025-12-30
 */

#ifndef NIMCP_BRAIN_INTERNAL_PARIETAL_CORTEX_H
#define NIMCP_BRAIN_INTERNAL_PARIETAL_CORTEX_H

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Forward Declarations for Parietal Cortex Types
 *===========================================================================*/

/* Full definitions are in core/brain/regions/parietal/ headers */

struct parietal_adapter;              /**< Unified parietal cortex adapter */
struct parietal_substrate_bridge;     /**< Metabolic modulation bridge */
struct parietal_thalamic_bridge;      /**< Thalamic signal routing bridge */
struct parietal_quantum_bridge;       /**< Quantum spatial reasoning bridge */

/*=============================================================================
 * Parietal Cortex Fields for brain_struct
 *===========================================================================*/

/**
 * PARIETAL CORTEX INTEGRATION (Spatial Processing & Sensorimotor)
 *
 * The Parietal Cortex (BA5/7/39/40) provides spatial and sensorimotor processing:
 * - S1/S2 (Primary/Secondary Somatosensory): Touch, proprioception, temperature, pain
 * - SPL (Superior Parietal Lobule): Spatial attention and navigation
 * - IPL (Inferior Parietal Lobule): Sensorimotor integration
 * - IPS (Intraparietal Sulcus): Reaching, grasping, attention
 *
 * The Parietal Cortex adapter unifies:
 * - Somatosensory Processor: Somatotopic mapping, 2-point discrimination
 * - Spatial Attention Processor: Multi-target attention, covert shifts
 * - Sensorimotor Integrator: Coordinate transforms, motor planning
 *
 * Integrates with:
 * - Motor Cortex: Motor plan execution for reaching/grasping
 * - Visual Cortex: Visuospatial attention modulation
 * - Frontal Cortex: Executive control of spatial behavior
 * - Working Memory: Spatial representation maintenance
 * - Thalamic Router: Pulvinar/LP routing of spatial signals
 * - Quantum Reasoner: Superposition for attention allocation
 * - Brain Immune System: Inflammation affects spatial acuity
 * - Training System: Sensorimotor learning
 *
 * FIELD NAMING CONVENTION:
 * - parietal_cortex_*: Core parietal cortex components
 * - parietal_cortex_*_bridge: Integration bridges to other subsystems
 * - parietal_cortex_enabled: Master enable flag
 * - last_parietal_cortex_update_us: Update timestamp for timing control
 *
 * NOTE: This is separate from parietal_lobe_t which handles math/science reasoning.
 * parietal_cortex provides spatial/motor processing in brain regions architecture.
 */

/**
 * @brief Macro defining parietal cortex fields for brain_struct
 *
 * USAGE: Include this macro in brain_struct definition
 * WHY:   Enables modular composition of brain_struct from separate headers
 *
 * FIELDS:
 * - parietal_cortex: Core adapter for somatosensory/spatial/sensorimotor
 * - parietal_cortex_substrate_bridge: Metabolic state integration
 * - parietal_cortex_thalamic_bridge: Thalamic routing (Pulvinar/LP)
 * - parietal_cortex_quantum_bridge: Quantum spatial reasoning
 * - parietal_cortex_enabled: Master enable flag for parietal cortex subsystem
 * - last_parietal_cortex_update_us: Timestamp for update rate limiting
 */
#define BRAIN_INTERNAL_FIELDS_PARIETAL_CORTEX                                  \
    /* === PARIETAL CORTEX INTEGRATION (Spatial Processing & Sensorimotor) === */ \
    struct parietal_adapter* parietal_cortex;                 /* Parietal cortex adapter (S1/S2/SPL/IPL) */ \
    struct parietal_substrate_bridge* parietal_cortex_substrate_bridge;  /* Substrate metabolic integration */ \
    struct parietal_thalamic_bridge* parietal_cortex_thalamic_bridge;    /* Thalamic (Pulvinar/LP) routing */ \
    struct parietal_quantum_bridge* parietal_cortex_quantum_bridge;      /* Quantum spatial reasoning */ \
    bool parietal_cortex_enabled;                             /* Parietal cortex enabled for this brain */ \
    uint64_t last_parietal_cortex_update_us;                  /* Last parietal cortex update timestamp */

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_INTERNAL_PARIETAL_CORTEX_H */
