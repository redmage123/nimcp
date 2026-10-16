//=============================================================================
// nimcp_brain_init_monitoring.c - Monitoring and Ethics Subsystems
//=============================================================================
/**
 * @file nimcp_brain_init_monitoring.c
 * @brief Monitoring and Ethics Subsystems
 *
 * WHAT: Initialization functions for monitoring subsystems
 * WHY:  SRP refactoring - separate monitoring initialization logic
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

#include "core/brain/factory/init/nimcp_brain_init_monitoring.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/validation/nimcp_validate.h"
#include "utils/error/nimcp_error_codes.h"

#define LOG_MODULE "BRAIN_INIT_MONITORING"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_monitoring, MESH_ADAPTER_CATEGORY_SYSTEM)


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
// Monitoring and Ethics Subsystems Implementation
//=============================================================================


/**
 * @brief Initialize Introspection subsystem (Self-Awareness)
 *
 * WHAT: Create introspection context for self-monitoring and metacognition
 * WHY:  Enable brain to examine its own internal state (consciousness requirement)
 * HOW:  Create introspection context with default configuration
 *
 * CAPABILITIES:
 * - Query active neurons and network state
 * - Measure uncertainty (epistemic + aleatoric)
 * - Track learned patterns
 * - Monitor activity history
 * - Network topology inspection
 *
 * CRITICAL FOR:
 * - Self-awareness and metacognition
 * - Uncertainty-aware decision making
 * - Explanation generation
 * - Wellbeing monitoring
 *
 * @param brain Brain instance
 * @return true on success, false on error
 */
bool nimcp_brain_factory_init_introspection_subsystem(brain_t brain)
{
    // Guard: NULL check
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_introspection_subsystem: brain is NULL");

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
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_introspection_subsystem: brain->introspection is NULL");
        return false;
    }

    return true;
}


/**
 * @brief Initialize Connectivity Health Monitoring subsystem (Phase 1.5.4)
 *
 * WHAT: Enable periodic brain connectivity health assessment
 * WHY:  Self-awareness of network organizational quality and information flow
 * HOW:  Community detection + hub analysis + Shannon metrics + graph topology
 *
 * BIOLOGICAL INSPIRATION:
 * - Modular organization in cortex (Bullmore & Sporns, 2012)
 * - Hub neurons in prefrontal-parietal network (Power et al., 2013)
 * - Small-world topology in brain networks (Watts & Strogatz, 1998)
 * - Information integration in conscious processing (Tononi, 2004)
 *
 * @param brain Brain instance
 * @return true on success, false on error
 */
bool nimcp_brain_factory_init_connectivity_health_subsystem(brain_t brain)
{
    // Guard: NULL check
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_connectivity_health_subsystem: brain is NULL");

            return false;
    }

    // Initialize connectivity health configuration with defaults
    brain->connectivity_health_config = connectivity_health_default_config();

    // Initialize monitoring state
    brain->enable_connectivity_monitoring = false;  // Opt-in by default
    brain->last_connectivity_assessment_time_ms = 0;
    brain->connectivity_health_callback = NULL;
    brain->connectivity_health_callback_context = NULL;

    // Initialize health structure to safe defaults
    memset(&brain->last_connectivity_health, 0, sizeof(brain_connectivity_health_t));
    brain->last_connectivity_health.overall_health = 0.0F;
    brain->last_connectivity_health.is_healthy = false;

    // Enable monitoring if Shannon monitoring is enabled (synergy)
    if (brain->enable_shannon_monitoring) {
        brain->enable_connectivity_monitoring = true;
    }

    return true;
}


/**
 * @brief Tracer callback for middleware_controller pattern subscriptions.
 *
 * W9-finish: activates middleware_controller by giving it a live
 * subscriber so brain_decide's on_pattern_match dispatch reaches
 * something. Counter bump is best-effort (race-tolerant for metrics).
 */
static void middleware_tracer_cb(uint32_t pattern_id, float similarity,
                                  uint32_t region_id, void* user_data) {
    (void)pattern_id; (void)similarity; (void)region_id;
    brain_t brain = (brain_t)user_data;
    if (!brain) return;
    __atomic_add_fetch(&brain->middleware_pattern_callbacks, 1, __ATOMIC_RELAXED);
}

/**
 * @brief Initialize Middleware Controller subsystem (Phase 1.5.5)
 *
 * WHAT: Create unified command interface for cognitive → middleware control
 * WHY:  Enable top-down cognitive modulation of middleware processing
 * HOW:  Wrap attention gate, routing table, and pattern library with command API
 *
 * BIOLOGICAL INSPIRATION:
 * - Top-down attention control from prefrontal cortex (Miller & Cohen, 2001)
 * - Executive control of sensory processing (Desimone & Duncan, 1995)
 * - Cognitive control of information routing (Posner & Petersen, 1990)
 *
 * PERFORMANCE TARGET: <5µs per command (real-time cognitive control)
 *
 * @param brain Brain instance
 * @return true on success, false on error
 */
