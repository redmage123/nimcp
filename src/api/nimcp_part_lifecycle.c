// nimcp_part_lifecycle.c - lifecycle functions
// Part of nimcp.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp.c


//=============================================================================
// Initialization
//=============================================================================

/**
 * @brief Internal initialization function
 *
 * NOTE: Previously used nimcp_platform_once() which prevented re-initialization
 * after shutdown. Now uses atomic compare-exchange for thread-safe init that
 * can be repeated after shutdown.
 *
 * @return NIMCP_OK on success, NIMCP_ERROR on failure
 */
static nimcp_status_t nimcp_init_internal(void) {
    // Initialize logging first so all subsequent LOG_* calls work
    // nimcp_log_init guards against double-init internally
    nimcp_log_init(NULL);

    LOG_INFO("Initializing NIMCP library version %s", NIMCP_VERSION_STRING);

    // ------------------------------------------------------------------------
    // SIGPIPE hardening: ignore broken-pipe signals so that a closed socket
    // peer never silent-kills the daemon. Without this, any write()/send()
    // on a socket whose reader has hung up delivers SIGPIPE, whose default
    // action is *terminate with no core* — producing the "silent daemon
    // death, no stack, no SIGSEGV" pattern observed in the 2026-04-23 crash.
    //
    // EWPIPE is still returned by the affected syscall so callers can handle
    // it explicitly. SIG_IGN is the correct default for any library that
    // might touch a Unix-socket or TCP endpoint.
    // ------------------------------------------------------------------------
    {
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = SIG_IGN;
        sigemptyset(&sa.sa_mask);
        sigaction(SIGPIPE, &sa, NULL);
    }

    // Install crash handler BEFORE any other subsystem so if anything
    // below SEGVs during init, we get a backtrace. The handler writes
    // to /var/log/nimcp_crash.log (or /tmp fallback). Calls a signal-
    // safe backtrace_symbols_fd() on any SIGSEGV/SIGABRT/SIGBUS/SIGFPE/
    // SIGILL, then exits. Overrides Python's faulthandler with richer
    // C-frame dump.
    {
        extern bool nimcp_install_crash_handler(void);
        (void)nimcp_install_crash_handler();
    }

    // Initialize memory tracking (unified memory management)
    LOG_DEBUG("Initializing memory tracking system");
    nimcp_memory_init();

    // Initialize bio-async system (core async communication infrastructure)
    LOG_INFO("Initializing bio-async communication system");
    nimcp_bio_async_config_t bio_async_config = {0};  // Use default config
    if (nimcp_bio_async_init(&bio_async_config) != NIMCP_SUCCESS) {
        LOG_ERROR("Failed to initialize bio-async system");
        nimcp_memory_cleanup();
        set_error("Failed to initialize bio-async system");
        return NIMCP_ERROR;
    }

    // Initialize bio-async router (message routing for modules)
    LOG_DEBUG("Initializing bio-async router");
    if (bio_router_init(NULL) != NIMCP_SUCCESS) {
        LOG_ERROR("Failed to initialize bio-async router");
        nimcp_bio_async_shutdown();
        nimcp_memory_cleanup();
        set_error("Failed to initialize bio-async router");
        return NIMCP_ERROR;
    }

    // Initialize COW cache system
    LOG_DEBUG("Initializing COW cache system");
    nimcp_cache_init();

    // Initialize tamper-resistant audit logging (always-on safety subsystem)
    // This is NOT gated by any config flag — audit logging is mandatory.
    LOG_DEBUG("Initializing tamper-resistant audit log");
    nimcp_safety_audit_init("/var/log/nimcp");

    set_error("No error");
    LOG_INFO("NIMCP library initialized successfully");
    return NIMCP_OK;
}


