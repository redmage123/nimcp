//=============================================================================
// nimcp_brain_factory.c - Brain Factory and Configuration Implementation
//=============================================================================
/**
 * @file nimcp_brain_factory.c
 * @brief Implementation of brain factory functions for creation and orchestration
 *
 * WHAT: Factory pattern implementation for brain instantiation
 * WHY:  Separates complex brain creation logic from core brain operations
 * HOW:  Orchestrates initialization, validation, and creation workflows
 *
 * ARCHITECTURE:
 * - Factory Pattern: Creates brains of different types with validated configs
 * - Builder Pattern: Modular configuration construction  
 * - Strategy Pattern Integration: Task-specific behaviors
 *
 * EXTRACTED FROM: Original nimcp_brain_factory.c (3220 lines)
 * DATE: 2025-11-19
 *
 * @version 2.7.0
 * @author NIMCP Development Team
 */

//=============================================================================
// Includes
//=============================================================================

// Bio-async integration
#include "async/nimcp_bio_async.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#include "core/brain/factory/nimcp_brain_factory.h"
#include "core/brain/factory/init/nimcp_brain_init_language.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain_bio_async.h"
#include "core/brain/strategy/nimcp_brain_strategy.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <cjson/cJSON.h>

// Core dependencies
#include "plasticity/adaptive/nimcp_adaptive.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/cache/nimcp_cache.h"
#include "utils/time/nimcp_time.h"
#include "utils/validation/nimcp_validate.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/logging/nimcp_logging.h"
#include "security/nimcp_security.h"
#include "core/topology/nimcp_community_detection.h"
#include "utils/algorithms/nimcp_graph_metrics.h"
#include "utils/containers/nimcp_graph.h"

// Subsystem dependencies
#include "glial/integration/nimcp_glial_integration.h"
#include "core/brain_oscillations/nimcp_brain_oscillations.h"
#include "cognitive/introspection/nimcp_introspection.h"
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

// Multi-modal integration
#include "core/integration/nimcp_multimodal_integration.h"
#include "perception/nimcp_visual_cortex.h"
#include "perception/nimcp_audio_cortex.h"
#include "perception/nimcp_speech_cortex.h"
#include "nlp/nimcp_nlp.h"

// Brain regions and cognitive systems
#include "core/brain_regions/nimcp_brain_regions.h"
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
#include "cognitive/nimcp_autobiographical_memory.h"

// Sub-module headers
#include "core/brain/factory/init/nimcp_brain_init.h"
#include "core/brain/factory/init/nimcp_brain_init_plasticity.h"
#include "core/brain/factory/init/nimcp_brain_init_subsystems.h"
#include "core/brain/factory/init/nimcp_brain_init_medulla.h"
#include "core/brain/factory/init/nimcp_brain_init_basal_ganglia.h"
#include "core/brain/factory/init/nimcp_brain_init_pr_memory.h"
#include "core/brain/factory/init/nimcp_brain_init_world_model.h"
#include "core/brain/factory/init/nimcp_brain_init_creative.h"
#include "core/brain/factory/nimcp_brain_init_state_manager.h"
#include "core/brain/factory/validation/nimcp_brain_validation.h"
#include "core/brain/factory/init/nimcp_brain_init_white_matter.h"
#include "core/brain/factory/init/nimcp_brain_init_inferior_colliculus.h"
#include "core/brain/factory/init/nimcp_brain_init_spinal_cord.h"
#include "core/brain/factory/init/nimcp_brain_init_cortical_interneurons.h"
#include "core/brain/factory/init/nimcp_brain_init_neuropeptide.h"
#include "core/brain/factory/init/nimcp_brain_init_endocannabinoid.h"
#include "core/brain/factory/init/nimcp_brain_init_glymphatic.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/signal/nimcp_signal_handler.h"
#include "core/brain/factory/nimcp_brain_parallel_init.h"

#define LOG_MODULE "BRAIN_FACTORY"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_factory, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// External Function Declarations
//=============================================================================

// Error handling (shared across all brain modules)
extern void set_error(const char* format, ...);

//=============================================================================
// Compatibility Macros (bridge to init module functions)
//=============================================================================

// These macros map short names to the full prefixed names defined in init module
#define init_output_labels                          nimcp_brain_factory_init_output_labels
#define init_event_bus                              nimcp_brain_factory_init_event_bus
#define init_glial_subsystem                        nimcp_brain_factory_init_glial_subsystem
#define init_multimodal_subsystems                  nimcp_brain_factory_init_multimodal_subsystems
#define init_pink_noise_subsystem                   nimcp_brain_factory_init_pink_noise_subsystem
#define init_neuromodulator_system                  nimcp_brain_factory_init_neuromodulator_system
#define init_spatial_neuromod_system                nimcp_brain_factory_init_spatial_neuromod_system
#define init_neuromod_nuclei                        nimcp_brain_factory_init_neuromod_nuclei
#define init_attention_subsystem                    nimcp_brain_factory_init_attention_subsystem
#define init_brain_regions_subsystem                nimcp_brain_factory_init_brain_regions_subsystem
#define init_symbolic_logic_subsystem               nimcp_brain_factory_init_symbolic_logic_subsystem
#define init_symbolic_reasoning_subsystem           nimcp_brain_factory_init_symbolic_reasoning_subsystem
#define init_epistemic_subsystem                    nimcp_brain_factory_init_epistemic_subsystem
#define init_working_memory_subsystem               nimcp_brain_factory_init_working_memory_subsystem
#define init_executive_subsystem                    nimcp_brain_factory_init_executive_subsystem
#define init_theory_of_mind_subsystem               nimcp_brain_factory_init_theory_of_mind_subsystem
#define init_natural_explanations_subsystem         nimcp_brain_factory_init_natural_explanations_subsystem
#define init_meta_learning_subsystem                nimcp_brain_factory_init_meta_learning_subsystem
#define init_mental_health_subsystem                nimcp_brain_factory_init_mental_health_subsystem
#define init_trauma_resilience                      nimcp_brain_factory_init_trauma_resilience
#define init_predictive_subsystem                   nimcp_brain_factory_init_predictive_subsystem
#define init_mirror_neurons                         nimcp_brain_factory_init_mirror_neurons
#define init_consolidation_subsystem                nimcp_brain_factory_init_consolidation_subsystem
#define init_curiosity_subsystem                    nimcp_brain_factory_init_curiosity_subsystem
#define init_salience_subsystem                     nimcp_brain_factory_init_salience_subsystem
#define init_introspection_subsystem                nimcp_brain_factory_init_introspection_subsystem
#define init_connectivity_health_subsystem          nimcp_brain_factory_init_connectivity_health_subsystem
#define init_middleware_controller_subsystem        nimcp_brain_factory_init_middleware_controller_subsystem
#define init_axon_subsystem                         nimcp_brain_factory_init_axon_subsystem
#define init_dendrite_subsystem                     nimcp_brain_factory_init_dendrite_subsystem
#define init_cortical_columns_subsystem             nimcp_brain_factory_init_cortical_columns_subsystem
#define init_substrate_gpu_subsystem                nimcp_brain_factory_init_substrate_gpu_subsystem
#define init_substrate_thalamic_subsystem           nimcp_brain_factory_init_substrate_thalamic_subsystem
#define init_ethics_engine_subsystem                nimcp_brain_factory_init_ethics_engine_subsystem
#define init_empathy_network_subsystem              nimcp_brain_factory_init_empathy_network_subsystem
#define init_empathetic_response_subsystem          nimcp_brain_factory_init_empathetic_response_subsystem
#define init_autobiographical_memory_subsystem      nimcp_brain_factory_init_autobiographical_memory_subsystem
#define init_self_model_subsystem                   nimcp_brain_factory_init_self_model_subsystem
#define init_global_workspace_subsystem             nimcp_brain_factory_init_global_workspace_subsystem
#define init_security_subsystem                     nimcp_brain_factory_init_security_subsystem
#define init_immune_subsystem                       nimcp_brain_factory_init_immune_subsystem
#define init_collective_cognition_subsystem         nimcp_brain_factory_init_collective_cognition_subsystem
#define init_rcog_engine_subsystem                 nimcp_brain_factory_init_rcog_engine_subsystem
#define init_inner_dialogue_subsystem              nimcp_brain_factory_init_inner_dialogue_subsystem
#define init_reasoning_engine_subsystem            nimcp_brain_factory_init_reasoning_engine_subsystem
#define init_imagination_subsystem                 nimcp_brain_factory_init_imagination_subsystem
#define init_homeostatic_plasticity_subsystem       nimcp_brain_factory_init_homeostatic_plasticity_subsystem
#define init_dendritic_computation_subsystem        nimcp_brain_factory_init_dendritic_computation_subsystem
#define init_biological_predictive_subsystem        nimcp_brain_factory_init_biological_predictive_subsystem
#define init_training_subsystem                     nimcp_brain_factory_init_training_subsystem
#define init_fep_orchestrator_subsystem             nimcp_brain_factory_init_fep_orchestrator_subsystem
#define init_core_directives_subsystem              nimcp_brain_factory_init_core_directives_subsystem
#define init_brain_config                           nimcp_brain_factory_init_brain_config
#define init_brain_stats                            nimcp_brain_factory_init_brain_stats

// Coordinator/Orchestrator subsystem macros
#define init_bio_async_orchestrator_subsystem       nimcp_brain_factory_init_bio_async_orchestrator_subsystem
#define init_plasticity_coordinator_subsystem       nimcp_brain_factory_init_plasticity_coordinator_subsystem

// Plasticity bridge subsystem macros (Phase 7: Cognitive Substrate Integration)
#define init_stdp_omni_bridge_subsystem             nimcp_brain_factory_init_stdp_omni_bridge_subsystem
#define init_stdp_pr_bridge_subsystem               nimcp_brain_factory_init_stdp_pr_bridge_subsystem
#define init_eligibility_pr_bridge_subsystem        nimcp_brain_factory_init_eligibility_pr_bridge_subsystem
#define init_stdp_quantum_bridge_subsystem          nimcp_brain_factory_init_stdp_quantum_bridge_subsystem

// Phase 6 Sensory Modules (BR-9/10/11: Somatosensory, Olfactory, Gustatory)
#define init_somatosensory_subsystem                nimcp_brain_factory_init_somatosensory_subsystem
#define init_olfactory_subsystem                    nimcp_brain_factory_init_olfactory_subsystem
#define init_gustatory_subsystem                    nimcp_brain_factory_init_gustatory_subsystem

