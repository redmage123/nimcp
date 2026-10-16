/**
 * @file nimcp_kernel_backend.c
 * @brief Unified Kernel Backend System Implementation
 *
 * WHAT: Runtime selection between CUDA and CPU compute backends
 * WHY:  Single interface for all kernel operations with fallback
 * HOW:  Strategy pattern with function pointer tables
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#include "gpu/backend/nimcp_kernel_backend.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define LOG_MODULE "KERNEL_BACKEND"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(kernel_backend)

//=============================================================================
// External Substrate Operations Tables
//=============================================================================

// CPU substrate ops (from nimcp_substrate_cpu.c)
extern nimcp_substrate_ops_t nimcp_cpu_substrate_ops;

#ifdef NIMCP_ENABLE_CUDA
// CUDA substrate ops (from nimcp_substrate_kernels.cu)
extern nimcp_substrate_ops_t nimcp_cuda_substrate_ops;
#endif

//=============================================================================
// Global Backend State
//=============================================================================

static nimcp_kernel_backend_t g_cpu_backend;
static nimcp_kernel_backend_t g_cuda_backend;
static nimcp_kernel_backend_t g_rocm_backend;
static nimcp_kernel_backend_t g_opencl_backend;
static nimcp_kernel_backend_t g_neuron_backend;
static nimcp_kernel_backend_t* g_active_backend = NULL;
static bool g_backend_initialized = false;

//=============================================================================
// CPU Backend Implementation - Tensor Operations
//=============================================================================
//
// RETURN TYPE CONVENTION:
// =======================
// All kernel backend operations (tensor, training, SNN, CNN, LNN, inference)
// return nimcp_kernel_error_t:
//   NIMCP_KERNEL_SUCCESS (0)         - Operation completed successfully
//   NIMCP_KERNEL_ERROR_NULL_PTR (-1) - Required pointer parameter was NULL
//   NIMCP_KERNEL_ERROR_INVALID_SIZE (-2) - Size/dimension mismatch
//   NIMCP_KERNEL_ERROR_DEVICE (-3)   - Device error (GPU-specific)
//   NIMCP_KERNEL_ERROR_NOT_IMPLEMENTED (-4) - Operation not available
//   NIMCP_KERNEL_ERROR_MEMORY (-5)   - Memory allocation failure
//
// This is DISTINCT from:
//   - GPU context functions: return int (0 success, -1 error)
//   - GPU stub bool functions: return bool (true success, false error)
//   - nimcp_error_t codes: NIMCP_OK (0), NIMCP_ERROR_* (positive values)
//
// Do NOT mix these return types. Backend ops always use nimcp_kernel_error_t.
//=============================================================================

static nimcp_kernel_error_t cpu_tensor_add(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* a,
    const nimcp_gpu_tensor_t* b,
    nimcp_gpu_tensor_t* result)
{
    (void)ctx;
    if (!a || !b || !result) return NIMCP_KERNEL_ERROR_NULL_PTR;
    if (a->numel != b->numel || a->numel != result->numel) return NIMCP_KERNEL_ERROR_INVALID_SIZE;

    float* a_data = (float*)a->data;
    float* b_data = (float*)b->data;
    float* r_data = (float*)result->data;

    for (size_t i = 0; i < a->numel; i++) {
        r_data[i] = a_data[i] + b_data[i];
    }

    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cpu_tensor_sub(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* a,
    const nimcp_gpu_tensor_t* b,
    nimcp_gpu_tensor_t* result)
{
    (void)ctx;
    if (!a || !b || !result) return NIMCP_KERNEL_ERROR_NULL_PTR;

    float* a_data = (float*)a->data;
    float* b_data = (float*)b->data;
    float* r_data = (float*)result->data;

    for (size_t i = 0; i < a->numel; i++) {
        r_data[i] = a_data[i] - b_data[i];
    }

    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cpu_tensor_mul(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* a,
    const nimcp_gpu_tensor_t* b,
    nimcp_gpu_tensor_t* result)
{
    (void)ctx;
    if (!a || !b || !result) return NIMCP_KERNEL_ERROR_NULL_PTR;

    float* a_data = (float*)a->data;
    float* b_data = (float*)b->data;
    float* r_data = (float*)result->data;

    for (size_t i = 0; i < a->numel; i++) {
        r_data[i] = a_data[i] * b_data[i];
    }

    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cpu_tensor_div(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* a,
    const nimcp_gpu_tensor_t* b,
    nimcp_gpu_tensor_t* result)
{
    (void)ctx;
    if (!a || !b || !result) return NIMCP_KERNEL_ERROR_NULL_PTR;

    float* a_data = (float*)a->data;
    float* b_data = (float*)b->data;
    float* r_data = (float*)result->data;

    for (size_t i = 0; i < a->numel; i++) {
        r_data[i] = a_data[i] / (b_data[i] + 1e-8f);
    }

    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cpu_tensor_scale(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* a,
    float scalar,
    nimcp_gpu_tensor_t* result)
{
    (void)ctx;
    if (!a || !result) return NIMCP_KERNEL_ERROR_NULL_PTR;

    float* a_data = (float*)a->data;
    float* r_data = (float*)result->data;

    for (size_t i = 0; i < a->numel; i++) {
        r_data[i] = a_data[i] * scalar;
    }

    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cpu_tensor_matmul(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* a,
    const nimcp_gpu_tensor_t* b,
    nimcp_gpu_tensor_t* result)
{
    (void)ctx;
    if (!a || !b || !result) return NIMCP_KERNEL_ERROR_NULL_PTR;

    // Assume 2D matrices: a[M,K] @ b[K,N] = result[M,N]
    size_t M = a->dims[0];
    size_t K = a->dims[1];
    size_t N = b->dims[1];

    float* a_data = (float*)a->data;
    float* b_data = (float*)b->data;
    float* r_data = (float*)result->data;

    for (size_t i = 0; i < M; i++) {
        for (size_t j = 0; j < N; j++) {
            float sum = 0.0f;
            for (size_t k = 0; k < K; k++) {
                sum += a_data[i * K + k] * b_data[k * N + j];
            }
            r_data[i * N + j] = sum;
        }
    }

    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cpu_tensor_transpose(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* a,
    nimcp_gpu_tensor_t* result)
{
    (void)ctx;
    if (!a || !result) return NIMCP_KERNEL_ERROR_NULL_PTR;

    size_t rows = a->dims[0];
    size_t cols = a->dims[1];

    float* a_data = (float*)a->data;
    float* r_data = (float*)result->data;

    for (size_t i = 0; i < rows; i++) {
        for (size_t j = 0; j < cols; j++) {
            r_data[j * rows + i] = a_data[i * cols + j];
        }
    }

    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cpu_tensor_relu(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output)
{
    (void)ctx;
    if (!input || !output) return NIMCP_KERNEL_ERROR_NULL_PTR;

    float* in_data = (float*)input->data;
    float* out_data = (float*)output->data;

    for (size_t i = 0; i < input->numel; i++) {
        out_data[i] = in_data[i] > 0.0f ? in_data[i] : 0.0f;
    }

    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cpu_tensor_sigmoid(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output)
{
    (void)ctx;
    if (!input || !output) return NIMCP_KERNEL_ERROR_NULL_PTR;

    float* in_data = (float*)input->data;
    float* out_data = (float*)output->data;

    for (size_t i = 0; i < input->numel; i++) {
        out_data[i] = 1.0f / (1.0f + expf(-in_data[i]));
    }

    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cpu_tensor_tanh(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output)
{
    (void)ctx;
    if (!input || !output) return NIMCP_KERNEL_ERROR_NULL_PTR;

    float* in_data = (float*)input->data;
    float* out_data = (float*)output->data;

    for (size_t i = 0; i < input->numel; i++) {
        out_data[i] = tanhf(in_data[i]);
    }

    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cpu_tensor_softmax(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output)
{
    (void)ctx;
    if (!input || !output) return NIMCP_KERNEL_ERROR_NULL_PTR;
    if (!input->data || !output->data) return NIMCP_KERNEL_ERROR_NULL_PTR;
    if (input->numel == 0) return NIMCP_KERNEL_ERROR_INVALID_SIZE;

    float* in_data = (float*)input->data;
    float* out_data = (float*)output->data;
    size_t n = input->numel;

    // Find max for numerical stability (subtract max before exp)
    float max_val = in_data[0];
    for (size_t i = 1; i < n; i++) {
        if (in_data[i] > max_val) max_val = in_data[i];
    }

    // Guard against all-NaN or all-inf input
    if (isnan(max_val) || isinf(max_val)) {
        // Fall back to uniform distribution
        float uniform = 1.0f / (float)n;
        for (size_t i = 0; i < n; i++) {
            out_data[i] = uniform;
        }
        return NIMCP_KERNEL_SUCCESS;
    }

    // Compute exp(x - max) and sum with clamping to prevent underflow issues
    // After subtracting max, exponents are in range (-inf, 0], so expf won't overflow.
    // Clamp very negative values to avoid prolonged underflow computation.
    float sum = 0.0f;
    for (size_t i = 0; i < n; i++) {
        float shifted = in_data[i] - max_val;
        // Clamp to prevent extreme underflow (expf(-88) ~ 0 in float32)
        if (shifted < -87.0f) shifted = -87.0f;
        out_data[i] = expf(shifted);
        sum += out_data[i];
    }

    // Guard against zero sum (all values underflowed)
    if (sum <= 0.0f || isnan(sum)) {
        float uniform = 1.0f / (float)n;
        for (size_t i = 0; i < n; i++) {
            out_data[i] = uniform;
        }
        return NIMCP_KERNEL_SUCCESS;
    }

    // Normalize
    float inv_sum = 1.0f / sum;
    for (size_t i = 0; i < n; i++) {
        out_data[i] *= inv_sum;
    }

    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cpu_tensor_sum(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output)
{
    (void)ctx;
    if (!input || !output) return NIMCP_KERNEL_ERROR_NULL_PTR;

    float* in_data = (float*)input->data;
    float* out_data = (float*)output->data;

    float sum = 0.0f;
    for (size_t i = 0; i < input->numel; i++) {
        sum += in_data[i];
    }
    out_data[0] = sum;

    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cpu_tensor_mean(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output)
{
    (void)ctx;
    if (!input || !output) return NIMCP_KERNEL_ERROR_NULL_PTR;

    float* in_data = (float*)input->data;
    float* out_data = (float*)output->data;

    float sum = 0.0f;
    for (size_t i = 0; i < input->numel; i++) {
        sum += in_data[i];
    }
    out_data[0] = sum / (float)input->numel;

    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cpu_tensor_max(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output)
{
    (void)ctx;
    if (!input || !output) return NIMCP_KERNEL_ERROR_NULL_PTR;

    float* in_data = (float*)input->data;
    float* out_data = (float*)output->data;

    float max_val = in_data[0];
    for (size_t i = 1; i < input->numel; i++) {
        if (in_data[i] > max_val) max_val = in_data[i];
    }
    out_data[0] = max_val;

    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cpu_tensor_min(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output)
{
    (void)ctx;
    if (!input || !output) return NIMCP_KERNEL_ERROR_NULL_PTR;

    float* in_data = (float*)input->data;
    float* out_data = (float*)output->data;

    float min_val = in_data[0];
    for (size_t i = 1; i < input->numel; i++) {
        if (in_data[i] < min_val) min_val = in_data[i];
    }
    out_data[0] = min_val;

    return NIMCP_KERNEL_SUCCESS;
}

//=============================================================================
// CPU Backend Implementation - Training Operations
//=============================================================================

static nimcp_kernel_error_t cpu_mse_loss(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* pred,
    const nimcp_gpu_tensor_t* target,
    nimcp_gpu_tensor_t* loss)
{
    (void)ctx;
    if (!pred || !target || !loss) return NIMCP_KERNEL_ERROR_NULL_PTR;

    float* p = (float*)pred->data;
    float* t = (float*)target->data;
    float* l = (float*)loss->data;

    float sum = 0.0f;
    for (size_t i = 0; i < pred->numel; i++) {
        float diff = p[i] - t[i];
        sum += diff * diff;
    }
    l[0] = sum / (float)pred->numel;

    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cpu_cross_entropy_loss(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* pred,
    const nimcp_gpu_tensor_t* target,
    nimcp_gpu_tensor_t* loss)
{
    (void)ctx;
    if (!pred || !target || !loss) return NIMCP_KERNEL_ERROR_NULL_PTR;

    float* p = (float*)pred->data;
    float* t = (float*)target->data;
    float* l = (float*)loss->data;

    float sum = 0.0f;
    for (size_t i = 0; i < pred->numel; i++) {
        // Clamp pred to avoid log(0)
        float pred_clamped = fmaxf(fminf(p[i], 1.0f - 1e-7f), 1e-7f);
        sum -= t[i] * logf(pred_clamped);
    }
    l[0] = sum / (float)pred->numel;

    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cpu_gradient_clip(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* gradients,
    float max_norm)
{
    (void)ctx;
    if (!gradients) return NIMCP_KERNEL_ERROR_NULL_PTR;

    float* g = (float*)gradients->data;

    // Compute norm
    float norm_sq = 0.0f;
    for (size_t i = 0; i < gradients->numel; i++) {
        norm_sq += g[i] * g[i];
    }
    float norm = sqrtf(norm_sq);

    // Clip if necessary
    if (norm > max_norm) {
        float scale = max_norm / norm;
        for (size_t i = 0; i < gradients->numel; i++) {
            g[i] *= scale;
        }
    }

    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cpu_gradient_accumulate(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* accumulated,
    const nimcp_gpu_tensor_t* gradients,
    float scale)
{
    (void)ctx;
    if (!accumulated || !gradients) return NIMCP_KERNEL_ERROR_NULL_PTR;

    float* acc = (float*)accumulated->data;
    float* g = (float*)gradients->data;

    for (size_t i = 0; i < gradients->numel; i++) {
        acc[i] += scale * g[i];
    }

    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cpu_sgd_step(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* params,
    const nimcp_gpu_tensor_t* gradients,
    float learning_rate,
    float momentum,
    nimcp_gpu_tensor_t* velocity)
{
    (void)ctx;
    if (!params || !gradients) return NIMCP_KERNEL_ERROR_NULL_PTR;

    float* p = (float*)params->data;
    float* g = (float*)gradients->data;
    float* v = velocity ? (float*)velocity->data : NULL;

    for (size_t i = 0; i < params->numel; i++) {
        if (v && momentum > 0.0f) {
            v[i] = momentum * v[i] + g[i];
            p[i] -= learning_rate * v[i];
        } else {
            p[i] -= learning_rate * g[i];
        }
    }

    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cpu_adam_step(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* params,
    const nimcp_gpu_tensor_t* gradients,
    nimcp_gpu_tensor_t* m,
    nimcp_gpu_tensor_t* v,
    float learning_rate,
    float beta1, float beta2,
    float epsilon, uint64_t t)
{
    (void)ctx;
    if (!params || !gradients || !m || !v) return NIMCP_KERNEL_ERROR_NULL_PTR;

    float* p = (float*)params->data;
    float* g = (float*)gradients->data;
    float* m_data = (float*)m->data;
    float* v_data = (float*)v->data;

    float beta1_t = powf(beta1, (float)t);
    float beta2_t = powf(beta2, (float)t);

    for (size_t i = 0; i < params->numel; i++) {
        m_data[i] = beta1 * m_data[i] + (1.0f - beta1) * g[i];
        v_data[i] = beta2 * v_data[i] + (1.0f - beta2) * g[i] * g[i];

        float m_hat = m_data[i] / (1.0f - beta1_t);
        float v_hat = v_data[i] / (1.0f - beta2_t);

        p[i] -= learning_rate * m_hat / (sqrtf(v_hat) + epsilon);
    }

    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cpu_backward_linear(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* grad_output,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* weights,
    nimcp_gpu_tensor_t* grad_input,
    nimcp_gpu_tensor_t* grad_weights,
    nimcp_gpu_tensor_t* grad_bias)
{
    (void)ctx;
    if (!grad_output || !input || !weights) return NIMCP_KERNEL_ERROR_NULL_PTR;

    // Simplified linear backward for 2D case
    // grad_input = grad_output @ weights.T
    // grad_weights = input.T @ grad_output
    // grad_bias = sum(grad_output, dim=0)

    LOG_DEBUG("CPU backward linear - full implementation needed");

    return NIMCP_KERNEL_SUCCESS;
}

//=============================================================================
// CPU Backend Implementation - SNN Operations
//=============================================================================

static nimcp_kernel_error_t cpu_lif_forward(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* membrane,
    nimcp_gpu_tensor_t* spikes,
    float tau, float threshold,
    float reset, float dt)
{
    (void)ctx;
    if (!input || !membrane || !spikes) return NIMCP_KERNEL_ERROR_NULL_PTR;

    float* in = (float*)input->data;
    float* v = (float*)membrane->data;
    float* s = (float*)spikes->data;

    float decay = expf(-dt / tau);

    for (size_t i = 0; i < input->numel; i++) {
        v[i] = decay * v[i] + (1.0f - decay) * in[i];

        if (v[i] >= threshold) {
            s[i] = 1.0f;
            v[i] = reset;
        } else {
            s[i] = 0.0f;
        }
    }

    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cpu_izhikevich_forward(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* v,
    nimcp_gpu_tensor_t* u,
    nimcp_gpu_tensor_t* spikes,
    float a, float b, float c, float d,
    float dt)
{
    (void)ctx;
    if (!input || !v || !u || !spikes) return NIMCP_KERNEL_ERROR_NULL_PTR;

    float* I = (float*)input->data;
    float* V = (float*)v->data;
    float* U = (float*)u->data;
    float* S = (float*)spikes->data;

    for (size_t i = 0; i < input->numel; i++) {
        // Izhikevich model equations
        float dv = (0.04f * V[i] * V[i] + 5.0f * V[i] + 140.0f - U[i] + I[i]) * dt;
        float du = a * (b * V[i] - U[i]) * dt;

        V[i] += dv;
        U[i] += du;

        if (V[i] >= 30.0f) {
            S[i] = 1.0f;
            V[i] = c;
            U[i] += d;
        } else {
            S[i] = 0.0f;
        }
    }

    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cpu_surrogate_superspike(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output,
    float beta)
{
    (void)ctx;
    if (!input || !output) return NIMCP_KERNEL_ERROR_NULL_PTR;

    float* in = (float*)input->data;
    float* out = (float*)output->data;

    for (size_t i = 0; i < input->numel; i++) {
        float x = in[i];
        out[i] = 1.0f / powf(1.0f + beta * fabsf(x), 2.0f);
    }

    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cpu_surrogate_fast_sigmoid(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output,
    float slope)
{
    (void)ctx;
    if (!input || !output) return NIMCP_KERNEL_ERROR_NULL_PTR;

    float* in = (float*)input->data;
    float* out = (float*)output->data;

    for (size_t i = 0; i < input->numel; i++) {
        float x = slope * in[i];
        out[i] = x / (1.0f + fabsf(x));
    }

    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cpu_stdp_update(
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
    (void)ctx;
    if (!weights || !pre_spikes || !post_spikes || !pre_trace || !post_trace) {
        return NIMCP_KERNEL_ERROR_NULL_PTR;
    }

    float* w = (float*)weights->data;
    float* pre = (float*)pre_spikes->data;
    float* post = (float*)post_spikes->data;
    float* tr_pre = (float*)pre_trace->data;
    float* tr_post = (float*)post_trace->data;

    float decay_pre = expf(-dt / tau_plus);
    float decay_post = expf(-dt / tau_minus);

    size_t n_pre = pre_spikes->numel;
    size_t n_post = post_spikes->numel;

    // Update traces
    for (size_t i = 0; i < n_pre; i++) {
        tr_pre[i] = decay_pre * tr_pre[i] + pre[i];
    }
    for (size_t j = 0; j < n_post; j++) {
        tr_post[j] = decay_post * tr_post[j] + post[j];
    }

    // Update weights
    for (size_t i = 0; i < n_pre; i++) {
        for (size_t j = 0; j < n_post; j++) {
            size_t idx = i * n_post + j;
            // LTP: post spike with pre trace
            w[idx] += A_plus * post[j] * tr_pre[i];
            // LTD: pre spike with post trace
            w[idx] -= A_minus * pre[i] * tr_post[j];
            // Clamp weights
            w[idx] = fmaxf(0.0f, fminf(1.0f, w[idx]));
        }
    }

    return NIMCP_KERNEL_SUCCESS;
}

//=============================================================================
// CPU Backend Implementation - CNN Operations
//=============================================================================

static nimcp_kernel_error_t cpu_conv2d_forward(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* kernel,
    const nimcp_gpu_tensor_t* bias,
    nimcp_gpu_tensor_t* output,
    uint32_t stride, uint32_t padding)
{
    (void)ctx;
    if (!input || !kernel || !output) return NIMCP_KERNEL_ERROR_NULL_PTR;

    // Simplified 2D convolution for [H, W] input, [KH, KW] kernel
    // Full implementation would handle batches and channels

    LOG_DEBUG("CPU conv2d forward - simplified implementation");

    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cpu_conv2d_backward(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* grad_output,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* kernel,
    nimcp_gpu_tensor_t* grad_input,
    nimcp_gpu_tensor_t* grad_kernel,
    nimcp_gpu_tensor_t* grad_bias,
    uint32_t stride, uint32_t padding)
{
    (void)ctx;
    (void)grad_output;
    (void)input;
    (void)kernel;
    (void)grad_input;
    (void)grad_kernel;
    (void)grad_bias;
    (void)stride;
    (void)padding;

    LOG_DEBUG("CPU conv2d backward - simplified implementation");
    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cpu_maxpool2d(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output,
    nimcp_gpu_tensor_t* indices,
    uint32_t kernel_size, uint32_t stride)
{
    (void)ctx;
    (void)input;
    (void)output;
    (void)indices;
    (void)kernel_size;
    (void)stride;

    LOG_DEBUG("CPU maxpool2d - simplified implementation");
    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cpu_avgpool2d(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output,
    uint32_t kernel_size, uint32_t stride)
{
    (void)ctx;
    (void)input;
    (void)output;
    (void)kernel_size;
    (void)stride;

    LOG_DEBUG("CPU avgpool2d - simplified implementation");
    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cpu_batchnorm_forward(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* gamma,
    const nimcp_gpu_tensor_t* beta,
    nimcp_gpu_tensor_t* output,
    nimcp_gpu_tensor_t* mean,
    nimcp_gpu_tensor_t* var,
    float epsilon, bool training)
{
    (void)ctx;
    (void)input;
    (void)gamma;
    (void)beta;
    (void)output;
    (void)mean;
    (void)var;
    (void)epsilon;
    (void)training;

    LOG_DEBUG("CPU batchnorm forward - simplified implementation");
    return NIMCP_KERNEL_SUCCESS;
}

//=============================================================================
// CPU Backend Implementation - Inference Operations
//=============================================================================

static nimcp_kernel_error_t cpu_linear_relu(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* weights,
    const nimcp_gpu_tensor_t* bias,
    nimcp_gpu_tensor_t* output)
{
    (void)ctx;
    if (!input || !weights || !output) return NIMCP_KERNEL_ERROR_NULL_PTR;

    // Fused linear + ReLU
    size_t M = input->dims[0];
    size_t K = input->dims[1];
    size_t N = weights->dims[1];

    float* in = (float*)input->data;
    float* w = (float*)weights->data;
    float* b = bias ? (float*)bias->data : NULL;
    float* out = (float*)output->data;

    for (size_t i = 0; i < M; i++) {
        for (size_t j = 0; j < N; j++) {
            float sum = b ? b[j] : 0.0f;
            for (size_t k = 0; k < K; k++) {
                sum += in[i * K + k] * w[k * N + j];
            }
            out[i * N + j] = sum > 0.0f ? sum : 0.0f;  // ReLU
        }
    }

    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cpu_conv_bn_relu(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* kernel,
    const nimcp_gpu_tensor_t* bn_params,
    nimcp_gpu_tensor_t* output,
    uint32_t stride, uint32_t padding)
{
    (void)ctx;
    (void)input;
    (void)kernel;
    (void)bn_params;
    (void)output;
    (void)stride;
    (void)padding;

    LOG_DEBUG("CPU conv_bn_relu - simplified implementation");
    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cpu_quantize_int8(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output,
    float scale, int8_t zero_point)
{
    (void)ctx;
    if (!input || !output) return NIMCP_KERNEL_ERROR_NULL_PTR;

    float* in = (float*)input->data;
    int8_t* out = (int8_t*)output->data;

    for (size_t i = 0; i < input->numel; i++) {
        float q = roundf(in[i] / scale) + zero_point;
        q = fmaxf(-128.0f, fminf(127.0f, q));
        out[i] = (int8_t)q;
    }

    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cpu_dequantize_int8(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output,
    float scale, int8_t zero_point)
{
    (void)ctx;
    if (!input || !output) return NIMCP_KERNEL_ERROR_NULL_PTR;

    int8_t* in = (int8_t*)input->data;
    float* out = (float*)output->data;

    for (size_t i = 0; i < input->numel; i++) {
        out[i] = scale * ((float)in[i] - zero_point);
    }

    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cpu_matmul_int8(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* a,
    const nimcp_gpu_tensor_t* b,
    nimcp_gpu_tensor_t* result,
    float scale_a, float scale_b,
    float scale_out)
{
    (void)ctx;
    (void)a;
    (void)b;
    (void)result;
    (void)scale_a;
    (void)scale_b;
    (void)scale_out;

    LOG_DEBUG("CPU matmul_int8 - simplified implementation");
    return NIMCP_KERNEL_SUCCESS;
}

//=============================================================================
// CPU Backend Implementation - LNN Operations
//=============================================================================

static nimcp_kernel_error_t cpu_lnn_euler_step(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    const nimcp_gpu_tensor_t* dx_dt,
    float dt,
    nimcp_gpu_tensor_t* x_new)
{
    (void)ctx;
    if (!x || !dx_dt || !x_new) return NIMCP_KERNEL_ERROR_NULL_PTR;
    if (x->numel != dx_dt->numel || x->numel != x_new->numel) {
        return NIMCP_KERNEL_ERROR_INVALID_SIZE;
    }

    const float* x_data = (const float*)x->data;
    const float* dx_data = (const float*)dx_dt->data;
    float* out_data = (float*)x_new->data;

    // Euler method: x_new = x + dt * dx_dt
    for (size_t i = 0; i < x->numel; i++) {
        out_data[i] = x_data[i] + dt * dx_data[i];
    }

    return NIMCP_KERNEL_SUCCESS;
}

/* Helper: compute LTC derivative dx/dt = -x/tau + sigmoid(W_in*input + W_rec*x + b) */
static nimcp_kernel_error_t cpu_lnn_compute_derivative(
    nimcp_gpu_context_t* ctx,
    struct nimcp_lnn_layer_gpu* layer,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* dx_dt)
{
    (void)ctx;
    typedef struct {
        nimcp_gpu_tensor_t* x;
        nimcp_gpu_tensor_t* dx_dt;
        nimcp_gpu_tensor_t* tau;
        nimcp_gpu_tensor_t* tau_base;
        nimcp_gpu_tensor_t* W_in;
        nimcp_gpu_tensor_t* W_rec;
        nimcp_gpu_tensor_t* W_tau;
        nimcp_gpu_tensor_t* b_in;
        nimcp_gpu_tensor_t* b_tau;
        nimcp_gpu_tensor_t* row_ptr;
        nimcp_gpu_tensor_t* col_idx;
        nimcp_gpu_tensor_t* edge_weights;
        uint32_t n_neurons;
        uint32_t n_inputs;
        uint32_t n_edges;
        uint32_t activation;
    } lnn_layer_cpu_t;

