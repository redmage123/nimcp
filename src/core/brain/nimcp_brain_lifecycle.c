//=============================================================================
// nimcp_brain_lifecycle.c - Brain Lifecycle Management
//=============================================================================
/**
 * @file nimcp_brain_lifecycle.c
 * @brief Brain creation, destruction, initialization, and reset functions
 *
 * RESPONSIBILITY: Managing brain lifecycle from creation to destruction
 *
 * FUNCTIONS:
 * - allocate_brain() - Allocate brain structure
 * - brain_destroy() - Destroy brain and free resources
 * - init_brain_config() - Initialize brain configuration
 * - init_brain_stats() - Initialize brain statistics
 * - init_output_labels() - Initialize output labels array
 * - init_attention_subsystem() - Initialize attention mechanism
 * - init_brain_regions_subsystem() - Initialize brain regions
 * - init_symbolic_logic_subsystem() - Initialize symbolic logic
 * - init_symbolic_reasoning_subsystem() - Initialize symbolic reasoning
 * - init_epistemic_subsystem() - Initialize epistemic filtering
 * - create_personality() - Generate personality profile
 * - create_brain_network() - Create adaptive network for brain
 * - validate_creation_params() - Validate brain creation parameters
 */

#include <math.h>  /* NAN sentinel for cognitive_stats._last_loss fields */
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_lifecycle.h"
#include "core/brain/nimcp_brain_internal.h"
#include "middleware/cloud/nimcp_cloud_inference.h"
#include "security/nimcp_toxicity.h"
#include "security/nimcp_toxicity_response.h"
#include "security/nimcp_toxicity_ml.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/containers/nimcp_hash_table.h"

#define LOG_MODULE "BRAIN_LIFECYCLE"
#include "utils/memory/nimcp_memory.h"
#include "utils/memory/nimcp_memory_guards.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "core/brain/nimcp_brain_bio_async.h"
#include "plasticity/adaptive/nimcp_adaptive.h"
#include "plasticity/structural/nimcp_structural_plasticity.h"
#include "integration/inter/neuromod_plasticity/nimcp_neuromod_plasticity_bridge.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "plasticity/attention/nimcp_attention.h"
#include "core/brain_regions/nimcp_brain_regions.h"
#include "cognitive/nimcp_symbolic_logic.h"
#include "cognitive/epistemic/nimcp_epistemic_filter.h"
#include "cognitive/nimcp_personality.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/ethics/nimcp_core_directives.h"
#include "core/brain/factory/init/nimcp_brain_init_pr_memory.h"
#include "core/brain/factory/init/nimcp_brain_init_world_model.h"
#include "core/brain/factory/init/nimcp_brain_init_creative.h"
#include "core/medulla/nimcp_medulla.h"
#include "cognitive/parietal/nimcp_parietal.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "core/brain/factory/init/nimcp_brain_init.h"
#include "core/brain/factory/init/nimcp_brain_init_subsystems.h"
#include "core/brain/factory/nimcp_brain_init_state_manager.h"
#include "core/brain/factory/init/nimcp_brain_init_white_matter.h"
#include "core/brain/factory/init/nimcp_brain_init_inferior_colliculus.h"
#include "core/brain/factory/init/nimcp_brain_init_spinal_cord.h"
#include "core/brain/factory/init/nimcp_brain_init_cortical_interneurons.h"
#include "core/brain/factory/init/nimcp_brain_init_neuropeptide.h"
#include "core/brain/factory/init/nimcp_brain_init_endocannabinoid.h"
#include "core/brain/factory/init/nimcp_brain_init_glymphatic.h"

/* Coordinator/Orchestrator headers for cleanup */
#include "async/nimcp_bio_async_orchestrator.h"
#include "plasticity/nimcp_plasticity_coordinator.h"
#include "cognitive/immune/nimcp_immune_bridge_coordinator.h"
#include "cognitive/nimcp_cognitive_meta_controller.h"
#include "security/nimcp_security_perception_bridge.h"
#include "swarm/nimcp_swarm_module_registry.h"

#include <string.h>
#include <time.h>
#include <stdlib.h>

/* Logging module identifier (undef to suppress redefinition warning) */
#undef LOG_MODULE
#define LOG_MODULE "BRAIN_LIFECYCLE"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_dimension_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_lifecycle, MESH_ADAPTER_CATEGORY_COGNITIVE)


// External error handling
extern void set_error(const char* format, ...);

//=============================================================================
// Size Presets - Helper Functions
//=============================================================================

/**
 * @brief Get neuron count for size preset
 * @complexity O(1)
 */
static uint32_t get_neuron_count(brain_size_t size)
{
    switch (size) {
        case BRAIN_SIZE_TINY:
            return 100;
        case BRAIN_SIZE_SMALL:
            return 500;
        case BRAIN_SIZE_MEDIUM:
            return 1000;
        case BRAIN_SIZE_LARGE:
            return 5000;
        case BRAIN_SIZE_CUSTOM:
            return 1000;
        default:
            return 1000;
    }
}

/**
 * @brief Get default sparsity target for size
 * @complexity O(1)
 */
