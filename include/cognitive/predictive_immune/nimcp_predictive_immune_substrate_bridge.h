/**
 * @file nimcp_predictive_immune_substrate_bridge.h
 * @brief Bridge between Predictive Immune system and neural substrate
 *
 * WHAT: Links predictive-immune integration to metabolic/energy state
 * WHY: Predictive immune processing requires ATP for cytokine modeling and interoception
 * HOW: Monitors ATP/fatigue; modulates prediction accuracy, immune response, and precision
 *
 * BIOLOGICAL BASIS:
 * - Immune-predictive coding requires prefrontal and insular resources
 * - ATP depletion impairs cytokine precision weighting
 * - Metabolic stress affects interoceptive prediction accuracy
 * - Fatigue reduces immune-cognitive integration fidelity
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_PREDICTIVE_IMMUNE_SUBSTRATE_BRIDGE_H
#define NIMCP_PREDICTIVE_IMMUNE_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_SUBSTRATE_PREDICTIVE_IMMUNE 0x130B

typedef struct {
    float prediction_accuracy;      /* Accuracy of interoceptive predictions [0-1] */
    float immune_precision;         /* Precision of immune state modeling [0-1] */
    float cytokine_sensitivity;     /* Sensitivity to cytokine signals [0-1] */
    float integration_capacity;     /* Capacity for immune-cognitive integration [0-1] */
    float overall_capacity;         /* Combined modulation [0-1] */
} predictive_immune_substrate_effects_t;

typedef struct {
    bool enable_atp_modulation;
    bool enable_fatigue_modulation;
    bool enable_bio_async;
    float atp_sensitivity;
    float fatigue_sensitivity;
    float min_capacity;
} predictive_immune_substrate_config_t;

typedef struct predictive_immune_substrate_bridge predictive_immune_substrate_bridge_t;

predictive_immune_substrate_config_t predictive_immune_substrate_default_config(void);
predictive_immune_substrate_bridge_t* predictive_immune_substrate_bridge_create(void* predictive_immune, neural_substrate_t* substrate, const predictive_immune_substrate_config_t* config);
void predictive_immune_substrate_bridge_destroy(predictive_immune_substrate_bridge_t* bridge);
int predictive_immune_substrate_bridge_update(predictive_immune_substrate_bridge_t* bridge);
int predictive_immune_substrate_bridge_get_effects(const predictive_immune_substrate_bridge_t* bridge, predictive_immune_substrate_effects_t* effects);
int predictive_immune_substrate_bridge_apply_effects(predictive_immune_substrate_bridge_t* bridge);
int predictive_immune_substrate_bridge_register_bio_async(predictive_immune_substrate_bridge_t* bridge, bio_router_t* router);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PREDICTIVE_IMMUNE_SUBSTRATE_BRIDGE_H */
