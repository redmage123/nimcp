/**
 * @file nimcp_vesicle_packaging.c
 * @brief Implementation of synaptic vesicle packaging and release dynamics (Phase C2.3 Enhancement #3)
 *
 * WHAT: Implements three-pool vesicle model with quantal release
 * WHY:  Model short-term plasticity (facilitation & depression)
 * HOW:  Binomial release, pool refilling, Ca²⁺-dependent facilitation
 *
 * BIOLOGICAL BASIS:
 * - Rizzoli & Betz (2005): Three-pool model (RRP, recycling, reserve)
 * - del Castillo & Katz (1954): Quantal release hypothesis
 * - Zucker & Regehr (2002): Short-term synaptic plasticity
 *
 * @version Phase C2.3 Enhancement #3
 * @date 2025-11-13
 */

#include "plasticity/neuromodulators/nimcp_vesicle_packaging.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/validation/nimcp_common.h"
#include "security/nimcp_security.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

#define LOG_MODULE "plasticity_vesicle_packaging"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(vesicle_packaging)

// ============================================================================
// Private Helper Functions
// ============================================================================

/**
 * @brief Generate uniform random number [0, 1]
 *
 * WHAT: Simple LCG random number generator (thread-safe)
 * WHY:  Need fast random for binomial vesicle release
 * HOW:  Linear congruential generator with thread-local seed
 *
 * Thread safety: Uses __thread storage to prevent race conditions
 * when multiple threads call this function concurrently.
 */
static inline float random_uniform(void) {
    static __thread uint32_t seed = 123456789;
    seed = (NIMCP_LCG_MULTIPLIER * seed + NIMCP_LCG_INCREMENT) & 0x7fffffff;
    return (float)seed / (float)0x7fffffff;
}

// ============================================================================
// Default Configurations
// ============================================================================

vesicle_pool_config_t vesicle_pool_get_default_config(void) {
    vesicle_pool_config_t config = {
        // Initial pool sizes
        .initial_rrp = VESICLE_DEFAULT_RRP_SIZE,
        .initial_recycling = VESICLE_DEFAULT_RECYCLING_SIZE,
        .initial_reserve = VESICLE_DEFAULT_RESERVE_SIZE,

        // Capacity limits
        .rrp_capacity = VESICLE_DEFAULT_RRP_SIZE,
        .recycling_capacity = VESICLE_DEFAULT_RECYCLING_SIZE,
        .reserve_capacity = VESICLE_DEFAULT_RESERVE_SIZE,

        // Release parameters
        .base_release_probability = VESICLE_DEFAULT_RELEASE_PROBABILITY,
        .quantal_size = VESICLE_DEFAULT_QUANTAL_SIZE,

        // Kinetics
        .refill_rate = VESICLE_DEFAULT_REFILL_RATE,
        .mobilization_rate = VESICLE_DEFAULT_MOBILIZATION_RATE,

        // Facilitation parameters
        .ca_decay_tau = VESICLE_CALCIUM_DECAY_TAU,
        .facilitation_gain = VESICLE_FACILITATION_GAIN,

        // Feature flags
        .enable_facilitation = true,
        .enable_depression = true,
        .enable_reserve_mobilization = true
    };
    return config;
}

// ============================================================================
// Initialization
// ============================================================================

void vesicle_pool_init(vesicle_pool_state_t* pool) {
    if (!pool) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vesicle_pool_init: null pool pointer");
        return;
    }
    vesicle_pool_config_t default_config = vesicle_pool_get_default_config();
    vesicle_pool_init_with_config(pool, &default_config);
}

void vesicle_pool_init_with_config(
    vesicle_pool_state_t* pool,
    const vesicle_pool_config_t* config
) {
    if (!pool) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vesicle_pool_init_with_config: null pool pointer");
        return;
    }
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vesicle_pool_init_with_config: null config pointer");
        return;
    }

    // Zero all memory
    memset(pool, 0, sizeof(*pool));

    // === Initialize Vesicle Pools ===
    pool->readily_releasable_pool = config->initial_rrp;
    pool->recycling_pool = config->initial_recycling;
    pool->reserve_pool = config->initial_reserve;

    // === Set Capacity Limits ===
    pool->rrp_capacity = config->rrp_capacity;
    pool->recycling_capacity = config->recycling_capacity;
    pool->reserve_capacity = config->reserve_capacity;

    // === Release Parameters ===
    pool->release_probability = config->base_release_probability;
    pool->facilitated_pr = config->base_release_probability;
    pool->vesicle_quantal_size = config->quantal_size;

    // === Refill Dynamics ===
    pool->refill_rate = config->refill_rate;
    pool->mobilization_rate = config->mobilization_rate;

    // === Facilitation State ===
    pool->calcium_residual = 0.0F;
    pool->ca_decay_tau = config->ca_decay_tau;
    pool->facilitation_gain = config->facilitation_gain;

    // === Depletion State ===
    pool->is_depleted = false;
    pool->depletion_factor = 1.0F;

    // === Statistics ===
    pool->total_releases = 0;
    pool->total_depleted_events = 0;
    pool->total_refills = 0;
    pool->avg_release_per_spike = 0.0F;

    // === Timing ===
    pool->last_release_time_us = 0;
    pool->last_refill_time_us = 0;
}

