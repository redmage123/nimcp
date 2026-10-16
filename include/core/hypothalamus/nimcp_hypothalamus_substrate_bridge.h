/**
 * @file nimcp_hypothalamus_substrate_bridge.h
 * @brief Bridge between Hypothalamus and neural substrate
 *
 * WHAT: Links hypothalamic function to metabolic state
 * WHY: Hypothalamus is the master regulator of metabolism and homeostasis
 * HOW: Monitors ATP/fatigue; modulates homeostasis, drives, circadian rhythm
 *
 * BIOLOGICAL BASIS:
 * - Hypothalamus controls hunger, thirst, temperature, circadian
 * - ATP sensors in hypothalamus regulate feeding
 * - Fatigue signals integrate with hypothalamic arousal
 * - Metabolic state is directly sensed by hypothalamic neurons
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_HYPOTHALAMUS_SUBSTRATE_BRIDGE_H
#define NIMCP_HYPOTHALAMUS_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_SUBSTRATE_HYPOTHALAMUS 0x123D

typedef struct {
    float homeostatic_drive;      /* Drive for homeostasis [0-1] */
    float hunger_signal;          /* Hunger/satiety signal [0-1] */
    float circadian_strength;     /* Circadian rhythm strength [0-1] */
    float stress_response;        /* HPA axis activation [0-1] */
    float overall_capacity;       /* Combined modulation [0-1] */
} hypothalamus_substrate_effects_t;

typedef struct {
    bool enable_atp_modulation;
    bool enable_fatigue_modulation;
    bool enable_bio_async;
    float atp_sensitivity;
    float fatigue_sensitivity;
    float min_capacity;
} hypothalamus_substrate_config_t;

typedef struct hypothalamus_substrate_bridge hypothalamus_substrate_bridge_t;

hypothalamus_substrate_config_t hypothalamus_substrate_default_config(void);
hypothalamus_substrate_bridge_t* hypothalamus_substrate_bridge_create(void* hypothalamus, neural_substrate_t* substrate, const hypothalamus_substrate_config_t* config);
void hypothalamus_substrate_bridge_destroy(hypothalamus_substrate_bridge_t* bridge);
int hypothalamus_substrate_bridge_update(hypothalamus_substrate_bridge_t* bridge);
int hypothalamus_substrate_bridge_get_effects(const hypothalamus_substrate_bridge_t* bridge, hypothalamus_substrate_effects_t* effects);
int hypothalamus_substrate_bridge_apply_effects(hypothalamus_substrate_bridge_t* bridge);
int hypothalamus_substrate_bridge_register_bio_async(hypothalamus_substrate_bridge_t* bridge, bio_router_t* router);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HYPOTHALAMUS_SUBSTRATE_BRIDGE_H */
