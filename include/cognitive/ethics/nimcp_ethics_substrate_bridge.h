/**
 * @file nimcp_ethics_substrate_bridge.h
 * @brief Bridge between Ethics system and neural substrate
 *
 * WHAT: Links ethical reasoning to metabolic/energy state
 * WHY: Moral reasoning requires high cognitive load and prefrontal resources
 * HOW: Monitors ATP/fatigue; modulates moral clarity, deliberation depth, bias resistance
 *
 * BIOLOGICAL BASIS:
 * - Moral reasoning involves ventromedial PFC and anterior cingulate
 * - ATP depletion leads to more utilitarian/less deontological decisions
 * - Fatigue increases susceptibility to moral biases
 * - Metabolic stress reduces moral deliberation depth
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_ETHICS_SUBSTRATE_BRIDGE_H
#define NIMCP_ETHICS_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_SUBSTRATE_ETHICS 0x1212

typedef struct {
    float moral_clarity;          /* Clarity of moral judgments [0-1] */
    float deliberation_depth;     /* Depth of ethical analysis [0-1] */
    float bias_resistance;        /* Resistance to moral biases [0-1] */
    float empathy_capacity;       /* Empathic processing capacity [0-1] */
    float overall_capacity;       /* Combined modulation [0-1] */
} ethics_substrate_effects_t;

typedef struct {
    bool enable_atp_modulation;
    bool enable_fatigue_modulation;
    bool enable_bio_async;
    float atp_sensitivity;
    float fatigue_sensitivity;
    float min_capacity;
} ethics_substrate_config_t;

typedef struct ethics_substrate_bridge ethics_substrate_bridge_t;

ethics_substrate_config_t ethics_substrate_default_config(void);
ethics_substrate_bridge_t* ethics_substrate_bridge_create(void* ethics, neural_substrate_t* substrate, const ethics_substrate_config_t* config);
void ethics_substrate_bridge_destroy(ethics_substrate_bridge_t* bridge);
int ethics_substrate_bridge_update(ethics_substrate_bridge_t* bridge);
int ethics_substrate_bridge_get_effects(const ethics_substrate_bridge_t* bridge, ethics_substrate_effects_t* effects);
int ethics_substrate_bridge_apply_effects(ethics_substrate_bridge_t* bridge);
int ethics_substrate_bridge_register_bio_async(ethics_substrate_bridge_t* bridge, bio_router_t* router);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ETHICS_SUBSTRATE_BRIDGE_H */
