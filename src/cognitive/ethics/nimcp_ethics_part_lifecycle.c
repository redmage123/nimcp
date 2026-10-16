// nimcp_ethics_part_lifecycle.c - lifecycle functions
// Part of nimcp_ethics.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_ethics.c


//=============================================================================
// Engine Creation/Destruction
//=============================================================================

ethics_engine_t ethics_engine_create(const ethics_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ethics_engine_create: config is NULL");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    ethics_heartbeat("ethics_engine_create", 0.0f);


    ethics_engine_t engine = nimcp_calloc(1, sizeof(struct ethics_engine_struct));
    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ethics_engine_create: failed to allocate engine");
        return NULL;
    }

    // Initialize design pattern components
    init_buffer_pool(&engine->buffer_pool);
    engine->policy_table = create_policy_hash_table();
    if (!engine->policy_table) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ethics_engine_create: failed to create policy_table");
        nimcp_free(engine);
        engine = NULL;
        return NULL;
    }
    ethics_init_strategy_table(&engine->strategy_table);

    // Create neural networks
    engine->golden_rule_evaluator = create_golden_rule_network(config->action_feature_size);
    if (!engine->golden_rule_evaluator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ethics_engine_create: failed to create golden_rule_evaluator");
        hash_table_destroy(engine->policy_table);
        nimcp_free(engine);
        engine = NULL;
        return NULL;
    }

    engine->empathy_net = create_empathy_network();
    if (!engine->empathy_net) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ethics_engine_create: failed to create empathy_net");
        brain_destroy(engine->golden_rule_evaluator);
        hash_table_destroy(engine->policy_table);
        nimcp_free(engine);
        engine = NULL;
        return NULL;
    }

    // Allocate storage
    if (!allocate_policy_storage(engine)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ethics_engine_create: failed to allocate policy_storage");
        empathy_network_destroy(engine->empathy_net);
        brain_destroy(engine->golden_rule_evaluator);
        hash_table_destroy(engine->policy_table);
        nimcp_free(engine);
        engine = NULL;
        return NULL;
    }
    if (!allocate_violation_storage(engine)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ethics_engine_create: failed to allocate violation_storage");
        nimcp_free(engine->policies);
        empathy_network_destroy(engine->empathy_net);
        brain_destroy(engine->golden_rule_evaluator);
        hash_table_destroy(engine->policy_table);
        nimcp_free(engine);
        engine = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ethics_engine_create: allocate_violation_storage is NULL");
        return NULL;
    }

    // Initialize incident logging (NIMCP 2.5.1)
    if (!ethics_init_incident_logging(engine)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ethics_engine_create: failed to init incident_logging");
        nimcp_free(engine->violations);
        nimcp_free(engine->policies);
        empathy_network_destroy(engine->empathy_net);
        brain_destroy(engine->golden_rule_evaluator);
        hash_table_destroy(engine->policy_table);
        nimcp_free(engine);
        engine = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "ethics_engine_create: ethics_init_incident_logging is NULL");
        return NULL;
    }

    // Set configuration
    engine->golden_rule_threshold = config->golden_rule_threshold;
    engine->empathy_weight = config->empathy_weight;
    engine->enable_learning = config->enable_learning;
    engine->bio_ctx = NULL;
    engine->bio_async_enabled = false;

    // Initialize Asimov's Laws configuration (NIMCP 2.5.2)
    engine->asimov_config = asimov_default_config();
    engine->asimov_laws_locked = false;
    engine->asimov_violations = 0;
    memset(engine->asimov_laws_hash, 0, sizeof(engine->asimov_laws_hash));

    // Initialize SNN and Plasticity bridges
    engine->snn_bridge = NULL;
    engine->plasticity_bridge = NULL;
    engine->bridges_enabled = false;

    ethics_snn_config_t snn_config = ethics_snn_config_default();
    engine->snn_bridge = ethics_snn_create(&snn_config);

    ethics_plasticity_config_t plasticity_config = ethics_plasticity_config_default();
    engine->plasticity_bridge = ethics_plasticity_create(&plasticity_config);

    if (engine->snn_bridge && engine->plasticity_bridge) {
        engine->bridges_enabled = true;
    }

    // Add foundational Golden Rule policy
    add_golden_rule_policy(engine);

    // Register with bio-async router if enabled
    if (config->enable_bio_async && bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_ETHICS,
            .module_name = "Ethics_Module",  /* Must match KG entity name */
            .inbox_capacity = 64,
            .user_data = engine
        };
        engine->bio_ctx = bio_router_register_module(&bio_info);
        if (engine->bio_ctx) {
            engine->bio_async_enabled = true;

            /* KG-Driven Wiring: Register callback for orchestrator to invoke
             * When orchestrator starts, it discovers HANDLES_MESSAGE relations
             * from the KG and invokes this callback with the message types */
            nimcp_error_t cb_result = bio_router_register_wiring_callback(
                BIO_MODULE_ETHICS,
                (void*)ethics_wiring_handler_callback,
                engine
            );

            if (cb_result != NIMCP_SUCCESS) {
                /* Fallback: Direct registration if orchestrator not available
                 * This ensures backward compatibility with non-KG systems */
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(engine->bio_ctx,
                        BIO_MSG_ETHICS_EVALUATION_REQUEST, handle_ethics_request)
                );
                LOG_INFO("Bio-async enabled (legacy direct registration)");
            } else {
                LOG_INFO("Bio-async enabled (KG-driven wiring callback registered)");
            }
        } else {
            LOG_WARN("Bio-async registration failed");
        }
    }

    return engine;
}


/* W11: set the back-reference to the parent brain for KG emission. */
void ethics_engine_set_brain(ethics_engine_t engine, void* brain) {
    if (!engine) return;
    engine->host_brain = (brain_t)brain;

    /* W11: emit a "configured" lifecycle event (first time brain is wired). */
    if (engine->host_brain) {
        w11_emit_ethics_lifecycle(engine->host_brain, "configured");
    }
}


void ethics_engine_destroy(ethics_engine_t engine)
{
    if (!engine)
        return;

    /* Phase 8: Heartbeat at operation start */
    ethics_heartbeat("ethics_engine_destroy", 0.0f);

    // Unregister from bio-async router
    if (engine->bio_async_enabled && engine->bio_ctx) {
        bio_router_unregister_module(engine->bio_ctx);
        engine->bio_ctx = NULL;
        engine->bio_async_enabled = false;
        LOG_INFO("Bio-async communication disabled");
    }

    // Cleanup incident logging (NIMCP 2.5.1)
    ethics_cleanup_incident_logging(engine);

    // Destroy hash table
    hash_table_destroy(engine->policy_table);

    // Destroy neural networks
    if (engine->golden_rule_evaluator) {
        brain_destroy(engine->golden_rule_evaluator);
    }
    if (engine->empathy_net) {
        empathy_network_destroy(engine->empathy_net);
    }

    // Cleanup SNN and Plasticity bridges
    if (engine->snn_bridge) {
        ethics_snn_destroy(engine->snn_bridge);
        engine->snn_bridge = NULL;
    }
    if (engine->plasticity_bridge) {
        ethics_plasticity_destroy(engine->plasticity_bridge);
        engine->plasticity_bridge = NULL;
    }

    // Free storage arrays
    nimcp_free(engine->policies);
    nimcp_free(engine->violations);
    nimcp_free(engine);
    engine = NULL;
}
