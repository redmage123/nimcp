// nimcp_brain_training_integration_part_lifecycle.c - lifecycle functions
// Part of nimcp_brain_training_integration.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_brain_training_integration.c


/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

nimcp_brain_training_ctx_t* nimcp_brain_training_create(
    const nimcp_brain_training_config_t* config)
{
    nimcp_brain_training_config_t local_config;

    if (config) {
        if (nimcp_brain_training_validate_config(config) != NIMCP_SUCCESS) {
            LOG_ERROR("Invalid brain-training configuration");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_training_create: validation failed");
            return NULL;
        }
        local_config = *config;
    } else {
        local_config = nimcp_brain_training_default_config();
    }

    nimcp_brain_training_ctx_t* ctx = nimcp_calloc(1, sizeof(nimcp_brain_training_ctx_t));
    if (!ctx) {
        LOG_ERROR("Failed to allocate brain-training context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_brain_training_create: ctx is NULL");
        return NULL;
    }

    ctx->config = local_config;
    ctx->mode = NIMCP_TRAINING_MODE_TRAIN;
    ctx->next_loss_id = 1;
    ctx->next_optimizer_id = 1;
    ctx->next_scheduler_id = 1;
    ctx->next_gradmgr_id = 1;
    ctx->event_callback = local_config.event_callback;
    ctx->callback_user_data = local_config.callback_user_data;

    /* Initialize statistics */
    ctx->stats.min_loss = DBL_MAX;
    ctx->stats.max_loss = -DBL_MAX;
    ctx->stats.initial_lr = local_config.default_learning_rate;
    ctx->stats.final_lr = local_config.default_learning_rate;
    ctx->stats.lr_min = local_config.default_learning_rate;
    ctx->stats.lr_max = local_config.default_learning_rate;
    ctx->stats.current_grad_scale = 1.0F;

    ctx->last_loss = FLT_MAX;
    ctx->best_loss = FLT_MAX;
    ctx->dropout_seed = (uint32_t)time(NULL);

    /* Initialize plasticity bridge fields (Phase TPB-1) */
    ctx->plasticity_bridge = NULL;
    ctx->biological_modulation_strength = local_config.biological_lr_modulation;
    ctx->prev_loss = FLT_MAX;
    ctx->cumulative_da = 0.0F;
    ctx->cumulative_lr_mod = 0.0F;
    ctx->bio_update_count = 0;

    /* Initialize second messenger cascade system */
    ctx->second_messengers = NULL;
    ctx->second_messengers_enabled = false;

    if (local_config.enable_second_messengers && local_config.num_neurons > 0) {
        second_messenger_config_t sm_config = second_messenger_default_config();
        sm_config.enable_bio_async = true;
        sm_config.enable_security = local_config.enable_security;

        ctx->second_messengers = second_messenger_create(
            local_config.num_neurons,
            &sm_config
        );

        if (ctx->second_messengers) {
            ctx->second_messengers_enabled = true;
            LOG_INFO("Second messenger cascade system created (%u neurons)",
                     local_config.num_neurons);
        } else {
            LOG_WARNING("Failed to create second messenger cascade system");
        }
    }

    /* Initialize Portia Resource Management fields */
    ctx->portia_context = NULL;
    ctx->current_tier = PLATFORM_TIER_FULL;  /* Assume best case initially */
    ctx->resource_aware_training = local_config.enable_portia_integration;
    ctx->tier_batch_size_multiplier = 1.0F;
    ctx->tier_lr_multiplier = 1.0F;
    ctx->training_paused = false;
    ctx->degradation_level = 0;  /* DEGRADATION_LEVEL_NONE */

    LOG_INFO("Brain-training context created (Phase TM-3)");
    return ctx;
}


