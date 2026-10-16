/**
 * @file nimcp_self_model_substrate_bridge.h
 * @brief Bridge between Self-Model system and neural substrate
 *
 * WHAT: Links self-modeling to metabolic state
 * WHY: Self-representation requires medial prefrontal and parietal resources
 * HOW: Monitors ATP/fatigue; modulates self-representation, body schema, agency
 *
 * BIOLOGICAL BASIS:
 * - Self-model involves medial PFC and posterior parietal cortex
 * - ATP depletion reduces self-representation accuracy
 * - Fatigue impairs body schema maintenance
 * - Metabolic stress affects sense of agency
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_SELF_MODEL_SUBSTRATE_BRIDGE_H
#define NIMCP_SELF_MODEL_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_SUBSTRATE_SELF_MODEL 0x121E

typedef struct {
    float self_representation;    /* Accuracy of self-model [0-1] */
    float body_schema;            /* Quality of body representation [0-1] */
    float agency_sense;           /* Clarity of agency [0-1] */
    float boundary_clarity;       /* Self-other boundary [0-1] */
    float overall_capacity;       /* Combined modulation [0-1] */
} self_model_substrate_effects_t;

typedef struct {
    bool enable_atp_modulation;
    bool enable_fatigue_modulation;
    bool enable_bio_async;
    float atp_sensitivity;
    float fatigue_sensitivity;
    float min_capacity;
} self_model_substrate_config_t;

typedef struct self_model_substrate_bridge self_model_substrate_bridge_t;

self_model_substrate_config_t self_model_substrate_default_config(void);
self_model_substrate_bridge_t* self_model_substrate_bridge_create(void* self_model, neural_substrate_t* substrate, const self_model_substrate_config_t* config);
void self_model_substrate_bridge_destroy(self_model_substrate_bridge_t* bridge);
int self_model_substrate_bridge_update(self_model_substrate_bridge_t* bridge);
int self_model_substrate_bridge_get_effects(const self_model_substrate_bridge_t* bridge, self_model_substrate_effects_t* effects);
int self_model_substrate_bridge_apply_effects(self_model_substrate_bridge_t* bridge);
int self_model_substrate_bridge_register_bio_async(self_model_substrate_bridge_t* bridge, bio_router_t* router);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SELF_MODEL_SUBSTRATE_BRIDGE_H */
