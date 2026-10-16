//=============================================================================
// nimcp_brain_init_memory.c - Memory and Learning Subsystems
//=============================================================================
/**
 * @file nimcp_brain_init_memory.c
 * @brief Memory and Learning Subsystems
 *
 * WHAT: Initialization functions for memory subsystems
 * WHY:  SRP refactoring - separate memory initialization logic
 * HOW:  Each function initializes a specific brain subsystem
 *
 * EXTRACTED FROM: nimcp_brain_init_subsystems.c
 * DATE: 2025-12-08
 *
 * @version 2.7.0
 * @author NIMCP Development Team
 */

//=============================================================================
// Includes
//=============================================================================

#include "core/brain/factory/init/nimcp_brain_init_memory.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/validation/nimcp_validate.h"
#include "utils/error/nimcp_error_codes.h"

#define LOG_MODULE "BRAIN_INIT_MEMORY"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_memory, MESH_ADAPTER_CATEGORY_SYSTEM)


// Compatibility macro for set_error (converts to LOG_ERROR)
#ifndef set_error
#define set_error(msg) LOG_ERROR(LOG_MODULE, "%s", msg)
#endif

// Subsystem includes (add as needed based on functions)
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

//=============================================================================
// Memory and Learning Subsystems Implementation
//=============================================================================


/**
 * @brief Initialize memory consolidation subsystem
 *
 * WHAT: Create and configure background memory consolidation system
 * WHY:  Enable sleep-like memory strengthening and replay
 * HOW:  Start background consolidation thread with configured interval
 *
 * BIOLOGICAL BASIS: Sleep-dependent memory consolidation
 * - Memory replay during sleep/rest periods
 * - Synaptic homeostasis and pruning
 * - Selective strengthening of important memories
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_consolidation_subsystem(brain_t brain)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_consolidation_subsystem: brain is NULL");

            return false;
    }

    // Check if already initialized
    if (brain->consolidation) {
        return true;  // Already initialized
    }

    // Only create if enabled in config
    if (!brain->config.enable_consolidation) {
        return true;  // Not enabled, but not an error
    }

    // Create consolidation config with defaults
    consolidation_config_t consolidation_config = consolidation_default_config();

    // Start background consolidation with 5-minute interval (300 seconds)
    brain->consolidation = brain_start_background_consolidation(
        brain,
        300,  // Consolidate every 5 minutes
        &consolidation_config
    );

    if (!brain->consolidation) {
        set_error("Failed to start background consolidation");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_consolidation_subsystem: brain->consolidation is NULL");
        return false;
    }

    return true;
}


/**
 * @brief Initialize Curiosity Engine subsystem
 *
 * WHAT: Create and configure curiosity-driven exploration system
 * WHY:  Enable novelty detection, knowledge gap detection, and exploration drive
 * HOW:  Create curiosity engine with learner name from brain config
 *
 * BIOLOGICAL BASIS: Intrinsic motivation and curiosity
 * - Dopaminergic response to novelty (midbrain)
 * - Exploration-exploitation trade-off (prefrontal cortex)
 * - Information-seeking behavior (anterior cingulate)
 * - Knowledge gap detection (metacognition)
 *
 * COGNITIVE BENEFITS (from COGNITIVE_AUDIT.md):
 * - 40% faster learning on novel patterns
 * - Intelligent exploration vs exploitation balance
 * - Prioritized learning of novel information
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_curiosity_subsystem(brain_t brain)
{
    // Guard: NULL check
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_curiosity_subsystem: brain is NULL");

            return false;
    }

    // Guard: Check if already initialized
    if (brain->curiosity) {
        return true;  // Already initialized
    }

    // Guard: Only create if enabled in config
    if (!brain->config.enable_curiosity) {
        return true;  // Not enabled, but not an error
    }

    // Create curiosity engine with parent brain reference (module pattern)
    // Curiosity uses parent brain's neuromodulator system instead of creating separate brains
    brain->curiosity = curiosity_engine_create(brain, "nimcp_brain");
    if (!brain->curiosity) {
        set_error("Failed to create curiosity engine");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_curiosity_subsystem: brain->curiosity is NULL");
        return false;
    }

    // Set baseline curiosity to moderate-high (like a curious adult learner)
    // Infants: 0.95, Children: 0.85, Adults: 0.6-0.7
    curiosity_set_baseline(brain->curiosity, 0.7F);

    return true;
}


/**
 * @brief Initialize Salience subsystem (Attention/Relevance Evaluation)
 *
 * WHAT: Create salience evaluator for fast attention/relevance scoring
 * WHY:  Enable brain to quickly determine what inputs deserve attention
 * HOW:  Create salience evaluator with default configuration
 *
 * CAPABILITIES:
 * - Fast salience evaluation (10x faster than full decision)
 * - Novelty detection (never seen before)
 * - Surprise measurement (violated expectations)
 * - Urgency scoring (requires immediate response)
 * - Attention competition in global workspace
 *
 * COGNITIVE BENEFITS:
 * - Selective attention to important stimuli
 * - Reduced computational cost (0.1ms vs 1ms per input)
 * - Emotional-salience integration for mood-biased attention
 * - Surprise-driven arousal modulation
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_salience_subsystem(brain_t brain)
{
    // Guard: NULL check
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_salience_subsystem: brain is NULL");

            return false;
    }

    // Guard: Check if already initialized
    if (brain->salience) {
        return true;  // Already initialized
    }

    // Guard: Only create if enabled in config
    if (!brain->config.enable_salience) {
        return true;  // Not enabled, but not an error
    }

    // Create salience evaluator with default configuration
    salience_config_t config = salience_default_config();
    brain->salience = salience_evaluator_create(brain, &config);

    if (!brain->salience) {
        set_error("Failed to create salience evaluator");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_salience_subsystem: brain->salience is NULL");
        return false;
    }

    return true;
}


/**
 * @brief Initialize Autobiographical Memory subsystem (Phase 12)
 *
 * WHAT: Create episodic self-memory system for continuous identity
 * WHY:  Self-awareness requires memory of "who I was" across time
 * HOW:  Create autobiographical memory with timeline-indexed episodes
 *
 * BIOLOGICAL BASIS: Hippocampal episodic memory system
 * - Timeline-indexed experiences
 * - Emotional tagging (amygdala integration)
 * - Sleep consolidation (memory strengthening/pruning)
 *
 * @param brain Brain instance
 * @return true on success, false on error
 */
