//=============================================================================
// nimcp_brain_init_plasticity.c - Plasticity and Training Subsystems
//=============================================================================
/**
 * @file nimcp_brain_init_plasticity.c
 * @brief Plasticity and Training Subsystems
 *
 * WHAT: Initialization functions for plasticity subsystems
 * WHY:  SRP refactoring - separate plasticity initialization logic
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

#include "core/brain/factory/init/nimcp_brain_init_plasticity.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/validation/nimcp_validate.h"
#include "utils/error/nimcp_error_codes.h"

#define LOG_MODULE "BRAIN_INIT_PLASTICITY"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_plasticity, MESH_ADAPTER_CATEGORY_SYSTEM)


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
#include "plasticity/nimcp_second_messengers.h"
#include "core/cortical_columns/nimcp_cortical_column.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <stdio.h>

//=============================================================================
// Plasticity and Training Subsystems Implementation
//=============================================================================


//=============================================================================
// Phase T1: Biological Framework Enhancements (Training Pipeline)
//=============================================================================

/**
 * @brief Initialize homeostatic plasticity subsystem
 *
 * WHAT: Create and configure homeostatic plasticity controller
 * WHY:  Maintains neural activity within healthy ranges via synaptic scaling
 * HOW:  Create homeostatic_controller_t with config-specified parameters
 *
 * BIOLOGICAL BASIS:
 * - Synaptic scaling: Adjusts all synaptic weights to maintain target firing rate
 * - Intrinsic plasticity: Modifies neuron excitability for homeostasis
 * - Metaplasticity: Adjusts plasticity rules based on activity history
 *
 * @param brain Brain instance
 * @return true on success, false on error
 */
bool nimcp_brain_factory_init_homeostatic_plasticity_subsystem(brain_t brain)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_homeostatic_plasticity_subsystem: brain is NULL");

            return false;
    }

    // Check if already initialized
    if (brain->homeostatic) {
        return true;  // Already initialized
    }

    // Only create if enabled in config
    if (!brain->config.enable_homeostatic_plasticity) {
        return true;  // Not enabled, but not an error
    }

    // Create homeostatic config from brain config
    homeostatic_config_t config = homeostatic_config_default();
    if (brain->config.homeostatic_target_rate_hz > 0.0F) {
        config.scaling_params.target_rate = brain->config.homeostatic_target_rate_hz;
    }
    if (brain->config.homeostatic_tau_ms > 0.0F) {
        config.scaling_params.scaling_time_constant = brain->config.homeostatic_tau_ms;
    }

    // Get number of neurons from network - use default if not available
    uint32_t num_neurons = 1000;  // Default
    if (brain->network) {
        neural_network_t base_net = adaptive_network_get_base_network(brain->network);
        if (base_net) {
            num_neurons = neural_network_get_num_neurons(base_net);
        }
    }

    // Create homeostatic controller
    brain->homeostatic = homeostatic_controller_create(&config, num_neurons);
    if (!brain->homeostatic) {
        set_error("Failed to create homeostatic plasticity controller");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_homeostatic_plasticity_subsystem: brain->homeostatic is NULL");
        return false;
    }

    LOG_INFO("Homeostatic plasticity subsystem initialized: target_rate=%.1f Hz, tau=%.1f ms, neurons=%u",
            config.scaling_params.target_rate, config.scaling_params.scaling_time_constant, num_neurons);
    return true;
}


/**
 * @brief Initialize dendritic computation subsystem
 *
 * WHAT: Create and configure dendritic tree with NMDA dynamics
 * WHY:  Enable local computation in dendritic branches
 * HOW:  Create dendritic_tree_t with compartmental modeling
 *
 * BIOLOGICAL BASIS:
 * - NMDA dynamics: Voltage-dependent magnesium block, NMDA-dependent LTP
 * - Dendritic branches: Each branch computes local activations
 * - Compartmental modeling: Spatial integration of inputs
 *
 * @param brain Brain instance
 * @return true on success, false on error
 */
