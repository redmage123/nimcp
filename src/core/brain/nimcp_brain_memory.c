//=============================================================================
// nimcp_brain_memory.c - Working Memory State Persistence
//=============================================================================
/**
 * @file nimcp_brain_memory.c
 * @brief Working memory save/load operations
 *
 * This module contains approximately 300 lines extracted from nimcp_brain.c:
 * - save_working_memory_state() - Serialize working memory to file
 * - load_working_memory_state() - Deserialize working memory from file
 * - load_working_memory_item() - Load individual memory items
 *
 * @version 1.0.0
 * @date 2025-12-08
 */

#include "core/brain/nimcp_brain_memory.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "api/nimcp_api_exception.h"
#include "utils/logging/nimcp_logging.h"
#include "cognitive/nimcp_working_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdio.h>

#define LOG_MODULE "BRAIN_MEM"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_memory, MESH_ADAPTER_CATEGORY_COGNITIVE)


// NOTE: Implementation functions are currently in nimcp_brain.c
// External declarations for linking
extern bool save_working_memory_state(working_memory_t* wm, FILE* file);
extern bool load_working_memory_item(working_memory_t* wm, FILE* file);
extern bool load_working_memory_state(brain_t brain, FILE* file);