nimcp_result_t nimcp_brain_training_init(
    nimcp_brain_training_ctx_t* ctx,
    nimcp_sec_integration_t* security_ctx,
    unified_mem_manager_t memory_mgr)
{
    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");

    ctx->security_ctx = security_ctx;
    ctx->memory_mgr = memory_mgr;

    /* Register with security system if enabled */
    if (ctx->config.enable_security && security_ctx) {
        nimcp_result_t res = nimcp_brain_training_register_security(ctx, security_ctx);
        if (res != NIMCP_SUCCESS) {
            LOG_WARNING("Failed to register training modules with security: %d", res);
            /* Continue anyway - security is optional */
        }
    }

    /* Register with bio-async router */
    ctx->bio_ctx = NULL;
    ctx->bio_async_enabled = false;
    if (bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_TRAINING_INTEGRATION,
            .module_name = "brain_training_integration",
            .inbox_capacity = 32,
            .user_data = ctx
        };
        ctx->bio_ctx = bio_router_register_module(&bio_info);
        if (ctx->bio_ctx) {
            ctx->bio_async_enabled = true;
            LOG_INFO("Registered brain_training_integration with bio-async router");
        }

        /* Register second messengers with bio-async if enabled */
        if (ctx->second_messengers && ctx->second_messengers_enabled) {
            bio_router_t router = bio_router_get_global();
            if (router) {
                nimcp_result_t sm_res = second_messenger_register_bioasync(
                    ctx->second_messengers,
                    router
                );
                if (sm_res == NIMCP_SUCCESS) {
                    LOG_INFO("Second messengers registered with bio-async router");
                } else {
                    LOG_WARNING("Failed to register second messengers with bio-async: %d",
                                sm_res);
                }
            }
        }
    }

    LOG_INFO("Brain-training integration initialized");
    return NIMCP_SUCCESS;
}


void nimcp_brain_training_destroy(nimcp_brain_training_ctx_t* ctx)
{
    if (!ctx) {
        return;
    }

    /* Destroy second messenger cascade system */
    if (ctx->second_messengers) {
        second_messenger_destroy(ctx->second_messengers);
        ctx->second_messengers = NULL;
        ctx->second_messengers_enabled = false;
        LOG_INFO("Second messenger cascade system destroyed");
    }

    /* Unregister from bio-async router */
    if (ctx->bio_async_enabled && ctx->bio_ctx) {
        bio_router_unregister_module(ctx->bio_ctx);
        ctx->bio_ctx = NULL;
        ctx->bio_async_enabled = false;
    }

    /* Unregister from security */
    if (ctx->security_registered) {
        nimcp_brain_training_unregister_security(ctx);
    }

    /* Destroy all loss contexts */
    for (uint32_t i = 0; i < NIMCP_TRAINING_MAX_LOSS_CONTEXTS; i++) {
        if (ctx->loss_slots[i].active && ctx->loss_slots[i].ctx) {
            nimcp_loss_destroy(ctx->loss_slots[i].ctx);
        }
    }

    /* Destroy all optimizer contexts */
    for (uint32_t i = 0; i < NIMCP_TRAINING_MAX_OPTIMIZER_CONTEXTS; i++) {
        if (ctx->optimizer_slots[i].active && ctx->optimizer_slots[i].ctx) {
            nimcp_optimizer_destroy(ctx->optimizer_slots[i].ctx);
        }
    }

    /* Destroy all scheduler contexts */
    for (uint32_t i = 0; i < NIMCP_TRAINING_MAX_SCHEDULER_CONTEXTS; i++) {
        if (ctx->scheduler_slots[i].active && ctx->scheduler_slots[i].ctx) {
            nimcp_lr_scheduler_destroy(ctx->scheduler_slots[i].ctx);
        }
    }

    /* Destroy all gradient manager contexts */
    for (uint32_t i = 0; i < NIMCP_TRAINING_MAX_GRADMGR_CONTEXTS; i++) {
        if (ctx->gradmgr_slots[i].active && ctx->gradmgr_slots[i].ctx) {
            nimcp_gradient_manager_destroy(ctx->gradmgr_slots[i].ctx);
        }
    }

    nimcp_free(ctx);
    LOG_INFO("Brain-training context destroyed");
}


/* ============================================================================
 * Loss Function Management
 * ============================================================================ */

static int find_free_loss_slot(nimcp_brain_training_ctx_t* ctx)
{
    for (uint32_t i = 0; i < NIMCP_TRAINING_MAX_LOSS_CONTEXTS; i++) {
        if (!ctx->loss_slots[i].active) {
            return (int)i;
        }
    }
    /* All slots occupied - not an error, just no free slot */
    return -1;
}


