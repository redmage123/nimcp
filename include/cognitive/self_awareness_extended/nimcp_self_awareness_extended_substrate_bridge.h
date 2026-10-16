/**
 * @file nimcp_self_awareness_extended_substrate_bridge.h
 * @brief Bridge between Extended Self-Awareness system and neural substrate
 *
 * WHAT: Links extended self-awareness to metabolic/energy state
 * WHY: Extended self-awareness requires high prefrontal and parietal resources
 * HOW: Monitors ATP/fatigue; modulates metacognition, temporal self, and narrative coherence
 *
 * BIOLOGICAL BASIS:
 * - Extended self requires sustained prefrontal-parietal network activity
 * - ATP depletion impairs metacognitive monitoring and future self-projection
 * - Fatigue reduces autobiographical memory integration
 * - Metabolic stress affects narrative identity coherence
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_SELF_AWARENESS_EXTENDED_SUBSTRATE_BRIDGE_H
#define NIMCP_SELF_AWARENESS_EXTENDED_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_SUBSTRATE_SELF_AWARENESS_EXT 0x130C

typedef struct {
    float metacognitive_depth;       /* Depth of metacognitive processing [0-1] */
    float temporal_self_coherence;   /* Coherence of temporal self-model [0-1] */
    float narrative_integration;     /* Integration of narrative identity [0-1] */
    float future_self_projection;    /* Capacity for future self-projection [0-1] */
    float overall_capacity;          /* Combined modulation [0-1] */
} self_awareness_ext_substrate_effects_t;

typedef struct {
    bool enable_atp_modulation;
    bool enable_fatigue_modulation;
    bool enable_bio_async;
    float atp_sensitivity;
    float fatigue_sensitivity;
    float min_capacity;
} self_awareness_ext_substrate_config_t;

typedef struct self_awareness_ext_substrate_bridge self_awareness_ext_substrate_bridge_t;

self_awareness_ext_substrate_config_t self_awareness_ext_substrate_default_config(void);
self_awareness_ext_substrate_bridge_t* self_awareness_ext_substrate_bridge_create(void* self_awareness_ext, neural_substrate_t* substrate, const self_awareness_ext_substrate_config_t* config);
void self_awareness_ext_substrate_bridge_destroy(self_awareness_ext_substrate_bridge_t* bridge);
int self_awareness_ext_substrate_bridge_update(self_awareness_ext_substrate_bridge_t* bridge);
int self_awareness_ext_substrate_bridge_get_effects(const self_awareness_ext_substrate_bridge_t* bridge, self_awareness_ext_substrate_effects_t* effects);
int self_awareness_ext_substrate_bridge_apply_effects(self_awareness_ext_substrate_bridge_t* bridge);
int self_awareness_ext_substrate_bridge_register_bio_async(self_awareness_ext_substrate_bridge_t* bridge, bio_router_t* router);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SELF_AWARENESS_EXTENDED_SUBSTRATE_BRIDGE_H */
