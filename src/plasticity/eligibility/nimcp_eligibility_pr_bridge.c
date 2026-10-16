#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_eligibility_pr_bridge.c - Eligibility Traces ↔ Prime Resonant Bridge
//=============================================================================
/**
 * @file nimcp_eligibility_pr_bridge.c
 * @brief Implementation of bidirectional eligibility-PR memory integration
 * @version 1.0.0
 * @date 2026-01-12
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "plasticity/eligibility/nimcp_eligibility_pr_bridge.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/memory/nimcp_memory.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "utils/logging/nimcp_logging.h"
#include "security/nimcp_bbb_helpers.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(eligibility_pr_bridge)

/* Security integration */
#define LOG_MODULE "ELIGIBILITY_PR_BRIDGE"


//=============================================================================
// Internal Structure
//=============================================================================

struct elig_pr_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    elig_pr_bridge_config_t config;
    elig_pr_bridge_state_t state;
    elig_pr_bridge_stats_t stats;
    bool connected;
    float lambda_sum;
    uint32_t lambda_count;
};

BRIDGE_DEFINE_SECURITY_SETTERS_TYPE(elig_pr_bridge, struct elig_pr_bridge_struct)

//=============================================================================
// Configuration Functions
//=============================================================================

elig_pr_bridge_config_t elig_pr_bridge_default_config(void) {
    elig_pr_bridge_config_t config = {
        /* Consolidation gating */
        .consolidation_boost_max = ELIG_PR_CONSOLIDATION_BOOST_MAX,
        .eligibility_threshold = ELIG_PR_ELIGIBILITY_THRESHOLD,
        .reward_threshold = ELIG_PR_REWARD_THRESHOLD,
        .enable_consolidation_gate = true,

        /* Decay modulation */
        .decay_consolidation_boost = ELIG_PR_DECAY_CONSOL_BOOST,
        .decay_min_lambda = ELIG_PR_DECAY_MIN_LAMBDA,
        .decay_max_lambda = ELIG_PR_DECAY_MAX_LAMBDA,
        .enable_decay_modulation = true,

        /* Tier parameters */
        .tier_lambdas = {
            ELIG_PR_TIER_Z0_LAMBDA,
            ELIG_PR_TIER_Z1_LAMBDA,
            ELIG_PR_TIER_Z2_LAMBDA,
            ELIG_PR_TIER_Z3_LAMBDA
        },
        .enable_tier_modulation = true,

        /* Resonance modulation */
        .resonance_elig_boost = ELIG_PR_RESONANCE_ELIG_BOOST,
        .enable_resonance_boost = true,

        /* Bio-async */
        .enable_bio_async = false
    };
    return config;
}

bool elig_pr_bridge_validate_config(const elig_pr_bridge_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_pr_bridge_validate_config: config is NULL");
        return false;
    }

    if (config->consolidation_boost_max < 0.0f ||
        config->consolidation_boost_max > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_pr_bridge_validate_config: config is NULL");
        return false;
    }

    if (config->eligibility_threshold < 0.0f ||
        config->eligibility_threshold > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_pr_bridge_validate_config: config is NULL");
        return false;
    }

    if (config->reward_threshold < 0.0f ||
        config->reward_threshold > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "elig_pr_bridge_validate_config: operation failed");
        return false;
    }

    if (config->decay_min_lambda < 0.0f ||
        config->decay_max_lambda > 1.0f ||
        config->decay_min_lambda > config->decay_max_lambda) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "elig_pr_bridge_validate_config: operation failed");
        return false;
    }

    for (int i = 0; i < ELIG_PR_TIER_COUNT; i++) {
        if (config->tier_lambdas[i] < 0.0f ||
            config->tier_lambdas[i] > 1.0f) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "elig_pr_bridge_validate_config: operation failed");
            return false;
        }
    }

    if (config->resonance_elig_boost < 0.0f) {
        return false;
    }

    return true;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

elig_pr_bridge_t elig_pr_bridge_create(const elig_pr_bridge_config_t* config) {
    if (!config || !elig_pr_bridge_validate_config(config)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_pr_bridge_create: required parameter is NULL (config)");
        return NULL;
    }

    elig_pr_bridge_t bridge = (elig_pr_bridge_t)nimcp_calloc(1, sizeof(struct elig_pr_bridge_struct));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "elig_pr_bridge_create: bridge allocation failed");
        return NULL;
    }

    if (bridge_base_init(&bridge->base, 0, "eligibility_pr") != 0) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "elig_pr_bridge_create: bridge_base_init failed");
        return NULL;
    }

    bridge->config = *config;
    bridge->connected = true;
    bridge->state.current_tier = ELIG_PR_TIER_Z0;
    bridge->state.bridge_coherence = 1.0f;

    NIMCP_LOGGING_INFO("Created %s bridge", "eligibility_pr");
    return bridge;
}

