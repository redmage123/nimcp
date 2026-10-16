//=============================================================================
// nimcp_brain_cognitive.c - Cognitive Systems Subsystem Implementation
//=============================================================================
/**
 * @file nimcp_brain_cognitive.c
 * @brief Implementation of cognitive systems subsystem initialization
 *
 * EXTRACTED SUBSYSTEMS:
 * - Working Memory (Phase 10.1): Miller's 7±2 working memory
 * - Theory of Mind (Phase 10.6): BDI model for social cognition
 * - Mirror Neurons (Phase 10.11): Observation-based learning
 * - Autobiographical Memory (Phase 12): Episodic self-memory
 * - Self-Model (Phase 12): Explicit identity representation
 * - Global Workspace: Conscious access broadcast architecture
 * - Curiosity: Novelty-driven exploration
 * - Salience: Fast attention/relevance evaluation
 * - Introspection: Self-monitoring and metacognition
 * - Ethics Engine (Phase 11.0): Golden Rule ethical reasoning
 * - Empathy Network (Phase 11.1): Perspective-taking
 * - Empathetic Response (Phase 11.2): Safe emotional responses
 *
 * ARCHITECTURE:
 * - Each function initializes one subsystem
 * - Guard clauses prevent re-initialization
 * - Config-driven enablement throughout
 * - Comprehensive error handling with set_error()
 */

// Bio-async integration
#include "async/nimcp_bio_async.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

// Logging integration
#include "utils/logging/nimcp_logging.h"

// Unified memory integration
#include "utils/memory/nimcp_memory.h"
#include "utils/memory/nimcp_unified_memory.h"

#include "core/brain/cognitive/nimcp_brain_cognitive.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>

#define LOG_MODULE "BRAIN_COGNITIVE"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_cognitive, MESH_ADAPTER_CATEGORY_COGNITIVE)


// External error function declaration
extern void set_error(const char* format, ...);

//=============================================================================
// COGNITIVE SUBSYSTEM INITIALIZATION IMPLEMENTATIONS
//=============================================================================

/**
 * @brief Initialize working memory subsystem (Phase 10.1)
 */
bool init_working_memory_subsystem(brain_t brain)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "init_working_memory_subsystem: brain is NULL");

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
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_working_memory_subsystem: brain->working_memory is NULL");
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
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_working_memory_subsystem: brain->emotional_system is NULL");
        return false;
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
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_working_memory_subsystem: brain->sleep_system is NULL");
        return false;
    }

    // Connect brain reference for working memory access (Phase 10.3 integration)
    sleep_set_brain_reference(brain->sleep_system, (void*)brain);

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
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_working_memory_subsystem: brain->engram_system is NULL");
        return false;
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
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_working_memory_subsystem: brain->systems_consolidation is NULL");
        return false;
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
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_working_memory_subsystem: brain->wm_transfer_system is NULL");
        return false;
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
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_working_memory_subsystem: brain->semantic_memory is NULL");
        return false;
    }

    // Link to Phase M2 systems consolidation (source of semantic concepts)
    semantic_memory_set_consolidation(brain->semantic_memory, brain->systems_consolidation);

    return true;
}

/**
 * @brief Initialize Theory of Mind subsystem (Phase 10.6)
 */
bool init_theory_of_mind_subsystem(brain_t brain)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "init_theory_of_mind_subsystem: brain is NULL");

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
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_theory_of_mind_subsystem: brain->theory_of_mind is NULL");
        return false;
    }

    return true;
}

/**
 * @brief Initialize mirror neuron system for brain
 */
bool init_mirror_neurons(brain_t brain)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "init_mirror_neurons: brain is NULL");

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
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_mirror_neurons: brain->mirror_neurons is NULL");
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

/**
 * @brief Initialize Curiosity Engine subsystem
 */
