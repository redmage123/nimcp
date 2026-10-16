//=============================================================================
// nimcp_brain_config_profiles.c - Brain Configuration Profiles Implementation
//=============================================================================
/**
 * @file nimcp_brain_config_profiles.c
 * @brief Implementation of configuration profiles and builder pattern
 *
 * WHAT: Pre-defined configuration profiles to simplify brain creation
 * WHY:  Replace 90+ boolean flags with semantic profiles for common use cases
 * HOW:  Provide profile-based initialization with optional customization
 *
 * @version 2.7.0
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#include "core/brain/nimcp_brain.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>

#define LOG_MODULE "BRAIN_CONFIG_PROFILES"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_buffer_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_config_profiles, MESH_ADAPTER_CATEGORY_SYSTEM)


//=============================================================================
// Profile Names and Descriptions
//=============================================================================

static const char* PROFILE_NAMES[] = {
    "MINIMAL",
    "STANDARD",
    "COGNITIVE",
    "RESEARCH",
    "EMBEDDED"
};

static const char* PROFILE_DESCRIPTIONS[] = {
    "Bare minimum for inference - unit tests, swarm drones, embedded systems",
    "Common features for general use - production applications, decision making",
    "Full cognitive systems - consciousness simulation, social cognition, ethics",
    "All features for research - scientific research, full biological realism",
    "Optimized for constrained devices - IoT, edge computing, battery-powered"
};

//=============================================================================
// Feature Mapping Table
//=============================================================================

typedef struct {
    const char* name;
    size_t offset;  // Offset into brain_config_t
} feature_mapping_t;

// Macro to compute offset of a boolean field in brain_config_t
#define FEATURE_OFFSET(field) offsetof(brain_config_t, field)

static const feature_mapping_t FEATURE_MAP[] = {
    {"working_memory",       FEATURE_OFFSET(enable_working_memory)},
    {"theory_of_mind",       FEATURE_OFFSET(enable_theory_of_mind)},
    {"ethics",               FEATURE_OFFSET(enable_ethics)},
    {"empathy",              FEATURE_OFFSET(enable_empathy_responses)},
    {"mirror_neurons",       FEATURE_OFFSET(enable_mirror_neurons)},
    {"global_workspace",     FEATURE_OFFSET(enable_global_workspace)},
    {"introspection",        FEATURE_OFFSET(enable_introspection)},
    {"salience",             FEATURE_OFFSET(enable_salience)},
    {"consolidation",        FEATURE_OFFSET(enable_consolidation)},
    {"curiosity",            FEATURE_OFFSET(enable_curiosity)},
    {"visual_cortex",        FEATURE_OFFSET(enable_visual_cortex)},
    {"audio_cortex",         FEATURE_OFFSET(enable_audio_cortex)},
    {"speech_cortex",        FEATURE_OFFSET(enable_speech_cortex)},
    {"multimodal",           FEATURE_OFFSET(enable_multimodal_integration)},
    {"glial",                FEATURE_OFFSET(enable_glial)},
    {"oscillations",         FEATURE_OFFSET(enable_oscillations)},
    {"myelin",               FEATURE_OFFSET(enable_myelin_sheath)},
    {"meta_learning",        FEATURE_OFFSET(enable_meta_learning)},
    {"predictive",           FEATURE_OFFSET(enable_predictive_processing)},
    {"executive",            FEATURE_OFFSET(enable_executive_control)},
    {"emotional_tagging",    FEATURE_OFFSET(enable_emotional_tagging)},
    {"sleep_wake",           FEATURE_OFFSET(enable_sleep_wake_cycle)},
    {"mental_health",        FEATURE_OFFSET(enable_mental_health_monitoring)},
    {"natural_explanations", FEATURE_OFFSET(enable_natural_explanations)},
    {"logic",                FEATURE_OFFSET(enable_logic)},
    {"epistemic_filter",     FEATURE_OFFSET(enable_epistemic_filter)},
    {"brain_regions",        FEATURE_OFFSET(enable_brain_regions)},
    {"cortical_columns",     FEATURE_OFFSET(enable_cortical_columns)},
    {"attention",            FEATURE_OFFSET(enable_multihead_attention)},
    {"parietal",             FEATURE_OFFSET(enable_parietal)},
    {"dragonfly",            FEATURE_OFFSET(enable_dragonfly)},
    {"fault_tolerance",      FEATURE_OFFSET(enable_fault_tolerance)},
    {"bio_security",         FEATURE_OFFSET(enable_bio_security)},
    {"bbb_protection",       FEATURE_OFFSET(enable_bbb_protection)},
    {"brain_immune",         FEATURE_OFFSET(enable_brain_immune)},
    {"security_monitoring",  FEATURE_OFFSET(enable_security_monitoring)},
    {"training_integration", FEATURE_OFFSET(enable_training_integration)},
    {"distributed",          FEATURE_OFFSET(enable_distributed)},
    {"collective_cognition", FEATURE_OFFSET(enable_collective_cognition)},
    {"knowledge",            FEATURE_OFFSET(enable_knowledge)},
    {"wellbeing",            FEATURE_OFFSET(enable_wellbeing)},
    {"pink_noise",           FEATURE_OFFSET(enable_pink_noise)},
    {"eligibility_traces",   FEATURE_OFFSET(enable_eligibility_traces)},
    {"fractal_topology",     FEATURE_OFFSET(enable_fractal_topology)},
    {NULL, 0}  // Sentinel
};

static const size_t FEATURE_COUNT = sizeof(FEATURE_MAP) / sizeof(FEATURE_MAP[0]) - 1;

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Apply minimal profile settings (base for all profiles)
 */
