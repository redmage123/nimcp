// nimcp_mirror_neurons_part_lifecycle.c - lifecycle functions
// Part of nimcp_mirror_neurons.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_mirror_neurons.c


/**
 * @brief Create action from features
 *
 * WHAT: Helper to construct action_t from components
 * WHY:  Simplify action creation in tests and applications
 * HOW:  Fill struct with provided data
 */
action_t mirror_neurons_create_action(
    uint32_t action_id,
    const char* action_name,
    const float* features,
    uint32_t num_features,
    uint32_t agent_id)
{
    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_heartbeat("mirror_neuro_create_action", 0.0f);


    action_t action = {0};
    action.action_id = action_id;
    action.agent_id = agent_id;
    action.num_features = (num_features > MIRROR_MAX_FEATURES) ? MIRROR_MAX_FEATURES : num_features;
    action.timestamp = nimcp_time_get_ms();
    action.confidence = 1.0F;

    if (action_name) {
        strncpy(action.action_name, action_name, sizeof(action.action_name) - 1);
    }

    if (features && num_features > 0) {
        memcpy(action.features, features, action.num_features * sizeof(float));
    }

    return action;
}


//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Find or create action mapping
 *
 * WHAT: Get index of action in actions array, create if not exists
 * WHY:  Maintain action-to-neuron mappings
 * HOW:  Linear search, then append if not found
 *
 * @return Action index, or UINT32_MAX on error
 */
static uint32_t find_or_create_action(mirror_neurons_t mirror, const action_t* action)
{
    if (!mirror || !action) {
        return UINT32_MAX;
    }

    // Search for existing action
    for (uint32_t i = 0; i < mirror->num_actions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mirror->num_actions > 256) {
            mirror_neurons_heartbeat("mirror_neuro_loop",
                             (float)(i + 1) / (float)mirror->num_actions);
        }

        if (mirror->actions[i].action_id == action->action_id) {
            return i;
        }
    }

    // Check capacity — recycle least-used action if full
    if (mirror->num_actions >= mirror->config.max_actions) {
        /* Find the action with the lowest usage (observations + executions) */
        uint32_t min_idx = 0;
        uint64_t min_usage = UINT64_MAX;
        for (uint32_t i = 0; i < mirror->num_actions; i++) {
            uint64_t usage = mirror->actions[i].total_observations +
                             mirror->actions[i].total_executions;
            if (usage < min_usage) {
                min_usage = usage;
                min_idx = i;
            }
        }
        /* Free the old action's neuron indices and reuse its slot */
        if (mirror->actions[min_idx].neuron_indices) {
            nimcp_free(mirror->actions[min_idx].neuron_indices);
        }
        memset(&mirror->actions[min_idx], 0, sizeof(action_mapping_t));
        /* Fill this slot with the new action below */
        mirror->actions[min_idx].action_id = action->action_id;
        strncpy(mirror->actions[min_idx].action_name, action->action_name,
                sizeof(mirror->actions[min_idx].action_name) - 1);
        mirror->actions[min_idx].neuron_indices =
            (uint32_t*)nimcp_calloc(10, sizeof(uint32_t));
        if (!mirror->actions[min_idx].neuron_indices) {
            mirror->actions[min_idx].capacity = 0;
            return UINT32_MAX;
        }
        mirror->actions[min_idx].capacity = 10;
        return min_idx;
    }

    // Grow actions buffer if needed (load path may allocate fewer than max_actions)
    if (mirror->num_actions >= mirror->actions_capacity) {
        uint32_t new_cap = mirror->actions_capacity == 0 ? 8 : mirror->actions_capacity * 2;
        if (new_cap > mirror->config.max_actions) {
            new_cap = mirror->config.max_actions;
        }
        action_mapping_t* new_actions = (action_mapping_t*)nimcp_calloc(new_cap, sizeof(action_mapping_t));
        if (!new_actions) {
            MIRROR_LOG_ERROR("Mirror neurons: failed to grow actions buffer");
            return UINT32_MAX;
        }
        if (mirror->actions && mirror->num_actions > 0) {
            memcpy(new_actions, mirror->actions, mirror->num_actions * sizeof(action_mapping_t));
        }
        nimcp_free(mirror->actions);
        mirror->actions = new_actions;
        mirror->actions_capacity = new_cap;
    }

    // Create new action mapping
    uint32_t idx = mirror->num_actions;
    action_mapping_t* mapping = &mirror->actions[idx];

    mapping->action_id = action->action_id;
    strncpy(mapping->action_name, action->action_name, sizeof(mapping->action_name) - 1);
    mapping->num_neurons = 0;
    mapping->capacity = 10;  // Initial capacity
    mapping->neuron_indices = (uint32_t*)nimcp_calloc(mapping->capacity, sizeof(uint32_t));
    if (!mapping->neuron_indices) {
        MIRROR_LOG_ERROR("Mirror neurons: failed to allocate neuron indices");
        return UINT32_MAX;
    }
    mapping->total_observations = 0;
    mapping->total_executions = 0;
    mapping->avg_similarity = 0.0F;

    /* Increment count only after successful initialization */
    mirror->num_actions++;

    return idx;
}


