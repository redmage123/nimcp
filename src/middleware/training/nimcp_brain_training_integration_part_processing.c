// nimcp_brain_training_integration_part_processing.c - processing functions
// Part of nimcp_brain_training_integration.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_brain_training_integration.c


nimcp_result_t nimcp_brain_training_step(
    nimcp_brain_training_ctx_t* ctx,
    uint32_t loss_id,
    uint32_t optimizer_id,
    float* params,
    const float* predictions,
    const float* targets,
    size_t batch_size,
    size_t output_size,
    size_t param_count,
    float* loss_value)
{
    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(params, NIMCP_ERROR_INVALID_PARAM, "params is NULL");
    NIMCP_CHECK_THROW(predictions, NIMCP_ERROR_INVALID_PARAM, "predictions is NULL");
    NIMCP_CHECK_THROW(targets, NIMCP_ERROR_INVALID_PARAM, "targets is NULL");
    NIMCP_CHECK_THROW(loss_value, NIMCP_ERROR_INVALID_PARAM, "loss_value is NULL");
    NIMCP_CHECK_THROW(batch_size > 0, NIMCP_ERROR_INVALID_PARAM, "batch_size is 0");
    NIMCP_CHECK_THROW(output_size > 0, NIMCP_ERROR_INVALID_PARAM, "output_size is 0");
    NIMCP_CHECK_THROW(param_count > 0, NIMCP_ERROR_INVALID_PARAM, "param_count is 0");

    /* Phase TPB-1: If plasticity bridge is connected and biological modulation > 0,
     * automatically route through biological training step */
    if (ctx->plasticity_bridge && ctx->biological_modulation_strength > 0.0F) {
        return nimcp_brain_training_step_biological(
            ctx, loss_id, optimizer_id, params,
            predictions, targets, batch_size, output_size,
            param_count, 0 /* default region */, loss_value
        );
    }

    /* Calculate gradient buffer size (batch_size * output_size for loss gradients) */
    size_t gradient_count = batch_size * output_size;
    size_t gradient_size = gradient_count * sizeof(float);

    /* Allocate temporary gradient buffer using unified memory if available */
    float* gradients = NULL;
    unified_mem_handle_t grad_handle = NULL;

    if (ctx->memory_mgr) {
        /* Use unified memory manager */
        unified_mem_request_t req = {
            .size = gradient_size,
            .initial_data = NULL,
            .strategy = UNIFIED_STRATEGY_POOL_DIRECT,
            .enable_cow = false,
            .alignment = 0
        };
        grad_handle = unified_mem_alloc(ctx->memory_mgr, &req);
        if (grad_handle) {
            gradients = (float*)unified_mem_write(grad_handle);
        }
    }

    /* Fallback to malloc if unified memory not available or failed */
    if (!gradients) {
        gradients = (float*)nimcp_malloc(gradient_size);
        NIMCP_CHECK_THROW(gradients, NIMCP_ERROR_MEMORY, "Failed to allocate gradient buffer");
    }

    /* Zero the gradient buffer */
    memset(gradients, 0, gradient_size);

    /* Compute loss and gradients */
    nimcp_result_t res = nimcp_brain_training_compute_loss(
        ctx, loss_id, predictions, targets,
        batch_size, output_size, loss_value, gradients
    );

    if (res != NIMCP_SUCCESS) {
        if (grad_handle) {
            unified_mem_free(grad_handle);
        } else {
            nimcp_free(gradients);
        }
        return res;
    }

    /* Update weights - use gradients for first param_count elements */
    /* Note: In practice, param_count often equals batch_size * output_size for simple cases */
    size_t update_count = (param_count < gradient_count) ? param_count : gradient_count;
    res = nimcp_brain_training_optimize(
        ctx, optimizer_id, params, gradients, update_count
    );

    /* Free gradient buffer */
    if (grad_handle) {
        unified_mem_free(grad_handle);
    } else {
        nimcp_free(gradients);
    }

    /* Check for convergence */
    float loss_change = fabsf(*loss_value - ctx->last_loss);
    if (loss_change < ctx->config.convergence_threshold) {
        ctx->stable_loss_count++;
        if (ctx->stable_loss_count >= 10) {
            ctx->converged = true;

            nimcp_training_event_t event = {0};
            event.type = NIMCP_TRAINING_EVENT_CONVERGENCE;
            event.timestamp = (nimcp_time_get_us() * 1000);
            event.loss_value = *loss_value;
            nimcp_brain_training_emit_event(ctx, &event);

            LOG_INFO("Training converged at loss = %f", *loss_value);
        }
    } else {
        ctx->stable_loss_count = 0;
    }
    ctx->last_loss = *loss_value;

    ctx->stats.total_batches++;

    return res;
}


