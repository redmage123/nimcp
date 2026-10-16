// nimcp_swarm_consciousness_part_lifecycle.c - lifecycle functions
// Part of nimcp_swarm_consciousness.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_swarm_consciousness.c


//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * WHAT: Create swarm consciousness context
 * WHY:  Initialize consciousness monitoring for swarm
 * HOW:  Allocate context, initialize mutex, register with BBB and bio-async
 */
swarm_consciousness_ctx_t* swarm_consciousness_create(
    const swarm_consciousness_config_t* config)
{
    // Guard: Validate configuration pointer
    if (!bbb_check_pointer(config, "swarm_consciousness_create")) {
        LOG_ERROR("Null configuration pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Null configuration pointer in swarm_consciousness_create");
        return NULL;
    }

    // Allocate context
    swarm_consciousness_ctx_t* ctx = (swarm_consciousness_ctx_t*)
        nimcp_calloc(1, sizeof(swarm_consciousness_ctx_t));
    if (!ctx) {
        LOG_ERROR("Failed to allocate consciousness context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate consciousness context");
        return NULL;
    }

    // Initialize magic and configuration
    ctx->magic = SWARM_CONSCIOUSNESS_MAGIC;
    memcpy(&ctx->config, config, sizeof(swarm_consciousness_config_t));

    // Initialize mutex
    if (nimcp_mutex_init(&ctx->lock, NULL) != 0) {
        LOG_ERROR("Failed to initialize mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Failed to initialize mutex in swarm_consciousness_create");
        nimcp_free(ctx);
        return NULL;
    }

    // Allocate current metrics
    ctx->current_metrics = (swarm_consciousness_metrics_t*)
        nimcp_calloc(1, sizeof(swarm_consciousness_metrics_t));
    if (!ctx->current_metrics) {
        LOG_ERROR("Failed to allocate metrics structure");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate consciousness metrics structure");
        nimcp_mutex_destroy(&ctx->lock);
        nimcp_free(ctx);
        return NULL;
    }

    // Initialize metrics
    ctx->current_metrics->collective_phi = 0.0f;
    ctx->current_metrics->consciousness_state = SWARM_CONSCIOUSNESS_DORMANT;
    ctx->current_metrics->drone_count = 0;
    ctx->current_metrics->timestamp = get_time_ms();

    // Initialize scaling model
    ctx->scaling_model.base_phi = 0.0f;
    ctx->scaling_model.scaling_exponent = 1.0f;
    ctx->scaling_model.synergy_factor = 0.0f;
    ctx->scaling_model.saturation_point = 100.0f;

    // Initialize history
    ctx->history_index = 0;
    ctx->history_count = 0;
    memset(ctx->phi_history, 0, sizeof(ctx->phi_history));

    // Initialize statistics
    ctx->total_computations = 0;
    ctx->state_transitions = 0;
    ctx->creation_time_ms = get_time_ms();

    // Initialize monitoring state
    ctx->monitoring_active = false;
    ctx->callback = NULL;
    ctx->user_data = NULL;
    ctx->swarm_brain = NULL;

    // Initialize imagination integration
    ctx->collective_imagination_callback = NULL;
    ctx->collective_imagination_user_data = NULL;
    ctx->imagination_handler_registered = false;
    ctx->bio_module_ctx = NULL;

    // Register with BBB security
    if (!bbb_register_module("swarm_consciousness", BBB_MODULE_TYPE_SWARM)) {
        LOG_WARN("Failed to register with BBB security module");
    }

    // Register with bio-async if enabled
    ctx->bio_async_registered = false;
    if (config->enable_bio_async && nimcp_bio_async_is_initialized()) {
        ctx->bio_async_registered = true;
        LOG_INFO("Bio-async integration enabled for swarm consciousness");
    }

    LOG_INFO("Swarm consciousness context created");
    bbb_audit_log(BBB_AUDIT_INFO, "swarm_consciousness", "create",
                  "Context created with method=%d", config->phi_aggregation_method);

    return (swarm_consciousness_ctx_t*)ctx;
}


/**
 * WHAT: Destroy swarm consciousness context
 * WHY:  Clean up resources and stop monitoring
 * HOW:  Stop monitoring thread, free memory, destroy mutex
 */
void swarm_consciousness_destroy(swarm_consciousness_ctx_t* context) {
    // Guard: Null check
    if (!context) {
        return;
    }

    swarm_consciousness_ctx_t* ctx = (swarm_consciousness_ctx_t*)context;

    // Guard: Validate magic
    if (ctx->magic != SWARM_CONSCIOUSNESS_MAGIC) {
        LOG_ERROR("Invalid context magic in destroy");
        return;
    }

    // Stop monitoring if active
    if (ctx->monitoring_active) {
        swarm_consciousness_stop_monitoring(context);
    }

    // Unregister imagination handlers and bio-router module
    if (ctx->bio_module_ctx) {
        if (ctx->imagination_handler_registered) {
            bio_router_unregister_handler(ctx->bio_module_ctx,
                                          BIO_MSG_IMAGINATION_COLLECTIVE_SHARE);
            bio_router_unregister_handler(ctx->bio_module_ctx,
                                          BIO_MSG_IMAGINATION_COLLECTIVE_INSIGHT);
        }
        bio_router_unregister_module(ctx->bio_module_ctx);
        ctx->bio_module_ctx = NULL;
    }

    // Unregister from BBB
    bbb_unregister_module("swarm_consciousness");

    // Free metrics
    if (ctx->current_metrics) {
        nimcp_free(ctx->current_metrics);
    }

    // Destroy mutex
    nimcp_mutex_destroy(&ctx->lock);

    // Clear magic
    ctx->magic = 0;

    // Free context
    nimcp_free(ctx);

    LOG_INFO("Swarm consciousness context destroyed");
}


/**
 * WHAT: Free consciousness metrics structure
 * WHY:  Release allocated memory
 * HOW:  Simple free (no sub-allocations)
 */
void swarm_consciousness_metrics_free(swarm_consciousness_metrics_t* metrics) {
    if (metrics) {
        nimcp_free(metrics);
    }
}
