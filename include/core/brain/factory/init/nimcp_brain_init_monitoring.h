//=============================================================================
// nimcp_brain_init_monitoring.h - Monitoring and Ethics Subsystems
//=============================================================================
/**
 * @file nimcp_brain_init_monitoring.h
 * @brief Monitoring and Ethics Subsystems
 *
 * WHAT: Initialization functions for monitoring subsystems
 * WHY:  SRP refactoring - separate monitoring initialization logic
 * HOW:  Each function initializes a specific brain subsystem
 *
 * EXTRACTED FROM: nimcp_brain_init_subsystems.c
 * DATE: 2025-12-08
 *
 * @version 2.7.0
 * @author NIMCP Development Team
 */

#ifndef NIMCP_BRAIN_INIT_MONITORING_H
#define NIMCP_BRAIN_INIT_MONITORING_H

#include "core/brain/nimcp_brain.h"

#ifdef __cplusplus
extern "C" {
#endif

// Monitoring and Ethics Subsystems initialization functions
bool nimcp_brain_factory_init_introspection_subsystem(brain_t brain);
bool nimcp_brain_factory_init_connectivity_health_subsystem(brain_t brain);
bool nimcp_brain_factory_init_middleware_controller_subsystem(brain_t brain);
bool nimcp_brain_factory_init_ethics_engine_subsystem(brain_t brain);
bool nimcp_brain_factory_init_empathy_network_subsystem(brain_t brain);
bool nimcp_brain_factory_init_empathetic_response_subsystem(brain_t brain);
bool nimcp_brain_factory_init_self_model_subsystem(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_BRAIN_INIT_MONITORING_H