nimcp_status_t nimcp_init(void) {
    // Fast path: already initialized
    if (nimcp_atomic_load_bool(&g_initialized, NIMCP_MEMORY_ORDER_ACQUIRE)) {
        return g_init_result;
    }

    // Try to acquire init lock using compare-exchange
    // Only one thread will succeed; others will spin-wait
    bool expected = false;
    if (!nimcp_atomic_compare_exchange_bool(&g_init_in_progress, &expected, true,
                                             NIMCP_MEMORY_ORDER_ACQ_REL)) {
        // Another thread is initializing - wait for it to complete
        while (nimcp_atomic_load_bool(&g_init_in_progress, NIMCP_MEMORY_ORDER_ACQUIRE)) {
            usleep(1);  // Yield CPU to avoid busy-spin burning cycles
        }
        return g_init_result;
    }

    // Double-check after acquiring lock (another thread may have initialized)
    if (nimcp_atomic_load_bool(&g_initialized, NIMCP_MEMORY_ORDER_ACQUIRE)) {
        nimcp_atomic_store_bool(&g_init_in_progress, false, NIMCP_MEMORY_ORDER_RELEASE);
        return g_init_result;
    }

    // Perform actual initialization
    g_init_result = nimcp_init_internal();

    if (g_init_result == NIMCP_OK) {
        nimcp_atomic_store_bool(&g_initialized, true, NIMCP_MEMORY_ORDER_RELEASE);
    }

    // Release init lock
    nimcp_atomic_store_bool(&g_init_in_progress, false, NIMCP_MEMORY_ORDER_RELEASE);

    return g_init_result;
}

void nimcp_shutdown(void) {
    LOG_INFO("Shutting down NIMCP library");

    // KNOWN LIMITATION: There is a race window between setting g_initialized=false
    // and completing subsystem teardown. In-flight API calls that already passed the
    // g_initialized check may access freed resources during shutdown. A proper fix
    // would require epoch-based reclamation (RCU) or a global quiescent-state check,
    // which is deferred due to complexity. Callers must ensure no concurrent API
    // calls are in progress when calling nimcp_shutdown().

    // Atomically set g_initialized to false at the START of shutdown.
    // This prevents other threads from entering API functions during cleanup.
    bool expected = true;
    if (!nimcp_atomic_compare_exchange_bool(&g_initialized, &expected, false,
                                             NIMCP_MEMORY_ORDER_ACQ_REL)) {
        LOG_DEBUG("NIMCP not initialized, nothing to shutdown");
        return;
    }

    // Cleanup cache system
    LOG_DEBUG("Cleaning up COW cache system");
    nimcp_cache_cleanup();

    // P1-8 FIX: Reset brain probe module context before shutting down bio-router
    nimcp_api_reset_brain_probe_ctx();

    // Shutdown bio-async router
    LOG_DEBUG("Shutting down bio-async router");
    bio_router_shutdown();

    // Shutdown bio-async system
    LOG_DEBUG("Shutting down bio-async communication system");
    nimcp_bio_async_shutdown();

    // Shutdown constant-time module (before memory cleanup to avoid double-free)
    LOG_DEBUG("Shutting down constant-time module");
    nimcp_ct_shutdown();

    // Cleanup adaptive module memory pool (before memory cleanup)
    LOG_DEBUG("Cleaning up adaptive network memory pool");
    adaptive_pool_cleanup();

    // Shutdown exception system (before memory cleanup to avoid dangling references)
    LOG_DEBUG("Shutting down exception system");
    bbb_helpers_shutdown();
    nimcp_exception_system_shutdown();

    // Cleanup memory tracking (last)
    LOG_DEBUG("Cleaning up memory tracking");
    nimcp_memory_cleanup();

    // Reset init state to allow re-initialization
    // KNOWN LIMITATION: Between setting g_initialized=false (top of shutdown) and
    // resetting g_init_result here, a concurrent nimcp_init() that passes the
    // fast-path check could read the old g_init_result. This is benign because
    // g_initialized is already false, so the fast path won't be taken. However,
    // if re-initialization races with the tail end of shutdown, the spinning
    // thread may read a stale g_init_result before this reset.
    g_init_result = NIMCP_OK;  // Reset to default for next init
    // g_initialized already set to false via CAS at top of shutdown
    LOG_INFO("NIMCP library shutdown complete");
}


//=============================================================================
// Brain API Implementation
//=============================================================================