void vesicle_pool_reset(vesicle_pool_state_t* pool) {
    if (!pool) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vesicle_pool_reset: null pool pointer");
        return;
    }

    // Restore pools to full capacity
    pool->readily_releasable_pool = pool->rrp_capacity;
    pool->recycling_pool = pool->recycling_capacity;
    pool->reserve_pool = pool->reserve_capacity;

    // Reset accumulators
    pool->refill_accumulator = 0.0F;
    pool->mobilization_accumulator = 0.0F;

    // Reset facilitation
    pool->calcium_residual = 0.0F;
    pool->facilitated_pr = pool->release_probability;

    // Reset depletion
    pool->is_depleted = false;
    pool->depletion_factor = 1.0F;

    // Don't reset statistics - they track lifetime performance
}

// ============================================================================
// Release Dynamics - Vesicle Fusion
// ============================================================================

float vesicle_pool_release(
    vesicle_pool_state_t* pool,
    bool action_potential,
    uint64_t current_time_us
) {
    if (!pool) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vesicle_pool_release: null pool pointer");
        return 0.0F;
    }

    // === Guard: No action potential ===
    if (!action_potential) {
        return 0.0F;
    }

    // === Guard: Depleted state ===
    if (pool->is_depleted) {
        return 0.0F;
    }

    // === 1. Binomial Release from RRP ===
    // For each vesicle in RRP, release with probability facilitated_pr
    uint32_t vesicles_released = 0;
    for (uint32_t i = 0; i < pool->readily_releasable_pool; i++) {
        if (random_uniform() < pool->facilitated_pr) {
            vesicles_released++;
        }
    }

    // === 2. Update RRP and Recycling Pool ===
    // Released vesicles leave RRP and enter recycling (endocytosis)
    pool->readily_releasable_pool -= vesicles_released;

    // Add to recycling pool (respecting capacity)
    uint32_t recycling_space = pool->recycling_capacity - pool->recycling_pool;
    uint32_t vesicles_to_recycle = (vesicles_released < recycling_space) ?
                                     vesicles_released : recycling_space;
    pool->recycling_pool += vesicles_to_recycle;

    // === 3. Update Calcium Residual (for facilitation) ===
    pool->calcium_residual += 1.0F;

    // === 4. Check Depletion ===
    if (pool->readily_releasable_pool < VESICLE_DEPLETION_THRESHOLD) {
        pool->is_depleted = true;
        pool->total_depleted_events++;
    }

    // Update depletion factor
    pool->depletion_factor = (pool->rrp_capacity > 0) ?
        (float)pool->readily_releasable_pool / (float)pool->rrp_capacity : 0.0f;

    // === 5. Update Statistics ===
    pool->total_releases++;
    float new_avg = (pool->avg_release_per_spike * (pool->total_releases - 1) + vesicles_released) / pool->total_releases;
    pool->avg_release_per_spike = new_avg;
    pool->last_release_time_us = current_time_us;

    // === 6. Return Total Molecules Released ===
    return (float)vesicles_released * pool->vesicle_quantal_size;
}

void vesicle_pool_update_facilitation(vesicle_pool_state_t* pool, float dt) {
    if (!pool) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vesicle_pool_update_facilitation: null pool pointer");
        return;
    }

    // === 1. Exponential Decay of Calcium Residual ===
    // Ca_residual(t) = Ca_residual(0) * exp(-t/τ)
    float decay_alpha = expf(-dt / pool->ca_decay_tau);
    pool->calcium_residual *= decay_alpha;

    // Clamp to zero when very small
    if (pool->calcium_residual < 0.001F) {
        pool->calcium_residual = 0.0F;
    }

    // === 2. Update Facilitated Release Probability ===
    // Pr_facilitated = Pr_base * (1 + gain * Ca_residual)
    pool->facilitated_pr = pool->release_probability *
                          (1.0F + pool->facilitation_gain * pool->calcium_residual);

    // Clamp to [0, 1]
    pool->facilitated_pr = nimcp_clampf(pool->facilitated_pr, 0.0F, 1.0F);
}

