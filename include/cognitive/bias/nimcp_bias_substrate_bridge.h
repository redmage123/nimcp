/**
 * @file nimcp_bias_substrate_bridge.h
 * @brief Bridge between Cognitive Bias system and neural substrate
 *
 * WHAT: Links cognitive bias detection/correction to metabolic state
 * WHY: Bias detection requires metacognitive prefrontal resources
 * HOW: Monitors ATP/fatigue; modulates bias detection, correction, awareness
 *
 * BIOLOGICAL BASIS:
 * - Bias detection involves anterior cingulate and prefrontal monitoring
 * - ATP depletion increases susceptibility to biases
 * - Fatigue reduces metacognitive oversight
 * - Metabolic stress leads to heuristic shortcuts
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_BIAS_SUBSTRATE_BRIDGE_H
#define NIMCP_BIAS_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_SUBSTRATE_BIAS 0x121A

typedef struct {
    float bias_detection;         /* Ability to detect biases [0-1] */
    float correction_strength;    /* Strength of bias correction [0-1] */
    float metacognitive_oversight;/* Quality of self-monitoring [0-1] */
    float heuristic_resistance;   /* Resistance to shortcut thinking [0-1] */
    float overall_capacity;       /* Combined modulation [0-1] */
} bias_substrate_effects_t;

typedef struct {
    bool enable_atp_modulation;
    bool enable_fatigue_modulation;
    bool enable_bio_async;
    float atp_sensitivity;
    float fatigue_sensitivity;
    float min_capacity;
} bias_substrate_config_t;

typedef struct bias_substrate_bridge bias_substrate_bridge_t;

bias_substrate_config_t bias_substrate_default_config(void);
bias_substrate_bridge_t* bias_substrate_bridge_create(void* bias, neural_substrate_t* substrate, const bias_substrate_config_t* config);
void bias_substrate_bridge_destroy(bias_substrate_bridge_t* bridge);
int bias_substrate_bridge_update(bias_substrate_bridge_t* bridge);
int bias_substrate_bridge_get_effects(const bias_substrate_bridge_t* bridge, bias_substrate_effects_t* effects);
int bias_substrate_bridge_apply_effects(bias_substrate_bridge_t* bridge);
int bias_substrate_bridge_register_bio_async(bias_substrate_bridge_t* bridge, bio_router_t* router);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BIAS_SUBSTRATE_BRIDGE_H */
