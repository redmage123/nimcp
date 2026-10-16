//=============================================================================
// nimcp_brain_core.c - Brain Core Allocation and Initialization
//=============================================================================
/**
 * @file nimcp_brain_core.c
 * @brief Core brain allocation, network creation, and subsystem initialization
 *
 * This module contains approximately 2200 lines extracted from nimcp_brain.c:
 * - allocate_brain() - Brain structure allocation
 * - create_brain_network() - Adaptive network creation
 * - init_*_subsystem() functions - All subsystem initializations
 * - brain_destroy() - Complete brain cleanup and destruction
 *
 * @version 1.0.0
 * @date 2025-12-08
 */

#include "core/brain/nimcp_brain_core.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "api/nimcp_api_exception.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/memory/nimcp_memory_guards.h"
#include "utils/validation/nimcp_validate.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "security/nimcp_security.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "plasticity/adaptive/nimcp_adaptive.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "utils/memory/nimcp_memory.h"
#include "glial/integration/nimcp_glial_integration.h"
#include "cognitive/nimcp_symbolic_logic.h"
#include "cognitive/epistemic/nimcp_epistemic_filter.h"
#include "core/brain_regions/nimcp_brain_regions.h"
#include "core/brain/factory/init/nimcp_brain_init.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"
#include "plasticity/attention/nimcp_attention.h"
#include "core/neuron_types/nimcp_neural_logic.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define LOG_MODULE "BRAIN_CORE"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_core, MESH_ADAPTER_CATEGORY_COGNITIVE)


// NOTE: Implementation functions are currently in nimcp_brain.c
// This file provides modular organization and will be fully populated
// in a future migration phase. For now, these are declared extern
// and linked from the main nimcp_brain.c compilation unit.

// External references to functions still in nimcp_brain.c
extern brain_t allocate_brain(void);
extern adaptive_network_t create_brain_network(uint32_t num_inputs, uint32_t num_outputs,
                                               uint32_t num_neurons, float sparsity_target,
                                               ode_integration_method_t integration_method);
extern bool init_output_labels(brain_t brain, uint32_t num_outputs);
extern bool init_attention_subsystem(brain_t brain);
extern bool init_brain_regions_subsystem(brain_t brain);
extern bool init_symbolic_logic_subsystem(brain_t brain);
extern bool init_symbolic_reasoning_subsystem(brain_t brain);
extern bool init_epistemic_subsystem(brain_t brain);

// Wrapper functions for modular interface
// These currently delegate to the main implementation

/**
 * @brief Initialize all core brain subsystems
 *
 * WHAT: Orchestrate initialization of attention, brain regions, logic, and epistemic systems
 * WHY:  Provide single entry point for complete subsystem initialization
 * HOW:  Call each init function in proper dependency order
 *
 * @param brain Brain instance to initialize
 * @return true on success, false on any subsystem failure
 */
bool brain_core_init_all_subsystems(brain_t brain)
{
    if (!brain) {
        LOG_MODULE_ERROR("BRAIN_CORE", "NULL brain in init_all_subsystems");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_core_init_all_subsystems: NULL brain");
        return false;
    }

    // Initialize in dependency order
    if (!init_attention_subsystem(brain)) {
        LOG_MODULE_ERROR("BRAIN_CORE", "Failed to initialize attention subsystem");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "brain_core_init_all_subsystems: init_attention_subsystem is NULL");
        return false;
    }

    if (!init_brain_regions_subsystem(brain)) {
        LOG_MODULE_ERROR("BRAIN_CORE", "Failed to initialize brain regions subsystem");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "brain_core_init_all_subsystems: init_brain_regions_subsystem is NULL");
        return false;
    }

    if (!init_symbolic_logic_subsystem(brain)) {
        LOG_MODULE_ERROR("BRAIN_CORE", "Failed to initialize symbolic logic subsystem");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "brain_core_init_all_subsystems: init_symbolic_logic_subsystem is NULL");
        return false;
    }

    if (!init_symbolic_reasoning_subsystem(brain)) {
        LOG_MODULE_ERROR("BRAIN_CORE", "Failed to initialize symbolic reasoning subsystem");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "brain_core_init_all_subsystems: init_symbolic_reasoning_subsystem is NULL");
        return false;
    }

    if (!init_epistemic_subsystem(brain)) {
        LOG_MODULE_ERROR("BRAIN_CORE", "Failed to initialize epistemic subsystem");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "brain_core_init_all_subsystems: init_epistemic_subsystem is NULL");
        return false;
    }

    LOG_MODULE_INFO("BRAIN_CORE", "All subsystems initialized successfully");
    return true;
}
