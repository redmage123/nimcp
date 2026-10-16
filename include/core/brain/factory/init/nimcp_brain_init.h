//=============================================================================
// nimcp_brain_init.h - Brain Initialization and Subsystem Setup
//=============================================================================
/**
 * @file nimcp_brain_init.h
 * @brief Brain initialization and subsystem setup functions
 *
 * WHAT: All brain structure and subsystem initialization logic
 * WHY:  Separates initialization from creation orchestration
 * HOW:  Modular initialization functions for each brain subsystem
 *
 * @version 2.7.0
 * @author NIMCP Development Team
 */

#ifndef NIMCP_BRAIN_INIT_H
#define NIMCP_BRAIN_INIT_H

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Includes
//=============================================================================

#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "plasticity/adaptive/nimcp_adaptive.h"
#include "security/nimcp_blood_brain_barrier.h"  // Phase IS-1: BBB perimeter defense
#include <stdbool.h>
#include <stdint.h>

// Forward declarations for internal types
typedef struct task_strategy task_strategy_t;

//=============================================================================
// Note: All function declarations are in the main nimcp_brain_factory.h header
// This module contains the implementations
//=============================================================================

//=============================================================================
// Core Infrastructure Initialization
//=============================================================================

bool nimcp_brain_factory_init_output_labels(brain_t brain, uint32_t num_outputs);
bool nimcp_brain_factory_init_event_bus(brain_t brain);

//=============================================================================
// Subsystem Initialization Functions (31 total)
//=============================================================================

bool nimcp_brain_factory_init_glial_subsystem(brain_t brain);
bool nimcp_brain_factory_init_multimodal_subsystems(brain_t brain);
bool nimcp_brain_factory_init_pink_noise_subsystem(brain_t brain);
bool nimcp_brain_factory_init_neuromodulator_system(brain_t brain);
bool nimcp_brain_factory_init_spatial_neuromod_system(brain_t brain);
bool nimcp_brain_factory_init_attention_subsystem(brain_t brain);
bool nimcp_brain_factory_init_brain_regions_subsystem(brain_t brain);
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
bool nimcp_brain_factory_init_consolidation_subsystem(brain_t brain);
bool nimcp_brain_factory_init_curiosity_subsystem(brain_t brain);
bool nimcp_brain_factory_init_salience_subsystem(brain_t brain);
bool nimcp_brain_factory_init_introspection_subsystem(brain_t brain);
bool nimcp_brain_factory_init_connectivity_health_subsystem(brain_t brain);  // Phase 1.5.4
bool nimcp_brain_factory_init_middleware_controller_subsystem(brain_t brain);  // Phase 1.5.5
bool nimcp_brain_factory_init_axon_subsystem(brain_t brain);                   // Phase 1.5.6
bool nimcp_brain_factory_init_dendrite_subsystem(brain_t brain);               // Phase 1.5.7
bool nimcp_brain_factory_init_cortical_columns_subsystem(brain_t brain);       // Phase CC-1 (Tier 0.65)
bool nimcp_brain_factory_init_ethics_engine_subsystem(brain_t brain);
bool nimcp_brain_factory_init_empathy_network_subsystem(brain_t brain);
bool nimcp_brain_factory_init_empathetic_response_subsystem(brain_t brain);
bool nimcp_brain_factory_init_autobiographical_memory_subsystem(brain_t brain);
bool nimcp_brain_factory_init_self_model_subsystem(brain_t brain);
bool nimcp_brain_factory_init_global_workspace_subsystem(brain_t brain);
bool nimcp_brain_factory_init_security_subsystem(brain_t brain);  // Phase SC-2 (Tier 0.7)

// Phase T1: Biological Framework Enhancements (Training Pipeline)
bool nimcp_brain_factory_init_homeostatic_plasticity_subsystem(brain_t brain);
bool nimcp_brain_factory_init_dendritic_computation_subsystem(brain_t brain);
bool nimcp_brain_factory_init_biological_predictive_subsystem(brain_t brain);

// Enhanced Basal Ganglia (Action Selection & Motor Control)
bool nimcp_brain_factory_init_basal_ganglia_subsystem(brain_t brain);

// GPU Context Integration (CUDA Kernel Acceleration)
bool nimcp_brain_factory_init_gpu_subsystem(brain_t brain);
void nimcp_brain_factory_destroy_gpu_subsystem(brain_t brain);

// GPU Inference Integration (Weight Cache for inference forward pass)
bool nimcp_brain_factory_init_gpu_inference(brain_t brain);

// Phase IS-1: BBB Global System Reference Management
void nimcp_bbb_release_global_system(void);

/**
 * @brief Get the global BBB system for cross-module protection
 *
 * WHAT: Returns the shared BBB system instance
 * WHY:  Enables all modules to use consistent perimeter security
 * HOW:  Thread-safe access to reference-counted global BBB system
 *
 * @return BBB system handle, or NULL if not initialized
 */
bbb_system_t nimcp_bbb_get_global_system(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_BRAIN_INIT_H
