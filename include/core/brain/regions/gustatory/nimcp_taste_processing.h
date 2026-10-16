/**
 * @file nimcp_taste_processing.h
 * @brief Taste Processing Module
 * @version Phase 6: Sensory Processing
 * @date 2026-01-12
 */

#ifndef NIMCP_TASTE_PROCESSING_H
#define NIMCP_TASTE_PROCESSING_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "core/brain/regions/gustatory/nimcp_gustatory.h"

#define TASTE_PROC_RECEPTOR_TYPES   25      /* Type I, II, III cells */
#define TASTE_PROC_INTEGRATION_TAU  100.0f  /* ms */

typedef struct {
    float detection_threshold[TASTE_COUNT];
    float discrimination_threshold;
    float adaptation_rate;
    bool enable_cross_modal;
    float temperature_modulation;
} taste_proc_config_t;

typedef struct {
    basic_taste_t dominant_taste;
    float taste_intensities[TASTE_COUNT];
    float quality_code;
    float temporal_pattern;
    float receptor_population_response;
} taste_quality_t;

typedef struct taste_proc_ctx* taste_proc_ctx_t;

taste_proc_config_t taste_proc_default_config(void);
taste_proc_ctx_t taste_proc_create(const taste_proc_config_t* config);
void taste_proc_destroy(taste_proc_ctx_t ctx);

int taste_proc_detect(taste_proc_ctx_t ctx, basic_taste_t taste, float concentration, bool* detected, float* intensity);
int taste_proc_discriminate(taste_proc_ctx_t ctx, basic_taste_t taste_a, float conc_a, basic_taste_t taste_b, float conc_b, bool* can_discriminate, float* confidence);
int taste_proc_compute_quality(taste_proc_ctx_t ctx, const float* receptor_response, uint32_t num_receptors, taste_quality_t* quality);
int taste_proc_apply_temperature(taste_proc_ctx_t ctx, float temperature, float* taste_modulation);
int taste_proc_mixture_suppression(taste_proc_ctx_t ctx, const float* taste_concentrations, float* suppressed_intensities);
float taste_proc_bitter_detection(taste_proc_ctx_t ctx, float concentration);
float taste_proc_umami_enhancement(taste_proc_ctx_t ctx, float glutamate, float nucleotide);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TASTE_PROCESSING_H */
