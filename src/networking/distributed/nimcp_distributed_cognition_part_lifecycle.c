// nimcp_distributed_cognition_part_lifecycle.c - lifecycle functions
// Part of nimcp_distributed_cognition.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_distributed_cognition.c

distrib_cognition_t distrib_cognition_create(
    const distrib_cognition_config_t* config,
    p2p_node_t p2p_node)
{
    if (!p2p_node) {
        LOG_ERROR(LOG_MODULE, "Invalid P2P node");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "distrib_cognition_create: p2p_node is NULL");
        return NULL;
    }

    // Allocate coordinator
    distrib_cognition_t dc = (distrib_cognition_t)nimcp_calloc(1, sizeof(struct distrib_cognition_struct));
    if (!dc) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(struct distrib_cognition_struct),
                          "Failed to allocate distributed cognition coordinator");
        LOG_ERROR(LOG_MODULE, "Failed to allocate coordinator");
        return NULL;
    }

    // Set configuration
    if (config) {
        dc->config = *config;
    } else {
        dc->config = DEFAULT_CONFIG;
    }

    // Store P2P node
    dc->p2p_node = p2p_node;

    // Initialize rwlock
    if (nimcp_rwlock_init(&dc->rwlock) != NIMCP_SUCCESS) {
        NIMCP_THROW_THREADING(NIMCP_ERROR_THREAD_CREATE, 0,
                             "Failed to initialize rwlock for distributed cognition coordinator");
        LOG_ERROR(LOG_MODULE, "Failed to initialize rwlock");
        nimcp_free(dc);
        return NULL;
    }

    // Allocate initial capacity for registered systems
    dc->neuromod_pool_capacity = 4;
    dc->neuromod_pools = (registered_neuromod_t*)nimcp_calloc(dc->neuromod_pool_capacity, sizeof(registered_neuromod_t));
    if (!dc->neuromod_pools) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY,
                          dc->neuromod_pool_capacity * sizeof(registered_neuromod_t),
                          "Failed to allocate neuromodulator pool storage");
        nimcp_rwlock_destroy(&dc->rwlock);
        nimcp_free(dc);
        return NULL;
    }

    dc->glial_system_capacity = 4;
    dc->glial_systems = (registered_glial_t*)nimcp_calloc(dc->glial_system_capacity, sizeof(registered_glial_t));
    if (!dc->glial_systems) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY,
                          dc->glial_system_capacity * sizeof(registered_glial_t),
                          "Failed to allocate glial system storage");
        nimcp_free(dc->neuromod_pools);
        nimcp_rwlock_destroy(&dc->rwlock);
        nimcp_free(dc);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "distrib_cognition_create: dc->glial_systems is NULL");
        return NULL;
    }

    dc->brain_region_capacity = 8;
    dc->brain_regions = (registered_region_t*)nimcp_calloc(dc->brain_region_capacity, sizeof(registered_region_t));
    if (!dc->brain_regions) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY,
                          dc->brain_region_capacity * sizeof(registered_region_t),
                          "Failed to allocate brain region storage");
        nimcp_free(dc->glial_systems);
        nimcp_free(dc->neuromod_pools);
        nimcp_rwlock_destroy(&dc->rwlock);
        nimcp_free(dc);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "distrib_cognition_create: dc->brain_regions is NULL");
        return NULL;
    }

    // Initialize statistics
    memset(&dc->stats, 0, sizeof(distrib_cognition_stats_t));

    // Not running yet
    dc->running = false;
    dc->shutdown_requested = false;

    // Initialize bio-async if enabled
    dc->bio_ctx = NULL;
    dc->bio_async_enabled = false;
    if (dc->config.enable_bio_async && bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_DISTRIBUTED,
            .module_name = "distributed_cognition",
            .inbox_capacity = 64,
            .user_data = dc
        };
        dc->bio_ctx = bio_router_register_module(&bio_info);
        if (dc->bio_ctx) {
            dc->bio_async_enabled = true;
            LOG_INFO(LOG_MODULE, "Bio-async communication registered");
        } else {
            LOG_WARN(LOG_MODULE, "Failed to register bio-async communication");
        }
    }

    LOG_INFO(LOG_MODULE, "Coordinator created successfully");

    return dc;
}


void distrib_cognition_destroy(distrib_cognition_t dc)
{
    if (!dc) {
        return;
    }

    // Stop if running
    if (dc->running) {
        distrib_cognition_stop(dc);
    }

    // Unregister bio-async
    if (dc->bio_async_enabled && dc->bio_ctx) {
        bio_router_unregister_module(dc->bio_ctx);
        dc->bio_ctx = NULL;
        dc->bio_async_enabled = false;
        LOG_INFO(LOG_MODULE, "Bio-async communication unregistered");
    }

    // Free registered systems
    if (dc->neuromod_pools) {
        nimcp_free(dc->neuromod_pools);
    }

    if (dc->glial_systems) {
        nimcp_free(dc->glial_systems);
    }

    if (dc->brain_regions) {
        nimcp_free(dc->brain_regions);
    }

    // Destroy rwlock
    nimcp_rwlock_destroy(&dc->rwlock);

    // Free coordinator
    nimcp_free(dc);

    log_message(LOG_LEVEL_INFO, "[distributed_cognition] Coordinator destroyed");
}
