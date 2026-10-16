/**
 * @file nimcp_hippocampus_substrate_bridge.h
 * @brief Bridge between Hippocampus and neural substrate
 *
 * WHAT: Links hippocampal memory function to metabolic state
 * WHY: Hippocampus is critical for memory and highly metabolically active
 * HOW: Monitors ATP/fatigue; modulates encoding, consolidation, retrieval
 *
 * BIOLOGICAL BASIS:
 * - Hippocampus requires sustained energy for LTP/memory formation
 * - ATP depletion impairs memory encoding
 * - Fatigue reduces memory consolidation quality
 * - Metabolic stress affects spatial navigation and episodic memory
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_HIPPOCAMPUS_SUBSTRATE_BRIDGE_H
#define NIMCP_HIPPOCAMPUS_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_SUBSTRATE_HIPPOCAMPUS 0x1234

typedef struct {
    float encoding_capacity;      /* Memory encoding quality [0-1] */
    float consolidation_rate;     /* Memory consolidation rate [0-1] */
    float retrieval_accuracy;     /* Retrieval accuracy [0-1] */
    float spatial_processing;     /* Spatial navigation [0-1] */
    float overall_capacity;       /* Combined modulation [0-1] */
} hippocampus_substrate_effects_t;

typedef struct {
    bool enable_atp_modulation;
    bool enable_fatigue_modulation;
    bool enable_bio_async;
    float atp_sensitivity;
    float fatigue_sensitivity;
    float min_capacity;
} hippocampus_substrate_config_t;

typedef struct hippocampus_substrate_bridge hippocampus_substrate_bridge_t;

hippocampus_substrate_config_t hippocampus_substrate_default_config(void);
hippocampus_substrate_bridge_t* hippocampus_substrate_bridge_create(void* hippocampus, neural_substrate_t* substrate, const hippocampus_substrate_config_t* config);
void hippocampus_substrate_bridge_destroy(hippocampus_substrate_bridge_t* bridge);
int hippocampus_substrate_bridge_update(hippocampus_substrate_bridge_t* bridge);
int hippocampus_substrate_bridge_get_effects(const hippocampus_substrate_bridge_t* bridge, hippocampus_substrate_effects_t* effects);
int hippocampus_substrate_bridge_apply_effects(hippocampus_substrate_bridge_t* bridge);
int hippocampus_substrate_bridge_register_bio_async(hippocampus_substrate_bridge_t* bridge, bio_router_t* router);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HIPPOCAMPUS_SUBSTRATE_BRIDGE_H */