static void apply_minimal_profile(brain_config_t* config)
{
    // Zero everything first
    memset(config, 0, sizeof(brain_config_t));

    // Set minimal mode flag
    config->minimal_mode = true;

    // Core defaults
    config->size = BRAIN_SIZE_SMALL;
    config->task = BRAIN_TASK_CLASSIFICATION;
    config->learning_rate = 0.01f;
    config->sparsity_target = 0.8f;

    // All cognitive features disabled by default in minimal mode
    config->enable_explanations = false;
    config->enable_working_memory = false;
    config->enable_theory_of_mind = false;
    config->enable_empathy_responses = false;
    config->enable_mirror_neurons = false;
    config->enable_global_workspace = false;
    config->enable_introspection = false;
    config->enable_ethics = false;
    config->enable_salience = false;
    config->enable_consolidation = false;
    config->enable_curiosity = false;
    config->enable_knowledge = false;
    config->enable_wellbeing = false;
    config->enable_logic = false;

    // All sensory cortices disabled
    config->enable_visual_cortex = false;
    config->enable_audio_cortex = false;
    config->enable_speech_cortex = false;
    config->enable_multimodal_integration = false;

    // All biological features disabled
    config->enable_glial = false;
    config->enable_oscillations = false;
    config->enable_myelin_sheath = false;
    config->enable_pink_noise = false;

    // All advanced features disabled
    config->enable_meta_learning = false;
    config->enable_predictive_processing = false;
    config->enable_executive_control = false;
    config->enable_emotional_tagging = false;
    config->enable_sleep_wake_cycle = false;
    config->enable_mental_health_monitoring = false;
    config->enable_natural_explanations = false;
    config->enable_brain_regions = false;
    config->enable_cortical_columns = false;
    config->enable_cortical_ternary = false;
    config->enable_multihead_attention = false;
    config->enable_parietal = false;
    config->enable_dragonfly = false;
    config->enable_fault_tolerance = false;

    // All security features disabled
    config->enable_bio_security = false;
    config->enable_bbb_protection = false;
    config->enable_brain_immune = false;
    config->enable_security_monitoring = false;
    config->enable_security_integration = false;

    // All distributed features disabled
    config->enable_distributed = false;
    config->enable_collective_cognition = false;
    config->enable_training_integration = false;

    // Lazy init - enabled for minimal mode
    config->lazy_init_mode = true;
    config->lazy_dendrite_init = true;
    config->lazy_axon_init = true;
    config->lazy_visual_init = true;
    config->lazy_audio_init = true;
    config->lazy_speech_init = true;
    config->lazy_working_memory_init = true;
    config->lazy_theory_of_mind_init = true;
    config->lazy_global_workspace_init = true;
    config->lazy_ethics_init = true;
    config->lazy_mirror_neurons_init = true;
    config->lazy_executive_init = true;
    config->lazy_consolidation_init = true;
    config->lazy_meta_learning_init = true;
    config->lazy_neuromod_init = true;
    config->lazy_glial_init = true;
    config->lazy_cortical_init = true;
    config->lazy_topographic_init = true;

    // Disable dendrites and axons in minimal mode
    config->enable_dendrites = false;
    config->enable_axons = false;
}

