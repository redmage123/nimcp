//=============================================================================
// nimcp_brain_init_perception.h - Perception and Sensory Subsystems
//=============================================================================
/**
 * @file nimcp_brain_init_perception.h
 * @brief Perception and Sensory Subsystems
 *
 * WHAT: Initialization functions for perception subsystems
 * WHY:  SRP refactoring - separate perception initialization logic
 * HOW:  Each function initializes a specific brain subsystem
 *
 * EXTRACTED FROM: nimcp_brain_init_subsystems.c
 * DATE: 2025-12-08
 *
 * @version 2.7.0
 * @author NIMCP Development Team
 */

#ifndef NIMCP_BRAIN_INIT_PERCEPTION_H
#define NIMCP_BRAIN_INIT_PERCEPTION_H

#include "core/brain/nimcp_brain.h"

#ifdef __cplusplus
extern "C" {
#endif

// Perception and Sensory Subsystems initialization functions
bool nimcp_brain_factory_init_glial_subsystem(brain_t brain);
bool nimcp_brain_factory_init_multimodal_subsystems(brain_t brain);
bool nimcp_brain_factory_init_pink_noise_subsystem(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_BRAIN_INIT_PERCEPTION_H
