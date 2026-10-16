// nimcp_lnn_gradient_part_helpers.c - helpers functions
// Part of nimcp_lnn_gradient.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_lnn_gradient.c


/*=============================================================================
 * Helper Functions
 *===========================================================================*/

/**
 * @brief Allocate gradient storage
 *
 * WHAT: Create tensors for adjoint and gradients
 * WHY:  Initialize gradient computation
 * HOW:  Allocate based on network size
 */
static int allocate_gradient_storage(lnn_gradient_ctx_t* ctx, lnn_network_t* network) {
    // Guard: validate inputs
    if (!ctx || !network) {
        return LNN_ERROR_NULL_POINTER;
    }

    // Count total parameters (simplified - assumes single layer)
    if (!network->layers || network->n_layers == 0) {
        NIMCP_LOGGING_ERROR("Network has no layers");
        return LNN_ERROR_INVALID_STATE;
    }

    // BUG FIX: Check that first layer is not NULL before dereferencing
    lnn_layer_t* layer = network->layers[0];
    if (!layer) {
        NIMCP_LOGGING_ERROR("Network layer[0] is NULL");
        return LNN_ERROR_NULL_POINTER;
    }
    uint32_t n_neurons = layer->n_neurons;

    // Allocate adjoint state tensor
    ctx->adjoint = nimcp_tensor_zeros((uint32_t[]){n_neurons}, 1, NIMCP_DTYPE_F32);
    if (!ctx->adjoint) {
        return LNN_ERROR_OUT_OF_MEMORY;
    }

    // Estimate parameter count (simplified)
    // Full implementation would count all W_in, W_rec, W_tau, biases, tau_base across all layers
    size_t n_params = n_neurons * (n_neurons + 10);  // Rough estimate

    // Allocate gradient parameter tensor
    ctx->grad_params = nimcp_tensor_zeros((uint32_t[]){(uint32_t)n_params}, 1, NIMCP_DTYPE_F32);
    if (!ctx->grad_params) {
        nimcp_tensor_destroy(ctx->adjoint);
        return LNN_ERROR_OUT_OF_MEMORY;
    }

    return 0;
}


/**
 * @brief Allocate checkpoints
 *
 * WHAT: Create checkpoint storage
 * WHY:  Enable memory-computation tradeoff
 * HOW:  Allocate array of tensor pointers
 */
static int allocate_checkpoints(lnn_gradient_ctx_t* ctx, uint32_t max_steps) {
    // Guard: validate inputs
    if (!ctx) {
        return LNN_ERROR_NULL_POINTER;
    }

    // Calculate number of checkpoints needed
    uint32_t n_checkpoints = (max_steps + ctx->checkpoint_interval - 1) / ctx->checkpoint_interval;

    ctx->checkpoint_capacity = n_checkpoints;
    ctx->checkpoints = (nimcp_tensor_t**)nimcp_malloc(n_checkpoints * sizeof(nimcp_tensor_t*));
    if (!ctx->checkpoints) {
        return LNN_ERROR_OUT_OF_MEMORY;
    }

    memset(ctx->checkpoints, 0, n_checkpoints * sizeof(nimcp_tensor_t*));

    NIMCP_LOGGING_DEBUG("Allocated %u checkpoint slots (interval=%u)", n_checkpoints, ctx->checkpoint_interval);

    return 0;
}


/**
 * @brief Free gradient storage
 *
 * WHAT: Destroy gradient tensors
 * WHY:  Clean shutdown
 * HOW:  Destroy each tensor
 */
static void free_gradient_storage(lnn_gradient_ctx_t* ctx) {
    if (!ctx) {
        return;
    }

    if (ctx->adjoint) {
        nimcp_tensor_destroy(ctx->adjoint);
        ctx->adjoint = NULL;
    }

    if (ctx->adjoint_prev) {
        nimcp_tensor_destroy(ctx->adjoint_prev);
        ctx->adjoint_prev = NULL;
    }

    if (ctx->grad_params) {
        nimcp_tensor_destroy(ctx->grad_params);
        ctx->grad_params = NULL;
    }

    if (ctx->dL_dx_final) {
        nimcp_tensor_destroy(ctx->dL_dx_final);
        ctx->dL_dx_final = NULL;
    }
}


