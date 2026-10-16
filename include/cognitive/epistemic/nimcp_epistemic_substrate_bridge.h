/**
 * @file nimcp_epistemic_substrate_bridge.h
 * @brief Bridge between Epistemic system and neural substrate
 *
 * WHAT: Links epistemic/knowledge-seeking to metabolic state
 * WHY: Epistemic evaluation requires sustained prefrontal processing
 * HOW: Monitors ATP/fatigue; modulates truth-seeking, evidence weighing, certainty
 *
 * BIOLOGICAL BASIS:
 * - Epistemic processing involves dorsolateral PFC and parietal
 * - ATP depletion reduces evidence integration quality
 * - Fatigue leads to premature certainty
 * - Metabolic stress impairs belief updating
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_EPISTEMIC_SUBSTRATE_BRIDGE_H
#define NIMCP_EPISTEMIC_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_SUBSTRATE_EPISTEMIC 0x121B

typedef struct {
    float evidence_integration;   /* Quality of evidence integration [0-1] */
    float belief_updating;        /* Rate of belief revision [0-1] */
    float certainty_calibration;  /* Accuracy of confidence [0-1] */
    float source_evaluation;      /* Quality of source assessment [0-1] */
    float overall_capacity;       /* Combined modulation [0-1] */
} epistemic_substrate_effects_t;

typedef struct {
    bool enable_atp_modulation;
    bool enable_fatigue_modulation;
    bool enable_bio_async;
    float atp_sensitivity;
    float fatigue_sensitivity;
    float min_capacity;
} epistemic_substrate_config_t;

typedef struct epistemic_substrate_bridge epistemic_substrate_bridge_t;

epistemic_substrate_config_t epistemic_substrate_default_config(void);
epistemic_substrate_bridge_t* epistemic_substrate_bridge_create(void* epistemic, neural_substrate_t* substrate, const epistemic_substrate_config_t* config);
void epistemic_substrate_bridge_destroy(epistemic_substrate_bridge_t* bridge);
int epistemic_substrate_bridge_update(epistemic_substrate_bridge_t* bridge);
int epistemic_substrate_bridge_get_effects(const epistemic_substrate_bridge_t* bridge, epistemic_substrate_effects_t* effects);
int epistemic_substrate_bridge_apply_effects(epistemic_substrate_bridge_t* bridge);
int epistemic_substrate_bridge_register_bio_async(epistemic_substrate_bridge_t* bridge, bio_router_t* router);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EPISTEMIC_SUBSTRATE_BRIDGE_H */
