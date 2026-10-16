// nimcp_global_workspace_part_lifecycle.c - lifecycle functions
// Part of nimcp_global_workspace.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_global_workspace.c


//=============================================================================
// Lifecycle Functions
//=============================================================================

global_workspace_t* global_workspace_create(void) {
    /* Phase 8: Heartbeat at operation start */
    global_workspace_heartbeat("global_works_create", 0.0f);


    global_workspace_config_t config = global_workspace_default_config();
    return global_workspace_create_custom(&config);
}


global_workspace_t* global_workspace_create_custom(
    const global_workspace_config_t* config)
{
    // Validate configuration
    if (config != NULL) {
        char error[NIMCP_ERROR_BUFFER_SIZE];
        if (!global_workspace_validate_config(config, error, sizeof(error))) {
            LOG_ERROR("Global workspace creation failed: %s", error);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "global_workspace_create_custom: global_workspace_validate_config is NULL");
            return NULL;
        }
    }

    // Use defaults if NULL config
    /* Phase 8: Heartbeat at operation start */
    global_workspace_heartbeat("global_works_create_custom", 0.0f);


    global_workspace_config_t actual_config;
    if (config != NULL) {
        actual_config = *config;
    } else {
        actual_config = global_workspace_default_config();
    }

    // Allocate workspace structure
    struct global_workspace_struct* workspace =
        (struct global_workspace_struct*)nimcp_calloc(1, sizeof(struct global_workspace_struct));
    if (workspace == NULL) {
        LOG_ERROR("Failed to allocate workspace structure");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "global_workspace_create_custom: failed to allocate workspace");
        return NULL;
    }

    // Copy configuration
    workspace->config = actual_config;

    // Initialize thread safety mutex
    nimcp_platform_mutex_init(&workspace->mutex, false);

    // Allocate broadcast content buffer
    workspace->broadcast_content =
        (float*)nimcp_calloc(actual_config.capacity_dim, sizeof(float));
    if (workspace->broadcast_content == NULL) {
        LOG_ERROR("Failed to allocate broadcast content buffer");
        goto cleanup_mutex;
    }

    // Initialize broadcast state
    workspace->current_broadcast.content = workspace->broadcast_content;
    workspace->current_broadcast.content_dim = actual_config.capacity_dim;
    workspace->current_broadcast.is_valid = false;

    // Allocate history (if enabled)
    if (actual_config.enable_history && actual_config.history_depth > 0) {
        workspace->history = (workspace_broadcast_t*)nimcp_calloc(
            actual_config.history_depth, sizeof(workspace_broadcast_t));
        if (workspace->history == NULL) {
            LOG_ERROR("Failed to allocate history buffer");
            goto cleanup_broadcast;
        }

        // Allocate content buffers for history
        workspace->history_content = (float**)nimcp_calloc(
            actual_config.history_depth, sizeof(float*));
        if (workspace->history_content == NULL) {
            LOG_ERROR("Failed to allocate history content array");
            goto cleanup_history;
        }

        for (uint32_t i = 0; i < actual_config.history_depth; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && actual_config.history_depth > 256) {
                global_workspace_heartbeat("global_works_loop",
                                 (float)(i + 1) / (float)actual_config.history_depth);
            }

            workspace->history_content[i] = (float*)nimcp_calloc(
                actual_config.capacity_dim, sizeof(float));
            if (workspace->history_content[i] == NULL) {
                LOG_ERROR("Failed to allocate history content buffer %u", i);
                // Clean up previously allocated history content buffers
                for (uint32_t j = 0; j < i; j++) {
                    nimcp_free(workspace->history_content[j]);
                }
                goto cleanup_history_content;
            }
        }
    }

    // Initialize competitor pool
    for (uint32_t i = 0; i < GLOBAL_WORKSPACE_MAX_COMPETITORS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && GLOBAL_WORKSPACE_MAX_COMPETITORS > 256) {
            global_workspace_heartbeat("global_works_loop",
                             (float)(i + 1) / (float)GLOBAL_WORKSPACE_MAX_COMPETITORS);
        }

        workspace->competitors[i].is_active = false;
    }
    workspace->num_active_competitors = 0;

    // Initialize subscribers
    workspace->num_subscribers = 0;

    // Initialize timing
    workspace->last_broadcast_time_ms = 0;
    workspace->pool_activation_time_ms = 0;
    workspace->next_broadcast_id = 1;
    workspace->last_winner_idx = 0;

    // Initialize statistics
    memset(&workspace->stats, 0, sizeof(workspace_statistics_t));

    // Phase 1.5: Initialize memory pools for hot-path allocations
    // Pool for broadcast content buffers (capacity_dim floats each)
    memory_pool_config_t content_pool_config = {
        .block_size = actual_config.capacity_dim * sizeof(float),
        .num_blocks = 4,  // Current + temp + 2 spare
        .alignment = 16,  // SIMD alignment
        .enable_tracking = false,
        .enable_guard_pages = false
    };
    workspace->broadcast_content_pool = memory_pool_create(&content_pool_config);
    /* Memory pool creation is non-fatal — workspace operates without pools (slower alloc path) */
    if (workspace->broadcast_content_pool == NULL) {
        LOG_WARN("Failed to create broadcast content pool — continuing without pool optimization");
    }

    // Pool for history content buffers (same size, more blocks)
    memory_pool_config_t history_pool_config = {
        .block_size = actual_config.capacity_dim * sizeof(float),
        .num_blocks = actual_config.history_depth + 2,  // history_depth + spares
        .alignment = 16,
        .enable_tracking = false,
        .enable_guard_pages = false
    };
    workspace->history_content_pool = memory_pool_create(&history_pool_config);
    /* Memory pool creation is non-fatal — workspace operates without pools (slower alloc path) */
    if (workspace->history_content_pool == NULL) {
        LOG_WARN("Failed to create history content pool — continuing without pool optimization");
    }

    // Initialize bio-async fields
    workspace->bio_ctx = NULL;
    workspace->bio_async_enabled = false;

    // Register with bio-async router if available
    NIMCP_LOGGING_DEBUG("global_workspace: Checking bio-async router initialization...");
    if (bio_router_is_initialized()) {
        NIMCP_LOGGING_DEBUG("global_workspace: Bio-router initialized, registering module (id=%d, inbox_capacity=64)...",
                           BIO_MODULE_GLOBAL_WORKSPACE);
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_GLOBAL_WORKSPACE,
            .module_name = "global_workspace",
            .inbox_capacity = 64,
            .user_data = workspace
        };
        workspace->bio_ctx = bio_router_register_module(&bio_info);
        if (workspace->bio_ctx) {
            workspace->bio_async_enabled = true;

            // Try KG-driven wiring callback registration first
            nimcp_error_t wiring_result = bio_router_register_wiring_callback(
                BIO_MODULE_GLOBAL_WORKSPACE,
                (void*)global_workspace_wiring_handler_callback,
                workspace
            );

            if (wiring_result == NIMCP_SUCCESS) {
                NIMCP_LOGGING_INFO("global_workspace: KG-driven wiring callback registered");
            } else {
                // Legacy fallback - register handlers directly
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(workspace->bio_ctx, BIO_MSG_ATTENTION_SHIFT, handle_attention_shift)
                );
                NIMCP_LOGGING_INFO("global_workspace: legacy handler registration");
            }
        } else {
            NIMCP_LOGGING_WARN("global_workspace: Bio-async registration failed - module will operate without async messaging");
        }
    } else {
        NIMCP_LOGGING_DEBUG("global_workspace: Bio-router not initialized, skipping async registration");
    }

    // Initialize SNN and Plasticity bridges
    workspace->snn_bridge = NULL;
    workspace->plasticity_bridge = NULL;
    workspace->bridges_enabled = false;

    // Create SNN bridge with default config
    gw_snn_config_t snn_config = gw_snn_config_default();
    workspace->snn_bridge = gw_snn_create(&snn_config);
    if (workspace->snn_bridge) {
        NIMCP_LOGGING_DEBUG("global_workspace: SNN bridge created successfully");
    } else {
        NIMCP_LOGGING_WARN("global_workspace: Failed to create SNN bridge - continuing without SNN integration");
    }

    // Create Plasticity bridge with default config
    gw_plasticity_config_t plasticity_config = gw_plasticity_config_default();
    workspace->plasticity_bridge = gw_plasticity_create(&plasticity_config);
    if (workspace->plasticity_bridge) {
        NIMCP_LOGGING_DEBUG("global_workspace: Plasticity bridge created successfully");
    } else {
        NIMCP_LOGGING_WARN("global_workspace: Failed to create Plasticity bridge - continuing without plasticity integration");
    }

    // Mark bridges as enabled if at least one was created
    if (workspace->snn_bridge || workspace->plasticity_bridge) {
        workspace->bridges_enabled = true;
        NIMCP_LOGGING_INFO("global_workspace: SNN/Plasticity bridge integration enabled");
    }

    // Note: global_workspace_t is typedef'd as a pointer, so function signature
    // expects global_workspace_t* (double pointer). Cast to match.
    return (global_workspace_t*)workspace;

    /* Rollback labels — destroy in reverse creation order */
