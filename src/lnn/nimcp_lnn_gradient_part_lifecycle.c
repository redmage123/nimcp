// nimcp_lnn_gradient_part_lifecycle.c - lifecycle functions
// Part of nimcp_lnn_gradient.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_lnn_gradient.c


/*=============================================================================
 * Lifecycle Functions
 *===========================================================================*/

/**
 * @brief Create gradient context
 *
 * WHAT: Allocate and initialize gradient computation context
 * WHY:  Centralized gradient state management
 * HOW:  Allocate tensors, setup checkpointing if enabled
 */
lnn_gradient_ctx_t* lnn_gradient_ctx_create(
    lnn_network_t* network,
    uint32_t max_steps,
    bool use_checkpointing,
    uint32_t checkpoint_interval
) {
    // Guard: validate inputs
    if (!network) {
        NIMCP_LOGGING_ERROR("Cannot create gradient context: network is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Null network pointer in lnn_gradient_ctx_create");
        return NULL;
    }

    if (max_steps == 0) {
        NIMCP_LOGGING_ERROR("Cannot create gradient context: max_steps must be > 0");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                             "max_steps must be > 0 in lnn_gradient_ctx_create");
        return NULL;
    }

    // Allocate context
    lnn_gradient_ctx_t* ctx = (lnn_gradient_ctx_t*)nimcp_malloc(sizeof(lnn_gradient_ctx_t));
    if (!ctx) {
        NIMCP_LOGGING_ERROR("Failed to allocate gradient context");
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(lnn_gradient_ctx_t),
                          "Failed to allocate gradient context");
        return NULL;
    }

    memset(ctx, 0, sizeof(lnn_gradient_ctx_t));

    // Initialize fields
    ctx->network = network;
    ctx->n_steps = max_steps;
    ctx->use_checkpointing = use_checkpointing;
    ctx->checkpoint_interval = (checkpoint_interval > 0) ? checkpoint_interval : DEFAULT_CHECKPOINT_INTERVAL;

    // Allocate gradient storage
    if (allocate_gradient_storage(ctx, network) != 0) {
        NIMCP_LOGGING_ERROR("Failed to allocate gradient storage");
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, 0,
                          "Failed to allocate gradient storage in lnn_gradient_ctx_create");
        lnn_gradient_ctx_destroy(ctx);
        return NULL;
    }

    // Allocate checkpoints if enabled
    if (use_checkpointing) {
        if (allocate_checkpoints(ctx, max_steps) != 0) {
            NIMCP_LOGGING_ERROR("Failed to allocate checkpoints");
            NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, 0,
                              "Failed to allocate checkpoints for %u steps", max_steps);
            lnn_gradient_ctx_destroy(ctx);
            return NULL;
        }
    }

    // Create mutex for thread safety
    ctx->mutex = nimcp_malloc(sizeof(nimcp_mutex_t));
    if (ctx->mutex) {
        nimcp_mutex_init((nimcp_mutex_t*)ctx->mutex, NULL);
    }

    ctx->max_adjoint_steps = 0;  /* Default: unlimited (set by training layer) */

    NIMCP_LOGGING_INFO("Created gradient context: max_steps=%u, checkpointing=%s",
                       max_steps, use_checkpointing ? "enabled" : "disabled");

    return ctx;
}


/**
 * @brief Destroy gradient context
 *
 * WHAT: Free all gradient computation resources
 * WHY:  Clean shutdown and memory management
 * HOW:  Destroy tensors, free checkpoints, release mutex
 */
void lnn_gradient_ctx_destroy(lnn_gradient_ctx_t* ctx) {
    // Guard: check NULL
    if (!ctx) {
        return;
    }

    // Free saved input gradient
    if (ctx->last_input_grad) {
        nimcp_tensor_destroy(ctx->last_input_grad);
        ctx->last_input_grad = NULL;
    }

    // Free gradient storage
    free_gradient_storage(ctx);

    // Free checkpoints
    if (ctx->use_checkpointing) {
        free_checkpoints(ctx);
    }

    // Destroy mutex
    if (ctx->mutex) {
        nimcp_mutex_free((nimcp_mutex_t*)ctx->mutex);
    }

    // Free context
    nimcp_free(ctx);

    NIMCP_LOGGING_DEBUG("Destroyed gradient context");
}


/**
 * @brief Reset gradient accumulator
 *
 * WHAT: Zero out gradients
 * WHY:  Prepare for next batch
 * HOW:  Set to zero
 */
void lnn_gradient_reset(lnn_gradient_ctx_t* ctx) {
    // Guard: check NULL
    if (!ctx) {
        return;
    }

    // Zero gradient tensor (legacy — not used by accumulate_parameter_gradients)
    if (ctx->grad_params) {
        size_t numel = nimcp_tensor_numel(ctx->grad_params);
        float* data = (float*)nimcp_tensor_data(ctx->grad_params);
        memset(data, 0, numel * sizeof(float));
    }

    // Zero per-layer gradient tensors (the ACTUAL accumulated gradients).
    // Without this, NaN from a failed adjoint step persists across batches.
    if (ctx->network) {
        lnn_network_zero_gradients(ctx->network);
    }

    // Reset health flags
    ctx->has_nan = false;
    ctx->has_inf = false;
    ctx->gradient_norm = 0.0f;

    // Reset the per-batch adjoint step counter. It's a cumulative log counter
    // and doesn't affect the actual compute (capped by max_adjoint_steps),
    // but unbounded growth in logs was misleading ops into thinking the
    // adjoint pass itself was scaling — it wasn't.
    ctx->adjoint_steps = 0;
}
