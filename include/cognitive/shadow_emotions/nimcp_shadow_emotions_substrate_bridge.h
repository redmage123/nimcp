/**
 * @file nimcp_shadow_emotions_substrate_bridge.h
 * @brief Bridge between Shadow Emotions system and neural substrate
 *
 * WHAT: Links repressed emotional processing to metabolic state
 * WHY: Shadow emotions require limbic and prefrontal regulation resources
 * HOW: Monitors ATP/fatigue; modulates suppression, emergence, regulation
 *
 * BIOLOGICAL BASIS:
 * - Shadow emotions involve amygdala and ventromedial PFC
 * - ATP depletion weakens emotional suppression
 * - Fatigue allows repressed emotions to surface
 * - Metabolic stress reduces emotional regulation capacity
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_SHADOW_EMOTIONS_SUBSTRATE_BRIDGE_H
#define NIMCP_SHADOW_EMOTIONS_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_SUBSTRATE_SHADOW_EMOTIONS 0x1227

typedef struct {
    float suppression_strength;   /* Strength of emotional suppression [0-1] */
    float emergence_threshold;    /* Threshold for emotion emergence [0-1] */
    float regulation_capacity;    /* Capacity for regulation [0-1] */
    float integration_ability;    /* Ability to integrate shadow emotions [0-1] */
    float overall_capacity;       /* Combined modulation [0-1] */
} shadow_emotions_substrate_effects_t;

typedef struct {
    bool enable_atp_modulation;
    bool enable_fatigue_modulation;
    bool enable_bio_async;
    float atp_sensitivity;
    float fatigue_sensitivity;
    float min_capacity;
} shadow_emotions_substrate_config_t;

typedef struct shadow_emotions_substrate_bridge shadow_emotions_substrate_bridge_t;

shadow_emotions_substrate_config_t shadow_emotions_substrate_default_config(void);
shadow_emotions_substrate_bridge_t* shadow_emotions_substrate_bridge_create(void* shadow_emotions, neural_substrate_t* substrate, const shadow_emotions_substrate_config_t* config);
void shadow_emotions_substrate_bridge_destroy(shadow_emotions_substrate_bridge_t* bridge);
int shadow_emotions_substrate_bridge_update(shadow_emotions_substrate_bridge_t* bridge);
int shadow_emotions_substrate_bridge_get_effects(const shadow_emotions_substrate_bridge_t* bridge, shadow_emotions_substrate_effects_t* effects);
int shadow_emotions_substrate_bridge_apply_effects(shadow_emotions_substrate_bridge_t* bridge);
int shadow_emotions_substrate_bridge_register_bio_async(shadow_emotions_substrate_bridge_t* bridge, bio_router_t* router);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SHADOW_EMOTIONS_SUBSTRATE_BRIDGE_H */
