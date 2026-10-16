/**
 * @file nimcp_autobio_substrate_bridge.h
 * @brief Bridge between Autobiographical Memory and neural substrate
 *
 * WHAT: Links autobiographical memory to metabolic state
 * WHY: Autobiographical recall requires hippocampal and cortical resources
 * HOW: Monitors ATP/fatigue; modulates recall vividness, detail, integration
 *
 * BIOLOGICAL BASIS:
 * - Autobiographical memory involves hippocampus and medial temporal lobe
 * - ATP depletion reduces memory retrieval quality
 * - Fatigue impairs episodic detail recall
 * - Metabolic state affects memory reconsolidation
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_AUTOBIO_SUBSTRATE_BRIDGE_H
#define NIMCP_AUTOBIO_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_SUBSTRATE_AUTOBIO 0x1228

typedef struct {
    float recall_vividness;       /* Vividness of memory recall [0-1] */
    float detail_resolution;      /* Level of detail retrieved [0-1] */
    float temporal_accuracy;      /* Accuracy of temporal ordering [0-1] */
    float narrative_coherence;    /* Coherence of life narrative [0-1] */
    float overall_capacity;       /* Combined modulation [0-1] */
} autobio_substrate_effects_t;

typedef struct {
    bool enable_atp_modulation;
    bool enable_fatigue_modulation;
    bool enable_bio_async;
    float atp_sensitivity;
    float fatigue_sensitivity;
    float min_capacity;
} autobio_substrate_config_t;

typedef struct autobio_substrate_bridge autobio_substrate_bridge_t;

autobio_substrate_config_t autobio_substrate_default_config(void);
autobio_substrate_bridge_t* autobio_substrate_bridge_create(void* autobio, neural_substrate_t* substrate, const autobio_substrate_config_t* config);
void autobio_substrate_bridge_destroy(autobio_substrate_bridge_t* bridge);
int autobio_substrate_bridge_update(autobio_substrate_bridge_t* bridge);
int autobio_substrate_bridge_get_effects(const autobio_substrate_bridge_t* bridge, autobio_substrate_effects_t* effects);
int autobio_substrate_bridge_apply_effects(autobio_substrate_bridge_t* bridge);
int autobio_substrate_bridge_register_bio_async(autobio_substrate_bridge_t* bridge, bio_router_t* router);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_AUTOBIO_SUBSTRATE_BRIDGE_H */
