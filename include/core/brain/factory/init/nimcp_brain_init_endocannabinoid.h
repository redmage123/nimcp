/**
 * @file nimcp_brain_init_endocannabinoid.h
 * @brief Factory initialization for the Endocannabinoid System (ECS) subsystem
 * @date 2026-03-05
 *
 * WHAT: Declaration of ECS factory init/destroy functions
 * WHY:  SRP refactoring - separate ECS initialization logic
 * HOW:  Called during brain factory init to wire ECS into the brain
 */

#ifndef NIMCP_BRAIN_INIT_ENDOCANNABINOID_H
#define NIMCP_BRAIN_INIT_ENDOCANNABINOID_H

#include "core/brain/nimcp_brain.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the Endocannabinoid System subsystem on a brain instance
 *
 * Creates the ECS with default configuration and wires it into the brain.
 * Idempotent: returns true immediately if already initialized.
 *
 * BIOLOGICAL MOTIVATION:
 * - Retrograde synaptic signaling (2-AG, anandamide)
 * - DSI/DSE for homeostatic synaptic regulation
 * - Pain modulation via CB1/CB2
 * - Appetite regulation via hypothalamic CB1
 * - Anti-inflammatory via peripheral CB2
 *
 * @param brain Brain instance (internal struct pointer)
 * @return true on success or already initialized, false on critical error
 */
bool nimcp_brain_factory_init_endocannabinoid_subsystem(brain_t brain);

/**
 * @brief Destroy the Endocannabinoid System subsystem on a brain instance
 *
 * Frees ECS resources and NULLs the pointer on the brain struct.
 * Safe to call if ECS was never initialized.
 *
 * @param brain Brain instance (internal struct pointer)
 */
void nimcp_brain_factory_destroy_endocannabinoid_subsystem(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_INIT_ENDOCANNABINOID_H */