nimcp_brain_t nimcp_brain_create(
    const char* name,
    nimcp_brain_size_t size,
    nimcp_brain_task_t task,
    uint32_t num_inputs,
    uint32_t num_outputs)
{
    LOG_INFO("Creating brain: name='%s', size=%d, task=%d, inputs=%u, outputs=%u",
             name ? name : "NULL", size, task, num_inputs, num_outputs);

    /* Use exception-integrated validation (returns NULL on error) */
    NIMCP_API_CHECK_NULL_RET_NULL(name, "Brain name cannot be NULL");

    /* Allocate handle with calloc (zero-init last_loss/last_gradient_norm) */
    LOG_DEBUG("Allocating brain handle (%zu bytes)", sizeof(struct nimcp_brain_handle));
    nimcp_brain_t handle = (nimcp_brain_t)nimcp_calloc(1, sizeof(struct nimcp_brain_handle));
    NIMCP_API_CHECK_ALLOC_SIZE(handle, sizeof(struct nimcp_brain_handle),
                               "Failed to allocate brain handle");

    // Map public enums to internal enums
    brain_size_t internal_size = (brain_size_t)size;
    brain_task_t internal_task = (brain_task_t)task;
    LOG_DEBUG("Mapped enums: internal_size=%d, internal_task=%d", internal_size, internal_task);

    // Create internal brain
    LOG_DEBUG("Creating internal brain structure");
    handle->internal_brain = brain_create(name, internal_size, internal_task,
                                          num_inputs, num_outputs);

    if (!handle->internal_brain) {
        LOG_ERROR("Failed to create internal brain for '%s'", name);
        set_error("Failed to create internal brain");
        nimcp_free(handle);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_brain_create: handle->internal_brain is NULL");
        return NULL;
    }

    set_error("No error");
    LOG_INFO("Brain '%s' created successfully (handle=%p)", name, (void*)handle);
    return handle;
}

nimcp_status_t nimcp_brain_freeze(nimcp_brain_t brain) {
    NIMCP_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "nimcp_brain_freeze: brain is NULL");
    NIMCP_CHECK_THROW(brain->internal_brain, NIMCP_ERROR_NULL_ARG, "nimcp_brain_freeze: internal_brain is NULL");
    bool ok = brain_freeze(brain->internal_brain);
    return ok ? NIMCP_OK : NIMCP_ERROR;
}

bool nimcp_brain_is_frozen(nimcp_brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_is_frozen: brain is NULL");
        return false;
    }
    if (!brain->internal_brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_is_frozen: internal_brain is NULL");
        return false;
    }
    return brain_is_frozen(brain->internal_brain);
}

void nimcp_brain_destroy(nimcp_brain_t brain) {
    if (!brain) {
        set_error("nimcp_brain_destroy: NULL brain handle");
        LOG_DEBUG("nimcp_brain_destroy called with NULL brain, ignoring");
        return;
    }

    LOG_INFO("Destroying brain (handle=%p)", (void*)brain);

    // Clean up training pipeline state BEFORE destroying internal brain
    // This prevents memory leaks from the global g_training_states array
    nimcp_api_training_cleanup_brain(brain);

    if (brain->internal_brain) {
        LOG_DEBUG("Destroying internal brain structure");
        brain_destroy(brain->internal_brain);
    } else {
        LOG_WARN("Brain handle has NULL internal_brain");
    }

    LOG_DEBUG("Freeing brain handle");
    nimcp_free(brain);
    LOG_DEBUG("Brain destroyed successfully");
}


nimcp_brain_t nimcp_brain_create_with_neurons(
    const char* name,
    nimcp_brain_task_t task,
    uint32_t num_inputs,
    uint32_t num_outputs,
    uint32_t neuron_count)
{
    LOG_INFO("Creating brain with %u neurons: name='%s', task=%d, inputs=%u, outputs=%u",
             neuron_count, name ? name : "NULL", task, num_inputs, num_outputs);

    NIMCP_API_CHECK_NULL_RET_NULL(name, "Brain name cannot be NULL");

    nimcp_brain_t handle = NULL;
    task_strategy_t* strategy = NULL;

    handle = (nimcp_brain_t)nimcp_calloc(1, sizeof(struct nimcp_brain_handle));
    if (!handle) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_brain_create_with_neurons: handle allocation failed");
        goto cleanup;
    }

    brain_size_t internal_size = BRAIN_SIZE_LARGE;
    brain_task_t internal_task = (brain_task_t)task;

    // Build config with neuron_count override
    brain_config_t config = {0};

    /* Enable lazy init for large brains BEFORE init_brain_config so sub-flags propagate.
     * Skips O(N) axon/dendrite init loops — created on-demand if needed. */
    if (neuron_count > 100000) {
        config.lazy_init_mode = true;
    }

    strategy = strategy_create(internal_task);
    if (!strategy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_brain_create_with_neurons: strategy allocation failed");
        goto cleanup;
    }
    nimcp_brain_factory_init_brain_config(&config, name, internal_size, internal_task,
                                          num_inputs, num_outputs, strategy);
    strategy_destroy(strategy);
    strategy = NULL;
    config.neuron_count = neuron_count;

    handle->internal_brain = brain_create_custom(&config);
    if (!handle->internal_brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_brain_create_with_neurons: brain_create_custom returned NULL");
        goto cleanup;
    }

    set_error("No error");
    LOG_INFO("Brain '%s' created with %u neurons (handle=%p)", name, neuron_count, (void*)handle);
    return handle;

