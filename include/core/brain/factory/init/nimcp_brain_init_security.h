//=============================================================================
// nimcp_brain_init_security.h - Security Subsystem Initialization
//=============================================================================
/**
 * @file nimcp_brain_init_security.h
 * @brief Security subsystem initialization for brain
 *
 * WHAT: Security monitoring, integration, and BBB protection initialization
 * WHY:  Separates security initialization from other subsystems
 * HOW:  Initializes security recovery bridge, security integration (SC-4), and BBB (IS-1)
 *
 * @version 2.7.0
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#ifndef NIMCP_BRAIN_INIT_SECURITY_H
#define NIMCP_BRAIN_INIT_SECURITY_H

#include "core/brain/nimcp_brain.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize security subsystem for brain
 *
 * WHAT: Initializes three security layers:
 *       1. Security Recovery Bridge (Phase SC-2)
 *       2. Universal Security Integration (Phase SC-4)
 *       3. Blood-Brain Barrier (Phase IS-1)
 *
 * WHY:  Provides comprehensive security monitoring and protection
 *
 * HOW:  Creates and configures security modules based on brain config
 *
 * @param brain Brain to initialize security for
 * @return true if initialization successful (or security disabled), false on error
 */
bool nimcp_brain_factory_init_security_subsystem(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_BRAIN_INIT_SECURITY_H