void nimcp_brain_training_update_stats(
    nimcp_brain_training_ctx_t* ctx,
    uint32_t samples_processed,
    float loss_value)
{
    if (!ctx) {
        return;
    }

    ctx->stats.total_samples += samples_processed;
    ctx->stats.total_batches += 1;
    ctx->stats.total_loss += (double)loss_value;

    /* Update min/max loss */
    if (loss_value < ctx->stats.min_loss) {
        ctx->stats.min_loss = loss_value;
    }
    if (loss_value > ctx->stats.max_loss) {
        ctx->stats.max_loss = loss_value;
    }
}


float nimcp_brain_training_step_scheduler(
    nimcp_brain_training_ctx_t* ctx,
    uint32_t scheduler_id,
    uint32_t optimizer_id)
{
    if (!ctx) {
        return 0.0F;
    }

    nimcp_lr_scheduler_ctx_t* sched = nimcp_brain_training_get_scheduler(ctx, scheduler_id);
    if (!sched) {
        return 0.0F;
    }

    float new_lr = nimcp_lr_scheduler_step(sched);

    /* Update optimizer learning rate */
    nimcp_optimizer_context_t* opt = nimcp_brain_training_get_optimizer(ctx, optimizer_id);
    if (opt) {
        nimcp_optimizer_set_lr(opt, new_lr);
    }

    /* Update stats */
    ctx->stats.lr_steps++;
    ctx->stats.final_lr = new_lr;
    if (new_lr < ctx->stats.lr_min) ctx->stats.lr_min = new_lr;
    if (new_lr > ctx->stats.lr_max) ctx->stats.lr_max = new_lr;

    /* Emit LR changed event */
    nimcp_training_event_t event = {0};
    event.type = NIMCP_TRAINING_EVENT_LR_CHANGED;
    event.timestamp = (nimcp_time_get_us() * 1000);
    event.learning_rate = new_lr;
    nimcp_brain_training_emit_event(ctx, &event);

    return new_lr;
}


float nimcp_brain_training_step_scheduler_metric(
    nimcp_brain_training_ctx_t* ctx,
    uint32_t scheduler_id,
    uint32_t optimizer_id,
    float metric)
{
    if (!ctx) {
        return 0.0F;
    }

    nimcp_lr_scheduler_ctx_t* sched = nimcp_brain_training_get_scheduler(ctx, scheduler_id);
    if (!sched) {
        return 0.0F;
    }

    float new_lr = nimcp_lr_scheduler_step_metric(sched, metric);

    /* Update optimizer learning rate */
    nimcp_optimizer_context_t* opt = nimcp_brain_training_get_optimizer(ctx, optimizer_id);
    if (opt) {
        nimcp_optimizer_set_lr(opt, new_lr);
    }

    /* Update stats */
    ctx->stats.lr_steps++;
    ctx->stats.final_lr = new_lr;
    if (new_lr < ctx->stats.lr_min) ctx->stats.lr_min = new_lr;
    if (new_lr > ctx->stats.lr_max) ctx->stats.lr_max = new_lr;

    return new_lr;
}