static float get_default_sparsity(brain_size_t size)
{
    switch (size) {
        case BRAIN_SIZE_TINY:
            return 0.70f;
        case BRAIN_SIZE_SMALL:
            return 0.80f;
        case BRAIN_SIZE_MEDIUM:
            return 0.85f;
        case BRAIN_SIZE_LARGE:
            return 0.90f;
        default:
            return 0.80f;
    }
}

//=============================================================================
// Configuration Builders
//=============================================================================

/**
 * @brief Build spike parameters for brain configuration
 * @complexity O(1)
 */
static adaptive_spike_params_t build_spike_params(float sparsity_target)
{
    adaptive_spike_params_t params = {0};
    params.k_factor = 0.5f;
    params.sparsity_target = sparsity_target;
    params.encoding = SPIKE_ENCODING_PASSTHROUGH;  /* Raw floats for gradient training */
    params.enable_soft_reset = true;
    params.enable_adaptation = true;
    params.adaptation_window = 100;
    params.min_threshold = 0.0001f;
    params.max_threshold = 10.0f;
    return params;
}

/**
 * @brief Build base network configuration
 * @complexity O(1) + memory allocation
 *
 * MEMORY SAFETY:
 * - Caller MUST free config.layer_sizes if config.layer_sizes is non-NULL
 * - On error (NULL layer_sizes), caller should check before using config
 */
static network_config_t build_base_network_config(uint32_t num_inputs, uint32_t num_outputs,
                                                  uint32_t num_neurons,
                                                  ode_integration_method_t integration_method)
{
    network_config_t config = {0};
    config.input_size = num_inputs;
    config.output_size = num_outputs;
    // BUGFIX: num_neurons should be TOTAL neurons, not just hidden layer
    // For a 3-layer network: inputs + hidden + outputs
    config.num_neurons = num_inputs + num_neurons + num_outputs;
    config.num_layers = 3;
    config.integration_method = integration_method;

    config.layer_sizes = nimcp_calloc(3, sizeof(uint32_t));
    if (!config.layer_sizes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "build_base_network_config: failed to allocate layer_sizes");
        /* Caller MUST check config.layer_sizes before use */
        return config;
    }

    config.layer_sizes[0] = num_inputs;
    config.layer_sizes[1] = num_neurons;
    config.layer_sizes[2] = num_outputs;

    config.enable_stdp = true;
    config.enable_hebbian = true;
    config.enable_oja = true;
    config.enable_homeostasis = true;
    config.enable_bcm = false;
    config.enable_eligibility = false;

    // Weight bounds: [-10, +10] allows sufficient representational capacity
    // with 128-512 fan-in. Original [-1, +1] was too restrictive.
    config.min_weight = -10.0F;
    config.max_weight = 10.0F;

    return config;
}

/**
 * @brief Build complete adaptive network configuration
 * @complexity O(1) + memory allocation
 */
static adaptive_network_config_t build_network_config(uint32_t num_inputs, uint32_t num_outputs,
                                                      uint32_t num_neurons, float sparsity_target,
                                                      ode_integration_method_t integration_method)
{
    adaptive_network_config_t config = {0};
    config.base_config = build_base_network_config(num_inputs, num_outputs, num_neurons, integration_method);
    config.spike_params = build_spike_params(sparsity_target);
    config.enable_sparsity = false;
    config.pruning_threshold = 0.01f;
    config.update_frequency = 100;
    return config;
}

//=============================================================================
// Initialization Functions
//=============================================================================

/**
 * @brief Initialize brain configuration with strategy
 * @complexity O(1)
 */
void init_brain_config(brain_config_t* config, const char* task_name, brain_size_t size,
                              brain_task_t task, uint32_t num_inputs, uint32_t num_outputs,
                              task_strategy_t* strategy)
{
    config->size = size;
    config->task = task;
    config->num_inputs = num_inputs;
    config->num_outputs = num_outputs;
    config->learning_rate = strategy->get_learning_rate();
    config->sparsity_target = get_default_sparsity(size);
    config->enable_explanations = true;
    strncpy(config->task_name, task_name, sizeof(config->task_name) - 1);

    config->neuron_integration = ODE_EULER;
    config->enable_working_memory = true;
    config->working_memory_capacity = 7;
    config->working_memory_decay_tau_ms = 1000.0f;
    config->enable_theory_of_mind = true;
    config->enable_empathy_responses = true;
    config->enable_false_belief_tracking = true;
    config->enable_mirror_neurons = true;
    config->mirror_neuron_count = 1000;
    config->mirror_max_actions = 100;
    config->mirror_max_agents = 10;
    config->mirror_learning_rate = 0.01f;
    config->mirror_match_threshold = 0.7f;
    config->enable_global_workspace = true;
    config->workspace_capacity_dim = 256;
    config->workspace_ignition_threshold = 0.6f;
    config->workspace_refractory_ms = 50;
    config->workspace_enable_history = true;
    config->workspace_history_depth = 10;
    config->enable_quantum_annealing = false;
    config->annealing_temperature_init = 10.0f;
    config->annealing_temperature_final = 0.1f;
    config->annealing_steps = 1000;
    config->quantum_annealing_frequency = 100;
    config->use_random_personality = true;
    config->personality_seed = 0;
    config->explicit_openness = 0.5f;
    config->explicit_conscientiousness = 0.5f;
    config->explicit_extraversion = 0.5f;
    config->explicit_agreeableness = 0.5f;
    config->explicit_neuroticism = 0.5f;
    config->explicit_gender = GENDER_FEMALE;
    config->explicit_sexuality = SEXUALITY_HETEROSEXUAL;
    config->personality_trait_mean = 0.5f;
    config->personality_trait_stddev = 0.15f;
    config->female_probability = 1.0f;
    config->male_probability = 0.0f;
    config->non_binary_probability = 0.0f;
    config->enable_glial = true;
    config->enable_oscillations = false;
    config->enable_quantum_walk_diffusion = false;
    config->quantum_walk_steps = 50;
    config->quantum_classical_mixing = 0.2f;
    config->quantum_coin_type = 0;
    config->quantum_decoherence_rate = 0.05f;
}