/**
 * @brief Free checkpoints
 *
 * WHAT: Destroy checkpoint storage
 * WHY:  Clean shutdown
 * HOW:  Destroy each checkpoint tensor
 */
static void free_checkpoints(lnn_gradient_ctx_t* ctx) {
    if (!ctx || !ctx->checkpoints) {
        return;
    }

    for (uint32_t i = 0; i < ctx->checkpoint_capacity; i++) {
        if (ctx->checkpoints[i]) {
            nimcp_tensor_destroy(ctx->checkpoints[i]);
        }
    }

    nimcp_free(ctx->checkpoints);
    ctx->checkpoints = NULL;
    ctx->n_checkpoints = 0;
}


/**
 * @brief Compute Jacobian numerically
 *
 * WHAT: Numerical Jacobian via finite differences
 * WHY:  Simple implementation (can be optimized to analytical)
 * HOW:  Perturb each state element, measure derivative
 *
 * NOTE: Uses fixed epsilon (JACOBIAN_EPSILON = 1e-5f)
 * LIMITATION: Fixed epsilon may not be optimal for all state magnitudes
 * FUTURE: Consider making epsilon configurable or adaptive (scaled by state magnitude)
 */
static int compute_jacobian_numerical(
    const lnn_layer_t* layer,
    const nimcp_tensor_t* x,
    nimcp_tensor_t* jacobian
) {
    // Guard: validate inputs
    if (!layer || !x || !jacobian) {
        return LNN_ERROR_NULL_POINTER;
    }

    uint32_t n = layer->n_neurons;
    /* Fixed epsilon for finite differences
     * Known limitation: Not scaled by state magnitude
     * Could be made configurable for improved numerical accuracy
     */
    float eps = JACOBIAN_EPSILON;

    // Clone state for perturbation
    nimcp_tensor_t* x_perturbed = nimcp_tensor_clone(x);
    if (!x_perturbed) {
        return LNN_ERROR_OUT_OF_MEMORY;
    }

    // Allocate buffers for f(x+eps) and f(x-eps)
    nimcp_tensor_t* f_plus = nimcp_tensor_create((uint32_t[]){n}, 1, NIMCP_DTYPE_F32);
    nimcp_tensor_t* f_minus = nimcp_tensor_create((uint32_t[]){n}, 1, NIMCP_DTYPE_F32);

    if (!f_plus || !f_minus) {
        nimcp_tensor_destroy(x_perturbed);
        if (f_plus) nimcp_tensor_destroy(f_plus);
        if (f_minus) nimcp_tensor_destroy(f_minus);
        return LNN_ERROR_OUT_OF_MEMORY;
    }

    float* x_data = (float*)nimcp_tensor_data(x_perturbed);
    float* jac_data = (float*)nimcp_tensor_data(jacobian);
    float* f_plus_data = (float*)nimcp_tensor_data(f_plus);
    float* f_minus_data = (float*)nimcp_tensor_data(f_minus);
    const float* x_orig_data = (const float*)nimcp_tensor_data_const(x);

    // For each state variable i (column of Jacobian)
    for (uint32_t i = 0; i < n; i++) {
        float x_orig = x_orig_data[i];

        // Compute f(x + eps*e_i)
        memcpy(x_data, x_orig_data, n * sizeof(float));
        x_data[i] = x_orig + eps;

        // Evaluate layer dynamics: dx/dt = -x/tau + activation(W_rec @ x)
        for (uint32_t j = 0; j < n; j++) {
            float tau_j = (layer->tau_base && nimcp_tensor_data(layer->tau_base)) ?
                         ((float*)nimcp_tensor_data(layer->tau_base))[j] : 10.0f;
            float recurrent_sum = 0.0f;

            // W_rec @ x (simplified dense matmul)
            if (layer->W_rec && nimcp_tensor_data(layer->W_rec)) {
                float* W_rec_data = (float*)nimcp_tensor_data(layer->W_rec);
                for (uint32_t k = 0; k < n; k++) {
                    recurrent_sum += W_rec_data[j * n + k] * x_data[k];
                }
            }

            // dx/dt = -x/tau + tanh(recurrent)
            f_plus_data[j] = -x_data[j] / tau_j + tanhf(recurrent_sum);
        }

        // Compute f(x - eps*e_i)
        memcpy(x_data, x_orig_data, n * sizeof(float));
        x_data[i] = x_orig - eps;

        for (uint32_t j = 0; j < n; j++) {
            float tau_j = (layer->tau_base && nimcp_tensor_data(layer->tau_base)) ?
                         ((float*)nimcp_tensor_data(layer->tau_base))[j] : 10.0f;
            float recurrent_sum = 0.0f;

            if (layer->W_rec && nimcp_tensor_data(layer->W_rec)) {
                float* W_rec_data = (float*)nimcp_tensor_data(layer->W_rec);
                for (uint32_t k = 0; k < n; k++) {
                    recurrent_sum += W_rec_data[j * n + k] * x_data[k];
                }
            }

            f_minus_data[j] = -x_data[j] / tau_j + tanhf(recurrent_sum);
        }

        // Jacobian column i: ∂f/∂x_i = (f_plus - f_minus) / (2*eps)
        for (uint32_t j = 0; j < n; j++) {
            jac_data[j * n + i] = (f_plus_data[j] - f_minus_data[j]) / (2.0f * eps);
        }
    }

    nimcp_tensor_destroy(x_perturbed);
    nimcp_tensor_destroy(f_plus);
    nimcp_tensor_destroy(f_minus);
    return 0;
}