nimcp_result_t nimcp_brain_training_create_loss(
    nimcp_brain_training_ctx_t* ctx,
    const nimcp_loss_config_t* config,
    uint32_t* loss_id)
{
    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(config, NIMCP_ERROR_INVALID_PARAM, "config is NULL");
    NIMCP_CHECK_THROW(loss_id, NIMCP_ERROR_INVALID_PARAM, "loss_id is NULL");

    int slot = find_free_loss_slot(ctx);
    NIMCP_CHECK_THROW(slot >= 0, NIMCP_ERROR_MEMORY, "No free loss context slots available");

    nimcp_loss_context_t* loss_ctx = nimcp_loss_create(
        config,
        ctx->security_ctx,
        ctx->memory_mgr
    );
    NIMCP_CHECK_THROW(loss_ctx, NIMCP_ERROR_MEMORY, "Failed to create loss context");

    nimcp_result_t res = nimcp_loss_init(loss_ctx);
    if (res != NIMCP_SUCCESS) {
        nimcp_loss_destroy(loss_ctx);
        return res;
    }

    ctx->loss_slots[slot].ctx = loss_ctx;
    ctx->loss_slots[slot].id = ctx->next_loss_id++;
    ctx->loss_slots[slot].active = true;
    ctx->loss_count++;

    *loss_id = ctx->loss_slots[slot].id;

    /* Record successful interaction with security */
    if (ctx->security_registered) {
        NIMCP_SEC_SUCCESS(ctx->security_ctx, ctx->loss_module_id);
    }

    LOG_DEBUG("Created loss context (id=%u, type=%s)",
              *loss_id, nimcp_loss_type_name(config->type));

    return NIMCP_SUCCESS;
}


nimcp_result_t nimcp_brain_training_destroy_loss(
    nimcp_brain_training_ctx_t* ctx,
    uint32_t loss_id)
{
    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");

    int slot = find_loss_slot_by_id(ctx, loss_id);
    if (slot < 0) {
        return NIMCP_NOT_FOUND;
    }

    nimcp_loss_destroy(ctx->loss_slots[slot].ctx);
    ctx->loss_slots[slot].ctx = NULL;
    ctx->loss_slots[slot].active = false;
    ctx->loss_count--;

    LOG_DEBUG("Destroyed loss context (id=%u)", loss_id);
    return NIMCP_SUCCESS;
}


/* ============================================================================
 * Optimizer Management
 * ============================================================================ */

