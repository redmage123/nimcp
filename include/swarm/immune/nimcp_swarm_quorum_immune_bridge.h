/**
 * @file nimcp_swarm_quorum_immune_bridge.h
 * @brief Swarm Quorum-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-18
 *
 * WHAT: Bidirectional integration between brain immune system and swarm quorum
 * WHY:  Inflammation affects quorum thresholds; failed quorums trigger immune
 * HOW:  Cytokines increase thresholds; split decisions trigger stress response
 *
 * BIOLOGICAL BASIS:
 * - Pro-inflammatory cytokines increase quorum thresholds
 * - Inflammation impairs signal integration for consensus
 * - Split decisions and failed quorums trigger stress response
 * - Successful quorum cascades release anti-inflammatory signals
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SWARM_QUORUM_IMMUNE_BRIDGE_H
#define NIMCP_SWARM_QUORUM_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/immune/nimcp_brain_immune.h"
#include "async/nimcp_bio_router.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Cytokine quorum impact factors */
#define CYTOKINE_IL1_THRESHOLD_INCREASE       0.2f
#define CYTOKINE_IL6_THRESHOLD_INCREASE       0.15f
#define CYTOKINE_TNF_THRESHOLD_INCREASE       0.3f
#define CYTOKINE_IL10_THRESHOLD_REDUCTION     0.1f

/* Inflammation quorum factors */
#define INFLAMMATION_NONE_QUORUM_FACTOR       1.0f
#define INFLAMMATION_LOCAL_QUORUM_FACTOR      1.1f
#define INFLAMMATION_REGIONAL_QUORUM_FACTOR   1.25f
#define INFLAMMATION_SYSTEMIC_QUORUM_FACTOR   1.5f
#define INFLAMMATION_STORM_QUORUM_FACTOR      2.0f

typedef struct {
    float threshold_increase;
    float signal_integration_reduction;
    float commitment_rate_reduction;
} cytokine_quorum_effects_t;

typedef struct {
    brain_inflammation_level_t current_level;
    float quorum_factor;
    float integration_efficiency;
    bool quorum_impaired;
} inflammation_quorum_state_t;

typedef struct {
    uint32_t failed_quorums;
    uint32_t split_decisions;
    bool stress_triggered;
    float il10_from_success;
} quorum_immune_modulation_t;

typedef struct {
    bool enable_cytokine_effects;
    bool enable_inflammation_effects;
    bool enable_failure_stress;
    bool enable_success_boost;
    float cytokine_sensitivity;
} swarm_quorum_immune_config_t;

typedef struct swarm_quorum_immune_bridge_struct swarm_quorum_immune_bridge_t;

int swarm_quorum_immune_default_config(swarm_quorum_immune_config_t* config);
swarm_quorum_immune_bridge_t* swarm_quorum_immune_bridge_create(
    const swarm_quorum_immune_config_t* config,
    brain_immune_system_t* immune_system,
    void* swarm_quorum);
void swarm_quorum_immune_bridge_destroy(swarm_quorum_immune_bridge_t* bridge);

int swarm_quorum_immune_apply_cytokine_effects(swarm_quorum_immune_bridge_t* bridge);
int swarm_quorum_immune_apply_inflammation_effects(swarm_quorum_immune_bridge_t* bridge);

int swarm_quorum_immune_trigger_from_failure(swarm_quorum_immune_bridge_t* bridge);
int swarm_quorum_immune_boost_from_success(swarm_quorum_immune_bridge_t* bridge);

int swarm_quorum_immune_bridge_update(swarm_quorum_immune_bridge_t* bridge, uint64_t delta_ms);

float swarm_quorum_immune_get_threshold_factor(const swarm_quorum_immune_bridge_t* bridge);
float swarm_quorum_immune_get_integration_factor(const swarm_quorum_immune_bridge_t* bridge);
bool swarm_quorum_immune_is_impaired(const swarm_quorum_immune_bridge_t* bridge);

int swarm_quorum_immune_connect_bio_async(swarm_quorum_immune_bridge_t* bridge);
int swarm_quorum_immune_disconnect_bio_async(swarm_quorum_immune_bridge_t* bridge);
bool swarm_quorum_immune_is_bio_async_connected(const swarm_quorum_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SWARM_QUORUM_IMMUNE_BRIDGE_H */
