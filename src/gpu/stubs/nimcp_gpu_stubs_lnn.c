/**
 * @file nimcp_gpu_stubs_lnn.c
 * @brief CPU fallback stubs for LNN GPU operations
 *
 * WHAT: Provides stub implementations for LNN GPU functions when CUDA unavailable
 * WHY:  Allows code to compile and link without CUDA toolkit
 * HOW:  Returns error codes or NULL, logs warnings on first call
 *
 * These stubs are linked when CUDAToolkit_FOUND is false. They allow the
 * library to build and run with CPU-only execution. LNN operations will
 * fall back to CPU implementations automatically.
 */

#include "gpu/lnn/nimcp_lnn_gpu.h"
#include "lnn/nimcp_lnn.h"
#include "utils/logging/nimcp_logging.h"
#include <stdbool.h>
#include <stddef.h>

static bool s_warned_lnn_gpu = false;

static void warn_once(const char* func) {
    if (!s_warned_lnn_gpu) {
        NIMCP_LOGGING_WARN("LNN GPU functions unavailable (CUDA not found). "
                          "Using CPU fallback. Called from: %s", func);
        s_warned_lnn_gpu = true;
    }
}

//=============================================================================
// LNN GPU Layer Lifecycle
//=============================================================================

nimcp_lnn_layer_gpu_t* nimcp_lnn_layer_gpu_create(
    nimcp_gpu_context_t* ctx,
    const lnn_layer_t* cpu_layer
) {
    warn_once("nimcp_lnn_layer_gpu_create");
    (void)ctx;
    (void)cpu_layer;
    return NULL;  /* GPU layer creation not available */
}

nimcp_lnn_layer_gpu_extended_t* nimcp_lnn_layer_gpu_create_from_config(
    nimcp_gpu_context_t* ctx,
    const nimcp_lnn_layer_config_t* config
) {
    warn_once("nimcp_lnn_layer_gpu_create_from_config");
    (void)ctx;
    (void)config;
    return NULL;
}

void nimcp_lnn_layer_gpu_destroy(nimcp_lnn_layer_gpu_t* layer) {
    (void)layer;
    /* No-op: NULL layer from stub create */
}

bool nimcp_lnn_layer_gpu_to_cpu(
    const nimcp_lnn_layer_gpu_t* gpu_layer,
    lnn_layer_t* cpu_layer
) {
    warn_once("nimcp_lnn_layer_gpu_to_cpu");
    (void)gpu_layer;
    (void)cpu_layer;
    return false;
}

//=============================================================================
// LNN GPU Forward Pass (ODE Integration)
//=============================================================================

bool nimcp_gpu_lnn_ode_step(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_layer_gpu_t* layer,
    const nimcp_gpu_tensor_t* input,
    const nimcp_lnn_ode_config_t* config
) {
    warn_once("nimcp_gpu_lnn_ode_step");
    (void)ctx;
    (void)layer;
    (void)input;
    (void)config;
    return false;
}

//=============================================================================
// LNN GPU Backward Pass (Adjoint Method)
//=============================================================================

bool nimcp_gpu_lnn_adjoint_step(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_layer_gpu_t* layer,
    nimcp_gpu_tensor_t* adjoint,
    const nimcp_gpu_tensor_t* x_at_t,
    const nimcp_gpu_tensor_t* input_at_t,
    float dt
) {
    warn_once("nimcp_gpu_lnn_adjoint_step");
    (void)ctx;
    (void)layer;
    (void)adjoint;
    (void)x_at_t;
    (void)input_at_t;
    (void)dt;
    return false;
}

bool nimcp_gpu_lnn_accumulate_gradients(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_layer_gpu_t* layer,
    const nimcp_gpu_tensor_t* adjoint,
    const nimcp_gpu_tensor_t* x_at_t,
    const nimcp_gpu_tensor_t* input_at_t,
    float dt
) {
    warn_once("nimcp_gpu_lnn_accumulate_gradients");
    (void)ctx;
    (void)layer;
    (void)adjoint;
    (void)x_at_t;
    (void)input_at_t;
    (void)dt;
    return false;
}

//=============================================================================
// LNN ODE Configuration
//=============================================================================

nimcp_lnn_ode_config_t nimcp_lnn_ode_default_config(void) {
    nimcp_lnn_ode_config_t config;

    /* Default ODE integration config (CPU fallback values) */
    config.method = 0;  /* RK4 */
    config.dt = 1.0f;
    config.dt_min = 0.01f;
    config.dt_max = 10.0f;
    config.error_tolerance = 1e-5f;
    config.max_steps = 1000;
    config.adaptive_stepping = false;

    return config;
}

