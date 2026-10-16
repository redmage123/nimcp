/**
 * @file nimcp_empathetic_response_substrate_bridge.h
 * @brief Bridge between Empathetic Response system and neural substrate
 *
 * WHAT: Links empathetic response generation to metabolic/energy state
 * WHY: Empathy requires significant cognitive and affective resources
 * HOW: Monitors ATP/fatigue; modulates empathic accuracy, response depth, timing
 *
 * BIOLOGICAL BASIS:
 * - Empathy involves mirror neuron system and prefrontal cortex
 * - ATP depletion leads to reduced empathic accuracy and emotional blunting
 * - Fatigue increases empathy fatigue and compassion burnout
 * - Metabolic stress reduces perspective-taking capacity
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_EMPATHETIC_RESPONSE_SUBSTRATE_BRIDGE_H
#define NIMCP_EMPATHETIC_RESPONSE_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_SUBSTRATE_EMPATHETIC_RESPONSE 0x1303

typedef struct {
    float empathic_accuracy;       /* Accuracy of emotion recognition [0-1] */
    float response_depth;          /* Depth of empathic response [0-1] */
    float perspective_taking;      /* Cognitive perspective-taking capacity [0-1] */
    float compassion_endurance;    /* Compassion fatigue resistance [0-1] */
    float overall_capacity;        /* Combined modulation [0-1] */
} empathetic_response_substrate_effects_t;

typedef struct {
    bool enable_atp_modulation;
    bool enable_fatigue_modulation;
    bool enable_bio_async;
    float atp_sensitivity;
    float fatigue_sensitivity;
    float min_capacity;
} empathetic_response_substrate_config_t;

typedef struct empathetic_response_substrate_bridge empathetic_response_substrate_bridge_t;

empathetic_response_substrate_config_t empathetic_response_substrate_default_config(void);
empathetic_response_substrate_bridge_t* empathetic_response_substrate_bridge_create(void* empathetic_response, neural_substrate_t* substrate, const empathetic_response_substrate_config_t* config);
void empathetic_response_substrate_bridge_destroy(empathetic_response_substrate_bridge_t* bridge);
int empathetic_response_substrate_bridge_update(empathetic_response_substrate_bridge_t* bridge);
int empathetic_response_substrate_bridge_get_effects(const empathetic_response_substrate_bridge_t* bridge, empathetic_response_substrate_effects_t* effects);
int empathetic_response_substrate_bridge_apply_effects(empathetic_response_substrate_bridge_t* bridge);
int empathetic_response_substrate_bridge_register_bio_async(empathetic_response_substrate_bridge_t* bridge, bio_router_t* router);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EMPATHETIC_RESPONSE_SUBSTRATE_BRIDGE_H */
