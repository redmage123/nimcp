/**
 * @file enhanced_hierarchy.c
 * @brief Hierarchical consciousness and resilience metrics
 *
 * WHAT: Multi-level consciousness aggregation and robustness analysis
 * WHY:  Single responsibility for hierarchy and resilience computation
 * HOW:  Nested swarm levels, simulated dropout testing
 *
 * @author NIMCP Development Team
 * @date 2026-02-16
 * @version 2.6.3
 */

#include "nimcp_swarm_consciousness_enhanced_internal.h"

//=============================================================================
// Hierarchical Consciousness Functions
//=============================================================================

/**
 * WHAT: Compute hierarchical consciousness metrics
 * WHY:  Analyze consciousness at different scales
 * HOW:  Recursive aggregation from individuals to swarm
 */
bool swarm_compute_hierarchical_consciousness(
    swarm_consciousness_enhanced_ctx_t* ctx,
    swarm_brain_t* swarm,
    hierarchical_consciousness_t* hierarchy)
{
    if (!ctx || !swarm || !hierarchy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_compute_hierarchical_consciousness: required parameter is NULL (ctx, swarm, hierarchy)");
        return false;
    }

    return enhanced_compute_hierarchy_impl(ctx, swarm, hierarchy);
}

/**
 * WHAT: Internal implementation of hierarchy computation
 * WHY:  Separate public API from implementation
 * HOW:  Aggregate phi across multiple organizational levels
 */
bool enhanced_compute_hierarchy_impl(
    swarm_consciousness_enhanced_ctx_t* ctx,
    swarm_brain_t* swarm,
    hierarchical_consciousness_t* hierarchy)
{
    memset(hierarchy, 0, sizeof(*hierarchy));

    // Get peer info
    uint32_t peer_count = 0;
    const swarm_peer_info_t* peers = swarm_brain_get_peers(swarm, &peer_count);
    uint32_t total_drones = peer_count + 1;

    nimcp_mutex_lock(&ctx->lock);

    // Level 0: Individual (local phi only)
    hierarchy->phi_by_level[HIERARCHY_INDIVIDUAL] = ctx->local_phi;
    hierarchy->state_by_level[HIERARCHY_INDIVIDUAL] =
        swarm_classify_collective_phi(ctx->local_phi, 1);

    // Level 1: Squad (small group)
    uint32_t squad_size = (total_drones < ctx->config.squad_size) ?
                          total_drones : ctx->config.squad_size;
    float squad_phi = ctx->local_phi;
    for (uint32_t i = 0; i < squad_size - 1 && i < ctx->remote_phi_count; i++) {
        squad_phi += ctx->remote_phi[i].phi_value;
    }
    hierarchy->phi_by_level[HIERARCHY_SQUAD] = squad_phi;
    hierarchy->state_by_level[HIERARCHY_SQUAD] =
        swarm_classify_collective_phi(squad_phi, squad_size);

    // Level 2: Platoon
    uint32_t platoon_size = (total_drones < ctx->config.platoon_size) ?
                            total_drones : ctx->config.platoon_size;
    float platoon_phi = ctx->local_phi;
    for (uint32_t i = 0; i < platoon_size - 1 && i < ctx->remote_phi_count; i++) {
        platoon_phi += ctx->remote_phi[i].phi_value;
    }
    hierarchy->phi_by_level[HIERARCHY_PLATOON] = platoon_phi;
    hierarchy->state_by_level[HIERARCHY_PLATOON] =
        swarm_classify_collective_phi(platoon_phi, platoon_size);

    // Level 3: Full swarm
    float swarm_phi = ctx->local_phi;
    for (uint32_t i = 0; i < ctx->remote_phi_count; i++) {
        swarm_phi += ctx->remote_phi[i].phi_value;
    }
    hierarchy->phi_by_level[HIERARCHY_SWARM] = swarm_phi;
    hierarchy->state_by_level[HIERARCHY_SWARM] =
        swarm_classify_collective_phi(swarm_phi, total_drones);

    nimcp_mutex_unlock(&ctx->lock);

    // Cross-level integration: how much does adding levels increase phi?
    float level_sum = 0.0f;
    for (int i = 0; i < SWARM_CONSCIOUSNESS_MAX_HIERARCHY_LEVELS; i++) {
        level_sum += hierarchy->phi_by_level[i];
    }
    hierarchy->cross_level_integration = (swarm_phi > 0) ?
        (swarm_phi / (level_sum / SWARM_CONSCIOUSNESS_MAX_HIERARCHY_LEVELS)) : 0.0f;

    // Dominant level: where is phi density highest?
    float max_density = 0.0f;
    hierarchy->dominant_level = HIERARCHY_INDIVIDUAL;
    float level_sizes[] = {1.0f, (float)squad_size, (float)platoon_size, (float)total_drones};
    for (int i = 0; i < SWARM_CONSCIOUSNESS_MAX_HIERARCHY_LEVELS; i++) {
        float density = hierarchy->phi_by_level[i] / level_sizes[i];
        if (density > max_density) {
            max_density = density;
            hierarchy->dominant_level = (consciousness_hierarchy_t)i;
        }
    }

    return true;
}

