/**
 * @file nimcp_adaptive_trainable.c
 * @brief Adapter: wraps neural_network_struct behind the unified trainable network interface
 *
 * @author NIMCP Development Team
 * @date 2026-03-11
 */

#include "training/nimcp_unified_training.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "core/neuralnet/nimcp_neuralnet_internal.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"

#include <string.h>
#include <math.h>

typedef struct {
    neural_network_t network;       /* NOT owned — pointer typedef */
    float* last_output;             /* Cached forward output for backward */
    uint32_t output_dim;
    uint32_t input_dim;
    bool managed_by_utm;            /* Item 1: When true, UTM owns optimizer step */
} adaptive_adapter_ctx_t;

/* --- vtable implementations --- */

static int adaptive_adapter_forward(void* ctx, const float* input, uint32_t input_dim,
                                    float* output, uint32_t output_dim) {
    adaptive_adapter_ctx_t* a = (adaptive_adapter_ctx_t*)ctx;
    if (!a || !a->network || !input || !output) return -1;

    bool ok = neural_network_forward(a->network, input, input_dim, output, output_dim);
    if (!ok) return -1;

    /* Cache output for backward pass MSE gradient */
    if (!a->last_output && output_dim > 0) {
        a->last_output = (float*)nimcp_calloc(output_dim, sizeof(float));
    }
    if (a->last_output) {
        memcpy(a->last_output, output, output_dim * sizeof(float));
    }
    return 0;
}

static int adaptive_adapter_backward(void* ctx, const float* dl_doutput, uint32_t output_dim,
                                     float* dl_dinput, uint32_t input_dim) {
    adaptive_adapter_ctx_t* a = (adaptive_adapter_ctx_t*)ctx;
    if (!a || !a->network) return -1;
    /* Adaptive network's backward pass is handled internally by
     * adaptive_network_learn(). This propagates scaled input gradients for
     * cross-network bridge gradient flow. */
    if (dl_dinput && dl_doutput) {
        /* Scale gradient by output-to-input ratio derived from cached forward pass.
         * This approximates the average Jacobian magnitude without computing the
         * full n×m matrix. Better than identity copy for bridge gradient flow. */
        float out_norm_sq = 0.0f;
        for (uint32_t i = 0; i < output_dim; i++) {
            out_norm_sq += dl_doutput[i] * dl_doutput[i];
        }
        /* Use last_output to estimate the network's gain (output_norm / expected_input_norm) */
        float output_mag = 0.0f;
        if (a->last_output) {
            for (uint32_t i = 0; i < a->output_dim && i < output_dim; i++) {
                output_mag += a->last_output[i] * a->last_output[i];
            }
            output_mag = sqrtf(output_mag + 1e-8f);
        }
        /* Scale factor: normalize gradient by network gain to prevent explosion/vanishing.
         * Target: gradient magnitude ~ 1.0 relative to loss gradient. */
        float scale = (output_mag > 0.1f) ? (1.0f / output_mag) : 1.0f;
        if (scale > 10.0f) scale = 10.0f; /* Clamp for stability */

        uint32_t dim = (input_dim < output_dim) ? input_dim : output_dim;
        for (uint32_t i = 0; i < dim; i++) {
            dl_dinput[i] = dl_doutput[i] * scale;
        }
        if (input_dim > dim) {
            memset(dl_dinput + dim, 0, (input_dim - dim) * sizeof(float));
        }
    } else if (dl_dinput) {
        memset(dl_dinput, 0, input_dim * sizeof(float));
    }
    return 0;
}

static int adaptive_adapter_get_param_groups(void* ctx,
                                             nimcp_utm_param_group_t** groups,
                                             uint32_t* num_groups) {
    (void)ctx;
    if (!groups || !num_groups) return -1;
    /* Adaptive network parameters are managed internally by GPU bridge.
     * Return empty — UTM composite loss still drives the network via
     * adaptive_network_learn() in brain_learn_vector(). */
    *groups = NULL;
    *num_groups = 0;
    return 0;
}

static int adaptive_adapter_zero_grad(void* ctx) {
    (void)ctx;
    return 0;
}

static uint32_t adaptive_adapter_get_output_dim(void* ctx) {
    adaptive_adapter_ctx_t* a = (adaptive_adapter_ctx_t*)ctx;
    return a ? a->output_dim : 0;
}

static uint32_t adaptive_adapter_get_input_dim(void* ctx) {
    adaptive_adapter_ctx_t* a = (adaptive_adapter_ctx_t*)ctx;
    return a ? a->input_dim : 0;
}

static float adaptive_adapter_auxiliary_loss(void* ctx) {
    adaptive_adapter_ctx_t* a = (adaptive_adapter_ctx_t*)ctx;
    if (!a || !a->last_output || a->output_dim == 0) return 0.0f;
    /* L2 activity regularization: penalize excessively large outputs
     * to encourage sparse, bounded activations (biologically plausible) */
    float l2 = 0.0f;
    for (uint32_t i = 0; i < a->output_dim; i++) {
        l2 += a->last_output[i] * a->last_output[i];
    }
    return 0.001f * l2 / (float)a->output_dim;
}

static void adaptive_adapter_destroy(void* ctx) {
    adaptive_adapter_ctx_t* a = (adaptive_adapter_ctx_t*)ctx;
    if (a) {
        nimcp_free(a->last_output);
        nimcp_free(a);
    }
}

/* --- public setter --- */

void nimcp_trainable_adaptive_set_dims(void* ctx, uint32_t input_dim, uint32_t output_dim) {
    adaptive_adapter_ctx_t* a = (adaptive_adapter_ctx_t*)ctx;
    if (a) {
        a->input_dim = input_dim;
        a->output_dim = output_dim;
    }
}

/* --- static vtable --- */

static const nimcp_trainable_network_ops_t adaptive_trainable_ops = {
    .name = "Adaptive",
    .type = NIMCP_TRAINABLE_ADAPTIVE,
    .forward = adaptive_adapter_forward,
    .backward = adaptive_adapter_backward,
    .get_param_groups = adaptive_adapter_get_param_groups,
    .zero_grad = adaptive_adapter_zero_grad,
    .get_output_dim = adaptive_adapter_get_output_dim,
    .get_input_dim = adaptive_adapter_get_input_dim,
    .compute_auxiliary_loss = adaptive_adapter_auxiliary_loss,
    .destroy = adaptive_adapter_destroy,
    .sync_params = NULL, /* Adaptive modifies weights in-place */
};

/* --- public creation --- */

int nimcp_trainable_adaptive_create(struct neural_network_struct* network,
                                    const nimcp_trainable_network_ops_t** ops,
                                    void** ctx) {
    if (!network || !ops || !ctx) return -1;

    adaptive_adapter_ctx_t* a = (adaptive_adapter_ctx_t*)nimcp_calloc(
        1, sizeof(adaptive_adapter_ctx_t));
    if (!a) return -1;

    a->network = network;  /* neural_network_t is already a pointer */
    /* Extract dims from network config (internal header gives struct access) */
    a->input_dim = network->config.input_size;
    a->output_dim = network->config.output_size;

    *ops = &adaptive_trainable_ops;
    *ctx = a;

    NIMCP_LOGGING_INFO("Created Adaptive trainable adapter (%ux → %ux)",
                       a->input_dim, a->output_dim);
    return 0;
}