bool nimcp_brain_factory_init_dendritic_computation_subsystem(brain_t brain)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_dendritic_computation_subsystem: brain is NULL");

            return false;
    }

    // Check if already initialized
    if (brain->dendritic) {
        return true;  // Already initialized
    }

    // Only create if enabled in config
    if (!brain->config.enable_dendritic_computation) {
        return true;  // Not enabled, but not an error
    }

    // Create dendritic tree config from brain config
    dendritic_tree_config_t config = dendritic_tree_config_default();
    if (brain->config.dendritic_branches > 0) {
        config.num_branches = brain->config.dendritic_branches;
    }
    if (brain->config.dendritic_compartments > 0) {
        config.compartments_per_branch = brain->config.dendritic_compartments;
    }

    // Create dendritic tree
    brain->dendritic = dendritic_tree_create(&config);
    if (!brain->dendritic) {
        set_error("Failed to create dendritic computation tree");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_dendritic_computation_subsystem: brain->dendritic is NULL");
        return false;
    }

    LOG_INFO("Dendritic computation subsystem initialized: branches=%u, compartments=%u",
            config.num_branches, config.compartments_per_branch);
    return true;
}


/**
 * @brief Initialize biological predictive coding subsystem
 *
 * WHAT: Create and configure hierarchical predictive coding network
 * WHY:  Enable error-driven learning via Free Energy Principle
 * HOW:  Create pc_hierarchy_t with prediction and error units
 *
 * BIOLOGICAL BASIS:
 * - Prediction units: Generate top-down expectations
 * - Error units: Compute mismatch between prediction and input
 * - Precision weighting: Modulates error importance by confidence
 *
 * NOTE: This is different from the Phase 10.9 predictive_network (cognitive)
 *       This is for biological-level predictive coding in sensory processing
 *
 * @param brain Brain instance
 * @return true on success, false on error
 */
bool nimcp_brain_factory_init_biological_predictive_subsystem(brain_t brain)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_biological_predictive_subsystem: brain is NULL");

            return false;
    }

    // Check if already initialized
    if (brain->predictive_coding) {
        return true;  // Already initialized
    }

    // Only create if enabled in config
    if (!brain->config.enable_biological_predictive) {
        return true;  // Not enabled, but not an error
    }

    // Determine number of hierarchy levels and units
    uint32_t num_levels = brain->config.predictive_levels;
    if (num_levels == 0) {
        num_levels = 3;  // Default: 3 levels
    }

    // Allocate units array and set up pyramid structure
    uint32_t* units = (uint32_t*)nimcp_malloc(num_levels * sizeof(uint32_t));
    if (!units) {
        set_error("Failed to allocate predictive coding units array");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_brain_factory_init_biological_predictive_subsystem: units is NULL");
        return false;
    }

    // Set up hierarchical pyramid: bottom level matches brain input dim
    // (so features can be directly copied in without size mismatch)
    uint32_t base_units = brain->config.num_inputs;
    if (base_units > 4096) base_units = 4096;  // Cap for memory
    if (base_units < 8) base_units = 8;
    for (uint32_t i = 0; i < num_levels; i++) {
        units[i] = base_units >> i;  // 64, 32, 16, 8, ...
        if (units[i] < 4) units[i] = 4;  // Minimum 4 units per level
    }

    // Create predictive coding config
    pc_hierarchy_config_t config = pc_hierarchy_config_default(num_levels, units);
    config.units_per_level = units;  // Must set explicitly

    if (brain->config.predictive_learning_rate > 0.0F) {
        config.learning_rate = brain->config.predictive_learning_rate;
    }

    // Create predictive coding hierarchy
    brain->predictive_coding = pc_hierarchy_create(&config);
    nimcp_free(units);  // pc_hierarchy_create copies the array

    if (!brain->predictive_coding) {
        set_error("Failed to create biological predictive coding hierarchy");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_biological_predictive_subsystem: brain->predictive_coding is NULL");
        return false;
    }

    LOG_INFO("Biological predictive coding subsystem initialized: levels=%u, learning_rate=%.3f",
            num_levels, config.learning_rate);
    return true;
}


/**
 * @brief Initialize second messenger cascade subsystem
 *
 * WHAT: Create and configure second messenger cascade system
 * WHY:  Enable GPCR-mediated signaling (cAMP, IP3/DAG, Ca2+, gene expression)
 * HOW:  Create second_messenger_system_t with bio-async integration
 *
 * BIOLOGICAL BASIS:
 * - cAMP pathway: Gs-coupled receptors (D1, beta-adrenergic) → adenylyl cyclase → PKA
 * - IP3/DAG pathway: Gq-coupled receptors (5-HT2A, mGluR1) → PLC → Ca2+ release + PKC
 * - Calcium signaling: IP3 → ER Ca2+ release → calmodulin → CaMKII
 * - Gene expression: CREB phosphorylation → IEGs (c-Fos, Arc, BDNF)
 *
 * INTEGRATION:
 * - Receives neuromodulator release events via bio-async router
 * - Broadcasts cascade state updates to plasticity modules
 * - Coordinates with neuromodulator system for GPCR activation
 *
 * @param brain Brain instance
 * @return true on success, false on error
 */
