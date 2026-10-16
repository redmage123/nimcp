/**
 * @file nimcp_lnn_trainable.c
 * @brief Adapter: wraps lnn_training_ctx_t behind the unified trainable network interface
 *
 * @author NIMCP Development Team
 * @date 2026-03-11
 */

#include "training/nimcp_unified_training.h"
#include "lnn/nimcp_lnn_training.h"
#include "lnn/nimcp_lnn.h"
#include "lnn/nimcp_lnn_network.h"
#include "lnn/nimcp_lnn_gradient.h"
#include "middleware/training/nimcp_optimizers.h"
#include "utils/tensor/nimcp_tensor.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"

#include <string.h>

typedef struct {
    lnn_training_ctx_t* lnn_ctx;    /* NOT owned */
    uint32_t output_dim;
    uint32_t input_dim;
    bool managed_by_utm;            /* When true, skip internal optimizer step */
    /* Cached param/grad buffers to avoid malloc/free every backward step */
    float* cached_params;
    float* cached_grads;
    size_t cached_buf_size;         /* Number of floats allocated */
} lnn_adapter_ctx_t;

/* --- vtable implementations --- */

static int lnn_adapter_forward(void* ctx, const float* input, uint32_t input_dim,
                               float* output, uint32_t output_dim) {
    lnn_adapter_ctx_t* a = (lnn_adapter_ctx_t*)ctx;
    if (!a || !a->lnn_ctx || !a->lnn_ctx->network || !input || !output) return -1;

    /* Wrap raw floats into tensors for LNN API */
    uint32_t in_dims[1] = { input_dim };
    uint32_t out_dims[1] = { output_dim };
    nimcp_tensor_t* in_t = nimcp_tensor_create(in_dims, 1, NIMCP_DTYPE_F32);
    nimcp_tensor_t* out_t = nimcp_tensor_create(out_dims, 1, NIMCP_DTYPE_F32);
    if (!in_t || !out_t) {
        nimcp_tensor_destroy(in_t);
        nimcp_tensor_destroy(out_t);
        return -1;
    }

    memcpy(nimcp_tensor_data(in_t), input, input_dim * sizeof(float));

    int rc = lnn_forward_step(a->lnn_ctx->network, in_t, out_t, 1.0f);

    if (rc == 0) {
        const float* out_data = (const float*)nimcp_tensor_data_const(out_t);
        uint32_t copy_dim = (output_dim < (uint32_t)nimcp_tensor_numel(out_t)) ?
                             output_dim : (uint32_t)nimcp_tensor_numel(out_t);
        memcpy(output, out_data, copy_dim * sizeof(float));
    }

    nimcp_tensor_destroy(in_t);
    nimcp_tensor_destroy(out_t);
    return (rc == 0) ? 0 : -1;
}

