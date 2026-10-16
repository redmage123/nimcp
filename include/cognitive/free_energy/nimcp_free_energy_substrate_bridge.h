/**
 * @file nimcp_free_energy_substrate_bridge.h
 * @brief Bridge between Free Energy Principle system and neural substrate
 *
 * WHAT: Links FEP computations to metabolic/energy state
 * WHY: Variational inference and active inference require substantial computation
 * HOW: Monitors ATP/fatigue; modulates precision weighting, prediction depth, inference
 *
 * BIOLOGICAL BASIS:
 * - Free energy minimization is computationally intensive
 * - ATP depletion leads to reduced precision weighting in predictions
 * - Fatigue impairs model complexity and active inference
 * - Metabolic stress simplifies generative models
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_FREE_ENERGY_SUBSTRATE_BRIDGE_H
#define NIMCP_FREE_ENERGY_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_SUBSTRATE_FREE_ENERGY 0x1306

typedef struct {
    float precision_weighting;     /* Precision in prediction errors [0-1] */
    float prediction_depth;        /* Hierarchical prediction depth [0-1] */
    float active_inference;        /* Active inference capacity [0-1] */
    float model_complexity;        /* Generative model complexity [0-1] */
    float overall_capacity;        /* Combined modulation [0-1] */
} free_energy_substrate_effects_t;

typedef struct {
    bool enable_atp_modulation;
    bool enable_fatigue_modulation;
    bool enable_bio_async;
    float atp_sensitivity;
    float fatigue_sensitivity;
    float min_capacity;
} free_energy_substrate_config_t;

typedef struct free_energy_substrate_bridge free_energy_substrate_bridge_t;

free_energy_substrate_config_t free_energy_substrate_default_config(void);
free_energy_substrate_bridge_t* free_energy_substrate_bridge_create(void* free_energy, neural_substrate_t* substrate, const free_energy_substrate_config_t* config);
void free_energy_substrate_bridge_destroy(free_energy_substrate_bridge_t* bridge);
int free_energy_substrate_bridge_update(free_energy_substrate_bridge_t* bridge);
int free_energy_substrate_bridge_get_effects(const free_energy_substrate_bridge_t* bridge, free_energy_substrate_effects_t* effects);
int free_energy_substrate_bridge_apply_effects(free_energy_substrate_bridge_t* bridge);
int free_energy_substrate_bridge_register_bio_async(free_energy_substrate_bridge_t* bridge, bio_router_t* router);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FREE_ENERGY_SUBSTRATE_BRIDGE_H */
