/**
 * @file nimcp_odor_identification.h
 * @brief Odor Identification and Classification Module
 * @version Phase 6: Sensory Processing
 * @date 2026-01-12
 */

#ifndef NIMCP_ODOR_IDENTIFICATION_H
#define NIMCP_ODOR_IDENTIFICATION_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "core/brain/regions/olfactory/nimcp_olfactory.h"

#define ODOR_ID_MAX_CANDIDATES  10
#define ODOR_ID_FEATURE_DIM     64
#define ODOR_ID_THRESHOLD       0.6f

typedef struct {
    float learning_rate;
    float similarity_threshold;
    bool enable_online_learning;
    uint32_t max_templates;
} odor_id_config_t;

typedef struct {
    uint32_t odor_id;
    float similarity;
    float confidence;
    odor_category_t category;
    hedonic_valence_t valence;
    char name[64];
} odor_id_candidate_t;

typedef struct {
    float* feature_vector;
    uint32_t feature_dim;
    float concentration;
    float distinctiveness;
    float complexity;
    uint32_t num_active_components;
} odor_features_t;

typedef struct odor_id_ctx* odor_id_ctx_t;

odor_id_config_t odor_id_default_config(void);
odor_id_ctx_t odor_id_create(const odor_id_config_t* config);
void odor_id_destroy(odor_id_ctx_t ctx);

int odor_id_extract_features(odor_id_ctx_t ctx, const float* receptor_pattern, uint32_t pattern_dim, odor_features_t* features);
int odor_id_identify(odor_id_ctx_t ctx, const odor_features_t* features, odor_id_candidate_t* candidates, uint32_t max_candidates, uint32_t* num_candidates);
int odor_id_classify_category(odor_id_ctx_t ctx, const odor_features_t* features, odor_category_t* category, float* confidence);
int odor_id_estimate_valence(odor_id_ctx_t ctx, const odor_features_t* features, hedonic_valence_t* valence, float* confidence);
int odor_id_learn_odor(odor_id_ctx_t ctx, const odor_features_t* features, const char* name, odor_category_t category, hedonic_valence_t valence, uint32_t* odor_id_out);
float odor_id_compute_similarity(odor_id_ctx_t ctx, const odor_features_t* a, const odor_features_t* b);
int odor_id_get_template_count(odor_id_ctx_t ctx);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ODOR_IDENTIFICATION_H */