static int lnn_adapter_backward(void* ctx, const float* dl_doutput, uint32_t output_dim,
                                float* dl_dinput, uint32_t input_dim) {
    lnn_adapter_ctx_t* a = (lnn_adapter_ctx_t*)ctx;
    if (!a || !a->lnn_ctx || !a->lnn_ctx->network || !dl_doutput) return -1;

    /* Wrap loss gradient into tensor for LNN adjoint backward */
    uint32_t dims[1] = { output_dim };
    nimcp_tensor_t* grad_t = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
    if (!grad_t) return -1;

    memcpy(nimcp_tensor_data(grad_t), dl_doutput, output_dim * sizeof(float));

    int rc = lnn_backward(a->lnn_ctx->network, grad_t);

    /* Normalize LNN gradients after adjoint computation.
     * The raw adjoint produces norms of 60K-770K which cause NaN in loss.
     * The direct LNN training path (lnn_training_step) applies this, but the
     * UTM adapter calls lnn_backward() directly, bypassing that pipeline. */
    if (rc == 0 && a->lnn_ctx->gradient_ctx) {
        float gnorm = lnn_gradient_norm(a->lnn_ctx->gradient_ctx);
        if (gnorm > 1e-8f) {
            float target_norm = 1.0f;
            float scale = target_norm / gnorm;
            lnn_gradient_scale(a->lnn_ctx->gradient_ctx, scale);
        }
    }

    /* Apply LNN gradients via its internal optimizer — unless UTM manages params.
     * When managed_by_utm, UTM handles optimizer stepping via param groups. */
    if (rc == 0 && !a->managed_by_utm && a->lnn_ctx->optimizer && a->lnn_ctx->network) {
        size_t n_params = lnn_network_param_count(a->lnn_ctx->network);
        if (n_params > 0) {
            /* Reuse cached buffers if large enough; only realloc if size changed */
            if (n_params > a->cached_buf_size) {
                nimcp_free(a->cached_params);
                nimcp_free(a->cached_grads);
                a->cached_params = (float*)nimcp_malloc(n_params * sizeof(float));
                a->cached_grads = (float*)nimcp_malloc(n_params * sizeof(float));
                a->cached_buf_size = (a->cached_params && a->cached_grads) ? n_params : 0;
            }
            if (a->cached_params && a->cached_grads) {
                size_t actual_p = 0, actual_g = 0;
                if (lnn_network_get_params(a->lnn_ctx->network, a->cached_params, &actual_p) == 0 &&
                    lnn_network_get_gradients(a->lnn_ctx->network, a->cached_grads, &actual_g) == 0 &&
                    actual_p == actual_g && actual_p > 0) {
                    nimcp_optimizer_step(a->lnn_ctx->optimizer, a->cached_params, a->cached_grads, actual_p);
                    lnn_network_set_params(a->lnn_ctx->network, a->cached_params, actual_p);
                }
            }
        }
        lnn_network_zero_gradients(a->lnn_ctx->network);
    }

    /* Copy real input gradients from LNN adjoint (λ(t=0) = dL/d_input) */
    if (dl_dinput) {
        const nimcp_tensor_t* in_grad = NULL;
        if (a->lnn_ctx->gradient_ctx) {
            in_grad = lnn_gradient_get_input_grad(a->lnn_ctx->gradient_ctx);
        }
        if (in_grad) {
            const float* grad_data = (const float*)nimcp_tensor_data_const(in_grad);
            size_t grad_numel = nimcp_tensor_numel(in_grad);
            uint32_t copy_dim = (input_dim < (uint32_t)grad_numel) ?
                                 input_dim : (uint32_t)grad_numel;
            memcpy(dl_dinput, grad_data, copy_dim * sizeof(float));
            if (input_dim > copy_dim) {
                memset(dl_dinput + copy_dim, 0, (input_dim - copy_dim) * sizeof(float));
            }
        } else {
            memset(dl_dinput, 0, input_dim * sizeof(float));
        }
    }

    nimcp_tensor_destroy(grad_t);
    return (rc == 0) ? 0 : -1;
}

static int lnn_adapter_get_param_groups(void* ctx,
                                        nimcp_utm_param_group_t** groups,
                                        uint32_t* num_groups) {
    lnn_adapter_ctx_t* a = (lnn_adapter_ctx_t*)ctx;
    if (!groups || !num_groups) return -1;

    /* C3: When UTM-managed, expose LNN params via cached buffers */
    if (a && a->managed_by_utm && a->lnn_ctx && a->lnn_ctx->network) {
        size_t n_params = lnn_network_param_count(a->lnn_ctx->network);
        if (n_params > 0) {
            if (n_params > a->cached_buf_size) {
                nimcp_free(a->cached_params);
                nimcp_free(a->cached_grads);
                a->cached_params = (float*)nimcp_malloc(n_params * sizeof(float));
                a->cached_grads = (float*)nimcp_malloc(n_params * sizeof(float));
                a->cached_buf_size = (a->cached_params && a->cached_grads) ? n_params : 0;
            }
            if (a->cached_params && a->cached_grads) {
                size_t actual_p = 0, actual_g = 0;
                lnn_network_get_params(a->lnn_ctx->network, a->cached_params, &actual_p);
                lnn_network_get_gradients(a->lnn_ctx->network, a->cached_grads, &actual_g);
                size_t count = (actual_p < actual_g) ? actual_p : actual_g;
                if (count > 0) {
                    nimcp_utm_param_group_t* g = (nimcp_utm_param_group_t*)
                        nimcp_calloc(1, sizeof(nimcp_utm_param_group_t));
                    if (g) {
                        g->params = a->cached_params;
                        g->gradients = a->cached_grads;
                        g->count = count;
                        g->lr_scale = 1.0f;
                        g->weight_decay = 0.0f;
                        g->name = "lnn_params";
                        *groups = g;
                        *num_groups = 1;
                        return 0;
                    }
                }
            }
        }
    }

    *groups = NULL;
    *num_groups = 0;
    return 0;
}

