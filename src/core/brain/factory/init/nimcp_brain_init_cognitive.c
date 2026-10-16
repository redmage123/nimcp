//=============================================================================
// nimcp_brain_init_cognitive.c - Core Cognitive Subsystems
//=============================================================================
/**
 * @file nimcp_brain_init_cognitive.c
 * @brief Core Cognitive Subsystems
 *
 * WHAT: Initialization functions for cognitive subsystems
 * WHY:  SRP refactoring - separate cognitive initialization logic
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

#include "core/brain/factory/init/nimcp_brain_init_cognitive.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "memory/nimcp_memory_store.h"
#include "memory/nimcp_memory_oodb.h"
#include "cognitive/nimcp_ood_detector.h"
#include "utils/validation/nimcp_validate.h"
#include "utils/error/nimcp_error_codes.h"

#define LOG_MODULE "BRAIN_INIT_COGNITIVE"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_cognitive, MESH_ADAPTER_CATEGORY_SYSTEM)


// Compatibility macro for set_error (converts to LOG_ERROR)
#ifndef set_error
#define set_error(msg) LOG_ERROR(LOG_MODULE, "%s", msg)
#endif

// Subsystem includes (add as needed based on functions)
#include "glial/integration/nimcp_glial_integration.h"
#include "glial/myelin_sheath/nimcp_myelin_sheath.h"
#include "cognitive/sleep_wake/nimcp_sleep_wake_substrate_bridge.h"
/* Do NOT include nimcp_sleep_wake_thalamic_bridge.h here: it typedefs
 * thalamic_router_t against the thalamic_router.h struct forward-decl,
 * but parietal.h (transitively pulled in via cognitive includes above)
 * defines thalamic_router_t as a pointer to thalamic_router_struct. The
 * typedef collision is pre-existing tech debt. We forward-declare the
 * opaque bridge type + extern the two functions we need instead. */
struct sleep_wake_thalamic_bridge;
struct thalamic_router;
struct sleep_wake_thalamic_config;
extern struct sleep_wake_thalamic_bridge* sleep_wake_thalamic_bridge_create(
    void* sleep_wake, struct thalamic_router* router,
    const struct sleep_wake_thalamic_config* config);
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
// Core Cognitive Subsystems Implementation
//=============================================================================


/**
 * WHAT: Initialize symbolic logic reasoning subsystem
 * WHY:  Enable logical inference, knowledge representation, and abstract reasoning
 * HOW:  Create symbolic logic engine if config enables it
 *
 * BIOLOGICAL MOTIVATION:
 * - Prefrontal cortex performs abstract logical reasoning
 * - Hippocampus stores declarative knowledge (facts)
 * - Working memory maintains active inferences
 *
 * INTEGRATION WITH BRAIN:
 * - Stores facts learned during experience
 * - Performs deductive/inductive reasoning
 * - Validates decisions against logical constraints
 * - Enables explanation generation ("because X implies Y")
 *
 * @param brain Brain instance to initialize
 * @return true on success, false on failure
 *
 * @version 2.7.0 Phase 8.9
 * @author NIMCP Development Team
 * @date 2025-11-08
 */
bool nimcp_brain_factory_init_symbolic_logic_subsystem(brain_t brain)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_symbolic_logic_subsystem: brain is NULL");

            return false;
    }

    // Check if already initialized
    if (brain->logic) {
        return true;  // Already initialized
    }

    // Check if symbolic logic is enabled via knowledge system or explicit flag
    // The knowledge system uses logic internally, so enable if knowledge is enabled
    if (!brain->config.enable_knowledge) {
        return true;  // Not enabled, not an error
    }

    // Create neural logic network with spiking logic gates (Phase 9.0)
    neural_logic_config_t logic_config = neural_logic_default_config(1000);
    logic_config.use_gpu = neural_logic_gpu_available();
    logic_config.integration_window_ms = 5.0F;
    logic_config.enable_learning = false;  // Combinational logic (no plasticity)

    brain->logic = neural_logic_create(&logic_config);
    if (!brain->logic) {
        set_error("Failed to create neural logic network");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_symbolic_logic_subsystem: brain->logic is NULL");
        return false;
    }

    return true;
}


