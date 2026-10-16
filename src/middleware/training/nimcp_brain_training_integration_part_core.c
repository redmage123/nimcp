// nimcp_brain_training_integration_part_core.c - core functions
// Part of nimcp_brain_training_integration.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_brain_training_integration.c


const char* nimcp_training_event_type_name(nimcp_training_event_type_t type)
{
    if (type >= 0 && type < NIMCP_TRAINING_EVENT_COUNT) {
        return s_training_event_names[type];
    }
    return "UNKNOWN";
}


const char* nimcp_training_mode_name(nimcp_training_mode_t mode)
{
    if (mode >= 0 && mode <= NIMCP_TRAINING_MODE_INFERENCE) {
        return s_training_mode_names[mode];
    }
    return "UNKNOWN";
}


/* ============================================================================
 * Security Registration
 * ============================================================================ */

nimcp_result_t nimcp_brain_training_register_security(
    nimcp_brain_training_ctx_t* ctx,
    nimcp_sec_integration_t* security_ctx)
{
    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(security_ctx, NIMCP_ERROR_INVALID_PARAM, "security_ctx is NULL");

    if (ctx->security_registered) {
        return NIMCP_SUCCESS; /* Already registered */
    }

    nimcp_result_t res;

    /* Register loss functions module */
    res = nimcp_sec_register_module(
        security_ctx,
        "training_loss_functions",
        NIMCP_SEC_CAT_PLASTICITY,
        &ctx->loss_module_id
    );
    if (res != NIMCP_SUCCESS) {
        LOG_WARNING("Failed to register loss functions with security: %d", res);
        return res;
    }

    /* Register optimizers module */
    res = nimcp_sec_register_module(
        security_ctx,
        "training_optimizers",
        NIMCP_SEC_CAT_PLASTICITY,
        &ctx->optimizer_module_id
    );
    if (res != NIMCP_SUCCESS) {
        LOG_WARNING("Failed to register optimizers with security: %d", res);
        /* Unregister loss module */
        nimcp_sec_unregister_module(security_ctx, ctx->loss_module_id);
        return res;
    }

    ctx->security_ctx = security_ctx;
    ctx->security_registered = true;

    LOG_INFO("Training modules registered with security (loss_id=%u, opt_id=%u)",
             ctx->loss_module_id, ctx->optimizer_module_id);

    return NIMCP_SUCCESS;
}


nimcp_result_t nimcp_brain_training_unregister_security(
    nimcp_brain_training_ctx_t* ctx)
{
    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(ctx->security_registered, NIMCP_ERROR_INVALID_PARAM, "not registered");
    NIMCP_CHECK_THROW(ctx->security_ctx, NIMCP_ERROR_INVALID_PARAM, "security_ctx is NULL");

    nimcp_sec_unregister_module(ctx->security_ctx, ctx->loss_module_id);
    nimcp_sec_unregister_module(ctx->security_ctx, ctx->optimizer_module_id);

    ctx->security_registered = false;
    LOG_INFO("Training modules unregistered from security");

    return NIMCP_SUCCESS;
}


