/**
 * @file nimcp_olfactory_memory.h
 * @brief Olfactory Memory (Proustian Memory) Module
 * @version Phase 6: Sensory Processing
 * @date 2026-01-12
 *
 * Olfactory memory is unique in its ability to trigger vivid emotional
 * memories (Proust phenomenon). This module implements odor-memory
 * associations through direct connections to amygdala and hippocampus.
 */

#ifndef NIMCP_OLFACTORY_MEMORY_H
#define NIMCP_OLFACTORY_MEMORY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "core/brain/regions/olfactory/nimcp_olfactory.h"

#define OLFACT_MEM_MAX_ASSOCIATIONS  16
#define OLFACT_MEM_DECAY_TAU         86400.0f /* 24 hours */
#define OLFACT_MEM_CONSOLIDATION_TAU 3600.0f  /* 1 hour */

typedef struct {
    uint32_t max_memories;
    float encoding_threshold;
    float recall_threshold;
    float emotional_weight;
    bool enable_consolidation;
    float consolidation_rate;
} olfact_mem_config_t;

typedef struct {
    uint32_t episode_id;
    char description[256];
    float emotional_valence;
    float emotional_arousal;
    float association_strength;
    uint64_t encoding_time;
} odor_association_t;

typedef struct {
    uint32_t memory_id;
    float* odor_pattern;
    uint32_t pattern_dim;
    char odor_name[64];
    odor_category_t category;
    hedonic_valence_t valence;
    float memory_strength;
    float consolidation_level;
    odor_association_t associations[OLFACT_MEM_MAX_ASSOCIATIONS];
    uint32_t num_associations;
    uint64_t creation_time;
    uint64_t last_access;
    uint32_t access_count;
} olfact_mem_entry_t;

typedef struct olfact_mem_ctx* olfact_mem_ctx_t;

olfact_mem_config_t olfact_mem_default_config(void);
olfact_mem_ctx_t olfact_mem_create(const olfact_mem_config_t* config);
void olfact_mem_destroy(olfact_mem_ctx_t ctx);

int olfact_mem_encode(olfact_mem_ctx_t ctx, const float* odor_pattern, uint32_t dim, const char* name, odor_category_t category, hedonic_valence_t valence, uint32_t* memory_id_out);
int olfact_mem_add_association(olfact_mem_ctx_t ctx, uint32_t memory_id, uint32_t episode_id, const char* description, float emotional_valence, float emotional_arousal);
int olfact_mem_recall(olfact_mem_ctx_t ctx, const float* cue_pattern, uint32_t dim, olfact_mem_entry_t* result, float* match_strength);
int olfact_mem_trigger_associations(olfact_mem_ctx_t ctx, uint32_t memory_id, odor_association_t* associations, uint32_t max_assoc, uint32_t* num_assoc);
int olfact_mem_consolidate(olfact_mem_ctx_t ctx, float dt);
int olfact_mem_decay(olfact_mem_ctx_t ctx, float dt);
int olfact_mem_strengthen(olfact_mem_ctx_t ctx, uint32_t memory_id, float amount);
uint32_t olfact_mem_get_count(olfact_mem_ctx_t ctx);
float olfact_mem_get_familiarity(olfact_mem_ctx_t ctx, const float* odor_pattern, uint32_t dim);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OLFACTORY_MEMORY_H */