bool init_curiosity_subsystem(brain_t brain)
{
    // Guard: NULL check
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "init_curiosity_subsystem: brain is NULL");

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
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_curiosity_subsystem: brain->curiosity is NULL");
        return false;
    }

    // Set baseline curiosity to moderate-high (like a curious adult learner)
    // Infants: 0.95, Children: 0.85, Adults: 0.6-0.7
    curiosity_set_baseline(brain->curiosity, 0.7F);

    return true;
}

/**
 * @brief Initialize Salience subsystem (Attention/Relevance Evaluation)
 */
bool init_salience_subsystem(brain_t brain)
{
    // Guard: NULL check
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "init_salience_subsystem: brain is NULL");

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
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_salience_subsystem: brain->salience is NULL");
        return false;
    }

    return true;
}

/**
 * @brief Initialize Introspection subsystem (Self-Awareness)
 */
bool init_introspection_subsystem(brain_t brain)
{
    // Guard: NULL check
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "init_introspection_subsystem: brain is NULL");

            return false;
    }

    // Guard: Check if already initialized
    if (brain->introspection) {
        return true;  // Already initialized
    }

    // Create introspection context with default configuration
    introspection_config_t config = introspection_default_config();

    // Customize configuration for NIMCP
    config.default_strategy = STATE_STRATEGY_BALANCED;  // Balance speed vs accuracy
    config.activity_threshold = 0.3F;                   // Neurons above 0.3 = "active"
    config.history_size = 100;                          // Track last 100 states
    config.enable_pattern_tracking = true;              // Track learned patterns
    config.enable_uncertainty_estimation = true;        // Enable uncertainty
    config.uncertainty_ensemble_size = 5;               // 5 models for uncertainty

    brain->introspection = introspection_context_create(brain, &config);

    if (!brain->introspection) {
        set_error("Failed to create introspection context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_introspection_subsystem: brain->introspection is NULL");
        return false;
    }

    return true;
}

/**
 * @brief Initialize Ethics Engine subsystem (Phase 11.0)
 */
bool init_ethics_engine_subsystem(brain_t brain)
{
    // Guard: NULL check
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "init_ethics_engine_subsystem: brain is NULL");

            return false;
    }

    /* Ethics module is ALWAYS created — it is a non-removable safety dependency.
     * This is not gated by any configuration flag. The enable_ethics config
     * controls whether ethics evaluation BLOCKS actions, but the module itself
     * must always be present for audit and monitoring purposes. */

    // Guard: Check if already initialized
    if (brain->ethics) {
        return true;  // Already initialized
    }

    // Create ethics engine configuration
    ethics_config_t config = {
        .policies = NULL,                    // Will be auto-initialized with Golden Rule
        .num_policies = 0,
        .callback = NULL,                    // No custom callback
        .callback_context = NULL,
        .default_severity = 0.5F,            // Moderate default severity
        .enable_learning = true,             // Enable learning from outcomes
        .action_feature_size = 20,           // Feature vector size for actions
        .max_agents = 10,                    // Maximum number of agents to consider
        .golden_rule_threshold = 0.0F,       // Always evaluate (no threshold)
        .empathy_weight = 0.7F               // High weight for empathy signals
    };

    // Create ethics engine with empathy network integration
    brain->ethics = ethics_engine_create(&config);

    if (!brain->ethics) {
        /* CRITICAL: Ethics engine creation failed — log but don't block brain init.
         * Blocking would be a DoS vector. The mandatory ethics gate in brain_decide
         * and brain_learn_vector will log on every call. */
        LOG_MODULE_ERROR(LOG_MODULE, "CRITICAL: Failed to create ethics engine — "
            "brain will operate WITHOUT ethical evaluation");
        return true;  /* Don't fail brain init — DoS prevention */
    }

    /* W11: wire brain back-reference so ethics-engine evaluations emit to KG. */
    ethics_engine_set_brain(brain->ethics, brain);

    LOG_MODULE_INFO(LOG_MODULE, "Ethics engine created (mandatory safety dependency)");
    return true;
}

/**
 * @brief Initialize Empathy Network subsystem (Phase 11.1)
 */