nimcp_result_t nimcp_brain_training_compute_loss(
    nimcp_brain_training_ctx_t* ctx,
    uint32_t loss_id,
    const float* predictions,
    const float* targets,
    size_t batch_size,
    size_t output_size,
    float* loss_value,
    float* gradients)
{
    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(predictions, NIMCP_ERROR_INVALID_PARAM, "predictions is NULL");
    NIMCP_CHECK_THROW(targets, NIMCP_ERROR_INVALID_PARAM, "targets is NULL");
    NIMCP_CHECK_THROW(loss_value, NIMCP_ERROR_INVALID_PARAM, "loss_value is NULL");

    nimcp_loss_context_t* loss_ctx = nimcp_brain_training_get_loss(ctx, loss_id);
    if (!loss_ctx) {
        return NIMCP_NOT_FOUND;
    }

    uint64_t start_time = (nimcp_time_get_us() * 1000);

    nimcp_result_t res;

    if (gradients && ctx->mode == NIMCP_TRAINING_MODE_TRAIN) {
        /* Forward + backward pass */
        nimcp_loss_result_t result = {0};
        res = nimcp_loss_forward_backward(
            loss_ctx, predictions, targets,
            batch_size, output_size, &result
        );
        if (res == NIMCP_SUCCESS) {
            *loss_value = result.loss_value;
            if (result.gradients && result.gradient_count > 0) {
                memcpy(gradients, result.gradients,
                       result.gradient_count * sizeof(float));
            }
            nimcp_loss_result_free(&result);
        }
    } else {
        /* Forward pass only */
        nimcp_loss_result_t result = {0};
        res = nimcp_loss_forward(
            loss_ctx, predictions, targets,
            batch_size, output_size, &result
        );
        if (res == NIMCP_SUCCESS) {
            *loss_value = result.loss_value;
            nimcp_loss_result_free(&result);
        }
    }

    if (res != NIMCP_SUCCESS) {
        if (ctx->security_registered) {
            NIMCP_SEC_FAILURE(ctx->security_ctx, ctx->loss_module_id);
        }
        return res;
    }

    /* Update statistics */
    ctx->stats.total_loss += *loss_value;
    if (*loss_value < ctx->stats.min_loss) {
        ctx->stats.min_loss = *loss_value;
    }
    if (*loss_value > ctx->stats.max_loss) {
        ctx->stats.max_loss = *loss_value;
    }
    ctx->stats.total_samples += batch_size;
    ctx->stats.training_time_ns += ((nimcp_time_get_us() * 1000) - start_time);

    /* Check for divergence */
    if (*loss_value > ctx->config.divergence_threshold) {
        ctx->diverged = true;

        nimcp_training_event_t event = {0};
        event.type = NIMCP_TRAINING_EVENT_DIVERGENCE;
        event.timestamp = (nimcp_time_get_us() * 1000);
        event.loss_value = *loss_value;
        nimcp_brain_training_emit_event(ctx, &event);

        LOG_WARNING("Training diverged: loss = %f", *loss_value);
    }

    /* Emit loss computed event */
    nimcp_training_event_t event = {0};
    event.type = NIMCP_TRAINING_EVENT_LOSS_COMPUTED;
    event.timestamp = (nimcp_time_get_us() * 1000);
    event.epoch = ctx->current_epoch;
    event.batch = ctx->current_batch;
    event.loss_value = *loss_value;
    nimcp_brain_training_emit_event(ctx, &event);

    if (ctx->security_registered) {
        NIMCP_SEC_SUCCESS(ctx->security_ctx, ctx->loss_module_id);
    }

    return NIMCP_SUCCESS;
}


nimcp_result_t nimcp_brain_training_optimize(
    nimcp_brain_training_ctx_t* ctx,
    uint32_t optimizer_id,
    float* params,
    const float* gradients,
    size_t count)
{
    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(params, NIMCP_ERROR_INVALID_PARAM, "params is NULL");
    NIMCP_CHECK_THROW(gradients, NIMCP_ERROR_INVALID_PARAM, "gradients is NULL");
    NIMCP_CHECK_THROW(count > 0, NIMCP_ERROR_INVALID_PARAM, "count is 0");

    if (ctx->mode != NIMCP_TRAINING_MODE_TRAIN) {
        return NIMCP_SUCCESS; /* Skip optimization in eval/inference mode */
    }

    nimcp_optimizer_context_t* opt_ctx = nimcp_brain_training_get_optimizer(ctx, optimizer_id);
    if (!opt_ctx) {
        return NIMCP_NOT_FOUND;
    }

    uint64_t start_time = (nimcp_time_get_us() * 1000);

    nimcp_result_t res = nimcp_optimizer_step(opt_ctx, params, gradients, count);

    if (res != NIMCP_SUCCESS) {
        if (ctx->security_registered) {
            NIMCP_SEC_FAILURE(ctx->security_ctx, ctx->optimizer_module_id);
        }
        return res;
    }

    /* Update statistics */
    ctx->stats.weight_updates++;
    ctx->stats.final_lr = nimcp_optimizer_get_lr(opt_ctx);
    ctx->stats.training_time_ns += ((nimcp_time_get_us() * 1000) - start_time);

    /* Emit weights updated event */
    nimcp_training_event_t event = {0};
    event.type = NIMCP_TRAINING_EVENT_WEIGHTS_UPDATED;
    event.timestamp = (nimcp_time_get_us() * 1000);
    event.epoch = ctx->current_epoch;
    event.batch = ctx->current_batch;
    event.learning_rate = ctx->stats.final_lr;
    event.gradient_norm = nimcp_optimizer_gradient_norm(gradients, count);
    nimcp_brain_training_emit_event(ctx, &event);

    if (ctx->security_registered) {
        NIMCP_SEC_SUCCESS(ctx->security_ctx, ctx->optimizer_module_id);
    }

    return NIMCP_SUCCESS;
}


