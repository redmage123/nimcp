//=============================================================================
// nimcp_kernel_backend_cuda.cu - CUDA Backend Operations Registration
//=============================================================================
/**
 * @file nimcp_kernel_backend_cuda.cu
 * @brief Wires up CUDA kernels to unified backend interface
 *
 * WHAT: Register all CUDA kernel implementations with the unified backend
 * WHY:  Enable Strategy Pattern for GPU/CPU kernel selection
 * HOW:  Create wrapper functions matching backend interface signatures
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

// Include CUDA headers FIRST (before any extern "C" blocks from our headers)
#ifdef NIMCP_ENABLE_CUDA
#include <cuda_runtime.h>
#endif

// Now include our headers (which have extern "C" blocks)
#include "gpu/backend/nimcp_kernel_backend.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "gpu/common/nimcp_cuda_utils.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/training/nimcp_training_gpu.h"
#include "gpu/snn/nimcp_snn_gpu.h"
#include "gpu/cnn/nimcp_cnn_gpu.h"
#include "gpu/lnn/nimcp_lnn_gpu.h"
#include "gpu/quantum/nimcp_quantum_gpu.h"
#include "gpu/inference/nimcp_inference_gpu.h"
#include "utils/logging/nimcp_logging.h"

#ifdef NIMCP_ENABLE_CUDA

#define LOG_MODULE "CUDA_BACKEND"

//=============================================================================
// CUDA Tensor Operations Wrappers
//=============================================================================

static nimcp_kernel_error_t cuda_tensor_add(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* a,
    const nimcp_gpu_tensor_t* b,
    nimcp_gpu_tensor_t* result)
{
    if (!nimcp_gpu_add(ctx, a, b, result)) {
        return NIMCP_KERNEL_ERROR_DEVICE;
    }
    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cuda_tensor_sub(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* a,
    const nimcp_gpu_tensor_t* b,
    nimcp_gpu_tensor_t* result)
{
    if (!nimcp_gpu_sub(ctx, a, b, result)) {
        return NIMCP_KERNEL_ERROR_DEVICE;
    }
    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cuda_tensor_mul(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* a,
    const nimcp_gpu_tensor_t* b,
    nimcp_gpu_tensor_t* result)
{
    if (!nimcp_gpu_mul(ctx, a, b, result)) {
        return NIMCP_KERNEL_ERROR_DEVICE;
    }
    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cuda_tensor_div(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* a,
    const nimcp_gpu_tensor_t* b,
    nimcp_gpu_tensor_t* result)
{
    if (!nimcp_gpu_div(ctx, a, b, result)) {
        return NIMCP_KERNEL_ERROR_DEVICE;
    }
    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cuda_tensor_scale(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* a,
    float scalar,
    nimcp_gpu_tensor_t* result)
{
    if (!nimcp_gpu_mul_scalar(ctx, a, scalar, result)) {
        return NIMCP_KERNEL_ERROR_DEVICE;
    }
    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cuda_tensor_matmul(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* a,
    const nimcp_gpu_tensor_t* b,
    nimcp_gpu_tensor_t* result)
{
    // Use GEMM with alpha=1, beta=0, no transpose
    if (!nimcp_gpu_gemm(ctx, a, b, result, 1.0f, 0.0f, false, false)) {
        return NIMCP_KERNEL_ERROR_DEVICE;
    }
    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cuda_tensor_transpose(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* a,
    nimcp_gpu_tensor_t* result)
{
    if (!nimcp_gpu_transpose(ctx, a, result)) {
        return NIMCP_KERNEL_ERROR_DEVICE;
    }
    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cuda_tensor_relu(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output)
{
    if (!nimcp_gpu_relu(ctx, input, output)) {
        return NIMCP_KERNEL_ERROR_DEVICE;
    }
    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cuda_tensor_sigmoid(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output)
{
    if (!nimcp_gpu_sigmoid(ctx, input, output)) {
        return NIMCP_KERNEL_ERROR_DEVICE;
    }
    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cuda_tensor_tanh(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output)
{
    if (!nimcp_gpu_tanh(ctx, input, output)) {
        return NIMCP_KERNEL_ERROR_DEVICE;
    }
    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cuda_tensor_softmax(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output)
{
    if (!nimcp_gpu_softmax(ctx, input, output)) {
        return NIMCP_KERNEL_ERROR_DEVICE;
    }
    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cuda_tensor_sum(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output)
{
    if (!nimcp_gpu_sum(ctx, input, output, -1, false)) {
        return NIMCP_KERNEL_ERROR_DEVICE;
    }
    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cuda_tensor_mean(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output)
{
    if (!nimcp_gpu_mean(ctx, input, output, -1, false)) {
        return NIMCP_KERNEL_ERROR_DEVICE;
    }
    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cuda_tensor_max(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output)
{
    if (!nimcp_gpu_max(ctx, input, output, -1, false)) {
        return NIMCP_KERNEL_ERROR_DEVICE;
    }
    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cuda_tensor_min(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output)
{
    if (!nimcp_gpu_min(ctx, input, output, -1, false)) {
        return NIMCP_KERNEL_ERROR_DEVICE;
    }
    return NIMCP_KERNEL_SUCCESS;
}

//=============================================================================
// CUDA Training Operations Wrappers
//=============================================================================

static nimcp_kernel_error_t cuda_mse_loss(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* pred,
    const nimcp_gpu_tensor_t* target,
    nimcp_gpu_tensor_t* loss)
{
    float loss_val;
    if (!nimcp_gpu_loss_mse(ctx, pred, target, &loss_val, NULL)) {
        return NIMCP_KERNEL_ERROR_DEVICE;
    }
    // Store loss in output tensor
    float* loss_data = (float*)loss->data;
    cudaMemcpy(loss_data, &loss_val, sizeof(float), cudaMemcpyHostToDevice);
    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cuda_cross_entropy_loss(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* pred,
    const nimcp_gpu_tensor_t* target,
    nimcp_gpu_tensor_t* loss)
{
    float loss_val;
    if (!nimcp_gpu_loss_cross_entropy(ctx, pred, target, &loss_val, NULL, 1)) {
        return NIMCP_KERNEL_ERROR_DEVICE;
    }
    float* loss_data = (float*)loss->data;
    cudaMemcpy(loss_data, &loss_val, sizeof(float), cudaMemcpyHostToDevice);
    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cuda_gradient_clip(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* gradients,
    float max_norm)
{
    float total_norm;
    nimcp_gpu_tensor_t* grad_array[1] = { gradients };
    if (!nimcp_gpu_gradient_clip_norm(ctx, grad_array, 1, max_norm, &total_norm)) {
        return NIMCP_KERNEL_ERROR_DEVICE;
    }
    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cuda_gradient_accumulate(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* accumulated,
    const nimcp_gpu_tensor_t* gradients,
    float scale)
{
    // Scale gradients first, then accumulate
    if (!nimcp_gpu_gradient_scale(ctx, (nimcp_gpu_tensor_t*)gradients, scale)) {
        return NIMCP_KERNEL_ERROR_DEVICE;
    }
    if (!nimcp_gpu_gradient_accumulate(ctx, gradients, accumulated)) {
        return NIMCP_KERNEL_ERROR_DEVICE;
    }
    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cuda_sgd_step(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* params,
    const nimcp_gpu_tensor_t* gradients,
    float learning_rate,
    float momentum,
    nimcp_gpu_tensor_t* velocity)
{
    // Create temporary optimizer state for the step
    nimcp_optim_state_t state;
    memset(&state, 0, sizeof(state));  // Zero-initialize to avoid undefined values
    state.type = (momentum > 0.0f) ? NIMCP_OPTIM_SGD_MOMENTUM : NIMCP_OPTIM_SGD;
    state.m = velocity;
    state.v = NULL;
    state.lr = learning_rate;
    state.momentum = momentum;
    state.weight_decay = 0.0f;
    state.nesterov = false;
    state.t = 1;

    if (!nimcp_gpu_optim_sgd(ctx, params, gradients, &state)) {
        return NIMCP_KERNEL_ERROR_DEVICE;
    }
    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cuda_adam_step(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* params,
    const nimcp_gpu_tensor_t* gradients,
    nimcp_gpu_tensor_t* m,
    nimcp_gpu_tensor_t* v,
    float learning_rate,
    float beta1, float beta2,
    float epsilon, uint64_t t)
{
    nimcp_optim_state_t state;
    state.type = NIMCP_OPTIM_ADAM;
    state.m = m;
    state.v = v;
    state.lr = learning_rate;
    state.beta1 = beta1;
    state.beta2 = beta2;
    state.eps = epsilon;
    state.t = t;
    state.weight_decay = 0.0f;

    if (!nimcp_gpu_optim_adam(ctx, params, gradients, &state)) {
        return NIMCP_KERNEL_ERROR_DEVICE;
    }
    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cuda_backward_linear(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* grad_output,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* weights,
    nimcp_gpu_tensor_t* grad_input,
    nimcp_gpu_tensor_t* grad_weights,
    nimcp_gpu_tensor_t* grad_bias)
{
    if (!nimcp_gpu_backward_linear(ctx, input, weights, grad_output,
                                    grad_input, grad_weights, grad_bias)) {
        return NIMCP_KERNEL_ERROR_DEVICE;
    }
    return NIMCP_KERNEL_SUCCESS;
}

//=============================================================================
// CUDA SNN Operations Wrappers
//=============================================================================

static nimcp_kernel_error_t cuda_lif_forward(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* membrane,
    nimcp_gpu_tensor_t* spikes,
    float tau, float threshold,
    float reset, float dt)
{
    // Create temporary LIF state from tensors
    nimcp_lif_params_t params;
    params.tau_mem = tau;
    params.tau_syn = tau;
    params.v_thresh = threshold;
    params.v_reset = reset;
    params.v_rest = reset;
    params.dt = dt;
    params.hard_reset = true;

    nimcp_lif_state_t state;
    state.v = membrane;
    state.spikes = spikes;
    state.i_syn = NULL;  // Not used in simple forward
    state.params = params;

    if (!nimcp_gpu_lif_forward(ctx, &state, input)) {
        return NIMCP_KERNEL_ERROR_DEVICE;
    }
    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cuda_izhikevich_forward(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* v,
    nimcp_gpu_tensor_t* u,
    nimcp_gpu_tensor_t* spikes,
    float a, float b, float c, float d,
    float dt)
{
    nimcp_izhikevich_params_t params;
    params.a = a;
    params.b = b;
    params.c = c;
    params.d = d;
    params.v_thresh = 30.0f;
    params.dt = dt;

    nimcp_izhikevich_state_t state;
    state.v = v;
    state.u = u;
    state.spikes = spikes;
    state.params = params;

    if (!nimcp_gpu_izhikevich_forward(ctx, &state, input)) {
        return NIMCP_KERNEL_ERROR_DEVICE;
    }
    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cuda_surrogate_superspike(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output,
    float beta)
{
    if (!nimcp_gpu_surrogate_gradient(ctx, input, 0.0f, output,
                                       NIMCP_SURROGATE_SUPERSPIKE, beta)) {
        return NIMCP_KERNEL_ERROR_DEVICE;
    }
    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cuda_surrogate_fast_sigmoid(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output,
    float slope)
{
    if (!nimcp_gpu_surrogate_gradient(ctx, input, 0.0f, output,
                                       NIMCP_SURROGATE_FAST_SIGMOID, slope)) {
        return NIMCP_KERNEL_ERROR_DEVICE;
    }
    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cuda_stdp_update(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* weights,
    const nimcp_gpu_tensor_t* pre_spikes,
    const nimcp_gpu_tensor_t* post_spikes,
    nimcp_gpu_tensor_t* pre_trace,
    nimcp_gpu_tensor_t* post_trace,
    float A_plus, float A_minus,
    float tau_plus, float tau_minus,
    float dt)
{
    // Update traces
    float decay_pre = expf(-dt / tau_plus);
    float decay_post = expf(-dt / tau_minus);

    if (!nimcp_gpu_eligibility_trace_update(ctx, pre_trace, pre_spikes, decay_pre)) {
        return NIMCP_KERNEL_ERROR_DEVICE;
    }
    if (!nimcp_gpu_eligibility_trace_update(ctx, post_trace, post_spikes, decay_post)) {
        return NIMCP_KERNEL_ERROR_DEVICE;
    }

    // Apply STDP
    nimcp_stdp_params_t params;
    params.A_plus = A_plus;
    params.A_minus = A_minus;
    params.tau_plus = tau_plus;
    params.tau_minus = tau_minus;
    params.w_max = 1.0f;
    params.w_min = 0.0f;

    if (!nimcp_gpu_stdp_pair(ctx, weights, pre_spikes, post_spikes,
                             pre_trace, post_trace, &params)) {
        return NIMCP_KERNEL_ERROR_DEVICE;
    }
    return NIMCP_KERNEL_SUCCESS;
}

//=============================================================================
// CUDA CNN Operations Wrappers (placeholder - full impl in conv kernels)
//=============================================================================

// Note: These would call into nimcp_cnn_gpu.h functions when implemented
// For now, returning NOT_IMPLEMENTED to indicate CNN ops need separate header

static nimcp_kernel_error_t cuda_conv2d_forward(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* kernel,
    const nimcp_gpu_tensor_t* bias,
    nimcp_gpu_tensor_t* output,
    uint32_t stride, uint32_t padding)
{
    // Full implementation in nimcp_conv_kernels.cu
    // This will be connected when CNN header is available
    LOG_DEBUG("CUDA conv2d forward called - full implementation in conv kernels");
    return NIMCP_KERNEL_ERROR_NOT_IMPLEMENTED;
}

static nimcp_kernel_error_t cuda_conv2d_backward(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* grad_output,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* kernel,
    nimcp_gpu_tensor_t* grad_input,
    nimcp_gpu_tensor_t* grad_kernel,
    nimcp_gpu_tensor_t* grad_bias,
    uint32_t stride, uint32_t padding)
{
    LOG_DEBUG("CUDA conv2d backward called - full implementation in conv kernels");
    return NIMCP_KERNEL_ERROR_NOT_IMPLEMENTED;
}

static nimcp_kernel_error_t cuda_maxpool2d(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output,
    nimcp_gpu_tensor_t* indices,
    uint32_t kernel_size, uint32_t stride)
{
    LOG_DEBUG("CUDA maxpool2d called - full implementation in conv kernels");
    return NIMCP_KERNEL_ERROR_NOT_IMPLEMENTED;
}

static nimcp_kernel_error_t cuda_avgpool2d(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output,
    uint32_t kernel_size, uint32_t stride)
{
    LOG_DEBUG("CUDA avgpool2d called - full implementation in conv kernels");
    return NIMCP_KERNEL_ERROR_NOT_IMPLEMENTED;
}

static nimcp_kernel_error_t cuda_batchnorm_forward(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* gamma,
    const nimcp_gpu_tensor_t* beta,
    nimcp_gpu_tensor_t* output,
    nimcp_gpu_tensor_t* mean,
    nimcp_gpu_tensor_t* var,
    float epsilon, bool training)
{
    LOG_DEBUG("CUDA batchnorm forward called - full implementation in conv kernels");
    return NIMCP_KERNEL_ERROR_NOT_IMPLEMENTED;
}

//=============================================================================
// CUDA LNN Operations Wrappers
//=============================================================================

static nimcp_kernel_error_t cuda_lnn_euler_step(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    const nimcp_gpu_tensor_t* dx_dt,
    float dt,
    nimcp_gpu_tensor_t* x_new)
{
    if (!nimcp_gpu_lnn_euler_step(ctx, x, dx_dt, dt, x_new)) {
        return NIMCP_KERNEL_ERROR_DEVICE;
    }
    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cuda_lnn_heun_step(
    nimcp_gpu_context_t* ctx,
    struct nimcp_lnn_layer_gpu* layer,
    const nimcp_gpu_tensor_t* input,
    float dt,
    const struct nimcp_lnn_ode_config* config)
{
    if (!nimcp_gpu_lnn_heun_step(ctx, (nimcp_lnn_layer_gpu_t*)layer, input, dt,
                                  (const nimcp_lnn_ode_config_t*)config)) {
        return NIMCP_KERNEL_ERROR_DEVICE;
    }
    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cuda_lnn_rk4_step(
    nimcp_gpu_context_t* ctx,
    struct nimcp_lnn_layer_gpu* layer,
    const nimcp_gpu_tensor_t* input,
    float dt,
    const struct nimcp_lnn_ode_config* config)
{
    if (!nimcp_gpu_lnn_rk4_step(ctx, (nimcp_lnn_layer_gpu_t*)layer, input, dt,
                                 (const nimcp_lnn_ode_config_t*)config)) {
        return NIMCP_KERNEL_ERROR_DEVICE;
    }
    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cuda_lnn_dopri5_step(
    nimcp_gpu_context_t* ctx,
    struct nimcp_lnn_layer_gpu* layer,
    const nimcp_gpu_tensor_t* input,
    float* dt_ptr,
    const struct nimcp_lnn_ode_config* config)
{
    if (!nimcp_gpu_lnn_dopri5_step(ctx, (nimcp_lnn_layer_gpu_t*)layer, input, dt_ptr,
                                    (const nimcp_lnn_ode_config_t*)config)) {
        return NIMCP_KERNEL_ERROR_DEVICE;
    }
    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cuda_lnn_compute_derivative(
    nimcp_gpu_context_t* ctx,
    struct nimcp_lnn_layer_gpu* layer,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* dx_dt)
{
    if (!nimcp_gpu_lnn_compute_derivative(ctx, (nimcp_lnn_layer_gpu_t*)layer,
                                           input, dx_dt)) {
        return NIMCP_KERNEL_ERROR_DEVICE;
    }
    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cuda_lnn_update_tau(
    nimcp_gpu_context_t* ctx,
    struct nimcp_lnn_layer_gpu* layer,
    const nimcp_gpu_tensor_t* input)
{
    if (!nimcp_gpu_lnn_update_tau(ctx, (nimcp_lnn_layer_gpu_t*)layer, input)) {
        return NIMCP_KERNEL_ERROR_DEVICE;
    }
    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cuda_sparse_matvec(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* row_ptr,
    const nimcp_gpu_tensor_t* col_idx,
    const nimcp_gpu_tensor_t* values,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* y,
    uint32_t n_rows,
    float alpha)
{
    if (!nimcp_gpu_sparse_matvec(ctx, row_ptr, col_idx, values, x, y, n_rows, alpha)) {
        return NIMCP_KERNEL_ERROR_DEVICE;
    }
    return NIMCP_KERNEL_SUCCESS;
}

//=============================================================================
// CUDA Quantum Operations Wrappers
//=============================================================================

static struct nimcp_quantum_state* cuda_quantum_state_create(
    nimcp_gpu_context_t* ctx,
    uint32_t n_qubits)
{
    return (struct nimcp_quantum_state*)nimcp_quantum_state_create(ctx, n_qubits);
}

static void cuda_quantum_state_destroy(struct nimcp_quantum_state* state)
{
    nimcp_quantum_state_destroy((nimcp_quantum_state_t*)state);
}

static nimcp_kernel_error_t cuda_quantum_state_hadamard_all(
    nimcp_gpu_context_t* ctx,
    struct nimcp_quantum_state* state)
{
    if (!nimcp_quantum_state_hadamard_all(ctx, (nimcp_quantum_state_t*)state)) {
        return NIMCP_KERNEL_ERROR_DEVICE;
    }
    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cuda_quantum_apply_gate(
    nimcp_gpu_context_t* ctx,
    struct nimcp_quantum_state* state,
    uint32_t qubit_idx,
    const float gate_real[2][2],
    const float gate_imag[2][2])
{
    if (!nimcp_quantum_apply_gate(ctx, (nimcp_quantum_state_t*)state,
                                   qubit_idx, gate_real, gate_imag)) {
        return NIMCP_KERNEL_ERROR_DEVICE;
    }
    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cuda_quantum_measure(
    nimcp_gpu_context_t* ctx,
    struct nimcp_quantum_state* state,
    uint32_t* measured_state,
    float* probability)
{
    if (!nimcp_quantum_measure(ctx, (nimcp_quantum_state_t*)state,
                                measured_state, probability)) {
        return NIMCP_KERNEL_ERROR_DEVICE;
    }
    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cuda_grover_oracle(
    nimcp_gpu_context_t* ctx,
    struct nimcp_quantum_state* state,
    const uint32_t* marked_states,
    uint32_t n_marked)
{
    if (!nimcp_grover_oracle(ctx, (nimcp_quantum_state_t*)state,
                              marked_states, n_marked)) {
        return NIMCP_KERNEL_ERROR_DEVICE;
    }
    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cuda_grover_diffusion(
    nimcp_gpu_context_t* ctx,
    struct nimcp_quantum_state* state)
{
    if (!nimcp_grover_diffusion(ctx, (nimcp_quantum_state_t*)state)) {
        return NIMCP_KERNEL_ERROR_DEVICE;
    }
    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cuda_grover_search(
    nimcp_gpu_context_t* ctx,
    const struct nimcp_grover_config* config,
    uint32_t* found_state,
    bool* success)
{
    if (!nimcp_grover_search(ctx, (const nimcp_grover_config_t*)config,
                              found_state, success)) {
        return NIMCP_KERNEL_ERROR_DEVICE;
    }
    return NIMCP_KERNEL_SUCCESS;
}

static struct nimcp_ising_model* cuda_ising_create(
    nimcp_gpu_context_t* ctx,
    uint32_t n_spins)
{
    return (struct nimcp_ising_model*)nimcp_ising_model_create(ctx, n_spins);
}

static void cuda_ising_destroy(struct nimcp_ising_model* model)
{
    nimcp_ising_model_destroy((nimcp_ising_model_t*)model);
}

static float cuda_quantum_anneal(
    nimcp_gpu_context_t* ctx,
    struct nimcp_ising_model* model,
    const struct nimcp_annealing_config* config)
{
    return nimcp_quantum_anneal(ctx, (nimcp_ising_model_t*)model,
                                 (const nimcp_annealing_config_t*)config);
}

//=============================================================================
// CUDA Inference Operations Wrappers
//=============================================================================

static nimcp_kernel_error_t cuda_linear_relu(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* weights,
    const nimcp_gpu_tensor_t* bias,
    nimcp_gpu_tensor_t* output)
{
    if (!nimcp_gpu_infer_linear_relu(ctx, input, weights, bias, output)) {
        return NIMCP_KERNEL_ERROR_DEVICE;
    }
    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cuda_conv_bn_relu(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* kernel,
    const nimcp_gpu_tensor_t* bn_params,
    nimcp_gpu_tensor_t* output,
    uint32_t stride, uint32_t padding)
{
    // bn_params contains [gamma, beta, mean, var] packed
    // For now, use the simplified API
    LOG_DEBUG("CUDA conv_bn_relu - requires unpacked bn_params");
    return NIMCP_KERNEL_ERROR_NOT_IMPLEMENTED;
}

static nimcp_kernel_error_t cuda_quantize_int8(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output,
    float scale, int8_t zero_point)
{
    nimcp_quant_params_t params;
    params.scale = scale;
    params.zero_point = zero_point;
    params.min_val = -128.0f * scale;
    params.max_val = 127.0f * scale;

    if (!nimcp_gpu_infer_quantize_int8(ctx, input, output, &params)) {
        return NIMCP_KERNEL_ERROR_DEVICE;
    }
    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cuda_dequantize_int8(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output,
    float scale, int8_t zero_point)
{
    nimcp_quant_params_t params;
    params.scale = scale;
    params.zero_point = zero_point;
    params.min_val = -128.0f * scale;
    params.max_val = 127.0f * scale;

    if (!nimcp_gpu_infer_dequantize_int8(ctx, input, output, &params)) {
        return NIMCP_KERNEL_ERROR_DEVICE;
    }
    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cuda_matmul_int8(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* a,
    const nimcp_gpu_tensor_t* b,
    nimcp_gpu_tensor_t* result,
    float scale_a, float scale_b,
    float scale_out)
{
    nimcp_quant_params_t params_a = { scale_a, 0, 0, 0 };
    nimcp_quant_params_t params_b = { scale_b, 0, 0, 0 };
    nimcp_quant_params_t params_c = { scale_out, 0, 0, 0 };

    if (!nimcp_gpu_infer_gemm_int8(ctx, a, b, result, &params_a, &params_b, &params_c)) {
        return NIMCP_KERNEL_ERROR_DEVICE;
    }
    return NIMCP_KERNEL_SUCCESS;
}

//=============================================================================
// Backend Registration
//=============================================================================

extern "C" void init_cuda_backend_ops(nimcp_kernel_backend_t* backend)
{
    if (!backend) return;

    // Initialize GPU recovery system if not already initialized
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    LOG_INFO("Registering CUDA backend operations");

    // Tensor operations
    backend->tensor.add = cuda_tensor_add;
    backend->tensor.sub = cuda_tensor_sub;
    backend->tensor.mul = cuda_tensor_mul;
    backend->tensor.div = cuda_tensor_div;
    backend->tensor.scale = cuda_tensor_scale;
    backend->tensor.matmul = cuda_tensor_matmul;
    backend->tensor.transpose = cuda_tensor_transpose;
    backend->tensor.relu = cuda_tensor_relu;
    backend->tensor.sigmoid = cuda_tensor_sigmoid;
    backend->tensor.tanh = cuda_tensor_tanh;
    backend->tensor.softmax = cuda_tensor_softmax;
    backend->tensor.sum = cuda_tensor_sum;
    backend->tensor.mean = cuda_tensor_mean;
    backend->tensor.max = cuda_tensor_max;
    backend->tensor.min = cuda_tensor_min;

    // Training operations
    backend->training.mse_loss = cuda_mse_loss;
    backend->training.cross_entropy_loss = cuda_cross_entropy_loss;
    backend->training.gradient_clip = cuda_gradient_clip;
    backend->training.gradient_accumulate = cuda_gradient_accumulate;
    backend->training.sgd_step = cuda_sgd_step;
    backend->training.adam_step = cuda_adam_step;
    backend->training.backward_linear = cuda_backward_linear;

    // SNN operations
    backend->snn.lif_forward = cuda_lif_forward;
    backend->snn.izhikevich_forward = cuda_izhikevich_forward;
    backend->snn.surrogate_superspike = cuda_surrogate_superspike;
    backend->snn.surrogate_fast_sigmoid = cuda_surrogate_fast_sigmoid;
    backend->snn.stdp_update = cuda_stdp_update;

    // CNN operations (placeholders - full impl in separate kernels)
    backend->cnn.conv2d_forward = cuda_conv2d_forward;
    backend->cnn.conv2d_backward = cuda_conv2d_backward;
    backend->cnn.maxpool2d = cuda_maxpool2d;
    backend->cnn.avgpool2d = cuda_avgpool2d;
    backend->cnn.batchnorm_forward = cuda_batchnorm_forward;

    // LNN operations
    backend->lnn.euler_step = cuda_lnn_euler_step;
    backend->lnn.heun_step = cuda_lnn_heun_step;
    backend->lnn.rk4_step = cuda_lnn_rk4_step;
    backend->lnn.dopri5_step = cuda_lnn_dopri5_step;
    backend->lnn.compute_derivative = cuda_lnn_compute_derivative;
    backend->lnn.update_tau = cuda_lnn_update_tau;
    backend->lnn.sparse_matvec = cuda_sparse_matvec;

    // Quantum operations
    backend->quantum.state_create = cuda_quantum_state_create;
    backend->quantum.state_destroy = cuda_quantum_state_destroy;
    backend->quantum.state_hadamard_all = cuda_quantum_state_hadamard_all;
    backend->quantum.apply_gate = cuda_quantum_apply_gate;
    backend->quantum.measure = cuda_quantum_measure;
    backend->quantum.grover_oracle = cuda_grover_oracle;
    backend->quantum.grover_diffusion = cuda_grover_diffusion;
    backend->quantum.grover_search = cuda_grover_search;
    backend->quantum.ising_create = cuda_ising_create;
    backend->quantum.ising_destroy = cuda_ising_destroy;
    backend->quantum.quantum_anneal = cuda_quantum_anneal;

    // Inference operations (some not implemented yet)
    backend->inference.linear_relu = cuda_linear_relu;
    backend->inference.conv_bn_relu = cuda_conv_bn_relu;
    backend->inference.quantize_int8 = cuda_quantize_int8;
    backend->inference.dequantize_int8 = cuda_dequantize_int8;
    backend->inference.matmul_int8 = cuda_matmul_int8;

    // Substrate operations (axons, dendrites, myelin, neuromodulators, glial, metabolic)
    extern nimcp_substrate_ops_t nimcp_cuda_substrate_ops;
    backend->substrate = nimcp_cuda_substrate_ops;

    LOG_INFO("CUDA backend operations registered successfully");
}

#endif // NIMCP_ENABLE_CUDA