bool nimcp_brain_factory_init_autobiographical_memory_subsystem(brain_t brain)
{
    // Guard: NULL check
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_autobiographical_memory_subsystem: brain is NULL");

            return false;
    }

    // Guard: Check if already initialized
    if (brain->autobio) {
        return true;  // Already initialized
    }

    // Create autobiographical memory with 10,000 memory capacity
    brain->autobio = autobio_create(10000);

    if (!brain->autobio) {
        set_error("Failed to create autobiographical memory system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_autobiographical_memory_subsystem: brain->autobio is NULL");
        return false;
    }

    return true;
}


/**
 * @brief Initialize Global Workspace Architecture subsystem
 *
 * WHAT: Create and configure global workspace for conscious access
 * WHY:  Enable broadcast architecture for cross-module information integration
 * HOW:  Create workspace with brain config parameters
 *
 * BIOLOGICAL BASIS: Global Workspace Theory (Baars, 1988; Dehaene, 2011)
 * - Limited-capacity broadcast architecture
 * - Winner-take-all competition for conscious access
 * - Refractory period matches attentional blink (~50ms)
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_global_workspace_subsystem(brain_t brain)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_global_workspace_subsystem: brain is NULL");

            return false;
    }

    // Check if already initialized
    if (brain->global_workspace) {
        return true;  // Already initialized
    }

    // Only create if enabled in config
    if (!brain->config.enable_global_workspace) {
        return true;  // Not enabled, but not an error
    }

    // Create global workspace config from brain config
    global_workspace_config_t gw_config = global_workspace_default_config();

    // Override defaults with brain config if specified
    if (brain->config.workspace_capacity_dim > 0) {
        gw_config.capacity_dim = brain->config.workspace_capacity_dim;
    } else if (brain->config.num_inputs > 0) {
        // Default to brain's input dimension if workspace capacity not specified
        // This ensures workspace accepts same-sized content as brain inputs
        gw_config.capacity_dim = brain->config.num_inputs;
    }
    if (brain->config.workspace_ignition_threshold > 0.0F) {
        gw_config.ignition_threshold = brain->config.workspace_ignition_threshold;
    }
    if (brain->config.workspace_refractory_ms > 0) {
        gw_config.refractory_period_ms = brain->config.workspace_refractory_ms;
    }
    if (brain->config.workspace_history_depth > 0) {
        gw_config.history_depth = brain->config.workspace_history_depth;
    }
    gw_config.enable_history = brain->config.workspace_enable_history;

    // Create global workspace
    brain->global_workspace = global_workspace_create_custom(&gw_config);
    if (!brain->global_workspace) {
        set_error("Failed to create global workspace");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_global_workspace_subsystem: brain->global_workspace is NULL");
        return false;
    }

    // Subscribe key cognitive modules to workspace broadcasts
    // This allows them to receive conscious broadcasts
    if (brain->working_memory) {
        global_workspace_subscribe(brain->global_workspace, MODULE_WORKING_MEMORY);
    }
    if (brain->executive) {
        global_workspace_subscribe(brain->global_workspace, MODULE_EXECUTIVE);
    }
    if (brain->ethics) {
        global_workspace_subscribe(brain->global_workspace, MODULE_ETHICS);
    }
    if (brain->introspection) {
        global_workspace_subscribe(brain->global_workspace, MODULE_INTROSPECTION);
    }
    if (brain->salience) {
        global_workspace_subscribe(brain->global_workspace, MODULE_SALIENCE);
    }
    if (brain->theory_of_mind) {
        global_workspace_subscribe(brain->global_workspace, MODULE_THEORY_OF_MIND);
    }

    // Phase 1.5.3: Enable Shannon information-weighted competition if requested
    // WHAT: Add Shannon entropy monitoring to workspace competition
    // WHY:  High-information, salient content should win workspace access
    // HOW:  Enable Shannon features, set subscriber capacities
    if (brain->enable_shannon_monitoring) {
        shannon_workspace_config_t shannon_config = shannon_workspace_default_config();

        // Enable all Shannon features
        shannon_config.enable_info_weighted_competition = true;
        shannon_config.enable_shannon_monitoring = true;
        shannon_config.enable_adaptive_rate = true;

        if (global_workspace_enable_shannon(brain->global_workspace, &shannon_config)) {
            // Set subscriber capacities based on module complexity
            // Higher capacity = can handle more information flow
            if (brain->working_memory) {
                global_workspace_set_subscriber_capacity(
                    brain->global_workspace, MODULE_WORKING_MEMORY, 150.0F);
            }
            if (brain->executive) {
                global_workspace_set_subscriber_capacity(
                    brain->global_workspace, MODULE_EXECUTIVE, 200.0F);
            }
            if (brain->ethics) {
                global_workspace_set_subscriber_capacity(
                    brain->global_workspace, MODULE_ETHICS, 100.0F);
            }
            if (brain->introspection) {
                global_workspace_set_subscriber_capacity(
                    brain->global_workspace, MODULE_INTROSPECTION, 80.0F);
            }
            if (brain->salience) {
                global_workspace_set_subscriber_capacity(
                    brain->global_workspace, MODULE_SALIENCE, 120.0F);
            }
            if (brain->theory_of_mind) {
                global_workspace_set_subscriber_capacity(
                    brain->global_workspace, MODULE_THEORY_OF_MIND, 100.0F);
            }
        }
    }

    return true;
}

//=============================================================================
// Destruction Functions
//=============================================================================

/**
 * @brief Destroy consolidation subsystem
 *
 * @param brain Brain instance
 */
