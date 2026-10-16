//=============================================================================
// nimcp_brain_init_structural.h - Structural Subsystems
//=============================================================================
/**
 * @file nimcp_brain_init_structural.h
 * @brief Structural Subsystems
 *
 * WHAT: Initialization functions for structural subsystems
 * WHY:  SRP refactoring - separate structural initialization logic
 * HOW:  Each function initializes a specific brain subsystem
 *
 * EXTRACTED FROM: nimcp_brain_init_subsystems.c
 * DATE: 2025-12-08
 *
 * @version 2.7.0
 * @author NIMCP Development Team
 */

#ifndef NIMCP_BRAIN_INIT_STRUCTURAL_H
#define NIMCP_BRAIN_INIT_STRUCTURAL_H

#include "core/brain/nimcp_brain.h"

#ifdef __cplusplus
extern "C" {
#endif

// Structural Subsystems initialization functions
bool nimcp_brain_factory_init_axon_subsystem(brain_t brain);
bool nimcp_brain_factory_init_dendrite_subsystem(brain_t brain);
bool nimcp_brain_factory_init_cortical_columns_subsystem(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_BRAIN_INIT_STRUCTURAL_H
