/**
 * @file nimcp_jepa_substrate_bridge.h
 * @brief Bridge between JEPA (Joint Embedding Predictive Architecture) and neural substrate
 *
 * WHAT: Links JEPA world modeling to metabolic state
 * WHY: Predictive world modeling requires sustained cortical computation
 * HOW: Monitors ATP/fatigue; modulates prediction quality, horizon, precision
 *
 * BIOLOGICAL BASIS:
 * - World modeling involves distributed cortical prediction networks
 * - ATP depletion reduces prediction horizon
 * - Fatigue impairs model updating and precision
 * - Metabolic stress favors simpler world models
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_JEPA_SUBSTRATE_BRIDGE_H
#define NIMCP_JEPA_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_SUBSTRATE_JEPA 0x122C

typedef struct {
    float prediction_horizon;     /* Temporal prediction depth [0-1] */
    float model_precision;        /* Precision of world model [0-1] */
    float embedding_quality;      /* Quality of latent embeddings [0-1] */
    float update_rate;            /* Rate of model updates [0-1] */
    float overall_capacity;       /* Combined modulation [0-1] */
} jepa_substrate_effects_t;

typedef struct {
    bool enable_atp_modulation;
    bool enable_fatigue_modulation;
    bool enable_bio_async;
    float atp_sensitivity;
    float fatigue_sensitivity;
    float min_capacity;
} jepa_substrate_config_t;

typedef struct jepa_substrate_bridge jepa_substrate_bridge_t;

jepa_substrate_config_t jepa_substrate_default_config(void);
jepa_substrate_bridge_t* jepa_substrate_bridge_create(void* jepa, neural_substrate_t* substrate, const jepa_substrate_config_t* config);
void jepa_substrate_bridge_destroy(jepa_substrate_bridge_t* bridge);
int jepa_substrate_bridge_update(jepa_substrate_bridge_t* bridge);
int jepa_substrate_bridge_get_effects(const jepa_substrate_bridge_t* bridge, jepa_substrate_effects_t* effects);
int jepa_substrate_bridge_apply_effects(jepa_substrate_bridge_t* bridge);
int jepa_substrate_bridge_register_bio_async(jepa_substrate_bridge_t* bridge, bio_router_t* router);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_JEPA_SUBSTRATE_BRIDGE_H */
