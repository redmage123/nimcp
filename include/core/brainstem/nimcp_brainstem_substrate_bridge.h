/**
 * @file nimcp_brainstem_substrate_bridge.h
 * @brief Bridge between Brainstem and neural substrate
 *
 * WHAT: Links brainstem function to metabolic state
 * WHY: Brainstem controls vital functions dependent on metabolic state
 * HOW: Monitors ATP/fatigue; modulates arousal, autonomic, vital functions
 *
 * BIOLOGICAL BASIS:
 * - Brainstem controls breathing, heart rate, arousal
 * - ATP depletion affects arousal systems
 * - Fatigue modulates reticular activating system
 * - Metabolic state affects autonomic balance
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_BRAINSTEM_SUBSTRATE_BRIDGE_H
#define NIMCP_BRAINSTEM_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_SUBSTRATE_BRAINSTEM 0x1236

typedef struct {
    float arousal_level;          /* Reticular arousal [0-1] */
    float autonomic_balance;      /* Sympathetic/parasympathetic [0-1] */
    float vital_stability;        /* Vital function stability [0-1] */
    float reflex_speed;           /* Reflex response speed [0-1] */
    float overall_capacity;       /* Combined modulation [0-1] */
} brainstem_substrate_effects_t;

typedef struct {
    bool enable_atp_modulation;
    bool enable_fatigue_modulation;
    bool enable_bio_async;
    float atp_sensitivity;
    float fatigue_sensitivity;
    float min_capacity;
} brainstem_substrate_config_t;

typedef struct brainstem_substrate_bridge brainstem_substrate_bridge_t;

brainstem_substrate_config_t brainstem_substrate_default_config(void);
brainstem_substrate_bridge_t* brainstem_substrate_bridge_create(void* brainstem, neural_substrate_t* substrate, const brainstem_substrate_config_t* config);
void brainstem_substrate_bridge_destroy(brainstem_substrate_bridge_t* bridge);
int brainstem_substrate_bridge_update(brainstem_substrate_bridge_t* bridge);
int brainstem_substrate_bridge_get_effects(const brainstem_substrate_bridge_t* bridge, brainstem_substrate_effects_t* effects);
int brainstem_substrate_bridge_apply_effects(brainstem_substrate_bridge_t* bridge);
int brainstem_substrate_bridge_register_bio_async(brainstem_substrate_bridge_t* bridge, bio_router_t* router);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAINSTEM_SUBSTRATE_BRIDGE_H */
