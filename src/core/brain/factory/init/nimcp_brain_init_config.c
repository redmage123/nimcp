//=============================================================================
// nimcp_brain_init_config.c - Brain Configuration Functions
//=============================================================================
/**
 * @file nimcp_brain_init_config.c
 * @brief Brain configuration and statistics initialization
 *
 * WHAT: Configuration building and parameter setup for brain initialization
 * WHY:  Separates configuration logic from brain creation
 * HOW:  Provides builder functions for network config and brain stats
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

#include "core/brain/factory/init/nimcp_brain_init_config.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/validation/nimcp_validate.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>

#define LOG_MODULE "BRAIN_INIT_CONFIG"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_config, MESH_ADAPTER_CATEGORY_SYSTEM)


// P3-56 FIX: Variadic set_error macro for formatted error messages
#ifndef set_error
#define set_error(fmt, ...) LOG_ERROR(LOG_MODULE, fmt, ##__VA_ARGS__)
#endif

//=============================================================================
// Neuron Count and Sparsity Functions
//=============================================================================

uint32_t nimcp_brain_factory_get_neuron_count(brain_size_t size)
{
    switch (size) {
        case BRAIN_SIZE_MICRO:
            return 25;    // Ultra-lightweight for unit tests and swarm drones
        case BRAIN_SIZE_TINY:
            return 100;
        case BRAIN_SIZE_SMALL:
            return 500;
        case BRAIN_SIZE_MEDIUM:
            return 1000;  // Reduced from 10000 for faster tests (1.8GB→180MB)
        case BRAIN_SIZE_LARGE:
            return 5000;  // Reduced from 100000 (9GB→450MB)
        case BRAIN_SIZE_CUSTOM:
            return 1000;
        default:
            return 1000;
    }
}

float nimcp_brain_factory_get_default_sparsity(brain_size_t size)
{
    switch (size) {
        case BRAIN_SIZE_MICRO:
            return 0.60F;   // Lower sparsity for micro brains (more dense connections)
        case BRAIN_SIZE_TINY:
            return 0.70F;
        case BRAIN_SIZE_SMALL:
            return 0.80F;
        case BRAIN_SIZE_MEDIUM:
            return 0.85F;
        case BRAIN_SIZE_LARGE:
            return 0.90F;
        default:
            return 0.80F;
    }
}

//=============================================================================
// Configuration Builders
//=============================================================================

adaptive_spike_params_t nimcp_brain_factory_build_spike_params(float sparsity_target)
{
    adaptive_spike_params_t params = {0};
    params.k_factor = 0.5F;
    params.sparsity_target = sparsity_target;
    params.encoding = SPIKE_ENCODING_PASSTHROUGH;  /* Raw floats — best for gradient training */
    params.enable_soft_reset = true;
    params.enable_adaptation = true;
    params.adaptation_window = 100;
    params.min_threshold = 0.0001F;  // Very low to allow tiny outputs from untrained networks
    params.max_threshold = 10.0F;
    return params;
}

