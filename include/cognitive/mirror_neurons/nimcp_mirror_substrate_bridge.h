/**
 * @file nimcp_mirror_substrate_bridge.h
 * @brief Bridge between Mirror Neuron system and neural substrate
 *
 * WHAT: Links mirror neuron simulation to metabolic state
 * WHY: Action understanding and imitation require motor-premotor resources
 * HOW: Monitors ATP/fatigue; modulates mirroring fidelity, empathy, imitation
 *
 * BIOLOGICAL BASIS:
 * - Mirror neurons in premotor and parietal cortex
 * - ATP depletion reduces simulation accuracy
 * - Fatigue impairs empathic resonance
 * - Metabolic stress affects action-perception coupling
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_MIRROR_SUBSTRATE_BRIDGE_H
#define NIMCP_MIRROR_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_SUBSTRATE_MIRROR 0x1215

typedef struct {
    float mirroring_fidelity;     /* Accuracy of action simulation [0-1] */
    float empathic_resonance;     /* Strength of empathic response [0-1] */
    float imitation_capacity;     /* Ability to imitate actions [0-1] */
    float action_prediction;      /* Action prediction accuracy [0-1] */
    float overall_capacity;       /* Combined modulation [0-1] */
} mirror_substrate_effects_t;

typedef struct {
    bool enable_atp_modulation;
    bool enable_fatigue_modulation;
    bool enable_bio_async;
    float atp_sensitivity;
    float fatigue_sensitivity;
    float min_capacity;
} mirror_substrate_config_t;

typedef struct mirror_substrate_bridge mirror_substrate_bridge_t;

mirror_substrate_config_t mirror_substrate_default_config(void);
mirror_substrate_bridge_t* mirror_substrate_bridge_create(void* mirror, neural_substrate_t* substrate, const mirror_substrate_config_t* config);
void mirror_substrate_bridge_destroy(mirror_substrate_bridge_t* bridge);
int mirror_substrate_bridge_update(mirror_substrate_bridge_t* bridge);
int mirror_substrate_bridge_get_effects(const mirror_substrate_bridge_t* bridge, mirror_substrate_effects_t* effects);
int mirror_substrate_bridge_apply_effects(mirror_substrate_bridge_t* bridge);
int mirror_substrate_bridge_register_bio_async(mirror_substrate_bridge_t* bridge, bio_router_t* router);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MIRROR_SUBSTRATE_BRIDGE_H */