/* ============================================================================
 * Event Bus Integration
 * ============================================================================ */

nimcp_result_t nimcp_brain_training_emit_event(
    nimcp_brain_training_ctx_t* ctx,
    const nimcp_training_event_t* event)
{
    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(event, NIMCP_ERROR_INVALID_PARAM, "event is NULL");

    /* Call registered callback */
    if (ctx->event_callback) {
        ctx->event_callback(event, ctx->callback_user_data);
    }

    return NIMCP_SUCCESS;
}


nimcp_result_t nimcp_brain_training_register_callback(
    nimcp_brain_training_ctx_t* ctx,
    nimcp_training_event_callback_t callback,
    void* user_data)
{
    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");

    ctx->event_callback = callback;
    ctx->callback_user_data = user_data;

    return NIMCP_SUCCESS;
}


nimcp_result_t nimcp_brain_training_accumulate_gradients(
    nimcp_brain_training_ctx_t* ctx,
    uint32_t gradmgr_id,
    const float* gradients,
    size_t count)
{
    NIMCP_CHECK_THROW(ctx && gradients && count > 0, NIMCP_ERROR_INVALID_PARAM,
                      "nimcp_brain_training_accumulate_gradients: invalid arguments");

    nimcp_gradient_manager_ctx_t* gm = nimcp_brain_training_get_gradient_manager(ctx, gradmgr_id);
    if (!gm) {
        return NIMCP_NOT_FOUND;
    }

    nimcp_result_t res = nimcp_gradient_accumulate(gm, gradients, count);
    if (res == NIMCP_SUCCESS) {
        ctx->stats.grad_accum_steps++;

        /* Emit accumulation event */
        nimcp_training_event_t event = {0};
        event.type = NIMCP_TRAINING_EVENT_GRAD_ACCUMULATED;
        event.timestamp = (nimcp_time_get_us() * 1000);
        event.accum_step = nimcp_gradient_get_accum_step(gm);
        nimcp_brain_training_emit_event(ctx, &event);
    }

    return res;
}


/* ============================================================================
 * Regularization Operations
 * ============================================================================ */

float nimcp_brain_training_apply_regularization(
    nimcp_brain_training_ctx_t* ctx,
    const float* weights,
    float* gradients,
    size_t count,
    nimcp_reg_type_t reg_type,
    float lambda)
{
    if (!ctx || !weights || !gradients || count == 0) {
        return 0.0F;
    }

    float reg_loss = 0.0F;

    switch (reg_type) {
        case NIMCP_REG_L1:
            reg_loss = nimcp_l1_loss(weights, count, lambda);
            nimcp_l1_gradient(weights, gradients, count, lambda);
            break;
        case NIMCP_REG_L2:
            reg_loss = nimcp_l2_loss(weights, count, lambda);
            nimcp_l2_gradient(weights, gradients, count, lambda);
            break;
        case NIMCP_REG_ELASTIC_NET: {
            /* Elastic net = alpha*L1 + (1-alpha)*L2, use alpha=0.5 */
            float alpha = 0.5F;
            reg_loss = nimcp_elastic_net_loss(weights, count, lambda, alpha);
            /* Apply both L1 and L2 gradients with appropriate weights */
            nimcp_l1_gradient(weights, gradients, count, lambda * alpha);
            nimcp_l2_gradient(weights, gradients, count, lambda * (1.0F - alpha));
            break;
        }
        default:
            break;
    }

    ctx->stats.total_reg_loss += reg_loss;

    /* Emit regularization event */
    nimcp_training_event_t event = {0};
    event.type = NIMCP_TRAINING_EVENT_REGULARIZED;
    event.timestamp = (nimcp_time_get_us() * 1000);
    event.regularization_loss = reg_loss;
    nimcp_brain_training_emit_event(ctx, &event);

    return reg_loss;
}