cleanup:
    set_error("Failed to create brain with custom neuron count");
    strategy_destroy(strategy);
    nimcp_free(handle);
    return NULL;
}

/**
 * Create a brain with ALL subsystems enabled (RESEARCH profile + extras).
 * This is the "everything on" mode for maximum capability.
 */
nimcp_brain_t nimcp_brain_create_full(
    const char* name,
    nimcp_brain_task_t task,
    uint32_t num_inputs,
    uint32_t num_outputs,
    uint32_t neuron_count)
{
    LOG_INFO("Creating FULL brain with %u neurons: name='%s', task=%d, inputs=%u, outputs=%u",
             neuron_count, name ? name : "NULL", task, num_inputs, num_outputs);

    NIMCP_API_CHECK_NULL_RET_NULL(name, "Brain name cannot be NULL");

    nimcp_brain_t handle = (nimcp_brain_t)nimcp_calloc(1, sizeof(struct nimcp_brain_handle));
    if (!handle) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_brain_create_full: handle allocation failed");
        return NULL;
    }

    /* Start from RESEARCH profile (includes cognitive + standard) */
    brain_config_t config = brain_config_from_profile(BRAIN_CONFIG_RESEARCH);

    /* Override dimensions and task */
    config.neuron_count = neuron_count;
    config.num_inputs = num_inputs;
    config.num_outputs = num_outputs;
    config.task = (brain_task_t)task;
    config.size = BRAIN_SIZE_LARGE;

    /* Enable subsystems not in the research profile */
    config.enable_world_model = true;
    config.enable_omni_world_model = true;
    config.enable_multimodal_world_model = true;
    config.omni_wm_enable_dreaming = true;
    config.mm_wm_enable_bio_async = true;
    config.enable_wm_security_immune_bridge = true;
    config.enable_wm_logging_bridge = true;
    config.enable_wm_cognitive_bridge = true;
    config.enable_wm_parietal_bridge = true;
    config.enable_wm_hypothalamus_bridge = true;
    config.enable_wm_thalamic_bridge = true;
    config.enable_wm_substrate_bridge = true;
    config.enable_wm_memory_bridge = true;
    config.enable_wm_kg_bridge = true;
    config.enable_wm_tom_bridge = true;
    config.enable_wm_plasticity_bridge = true;

    /* Creative system */
    config.enable_creative_system = true;
    config.enable_creative_text = true;
    config.enable_creative_music = true;
    config.enable_creative_visual = true;
    config.enable_creative_video = true;
    config.enable_creative_appreciation = true;
    config.enable_creative_ethics = true;

    /* LGSS safety */
    config.enable_lgss = true;
    config.enable_lgss_telemetry = true;
    config.lgss_enable_bio_async = true;
    config.lgss_enable_ethics_bridge = true;
    config.lgss_enable_plasticity_bridge = true;
    config.lgss_enable_output_gates = true;

    /* Neuromodulators */
    config.enable_lc = true;
    config.enable_vta = true;
    config.enable_raphe = true;
    config.enable_habenula = true;
    config.enable_neuromod_intra = true;
    config.neuromod_enable_security_bridge = true;
    config.neuromod_enable_immune_bridge = true;
    config.neuromod_enable_logging_bridge = true;
    config.neuromod_enable_training_bridge = true;

    /* Immune system bridges */
    config.immune_enable_bbb_integration = true;
    config.immune_enable_bft_integration = true;
    config.immune_enable_swarm_integration = true;
    config.immune_enable_bio_async = true;

    /* Collective cognition bridges */
    config.collective_enable_leader_detection = true;
    config.collective_enable_immune_integration = true;
    config.collective_enable_bio_async = true;

    /* Spike NLP */
    config.enable_spike_nlp = true;

    /* Health agent */
    config.enable_health_agent = true;

    /* Cycle coordinator */
    config.enable_cycle_coordinator = true;

    /* Auto intervention */
    config.enable_auto_intervention = true;

    /* Global workspace dimension must match num_inputs
     * WHY:  brain_part_core.c passes (features, num_features) to
     *       global_workspace_compete() — num_features == num_inputs.
     *       Default is 256, causing dimension mismatch THROW every step. */
    config.workspace_capacity_dim = num_inputs;

    /* Enable lazy init to defer cognitive subsystems until first access.
     * "Full" means all features *enabled*, not all eagerly allocated.
     * Without lazy init, 2M neurons + all subsystems = ~57 GB peak → OOM.
     * With lazy init, peak is ~29 GB (neural core only at creation). */
    config.lazy_init_mode = true;
    config.lazy_consolidation_init = true;
    config.lazy_global_workspace_init = true;

    /* Create the brain with full subsystem initialization */
    handle->internal_brain = brain_create_custom(&config);
    if (!handle->internal_brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_brain_create_full: brain_create_custom returned NULL");
        nimcp_free(handle);
        return NULL;
    }

    set_error("No error");
    LOG_INFO("FULL brain '%s' created with %u neurons, all subsystems enabled (handle=%p)",
             name, neuron_count, (void*)handle);
    return handle;
}

