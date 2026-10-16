/**
 * @file nimcp_brain_init_neuropeptide.h
 * @brief Factory initialization for Neuropeptide Subsystem
 * @version 1.0.0
 * @date 2026-03-05
 *
 * WHAT: Declares factory init/destroy for the neuropeptide subsystem
 * WHY:  SRP refactoring - separate neuropeptide initialization from main init
 * HOW:  Creates neuropeptide_system_t, wires to brain, registers bridges
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_BRAIN_INIT_NEUROPEPTIDE_H
#define NIMCP_BRAIN_INIT_NEUROPEPTIDE_H

#include "core/brain/nimcp_brain.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the neuropeptide subsystem for a brain instance
 *
 * Creates a neuropeptide_system_t with default or brain-config-derived
 * parameters and stores it on the brain struct. Idempotent: returns true
 * if already initialized.
 *
 * BIOLOGICAL MOTIVATION:
 * Neuropeptides provide slow, sustained neuromodulation (minutes-hours)
 * that shapes behavioral states: social bonding (oxytocin), stress (CRH),
 * wakefulness (orexin), pain (substance P), satiety (CCK), reward (endorphin).
 *
 * DEPENDENCIES:
 * - Brain must be allocated
 * - No hard dependencies on other subsystems (bridges are optional)
 *
 * @param brain Brain instance to initialize
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_neuropeptide_subsystem(brain_t brain);

/**
 * @brief Destroy the neuropeptide subsystem for a brain instance
 *
 * Frees the neuropeptide_system_t and NULLs the brain pointer.
 * Safe to call on already-destroyed or NULL systems.
 *
 * @param brain Brain instance to clean up
 */
void nimcp_brain_factory_destroy_neuropeptide_subsystem(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_INIT_NEUROPEPTIDE_H */