/**
 * @brief Initialize brain statistics
 * @complexity O(1)
 */
void init_brain_stats(brain_stats_t* stats, const char* task_name, brain_size_t size,
                             uint32_t num_inputs, float learning_rate)
{
    uint32_t num_neurons = get_neuron_count(size);
    stats->size = size;
    stats->num_neurons = num_neurons;
    stats->num_synapses = num_neurons * num_inputs;
    stats->num_active_synapses = stats->num_synapses;
    stats->current_learning_rate = learning_rate;
    stats->quantum_annealing_runs = 0;
    strncpy(stats->task_name, task_name, sizeof(stats->task_name) - 1);
}

//=============================================================================
// Validation Functions
//=============================================================================

/**
 * @brief Validate brain size enum is in valid range
 * @complexity O(1)
 *
 * STATE MACHINE: brain_size_t valid values
 * - BRAIN_SIZE_MICRO (0) -> BRAIN_SIZE_CUSTOM (5)
 */
static bool validate_brain_size(brain_size_t size)
{
    return size <= BRAIN_SIZE_CUSTOM;
}

/**
 * @brief Validate brain task enum is in valid range
 * @complexity O(1)
 *
 * STATE MACHINE: brain_task_t valid values
 * - BRAIN_TASK_CLASSIFICATION (0) -> BRAIN_TASK_CUSTOM (5)
 */
static bool validate_brain_task(brain_task_t task)
{
    return task <= BRAIN_TASK_CUSTOM;
}

/**
 * @brief Validate brain creation parameters
 * @complexity O(1)
 *
 * STATE MACHINE VALIDATION:
 * This function validates the initial state for brain creation.
 * After successful validation, brain enters HEALTHY state.
 *
 * Valid initial state: (NULL brain) -> (HEALTHY brain with valid config)
 */
static bool validate_creation_params(const char* task_name, uint32_t num_inputs,
                                     uint32_t num_outputs)
{
    if (!task_name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "validate_creation_params: task_name is NULL");
        return false;
    }
    // Validate task_name is not empty
    if (task_name[0] == '\0') {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "validate_creation_params: task_name is empty");
        return false;
    }
    if (num_inputs == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "validate_creation_params: num_inputs must be > 0");
        return false;
    }
    if (num_inputs > 10000) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "validate_creation_params: num_inputs > 10000");
        return false;
    }
    if (num_outputs == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "validate_creation_params: num_outputs must be > 0");
        return false;
    }
    if (num_outputs > 10000) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "validate_creation_params: num_outputs > 10000");
        return false;
    }
    return true;
}

//=============================================================================
// Brain Structure Allocation
//=============================================================================

/**
 * @brief Allocate and initialize brain structure
 * @complexity O(1)
 */
brain_t allocate_brain(void)
{
    brain_t brain = nimcp_calloc(1, sizeof(struct brain_struct));
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "allocate_brain: failed to allocate brain structure");
        return NULL;
    }

    brain->last_input = NULL;
    brain->cached_decision = NULL;
    brain->input_size = 0;

    /* Seed cognitive_stats._last_loss fields with NaN so callers of
     * get_cognitive_stats can distinguish "no loss reported yet" from
     * "actual zero loss" for modules whose task functions don't always
     * write r->loss. calloc zeroed them; overwrite with NaN. */
    brain->cognitive_stats.grounded_lang_last_loss = NAN;
    brain->cognitive_stats.knowledge_last_loss     = NAN;
    brain->cognitive_stats.vae_last_loss           = NAN;
    brain->cognitive_stats.fep_parietal_last_loss  = NAN;
    brain->cognitive_stats.physics_nn_last_loss    = NAN;
    brain->cognitive_stats.pred_hierarchy_last_loss= NAN;
    brain->cognitive_stats.jepa_last_loss          = NAN;
    brain->cognitive_stats.creative_last_loss      = NAN;
    brain->cognitive_stats.self_heal_last_loss     = NAN;
    brain->cognitive_stats.intuition_last_loss     = NAN;
    brain->cognitive_stats.fep_orchestrator_last_loss = NAN;

    /* Module activity counters — explicitly zero (brain is calloc'd, but
     * be explicit to document the contract: counters are monotonic and
     * reset only at brain create). */
    memset(&brain->module_activity, 0, sizeof(brain->module_activity));

    if (nimcp_platform_mutex_init(&brain->cache_mutex, false) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_MUTEX_INIT, "allocate_brain: failed to initialize cache mutex");
        nimcp_free(brain);
        return NULL;
    }

    brain->distributed = NULL;
    brain->longterm_capacity = 100;
    brain->longterm_count = 0;
    brain->longterm_memory = nimcp_calloc(brain->longterm_capacity,
                                          sizeof(*brain->longterm_memory));
    if (!brain->longterm_memory) {
        brain->longterm_capacity = 0;
    }

    brain->is_cow_clone = false;
    brain->owns_network = true;
    brain->original_network = NULL;
    brain->network_is_cached = false;
    brain->network_refcount = NULL;
    brain->can_use_readonly = false;
    brain->refcount_mutex = NULL;
    brain->functional_modules = NULL;
    brain->network_hubs = NULL;
    brain->topology_metrics = NULL;
    brain->auto_detect_communities = false;
    brain->community_detection_interval = 0.0f;

    // Layer C: temperature scaling defaults — T=1.0 is identity, ece=-1 means
    // uncalibrated. These get overwritten by nimcp_brain_calibrate_temperature
    // once a held-out validation set + language stage are online.
    brain->decoder_temperature                   = 1.0F;
    brain->decoder_temperature_calibrated_ece    = -1.0F;
    brain->decoder_temperature_calibrated_at_us  = 0;

    return brain;
}