network_config_t nimcp_brain_factory_build_base_network_config(uint32_t num_inputs, uint32_t num_outputs,
                                                  uint32_t num_neurons,
                                                  ode_integration_method_t integration_method)
{
    network_config_t config = {0};

    // Validation: Check for zero parameters
    if (num_inputs == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_brain_factory_build_base_network_config: num_inputs cannot be zero");
        return config;  // Return with layer_sizes = NULL to signal error
    }
    if (num_outputs == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_brain_factory_build_base_network_config: num_outputs cannot be zero");
        return config;  // Return with layer_sizes = NULL to signal error
    }
    if (num_neurons == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_brain_factory_build_base_network_config: num_neurons cannot be zero");
        return config;  // Return with layer_sizes = NULL to signal error
    }

    config.input_size = num_inputs;
    config.output_size = num_outputs;
    config.num_neurons = num_neurons;
    config.integration_method = integration_method;

    // ========================================================================
    // MULTI-LAYER ARCHITECTURE: Diamond/pyramid topology
    //
    // WHY: A single hidden layer wastes neurons (1.5M in one flat layer means
    //      no hierarchical feature extraction). Deep networks learn low-level
    //      features early and compose them into abstractions in later layers.
    //
    // HOW: Distribute neurons across a diamond-shaped layer pyramid:
    //      - Expand from input to a wide middle layer
    //      - Contract from middle to output
    //      - Layer count scales with log2(num_neurons)
    //
    // Small networks (<5000): 3 layers (input → hidden → output)
    //   [64, 500, 32] — simple, proven architecture
    //
    // Medium networks (5K-100K): 5 layers (diamond)
    //   [256, 2048, 8192, 2048, 128] — hierarchical features
    //
    // Large networks (100K+): 7 layers (deep diamond)
    //   [1024, 16384, 131072, 524288, 131072, 16384, 2048]
    // ========================================================================

    // Hidden budget = total neurons minus input/output layers
    // The layer_sizes array includes input/output layers, so hidden_budget
    // must exclude them to avoid total exceeding num_neurons.
    uint32_t hidden_budget = num_neurons;
    if (num_neurons > num_inputs + num_outputs) {
        hidden_budget = num_neurons - num_inputs - num_outputs;
    }

    if (hidden_budget <= 5000) {
        // Small: 3 layers — single hidden layer (backward compatible)
        config.num_layers = 3;
        config.layer_sizes = nimcp_calloc(3, sizeof(uint32_t));
        if (!config.layer_sizes) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
                "nimcp_brain_factory_build_base_network_config: failed to allocate layer_sizes");
            return config;
        }
        config.layer_sizes[0] = num_inputs;
        config.layer_sizes[1] = hidden_budget;
        config.layer_sizes[2] = num_outputs;

    } else if (hidden_budget <= 100000) {
        // Medium: 5 layers — diamond shape
        // Ratios: 1/8, 3/8, 3/8, 1/8 of hidden budget
        config.num_layers = 5;
        config.layer_sizes = nimcp_calloc(5, sizeof(uint32_t));
        if (!config.layer_sizes) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
                "nimcp_brain_factory_build_base_network_config: failed to allocate layer_sizes");
            return config;
        }
        uint32_t h1 = hidden_budget / 8;       // Narrow entry
        uint32_t h3 = hidden_budget / 8;       // Narrow exit
        uint32_t h2 = hidden_budget - h1 - h3; // Wide middle (3/4 of budget)
        // Split middle into 2 layers
        uint32_t h2a = h2 / 2;
        uint32_t h2b = h2 - h2a;

        config.layer_sizes[0] = num_inputs;
        config.layer_sizes[1] = h1 > 0 ? h1 : 1;
        config.layer_sizes[2] = h2a > 0 ? h2a : 1;
        config.layer_sizes[3] = h2b > 0 ? h2b : 1;
        // Re-distribute: make it a proper diamond (expand then contract)
        // [input, small, big, small, output]
        config.layer_sizes[1] = hidden_budget / 6;        // ~17%
        config.layer_sizes[2] = hidden_budget * 2 / 3;    // ~67% (widest)
        config.layer_sizes[3] = hidden_budget - config.layer_sizes[1] - config.layer_sizes[2]; // ~17%
        config.layer_sizes[4] = num_outputs;

    } else if (hidden_budget <= 2000000) {
        // Large (100K-2M): 7 layers — deep diamond
        // Distribution: expand to peak then contract
        // Ratios: 2%, 12%, 36%, 36%, 12%, 2%
        config.num_layers = 7;
        config.layer_sizes = nimcp_calloc(7, sizeof(uint32_t));
        if (!config.layer_sizes) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
                "nimcp_brain_factory_build_base_network_config: failed to allocate layer_sizes");
            return config;
        }

        uint32_t l1 = hidden_budget * 2 / 100;
        if (l1 < 256) l1 = 256;
        uint32_t l5 = l1;
        uint32_t l2 = hidden_budget * 12 / 100;
        uint32_t l4 = l2;
        uint32_t l3 = hidden_budget - l1 - l2 - l4 - l5;
        uint32_t l3a = l3 / 2;
        uint32_t l3b = l3 - l3a;

        config.layer_sizes[0] = num_inputs;
        config.layer_sizes[1] = l1;
        config.layer_sizes[2] = l2;
        config.layer_sizes[3] = l3a;
        config.layer_sizes[4] = l3b;
        config.layer_sizes[5] = l4 + l5;
        config.layer_sizes[6] = num_outputs;

    } else {
        // Massive (2M+): 9 layers — deep diamond with finer hierarchy
        // Maps to biological cortical layers (6 layers per column × ~1.5)
        //
        // L1: feature extraction     (1%)  — edge/texture detection
        // L2: low-level patterns     (5%)  — simple shapes, phonemes
        // L3: mid-level features     (15%) — objects, words
        // L4: peak abstraction       (28%) — concepts, relationships
        // L5: peak integration       (28%) — binding, reasoning
        // L6: high-level output      (15%) — plans, responses
        // L7: output preparation     (5%)  — motor planning, articulation
        // L8: exit projection        (3%)  — final output mapping
        config.num_layers = 9;
        config.layer_sizes = nimcp_calloc(9, sizeof(uint32_t));
        if (!config.layer_sizes) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
                "nimcp_brain_factory_build_base_network_config: failed to allocate layer_sizes");
            return config;
        }

        uint32_t l1 = hidden_budget * 1 / 100;
        if (l1 < 1024) l1 = 1024;
        uint32_t l2 = hidden_budget * 5 / 100;
        uint32_t l3 = hidden_budget * 15 / 100;
        uint32_t l4 = hidden_budget * 28 / 100;
        uint32_t l5 = hidden_budget * 28 / 100;
        uint32_t l6 = hidden_budget * 15 / 100;
        uint32_t l7 = hidden_budget * 5 / 100;
        /* l8 gets the remainder to ensure total == hidden_budget */
        uint32_t l8 = hidden_budget - l1 - l2 - l3 - l4 - l5 - l6 - l7;

        config.layer_sizes[0] = num_inputs;
        config.layer_sizes[1] = l1;     /* 1%  — feature extraction */
        config.layer_sizes[2] = l2;     /* 5%  — low-level patterns */
        config.layer_sizes[3] = l3;     /* 15% — mid-level features */
        config.layer_sizes[4] = l4;     /* 28% — peak abstraction */
        config.layer_sizes[5] = l5;     /* 28% — peak integration */
        config.layer_sizes[6] = l6;     /* 15% — high-level output */
        config.layer_sizes[7] = l7 + l8; /* ~8% — output preparation */
        config.layer_sizes[8] = num_outputs;
    }

    // Log the architecture
    {
        char buf[256];
        int pos = 0;
        for (uint32_t l = 0; l < config.num_layers && pos < 240; l++) {
            pos += snprintf(buf + pos, sizeof(buf) - pos, "%s%u",
                            l > 0 ? " → " : "", config.layer_sizes[l]);
        }
        LOG_INFO(LOG_MODULE, "Network architecture (%u layers, %u total neurons): %s",
                 config.num_layers, num_neurons, buf);
    }

    config.enable_stdp = true;
    config.enable_hebbian = true;
    config.enable_oja = true;
    config.enable_homeostasis = true;

    // Enable BCM for homeostatic plasticity (prevents weight saturation via sliding threshold)
    // Disabled for large networks (>100K neurons) — each does nimcp_calloc per synapse,
    // creating 20M+ mutex-locked allocs for 10M backbone connections.
    config.enable_bcm = (num_neurons <= 100000);
    config.enable_eligibility = (num_neurons <= 100000);

    // Weight bounds: [-10, +10] allows sufficient representational capacity
    // with 128-512 fan-in. Original [-1, +1] was too restrictive — limited
    // output range per neuron and interacted poorly with delta clamping.
    config.min_weight = -10.0F;
    config.max_weight = 10.0F;

    return config;
}