#define init_immune_bridge_coordinator_subsystem    nimcp_brain_factory_init_immune_bridge_coordinator_subsystem
#define init_cognitive_meta_controller_subsystem    nimcp_brain_factory_init_cognitive_meta_controller_subsystem
#define init_security_perception_bridge_subsystem   nimcp_brain_factory_init_security_perception_bridge_subsystem
#define init_swarm_module_registry_subsystem        nimcp_brain_factory_init_swarm_module_registry_subsystem

// Medulla Oblongata subsystem macro (brainstem autonomic regulation)
#define init_medulla_subsystem                      nimcp_brain_factory_init_medulla_subsystem

// Hypothalamus subsystem macro (homeostatic regulation, circadian, HPA axis)
#define init_hypothalamus_subsystem                 nimcp_brain_factory_init_hypothalamus_subsystem

// Parietal Lobe subsystem macro (mathematical/scientific reasoning)
#define init_parietal_subsystem                     nimcp_brain_factory_init_parietal_subsystem

// Intuition System subsystem macro (Phase 6 creative/intuitive reasoning)
#define init_intuition_subsystem                    nimcp_brain_factory_init_intuition_subsystem

// Dragonfly subsystem macro (bio-inspired target tracking)
#define init_dragonfly_subsystem                    nimcp_brain_factory_init_dragonfly_subsystem

// Edge subsystem macro (sensor hub, safety watchdog, swarm bridges)
#define init_edge_subsystem                         nimcp_brain_factory_init_edge_subsystem

// Knowledge Graph Reader subsystem macro (self-awareness)
#define init_kg_reader_subsystem                    nimcp_brain_factory_init_kg_reader_subsystem

// Internal Knowledge Graph subsystem macro (runtime CRUD, security)
#define init_internal_kg_subsystem                  nimcp_brain_factory_init_internal_kg_subsystem
#define destroy_internal_kg_subsystem               nimcp_brain_factory_destroy_internal_kg_subsystem

// GPU Context subsystem macro (CUDA kernel acceleration)
#define init_gpu_subsystem                          nimcp_brain_factory_init_gpu_subsystem
#define destroy_gpu_subsystem                       nimcp_brain_factory_destroy_gpu_subsystem
#define init_gpu_inference                           nimcp_brain_factory_init_gpu_inference

// Fault Tolerance subsystem macro (intelligent recovery with parietal integration)
// Fuzzy Logic subsystem macro (cross-cutting utility)
#define init_fuzzy_subsystem                       nimcp_brain_factory_init_fuzzy_subsystem

// Creative System subsystem macro (artistic appreciation & generation)
#define init_creative_subsystem                    nimcp_brain_factory_init_creative_subsystem
#define destroy_creative_subsystem                 nimcp_brain_factory_destroy_creative_subsystem

#define init_fault_tolerance_subsystem              nimcp_brain_factory_init_fault_tolerance_subsystem

// Health Agent subsystem macro (autonomous health monitoring)
#define init_health_agent_subsystem                 nimcp_brain_factory_init_health_agent_subsystem
#define destroy_health_agent_subsystem              nimcp_brain_factory_destroy_health_agent_subsystem

// State Manager subsystem macro (checkpointing and recovery - Phase 8)
#define init_state_manager_subsystem                brain_init_state_manager
#define destroy_state_manager_subsystem             brain_shutdown_state_manager

// Enhanced Basal Ganglia subsystem macro (action selection & motor control)
#define init_basal_ganglia_subsystem                nimcp_brain_factory_init_basal_ganglia_subsystem

// Prime Resonant Memory subsystem macro (content-addressable consolidation)
#define init_pr_memory_subsystem                    nimcp_brain_factory_init_pr_memory_subsystem

// World Model subsystem macro (generative world model for mental simulation)
#define init_world_model_subsystem                  nimcp_brain_factory_init_world_model_subsystem
#define wire_world_model_active_inference           nimcp_brain_factory_wire_world_model_active_inference
#define wire_world_model_imagination                nimcp_brain_factory_wire_world_model_imagination

// LNN temporal context processor (NCP architecture)
extern bool nimcp_brain_factory_init_lnn_subsystem(brain_t brain);
#define init_lnn_subsystem                          nimcp_brain_factory_init_lnn_subsystem

// White Matter Tracts subsystem macro (long-range myelinated connectivity)
#define init_white_matter_subsystem                 nimcp_brain_factory_init_white_matter_subsystem

// Inferior Colliculus subsystem macro (auditory midbrain processing)
#define init_inferior_colliculus_subsystem           nimcp_brain_factory_init_inferior_colliculus_subsystem

// Spinal Cord subsystem macro (motor output, CPGs, reflex arcs)
#define init_spinal_cord_subsystem                  nimcp_brain_factory_init_spinal_cord_subsystem

// Cortical Interneurons subsystem macro (GABAergic inhibitory microcircuits)
#define init_cortical_interneurons_subsystem        nimcp_brain_factory_init_cortical_interneurons_subsystem

// Neuropeptide System subsystem macro (slow neuromodulation: 8 peptides)
#define init_neuropeptide_subsystem                 nimcp_brain_factory_init_neuropeptide_subsystem

// Endocannabinoid System subsystem macro (retrograde synaptic modulation)
#define init_endocannabinoid_subsystem              nimcp_brain_factory_init_endocannabinoid_subsystem

// Glymphatic System subsystem macro (brain waste clearance during sleep)
#define init_glymphatic_subsystem                   nimcp_brain_factory_init_glymphatic_subsystem

//=============================================================================
// Main Factory Functions
//=============================================================================

// P3 NOTE: personality_profile_t is assumed to be a flat POD type (no internal allocations).
// If personality_profile_t ever gains dynamically allocated fields (e.g., char* name),
// then ALL callers using nimcp_free(brain->personality) MUST change to a dedicated
// personality_destroy() function. See also: nimcp_brain.c brain_destroy().
personality_profile_t* create_personality(const brain_config_t* config)
{
    // Guard: NULL check
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return NULL;

    }

    // Allocate personality profile
    personality_profile_t* profile = nimcp_malloc(sizeof(personality_profile_t));
    if (!profile) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "profile is NULL");

        return NULL;

    }

    // Generate personality based on configuration
    if (config->use_random_personality) {
        // Random generation with configured probabilities
        personality_generation_config_t gen_config;
        gen_config.trait_mean = config->personality_trait_mean;
        gen_config.trait_stddev = config->personality_trait_stddev;
        gen_config.female_probability = config->female_probability;
        gen_config.male_probability = config->male_probability;
        gen_config.non_binary_probability = config->non_binary_probability;
        gen_config.seed = config->personality_seed;
        gen_config.enforce_balanced_traits = false;

        *profile = personality_generate_random(&gen_config);
    } else {
        // Explicit specification
        personality_traits_t traits;
        traits.openness = config->explicit_openness;
        traits.conscientiousness = config->explicit_conscientiousness;
        traits.extraversion = config->explicit_extraversion;
        traits.agreeableness = config->explicit_agreeableness;
        traits.neuroticism = config->explicit_neuroticism;

        identity_profile_t identity = {0};
        identity.gender = (gender_identity_t)config->explicit_gender;
        identity.sexuality = (sexual_orientation_t)config->explicit_sexuality;
        identity.gender_certainty = 1.0F;
        identity.sexuality_certainty = 1.0F;
        identity.gender_is_core_identity = true;
        identity.sexuality_is_core_identity = false;

        *profile = personality_create_custom(&traits, &identity);
    }

    return profile;
}

/**
 * @brief Create brain with preset size and task
 *
 * WHY: Factory pattern - single creation entry point
 * Encapsulates all creation complexity with validation
 *
 * COMPLEXITY: O(n) where n = num_neurons
 * MEMORY: O(n*c) where c = connections per neuron
 *
 * PATTERN: Factory pattern with guard clauses
 *
 * @param task_name Human-readable name
 * @param size Brain size preset
 * @param task Task template
 * @param num_inputs Input dimension
 * @param num_outputs Output dimension
 * @return Brain handle or NULL on error
 */
/**
 * @brief Create brain with custom configuration (MAIN IMPLEMENTATION)
 *
 * WHY: Single source of truth for all brain initialization (DRY principle)
 * All brain creation flows through this function.
 *
 * COMPLEXITY: O(n) where n = num_neurons
 *
 * @param config Custom configuration (must be fully initialized)
 * @return Brain handle or NULL on error
 */