float nimcp_brain_training_clip_gradients(
    nimcp_brain_training_ctx_t* ctx,
    float* gradients,
    size_t count,
    nimcp_clip_mode_t mode,
    float threshold)
{
    if (!ctx || !gradients || count == 0) {
        return 1.0F;
    }

    float original_norm = nimcp_gradient_norm(gradients, count);
    float clip_ratio = 1.0F;

    switch (mode) {
        case NIMCP_CLIP_BY_VALUE:
            nimcp_gradient_clip_by_value(gradients, count, threshold);
            break;
        case NIMCP_CLIP_BY_NORM:
        case NIMCP_CLIP_BY_GLOBAL_NORM:
            /* Both norm-based clipping use the same function */
            clip_ratio = nimcp_gradient_clip_by_norm(gradients, count, threshold);
            break;
        default:
            return 1.0F;
    }

    /* For value clipping, compute ratio from norms */
    if (mode == NIMCP_CLIP_BY_VALUE) {
        float clipped_norm = nimcp_gradient_norm(gradients, count);
        if (clipped_norm > 0.0F) {
            clip_ratio = original_norm / clipped_norm;
        }
    }

    if (clip_ratio > 1.0F) {
        ctx->stats.gradient_clips++;

        /* Emit clip event */
        nimcp_training_event_t event = {0};
        event.type = NIMCP_TRAINING_EVENT_GRAD_CLIPPED;
        event.timestamp = (nimcp_time_get_us() * 1000);
        event.clip_ratio = clip_ratio;
        event.gradient_norm = original_norm;
        nimcp_brain_training_emit_event(ctx, &event);
    }

    return clip_ratio;
}


uint64_t nimcp_brain_training_apply_dropout(
    nimcp_brain_training_ctx_t* ctx,
    const float* input,
    float* output,
    bool* mask,
    size_t count,
    float dropout_rate)
{
    if (!ctx || !input || !output || count == 0) {
        return 0;
    }

    /* Only apply dropout in training mode */
    if (ctx->mode != NIMCP_TRAINING_MODE_TRAIN) {
        if (input != output) {
            memcpy(output, input, count * sizeof(float));
        }
        return 0;
    }

    /* Create temporary dropout context */
    nimcp_dropout_config_t cfg = nimcp_dropout_default_config(dropout_rate);
    nimcp_dropout_ctx_t* dropout_ctx = nimcp_dropout_create(&cfg);
    if (!dropout_ctx) {
        if (input != output) {
            memcpy(output, input, count * sizeof(float));
        }
        return 0;
    }

    /* Copy input to output first (dropout modifies in place) */
    if (input != output) {
        memcpy(output, input, count * sizeof(float));
    }

    /* Apply dropout */
    uint8_t* dropout_mask = mask ? (uint8_t*)nimcp_malloc(count) : NULL;
    nimcp_result_t res = nimcp_dropout_forward(dropout_ctx, output, count, dropout_mask);

    /* Count dropped elements */
    uint64_t dropped = 0;
    if (res == NIMCP_SUCCESS && dropout_mask) {
        for (size_t i = 0; i < count; i++) {
            if (dropout_mask[i] == 0) dropped++;
        }
        /* Convert mask to bool array if requested */
        if (mask) {
            for (size_t i = 0; i < count; i++) {
                mask[i] = (dropout_mask[i] != 0);
            }
        }
    }

    if (dropout_mask) {
        nimcp_free(dropout_mask);
    }
    nimcp_dropout_destroy(dropout_ctx);

    ctx->stats.dropout_masks++;
    return dropped;
}


bool nimcp_brain_training_check_early_stop(
    nimcp_brain_training_ctx_t* ctx,
    float current_loss)
{
    if (!ctx || !ctx->config.enable_early_stopping) {
        return false;
    }

    if (ctx->early_stopped) {
        return true;  /* Already stopped */
    }

    /* Check for improvement */
    float improvement = ctx->best_loss - current_loss;

    if (improvement > ctx->config.early_stop_min_delta) {
        /* Improvement found */
        ctx->best_loss = current_loss;
        ctx->early_stop_wait = 0;
    } else {
        /* No improvement */
        ctx->early_stop_wait++;
    }

    ctx->stats.early_stop_wait_count = ctx->early_stop_wait;
    ctx->stats.best_loss = ctx->best_loss;

    if (ctx->early_stop_wait >= ctx->config.early_stop_patience) {
        ctx->early_stopped = true;
        ctx->stats.early_stopped = true;

        /* Emit early stop event */
        nimcp_training_event_t event = {0};
        event.type = NIMCP_TRAINING_EVENT_EARLY_STOP;
        event.timestamp = (nimcp_time_get_us() * 1000);
        event.loss_value = current_loss;
        nimcp_brain_training_emit_event(ctx, &event);

        LOG_INFO("Early stopping triggered at loss = %f", current_loss);
        return true;
    }

    return false;
}