/**
 * @brief Apply standard profile settings
 */
static void apply_standard_profile(brain_config_t* config)
{
    apply_minimal_profile(config);

    // Override minimal mode
    config->minimal_mode = false;

    // Enable standard cognitive features
    config->enable_working_memory = true;
    config->working_memory_capacity = 7;
    config->working_memory_decay_tau_ms = 1000.0f;

    config->enable_global_workspace = true;
    config->workspace_capacity_dim = 256;
    config->workspace_ignition_threshold = 0.6f;
    config->workspace_refractory_ms = 50;
    config->workspace_enable_history = true;
    config->workspace_history_depth = 10;

    config->enable_introspection = true;
    config->enable_salience = true;
    config->enable_consolidation = true;
    config->enable_curiosity = true;
    config->enable_knowledge = true;
    config->enable_explanations = true;

    // Enable fuzzy logic and internal KG
    config->enable_fuzzy_logic = true;
    config->enable_internal_kg = true;

    // Enable basic biological features
    config->enable_pink_noise = true;
    config->enable_glial = true;
    config->enable_myelin_sheath = true;
    config->enable_dendrites = true;
    config->enable_axons = true;

    // Use glial ratio constants
    // (num_astrocytes, num_oligodendrocytes, num_microglia will be set
    //  based on neuron count when brain is created)

    // Keep lazy init enabled by default to reduce peak memory.
    // Subsystems are initialized on first access via BRAIN_ENSURE_* macros.
    config->lazy_init_mode = true;
    config->lazy_working_memory_init = true;
    config->lazy_global_workspace_init = true;
    config->lazy_consolidation_init = true;
}

/**
 * @brief Apply cognitive profile settings
 */
static void apply_cognitive_profile(brain_config_t* config)
{
    apply_standard_profile(config);

    // Enable all cognitive features
    config->enable_theory_of_mind = true;
    config->enable_empathy_responses = true;
    config->enable_false_belief_tracking = true;

    config->enable_mirror_neurons = true;
    config->mirror_neuron_count = 1000;
    config->mirror_max_actions = 100;
    config->mirror_max_agents = 10;
    config->mirror_learning_rate = 0.01f;
    config->mirror_match_threshold = 0.7f;

    config->enable_ethics = true;
    config->enable_wellbeing = true;

    config->enable_emotional_tagging = true;
    config->enable_emotional_memories = true;

    config->enable_executive_control = true;
    config->enable_task_switching = true;
    config->enable_planning = true;

    config->enable_meta_learning = true;
    config->enable_adaptive_meta_lr = true;

    config->enable_predictive_processing = true;
    config->enable_active_inference = true;

    config->enable_natural_explanations = true;
    config->enable_causal_explanations = true;

    config->enable_logic = true;
    config->enable_epistemic_filter = true;

    config->enable_sleep_wake_cycle = true;
    config->sleep_pressure_threshold = 0.8f;
    config->enable_memory_replay = true;
    config->enable_synaptic_homeostasis = true;

    config->enable_mental_health_monitoring = true;

    // Enable brain regions and columns
    config->enable_brain_regions = true;
    config->num_brain_regions = 4;
    config->neurons_per_region = 1000;

    config->enable_multihead_attention = true;
    config->num_attention_heads = 8;
    config->attention_key_dim = 64;
    config->enable_thalamic_gate = true;
    config->enable_salience_weighting = true;

    // Enable oscillations for cognitive binding
    config->enable_oscillations = true;

    // Disable lazy init for cognitive features
    config->lazy_theory_of_mind_init = false;
    config->lazy_ethics_init = false;
    config->lazy_mirror_neurons_init = false;
    config->lazy_executive_init = false;
    config->lazy_meta_learning_init = false;
}

