/**
 * @file nimcp_somatosensory_substrate_bridge.h
 * @brief Bridge between Somatosensory processing and neural substrate
 *
 * WHAT: Links touch/proprioception to metabolic state
 * WHY: Somatosensory processing requires sustained parietal resources
 * HOW: Monitors ATP/fatigue; modulates tactile acuity, proprioception
 *
 * BIOLOGICAL BASIS:
 * - Somatosensory cortex in parietal lobe
 * - ATP depletion reduces tactile discrimination
 * - Fatigue impairs proprioceptive accuracy
 * - Metabolic stress affects body schema maintenance
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_SOMATOSENSORY_SUBSTRATE_BRIDGE_H
#define NIMCP_SOMATOSENSORY_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_SUBSTRATE_SOMATOSENSORY 0x1233

typedef struct {
    float tactile_acuity;         /* Touch discrimination [0-1] */
    float proprioception;         /* Position sense accuracy [0-1] */
    float pain_processing;        /* Pain signal processing [0-1] */
    float temperature_sense;      /* Temperature sensing [0-1] */
    float overall_capacity;       /* Combined modulation [0-1] */
} somatosensory_substrate_effects_t;

typedef struct {
    bool enable_atp_modulation;
    bool enable_fatigue_modulation;
    bool enable_bio_async;
    float atp_sensitivity;
    float fatigue_sensitivity;
    float min_capacity;
} somatosensory_substrate_config_t;

typedef struct somatosensory_substrate_bridge somatosensory_substrate_bridge_t;

somatosensory_substrate_config_t somatosensory_substrate_default_config(void);
somatosensory_substrate_bridge_t* somatosensory_substrate_bridge_create(void* somatosensory, neural_substrate_t* substrate, const somatosensory_substrate_config_t* config);
void somatosensory_substrate_bridge_destroy(somatosensory_substrate_bridge_t* bridge);
int somatosensory_substrate_bridge_update(somatosensory_substrate_bridge_t* bridge);
int somatosensory_substrate_bridge_get_effects(const somatosensory_substrate_bridge_t* bridge, somatosensory_substrate_effects_t* effects);
int somatosensory_substrate_bridge_apply_effects(somatosensory_substrate_bridge_t* bridge);
int somatosensory_substrate_bridge_register_bio_async(somatosensory_substrate_bridge_t* bridge, bio_router_t* router);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SOMATOSENSORY_SUBSTRATE_BRIDGE_H */
