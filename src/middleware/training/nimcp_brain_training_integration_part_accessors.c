// nimcp_brain_training_integration_part_accessors.c - accessors functions
// Part of nimcp_brain_training_integration.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_brain_training_integration.c


/* ============================================================================
 * Default Configuration
 * ============================================================================ */

nimcp_brain_training_config_t nimcp_brain_training_default_config(void)
{
    nimcp_brain_training_config_t config = {0};

    /* Loss function defaults */
    config.default_loss_type = NIMCP_LOSS_MSE;
    config.default_reduction = NIMCP_LOSS_REDUCE_MEAN;

    /* Optimizer defaults */
    config.default_optimizer = NIMCP_OPTIMIZER_ADAM;
    config.default_learning_rate = 0.001F;

    /* Learning rate scheduler defaults */
    config.default_lr_scheduler = NIMCP_LR_STEP;
    config.enable_lr_scheduler = false;

    /* Regularization defaults */
    config.default_reg_type = NIMCP_REG_L2;
    config.default_reg_lambda = 0.0001F;
    config.l1_lambda = 0.0F;             /* No L1 by default */
    config.l2_lambda = 0.0001F;          /* Small L2 weight decay */
    config.dropout_rate = 0.0F;          /* No dropout by default */
    config.default_clip_mode = NIMCP_CLIP_BY_NORM;
    config.default_clip_value = 1.0F;
    config.gradient_clip_norm = 0.0F;    /* No norm clipping by default */
    config.enable_regularization = false;
    config.enable_gradient_clipping = false;

    /* Gradient management defaults */
    config.gradient_accum_steps = 1;          /* No accumulation by default */
    config.gradient_accumulation_steps = 1;   /* Alias */
    config.gradient_clip_value = 0.0F;        /* No value clipping by default */
    config.enable_gradient_management = false;
    config.enable_gradient_scaling = false;
    config.enable_gradient_health_check = true;

    /* Early stopping */
    config.enable_early_stopping = false;
    config.early_stop_patience = 10;
    config.early_stop_min_delta = 1e-4F;

    /* Training parameters */
    config.max_epochs = 1000;
    config.batch_size = NIMCP_DEFAULT_BATCH_SIZE;
    config.convergence_threshold = 1e-6F;
    config.divergence_threshold = 1e6F;

    /* Security integration */
    config.enable_security = true;
    config.register_with_bbb = true;

    /* Event callbacks */
    config.event_callback = NULL;
    config.callback_user_data = NULL;

    /* Memory management */
    config.use_memory_pool = true;
    config.cow_strategy = UNIFIED_STRATEGY_AUTO;

    /* Plasticity Bridge defaults (Phase TPB-1) */
    config.enable_plasticity_bridge = false;  /* Disabled by default */
    config.rpe_to_da_gain = 0.5F;
    config.biological_lr_modulation = 0.5F;   /* 50% biological, 50% computational */

    /* Second Messenger Cascade defaults */
    config.enable_second_messengers = true;   /* Enable by default - biological fidelity */
    config.num_neurons = 1000;                /* Default neuron count */

    /* Portia Resource Management defaults */
    config.enable_portia_integration = false; /* Disabled by default - opt-in */
    config.min_batch_size_ratio = 0.25F;      /* Minimum 25% of base batch size */
    config.allow_training_pause = true;       /* Allow pause in EMERGENCY mode */
    config.adapt_to_tier_changes = true;      /* Automatically adapt to tier changes */

    return config;
}


/* ============================================================================
 * Configuration Validation
 * ============================================================================ */

nimcp_result_t nimcp_brain_training_validate_config(
    const nimcp_brain_training_config_t* config)
{
    NIMCP_CHECK_THROW(config, NIMCP_ERROR_INVALID_PARAM, "config is NULL");
    NIMCP_CHECK_THROW(config->default_loss_type >= 0 &&
                      config->default_loss_type < NIMCP_LOSS_TYPE_COUNT,
                      NIMCP_ERROR_INVALID_PARAM, "invalid default_loss_type");
    NIMCP_CHECK_THROW(config->default_optimizer >= 0 &&
                      config->default_optimizer < NIMCP_OPTIMIZER_TYPE_COUNT,
                      NIMCP_ERROR_INVALID_PARAM, "invalid default_optimizer");
    NIMCP_CHECK_THROW(config->default_learning_rate > 0.0F &&
                      config->default_learning_rate <= 10.0F,
                      NIMCP_ERROR_INVALID_PARAM, "invalid default_learning_rate");
    NIMCP_CHECK_THROW(config->convergence_threshold > 0.0F,
                      NIMCP_ERROR_INVALID_PARAM, "invalid convergence_threshold");
    NIMCP_CHECK_THROW(config->divergence_threshold > 0.0F,
                      NIMCP_ERROR_INVALID_PARAM, "invalid divergence_threshold");

    return NIMCP_SUCCESS;
}