brain_t brain_create_custom(const brain_config_t* config)
{
    if (!config) {
        set_error("Null config provided");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_create_custom: config is NULL");
        return NULL;
    }

    // Auto-load from checkpoint if enabled (default behavior)
    if (config->checkpoint_path && config->auto_load) {
        // P3-55 FIX: Use access() instead of fopen/fclose just to check existence
        if (access(config->checkpoint_path, R_OK) == 0) {
            brain_t loaded_brain = brain_load(config->checkpoint_path);
            if (loaded_brain) {
                // P1-53 FIX: Only copy behavioral config fields, NOT structural fields.
                // memcpy(&loaded_brain->config, config, sizeof(brain_config_t)) would
                // overwrite num_inputs/num_outputs/num_neurons from the loaded topology,
                // replacing the trained architecture with the (possibly different) config.
                loaded_brain->config.learning_rate = config->learning_rate;
                loaded_brain->config.sparsity_target = config->sparsity_target;
                loaded_brain->config.enable_explanations = config->enable_explanations;
                loaded_brain->config.task = config->task;
                loaded_brain->config.enable_working_memory = config->enable_working_memory;
                loaded_brain->config.enable_theory_of_mind = config->enable_theory_of_mind;
                loaded_brain->config.enable_empathy_responses = config->enable_empathy_responses;
                loaded_brain->config.enable_mirror_neurons = config->enable_mirror_neurons;
                loaded_brain->config.enable_quantum_annealing = config->enable_quantum_annealing;
                loaded_brain->config.enable_glial = config->enable_glial;
                /* enable_homeostasis removed - field no longer exists in brain_config_t */
                loaded_brain->config.minimal_mode = config->minimal_mode;

                // Initialize sparse coding on loaded brain (not saved in checkpoint)
                if (!loaded_brain->sparse_coding_system) {
                    sparse_coding_config_t sc_config;
                    cortical_sparse_default_config(&sc_config);
                    sc_config.sparsity_method = SPARSITY_METHOD_K_WTA;
                    sc_config.target_sparsity = 0.05f;
                    sc_config.num_columns = loaded_brain->config.num_outputs > 0
                        ? loaded_brain->config.num_outputs : 4096;
                    sc_config.k_winners = (uint32_t)(sc_config.num_columns * sc_config.target_sparsity);
                    if (sc_config.k_winners < 8) sc_config.k_winners = 8;
                    sc_config.enable_homeostasis = true;
                    sc_config.adaptation_rate = 0.002f;
                    sc_config.enable_bio_async = false;
                    sc_config.enable_lateral_inhibition = false;
                    loaded_brain->sparse_coding_system = cortical_sparse_create(&sc_config);
                    loaded_brain->enable_sparse_coding = (loaded_brain->sparse_coding_system != NULL);
                    if (loaded_brain->enable_sparse_coding) {
                        LOG_INFO(LOG_MODULE, "Sparse coding initialized on loaded brain: K-WTA k=%u/%u",
                                 sc_config.k_winners, sc_config.num_columns);
                    }
                }

                /* Neuromodulator system isn't persisted in the checkpoint and
                 * brain_load() doesn't recreate it. Without this, the SNN
                 * language bridge's DA gate stays inert forever on resume
                 * (bridge_da_gated_stdp_passes=0) because
                 * snn_language_bridge_connect_neuromod() is gated on
                 * brain->neuromodulator_system != NULL. The fresh-brain path
                 * gets it via parallel-init Wave 2; the resume path skipped
                 * the entire factory orchestration, so we restore it here
                 * before returning the loaded brain. */
                if (!loaded_brain->neuromodulator_system) {
                    extern bool nimcp_brain_factory_init_neuromodulator_system(brain_t brain);
                    if (!nimcp_brain_factory_init_neuromodulator_system(loaded_brain)) {
                        LOG_WARN(LOG_MODULE,
                            "Resume: neuromod system init failed — DA gate will stay inert");
                    } else {
                        LOG_INFO(LOG_MODULE,
                            "Resume: neuromod system created on loaded brain");
                    }
                }

                return loaded_brain;
            }
            LOG_WARN(LOG_MODULE, "Failed to load checkpoint from '%s', creating fresh brain",
                     config->checkpoint_path);
        }
    }

    // Validate config fields
    size_t task_name_size = sizeof(config->task_name);
    if (!nimcp_validate_string_field(config->task_name, task_name_size)) {
        set_error("Invalid task_name in config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_create_custom: nimcp_validate_string_field is NULL");
        return NULL;
    }
    if (!nimcp_validate_integer_field(&config->num_inputs, sizeof(config->num_inputs)) ||
        config->num_inputs < 1 || config->num_inputs > 10000) {
        set_error("Invalid num_inputs in config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_create_custom: nimcp_validate_integer_field is NULL");
        return NULL;
    }
    if (!nimcp_validate_integer_field(&config->num_outputs, sizeof(config->num_outputs)) ||
        config->num_outputs < 1 || config->num_outputs > 10000) {
        set_error("Invalid num_outputs in config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_create_custom: nimcp_validate_integer_field is NULL");
        return NULL;
    }
    if (!nimcp_validate_float_field(&config->learning_rate, sizeof(config->learning_rate))) {
        set_error("Invalid learning_rate in config (NaN or Inf)");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_create_custom: nimcp_validate_float_field is NULL");
        return NULL;
    }
    // Allow 0.0 for zero-initialized configs - will use default downstream
    // Validate learning_rate is in sensible range (0.0 to 1.0) - 0.0 means "use default"
    if (config->learning_rate < 0.0f || config->learning_rate > 1.0f) {
        set_error("learning_rate must be in range [0.0, 1.0], got %f", config->learning_rate);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_create_custom: validation failed");
        return NULL;
    }
    if (!nimcp_validate_float_field(&config->sparsity_target, sizeof(config->sparsity_target))) {
        set_error("Invalid sparsity_target in config (NaN or Inf)");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_create_custom: nimcp_validate_float_field is NULL");
        return NULL;
    }
    // Validate sparsity_target is in valid range (0.0 to 1.0)
    if (config->sparsity_target < 0.0f || config->sparsity_target > 1.0f) {
        set_error("sparsity_target must be in range [0.0, 1.0], got %f", config->sparsity_target);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_create_custom: validation failed");
        return NULL;
    }

    // ========================================================================
    // CORE BRAIN ALLOCATION AND INITIALIZATION
    // ========================================================================

    brain_t brain = nimcp_brain_factory_allocate_brain();
    if (!brain) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain is NULL");

        return NULL;

    }

    // Copy configuration
    brain->config = *config;

    // Copy KG persistence settings (Phase SNAPSHOT-KG)
    // NOTE: C-side kg_persistence module has API mismatches and is disabled.
    // QuestDB checkpoint registration is handled Python-side in immerse_athena.py.
    brain->snapshot_backend = config->snapshot_backend;
    brain->kg_persistence = NULL;
    brain->owns_kg_persistence = false;

    // Create strategy for task
    brain->strategy = strategy_create(config->task);
    if (!brain->strategy) {
        set_error("Failed to create task strategy");
        nimcp_free(brain);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_create_custom: brain->strategy is NULL");
        return NULL;
    }

    // Create personality profile
    brain->personality = create_personality(&brain->config);
    if (!brain->personality) {
        set_error("Failed to create personality profile");
        strategy_destroy(brain->strategy);
        nimcp_free(brain);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_create_custom: brain->personality is NULL");
        return NULL;
    }

    // Create neural network
    uint32_t num_neurons = (config->neuron_count > 0)
        ? config->neuron_count
        : nimcp_brain_factory_get_neuron_count(config->size);
    LOG_MODULE_DEBUG(LOG_MODULE, "Creating network (%u neurons, %u in, %u out)",
                     num_neurons, config->num_inputs, config->num_outputs);
    brain->network = nimcp_brain_factory_create_brain_network(
        config->num_inputs, config->num_outputs, num_neurons,
        config->sparsity_target, config->neuron_integration);
    LOG_MODULE_DEBUG(LOG_MODULE, "Network created");
    if (!brain->network) {
        set_error("Failed to create adaptive network");
        strategy_destroy(brain->strategy);
        nimcp_free(brain->personality);
        nimcp_free(brain);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_create_custom: brain->network is NULL");
        return NULL;
    }

    // Initialize output labels
    if (!init_output_labels(brain, config->num_outputs)) {
        adaptive_network_destroy(brain->network);
        strategy_destroy(brain->strategy);
        nimcp_free(brain->personality);
        nimcp_free(brain);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "brain_create_custom: init_output_labels is NULL");
        return NULL;
    }

    // Initialize universal event bus
    if (!init_event_bus(brain)) {
        adaptive_network_destroy(brain->network);
        strategy_destroy(brain->strategy);
        nimcp_free(brain->personality);
        nimcp_free(brain);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "brain_create_custom: init_event_bus is NULL");
        return NULL;
    }

    // Initialize statistics
    init_brain_stats(&brain->stats, config->task_name, config->size,
                     config->num_inputs, config->learning_rate);

    // Fix: init_brain_stats uses the preset neuron count for the size enum,
    // but if neuron_count was overridden (e.g. 1.5M instead of preset 5000),
    // we must update stats to reflect the actual network size.
    if (num_neurons != brain->stats.num_neurons) {
        brain->stats.num_neurons = num_neurons;
        // Recalculate synapse estimate with actual neuron count
        uint64_t synapse_count = (uint64_t)num_neurons * (uint64_t)config->num_inputs;
        brain->stats.num_synapses = (synapse_count > UINT32_MAX) ? UINT32_MAX : (uint32_t)synapse_count;
        brain->stats.num_active_synapses = brain->stats.num_synapses;
    }

    // Initialize unified experience API (developmental learning)
    brain->experience_config = brain_experience_default_config();
    brain->inference_learning_enabled = false;  // Off by default — enable via brain_experience_configure()
    brain->last_experience_prediction = NULL;
    brain->last_experience_input = NULL;
    brain->last_experience_input_size = 0;
    brain->last_experience_output_size = 0;
    brain->experience_count = 0;
    brain->synaptogenesis_count = 0;

    // Pre-allocate scratch buffers for learn() hot path (eliminates 4 malloc/free per call)
    brain->learn_scratch.target_cap = config->num_outputs;
    brain->learn_scratch.features_cap = config->num_inputs;
    brain->learn_scratch.target = nimcp_malloc(config->num_outputs * sizeof(float));
    brain->learn_scratch.prediction = nimcp_malloc(config->num_outputs * sizeof(float));
    brain->learn_scratch.attended_features = nimcp_malloc(config->num_inputs * sizeof(float));
    brain->learn_scratch.cue_neurons = nimcp_malloc(config->num_inputs * sizeof(uint32_t));
    // Non-fatal if any fail — learn() will fall back to per-call malloc

    // ========================================================================
    // PHASE 1.5: MEMORY POOLS FOR HOT-PATH ALLOCATIONS
    // P2 FIX: Check each pool creation for failure. Pools are optional
    // optimizations - code falls back to heap allocation if pool is NULL.
    // ========================================================================
    memory_pool_config_t decision_pool_config = {
        .block_size = sizeof(brain_decision_t),
        .num_blocks = 4, .alignment = 16,
        .enable_tracking = false, .enable_guard_pages = false
    };
    brain->decision_struct_pool = memory_pool_create(&decision_pool_config);
    if (!brain->decision_struct_pool) {
        LOG_MODULE_WARN("BRAIN_FACTORY", "Decision pool creation failed, using heap fallback");
    }

    memory_pool_config_t output_pool_config = {
        .block_size = config->num_outputs * sizeof(float),
        .num_blocks = 4, .alignment = 16,
        .enable_tracking = false, .enable_guard_pages = false
    };
    brain->output_vector_pool = memory_pool_create(&output_pool_config);
    if (!brain->output_vector_pool) {
        LOG_MODULE_WARN("BRAIN_FACTORY", "Output vector pool creation failed, using heap fallback");
    }

    uint32_t max_active = num_neurons / 10;
    if (max_active < 100) max_active = 100;
    brain->max_active_neurons_for_pool = max_active;

    memory_pool_config_t active_ids_pool_config = {
        .block_size = max_active * sizeof(uint32_t),
        .num_blocks = 4, .alignment = 16,
        .enable_tracking = false, .enable_guard_pages = false
    };
    brain->active_neuron_ids_pool = memory_pool_create(&active_ids_pool_config);
    if (!brain->active_neuron_ids_pool) {
        LOG_MODULE_WARN("BRAIN_FACTORY", "Active neuron IDs pool creation failed, using heap fallback");
    }

    // ========================================================================
    // SUBSYSTEM INITIALIZATION (in dependency order)
    // Lazy evaluation: Skip heavy subsystems when lazy_*_init flags are set.
    // These will be initialized on-demand when first accessed.
    // ========================================================================

    // Parallel init: wave-based execution of independent subsystem inits.
    // Falls through to sequential path if parallel_init is disabled or pool fails.
    bool parallel_init_done = false;
    if (config->parallel_init && !config->minimal_mode) {
        if (nimcp_brain_parallel_init_subsystems(brain, config)) {
            parallel_init_done = true;
            goto post_init;
        }
        // If parallel init returned false due to pool creation failure (not subsystem failure),
        // fall through to sequential path. If a subsystem actually failed, brain is corrupted.
        // Simple heuristic: if GPU was already initialized, a subsystem failed (not just pool).
        if (brain->gpu_ctx) {
            brain_destroy(brain);
            return NULL;
        }
        LOG_WARN("BRAIN_FACTORY", "Parallel init unavailable, using sequential fallback");
    }

    // GPU Context (auto-init if GPU available - enables CUDA kernel acceleration)
    // This is initialized first as many subsystems can benefit from GPU acceleration.
    if (!init_gpu_subsystem(brain)) { brain_destroy(brain); return NULL; }

    // GPU Inference: Initialize weight cache for adaptive network GPU forward pass
    if (!init_gpu_inference(brain)) { brain_destroy(brain); return NULL; }

    // Inference thread pool for parallel cognitive stages
    brain->inference_pool = NULL;
    brain->frozen = false;
    if (!brain->config.force_serial_inference) {
        uint32_t pool_threads = brain->config.inference_threads;
        if (pool_threads == 0) pool_threads = 4;  // Default: 4 threads
        brain->inference_pool = nimcp_pool_create(pool_threads);
        // Non-fatal if pool creation fails — serial fallback
    }

    // Phase 5/6: Glial integration (heavy - astrocyte/oligodendrocyte networks)
    if (!brain->config.lazy_glial_init) {
        if (!init_glial_subsystem(brain)) { brain_destroy(brain); return NULL; }
    }

    // Phase 1.5.6: Axon network (heavy - myelination modeling)
    if (!brain->config.lazy_axon_init) {
        if (!init_axon_subsystem(brain)) { brain_destroy(brain); return NULL; }
    }

    // Phase 1.5.7: Dendrite network (heavy - dendritic computation)
    if (!brain->config.lazy_dendrite_init) {
        if (!init_dendrite_subsystem(brain)) { brain_destroy(brain); return NULL; }
    }

    // Phase 8: Multi-modal subsystems (heavy - visual/audio/speech cortices)
    // Check individual lazy flags for perception subsystems
    if (!brain->config.lazy_visual_init && !brain->config.lazy_audio_init &&
        !brain->config.lazy_speech_init) {
        if (!init_multimodal_subsystems(brain)) { brain_destroy(brain); return NULL; }
    }

    // Phase 8.6: Pink noise neuromodulation (light - always init)
    if (!init_pink_noise_subsystem(brain)) { brain_destroy(brain); return NULL; }

    // Phase 10.5: Neuromodulator system (heavy - spatial/temporal dynamics)
    if (!brain->config.lazy_neuromod_init) {
        if (!init_neuromodulator_system(brain)) { brain_destroy(brain); return NULL; }
        if (!init_spatial_neuromod_system(brain)) { brain_destroy(brain); return NULL; }
    }

    // Phase 4: Neuromodulatory Nuclei (LC, VTA, Raphe, Habenula)
    // Depends on neuromodulator_system for baseline levels
    if (!init_neuromod_nuclei(brain)) { brain_destroy(brain); return NULL; }

    // Phase GPU-1: Unified GPU Neural Substrate (depends on GPU context, brain regions, neuromod)
    // Initializes GPU tensors for axons, dendrites, myelin, neuromodulators, glial, metabolic
    if (!init_substrate_gpu_subsystem(brain)) { brain_destroy(brain); return NULL; }

    // Phase 1-4 CPU substrate + thalamic router (biological adapters for SNN/LNN/CNN)
    // Creates neural_substrate_t + thalamic_router_t so *_attach_substrate /
    // *_attach_thalamic_router can wire them into specialized networks at
    // creation time (multi-network training) and lazy cortex CNN creation.
    // Non-fatal: adapters fall back to biological defaults if creation fails.
    if (!init_substrate_thalamic_subsystem(brain)) {
        LOG_WARN(LOG_MODULE, "Substrate+thalamic init failed — Phase 1-4 adapters "
                              "will run in biological-default fallback mode");
        /* Non-fatal: continue with NULL substrate/router */
    }

    // Phase 8.9: Neural logic gates
    if (!init_symbolic_logic_subsystem(brain)) { brain_destroy(brain); return NULL; }

    // Phase 9.4: Symbolic reasoning
    if (!init_symbolic_reasoning_subsystem(brain)) { brain_destroy(brain); return NULL; }

    // Phase 9.3: Wellbeing monitoring
    brain->wellbeing_monitoring_enabled = true;
    brain->wellbeing_check_interval_ms = 0;
    brain->last_wellbeing_check_time = 0;
    memset(&brain->last_distress, 0, sizeof(distress_assessment_t));
    brain->last_distress.type = DISTRESS_NONE;
    brain->last_distress.severity = DISTRESS_SEVERITY_NORMAL;

    // Simulation time tracking
    brain->current_time_us = 0;
    brain->last_glial_update_us = 0;
    brain->glial_update_counter = 0;

    // Default input modality: text only (sensory bridges dormant until enabled)
    brain->active_modalities = BRAIN_MODALITY_TEXT;

    // Phase 10.2: Working memory (heavy - item buffers, decay dynamics)
    if (!brain->config.lazy_working_memory_init) {
        if (!init_working_memory_subsystem(brain)) { brain_destroy(brain); return NULL; }
    }

    // ========================================================================
    // PHASE 2 MIDDLEWARE: SPIKE ANALYSIS & POPULATION CODING
    // ========================================================================
    brain->enable_spike_analysis = true;
    brain->enable_population_coding = true;
    brain->spike_feature_extractor = brain_create_spike_feature_extractor(1000, true, true);
    brain->population_analyzer = brain_create_population_analyzer();
    brain->quantum_annealer = NULL;

    // ========================================================================
    // PHASE C4: SHANNON INFORMATION THEORY
    // ========================================================================
    brain->shannon_config = shannon_default_config();
    brain->enable_shannon_monitoring = false;
    memset(&brain->last_shannon_metrics, 0, sizeof(shannon_network_metrics_t));

    // Phase C4.1: Quantum-Shannon diffusion
    brain->quantum_shannon_diffusion = NULL;
    brain->enable_quantum_shannon_diffusion = false;
    brain->quantum_shannon_mixing_ratio = 0.2F;
    brain->quantum_shannon_evolution_steps = 100;
    memset(&brain->last_quantum_shannon_metrics, 0, sizeof(shannon_diffusion_metrics_t));

    // Phase C4.7: Cross-modal information flow
    brain->cross_modal_graph = NULL;
    brain->enable_cross_modal_monitoring = false;
    memset(&brain->last_cross_modal_metrics, 0, sizeof(multi_modal_integration_t));
    brain->cross_modal_bottleneck_threshold = 0.5F;
    brain->cross_modal_sample_count = 50;

    // ========================================================================
    // PHASE E: EMOTIONAL INTELLIGENCE SYSTEMS
    // ========================================================================
    // P2-56 FIX: Check return values of emotional subsystem creation and log warnings
    brain->shadow_emotions = shadow_system_create(8);
    if (!brain->shadow_emotions) { LOG_WARN(LOG_MODULE, "Shadow emotions subsystem creation failed"); }
    brain->bias_detection = bias_system_create(8);
    if (!brain->bias_detection) { LOG_WARN(LOG_MODULE, "Bias detection subsystem creation failed"); }
    brain->grief_system = grief_system_create();
    if (!brain->grief_system) { LOG_WARN(LOG_MODULE, "Grief system creation failed"); }
    brain->joy_system = joy_system_create();
    if (!brain->joy_system) { LOG_WARN(LOG_MODULE, "Joy system creation failed"); }
    brain->remorse_system = remorse_regret_system_create();
    if (!brain->remorse_system) { LOG_WARN(LOG_MODULE, "Remorse/regret system creation failed"); }
    brain->social_bond_system = social_bond_system_create();
    if (!brain->social_bond_system) { LOG_WARN(LOG_MODULE, "Social bond system creation failed"); }

    // ========================================================================
    // COGNITIVE SUBSYSTEMS
    // ========================================================================

    // Phase 3.0: Multihead attention
    if (!init_attention_subsystem(brain)) { brain_destroy(brain); return NULL; }

    // Module Integration: Brain regions
    if (!init_brain_regions_subsystem(brain)) { brain_destroy(brain); return NULL; }

    // Phase CC-1: Cortical columns architecture (Tier 0.65)
    if (!init_cortical_columns_subsystem(brain)) { brain_destroy(brain); return NULL; }

    // Sparse coding: K-WTA sparsity enforcement on output layer
    // Breaks mode collapse by forcing only ~5% of output neurons to fire per input
    {
        sparse_coding_config_t sc_config;
        cortical_sparse_default_config(&sc_config);
        sc_config.sparsity_method = SPARSITY_METHOD_K_WTA;
        sc_config.target_sparsity = 0.05f;  // 5% active (slightly more permissive than cortical 3%)
        sc_config.num_columns = brain->config.num_outputs > 0 ? brain->config.num_outputs : 4096;
        sc_config.k_winners = (uint32_t)(sc_config.num_columns * sc_config.target_sparsity);
        if (sc_config.k_winners < 8) sc_config.k_winners = 8;  // At least 8 winners
        sc_config.enable_homeostasis = true;
        sc_config.adaptation_rate = 0.002f;
        sc_config.enable_bio_async = false;  // Don't need messaging overhead
        sc_config.enable_lateral_inhibition = false;  // K-WTA is sufficient
        brain->sparse_coding_system = cortical_sparse_create(&sc_config);
        brain->enable_sparse_coding = (brain->sparse_coding_system != NULL);
        if (brain->enable_sparse_coding) {
            LOG_INFO("Sparse coding initialized: K-WTA with k=%u/%u (%.1f%% sparsity)",
                     sc_config.k_winners, sc_config.num_columns,
                     sc_config.target_sparsity * 100.0f);
        }
    }

    // Language Layer: Orchestrator + LNN generator + tokenizer + embeddings
    nimcp_brain_factory_init_language_subsystem(brain);  // Non-fatal if prerequisites missing

    // Phase 9.2: Epistemic filtering
    if (!init_epistemic_subsystem(brain)) { brain_destroy(brain); return NULL; }

    // Phase 10.2: Memory consolidation (heavy - engram formation, replay)
    if (!brain->config.lazy_consolidation_init) {
        if (!init_consolidation_subsystem(brain)) { brain_destroy(brain); return NULL; }
    }

    // Prime Resonant Memory System (content-addressable consolidation)
    // Provides: Z-Ladder (4-tier memory), theta-gamma coupling, entanglement graph
    if (brain->config.enable_pr_memory && !brain->config.lazy_pr_memory_init) {
        if (!init_pr_memory_subsystem(brain)) { brain_destroy(brain); return NULL; }
    }

    // LNN temporal context processor (NCP architecture: 128→64→32→64 = 288 neurons)
    // Creates compact continuous-time ODE network for sequence-aware temporal context.
    // Skipped in fast_training_mode. Connected to TPB, immune, bio-async.
    if (!brain->config.fast_training_mode) {
        if (!init_lnn_subsystem(brain)) { brain_destroy(brain); return NULL; }
    }

    // World Model System (generative world model for mental simulation)
    // Provides: Omni World Model (RSSM dynamics), Multimodal World Model (cross-modal fusion)
    // Enables: Counterfactual reasoning, policy evaluation, dreaming, mental imagery
    // Auto-enable when not in fast training mode (biological development needs world model)
    if ((brain->config.enable_world_model || !brain->config.fast_training_mode) &&
        !brain->config.lazy_world_model_init) {
        brain->config.enable_world_model = true;
        if (!init_world_model_subsystem(brain)) { brain_destroy(brain); return NULL; }
    }

    // Phase 10.3: Executive functions (heavy - Portia integration, planning)
    if (!brain->config.lazy_executive_init) {
        if (!init_executive_subsystem(brain)) { brain_destroy(brain); return NULL; }
    }

    // Phase 10.6: Theory of Mind (heavy - agent modeling, belief tracking)
    if (!brain->config.lazy_theory_of_mind_init) {
        if (!init_theory_of_mind_subsystem(brain)) { brain_destroy(brain); return NULL; }
    }

    // Phase 10.7: Natural Explanations (light - always init)
    if (!init_natural_explanations_subsystem(brain)) { brain_destroy(brain); return NULL; }

    // Phase 10.8: Meta-Learning (heavy - learning-to-learn dynamics)
    if (!brain->config.lazy_meta_learning_init) {
        if (!init_meta_learning_subsystem(brain)) { brain_destroy(brain); return NULL; }
    }

    // Phase 10.5: Mental Health Monitoring
    if (!init_mental_health_subsystem(brain)) { brain_destroy(brain); return NULL; }

    // Trauma Resilience: recall dampening + arousal homeostasis
    if (!init_trauma_resilience(brain)) { brain_destroy(brain); return NULL; }

    // Phase 10.9: Predictive Processing
    if (!init_predictive_subsystem(brain)) { brain_destroy(brain); return NULL; }

    // Phase 10.11: Mirror Neurons (heavy - action observation, imitation learning)
    if (!brain->config.lazy_mirror_neurons_init) {
        if (!init_mirror_neurons(brain)) { brain_destroy(brain); return NULL; }
    }

    // Phase 10.11.2: Curiosity Engine (light - always init)
    if (!init_curiosity_subsystem(brain)) { brain_destroy(brain); return NULL; }

    // Phase 10.11.3: Salience Evaluator (light - always init)
    if (!init_salience_subsystem(brain)) { brain_destroy(brain); return NULL; }

    // Phase 11: Ethics Engine (heavy - value alignment, constraint checking)
    // Skip for minimal_mode to prevent recursive brain creation (ethics creates golden_rule brain)
    if (!brain->config.lazy_ethics_init && !brain->config.minimal_mode) {
        if (!init_ethics_engine_subsystem(brain)) { brain_destroy(brain); return NULL; }
        if (!init_empathy_network_subsystem(brain)) { brain_destroy(brain); return NULL; }
        if (!init_empathetic_response_subsystem(brain)) { brain_destroy(brain); return NULL; }
    }

    // Phase 12: Introspection
    if (!init_introspection_subsystem(brain)) { brain_destroy(brain); return NULL; }

    // Phase 1.5.4: Connectivity Health Monitoring
    if (!init_connectivity_health_subsystem(brain)) { brain_destroy(brain); return NULL; }

    // Phase 1.5.5: Middleware Controller
    if (!init_middleware_controller_subsystem(brain)) { brain_destroy(brain); return NULL; }

    // Phase 12: Autobiographical Memory
    if (!init_autobiographical_memory_subsystem(brain)) { brain_destroy(brain); return NULL; }

    // Phase 12: Self-Model
    if (!init_self_model_subsystem(brain)) { brain_destroy(brain); return NULL; }

    // Global Workspace Architecture (heavy - conscious access, broadcast)
    if (!brain->config.lazy_global_workspace_init) {
        if (!init_global_workspace_subsystem(brain)) { brain_destroy(brain); return NULL; }
    }

    // Phase SC-2: Security-Fault Tolerance Integration
    if (!init_security_subsystem(brain)) { brain_destroy(brain); return NULL; }

    // Brain Immune System (Adaptive Defense Coordination)
    if (!init_immune_subsystem(brain)) { brain_destroy(brain); return NULL; }

    // Signal Handler Installation (SIGSEGV, SIGABRT, etc.)
    // Install after immune system so crash signals can be reported to immune.
    // Uses default config: fatal signals → LOG_SHUTDOWN with stack trace.
    {
        signal_handler_config_t sig_cfg = signal_handler_default_config();
        sig_cfg.enable_stack_trace = true;
        sig_cfg.enable_checkpoint_save = true;
        if (!signal_handler_install(&sig_cfg)) {
            LOG_MODULE_WARN("BRAIN_FACTORY", "Signal handler installation failed");
        }
    }

    // Phase T1: Biological Framework Enhancements (Training Pipeline)
    if (!init_homeostatic_plasticity_subsystem(brain)) { brain_destroy(brain); return NULL; }
    if (!init_dendritic_computation_subsystem(brain)) { brain_destroy(brain); return NULL; }
    if (!init_biological_predictive_subsystem(brain)) { brain_destroy(brain); return NULL; }

    // Phase TM-3: Brain-Training Integration (Loss Functions, Optimizers)
    if (!init_training_subsystem(brain)) { brain_destroy(brain); return NULL; }

    // ========================================================================
    // BIO-ASYNC INTEGRATION
    // Skip for minimal_mode - lightweight brains don't need async messaging
    // ========================================================================

    if (!brain->config.minimal_mode) {
        // Initialize bio-router globally if not already initialized
        if (!bio_router_is_initialized()) {
            bio_router_config_t router_config = bio_router_default_config();
            router_config.max_modules = 64;              // Support many brain modules
            router_config.inbox_capacity = 128;          // Messages per module
            router_config.outbox_capacity = 128;
            router_config.max_message_size = 4096;       // 4KB max message
            router_config.worker_threads = 2;            // Light async processing
            router_config.enable_logging = true;
            router_config.enable_statistics = true;
            router_config.routing_timeout_ms = 100.0F;   // 100ms timeout

            nimcp_error_t router_err = bio_router_init(&router_config);
            if (router_err != NIMCP_SUCCESS) {
                LOG_WARN(LOG_MODULE, "Bio-router initialization failed! Bio-async features disabled.");
                brain->bio_async_enabled = false;
            }
        }

        // Initialize bio-async for this brain instance
        if (bio_router_is_initialized()) {
            nimcp_error_t async_err = brain_bio_async_init(brain);
            if (async_err != NIMCP_SUCCESS) {
                LOG_WARN(LOG_MODULE, "Brain bio-async initialization failed! Async messaging disabled.");
                brain->bio_async_enabled = false;
            } else {
                brain->bio_async_enabled = true;
            }
        }
    }

    // ========================================================================
    // FEP ORCHESTRATOR (CENTRAL FEP BRIDGE COORDINATION)
    // ========================================================================

    // Initialize FEP orchestrator after immune and bio-async are ready
    // (orchestrator connects to both for unified FEP bridge coordination)
    if (!init_fep_orchestrator_subsystem(brain)) { brain_destroy(brain); return NULL; }

    // ========================================================================
    // MEDULLA OBLONGATA (BRAINSTEM AUTONOMIC REGULATION)
    // ========================================================================

    // Initialize medulla early - it provides foundational autonomic regulation
    // The medulla operates at the lowest level of the brain hierarchy:
    // - Arousal State: Global activation level for all higher functions
    // - Protective Cutoff: Emergency shutdown capability
    // - Circadian Rhythm: 24-hour biological clock simulation
    // - Brainstem Coupling: Coordination with other brainstem nuclei
    if (!init_medulla_subsystem(brain)) { brain_destroy(brain); return NULL; }

    // ========================================================================
    // HYPOTHALAMUS (HOMEOSTATIC REGULATION)
    // ========================================================================

    // Initialize hypothalamus for homeostatic regulation:
    // - Temperature Regulation: Thermostat-like setpoint maintenance
    // - Hunger/Thirst: Drive states for resource acquisition
    // - Circadian Integration: Sleep-wake coordination with medulla
    // - HPA Axis: Stress response coordination (cortisol regulation)
    // - Autonomic Balance: Sympathetic/parasympathetic coordination
    // BIOLOGICAL: Master regulator connecting drives to behavior
    // DEPENDS ON: Medulla (for arousal input)
    // CONNECTS TO: Emotional, Sleep, Immune, Wellbeing systems
    if (!init_hypothalamus_subsystem(brain)) { brain_destroy(brain); return NULL; }

    // ========================================================================
    // PHASE 6 SENSORY MODULES (BR-9/10/11: TOUCH, SMELL, TASTE)
    // ========================================================================
    // Initialize Phase 6 sensory processing modules:
    // - BR-9 Somatosensory: Touch, proprioception, temperature, pain (S1/S2)
    // - BR-10 Olfactory: Smell processing (piriform cortex, bypasses thalamus)
    // - BR-11 Gustatory: Taste processing (insular cortex, 5 basic tastes)
    // BIOLOGICAL: These modules provide multi-modal sensory input integration
    // DEPENDS ON: Thalamus (relay), Hypothalamus (homeostasis), Brain regions (amygdala/hippocampus)
    if (!init_somatosensory_subsystem(brain)) { brain_destroy(brain); return NULL; }
    if (!init_olfactory_subsystem(brain)) { brain_destroy(brain); return NULL; }
    if (!init_gustatory_subsystem(brain)) { brain_destroy(brain); return NULL; }

    // ========================================================================
    // WHITE MATTER TRACTS (LONG-RANGE MYELINATED CONNECTIVITY)
    // ========================================================================
    // Initialize white matter tract system:
    // - 8 major tracts with conduction velocity and myelination modeling
    // - Foundation for inter-regional signal routing and timing
    // - Activity-dependent myelination via glial integration
    // BIOLOGICAL: Myelinated axon bundles connecting distant brain regions
    // DEPENDS ON: Thalamus (relay), Glial (myelination), Brain regions
    if (!init_white_matter_subsystem(brain)) { brain_destroy(brain); return NULL; }

    // ========================================================================
    // INFERIOR COLLICULUS (AUDITORY MIDBRAIN PROCESSING)
    // ========================================================================
    // Initialize inferior colliculus for auditory relay:
    // - Tonotopic map (64 channels, 20Hz-20kHz log-spaced)
    // - Binaural integration: ITD/ILD for sound localization
    // - ICC (central): primary ascending relay
    // - ICX (external): multisensory spatial integration
    // BIOLOGICAL: Primary auditory midbrain nucleus, relay to MGN
    // DEPENDS ON: Thalamus (MGN relay), Audio cortex (downstream)
    if (!init_inferior_colliculus_subsystem(brain)) { brain_destroy(brain); return NULL; }

    // ========================================================================
    // SPINAL CORD (MOTOR OUTPUT & REFLEX ARCS)
    // ========================================================================
    // Initialize spinal cord motor output system:
    // - Central Pattern Generators for rhythmic movement
    // - Reflex arcs: stretch, withdrawal, crossed extension
    // - Motor neuron pools grouped by effector
    // - Proprioceptive feedback (Ia, II, Ib afferents)
    // BIOLOGICAL: Final common pathway for motor commands
    // DEPENDS ON: White matter (corticospinal tract), Motor cortex
    if (!init_spinal_cord_subsystem(brain)) { brain_destroy(brain); return NULL; }

    // ========================================================================
    // CORTICAL INTERNEURONS (INHIBITORY MICROCIRCUIT CONTROL)
    // ========================================================================
    // Initialize cortical interneuron system:
    // - PV basket: gamma oscillations, perisomatic inhibition
    // - PV chandelier: output gating at AIS
    // - SST Martinotti: dendritic feedback inhibition
    // - VIP: disinhibition for attention gating
    // - NGF L1: slow volume transmission GABA
    // BIOLOGICAL: E/I balance, feature binding, predictive coding
    // DEPENDS ON: Cortical columns (host architecture)
    if (!init_cortical_interneurons_subsystem(brain)) { brain_destroy(brain); return NULL; }

    // ========================================================================
    // NEUROPEPTIDE SYSTEM (SLOW NEUROMODULATION)
    // ========================================================================
    // Initialize neuropeptide system with 8 peptides:
    // - Oxytocin, Vasopressin, NPY, Substance P
    // - Orexin, CRH, Endorphin, CCK
    // BIOLOGICAL: Slow modulatory effects on social, stress, pain, appetite
    // DEPENDS ON: Hypothalamus (primary release site)
    if (!init_neuropeptide_subsystem(brain)) { brain_destroy(brain); return NULL; }

    // ========================================================================
    // ENDOCANNABINOID SYSTEM (RETROGRADE SYNAPTIC MODULATION)
    // ========================================================================
    // Initialize endocannabinoid system:
    // - CB1/CB2 receptor dynamics with regional density
    // - 2-AG and anandamide synthesis/degradation
    // - DSI/DSE retrograde suppression
    // - Pain modulation via spinal gate control
    // BIOLOGICAL: Retrograde signaling modulates mood, pain, appetite, memory
    // DEPENDS ON: Neuropeptide (pain pathways), Somatosensory
    if (!init_endocannabinoid_subsystem(brain)) { brain_destroy(brain); return NULL; }

    // ========================================================================
    // GLYMPHATIC SYSTEM (BRAIN WASTE CLEARANCE)
    // ========================================================================
    // Initialize glymphatic waste clearance system:
    // - AQP4-driven CSF/ISF exchange
    // - 10-60x more active during NREM sleep
    // - Clears beta-amyloid, tau, metabolic waste
    // - High waste degrades learning rate and decision confidence
    // BIOLOGICAL: Sleep-dependent brain cleaning, Alzheimer's prevention
    // DEPENDS ON: Glial (AQP4 expression), Sleep system
    if (!init_glymphatic_subsystem(brain)) { brain_destroy(brain); return NULL; }

    // ========================================================================
    // HEMISPHERIC ARCHITECTURE (CALLOSUM + LATERALIZATION)
    // ========================================================================
    // Initialize corpus callosum and lateralization for inter-hemispheric
    // communication. Lightweight: no sub-brains, just callosum channels and
    // domain routing weights. Neurons are logically partitioned.
    // BIOLOGICAL: Corpus callosum (~200M axons), lateralized processing
    // DEPENDS ON: White matter tracts (callosum IS a white matter tract)
    {
        callosum_config_t cc_cfg = callosum_default_config();
        cc_cfg.bandwidth_mode = CALLOSUM_BW_REALISTIC;
        cc_cfg.queue_capacity = 256;
        cc_cfg.drop_on_overflow = true;
        cc_cfg.initial_connection_strength = 1.0f;
        cc_cfg.enable_bio_async = false;

        brain->callosum = callosum_create(&cc_cfg);
        if (!brain->callosum) {
            LOG_WARN(LOG_MODULE, "Corpus callosum creation failed — hemispheric features disabled");
        }

        brain->lateralization = lateralization_default_profile();
        brain->dominant_hemisphere = HEMISPHERE_LEFT;
        brain->hemispheric_balance = 0.0f;
        brain->hemispheric_enabled = (brain->callosum != NULL);
        brain->last_callosum_process_us = 0;

        if (brain->hemispheric_enabled) {
            LOG_INFO(LOG_MODULE, "Hemispheric architecture initialized (5-channel callosum)");
        }
    }

    // ========================================================================
    // EDGE-CLOUD HYBRID INFERENCE (standalone by default — user wires backend)
    // ========================================================================
    brain->cloud_bridge = NULL;
    brain->cloud_inference_enabled = false;

    // ========================================================================
    // RECURRENT FORWARD PASS + BPTT (Iterative Refinement & Temporal Learning)
    // ========================================================================
    {
        // Recurrent forward pass: re-process uncertain outputs
        brain->recurrent_enabled = true;
        brain->recurrent_max_iterations = 3;
        brain->recurrent_confidence_threshold = 0.7f;
        brain->recurrent_blend_alpha = 0.3f;
        brain->recurrent_iteration_count = 0;

        // BPTT: temporal gradient accumulation over recent examples
        brain->bptt_enabled = true;
        brain->bptt_window_size = 3;  /* Reduced from 8: steps 4-7 contribute <0.66 weight, not worth 5x compute. Increase for Stage 3. */
        brain->bptt_discount = 0.9f;
        brain->bptt_buffer = nimcp_calloc(brain->bptt_window_size,
                                          sizeof(*brain->bptt_buffer));
        brain->bptt_head = 0;
        brain->bptt_count = 0;
        brain->bptt_input_dim = 0;   // Lazy-allocated on first learn_vector call
        brain->bptt_output_dim = 0;

        if (brain->bptt_buffer) {
            LOG_INFO(LOG_MODULE, "BPTT enabled (window=%u, discount=%.1f) + recurrent forward (max_iter=%u)",
                     brain->bptt_window_size, brain->bptt_discount, brain->recurrent_max_iterations);
        }

        // Plasticity interval gating counter (run bio-plasticity every 10 steps)
        brain->plasticity_step_counter = 0;

        // Pre-allocated inference output buffers (lazy-allocated on first forward pass)
        brain->inference_buf_adaptive = NULL;
        brain->inference_buf_cnn = NULL;
        brain->inference_buf_snn = NULL;
        brain->inference_buf_size = 0;

        // Multi-network fusion defaults (Phase 6)
        brain->enable_fusion = false;
        brain->fusion_weights[0] = 0.7f;  // Adaptive
        brain->fusion_weights[1] = 0.1f;  // CNN
        brain->fusion_weights[2] = 0.1f;  // SNN
        brain->fusion_weights[3] = 0.1f;  // LNN
        brain->fusion_scratch_lnn = NULL;
    }

    // ========================================================================
    // FUZZY LOGIC (CROSS-CUTTING UTILITY)
    // ========================================================================

    // Initialize fuzzy logic utility module:
    // - Graded membership reasoning for all modules
    // - Risk assessment with continuous risk grades
    // - Training convergence detection via fuzzy inference
    // - Plasticity rate scheduling via fuzzy rules
    // BIOLOGICAL: Neural firing rates encode graded activation
    if (!init_fuzzy_subsystem(brain)) { brain_destroy(brain); return NULL; }

    // ========================================================================
    // CREATIVE SYSTEM (ARTISTIC APPRECIATION & GENERATION)
    // ========================================================================

    // Initialize creative/artistic cognitive system:
    // - Aesthetic Appreciation: Evaluate art quality (Berlyne aesthetics)
    // - Style Learning: Learn and represent artistic styles via embeddings
    // - Text Generation: Poetry, prose, screenplay, lyrics
    // - Music Generation: Composition, arrangement (MIDI/audio)
    // - Visual Generation: Images via diffusion models and GANs
    // - Video Generation: Video synthesis and cinema production
    // - Multimodal Direction: Full-length film/creative project coordination
    // - Ethics Validation: Copyright, safety, bias detection
    // BIOLOGICAL: Creative cognition integrates perception, emotion, memory, and motor planning
    if (!init_creative_subsystem(brain)) { brain_destroy(brain); return NULL; }

    // ========================================================================
    // PARIETAL LOBE (MATHEMATICAL/SCIENTIFIC REASONING)
    // ========================================================================

    // Initialize parietal lobe for quantitative cognition:
    // - Number Sense: Weber-Fechner magnitude estimation, subitizing
    // - Spatial Reasoning: Mental rotation, coordinate transforms
    // - Mathematical Intuition: Pattern detection, analogical reasoning
    // - Scientific Reasoning: Hypothesis testing, dimensional analysis
    // - Equation Manipulation: Symbolic math, differentiation
    if (!init_parietal_subsystem(brain)) { brain_destroy(brain); return NULL; }

    // ========================================================================
    // INTUITION SYSTEM (PHASE 6: CREATIVE/INTUITIVE REASONING)
    // ========================================================================
    // Initialize Phase 6 intuition integration system:
    // - Intuitive Reasoning: Pattern-based hunch generation
    // - Analogical Reasoning: Cross-domain mapping and transfer
    // - Insight Discovery: Aha! moments and cognitive restructuring
    // - Hypothesis Generation: Abductive and creative theory formation
    // - Conceptual Blending: Novel concept synthesis from multiple inputs
    // - Counterfactual Reasoning: What-if scenario exploration
    // - Meta-Reasoning: Reasoning about reasoning strategies
    // BIOLOGICAL: Higher-order cognition through intuitive leaps
    if (!init_intuition_subsystem(brain)) { brain_destroy(brain); return NULL; }

    // ========================================================================
    // DRAGONFLY SUBSYSTEM (BIO-INSPIRED TARGET TRACKING)
    // ========================================================================
    // Initialize dragonfly target tracking system:
    // - TSDN: Target-selective descending neurons (motion detection)
    // - CSTMD1: Winner-take-all target selection
    // - Prediction: IMM-based multi-model trajectory prediction
    // - Interception: Proportional navigation guidance
    // BIOLOGICAL: Achieves 95%+ success rate matching Hemicordulia tau behavior
    if (!init_dragonfly_subsystem(brain)) { brain_destroy(brain); return NULL; }

    // ========================================================================
    // FAULT TOLERANCE SUBSYSTEM (INTELLIGENT RECOVERY)
    // ========================================================================
    // Initialize fault tolerance with parietal integration for intelligent repair:
    // - Recovery Executive: Multi-step recovery planning and execution
    // - Parietal Bridge: Code analysis, pattern matching, spatial reasoning
    // - Metacognitive monitoring: Self-awareness during recovery
    // DEPENDS ON: Parietal lobe (for code analysis), Working memory (fault tracking)
    if (!init_fault_tolerance_subsystem(brain)) { brain_destroy(brain); return NULL; }

    // ========================================================================
    // HEALTH AGENT SUBSYSTEM (AUTONOMOUS MONITORING)
    // ========================================================================
    // Initialize health agent for independent, continuous health monitoring:
    // - Memory: Track allocations, detect leaks and corruption
    // - Neural: Monitor SNN/LNN stability, detect divergence
    // - Behavioral: Monitor Dragonfly/Portia behavioral modules
    // - Oscillations: Detect abnormal brain wave patterns
    // - Cross-module: Coordinate health responses across subsystems
    // BIOLOGICAL: Maps to autonomous nervous system monitoring
    // DEPENDS ON: Immune system, Oscillations, SNN/LNN (if present)
    if (!init_health_agent_subsystem(brain)) { brain_destroy(brain); return NULL; }

    // ========================================================================
    // STATE MANAGER (CHECKPOINTING & RECOVERY - Phase 8)
    // ========================================================================
    // Initialize state manager for module checkpointing:
    // - Checkpoint: Serialize brain subsystem states for recovery
    // - Restore: Deserialize states after failure
    // - Validate: Verify state integrity before/after operations
    // - Reset: Return invalid modules to safe default state
    // Registered subsystems: brain_stats, working_memory, executive
    // BIOLOGICAL: Maps to memory consolidation and state preservation
    // DEPENDS ON: Health agent (for heartbeat during checkpoint operations)
    if (!init_state_manager_subsystem(brain)) { brain_destroy(brain); return NULL; }

    // ========================================================================
    // ENHANCED BASAL GANGLIA (ACTION SELECTION & MOTOR CONTROL)
    // ========================================================================
    // Initialize enhanced basal ganglia for biologically-complete action selection:
    // - Core BG: Striatum (D1/D2 MSNs), GPe/GPi, STN, SNc/SNr with DA modulation
    // - Beta oscillations: 13-30 Hz movement suppression, pathological states
    // - Multi-neuromodulators: DA, 5HT, ACh, NE, adenosine interactions
    // - Hierarchical RL: Options framework with temporal abstraction
    // - Model-based planning: World model with MB/MF arbitration
    // - Nucleus accumbens: Wanting/liking, Pavlovian-Instrumental Transfer
    // - Superior colliculus: Saccade generation and orienting
    // - Striatal interneurons: FSI, TAN, LTS timing and modulation
    // - Cerebellar coordination: Timing and error sharing
    // - Outcome devaluation: Goal-directed vs habitual behavior
    // BIOLOGICAL: Maps to subcortical action selection loop
    // DEPENDS ON: Executive (goal-directed), Emotional system (reward), Dragonfly (motor)
    if (!init_basal_ganglia_subsystem(brain)) { brain_destroy(brain); return NULL; }

    // ========================================================================
    // CORE DIRECTIVES (ETHICAL FOUNDATION)
    // ========================================================================

    // Initialize core directives after immune, bio-async, and FEP are ready
    // Core directives must be the FIRST checkpoint before any action execution
    // Implements Asimov's Laws, Golden Rule, and Combinatorial Harm Detection
    // CRITICAL: Skip for minimal_mode to prevent infinite recursion
    // (core_directives -> ethics_engine -> brain_create_minimal -> core_directives...)
    if (!brain->config.minimal_mode) {
        if (!init_core_directives_subsystem(brain)) { brain_destroy(brain); return NULL; }
    }

    // ========================================================================
    // KNOWLEDGE GRAPH READER (SELF-AWARENESS FOUNDATION)
    // ========================================================================
    // Initialize KG reader early - provides structural self-knowledge that
    // other subsystems (introspection, self_model, autobiographical) may query.
    // Loads from .aim/memory-nimcp.jsonl or custom path in config.
    if (!init_kg_reader_subsystem(brain)) { brain_destroy(brain); return NULL; }

    // ========================================================================
    // INTERNAL KNOWLEDGE GRAPH (RUNTIME MODULE MAPPING)
    // ========================================================================
    // Initialize internal KG - provides in-memory CRUD for dynamic module mapping.
    // Security features: token-based access control, integrity checks, immune integration.
    // Complements the static KG reader with a live, mutable module topology graph.
    if (!init_internal_kg_subsystem(brain)) { brain_destroy(brain); return NULL; }

    // ========================================================================
    // COORDINATOR/ORCHESTRATOR SUBSYSTEMS
    // ========================================================================
    // These coordinators provide system-wide coordination across NIMCP:
    // - Bio-Async Orchestrator: Central coordinator for 200+ bio-async modules
    // - Plasticity Coordinator: Unified manager for all plasticity mechanisms
    // - Immune Bridge Coordinator: Central registry for 27+ immune bridges
    // - Cognitive Meta-Controller: Arbitrator for cognitive subsystem resources
    // - Security-Perception Bridge: Sensory threat analysis and defense
    // - Swarm Module Registry: Plugin architecture for swarm behaviors
    //
    // Initialization order matters due to dependencies:
    // 1. Bio-Async Orchestrator (foundation for inter-module messaging)
    // 2. Plasticity Coordinator (depends on bio-async)
    // 3. Immune Bridge Coordinator (depends on bio-async, brain immune)
    // 4. Cognitive Meta-Controller (depends on plasticity, working memory, executive)
    // 5. Security-Perception Bridge (depends on BBB, immune, perception cortices)
    // 6. Swarm Module Registry (depends on all above, swarm_brain)

    // 1. Bio-Async Orchestrator (foundation for messaging)
    if (!init_bio_async_orchestrator_subsystem(brain)) { brain_destroy(brain); return NULL; }

    // 2. Plasticity Coordinator (depends on bio-async)
    if (!init_plasticity_coordinator_subsystem(brain)) { brain_destroy(brain); return NULL; }

    // 2.5 Plasticity Bridges (depend on plasticity coordinator)
    // These bridges connect plasticity mechanisms with higher-level cognitive systems:
    // - STDP-Omni: Forward/backward prediction error → STDP modulation
    // - STDP-PR: Memory resonance/consolidation → STDP gating
    // - Eligibility-PR: Three-factor learning with tag-and-capture
    // - STDP-Quantum: Quantum annealing for learning rate optimization
    if (!init_stdp_omni_bridge_subsystem(brain)) { brain_destroy(brain); return NULL; }
    if (!init_stdp_pr_bridge_subsystem(brain)) { brain_destroy(brain); return NULL; }
    if (!init_eligibility_pr_bridge_subsystem(brain)) { brain_destroy(brain); return NULL; }
    if (!init_stdp_quantum_bridge_subsystem(brain)) { brain_destroy(brain); return NULL; }

    // 3. Immune Bridge Coordinator (depends on bio-async, brain immune)
    if (!init_immune_bridge_coordinator_subsystem(brain)) { brain_destroy(brain); return NULL; }

    // 4. Cognitive Meta-Controller (depends on plasticity, working memory, executive)
    if (!init_cognitive_meta_controller_subsystem(brain)) { brain_destroy(brain); return NULL; }

    // 5. Security-Perception Bridge (depends on BBB, immune, perception cortices)
    if (!init_security_perception_bridge_subsystem(brain)) { brain_destroy(brain); return NULL; }

    // 6. Swarm Module Registry (depends on all above, swarm_brain)
    if (!init_swarm_module_registry_subsystem(brain)) { brain_destroy(brain); return NULL; }

    // ========================================================================
    // EDGE/ROBOT INTEGRATION (Sensor Hub + Safety + Swarm Bridges)
    // ========================================================================
    // Initialize edge deployment subsystems:
    // - Sensor Hub: Unified interface for heterogeneous sensors
    // - Safety Watchdog: Heartbeat-based deadman switch + output validation
    // - Portia-Swarm Bridge: Resource-adaptive collective intelligence
    // - Dragonfly-Swarm Bridge: Coordinated multi-drone pursuit
    // DEPENDS ON: Dragonfly (for swarm bridge), Portia (global singleton)
    if (!init_edge_subsystem(brain)) { brain_destroy(brain); return NULL; }

    // Connect world prior to world model trainer (both created by this point)
    if (NULL /* {field} removed from brain_struct */ && brain->world_model_trainer) {
        extern void nimcp_wmt_set_world_prior(void* wmt, void* world_prior);
        nimcp_wmt_set_world_prior(brain->world_model_trainer, NULL /* {field} removed from brain_struct */);
    }

    // ========================================================================
    // POST-INITIALIZATION
    // ========================================================================