adaptive_network_config_t nimcp_brain_factory_build_network_config(uint32_t num_inputs, uint32_t num_outputs,
                                                      uint32_t num_neurons, float sparsity_target,
                                                      ode_integration_method_t integration_method)
{
    adaptive_network_config_t config = {0};

    config.base_config = nimcp_brain_factory_build_base_network_config(num_inputs, num_outputs, num_neurons, integration_method);
    config.spike_params = nimcp_brain_factory_build_spike_params(sparsity_target);

    config.enable_sparsity = false;  // Disabled for regression tests
    config.pruning_threshold = 0.01F;
    config.update_frequency = 100;

    return config;
}

void nimcp_brain_factory_init_brain_config(brain_config_t* config, const char* task_name, brain_size_t size,
                              brain_task_t task, uint32_t num_inputs, uint32_t num_outputs,
                              task_strategy_t* strategy)
{
    // Guard: NULL check
    if (!config || !strategy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_brain_factory_init_brain_config: config or strategy is NULL");
        return;
    }

    // Save minimal_mode flag (may be pre-set by caller)
    bool minimal = config->minimal_mode;

    config->size = size;
    config->task = task;
    config->num_inputs = num_inputs;
    config->num_outputs = num_outputs;
    config->learning_rate = strategy->get_learning_rate();
    config->sparsity_target = nimcp_brain_factory_get_default_sparsity(size);
    config->enable_explanations = !minimal;  // Skip in minimal mode
    strncpy(config->task_name, task_name, sizeof(config->task_name) - 1);
    config->minimal_mode = minimal;  // Restore after strncpy may have zeroed it

    // Part A: Differential Equations - ODE Integration Method (A1.x)
    config->neuron_integration = ODE_EULER;

    // Phase 10.2: Working Memory defaults (Miller's 7±2)
    config->enable_working_memory = !minimal;
    config->working_memory_capacity = 7;
    config->working_memory_decay_tau_ms = 1000.0F;

    // Phase 10.6: Theory of Mind defaults (social cognition, empathy)
    config->enable_theory_of_mind = !minimal;
    config->enable_empathy_responses = !minimal;
    config->enable_false_belief_tracking = !minimal;

    // Phase 10.11: Mirror Neurons defaults (observation-based learning)
    config->enable_mirror_neurons = !minimal;
    config->mirror_neuron_count = minimal ? 0 : 1000;
    config->mirror_max_actions = 100;
    config->mirror_max_agents = 10;
    config->mirror_learning_rate = 0.01F;
    config->mirror_match_threshold = 0.7F;

    // Global Workspace Architecture defaults (Global Workspace Theory - Baars 1988)
    config->enable_global_workspace = !minimal;
    config->workspace_capacity_dim = 0;  // 0 = auto-derive from brain's input dimension
    config->workspace_ignition_threshold = 0.6F;
    config->workspace_refractory_ms = 50;
    config->workspace_enable_history = !minimal;
    config->workspace_history_depth = 10;

    // Phase 11 Enhancement C1.1: Quantum Annealing defaults
    config->enable_quantum_annealing = false;
    config->annealing_temperature_init = 10.0F;
    config->annealing_temperature_final = 0.1F;
    config->annealing_steps = 1000;
    config->quantum_annealing_frequency = 100;

    // Phase 12: Personality and Identity defaults
    config->use_random_personality = true;
    config->personality_seed = 0;
    config->explicit_openness = 0.5F;
    config->explicit_conscientiousness = 0.5F;
    config->explicit_extraversion = 0.5F;
    config->explicit_agreeableness = 0.5F;
    config->explicit_neuroticism = 0.5F;
    config->explicit_gender = GENDER_FEMALE;
    config->explicit_sexuality = SEXUALITY_HETEROSEXUAL;
    config->personality_trait_mean = 0.5F;
    config->personality_trait_stddev = 0.15F;
    config->female_probability = 1.0F;
    config->male_probability = 0.0F;
    config->non_binary_probability = 0.0F;

    // Cognitive pipeline flags (enable stages in brain_decide)
    // Enables 4 additional stages: natural explanations, curiosity,
    // executive control, and epistemic filtering. Other flags
    // (predictive_processing, ethics, emotional_tagging, mental_health,
    // sleep_wake) have module re-registration issues in multi-brain tests.
    config->enable_natural_explanations = !minimal;
    config->enable_curiosity = !minimal;
    config->enable_executive_control = !minimal;
    config->enable_epistemic_filter = !minimal;

    // Network ablation flags (all enabled by default). Dynamically
    // toggleable at runtime via the set_train_ann/cnn/snn/lnn RPCs.
    config->train_ann = true;
    config->train_cnn = true;
    config->train_snn = true;
    config->train_lnn = true;
    config->training_mode_active = false;  // Set to true during training to skip cognitive modules
    config->snn_only_recovery_mode = false;  // Normal joint training by default
    config->ensemble_warmup_scale = 1.0f;    // Full-rate ensemble training by default

    // Intuitive Physics defaults
    config->enable_intuitive_physics = !minimal;
    config->enable_physics_prior = !minimal;
    config->intuitive_physics_dt = 0.01f;
    config->intuitive_physics_max_objects = 128;
    config->physics_prior_weight = 0.3f;

    // Fuzzy logic and Internal KG defaults
    config->enable_fuzzy_logic = !minimal;
    config->enable_internal_kg = true;   /* always on — perspective-network
                                          * sub-brains also get a KG so
                                          * empathy simulation has facts to
                                          * reason over, not just weights. */

    // Phase 5/6: Biological Realism defaults
    config->enable_glial = !minimal;
    config->enable_oscillations = false;
    config->enable_myelin_sheath = !minimal;

    // Lazy Initialization defaults (performance optimization)
    // Default: lazy_init_mode=true to reduce peak memory.
    // Callers can disable by setting config->lazy_init_mode=false AFTER init_brain_config().
    bool fast = (config->init_mode == BRAIN_INIT_FAST);
    bool lazy = true;  // Default to lazy init for all brains
    if (minimal || fast) lazy = true;  // Also true for minimal/fast modes
    config->lazy_init_mode = lazy;  // Propagate lazy mode
    if (fast) {
        config->minimal_mode = false;  // FAST is not minimal — subsystems exist, just deferred
    }
    config->lazy_dendrite_init = lazy;
    config->lazy_axon_init = lazy;
    config->lazy_visual_init = lazy;
    config->lazy_audio_init = lazy;
    config->lazy_speech_init = lazy;
    config->lazy_working_memory_init = lazy;
    config->lazy_theory_of_mind_init = lazy;
    config->lazy_global_workspace_init = lazy;
    config->lazy_ethics_init = lazy;
    config->lazy_mirror_neurons_init = lazy;
    config->lazy_executive_init = lazy;
    config->lazy_consolidation_init = lazy;
    config->lazy_meta_learning_init = lazy;
    config->lazy_neuromod_init = lazy;
    config->lazy_glial_init = lazy;
    config->lazy_cortical_init = lazy;
    config->lazy_topographic_init = lazy;
    config->lazy_pr_memory_init = lazy;
    config->enable_dendrites = !minimal;
    config->enable_axons = !minimal;
    config->enable_pr_memory = !minimal;  // PR memory enabled by default (except minimal mode)

    // === WORLD MODEL CONFIGURATION ===
    // World model enables generative simulation for counterfactual reasoning,
    // policy evaluation, and mental imagery. Based on DreamerV3 and JEPA.
    config->enable_world_model = false;            // Disabled by default (opt-in feature)
    config->lazy_world_model_init = lazy;          // Lazy init follows global lazy setting
    config->enable_omni_world_model = true;        // Omni WM on if world model enabled
    config->enable_multimodal_world_model = true;  // Multimodal WM on if world model enabled

    // Omni World Model defaults (DreamerV3-inspired)
    config->omni_wm_state_dim = 64;                // State dimensionality
    config->omni_wm_action_dim = 32;               // Action dimensionality
    config->omni_wm_obs_dim = 64;                  // Observation dimensionality
    config->omni_wm_latent_dim = 64;               // Latent space dimension
    config->omni_wm_rssm_h_dim = 128;              // RSSM deterministic state
    config->omni_wm_rssm_z_dim = 32;               // RSSM stochastic state
    config->omni_wm_learning_rate = 0.0003F;       // Learning rate
    config->omni_wm_enable_dreaming = true;        // Offline simulation enabled
    config->omni_wm_dream_horizon = 15;            // Dream episode length

    // Multimodal World Model defaults
    config->mm_wm_latent_dim = 256;                // Latent dimension for cross-modal fusion
    config->mm_wm_max_entities = 128;              // Max tracked entities
    config->mm_wm_max_prediction_steps = 50;       // Max prediction horizon
    config->mm_wm_learning_rate = 0.001F;          // Learning rate
    config->mm_wm_enable_bio_async = !minimal;     // Bio-async integration

    // World Model Integration Flags
    config->world_model_connect_active_inference = true;  // Connect to active inference
    config->world_model_connect_imagination = true;       // Connect to imagination engine
    config->world_model_connect_hippocampus = true;       // Connect to hippocampus
    config->world_model_connect_predictive = true;        // Connect to predictive processing

    // === CORTICAL COLUMNS CONFIGURATION ===
    // Disable cortical columns for TINY/SMALL brains (huge memory savings: ~48MB)
    // Only enable for MEDIUM and larger brains that need hierarchical feature processing
    bool large_enough_for_cortical = (config->size >= BRAIN_SIZE_MEDIUM) && !minimal;
    config->enable_cortical_columns = large_enough_for_cortical;
    config->num_hypercolumns = large_enough_for_cortical ? 10 : 0;
    config->minicolumns_per_hypercolumn = large_enough_for_cortical ? 100 : 0;
    config->neurons_per_minicolumn = large_enough_for_cortical ? 80 : 0;
    config->enable_laminar_structure = large_enough_for_cortical;
    config->enable_columnar_connectivity = large_enough_for_cortical;

    // Topographic maps only for brains with sensory processing enabled
    config->enable_visual_topographic = large_enough_for_cortical && config->enable_visual_cortex;
    config->enable_auditory_topographic = large_enough_for_cortical && config->enable_audio_cortex;
    config->enable_somatosensory_topographic = large_enough_for_cortical;
    config->enable_orientation_columns = large_enough_for_cortical && config->enable_visual_cortex;
    config->num_orientation_columns = large_enough_for_cortical ? 16 : 0;
    config->enable_feature_hypercolumns = large_enough_for_cortical;

    // Ternary WTA wrapper — opt-in only (default OFF to preserve checkpoint compat).
    config->enable_cortical_ternary = false;

    // Phase C2.1: Quantum Walk defaults
    config->enable_quantum_walk_diffusion = false;
    config->quantum_walk_steps = 50;
    config->quantum_classical_mixing = 0.2F;
    config->quantum_coin_type = 0;
    config->quantum_decoherence_rate = 0.05F;

    // Phase 10.12: Second Messenger Cascades (biological neuromodulation)
    // Second messengers are fundamental to how neuromodulators actually work in the brain.
    // Without them, dopamine/serotonin/etc. effects are simplified approximations.
    config->enable_second_messengers = !minimal;

    // Phase TM-3: Brain-Training Integration defaults
    config->enable_training_integration = !minimal;
    config->training_register_security = !minimal;
    config->training_default_lr = 0.0F;

    // Phase TM-4/5/6: Advanced Training Pipeline defaults
    // Enabled by default for non-minimal brains — essential for effective training
    config->enable_lr_scheduler = !minimal;
    config->enable_regularization = !minimal;
    config->regularization_l1_lambda = 0.0F;
    config->regularization_l2_lambda = 0.0001F;
    config->dropout_rate = 0.0F;
    config->enable_gradient_management = !minimal;
    config->enable_gradient_health_check = true;
    config->gradient_accumulation_steps = 1;
    config->gradient_clip_value = 100.0F;
    config->gradient_clip_norm = 100.0F;

    // Parallel subsystem initialization (wave-based thread pool)
    config->parallel_init = true;

    // Cognitive stage parameters (used in brain_decide())
    config->reasoning_blend_weight = 0.3F;
    config->dialogue_confidence_min = 0.2F;
    config->dialogue_confidence_max = 0.85F;
    config->imagination_confidence_min = 0.3F;
    config->rcog_confidence_max = 0.5F;
}

