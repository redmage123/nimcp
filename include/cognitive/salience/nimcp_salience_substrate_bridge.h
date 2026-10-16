/**
 * @file nimcp_salience_substrate_bridge.h
 * @brief Bridge between Salience system and neural substrate
 *
 * WHAT: Links salience detection to metabolic state
 * WHY: Salience processing requires anterior insula and ACC resources
 * HOW: Monitors ATP/fatigue; modulates salience detection, prioritization, filtering
 *
 * BIOLOGICAL BASIS:
 * - Salience network involves anterior insula and anterior cingulate
 * - ATP depletion reduces salience discrimination
 * - Fatigue impairs attentional capture
 * - Metabolic stress affects priority assignment
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_SALIENCE_SUBSTRATE_BRIDGE_H
#define NIMCP_SALIENCE_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_SUBSTRATE_SALIENCE 0x121D

typedef struct {
    float detection_sensitivity;  /* Sensitivity to salient stimuli [0-1] */
    float priority_accuracy;      /* Accuracy of priority assignment [0-1] */
    float filtering_quality;      /* Quality of noise filtering [0-1] */
    float switching_speed;        /* Speed of attention switching [0-1] */
    float overall_capacity;       /* Combined modulation [0-1] */
} salience_substrate_effects_t;

typedef struct {
    bool enable_atp_modulation;
    bool enable_fatigue_modulation;
    bool enable_bio_async;
    float atp_sensitivity;
    float fatigue_sensitivity;
    float min_capacity;
} salience_substrate_config_t;

typedef struct salience_substrate_bridge salience_substrate_bridge_t;

salience_substrate_config_t salience_substrate_default_config(void);
salience_substrate_bridge_t* salience_substrate_bridge_create(void* salience, neural_substrate_t* substrate, const salience_substrate_config_t* config);
void salience_substrate_bridge_destroy(salience_substrate_bridge_t* bridge);
int salience_substrate_bridge_update(salience_substrate_bridge_t* bridge);
int salience_substrate_bridge_get_effects(const salience_substrate_bridge_t* bridge, salience_substrate_effects_t* effects);
int salience_substrate_bridge_apply_effects(salience_substrate_bridge_t* bridge);
int salience_substrate_bridge_register_bio_async(salience_substrate_bridge_t* bridge, bio_router_t* router);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SALIENCE_SUBSTRATE_BRIDGE_H */