/* ============================================================================
 * Plasticity Bridge Integration (Phase TPB-1)
 * ============================================================================ */

nimcp_result_t nimcp_brain_training_connect_plasticity_bridge(
    nimcp_brain_training_ctx_t* ctx,
    tpb_context_t* bridge)
{
    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM,
                      "nimcp_brain_training_connect_plasticity_bridge: ctx is NULL");

    ctx->plasticity_bridge = bridge;

    if (bridge) {
        LOG_INFO("Plasticity bridge connected to training integration");

        /* If callbacks exist, wire them to plasticity bridge */
        if (ctx->callbacks) {
            tpb_connect_callbacks(bridge, ctx->callbacks);
            tpb_register_plasticity_callbacks(bridge);
            LOG_INFO("Auto-wired callbacks to plasticity bridge");
        }
    } else {
        LOG_INFO("Plasticity bridge disconnected from training integration");
    }

    return NIMCP_SUCCESS;
}


/* ============================================================================
 * Second Messenger Cascade Integration
 * ============================================================================ */

nimcp_result_t nimcp_brain_training_connect_second_messengers(
    nimcp_brain_training_ctx_t* ctx,
    void* second_messengers)
{
    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM,
                      "nimcp_brain_training_connect_second_messengers: ctx is NULL");

    /* Guard: NULL is allowed for disconnection */
    ctx->second_messengers = (second_messenger_system_t*)second_messengers;

    if (second_messengers) {
        ctx->second_messengers_enabled = true;
        LOG_INFO("Second messenger cascade system connected to training");
    } else {
        ctx->second_messengers_enabled = false;
        LOG_INFO("Second messenger cascade system disconnected from training");
    }

    return NIMCP_SUCCESS;
}


nimcp_result_t nimcp_brain_training_connect_portia(
    nimcp_brain_training_ctx_t* ctx,
    void* portia_ctx)
{
    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM,
                      "nimcp_brain_training_connect_portia: ctx is NULL");

    /* Allow NULL to disconnect */
    ctx->portia_context = portia_ctx;

    if (portia_ctx) {
        /* Query current tier from Portia */
        ctx->current_tier = portia_get_current_tier();

        /* Calculate initial multipliers */
        calculate_tier_multipliers(
            ctx->current_tier,
            &ctx->tier_batch_size_multiplier,
            &ctx->tier_lr_multiplier
        );

        ctx->resource_aware_training = true;
        LOG_INFO("Portia connected to training system (tier=%s)",
                 platform_tier_get_name(ctx->current_tier));
    } else {
        /* Reset to defaults when disconnecting */
        ctx->resource_aware_training = false;
        ctx->tier_batch_size_multiplier = 1.0F;
        ctx->tier_lr_multiplier = 1.0F;
        LOG_INFO("Portia disconnected from training system");
    }

    return NIMCP_SUCCESS;
}


nimcp_result_t nimcp_brain_training_on_tier_change(
    nimcp_brain_training_ctx_t* ctx,
    platform_tier_t new_tier)
{
    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM,
                      "nimcp_brain_training_on_tier_change: ctx is NULL");

    /* Guard: Check if resource-aware training enabled */
    if (!ctx->resource_aware_training) {
        return NIMCP_SUCCESS;  /* Silently ignore if disabled */
    }

    platform_tier_t old_tier = ctx->current_tier;
    ctx->current_tier = new_tier;

    /* Calculate new multipliers */
    calculate_tier_multipliers(
        new_tier,
        &ctx->tier_batch_size_multiplier,
        &ctx->tier_lr_multiplier
    );

    /* Log tier change */
    LOG_INFO("Training tier change: %s -> %s (batch=%.0f%%, lr=%.0f%%)",
             platform_tier_get_name(old_tier),
             platform_tier_get_name(new_tier),
             ctx->tier_batch_size_multiplier * 100.0F,
             ctx->tier_lr_multiplier * 100.0F);

    /* Handle EMERGENCY tier: pause training if allowed */
    if (new_tier == PLATFORM_TIER_MINIMAL && !ctx->training_paused) {
        ctx->training_paused = true;
        LOG_WARNING("Training paused due to EMERGENCY tier");

        /* TODO: Save checkpoint here in production */
    }

    /* Resume training if returning from MINIMAL tier */
    if (old_tier == PLATFORM_TIER_MINIMAL && new_tier != PLATFORM_TIER_MINIMAL) {
        if (ctx->training_paused) {
            ctx->training_paused = false;
            LOG_INFO("Training resumed after tier upgrade");
        }
    }

    return NIMCP_SUCCESS;
}


