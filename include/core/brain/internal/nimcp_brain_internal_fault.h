//=============================================================================
// nimcp_brain_internal_fault.h - Fault Tolerance Internal Fields
//=============================================================================
/**
 * @file nimcp_brain_internal_fault.h
 * @brief Internal brain_struct fields for fault tolerance and recovery
 *
 * WHAT: Defines brain_struct fields for intelligent error recovery
 * WHY:  Modularize brain_internal.h - separate fault tolerance fields
 * HOW:  Included by nimcp_brain_internal.h to compose brain_struct
 *
 * SUBSYSTEMS:
 * - Recovery Executive: Multi-step recovery planning and execution
 * - Failure Prediction: Proactive fault detection
 * - Metacognitive Monitoring: "Is this working?" self-assessment
 * - Pattern Learning: Learn from past failures
 *
 * BIOLOGICAL BASIS:
 * - Anterior Cingulate Cortex: Error monitoring
 * - Prefrontal Cortex: Recovery planning (executive function)
 * - Hippocampus: Episodic memory of past failures
 *
 * PARIETAL INTEGRATION:
 * - Software Engineering Analysis: Code structure at failure
 * - Pattern Detection: Match against historical failures
 * - Spatial Reasoning: Dependency graph analysis
 * - Mathematical Intuition: Recovery feasibility estimation
 *
 * REFACTORING HISTORY:
 * - Extracted from monolithic nimcp_brain_internal.h (Phase B3.1)
 *
 * @version Phase B3.1: Fault Tolerance Modularization
 * @author NIMCP Development Team
 * @date 2025-12-30
 */

#ifndef NIMCP_BRAIN_INTERNAL_FAULT_H
#define NIMCP_BRAIN_INTERNAL_FAULT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations for Fault Tolerance Types
//=============================================================================

// Recovery Executive (uses internal struct to avoid diagnostics.h conflicts)
struct recovery_executive_internal;

//=============================================================================
// Fault Tolerance Fields for brain_struct
//=============================================================================

/**
 * @brief Macro defining fault tolerance fields for brain_struct
 *
 * USAGE: Include this macro in brain_struct definition
 *
 * CAPABILITIES:
 * - Multi-step recovery planning
 * - Proactive failure prediction
 * - Metacognitive self-assessment
 * - Historical failure pattern learning
 *
 * PARIETAL INTEGRATION:
 * - Uses parietal lobe for software engineering analysis
 * - Pattern detection matches against failure history
 * - Spatial reasoning analyzes dependency graphs
 * - Mathematical intuition estimates recovery feasibility
 */
#define BRAIN_INTERNAL_FIELDS_FAULT                                            \
    /* === FAULT TOLERANCE (Intelligent Recovery) === */                       \
    struct recovery_executive_internal* recovery_executive; /* Recovery planner */ \
    bool fault_tolerance_enabled;               /* Fault tolerance enabled */  \
    uint64_t last_fault_check_us;               /* Last fault check timestamp */

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_INTERNAL_FAULT_H */