post_init:
    (void)parallel_init_done;  // Suppress unused warning when goto skips sequential
    LOG_MODULE_DEBUG(LOG_MODULE, "Entering post_init");

    // Language Layer: Orchestrator + LNN generator + tokenizer + embeddings
    // Must be in post_init so it runs for both parallel and sequential paths
    if (!brain->lang_generator) {
        LOG_MODULE_DEBUG(LOG_MODULE, "Initializing language subsystem...");
        nimcp_brain_factory_init_language_subsystem(brain);  // Non-fatal if prerequisites missing
        LOG_MODULE_DEBUG(LOG_MODULE, "Language subsystem done");
    }

    /* Initialize learning workspace with reasonable defaults */
    if (!brain->learning_workspace.temp_float) {
        LOG_MODULE_DEBUG(LOG_MODULE, "Initializing learning workspace...");
        uint32_t ws_size = brain->config.num_inputs > brain->config.num_outputs ?
                           brain->config.num_inputs : brain->config.num_outputs;
        if (ws_size < 1024) ws_size = 1024;
        brain->learning_workspace.temp_float = nimcp_calloc(ws_size, sizeof(float));
        brain->learning_workspace.temp_float_capacity = ws_size;
        brain->learning_workspace.temp_uint = nimcp_calloc(ws_size, sizeof(uint32_t));
        brain->learning_workspace.temp_uint_capacity = ws_size;
        LOG_MODULE_DEBUG(LOG_MODULE, "Learning workspace done");
    }

    // ========================================================================
    // WORLD MODEL WIRING (Connect to Active Inference & Imagination)
    // ========================================================================
    if (brain->world_model_enabled) {
        LOG_MODULE_DEBUG(LOG_MODULE, "Wiring world model...");
        if (!wire_world_model_active_inference(brain)) {
            LOG_WARN(LOG_MODULE, "Failed to wire world model to active inference");
        }
        if (!wire_world_model_imagination(brain)) {
            LOG_WARN(LOG_MODULE, "Failed to wire world model to imagination engine");
        }
        LOG_MODULE_DEBUG(LOG_MODULE, "World model wiring done");
    }

    // Save initial snapshot if configured
    if (config->snapshot_dir && config->save_initial_snapshot) {
        brain_save_snapshot(brain, "initial", "Snapshot at brain creation");
    }

    LOG_MODULE_DEBUG(LOG_MODULE, "Post-init final steps...");
    // Phase 11: Quantum annealer (if enabled)
    if (brain->config.enable_quantum_annealing) {
        quantum_annealing_config_t qa_config = {
            .initial_temperature = brain->config.annealing_temperature_init,
            .final_temperature = brain->config.annealing_temperature_final,
            .num_iterations = brain->config.annealing_steps,
            .cooling_schedule = COOLING_EXPONENTIAL,
            .quantum_strength = 0.5F,
            .enable_tunneling = true,
            .seed = (uint32_t)time(NULL)
        };
        brain->quantum_annealer = quantum_annealer_create(&qa_config);
        if (!brain->quantum_annealer) {
            LOG_WARN(LOG_MODULE, "Quantum annealer creation failed! Disabling.");
            brain->config.enable_quantum_annealing = false;
        }
    }

    // P1-9 FIX: Increment bio-async reference counter for this brain.
    // Only the last brain to be destroyed will unregister the global bio-async context.
    extern volatile int g_brain_bio_ref_count;
    __atomic_fetch_add(&g_brain_bio_ref_count, 1, __ATOMIC_ACQ_REL);

    // Register brain with signal handler for crash checkpoint saves
    signal_handler_register_brain(brain);

    brain_clear_error();
    return brain;
}