static int find_free_optimizer_slot(nimcp_brain_training_ctx_t* ctx)
{
    for (uint32_t i = 0; i < NIMCP_TRAINING_MAX_OPTIMIZER_CONTEXTS; i++) {
        if (!ctx->optimizer_slots[i].active) {
            return (int)i;
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_free_optimizer_slot: ctx->optimizer_slots is NULL");
    return -1;
}


nimcp_result_t nimcp_brain_training_create_optimizer(
    nimcp_brain_training_ctx_t* ctx,
    const nimcp_optimizer_config_t* config,
    uint32_t* optimizer_id)
{
    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(config, NIMCP_ERROR_INVALID_PARAM, "config is NULL");
    NIMCP_CHECK_THROW(optimizer_id, NIMCP_ERROR_INVALID_PARAM, "optimizer_id is NULL");

    int slot = find_free_optimizer_slot(ctx);
    NIMCP_CHECK_THROW(slot >= 0, NIMCP_ERROR_MEMORY, "No free optimizer context slots available");

    nimcp_optimizer_context_t* opt_ctx = nimcp_optimizer_create(
        config,
        ctx->security_ctx,
        ctx->memory_mgr
    );
    NIMCP_CHECK_THROW(opt_ctx, NIMCP_ERROR_MEMORY, "Failed to create optimizer context");

    ctx->optimizer_slots[slot].ctx = opt_ctx;
    ctx->optimizer_slots[slot].id = ctx->next_optimizer_id++;
    ctx->optimizer_slots[slot].active = true;
    ctx->optimizer_count++;

    *optimizer_id = ctx->optimizer_slots[slot].id;

    /* Record successful interaction with security */
    if (ctx->security_registered) {
        NIMCP_SEC_SUCCESS(ctx->security_ctx, ctx->optimizer_module_id);
    }

    LOG_DEBUG("Created optimizer context (id=%u, type=%s)",
              *optimizer_id, nimcp_optimizer_type_name(config->type));

    return NIMCP_SUCCESS;
}


nimcp_result_t nimcp_brain_training_destroy_optimizer(
    nimcp_brain_training_ctx_t* ctx,
    uint32_t optimizer_id)
{
    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");

    int slot = find_optimizer_slot_by_id(ctx, optimizer_id);
    if (slot < 0) {
        return NIMCP_NOT_FOUND;
    }

    nimcp_optimizer_destroy(ctx->optimizer_slots[slot].ctx);
    ctx->optimizer_slots[slot].ctx = NULL;
    ctx->optimizer_slots[slot].active = false;
    ctx->optimizer_count--;

    LOG_DEBUG("Destroyed optimizer context (id=%u)", optimizer_id);
    return NIMCP_SUCCESS;
}


void nimcp_brain_training_reset_stats(nimcp_brain_training_ctx_t* ctx)
{
    if (!ctx) {
        return;
    }

    memset(&ctx->stats, 0, sizeof(nimcp_training_session_stats_t));
    ctx->stats.min_loss = DBL_MAX;
    ctx->stats.max_loss = -DBL_MAX;
    ctx->stats.initial_lr = ctx->config.default_learning_rate;
    ctx->stats.final_lr = ctx->config.default_learning_rate;

    ctx->last_loss = FLT_MAX;
    ctx->stable_loss_count = 0;
    ctx->converged = false;
    ctx->diverged = false;
    ctx->current_epoch = 0;
    ctx->current_batch = 0;
}


/* ============================================================================
 * LR Scheduler Management
 * ============================================================================ */

static int find_free_scheduler_slot(nimcp_brain_training_ctx_t* ctx)
{
    for (uint32_t i = 0; i < NIMCP_TRAINING_MAX_SCHEDULER_CONTEXTS; i++) {
        if (!ctx->scheduler_slots[i].active) {
            return (int)i;
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_free_scheduler_slot: ctx->scheduler_slots is NULL");
    return -1;
}


nimcp_result_t nimcp_brain_training_create_scheduler(
    nimcp_brain_training_ctx_t* ctx,
    const nimcp_lr_scheduler_config_t* config,
    uint32_t* scheduler_id)
{
    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(config, NIMCP_ERROR_INVALID_PARAM, "config is NULL");
    NIMCP_CHECK_THROW(scheduler_id, NIMCP_ERROR_INVALID_PARAM, "scheduler_id is NULL");

    int slot = find_free_scheduler_slot(ctx);
    NIMCP_CHECK_THROW(slot >= 0, NIMCP_ERROR_MEMORY, "No free scheduler slots available");

    nimcp_lr_scheduler_ctx_t* sched_ctx = nimcp_lr_scheduler_create(config);
    NIMCP_CHECK_THROW(sched_ctx, NIMCP_ERROR_MEMORY, "Failed to create scheduler context");

    ctx->scheduler_slots[slot].ctx = sched_ctx;
    ctx->scheduler_slots[slot].id = ctx->next_scheduler_id++;
    ctx->scheduler_slots[slot].active = true;
    ctx->scheduler_count++;

    *scheduler_id = ctx->scheduler_slots[slot].id;

    LOG_DEBUG("Created LR scheduler context (id=%u, type=%s)",
              *scheduler_id, nimcp_lr_scheduler_type_name(config->type));

    return NIMCP_SUCCESS;
}


nimcp_result_t nimcp_brain_training_destroy_scheduler(
    nimcp_brain_training_ctx_t* ctx,
    uint32_t scheduler_id)
{
    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");

    int slot = find_scheduler_slot_by_id(ctx, scheduler_id);
    if (slot < 0) {
        return NIMCP_NOT_FOUND;
    }

    nimcp_lr_scheduler_destroy(ctx->scheduler_slots[slot].ctx);
    ctx->scheduler_slots[slot].ctx = NULL;
    ctx->scheduler_slots[slot].active = false;
    ctx->scheduler_count--;

    LOG_DEBUG("Destroyed scheduler context (id=%u)", scheduler_id);
    return NIMCP_SUCCESS;
}


/* ============================================================================
 * Gradient Management
 * ============================================================================ */

static int find_free_gradmgr_slot(nimcp_brain_training_ctx_t* ctx)
{
    for (uint32_t i = 0; i < NIMCP_TRAINING_MAX_GRADMGR_CONTEXTS; i++) {
        if (!ctx->gradmgr_slots[i].active) {
            return (int)i;
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_free_gradmgr_slot: ctx->gradmgr_slots is NULL");
    return -1;
}


nimcp_result_t nimcp_brain_training_create_gradient_manager(
    nimcp_brain_training_ctx_t* ctx,
    const nimcp_gradient_manager_config_t* config,
    uint32_t* gradmgr_id)
{
    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(config, NIMCP_ERROR_INVALID_PARAM, "config is NULL");
    NIMCP_CHECK_THROW(gradmgr_id, NIMCP_ERROR_INVALID_PARAM, "gradmgr_id is NULL");

    int slot = find_free_gradmgr_slot(ctx);
    NIMCP_CHECK_THROW(slot >= 0, NIMCP_ERROR_MEMORY, "No free gradient manager slots available");

    nimcp_gradient_manager_ctx_t* gm_ctx = nimcp_gradient_manager_create(config);
    NIMCP_CHECK_THROW(gm_ctx, NIMCP_ERROR_MEMORY, "Failed to create gradient manager context");

    ctx->gradmgr_slots[slot].ctx = gm_ctx;
    ctx->gradmgr_slots[slot].id = ctx->next_gradmgr_id++;
    ctx->gradmgr_slots[slot].active = true;
    ctx->gradmgr_count++;

    *gradmgr_id = ctx->gradmgr_slots[slot].id;

    LOG_DEBUG("Created gradient manager context (id=%u)", *gradmgr_id);

    return NIMCP_SUCCESS;
}


nimcp_result_t nimcp_brain_training_destroy_gradient_manager(
    nimcp_brain_training_ctx_t* ctx,
    uint32_t gradmgr_id)
{
    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");

    int slot = find_gradmgr_slot_by_id(ctx, gradmgr_id);
    if (slot < 0) {
        return NIMCP_NOT_FOUND;
    }

    nimcp_gradient_manager_destroy(ctx->gradmgr_slots[slot].ctx);
    ctx->gradmgr_slots[slot].ctx = NULL;
    ctx->gradmgr_slots[slot].active = false;
    ctx->gradmgr_count--;

    LOG_DEBUG("Destroyed gradient manager context (id=%u)", gradmgr_id);
    return NIMCP_SUCCESS;
}


void nimcp_brain_training_reset_early_stop(nimcp_brain_training_ctx_t* ctx)
{
    if (!ctx) {
        return;
    }

    ctx->best_loss = FLT_MAX;
    ctx->early_stop_wait = 0;
    ctx->early_stopped = false;
    ctx->stats.early_stop_wait_count = 0;
    ctx->stats.best_loss = FLT_MAX;
    ctx->stats.early_stopped = false;
}


//=============================================================================
// Training Callbacks Integration (Phase TCB-1)
//=============================================================================

nimcp_result_t nimcp_brain_training_create_callbacks(nimcp_brain_training_ctx_t* ctx)
{
    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM,
                      "nimcp_brain_training_create_callbacks: ctx is NULL");

    if (ctx->callbacks) {
        LOG_WARNING("Callbacks already created");
        return NIMCP_SUCCESS;
    }

    /* Create callbacks context */
    ctx->callbacks = tcb_create(NULL);
    NIMCP_CHECK_THROW(ctx->callbacks, NIMCP_ERROR_MEMORY,
                      "nimcp_brain_training_create_callbacks: failed to create callbacks");

    /* If plasticity bridge is connected, wire it up */
    if (ctx->plasticity_bridge) {
        tpb_connect_callbacks(ctx->plasticity_bridge, ctx->callbacks);
        tpb_register_plasticity_callbacks(ctx->plasticity_bridge);
    }

    LOG_INFO("Training callbacks created");
    return NIMCP_SUCCESS;
}
