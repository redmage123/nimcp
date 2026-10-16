// nimcp_lnn_gradient_part_io.c - io functions
// Part of nimcp_lnn_gradient.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_lnn_gradient.c


/*=============================================================================
 * Checkpointing Functions
 *===========================================================================*/

/**
 * @brief Save state checkpoint
 *
 * WHAT: Save network state at time step
 * WHY:  Tradeoff memory for computation
 * HOW:  Store copy in checkpoint array
 */
int lnn_gradient_save_checkpoint(
    lnn_gradient_ctx_t* ctx,
    uint32_t step,
    const nimcp_tensor_t* state
) {
    // Guard: validate inputs
    if (!ctx || !state) {
        return LNN_ERROR_NULL_POINTER;
    }

    if (!ctx->use_checkpointing) {
        return 0;  // Checkpointing disabled
    }

    // Check capacity
    if (step >= ctx->checkpoint_capacity) {
        NIMCP_LOGGING_WARN("Checkpoint step %u exceeds capacity %u", step, ctx->checkpoint_capacity);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                             "Checkpoint step %u exceeds capacity %u", step, ctx->checkpoint_capacity);
        return LNN_ERROR_INVALID_PARAM;
    }

    // Free existing checkpoint if present
    if (ctx->checkpoints[step]) {
        nimcp_tensor_destroy(ctx->checkpoints[step]);
    }

    // Clone state
    ctx->checkpoints[step] = nimcp_tensor_clone(state);
    if (!ctx->checkpoints[step]) {
        return LNN_ERROR_OUT_OF_MEMORY;
    }

    ctx->n_checkpoints++;
    return 0;
}


/**
 * @brief Load state checkpoint
 *
 * WHAT: Restore state from checkpoint
 * WHY:  Resume forward pass
 * HOW:  Copy checkpoint to output
 */
int lnn_gradient_load_checkpoint(
    lnn_gradient_ctx_t* ctx,
    uint32_t step,
    nimcp_tensor_t* state
) {
    // Guard: validate inputs
    if (!ctx || !state) {
        return LNN_ERROR_NULL_POINTER;
    }

    if (!ctx->use_checkpointing) {
        return LNN_ERROR_INVALID_STATE;
    }

    if (step >= ctx->checkpoint_capacity || !ctx->checkpoints[step]) {
        NIMCP_LOGGING_ERROR("No checkpoint at step %u", step);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                             "No checkpoint at step %u (capacity=%u)", step, ctx->checkpoint_capacity);
        return LNN_ERROR_INVALID_PARAM;
    }

    // Copy checkpoint data
    size_t numel = nimcp_tensor_numel(ctx->checkpoints[step]);
    memcpy(nimcp_tensor_data(state),
           nimcp_tensor_data_const(ctx->checkpoints[step]),
           numel * sizeof(float));

    ctx->checkpoints_used++;
    return 0;
}