// ============================================================================
// Refill Dynamics - Pool Recovery
// ============================================================================

void vesicle_pool_refill(vesicle_pool_state_t* pool, float dt) {
    if (!pool) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vesicle_pool_refill: null pool pointer");
        return;
    }

    // === 1. Accumulate Fractional Vesicles ===
    // Rate is in vesicles/second, dt is in seconds
    pool->refill_accumulator += pool->refill_rate * dt;

    // === 2. Always Update Depletion Factor Based on Current RRP ===
    pool->depletion_factor = (pool->rrp_capacity > 0) ?
        (float)pool->readily_releasable_pool / (float)pool->rrp_capacity : 0.0f;

    // === 3. Extract Integer Vesicles to Transfer ===
    uint32_t vesicles_to_transfer = (uint32_t)pool->refill_accumulator;

    // Guard: No vesicles ready yet
    if (vesicles_to_transfer == 0) {
        return;
    }

    // === 4. Limit by Recycling Pool Availability ===
    uint32_t available_in_recycling = pool->recycling_pool;
    if (vesicles_to_transfer > available_in_recycling) {
        vesicles_to_transfer = available_in_recycling;
    }

    // === 5. Limit by RRP Capacity ===
    uint32_t rrp_space = pool->rrp_capacity - pool->readily_releasable_pool;
    if (vesicles_to_transfer > rrp_space) {
        vesicles_to_transfer = rrp_space;
    }

    // Guard: No space or resources available
    if (vesicles_to_transfer == 0) {
        return;
    }

    // === 6. Transfer Vesicles: Recycling → RRP ===
    pool->recycling_pool -= vesicles_to_transfer;
    pool->readily_releasable_pool += vesicles_to_transfer;

    // === 7. Update Accumulator (subtract transferred) ===
    pool->refill_accumulator -= (float)vesicles_to_transfer;

    // === 8. Update Statistics ===
    pool->total_refills += vesicles_to_transfer;

    // === 9. Check Recovery from Depletion ===
    if (pool->readily_releasable_pool >= (VESICLE_DEPLETION_THRESHOLD + 2)) {
        pool->is_depleted = false;
    }

    // Depletion factor already updated at beginning of function
}

void vesicle_pool_mobilize_reserve(vesicle_pool_state_t* pool, float dt) {
    if (!pool) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vesicle_pool_mobilize_reserve: null pool pointer");
        return;
    }

    // === 1. Accumulate Fractional Vesicles ===
    pool->mobilization_accumulator += pool->mobilization_rate * dt;

    // === 2. Extract Integer Vesicles to Mobilize ===
    uint32_t vesicles_to_mobilize = (uint32_t)pool->mobilization_accumulator;

    // Guard: No vesicles ready yet
    if (vesicles_to_mobilize == 0) {
        return;
    }

    // === 3. Limit by Reserve Pool Availability ===
    uint32_t available_in_reserve = pool->reserve_pool;
    if (vesicles_to_mobilize > available_in_reserve) {
        vesicles_to_mobilize = available_in_reserve;
    }

    // === 4. Limit by Recycling Pool Capacity ===
    uint32_t recycling_space = pool->recycling_capacity - pool->recycling_pool;
    if (vesicles_to_mobilize > recycling_space) {
        vesicles_to_mobilize = recycling_space;
    }

    // Guard: No space or resources available
    if (vesicles_to_mobilize == 0) {
        return;
    }

    // === 5. Transfer Vesicles: Reserve → Recycling ===
    pool->reserve_pool -= vesicles_to_mobilize;
    pool->recycling_pool += vesicles_to_mobilize;

    // === 6. Update Accumulator (subtract transferred) ===
    pool->mobilization_accumulator -= (float)vesicles_to_mobilize;
}

void vesicle_pool_update(vesicle_pool_state_t* pool, float dt) {
    if (!pool) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vesicle_pool_update: null pool pointer");
        return;
    }

    // === 1. Refill RRP from Recycling ===
    vesicle_pool_refill(pool, dt);

    // === 2. Mobilize Reserve to Recycling ===
    vesicle_pool_mobilize_reserve(pool, dt);

    // === 3. Update Facilitation ===
    vesicle_pool_update_facilitation(pool, dt);
}