/**
 * @brief Create brain with preset size and task (convenience wrapper)
 *
 * WHY: Simple API for common use cases - delegates to brain_create_custom
 * This follows the DRY principle - all initialization lives in one place.
 *
 * COMPLEXITY: O(n) where n = num_neurons
 *
 * @param task_name Human-readable name
 * @param size Brain size preset (SMALL/MEDIUM/LARGE)
 * @param task Task template
 * @param num_inputs Input dimension
 * @param num_outputs Output dimension
 * @return Brain handle or NULL on error
 */
brain_t brain_create(const char* task_name, brain_size_t size, brain_task_t task,
                     uint32_t num_inputs, uint32_t num_outputs)
{
    // Guard: Validate parameters
    if (!nimcp_brain_factory_validate_creation_params(task_name, num_inputs, num_outputs)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_create: nimcp_brain_factory_validate_creation_params is NULL");
        return NULL;
    }

    // Build default configuration
    brain_config_t config = {0};

    // Create temporary strategy to get learning rate for config
    task_strategy_t* strategy = strategy_create(task);
    if (!strategy) {
        set_error("Failed to create task strategy");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_create: strategy is NULL");
        return NULL;
    }

    // Initialize config with defaults
    nimcp_brain_factory_init_brain_config(&config, task_name, size, task,
                                          num_inputs, num_outputs, strategy);

    // Strategy will be recreated in brain_create_custom
    strategy_destroy(strategy);

    // Delegate to main implementation
    return brain_create_custom(&config);
}