/**
 * @brief Initialize symbolic reasoning subsystem (Phase 9.4)
 *
 * WHAT: Creates symbolic logic engine for first-order logic reasoning
 * WHY:  Enable logical inference, consistency checking for communication
 * HOW:  Allocate logic engine with inference and knowledge base capabilities
 *
 * @param brain Brain to initialize
 * @return true on success, false on error
 */
bool nimcp_brain_factory_init_symbolic_reasoning_subsystem(brain_t brain)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_symbolic_reasoning_subsystem: brain is NULL");

            return false;
    }

    // Check if already initialized
    if (brain->symbolic_logic) {
        return true;  // Already initialized
    }

    // Only initialize if explicitly enabled
    if (!brain->config.enable_logic) {
        brain->symbolic_logic = NULL;
        return true;  // Not enabled, not an error
    }

    // Create symbolic logic engine with default configuration
    logic_config_t logic_config = {
        .max_predicates = LOGIC_MAX_PREDICATES,
        .max_rules = LOGIC_MAX_RULES,
        .max_kb_size = 10000,           // 10K facts
        .max_inference_depth = 10,       // Max 10 inference steps
        .enable_forward_chaining = true,
        .enable_backward_chaining = true,
        .enable_resolution = true,
        .enable_memory_consolidation = false  // Handled by brain->consolidation
    };

    brain->symbolic_logic = symbolic_logic_create(&logic_config);
    if (!brain->symbolic_logic) {
        set_error("Failed to create symbolic logic engine");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_symbolic_reasoning_subsystem: brain->symbolic_logic is NULL");
        return false;
    }

    return true;
}


/**
 * @brief Initialize epistemic filtering subsystem (Phase 9.2)
 *
 * WHAT: Creates epistemic filter for cognitive bias prevention
 * WHY:  Prevents conspiracy-theory thinking and cognitive biases
 * HOW:  Applies skepticism, evidence evaluation, bias detection
 *
 * @param brain Brain to initialize
 * @return true on success, false on error
 */
bool nimcp_brain_factory_init_epistemic_subsystem(brain_t brain)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_epistemic_subsystem: brain is NULL");

            return false;
    }

    // Check if already initialized
    if (brain->epistemic) {
        return true;  // Already initialized
    }

    // Epistemic filtering is recommended for all brains to prevent
    // accepting unproven information or developing biased reasoning

    // Skepticism level:
    // 0.0 = credulous (accepts most claims)
    // 0.5 = balanced (reasonable skepticism)
    // 0.7 = cautious (requires strong evidence)
    // 1.0 = extreme skeptic (rejects almost everything)
    //
    // We default to 0.6 (cautious but not paranoid)
    float skepticism_level = 0.6F;

    brain->epistemic = epistemic_filter_create(skepticism_level);
    if (!brain->epistemic) {
        set_error("Failed to create epistemic filter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_epistemic_subsystem: brain->epistemic is NULL");
        return false;
    }

    return true;
}


/**
 * @brief Initialize working memory subsystem (Phase 10.2)
 *
 * WHAT: Creates Miller's 7±2 working memory buffer with temporal decay
 * WHY:  Provides active representation buffer for reasoning and planning
 * HOW:  Uses config settings or defaults (capacity=7, tau=1000ms)
 *
 * @param brain Brain instance
 * @return true on success, false on error
 */
