/**
 * @file nimcp_soma_jepa_bridge.c
 * @brief Somatosensory ↔ JEPA body-state prediction bridge (Phase 4r).
 *
 * Self-driving: each train_step reads the current body state directly
 * from the somatosensory module, compares to stored previous snapshot,
 * trains JEPA on (prev → cur), rolls snapshot forward.
 *
 * Embedding packing (first 32 channels; extras zeroed, shortened truncated):
 *   [0..15]   per-segment pain (first 16 body segments)
 *   [16..31]  per-segment temperature_sensation_t / 6 (first 16 segments)
 */
#include "core/brain/regions/somatosensory/nimcp_soma_jepa_bridge.h"
#include "core/brain/regions/somatosensory/nimcp_somatosensory.h"

#include "cognitive/jepa/nimcp_jepa_predictor.h"
#include "cognitive/jepa/nimcp_jepa_latent.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#include <string.h>

#define _SOMA_JEPA_SEGS 16u

struct soma_jepa_bridge_s {
    nimcp_somatosensory_t* soma;   /* non-owning */
    jepa_predictor_t*      predictor;
    jepa_latent_t*         ctx_latent;
    jepa_latent_t*         tgt_latent;
    uint32_t               embed_dim;
    float*                 prev_embed;    /* heap, size embed_dim */
    bool                   has_prev;
    uint32_t               n_steps;
    float                  last_loss;
};

static void _free_internals(soma_jepa_bridge_t* b) {
    if (!b) return;
    if (b->predictor)  { jepa_predictor_destroy(b->predictor);  b->predictor  = NULL; }
    if (b->ctx_latent) { jepa_latent_destroy(b->ctx_latent);    b->ctx_latent = NULL; }
    if (b->tgt_latent) { jepa_latent_destroy(b->tgt_latent);    b->tgt_latent = NULL; }
    if (b->prev_embed) { nimcp_free(b->prev_embed);             b->prev_embed = NULL; }
}

soma_jepa_bridge_t* soma_jepa_bridge_create(nimcp_somatosensory_t* soma,
                                             uint32_t embed_dim) {
    if (!soma || embed_dim == 0) return NULL;
    soma_jepa_bridge_t* b = (soma_jepa_bridge_t*)nimcp_calloc(1, sizeof(*b));
    if (!b) return NULL;
    b->soma = soma;
    b->embed_dim = embed_dim;

    jepa_predictor_config_t cfg = {0};
    (void)jepa_predictor_default_config(&cfg);
    cfg.input_dim        = embed_dim;
    cfg.output_dim       = embed_dim;
    cfg.hidden_dim       = embed_dim;
    cfg.num_layers       = 1;
    cfg.learning_rate    = 1e-3f;
    cfg.weight_decay     = 1e-5f;
    cfg.enable_layer_norm = true;
    cfg.enable_fep       = false;

    b->predictor  = jepa_predictor_create(&cfg);
    b->ctx_latent = jepa_latent_create_dim(embed_dim);
    b->tgt_latent = jepa_latent_create_dim(embed_dim);
    b->prev_embed = (float*)nimcp_calloc(embed_dim, sizeof(float));
    if (!b->predictor || !b->ctx_latent || !b->tgt_latent || !b->prev_embed) {
        _free_internals(b);
        nimcp_free(b);
        NIMCP_LOGGING_WARN("soma_jepa_bridge_create: sub-alloc failed");
        return NULL;
    }
    (void)jepa_predictor_set_training(b->predictor, true);
    NIMCP_LOGGING_INFO("soma_jepa_bridge: live (embed_dim=%u)", embed_dim);
    return b;
}

void soma_jepa_bridge_destroy(soma_jepa_bridge_t* b) {
    if (!b) return;
    _free_internals(b);
    nimcp_free(b);
}

/* Pack first 16 body segments' pain + temperature into a 32-dim embed,
 * zero-padding / truncating to b->embed_dim. */
static void _pack_soma_embed(soma_jepa_bridge_t* b, float* out) {
    memset(out, 0, sizeof(float) * b->embed_dim);
    uint32_t n_pain = b->embed_dim < _SOMA_JEPA_SEGS ? b->embed_dim : _SOMA_JEPA_SEGS;
    for (uint32_t s = 0; s < n_pain; s++) {
        float p = soma_get_pain_level(b->soma, (body_segment_t)s);
        if (p < 0.0f) p = 0.0f;
        if (p > 1.0f) p = 1.0f;
        out[s] = p;
    }
    uint32_t temp_base = _SOMA_JEPA_SEGS;
    uint32_t n_temp = (b->embed_dim > temp_base)
        ? (b->embed_dim - temp_base < _SOMA_JEPA_SEGS
           ? b->embed_dim - temp_base : _SOMA_JEPA_SEGS)
        : 0;
    for (uint32_t s = 0; s < n_temp; s++) {
        temp_sensation_t t = soma_get_temperature_sensation(
            b->soma, (body_segment_t)s);
        int ti = (int)t;
        if (ti < 0) ti = 0;
        if (ti > 6) ti = 6;
        out[temp_base + s] = (float)ti / 6.0f;
    }
}

int soma_jepa_bridge_train_step(soma_jepa_bridge_t* b, float* loss_out) {
    if (!b || !b->soma) return -1;

    float cur_embed[64];  /* fits the 32 default + headroom */
    /* If embed_dim > 64, heap-fallback. */
    float* work = (b->embed_dim <= 64)
        ? cur_embed
        : (float*)nimcp_calloc(b->embed_dim, sizeof(float));
    if (!work) return -1;

    _pack_soma_embed(b, work);

    int rc = -1;
    if (b->has_prev) {
        if (jepa_latent_set_embedding(b->ctx_latent, b->prev_embed, b->embed_dim) == 0 &&
            jepa_latent_set_embedding(b->tgt_latent, work,          b->embed_dim) == 0) {
            float step_loss = 0.0f;
            if (jepa_predictor_train_step(b->predictor,
                                           b->ctx_latent,
                                           b->tgt_latent, &step_loss) == 0) {
                b->n_steps++;
                b->last_loss = step_loss;
                if (loss_out) *loss_out = step_loss;
                rc = 0;
                NIMCP_LOGGING_DEBUG("soma_jepa_bridge: step %u loss=%.4f",
                                    b->n_steps, step_loss);
            }
        }
    }
    /* Roll snapshot forward regardless of training success (next tick
     * still wants a pair to work with). */
    memcpy(b->prev_embed, work, sizeof(float) * b->embed_dim);
    b->has_prev = true;

    if (work != cur_embed) nimcp_free(work);
    return rc;
}

uint32_t soma_jepa_bridge_n_steps(const soma_jepa_bridge_t* b) {
    return b ? b->n_steps : 0u;
}

float soma_jepa_bridge_last_loss(const soma_jepa_bridge_t* b) {
    return b ? b->last_loss : 0.0f;
}
