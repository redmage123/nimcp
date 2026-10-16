//=============================================================================
// nimcp_brain_internal_security.h - Security Systems Internal Fields
//=============================================================================
/**
 * @file nimcp_brain_internal_security.h
 * @brief Internal brain_struct fields for security systems
 *
 * WHAT: Defines brain_struct fields for security and defense
 * WHY:  Modularize brain_internal.h - separate security fields
 * HOW:  Included by nimcp_brain_internal.h to compose brain_struct
 *
 * SECURITY SYSTEMS:
 * - Blood-Brain Barrier (BBB): Perimeter defense for input validation
 * - Security Integration: Universal security framework (SC-4)
 * - Security-Fault Bridge: Security-recovery integration (SC-2)
 *
 * CAPABILITIES:
 * - Input Gate: Validates and sanitizes external inputs
 * - Code Signing: Verifies integrity of loaded weights/models
 * - Memory Boundary: Protects critical memory regions
 * - Access Control: Role-based access to brain operations
 * - Entropy Monitoring: Detect tampering via Shannon entropy
 * - Trust Management: Bayesian trust propagation
 * - Differential Privacy: Privacy-preserving statistics
 *
 * REFACTORING HISTORY:
 * - Extracted from monolithic nimcp_brain_internal.h (Phase B3.1)
 *
 * @version Phase B3.1: Security Modularization
 * @author NIMCP Development Team
 * @date 2025-12-30
 */

#ifndef NIMCP_BRAIN_INTERNAL_SECURITY_H
#define NIMCP_BRAIN_INTERNAL_SECURITY_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations for Security Types
//=============================================================================

// BBB System
struct bbb_system_struct;
typedef struct bbb_system_struct* bbb_system_t;

// Security Integration (Phase SC-4)
struct nimcp_sec_integration;
typedef struct nimcp_sec_integration nimcp_sec_integration_t;

//=============================================================================
// Security Fields for brain_struct
//=============================================================================

/**
 * @brief Macro defining security fields for brain_struct
 *
 * USAGE: Include this macro in brain_struct definition
 *
 * SUBSYSTEMS:
 * 1. SECURITY-FAULT TOLERANCE BRIDGE (SC-2)
 *    - Security Coverage: Protected memory regions with hash verification
 *    - Fractal Security: Hierarchical integrity checking (Merkle tree)
 *    - CFI/Shadow Stack: Control flow protection against ROP/JOP
 *    - Fast Recovery: Sub-millisecond repair for common errors
 *
 * 2. UNIVERSAL SECURITY INTEGRATION (SC-4)
 *    - Entropy monitoring: Shannon entropy analysis for tampering
 *    - Trust management: Bayesian trust propagation
 *    - Differential privacy: Privacy-preserving queries
 *    - Event system: Security event propagation
 *
 * 3. BLOOD-BRAIN BARRIER (IS-1)
 *    - Input Gate: External input validation
 *    - Code Signing: Weight/model integrity
 *    - Memory Boundary: Critical region protection
 *    - Access Control: Role-based operation access
 */
#define BRAIN_INTERNAL_FIELDS_SECURITY                                         \
    /* === SECURITY-FAULT TOLERANCE BRIDGE (SC-2) === */                       \
    void* security_bridge;                      /* nimcp_security_recovery_bridge_t* */ \
    bool enable_security_monitoring;            /* Enable security-fault integration */ \
    uint32_t security_check_interval_ms;        /* Verification cycle interval */ \
    uint64_t last_security_check_ms;            /* Last verification timestamp */ \
                                                                               \
    /* === UNIVERSAL SECURITY INTEGRATION (SC-4) === */                        \
    nimcp_sec_integration_t* security_integration; /* Global security context */ \
    uint32_t sec_module_id;                     /* Brain's module ID in security */ \
    uint32_t* sec_region_ids;                   /* Monitored memory region IDs */ \
    uint32_t num_sec_regions;                   /* Number of monitored regions */ \
    bool enable_security_integration;           /* Enable Phase SC-4 security */ \
                                                                               \
    /* === BLOOD-BRAIN BARRIER (IS-1) === */                                   \
    bbb_system_t bbb_system;                    /* Global BBB system reference */ \
    uint32_t bbb_memory_region_id;              /* BBB memory region registration */ \
    uint32_t bbb_subject_id;                    /* BBB access control subject ID */ \
    bool bbb_enabled;                           /* BBB protection enabled */

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_INTERNAL_SECURITY_H */