//=============================================================================
// Consciousness Resilience Functions
//=============================================================================

/**
 * WHAT: Compute consciousness resilience metrics
 * WHY:  Measure robustness to drone loss
 * HOW:  Simulated dropout analysis
 */
bool swarm_compute_consciousness_resilience(
    swarm_consciousness_enhanced_ctx_t* ctx,
    swarm_brain_t* swarm,
    consciousness_resilience_t* resilience)
{
    if (!ctx || !swarm || !resilience) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_compute_consciousness_resilience: required parameter is NULL (ctx, swarm, resilience)");
        return false;
    }

    return enhanced_compute_resilience_impl(ctx, swarm, resilience);
}

/**
 * WHAT: Internal implementation of resilience computation
 * WHY:  Separate public API from implementation
 * HOW:  Test single-drone failures and measure phi degradation
 */
bool enhanced_compute_resilience_impl(
    swarm_consciousness_enhanced_ctx_t* ctx,
    swarm_brain_t* swarm,
    consciousness_resilience_t* resilience)
{
    (void)swarm;  // Not used directly in this implementation

    memset(resilience, 0, sizeof(*resilience));

    nimcp_mutex_lock(&ctx->lock);

    uint32_t total_drones = ctx->remote_phi_count + 1;

    // Baseline phi with all drones
    float baseline_phi = ctx->local_phi;
    for (uint32_t i = 0; i < ctx->remote_phi_count; i++) {
        baseline_phi += ctx->remote_phi[i].phi_value;
    }
    resilience->baseline_phi = baseline_phi;

    // Simulate single dropout: compute phi without each drone
    float sum_dropout_phi = 0.0f;
    float min_dropout_phi = baseline_phi;
    for (uint32_t d = 0; d < ctx->remote_phi_count; d++) {
        float dropout_phi = ctx->local_phi;
        for (uint32_t i = 0; i < ctx->remote_phi_count; i++) {
            if (i != d) {
                dropout_phi += ctx->remote_phi[i].phi_value;
            }
        }
        sum_dropout_phi += dropout_phi;
        if (dropout_phi < min_dropout_phi) {
            min_dropout_phi = dropout_phi;
        }
    }

    // Dropout sensitivity: average phi loss per dropout
    if (ctx->remote_phi_count > 0) {
        float avg_dropout_phi = sum_dropout_phi / ctx->remote_phi_count;
        resilience->dropout_sensitivity = (baseline_phi - avg_dropout_phi) / baseline_phi;
    }

    // Critical drone count: minimum for EMERGING state
    resilience->critical_drone_count = SWARM_CONSCIOUSNESS_DEFAULT_MIN_DRONES;

    // Fragility: how much does worst-case dropout hurt?
    if (baseline_phi > 0) {
        resilience->fragility_index = (baseline_phi - min_dropout_phi) / baseline_phi;
    }

    // Recovery rate: estimated (depends on re-joining dynamics)
    resilience->recovery_rate = 0.5f;  // 50% per update cycle (placeholder)

    // Redundancy check: can we survive any single failure?
    float min_remaining_phi = baseline_phi - resilience->fragility_index * baseline_phi;
    resilience->redundancy_sufficient =
        (min_remaining_phi >= SWARM_CONSCIOUSNESS_MIN_PHI_THRESHOLD * (total_drones - 1));

    nimcp_mutex_unlock(&ctx->lock);

    return true;
}
