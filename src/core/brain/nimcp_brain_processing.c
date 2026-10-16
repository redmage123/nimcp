//=============================================================================
// nimcp_brain_processing.c - Brain Processing and Decision Making
//=============================================================================
/**
 * @file nimcp_brain_processing.c
 * @brief Forward pass computation and decision making logic
 *
 * This module contains approximately 1400 lines extracted from nimcp_brain.c:
 * - perform_forward_pass() - Neural network forward propagation
 * - brain_decide() - Decision making and inference
 * - Helper functions for label determination and statistics
 *
 * @version 1.0.0
 * @date 2025-12-08
 */

#include "core/brain/nimcp_brain_processing.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "api/nimcp_api_exception.h"
#include "utils/logging/nimcp_logging.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "plasticity/adaptive/nimcp_adaptive.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "BRAIN_PROC"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_processing, MESH_ADAPTER_CATEGORY_COGNITIVE)


// NOTE: Implementation functions are currently in nimcp_brain.c
// This file provides modular organization and will be fully populated
// in a future migration phase.
