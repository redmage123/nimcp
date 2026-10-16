/**
 * @file nimcp_insula_substrate_bridge.h
 * @brief Bridge between Insula and neural substrate
 *
 * WHAT: Links insular function to metabolic state
 * WHY: Insula integrates interoception and is sensitive to metabolic state
 * HOW: Monitors ATP/fatigue; modulates interoception, disgust, empathy
 *
 * BIOLOGICAL BASIS:
 * - Insula integrates bodily signals (interoception)
 * - ATP state directly sensed by insular cortex
 * - Fatigue affects interoceptive accuracy
 * - Metabolic stress modulates bodily awareness
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_INSULA_SUBSTRATE_BRIDGE_H
#define NIMCP_INSULA_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_SUBSTRATE_INSULA 0x1239

typedef struct {
    float interoceptive_accuracy; /* Body signal accuracy [0-1] */
    float emotional_awareness;    /* Awareness of emotions [0-1] */
    float disgust_sensitivity;    /* Disgust response [0-1] */
    float empathic_response;      /* Empathic processing [0-1] */
    float overall_capacity;       /* Combined modulation [0-1] */
} insula_substrate_effects_t;

typedef struct {
    bool enable_atp_modulation;
    bool enable_fatigue_modulation;
    bool enable_bio_async;
    float atp_sensitivity;
    float fatigue_sensitivity;
    float min_capacity;
} insula_substrate_config_t;

typedef struct insula_substrate_bridge insula_substrate_bridge_t;

insula_substrate_config_t insula_substrate_default_config(void);
insula_substrate_bridge_t* insula_substrate_bridge_create(void* insula, neural_substrate_t* substrate, const insula_substrate_config_t* config);
void insula_substrate_bridge_destroy(insula_substrate_bridge_t* bridge);
int insula_substrate_bridge_update(insula_substrate_bridge_t* bridge);
int insula_substrate_bridge_get_effects(const insula_substrate_bridge_t* bridge, insula_substrate_effects_t* effects);
int insula_substrate_bridge_apply_effects(insula_substrate_bridge_t* bridge);
int insula_substrate_bridge_register_bio_async(insula_substrate_bridge_t* bridge, bio_router_t* router);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_INSULA_SUBSTRATE_BRIDGE_H */
