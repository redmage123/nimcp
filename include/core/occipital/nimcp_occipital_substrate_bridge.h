/**
 * @file nimcp_occipital_substrate_bridge.h
 * @brief Bridge between Occipital Cortex and neural substrate
 *
 * WHAT: Links occipital visual cortex to metabolic state
 * WHY: Visual cortex has high metabolic demands for processing
 * HOW: Monitors ATP/fatigue; modulates visual processing at all levels
 *
 * BIOLOGICAL BASIS:
 * - Occipital cortex is primary visual processing region
 * - ATP depletion affects early visual processing
 * - Fatigue impairs visual perception quality
 * - Metabolic state affects V1-V4 processing cascade
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_OCCIPITAL_SUBSTRATE_BRIDGE_H
#define NIMCP_OCCIPITAL_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_SUBSTRATE_OCCIPITAL 0x123C

typedef struct {
    float primary_visual;         /* V1 processing quality [0-1] */
    float color_processing;       /* Color perception [0-1] */
    float form_processing;        /* Shape/form perception [0-1] */
    float motion_perception;      /* Motion processing [0-1] */
    float overall_capacity;       /* Combined modulation [0-1] */
} occipital_substrate_effects_t;

typedef struct {
    bool enable_atp_modulation;
    bool enable_fatigue_modulation;
    bool enable_bio_async;
    float atp_sensitivity;
    float fatigue_sensitivity;
    float min_capacity;
} occipital_substrate_config_t;

typedef struct occipital_substrate_bridge occipital_substrate_bridge_t;

occipital_substrate_config_t occipital_substrate_default_config(void);
occipital_substrate_bridge_t* occipital_substrate_bridge_create(void* occipital, neural_substrate_t* substrate, const occipital_substrate_config_t* config);
void occipital_substrate_bridge_destroy(occipital_substrate_bridge_t* bridge);
int occipital_substrate_bridge_update(occipital_substrate_bridge_t* bridge);
int occipital_substrate_bridge_get_effects(const occipital_substrate_bridge_t* bridge, occipital_substrate_effects_t* effects);
int occipital_substrate_bridge_apply_effects(occipital_substrate_bridge_t* bridge);
int occipital_substrate_bridge_register_bio_async(occipital_substrate_bridge_t* bridge, bio_router_t* router);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OCCIPITAL_SUBSTRATE_BRIDGE_H */
