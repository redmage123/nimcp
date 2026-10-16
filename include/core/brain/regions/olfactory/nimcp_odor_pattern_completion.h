/**
 * @file nimcp_odor_pattern_completion.h
 * @brief Odor Pattern Completion and Separation Module
 * @version Phase 6: Sensory Processing
 * @date 2026-01-12
 *
 * Similar to hippocampal pattern completion/separation, the piriform
 * cortex can complete partial odor patterns and separate similar odors.
 */

#ifndef NIMCP_ODOR_PATTERN_COMPLETION_H
#define NIMCP_ODOR_PATTERN_COMPLETION_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "core/brain/regions/olfactory/nimcp_olfactory.h"

#define PATTERN_COMP_ATTRACTOR_ITER 10
#define PATTERN_COMP_MIN_OVERLAP    0.3f
#define PATTERN_SEP_THRESHOLD       0.2f

typedef struct {
    float completion_threshold;
    float separation_threshold;
    uint32_t max_iterations;
    float learning_rate;
    float sparsity_target;
} odor_pattern_config_t;

typedef struct odor_pattern_ctx* odor_pattern_ctx_t;

odor_pattern_config_t odor_pattern_default_config(void);
odor_pattern_ctx_t odor_pattern_create(const odor_pattern_config_t* config);
void odor_pattern_destroy(odor_pattern_ctx_t ctx);

int odor_pattern_complete(odor_pattern_ctx_t ctx, const float* partial, uint32_t dim, float* completed, float* completion_confidence);
int odor_pattern_separate(odor_pattern_ctx_t ctx, const float* pattern, uint32_t dim, float* separated, float* separation_score);
int odor_pattern_store(odor_pattern_ctx_t ctx, const float* pattern, uint32_t dim, uint32_t* pattern_id_out);
bool odor_pattern_is_novel(odor_pattern_ctx_t ctx, const float* pattern, uint32_t dim, float* novelty_score);
float odor_pattern_get_overlap(odor_pattern_ctx_t ctx, const float* a, const float* b, uint32_t dim);
int odor_pattern_update_attractor(odor_pattern_ctx_t ctx, const float* pattern, uint32_t dim, float learning_rate);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ODOR_PATTERN_COMPLETION_H */