/**
 * @brief Apply research profile settings
 */
static void apply_research_profile(brain_config_t* config)
{
    apply_cognitive_profile(config);

    // Enable ALL features for maximum research capability

    // Sensory cortices
    config->enable_visual_cortex = true;
    config->enable_audio_cortex = true;
    config->enable_speech_cortex = true;
    config->enable_multimodal_integration = true;
    config->visual_feature_dim = 128;
    config->audio_feature_dim = 64;
    config->speech_feature_dim = 64;
    config->language_feature_dim = 256;

    // Cortical columns
    config->enable_cortical_columns = true;
    config->enable_cortical_ternary = false;  /* opt-in even on full profile */
    config->num_hypercolumns = 10;
    config->minicolumns_per_hypercolumn = 100;
    config->neurons_per_minicolumn = 80;
    config->enable_laminar_structure = true;
    config->enable_columnar_connectivity = true;
    config->enable_visual_topographic = true;
    config->enable_auditory_topographic = true;
    config->enable_somatosensory_topographic = true;
    config->enable_orientation_columns = true;
    config->num_orientation_columns = 16;
    config->enable_feature_hypercolumns = true;

    // Parietal and dragonfly systems
    config->enable_parietal = true;
    config->parietal_weber_fraction = 0.1f;
    config->parietal_subitizing_limit = 4;
    config->parietal_rotation_rate_deg_ms = 0.053f;

    config->enable_dragonfly = true;
    config->dragonfly_tsdn_neurons = 16;
    config->dragonfly_tsdn_tuning_width = 0.5f;
    config->dragonfly_attention_threshold = 0.7f;
    config->dragonfly_enable_imm = true;
    config->dragonfly_prediction_horizon_ms = 200.0f;
    config->dragonfly_nav_gain = 3.0f;
    config->dragonfly_max_turn_rate = 6.0f;

    // Security features
    config->enable_bio_security = true;
    config->activity_warning_threshold = 0.8f;
    config->activity_danger_threshold = 0.95f;
    config->max_weight_delta_per_step = 0.1f;
    config->max_neuromod_rate_per_step = 0.2f;
    config->max_plasticity_disable_ratio = 0.1f;
    config->emergency_inhibit_on_attack = true;

    config->enable_bbb_protection = true;
    config->enable_brain_immune = true;
    config->enable_security_monitoring = true;
    config->enable_security_integration = true;

    // Fault tolerance
    config->enable_fault_tolerance = true;
    config->fault_tolerance_max_steps = 10;
    config->fault_tolerance_replanning_threshold = 0.3f;

    // Training integration
    config->enable_training_integration = true;
    config->training_register_security = true;
    config->enable_lr_scheduler = true;
    config->enable_regularization = true;
    config->enable_gradient_management = true;
    config->enable_gradient_health_check = true;

    // Biological plasticity
    config->enable_homeostatic_plasticity = true;
    config->homeostatic_target_rate_hz = 5.0f;
    config->homeostatic_tau_ms = 10000.0f;

    /* Brain-level dendritic_tree is a SINGLE tree, not a per-neuron 1:1
     * mapping with the 2M main-net population.  dendritic_tree_update() runs
     * but dendrite_compute_somatic_current() has no neuron to inject into.
     * The cortical-column dendrite path (cortex_cnn_dendritic) is a separate,
     * fully-wired subsystem and is unaffected by this flag.  Default OFF here
     * to match the documented default in nimcp_brain.h:1583 and prevent the
     * statue-shaped per-step update from running at the brain level. */
    config->enable_dendritic_computation = false;
    config->dendritic_branches = 8;
    config->dendritic_compartments = 5;

    config->enable_biological_predictive = true;
    config->predictive_levels = 3;
    config->predictive_learning_rate = 0.01f;

    config->enable_second_messengers = true;

    config->enable_eligibility_traces = true;
    config->enable_fractal_topology = true;

    // Quantum-inspired features
    config->enable_quantum_annealing = true;
    config->annealing_temperature_init = 10.0f;
    config->annealing_temperature_final = 0.1f;
    config->annealing_steps = 1000;
    config->quantum_annealing_frequency = 100;

    config->enable_quantum_walk_diffusion = true;
    config->quantum_walk_steps = 50;
    config->quantum_classical_mixing = 0.2f;

    config->complex_oscillation_enabled = true;
    config->complex_phase_update_rate = 0.1f;
    config->complex_amplitude_decay = 0.95f;

    // Geometric methods
    config->use_hyperbolic_knowledge = true;
    config->hyperbolic_curvature = -1.0f;
    config->hyperbolic_embedding_dim = 32;

    config->use_natural_gradient = true;
    config->fisher_damping = 1e-4f;

    config->learn_manifold_structure = true;
    config->manifold_neighborhood_size = 10;

    // MPS compression
    config->use_mps_weights = true;
    config->mps_bond_dimension = 10;
    config->mps_adaptive_bond_dim = true;
    config->mps_svd_tolerance = 1e-6f;

    // Collective cognition
    config->enable_collective_cognition = true;
    config->collective_max_instances = 16;
    config->collective_max_extensions = 32;
    config->collective_max_shared_goals = 64;
    config->collective_max_joint_attentions = 32;
    config->collective_sync_threshold = 0.7f;
    config->collective_enable_leader_detection = true;

    // Enable intuition system
    config->enable_intuition_system = true;

    // Enable KG reader
    config->enable_kg_reader = true;

    // Enable internal runtime KG
    config->enable_internal_kg = true;

    // Enable mental health guardian
    config->enable_mental_health_guardian = true;

    // Disable all lazy init for research mode
    config->lazy_init_mode = false;
    config->lazy_dendrite_init = false;
    config->lazy_axon_init = false;
    config->lazy_visual_init = false;
    config->lazy_audio_init = false;
    config->lazy_speech_init = false;
    config->lazy_neuromod_init = false;
    config->lazy_glial_init = false;
    config->lazy_cortical_init = false;
    config->lazy_topographic_init = false;
}

