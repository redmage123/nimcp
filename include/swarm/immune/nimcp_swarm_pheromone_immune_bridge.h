/**
 * @file nimcp_swarm_pheromone_immune_bridge.h
 * @brief Swarm Pheromone-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-18
 *
 * WHAT: Bidirectional integration between brain immune system and swarm pheromone
 * WHY:  Inflammation affects pheromone sensing; contamination triggers immune
 * HOW:  Cytokines reduce sensing threshold; contamination triggers antigen response
 *
 * BIOLOGICAL BASIS:
 * - Pro-inflammatory cytokines reduce pheromone sensing sensitivity
 * - Inflammation increases evaporation rate and reduces gradient detection
 * - Contaminated pheromone patterns trigger immune response
 * - Clean pheromone paths release anti-inflammatory signals
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SWARM_PHEROMONE_IMMUNE_BRIDGE_H
#define NIMCP_SWARM_PHEROMONE_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/immune/nimcp_brain_immune.h"
#include "async/nimcp_bio_router.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Cytokine pheromone impact factors */
#define CYTOKINE_IL1_SENSING_REDUCTION        -0.2f
#define CYTOKINE_IL6_SENSING_REDUCTION        -0.15f
#define CYTOKINE_TNF_SENSING_REDUCTION        -0.3f
#define CYTOKINE_IL10_SENSING_BOOST            0.15f

/* Inflammation pheromone factors */
#define INFLAMMATION_NONE_PHEROMONE_FACTOR    1.0f
#define INFLAMMATION_LOCAL_PHEROMONE_FACTOR   0.95f
#define INFLAMMATION_REGIONAL_PHEROMONE_FACTOR 0.85f
#define INFLAMMATION_SYSTEMIC_PHEROMONE_FACTOR 0.70f
#define INFLAMMATION_STORM_PHEROMONE_FACTOR   0.50f

typedef struct {
    float sensing_threshold_increase;
    float evaporation_rate_increase;
    float gradient_detection_reduction;
} cytokine_pheromone_effects_t;

typedef struct {
    brain_inflammation_level_t current_level;
    float pheromone_factor;
    float sensing_efficiency;
    bool gradient_impaired;
} inflammation_pheromone_state_t;

typedef struct {
    uint32_t contamination_events;
    float contamination_level;
    bool immune_activated;
    float cleanup_signal;
} pheromone_immune_modulation_t;

typedef struct {
    bool enable_cytokine_effects;
    bool enable_inflammation_effects;
    bool enable_contamination_detection;
    float cytokine_sensitivity;
} swarm_pheromone_immune_config_t;

typedef struct swarm_pheromone_immune_bridge_struct swarm_pheromone_immune_bridge_t;

int swarm_pheromone_immune_default_config(swarm_pheromone_immune_config_t* config);
swarm_pheromone_immune_bridge_t* swarm_pheromone_immune_bridge_create(
    const swarm_pheromone_immune_config_t* config,
    brain_immune_system_t* immune_system,
    void* swarm_pheromone);
void swarm_pheromone_immune_bridge_destroy(swarm_pheromone_immune_bridge_t* bridge);

int swarm_pheromone_immune_apply_cytokine_effects(swarm_pheromone_immune_bridge_t* bridge);
int swarm_pheromone_immune_apply_inflammation_effects(swarm_pheromone_immune_bridge_t* bridge);

int swarm_pheromone_immune_report_contamination(swarm_pheromone_immune_bridge_t* bridge, float level);
int swarm_pheromone_immune_boost_from_clean_path(swarm_pheromone_immune_bridge_t* bridge);

int swarm_pheromone_immune_bridge_update(swarm_pheromone_immune_bridge_t* bridge, uint64_t delta_ms);

float swarm_pheromone_immune_get_sensing_factor(const swarm_pheromone_immune_bridge_t* bridge);
float swarm_pheromone_immune_get_evaporation_factor(const swarm_pheromone_immune_bridge_t* bridge);
bool swarm_pheromone_immune_is_gradient_impaired(const swarm_pheromone_immune_bridge_t* bridge);

int swarm_pheromone_immune_connect_bio_async(swarm_pheromone_immune_bridge_t* bridge);
int swarm_pheromone_immune_disconnect_bio_async(swarm_pheromone_immune_bridge_t* bridge);
bool swarm_pheromone_immune_is_bio_async_connected(const swarm_pheromone_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SWARM_PHEROMONE_IMMUNE_BRIDGE_H */