bool nimcp_brain_factory_init_working_memory_subsystem(brain_t brain)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_working_memory_subsystem: brain is NULL");

            return false;
    }

    // Check if already initialized
    if (brain->working_memory) {
        return true;  // Already initialized
    }

    // Only create if enabled in config
    if (!brain->config.enable_working_memory) {
        return true;  // Not enabled, but not an error
    }

    // Create custom config from brain config
    working_memory_config_t wm_config = working_memory_default_config();

    // Override defaults with brain config if specified
    if (brain->config.working_memory_capacity > 0) {
        wm_config.capacity = brain->config.working_memory_capacity;
    }
    if (brain->config.working_memory_decay_tau_ms > 0.0F) {
        wm_config.decay_tau_ms = brain->config.working_memory_decay_tau_ms;
    }

    brain->working_memory = working_memory_create_custom(&wm_config);
    if (!brain->working_memory) {
        set_error("Failed to create working memory");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_brain_factory_init_working_memory_subsystem: brain->working_memory is NULL");
        return false;
    }

    /* =========================================================================
     * PHASE 10.2: Emotional System Initialization
     * =========================================================================
     * WHAT: Create integrated emotional processing system
     * WHY:  Enable emotional tagging, regulation, salience boost, mental health
     * HOW:  Create with defaults (Russell's Circumplex Model + CBT/DBT regulation)
     */

    brain->emotional_system = emotion_system_create(NULL);  // NULL = use defaults
    if (!brain->emotional_system) {
        set_error("Failed to create emotional system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_working_memory_subsystem: brain->emotional_system is NULL");
        goto cleanup_wm;
    }

    /* =========================================================================
     * PHASE 10.1: Sleep/Wake Cycle Initialization
     * =========================================================================
     * WHAT: Create sleep system and connect to brain
     * WHY:  Enable memory consolidation and synaptic homeostasis
     * HOW:  Create with defaults, link brain reference for working memory access
     */

    // Create sleep system with default configuration
    brain->sleep_system = sleep_system_create(NULL);  // NULL = use defaults
    if (!brain->sleep_system) {
        set_error("Failed to create sleep system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_working_memory_subsystem: brain->sleep_system is NULL");
        goto cleanup_emotional;
    }

    // Connect brain reference for working memory access (Phase 10.3 integration)
    sleep_set_brain_reference(brain->sleep_system, (void*)brain);

    /* Sleep-wake bridges: couple sleep cycle to substrate (ATP→pressure) and
     * thalamic router (arousal→attention gating). Both NULL-tolerant —
     * absence of substrate/thalamic just skips bridge creation. */
    brain->sleep_wake_substrate_bridge = NULL;
    brain->sleep_wake_thalamic_bridge  = NULL;
    if (brain->substrate_enabled && brain->substrate) {
        brain->sleep_wake_substrate_bridge = sleep_wake_substrate_bridge_create(
            brain->sleep_system, brain->substrate, NULL);
        if (brain->sleep_wake_substrate_bridge) {
            LOG_INFO("[sleep_wake] substrate bridge created");
        } else {
            LOG_WARN("[sleep_wake] substrate bridge creation failed — sleep will not read ATP");
        }
    }
    if (brain->thalamic_router_enabled && brain->thalamic_router) {
        brain->sleep_wake_thalamic_bridge = sleep_wake_thalamic_bridge_create(
            brain->sleep_system, brain->thalamic_router, NULL);
        if (brain->sleep_wake_thalamic_bridge) {
            LOG_INFO("[sleep_wake] thalamic bridge created");
        } else {
            LOG_WARN("[sleep_wake] thalamic bridge creation failed — arousal will not gate attention");
        }
    }

    /* =========================================================================
     * PHASE M1: Memory Engram System Initialization
     * =========================================================================
     * WHAT: Create engram system for distributed memory traces
     * WHY:  Enable biological memory encoding, consolidation, and pattern completion
     * HOW:  Create with default capacity (512 engrams), integrates with sleep/emotion
     */

    // Create engram system with default capacity (512 engrams)
    brain->engram_system = engram_system_create();
    if (!brain->engram_system) {
        set_error("Failed to create engram system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_working_memory_subsystem: brain->engram_system is NULL");
        goto cleanup_sleep;
    }

    /**
     * PHASE M1b: PERSISTENT MEMORY STORE + OODB + OOD DETECTOR
     *
     * WHAT: Create SQLite-backed persistent memory, OODB cache, and OOD detector
     * WHY:  Enable cross-session memory persistence, fast in-memory object graph,
     *       and out-of-distribution detection during inference
     * HOW:  Create store with brain's checkpoint path, wrap with OODB, create OOD
     */
    {
        nimcp_memory_store_config_t ms_config = nimcp_memory_store_config_default();
        /* Use checkpoint directory for the memory DB if available */
        char db_path[512];
        snprintf(db_path, sizeof(db_path), "%s/athena_memory.db",
                 brain->config.task_name[0] ? "checkpoints/athena" : "/tmp");
        ms_config.db_path = db_path;
        ms_config.enable_questdb_sync = false; /* Enable later when QuestDB is confirmed running */

        brain->memory_store = (struct nimcp_memory_store*)nimcp_memory_store_create(&ms_config);
        if (brain->memory_store) {
            NIMCP_LOGGING_INFO("Persistent memory store created: %s", db_path);

            /* OODB cache wrapping the store */
            nimcp_oodb_config_t oodb_config = nimcp_oodb_config_default();
            brain->memory_oodb = (struct nimcp_oodb*)nimcp_oodb_create(
                &oodb_config, (nimcp_memory_store_t*)brain->memory_store);
            if (brain->memory_oodb) {
                NIMCP_LOGGING_INFO("OODB memory cache created (max=%u objects)",
                                   oodb_config.max_cached_objects);
            }
        }

        /* OOD detector (persistent, reused across inferences) */
        nimcp_ood_config_t ood_config = nimcp_ood_config_default();
        brain->ood_detector = (struct nimcp_ood_detector*)nimcp_ood_detector_create(&ood_config);
        if (brain->ood_detector) {
            NIMCP_LOGGING_INFO("OOD detector created (threshold=%.2f)", ood_config.ood_threshold);
        }
    }

    /**
     * PHASE M2: SYSTEMS CONSOLIDATION INITIALIZATION
     *
     * WHAT: Create systems consolidation system for hippocampus → cortex transfer
     * WHY:  Enable sleep-dependent memory consolidation and semantic abstraction
     * HOW:  Create with default capacities, link to engram and sleep systems
     */

    // Create systems consolidation system with default capacity (2048 cortical nodes)
    brain->systems_consolidation = systems_consolidation_create();
    if (!brain->systems_consolidation) {
        set_error("Failed to create systems consolidation system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_working_memory_subsystem: brain->systems_consolidation is NULL");
        goto cleanup_engram;
    }

    // Link to Phase M1 engram system (source of memories to consolidate)
    systems_consolidation_set_engram_system(brain->systems_consolidation, brain->engram_system);

    // Link to sleep-wake cycle system (controls consolidation rate and replay)
    systems_consolidation_set_sleep_system(brain->systems_consolidation, brain->sleep_system);

    /**
     * PHASE M3: WORKING MEMORY TRANSFER INITIALIZATION
     *
     * WHAT: Create WM transfer system for working memory → engram encoding
     * WHY:  Enable selective consolidation based on rehearsal, attention, and emotion
     * HOW:  Create system, link to working memory, engrams, and emotional tagging
     */

    // Create working memory transfer system with default criteria
    brain->wm_transfer_system = wm_transfer_create();
    if (!brain->wm_transfer_system) {
        set_error("Failed to create working memory transfer system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_working_memory_subsystem: brain->wm_transfer_system is NULL");
        goto cleanup_systems_consol;
    }

    // Link to Phase 10.1 working memory (source of temporary information)
    wm_transfer_set_working_memory(brain->wm_transfer_system, brain->working_memory);

    // Link to Phase M1 engram system (destination for transferred memories)
    wm_transfer_set_engram_system(brain->wm_transfer_system, brain->engram_system);

    // Link to Phase 10.2 emotional tagging system (emotional salience for encoding)
    wm_transfer_set_emotional_system(brain->wm_transfer_system, brain->emotional_system);

    /*
     * PHASE M4: SEMANTIC MEMORY NETWORK INITIALIZATION
     *
     * WHAT: Create semantic memory for concept network and spreading activation
     * WHY:  Enable abstract reasoning, inference, and knowledge retrieval
     * HOW:  Create system, link to Phase M2 for concept extraction
     */

    // Create semantic memory network system
    brain->semantic_memory = semantic_memory_create();
    if (!brain->semantic_memory) {
        set_error("Failed to create semantic memory network");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_brain_factory_init_working_memory_subsystem: brain->semantic_memory is NULL");
        goto cleanup_wm_transfer;
    }

    // Link to Phase M2 systems consolidation (source of semantic concepts)
    semantic_memory_set_consolidation(brain->semantic_memory, brain->systems_consolidation);

    return true;

    /* Rollback labels — destroy in reverse creation order */