/**
 * @brief Apply embedded profile settings
 */
static void apply_embedded_profile(brain_config_t* config)
{
    apply_minimal_profile(config);

    // Override for embedded: minimal mode but with some useful features
    config->minimal_mode = false;

    // Enable reduced working memory
    config->enable_working_memory = true;
    config->working_memory_capacity = 4;  // Reduced from 7
    config->working_memory_decay_tau_ms = 500.0f;  // Faster decay

    // Enable basic cognitive features
    config->enable_introspection = true;
    config->enable_salience = true;

    // Keep all lazy init enabled for embedded
    config->lazy_init_mode = true;

    // Enable MPS compression for memory efficiency
    config->use_mps_weights = true;
    config->mps_bond_dimension = 5;  // High compression
    config->mps_adaptive_bond_dim = true;

    // Disable heavy biological features
    config->enable_cortical_columns = false;
    config->enable_cortical_ternary = false;
    config->enable_oscillations = false;
}

//=============================================================================
// Public API Implementation
//=============================================================================

brain_config_t brain_config_from_profile(brain_config_profile_t profile)
{
    brain_config_t config;

    switch (profile) {
        case BRAIN_CONFIG_MINIMAL:
            apply_minimal_profile(&config);
            break;

        case BRAIN_CONFIG_STANDARD:
            apply_standard_profile(&config);
            break;

        case BRAIN_CONFIG_COGNITIVE:
            apply_cognitive_profile(&config);
            break;

        case BRAIN_CONFIG_RESEARCH:
            apply_research_profile(&config);
            break;

        case BRAIN_CONFIG_EMBEDDED:
            apply_embedded_profile(&config);
            break;

        default:
            LOG_WARN(LOG_MODULE, "Unknown profile %d, using STANDARD", profile);
            apply_standard_profile(&config);
            break;
    }

    return config;
}

