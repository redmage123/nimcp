// nimcp_portia_part_lifecycle.c - lifecycle functions
// Part of nimcp_portia.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_portia.c


//=============================================================================
// Main API Implementation
//=============================================================================

nimcp_error_t portia_init(const portia_config_t* config) {
    /* Quick check: Already initialized (atomic read for fast path) */
    if (atomic_load(&g_portia_ctx) != NULL) {
        LOG_ERROR(LOG_MODULE, "Portia already initialized");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Portia already initialized");
        return NIMCP_PORTIA_ERROR_ALREADY_INITIALIZED;
    }

    /* Lock to prevent concurrent initialization */
    nimcp_mutex_lock(&g_portia_state_mutex);

    /* Double-check under lock (another thread may have initialized) */
    if (atomic_load(&g_portia_ctx) != NULL) {
        nimcp_mutex_unlock(&g_portia_state_mutex);
        LOG_ERROR(LOG_MODULE, "Portia already initialized");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Portia already initialized (double-check)");
        return NIMCP_PORTIA_ERROR_ALREADY_INITIALIZED;
    }

    /* Security validation - must happen BEFORE dereferencing config */
    if (config && !bbb_check_pointer(config, "portia_init")) {
        LOG_ERROR(LOG_MODULE, "Invalid config pointer");
        nimcp_mutex_unlock(&g_portia_state_mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Invalid config pointer in portia_init");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Use default config if none provided */
    portia_config_t cfg = config ? *config : portia_get_default_config();

    /* Allocate context */
    portia_context_t* ctx = nimcp_calloc(1, sizeof(portia_context_t));
    if (!ctx) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate Portia context");
        nimcp_mutex_unlock(&g_portia_state_mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate Portia context");
        return NIMCP_ERROR_NO_MEMORY;
    }

    ctx->config = cfg;
    nimcp_mutex_init(&ctx->lock, NULL);

    LOG_INFO(LOG_MODULE, "Initializing Portia adaptive intelligence system");

    /* Initialize tier manager */
    nimcp_error_t err = portia_tier_manager_create(&ctx->tier_manager, &cfg.tier_config);
    if (err != NIMCP_SUCCESS) {
        LOG_ERROR(LOG_MODULE, "Failed to create tier manager: %d", err);
        nimcp_free(ctx);
        nimcp_mutex_unlock(&g_portia_state_mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Failed to create tier manager: %d", err);
        return err;
    }

    /* Initialize power monitor */
    err = portia_power_monitor_create(&ctx->power_monitor, &cfg.power_config);
    if (err != NIMCP_SUCCESS) {
        LOG_ERROR(LOG_MODULE, "Failed to create power monitor: %d", err);
        portia_tier_manager_destroy(ctx->tier_manager);
        nimcp_free(ctx);
        nimcp_mutex_unlock(&g_portia_state_mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Failed to create power monitor: %d", err);
        return err;
    }

    /* Initialize resource tracker */
    err = portia_resource_tracker_create(&ctx->resource_tracker, &cfg.resource_config);
    if (err != NIMCP_SUCCESS) {
        LOG_ERROR(LOG_MODULE, "Failed to create resource tracker: %d", err);
        portia_power_monitor_destroy(ctx->power_monitor);
        portia_tier_manager_destroy(ctx->tier_manager);
        nimcp_free(ctx);
        nimcp_mutex_unlock(&g_portia_state_mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Failed to create resource tracker: %d", err);
        return err;
    }

    /* Initialize degradation controller */
    err = portia_degradation_controller_create(&ctx->degradation_controller, &cfg.degradation_config);
    if (err != NIMCP_SUCCESS) {
        LOG_ERROR(LOG_MODULE, "Failed to create degradation controller: %d", err);
        portia_resource_tracker_destroy(ctx->resource_tracker);
        portia_power_monitor_destroy(ctx->power_monitor);
        portia_tier_manager_destroy(ctx->tier_manager);
        nimcp_free(ctx);
        nimcp_mutex_unlock(&g_portia_state_mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Failed to create degradation controller: %d", err);
        return err;
    }

    /* Initialize accelerator detector */
    err = portia_accelerator_detector_create(&ctx->accelerator_detector, &cfg.accelerator_config);
    if (err != NIMCP_SUCCESS) {
        LOG_ERROR(LOG_MODULE, "Failed to create accelerator detector: %d", err);
        portia_degradation_controller_destroy(ctx->degradation_controller);
        portia_resource_tracker_destroy(ctx->resource_tracker);
        portia_power_monitor_destroy(ctx->power_monitor);
        portia_tier_manager_destroy(ctx->tier_manager);
        nimcp_free(ctx);
        nimcp_mutex_unlock(&g_portia_state_mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Failed to create accelerator detector: %d", err);
        return err;
    }

    /* Initialize sensor fusion */
    err = portia_sensor_fusion_create(&ctx->sensor_fusion);
    if (err != NIMCP_SUCCESS) {
        LOG_ERROR(LOG_MODULE, "Failed to create sensor fusion: %d", err);
        portia_accelerator_detector_destroy(ctx->accelerator_detector);
        portia_degradation_controller_destroy(ctx->degradation_controller);
        portia_resource_tracker_destroy(ctx->resource_tracker);
        portia_power_monitor_destroy(ctx->power_monitor);
        portia_tier_manager_destroy(ctx->tier_manager);
        nimcp_free(ctx);
        nimcp_mutex_unlock(&g_portia_state_mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Failed to create sensor fusion: %d", err);
        return err;
    }

    /* Initialize planning engine */
    err = portia_planning_engine_create(&ctx->planning_engine);
    if (err != NIMCP_SUCCESS) {
        LOG_ERROR(LOG_MODULE, "Failed to create planning engine: %d", err);
        portia_sensor_fusion_destroy(ctx->sensor_fusion);
        portia_accelerator_detector_destroy(ctx->accelerator_detector);
        portia_degradation_controller_destroy(ctx->degradation_controller);
        portia_resource_tracker_destroy(ctx->resource_tracker);
        portia_power_monitor_destroy(ctx->power_monitor);
        portia_tier_manager_destroy(ctx->tier_manager);
        nimcp_free(ctx);
        nimcp_mutex_unlock(&g_portia_state_mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Failed to create planning engine: %d", err);
        return err;
    }

    /* Initialize target classifier */
    err = portia_target_classifier_create(&ctx->target_classifier);
    if (err != NIMCP_SUCCESS) {
        LOG_ERROR(LOG_MODULE, "Failed to create target classifier: %d", err);
        portia_planning_engine_destroy(ctx->planning_engine);
        portia_sensor_fusion_destroy(ctx->sensor_fusion);
        portia_accelerator_detector_destroy(ctx->accelerator_detector);
        portia_degradation_controller_destroy(ctx->degradation_controller);
        portia_resource_tracker_destroy(ctx->resource_tracker);
        portia_power_monitor_destroy(ctx->power_monitor);
        portia_tier_manager_destroy(ctx->tier_manager);
        nimcp_free(ctx);
        nimcp_mutex_unlock(&g_portia_state_mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Failed to create target classifier: %d", err);
        return err;
    }

    /* Register with bio-router if enabled */
    ctx->bio_ctx = NULL;
    if (cfg.enable_bio_async && bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_PORTIA,
            .module_name = "portia",
            .inbox_capacity = 32,
            .user_data = ctx
        };
        ctx->bio_ctx = bio_router_register_module(&bio_info);
        if (!ctx->bio_ctx) {
            LOG_WARN(LOG_MODULE, "Failed to register with bio-router (continuing anyway)");
        } else {
            /* KG-Driven Wiring: Register callback for orchestrator to invoke */
            nimcp_error_t cb_result = bio_router_register_wiring_callback(
                BIO_MODULE_PORTIA,
                (void*)portia_wiring_handler_callback,
                ctx
            );

            if (cb_result == NIMCP_SUCCESS) {
                LOG_INFO(LOG_MODULE, "Bio-async registered with KG-driven wiring callback (module_id=0x%04X)", BIO_MODULE_PORTIA);
            } else {
                /* Fallback: Direct registration if orchestrator not available */
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(ctx->bio_ctx,
                                               (bio_message_type_t)BIO_MSG_TYPE_PORTIA_STATUS_QUERY,
                                               portia_message_handler)
                );
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(ctx->bio_ctx,
                                               (bio_message_type_t)BIO_MSG_TYPE_PORTIA_TIER_QUERY,
                                               portia_message_handler)
                );
                LOG_INFO(LOG_MODULE, "Bio-async registered with legacy handler registration (module_id=0x%04X)", BIO_MODULE_PORTIA);
            }
        }
    }

    /* Run initial accelerator detection */
    err = portia_accelerator_detector_scan(ctx->accelerator_detector);
    if (err != NIMCP_SUCCESS) {
        LOG_WARN(LOG_MODULE, "Accelerator detection failed: %d", err);
    }

    ctx->initialized = true;

    /* Set global context atomically (must be last before unlock) */
    atomic_store(&g_portia_ctx, ctx);

    nimcp_mutex_unlock(&g_portia_state_mutex);

    /* Security audit: Log Portia initialization via BBB audit system */
    bbb_audit_log(BBB_AUDIT_INFO, LOG_MODULE, "system_init",
                  "tier=%d subsystems=%d bio_async=%s",
                  ctx->tier_manager ? (int)ctx->tier_manager->current_tier : -1,
                  6,  /* Number of subsystems initialized */
                  ctx->bio_ctx ? "enabled" : "disabled");

    LOG_INFO(LOG_MODULE, "Portia initialization complete");
    return NIMCP_SUCCESS;
}


void portia_destroy(void) {
    /* Quick check: Not initialized (atomic read for fast path) */
    if (atomic_load(&g_portia_ctx) == NULL) {
        return;
    }

    /* Lock to prevent concurrent destroy/init
     * CRITICAL: Hold lock throughout entire destruction to prevent race conditions
     * where another thread could call portia_init() during destruction.
     */
    nimcp_mutex_lock(&g_portia_state_mutex);

    /* Double-check under lock */
    portia_context_t* ctx = atomic_load(&g_portia_ctx);
    if (ctx == NULL) {
        nimcp_mutex_unlock(&g_portia_state_mutex);
        return;
    }

    /* Clear global context atomically first to prevent new operations */
    atomic_store(&g_portia_ctx, NULL);

    LOG_INFO(LOG_MODULE, "Shutting down Portia system");

    /* Unregister from bio-router */
    if (ctx->bio_ctx && bio_router_is_initialized()) {
        bio_router_unregister_module(ctx->bio_ctx);
        ctx->bio_ctx = NULL;
    }

    /* Destroy subsystems */
    portia_target_classifier_destroy(ctx->target_classifier);
    portia_planning_engine_destroy(ctx->planning_engine);
    portia_sensor_fusion_destroy(ctx->sensor_fusion);
    portia_accelerator_detector_destroy(ctx->accelerator_detector);
    portia_degradation_controller_destroy(ctx->degradation_controller);
    portia_resource_tracker_destroy(ctx->resource_tracker);
    portia_power_monitor_destroy(ctx->power_monitor);
    portia_tier_manager_destroy(ctx->tier_manager);

    nimcp_mutex_destroy(&ctx->lock);
    nimcp_free(ctx);

    /* Unlock only after destruction is complete */
    nimcp_mutex_unlock(&g_portia_state_mutex);

    /* bbb_audit_log(BBB_AUDIT_INFO, LOG_MODULE, "Portia system destroyed"); */
    LOG_INFO(LOG_MODULE, "Portia shutdown complete");
}


bool portia_is_initialized(void) {
    /* Load context pointer atomically to prevent tearing
     *
     * THREAD SAFETY ANALYSIS:
     * - ctx->initialized is only set to true at end of portia_init()
     *   AFTER g_portia_ctx is published via atomic_store
     * - ctx is only freed AFTER g_portia_ctx is atomically set to NULL
     * - Therefore, if atomic_load returns non-NULL, ctx is valid and
     *   ctx->initialized is safe to read
     * - No lock needed here: the atomic pointer load provides sufficient
     *   memory ordering for the subsequent plain read
     */
    portia_context_t* ctx = atomic_load(&g_portia_ctx);
    if (ctx == NULL) {
        return false;
    }
    return ctx->initialized;
}


uint32_t portia_recommend_neuron_count(void) {
    if (!portia_is_initialized()) {
        return 1000;
    }

    portia_context_t* ctx = atomic_load(&g_portia_ctx);
    if (!ctx) {
        return 1000;
    }

    nimcp_mutex_lock(&ctx->lock);

    /* Get base recommendation from tier */
    platform_tier_t tier = ctx->tier_manager->current_tier;
    uint32_t base_count = platform_tier_recommend_neuron_count(tier,
        &ctx->resource_tracker->current_resources);

    /* Apply degradation multiplier */
    float degradation_multiplier = 1.0F;
    switch (ctx->degradation_controller->current_level) {
        case PORTIA_DEGRADATION_NONE:     degradation_multiplier = 1.0F; break;
        case PORTIA_DEGRADATION_MINOR:    degradation_multiplier = 0.8F; break;
        case PORTIA_DEGRADATION_MODERATE: degradation_multiplier = 0.5F; break;
        case PORTIA_DEGRADATION_SEVERE:   degradation_multiplier = 0.25F; break;
        case PORTIA_DEGRADATION_EMERGENCY: degradation_multiplier = 0.1F; break;
    }

    /* Apply power state multiplier */
    float power_multiplier = 1.0F;
    switch (ctx->power_monitor->current_state) {
        case PORTIA_POWER_AC:             power_multiplier = 1.0F; break;
        case PORTIA_POWER_BATTERY_FULL:   power_multiplier = 1.0F; break;
        case PORTIA_POWER_BATTERY_MID:    power_multiplier = 0.7F; break;
        case PORTIA_POWER_BATTERY_LOW:    power_multiplier = 0.4F; break;
        case PORTIA_POWER_BATTERY_CRITICAL: power_multiplier = 0.2F; break;
        case PORTIA_POWER_UNKNOWN:        power_multiplier = 0.8F; break;
    }

    nimcp_mutex_unlock(&ctx->lock);

    uint32_t recommended = (uint32_t)(base_count * degradation_multiplier * power_multiplier);

    /* Ensure minimum */
    if (recommended < 100) {
        recommended = 100;
    }

    LOG_DEBUG(LOG_MODULE, "Recommended neuron count: %u (base=%u, degrade=%.2f, power=%.2f)",
              recommended, base_count, degradation_multiplier, power_multiplier);

    return recommended;
}


//=============================================================================
// Subsystem Implementations (Simplified for Core Infrastructure)
//=============================================================================

/* Tier Manager */
static nimcp_error_t portia_tier_manager_create(portia_tier_manager_t** out_mgr, const portia_tier_config_t* config) {
    portia_tier_manager_t* mgr = nimcp_calloc(1, sizeof(portia_tier_manager_t));
    if (!mgr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate tier manager");
        return NIMCP_ERROR_NO_MEMORY;
    }

    mgr->config = *config;
    mgr->current_tier = platform_tier_detect();
    mgr->recommended_tier = mgr->current_tier;
    nimcp_mutex_init(&mgr->lock, NULL);

    *out_mgr = mgr;
    LOG_INFO(LOG_MODULE, "Tier manager created (initial tier: %s)", platform_tier_get_name(mgr->current_tier));
    return NIMCP_SUCCESS;
}


static void portia_tier_manager_destroy(portia_tier_manager_t* mgr) {
    if (!mgr) return;
    nimcp_mutex_destroy(&mgr->lock);
    nimcp_free(mgr);
}


/* Power Monitor */
static nimcp_error_t portia_power_monitor_create(portia_power_monitor_t** out_mon, const portia_power_config_t* config) {
    portia_power_monitor_t* mon = nimcp_calloc(1, sizeof(portia_power_monitor_t));
    if (!mon) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate power monitor");
        return NIMCP_ERROR_NO_MEMORY;
    }

    mon->config = *config;
    mon->current_state = PORTIA_POWER_UNKNOWN;
    mon->battery_level = -1.0F;
    mon->is_on_ac = true;
    nimcp_mutex_init(&mon->lock, NULL);

    *out_mon = mon;
    LOG_INFO(LOG_MODULE, "Power monitor created");
    return NIMCP_SUCCESS;
}


static void portia_power_monitor_destroy(portia_power_monitor_t* mon) {
    if (!mon) return;
    nimcp_mutex_destroy(&mon->lock);
    nimcp_free(mon);
}


/* Resource Tracker */
static nimcp_error_t portia_resource_tracker_create(portia_resource_tracker_t** out_tracker, const portia_resource_config_t* config) {
    portia_resource_tracker_t* tracker = nimcp_calloc(1, sizeof(portia_resource_tracker_t));
    if (!tracker) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate resource tracker");
        return NIMCP_ERROR_NO_MEMORY;
    }

    tracker->config = *config;
    tracker->thermal_state = PORTIA_THERMAL_NOMINAL;
    nimcp_mutex_init(&tracker->lock, NULL);

    *out_tracker = tracker;
    LOG_INFO(LOG_MODULE, "Resource tracker created");
    return NIMCP_SUCCESS;
}


static void portia_resource_tracker_destroy(portia_resource_tracker_t* tracker) {
    if (!tracker) return;
    nimcp_mutex_destroy(&tracker->lock);
    nimcp_free(tracker);
}


/* Degradation Controller */
static nimcp_error_t portia_degradation_controller_create(portia_degradation_controller_t** out_ctrl, const portia_degradation_config_t* config) {
    portia_degradation_controller_t* ctrl = nimcp_calloc(1, sizeof(portia_degradation_controller_t));
    if (!ctrl) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate degradation controller");
        return NIMCP_ERROR_NO_MEMORY;
    }

    ctrl->config = *config;
    ctrl->current_level = PORTIA_DEGRADATION_NONE;
    nimcp_mutex_init(&ctrl->lock, NULL);

    *out_ctrl = ctrl;
    LOG_INFO(LOG_MODULE, "Degradation controller created");
    return NIMCP_SUCCESS;
}


static void portia_degradation_controller_destroy(portia_degradation_controller_t* ctrl) {
    if (!ctrl) return;
    nimcp_mutex_destroy(&ctrl->lock);
    nimcp_free(ctrl);
}


/* Accelerator Detector */
static nimcp_error_t portia_accelerator_detector_create(portia_accelerator_detector_t** out_det, const portia_accelerator_config_t* config) {
    portia_accelerator_detector_t* det = nimcp_calloc(1, sizeof(portia_accelerator_detector_t));
    if (!det) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate accelerator detector");
        return NIMCP_ERROR_NO_MEMORY;
    }

    det->config = *config;
    det->num_accelerators = 0;
    det->detection_complete = false;
    nimcp_mutex_init(&det->lock, NULL);

    *out_det = det;
    LOG_INFO(LOG_MODULE, "Accelerator detector created");
    return NIMCP_SUCCESS;
}


static void portia_accelerator_detector_destroy(portia_accelerator_detector_t* det) {
    if (!det) return;
    nimcp_mutex_destroy(&det->lock);
    nimcp_free(det);
}


static nimcp_error_t portia_accelerator_detector_scan(portia_accelerator_detector_t* det) {
    if (!det) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL detector in portia_accelerator_detector_scan");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(&det->lock);

    /* Simplified: no accelerators detected for now */
    det->num_accelerators = 0;
    det->detection_complete = true;

    nimcp_mutex_unlock(&det->lock);

    LOG_INFO(LOG_MODULE, "Accelerator scan complete (found %u accelerators)", det->num_accelerators);
    return NIMCP_SUCCESS;
}


/* Sensor Fusion */
static nimcp_error_t portia_sensor_fusion_create(portia_sensor_fusion_t** out_fusion) {
    portia_sensor_fusion_t* fusion = nimcp_calloc(1, sizeof(portia_sensor_fusion_t));
    if (!fusion) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate sensor fusion");
        return NIMCP_ERROR_NO_MEMORY;
    }

    fusion->overall_health = 1.0F;
    fusion->resource_pressure = 0.0F;
    fusion->performance_score = 1.0F;
    fusion->efficiency_score = 1.0F;
    nimcp_mutex_init(&fusion->lock, NULL);

    *out_fusion = fusion;
    LOG_INFO(LOG_MODULE, "Sensor fusion created");
    return NIMCP_SUCCESS;
}


static void portia_sensor_fusion_destroy(portia_sensor_fusion_t* fusion) {
    if (!fusion) return;
    nimcp_mutex_destroy(&fusion->lock);
    nimcp_free(fusion);
}


/* Planning Engine */
static nimcp_error_t portia_planning_engine_create(portia_planning_engine_t** out_planner) {
    portia_planning_engine_t* planner = nimcp_calloc(1, sizeof(portia_planning_engine_t));
    if (!planner) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate planning engine");
        return NIMCP_ERROR_NO_MEMORY;
    }

    planner->current_workload = PORTIA_WORKLOAD_UNKNOWN;
    planner->planning_active = false;
    nimcp_mutex_init(&planner->lock, NULL);

    *out_planner = planner;
    LOG_INFO(LOG_MODULE, "Planning engine created");
    return NIMCP_SUCCESS;
}


static void portia_planning_engine_destroy(portia_planning_engine_t* planner) {
    if (!planner) return;
    nimcp_mutex_destroy(&planner->lock);
    nimcp_free(planner);
}


/* Target Classifier */
static nimcp_error_t portia_target_classifier_create(portia_target_classifier_t** out_classifier) {
    portia_target_classifier_t* classifier = nimcp_calloc(1, sizeof(portia_target_classifier_t));
    if (!classifier) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate target classifier");
        return NIMCP_ERROR_NO_MEMORY;
    }

    classifier->classified_workload = PORTIA_WORKLOAD_UNKNOWN;
    classifier->classification_confidence = 0.0F;
    nimcp_mutex_init(&classifier->lock, NULL);

    *out_classifier = classifier;
    LOG_INFO(LOG_MODULE, "Target classifier created");
    return NIMCP_SUCCESS;
}


static void portia_target_classifier_destroy(portia_target_classifier_t* classifier) {
    if (!classifier) return;
    nimcp_mutex_destroy(&classifier->lock);
    nimcp_free(classifier);
}
