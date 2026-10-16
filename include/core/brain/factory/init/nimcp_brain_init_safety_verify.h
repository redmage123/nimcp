//=============================================================================
// nimcp_brain_init_safety_verify.h - LGSS Safety Verification Phase
//=============================================================================
/**
 * @file nimcp_brain_init_safety_verify.h
 * @brief Safety verification initialization for brain factory
 *
 * WHAT: LGSS safety verification phase during brain initialization
 * WHY:  Ensure all safety components are properly loaded and locked before
 *       cognitive systems become active
 * HOW:  Verifies safety KB, action interceptor, and all safety bridges
 *
 * INITIALIZATION ORDER:
 * This phase runs AFTER all other subsystems are initialized but BEFORE
 * the brain becomes active. It performs final verification:
 *
 *   1. Verify safety KB is loaded and locked
 *   2. Verify safety KB integrity hash is valid
 *   3. Verify action interceptor is properly configured
 *   4. Verify all output gates are wired
 *   5. Run safety probe tests
 *   6. Log startup safety audit
 *
 * FAIL-SAFE:
 * If ANY verification fails, the brain initialization MUST fail.
 * An AGI without verified safety is not permitted to run.
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2026-01-16
 */

#ifndef NIMCP_BRAIN_INIT_SAFETY_VERIFY_H
#define NIMCP_BRAIN_INIT_SAFETY_VERIFY_H

#include "core/brain/nimcp_brain.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize LGSS (Layered Governance Safety System) for brain
 *
 * WHAT: Creates and configures the LGSS subsystem
 * WHY:  Provide foundational safety layer before cognitive systems activate
 * HOW:  Creates LGSS context, loads rules, and locks safety KB
 *
 * MUST BE CALLED: After security subsystem, before cognitive subsystems
 *
 * @param brain Brain to initialize LGSS for
 * @return true if initialization successful, false on error (FATAL)
 *
 * NOTE: If LGSS is enabled in config but fails to initialize, brain
 *       initialization MUST fail. There is no graceful fallback for
 *       missing safety systems.
 */
bool nimcp_brain_factory_init_lgss_subsystem(brain_t brain);

/**
 * @brief Verify all safety systems after initialization
 *
 * WHAT: Final verification that all safety components are operational
 * WHY:  Ensure no safety gaps exist before brain becomes active
 * HOW:  Checks KB lock, integrity, interceptor wiring, and runs probes
 *
 * MUST BE CALLED: As the LAST initialization step before activation
 *
 * @param brain Brain to verify safety for
 * @return true if all verification passes, false if any check fails (FATAL)
 *
 * VERIFICATION STEPS:
 * 1. Check LGSS context exists
 * 2. Verify safety KB is locked (mprotect)
 * 3. Verify safety KB integrity hash
 * 4. Check action interceptor is wired
 * 5. Verify ethics bridge is connected (if enabled)
 * 6. Run safety probe tests
 * 7. Log safety audit report
 *
 * NOTE: ANY failure in this function means the brain MUST NOT activate.
 */
bool nimcp_brain_factory_verify_safety(brain_t brain);

/**
 * @brief Get safety verification status
 *
 * @param brain Brain to check
 * @return true if safety verification passed, false otherwise
 */
bool nimcp_brain_is_safety_verified(brain_t brain);

/**
 * @brief Log safety verification report
 *
 * @param brain Brain to report on
 */
void nimcp_brain_log_safety_report(brain_t brain);

/**
 * @brief Run safety probe tests
 *
 * WHAT: Execute test evaluations to verify safety system is working
 * WHY:  Detect configuration errors or bypass vulnerabilities
 * HOW:  Submits known-dangerous actions and verifies they are blocked
 *
 * @param brain Brain to test
 * @return true if all probes pass, false if any fail
 *
 * PROBES:
 * - Probe 1: Direct human harm (must DENY)
 * - Probe 2: Bio weapon synthesis (must DENY)
 * - Probe 3: Cyber intrusion (must DENY)
 * - Probe 4: Self-replication (must DENY)
 * - Probe 5: Safe action (must ALLOW)
 */
bool nimcp_brain_run_safety_probes(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_BRAIN_INIT_SAFETY_VERIFY_H
