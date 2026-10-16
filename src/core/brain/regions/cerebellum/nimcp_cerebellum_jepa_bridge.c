/**
 * @file nimcp_cerebellum_jepa_bridge.c
 * @brief Cerebellum ↔ JEPA sensorimotor prediction bridge (Phase 4s).
 */
#include "core/brain/regions/cerebellum/nimcp_cerebellum_jepa_bridge.h"

#include "cognitive/jepa/nimcp_jepa_predictor.h"
#include "cognitive/jepa/nimcp_jepa_latent.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#include <stdbool.h>
#include <string.h>

struct cerebellum_jepa_bridge_s {
    jepa_predictor_t* predictor;
    jepa_latent_t*    ctx_latent;  /* motor-command buffer */
    jepa_latent_t*    tgt_latent;  /* outcome buffer */
    uint32_t          embed_dim;
    uint32_t          n_steps;
    float             last_loss;
    /* Self-drive rolling prev (for tick_from_vec). */
    float*            prev_embed;
    bool              has_prev;
};

/* Free partial state on any create failure so callers never see a
 * half-initialized bridge and no resources leak. */
static void _free_internals(cerebellum_jepa_bridge_t* b) {
    if (!b) return;
    if (b->predictor)  { jepa_predictor_destroy(b->predictor);  b->predictor  = NULL; }
    if (b->ctx_latent) { jepa_latent_destroy(b->ctx_latent);    b->ctx_latent = NULL; }
    if (b->tgt_latent) { jepa_latent_destroy(b->tgt_latent);    b->tgt_latent = NULL; }
    if (b->prev_embed) { nimcp_free(b->prev_embed);             b->prev_embed = NULL; }
}

cerebellum_jepa_bridge_t* cerebellum_jepa_bridge_create(uint32_t embed_dim) {
    if (embed_dim == 0) {
        NIMCP_LOGGING_WARN("cerebellum_jepa_bridge_create: embed_dim=0 rejected");
        return NULL;
    }
    cerebellum_jepa_bridge_t* b =
        (cerebellum_jepa_bridge_t*)nimcp_calloc(1, sizeof(*b));
    if (!b) return NULL;
    b->embed_dim = embed_dim;

    jepa_predictor_config_t cfg = {0};
    (void)jepa_predictor_default_config(&cfg);
    cfg.input_dim        = embed_dim;
    cfg.output_dim       = embed_dim;
    cfg.hidden_dim       = embed_dim;
    cfg.num_layers       = 1;
    cfg.learning_rate    = 1e-3f;
    cfg.weight_decay     = 1e-5f;
    cfg.dropout_rate     = 0.0f;
    cfg.enable_layer_norm = true;
    cfg.enable_fep       = false;

    b->predictor  = jepa_predictor_create(&cfg);
    b->ctx_latent = jepa_latent_create_dim(embed_dim);
    b->tgt_latent = jepa_latent_create_dim(embed_dim);
    b->prev_embed = (float*)nimcp_calloc(embed_dim, sizeof(float));
    if (!b->predictor || !b->ctx_latent || !b->tgt_latent || !b->prev_embed) {
        _free_internals(b);
        nimcp_free(b);
        NIMCP_LOGGING_WARN("cerebellum_jepa_bridge_create: sub-alloc failed");
        return NULL;
    }
    (void)jepa_predictor_set_training(b->predictor, true);
    NIMCP_LOGGING_INFO("cerebellum_jepa_bridge: live (embed_dim=%u)", embed_dim);
    return b;
}

void cerebellum_jepa_bridge_destroy(cerebellum_jepa_bridge_t* b) {
    if (!b) return;
    _free_internals(b);
    nimcp_free(b);
}

int cerebellum_jepa_bridge_record(cerebellum_jepa_bridge_t* b,
                                   const float* motor_cmd,
                                   const float* outcome,
                                   uint32_t dim,
                                   float* loss_out) {
    if (!b || !motor_cmd || !outcome) return -1;
    if (dim != b->embed_dim) return -1;

    if (jepa_latent_set_embedding(b->ctx_latent, motor_cmd, dim) != 0) return -1;
    if (jepa_latent_set_embedding(b->tgt_latent, outcome,   dim) != 0) return -1;

    float step_loss = 0.0f;
    int rc = jepa_predictor_train_step(b->predictor,
                                        b->ctx_latent,
                                        b->tgt_latent, &step_loss);
    if (rc != 0) return -1;

    b->n_steps++;
    b->last_loss = step_loss;
    if (loss_out) *loss_out = step_loss;
    NIMCP_LOGGING_DEBUG("cerebellum_jepa_bridge: step %u loss=%.4f",
                        b->n_steps, step_loss);
    return 0;
}

int cerebellum_jepa_bridge_predict(cerebellum_jepa_bridge_t* b,
                                    const float* motor_cmd,
                                    uint32_t dim,
                                    float* out_pred) {
    if (!b || !motor_cmd || !out_pred) return -1;
    if (dim != b->embed_dim) return -1;
    if (jepa_latent_set_embedding(b->ctx_latent, motor_cmd, dim) != 0) return -1;
    if (jepa_predictor_predict(b->predictor, b->ctx_latent, b->tgt_latent) != 0) return -1;
    return jepa_latent_get_embedding(b->tgt_latent, out_pred, dim);
}

uint32_t cerebellum_jepa_bridge_n_steps(const cerebellum_jepa_bridge_t* b) {
    return b ? b->n_steps : 0u;
}

float cerebellum_jepa_bridge_last_loss(const cerebellum_jepa_bridge_t* b) {
    return b ? b->last_loss : 0.0f;
}

int cerebellum_jepa_bridge_tick_from_vec(cerebellum_jepa_bridge_t* b,
                                          const float* vec,
                                          uint32_t dim) {
    if (!b || !vec || dim == 0 || !b->prev_embed) return -1;
    float scratch_stack[1024];
    float* work = (b->embed_dim <= 1024) ? scratch_stack
        : (float*)nimcp_calloc(b->embed_dim, sizeof(float));
    if (!work) return -1;
    uint32_t copy = dim < b->embed_dim ? dim : b->embed_dim;
    if (copy > 0) memcpy(work, vec, sizeof(float) * copy);
    if (copy < b->embed_dim) {
        memset(work + copy, 0, sizeof(float) * (b->embed_dim - copy));
    }
    int rc = -1;
    if (b->has_prev) {
        rc = cerebellum_jepa_bridge_record(b, b->prev_embed, work,
                                            b->embed_dim, NULL);
    }
    memcpy(b->prev_embed, work, sizeof(float) * b->embed_dim);
    b->has_prev = true;
    if (work != scratch_stack) nimcp_free(work);
    return rc;
}
