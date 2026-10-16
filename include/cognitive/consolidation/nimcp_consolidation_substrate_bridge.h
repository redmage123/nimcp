/**
 * @file nimcp_consolidation_substrate_bridge.h
 * @brief Bridge between Memory Consolidation system and neural substrate
 *
 * WHAT: Links memory consolidation to metabolic/energy state
 * WHY: Memory consolidation is ATP-intensive, requiring high metabolic resources
 * HOW: Monitors ATP/fatigue; modulates consolidation rate, fidelity, and priority
 *
 * BIOLOGICAL BASIS:
 * - Memory consolidation involves hippocampus-cortex transfer during sleep
 * - ATP depletion impairs synaptic potentiation and protein synthesis
 * - Fatigue reduces consolidation fidelity and increases forgetting
 * - Metabolic stress prioritizes essential memories over incidental ones
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_CONSOLIDATION_SUBSTRATE_BRIDGE_H
#define NIMCP_CONSOLIDATION_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_SUBSTRATE_CONSOLIDATION 0x1301

typedef struct {
    float consolidation_rate;      /* Rate of memory consolidation [0-1] */
    float encoding_fidelity;       /* Fidelity of memory encoding [0-1] */
    float priority_threshold;      /* Threshold for memory priority [0-1] */
    float protein_synthesis;       /* Protein synthesis capacity [0-1] */
    float overall_capacity;        /* Combined modulation [0-1] */
} consolidation_substrate_effects_t;

typedef struct {
    bool enable_atp_modulation;
    bool enable_fatigue_modulation;
    bool enable_bio_async;
    float atp_sensitivity;
    float fatigue_sensitivity;
    float min_capacity;
} consolidation_substrate_config_t;

typedef struct consolidation_substrate_bridge consolidation_substrate_bridge_t;

consolidation_substrate_config_t consolidation_substrate_default_config(void);
consolidation_substrate_bridge_t* consolidation_substrate_bridge_create(void* consolidation, neural_substrate_t* substrate, const consolidation_substrate_config_t* config);
void consolidation_substrate_bridge_destroy(consolidation_substrate_bridge_t* bridge);
int consolidation_substrate_bridge_update(consolidation_substrate_bridge_t* bridge);
int consolidation_substrate_bridge_get_effects(const consolidation_substrate_bridge_t* bridge, consolidation_substrate_effects_t* effects);
int consolidation_substrate_bridge_apply_effects(consolidation_substrate_bridge_t* bridge);
int consolidation_substrate_bridge_register_bio_async(consolidation_substrate_bridge_t* bridge, bio_router_t* router);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CONSOLIDATION_SUBSTRATE_BRIDGE_H */