bool nimcp_brain_factory_init_second_messenger_subsystem(brain_t brain)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_second_messenger_subsystem: brain is NULL");

            return false;
    }

    /* Check if already initialized */
    if (brain->second_messengers) {
        return true;
    }

    /* Only create if enabled in config */
    if (!brain->config.enable_second_messengers) {
        brain->enable_second_messengers = false;
        return true;  /* Not enabled, but not an error */
    }

    /* Get number of neurons from network - use default if not available */
    uint32_t num_neurons = 1000;  /* Default */
    if (brain->network) {
        neural_network_t base_net = adaptive_network_get_base_network(brain->network);
        if (base_net) {
            num_neurons = neural_network_get_num_neurons(base_net);
        }
    }

    /* Create second messenger config */
    second_messenger_config_t config = second_messenger_default_config();
    config.enable_bio_async = true;  /* Enable bio-async integration */
    config.enable_security = (brain->security_integration != NULL);

    /* Create second messenger system */
    brain->second_messengers = second_messenger_create(num_neurons, &config);
    if (!brain->second_messengers) {
        set_error("Failed to create second messenger cascade system");
        brain->enable_second_messengers = false;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_second_messenger_subsystem: brain->second_messengers is NULL");
        return false;
    }

    /* Register with bio-async router for message routing */
    nimcp_result_t result = second_messenger_register_bioasync(brain->second_messengers, NULL);
    if (result != NIMCP_SUCCESS) {
        LOG_WARNING("Second messengers: bio-async registration failed (result=%d), continuing without bio-async",
                    result);
        /* Non-fatal - can work without bio-async */
    }

    brain->enable_second_messengers = true;

    LOG_INFO("Second messenger cascade subsystem initialized: neurons=%u, bio_async=%s",
             num_neurons, config.enable_bio_async ? "enabled" : "disabled");
    return true;
}


//=============================================================================
// Phase TM-3: Brain-Training Integration
//=============================================================================

/**
 * @brief Initialize brain-training integration subsystem
 *
 * WHAT: Create and configure training integration context
 * WHY:  Provides unified interface for loss functions, optimizers, and training events
 * HOW:  Create nimcp_brain_training_ctx with config-specified parameters
 *
 * FEATURES:
 * - Loss function management (MSE, CrossEntropy, Huber, etc.)
 * - Optimizer management (SGD, Adam, RMSProp, etc.)
 * - Automatic security registration with BBB and Security Integration
 * - Event bus integration for training events
 * - Memory pool support for gradient buffers
 *
 * @param brain Brain instance
 * @return true on success, false on error
 */