nimcp_brain_t nimcp_brain_create_fast(
    const char* name,
    nimcp_brain_task_t task,
    uint32_t num_inputs,
    uint32_t num_outputs,
    uint32_t neuron_count)
{
    LOG_INFO("Creating FAST brain with %u neurons: name='%s', task=%d, inputs=%u, outputs=%u",
             neuron_count, name ? name : "NULL", task, num_inputs, num_outputs);

    NIMCP_API_CHECK_NULL_RET_NULL(name, "Brain name cannot be NULL");

    nimcp_brain_t handle = (nimcp_brain_t)nimcp_calloc(1, sizeof(struct nimcp_brain_handle));
    if (!handle) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_brain_create_fast: handle allocation failed");
        return NULL;
    }

    brain_size_t internal_size = BRAIN_SIZE_LARGE;
    brain_task_t internal_task = (brain_task_t)task;

    brain_config_t config = {0};
    config.init_mode = BRAIN_INIT_FAST;  // Set before init_brain_config so lazy flags propagate

    task_strategy_t* strategy = strategy_create(internal_task);
    if (!strategy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_brain_create_fast: strategy allocation failed");
        nimcp_free(handle);
        return NULL;
    }

    nimcp_brain_factory_init_brain_config(&config, name, internal_size, internal_task,
                                          num_inputs, num_outputs, strategy);
    strategy_destroy(strategy);
    config.neuron_count = neuron_count;

    handle->internal_brain = brain_create_custom(&config);
    if (!handle->internal_brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_brain_create_fast: brain_create_custom returned NULL");
        nimcp_free(handle);
        return NULL;
    }

    set_error("No error");
    LOG_INFO("FAST brain '%s' created with %u neurons (handle=%p)", name, neuron_count, (void*)handle);
    return handle;
}

nimcp_brain_t nimcp_brain_create_from_config(const char* config_filepath) {
    if (!config_filepath) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_create_from_config: config_filepath is NULL");
        return NULL;
    }

    // Load configuration from YAML/JSON
    nimcp_brain_config_t config;
    if (!nimcp_config_load(config_filepath, &config)) {
        set_error("Failed to load config from %s", config_filepath);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_IO, "nimcp_brain_create_from_config: failed to load config file");
        return NULL;
    }

    // Create brain using loaded configuration
    nimcp_brain_t brain = nimcp_brain_create(
        config.name,
        (nimcp_brain_size_t)config.size,
        (nimcp_brain_task_t)config.task,
        config.num_inputs,
        config.num_outputs
    );

    if (!brain) {
        set_error("Failed to create brain from config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_brain_create_from_config: brain_create returned NULL");
        return NULL;
    }

    // Note: Additional configuration like BCM, STDP, ethics could be applied here
    // For now, we just create the basic brain structure
    // TODO: Apply plasticity settings, ethics config, etc.

    set_error("No error");
    return brain;
}

static void brain_probe_module_init_once(void) {
    if (!bio_router_is_initialized()) {
        return;
    }
    bio_module_info_t info = {
        .module_id = BIO_MODULE_BRAIN,
        .module_name = "brain_probe",
        .inbox_capacity = 64,
        .user_data = NULL
    };
    g_brain_probe_module_ctx = bio_router_register_module(&info);
}


/**
 * @brief Reset brain probe module state for re-initialization after shutdown.
 * Called from nimcp_shutdown() (forward-declared there).
 */
void nimcp_api_reset_brain_probe_ctx(void) {
    g_brain_probe_module_ctx = NULL;
    g_brain_probe_once = (nimcp_platform_once_t)NIMCP_PLATFORM_ONCE_INIT;
}


/**
 * WHAT: Destroy brain snapshot and release COW references
 * WHY:  Free snapshot resources and decrement reference counts
 * HOW:  Release cached references and free snapshot handle
 */
