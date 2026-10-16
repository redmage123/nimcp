//=============================================================================
// nimcp_brain_internal_cerebellum.h - Cerebellum Internal Fields
//=============================================================================
/**
 * @file nimcp_brain_internal_cerebellum.h
 * @brief Internal brain_struct fields for Cerebellum (motor coordination)
 *
 * WHAT: Defines brain_struct fields for Cerebellum and integration bridges
 * WHY:  Modularize brain_internal.h - separate Cerebellum fields for maintainability
 * HOW:  Included by nimcp_brain_internal.h to compose brain_struct
 *
 * BIOLOGICAL BASIS:
 * - "Little brain" at the back of the brainstem
 * - Contains ~50% of brain's neurons in 10% of volume
 * - Critical for motor coordination, timing, and error-based learning
 * - Damage causes ataxia (uncoordinated movements)
 *
 * KEY STRUCTURES:
 * - Granule cells: Sparse coding of mossy fiber inputs (~50B cells)
 * - Purkinje cells: Integrate parallel fibers, inhibit deep nuclei (~15M cells)
 * - Deep nuclei: Dentate (planning), Interposed (execution), Fastigial (balance)
 * - Climbing fibers: Error signals from inferior olive trigger LTD
 *
 * INTEGRATION BRIDGES:
 * - Substrate Bridge: ATP/fatigue modulation of motor precision
 * - Thalamic Bridge: VL routing of motor commands to cortex
 * - Quantum Bridge: Grover-accelerated timing optimization
 *
 * REFACTORING HISTORY:
 * - Created as part of Phase B4: Cerebellum Brain Integration
 * - Follows modular brain header pattern from Phase B3.1
 *
 * @version Phase B4: Cerebellum Brain Integration
 * @author NIMCP Development Team
 * @date 2025-12-30
 */

#ifndef NIMCP_BRAIN_INTERNAL_CEREBELLUM_H
#define NIMCP_BRAIN_INTERNAL_CEREBELLUM_H

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations for Cerebellum Types
//=============================================================================
// Full definitions are in core/brain/regions/cerebellum/ headers

struct cerebellum_adapter;           // Unified Cerebellum adapter
struct cerebellum_substrate_bridge;  // Metabolic modulation bridge
struct cerebellum_thalamic_bridge;   // Thalamic signal routing bridge
struct cerebellum_quantum_bridge;    // Quantum-accelerated timing bridge

//=============================================================================
// Cerebellum Fields for brain_struct
//=============================================================================
/**
 * CEREBELLUM INTEGRATION (Motor Coordination & Timing)
 *
 * The Cerebellum ("little brain") provides motor coordination:
 * - Motor Timing: Precise temporal control of movements
 * - Error-Based Learning: Climbing fiber signals from inferior olive
 * - Forward Models: Predictive motor control
 * - Motor Adaptation: Gain adjustments for motor accuracy
 *
 * BIOLOGICAL BASIS:
 * - Granule cells: Sparse coding of mossy fiber inputs
 * - Purkinje cells: Integrate parallel fiber inputs, inhibit deep nuclei
 * - Deep nuclei: Dentate (planning), Interposed (execution), Fastigial (balance)
 * - Climbing fibers: Error signals from inferior olive trigger LTD
 *
 * The Cerebellum adapter unifies:
 * - Granule Layer: Sparse expansion of motor inputs
 * - Purkinje Layer: Pattern recognition and inhibitory output
 * - Deep Nuclei: Motor command generation
 * - Forward Model: Predictive control
 *
 * Integrates with:
 * - Motor Cortex: Motor command execution
 * - Basal Ganglia: Action selection coordination
 * - Brainstem: Postural control and balance
 * - Thalamic Router: VL nucleus for cortical relay
 * - Quantum Reasoner: Grover-accelerated timing optimization
 * - Training System: Error-based learning
 * - Brain Immune System: Inflammation affects coordination
 *
 * FIELD NAMING CONVENTION:
 * - cerebellum_*: Core Cerebellum components
 * - cerebellum_*_bridge: Integration bridges to other subsystems
 * - cerebellum_enabled: Master enable flag
 * - last_cerebellum_update_us: Update timestamp for timing control
 */

/**
 * @brief Macro defining Cerebellum fields for brain_struct
 *
 * USAGE: Include this macro in brain_struct definition
 * WHY:   Enables modular composition of brain_struct from separate headers
 *
 * FIELDS:
 * - cerebellum: Core adapter for motor coordination
 * - cerebellum_substrate_bridge: Metabolic state integration (ATP, fatigue)
 * - cerebellum_thalamic_bridge: Thalamic routing (VL -> motor cortex)
 * - cerebellum_quantum_bridge: Quantum-accelerated timing optimization
 * - cerebellum_enabled: Master enable flag for Cerebellum subsystem
 * - last_cerebellum_update_us: Timestamp for update rate limiting
 */
#define BRAIN_INTERNAL_FIELDS_CEREBELLUM                                        \
    /* === CEREBELLUM INTEGRATION (Motor Coordination & Timing) === */          \
    struct cerebellum_adapter* cerebellum;            /* Cerebellum adapter */  \
    struct cerebellum_substrate_bridge* cerebellum_substrate_bridge;  /* Substrate metabolic integration */ \
    struct cerebellum_thalamic_bridge* cerebellum_thalamic_bridge;    /* Thalamic signal routing */ \
    struct cerebellum_quantum_bridge* cerebellum_quantum_bridge;      /* Quantum-accelerated timing */ \
    bool cerebellum_enabled;                          /* Cerebellum enabled for this brain */ \
    uint64_t last_cerebellum_update_us;               /* Last Cerebellum update timestamp */

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_INTERNAL_CEREBELLUM_H */