void elig_pr_bridge_destroy(elig_pr_bridge_t bridge) {
    if (bridge) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
    }
}

bool elig_pr_bridge_is_connected(elig_pr_bridge_t bridge) {
    return bridge ? bridge->connected : false;
}

//=============================================================================
// Forward Direction: Eligibility → Prime Resonant
//=============================================================================

int elig_pr_apply_consolidation_gate(elig_pr_bridge_t bridge,
                                     uint64_t node_id,
                                     float eligibility,
                                     float reward_signal,
                                     elig_pr_forward_effect_t* effect) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_pr_apply_consolidation_gate: bridge is NULL");
        return -1;
    }
    if (!effect) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_pr_apply_consolidation_gate: effect is NULL");
        return -1;
    }

    memset(effect, 0, sizeof(elig_pr_forward_effect_t));
    effect->node_id = node_id;
    effect->eligibility = eligibility;
    effect->reward_signal = reward_signal;

    bridge->stats.forward_calls++;

    if (!bridge->config.enable_consolidation_gate) {
        return 0;
    }

    /* Check thresholds for consolidation */
    if (eligibility >= bridge->config.eligibility_threshold &&
        reward_signal >= bridge->config.reward_threshold) {

        /* Compute consolidation delta: eligibility × reward × max_boost */
        float gate_signal = eligibility * reward_signal;
        effect->consolidation_delta = gate_signal * bridge->config.consolidation_boost_max;

        /* Clamp to max boost */
        if (effect->consolidation_delta > bridge->config.consolidation_boost_max) {
            effect->consolidation_delta = bridge->config.consolidation_boost_max;
        }

        /* Update state */
        bridge->state.cumulative_consol_delta += effect->consolidation_delta;
        bridge->state.current_consolidation += effect->consolidation_delta;
        if (bridge->state.current_consolidation > 1.0f) {
            bridge->state.current_consolidation = 1.0f;
        }

        bridge->stats.consolidation_events++;
        bridge->stats.total_consolidation_delta += effect->consolidation_delta;

        /* Entanglement boost for co-eligible nodes */
        effect->entanglement_boost = eligibility * 0.1f;
    }

    return 0;
}

int elig_pr_check_tier_promotion(elig_pr_bridge_t bridge,
                                 uint64_t node_id,
                                 float eligibility,
                                 float reward_signal,
                                 bool* should_promote) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_pr_check_tier_promotion: bridge is NULL");
        return -1;
    }
    if (!should_promote) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_pr_check_tier_promotion: should_promote is NULL");
        return -1;
    }

    *should_promote = false;
    (void)node_id; /* Used for logging/tracking in full implementation */

    if (!bridge->config.enable_consolidation_gate) {
        return 0;
    }

    /* Strong eligibility + strong reward triggers tier promotion */
    float promotion_threshold = 0.7f;
    if (eligibility >= promotion_threshold &&
        reward_signal >= promotion_threshold) {

        /* Check if consolidation is high enough */
        if (bridge->state.current_consolidation >= 0.8f) {
            *should_promote = true;
            bridge->stats.tier_promotions++;
            bridge->state.tier_promotions++;

            /* Advance tier if possible */
            if (bridge->state.current_tier < ELIG_PR_TIER_Z3) {
                bridge->state.current_tier++;
            }
        }
    }

    return 0;
}

int elig_pr_apply_entanglement_update(elig_pr_bridge_t bridge,
                                      uint64_t source_id, uint64_t target_id,
                                      float eligibility,
                                      float* entangle_delta) {
    BRIDGE_BBB_VALIDATE(bridge, entangle_delta, sizeof(*entangle_delta));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_pr_apply_entanglement_update: bridge is NULL");
        return -1;
    }
    if (!entangle_delta) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_pr_apply_entanglement_update: entangle_delta is NULL");
        return -1;
    }

    (void)source_id;
    (void)target_id;

    /* Entanglement strengthens based on eligibility */
    *entangle_delta = eligibility * 0.05f;

    /* Boost if both source and target have high consolidation */
    if (bridge->state.current_consolidation > 0.5f) {
        *entangle_delta *= (1.0f + bridge->state.current_consolidation * 0.5f);
    }

    return 0;
}

