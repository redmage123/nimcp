/**
 * @file nimcp_temporal_substrate_bridge.h
 * @brief Bridge between Temporal Cortex and neural substrate
 *
 * WHAT: Links temporal lobe function to metabolic state
 * WHY: Temporal cortex handles language, object recognition, semantic memory
 * HOW: Monitors ATP/fatigue; modulates language, recognition, semantic access
 *
 * BIOLOGICAL BASIS:
 * - Temporal cortex processes auditory, language, semantic memory
 * - ATP depletion impairs word finding and recognition
 * - Fatigue affects language comprehension
 * - Metabolic stress reduces semantic access speed
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_TEMPORAL_SUBSTRATE_BRIDGE_H
#define NIMCP_TEMPORAL_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_SUBSTRATE_TEMPORAL 0x123A

typedef struct {
    float language_processing;    /* Language comprehension [0-1] */
    float object_recognition;     /* Visual object recognition [0-1] */
    float semantic_access;        /* Semantic memory access [0-1] */
    float auditory_processing;    /* Complex auditory processing [0-1] */
    float overall_capacity;       /* Combined modulation [0-1] */
} temporal_substrate_effects_t;

typedef struct {
    bool enable_atp_modulation;
    bool enable_fatigue_modulation;
    bool enable_bio_async;
    float atp_sensitivity;
    float fatigue_sensitivity;
    float min_capacity;
} temporal_substrate_config_t;

typedef struct temporal_substrate_bridge temporal_substrate_bridge_t;

temporal_substrate_config_t temporal_substrate_default_config(void);
temporal_substrate_bridge_t* temporal_substrate_bridge_create(void* temporal, neural_substrate_t* substrate, const temporal_substrate_config_t* config);
void temporal_substrate_bridge_destroy(temporal_substrate_bridge_t* bridge);
int temporal_substrate_bridge_update(temporal_substrate_bridge_t* bridge);
int temporal_substrate_bridge_get_effects(const temporal_substrate_bridge_t* bridge, temporal_substrate_effects_t* effects);
int temporal_substrate_bridge_apply_effects(temporal_substrate_bridge_t* bridge);
int temporal_substrate_bridge_register_bio_async(temporal_substrate_bridge_t* bridge, bio_router_t* router);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TEMPORAL_SUBSTRATE_BRIDGE_H */
