//=============================================================================
// nimcp_brain_validation.h - Brain Creation Validation
//=============================================================================
/**
 * @file nimcp_brain_validation.h
 * @brief Validation and helper functions for brain creation
 *
 * WHAT: Parameter validation and caching helpers for brain factory
 * WHY:  Separates validation logic from initialization and creation
 * HOW:  Provides validators and decision caching for efficiency
 *
 * @version 2.7.0
 * @author NIMCP Development Team
 */

#ifndef NIMCP_BRAIN_VALIDATION_H
#define NIMCP_BRAIN_VALIDATION_H

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Includes
//=============================================================================

#include "core/brain/nimcp_brain.h"
#include <stdbool.h>
#include <stdint.h>

//=============================================================================
// Note: All function declarations are in the main nimcp_brain_factory.h header
// This module contains the implementations for validation and caching functions
//=============================================================================

#ifdef __cplusplus
}
#endif

#endif // NIMCP_BRAIN_VALIDATION_H