const char* brain_config_profile_name(brain_config_profile_t profile)
{
    if (profile >= 0 && profile < BRAIN_CONFIG_PROFILE_COUNT) {
        return PROFILE_NAMES[profile];
    }
    return "UNKNOWN";
}

const char* brain_config_profile_description(brain_config_profile_t profile)
{
    if (profile >= 0 && profile < BRAIN_CONFIG_PROFILE_COUNT) {
        return PROFILE_DESCRIPTIONS[profile];
    }
    return "Unknown profile";
}

brain_config_t brain_config_builder_create(brain_config_profile_t base)
{
    return brain_config_from_profile(base);
}

brain_config_t brain_config_builder_enable(brain_config_t config, const char* feature)
{
    if (!feature) {
        return config;
    }

    for (size_t i = 0; FEATURE_MAP[i].name != NULL; i++) {
        if (strcmp(FEATURE_MAP[i].name, feature) == 0) {
            bool* flag = (bool*)((char*)&config + FEATURE_MAP[i].offset);
            *flag = true;
            return config;
        }
    }

    LOG_WARN(LOG_MODULE, "Unknown feature: %s", feature);
    return config;
}

brain_config_t brain_config_builder_disable(brain_config_t config, const char* feature)
{
    if (!feature) {
        return config;
    }

    for (size_t i = 0; FEATURE_MAP[i].name != NULL; i++) {
        if (strcmp(FEATURE_MAP[i].name, feature) == 0) {
            bool* flag = (bool*)((char*)&config + FEATURE_MAP[i].offset);
            *flag = false;
            return config;
        }
    }

    LOG_WARN(LOG_MODULE, "Unknown feature: %s", feature);
    return config;
}

bool brain_config_is_feature_enabled(const brain_config_t* config, const char* feature)
{
    if (!config || !feature) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "brain_config_is_feature_enabled: invalid parameters");

            return false;
    }

    for (size_t i = 0; FEATURE_MAP[i].name != NULL; i++) {
        if (strcmp(FEATURE_MAP[i].name, feature) == 0) {
            const bool* flag = (const bool*)((const char*)config + FEATURE_MAP[i].offset);
            return *flag;
        }
    }

    return false;
}

uint32_t brain_config_get_enabled_features(const brain_config_t* config,
                                            char* buffer, size_t buffer_size)
{
    if (!config || !buffer || buffer_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "brain_config_get_enabled_features: invalid parameters");

            return 0;
    }

    buffer[0] = '\0';
    uint32_t count = 0;
    size_t written = 0;

    for (size_t i = 0; FEATURE_MAP[i].name != NULL; i++) {
        const bool* flag = (const bool*)((const char*)config + FEATURE_MAP[i].offset);
        if (*flag) {
            size_t name_len = strlen(FEATURE_MAP[i].name);
            if (written + name_len + 2 < buffer_size) {
                if (count > 0) {
                    buffer[written++] = ',';
                    buffer[written++] = ' ';
                }
                size_t remaining = buffer_size - written;
                strncpy(buffer + written, FEATURE_MAP[i].name, remaining - 1);
                buffer[written + remaining - 1] = '\0';
                written += name_len;
                count++;
            }
        }
    }

    return count;
}