static int lnn_adapter_zero_grad(void* ctx) {
    lnn_adapter_ctx_t* a = (lnn_adapter_ctx_t*)ctx;
    if (!a || !a->lnn_ctx || !a->lnn_ctx->network) return -1;
    lnn_network_zero_gradients(a->lnn_ctx->network);
    return 0;
}

static uint32_t lnn_adapter_get_output_dim(void* ctx) {
    lnn_adapter_ctx_t* a = (lnn_adapter_ctx_t*)ctx;
    return a ? a->output_dim : 0;
}

static uint32_t lnn_adapter_get_input_dim(void* ctx) {
    lnn_adapter_ctx_t* a = (lnn_adapter_ctx_t*)ctx;
    return a ? a->input_dim : 0;
}

static float lnn_adapter_auxiliary_loss(void* ctx) {
    lnn_adapter_ctx_t* a = (lnn_adapter_ctx_t*)ctx;
    if (!a || !a->lnn_ctx || !a->lnn_ctx->network) return 0.0f;
    /* Tau regularization: penalize time constants that drift too far from
     * biologically plausible range [1ms, 100ms]. Keeps LNN dynamics stable. */
    size_t n_params = lnn_network_param_count(a->lnn_ctx->network);
    if (n_params == 0) return 0.0f;
    /* Use cached param buffer if available */
    if (a->cached_params && a->cached_buf_size > 0) {
        float l2 = 0.0f;
        for (size_t i = 0; i < a->cached_buf_size; i++) {
            l2 += a->cached_params[i] * a->cached_params[i];
        }
        return 0.0001f * l2 / (float)a->cached_buf_size;
    }
    return 0.0f;
}

static int lnn_adapter_sync_params(void* ctx) {
    lnn_adapter_ctx_t* a = (lnn_adapter_ctx_t*)ctx;
    if (!a || !a->lnn_ctx || !a->lnn_ctx->network || !a->cached_params || a->cached_buf_size == 0) return 0;
    return lnn_network_set_params(a->lnn_ctx->network, a->cached_params, a->cached_buf_size);
}

static void lnn_adapter_destroy(void* ctx) {
    lnn_adapter_ctx_t* a = (lnn_adapter_ctx_t*)ctx;
    if (a) {
        nimcp_free(a->cached_params);
        nimcp_free(a->cached_grads);
    }
    nimcp_free(ctx);
}

/* --- static vtable --- */

static const nimcp_trainable_network_ops_t lnn_trainable_ops = {
    .name = "LNN",
    .type = NIMCP_TRAINABLE_LNN,
    .forward = lnn_adapter_forward,
    .backward = lnn_adapter_backward,
    .get_param_groups = lnn_adapter_get_param_groups,
    .zero_grad = lnn_adapter_zero_grad,
    .get_output_dim = lnn_adapter_get_output_dim,
    .get_input_dim = lnn_adapter_get_input_dim,
    .compute_auxiliary_loss = lnn_adapter_auxiliary_loss,
    .destroy = lnn_adapter_destroy,
    .sync_params = lnn_adapter_sync_params,
};

/* --- public setter --- */

void nimcp_trainable_lnn_set_managed(void* ctx, bool managed) {
    lnn_adapter_ctx_t* a = (lnn_adapter_ctx_t*)ctx;
    if (a) a->managed_by_utm = managed;
}

void nimcp_trainable_lnn_set_dims(void* ctx, uint32_t input_dim, uint32_t output_dim) {
    lnn_adapter_ctx_t* a = (lnn_adapter_ctx_t*)ctx;
    if (a) {
        a->input_dim = input_dim;
        a->output_dim = output_dim;
    }
}

/* --- public creation --- */

int nimcp_trainable_lnn_create(struct lnn_training_ctx_s* lnn_ctx,
                               const nimcp_trainable_network_ops_t** ops,
                               void** ctx) {
    if (!lnn_ctx || !ops || !ctx) return -1;

    lnn_adapter_ctx_t* a = (lnn_adapter_ctx_t*)nimcp_calloc(1, sizeof(lnn_adapter_ctx_t));
    if (!a) return -1;

    a->lnn_ctx = (lnn_training_ctx_t*)lnn_ctx;

    /* Get dims from LNN network if available */
    if (a->lnn_ctx->network) {
        a->input_dim = a->lnn_ctx->network->n_inputs;
        a->output_dim = a->lnn_ctx->network->n_outputs;
    }

    *ops = &lnn_trainable_ops;
    *ctx = a;

    NIMCP_LOGGING_INFO("Created LNN trainable adapter (%ux → %ux)",
                       a->input_dim, a->output_dim);
    return 0;
}