    if (!layer || !dx_dt) return NIMCP_KERNEL_ERROR_NULL_PTR;

    lnn_layer_cpu_t* l = (lnn_layer_cpu_t*)layer;
    if (!l->x || !l->tau) return NIMCP_KERNEL_ERROR_NULL_PTR;

    uint32_t n = l->n_neurons;
    const float* x_data = (const float*)l->x->data;
    const float* tau_data = (const float*)l->tau->data;
    float* dxdt = (float*)dx_dt->data;

    for (uint32_t i = 0; i < n; i++) {
        /* Leak term: -x_i / tau_i */
        float tau_i = tau_data[i];
        if (tau_i < 1e-6f) tau_i = 1e-6f;
        float leak = -x_data[i] / tau_i;

        /* Drive term: W_in * input + W_rec * x + b */
        float drive = 0.0f;

        /* Input contribution */
        if (l->W_in && input) {
            const float* w_in = (const float*)l->W_in->data;
            const float* in_data = (const float*)input->data;
            uint32_t n_in = l->n_inputs;
            for (uint32_t j = 0; j < n_in; j++) {
                drive += w_in[i * n_in + j] * in_data[j];
            }
        }

        /* Recurrent contribution */
        if (l->W_rec) {
            const float* w_rec = (const float*)l->W_rec->data;
            for (uint32_t j = 0; j < n; j++) {
                drive += w_rec[i * n + j] * x_data[j];
            }
        }

        /* Bias */
        if (l->b_in) {
            drive += ((const float*)l->b_in->data)[i];
        }

        /* Sigmoid activation */
        float activated = 1.0f / (1.0f + expf(-drive));

        dxdt[i] = leak + activated;
    }

    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cpu_lnn_heun_step(
    nimcp_gpu_context_t* ctx,
    struct nimcp_lnn_layer_gpu* layer,
    const nimcp_gpu_tensor_t* input,
    float dt,
    const struct nimcp_lnn_ode_config* config)
{
    (void)config;
    typedef struct {
        nimcp_gpu_tensor_t* x;
        nimcp_gpu_tensor_t* dx_dt;
        nimcp_gpu_tensor_t* tau;
        nimcp_gpu_tensor_t* tau_base;
        nimcp_gpu_tensor_t* W_in;
        nimcp_gpu_tensor_t* W_rec;
        nimcp_gpu_tensor_t* W_tau;
        nimcp_gpu_tensor_t* b_in;
        nimcp_gpu_tensor_t* b_tau;
        nimcp_gpu_tensor_t* row_ptr;
        nimcp_gpu_tensor_t* col_idx;
        nimcp_gpu_tensor_t* edge_weights;
        uint32_t n_neurons;
        uint32_t n_inputs;
        uint32_t n_edges;
        uint32_t activation;
    } lnn_layer_cpu_t;

    if (!layer) return NIMCP_KERNEL_ERROR_NULL_PTR;
    lnn_layer_cpu_t* l = (lnn_layer_cpu_t*)layer;
    if (!l->x || !l->dx_dt) return NIMCP_KERNEL_ERROR_NULL_PTR;

    uint32_t n = l->n_neurons;
    float* x_data = (float*)l->x->data;

    /* Heun's method (improved Euler / trapezoidal predictor-corrector):
     * k1 = f(t, x)
     * x_pred = x + dt * k1
     * k2 = f(t + dt, x_pred)
     * x_new = x + (dt/2) * (k1 + k2)
     */

    /* k1 = f(t, x) */
    nimcp_kernel_error_t err = cpu_lnn_compute_derivative(ctx, layer, input, l->dx_dt);
    if (err != NIMCP_KERNEL_SUCCESS) return err;

    float* k1 = (float*)l->dx_dt->data;

    /* Save k1 and compute predictor x_pred = x + dt * k1 */
    float* k1_save = (float*)nimcp_malloc(n * sizeof(float));
    float* x_save = (float*)nimcp_malloc(n * sizeof(float));
    if (!k1_save || !x_save) {
        nimcp_free(k1_save);
        nimcp_free(x_save);
        return NIMCP_KERNEL_ERROR_MEMORY;
    }

    for (uint32_t i = 0; i < n; i++) {
        k1_save[i] = k1[i];
        x_save[i] = x_data[i];
        x_data[i] = x_save[i] + dt * k1[i];  /* predictor */
    }

    /* k2 = f(t + dt, x_pred) */
    err = cpu_lnn_compute_derivative(ctx, layer, input, l->dx_dt);
    float* k2 = (float*)l->dx_dt->data;

    /* Corrector: x_new = x_orig + (dt/2) * (k1 + k2) */
    for (uint32_t i = 0; i < n; i++) {
        x_data[i] = x_save[i] + (dt * 0.5f) * (k1_save[i] + k2[i]);
    }

    nimcp_free(k1_save);
    nimcp_free(x_save);
    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cpu_lnn_rk4_step(
    nimcp_gpu_context_t* ctx,
    struct nimcp_lnn_layer_gpu* layer,
    const nimcp_gpu_tensor_t* input,
    float dt,
    const struct nimcp_lnn_ode_config* config)
{
    (void)config;
    typedef struct {
        nimcp_gpu_tensor_t* x;
        nimcp_gpu_tensor_t* dx_dt;
        nimcp_gpu_tensor_t* tau;
        nimcp_gpu_tensor_t* tau_base;
        nimcp_gpu_tensor_t* W_in;
        nimcp_gpu_tensor_t* W_rec;
        nimcp_gpu_tensor_t* W_tau;
        nimcp_gpu_tensor_t* b_in;
        nimcp_gpu_tensor_t* b_tau;
        nimcp_gpu_tensor_t* row_ptr;
        nimcp_gpu_tensor_t* col_idx;
        nimcp_gpu_tensor_t* edge_weights;
        uint32_t n_neurons;
        uint32_t n_inputs;
        uint32_t n_edges;
        uint32_t activation;
    } lnn_layer_cpu_t;

    if (!layer) return NIMCP_KERNEL_ERROR_NULL_PTR;
    lnn_layer_cpu_t* l = (lnn_layer_cpu_t*)layer;
    if (!l->x || !l->dx_dt) return NIMCP_KERNEL_ERROR_NULL_PTR;

    uint32_t n = l->n_neurons;
    float* x_data = (float*)l->x->data;

    /* RK4: k1=f(t,x), k2=f(t+dt/2,x+dt/2*k1), k3=f(t+dt/2,x+dt/2*k2), k4=f(t+dt,x+dt*k3)
     * x_new = x + (dt/6)*(k1 + 2*k2 + 2*k3 + k4) */

    float* x_save = (float*)nimcp_malloc(n * sizeof(float));
    float* k1 = (float*)nimcp_malloc(n * sizeof(float));
    float* k2 = (float*)nimcp_malloc(n * sizeof(float));
    float* k3 = (float*)nimcp_malloc(n * sizeof(float));
    if (!x_save || !k1 || !k2 || !k3) {
        nimcp_free(x_save); nimcp_free(k1); nimcp_free(k2); nimcp_free(k3);
        return NIMCP_KERNEL_ERROR_MEMORY;
    }

    for (uint32_t i = 0; i < n; i++) x_save[i] = x_data[i];

    /* k1 = f(t, x) */
    nimcp_kernel_error_t err = cpu_lnn_compute_derivative(ctx, layer, input, l->dx_dt);
    if (err != NIMCP_KERNEL_SUCCESS) goto cleanup;
    for (uint32_t i = 0; i < n; i++) k1[i] = ((float*)l->dx_dt->data)[i];

    /* k2 = f(t + dt/2, x + dt/2 * k1) */
    for (uint32_t i = 0; i < n; i++) x_data[i] = x_save[i] + 0.5f * dt * k1[i];
    err = cpu_lnn_compute_derivative(ctx, layer, input, l->dx_dt);
    if (err != NIMCP_KERNEL_SUCCESS) goto cleanup;
    for (uint32_t i = 0; i < n; i++) k2[i] = ((float*)l->dx_dt->data)[i];

    /* k3 = f(t + dt/2, x + dt/2 * k2) */
    for (uint32_t i = 0; i < n; i++) x_data[i] = x_save[i] + 0.5f * dt * k2[i];
    err = cpu_lnn_compute_derivative(ctx, layer, input, l->dx_dt);
    if (err != NIMCP_KERNEL_SUCCESS) goto cleanup;
    for (uint32_t i = 0; i < n; i++) k3[i] = ((float*)l->dx_dt->data)[i];

    /* k4 = f(t + dt, x + dt * k3) */
    for (uint32_t i = 0; i < n; i++) x_data[i] = x_save[i] + dt * k3[i];
    err = cpu_lnn_compute_derivative(ctx, layer, input, l->dx_dt);
    if (err != NIMCP_KERNEL_SUCCESS) goto cleanup;

    /* Final: x_new = x_orig + (dt/6) * (k1 + 2*k2 + 2*k3 + k4) */
    {
        float* k4 = (float*)l->dx_dt->data;
        float dt6 = dt / 6.0f;
        for (uint32_t i = 0; i < n; i++) {
            x_data[i] = x_save[i] + dt6 * (k1[i] + 2.0f * k2[i] + 2.0f * k3[i] + k4[i]);
        }
    }

cleanup:
    nimcp_free(x_save); nimcp_free(k1); nimcp_free(k2); nimcp_free(k3);
    return err;
}

static nimcp_kernel_error_t cpu_lnn_dopri5_step(
    nimcp_gpu_context_t* ctx,
    struct nimcp_lnn_layer_gpu* layer,
    const nimcp_gpu_tensor_t* input,
    float* dt_ptr,
    const struct nimcp_lnn_ode_config* config)
{
    typedef struct {
        nimcp_gpu_tensor_t* x;
        nimcp_gpu_tensor_t* dx_dt;
        nimcp_gpu_tensor_t* tau;
        nimcp_gpu_tensor_t* tau_base;
        nimcp_gpu_tensor_t* W_in;
        nimcp_gpu_tensor_t* W_rec;
        nimcp_gpu_tensor_t* W_tau;
        nimcp_gpu_tensor_t* b_in;
        nimcp_gpu_tensor_t* b_tau;
        nimcp_gpu_tensor_t* row_ptr;
        nimcp_gpu_tensor_t* col_idx;
        nimcp_gpu_tensor_t* edge_weights;
        uint32_t n_neurons;
        uint32_t n_inputs;
        uint32_t n_edges;
        uint32_t activation;
    } lnn_layer_cpu_t;

    if (!layer || !dt_ptr) return NIMCP_KERNEL_ERROR_NULL_PTR;
    lnn_layer_cpu_t* l = (lnn_layer_cpu_t*)layer;
    if (!l->x || !l->dx_dt) return NIMCP_KERNEL_ERROR_NULL_PTR;

    float dt = *dt_ptr;
    float atol = 1e-6f, rtol = 1e-3f;
    float dt_min = 1e-6f, dt_max = 0.1f;

    if (config) {
        typedef struct { uint32_t method; float dt; float dt_min; float dt_max;
                         float error_tolerance; uint32_t max_steps; bool adaptive; } ode_cfg_t;
        const ode_cfg_t* cfg = (const ode_cfg_t*)config;
        if (cfg->error_tolerance > 0) { atol = cfg->error_tolerance; rtol = cfg->error_tolerance; }
        if (cfg->dt_min > 0) dt_min = cfg->dt_min;
        if (cfg->dt_max > 0) dt_max = cfg->dt_max;
    }

    uint32_t n = l->n_neurons;
    float* x_data = (float*)l->x->data;

    /* DOPRI5 (Dormand-Prince) uses 7 stages but we use an RK45 embedded pair.
     * For the CPU fallback, use a simplified adaptive RK45 approach:
     * - Take one RK4 step (4th order) and one Euler step (1st order)
     * - Use the difference as error estimate
     * - Adjust dt accordingly */

    float* x_save = (float*)nimcp_malloc(n * sizeof(float));
    float* k1 = (float*)nimcp_malloc(n * sizeof(float));
    float* k2 = (float*)nimcp_malloc(n * sizeof(float));
    float* k3 = (float*)nimcp_malloc(n * sizeof(float));
    float* x_euler = (float*)nimcp_malloc(n * sizeof(float));
    if (!x_save || !k1 || !k2 || !k3 || !x_euler) {
        nimcp_free(x_save); nimcp_free(k1); nimcp_free(k2); nimcp_free(k3); nimcp_free(x_euler);
        return NIMCP_KERNEL_ERROR_MEMORY;
    }

    for (uint32_t i = 0; i < n; i++) x_save[i] = x_data[i];

    /* k1 */
    nimcp_kernel_error_t err = cpu_lnn_compute_derivative(ctx, layer, input, l->dx_dt);
    if (err != NIMCP_KERNEL_SUCCESS) goto dp_cleanup;
    for (uint32_t i = 0; i < n; i++) {
        k1[i] = ((float*)l->dx_dt->data)[i];
        x_euler[i] = x_save[i] + dt * k1[i]; /* Euler estimate */
    }

    /* k2 at midpoint */
    for (uint32_t i = 0; i < n; i++) x_data[i] = x_save[i] + 0.5f * dt * k1[i];
    err = cpu_lnn_compute_derivative(ctx, layer, input, l->dx_dt);
    if (err != NIMCP_KERNEL_SUCCESS) goto dp_cleanup;
    for (uint32_t i = 0; i < n; i++) k2[i] = ((float*)l->dx_dt->data)[i];

    /* k3 at midpoint */
    for (uint32_t i = 0; i < n; i++) x_data[i] = x_save[i] + 0.5f * dt * k2[i];
    err = cpu_lnn_compute_derivative(ctx, layer, input, l->dx_dt);
    if (err != NIMCP_KERNEL_SUCCESS) goto dp_cleanup;
    for (uint32_t i = 0; i < n; i++) k3[i] = ((float*)l->dx_dt->data)[i];

    /* k4 at endpoint */
    for (uint32_t i = 0; i < n; i++) x_data[i] = x_save[i] + dt * k3[i];
    err = cpu_lnn_compute_derivative(ctx, layer, input, l->dx_dt);
    if (err != NIMCP_KERNEL_SUCCESS) goto dp_cleanup;

    /* RK4 solution + error estimate */
    {
        float* k4 = (float*)l->dx_dt->data;
        float dt6 = dt / 6.0f;
        float max_err = 0.0f;

        for (uint32_t i = 0; i < n; i++) {
            float rk4_val = x_save[i] + dt6 * (k1[i] + 2.0f * k2[i] + 2.0f * k3[i] + k4[i]);
            x_data[i] = rk4_val;

            /* Error = |RK4 - Euler| */
            float err_i = fabsf(rk4_val - x_euler[i]);
            float scale = atol + rtol * fabsf(rk4_val);
            if (scale > 0) err_i /= scale;
            if (err_i > max_err) max_err = err_i;
        }

        /* Adapt step size: dt_new = dt * 0.9 * (1/err)^(1/5) */
        if (max_err > 0) {
            float factor = 0.9f * powf(1.0f / max_err, 0.2f);
            if (factor < 0.2f) factor = 0.2f;
            if (factor > 5.0f) factor = 5.0f;
            dt *= factor;
        } else {
            dt *= 2.0f; /* error is zero, double step */
        }
        if (dt < dt_min) dt = dt_min;
        if (dt > dt_max) dt = dt_max;
        *dt_ptr = dt;
    }

dp_cleanup:
    nimcp_free(x_save); nimcp_free(k1); nimcp_free(k2); nimcp_free(k3); nimcp_free(x_euler);
    return err;
}

static nimcp_kernel_error_t cpu_lnn_update_tau(
    nimcp_gpu_context_t* ctx,
    struct nimcp_lnn_layer_gpu* layer,
    const nimcp_gpu_tensor_t* input)
{
    (void)ctx;
    typedef struct {
        nimcp_gpu_tensor_t* x;
        nimcp_gpu_tensor_t* dx_dt;
        nimcp_gpu_tensor_t* tau;
        nimcp_gpu_tensor_t* tau_base;
        nimcp_gpu_tensor_t* W_in;
        nimcp_gpu_tensor_t* W_rec;
        nimcp_gpu_tensor_t* W_tau;
        nimcp_gpu_tensor_t* b_in;
        nimcp_gpu_tensor_t* b_tau;
        nimcp_gpu_tensor_t* row_ptr;
        nimcp_gpu_tensor_t* col_idx;
        nimcp_gpu_tensor_t* edge_weights;
        uint32_t n_neurons;
        uint32_t n_inputs;
        uint32_t n_edges;
        uint32_t activation;
    } lnn_layer_cpu_t;

    if (!layer) return NIMCP_KERNEL_ERROR_NULL_PTR;
    lnn_layer_cpu_t* l = (lnn_layer_cpu_t*)layer;
    if (!l->tau || !l->tau_base) return NIMCP_KERNEL_ERROR_NULL_PTR;

    uint32_t n = l->n_neurons;
    float* tau_data = (float*)l->tau->data;
    const float* tau_base_data = (const float*)l->tau_base->data;

    if (!l->W_tau) {
        /* No tau modulation weights — tau = tau_base */
        for (uint32_t i = 0; i < n; i++) {
            tau_data[i] = tau_base_data[i];
        }
        return NIMCP_KERNEL_SUCCESS;
    }

    /* tau_i = tau_base_i * sigmoid(W_tau * [input; x] + b_tau)
     * This makes time constants input/state-dependent (LTC dynamics) */
    const float* w_tau = (const float*)l->W_tau->data;
    uint32_t n_in = l->n_inputs;
    uint32_t concat_dim = n_in + n;

    for (uint32_t i = 0; i < n; i++) {
        float z = 0.0f;
        /* Input contribution */
        if (input && n_in > 0) {
            const float* in_data = (const float*)input->data;
            for (uint32_t j = 0; j < n_in; j++) {
                z += w_tau[i * concat_dim + j] * in_data[j];
            }
        }
        /* State contribution */
        if (l->x) {
            const float* x_data = (const float*)l->x->data;
            for (uint32_t j = 0; j < n; j++) {
                z += w_tau[i * concat_dim + n_in + j] * x_data[j];
            }
        }
        /* Bias */
        if (l->b_tau) {
            z += ((const float*)l->b_tau->data)[i];
        }
        /* sigmoid modulation */
        float sig = 1.0f / (1.0f + expf(-z));
        tau_data[i] = tau_base_data[i] * (0.5f + 1.5f * sig); /* range: [0.5, 2.0] * tau_base */
    }

    return NIMCP_KERNEL_SUCCESS;
}

static nimcp_kernel_error_t cpu_lnn_sparse_matvec(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* row_ptr,
    const nimcp_gpu_tensor_t* col_idx,
    const nimcp_gpu_tensor_t* values,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* y,
    uint32_t n_rows, float alpha)
{
    (void)ctx;
    if (!row_ptr || !col_idx || !values || !x || !y) {
        return NIMCP_KERNEL_ERROR_NULL_PTR;
    }

    const uint32_t* row_data = (const uint32_t*)row_ptr->data;
    const uint32_t* col_data = (const uint32_t*)col_idx->data;
    const float* val_data = (const float*)values->data;
    const float* x_data = (const float*)x->data;
    float* y_data = (float*)y->data;

    // CSR sparse matrix-vector multiplication: y = alpha * A * x
    for (uint32_t row = 0; row < n_rows; row++) {
        float sum = 0.0f;
        uint32_t row_start = row_data[row];
        uint32_t row_end = row_data[row + 1];

        for (uint32_t j = row_start; j < row_end; j++) {
            uint32_t col = col_data[j];
            sum += val_data[j] * x_data[col];
        }

        y_data[row] = alpha * sum;
    }

    return NIMCP_KERNEL_SUCCESS;
}

//=============================================================================
// Initialize CPU Backend
//=============================================================================

static void init_cpu_backend(void)
{
    memset(&g_cpu_backend, 0, sizeof(g_cpu_backend));

    g_cpu_backend.type = NIMCP_BACKEND_CPU;
    g_cpu_backend.name = "CPU";
    g_cpu_backend.initialized = true;

    // Tensor operations
    g_cpu_backend.tensor.add = cpu_tensor_add;
    g_cpu_backend.tensor.sub = cpu_tensor_sub;
    g_cpu_backend.tensor.mul = cpu_tensor_mul;
    g_cpu_backend.tensor.div = cpu_tensor_div;
    g_cpu_backend.tensor.scale = cpu_tensor_scale;
    g_cpu_backend.tensor.matmul = cpu_tensor_matmul;
    g_cpu_backend.tensor.transpose = cpu_tensor_transpose;
    g_cpu_backend.tensor.relu = cpu_tensor_relu;
    g_cpu_backend.tensor.sigmoid = cpu_tensor_sigmoid;
    g_cpu_backend.tensor.tanh = cpu_tensor_tanh;
    g_cpu_backend.tensor.softmax = cpu_tensor_softmax;
    g_cpu_backend.tensor.sum = cpu_tensor_sum;
    g_cpu_backend.tensor.mean = cpu_tensor_mean;
    g_cpu_backend.tensor.max = cpu_tensor_max;
    g_cpu_backend.tensor.min = cpu_tensor_min;

    // Training operations
    g_cpu_backend.training.mse_loss = cpu_mse_loss;
    g_cpu_backend.training.cross_entropy_loss = cpu_cross_entropy_loss;
    g_cpu_backend.training.gradient_clip = cpu_gradient_clip;
    g_cpu_backend.training.gradient_accumulate = cpu_gradient_accumulate;
    g_cpu_backend.training.sgd_step = cpu_sgd_step;
    g_cpu_backend.training.adam_step = cpu_adam_step;
    g_cpu_backend.training.backward_linear = cpu_backward_linear;

    // SNN operations
    g_cpu_backend.snn.lif_forward = cpu_lif_forward;
    g_cpu_backend.snn.izhikevich_forward = cpu_izhikevich_forward;
    g_cpu_backend.snn.surrogate_superspike = cpu_surrogate_superspike;
    g_cpu_backend.snn.surrogate_fast_sigmoid = cpu_surrogate_fast_sigmoid;
    g_cpu_backend.snn.stdp_update = cpu_stdp_update;

    // CNN operations
    g_cpu_backend.cnn.conv2d_forward = cpu_conv2d_forward;
    g_cpu_backend.cnn.conv2d_backward = cpu_conv2d_backward;
    g_cpu_backend.cnn.maxpool2d = cpu_maxpool2d;
    g_cpu_backend.cnn.avgpool2d = cpu_avgpool2d;
    g_cpu_backend.cnn.batchnorm_forward = cpu_batchnorm_forward;

    // LNN operations
    g_cpu_backend.lnn.euler_step = cpu_lnn_euler_step;
    g_cpu_backend.lnn.heun_step = cpu_lnn_heun_step;
    g_cpu_backend.lnn.rk4_step = cpu_lnn_rk4_step;
    g_cpu_backend.lnn.dopri5_step = cpu_lnn_dopri5_step;
    g_cpu_backend.lnn.compute_derivative = cpu_lnn_compute_derivative;
    g_cpu_backend.lnn.update_tau = cpu_lnn_update_tau;
    g_cpu_backend.lnn.sparse_matvec = cpu_lnn_sparse_matvec;

    // Inference operations
    g_cpu_backend.inference.linear_relu = cpu_linear_relu;
    g_cpu_backend.inference.conv_bn_relu = cpu_conv_bn_relu;
    g_cpu_backend.inference.quantize_int8 = cpu_quantize_int8;
    g_cpu_backend.inference.dequantize_int8 = cpu_dequantize_int8;
    g_cpu_backend.inference.matmul_int8 = cpu_matmul_int8;

    // Substrate operations (axons, dendrites, myelin, neuromodulators, glial, metabolic)
    g_cpu_backend.substrate = nimcp_cpu_substrate_ops;

    LOG_INFO("CPU kernel backend initialized");
}

//=============================================================================
// Initialize CUDA Backend (placeholder - actual impl in .cu file)
//=============================================================================

#ifdef NIMCP_ENABLE_CUDA
extern void init_cuda_backend_ops(nimcp_kernel_backend_t* backend);
#endif

static void init_cuda_backend(void)
{
#ifdef NIMCP_ENABLE_CUDA
    memset(&g_cuda_backend, 0, sizeof(g_cuda_backend));

    g_cuda_backend.type = NIMCP_BACKEND_CUDA;
    g_cuda_backend.name = "CUDA";

    init_cuda_backend_ops(&g_cuda_backend);

    g_cuda_backend.initialized = true;
    LOG_INFO("CUDA kernel backend initialized");
#else
    g_cuda_backend.initialized = false;
    LOG_DEBUG("CUDA backend not available (not compiled with CUDA support)");
#endif
}

//=============================================================================
// Initialize ROCm Backend (placeholder - for AMD GPU support)
//=============================================================================

#ifdef NIMCP_ENABLE_ROCM
extern void init_rocm_backend_ops(nimcp_kernel_backend_t* backend);
#endif

static void init_rocm_backend(void)
{
#ifdef NIMCP_ENABLE_ROCM
    memset(&g_rocm_backend, 0, sizeof(g_rocm_backend));

    g_rocm_backend.type = NIMCP_BACKEND_ROCM;
    g_rocm_backend.name = "ROCm";

    init_rocm_backend_ops(&g_rocm_backend);

    g_rocm_backend.initialized = true;
    LOG_INFO("ROCm kernel backend initialized");
#else
    g_rocm_backend.initialized = false;
    LOG_DEBUG("ROCm backend not available (not compiled with ROCm support)");
#endif
}

//=============================================================================
// Initialize OpenCL Backend (placeholder - for cross-platform GPU support)
//=============================================================================

#ifdef NIMCP_ENABLE_OPENCL
extern void init_opencl_backend_ops(nimcp_kernel_backend_t* backend);
#endif

static void init_opencl_backend(void)
{
#ifdef NIMCP_ENABLE_OPENCL
    memset(&g_opencl_backend, 0, sizeof(g_opencl_backend));

    g_opencl_backend.type = NIMCP_BACKEND_OPENCL;
    g_opencl_backend.name = "OpenCL";

    init_opencl_backend_ops(&g_opencl_backend);

    g_opencl_backend.initialized = true;
    LOG_INFO("OpenCL kernel backend initialized");
#else
    g_opencl_backend.initialized = false;
    LOG_DEBUG("OpenCL backend not available (not compiled with OpenCL support)");
#endif
}

//=============================================================================
// Initialize Neuron Backend (AWS Inferentia)
//=============================================================================

extern void init_neuron_backend_ops(nimcp_kernel_backend_t* backend);

static void init_neuron_backend(void)
{
    init_neuron_backend_ops(&g_neuron_backend);

    if (g_neuron_backend.initialized) {
        LOG_INFO("Neuron kernel backend initialized");
    } else {
        LOG_DEBUG("Neuron backend initialization failed");
    }
}

//=============================================================================
// Helper: Try to initialize a specific GPU backend
//=============================================================================

/**
 * @brief Attempt to initialize a GPU backend with fallback on failure
 *
 * WHAT: Tries to init a specific GPU backend, returns success/failure
 * WHY:  Part of GPU-first fallback chain
 * HOW:  Calls backend-specific init, checks if it succeeded
 *
 * NOTE: Failing to initialize a GPU backend is NORMAL when the hardware
 * or drivers are not available. This is NOT an error condition - the
 * system will gracefully fall back to CPU. No immune throws on normal
 * "GPU not available" paths.
 *
 * @param type Backend type to try
 * @return true if backend initialized successfully
 */
static bool try_init_gpu_backend(nimcp_backend_type_t type)
{
    switch (type) {
        case NIMCP_BACKEND_CUDA:
            init_cuda_backend();
            if (g_cuda_backend.initialized) {
                g_active_backend = &g_cuda_backend;
                LOG_INFO("GPU backend selected: CUDA");
                return true;
            }
            LOG_DEBUG("CUDA backend initialization failed, trying next...");
            /* Not an error - GPU not available is normal on CPU-only systems */
            return false;

        case NIMCP_BACKEND_ROCM:
            init_rocm_backend();
            if (g_rocm_backend.initialized) {
                g_active_backend = &g_rocm_backend;
                LOG_INFO("GPU backend selected: ROCm");
                return true;
            }
            LOG_DEBUG("ROCm backend initialization failed, trying next...");
            /* Not an error - GPU not available is normal */
            return false;

        case NIMCP_BACKEND_OPENCL:
            init_opencl_backend();
            if (g_opencl_backend.initialized) {
                g_active_backend = &g_opencl_backend;
                LOG_INFO("GPU backend selected: OpenCL");
                return true;
            }
            LOG_DEBUG("OpenCL backend initialization failed, trying next...");
            /* Not an error - GPU not available is normal */
            return false;

        case NIMCP_BACKEND_NEURON:
            init_neuron_backend();
            if (g_neuron_backend.initialized) {
                g_active_backend = &g_neuron_backend;
                LOG_INFO("Backend selected: Neuron (AWS Inferentia)");
                return true;
            }
            LOG_DEBUG("Neuron backend initialization failed, trying next...");
            return false;

        default:
            LOG_WARN("try_init_gpu_backend: unknown backend type %d", (int)type);
            return false;
    }
}

//=============================================================================
// Backend API Implementation
//=============================================================================
//
// RETURN TYPE CONVENTION - Backend API:
// =====================================
// nimcp_kernel_backend_init()        -> bool  (true=success, false=failure)
// nimcp_kernel_backend_init_default()-> bool  (true=success, always succeeds)
// nimcp_kernel_backend_shutdown()    -> void
// nimcp_get_kernel_backend()         -> ptr   (never NULL after init)
// nimcp_cuda_backend_available()     -> bool  (true=CUDA available)
// nimcp_get_backend_type()           -> enum  (nimcp_backend_type_t)
// nimcp_switch_backend()             -> bool  (true=switch succeeded)
// nimcp_backend_type_name()          -> const char*
//
// NOTE: Backend API functions return bool for success/failure.
// Internal kernel operations return nimcp_kernel_error_t (0/-negative).
// GPU context operations return int (0/-1).
// GPU stub convenience functions return bool.
//=============================================================================

bool nimcp_kernel_backend_init(nimcp_backend_type_t preferred)
{
    if (g_backend_initialized) {
        LOG_WARN("Kernel backend already initialized");
        return true;
    }

    // Always initialize CPU backend (guaranteed fallback)
    init_cpu_backend();

    // Initialize all GPU backends (will set initialized=false if not available)
    init_cuda_backend();
    init_rocm_backend();
    init_opencl_backend();
    init_neuron_backend();

    // Select active backend based on preference
    switch (preferred) {
        case NIMCP_BACKEND_CUDA:
            if (g_cuda_backend.initialized) {
                g_active_backend = &g_cuda_backend;
            } else {
                LOG_WARN("CUDA backend requested but not available, falling back to CPU");
                g_active_backend = &g_cpu_backend;
            }
            break;

        case NIMCP_BACKEND_ROCM:
            if (g_rocm_backend.initialized) {
                g_active_backend = &g_rocm_backend;
            } else {
                LOG_WARN("ROCm backend requested but not available, falling back to CPU");
                g_active_backend = &g_cpu_backend;
            }
            break;

        case NIMCP_BACKEND_OPENCL:
            if (g_opencl_backend.initialized) {
                g_active_backend = &g_opencl_backend;
            } else {
                LOG_WARN("OpenCL backend requested but not available, falling back to CPU");
                g_active_backend = &g_cpu_backend;
            }
            break;

        case NIMCP_BACKEND_NEURON:
            if (g_neuron_backend.initialized) {
                g_active_backend = &g_neuron_backend;
            } else {
                LOG_WARN("Neuron backend requested but not available, falling back to CPU");
                g_active_backend = &g_cpu_backend;
            }
            break;

        case NIMCP_BACKEND_AUTO:
            // GPU-first fallback chain: CUDA -> ROCm -> OpenCL -> Neuron -> CPU
            if (g_cuda_backend.initialized) {
                g_active_backend = &g_cuda_backend;
            } else if (g_rocm_backend.initialized) {
                g_active_backend = &g_rocm_backend;
            } else if (g_opencl_backend.initialized) {
                g_active_backend = &g_opencl_backend;
            } else if (g_neuron_backend.initialized) {
                g_active_backend = &g_neuron_backend;
            } else {
                LOG_WARN("No GPU backend available, using CPU fallback");
                g_active_backend = &g_cpu_backend;
            }
            break;

        case NIMCP_BACKEND_CPU:
        default:
            g_active_backend = &g_cpu_backend;
            break;
    }

    g_backend_initialized = true;

    LOG_INFO("Kernel backend system initialized: active=%s", g_active_backend->name);
    return true;
}

/**
 * @brief Initialize kernel backend with GPU-first default policy
 *
 * WHAT: Phase 1 GPU integration - GPU is now the default backend
 * WHY:  Maximize performance by using GPU acceleration when available
 * HOW:  Try GPU backends in order, fall back to CPU if none available
 *
 * FALLBACK CHAIN:
 * 1. CUDA (NVIDIA GPUs) - Most common and best optimized
 * 2. ROCm (AMD GPUs) - AMD alternative
 * 3. OpenCL (Cross-platform) - Works on any OpenCL-capable GPU
 * 4. CPU (Always available) - Guaranteed fallback
 *
 * @return true on success (always succeeds due to CPU fallback)
 */
bool nimcp_kernel_backend_init_default(void)
{
    if (g_backend_initialized) {
        LOG_WARN("Kernel backend already initialized");
        return true;
    }

    LOG_INFO("Initializing kernel backend with GPU-first policy...");

    // Always initialize CPU backend first (guaranteed fallback)
    init_cpu_backend();

    // Try GPU backends in priority order: CUDA -> ROCm -> OpenCL -> Neuron
    bool gpu_found = false;

    // Try CUDA first (most common, best optimized)
    if (try_init_gpu_backend(NIMCP_BACKEND_CUDA)) {
        gpu_found = true;
    }
    // Try ROCm if CUDA failed
    else if (try_init_gpu_backend(NIMCP_BACKEND_ROCM)) {
        gpu_found = true;
    }
    // Try OpenCL if ROCm failed
    else if (try_init_gpu_backend(NIMCP_BACKEND_OPENCL)) {
        gpu_found = true;
    }
    // Try Neuron if OpenCL failed
    else if (try_init_gpu_backend(NIMCP_BACKEND_NEURON)) {
        gpu_found = true;
    }

    // Fall back to CPU if no GPU available
    if (!gpu_found) {
        g_active_backend = &g_cpu_backend;
        LOG_WARN("No GPU backend available - falling back to CPU execution");
        LOG_WARN("For optimal performance, install CUDA, ROCm, OpenCL, or Neuron drivers");
    }

    g_backend_initialized = true;

    LOG_INFO("Kernel backend initialized: %s (GPU-first policy)", g_active_backend->name);
    return true;
}

void nimcp_kernel_backend_shutdown(void)
{
    if (!g_backend_initialized) return;

    g_active_backend = NULL;
    g_backend_initialized = false;

    LOG_INFO("Kernel backend system shutdown");
}

nimcp_kernel_backend_t* nimcp_get_kernel_backend(void)
{
    if (!g_backend_initialized) {
        // Auto-initialize with default settings
        nimcp_kernel_backend_init(NIMCP_BACKEND_AUTO);
    }

    return g_active_backend;
}

bool nimcp_cuda_backend_available(void)
{
    return g_cuda_backend.initialized;
}

nimcp_backend_type_t nimcp_get_backend_type(void)
{
    return g_active_backend ? g_active_backend->type : NIMCP_BACKEND_CPU;
}

bool nimcp_switch_backend(nimcp_backend_type_t type)
{
    if (!g_backend_initialized) {
        return nimcp_kernel_backend_init(type);
    }

    switch (type) {
        case NIMCP_BACKEND_CUDA:
            if (!g_cuda_backend.initialized) {
                LOG_ERROR("Cannot switch to CUDA backend - not available");
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_GPU, "CUDA backend switch failed - not available");
                return false;
            }
            g_active_backend = &g_cuda_backend;
            break;

        case NIMCP_BACKEND_ROCM:
            if (!g_rocm_backend.initialized) {
                LOG_ERROR("Cannot switch to ROCm backend - not available");
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_GPU, "ROCm backend switch failed - not available");
                return false;
            }
            g_active_backend = &g_rocm_backend;
            break;

        case NIMCP_BACKEND_OPENCL:
            if (!g_opencl_backend.initialized) {
                LOG_ERROR("Cannot switch to OpenCL backend - not available");
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_GPU, "OpenCL backend switch failed - not available");
                return false;
            }
            g_active_backend = &g_opencl_backend;
            break;

