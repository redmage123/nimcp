/**
 * @file nimcp_self_awareness_substrate_bridge.h
 * @brief Bridge between Self-Awareness system and neural substrate
 *
 * WHAT: Links self-awareness to metabolic state
 * WHY: Metacognition requires medial prefrontal and cingulate resources
 * HOW: Monitors ATP/fatigue; modulates self-reflection, introspection, metacognition
 *
 * BIOLOGICAL BASIS:
 * - Self-awareness involves medial PFC and posterior cingulate
 * - ATP depletion reduces metacognitive accuracy
 * - Fatigue impairs self-monitoring
 * - Default mode network requires sustained resources
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_SELF_AWARENESS_SUBSTRATE_BRIDGE_H
#define NIMCP_SELF_AWARENESS_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_SUBSTRATE_SELF_AWARENESS 0x1217

typedef struct {
    float introspection_depth;    /* Depth of self-reflection [0-1] */
    float metacognitive_accuracy; /* Accuracy of self-knowledge [0-1] */
    float self_monitoring;        /* Self-monitoring capacity [0-1] */
    float identity_coherence;     /* Coherence of self-model [0-1] */
    float overall_capacity;       /* Combined modulation [0-1] */
} self_awareness_substrate_effects_t;

typedef struct {
    bool enable_atp_modulation;
    bool enable_fatigue_modulation;
    bool enable_bio_async;
    float atp_sensitivity;
    float fatigue_sensitivity;
    float min_capacity;
} self_awareness_substrate_config_t;

typedef struct self_awareness_substrate_bridge self_awareness_substrate_bridge_t;

self_awareness_substrate_config_t self_awareness_substrate_default_config(void);
self_awareness_substrate_bridge_t* self_awareness_substrate_bridge_create(void* self_awareness, neural_substrate_t* substrate, const self_awareness_substrate_config_t* config);
void self_awareness_substrate_bridge_destroy(self_awareness_substrate_bridge_t* bridge);
int self_awareness_substrate_bridge_update(self_awareness_substrate_bridge_t* bridge);
int self_awareness_substrate_bridge_get_effects(const self_awareness_substrate_bridge_t* bridge, self_awareness_substrate_effects_t* effects);
int self_awareness_substrate_bridge_apply_effects(self_awareness_substrate_bridge_t* bridge);
int self_awareness_substrate_bridge_register_bio_async(self_awareness_substrate_bridge_t* bridge, bio_router_t* router);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SELF_AWARENESS_SUBSTRATE_BRIDGE_H */