cleanup_wm_transfer:
    wm_transfer_destroy(brain->wm_transfer_system);
    brain->wm_transfer_system = NULL;
cleanup_systems_consol:
    systems_consolidation_destroy(brain->systems_consolidation);
    brain->systems_consolidation = NULL;
cleanup_engram:
    engram_system_destroy(brain->engram_system);
    brain->engram_system = NULL;
cleanup_sleep:
    sleep_system_destroy(brain->sleep_system);
    brain->sleep_system = NULL;
cleanup_emotional:
    emotion_system_destroy(brain->emotional_system);
    brain->emotional_system = NULL;
cleanup_wm:
    working_memory_destroy(brain->working_memory);
    brain->working_memory = NULL;
    return false;
}


/**
 * @brief Initialize executive functions subsystem (Phase 10.3)
 *
 * WHAT: Create executive controller for task management
 * WHY:  Enable goal-directed behavior and multi-tasking
 * HOW:  Create with defaults or custom config from brain
 */
bool nimcp_brain_factory_init_executive_subsystem(brain_t brain)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_executive_subsystem: brain is NULL");

            return false;
    }

    // Check if already initialized
    if (brain->executive) {
        return true;  // Already initialized
    }

    // Only create if enabled in config
    if (!brain->config.enable_executive_control) {
        return true;  // Not enabled, but not an error
    }

    // Create executive controller
    brain->executive = executive_create();
    if (!brain->executive) {
        set_error("Failed to create executive controller");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_executive_subsystem: brain->executive is NULL");
        return false;
    }

    return true;
}


