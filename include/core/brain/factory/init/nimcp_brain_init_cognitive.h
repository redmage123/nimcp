//=============================================================================
// nimcp_brain_init_cognitive.h - Core Cognitive Subsystems
//=============================================================================
/**
 * @file nimcp_brain_init_cognitive.h
 * @brief Core Cognitive Subsystems
 *
 * WHAT: Initialization functions for cognitive subsystems
 * WHY:  SRP refactoring - separate cognitive initialization logic
 * HOW:  Each function initializes a specific brain subsystem
 *
 * EXTRACTED FROM: nimcp_brain_init_subsystems.c
 * DATE: 2025-12-08
 *
 * @version 2.7.0
 * @author NIMCP Development Team
 */

#ifndef NIMCP_BRAIN_INIT_COGNITIVE_H
#define NIMCP_BRAIN_INIT_COGNITIVE_H

#include "core/brain/nimcp_brain.h"

#ifdef __cplusplus
extern "C" {
#endif

// Core Cognitive Subsystems initialization functions
bool nimcp_brain_factory_init_symbolic_logic_subsystem(brain_t brain);
bool nimcp_brain_factory_init_symbolic_reasoning_subsystem(brain_t brain);
bool nimcp_brain_factory_init_epistemic_subsystem(brain_t brain);
bool nimcp_brain_factory_init_working_memory_subsystem(brain_t brain);
bool nimcp_brain_factory_init_executive_subsystem(brain_t brain);
bool nimcp_brain_factory_init_theory_of_mind_subsystem(brain_t brain);
bool nimcp_brain_factory_init_natural_explanations_subsystem(brain_t brain);
bool nimcp_brain_factory_init_meta_learning_subsystem(brain_t brain);
bool nimcp_brain_factory_init_mental_health_subsystem(brain_t brain);
bool nimcp_brain_factory_init_trauma_resilience(brain_t brain);
bool nimcp_brain_factory_init_predictive_subsystem(brain_t brain);
bool nimcp_brain_factory_init_mirror_neurons(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_BRAIN_INIT_COGNITIVE_H
