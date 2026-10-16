#ifndef NIMCP_SELF_CURRICULUM_H
#define NIMCP_SELF_CURRICULUM_H

/**
 * @file nimcp_self_curriculum.h
 * @brief Self-Generated Curriculum — brain creates its own training items
 *
 * The brain monitors per-domain uncertainty (EMA of loss) and, when
 * uncertainty exceeds a threshold, uses imagination (probe → inference →
 * noisy target) to generate new training items for weak domains.
 *
 * Flow: curiosity → imagination → training.
 */

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration — avoids pulling in the full brain header */
#ifndef NIMCP_BRAIN_T_DEFINED
#define NIMCP_BRAIN_T_DEFINED
typedef struct brain_struct* brain_t;
#endif

/* ============================================================================
 * Configuration
 * ============================================================================ */

typedef struct {
    uint32_t generation_interval;      /* Generate every N steps (default: 50)     */
    uint32_t max_generated;            /* Max items per generation (default: 10)    */
    float    uncertainty_threshold;    /* Generate only for domains above (0.5)     */
    uint32_t imagination_steps;        /* Noise blending iterations (default: 3)    */
} nimcp_self_curriculum_config_t;

/* ============================================================================
 * Generated item
 * ============================================================================ */

#define NIMCP_SC_FEATURE_DIM  1024
#define NIMCP_SC_TARGET_DIM   4096
#define NIMCP_SC_LABEL_LEN      64
#define NIMCP_SC_MAX_DOMAINS    32

typedef struct {
    float    features[NIMCP_SC_FEATURE_DIM];
    float    target[NIMCP_SC_TARGET_DIM];
    char     label[NIMCP_SC_LABEL_LEN];
} nimcp_sc_item_t;

/* Opaque handle */
typedef struct nimcp_self_curriculum nimcp_self_curriculum_t;

/* ============================================================================
 * API
 * ============================================================================ */

nimcp_self_curriculum_config_t nimcp_self_curriculum_config_default(void);

nimcp_self_curriculum_t* nimcp_self_curriculum_create(
    const nimcp_self_curriculum_config_t* config);

void nimcp_self_curriculum_destroy(nimcp_self_curriculum_t* handle);

/**
 * @brief Update domain uncertainty EMA from a training loss.
 *
 * Domain is extracted from label prefix (everything before the first '_').
 * E.g. "ethics_trolley" → domain "ethics".
 */
int nimcp_self_curriculum_update_uncertainty(
    nimcp_self_curriculum_t* handle,
    const char* label,
    float loss);

/**
 * @brief Check whether curriculum generation should run this step.
 * @return true if step % interval == 0 AND any domain exceeds threshold.
 */
bool nimcp_self_curriculum_should_generate(
    const nimcp_self_curriculum_t* handle,
    uint64_t step);

/**
 * @brief Generate self-curriculum items for the most uncertain domains.
 *
 * 1. Pick top-3 most uncertain domains.
 * 2. For each: build a domain-specific probe, run brain inference,
 *    blend output with noise to create a training target.
 * 3. Store as "self_curriculum_{domain}" items.
 *
 * @return Number of items generated, or -1 on error.
 */
int nimcp_self_curriculum_generate(
    nimcp_self_curriculum_t* handle,
    brain_t brain);

/**
 * @brief Pop the next generated item for training.
 * @return 0 if an item was returned, -1 if the queue is empty.
 */
int nimcp_self_curriculum_get_next_item(
    nimcp_self_curriculum_t* handle,
    float* features_out, uint32_t feat_dim,
    float* target_out,   uint32_t tgt_dim,
    char*  label_out);

/**
 * @brief Return the most uncertain domain and its uncertainty score.
 * @return 0 on success, -1 if no domains are tracked.
 */
int nimcp_self_curriculum_get_most_uncertain_domain(
    const nimcp_self_curriculum_t* handle,
    char* domain_name_out,
    float* uncertainty_out);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SELF_CURRICULUM_H */
