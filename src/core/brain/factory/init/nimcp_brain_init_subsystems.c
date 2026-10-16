//=============================================================================
// nimcp_brain_init_subsystems.c - Brain Subsystem Initialization
//=============================================================================
/**
 * @file nimcp_brain_init_subsystems.c
 * @brief Brain subsystem initialization functions
 *
 * WHAT: All brain subsystem initialization functions (37 functions)
 * WHY:  Separates subsystem initialization from core brain creation
 * HOW:  Each function initializes a specific brain subsystem
 *
 * EXTRACTED FROM: nimcp_brain_init.c
 * DATE: 2025-12-08
 *
 * @version 2.7.0
 * @author NIMCP Development Team
 */

//=============================================================================
// Includes
//=============================================================================

#include "core/brain/factory/init/nimcp_brain_init_subsystems.h"

// Extracted subsystem modules
#include "core/brain/factory/init/nimcp_brain_init_perception.h"
#include "core/brain/factory/init/nimcp_brain_init_neuromod.h"
#include "core/brain/factory/init/nimcp_brain_init_cognitive.h"
#include "core/brain/factory/init/nimcp_brain_init_memory.h"
#include "core/brain/factory/init/nimcp_brain_init_plasticity.h"
#include "core/brain/factory/init/nimcp_brain_init_monitoring.h"
#include "core/brain/factory/init/nimcp_brain_init_structural.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/validation/nimcp_validate.h"
#include "utils/error/nimcp_error_codes.h"

#define LOG_MODULE "BRAIN_INIT_SUBSYSTEMS"

// Compatibility macro for set_error (converts to LOG_ERROR)
#ifndef set_error
#define set_error(msg) LOG_ERROR(LOG_MODULE, "%s", msg)
#endif

// Subsystem includes
#include "glial/integration/nimcp_glial_integration.h"
#include "glial/myelin_sheath/nimcp_myelin_sheath.h"
#include "core/brain_oscillations/nimcp_brain_oscillations.h"
#include "cognitive/introspection/nimcp_introspection.h"
#include "cognitive/introspection/nimcp_connectivity_health.h"
#include "middleware/integration/nimcp_middleware_controller.h"
#include "core/axon/nimcp_axon.h"
#include "core/dendrite/nimcp_dendrite.h"
#include "cognitive/ethics/nimcp_ethics.h"
#include "cognitive/salience/nimcp_salience.h"
#include "cognitive/consolidation/nimcp_consolidation.h"
#include "cognitive/curiosity/nimcp_curiosity.h"
#include "cognitive/knowledge/nimcp_knowledge.h"
#include "cognitive/nimcp_symbolic_logic.h"
#include "cognitive/epistemic/nimcp_epistemic_filter.h"
#include "cognitive/wellbeing/nimcp_wellbeing.h"
#include "plasticity/neuromodulators/nimcp_neuromod_pink_noise.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"
#include "plasticity/neuromodulators/nimcp_spatial_neuromod.h"
#include "plasticity/attention/nimcp_attention.h"
#include "core/neuron_types/nimcp_neural_logic.h"
#include "cognitive/nimcp_fractal_cognitive.h"
#include "core/integration/nimcp_multimodal_integration.h"
#include "perception/nimcp_visual_cortex.h"
#include "perception/nimcp_audio_cortex.h"
#include "perception/nimcp_speech_cortex.h"
#include "nlp/nimcp_nlp.h"
#include "core/brain_regions/nimcp_brain_regions.h"
#include "core/events/nimcp_event_bus.h"
#include "cognitive/nimcp_working_memory.h"
#include "cognitive/nimcp_emotional_tagging.h"
#include "cognitive/nimcp_emotional_system.h"
#include "cognitive/nimcp_executive.h"
#include "cognitive/nimcp_sleep_wake.h"
#include "cognitive/memory/nimcp_engram.h"
#include "cognitive/memory/nimcp_systems_consolidation.h"
#include "cognitive/memory/nimcp_wm_transfer.h"
#include "cognitive/memory/nimcp_semantic_memory.h"
#include "cognitive/nimcp_mental_health.h"
#include "cognitive/nimcp_theory_of_mind.h"
#include "cognitive/nimcp_explanations.h"
#include "cognitive/nimcp_meta_learning.h"
#include "cognitive/nimcp_predictive.h"
#include "cognitive/nimcp_mirror_neurons.h"
#include "cognitive/global_workspace/nimcp_global_workspace.h"
#include "cognitive/global_workspace/nimcp_global_workspace_shannon.h"
#include "cognitive/nimcp_autobiographical_memory.h"
#include "middleware/training/nimcp_brain_training_integration.h"
#include "plasticity/adaptive/nimcp_adaptive.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "plasticity/homeostatic/nimcp_homeostatic.h"
#include "plasticity/dendritic/nimcp_dendritic.h"
#include "plasticity/predictive/nimcp_predictive_coding.h"
#include "core/cortical_columns/nimcp_cortical_column.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <stdio.h>

#define LOG_MODULE "BRAIN_INIT_SUBSYSTEMS"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_subsystems, MESH_ADAPTER_CATEGORY_SYSTEM)


//=============================================================================
// Phase SC-2: Security-Fault Tolerance Integration
//=============================================================================

/**
 * @brief Initialize security subsystem with fault tolerance integration
 *
 * WHAT: Sets up the security-recovery bridge for comprehensive protection
 * WHY:  Detect security violations and automatically trigger recovery
 * HOW:  Creates security coverage, fractal security, and connects to fault tolerance
 *
 * ARCHITECTURE:
 * - Security Coverage: Memory region protection with hash verification
 * - Fractal Security: Hierarchical Merkle tree integrity checking
 * - Recovery Bridge: Routes violations to fast recovery or checkpoints
 *
 * @param brain Brain to initialize security for
 * @return true on success, false on failure
 *
 * COMPLEXITY: O(1) setup, O(n) region registration
 */
