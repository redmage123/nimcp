// nimcp_brain_part_accessors.c - accessors functions
// Part of nimcp_brain.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_brain.c

adaptive_network_t brain_get_network(brain_t brain)
{
    if (!brain) {
        set_error("NULL brain passed to brain_get_network");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_get_network: brain is NULL");
        return NULL;
    }

    // Phase 2: CRITICAL - Ensure network is writable before exposing it
    // WHY: External subsystems (introspection, salience, consolidation) may mutate the network
    // RISK: Exposing shared network allows corruption from external modifications
    if (!ensure_writable_network(brain)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_get_network: ensure_writable_network is NULL");
        return NULL;  // Error already set
    }

    return brain->network;
}


/**
 * WHAT: Get neuromodulator system from brain
 * WHY:  Mental health monitoring needs to read/write neurotransmitter levels
 * HOW:  Returns opaque handle, no COW concerns (neuromodulator state is per-brain)
 *
 * SAFETY: Neuromodulator system is not shared (unlike network), so no COW needed
 */
neuromodulator_system_t brain_get_neuromodulator_system(brain_t brain)
{
    if (!brain) {
        set_error("NULL brain passed to brain_get_neuromodulator_system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_get_neuromodulator_system: brain is NULL");
        return NULL;
    }

    return brain->neuromodulator_system;
}


/**
 * @brief Build base network configuration
 *
 * WHY: Isolates network config from brain config
 * Enables reuse and testing of network setup
 *
 * COMPLEXITY: O(1) + memory allocation
 *
 * @param num_inputs Input dimension
 * @param num_outputs Output dimension
 * @param num_neurons Total neuron count
 * @return Base network config (caller must free layer_sizes)
 */
static network_config_t build_base_network_config(uint32_t num_inputs, uint32_t num_outputs,
                                                  uint32_t num_neurons,
                                                  ode_integration_method_t integration_method)
{
    network_config_t config = {0};
    config.input_size = num_inputs;
    config.output_size = num_outputs;
    config.num_neurons = num_neurons;
    config.num_layers = 3;
    config.integration_method = integration_method;  // Part A1.1: Pass through RK4 config

    config.layer_sizes = nimcp_calloc(3, sizeof(uint32_t));
    // Guard: Check allocation
    // WHY: If allocation fails, returning config with NULL layer_sizes will crash
    if (!config.layer_sizes) {
        set_error("Failed to allocate layer_sizes array");
        return config;  // Return with layer_sizes = NULL to signal error
    }

    config.layer_sizes[0] = num_inputs;
    config.layer_sizes[1] = num_neurons;
    config.layer_sizes[2] = num_outputs;

    config.enable_stdp = true;
    config.enable_hebbian = true;
    config.enable_oja = true;
    config.enable_homeostasis = true;

    // SCALABILITY: Disable BCM and eligibility traces by default
    // WHY: These require per-synapse heap allocation
    // IMPACT: With 1M neurons × 256 synapses = 256M allocations
    // SOLUTION: Only enable when explicitly configured by brain_config
    config.enable_bcm = false;          // Conditional BCM allocation
    config.enable_eligibility = false;  // Conditional eligibility allocation

    return config;
}


/**
 * @brief Build complete adaptive network configuration
 *
 * WHY: Combines base config and spike params
 * Single point of network configuration assembly
 *
 * COMPLEXITY: O(1) + memory allocation
 *
 * @param num_inputs Input dimension
 * @param num_outputs Output dimension
 * @param num_neurons Neuron count
 * @param sparsity_target Target sparsity
 * @return Complete adaptive network config
 */
static adaptive_network_config_t build_network_config(uint32_t num_inputs, uint32_t num_outputs,
                                                      uint32_t num_neurons, float sparsity_target,
                                                      ode_integration_method_t integration_method)
{
    adaptive_network_config_t config = {0};

    config.base_config = build_base_network_config(num_inputs, num_outputs, num_neurons, integration_method);

    config.spike_params = build_spike_params(sparsity_target);

    config.enable_sparsity = false;  // Disabled for regression tests - untrained networks produce zeros
    config.pruning_threshold = 0.01F;
    config.update_frequency = 100;

    return config;
}