/**
 * @brief Find or create agent tracking entry
 *
 * WHAT: Get index of agent in agents array, create if not exists
 * WHY:  Track which agents we've observed
 * HOW:  Linear search, then append if not found
 *
 * @return Agent index, or UINT32_MAX on error
 */
static uint32_t find_or_create_agent(mirror_neurons_t mirror, uint32_t agent_id)
{
    if (!mirror) {
        return UINT32_MAX;
    }

    // Search for existing agent
    for (uint32_t i = 0; i < mirror->num_agents; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mirror->num_agents > 256) {
            mirror_neurons_heartbeat("mirror_neuro_loop",
                             (float)(i + 1) / (float)mirror->num_agents);
        }

        if (mirror->agents[i].agent_id == agent_id) {
            return i;
        }
    }

    // Check capacity
    if (mirror->num_agents >= mirror->config.max_agents) {
        MIRROR_LOG_WARN("Mirror neurons: max agents limit reached (%u)",
                      mirror->config.max_agents);
        return UINT32_MAX;
    }

    // Create new agent entry
    uint32_t idx = mirror->num_agents++;
    agent_info_t* agent = &mirror->agents[idx];

    agent->agent_id = agent_id;
    agent->observation_count = 0;
    agent->last_observation_time = nimcp_time_get_ms();
    agent->trust_score = 0.5F;  // Neutral trust initially

    return idx;
}


//=============================================================================
// Core API Implementation - Lifecycle
//=============================================================================

/**
 * @brief Create mirror neuron system
 */