bool brain_config_validate(const brain_config_t* config, brain_config_error_t* error)
{
    if (!config) {
        if (error) {
            error->code = BRAIN_CONFIG_ERR_NULL;
            snprintf(error->message, sizeof(error->message), "Configuration is NULL");
            error->field_name = NULL;
            error->dependency = NULL;
        }
        return false;
    }

    // Check size validity
    if (config->size > BRAIN_SIZE_CUSTOM) {
        if (error) {
            error->code = BRAIN_CONFIG_ERR_INVALID_SIZE;
            snprintf(error->message, sizeof(error->message),
                     "Invalid brain size: %d", config->size);
            error->field_name = "size";
            error->dependency = NULL;
        }
        return false;
    }

    // Check task validity
    if (config->task > BRAIN_TASK_CUSTOM) {
        if (error) {
            error->code = BRAIN_CONFIG_ERR_INVALID_TASK;
            snprintf(error->message, sizeof(error->message),
                     "Invalid brain task: %d", config->task);
            error->field_name = "task";
            error->dependency = NULL;
        }
        return false;
    }

    // Check learning rate range
    if (config->learning_rate < 0.0f || config->learning_rate > 1.0f) {
        if (error) {
            error->code = BRAIN_CONFIG_ERR_LEARNING_RATE;
            snprintf(error->message, sizeof(error->message),
                     "learning_rate must be in range [0.0, 1.0], got %f",
                     config->learning_rate);
            error->field_name = "learning_rate";
            error->dependency = NULL;
        }
        return false;
    }

    // Check sparsity target range
    if (config->sparsity_target < 0.0f || config->sparsity_target > 1.0f) {
        if (error) {
            error->code = BRAIN_CONFIG_ERR_SPARSITY;
            snprintf(error->message, sizeof(error->message),
                     "sparsity_target must be in range [0.0, 1.0], got %f",
                     config->sparsity_target);
            error->field_name = "sparsity_target";
            error->dependency = NULL;
        }
        return false;
    }

    // Check dependencies

    // Visual cortex requires multimodal integration
    if (config->enable_visual_cortex && !config->enable_multimodal_integration) {
        if (error) {
            error->code = BRAIN_CONFIG_ERR_DEPENDENCY;
            snprintf(error->message, sizeof(error->message),
                     "visual_cortex requires multimodal_integration to be enabled");
            error->field_name = "enable_visual_cortex";
            error->dependency = "enable_multimodal_integration";
        }
        return false;
    }

    // Audio cortex requires multimodal integration
    if (config->enable_audio_cortex && !config->enable_multimodal_integration) {
        if (error) {
            error->code = BRAIN_CONFIG_ERR_DEPENDENCY;
            snprintf(error->message, sizeof(error->message),
                     "audio_cortex requires multimodal_integration to be enabled");
            error->field_name = "enable_audio_cortex";
            error->dependency = "enable_multimodal_integration";
        }
        return false;
    }

    // Speech cortex requires multimodal integration
    if (config->enable_speech_cortex && !config->enable_multimodal_integration) {
        if (error) {
            error->code = BRAIN_CONFIG_ERR_DEPENDENCY;
            snprintf(error->message, sizeof(error->message),
                     "speech_cortex requires multimodal_integration to be enabled");
            error->field_name = "enable_speech_cortex";
            error->dependency = "enable_multimodal_integration";
        }
        return false;
    }

    // Empathy responses require theory of mind
    if (config->enable_empathy_responses && !config->enable_theory_of_mind) {
        if (error) {
            error->code = BRAIN_CONFIG_ERR_DEPENDENCY;
            snprintf(error->message, sizeof(error->message),
                     "empathy_responses requires theory_of_mind to be enabled");
            error->field_name = "enable_empathy_responses";
            error->dependency = "enable_theory_of_mind";
        }
        return false;
    }

    // False belief tracking requires theory of mind
    if (config->enable_false_belief_tracking && !config->enable_theory_of_mind) {
        if (error) {
            error->code = BRAIN_CONFIG_ERR_DEPENDENCY;
            snprintf(error->message, sizeof(error->message),
                     "false_belief_tracking requires theory_of_mind to be enabled");
            error->field_name = "enable_false_belief_tracking";
            error->dependency = "enable_theory_of_mind";
        }
        return false;
    }

    // Causal explanations require natural explanations
    if (config->enable_causal_explanations && !config->enable_natural_explanations) {
        if (error) {
            error->code = BRAIN_CONFIG_ERR_DEPENDENCY;
            snprintf(error->message, sizeof(error->message),
                     "causal_explanations requires natural_explanations to be enabled");
            error->field_name = "enable_causal_explanations";
            error->dependency = "enable_natural_explanations";
        }
        return false;
    }

    // Check for incompatible combinations

    // minimal_mode with heavy features is a warning, not error
    // (the brain factory will handle this by ignoring enable flags in minimal mode)

    // All checks passed
    if (error) {
        error->code = BRAIN_CONFIG_OK;
        snprintf(error->message, sizeof(error->message), "Configuration is valid");
        error->field_name = NULL;
        error->dependency = NULL;
    }

    return true;
}