bool nimcp_brain_training_is_security_registered(
    const nimcp_brain_training_ctx_t* ctx)
{
    return ctx && ctx->security_registered;
}


nimcp_result_t nimcp_brain_training_get_security_ids(
    const nimcp_brain_training_ctx_t* ctx,
    uint32_t* loss_module_id,
    uint32_t* optimizer_module_id)
{
    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");

    if (!ctx->security_registered) {
        return NIMCP_NOT_FOUND;
    }

    if (loss_module_id) {
        *loss_module_id = ctx->loss_module_id;
    }
    if (optimizer_module_id) {
        *optimizer_module_id = ctx->optimizer_module_id;
    }

    return NIMCP_SUCCESS;
}


nimcp_loss_context_t* nimcp_brain_training_get_loss(
    nimcp_brain_training_ctx_t* ctx,
    uint32_t loss_id)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return NULL;
    }

    int slot = find_loss_slot_by_id(ctx, loss_id);
    if (slot < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_training_get_loss: validation failed");
        return NULL;
    }

    return ctx->loss_slots[slot].ctx;
}


uint32_t nimcp_brain_training_get_optimizer_count(
    nimcp_brain_training_ctx_t* ctx)
{
    if (!ctx) {
        return 0;
    }
    return ctx->optimizer_count;
}


nimcp_optimizer_context_t* nimcp_brain_training_get_optimizer(
    nimcp_brain_training_ctx_t* ctx,
    uint32_t optimizer_id)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return NULL;
    }

    int slot = find_optimizer_slot_by_id(ctx, optimizer_id);
    if (slot < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_training_get_optimizer: validation failed");
        return NULL;
    }

    return ctx->optimizer_slots[slot].ctx;
}


/* ============================================================================
 * Training Operations
 * ============================================================================ */

nimcp_result_t nimcp_brain_training_set_mode(
    nimcp_brain_training_ctx_t* ctx,
    nimcp_training_mode_t mode)
{
    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(mode >= NIMCP_TRAINING_MODE_TRAIN && mode <= NIMCP_TRAINING_MODE_INFERENCE,
        NIMCP_ERROR_INVALID_PARAM, "Invalid training mode");

    ctx->mode = mode;
    LOG_DEBUG("Training mode set to %s", nimcp_training_mode_name(mode));
    return NIMCP_SUCCESS;
}


nimcp_training_mode_t nimcp_brain_training_get_mode(
    const nimcp_brain_training_ctx_t* ctx)
{
    if (!ctx) {
        return NIMCP_TRAINING_MODE_INFERENCE;
    }
    return ctx->mode;
}


/* ============================================================================
 * Statistics and Monitoring
 * ============================================================================ */

nimcp_result_t nimcp_brain_training_get_stats(
    const nimcp_brain_training_ctx_t* ctx,
    nimcp_training_session_stats_t* stats)
{
    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(stats, NIMCP_ERROR_INVALID_PARAM, "stats is NULL");

    *stats = ctx->stats;

    /* Compute average loss */
    if (ctx->stats.total_batches > 0) {
        stats->avg_loss = ctx->stats.total_loss / (double)ctx->stats.total_batches;
    }

    stats->converged = ctx->converged;
    stats->diverged = ctx->diverged;

    return NIMCP_SUCCESS;
}


bool nimcp_brain_training_is_converged(const nimcp_brain_training_ctx_t* ctx)
{
    return ctx && ctx->converged;
}


bool nimcp_brain_training_is_diverged(const nimcp_brain_training_ctx_t* ctx)
{
    return ctx && ctx->diverged;
}


nimcp_lr_scheduler_ctx_t* nimcp_brain_training_get_scheduler(
    nimcp_brain_training_ctx_t* ctx,
    uint32_t scheduler_id)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return NULL;
    }

    int slot = find_scheduler_slot_by_id(ctx, scheduler_id);
    if (slot < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_training_get_scheduler: validation failed");
        return NULL;
    }

    return ctx->scheduler_slots[slot].ctx;
}


nimcp_gradient_manager_ctx_t* nimcp_brain_training_get_gradient_manager(
    nimcp_brain_training_ctx_t* ctx,
    uint32_t gradmgr_id)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return NULL;
    }

    int slot = find_gradmgr_slot_by_id(ctx, gradmgr_id);
    if (slot < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_training_get_gradient_manager: validation failed");
        return NULL;
    }

    return ctx->gradmgr_slots[slot].ctx;
}