// ============================================================================
// Statistics & Monitoring
// ============================================================================

void vesicle_pool_get_stats(
    const vesicle_pool_state_t* pool,
    uint32_t* rrp_count,
    uint32_t* recycling_count,
    uint32_t* reserve_count,
    float* depletion_fraction,
    float* facilitated_pr
) {
    if (!pool) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vesicle_pool_get_stats: null pool pointer");
        return;
    }

    if (rrp_count) *rrp_count = pool->readily_releasable_pool;
    if (recycling_count) *recycling_count = pool->recycling_pool;
    if (reserve_count) *reserve_count = pool->reserve_pool;
    if (depletion_fraction) *depletion_fraction = 1.0F - pool->depletion_factor;
    if (facilitated_pr) *facilitated_pr = pool->facilitated_pr;
}

bool vesicle_pool_is_depleted(const vesicle_pool_state_t* pool) {
    if (!pool) {
        return false;
    }
    return pool->is_depleted;
}

float vesicle_pool_get_avg_release(const vesicle_pool_state_t* pool) {
    if (!pool) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vesicle_pool_get_avg_release: null pool pointer");
        return 0.0F;
    }
    return pool->avg_release_per_spike;
}

// ============================================================================
// Pharmacological Interventions
// ============================================================================

void vesicle_pool_apply_botulinum(vesicle_pool_state_t* pool, float blockade) {
    if (!pool) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vesicle_pool_apply_botulinum: null pool pointer");
        return;
    }

    // WHAT: Botulinum toxin cleaves SNAP-25, preventing vesicle fusion
    // WHY:  Clinical use (cosmetic Botox, muscle spasms, hyperhidrosis)
    // HOW:  Reduce release probability to near-zero

    // Clamp blockade to [0, 1]
    blockade = nimcp_clampf(blockade, 0.0F, 1.0F);

    // Reduce Pr: 1.0 blockade = 99% reduction (leave 1% residual)
    pool->release_probability *= (1.0F - 0.99F * blockade);
    pool->facilitated_pr *= (1.0F - 0.99F * blockade);

    // Ensure minimum Pr
    if (pool->release_probability < 0.001F) {
        pool->release_probability = 0.001F;
    }
    if (pool->facilitated_pr < 0.001F) {
        pool->facilitated_pr = 0.001F;
    }
}

void vesicle_pool_apply_amphetamine(vesicle_pool_state_t* pool, float depletion) {
    if (!pool) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vesicle_pool_apply_amphetamine: null pool pointer");
        return;
    }

    // WHAT: Amphetamine causes reverse transport, depleting vesicle stores
    // WHY:  Model psychostimulant effects (Adderall, methamphetamine)
    // HOW:  Rapidly deplete RRP and recycling pools

    // Clamp depletion to [0, 1]
    depletion = nimcp_clampf(depletion, 0.0F, 1.0F);

    // Deplete RRP to 10% of current
    pool->readily_releasable_pool = (uint32_t)((1.0F - 0.9F * depletion) * pool->readily_releasable_pool);

    // Deplete recycling to 30% of current
    pool->recycling_pool = (uint32_t)((1.0F - 0.7F * depletion) * pool->recycling_pool);

    // Update depletion state
    if (pool->readily_releasable_pool < VESICLE_DEPLETION_THRESHOLD) {
        pool->is_depleted = true;
        pool->total_depleted_events++;
    }
    pool->depletion_factor = (pool->rrp_capacity > 0) ?
        (float)pool->readily_releasable_pool / (float)pool->rrp_capacity : 0.0f;
}

void vesicle_pool_apply_4ap(vesicle_pool_state_t* pool, float potentiation) {
    if (!pool) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vesicle_pool_apply_4ap: null pool pointer");
        return;
    }

    // WHAT: 4-aminopyridine blocks K⁺ channels, prolonging Ca²⁺ influx
    // WHY:  Experimental tool to study synaptic transmission, Lambert-Eaton syndrome treatment
    // HOW:  Increase release probability

    // Clamp potentiation to [1, 3] (up to 3x increase)
    potentiation = nimcp_clampf(potentiation, 1.0F, 3.0F);

    // Increase Pr
    pool->release_probability *= potentiation;
    pool->facilitated_pr *= potentiation;

    // Clamp to [0, 1]
    pool->release_probability = nimcp_clampf(pool->release_probability, 0.0F, 1.0F);
    pool->facilitated_pr = nimcp_clampf(pool->facilitated_pr, 0.0F, 1.0F);
}