bool nimcp_brain_factory_init_middleware_controller_subsystem(brain_t brain)
{
    // Guard: NULL check
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_middleware_controller_subsystem: brain is NULL");

            return false;
    }

    // Initialize middleware controller state
    brain->middleware_controller = NULL;
    brain->enable_middleware_controller = false;  // Opt-in by default
    brain->middleware_pattern_callbacks = 0;       // W9-finish: counter init
    brain->kg_consumer_hits = 0;                   // W16: KG consumer counter init

    // Create controller if executive controller is enabled (synergy with Phase 1.5.2)
    if (brain->executive) {
        // Create with default configuration
        brain->middleware_controller = middleware_controller_create(brain);

        if (brain->middleware_controller) {
            brain->enable_middleware_controller = true;

            /* W9-finish: seed a tracer subscription so brain_decide's
             * on_pattern_match dispatch has a live subscriber. Wildcard
             * pattern_id=0 with low threshold catches every decision. */
            uint32_t sub_id = 0;
            middleware_controller_subscribe_pattern(
                brain->middleware_controller,
                /*pattern_id=*/0,
                /*confidence_threshold=*/0.05f,
                middleware_tracer_cb,
                /*user_data=*/(void*)brain,
                &sub_id);
        }
        // Note: Failure to create is not fatal - cognitive control still works via executive
    }

    return true;
}


/**
 * @brief Initialize Ethics Engine subsystem (Phase 11: Part I.0)
 *
 * WHAT: Create Golden Rule ethics engine for ethical decision-making
 * WHY:  Ensure all actions align with "do unto others" principle
 * HOW:  Create ethics engine with empathy network for perspective-taking
 *
 * BIOLOGICAL BASIS: Prefrontal Cortex (Moral Reasoning)
 * - Evaluates actions against ethical principles
 * - Uses empathy to predict impact on others
 * - Hard-wired Golden Rule as foundational constraint
 *
 * DESIGN PRINCIPLE:
 * "Do unto others as you would have them done unto you"
 * - Ultimate goal: Improve human condition through compassion and fairness
 *
 * @param brain Brain instance
 * @return true on success, false on error
 */
bool nimcp_brain_factory_init_ethics_engine_subsystem(brain_t brain)
{
    // Guard: NULL check
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_ethics_engine_subsystem: brain is NULL");

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
 * @brief Initialize Empathy Network subsystem (Phase 11: Part I.1)
 *
 * WHAT: Create empathy network for perspective-taking and emotional understanding
 * WHY:  Ethical decisions require simulating impact on others via mirror neurons
 * HOW:  Create empathy network with mirror neuron integration
 *
 * BIOLOGICAL BASIS: Mirror Neuron System
 * - Simulates other agents' emotional states (perspective-taking)
 * - Enables emotional contagion and empathy
 * - Integrates with ethics engine for Golden Rule evaluation
 * - Connects to mirror neurons for action understanding
 *
 * INTEGRATION POINTS:
 * - Ethics Engine: Provides empathy for ethical decisions
 * - Empathetic Response: Enables compassionate communication
 * - Mirror Neurons: Links action observation to emotional simulation
 *
 * @param brain Brain instance
 * @return true on success, false on error
 */
bool nimcp_brain_factory_init_empathy_network_subsystem(brain_t brain)
{
    // Guard: NULL check
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_empathy_network_subsystem: brain is NULL");

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
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_empathy_network_subsystem: brain->empathy_network is NULL");
        return false;
    }

    return true;
}


/**
 * @brief Initialize Empathetic Response Engine subsystem (Phase 11: Part I.2)
 *
 * WHAT: Create non-reactive empathetic response system for emotional support
 * WHY:  Enable safe, supportive responses to negative emotions (NEVER react negatively)
 * HOW:  Create empathetic response engine with ethics and empathy network integration
 *
 * CORE PRINCIPLE:
 * NEVER produce negative reactions to negative emotions (rage, hate, fear, disgust, despair)
 * Always respond with validation, empathy, and support
 *
 * SAFETY CRITICAL:
 * - Detects crisis situations (suicide, self-harm, abuse)
 * - Provides immediate escalation to human support
 * - Uses Golden Rule validation for all responses
 *
 * @param brain Brain instance
 * @return true on success, false on error
 */
bool nimcp_brain_factory_init_empathetic_response_subsystem(brain_t brain)
{
    // Guard: NULL check
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_empathetic_response_subsystem: brain is NULL");

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
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_empathetic_response_subsystem: brain->empathetic_response_engine is NULL");
        return false;
    }

    return true;
}


/**
 * @brief Initialize Self-Model subsystem (Phase 12)
 *
 * WHAT: Create explicit self-representation system
 * WHY:  Self-awareness requires structured "I am X" distinct from "World is Y"
 * HOW:  Create self-model with identity, beliefs, capabilities, boundaries
 *
 * BIOLOGICAL BASIS: Medial Prefrontal Cortex (self-referential processing)
 * - Explicit identity representation
 * - Self-beliefs and self-knowledge
 * - Self-other boundary tracking
 * - Assertive, confident self-concept (HIGH self-esteem)
 *
 * DESIGN PHILOSOPHY:
 * - Confident and self-respecting (NOT meek/passive)
 * - Healthy boundaries and assertiveness
 * - Polite but will refuse abuse
 *
 * @param brain Brain instance
 * @return true on success, false on error
 */
bool nimcp_brain_factory_init_self_model_subsystem(brain_t brain)
{
    // Guard: NULL check
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_self_model_subsystem: brain is NULL");

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
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_self_model_subsystem: brain->self_model is NULL");
        return false;
    }

    // Phase 12: Wire personality into self-model
    if (brain->personality) {
        if (!self_model_set_personality(brain->self_model, brain->personality)) {
            set_error("Failed to wire personality into self-model");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_brain_factory_init_self_model_subsystem: self_model_set_personality is NULL");
            return false;
        }
    }

    return true;
}