bool nimcp_brain_training_gradients_ready(
    const nimcp_brain_training_ctx_t* ctx,
    uint32_t gradmgr_id)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_training_gradients_ready: ctx is NULL");
        return false;
    }

    /* Find slot without modifying ctx */
    for (uint32_t i = 0; i < NIMCP_TRAINING_MAX_GRADMGR_CONTEXTS; i++) {
        if (ctx->gradmgr_slots[i].active && ctx->gradmgr_slots[i].id == gradmgr_id) {
            return nimcp_gradient_accum_ready(ctx->gradmgr_slots[i].ctx);
        }
    }
    return false;
}


nimcp_result_t nimcp_brain_training_get_accumulated_gradients(
    nimcp_brain_training_ctx_t* ctx,
    uint32_t gradmgr_id,
    float* output,
    size_t count)
{
    NIMCP_CHECK_THROW(ctx && output && count > 0, NIMCP_ERROR_INVALID_PARAM,
                      "nimcp_brain_training_get_and_reset_gradients: invalid arguments");

    nimcp_gradient_manager_ctx_t* gm = nimcp_brain_training_get_gradient_manager(ctx, gradmgr_id);
    if (!gm) {
        return NIMCP_NOT_FOUND;
    }

    nimcp_result_t res = nimcp_gradient_get_accumulated(gm, output, count);
    if (res == NIMCP_SUCCESS) {
        nimcp_gradient_reset_accum(gm);
    }

    return res;
}


tpb_context_t* nimcp_brain_training_get_plasticity_bridge(
    const nimcp_brain_training_ctx_t* ctx)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return NULL;
    }
    return ctx->plasticity_bridge;
}


nimcp_result_t nimcp_brain_training_set_biological_modulation(
    nimcp_brain_training_ctx_t* ctx,
    float strength)
{
    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM,
                      "nimcp_brain_training_set_biological_modulation: ctx is NULL");

    if (strength < 0.0F) strength = 0.0F;
    if (strength > 1.0F) strength = 1.0F;

    ctx->biological_modulation_strength = strength;
    LOG_DEBUG("Biological modulation strength set to %.2f", strength);

    return NIMCP_SUCCESS;
}


float nimcp_brain_training_get_biological_modulation(
    const nimcp_brain_training_ctx_t* ctx)
{
    if (!ctx) {
        return 0.0F;
    }
    return ctx->biological_modulation_strength;
}


tcb_context_t* nimcp_brain_training_get_callbacks(const nimcp_brain_training_ctx_t* ctx)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return NULL;
    }
    return ctx->callbacks;
}


float nimcp_brain_training_get_cascade_modulation(
    const nimcp_brain_training_ctx_t* ctx,
    uint32_t neuron_id)
{
    /* Guard: Return baseline modulation if context invalid */
    if (!ctx) {
        return 1.0F;
    }

    /* Guard: Return baseline if second messengers not enabled */
    if (!ctx->second_messengers_enabled || !ctx->second_messengers) {
        return 1.0F;
    }

    /* Query plasticity modulation from second messenger system
     * Returns factor in range [0.5, 2.0] where 1.0 = baseline */
    float modulation = second_messenger_get_plasticity_modulation(
        ctx->second_messengers,
        neuron_id
    );

    /* Guard: Clamp to safe range in case of error */
    if (modulation < 0.5F) modulation = 0.5F;
    if (modulation > 2.0F) modulation = 2.0F;

    return modulation;
}


bool nimcp_brain_training_is_paused(
    const nimcp_brain_training_ctx_t* ctx)
{
    if (!ctx) {
        return false;
    }

    return ctx->training_paused;
}


size_t nimcp_brain_training_get_adjusted_batch_size(
    const nimcp_brain_training_ctx_t* ctx,
    size_t base_batch_size)
{
    /* Guard: Return base if context invalid */
    if (!ctx || !ctx->resource_aware_training) {
        return base_batch_size;
    }

    /* Guard: Return 0 if training paused */
    if (ctx->training_paused) {
        return 0;
    }

    /* Apply tier multiplier */
    size_t adjusted = (size_t)(base_batch_size * ctx->tier_batch_size_multiplier);

    /* Ensure minimum batch size (at least 1) */
    if (adjusted == 0 && base_batch_size > 0) {
        adjusted = 1;
    }

    return adjusted;
}


float nimcp_brain_training_get_adjusted_lr(
    const nimcp_brain_training_ctx_t* ctx,
    float base_lr)
{
    /* Guard: Return base if context invalid */
    if (!ctx || !ctx->resource_aware_training) {
        return base_lr;
    }

    /* Guard: Return 0 if training paused */
    if (ctx->training_paused) {
        return 0.0F;
    }

    /* Apply tier multiplier */
    float adjusted = base_lr * ctx->tier_lr_multiplier;

    return adjusted;
}