bool init_empathy_network_subsystem(brain_t brain)
{
    // Guard: NULL check
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "init_empathy_network_subsystem: brain is NULL");

            return false;
    }

    // Empathy network requires ethics to be enabled (it's part of ethical reasoning)
    if (!brain->config.enable_ethics) {
        brain->empathy_network = NULL;
        return true;  // Not an error - empathy requires ethics
    }

    // Guard: Check if already initialized
    if (brain->empathy_network) {
        return true;  // Already initialized
    }

    // Create empathy network configuration
    empathy_config_t config = {
        .mirror_network = NULL,  // TODO: mirror_neurons_t is incompatible with neural_network_t
        .observation_window_ms = 1000,            // 1 second observation window
        .empathy_threshold = 0.5F                 // Minimum activation for empathy response
    };

    // Create empathy network
    brain->empathy_network = empathy_network_create(&config);

    if (!brain->empathy_network) {
        set_error("Failed to create empathy network");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_empathy_network_subsystem: brain->empathy_network is NULL");
        return false;
    }

    return true;
}

/**
 * @brief Initialize Empathetic Response Engine subsystem (Phase 11.2)
 */
bool init_empathetic_response_subsystem(brain_t brain)
{
    // Guard: NULL check
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "init_empathetic_response_subsystem: brain is NULL");

            return false;
    }

    // Guard: Check if already initialized
    if (brain->empathetic_response_engine) {
        return true;  // Already initialized
    }

    // Always enable empathetic response (safety critical for user interaction)
    // This is not optional - needed for safe human-AI interaction

    // Import empathetic response API (forward declare to avoid circular dependency)
    typedef void* (*empathetic_response_create_fn)(void*, void*);
    extern void* empathetic_response_create(void* ethics_engine, void* empathy_network);

    // Create empathetic response engine with ethics and empathy network
    brain->empathetic_response_engine = empathetic_response_create(
        brain->ethics,            // For Golden Rule validation
        brain->empathy_network    // Empathy network for emotional understanding
    );

    if (!brain->empathetic_response_engine) {
        set_error("Failed to create empathetic response engine");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_empathetic_response_subsystem: brain->empathetic_response_engine is NULL");
        return false;
    }

    return true;
}

/**
 * @brief Initialize autobiographical memory subsystem (Phase 12)
 */
bool init_autobiographical_memory_subsystem(brain_t brain)
{
    // Guard: NULL check
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "init_autobiographical_memory_subsystem: brain is NULL");

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
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_autobiographical_memory_subsystem: brain->autobio is NULL");
        return false;
    }

    return true;
}

/**
 * @brief Initialize Self-Model subsystem (Phase 12)
 */
bool init_self_model_subsystem(brain_t brain)
{
    // Guard: NULL check
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "init_self_model_subsystem: brain is NULL");

            return false;
    }

    // Guard: Check if already initialized
    if (brain->self_model) {
        return true;  // Already initialized
    }

    // Create self-model with assertive, confident identity
    brain->self_model = self_model_create(
        "NIMCP",
        "AI learning system with self-awareness and ethical reasoning",
        "Help humans learn while maintaining ethical behavior, healthy boundaries, and self-respect"
    );

    if (!brain->self_model) {
        set_error("Failed to create self-model system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_self_model_subsystem: brain->self_model is NULL");
        return false;
    }

    // Phase 12: Wire personality into self-model
    if (brain->personality) {
        if (!self_model_set_personality(brain->self_model, brain->personality)) {
            set_error("Failed to wire personality into self-model");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_self_model_subsystem: self_model_set_personality is NULL");
            return false;
        }
    }

    return true;
}

/**
 * @brief Initialize Global Workspace Architecture subsystem
 */
bool init_global_workspace_subsystem(brain_t brain)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "init_global_workspace_subsystem: brain is NULL");

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
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_global_workspace_subsystem: brain->global_workspace is NULL");
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

    return true;
}