bool brain_config_validate_and_fix(brain_config_t* config, brain_config_error_t* error)
{
    if (!config) {
        if (error) {
            error->code = BRAIN_CONFIG_ERR_NULL;
            snprintf(error->message, sizeof(error->message), "Configuration is NULL");
            error->field_name = NULL;
            error->dependency = NULL;
        }
        return false;
    }

    // Clamp learning rate
    if (config->learning_rate < 0.0f) {
        config->learning_rate = 0.0f;
    } else if (config->learning_rate > 1.0f) {
        config->learning_rate = 1.0f;
    }

    // Clamp sparsity target
    if (config->sparsity_target < 0.0f) {
        config->sparsity_target = 0.5f;
    } else if (config->sparsity_target > 1.0f) {
        config->sparsity_target = 0.99f;
    }

    // Auto-enable dependencies

    // Sensory cortices need multimodal integration
    if (config->enable_visual_cortex || config->enable_audio_cortex ||
        config->enable_speech_cortex) {
        config->enable_multimodal_integration = true;
    }

    // Empathy needs theory of mind
    if (config->enable_empathy_responses || config->enable_false_belief_tracking) {
        config->enable_theory_of_mind = true;
    }

    // Causal explanations need natural explanations
    if (config->enable_causal_explanations) {
        config->enable_natural_explanations = true;
    }

    // Now validate
    return brain_config_validate(config, error);
}

void brain_config_print_summary(const brain_config_t* config)
{
    if (!config) {
        printf("Configuration: NULL\n");
        return;
    }

    printf("=== Brain Configuration Summary ===\n");
    printf("Size: %d, Task: %d\n", config->size, config->task);
    printf("Inputs: %u, Outputs: %u\n", config->num_inputs, config->num_outputs);
    printf("Learning Rate: %.4f, Sparsity: %.2f\n",
           config->learning_rate, config->sparsity_target);
    printf("Minimal Mode: %s\n", config->minimal_mode ? "YES" : "NO");
    printf("Lazy Init: %s\n", config->lazy_init_mode ? "YES" : "NO");

    printf("\nEnabled Features:\n");
    char features[NIMCP_LOG_BUFFER_SIZE];
    uint32_t count = brain_config_get_enabled_features(config, features, sizeof(features));
    if (count > 0) {
        printf("  %s\n", features);
    } else {
        printf("  (none)\n");
    }
    printf("Total enabled: %u\n", count);
    printf("===================================\n");
}
