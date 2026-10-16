/**
 * @file nimcp_parietal_substrate_bridge.h
 * @brief Bridge between Parietal Cortex and neural substrate
 *
 * WHAT: Links parietal lobe function to metabolic state
 * WHY: Parietal cortex handles spatial processing and attention
 * HOW: Monitors ATP/fatigue; modulates spatial awareness, attention, integration
 *
 * BIOLOGICAL BASIS:
 * - Parietal cortex handles spatial awareness and sensory integration
 * - ATP depletion impairs spatial attention
 * - Fatigue affects numerical processing and attention
 * - Metabolic stress reduces multimodal integration
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_PARIETAL_SUBSTRATE_BRIDGE_H
#define NIMCP_PARIETAL_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_SUBSTRATE_PARIETAL 0x123B

typedef struct {
    float spatial_attention;      /* Spatial attention capacity [0-1] */
    float numerical_processing;   /* Mathematical processing [0-1] */
    float sensory_integration;    /* Multimodal integration [0-1] */
    float body_awareness;         /* Body schema/awareness [0-1] */
    float overall_capacity;       /* Combined modulation [0-1] */
} parietal_substrate_effects_t;

typedef struct {
    bool enable_atp_modulation;
    bool enable_fatigue_modulation;
    bool enable_bio_async;
    float atp_sensitivity;
    float fatigue_sensitivity;
    float min_capacity;
} parietal_substrate_config_t;

typedef struct parietal_substrate_bridge parietal_substrate_bridge_t;

parietal_substrate_config_t parietal_substrate_default_config(void);
parietal_substrate_bridge_t* parietal_substrate_bridge_create(void* parietal, neural_substrate_t* substrate, const parietal_substrate_config_t* config);
void parietal_substrate_bridge_destroy(parietal_substrate_bridge_t* bridge);
int parietal_substrate_bridge_update(parietal_substrate_bridge_t* bridge);
int parietal_substrate_bridge_get_effects(const parietal_substrate_bridge_t* bridge, parietal_substrate_effects_t* effects);
int parietal_substrate_bridge_apply_effects(parietal_substrate_bridge_t* bridge);
int parietal_substrate_bridge_register_bio_async(parietal_substrate_bridge_t* bridge, bio_router_t* router);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PARIETAL_SUBSTRATE_BRIDGE_H */
