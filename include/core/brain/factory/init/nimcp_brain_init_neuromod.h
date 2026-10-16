//=============================================================================
// nimcp_brain_init_neuromod.h - Neuromodulation Subsystems
//=============================================================================
/**
 * @file nimcp_brain_init_neuromod.h
 * @brief Neuromodulation Subsystems
 *
 * WHAT: Initialization functions for neuromod subsystems
 * WHY:  SRP refactoring - separate neuromod initialization logic
 * HOW:  Each function initializes a specific brain subsystem
 *
 * EXTRACTED FROM: nimcp_brain_init_subsystems.c
 * DATE: 2025-12-08
 *
 * @version 2.7.0
 * @author NIMCP Development Team
 */

#ifndef NIMCP_BRAIN_INIT_NEUROMOD_H
#define NIMCP_BRAIN_INIT_NEUROMOD_H

#include "core/brain/nimcp_brain.h"

#ifdef __cplusplus
extern "C" {
#endif

// Neuromodulation Subsystems initialization functions
bool nimcp_brain_factory_init_neuromodulator_system(brain_t brain);
bool nimcp_brain_factory_init_spatial_neuromod_system(brain_t brain);
bool nimcp_brain_factory_init_attention_subsystem(brain_t brain);
bool nimcp_brain_factory_init_brain_regions_subsystem(brain_t brain);

/**
 * @brief Initialize Phase 4 Neuromodulatory Nuclei (LC, VTA, Raphe, Habenula)
 *
 * WHAT: Initializes the specific neuromodulatory nuclei and their intra-coordinator
 * WHY:  Enable fine-grained neuromodulatory control and cross-nuclei coordination
 * HOW:  Creates adapters for each nucleus, wires up the intra-coordinator
 *
 * This function initializes:
 * - Locus Coeruleus (LC): Norepinephrine (NE) - arousal, attention, stress
 * - Ventral Tegmental Area (VTA): Dopamine (DA) - reward, motivation, learning
 * - Raphe Nuclei: Serotonin (5-HT) - mood, impulse control, patience
 * - Habenula: Aversion - disappointment, negative outcomes, avoidance
 * - Neuromodulatory Intra-Coordinator: Cross-nuclei coupling and synchronization
 *
 * DEPENDENCIES:
 * - Brain must be initialized
 * - Neuromodulator system should be initialized first (provides baseline levels)
 * - Bio-async orchestrator for messaging (optional but recommended)
 *
 * @param brain Brain instance to initialize
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_neuromod_nuclei(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_BRAIN_INIT_NEUROMOD_H