/**
 * @brief Create minimal brain for fast initialization (test/embedded use)
 *
 * WHY: 5-10x faster initialization by skipping optional cognitive subsystems
 * This is ideal for tests that only need core neural network functionality.
 *
 * COMPLEXITY: O(n) where n = num_neurons (but with smaller constant factor)
 *
 * @param task_name Human-readable name
 * @param size Brain size preset
 * @param task Task template
 * @param num_inputs Input dimension
 * @param num_outputs Output dimension
 * @return Brain handle or NULL on error
 */
brain_t brain_create_minimal(const char* task_name, brain_size_t size, brain_task_t task,
                             uint32_t num_inputs, uint32_t num_outputs)
{
    // Guard: Validate parameters
    if (!nimcp_brain_factory_validate_creation_params(task_name, num_inputs, num_outputs)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_create_minimal: nimcp_brain_factory_validate_creation_params is NULL");
        return NULL;
    }

    // Build configuration with minimal_mode enabled
    brain_config_t config = {0};
    config.minimal_mode = true;  // CRITICAL: Set before init_brain_config

    // Create temporary strategy to get learning rate for config
    task_strategy_t* strategy = strategy_create(task);
    if (!strategy) {
        set_error("Failed to create task strategy");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_create_minimal: strategy is NULL");
        return NULL;
    }

    // Initialize config with defaults (respects minimal_mode flag)
    nimcp_brain_factory_init_brain_config(&config, task_name, size, task,
                                          num_inputs, num_outputs, strategy);

    // Strategy will be recreated in brain_create_custom
    strategy_destroy(strategy);

    // Delegate to main implementation
    return brain_create_custom(&config);
}