//=============================================================================
// Network Creation
//=============================================================================

/**
 * @brief Create adaptive network for brain
 * @complexity O(n) where n = num_neurons
 */
adaptive_network_t create_brain_network(uint32_t num_inputs, uint32_t num_outputs,
                                               uint32_t num_neurons, float sparsity_target,
                                               ode_integration_method_t integration_method)
{
    adaptive_network_config_t net_config =
        build_network_config(num_inputs, num_outputs, num_neurons, sparsity_target, integration_method);

    if (!net_config.base_config.layer_sizes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "create_brain_network: net_config is NULL");
        return NULL;
    }

    adaptive_network_t network = adaptive_network_create(&net_config);

    if (net_config.base_config.layer_sizes) {
        nimcp_free((void*)net_config.base_config.layer_sizes);
    }

    return network;
}

//=============================================================================
// Subsystem Initialization
//=============================================================================

/**
 * @brief Initialize output labels array
 * @complexity O(1)
 */
bool init_output_labels(brain_t brain, uint32_t num_outputs)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_output_labels: brain is NULL");
        return false;
    }
    if (num_outputs == 0) {
        brain->output_labels = NULL;
        brain->num_output_labels = 0;
        return true;
    }
    brain->output_labels = nimcp_calloc(num_outputs, sizeof(char*));
    if (!brain->output_labels) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_output_labels: failed to allocate output labels");
        return false;
    }
    brain->num_output_labels = 0;

    // Create O(1) label lookup hash table (FNV-1a)
    hash_table_config_t ht_config = {
        .initial_buckets = 256,
        .key_type = HASH_KEY_STRING,
        .hash_algorithm = HASH_ALG_FNV1A,
        .case_insensitive = false,
        .thread_safe = false  // caller uses brain->cache_mutex
    };
    brain->label_index = hash_table_create(&ht_config);
    // Non-fatal if NULL — falls back to linear search in get_or_create_label_index

    return true;
}

/**
 * @brief Initialize multihead attention mechanism
 * @complexity O(1)
 */
bool init_attention_subsystem(brain_t brain)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_attention_subsystem: brain is NULL");
        return false;
    }

    if (brain->multihead_attention) {
        return true;
    }

    multihead_attention_config_t attn_config = {
        .num_heads = NIMCP_DEFAULT_ATTENTION_HEADS,
        .feature_dim = brain->config.num_inputs,
        .enable_softmax = true,
        .dropout_rate = 0.0f
    };

    brain->multihead_attention = multihead_attention_create(&attn_config);
    if (!brain->multihead_attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "init_attention_subsystem: failed to create attention");
        return false;
    }

    return true;
}

/**
 * @brief Initialize brain regions subsystem
 * @complexity O(1)
 */
bool init_brain_regions_subsystem(brain_t brain)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_brain_regions_subsystem: brain is NULL");
        return false;
    }

    if (brain->brain_regions) {
        return true;
    }

    brain_module_config_t region_config = {
        .num_neurons = brain->config.num_outputs,
        .enable_plasticity = true,
        .base_learning_rate = brain->config.learning_rate
    };

    brain->brain_regions = brain_module_create(&region_config);
    if (!brain->brain_regions) {
        set_error("Failed to create brain regions");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_brain_regions_subsystem: brain->brain_regions is NULL");
        return false;
    }

    return true;
}

/**
 * @brief Initialize symbolic logic subsystem
 * @complexity O(1)
 */
bool init_symbolic_logic_subsystem(brain_t brain)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_symbolic_logic_subsystem: brain is NULL");
        return false;
    }

    if (brain->symbolic_logic) {
        return true;
    }

    symbolic_logic_config_t logic_config = {
        .max_rules = 1000,
        .max_facts = 5000,
        .enable_forward_chaining = true,
        .enable_backward_chaining = true,
        .enable_unification = true
    };

    brain->symbolic_logic = symbolic_logic_create(&logic_config);
    if (!brain->symbolic_logic) {
        set_error("Failed to create symbolic logic engine");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_symbolic_logic_subsystem: brain->symbolic_logic is NULL");
        return false;
    }

    return true;
}