void nimcp_brain_factory_destroy_consolidation_subsystem(brain_t brain) {
    if (!brain) { NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_destroy_consolidation_subsystem: brain is NULL"); return; }

    if (brain->consolidation) {
        brain_stop_background_consolidation(brain->consolidation);
        brain->consolidation = NULL;
    }

    LOG_DEBUG(LOG_MODULE, "Consolidation subsystem destroyed");
}

/**
 * @brief Destroy curiosity subsystem
 *
 * @param brain Brain instance
 */
void nimcp_brain_factory_destroy_curiosity_subsystem(brain_t brain) {
    if (!brain) { NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_destroy_curiosity_subsystem: brain is NULL"); return; }

    if (brain->curiosity) {
        curiosity_engine_destroy(brain->curiosity);
        brain->curiosity = NULL;
    }

    LOG_DEBUG(LOG_MODULE, "Curiosity subsystem destroyed");
}

/**
 * @brief Destroy salience subsystem
 *
 * @param brain Brain instance
 */
void nimcp_brain_factory_destroy_salience_subsystem(brain_t brain) {
    if (!brain) { NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_destroy_salience_subsystem: brain is NULL"); return; }

    if (brain->salience) {
        salience_evaluator_destroy(brain->salience);
        brain->salience = NULL;
    }

    LOG_DEBUG(LOG_MODULE, "Salience subsystem destroyed");
}

/**
 * @brief Destroy autobiographical memory subsystem
 *
 * @param brain Brain instance
 */
void nimcp_brain_factory_destroy_autobiographical_memory_subsystem(brain_t brain) {
    if (!brain) { NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_destroy_autobiographical_memory_subsystem: brain is NULL"); return; }

    if (brain->autobio) {
        autobio_destroy(brain->autobio);
        brain->autobio = NULL;
    }

    LOG_DEBUG(LOG_MODULE, "Autobiographical memory subsystem destroyed");
}

/**
 * @brief Destroy global workspace subsystem
 *
 * @param brain Brain instance
 */
void nimcp_brain_factory_destroy_global_workspace_subsystem(brain_t brain) {
    if (!brain) { NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_destroy_global_workspace_subsystem: brain is NULL"); return; }

    if (brain->global_workspace) {
        global_workspace_destroy(brain->global_workspace);
        brain->global_workspace = NULL;
    }

    LOG_DEBUG(LOG_MODULE, "Global workspace subsystem destroyed");
}
