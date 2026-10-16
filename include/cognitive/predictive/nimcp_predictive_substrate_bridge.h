/**
 * @file nimcp_predictive_substrate_bridge.h
 * @brief Bridge between Predictive Coding system and neural substrate
 *
 * WHAT: Links predictive processing to metabolic state
 * WHY: Prediction requires hierarchical cortical resources
 * HOW: Monitors ATP/fatigue; modulates prediction accuracy, precision, update rate
 *
 * BIOLOGICAL BASIS:
 * - Predictive coding involves hierarchical cortical computation
 * - ATP depletion reduces prediction precision
 * - Fatigue increases prediction errors
 * - Metabolic stress impairs model updating
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_PREDICTIVE_SUBSTRATE_BRIDGE_H
#define NIMCP_PREDICTIVE_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_SUBSTRATE_PREDICTIVE 0x1218

typedef struct {
    float prediction_precision;   /* Precision of predictions [0-1] */
    float error_sensitivity;      /* Sensitivity to pred errors [0-1] */
    float model_update_rate;      /* Rate of model updating [0-1] */
    float hierarchical_depth;     /* Depth of pred hierarchy [0-1] */
    float overall_capacity;       /* Combined modulation [0-1] */
} predictive_substrate_effects_t;

typedef struct {
    bool enable_atp_modulation;
    bool enable_fatigue_modulation;
    bool enable_bio_async;
    float atp_sensitivity;
    float fatigue_sensitivity;
    float min_capacity;
} predictive_substrate_config_t;

typedef struct predictive_substrate_bridge predictive_substrate_bridge_t;

predictive_substrate_config_t predictive_substrate_default_config(void);
predictive_substrate_bridge_t* predictive_substrate_bridge_create(void* predictive, neural_substrate_t* substrate, const predictive_substrate_config_t* config);
void predictive_substrate_bridge_destroy(predictive_substrate_bridge_t* bridge);
int predictive_substrate_bridge_update(predictive_substrate_bridge_t* bridge);
int predictive_substrate_bridge_get_effects(const predictive_substrate_bridge_t* bridge, predictive_substrate_effects_t* effects);
int predictive_substrate_bridge_apply_effects(predictive_substrate_bridge_t* bridge);
int predictive_substrate_bridge_register_bio_async(predictive_substrate_bridge_t* bridge, bio_router_t* router);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PREDICTIVE_SUBSTRATE_BRIDGE_H */