/**
 * @brief Accumulate parameter gradients
 *
 * WHAT: Accumulate ∂L/∂θ += λ^T ∂f/∂θ * dt
 * WHY:  Build up total gradient over time
 * HOW:  Compute parameter Jacobian, multiply by adjoint
 */
static int accumulate_parameter_gradients(
    lnn_gradient_ctx_t* ctx,
    lnn_network_t* network,
    const nimcp_tensor_t* adjoint,
    float dt
) {
    // Guard: validate inputs
    if (!ctx || !network || !adjoint) {
        return LNN_ERROR_NULL_POINTER;
    }

    if (!network->layers || network->n_layers == 0) {
        return LNN_ERROR_INVALID_STATE;
    }

    // Early-exit if adjoint contains NaN — accumulating NaN gradients is pointless
    // and poisons per-layer grad tensors permanently until zeroed.
    if (!check_tensor_health(adjoint)) {
        NIMCP_LOGGING_DEBUG("Skipping gradient accumulation: adjoint contains NaN/Inf");
        return 0;  // Not an error — just skip this step
    }

    // For each layer, compute and accumulate parameter gradients.
    // NOTE: The adjoint tensor has shape [n_outputs] (last layer output dim).
    // Only accumulate gradients for layers whose n_neurons <= adjoint numel,
    // otherwise adjoint_data[j] reads beyond the buffer (heap corruption).
    uint32_t adjoint_numel = nimcp_tensor_numel(adjoint);

    for (uint32_t layer_idx = 0; layer_idx < network->n_layers; layer_idx++) {
        lnn_layer_t* layer = network->layers[layer_idx];
        if (!layer || !layer->x) continue;

        uint32_t n = layer->n_neurons;

        /* Skip layers larger than the adjoint — reading adjoint_data[j] for
         * j >= adjoint_numel would be a buffer overread. In a multi-layer LNN,
         * only the output layer's gradients can be directly accumulated from
         * the output-space adjoint. */
        if (n > adjoint_numel) continue;

        const float* adjoint_data = (const float*)nimcp_tensor_data_const(adjoint);
        const float* x_data = (const float*)nimcp_tensor_data_const(layer->x);

        // Gradient w.r.t. W_rec: ∂L/∂W_rec = λ^T * ∂f/∂W_rec * dt
        // ∂f/∂W_rec[j,k] = activation'(...) * x[k] for neuron j
        if (layer->grad_W_rec && layer->W_rec) {
            float* grad_W_rec = (float*)nimcp_tensor_data(layer->grad_W_rec);

            for (uint32_t j = 0; j < n; j++) {
                // Get recurrent sum for derivative computation
                float recurrent_sum = 0.0f;
                float* W_rec_data = (float*)nimcp_tensor_data(layer->W_rec);

                for (uint32_t k = 0; k < n; k++) {
                    recurrent_sum += W_rec_data[j * n + k] * x_data[k];
                }

                // Activation derivative (tanh': 1 - tanh^2)
                float tanh_val = tanhf(recurrent_sum);
                float act_deriv = 1.0f - tanh_val * tanh_val;

                // Accumulate gradient for each weight W_rec[j,k]
                for (uint32_t k = 0; k < n; k++) {
                    float inc = adjoint_data[j] * act_deriv * x_data[k] * dt;
                    grad_W_rec[j * n + k] += inc;
                    /* Per-step clamp — ±1e4 was too loose (norm → 640K for 4096 params).
                     * ±10 keeps overall norm ≤ sqrt(4096)*10 ≈ 640 per layer. */
                    if (grad_W_rec[j * n + k] > 10.0f) grad_W_rec[j * n + k] = 10.0f;
                    else if (grad_W_rec[j * n + k] < -10.0f) grad_W_rec[j * n + k] = -10.0f;
                }
            }
        }

        // Gradient w.r.t. tau_base: ∂L/∂tau_base = λ^T * ∂f/∂tau * dt
        // ∂f/∂tau[j] = x[j] / (tau^2) (from -x/tau term)
        if (layer->grad_tau_base && layer->tau_base) {
            float* grad_tau_base = (float*)nimcp_tensor_data(layer->grad_tau_base);
            float* tau_base_data = (float*)nimcp_tensor_data(layer->tau_base);

            for (uint32_t j = 0; j < n; j++) {
                float tau_j = fmaxf(tau_base_data[j], 0.01f);  /* Floor prevents 1/tau^2 explosion */
                // ∂(-x/tau)/∂tau = x / tau^2
                float df_dtau = x_data[j] / (tau_j * tau_j);
                float inc = adjoint_data[j] * df_dtau * dt;
                grad_tau_base[j] += inc;
                /* Per-step clamp */
                if (grad_tau_base[j] > 10.0f) grad_tau_base[j] = 10.0f;
                else if (grad_tau_base[j] < -10.0f) grad_tau_base[j] = -10.0f;
            }
        }

        // Gradient w.r.t. W_in (if input is available in context)
        // This would require storing the input, which we don't have here
        // In a full implementation, we'd need input history or checkpointing

        // Gradient w.r.t. biases can be computed similarly
        if (layer->grad_b_in) {
            float* grad_b_in = (float*)nimcp_tensor_data(layer->grad_b_in);

            for (uint32_t j = 0; j < n; j++) {
                // ∂f/∂b_in = activation'(...)
                // Simplified: assume contribution through recurrent path
                grad_b_in[j] += adjoint_data[j] * dt;
                /* Per-step clamp */
                if (grad_b_in[j] > 10.0f) grad_b_in[j] = 10.0f;
                else if (grad_b_in[j] < -10.0f) grad_b_in[j] = -10.0f;
            }
        }
    }

    return 0;
}


/**
 * @brief Check tensor health
 *
 * WHAT: Scan tensor for NaN/Inf
 * WHY:  Early detection of numerical issues
 * HOW:  Iterate through elements
 */
static bool check_tensor_health(const nimcp_tensor_t* t) {
    if (!t) {
        return false;
    }

    size_t numel = nimcp_tensor_numel(t);
    const float* data = (const float*)nimcp_tensor_data_const(t);

    for (size_t i = 0; i < numel; i++) {
        if (isnan(data[i]) || isinf(data[i])) {
            return false;
        }
    }

    return true;
}
