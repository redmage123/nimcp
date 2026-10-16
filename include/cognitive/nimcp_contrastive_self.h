/**
 * @file nimcp_contrastive_self.h
 * @brief Contrastive self-learning — train with hard negatives ("this is NOT that")
 */
#ifndef NIMCP_CONTRASTIVE_SELF_H
#define NIMCP_CONTRASTIVE_SELF_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t buffer_capacity;      /* Max stored embeddings (default 200) */
    float negative_margin;         /* Min distance for negatives (default 0.3) */
    float contrastive_weight;      /* Loss weight for contrastive term (default 0.1) */
    uint32_t negatives_per_sample; /* Hard negatives per positive (default 3) */
} nimcp_contrastive_self_config_t;

typedef struct nimcp_contrastive_self nimcp_contrastive_self_t;

nimcp_contrastive_self_t* nimcp_contrastive_self_create(
    const nimcp_contrastive_self_config_t* config);
void nimcp_contrastive_self_destroy(nimcp_contrastive_self_t* cs);

/* Record a (input, output, label) for negative mining */
void nimcp_contrastive_self_record(nimcp_contrastive_self_t* cs,
    const float* input, uint32_t input_dim,
    const float* output, uint32_t output_dim,
    const char* label);

/* Generate hard negative training pairs. Returns count generated.
 * For each pair: (input_a, input_b) are similar inputs with different outputs.
 * Training the brain to distinguish them sharpens category boundaries. */
int nimcp_contrastive_self_generate_negatives(nimcp_contrastive_self_t* cs,
    float* neg_features, float* neg_targets, uint32_t max_pairs,
    uint32_t feat_dim, uint32_t target_dim);

nimcp_contrastive_self_config_t nimcp_contrastive_self_config_default(void);

#ifdef __cplusplus
}
#endif
#endif