/**
 * @brief Initialize brain configuration
 *
 * WHY: Centralizes config initialization with strategy
 * Ensures consistent config setup
 *
 * COMPLEXITY: O(1)
 *
 * @param config Output config structure
 * @param task_name Name for brain
 * @param size Size preset
 * @param task Task type
 * @param num_inputs Input dimension
 * @param num_outputs Output dimension
 * @param strategy Task strategy for learning rate
 */
static void init_brain_config(brain_config_t* config, const char* task_name, brain_size_t size,
                              brain_task_t task, uint32_t num_inputs, uint32_t num_outputs,
                              task_strategy_t* strategy)
{
    config->size = size;
    config->task = task;
    config->num_inputs = num_inputs;
    config->num_outputs = num_outputs;
    config->learning_rate = (strategy && strategy->get_learning_rate)
                            ? strategy->get_learning_rate() : 0.01f;
    config->sparsity_target = get_default_sparsity(size);
    config->enable_explanations = true;
    strncpy(config->task_name, task_name, sizeof(config->task_name) - 1);

    // Part A: Differential Equations - ODE Integration Method (A1.x)
    config->neuron_integration = ODE_EULER;  // Default: Fast Euler (backward compatible)

    // Phase 10.2: Working Memory defaults (Miller's 7±2)
    config->enable_working_memory = true;           // Enable by default
    config->working_memory_capacity = 7;            // Miller's magic number
    config->working_memory_decay_tau_ms = 1000.0F;  // 1 second decay

    // Phase 10.6: Theory of Mind defaults (social cognition, empathy)
    config->enable_theory_of_mind = true;           // Enable by default for social cognition
    config->enable_empathy_responses = true;        // Enable empathetic responses
    config->enable_false_belief_tracking = true;    // Enable false belief understanding

    // Phase 10.11: Mirror Neurons defaults (observation-based learning)
    config->enable_mirror_neurons = true;           // Enable by default for social learning
    config->mirror_neuron_count = 1000;             // Standard population size
    config->mirror_max_actions = 100;               // Diverse action repertoire
    config->mirror_max_agents = 10;                 // Multi-agent social learning
    config->mirror_learning_rate = 0.01F;           // Hebbian association rate
    config->mirror_match_threshold = 0.7F;          // Action recognition threshold

    // Global Workspace Architecture defaults (Global Workspace Theory - Baars 1988)
    config->enable_global_workspace = true;         // Enable by default for conscious access
    config->workspace_capacity_dim = 256;           // Content dimension (256 floats)
    config->workspace_ignition_threshold = 0.6F;    // Threshold for conscious access
    config->workspace_refractory_ms = 50;           // 50ms refractory between broadcasts
    config->workspace_enable_history = true;        // Enable history tracking
    config->workspace_history_depth = 10;           // Last 10 broadcasts

    // Phase 11 Enhancement C1.1: Quantum Annealing defaults
    config->enable_quantum_annealing = false;       // Disable by default (opt-in for optimization)
    config->annealing_temperature_init = 10.0F;     // Initial exploration temperature
    config->annealing_temperature_final = 0.1F;     // Final exploitation temperature
    config->annealing_steps = 1000;                 // Number of optimization steps
    config->quantum_annealing_frequency = 100;      // Run every 100 learning steps

    // Phase 12: Personality and Identity defaults
    config->use_random_personality = true;          // Default: generate random personality
    config->personality_seed = 0;                   // Time-based seed for uniqueness
    config->explicit_openness = 0.5F;               // Moderate openness (if explicit)
    config->explicit_conscientiousness = 0.5F;      // Moderate conscientiousness (if explicit)
    config->explicit_extraversion = 0.5F;           // Moderate extraversion (if explicit)
    config->explicit_agreeableness = 0.5F;          // Moderate agreeableness (if explicit)
    config->explicit_neuroticism = 0.5F;            // Moderate neuroticism (if explicit)
    config->explicit_gender = GENDER_FEMALE;        // Default: female (per user request)
    config->explicit_sexuality = SEXUALITY_HETEROSEXUAL; // Default: heterosexual
    config->personality_trait_mean = 0.5F;          // Mean for random trait generation
    config->personality_trait_stddev = 0.15F;       // Stddev for random trait generation
    config->female_probability = 1.0F;              // Default 100% female (per user request)
    config->male_probability = 0.0F;                // 0% male by default
    config->non_binary_probability = 0.0F;          // 0% non-binary by default

    // Fuzzy logic and Internal KG defaults
    config->enable_fuzzy_logic = true;              // Enable fuzzy logic (graded neural encoding)
    config->enable_internal_kg = true;              // Enable internal runtime knowledge graph

    // Phase 5/6: Biological Realism defaults
    config->enable_glial = true;                    // Enable glial integration by default
    config->enable_oscillations = false;            // Disable oscillations by default (opt-in)

    // Phase C2.1: Quantum Walk defaults (disabled by default for stability/testing)
    config->enable_quantum_walk_diffusion = false;  // Opt-in: requires testing for production
    config->quantum_walk_steps = 50;                // Moderate steps for √N speedup
    config->quantum_classical_mixing = 0.2F;        // 80% quantum + 20% classical (hybrid)
    config->quantum_coin_type = 0;                  // 0=Hadamard (balanced superposition)
    config->quantum_decoherence_rate = 0.05F;       // Minimal decoherence (5% per step)
}


