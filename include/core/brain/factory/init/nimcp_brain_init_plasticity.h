//=============================================================================
// nimcp_brain_init_plasticity.h - Plasticity and Training Subsystems
//=============================================================================
/**
 * @file nimcp_brain_init_plasticity.h
 * @brief Plasticity and Training Subsystems
 *
 * WHAT: Initialization functions for plasticity subsystems
 * WHY:  SRP refactoring - separate plasticity initialization logic
 * HOW:  Each function initializes a specific brain subsystem
 *
 * EXTRACTED FROM: nimcp_brain_init_subsystems.c
 * DATE: 2025-12-08
 *
 * @version 2.7.0
 * @author NIMCP Development Team
 */

#ifndef NIMCP_BRAIN_INIT_PLASTICITY_H
#define NIMCP_BRAIN_INIT_PLASTICITY_H

#include "core/brain/nimcp_brain.h"

#ifdef __cplusplus
extern "C" {
#endif

// Plasticity and Training Subsystems initialization functions
bool nimcp_brain_factory_init_homeostatic_plasticity_subsystem(brain_t brain);
bool nimcp_brain_factory_init_dendritic_computation_subsystem(brain_t brain);
bool nimcp_brain_factory_init_biological_predictive_subsystem(brain_t brain);
bool nimcp_brain_factory_init_second_messenger_subsystem(brain_t brain);
bool nimcp_brain_factory_init_training_subsystem(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_BRAIN_INIT_PLASTICITY_H
