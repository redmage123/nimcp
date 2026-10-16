//=============================================================================
// nimcp_brain_init_memory.h - Memory and Learning Subsystems
//=============================================================================
/**
 * @file nimcp_brain_init_memory.h
 * @brief Memory and Learning Subsystems
 *
 * WHAT: Initialization functions for memory subsystems
 * WHY:  SRP refactoring - separate memory initialization logic
 * HOW:  Each function initializes a specific brain subsystem
 *
 * EXTRACTED FROM: nimcp_brain_init_subsystems.c
 * DATE: 2025-12-08
 *
 * @version 2.7.0
 * @author NIMCP Development Team
 */

#ifndef NIMCP_BRAIN_INIT_MEMORY_H
#define NIMCP_BRAIN_INIT_MEMORY_H

#include "core/brain/nimcp_brain.h"

#ifdef __cplusplus
extern "C" {
#endif

// Memory and Learning Subsystems initialization functions
bool nimcp_brain_factory_init_consolidation_subsystem(brain_t brain);
bool nimcp_brain_factory_init_curiosity_subsystem(brain_t brain);
bool nimcp_brain_factory_init_salience_subsystem(brain_t brain);
bool nimcp_brain_factory_init_autobiographical_memory_subsystem(brain_t brain);
bool nimcp_brain_factory_init_global_workspace_subsystem(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_BRAIN_INIT_MEMORY_H