/* ============================================================================
 * Enhanced Training Operations
 * ============================================================================ */

nimcp_result_t nimcp_brain_training_step_full(
    nimcp_brain_training_ctx_t* ctx,
    uint32_t loss_id,
    uint32_t optimizer_id,
    uint32_t scheduler_id,
    uint32_t gradmgr_id,
    float* params,
    const float* predictions,
    const float* targets,
    size_t batch_size,
    size_t output_size,
    size_t param_count,
    float* loss_value)
{
    NIMCP_CHECK_THROW(ctx && params && predictions && targets && loss_value,
                      NIMCP_ERROR_INVALID_PARAM,
                      "nimcp_brain_training_step_full: NULL argument");
    NIMCP_CHECK_THROW(batch_size > 0 && output_size > 0 && param_count > 0,
                      NIMCP_ERROR_INVALID_PARAM,
                      "nimcp_brain_training_step_full: size is 0");

    /* Allocate gradient buffer */
    size_t gradient_count = batch_size * output_size;
    float* gradients = (float*)nimcp_malloc(gradient_count * sizeof(float));
    NIMCP_CHECK_THROW(gradients, NIMCP_ERROR_MEMORY,
                      "nimcp_brain_training_step_full: failed to allocate gradients");
    memset(gradients, 0, gradient_count * sizeof(float));

    /* Step 1: Compute loss and gradients */
    nimcp_result_t res = nimcp_brain_training_compute_loss(
        ctx, loss_id, predictions, targets,
        batch_size, output_size, loss_value, gradients
    );

    if (res != NIMCP_SUCCESS) {
        nimcp_free(gradients);
        return res;
    }

    /* Phase TPB-1: Report loss to plasticity bridge for RPE computation */
    if (ctx->plasticity_bridge && ctx->biological_modulation_strength > 0.0F) {
        float rpe = 0.0F;
        if (tpb_report_loss(ctx->plasticity_bridge, *loss_value, &rpe) == NIMCP_SUCCESS) {
            ctx->stats.rpe_computations++;

            /* Get current dopamine level for tracking */
            float da_level = 0.5F, ach_level, ht5_level, ne_level;
            if (tpb_get_neuromod_levels(ctx->plasticity_bridge, &da_level, &ach_level,
                                         &ht5_level, &ne_level) == NIMCP_SUCCESS) {
                ctx->cumulative_da += da_level;
            }
        }
    }

    /* Step 2: Check gradient health */
    if (ctx->config.enable_gradient_health_check) {
        nimcp_grad_health_t health = nimcp_gradient_check_health(gradients, gradient_count);
        if (health == NIMCP_GRAD_HAS_NAN) {
            ctx->stats.grad_nan_count++;
            nimcp_free(gradients);
            return NIMCP_TRAINING_ERROR_GRAD_NAN;  /* Skip this step */
        }
        if (health == NIMCP_GRAD_HAS_INF) {
            ctx->stats.grad_inf_count++;
            /* Sanitize infinity values */
            nimcp_gradient_sanitize(gradients, gradient_count, 0.0F);
        }
    }

    /* Step 3: Gradient accumulation (if using gradient manager) */
    if (gradmgr_id > 0) {
        nimcp_gradient_manager_ctx_t* gm = nimcp_brain_training_get_gradient_manager(ctx, gradmgr_id);
        if (gm) {
            res = nimcp_gradient_accumulate(gm, gradients, gradient_count);
            ctx->stats.grad_accum_steps++;

            if (!nimcp_gradient_accum_ready(gm)) {
                /* Not ready to apply yet, skip optimization */
                nimcp_free(gradients);
                return NIMCP_SUCCESS;
            }

            /* Get accumulated gradients */
            res = nimcp_gradient_get_accumulated(gm, gradients, gradient_count);
            nimcp_gradient_reset_accum(gm);
        }
    }

    /* Step 4: Gradient clipping (if enabled) */
    if (ctx->config.enable_gradient_clipping) {
        nimcp_brain_training_clip_gradients(
            ctx, gradients, gradient_count,
            ctx->config.default_clip_mode,
            ctx->config.default_clip_value
        );
    }

    /* Step 5: Add regularization gradients (if enabled) */
    if (ctx->config.enable_regularization) {
        float reg_loss = nimcp_brain_training_apply_regularization(
            ctx, params, gradients,
            (param_count < gradient_count) ? param_count : gradient_count,
            ctx->config.default_reg_type,
            ctx->config.default_reg_lambda
        );
        *loss_value += reg_loss;
    }

    /* Phase TPB-1: Apply biological learning rate modulation before optimization */
    float original_lr = 0.0F;
    nimcp_optimizer_context_t* opt = NULL;
    if (ctx->plasticity_bridge && ctx->biological_modulation_strength > 0.0F) {
        opt = nimcp_brain_training_get_optimizer(ctx, optimizer_id);
        if (opt) {
            original_lr = nimcp_optimizer_get_lr(opt);
            float bio_lr = original_lr;
            if (tpb_get_modulated_lr(ctx->plasticity_bridge, 0, original_lr, &bio_lr) == NIMCP_SUCCESS) {
                float blend = ctx->biological_modulation_strength;
                float effective_lr = (1.0F - blend) * original_lr + blend * bio_lr;
                nimcp_optimizer_set_lr(opt, effective_lr);
                ctx->cumulative_lr_mod += (effective_lr / original_lr);
                ctx->bio_update_count++;
            }
        }
    }

    /* Step 6: Optimization step */
    size_t update_count = (param_count < gradient_count) ? param_count : gradient_count;
    res = nimcp_brain_training_optimize(ctx, optimizer_id, params, gradients, update_count);

    /* Restore original learning rate if modified */
    if (opt && original_lr > 0.0F) {
        nimcp_optimizer_set_lr(opt, original_lr);
    }

    nimcp_free(gradients);

    if (res != NIMCP_SUCCESS) {
        return res;
    }

    /* Step 7: Learning rate scheduling (if enabled) */
    if (scheduler_id > 0 && ctx->config.enable_lr_scheduler) {
        nimcp_brain_training_step_scheduler(ctx, scheduler_id, optimizer_id);
    }

    /* Step 8: Check early stopping */
    if (ctx->config.enable_early_stopping) {
        if (nimcp_brain_training_check_early_stop(ctx, *loss_value)) {
            return NIMCP_NOT_FOUND;  /* Use as NIMCP_EARLY_STOP indicator */
        }
    }

    return NIMCP_SUCCESS;
}


