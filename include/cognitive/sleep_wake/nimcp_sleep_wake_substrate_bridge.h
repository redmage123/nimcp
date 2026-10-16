/**
 * @file nimcp_sleep_wake_substrate_bridge.h
 * @brief Bridge between Sleep-Wake system and neural substrate
 *
 * WHAT: Links sleep-wake regulation to metabolic state
 * WHY: Sleep-wake transitions are fundamentally metabolic processes
 * HOW: Monitors ATP/fatigue; modulates arousal, sleep pressure, circadian phase
 *
 * BIOLOGICAL BASIS:
 * - Sleep-wake involves hypothalamic and brainstem nuclei
 * - ATP depletion increases sleep pressure
 * - Adenosine accumulation drives sleep need
 * - Metabolic restoration is primary sleep function
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_SLEEP_WAKE_SUBSTRATE_BRIDGE_H
#define NIMCP_SLEEP_WAKE_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_SUBSTRATE_SLEEP_WAKE 0x121F

typedef struct {
    float arousal_level;          /* Current arousal/alertness [0-1] */
    float sleep_pressure;         /* Homeostatic sleep drive [0-1] */
    float circadian_phase;        /* Circadian alertness signal [0-1] */
    float recovery_rate;          /* Rate of metabolic recovery [0-1] */
    float overall_capacity;       /* Combined modulation [0-1] */
} sleep_wake_substrate_effects_t;

typedef struct {
    bool enable_atp_modulation;
    bool enable_fatigue_modulation;
    bool enable_bio_async;
    float atp_sensitivity;
    float fatigue_sensitivity;
    float min_capacity;
} sleep_wake_substrate_config_t;

typedef struct sleep_wake_substrate_bridge sleep_wake_substrate_bridge_t;

sleep_wake_substrate_config_t sleep_wake_substrate_default_config(void);
sleep_wake_substrate_bridge_t* sleep_wake_substrate_bridge_create(void* sleep_wake, neural_substrate_t* substrate, const sleep_wake_substrate_config_t* config);
void sleep_wake_substrate_bridge_destroy(sleep_wake_substrate_bridge_t* bridge);
int sleep_wake_substrate_bridge_update(sleep_wake_substrate_bridge_t* bridge);
int sleep_wake_substrate_bridge_get_effects(const sleep_wake_substrate_bridge_t* bridge, sleep_wake_substrate_effects_t* effects);
int sleep_wake_substrate_bridge_apply_effects(sleep_wake_substrate_bridge_t* bridge);
int sleep_wake_substrate_bridge_register_bio_async(sleep_wake_substrate_bridge_t* bridge, bio_router_t* router);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SLEEP_WAKE_SUBSTRATE_BRIDGE_H */