void nimcp_brain_snapshot_destroy(nimcp_brain_snapshot_t snapshot) {
    if (!snapshot) {
        return;
    }

    // Phase 1: Free the cloned internal brain
    if (snapshot->internal_brain_snapshot) {
        brain_destroy(snapshot->internal_brain_snapshot);
    }

    // Free snapshot handle
    nimcp_free(snapshot);
}


//=============================================================================
// Neural Network API Implementation
//=============================================================================

nimcp_network_t nimcp_network_create(
    uint32_t num_inputs,
    uint32_t num_outputs,
    uint32_t num_hidden,
    float learning_rate)
{
    // Allocate handle
    nimcp_network_t handle = (nimcp_network_t)nimcp_malloc(sizeof(struct nimcp_network_handle));
    if (!handle) {
        set_error("Failed to allocate network handle");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "handle is NULL");

        return NULL;
    }

    // Create config for internal API
    network_config_t config = {0};
    config.input_size = num_inputs;
    config.output_size = num_outputs;
    // Calculate total neurons: inputs + hidden layers + outputs
    config.num_neurons = num_inputs + num_hidden + num_outputs;
    config.learning_rate = learning_rate;

    // Create internal neural network
    handle->internal_network = neural_network_create(&config);

    if (!handle->internal_network) {
        set_error("Failed to create internal neural network");
        nimcp_free(handle);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_network_create: handle->internal_network is NULL");
        return NULL;
    }

    set_error("No error");
    return handle;
}


void nimcp_network_destroy(nimcp_network_t network) {
    if (!network) {
        return;
    }

    if (network->internal_network) {
        neural_network_destroy(network->internal_network);
    }

    nimcp_free(network);
}


//=============================================================================
// Ethics API Implementation
//=============================================================================

nimcp_ethics_t nimcp_ethics_create(void) {
    nimcp_ethics_t handle = (nimcp_ethics_t)nimcp_malloc(sizeof(struct nimcp_ethics_handle));
    if (!handle) {
        set_error("Failed to allocate ethics handle");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "handle is NULL");

        return NULL;
    }

    // Create default ethics configuration
    ethics_config_t config = {0};
    config.policies = NULL;
    config.num_policies = 0;
    config.callback = NULL;
    config.callback_context = NULL;
    config.default_severity = 0.5F;
    config.enable_learning = true;
    config.action_feature_size = 32;
    config.max_agents = 16;
    config.golden_rule_threshold = 0.0F;
    config.empathy_weight = 0.5F;

    // Create internal ethics engine
    handle->internal_ethics = ethics_engine_create(&config);

    if (!handle->internal_ethics) {
        set_error("Failed to create internal ethics engine");
        nimcp_free(handle);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_ethics_create: handle->internal_ethics is NULL");
        return NULL;
    }

    set_error("No error");
    return handle;
}


void nimcp_ethics_destroy(nimcp_ethics_t ethics) {
    if (!ethics) {
        return;
    }

    if (ethics->internal_ethics) {
        ethics_engine_destroy(ethics->internal_ethics);
    }

    nimcp_free(ethics);
}


//=============================================================================
// Knowledge API Implementation
//=============================================================================

nimcp_knowledge_t nimcp_knowledge_create(void) {
    nimcp_knowledge_t handle = (nimcp_knowledge_t)nimcp_malloc(sizeof(struct nimcp_knowledge_handle));
    if (!handle) {
        set_error("Failed to allocate knowledge handle");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "handle is NULL");

        return NULL;
    }

    // Create internal knowledge system
    handle->internal_knowledge = knowledge_system_create("default");

    if (!handle->internal_knowledge) {
        set_error("Failed to create internal knowledge system");
        nimcp_free(handle);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_knowledge_create: handle->internal_knowledge is NULL");
        return NULL;
    }

    set_error("No error");
    return handle;
}


void nimcp_knowledge_destroy(nimcp_knowledge_t knowledge) {
    if (!knowledge) {
        return;
    }

    if (knowledge->internal_knowledge) {
        knowledge_system_destroy(knowledge->internal_knowledge);
    }

    nimcp_free(knowledge);
}


/**
 * @brief Cleanup training state when brain is destroyed
 *
 * Called from nimcp_brain_destroy to prevent memory leaks from training state.
 */
void nimcp_api_training_cleanup_brain(nimcp_brain_t brain) {
    if (!brain) return;
    clear_training_state(brain);
}
