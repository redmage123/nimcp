/**
 * @file nimcp_social_substrate_bridge.h
 * @brief Bridge between Social Cognition system and neural substrate
 *
 * WHAT: Links social cognition (love, loyalty, friendship) to metabolic/energy state
 * WHY: Social bonding and relationship maintenance require resources
 * HOW: Monitors ATP/fatigue; modulates bonding strength, loyalty, trust
 *
 * BIOLOGICAL BASIS:
 * - Social cognition involves prefrontal and limbic circuits
 * - ATP depletion leads to reduced social engagement and bonding
 * - Fatigue impairs social judgment and relationship maintenance
 * - Metabolic stress reduces prosocial behavior and trust
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_SOCIAL_SUBSTRATE_BRIDGE_H
#define NIMCP_SOCIAL_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_SUBSTRATE_SOCIAL 0x1309

typedef struct {
    float bonding_capacity;        /* Capacity for social bonding [0-1] */
    float loyalty_strength;        /* Loyalty maintenance strength [0-1] */
    float trust_evaluation;        /* Trust evaluation capacity [0-1] */
    float prosocial_motivation;    /* Prosocial behavior motivation [0-1] */
    float overall_capacity;        /* Combined modulation [0-1] */
} social_substrate_effects_t;

typedef struct {
    bool enable_atp_modulation;
    bool enable_fatigue_modulation;
    bool enable_bio_async;
    float atp_sensitivity;
    float fatigue_sensitivity;
    float min_capacity;
} social_substrate_config_t;

typedef struct social_substrate_bridge social_substrate_bridge_t;

social_substrate_config_t social_substrate_default_config(void);
social_substrate_bridge_t* social_substrate_bridge_create(void* social, neural_substrate_t* substrate, const social_substrate_config_t* config);
void social_substrate_bridge_destroy(social_substrate_bridge_t* bridge);
int social_substrate_bridge_update(social_substrate_bridge_t* bridge);
int social_substrate_bridge_get_effects(const social_substrate_bridge_t* bridge, social_substrate_effects_t* effects);
int social_substrate_bridge_apply_effects(social_substrate_bridge_t* bridge);
int social_substrate_bridge_register_bio_async(social_substrate_bridge_t* bridge, bio_router_t* router);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SOCIAL_SUBSTRATE_BRIDGE_H */