//=============================================================================
// Backward Direction: Prime Resonant → Eligibility
//=============================================================================

int elig_pr_get_decay_modulation(elig_pr_bridge_t bridge,
                                 float consolidation,
                                 float base_lambda,
                                 float* modulated_lambda) {
    BRIDGE_BBB_VALIDATE(bridge, modulated_lambda, sizeof(*modulated_lambda));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_pr_get_decay_modulation: bridge is NULL");
        return -1;
    }
    if (!modulated_lambda) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_pr_get_decay_modulation: modulated_lambda is NULL");
        return -1;
    }

    bridge->stats.backward_calls++;

    if (!bridge->config.enable_decay_modulation) {
        *modulated_lambda = base_lambda;
        return 0;
    }

    /* Higher consolidation → slower decay (higher lambda) */
    /* λ_effective = λ_base × (1 + consolidation × boost) */
    float boost = consolidation * bridge->config.decay_consolidation_boost;
    *modulated_lambda = base_lambda * (1.0f + boost);

    /* Clamp to valid range */
    if (*modulated_lambda < bridge->config.decay_min_lambda) {
        *modulated_lambda = bridge->config.decay_min_lambda;
    }
    if (*modulated_lambda > bridge->config.decay_max_lambda) {
        *modulated_lambda = bridge->config.decay_max_lambda;
    }

    bridge->stats.decay_modulations++;

    /* Track for average */
    bridge->lambda_sum += *modulated_lambda;
    bridge->lambda_count++;
    bridge->stats.avg_effective_lambda = bridge->lambda_sum / (float)bridge->lambda_count;

    return 0;
}

int elig_pr_get_tier_parameters(elig_pr_bridge_t bridge,
                                elig_pr_memory_tier_t tier,
                                float* lambda,
                                float* sensitivity) {
    BRIDGE_BBB_VALIDATE(bridge, lambda, sizeof(*lambda));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }

    if (tier >= ELIG_PR_TIER_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "elig_pr_bridge_is_connected: capacity exceeded");
        return -1;
    }

    if (lambda) {
        if (bridge->config.enable_tier_modulation) {
            *lambda = bridge->config.tier_lambdas[tier];
        } else {
            *lambda = 0.95f; /* Default */
        }
    }

    if (sensitivity) {
        /* Higher tiers = lower sensitivity (more stable) */
        /* Z0: 1.0, Z1: 0.75, Z2: 0.5, Z3: 0.25 */
        *sensitivity = 1.0f - (tier * 0.25f);
    }

    return 0;
}

int elig_pr_apply_resonance_boost(elig_pr_bridge_t bridge,
                                  float resonance,
                                  float base_eligibility,
                                  float* boosted_eligibility) {
    BRIDGE_BBB_VALIDATE(bridge, boosted_eligibility, sizeof(*boosted_eligibility));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_pr_apply_resonance_boost: bridge is NULL");
        return -1;
    }
    if (!boosted_eligibility) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_pr_apply_resonance_boost: boosted_eligibility is NULL");
        return -1;
    }

    if (!bridge->config.enable_resonance_boost) {
        *boosted_eligibility = base_eligibility;
        return 0;
    }

    /* Resonance amplifies eligibility */
    /* e_boosted = e_base × (1 + resonance × boost_factor) */
    float boost = resonance * bridge->config.resonance_elig_boost;
    *boosted_eligibility = base_eligibility * (1.0f + boost);

    /* Clamp to [0, 1] */
    if (*boosted_eligibility > 1.0f) {
        *boosted_eligibility = 1.0f;
    }

    bridge->state.current_resonance = resonance;
    bridge->stats.resonance_boosts++;

    return 0;
}