void nimcp_brain_factory_init_brain_stats(brain_stats_t* stats, const char* task_name, brain_size_t size,
                             uint32_t num_inputs, float learning_rate)
{
    uint32_t num_neurons = nimcp_brain_factory_get_neuron_count(size);

    stats->size = size;
    stats->num_neurons = num_neurons;

    // P2-59 FIX: Check for integer overflow in num_neurons * num_inputs
    // Use 64-bit multiplication to detect overflow
    uint64_t synapse_count = (uint64_t)num_neurons * (uint64_t)num_inputs;
    if (synapse_count > UINT32_MAX) {
        LOG_WARN(LOG_MODULE, "Synapse count overflow: %u * %u exceeds uint32_t, clamping",
                 num_neurons, num_inputs);
        stats->num_synapses = UINT32_MAX;
    } else {
        stats->num_synapses = (uint32_t)synapse_count;
    }
    stats->num_active_synapses = stats->num_synapses;
    stats->current_learning_rate = learning_rate;
    stats->quantum_annealing_runs = 0;

    // P2-58 FIX: Guard against task_name==NULL before strncpy
    if (task_name) {
        strncpy(stats->task_name, task_name, sizeof(stats->task_name) - 1);
        stats->task_name[sizeof(stats->task_name) - 1] = '\0';
    } else {
        stats->task_name[0] = '\0';
    }
}