/**
 * @brief Create brain with lazy initialization for heavy subsystems
 *
 * WHY: 2-5x faster initialization by deferring heavy subsystems until first use.
 * Unlike minimal_mode, all subsystems ARE enabled - they're just initialized lazily.
 * This is ideal for production use where startup time matters but full functionality
 * is eventually needed.
 *
 * COMPLEXITY: O(n) where n = num_neurons (smaller constant factor initially)
 *
 * @param task_name Human-readable name
 * @param size Brain size preset
 * @param task Task template
 * @param num_inputs Input dimension
 * @param num_outputs Output dimension
 * @return Brain handle or NULL on error
 */
brain_t brain_create_lazy(const char* task_name, brain_size_t size, brain_task_t task,
                          uint32_t num_inputs, uint32_t num_outputs)
{
    // Guard: Validate parameters
    if (!nimcp_brain_factory_validate_creation_params(task_name, num_inputs, num_outputs)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_create_lazy: nimcp_brain_factory_validate_creation_params is NULL");
        return NULL;
    }

    // Build configuration with lazy_init_mode enabled
    brain_config_t config = {0};
    config.lazy_init_mode = true;  // CRITICAL: Set before init_brain_config

    // Create temporary strategy to get learning rate for config
    task_strategy_t* strategy = strategy_create(task);
    if (!strategy) {
        set_error("Failed to create task strategy");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_create_lazy: strategy is NULL");
        return NULL;
    }

    // Initialize config with defaults (respects lazy_init_mode flag)
    nimcp_brain_factory_init_brain_config(&config, task_name, size, task,
                                          num_inputs, num_outputs, strategy);

    // Strategy will be recreated in brain_create_custom
    strategy_destroy(strategy);

    // Delegate to main implementation
    return brain_create_custom(&config);
}

brain_t brain_create_fast(const char* task_name, brain_size_t size, brain_task_t task,
                          uint32_t num_inputs, uint32_t num_outputs)
{
    // Guard: Validate parameters
    if (!nimcp_brain_factory_validate_creation_params(task_name, num_inputs, num_outputs)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_create_fast: invalid params");
        return NULL;
    }

    // Build configuration with fast init mode
    brain_config_t config = {0};
    config.init_mode = BRAIN_INIT_FAST;  // CRITICAL: Set before init_brain_config

    // Create temporary strategy to get learning rate for config
    task_strategy_t* strategy = strategy_create(task);
    if (!strategy) {
        set_error("Failed to create task strategy");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_create_fast: strategy is NULL");
        return NULL;
    }

    // Initialize config with defaults (respects init_mode)
    nimcp_brain_factory_init_brain_config(&config, task_name, size, task,
                                          num_inputs, num_outputs, strategy);

    // Strategy will be recreated in brain_create_custom
    strategy_destroy(strategy);

    // Delegate to main implementation
    return brain_create_custom(&config);
}
