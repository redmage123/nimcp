/**
 * @file nimcp_snn_trainable.c
 * @brief Adapter: wraps snn_backprop_ctx_t behind the unified trainable network interface
 *
 * @author NIMCP Development Team
 * @date 2026-03-11
 */

#include "training/nimcp_unified_training.h"
#include "training/nimcp_snn_backprop.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"

#include <string.h>

typedef struct {
    snn_backprop_ctx_t* snn_ctx;    /* NOT owned */
    uint32_t output_dim;
    uint32_t input_dim;
    bool managed_by_utm;            /* When true, skip internal optimizer step */
    /* C2: Cached flat weights for UTM param group exposure */
    float* cached_weights;
    float* cached_grads;
    size_t cached_count;
} snn_adapter_ctx_t;

/* --- vtable implementations --- */

static int snn_adapter_forward(void* ctx, const float* input, uint32_t input_dim,
                               float* output, uint32_t output_dim) {
    snn_adapter_ctx_t* a = (snn_adapter_ctx_t*)ctx;
    if (!a || !a->snn_ctx || !input || !output) return -1;

    /* SNN forward: single sample, 1ms duration */
    int rc = snn_backprop_forward(a->snn_ctx, input, 1, 1.0f, output);
    return (rc == 0) ? 0 : -1;
}

static int snn_adapter_backward(void* ctx, const float* dl_doutput, uint32_t output_dim,
                                float* dl_dinput, uint32_t input_dim) {
    snn_adapter_ctx_t* a = (snn_adapter_ctx_t*)ctx;
    if (!a || !a->snn_ctx || !dl_doutput) return -1;

    /* SNN backward: accumulates gradients into ctx->gradients */
    int rc = snn_backprop_backward(a->snn_ctx, dl_doutput, 1);

    /* Run SNN's internal optimizer step after backward — unless UTM manages params */
    if (rc == 0 && !a->managed_by_utm) {
        snn_backprop_step(a->snn_ctx, 0.0f);  /* 0 = use config LR */
    }

    /* Copy real input gradients from SNN surrogate gradient backprop */
    if (dl_dinput) {
        uint32_t grad_size = 0;
        const float* in_grad = snn_backprop_get_input_grad(a->snn_ctx, &grad_size);
        if (in_grad && grad_size > 0) {
            uint32_t copy_dim = (input_dim < grad_size) ? input_dim : grad_size;
            memcpy(dl_dinput, in_grad, copy_dim * sizeof(float));
            if (input_dim > copy_dim) {
                memset(dl_dinput + copy_dim, 0, (input_dim - copy_dim) * sizeof(float));
            }
        } else {
            memset(dl_dinput, 0, input_dim * sizeof(float));
        }
    }
    return (rc == 0) ? 0 : -1;
}

static int snn_adapter_get_param_groups(void* ctx,
                                        nimcp_utm_param_group_t** groups,
                                        uint32_t* num_groups) {
    snn_adapter_ctx_t* a = (snn_adapter_ctx_t*)ctx;
    if (!groups || !num_groups) return -1;

    /* C2: When UTM-managed, expose flat weights as param group */
    if (a && a->managed_by_utm && a->snn_ctx) {
        size_t w_count = 0;
        float* flat_w = snn_backprop_get_flat_weights(a->snn_ctx, &w_count);
        if (flat_w && w_count > 0) {
            /* Update cached weights */
            if (a->cached_count != w_count) {
                nimcp_free(a->cached_weights);
                nimcp_free(a->cached_grads);
                a->cached_weights = flat_w;
                a->cached_grads = (float*)nimcp_calloc(w_count, sizeof(float));
                a->cached_count = w_count;
            } else {
                memcpy(a->cached_weights, flat_w, w_count * sizeof(float));
                nimcp_free(flat_w);
            }

            /* Get gradient data */
            size_t g_count = 0;
            float* grad_ptr = snn_backprop_get_flat_weight_grads(a->snn_ctx, &g_count);
            if (grad_ptr && g_count > 0 && a->cached_grads) {
                size_t copy_n = (g_count < w_count) ? g_count : w_count;
                memcpy(a->cached_grads, grad_ptr, copy_n * sizeof(float));
            }

            nimcp_utm_param_group_t* g = (nimcp_utm_param_group_t*)
                nimcp_calloc(1, sizeof(nimcp_utm_param_group_t));
            if (g) {
                g->params = a->cached_weights;
                g->gradients = a->cached_grads;
                g->count = w_count;
                g->lr_scale = 1.0f;
                g->weight_decay = 0.0f;
                g->name = "snn_weights";
                *groups = g;
                *num_groups = 1;
                return 0;
            }
        } else {
            nimcp_free(flat_w);
        }
    }

    *groups = NULL;
    *num_groups = 0;
    return 0;
}

