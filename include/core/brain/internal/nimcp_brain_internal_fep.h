//=============================================================================
// nimcp_brain_internal_fep.h - FEP & Core Directives Internal Fields
//=============================================================================
/**
 * @file nimcp_brain_internal_fep.h
 * @brief Internal brain_struct fields for FEP orchestrator and core directives
 *
 * WHAT: Defines brain_struct fields for free energy principle and ethics
 * WHY:  Modularize brain_internal.h - separate FEP/ethics fields
 * HOW:  Included by nimcp_brain_internal.h to compose brain_struct
 *
 * SUBSYSTEMS:
 * - FEP Orchestrator: Unified coordination of 93+ FEP bridges
 * - Core Directives: Ethical foundation (Asimov's Laws, Golden Rule)
 *
 * THEORETICAL BASIS:
 * - Free Energy Principle (Friston): Minimize prediction error
 * - Asimov's Three Laws: Harm prevention, obedience, self-preservation
 * - Golden Rule: Reciprocity and fairness evaluation
 *
 * REFACTORING HISTORY:
 * - Extracted from monolithic nimcp_brain_internal.h (Phase B3.1)
 *
 * @version Phase B3.1: FEP Modularization
 * @author NIMCP Development Team
 * @date 2025-12-30
 */

#ifndef NIMCP_BRAIN_INTERNAL_FEP_H
#define NIMCP_BRAIN_INTERNAL_FEP_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations for FEP Types
//=============================================================================

// FEP Orchestrator
struct fep_orchestrator;

// Core Directives System
struct core_directives_system;
typedef struct core_directives_system core_directives_system_t;

// Directive Bridges
struct directive_immune_bridge;
typedef struct directive_immune_bridge directive_immune_bridge_t;

struct directive_fep_bridge;
typedef struct directive_fep_bridge directive_fep_bridge_t;

//=============================================================================
// FEP & Core Directives Fields for brain_struct
//=============================================================================

/**
 * @brief Macro defining FEP and ethics fields for brain_struct
 *
 * USAGE: Include this macro in brain_struct definition
 *
 * SUBSYSTEMS:
 * 1. FEP ORCHESTRATOR
 *    - Manages 93+ FEP bridges across 9 categories
 *    - Category-based update intervals (biologically-plausible)
 *    - Bio-async integration for inter-bridge messaging
 *    - Brain immune integration for precision modulation
 *
 * 2. CORE DIRECTIVES (Ethical Foundation)
 *    - Asimov's Three Laws (harm, obedience, self-preservation)
 *    - Golden Rule (reciprocity and fairness)
 *    - Combinatorial Harm Detection (emergent harm)
 *    - All outputs pass through directive evaluation
 *    - Cannot be overridden by higher cognitive functions
 */
#define BRAIN_INTERNAL_FIELDS_FEP                                              \
    /* === FEP ORCHESTRATOR === */                                             \
    struct fep_orchestrator* fep_orchestrator;  /* FEP bridge coordinator */   \
    bool fep_orchestrator_enabled;              /* FEP orchestrator enabled */ \
                                                                               \
    /* === CORE DIRECTIVES (Ethical Foundation) === */                         \
    core_directives_system_t* core_directives;  /* Core ethical directives */  \
    directive_immune_bridge_t* directive_immune_bridge; /* Directives-immune */ \
    directive_fep_bridge_t* directive_fep_bridge; /* Directives-FEP */         \
    bool core_directives_enabled;               /* Core directives enabled */

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_INTERNAL_FEP_H */
