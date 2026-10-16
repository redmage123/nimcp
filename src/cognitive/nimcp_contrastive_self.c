/**
 * @file nimcp_contrastive_self.c
 * @brief Contrastive self-learning — sharpen categories via hard negatives
 *
 * KG INTEGRATION (W13, 2026-04-24): This module is a pure helper with no
 * brain pointer. Contrastive-record events are emitted to the KG from the
 * brain-side caller (nimcp_brain_learning.c), where the brain handle is in
 * scope. See include/cognitive/kg/nimcp_wave13_metacog_kg.h.
 */
#include "cognitive/nimcp_contrastive_self.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>
#include <string.h>

#define LOG_MODULE "CONTRASTIVE_SELF"
#define MAX_DIM 1024

typedef struct {
    float input[MAX_DIM];
    float output[MAX_DIM];
    char label[64];
    uint32_t input_dim;
    uint32_t output_dim;
} stored_sample_t;

struct nimcp_contrastive_self {
    nimcp_contrastive_self_config_t config;
    stored_sample_t* buffer;
    uint32_t count;
    uint32_t head;
};

nimcp_contrastive_self_config_t nimcp_contrastive_self_config_default(void) {
    nimcp_contrastive_self_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.buffer_capacity = 200;
    cfg.negative_margin = 0.3f;
    cfg.contrastive_weight = 0.1f;
    cfg.negatives_per_sample = 3;
    return cfg;
}

nimcp_contrastive_self_t* nimcp_contrastive_self_create(
    const nimcp_contrastive_self_config_t* config)
{
    nimcp_contrastive_self_t* cs = nimcp_calloc(1, sizeof(nimcp_contrastive_self_t));
    if (!cs) return NULL;
    cs->config = config ? *config : nimcp_contrastive_self_config_default();
    cs->buffer = nimcp_calloc(cs->config.buffer_capacity, sizeof(stored_sample_t));
    if (!cs->buffer) { nimcp_free(cs); return NULL; }
    return cs;
}

void nimcp_contrastive_self_destroy(nimcp_contrastive_self_t* cs) {
    if (!cs) return;
    nimcp_free(cs->buffer);
    nimcp_free(cs);
}

static float _cosine_sim(const float* a, const float* b, uint32_t n) {
    float dot = 0, na = 0, nb = 0;
    for (uint32_t i = 0; i < n; i++) {
        dot += a[i] * b[i];
        na += a[i] * a[i];
        nb += b[i] * b[i];
    }
    return dot / (sqrtf(na) * sqrtf(nb) + 1e-8f);
}

void nimcp_contrastive_self_record(nimcp_contrastive_self_t* cs,
    const float* input, uint32_t input_dim,
    const float* output, uint32_t output_dim,
    const char* label)
{
    if (!cs || !input || !output) return;
    uint32_t idx = cs->head % cs->config.buffer_capacity;
    stored_sample_t* s = &cs->buffer[idx];

    uint32_t id = input_dim < MAX_DIM ? input_dim : MAX_DIM;
    uint32_t od = output_dim < MAX_DIM ? output_dim : MAX_DIM;
    memcpy(s->input, input, id * sizeof(float));
    memcpy(s->output, output, od * sizeof(float));
    s->input_dim = id;
    s->output_dim = od;
    if (label) snprintf(s->label, sizeof(s->label), "%s", label);

    cs->head++;
    if (cs->count < cs->config.buffer_capacity) cs->count++;
}

int nimcp_contrastive_self_generate_negatives(nimcp_contrastive_self_t* cs,
    float* neg_features, float* neg_targets, uint32_t max_pairs,
    uint32_t feat_dim, uint32_t target_dim)
{
    if (!cs || !neg_features || !neg_targets || cs->count < 10) return 0;

    uint32_t generated = 0;
    uint32_t fd = feat_dim < MAX_DIM ? feat_dim : MAX_DIM;
    uint32_t td = target_dim < MAX_DIM ? target_dim : MAX_DIM;

    /* For each recent sample, find hard negatives: similar input, different label */
    for (uint32_t i = 0; i < cs->count && generated < max_pairs; i++) {
        stored_sample_t* anchor = &cs->buffer[i];
        uint32_t neg_found = 0;

        for (uint32_t j = 0; j < cs->count && neg_found < cs->config.negatives_per_sample; j++) {
            if (i == j) continue;
            stored_sample_t* candidate = &cs->buffer[j];

            /* Same label = positive, skip */
            if (strcmp(anchor->label, candidate->label) == 0) continue;

            /* Check input similarity (want similar inputs with different outputs) */
            float input_sim = _cosine_sim(anchor->input, candidate->input,
                                           fd < anchor->input_dim ? fd : anchor->input_dim);

            /* Verify outputs are actually different (not just different labels) */
            float output_sim = _cosine_sim(anchor->output, candidate->output,
                                            td < anchor->output_dim ? td : anchor->output_dim);
            if (output_sim > 0.95f) continue; /* Too similar — not a useful negative */

            if (input_sim > 0.5f) {
                /* Hard negative found — inputs are similar but labels differ */
                uint32_t offset_f = generated * fd;
                uint32_t offset_t = generated * td;
                if (offset_f + fd > max_pairs * fd) break;

                /* Features: the anchor input */
                memcpy(neg_features + offset_f, anchor->input,
                       fd * sizeof(float));
                /* Target: push AWAY from candidate output (negate) */
                for (uint32_t k = 0; k < td && k < candidate->output_dim; k++)
                    neg_targets[offset_t + k] = -candidate->output[k] *
                                                 cs->config.contrastive_weight;

                generated++;
                neg_found++;
            }
        }
    }

    return (int)generated;
}