cleanup_history_content:
    nimcp_free(workspace->history_content);
    workspace->history_content = NULL;
cleanup_history:
    nimcp_free(workspace->history);
    workspace->history = NULL;
cleanup_broadcast:
    nimcp_free(workspace->broadcast_content);
    workspace->broadcast_content = NULL;
cleanup_mutex:
    nimcp_platform_mutex_destroy(&workspace->mutex);
    nimcp_free(workspace);
    workspace = NULL;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "global_workspace_create_custom: allocation failed");
    return NULL;
}


void global_workspace_destroy(global_workspace_t* workspace) {
    if (workspace == NULL) return;

    /* Phase 8: Heartbeat at operation start */
    global_workspace_heartbeat("global_works_destroy", 0.0f);


    struct global_workspace_struct* ws = (struct global_workspace_struct*)workspace;

    /* Caller must ensure exclusive access — no threads may be using the workspace */

    // Unregister from bio-async router
    if (ws->bio_async_enabled && ws->bio_ctx) {
        bio_router_unregister_module(ws->bio_ctx);
        ws->bio_ctx = NULL;
        ws->bio_async_enabled = false;
        NIMCP_LOGGING_INFO("Bio-async communication disabled for global_workspace");
    }

    // Destroy SNN and Plasticity bridges
    if (ws->snn_bridge) {
        gw_snn_destroy(ws->snn_bridge);
        ws->snn_bridge = NULL;
        NIMCP_LOGGING_DEBUG("global_workspace: SNN bridge destroyed");
    }
    if (ws->plasticity_bridge) {
        gw_plasticity_destroy(ws->plasticity_bridge);
        ws->plasticity_bridge = NULL;
        NIMCP_LOGGING_DEBUG("global_workspace: Plasticity bridge destroyed");
    }
    ws->bridges_enabled = false;

    // Free history content buffers
    if (ws->history_content != NULL) {
        for (uint32_t i = 0; i < ws->config.history_depth; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && ws->config.history_depth > 256) {
                global_workspace_heartbeat("global_works_loop",
                                 (float)(i + 1) / (float)ws->config.history_depth);
            }

            nimcp_free(ws->history_content[i]);
        }
        nimcp_free(ws->history_content);
    }

    // Free history metadata
    nimcp_free(ws->history);

    // Free broadcast content
    nimcp_free(ws->broadcast_content);

    // Phase 1.5: Destroy memory pools
    if (ws->broadcast_content_pool != NULL) {
        memory_pool_destroy(ws->broadcast_content_pool);
    }
    if (ws->history_content_pool != NULL) {
        memory_pool_destroy(ws->history_content_pool);
    }

    // Free competitor content buffers that we own
    for (uint32_t i = 0; i < GLOBAL_WORKSPACE_MAX_COMPETITORS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && GLOBAL_WORKSPACE_MAX_COMPETITORS > 256) {
            global_workspace_heartbeat("global_works_loop",
                             (float)(i + 1) / (float)GLOBAL_WORKSPACE_MAX_COMPETITORS);
        }

        if (ws->competitors[i].is_active && ws->competitors[i].content) {
            nimcp_free(ws->competitors[i].content);
            ws->competitors[i].content = NULL;
        }
    }

    // Destroy mutex (no unlock needed — caller guarantees exclusive access)
    nimcp_platform_mutex_destroy(&ws->mutex);

    // Free workspace structure
    nimcp_free(ws);
    ws = NULL;
}


void global_workspace_reset_statistics(global_workspace_t* workspace) {
    if (workspace == NULL) return;

    /* Phase 8: Heartbeat at operation start */
    global_workspace_heartbeat("global_works_reset_statistics", 0.0f);


    struct global_workspace_struct* ws = (struct global_workspace_struct*)workspace;

    nimcp_platform_mutex_lock(&ws->mutex);
    memset(&ws->stats, 0, sizeof(workspace_statistics_t));

    // Restore current counts
    ws->stats.current_subscribers = ws->num_subscribers;
    ws->stats.current_competitors = ws->num_active_competitors;
    nimcp_platform_mutex_unlock(&ws->mutex);
}
