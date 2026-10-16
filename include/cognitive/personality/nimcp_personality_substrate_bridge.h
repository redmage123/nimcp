/**
 * @file nimcp_personality_substrate_bridge.h
 * @brief Bridge between Personality system and neural substrate
 *
 * WHAT: Links personality traits to metabolic state
 * WHY: Personality expression varies with energy and fatigue levels
 * HOW: Monitors ATP/fatigue; modulates trait expression, self-regulation, consistency
 *
 * BIOLOGICAL BASIS:
 * - Personality involves prefrontal self-regulation circuits
 * - ATP depletion reduces self-control (ego depletion)
 * - Fatigue increases impulsivity, reduces agreeableness
 * - Stress reveals "true" personality traits
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_PERSONALITY_SUBSTRATE_BRIDGE_H
#define NIMCP_PERSONALITY_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_SUBSTRATE_PERSONALITY 0x1216

typedef struct {
    float self_regulation;        /* Self-control capacity [0-1] */
    float trait_consistency;      /* Consistency of expression [0-1] */
    float stress_resilience;      /* Resistance to personality shifts [0-1] */
    float social_energy;          /* Energy for social interaction [0-1] */
    float overall_capacity;       /* Combined modulation [0-1] */
} personality_substrate_effects_t;

typedef struct {
    bool enable_atp_modulation;
    bool enable_fatigue_modulation;
    bool enable_bio_async;
    float atp_sensitivity;
    float fatigue_sensitivity;
    float min_capacity;
} personality_substrate_config_t;

typedef struct personality_substrate_bridge personality_substrate_bridge_t;

personality_substrate_config_t personality_substrate_default_config(void);
personality_substrate_bridge_t* personality_substrate_bridge_create(void* personality, neural_substrate_t* substrate, const personality_substrate_config_t* config);
void personality_substrate_bridge_destroy(personality_substrate_bridge_t* bridge);
int personality_substrate_bridge_update(personality_substrate_bridge_t* bridge);
int personality_substrate_bridge_get_effects(const personality_substrate_bridge_t* bridge, personality_substrate_effects_t* effects);
int personality_substrate_bridge_apply_effects(personality_substrate_bridge_t* bridge);
int personality_substrate_bridge_register_bio_async(personality_substrate_bridge_t* bridge, bio_router_t* router);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PERSONALITY_SUBSTRATE_BRIDGE_H */
