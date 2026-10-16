// nimcp_lnn_gradient_part_accessors.c - accessors functions
// Part of nimcp_lnn_gradient.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_lnn_gradient.c


/*=============================================================================
 * Gradient Access and Application
 *===========================================================================*/

/**
 * @brief Get accumulated parameter gradients
 *
 * WHAT: Extract final ∂L/∂θ
 * WHY:  Transfer to optimizer
 * HOW:  Copy from internal storage
 */
int lnn_gradient_get_params(
    const lnn_gradient_ctx_t* ctx,
    nimcp_tensor_t* grad_params
) {
    // Guard: validate inputs
    if (!ctx || !grad_params) {
        NIMCP_LOGGING_ERROR("NULL input to get_params");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Null input to lnn_gradient_get_params: ctx=%p, grad_params=%p",
                             (void*)ctx, (void*)grad_params);
        return LNN_ERROR_NULL_POINTER;
    }

    if (!ctx->grad_params) {
        NIMCP_LOGGING_ERROR("No gradients available");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE,
                             "No gradients available in lnn_gradient_get_params");
        return LNN_ERROR_INVALID_STATE;
    }

    // Copy gradients
    size_t numel = nimcp_tensor_numel(ctx->grad_params);
    size_t numel_out = nimcp_tensor_numel(grad_params);

    if (numel != numel_out) {
        NIMCP_LOGGING_ERROR("Gradient size mismatch: %zu vs %zu", numel, numel_out);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_DIMENSION_MISMATCH,
                             "Gradient size mismatch: %zu vs %zu", numel, numel_out);
        return LNN_ERROR_INVALID_PARAM;
    }

    memcpy(nimcp_tensor_data(grad_params),
           nimcp_tensor_data_const(ctx->grad_params),
           numel * sizeof(float));

    return 0;
}


/**
 * @brief Get gradient statistics
 *
 * WHAT: Retrieve gradient metrics
 * WHY:  Performance profiling
 * HOW:  Return accumulated stats
 */
int lnn_gradient_get_stats(
    const lnn_gradient_ctx_t* ctx,
    uint64_t* adjoint_steps,
    uint64_t* jacobian_evals,
    uint64_t* checkpoints_used,
    double* compute_time_ms
) {
    // Guard: validate inputs
    if (!ctx) {
        return LNN_ERROR_NULL_POINTER;
    }

    if (adjoint_steps) *adjoint_steps = ctx->adjoint_steps;
    if (jacobian_evals) *jacobian_evals = ctx->jacobian_evals;
    if (checkpoints_used) *checkpoints_used = ctx->checkpoints_used;
    if (compute_time_ms) *compute_time_ms = ctx->compute_time_ms;

    return 0;
}


/**
 * @brief Get current time in milliseconds
 *
 * WHAT: Platform-independent time measurement
 * WHY:  Performance profiling
 * HOW:  Use system clock
 */
static double lnn_gradient_get_time_ms(void) {
    // Simplified placeholder - should use proper timer
    // In real implementation, use clock_gettime or QueryPerformanceCounter
    return 0.0;
}
