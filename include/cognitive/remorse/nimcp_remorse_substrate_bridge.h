/**
 * @file nimcp_remorse_substrate_bridge.h
 * @brief Bridge between Remorse system and neural substrate
 *
 * WHAT: Links remorse/guilt processing to metabolic state
 * WHY: Remorse requires sustained anterior cingulate and insula processing
 * HOW: Monitors ATP/fatigue; modulates guilt intensity, repair motivation, learning
 *
 * BIOLOGICAL BASIS:
 * - Remorse involves anterior cingulate and insula
 * - ATP depletion can intensify or flatten guilt responses
 * - Fatigue impairs moral reasoning about transgressions
 * - Metabolic state affects repair/atonement motivation
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_REMORSE_SUBSTRATE_BRIDGE_H
#define NIMCP_REMORSE_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_SUBSTRATE_REMORSE 0x1225

typedef struct {
    float guilt_processing;       /* Capacity for guilt processing [0-1] */
    float repair_motivation;      /* Motivation to make amends [0-1] */
    float moral_learning;         /* Learning from transgressions [0-1] */
    float self_forgiveness;       /* Capacity for self-forgiveness [0-1] */
    float overall_capacity;       /* Combined modulation [0-1] */
} remorse_substrate_effects_t;

typedef struct {
    bool enable_atp_modulation;
    bool enable_fatigue_modulation;
    bool enable_bio_async;
    float atp_sensitivity;
    float fatigue_sensitivity;
    float min_capacity;
} remorse_substrate_config_t;

typedef struct remorse_substrate_bridge remorse_substrate_bridge_t;

remorse_substrate_config_t remorse_substrate_default_config(void);
remorse_substrate_bridge_t* remorse_substrate_bridge_create(void* remorse, neural_substrate_t* substrate, const remorse_substrate_config_t* config);
void remorse_substrate_bridge_destroy(remorse_substrate_bridge_t* bridge);
int remorse_substrate_bridge_update(remorse_substrate_bridge_t* bridge);
int remorse_substrate_bridge_get_effects(const remorse_substrate_bridge_t* bridge, remorse_substrate_effects_t* effects);
int remorse_substrate_bridge_apply_effects(remorse_substrate_bridge_t* bridge);
int remorse_substrate_bridge_register_bio_async(remorse_substrate_bridge_t* bridge, bio_router_t* router);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_REMORSE_SUBSTRATE_BRIDGE_H */