nimcp_result_t nimcp_brain_training_step_biological(
    nimcp_brain_training_ctx_t* ctx,
    uint32_t loss_id,
    uint32_t optimizer_id,
    float* params,
    const float* predictions,
    const float* targets,
    size_t batch_size,
    size_t output_size,
    size_t param_count,
    uint32_t region_id,
    float* loss_value)
{
    NIMCP_CHECK_THROW(ctx && params && predictions && targets && loss_value,
                      NIMCP_ERROR_INVALID_PARAM,
                      "nimcp_brain_training_step_biological: NULL argument");
    NIMCP_CHECK_THROW(batch_size > 0 && output_size > 0 && param_count > 0,
                      NIMCP_ERROR_INVALID_PARAM,
                      "nimcp_brain_training_step_biological: size is 0");

    /* If no plasticity bridge or biological modulation is zero, fallback to regular step */
    if (!ctx->plasticity_bridge || ctx->biological_modulation_strength <= 0.0F) {
        return nimcp_brain_training_step(
            ctx, loss_id, optimizer_id, params,
            predictions, targets, batch_size, output_size,
            param_count, loss_value
        );
    }

    /* Allocate gradient buffer */
    size_t gradient_count = batch_size * output_size;
    float* gradients = (float*)nimcp_malloc(gradient_count * sizeof(float));
    NIMCP_CHECK_THROW(gradients, NIMCP_ERROR_MEMORY,
                      "nimcp_brain_training_step_biological: failed to allocate gradients");
    memset(gradients, 0, gradient_count * sizeof(float));

    /* Step 1: Compute loss and gradients */
    nimcp_result_t res = nimcp_brain_training_compute_loss(
        ctx, loss_id, predictions, targets,
        batch_size, output_size, loss_value, gradients
    );

    if (res != NIMCP_SUCCESS) {
        nimcp_free(gradients);
        return res;
    }

    /* Step 2: Report loss to plasticity bridge and get RPE */
    float rpe = 0.0F;
    res = tpb_report_loss(ctx->plasticity_bridge, *loss_value, &rpe);
    if (res == NIMCP_SUCCESS) {
        ctx->stats.rpe_computations++;

        /* Get current dopamine level from neuromodulator system */
        float da_level = 0.5F, ach_level, ht5_level, ne_level;
        if (tpb_get_neuromod_levels(ctx->plasticity_bridge, &da_level, &ach_level,
                                     &ht5_level, &ne_level) == NIMCP_SUCCESS) {
            ctx->cumulative_da += da_level;

            /* Activate second messenger cascades based on reward signal
             * Positive RPE/dopamine activates Gs-coupled D1 receptors -> cAMP -> PKA
             * This enhances learning for reward-related updates */
            if (ctx->second_messengers_enabled && ctx->second_messengers && rpe > 0.0F) {
                uint64_t timestamp_ms = nimcp_time_get_us() / 1000;
                /* Activate Gs pathway for first neuron (representative)
                 * occupancy scales with dopamine level [0, 1] */
                float occupancy = da_level;
                if (occupancy > 1.0F) occupancy = 1.0F;

                second_messenger_activate_gs(
                    ctx->second_messengers,
                    0,  /* neuron_id */
                    occupancy,
                    timestamp_ms
                );

                LOG_DEBUG("Activated Gs cascade for reward (RPE=%.3f, DA=%.3f)",
                          rpe, da_level);
            }
        }
    }

    /* Step 3: Get biologically-modulated learning rate */
    nimcp_optimizer_context_t* opt = nimcp_brain_training_get_optimizer(ctx, optimizer_id);
    float base_lr = opt ? nimcp_optimizer_get_lr(opt) : ctx->config.default_learning_rate;

    float bio_lr = base_lr;
    tpb_get_modulated_lr(ctx->plasticity_bridge, region_id, base_lr, &bio_lr);
    float blend = ctx->biological_modulation_strength;
    float effective_lr = (1.0F - blend) * base_lr + blend * bio_lr;

    /* Apply second messenger cascade modulation if enabled
     * PKA/CaMKII activity boosts learning rates for enhanced plasticity
     * Use first neuron as representative for batch (neuron_id = 0) */
    if (ctx->second_messengers_enabled && ctx->second_messengers) {
        float cascade_mod = nimcp_brain_training_get_cascade_modulation(ctx, 0);
        effective_lr *= cascade_mod;
        LOG_DEBUG("Cascade modulation applied: %.3f (effective LR: %.6f)",
                  cascade_mod, effective_lr);

        /* Update second messenger dynamics for this timestep
         * This advances the cascade state based on current activity */
        uint64_t timestamp_ms = nimcp_time_get_us() / 1000;
        second_messenger_update(ctx->second_messengers, 1.0F, timestamp_ms);
    }

    ctx->cumulative_lr_mod += (effective_lr / base_lr);

    /* Temporarily set modulated learning rate */
    if (opt) {
        nimcp_optimizer_set_lr(opt, effective_lr);
    }

    /* Step 4: Route weight updates through plasticity bridge */
    size_t update_count = (param_count < gradient_count) ? param_count : gradient_count;

    /* Get CaMKII activity for STDP enhancement if cascades enabled */
    second_messenger_state_t sm_state = {0};
    bool have_cascade_state = false;
    if (ctx->second_messengers_enabled && ctx->second_messengers) {
        if (second_messenger_get_state(ctx->second_messengers, 0, &sm_state) == NIMCP_SUCCESS) {
            have_cascade_state = true;
        }
    }

    for (size_t i = 0; i < update_count; i++) {
        /* For biological routing, we compute weight delta through the bridge */
        float pre_activity = fabsf(gradients[i]);  /* Use gradient magnitude as proxy */
        float post_activity = fabsf(params[i]);     /* Use weight magnitude as proxy */
        float spike_delta = (gradients[i] > 0) ? 0.01F : -0.01F;  /* Sign indicates timing */

        float weight_delta = 0.0F;
        nimcp_result_t route_res = tpb_route_weight_update(
            ctx->plasticity_bridge,
            (uint32_t)i,    /* neuron_id - use index */
            pre_activity,
            post_activity,
            spike_delta,
            &weight_delta
        );

        /* Blend biological update with gradient-based update */
        float grad_delta = -effective_lr * gradients[i];
        float final_delta;
        if (route_res == NIMCP_SUCCESS) {
            final_delta = (1.0F - blend) * grad_delta + blend * weight_delta;

            /* Enhance STDP-like weight changes with CaMKII activity
             * CaMKII is activated by coincident pre/post activity (calcium influx)
             * and enhances LTP magnitude */
            if (have_cascade_state && sm_state.calcium.camkii_activity > 0.5F) {
                float camkii_boost = 1.0F + (sm_state.calcium.camkii_activity - 0.5F);
                final_delta *= camkii_boost;
            }
        } else {
            final_delta = grad_delta;  /* Fallback to pure gradient if routing fails */
        }
        params[i] += final_delta;
    }

    ctx->stats.biological_updates += update_count;
    ctx->bio_update_count++;

    /* Check for CREB phosphorylation signaling consolidation
     * High CREB phosphorylation indicates readiness for long-term plasticity
     * This could trigger additional consolidation mechanisms */
    if (have_cascade_state && sm_state.gene_expr.creb_phosphorylation > 0.7F) {
        LOG_DEBUG("CREB phosphorylation high (%.3f) - consolidation signaled",
                  sm_state.gene_expr.creb_phosphorylation);

        /* Emit training event for consolidation marker */
        nimcp_training_event_t event = {0};
        event.type = NIMCP_TRAINING_EVENT_WEIGHTS_UPDATED;
        event.timestamp = nimcp_time_get_us() * 1000;
        event.epoch = ctx->current_epoch;
        event.batch = ctx->current_batch;
        event.loss_value = *loss_value;
        event.learning_rate = effective_lr;
        nimcp_brain_training_emit_event(ctx, &event);
    }

    /* Restore base learning rate */
    if (opt) {
        nimcp_optimizer_set_lr(opt, base_lr);
    }

    /* Update tracking */
    ctx->prev_loss = *loss_value;

    /* Update average stats */
    if (ctx->bio_update_count > 0) {
        ctx->stats.avg_dopamine_level = ctx->cumulative_da / (float)ctx->stats.rpe_computations;
        ctx->stats.avg_lr_modulation = ctx->cumulative_lr_mod / (float)ctx->bio_update_count;
    }

    nimcp_free(gradients);

    /* Check convergence/divergence */
    float loss_change = fabsf(*loss_value - ctx->last_loss);
    if (loss_change < ctx->config.convergence_threshold) {
        ctx->stable_loss_count++;
        if (ctx->stable_loss_count >= 10) {
            ctx->converged = true;
            LOG_INFO("Biological training converged at loss = %f", *loss_value);
        }
    } else {
        ctx->stable_loss_count = 0;
    }
    ctx->last_loss = *loss_value;
    ctx->stats.total_batches++;

    return NIMCP_SUCCESS;
}