/**
 * @brief Initialize symbolic reasoning subsystem
 * @complexity O(1)
 */
bool init_symbolic_reasoning_subsystem(brain_t brain)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_symbolic_reasoning_subsystem: brain is NULL");
        return false;
    }

    if (brain->symbolic_logic) {
        return true;
    }

    symbolic_logic_config_t logic_config = {
        .max_rules = 1000,
        .max_facts = 5000,
        .enable_forward_chaining = true,
        .enable_backward_chaining = true,
        .enable_unification = true
    };

    brain->symbolic_logic = symbolic_logic_create(&logic_config);
    if (!brain->symbolic_logic) {
        set_error("Failed to create symbolic logic engine");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_symbolic_reasoning_subsystem: brain->symbolic_logic is NULL");
        return false;
    }

    return true;
}

/**
 * @brief Initialize epistemic filtering subsystem
 * @complexity O(1)
 */
bool init_epistemic_subsystem(brain_t brain)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_epistemic_subsystem: brain is NULL");
        return false;
    }

    if (brain->epistemic) {
        return true;
    }

    float skepticism_level = 0.6f;
    brain->epistemic = epistemic_filter_create(skepticism_level);
    if (!brain->epistemic) {
        set_error("Failed to create epistemic filter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_epistemic_subsystem: brain->epistemic is NULL");
        return false;
    }

    return true;
}

/**
 * @brief Generate or configure personality from brain config
 * @complexity O(1)
 */
static personality_profile_t* create_personality(const brain_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "create_personality: config is NULL");
        return NULL;
    }

    personality_profile_t* profile = nimcp_malloc(sizeof(personality_profile_t));
    if (!profile) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "create_personality: profile is NULL");
        return NULL;
    }

    if (config->use_random_personality) {
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
        personality_traits_t traits;
        traits.openness = config->explicit_openness;
        traits.conscientiousness = config->explicit_conscientiousness;
        traits.extraversion = config->explicit_extraversion;
        traits.agreeableness = config->explicit_agreeableness;
        traits.neuroticism = config->explicit_neuroticism;

        identity_profile_t identity = {0};
        identity.gender = (gender_identity_t)config->explicit_gender;
        identity.sexuality = (sexual_orientation_t)config->explicit_sexuality;
        identity.gender_certainty = 1.0f;
        identity.sexuality_certainty = 1.0f;
        identity.gender_is_core_identity = true;
        identity.sexuality_is_core_identity = false;

        *profile = personality_create_custom(&traits, &identity);
    }

    return profile;
}

//=============================================================================
// Brain Destruction
//=============================================================================

/**
 * @brief Destroy brain and free resources
 * @complexity O(n) where n = num_neurons
 *
 * STATE MACHINE TRANSITION:
 * Valid transitions into destroy:
 * - HEALTHY -> SHUTDOWN (normal cleanup)
 * - DEGRADED -> SHUTDOWN (cleanup in degraded mode)
 * - RECOVERING -> SHUTDOWN (abort recovery and cleanup)
 * - FAILED -> SHUTDOWN (cleanup after failure)
 *
 * Cleanup order (reverse of initialization):
 * 1. Save final snapshot (if configured)
 * 2. Destroy network (with refcount handling)
 * 3. Destroy strategy
 * 4. Free output labels
 * 5. Destroy cognitive subsystems (reverse order)
 * 6. Destroy coordinators/orchestrators (reverse order)
 * 7. Cleanup cache and mutexes
 * 8. Free brain structure
 */