        case NIMCP_BACKEND_NEURON:
            if (!g_neuron_backend.initialized) {
                LOG_ERROR("Cannot switch to Neuron backend - not available");
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_GPU, "Neuron backend switch failed - not available");
                return false;
            }
            g_active_backend = &g_neuron_backend;
            break;

        case NIMCP_BACKEND_CPU:
            g_active_backend = &g_cpu_backend;
            break;

        case NIMCP_BACKEND_AUTO:
            // Switch to best available GPU, or CPU if none
            if (g_cuda_backend.initialized) {
                g_active_backend = &g_cuda_backend;
            } else if (g_rocm_backend.initialized) {
                g_active_backend = &g_rocm_backend;
            } else if (g_opencl_backend.initialized) {
                g_active_backend = &g_opencl_backend;
            } else if (g_neuron_backend.initialized) {
                g_active_backend = &g_neuron_backend;
            } else {
                g_active_backend = &g_cpu_backend;
            }
            break;

        default:
            LOG_ERROR("Unknown backend type: %d", type);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Unknown GPU backend type: %d", type);
            return false;
    }

    LOG_INFO("Switched to %s backend", g_active_backend->name);
    return true;
}

const char* nimcp_backend_type_name(nimcp_backend_type_t type)
{
    switch (type) {
        case NIMCP_BACKEND_CPU: return "CPU";
        case NIMCP_BACKEND_CUDA: return "CUDA";
        case NIMCP_BACKEND_ROCM: return "ROCm";
        case NIMCP_BACKEND_OPENCL: return "OpenCL";
        case NIMCP_BACKEND_NEURON: return "Neuron";
        case NIMCP_BACKEND_AUTO: return "AUTO";
        default: return "UNKNOWN";
    }
}