/**
 * @brief Initialize Theory of Mind subsystem (Phase 10.6)
 *
 * WHAT: Create Theory of Mind module for social cognition
 * WHY:  Enable understanding of others' beliefs, goals, and emotions
 * HOW:  Create ToM with brain reference for self-model
 *
 * COMPLEXITY: O(1)
 * MEMORY: O(1) - small fixed structures for BDI model
 *
 * @param brain Brain instance
 * @return true on success, false on error
 */
bool nimcp_brain_factory_init_theory_of_mind_subsystem(brain_t brain)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_theory_of_mind_subsystem: brain is NULL");

            return false;
    }

    // Check if already initialized
    if (brain->theory_of_mind) {
        return true;  // Already initialized
    }

    // Only create if enabled in config
    if (!brain->config.enable_theory_of_mind) {
        return true;  // Not enabled, but not an error
    }

    // Create Theory of Mind module with brain reference for self-model
    brain->theory_of_mind = tom_create(brain);
    if (!brain->theory_of_mind) {
        set_error("Failed to create Theory of Mind module");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_theory_of_mind_subsystem: brain->theory_of_mind is NULL");
        return false;
    }

    return true;
}


/**
 * @brief Initialize Natural Explanations subsystem (Phase 10.7)
 *
 * WHAT: Create explanation generator for human-readable AI interpretability
 * WHY:  Enable "what-why-how" explanations of brain decisions
 * HOW:  Create explanation_generator with config-driven generation
 *
 * COMPLEXITY: O(1)
 * MEMORY: O(1) - small fixed structures for explanation templates
 *
 * @param brain Brain instance
 * @return true on success, false on error
 */
bool nimcp_brain_factory_init_natural_explanations_subsystem(brain_t brain)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_natural_explanations_subsystem: brain is NULL");

            return false;
    }

    // Check if already initialized
    if (brain->explanation_gen) {
        return true;  // Already initialized
    }

    // Only create if enabled in config
    if (!brain->config.enable_natural_explanations) {
        return true;  // Not enabled, but not an error
    }

    // Create explanation config from brain config flags
    explanation_config_t exp_config = explanation_default_config();
    exp_config.generate_what = brain->config.enable_natural_explanations;
    exp_config.generate_why = brain->config.enable_natural_explanations;
    exp_config.generate_how = brain->config.enable_natural_explanations;
    exp_config.generate_confidence = brain->config.enable_natural_explanations;
    exp_config.generate_alternatives = brain->config.enable_natural_explanations;
    exp_config.generate_counterfactuals = brain->config.enable_causal_explanations;
    exp_config.use_symbolic_logic = (brain->symbolic_logic != NULL);

    // Create Natural Explanations module
    brain->explanation_gen = explanation_generator_create(&exp_config);
    if (!brain->explanation_gen) {
        set_error("Failed to create Natural Explanations module");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_natural_explanations_subsystem: brain->explanation_gen is NULL");
        return false;
    }

    return true;
}


