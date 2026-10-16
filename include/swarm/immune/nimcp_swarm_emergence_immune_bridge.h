/**
 * @file nimcp_swarm_emergence_immune_bridge.h
 * @brief Swarm Emergence-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-18
 *
 * WHAT: Bidirectional integration between brain immune system and swarm emergence
 * WHY:  Inflammation suppresses emergence; tier drops trigger immune responses
 * HOW:  Cytokines block tier advancement; tier regression triggers stress
 *
 * BIOLOGICAL BASIS:
 * - Pro-inflammatory cytokines suppress tier advancement
 * - Inflammation increases coherence thresholds for emergence
 * - Tier regression triggers stress-induced inflammation
 * - High emergence tier releases anti-inflammatory signals
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SWARM_EMERGENCE_IMMUNE_BRIDGE_H
#define NIMCP_SWARM_EMERGENCE_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/immune/nimcp_brain_immune.h"
#include "async/nimcp_bio_router.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Cytokine emergence impact factors */
#define CYTOKINE_IL1_EMERGENCE_SUPPRESSION    0.3f
#define CYTOKINE_IL6_EMERGENCE_SUPPRESSION    0.2f
#define CYTOKINE_TNF_EMERGENCE_SUPPRESSION    0.4f
#define CYTOKINE_IL10_EMERGENCE_BOOST         0.2f

/* Inflammation emergence factors */
#define INFLAMMATION_NONE_EMERGENCE_FACTOR    1.0f
#define INFLAMMATION_LOCAL_EMERGENCE_FACTOR   0.95f
#define INFLAMMATION_REGIONAL_EMERGENCE_FACTOR 0.80f
#define INFLAMMATION_SYSTEMIC_EMERGENCE_FACTOR 0.60f
#define INFLAMMATION_STORM_EMERGENCE_FACTOR   0.30f

typedef struct {
    float tier_advancement_suppression;
    float coherence_threshold_increase;
    float capability_degradation;
} cytokine_emergence_effects_t;

typedef struct {
    brain_inflammation_level_t current_level;
    float emergence_factor;
    int tier_penalty;
    bool advancement_blocked;
} inflammation_emergence_state_t;

typedef struct {
    uint32_t current_tier;
    uint32_t tier_drops;
    bool stress_triggered;
    float il10_from_tier;
} emergence_immune_modulation_t;

typedef struct {
    bool enable_cytokine_effects;
    bool enable_inflammation_effects;
    bool enable_regression_stress;
    bool enable_tier_boost;
    float cytokine_sensitivity;
} swarm_emergence_immune_config_t;

typedef struct swarm_emergence_immune_bridge_struct swarm_emergence_immune_bridge_t;

int swarm_emergence_immune_default_config(swarm_emergence_immune_config_t* config);
swarm_emergence_immune_bridge_t* swarm_emergence_immune_bridge_create(
    const swarm_emergence_immune_config_t* config,
    brain_immune_system_t* immune_system,
    void* swarm_emergence);
void swarm_emergence_immune_bridge_destroy(swarm_emergence_immune_bridge_t* bridge);

int swarm_emergence_immune_apply_cytokine_effects(swarm_emergence_immune_bridge_t* bridge);
int swarm_emergence_immune_apply_inflammation_effects(swarm_emergence_immune_bridge_t* bridge);

int swarm_emergence_immune_trigger_from_regression(swarm_emergence_immune_bridge_t* bridge);
int swarm_emergence_immune_boost_from_advancement(swarm_emergence_immune_bridge_t* bridge);

int swarm_emergence_immune_bridge_update(swarm_emergence_immune_bridge_t* bridge, uint64_t delta_ms);

float swarm_emergence_immune_get_emergence_factor(const swarm_emergence_immune_bridge_t* bridge);
int swarm_emergence_immune_get_tier_penalty(const swarm_emergence_immune_bridge_t* bridge);
bool swarm_emergence_immune_is_advancement_blocked(const swarm_emergence_immune_bridge_t* bridge);

int swarm_emergence_immune_connect_bio_async(swarm_emergence_immune_bridge_t* bridge);
int swarm_emergence_immune_disconnect_bio_async(swarm_emergence_immune_bridge_t* bridge);
bool swarm_emergence_immune_is_bio_async_connected(const swarm_emergence_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SWARM_EMERGENCE_IMMUNE_BRIDGE_H */