nimcp_result_t nimcp_brain_training_on_degradation_event(
    nimcp_brain_training_ctx_t* ctx,
    uint32_t degradation_level)
{
    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM,
                      "nimcp_brain_training_on_degradation_change: ctx is NULL");

    /* Guard: Check if resource-aware training enabled */
    if (!ctx->resource_aware_training) {
        return NIMCP_SUCCESS;
    }

    uint32_t old_level = ctx->degradation_level;
    ctx->degradation_level = degradation_level;

    LOG_INFO("Training degradation level change: %u -> %u",
             old_level, degradation_level);

    /* Handle critical degradation levels */
    if (degradation_level >= 4) {  /* DEGRADATION_LEVEL_CRITICAL */
        if (!ctx->training_paused) {
            ctx->training_paused = true;
            LOG_WARNING("Training paused due to CRITICAL degradation");
        }
    } else if (degradation_level >= 3) {  /* DEGRADATION_LEVEL_SEVERE */
        /* Further reduce batch size beyond tier multiplier */
        ctx->tier_batch_size_multiplier *= 0.5F;
        LOG_INFO("Batch size reduced further due to SEVERE degradation");
    }

    /* Resume if degradation improves */
    if (old_level >= 4 && degradation_level < 4 && ctx->training_paused) {
        ctx->training_paused = false;
        /* Restore tier-based multiplier */
        calculate_tier_multipliers(
            ctx->current_tier,
            &ctx->tier_batch_size_multiplier,
            &ctx->tier_lr_multiplier
        );
        LOG_INFO("Training resumed after degradation improvement");
    }

    return NIMCP_SUCCESS;
}


nimcp_result_t nimcp_brain_training_resume(
    nimcp_brain_training_ctx_t* ctx)
{
    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM,
                      "nimcp_brain_training_resume: ctx is NULL");

    if (ctx->training_paused) {
        ctx->training_paused = false;
        LOG_INFO("Training manually resumed");
    }

    return NIMCP_SUCCESS;
}


nimcp_result_t nimcp_brain_training_request_resources(
    nimcp_brain_training_ctx_t* ctx,
    size_t batch_size,
    size_t param_count)
{
    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM,
                      "nimcp_brain_training_request_resources: ctx is NULL");

    /* Guard: Check if bio-async enabled */
    if (!ctx->bio_async_enabled) {
        return NIMCP_SUCCESS;  /* Silently skip if bio-async disabled */
    }

    /* Guard: Check if Portia connected */
    if (!ctx->portia_context || !ctx->resource_aware_training) {
        return NIMCP_SUCCESS;  /* Silently skip if Portia not connected */
    }

    /* Guard: Check if bio context available */
    if (!ctx->bio_ctx) {
        return NIMCP_SUCCESS;  /* Silently skip if no bio context */
    }

    /* Create resource request message */
    struct {
        bio_message_header_t header;
        uint64_t payload[4];
    } msg = {
        .header = {
            .type = BIO_MSG_TRAINING_RESOURCE_REQUEST,
            .sequence_id = 0,  /* Will be set by router */
            .source_module = BIO_MODULE_TRAINING,
            .target_module = BIO_MODULE_PORTIA,
            .timestamp_us = 0,  /* Will be set by router */
            .channel = BIO_CHANNEL_SEROTONIN,
            .payload_size = sizeof(uint64_t) * 4,
            .flags = 0
        },
        .payload = {
            (uint64_t)batch_size,
            (uint64_t)param_count,
            (uint64_t)ctx->current_tier,
            0  /* Reserved */
        }
    };

    /* Send via bio-router */
    nimcp_error_t result = bio_router_send(
        ctx->bio_ctx,
        &msg,
        sizeof(msg),
        100  /* 100ms timeout */
    );

    if (result != NIMCP_SUCCESS) {
        LOG_WARNING("Failed to send training resource request to Portia");
        return (nimcp_result_t)result;
    }

    return NIMCP_SUCCESS;
}