/**
 * @brief Initialize Meta-Learning subsystem (Phase 10.8)
 *
 * WHAT: Create MAML meta-learner for few-shot learning
 * WHY:  Enable rapid adaptation from 1, 5, or 10 examples
 * HOW:  Initialize meta-learner with adaptive learning rates per region
 *
 * COMPLEXITY: O(num_regions)
 * MEMORY: O(1) - small fixed structures for MAML state
 *
 * @param brain Brain instance
 * @return true on success, false on error
 */
bool nimcp_brain_factory_init_meta_learning_subsystem(brain_t brain)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_meta_learning_subsystem: brain is NULL");

            return false;
    }

    // Check if already initialized
    if (brain->meta_learner) {
        return true;  // Already initialized
    }

    // Only create if enabled in config
    if (!brain->config.enable_meta_learning) {
        return true;  // Not enabled, but not an error
    }

    // Create meta-learning config from brain config
    meta_learning_config_t meta_config = meta_learning_default_config();
    meta_config.few_shot_k = (few_shot_mode_t)brain->config.meta_k_shot;
    meta_config.enable_adaptive_lr = brain->config.enable_adaptive_meta_lr;
    meta_config.enable_task_similarity = true;

    // Determine number of regions (simplified: 3 main regions)
    uint32_t num_regions = 3;  // Sensory, Association, Prefrontal

    // Create Meta-Learning module
    brain->meta_learner = meta_learner_create(&meta_config, num_regions);
    if (!brain->meta_learner) {
        set_error("Failed to create Meta-Learning module");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_meta_learning_subsystem: brain->meta_learner is NULL");
        return false;
    }

    return true;
}


/**
 * @brief Initialize Mental Health subsystem (Phase 10.5)
 *
 * WHAT: Create mental health monitor for disorder detection
 * WHY:  Track cognitive health and prevent harmful states
 * HOW:  Initialize monitor with 9 disorder detectors
 *
 * COMPLEXITY: O(1)
 * MEMORY: O(1) - fixed structures for monitoring state
 *
 * @param brain Brain instance
 * @return true on success, false on error
 */
bool nimcp_brain_factory_init_mental_health_subsystem(brain_t brain)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_mental_health_subsystem: brain is NULL");

            return false;
    }

    // Check if already initialized
    if (brain->mental_health_monitor) {
        return true;  // Already initialized
    }

    // Only create if enabled in config
    if (!brain->config.enable_mental_health_monitoring) {
        return true;  // Not enabled, but not an error
    }

    // Create Mental Health monitor with default config
    brain->mental_health_monitor = mental_health_create_default();
    if (!brain->mental_health_monitor) {
        set_error("Failed to create Mental Health monitor");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_mental_health_subsystem: brain->mental_health_monitor is NULL");
        return false;
    }

    /* W11: wire brain back-reference so disorder/intervention events emit to KG. */
    mental_health_set_brain_ref(brain->mental_health_monitor, brain);

    return true;
}


/**
 * @brief Initialize Trauma Resilience subsystem
 *
 * WHAT: Create recall dampening + arousal homeostasis system
 * WHY:  Prevent PTSD-like feedback loops from involuntary recall
 * HOW:  Per-engram frequency tracking, habituation, arousal regulation
 *
 * COMPLEXITY: O(1)
 * MEMORY: ~5KB (tracking buffers + config)
 *
 * @param brain Brain instance
 * @return true on success, false on error
 */
bool nimcp_brain_factory_init_trauma_resilience(brain_t brain)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_brain_factory_init_trauma_resilience: brain is NULL");
        return false;
    }

    if (brain->trauma_resilience) {
        return true;  /* Already initialized */
    }

    extern void* nimcp_trauma_resilience_create(const void*);
    brain->trauma_resilience = nimcp_trauma_resilience_create(NULL);
    if (!brain->trauma_resilience) {
        LOG_WARN(LOG_MODULE, "Failed to create trauma resilience system (non-fatal)");
        return true;  /* Non-fatal — brain can operate without it */
    }

    LOG_INFO(LOG_MODULE, "Trauma resilience system initialized");
    return true;
}


