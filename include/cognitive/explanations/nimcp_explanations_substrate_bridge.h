/**
 * @file nimcp_explanations_substrate_bridge.h
 * @brief Bridge between Explanations system and neural substrate
 *
 * WHAT: Links explanation generation to metabolic state
 * WHY: Causal reasoning requires extensive cortical integration
 * HOW: Monitors ATP/fatigue; modulates explanation depth, coherence, abstraction
 *
 * BIOLOGICAL BASIS:
 * - Explanation generation involves temporal-parietal-frontal integration
 * - ATP depletion reduces explanation complexity
 * - Fatigue leads to simpler causal models
 * - Metabolic stress impairs multi-step reasoning
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_EXPLANATIONS_SUBSTRATE_BRIDGE_H
#define NIMCP_EXPLANATIONS_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_SUBSTRATE_EXPLANATIONS 0x121C

typedef struct {
    float explanation_depth;      /* Depth of causal chains [0-1] */
    float coherence_quality;      /* Internal consistency [0-1] */
    float abstraction_level;      /* Level of abstraction [0-1] */
    float integration_breadth;    /* Breadth of evidence integration [0-1] */
    float overall_capacity;       /* Combined modulation [0-1] */
} explanations_substrate_effects_t;

typedef struct {
    bool enable_atp_modulation;
    bool enable_fatigue_modulation;
    bool enable_bio_async;
    float atp_sensitivity;
    float fatigue_sensitivity;
    float min_capacity;
} explanations_substrate_config_t;

typedef struct explanations_substrate_bridge explanations_substrate_bridge_t;

explanations_substrate_config_t explanations_substrate_default_config(void);
explanations_substrate_bridge_t* explanations_substrate_bridge_create(void* explanations, neural_substrate_t* substrate, const explanations_substrate_config_t* config);
void explanations_substrate_bridge_destroy(explanations_substrate_bridge_t* bridge);
int explanations_substrate_bridge_update(explanations_substrate_bridge_t* bridge);
int explanations_substrate_bridge_get_effects(const explanations_substrate_bridge_t* bridge, explanations_substrate_effects_t* effects);
int explanations_substrate_bridge_apply_effects(explanations_substrate_bridge_t* bridge);
int explanations_substrate_bridge_register_bio_async(explanations_substrate_bridge_t* bridge, bio_router_t* router);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EXPLANATIONS_SUBSTRATE_BRIDGE_H */
