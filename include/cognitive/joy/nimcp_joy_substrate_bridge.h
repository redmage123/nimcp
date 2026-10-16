/**
 * @file nimcp_joy_substrate_bridge.h
 * @brief Bridge between Joy system and neural substrate
 *
 * WHAT: Links joy/positive affect to metabolic state
 * WHY: Joy involves dopaminergic reward circuits requiring energy
 * HOW: Monitors ATP/fatigue; modulates joy intensity, hedonic tone, savoring
 *
 * BIOLOGICAL BASIS:
 * - Joy involves ventral striatum and orbitofrontal cortex
 * - ATP availability affects dopamine release capacity
 * - Fatigue reduces capacity for positive affect
 * - Metabolic wellness enables hedonic experience
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_JOY_SUBSTRATE_BRIDGE_H
#define NIMCP_JOY_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_SUBSTRATE_JOY 0x1222

typedef struct {
    float hedonic_capacity;       /* Capacity for pleasure [0-1] */
    float joy_intensity;          /* Maximum joy intensity [0-1] */
    float savoring_ability;       /* Ability to sustain joy [0-1] */
    float positive_anticipation;  /* Anticipatory pleasure [0-1] */
    float overall_capacity;       /* Combined modulation [0-1] */
} joy_substrate_effects_t;

typedef struct {
    bool enable_atp_modulation;
    bool enable_fatigue_modulation;
    bool enable_bio_async;
    float atp_sensitivity;
    float fatigue_sensitivity;
    float min_capacity;
} joy_substrate_config_t;

typedef struct joy_substrate_bridge joy_substrate_bridge_t;

joy_substrate_config_t joy_substrate_default_config(void);
joy_substrate_bridge_t* joy_substrate_bridge_create(void* joy, neural_substrate_t* substrate, const joy_substrate_config_t* config);
void joy_substrate_bridge_destroy(joy_substrate_bridge_t* bridge);
int joy_substrate_bridge_update(joy_substrate_bridge_t* bridge);
int joy_substrate_bridge_get_effects(const joy_substrate_bridge_t* bridge, joy_substrate_effects_t* effects);
int joy_substrate_bridge_apply_effects(joy_substrate_bridge_t* bridge);
int joy_substrate_bridge_register_bio_async(joy_substrate_bridge_t* bridge, bio_router_t* router);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_JOY_SUBSTRATE_BRIDGE_H */