/**
 * @brief Initialize Predictive Processing subsystem (Phase 10.9)
 *
 * WHAT: Create hierarchical predictive coding network
 * WHY:  Enable free energy minimization and active inference
 * HOW:  Initialize multi-layer predictive network
 *
 * COMPLEXITY: O(sum(layer_sizes))
 * MEMORY: O(sum(layer_sizes)) - hierarchical state space
 *
 * @param brain Brain instance
 * @return true on success, false on error
 */
bool nimcp_brain_factory_init_predictive_subsystem(brain_t brain)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_predictive_subsystem: brain is NULL");

            return false;
    }

    // Check if already initialized
    if (brain->predictive_network) {
        return true;  // Already initialized
    }

    // Only create if enabled in config
    if (!brain->config.enable_predictive_processing) {
        return true;  // Not enabled, but not an error
    }

    // Create Predictive network with default config
    brain->predictive_network = predictive_create(NULL);  // NULL = use defaults
    if (!brain->predictive_network) {
        set_error("Failed to create Predictive Processing network");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_predictive_subsystem: brain->predictive_network is NULL");
        return false;
    }

    return true;
}


/**
 * @brief Initialize mirror neuron system for brain
 *
 * WHAT: Create and configure mirror neuron system for observation-based learning
 * WHY:  Enable social cognition, imitation learning, and action understanding
 * HOW:  Create mirror_neurons_t with config-specified parameters
 *
 * @param brain Brain to initialize mirror neurons for
 * @return true on success, false on error
 */
bool nimcp_brain_factory_init_mirror_neurons(brain_t brain)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_mirror_neurons: brain is NULL");

            return false;
    }

    // Don't re-initialize
    if (brain->mirror_neurons) {
        return true;  // Already initialized
    }

    // Only create if enabled in config
    if (!brain->config.enable_mirror_neurons) {
        return true;  // Not enabled, but not an error
    }

    // Create mirror neuron config from brain config
    mirror_neuron_config_t mirror_config = mirror_neurons_get_default_config();
    // Only override defaults if values are explicitly set (non-zero)
    if (brain->config.mirror_neuron_count > 0) {
        mirror_config.num_mirror_neurons = brain->config.mirror_neuron_count;
    }
    if (brain->config.mirror_max_actions > 0) {
        mirror_config.max_actions = brain->config.mirror_max_actions;
    }
    if (brain->config.mirror_max_agents > 0) {
        mirror_config.max_agents = brain->config.mirror_max_agents;
    }
    if (brain->config.mirror_learning_rate > 0.0F) {
        mirror_config.learning_rate = brain->config.mirror_learning_rate;
    }
    if (brain->config.mirror_match_threshold > 0.0F) {
        mirror_config.match_threshold = brain->config.mirror_match_threshold;
    }

    // Enable integration with other cognitive systems
    mirror_config.enable_working_memory = brain->config.enable_working_memory;
    mirror_config.enable_theory_of_mind = brain->config.enable_theory_of_mind;
    mirror_config.enable_prediction = brain->config.enable_predictive_processing;

    // Create mirror neuron system
    brain->mirror_neurons = mirror_neurons_create(&mirror_config);
    if (!brain->mirror_neurons) {
        set_error("Failed to create mirror neuron system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_mirror_neurons: brain->mirror_neurons is NULL");
        return false;
    }

    // Integrate with other cognitive systems if they exist
    if (brain->working_memory && mirror_config.enable_working_memory) {
        mirror_neurons_integrate_working_memory(brain->mirror_neurons, brain->working_memory);
    }

    if (brain->theory_of_mind && mirror_config.enable_theory_of_mind) {
        mirror_neurons_integrate_theory_of_mind(brain->mirror_neurons, brain->theory_of_mind);
    }

    if (brain->predictive_network && mirror_config.enable_prediction) {
        mirror_neurons_integrate_predictive(brain->mirror_neurons, brain->predictive_network);
    }

    return true;
}
