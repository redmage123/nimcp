/**
 * @file nimcp_shadow_substrate_bridge.h
 * @brief Bridge between Shadow (unconscious) system and neural substrate
 *
 * WHAT: Links shadow/unconscious processing to metabolic state
 * WHY: Unconscious processing requires subcortical and default mode resources
 * HOW: Monitors ATP/fatigue; modulates repression, projection, integration
 *
 * BIOLOGICAL BASIS:
 * - Shadow processing involves subcortical and limbic circuits
 * - ATP depletion weakens repression mechanisms
 * - Fatigue increases projection and shadow emergence
 * - Metabolic stress reveals suppressed content
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_SHADOW_SUBSTRATE_BRIDGE_H
#define NIMCP_SHADOW_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_SUBSTRATE_SHADOW 0x1226

typedef struct {
    float repression_strength;    /* Strength of repression [0-1] */
    float integration_capacity;   /* Capacity for shadow integration [0-1] */
    float awareness_threshold;    /* Threshold for conscious awareness [0-1] */
    float projection_tendency;    /* Tendency to project [0-1] */
    float overall_capacity;       /* Combined modulation [0-1] */
} shadow_substrate_effects_t;

typedef struct {
    bool enable_atp_modulation;
    bool enable_fatigue_modulation;
    bool enable_bio_async;
    float atp_sensitivity;
    float fatigue_sensitivity;
    float min_capacity;
} shadow_substrate_config_t;

typedef struct shadow_substrate_bridge shadow_substrate_bridge_t;

shadow_substrate_config_t shadow_substrate_default_config(void);
shadow_substrate_bridge_t* shadow_substrate_bridge_create(void* shadow, neural_substrate_t* substrate, const shadow_substrate_config_t* config);
void shadow_substrate_bridge_destroy(shadow_substrate_bridge_t* bridge);
int shadow_substrate_bridge_update(shadow_substrate_bridge_t* bridge);
int shadow_substrate_bridge_get_effects(const shadow_substrate_bridge_t* bridge, shadow_substrate_effects_t* effects);
int shadow_substrate_bridge_apply_effects(shadow_substrate_bridge_t* bridge);
int shadow_substrate_bridge_register_bio_async(shadow_substrate_bridge_t* bridge, bio_router_t* router);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SHADOW_SUBSTRATE_BRIDGE_H */