static int snn_adapter_zero_grad(void* ctx) {
    snn_adapter_ctx_t* a = (snn_adapter_ctx_t*)ctx;
    if (!a || !a->snn_ctx) return -1;
    snn_backprop_zero_grad(a->snn_ctx);
    return 0;
}

static uint32_t snn_adapter_get_output_dim(void* ctx) {
    snn_adapter_ctx_t* a = (snn_adapter_ctx_t*)ctx;
    return a ? a->output_dim : 0;
}

static uint32_t snn_adapter_get_input_dim(void* ctx) {
    snn_adapter_ctx_t* a = (snn_adapter_ctx_t*)ctx;
    return a ? a->input_dim : 0;
}

static float snn_adapter_auxiliary_loss(void* ctx) {
    snn_adapter_ctx_t* a = (snn_adapter_ctx_t*)ctx;
    if (!a || !a->snn_ctx) return 0.0f;
    /* L2 weight regularization to prevent SNN weight explosion */
    float w_norm = snn_backprop_get_weight_norm(a->snn_ctx);
    return 0.0001f * w_norm * w_norm;
}

static int snn_adapter_sync_params(void* ctx) {
    snn_adapter_ctx_t* a = (snn_adapter_ctx_t*)ctx;
    if (!a || !a->snn_ctx || !a->cached_weights || a->cached_count == 0) return 0;
    return snn_backprop_set_flat_weights(a->snn_ctx, a->cached_weights, a->cached_count);
}

static void snn_adapter_destroy(void* ctx) {
    snn_adapter_ctx_t* a = (snn_adapter_ctx_t*)ctx;
    if (a) {
        nimcp_free(a->cached_weights);
        nimcp_free(a->cached_grads);
    }
    nimcp_free(ctx);
}

/* --- static vtable --- */

static const nimcp_trainable_network_ops_t snn_trainable_ops = {
    .name = "SNN",
    .type = NIMCP_TRAINABLE_SNN,
    .forward = snn_adapter_forward,
    .backward = snn_adapter_backward,
    .get_param_groups = snn_adapter_get_param_groups,
    .zero_grad = snn_adapter_zero_grad,
    .get_output_dim = snn_adapter_get_output_dim,
    .get_input_dim = snn_adapter_get_input_dim,
    .compute_auxiliary_loss = snn_adapter_auxiliary_loss,
    .destroy = snn_adapter_destroy,
    .sync_params = snn_adapter_sync_params,
};

/* --- public setters --- */

void nimcp_trainable_snn_set_managed(void* ctx, bool managed) {
    snn_adapter_ctx_t* a = (snn_adapter_ctx_t*)ctx;
    if (a) a->managed_by_utm = managed;
}

void nimcp_trainable_snn_set_dims(void* ctx, uint32_t input_dim, uint32_t output_dim) {
    snn_adapter_ctx_t* a = (snn_adapter_ctx_t*)ctx;
    if (a) {
        a->input_dim = input_dim;
        a->output_dim = output_dim;
    }
}

/* --- public creation --- */

int nimcp_trainable_snn_create(struct snn_backprop_ctx_s* snn_ctx,
                               const nimcp_trainable_network_ops_t** ops,
                               void** ctx) {
    if (!snn_ctx || !ops || !ctx) return -1;

    snn_adapter_ctx_t* a = (snn_adapter_ctx_t*)nimcp_calloc(1, sizeof(snn_adapter_ctx_t));
    if (!a) return -1;

    a->snn_ctx = (snn_backprop_ctx_t*)snn_ctx;
    /* Dims set from brain config at registration time.
     * SNN doesn't expose dims via public API — these are set by
     * the caller after nimcp_trainable_snn_create(). */
    a->output_dim = 0;
    a->input_dim = 0;

    *ops = &snn_trainable_ops;
    *ctx = a;

    NIMCP_LOGGING_INFO("Created SNN trainable adapter");
    return 0;
}