bool nimcp_brain_factory_init_training_subsystem(brain_t brain)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_training_subsystem: brain is NULL");

            return false;
    }

    // Check if already initialized
    if (brain->training_ctx) {
        return true;  // Already initialized
    }

    // Only create if enabled in config
    if (!brain->config.enable_training_integration) {
        brain->enable_training_integration = false;
        return true;  // Not enabled, but not an error
    }

    // Build training integration configuration
    nimcp_brain_training_config_t training_config = nimcp_brain_training_default_config();

    // Override with brain config values
    if (brain->config.training_default_lr > 0.0F) {
        training_config.default_learning_rate = brain->config.training_default_lr;
    } else {
        training_config.default_learning_rate = brain->config.learning_rate;
    }

    training_config.enable_security = brain->config.training_register_security;
    training_config.register_with_bbb = (brain->bbb_system != NULL);
    training_config.use_memory_pool = true;

    // Phase TM-4/5/6: Advanced Training Pipeline Configuration
    training_config.enable_lr_scheduler = brain->config.enable_lr_scheduler;
    training_config.enable_regularization = brain->config.enable_regularization;
    training_config.enable_gradient_management = brain->config.enable_gradient_management;
    training_config.enable_gradient_health_check = brain->config.enable_gradient_health_check;

    // Regularization parameters
    training_config.l1_lambda = brain->config.regularization_l1_lambda;
    training_config.l2_lambda = brain->config.regularization_l2_lambda;
    training_config.dropout_rate = brain->config.dropout_rate;

    // Gradient management parameters
    training_config.gradient_accumulation_steps = brain->config.gradient_accumulation_steps;
    training_config.gradient_clip_value = brain->config.gradient_clip_value;
    training_config.gradient_clip_norm = brain->config.gradient_clip_norm;

    // Create training integration context
    brain->training_ctx = nimcp_brain_training_create(&training_config);
    if (!brain->training_ctx) {
        set_error("Failed to create brain-training integration context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_training_subsystem: brain->training_ctx is NULL");
        return false;
    }

    // Initialize with security integration if available
    nimcp_result_t result = nimcp_brain_training_init(
        brain->training_ctx,
        brain->security_integration,
        NULL  // Memory manager - use internal pools
    );

    if (result != NIMCP_SUCCESS) {
        LOG_WARNING("Failed to initialize training security integration (result=%d), continuing without security", result);
        // Non-fatal - training can work without security registration
    }

    // Phase TM-4: Create default LR scheduler if enabled
    if (brain->config.enable_lr_scheduler) {
        nimcp_lr_scheduler_config_t sched_config = {
            .type = NIMCP_LR_STEP,
            .params.step = {
                .initial_lr = training_config.default_learning_rate,
                .step_size = 100,     // Decay every 100 epochs
                .gamma = 0.9F,        // 10% decay
                .min_lr = 1e-6F
            },
            .verbose = false,
            .lr_epsilon = 1e-8F
        };
        uint32_t sched_id = 0;
        nimcp_result_t sres = nimcp_brain_training_create_scheduler(brain->training_ctx, &sched_config, &sched_id);
        if (sres != NIMCP_SUCCESS || sched_id == 0) {
            LOG_WARNING("Failed to create default LR scheduler (result=%d)", sres);
        } else {
            LOG_INFO("Created default StepLR scheduler (id=%u, step_size=100, gamma=0.9)", sched_id);
        }
    }

    // Phase TM-6: Create default gradient manager if enabled
    if (brain->config.enable_gradient_management && brain->config.gradient_accumulation_steps > 1) {
        nimcp_gradient_manager_config_t gm_config = nimcp_gradient_manager_default_config();
        gm_config.use_accumulation = true;
        gm_config.accumulation.accumulation_steps = brain->config.gradient_accumulation_steps;
        gm_config.accumulation.mode = NIMCP_GRAD_ACCUM_SUM;
        gm_config.accumulation.sync_on_step = false;
        gm_config.check_nan_inf = brain->config.enable_gradient_health_check;

        uint32_t gm_id = 0;
        nimcp_result_t gres = nimcp_brain_training_create_gradient_manager(brain->training_ctx, &gm_config, &gm_id);
        if (gres != NIMCP_SUCCESS || gm_id == 0) {
            LOG_WARNING("Failed to create default gradient manager (result=%d)", gres);
        } else {
            LOG_INFO("Created default gradient manager (id=%u, accum_steps=%u)",
                    gm_id, brain->config.gradient_accumulation_steps);
        }
    }

    brain->enable_training_integration = true;

    // Phase TPB-1: Initialize Training-Plasticity Bridge
    // This connects the training pipeline to biological plasticity systems
    if (brain->config.enable_training_integration && brain->neuromodulator_system) {
        tpb_config_t tpb_config = tpb_config_default();

        // Link to the brain's neuromodulator system
        tpb_config.neuromod_system = brain->neuromodulator_system;

        // Configure RPE parameters from training config
        tpb_config.rpe_to_da_gain = 0.5F;  // Moderate DA sensitivity to loss changes

        // Enable CoW for weight snapshots
        tpb_config.enable_cow = true;

        // Create the plasticity bridge
        brain->plasticity_bridge = tpb_create(&tpb_config);
        if (brain->plasticity_bridge) {
            brain->enable_plasticity_bridge = true;

            // Configure default cortical region covering all neurons
            tpb_region_config_t cortical = tpb_region_cortical_default();
            cortical.neuron_start_idx = 0;
            cortical.neuron_end_idx = brain_get_neuron_count(brain);
            tpb_configure_region(brain->plasticity_bridge, &cortical, NULL);

            // Connect the plasticity bridge to the training context
            if (brain->training_ctx) {
                nimcp_result_t conn_result = nimcp_brain_training_connect_plasticity_bridge(
                    brain->training_ctx, brain->plasticity_bridge);
                if (conn_result == NIMCP_SUCCESS) {
                    // Enable biological modulation by default (50% blend)
                    nimcp_brain_training_set_biological_modulation(brain->training_ctx, 0.5F);
                    LOG_INFO("Training-Plasticity Bridge connected to training context");
                } else {
                    LOG_WARNING("Failed to connect plasticity bridge to training context");
                }
            }

            LOG_INFO("Training-Plasticity Bridge initialized: connected to neuromodulator system");
        } else {
            brain->enable_plasticity_bridge = false;
            LOG_WARNING("Failed to create Training-Plasticity Bridge, continuing without");
        }
    } else {
        brain->plasticity_bridge = NULL;
        brain->enable_plasticity_bridge = false;
    }

    // Phase EDP-1: Initialize Event-Driven Plasticity (continuous learning from sensory events)
    // This connects the event bus to the plasticity bridge for real-time learning
    if (brain->enable_plasticity_bridge && brain->event_bus) {
        edp_config_t edp_config = edp_config_default();

        // Configure for immediate processing mode (real-time learning)
        edp_config.mode = EDP_MODE_IMMEDIATE;

        // Configure spike timing parameters (STDP window)
        edp_config.stdp_window_ms = 40.0F;  // ±40ms STDP window (biological)
        edp_config.ltp_rate = brain->config.learning_rate * 0.5F;  // Scale from brain LR
        edp_config.ltd_rate = brain->config.learning_rate * 0.6F;  // Slightly stronger LTD
        edp_config.spike_threshold = 0.1F;

        // Configure eligibility traces for three-factor learning
        edp_config.enable_eligibility = true;
        edp_config.eligibility_tau_ms = 1000.0F;  // 1 second decay
        edp_config.eligibility_threshold = 0.01F;

        // Configure learning signal gains
        edp_config.error_gain = 1.0F;
        edp_config.reward_gain = 0.5F;   // Reward modulation strength
        edp_config.novelty_gain = 0.3F;  // Novelty modulation strength

        // Security integration
        edp_config.enable_security = (brain->security_integration != NULL);
        edp_config.security_ctx = brain->security_integration;

        // Create the EDP context
        brain->event_driven_plasticity = edp_create(&edp_config);
        if (brain->event_driven_plasticity) {
            brain->enable_event_driven_plasticity = true;

            // Connect to the plasticity bridge
            nimcp_result_t bridge_result = edp_connect_bridge(
                brain->event_driven_plasticity, brain->plasticity_bridge);
            if (bridge_result != NIMCP_SUCCESS) {
                LOG_WARNING("EDP: Failed to connect to plasticity bridge");
            }

            // Connect to the event bus
            nimcp_result_t bus_result = edp_connect_event_bus(
                brain->event_driven_plasticity, brain->event_bus);
            if (bus_result != NIMCP_SUCCESS) {
                LOG_WARNING("EDP: Failed to connect to event bus");
            }

            // Register with security module if available
            if (brain->security_integration) {
                nimcp_result_t sec_result = edp_register_security(
                    brain->event_driven_plasticity, brain->security_integration);
                if (sec_result != NIMCP_SUCCESS) {
                    LOG_WARNING("EDP: Failed to register with security module");
                }
            }

            /* Start EDP so it actively processes events */
            nimcp_result_t start_result = edp_start(brain->event_driven_plasticity);
            if (start_result != NIMCP_OK) {
                LOG_WARN(LOG_MODULE, "EDP start returned %d (non-fatal)", start_result);
            }

            LOG_INFO("Event-Driven Plasticity initialized: stdp_window=%.1fms, eligibility_tau=%.0fms",
                     edp_config.stdp_window_ms, edp_config.eligibility_tau_ms);
        } else {
            brain->enable_event_driven_plasticity = false;
            LOG_WARNING("Failed to create Event-Driven Plasticity adapter, continuing without");
        }
    } else {
        brain->event_driven_plasticity = NULL;
        brain->enable_event_driven_plasticity = false;
    }

    LOG_INFO("Brain-training integration initialized: lr=%.4f, security=%s, "
             "lr_sched=%s, regularization=%s, grad_mgmt=%s, plasticity_bridge=%s",
            training_config.default_learning_rate,
            training_config.enable_security ? "enabled" : "disabled",
            brain->config.enable_lr_scheduler ? "enabled" : "disabled",
            brain->config.enable_regularization ? "enabled" : "disabled",
            brain->config.enable_gradient_management ? "enabled" : "disabled",
            brain->enable_plasticity_bridge ? "enabled" : "disabled");

    return true;
}
