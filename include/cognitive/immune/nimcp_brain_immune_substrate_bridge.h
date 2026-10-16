/**
 * @file nimcp_brain_immune_substrate_bridge.h
 * @brief Bridge between Brain Immune system and neural substrate
 *
 * WHAT: Links brain immune coordination to metabolic/energy state
 * WHY: Immune responses require substantial metabolic resources
 * HOW: Monitors ATP/fatigue; modulates response strength, antibody production, memory
 *
 * BIOLOGICAL BASIS:
 * - Immune activation is highly metabolically demanding
 * - ATP depletion reduces immune response strength
 * - Fatigue impairs antibody production and immune memory formation
 * - Metabolic stress leads to immune suppression
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#ifndef NIMCP_BRAIN_IMMUNE_SUBSTRATE_BRIDGE_H
#define NIMCP_BRAIN_IMMUNE_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_SUBSTRATE_BRAIN_IMMUNE 0x1308

typedef struct {
    float response_strength;       /* Immune response strength [0-1] */
    float antibody_production;     /* Antibody production capacity [0-1] */
    float memory_formation;        /* Immune memory formation [0-1] */
    float cytokine_regulation;     /* Cytokine regulation capacity [0-1] */
    float overall_capacity;        /* Combined modulation [0-1] */
} brain_immune_substrate_effects_t;

typedef struct {
    bool enable_atp_modulation;
    bool enable_fatigue_modulation;
    bool enable_bio_async;
    float atp_sensitivity;
    float fatigue_sensitivity;
    float min_capacity;
} brain_immune_substrate_config_t;

typedef struct brain_immune_substrate_bridge brain_immune_substrate_bridge_t;

brain_immune_substrate_config_t brain_immune_substrate_default_config(void);
brain_immune_substrate_bridge_t* brain_immune_substrate_bridge_create(void* brain_immune, neural_substrate_t* substrate, const brain_immune_substrate_config_t* config);
void brain_immune_substrate_bridge_destroy(brain_immune_substrate_bridge_t* bridge);
int brain_immune_substrate_bridge_update(brain_immune_substrate_bridge_t* bridge);
int brain_immune_substrate_bridge_get_effects(const brain_immune_substrate_bridge_t* bridge, brain_immune_substrate_effects_t* effects);
int brain_immune_substrate_bridge_apply_effects(brain_immune_substrate_bridge_t* bridge);
int brain_immune_substrate_bridge_register_bio_async(brain_immune_substrate_bridge_t* bridge, bio_router_t* router);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_IMMUNE_SUBSTRATE_BRIDGE_H */