/**
 * @brief Get working memory from brain (Phase 10.2 accessor)
 *
 * WHAT: Retrieve pointer to brain's working memory subsystem
 * WHY:  Allow API wrapper functions to access working memory
 * HOW:  Return brain->working_memory field (NULL if not enabled)
 *
 * @param brain Brain instance
 * @return Working memory pointer or NULL if not enabled/invalid brain
 */
working_memory_t* brain_get_working_memory(brain_t brain)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain is NULL");

        return NULL;
    }

    // Lazy initialization: Initialize on first access if deferred
    if (!brain->working_memory && brain->config.lazy_working_memory_init) {
        nimcp_platform_mutex_lock(&brain->cache_mutex);
        // Double-check after acquiring lock (another thread may have initialized)
        if (!brain->working_memory) {
            // Call the init function (declared in nimcp_brain_init.h)
            extern bool nimcp_brain_factory_init_working_memory_subsystem(brain_t brain);
            nimcp_brain_factory_init_working_memory_subsystem(brain);
        }
        nimcp_platform_mutex_unlock(&brain->cache_mutex);
    }

    return brain->working_memory;
}


/**
 * @brief Get executive controller from brain
 *
 * WHAT: Retrieve pointer to brain's executive control subsystem
 * WHY:  Allow cognitive modules to access executive function stats
 * HOW:  Return brain->executive field (NULL if not enabled)
 *
 * BIOLOGICAL BASIS: Prefrontal cortex executive functions (Miller & Cohen, 2001)
 *
 * @param brain Brain instance
 * @return Executive controller pointer or NULL if not enabled/invalid brain
 */
executive_controller_t* brain_get_executive(brain_t brain)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain is NULL");

        return NULL;
    }
    return brain->executive;
}


/**
 * @brief Get global workspace from brain
 *
 * WHAT: Retrieve pointer to brain's global workspace subsystem
 * WHY:  Allow cognitive modules to access workspace for competition and broadcasting
 * HOW:  Return brain->global_workspace field (NULL if not enabled)
 *
 * BIOLOGICAL BASIS: Global Workspace Theory (Baars, 1988; Dehaene, 2011)
 *
 * @param brain Brain instance
 * @return Global workspace pointer or NULL if not enabled/invalid brain
 */
global_workspace_t* brain_get_global_workspace(brain_t brain)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain is NULL");

        return NULL;
    }

    // Lazy initialization: Initialize on first access if deferred
    if (!brain->global_workspace && brain->config.lazy_global_workspace_init) {
        nimcp_platform_mutex_lock(&brain->cache_mutex);
        // Double-check after acquiring lock (another thread may have initialized)
        if (!brain->global_workspace) {
            extern bool nimcp_brain_factory_init_global_workspace_subsystem(brain_t brain);
            nimcp_brain_factory_init_global_workspace_subsystem(brain);
        }
        nimcp_platform_mutex_unlock(&brain->cache_mutex);
    }

    return brain->global_workspace;
}