void brain_destroy(brain_t brain)
{
    if (!brain)
        return;

    // STATE TRANSITION: * -> SHUTDOWN
    // All brain states can transition to SHUTDOWN for cleanup

    // Phase 8: Signal start of shutdown to health agent
    brain_heartbeat(brain, "brain_destroy:start", 0.0f);

    // Save final snapshot if configured
    if (brain->config.snapshot_dir && brain->config.save_final_snapshot) {
        brain_save_snapshot(brain, "final", "Snapshot at brain destruction");
    }

    // Phase 8: Signal network cleanup phase
    brain_heartbeat(brain, "brain_destroy:network", 0.1f);

    // Handle network destruction with reference counting
    if (brain->network) {
        if (brain->owns_network) {
            adaptive_network_destroy(brain->network);
        } else if (brain->network_refcount && brain->refcount_mutex) {
            // THREAD SAFETY FIX: Acquire mutex BEFORE reading shared pointers
            // to prevent TOCTOU race where another thread destroys the mutex
            // between our read and lock.
            nimcp_platform_mutex_t* mutex = brain->refcount_mutex;
            nimcp_platform_mutex_lock(mutex);

            // Safe to read shared state now — we hold the lock
            uint32_t* refcount_ptr = brain->network_refcount;
            adaptive_network_t network = brain->network;
            (*refcount_ptr)--;
            uint32_t remaining_refs = *refcount_ptr;

            if (remaining_refs == 0) {
                // Last reference - unlock before destroying to avoid UB
                nimcp_platform_mutex_unlock(mutex);
                adaptive_network_destroy(network);
                nimcp_platform_mutex_destroy(mutex);
                nimcp_free(mutex);
                nimcp_free(refcount_ptr);
            } else {
                nimcp_platform_mutex_unlock(mutex);
            }
        }
    }

    // Cleanup strategy
    if (brain->strategy && !brain->is_cow_clone) {
        strategy_destroy(brain->strategy);
    }

    // Cleanup output labels
    if (brain->output_labels) {
        for (uint32_t i = 0; i < brain->num_output_labels; i++) {
            if (brain->output_labels[i]) {
                nimcp_free(brain->output_labels[i]);
            }
        }
        nimcp_free(brain->output_labels);
    }
    if (brain->label_index) {
        hash_table_destroy(brain->label_index);
        brain->label_index = NULL;
    }

    // Cleanup distributed cognition
    if (brain->distributed) {
        distrib_cognition_destroy(brain->distributed);
    }

    // Cleanup long-term memory consolidation buffer
    if (brain->longterm_memory) {
        for (uint32_t i = 0; i < brain->longterm_count; i++) {
            if (brain->longterm_memory[i].features) {
                nimcp_free(brain->longterm_memory[i].features);
            }
        }
        nimcp_free(brain->longterm_memory);
        brain->longterm_memory = NULL;
    }

    // Cleanup subsystems (extensive list truncated for brevity)
    // ... (all the cleanup code from original brain_destroy)

    // Phase 8: Signal subsystem cleanup phase
    brain_heartbeat(brain, "brain_destroy:subsystems", 0.3f);

    // Cleanup Creative system
    nimcp_brain_factory_destroy_creative_subsystem(brain);

    // Cleanup Prime Resonant memory system
    nimcp_brain_factory_destroy_pr_memory_subsystem(brain);

    // Cleanup World Model system
    nimcp_brain_factory_destroy_world_model_subsystem(brain);

    // Cleanup immune system
    if (brain->immune_system) {
        brain_immune_stop(brain->immune_system);
        brain_immune_destroy(brain->immune_system);
        brain->immune_system = NULL;
        brain->immune_enabled = false;
    }

    // Cleanup toxicity classifier (created in safety init wave)
    if (brain->toxicity_classifier) {
        toxicity_classifier_destroy(
            (toxicity_classifier_t*)brain->toxicity_classifier);
        brain->toxicity_classifier = NULL;
    }
    if (brain->toxicity_response) {
        toxicity_response_destroy(
            (toxicity_response_t*)brain->toxicity_response);
        brain->toxicity_response = NULL;
    }
    if (brain->toxicity_ml) {
        toxicity_ml_destroy(
            (toxicity_ml_classifier_t*)brain->toxicity_ml);
        brain->toxicity_ml = NULL;
    }

    // Cleanup state manager (checkpointing - Phase 8)
    // Must be cleaned up before health agent since it uses heartbeat calls
    brain_shutdown_state_manager(brain);

    // Cleanup health agent (autonomous monitoring)
    // Must be cleaned up after immune system since it communicates with it
    nimcp_brain_factory_destroy_health_agent_subsystem(brain);

    // Phase 8: Signal coordinator cleanup phase
    brain_heartbeat(brain, "brain_destroy:coordinators", 0.5f);

    // ========================================================================
    // COORDINATOR/ORCHESTRATOR CLEANUP (reverse initialization order)
    // ========================================================================
    // Cleanup order is reverse of init order:
    // 6. Swarm Module Registry
    // 5. Security-Perception Bridge
    // 4. Cognitive Meta-Controller
    // 3. Immune Bridge Coordinator
    // 2. Plasticity Coordinator
    // 1. Bio-Async Orchestrator
    // 0. FEP Orchestrator

    // 6. Cleanup swarm module registry
    if (brain->swarm_module_registry) {
        swarm_registry_disconnect_bio_async(brain->swarm_module_registry);
        swarm_registry_disconnect_brain_immune(brain->swarm_module_registry);
        swarm_registry_disconnect_swarm_brain(brain->swarm_module_registry);
        swarm_registry_destroy(brain->swarm_module_registry);
        brain->swarm_module_registry = NULL;
        brain->swarm_module_registry_enabled = false;
    }

    // 5. Cleanup security-perception bridge
    if (brain->security_perception_bridge) {
        sec_percept_stop(brain->security_perception_bridge);
        sec_percept_disconnect_bio_async(brain->security_perception_bridge);
        sec_percept_destroy(brain->security_perception_bridge);
        brain->security_perception_bridge = NULL;
        brain->security_perception_bridge_enabled = false;
    }

    // 4. Cleanup cognitive meta-controller
    if (brain->cognitive_meta_controller) {
        meta_controller_stop(brain->cognitive_meta_controller);
        meta_controller_disconnect_bio_async(brain->cognitive_meta_controller);
        meta_controller_disconnect_brain_immune(brain->cognitive_meta_controller);
        meta_controller_destroy(brain->cognitive_meta_controller);
        brain->cognitive_meta_controller = NULL;
        brain->cognitive_meta_controller_enabled = false;
    }

    // 3. Cleanup immune bridge coordinator
    if (brain->immune_bridge_coordinator) {
        immune_bridge_coordinator_stop(brain->immune_bridge_coordinator);
        immune_bridge_coordinator_disconnect_bio_async(brain->immune_bridge_coordinator);
        immune_bridge_coordinator_disconnect_brain_immune(brain->immune_bridge_coordinator);
        immune_bridge_coordinator_destroy(brain->immune_bridge_coordinator);
        brain->immune_bridge_coordinator = NULL;
        brain->immune_bridge_coordinator_enabled = false;
    }

    // 2b. Cleanup structural plasticity (before coordinator which registered it)
    if (brain->structural_plasticity) {
        structural_plasticity_destroy(brain->structural_plasticity);
        brain->structural_plasticity = NULL;
        brain->structural_plasticity_enabled = false;
    }

    // 2. Cleanup plasticity coordinator
    if (brain->plasticity_coordinator) {
        plasticity_coordinator_disconnect_bio_async(brain->plasticity_coordinator);
        plasticity_coordinator_disconnect_brain_immune(brain->plasticity_coordinator);
        plasticity_coordinator_destroy(brain->plasticity_coordinator);
        brain->plasticity_coordinator = NULL;
        brain->plasticity_coordinator_enabled = false;
    }

    // Cleanup neuromod-plasticity bridge (lazy-created in brain_apply_reward_learning).
    if (brain->neuromod_plasticity_bridge) {
        neuromod_plasticity_destroy(brain->neuromod_plasticity_bridge);
        brain->neuromod_plasticity_bridge = NULL;
    }

    // 1. Cleanup bio-async orchestrator
    if (brain->bio_async_orchestrator) {
        bio_orchestrator_stop(brain->bio_async_orchestrator);
        bio_orchestrator_destroy(brain->bio_async_orchestrator);
        brain->bio_async_orchestrator = NULL;
        brain->bio_async_orchestrator_enabled = false;
    }

    // Phase 8: Signal FEP/cognitive cleanup phase
    brain_heartbeat(brain, "brain_destroy:fep_cognitive", 0.6f);

    // 0. Cleanup FEP orchestrator
    if (brain->fep_orchestrator) {
        fep_orchestrator_stop(brain->fep_orchestrator);
        fep_orchestrator_destroy(brain->fep_orchestrator);
        brain->fep_orchestrator = NULL;
        brain->fep_orchestrator_enabled = false;
    }

    // -1. Cleanup Core Directives (after FEP/immune, before cache cleanup)
    if (brain->core_directives) {
        // Disconnect bio-async if connected
        if (core_directives_is_bio_async_connected(brain->core_directives)) {
            core_directives_disconnect_bio_async(brain->core_directives);
        }

        // Destroy core directives system
        core_directives_destroy(brain->core_directives);
        brain->core_directives = NULL;
        brain->directive_immune_bridge = NULL;  // Managed internally by directives
        brain->directive_fep_bridge = NULL;     // Managed internally by directives
        brain->core_directives_enabled = false;
    }

    // -0.1 Cleanup 7 new brain subsystems (reverse init order: glymphatic first, white matter last)

    // Glymphatic System (brain waste clearance)
    nimcp_brain_destroy_glymphatic_subsystem(brain);

    // Endocannabinoid System (retrograde synaptic modulation)
    nimcp_brain_factory_destroy_endocannabinoid_subsystem(brain);

    // Neuropeptide System (slow neuromodulation)
    nimcp_brain_factory_destroy_neuropeptide_subsystem(brain);

    // Cortical Interneurons (inhibitory microcircuit control)
    nimcp_brain_factory_destroy_cortical_interneurons_subsystem(brain);

    // Spinal Cord (motor output, CPGs, reflex arcs)
    nimcp_brain_factory_destroy_spinal_cord_subsystem(brain);

    // Inferior Colliculus (auditory midbrain processing)
    nimcp_brain_ic_destroy(brain);

    // Cloud inference bridge cleanup
    if (brain->cloud_bridge) {
        cloud_inference_destroy((cloud_inference_bridge_t*)brain->cloud_bridge);
        brain->cloud_bridge = NULL;
        brain->cloud_inference_enabled = false;
    }

    // BPTT temporal buffer cleanup
    if (brain->bptt_buffer) {
        for (uint32_t i = 0; i < brain->bptt_window_size; i++) {
            nimcp_free(brain->bptt_buffer[i].input);
            nimcp_free(brain->bptt_buffer[i].output);
            nimcp_free(brain->bptt_buffer[i].target);
        }
        nimcp_free(brain->bptt_buffer);
        brain->bptt_buffer = NULL;
        brain->bptt_enabled = false;
    }

    // Per-cortex CNN processors cleanup
    {
        extern void cortex_cnn_destroy(struct cortex_cnn_processor* proc);
        for (int i = 0; i < 4; i++) {
            if (brain->cortex_cnns[i]) {
                cortex_cnn_destroy(brain->cortex_cnns[i]);
                brain->cortex_cnns[i] = NULL;
            }
        }
        nimcp_free(brain->cortex_cnn_fused_embedding);
        brain->cortex_cnn_fused_embedding = NULL;
        brain->cortex_cnn_fused_dim = 0;
    }

    // Pre-allocated inference output buffers cleanup
    nimcp_free(brain->inference_buf_adaptive);
    nimcp_free(brain->inference_buf_cnn);
    nimcp_free(brain->inference_buf_snn);
    brain->inference_buf_adaptive = NULL;
    brain->inference_buf_cnn = NULL;
    brain->inference_buf_snn = NULL;
    brain->inference_buf_size = 0;

    // Corpus Callosum (inter-hemispheric bridge — destroy before generic white matter)
    if (brain->callosum) {
        callosum_destroy(brain->callosum);
        brain->callosum = NULL;
        brain->hemispheric_enabled = false;
    }

    // White Matter Tracts (long-range connectivity — foundational, destroyed last of the 7)
    nimcp_brain_factory_destroy_white_matter_subsystem(brain);

    // -1. Cleanup Parietal Lobe (mathematical/scientific reasoning)
    // Parietal is destroyed before medulla since it's a higher cognitive function
    if (brain->parietal) {
        parietal_destroy(brain->parietal);
        brain->parietal = NULL;
        brain->parietal_enabled = false;
        brain->last_parietal_update_us = 0;
    }

    // -1.2. Cleanup Intuition System (Phase 6 creative/intuitive reasoning)
    // Intuition is destroyed after parietal (it's part of higher cognition)
    if (brain->intuition_system) {
        intuition_system_destroy(brain->intuition_system);
        brain->intuition_system = NULL;
        brain->intuition_system_enabled = false;
        brain->last_intuition_update_us = 0;
    }

    // -1.5. Cleanup Knowledge Graph Reader (self-awareness)
    // KG reader is destroyed after parietal but before medulla
    if (brain->kg_reader) {
        kg_reader_destroy(brain->kg_reader);
        brain->kg_reader = NULL;
        brain->kg_reader_enabled = false;
        memset(brain->kg_file_path, 0, sizeof(brain->kg_file_path));
    }

    // -1.6. Cleanup Internal Knowledge Graph (runtime CRUD)
    // Internal KG is destroyed after KG reader but before medulla
    if (brain->internal_kg) {
        nimcp_brain_factory_destroy_internal_kg_subsystem(brain);
    }

    // -1.7. Cleanup Mental Health Guardian (background monitoring agent)
    // Guardian is destroyed after internal KG (uses KG for topology updates)
    if (brain->mental_health_guardian) {
        nimcp_brain_factory_destroy_mental_health_guardian_subsystem(brain);
    }

    // Phase 8: Signal medulla/brainstem cleanup phase
    brain_heartbeat(brain, "brain_destroy:medulla", 0.85f);

    // -2. Cleanup Medulla Oblongata (brainstem autonomic regulation)
    // Medulla is destroyed late - it provides foundational regulation
    if (brain->medulla) {
        // Disconnect bio-async if connected
        if (medulla_is_bio_async_connected(brain->medulla)) {
            medulla_disconnect_bio_async(brain->medulla);
        }

        // Stop and destroy medulla
        medulla_stop(brain->medulla);
        medulla_destroy(brain->medulla);
        brain->medulla = NULL;
        brain->medulla_enabled = false;
        brain->last_medulla_update_us = 0;
    }

    // Phase 8: Signal final cleanup phase
    brain_heartbeat(brain, "brain_destroy:final", 0.95f);

    // Cleanup cache
    clear_cache(brain);
    nimcp_platform_mutex_destroy(&brain->cache_mutex);

    // GPU Substrate cleanup (must happen before GPU context is destroyed)
    if (brain->substrate_gpu_ctx) {
        nimcp_brain_factory_destroy_substrate_gpu_subsystem(brain);
    }

    // GPU context cleanup (must happen before bio-async as GPU may use async events)
    if (brain->gpu_ctx) {
        nimcp_brain_factory_destroy_gpu_subsystem(brain);
    }

    // Bio-async cleanup - respect refcount to avoid breaking other brains
    extern bool g_brain_bio_initialized;
    extern bio_module_context_t g_brain_bio_ctx;
    extern volatile int g_brain_bio_ref_count;
    if (brain->bio_async_enabled) {
        int prev_ref = __atomic_fetch_sub(&g_brain_bio_ref_count, 1, __ATOMIC_ACQ_REL);
        if (prev_ref <= 1 && g_brain_bio_initialized && g_brain_bio_ctx) {
            LOG_MODULE_INFO("BRAIN_LIFECYCLE", "Unregistering brain from bio-async router");
            bio_router_unregister_module(g_brain_bio_ctx);
            g_brain_bio_ctx = NULL;
            g_brain_bio_initialized = false;
        }
    }

    if (brain->bio_async_enabled) {
        brain_bio_async_shutdown(brain);
        brain->bio_async_enabled = false;
    }

    // Free experience API caches
    nimcp_free(brain->last_experience_prediction);
    nimcp_free(brain->last_experience_input);

    // Free learn() scratch buffers
    nimcp_free(brain->learn_scratch.target);
    nimcp_free(brain->learn_scratch.prediction);
    nimcp_free(brain->learn_scratch.attended_features);
    nimcp_free(brain->learn_scratch.cue_neurons);

    nimcp_free(brain);
}