mirror_neurons_t mirror_neurons_create(const mirror_neuron_config_t* config)
{
    // Allocate system structure
    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_heartbeat("mirror_neuro_create", 0.0f);


    mirror_neurons_t mirror = (mirror_neurons_t)nimcp_malloc(sizeof(struct mirror_neurons_system));
    if (!mirror) {
        MIRROR_LOG_ERROR("Mirror neurons: failed to allocate system structure");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mirror_neurons_create: mirror is NULL");
        return NULL;
    }

    memset(mirror, 0, sizeof(struct mirror_neurons_system));

    // Use default config if none provided
    if (config) {
        memcpy(&mirror->config, config, sizeof(mirror_neuron_config_t));
    } else {
        mirror->config = mirror_neurons_get_default_config();
    }

    // Validate config
    if (mirror->config.num_mirror_neurons == 0 || mirror->config.max_actions == 0) {
        MIRROR_LOG_ERROR("Mirror neurons: invalid configuration");
        nimcp_free(mirror);
        mirror = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_neurons_create: mirror->config.num_mirror_neurons is zero");
        return NULL;
    }

    // Allocate neurons
    mirror->neurons = (mirror_neuron_unit_t*)nimcp_calloc(
        mirror->config.num_mirror_neurons,
        sizeof(mirror_neuron_unit_t)
    );
    if (!mirror->neurons) {
        MIRROR_LOG_ERROR("Mirror neurons: failed to allocate neurons");
        nimcp_free(mirror);
        mirror = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mirror_neurons_create: mirror->neurons is NULL");
        return NULL;
    }
    mirror->num_neurons = mirror->config.num_mirror_neurons;

    // Allocate action mappings
    mirror->actions_capacity = mirror->config.max_actions;
    mirror->actions = (action_mapping_t*)nimcp_calloc(
        mirror->actions_capacity,
        sizeof(action_mapping_t)
    );
    if (!mirror->actions) {
        MIRROR_LOG_ERROR("Mirror neurons: failed to allocate actions");
        nimcp_free(mirror->neurons);
        nimcp_free(mirror);
        mirror = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mirror_neurons_create: mirror->actions is NULL");
        return NULL;
    }

    // Allocate agent tracking
    mirror->agents_capacity = mirror->config.max_agents;
    mirror->agents = (agent_info_t*)nimcp_calloc(
        mirror->agents_capacity,
        sizeof(agent_info_t)
    );
    if (!mirror->agents) {
        MIRROR_LOG_ERROR("Mirror neurons: failed to allocate agents");
        nimcp_free(mirror->actions);
        nimcp_free(mirror->neurons);
        nimcp_free(mirror);
        mirror = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mirror_neurons_create: mirror->agents is NULL");
        return NULL;
    }

    /* P2-COG-08: Create mutex for thread safety */
    {
        mutex_attr_t mattr = {.type = MUTEX_TYPE_NORMAL};
        mirror->mutex = nimcp_mutex_create(&mattr);
        if (!mirror->mutex) {
            MIRROR_LOG_ERROR("Mirror neurons: failed to create mutex");
            nimcp_free(mirror->agents);
            nimcp_free(mirror->actions);
            nimcp_free(mirror->neurons);
            nimcp_free(mirror);
            mirror = NULL;
            return NULL;
        }
    }

    // Initialize temporal state
    mirror->creation_time = nimcp_time_get_ms();
    mirror->last_update_time = 0;  // Set to 0 until first observation
    mirror->brain = NULL;  // Initialize brain reference
    mirror->initialized = true;

    MIRROR_LOG_INFO("Mirror neurons: created system with %u neurons, max %u actions",
                  mirror->num_neurons, mirror->config.max_actions);

    // Initialize bio-async fields
    mirror->bio_ctx = NULL;
    mirror->bio_async_enabled = false;

    // Register with bio-async router if available
    MIRROR_LOG_INFO("mirror_neurons: Checking bio-async router initialization...");
    if (bio_router_is_initialized()) {
        MIRROR_LOG_INFO("mirror_neurons: Bio-router initialized, registering module (id=%d, inbox_capacity=32)...",
                       BIO_MODULE_MIRROR_NEURONS);
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_MIRROR_NEURONS,
            .module_name = "mirror_neurons",
            .inbox_capacity = 32,
            .user_data = mirror
        };
        mirror->bio_ctx = bio_router_register_module(&bio_info);
        if (mirror->bio_ctx) {
            mirror->bio_async_enabled = true;

            /* Register handlers via KG-driven wiring callback */
            nimcp_error_t wiring_result = bio_router_register_wiring_callback(
                BIO_MODULE_MIRROR_NEURONS,
                (void*)mirror_neurons_wiring_handler_callback,
                mirror
            );

            if (wiring_result != NIMCP_SUCCESS) {
                /* Legacy fallback: direct handler registration */
                MIRROR_LOG_INFO("mirror_neurons: KG wiring unavailable, using legacy registration");
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(mirror->bio_ctx, BIO_MSG_MIRROR_NEURON_ACTIVATION, handle_mirror_activation));
            }

            MIRROR_LOG_INFO("mirror_neurons: Bio-async communication enabled with handlers (module_id=%d)",
                           BIO_MODULE_MIRROR_NEURONS);
        } else {
            MIRROR_LOG_WARN("mirror_neurons: Bio-async registration failed - module will operate without async messaging");
        }
    } else {
        MIRROR_LOG_INFO("mirror_neurons: Bio-router not initialized, skipping async registration");
    }

    // Initialize SNN and Plasticity bridges
    mirror->snn_bridge = NULL;
    mirror->plasticity_bridge = NULL;
    mirror->bridges_enabled = false;

    // Create SNN bridge with default config
    mirror_snn_config_t snn_config = mirror_snn_config_default();
    mirror->snn_bridge = mirror_snn_create(&snn_config);
    if (!mirror->snn_bridge) {
        MIRROR_LOG_WARN("mirror_neurons: Failed to create SNN bridge - continuing without spike-based computation");
    }

    // Create Plasticity bridge with default config
    mirror_plasticity_config_t plasticity_config = mirror_plasticity_config_default();
    mirror->plasticity_bridge = mirror_plasticity_create(&plasticity_config);
    if (!mirror->plasticity_bridge) {
        MIRROR_LOG_WARN("mirror_neurons: Failed to create Plasticity bridge - continuing without unified learning");
    }

    // Mark bridges as enabled if at least one succeeded
    if (mirror->snn_bridge || mirror->plasticity_bridge) {
        mirror->bridges_enabled = true;
        MIRROR_LOG_INFO("mirror_neurons: Bridge integration enabled (SNN=%s, Plasticity=%s)",
                       mirror->snn_bridge ? "yes" : "no",
                       mirror->plasticity_bridge ? "yes" : "no");
    }

    return mirror;
}


