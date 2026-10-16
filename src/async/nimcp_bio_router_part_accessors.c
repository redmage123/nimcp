// nimcp_bio_router_part_accessors.c - accessors functions
// Part of nimcp_bio_router.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_bio_router.c


/*=============================================================================
 * ROUTER LIFECYCLE
 *============================================================================*/

bio_router_config_t bio_router_default_config(void) {
    bio_router_config_t config = {
        .max_modules = 64,
        // Tier-optimized inbox/outbox capacity (saves 150KB+ on MINIMAL tier)
        .inbox_capacity = NIMCP_BIO_INBOX_CAPACITY * 8,  // Router-level default
        .outbox_capacity = NIMCP_BIO_INBOX_CAPACITY * 8,
        .max_message_size = nimcp_tier_scale_size(64 * 1024),  // Tier-scaled max message
        .worker_threads = nimcp_tier_thread_count(),
        .enable_logging = NIMCP_ENABLE_STATISTICS,  // Disable logging on MINIMAL
        .enable_statistics = NIMCP_ENABLE_STATISTICS,
        .routing_timeout_ms = DEFAULT_TIMEOUT_MS,
        .enable_predictive_protocol = (NIMCP_BUILD_TIER <= PLATFORM_TIER_MEDIUM)
    };
    return config;
}


bio_router_t bio_router_get_global(void) {
    return g_router;
}


nimcp_error_t bio_router_get_stats(bio_router_stats_t* stats) {
    NIMCP_CHECK_THROW(g_router && stats, NIMCP_ERROR_NULL_POINTER,
                      "bio_router_get_stats: router or stats is NULL");

    nimcp_platform_mutex_lock(&g_router->stats_mutex);
    *stats = g_router->stats;
    nimcp_platform_mutex_unlock(&g_router->stats_mutex);

    return NIMCP_SUCCESS;
}


/*=============================================================================
 * MODULE CONTEXT ACCESSORS
 *============================================================================*/

bio_module_id_t bio_module_context_get_id(bio_module_context_t ctx) {
    if (!ctx || ctx->magic != BIO_MODULE_MAGIC) return BIO_MODULE_UNKNOWN;

    bio_module_entry_t* entry = ctx->entry;
    if (!entry || entry->magic != BIO_MODULE_MAGIC) return BIO_MODULE_UNKNOWN;

    return entry->module_id;
}


const char* bio_module_context_get_name(bio_module_context_t ctx) {
    if (!ctx || ctx->magic != BIO_MODULE_MAGIC) return "unknown";

    bio_module_entry_t* entry = ctx->entry;
    if (!entry || entry->magic != BIO_MODULE_MAGIC) return "unknown";

    return entry->module_name;
}


void* bio_module_context_get_user_data(bio_module_context_t ctx) {
    if (!ctx || ctx->magic != BIO_MODULE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bio_module_context_get_user_data: ctx is NULL");
        return NULL;
    }

    bio_module_entry_t* entry = ctx->entry;
    if (!entry || entry->magic != BIO_MODULE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bio_module_context_get_user_data: entry is NULL");
        return NULL;
    }

    return entry->user_data;
}


/*=============================================================================
 * ORCHESTRATOR INTEGRATION (KG-Based Runtime Module Assembly)
 *============================================================================*/

nimcp_error_t bio_router_set_orchestrator(struct bio_async_orchestrator* orchestrator) {
    g_router_orchestrator = orchestrator;
    if (orchestrator) {
        LOG_INFO("bio_router_set_orchestrator: orchestrator linked for KG-driven wiring");
    }
    return NIMCP_SUCCESS;
}

nimcp_error_t bio_router_set_brain_kg(struct brain_kg* kg) {
    /* Ensure mutex is initialized */
    nimcp_platform_once(&g_router_init_once, init_router_mutex_once);

    nimcp_platform_mutex_lock(&g_router_brain_kg_mutex);
    g_router_brain_kg = kg;
    nimcp_platform_mutex_unlock(&g_router_brain_kg_mutex);

    if (kg) {
        LOG_INFO("bio_router_set_brain_kg: brain KG linked for message-type dispatch");
    } else {
        LOG_INFO("bio_router_set_brain_kg: brain KG disconnected");
    }
    return NIMCP_SUCCESS;
}
