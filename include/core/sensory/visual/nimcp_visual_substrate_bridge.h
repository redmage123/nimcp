/**
 * @file nimcp_visual_substrate_bridge.h
 * @brief Bridge between Visual processing and neural substrate
 *
 * WHAT: Links visual processing to metabolic state
 * WHY: Visual cortex is highly metabolically demanding
 * HOW: Monitors ATP/fatigue; modulates acuity, contrast, motion processing
 *
 * BIOLOGICAL BASIS:
 * - Visual cortex consumes significant glucose/ATP
 * - ATP depletion reduces visual acuity and processing speed
 * - Fatigue impairs visual attention and feature binding
 * - Metabolic stress affects low-level to high-level vision
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_VISUAL_SUBSTRATE_BRIDGE_H
#define NIMCP_VISUAL_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_SUBSTRATE_VISUAL 0x1231

typedef struct {
    float visual_acuity;          /* Visual acuity/resolution [0-1] */
    float contrast_sensitivity;   /* Contrast sensitivity [0-1] */
    float motion_processing;      /* Motion detection quality [0-1] */
    float feature_binding;        /* Feature integration quality [0-1] */
    float overall_capacity;       /* Combined modulation [0-1] */
} visual_substrate_effects_t;

typedef struct {
    bool enable_atp_modulation;
    bool enable_fatigue_modulation;
    bool enable_bio_async;
    float atp_sensitivity;
    float fatigue_sensitivity;
    float min_capacity;
} visual_substrate_config_t;

typedef struct visual_substrate_bridge visual_substrate_bridge_t;

visual_substrate_config_t visual_substrate_default_config(void);
visual_substrate_bridge_t* visual_substrate_bridge_create(void* visual, neural_substrate_t* substrate, const visual_substrate_config_t* config);
void visual_substrate_bridge_destroy(visual_substrate_bridge_t* bridge);
int visual_substrate_bridge_update(visual_substrate_bridge_t* bridge);
int visual_substrate_bridge_get_effects(const visual_substrate_bridge_t* bridge, visual_substrate_effects_t* effects);
int visual_substrate_bridge_apply_effects(visual_substrate_bridge_t* bridge);
int visual_substrate_bridge_register_bio_async(visual_substrate_bridge_t* bridge, bio_router_t* router);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_VISUAL_SUBSTRATE_BRIDGE_H */