/**
 * @brief Destroy mirror neuron system
 */
void mirror_neurons_destroy(mirror_neurons_t mirror)
{
    if (!mirror) {
        return;
    }

    // Destroy SNN and Plasticity bridges first (before other cleanup)
    /* Phase 8: Heartbeat at operation start */
    mirror_neurons_heartbeat("mirror_neuro_destroy", 0.0f);


    if (mirror->snn_bridge) {
        mirror_snn_destroy(mirror->snn_bridge);
        mirror->snn_bridge = NULL;
        MIRROR_LOG_INFO("mirror_neurons: SNN bridge destroyed");
    }

    if (mirror->plasticity_bridge) {
        mirror_plasticity_destroy(mirror->plasticity_bridge);
        mirror->plasticity_bridge = NULL;
        MIRROR_LOG_INFO("mirror_neurons: Plasticity bridge destroyed");
    }
    mirror->bridges_enabled = false;

    // Destroy enhancement systems (Phase 10.11.4-6)
    if (mirror->stdp_enabled && mirror->stdp_system) {
        mirror_stdp_destroy((mirror_stdp_t)mirror->stdp_system);
        mirror->stdp_system = NULL;
        mirror->stdp_enabled = false;
    }

    if (mirror->resonance_enabled && mirror->resonance_system) {
        motor_resonance_destroy((motor_resonance_t)mirror->resonance_system);
        mirror->resonance_system = NULL;
        mirror->resonance_enabled = false;
    }

    if (mirror->hierarchy_enabled && mirror->hierarchy_system) {
        mirror_hierarchy_destroy((mirror_hierarchy_t)mirror->hierarchy_system);
        mirror->hierarchy_system = NULL;
        mirror->hierarchy_enabled = false;
    }

    // Free substrate backings and pool (Phase 10.11.2)
    if (mirror->substrate_enabled) {
        for (uint32_t i = 0; i < mirror->num_neurons; i++) {
            mirror_neuron_unit_t* unit = &mirror->neurons[i];
            if (unit->has_substrate && unit->substrate) {
                mirror_substrate_backing_destroy(unit->substrate, mirror->substrate_pool);
                unit->substrate = NULL;
                unit->has_substrate = false;
            }
        }

        if (mirror->substrate_pool) {
            mirror_substrate_pool_destroy(mirror->substrate_pool);
            mirror->substrate_pool = NULL;
        }
        mirror->substrate_enabled = false;
    }

    // Unregister from bio-async router
    if (mirror->bio_async_enabled && mirror->bio_ctx) {
        bio_router_unregister_module(mirror->bio_ctx);
        mirror->bio_ctx = NULL;
        mirror->bio_async_enabled = false;
        MIRROR_LOG_INFO("Bio-async communication disabled for mirror_neurons");
    }

    // Free action neuron indices
    for (uint32_t i = 0; i < mirror->num_actions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mirror->num_actions > 256) {
            mirror_neurons_heartbeat("mirror_neuro_loop",
                             (float)(i + 1) / (float)mirror->num_actions);
        }

        if (mirror->actions[i].neuron_indices) {
            nimcp_free(mirror->actions[i].neuron_indices);
        }
    }

    // Free all arrays
    nimcp_free(mirror->agents);
    nimcp_free(mirror->actions);
    nimcp_free(mirror->neurons);

    /* P2-COG-08: Destroy mutex */
    if (mirror->mutex) {
        nimcp_mutex_destroy(mirror->mutex);
        mirror->mutex = NULL;
    }

    MIRROR_LOG_INFO("Mirror neurons: system destroyed");
    nimcp_free(mirror);
    mirror = NULL;
}
