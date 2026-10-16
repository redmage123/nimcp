/**
 * @file nimcp_disgust_response.h
 * @brief Disgust Response Module
 * @version Phase 6: Sensory Processing
 * @date 2026-01-12
 *
 * Implements disgust response processing including detection of
 * potentially harmful foods, conditioned taste aversion, and
 * integration with the amygdala for emotional response.
 */

#ifndef NIMCP_DISGUST_RESPONSE_H
#define NIMCP_DISGUST_RESPONSE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "core/brain/regions/gustatory/nimcp_gustatory.h"

#define DISGUST_MAX_AVERSIONS   50
#define DISGUST_BITTER_WEIGHT   2.0f    /* Bitter strongly triggers disgust */

typedef struct {
    float disgust_threshold;
    float bitter_sensitivity;
    float spoilage_detection_weight;
    bool enable_conditioned_aversion;
    float aversion_learning_rate;
    float aversion_extinction_rate;
} disgust_config_t;

typedef struct {
    uint32_t aversion_id;
    float* taste_pattern;
    uint32_t pattern_dim;
    float aversion_strength;
    bool is_innate;             /* Innate (bitter) vs learned */
    uint64_t acquisition_time;
    uint32_t extinction_trials;
} conditioned_aversion_t;

typedef struct {
    disgust_level_t level;
    float intensity;
    bool gag_reflex_triggered;
    float rejection_probability;
    char trigger_description[64];
    conditioned_aversion_t* matching_aversion;
} disgust_response_t;

typedef struct disgust_ctx* disgust_ctx_t;

disgust_config_t disgust_default_config(void);
disgust_ctx_t disgust_create(const disgust_config_t* config);
void disgust_destroy(disgust_ctx_t ctx);

int disgust_evaluate(disgust_ctx_t ctx, const taste_perception_t* perception, disgust_response_t* response);
bool disgust_detect_toxin(disgust_ctx_t ctx, const taste_perception_t* perception, float* toxin_probability);
bool disgust_detect_spoilage(disgust_ctx_t ctx, const taste_perception_t* perception, float* spoilage_probability);
int disgust_learn_aversion(disgust_ctx_t ctx, const float* taste_pattern, uint32_t dim, float aversion_strength, uint32_t* aversion_id);
int disgust_extinguish_aversion(disgust_ctx_t ctx, uint32_t aversion_id, float extinction_amount);
int disgust_check_aversion(disgust_ctx_t ctx, const float* taste_pattern, uint32_t dim, conditioned_aversion_t* aversion, bool* matches);
int disgust_trigger_response(disgust_ctx_t ctx, disgust_level_t level, float* autonomic_response, float* facial_expression);
float disgust_get_bitter_response(disgust_ctx_t ctx, float bitter_intensity);
uint32_t disgust_get_aversion_count(disgust_ctx_t ctx);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DISGUST_RESPONSE_H */
