/**
 * @file nimcp_fep_orchestrator.c
 * @brief FEP Orchestrator Implementation
 * @version 1.0.0
 * @date 2025-12-15
 */

#include "cognitive/free_energy/nimcp_fep_orchestrator.h"
#include "core/brain/nimcp_brain_internal.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/platform/nimcp_platform_time.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(fep_orchestrator, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * String Constants
 * ============================================================================ */

static const char* CATEGORY_NAMES[] = {
    "cognitive",
    "swarm",
    "security",
    "plasticity",
    "middleware",
    "perception",
    "async",
    "glial",
    "core",
    "jepa"
};

static const char* STATE_NAMES[] = {
    "stopped",
    "starting",
    "running",
    "paused",
    "stopping",
    "error"
};


// Forward declarations for static functions (SRP split)
static fep_bridge_entry_t* find_bridge_by_id( fep_orchestrator_t* orchestrator, uint32_t bridge_id );
static bool category_needs_update( const fep_orchestrator_t* orchestrator, fep_bridge_category_t category, uint64_t current_time_ms );
static int update_single_bridge( fep_orchestrator_t* orchestrator, fep_bridge_entry_t* entry, uint64_t current_time_ms );
static float get_effective_interval_ms( const fep_orchestrator_t* orchestrator, fep_bridge_category_t category );

//=============================================================================
// SRP Split: Function implementations organized by responsibility
//=============================================================================
#include "nimcp_fep_orchestrator_part_helpers.c"  // 2 functions: helpers
#include "nimcp_fep_orchestrator_part_processing.c"  // 7 functions: processing
#include "nimcp_fep_orchestrator_part_accessors.c"  // 12 functions: accessors
#include "nimcp_fep_orchestrator_part_lifecycle.c"  // 3 functions: lifecycle
#include "nimcp_fep_orchestrator_part_core.c"  // 15 functions: core
#include "nimcp_fep_orchestrator_part_io.c"  // 2 functions: io