/**
 * @brief Get sleep system from brain (Phase 10.1 accessor)
 *
 * WHAT: Retrieve pointer to brain's sleep/wake subsystem
 * WHY:  Allow external control of sleep cycles and pressure monitoring
 * HOW:  Return brain->sleep_system field (NULL if not enabled)
 *
 * @param brain Brain instance
 * @return Sleep system pointer or NULL if invalid brain
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (read-only)
 */
sleep_system_t brain_get_sleep_system(brain_t brain)
{
    /* Guard clause: Validate input */
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain is NULL");

        return NULL;
    }

    return brain->sleep_system;
}


/**
 * @brief Get Theory of Mind from brain (Phase 10.6 accessor)
 *
 * WHAT: Retrieve pointer to brain's Theory of Mind subsystem
 * WHY:  Allow external access to social cognition and empathy functions
 * HOW:  Return brain->theory_of_mind field (NULL if not enabled)
 *
 * @param brain Brain instance
 * @return Theory of Mind pointer or NULL if not enabled/invalid brain
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (read-only)
 */
theory_of_mind_t brain_get_theory_of_mind(brain_t brain)
{
    /* Guard clause: Validate input */
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain is NULL");

        return NULL;
    }

    // Lazy initialization: Initialize on first access if deferred
    if (!brain->theory_of_mind && brain->config.lazy_theory_of_mind_init) {
        nimcp_platform_mutex_lock(&brain->cache_mutex);
        // Double-check after acquiring lock (another thread may have initialized)
        if (!brain->theory_of_mind) {
            extern bool nimcp_brain_factory_init_theory_of_mind_subsystem(brain_t brain);
            nimcp_brain_factory_init_theory_of_mind_subsystem(brain);
        }
        nimcp_platform_mutex_unlock(&brain->cache_mutex);
    }

    return brain->theory_of_mind;
}


/**
 * @brief Get explanation generator from brain (Phase 10.7 accessor)
 *
 * WHAT: Retrieve pointer to brain's Natural Explanations generator
 * WHY:  Allow external modules to generate explanations
 * HOW:  Return brain->explanation_gen field (NULL if not enabled)
 *
 * @param brain Brain instance
 * @return Explanation generator pointer or NULL if not enabled/invalid brain
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (read-only)
 */
explanation_generator_t brain_get_explanation_generator(brain_t brain)
{
    /* Guard clause: Validate input */
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain is NULL");

        return NULL;
    }

    return brain->explanation_gen;
}


// brain_save_snapshot() - MOVED TO: src/core/brain/persistence/nimcp_brain_persistence.c

// brain_restore_snapshot() - MOVED TO: src/core/brain/persistence/nimcp_brain_persistence.c

// brain_list_snapshots() - MOVED TO: src/core/brain/persistence/nimcp_brain_persistence.c

// brain_delete_snapshot() - MOVED TO: src/core/brain/persistence/nimcp_brain_persistence.c

/**
 * @brief Get brain memory footprint
 *
 * WHY: Enables memory usage monitoring
 * Important for embedded and resource-constrained environments
 *
 * COMPLEXITY: O(n) where n = num_neurons
 *
 * @param brain Brain handle
 * @return Memory usage in bytes
 */
size_t brain_get_memory_usage(brain_t brain)
{
    if (!brain)
        return 0;

    size_t size = sizeof(struct brain_struct);
    size += adaptive_network_get_size(brain->network);

    for (uint32_t i = 0; i < brain->num_output_labels; i++) {
        size += strlen(brain->output_labels[i]) + 1;
    }

    return size;
}


struct nimcp_memory_store* brain_get_memory_store(brain_t brain)
{
    if (!brain) return NULL;
    return brain->memory_store;
}


struct nimcp_ood_detector* brain_get_ood_detector(brain_t brain)
{
    if (!brain) return NULL;
    return brain->ood_detector;
}
