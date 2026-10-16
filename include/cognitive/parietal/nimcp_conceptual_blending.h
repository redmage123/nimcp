/**
 * @file nimcp_conceptual_blending.h
 * @brief Conceptual blending engine for creative combination
 *
 * WHAT: Engine for blending concepts to create novel ideas
 * WHY:  Enable creativity through conceptual combination
 * HOW:  Mental space mapping, selective projection, emergence
 *
 * @author NIMCP Development Team
 * @date 2025-12-29
 * @version 1.0.0
 */

#ifndef NIMCP_CONCEPTUAL_BLENDING_H
#define NIMCP_CONCEPTUAL_BLENDING_H

#include "utils/validation/nimcp_common.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BLEND_MAX_NAME          128
#define BLEND_MAX_ELEMENTS      64
#define BLEND_MAX_PROPERTIES    32
#define BLEND_MAX_MAPPINGS      64
#define BIO_MODULE_BLENDING     0x03A4

typedef struct blending_engine blending_engine_t;

typedef struct {
    uint32_t id;
    char name[BLEND_MAX_NAME];
    float* features;
    uint32_t num_features;
} blend_element_t;

typedef struct {
    uint32_t id;
    char name[BLEND_MAX_NAME];
    blend_element_t* elements;
    uint32_t num_elements;
    float* frame_structure;
    uint32_t frame_dim;
} blend_mental_space_t;

typedef struct {
    uint32_t source_id;
    uint32_t target_id;
    float strength;
} blend_mapping_t;

typedef struct {
    char name[BLEND_MAX_NAME];
    char description[256];
    float strength;
    bool is_emergent;
} blend_property_t;

typedef struct {
    uint32_t id;
    blend_mental_space_t* input1;
    blend_mental_space_t* input2;
    blend_mental_space_t* generic;
    blend_mental_space_t* blend;
    blend_mapping_t* mappings;
    uint32_t num_mappings;
    blend_property_t* emergent_properties;
    uint32_t num_emergent;
    float integration_score;
    float novelty_score;
} conceptual_blend_t;

typedef struct {
    float min_integration;
    float novelty_weight;
    bool enable_emergence_detection;
    bool enable_optimization;
    uint32_t max_blend_iterations;
    float inflammation_sensitivity;
    float fatigue_sensitivity;
} blend_config_t;

typedef struct {
    uint64_t blends_created;
    uint64_t emergent_properties_found;
    uint64_t mappings_made;
    float avg_novelty;
    float avg_integration;
} blend_stats_t;

/* Lifecycle */
blending_engine_t* blending_engine_create(void);
blending_engine_t* blending_engine_create_custom(const blend_config_t* config);
void blending_engine_destroy(blending_engine_t* engine);
blend_config_t blending_engine_default_config(void);

/* Mental Space Management */
blend_mental_space_t* blending_create_space(const char* name);
int blending_add_element(blend_mental_space_t* space, const char* name,
    const float* features, uint32_t num_features);
void blending_free_space(blend_mental_space_t* space);

/* Blending Operations */
conceptual_blend_t* blending_create_blend(blending_engine_t* engine,
    const blend_mental_space_t* concept1, const blend_mental_space_t* concept2);
int blending_find_mappings(blending_engine_t* engine,
    const blend_mental_space_t* space1, const blend_mental_space_t* space2,
    blend_mapping_t* mappings, uint32_t max_mappings, uint32_t* num_found);
blend_property_t** blending_find_emergent(blending_engine_t* engine,
    const conceptual_blend_t* blend, uint32_t* num_found);

/* Blend Evaluation */
float blending_evaluate_novelty(blending_engine_t* engine, const conceptual_blend_t* blend);
float blending_evaluate_integration(blending_engine_t* engine, const conceptual_blend_t* blend);
int blending_optimize_blend(blending_engine_t* engine, conceptual_blend_t* blend);

/* Cleanup */
void blending_free_blend(conceptual_blend_t* blend);

/* Modulation */
int blending_set_inflammation(blending_engine_t* engine, float level);
int blending_set_fatigue(blending_engine_t* engine, float level);

/* Statistics */
int blending_get_stats(const blending_engine_t* engine, blend_stats_t* stats);
void blending_reset_stats(blending_engine_t* engine);
const char* blending_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CONCEPTUAL_BLENDING_H */
