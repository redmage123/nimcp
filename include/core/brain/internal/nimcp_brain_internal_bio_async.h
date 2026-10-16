//=============================================================================
// nimcp_brain_internal_bio_async.h - Bio-Async & Immune Internal Fields
//=============================================================================
/**
 * @file nimcp_brain_internal_bio_async.h
 * @brief Internal brain_struct fields for bio-async messaging and immune system
 *
 * WHAT: Defines brain_struct fields for biological async communication
 * WHY:  Modularize brain_internal.h - separate bio-async fields
 * HOW:  Included by nimcp_brain_internal.h to compose brain_struct
 *
 * SUBSYSTEMS:
 * - Bio-Async Messaging: Neuromodulator channels, predictive signaling
 * - Brain Immune System: Unified adaptive defense coordination
 * - Bio-Async Orchestrator: Central coordinator for 200+ modules
 *
 * BIOLOGICAL BASIS:
 * - Neuromodulator diffusion (dopamine, serotonin, etc.)
 * - Cytokine signaling for immune coordination
 * - Inflammation cascades for threat response
 *
 * REFACTORING HISTORY:
 * - Extracted from monolithic nimcp_brain_internal.h (Phase B3.1)
 *
 * @version Phase B3.1: Bio-Async Modularization
 * @author NIMCP Development Team
 * @date 2025-12-30
 */

#ifndef NIMCP_BRAIN_INTERNAL_BIO_ASYNC_H
#define NIMCP_BRAIN_INTERNAL_BIO_ASYNC_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations for Bio-Async Types
//=============================================================================

// Brain Immune System
struct brain_immune_system;
typedef struct brain_immune_system brain_immune_system_t;

// Bio-Async Orchestrator
struct bio_async_orchestrator;

//=============================================================================
// Bio-Async Fields for brain_struct
//=============================================================================

/**
 * @brief Macro defining bio-async fields for brain_struct
 *
 * USAGE: Include this macro in brain_struct definition
 *
 * SUBSYSTEMS:
 * 1. BIO-ASYNC MESSAGING
 *    - Neuromodulator channels (dopamine, serotonin, norepinephrine, ACh)
 *    - Message handlers for brain state queries
 *    - Predictive signal publishing (triggers on prediction errors)
 *    - Decoupled from cognitive modules via bio-router
 *
 * 2. BRAIN IMMUNE SYSTEM
 *    - Antigen presentation: BBB/BFT/swarm threats → immune antigens
 *    - B cells: Antibody production (PLASMA state required)
 *    - T cells: Helper coordination, killer actions
 *    - Cytokines: Cross-module immune signaling
 *    - Inflammation: Hierarchical recovery escalation
 *
 * 3. BIO-ASYNC ORCHESTRATOR
 *    - Central coordinator for 200+ bio-async modules
 *    - Module registration and lifecycle management
 *    - Cross-module message routing
 */
#define BRAIN_INTERNAL_FIELDS_BIO_ASYNC                                        \
    /* === BIO-ASYNC MESSAGING === */                                          \
    void* bio_async_ctx;                        /* brain_bio_async_ctx_t* */   \
    void* bio_async_ctx_handle;                 /* unified_mem_handle_t */     \
    void* bio_async_mem_mgr;                    /* unified_mem_manager_t */    \
    bool bio_async_enabled;                     /* Bio-async messaging enabled */ \
                                                                               \
    /* === BRAIN IMMUNE SYSTEM === */                                          \
    brain_immune_system_t* immune_system;       /* Brain immune coordination */ \
    bool immune_enabled;                        /* Immune system enabled */    \
                                                                               \
    /* === BIO-ASYNC ORCHESTRATOR === */                                       \
    struct bio_async_orchestrator* bio_async_orchestrator; /* Module coordinator */ \
    bool bio_async_orchestrator_enabled;        /* Orchestrator enabled */

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_INTERNAL_BIO_ASYNC_H */