int elig_pr_compute_modulation(elig_pr_bridge_t bridge,
                               float consolidation, float resonance,
                               elig_pr_memory_tier_t tier,
                               float base_lambda,
                               elig_pr_backward_effect_t* effect) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_pr_compute_modulation: bridge is NULL");
        return -1;
    }
    if (!effect) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_pr_compute_modulation: effect is NULL");
        return -1;
    }

    memset(effect, 0, sizeof(elig_pr_backward_effect_t));
    effect->consolidation_level = consolidation;
    effect->resonance_score = resonance;
    effect->tier = tier;

    /* Get tier-based lambda */
    float tier_lambda;
    float sensitivity;
    if (elig_pr_get_tier_parameters(bridge, tier, &tier_lambda, &sensitivity) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: validation failed");
        return -1;
    }

    /* Apply decay modulation from consolidation */
    float base = (base_lambda > 0.0f) ? base_lambda : tier_lambda;
    if (elig_pr_get_decay_modulation(bridge, consolidation, base,
                                     &effect->effective_lambda) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: validation failed");
        return -1;
    }

    /* Apply resonance boost (to a hypothetical base eligibility of 0.5) */
    float boosted_elig;
    if (elig_pr_apply_resonance_boost(bridge, resonance, 0.5f, &boosted_elig) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: validation failed");
        return -1;
    }
    effect->eligibility_boost = boosted_elig - 0.5f;

    /* Learning rate modulation based on tier and consolidation */
    /* Lower tiers + low consolidation = higher learning rate */
    float tier_factor = 1.0f - (tier * 0.2f);
    float consol_factor = 1.0f - (consolidation * 0.5f);
    effect->learning_rate_mod = tier_factor * consol_factor;

    /* Update bridge state */
    bridge->state.current_consolidation = consolidation;
    bridge->state.current_resonance = resonance;
    bridge->state.current_tier = tier;

    return 0;
}

//=============================================================================
// State and Statistics
//=============================================================================

int elig_pr_bridge_get_state(elig_pr_bridge_t bridge, elig_pr_bridge_state_t* state) {
    BRIDGE_BBB_VALIDATE(bridge, state, sizeof(*state));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_pr_bridge_get_state: bridge is NULL");
        return -1;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_pr_bridge_get_state: state is NULL");
        return -1;
    }

    *state = bridge->state;
    return 0;
}

int elig_pr_bridge_get_stats(elig_pr_bridge_t bridge, elig_pr_bridge_stats_t* stats) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_pr_bridge_get_stats: bridge is NULL");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "elig_pr_bridge_get_stats: stats is NULL");
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}

int elig_pr_bridge_reset_stats(elig_pr_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    memset(&bridge->stats, 0, sizeof(elig_pr_bridge_stats_t));
    bridge->lambda_sum = 0.0f;
    bridge->lambda_count = 0;

    return 0;
}

int elig_pr_bridge_update(elig_pr_bridge_t bridge, float dt_ms) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    (void)dt_ms;

    /* Update coherence based on how well eligibility and consolidation align */
    /* High eligibility should correlate with increasing consolidation */
    float alignment = 1.0f;
    if (bridge->stats.forward_calls > 0) {
        float avg_consol = bridge->stats.total_consolidation_delta /
                           (float)bridge->stats.forward_calls;
        /* Coherence based on consolidation activity */
        alignment = 0.5f + (avg_consol * 2.5f);
        if (alignment > 1.0f) alignment = 1.0f;
    }

    /* Smooth update */
    bridge->state.bridge_coherence = 0.9f * bridge->state.bridge_coherence +
                                     0.1f * alignment;

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

float elig_pr_bridge_get_coherence(elig_pr_bridge_t bridge) {
    return bridge ? bridge->state.bridge_coherence : 0.0f;
}

void elig_pr_bridge_print_summary(elig_pr_bridge_t bridge) {
    if (!bridge) {
        printf("Eligibility-PR Bridge: NULL\n");
        return;
    }

    printf("=== Eligibility-PR Bridge Summary ===\n");
    printf("State:\n");
    printf("  Current consolidation: %.3f\n", bridge->state.current_consolidation);
    printf("  Current resonance:     %.3f\n", bridge->state.current_resonance);
    printf("  Current tier:          Z%d\n", bridge->state.current_tier);
    printf("  Tier promotions:       %u\n", bridge->state.tier_promotions);
    printf("  Bridge coherence:      %.3f\n", bridge->state.bridge_coherence);
    printf("\nStatistics:\n");
    printf("  Forward calls:         %lu\n", (unsigned long)bridge->stats.forward_calls);
    printf("  Backward calls:        %lu\n", (unsigned long)bridge->stats.backward_calls);
    printf("  Consolidation events:  %lu\n", (unsigned long)bridge->stats.consolidation_events);
    printf("  Tier promotions:       %lu\n", (unsigned long)bridge->stats.tier_promotions);
    printf("  Decay modulations:     %lu\n", (unsigned long)bridge->stats.decay_modulations);
    printf("  Resonance boosts:      %lu\n", (unsigned long)bridge->stats.resonance_boosts);
    printf("  Total consol delta:    %.4f\n", bridge->stats.total_consolidation_delta);
    printf("  Avg effective lambda:  %.4f\n", bridge->stats.avg_effective_lambda);
    printf("=====================================\n");
}
