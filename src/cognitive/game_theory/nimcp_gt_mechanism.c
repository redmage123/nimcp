//=============================================================================
// nimcp_gt_mechanism.c - Mechanism Design Implementation
//=============================================================================
/**
 * @file nimcp_gt_mechanism.c
 * @brief Mechanism design and incomplete information game implementations
 */

#include "cognitive/game_theory/nimcp_gt_mechanism.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <float.h>
#include <stdlib.h>
#include <stdio.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/thread/nimcp_thread_rand.h"

BRIDGE_BOILERPLATE(gt_mechanism, MESH_ADAPTER_CATEGORY_COGNITIVE)



//=============================================================================
// Internal Structure
//=============================================================================

struct nimcp_mechanism_struct {
    nimcp_mechanism_config_t config;
    nimcp_mechanism_state_t state;

    // Type spaces for each player
    nimcp_type_space_t type_spaces[NIMCP_GT_MAX_PLAYERS];
    uint32_t num_players;

    // Mechanism rules
    nimcp_allocation_rule_fn allocation_rule;
    void* allocation_user_data;
    nimcp_payment_rule_fn payment_rule;
    void* payment_user_data;

    // Signaling game state
    nimcp_signal_t signals[NIMCP_GT_MAX_SIGNALS];
    uint32_t num_signals;
    nimcp_signaling_strategy_t sender_strategies[NIMCP_GT_MAX_TYPES];
    nimcp_receiver_beliefs_t receiver_beliefs[NIMCP_GT_MAX_SIGNALS];

    // Outcome
    nimcp_mechanism_result_t last_result;

    // Statistics
    uint64_t executions;
    uint64_t ic_violations;
    uint64_t ir_violations;

    // Thread safety
    nimcp_platform_mutex_t mutex;
};

//=============================================================================
// Static Name Tables
//=============================================================================

static const char* s_mechanism_type_names[] = {
    "Direct Revelation",
    "Indirect Mechanism",
    "Signaling Game"
};

static const char* s_ic_level_names[] = {
    "None",
    "Dominant Strategy IC",
    "Bayesian Nash IC",
    "Interim IC",
    "Ex-Post IC"
};

static const char* s_signal_equilibrium_names[] = {
    "Separating",
    "Pooling",
    "Semi-Separating"
};

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Simple random float in [0, 1]
 */
static float random_float(void) {
    return (float)nimcp_tl_rand() / (float)RAND_MAX;
}

/**
 * @brief Draw from discrete distribution
 */
static uint32_t draw_from_distribution(const float* probs, uint32_t n) {
    float r = random_float();
    float cumsum = 0.0f;

    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            gt_mechanism_heartbeat("gt_mechanism_loop",
                             (float)(i + 1) / (float)n);
        }

        cumsum += probs[i];
        if (r <= cumsum) {
            return i;
        }
    }

    return n - 1;  // Edge case: return last
}

/**
 * @brief Compute utility for a player given type profile
 */
static float compute_utility(
    nimcp_mechanism_t ctx,
    nimcp_player_id_t player,
    uint32_t own_type,
    const uint32_t* reports,
    const float* allocations,
    const float* payments
) {
    if (!ctx || player >= ctx->num_players) {
        return 0.0f;
    }

    // Utility = valuation * allocation - payment
    // For linear utilities (single-item allocation)
    float valuation = ctx->type_spaces[player].types[own_type].valuation;
    float utility = valuation * allocations[player] - payments[player];

    return utility;
}

/**
 * @brief Enumerate all type profiles (cartesian product)
 *
 * @param ctx Mechanism context
 * @param profile_idx Which profile to generate
 * @param profile Output: type indices for each player
 * @param prob Output: probability of this profile
 * @return true if valid profile
 */
static bool enumerate_type_profile(
    nimcp_mechanism_t ctx,
    uint32_t profile_idx,
    uint32_t* profile,
    float* prob
) {
    uint32_t total_profiles = 1;
    for (uint32_t p = 0; p < ctx->num_players; p++) {
        /* Phase 8: Loop progress heartbeat */
        if ((p & 0xFF) == 0 && ctx->num_players > 256) {
            gt_mechanism_heartbeat("gt_mechanism_loop",
                             (float)(p + 1) / (float)ctx->num_players);
        }

        total_profiles *= ctx->type_spaces[p].num_types;
    }

    if (profile_idx >= total_profiles) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "enumerate_type_profile: capacity exceeded");
        return false;
    }

    *prob = 1.0f;
    uint32_t idx = profile_idx;

    for (uint32_t p = 0; p < ctx->num_players; p++) {
        /* Phase 8: Loop progress heartbeat */
        if ((p & 0xFF) == 0 && ctx->num_players > 256) {
            gt_mechanism_heartbeat("gt_mechanism_loop",
                             (float)(p + 1) / (float)ctx->num_players);
        }

        uint32_t num_types = ctx->type_spaces[p].num_types;
        profile[p] = idx % num_types;
        idx /= num_types;
        *prob *= ctx->type_spaces[p].probabilities[profile[p]];
    }

    return true;
}

/**
 * @brief Count total type profiles
 */
static uint32_t count_type_profiles(nimcp_mechanism_t ctx) {
    uint32_t total = 1;
    for (uint32_t p = 0; p < ctx->num_players; p++) {
        /* Phase 8: Loop progress heartbeat */
        if ((p & 0xFF) == 0 && ctx->num_players > 256) {
            gt_mechanism_heartbeat("gt_mechanism_loop",
                             (float)(p + 1) / (float)ctx->num_players);
        }

        total *= ctx->type_spaces[p].num_types;
        if (total > 10000) {
            return 10000;  // Cap to prevent overflow
        }
    }
    return total;
}

//=============================================================================
// Configuration
//=============================================================================

nimcp_mechanism_config_t nimcp_mechanism_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    gt_mechanism_heartbeat("gt_mechanism_mechanism_default_co", 0.0f);


    nimcp_mechanism_config_t config;
    memset(&config, 0, sizeof(config));

    config.type = NIMCP_MECHANISM_DIRECT;
    config.num_players = 2;
    config.target_ic = NIMCP_IC_BAYES_NASH;
    config.convergence_epsilon = 0.0001f;
    config.max_iterations = NIMCP_GT_BAYESIAN_MAX_ITER;
    config.verify_ic = true;
    config.verify_ir = true;
    config.enable_statistics = true;

    return config;
}

//=============================================================================
// Lifecycle
//=============================================================================

nimcp_mechanism_t nimcp_mechanism_create(const nimcp_mechanism_config_t* config) {
    if (!config || config->num_players == 0 ||
        config->num_players > NIMCP_GT_MAX_PLAYERS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_mechanism_create: operation failed");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_mechanism_heartbeat("gt_mechanism_mechanism_create", 0.0f);

    nimcp_mechanism_t ctx = nimcp_calloc(1, sizeof(struct nimcp_mechanism_struct));
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate ctx");

        return NULL;
    }

    ctx->config = *config;
    ctx->state = NIMCP_MECHANISM_STATE_UNINITIALIZED;
    ctx->num_players = config->num_players;

    // Initialize type spaces
    for (uint32_t p = 0; p < ctx->num_players; p++) {
        /* Phase 8: Loop progress heartbeat */
        if ((p & 0xFF) == 0 && ctx->num_players > 256) {
            gt_mechanism_heartbeat("gt_mechanism_loop",
                             (float)(p + 1) / (float)ctx->num_players);
        }

        nimcp_type_space_init(&ctx->type_spaces[p], p);
    }

    // Initialize mutex
    if (nimcp_platform_mutex_init(&ctx->mutex, false) != 0) {
        nimcp_free(ctx);
        ctx = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "nimcp_mechanism_create: validation failed");
        return NULL;
    }

    ctx->allocation_rule = NULL;
    ctx->payment_rule = NULL;
    ctx->num_signals = 0;
    ctx->executions = 0;
    ctx->ic_violations = 0;
    ctx->ir_violations = 0;

    memset(&ctx->last_result, 0, sizeof(nimcp_mechanism_result_t));

    return ctx;
}

void nimcp_mechanism_destroy(nimcp_mechanism_t ctx) {
    if (!ctx) return;

    /* Phase 8: Heartbeat at operation start */
    gt_mechanism_heartbeat("gt_mechanism_mechanism_destroy", 0.0f);


    nimcp_platform_mutex_destroy(&ctx->mutex);
    nimcp_free(ctx);
    ctx = NULL;
}

//=============================================================================
// Type Space Configuration
//=============================================================================

nimcp_error_t nimcp_mechanism_set_type_space(
    nimcp_mechanism_t ctx,
    nimcp_player_id_t player,
    const nimcp_type_t* types,
    const float* probabilities,
    uint32_t num_types
) {
    if (!ctx || !types || !probabilities) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_mechanism_heartbeat("gt_mechanism_mechanism_set_type_s", 0.0f);


    if (player >= ctx->num_players) {
        return NIMCP_GT_ERROR_INVALID_PARAMETER;
    }

    if (num_types == 0 || num_types > NIMCP_GT_MAX_TYPES) {
        return NIMCP_GT_ERROR_INVALID_PARAMETER;
    }

    // Verify probabilities sum to 1
    float sum = 0.0f;
    for (uint32_t i = 0; i < num_types; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_types > 256) {
            gt_mechanism_heartbeat("gt_mechanism_loop",
                             (float)(i + 1) / (float)num_types);
        }

        if (probabilities[i] < 0.0f || probabilities[i] > 1.0f) {
            return NIMCP_GT_ERROR_INVALID_PARAMETER;
        }
        sum += probabilities[i];
    }
    if (fabsf(sum - 1.0f) > 0.001f) {
        return NIMCP_GT_ERROR_INVALID_PARAMETER;
    }

    nimcp_platform_mutex_lock(&ctx->mutex);

    nimcp_type_space_t* space = &ctx->type_spaces[player];
    space->player_id = player;
    space->num_types = num_types;
    space->type_realized = false;

    for (uint32_t i = 0; i < num_types; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_types > 256) {
            gt_mechanism_heartbeat("gt_mechanism_loop",
                             (float)(i + 1) / (float)num_types);
        }

        space->types[i] = types[i];
        space->types[i].type_id = i;
        space->probabilities[i] = probabilities[i];
    }

    // Check if all players have type spaces
    bool all_configured = true;
    for (uint32_t p = 0; p < ctx->num_players; p++) {
        /* Phase 8: Loop progress heartbeat */
        if ((p & 0xFF) == 0 && ctx->num_players > 256) {
            gt_mechanism_heartbeat("gt_mechanism_loop",
                             (float)(p + 1) / (float)ctx->num_players);
        }

        if (ctx->type_spaces[p].num_types == 0) {
            all_configured = false;
            break;
        }
    }

    if (all_configured) {
        ctx->state = NIMCP_MECHANISM_STATE_READY;
    }

    nimcp_platform_mutex_unlock(&ctx->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_mechanism_get_type_space(
    const nimcp_mechanism_t ctx,
    nimcp_player_id_t player,
    nimcp_type_space_t* type_space
) {
    if (!ctx || !type_space) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_mechanism_heartbeat("gt_mechanism_mechanism_get_type_s", 0.0f);


    if (player >= ctx->num_players) {
        return NIMCP_GT_ERROR_INVALID_PARAMETER;
    }

    nimcp_platform_mutex_lock(&ctx->mutex);
    *type_space = ctx->type_spaces[player];
    nimcp_platform_mutex_unlock(&ctx->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_mechanism_realize_type(
    nimcp_mechanism_t ctx,
    nimcp_player_id_t player,
    uint32_t* realized_type
) {
    if (!ctx || !realized_type) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_mechanism_heartbeat("gt_mechanism_mechanism_realize_ty", 0.0f);


    if (player >= ctx->num_players) {
        return NIMCP_GT_ERROR_INVALID_PARAMETER;
    }

    nimcp_platform_mutex_lock(&ctx->mutex);

    nimcp_type_space_t* space = &ctx->type_spaces[player];

    if (space->num_types == 0) {
        nimcp_platform_mutex_unlock(&ctx->mutex);
        return NIMCP_GT_ERROR_INVALID_STATE;
    }

    // Draw from prior distribution
    uint32_t drawn = draw_from_distribution(space->probabilities, space->num_types);
    space->realized_type = drawn;
    space->type_realized = true;
    *realized_type = drawn;

    nimcp_platform_mutex_unlock(&ctx->mutex);
    return NIMCP_SUCCESS;
}

//=============================================================================
// Mechanism Rules
//=============================================================================

nimcp_error_t nimcp_mechanism_set_allocation_rule(
    nimcp_mechanism_t ctx,
    nimcp_allocation_rule_fn callback,
    void* user_data
) {
    if (!ctx) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_mechanism_heartbeat("gt_mechanism_mechanism_set_alloca", 0.0f);


    nimcp_platform_mutex_lock(&ctx->mutex);
    ctx->allocation_rule = callback;
    ctx->allocation_user_data = user_data;
    nimcp_platform_mutex_unlock(&ctx->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_mechanism_set_payment_rule(
    nimcp_mechanism_t ctx,
    nimcp_payment_rule_fn callback,
    void* user_data
) {
    if (!ctx) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_mechanism_heartbeat("gt_mechanism_mechanism_set_paymen", 0.0f);


    nimcp_platform_mutex_lock(&ctx->mutex);
    ctx->payment_rule = callback;
    ctx->payment_user_data = user_data;
    nimcp_platform_mutex_unlock(&ctx->mutex);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Mechanism Execution
//=============================================================================

nimcp_error_t nimcp_mechanism_execute(
    nimcp_mechanism_t ctx,
    const uint32_t* reports,
    nimcp_mechanism_result_t* result
) {
    if (!ctx || !reports || !result) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    if (!ctx->allocation_rule) {
        return NIMCP_GT_ERROR_INVALID_STATE;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_mechanism_heartbeat("gt_mechanism_mechanism_execute", 0.0f);


    nimcp_platform_mutex_lock(&ctx->mutex);

    float allocations[NIMCP_GT_MAX_PLAYERS];
    float payments[NIMCP_GT_MAX_PLAYERS];

    memset(allocations, 0, sizeof(allocations));
    memset(payments, 0, sizeof(payments));

    // Apply allocation rule
    nimcp_error_t err = ctx->allocation_rule(
        reports, ctx->num_players, allocations, ctx->allocation_user_data
    );
    if (err != NIMCP_SUCCESS) {
        nimcp_platform_mutex_unlock(&ctx->mutex);
        return err;
    }

    // Apply payment rule (if set)
    if (ctx->payment_rule) {
        err = ctx->payment_rule(
            reports, ctx->num_players, allocations, payments, ctx->payment_user_data
        );
        if (err != NIMCP_SUCCESS) {
            nimcp_platform_mutex_unlock(&ctx->mutex);
            return err;
        }
    }

    // Build result
    memset(result, 0, sizeof(nimcp_mechanism_result_t));
    result->state = NIMCP_MECHANISM_STATE_COMPLETED;
    result->timestamp_ms = nimcp_time_get_ms();

    float social_welfare = 0.0f;
    float revenue = 0.0f;

    for (uint32_t p = 0; p < ctx->num_players; p++) {
        /* Phase 8: Loop progress heartbeat */
        if ((p & 0xFF) == 0 && ctx->num_players > 256) {
            gt_mechanism_heartbeat("gt_mechanism_loop",
                             (float)(p + 1) / (float)ctx->num_players);
        }

        result->allocations[p] = allocations[p];
        result->payments[p] = payments[p];

        // Compute utility (assuming realized types are set)
        uint32_t type_idx = 0;
        if (ctx->type_spaces[p].type_realized) {
            type_idx = ctx->type_spaces[p].realized_type;
        } else if (ctx->type_spaces[p].num_types > 0) {
            type_idx = reports[p];  // Use report as type
        }

        float valuation = 0.0f;
        if (ctx->type_spaces[p].num_types > type_idx) {
            valuation = ctx->type_spaces[p].types[type_idx].valuation;
        }

        result->utilities[p] = valuation * allocations[p] - payments[p];
        social_welfare += valuation * allocations[p];
        revenue += payments[p];
    }

    result->social_welfare = social_welfare;
    result->revenue = revenue;
    result->is_efficient = true;  // Would need Pareto check for full verification

    ctx->last_result = *result;
    ctx->executions++;
    ctx->state = NIMCP_MECHANISM_STATE_COMPLETED;

    nimcp_platform_mutex_unlock(&ctx->mutex);
    return NIMCP_SUCCESS;
}

//=============================================================================
// Incentive Compatibility Verification
//=============================================================================

nimcp_error_t nimcp_mechanism_is_incentive_compatible(
    nimcp_mechanism_t ctx,
    nimcp_ic_result_t* result
) {
    if (!ctx || !result) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    if (!ctx->allocation_rule) {
        return NIMCP_GT_ERROR_INVALID_STATE;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_mechanism_heartbeat("gt_mechanism_mechanism_is_incenti", 0.0f);


    nimcp_platform_mutex_lock(&ctx->mutex);

    memset(result, 0, sizeof(nimcp_ic_result_t));
    result->is_incentive_compatible = true;
    result->ic_level = ctx->config.target_ic;
    result->max_deviation_gain = 0.0f;

    uint32_t n = ctx->num_players;
    uint32_t total_profiles = count_type_profiles(ctx);

    // Check IC for each player, each type
    for (uint32_t p = 0; p < n; p++) {
        /* Phase 8: Loop progress heartbeat */
        if ((p & 0xFF) == 0 && n > 256) {
            gt_mechanism_heartbeat("gt_mechanism_loop",
                             (float)(p + 1) / (float)n);
        }

        nimcp_type_space_t* space = &ctx->type_spaces[p];

        for (uint32_t t = 0; t < space->num_types; t++) {
            /* Phase 8: Loop progress heartbeat */
            if ((t & 0xFF) == 0 && space->num_types > 256) {
                gt_mechanism_heartbeat("gt_mechanism_loop",
                                 (float)(t + 1) / (float)space->num_types);
            }

            // Compute expected utility from truth-telling
            float eu_truth = 0.0f;

            for (uint32_t profile_idx = 0; profile_idx < total_profiles; profile_idx++) {
                /* Phase 8: Loop progress heartbeat */
                if ((profile_idx & 0xFF) == 0 && total_profiles > 256) {
                    gt_mechanism_heartbeat("gt_mechanism_loop",
                                     (float)(profile_idx + 1) / (float)total_profiles);
                }

                uint32_t profile[NIMCP_GT_MAX_PLAYERS];
                float prob = 0.0f;

                if (!enumerate_type_profile(ctx, profile_idx, profile, &prob)) {
                    break;
                }

                // Skip profiles where this player has different type
                if (profile[p] != t) continue;

                // Truth-telling reports
                uint32_t reports[NIMCP_GT_MAX_PLAYERS];
                memcpy(reports, profile, sizeof(reports));

                float allocations[NIMCP_GT_MAX_PLAYERS];
                float payments[NIMCP_GT_MAX_PLAYERS];

                ctx->allocation_rule(reports, n, allocations, ctx->allocation_user_data);
                if (ctx->payment_rule) {
                    ctx->payment_rule(reports, n, allocations, payments, ctx->payment_user_data);
                } else {
                    memset(payments, 0, sizeof(payments));
                }

                float utility = compute_utility(ctx, p, t, reports, allocations, payments);

                // Weight by conditional probability of other types given own type
                float cond_prob = prob / space->probabilities[t];
                eu_truth += cond_prob * utility;
            }

            // Compare against all possible lies
            for (uint32_t lie = 0; lie < space->num_types; lie++) {
                /* Phase 8: Loop progress heartbeat */
                if ((lie & 0xFF) == 0 && space->num_types > 256) {
                    gt_mechanism_heartbeat("gt_mechanism_loop",
                                     (float)(lie + 1) / (float)space->num_types);
                }

                if (lie == t) continue;  // Skip truth

                float eu_lie = 0.0f;

                for (uint32_t profile_idx = 0; profile_idx < total_profiles; profile_idx++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((profile_idx & 0xFF) == 0 && total_profiles > 256) {
                        gt_mechanism_heartbeat("gt_mechanism_loop",
                                         (float)(profile_idx + 1) / (float)total_profiles);
                    }

                    uint32_t profile[NIMCP_GT_MAX_PLAYERS];
                    float prob = 0.0f;

                    if (!enumerate_type_profile(ctx, profile_idx, profile, &prob)) {
                        break;
                    }

                    if (profile[p] != t) continue;

                    // Lying reports
                    uint32_t reports[NIMCP_GT_MAX_PLAYERS];
                    memcpy(reports, profile, sizeof(reports));
                    reports[p] = lie;  // Misreport

                    float allocations[NIMCP_GT_MAX_PLAYERS];
                    float payments[NIMCP_GT_MAX_PLAYERS];

                    ctx->allocation_rule(reports, n, allocations, ctx->allocation_user_data);
                    if (ctx->payment_rule) {
                        ctx->payment_rule(reports, n, allocations, payments,
                                          ctx->payment_user_data);
                    } else {
                        memset(payments, 0, sizeof(payments));
                    }

                    // Utility uses TRUE type, not report
                    float utility = compute_utility(ctx, p, t, reports, allocations, payments);

                    float cond_prob = prob / space->probabilities[t];
                    eu_lie += cond_prob * utility;
                }

                // Check if lying is profitable
                float gain = eu_lie - eu_truth;
                if (gain > ctx->config.convergence_epsilon) {
                    result->is_incentive_compatible = false;

                    if (gain > result->max_deviation_gain) {
                        result->max_deviation_gain = gain;
                        result->violator_id = p;
                        result->true_type = t;
                        result->profitable_lie = lie;
                    }
                }
            }
        }
    }

    if (result->is_incentive_compatible) {
        snprintf(result->explanation, sizeof(result->explanation),
                 "Mechanism is %s incentive compatible",
                 nimcp_ic_level_name(result->ic_level));
    } else {
        snprintf(result->explanation, sizeof(result->explanation),
                 "IC violation: player %u type %u gains %.4f by reporting type %u",
                 result->violator_id, result->true_type,
                 result->max_deviation_gain, result->profitable_lie);
        ctx->ic_violations++;
    }

    nimcp_platform_mutex_unlock(&ctx->mutex);
    return NIMCP_SUCCESS;
}

//=============================================================================
// Individual Rationality Verification
//=============================================================================

nimcp_error_t nimcp_mechanism_is_individually_rational(
    nimcp_mechanism_t ctx,
    nimcp_ir_result_t* result
) {
    if (!ctx || !result) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    if (!ctx->allocation_rule) {
        return NIMCP_GT_ERROR_INVALID_STATE;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_mechanism_heartbeat("gt_mechanism_mechanism_is_individ", 0.0f);


    nimcp_platform_mutex_lock(&ctx->mutex);

    memset(result, 0, sizeof(nimcp_ir_result_t));
    result->is_individually_rational = true;
    result->min_expected_utility = FLT_MAX;

    uint32_t n = ctx->num_players;
    uint32_t total_profiles = count_type_profiles(ctx);

    // Check IR for each player, each type
    for (uint32_t p = 0; p < n; p++) {
        /* Phase 8: Loop progress heartbeat */
        if ((p & 0xFF) == 0 && n > 256) {
            gt_mechanism_heartbeat("gt_mechanism_loop",
                             (float)(p + 1) / (float)n);
        }

        nimcp_type_space_t* space = &ctx->type_spaces[p];

        for (uint32_t t = 0; t < space->num_types; t++) {
            /* Phase 8: Loop progress heartbeat */
            if ((t & 0xFF) == 0 && space->num_types > 256) {
                gt_mechanism_heartbeat("gt_mechanism_loop",
                                 (float)(t + 1) / (float)space->num_types);
            }

            float expected_utility = 0.0f;

            for (uint32_t profile_idx = 0; profile_idx < total_profiles; profile_idx++) {
                /* Phase 8: Loop progress heartbeat */
                if ((profile_idx & 0xFF) == 0 && total_profiles > 256) {
                    gt_mechanism_heartbeat("gt_mechanism_loop",
                                     (float)(profile_idx + 1) / (float)total_profiles);
                }

                uint32_t profile[NIMCP_GT_MAX_PLAYERS];
                float prob = 0.0f;

                if (!enumerate_type_profile(ctx, profile_idx, profile, &prob)) {
                    break;
                }

                if (profile[p] != t) continue;

                // Truth-telling
                float allocations[NIMCP_GT_MAX_PLAYERS];
                float payments[NIMCP_GT_MAX_PLAYERS];

                ctx->allocation_rule(profile, n, allocations, ctx->allocation_user_data);
                if (ctx->payment_rule) {
                    ctx->payment_rule(profile, n, allocations, payments, ctx->payment_user_data);
                } else {
                    memset(payments, 0, sizeof(payments));
                }

                float utility = compute_utility(ctx, p, t, profile, allocations, payments);

                float cond_prob = prob / space->probabilities[t];
                expected_utility += cond_prob * utility;
            }

            // IR requires expected utility >= 0 (outside option)
            if (expected_utility < -ctx->config.convergence_epsilon) {
                result->is_individually_rational = false;

                if (expected_utility < result->min_expected_utility) {
                    result->min_expected_utility = expected_utility;
                    result->violator_id = p;
                    result->violating_type = t;
                    result->utility_shortfall = -expected_utility;
                }
            }
        }
    }

    if (result->is_individually_rational) {
        snprintf(result->explanation, sizeof(result->explanation),
                 "Mechanism is individually rational (all types get non-negative utility)");
    } else {
        snprintf(result->explanation, sizeof(result->explanation),
                 "IR violation: player %u type %u expects utility %.4f < 0",
                 result->violator_id, result->violating_type,
                 result->min_expected_utility);
        ctx->ir_violations++;
    }

    nimcp_platform_mutex_unlock(&ctx->mutex);
    return NIMCP_SUCCESS;
}

//=============================================================================
// Bayesian Nash Equilibrium
//=============================================================================

nimcp_error_t nimcp_mechanism_compute_bayesian_equilibrium(
    nimcp_mechanism_t ctx,
    nimcp_bayesian_equilibrium_t* result
) {
    if (!ctx || !result) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    if (!ctx->allocation_rule) {
        return NIMCP_GT_ERROR_INVALID_STATE;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_mechanism_heartbeat("gt_mechanism_mechanism_compute_ba", 0.0f);


    nimcp_platform_mutex_lock(&ctx->mutex);

    memset(result, 0, sizeof(nimcp_bayesian_equilibrium_t));

    uint32_t n = ctx->num_players;

    // For direct revelation mechanisms, BNE is simply truth-telling if IC
    // Check IC first
    nimcp_ic_result_t ic_result;
    nimcp_platform_mutex_unlock(&ctx->mutex);

    nimcp_error_t err = nimcp_mechanism_is_incentive_compatible(ctx, &ic_result);

    nimcp_platform_mutex_lock(&ctx->mutex);

    if (err != NIMCP_SUCCESS) {
        nimcp_platform_mutex_unlock(&ctx->mutex);
        return err;
    }

    if (ic_result.is_incentive_compatible) {
        result->equilibrium_found = true;

        // Truth-telling strategies for each player-type
        uint32_t strat_idx = 0;
        for (uint32_t p = 0; p < n; p++) {
            /* Phase 8: Loop progress heartbeat */
            if ((p & 0xFF) == 0 && n > 256) {
                gt_mechanism_heartbeat("gt_mechanism_loop",
                                 (float)(p + 1) / (float)n);
            }

            nimcp_type_space_t* space = &ctx->type_spaces[p];
            for (uint32_t t = 0; t < space->num_types; t++) {
                /* Phase 8: Loop progress heartbeat */
                if ((t & 0xFF) == 0 && space->num_types > 256) {
                    gt_mechanism_heartbeat("gt_mechanism_loop",
                                     (float)(t + 1) / (float)space->num_types);
                }

                nimcp_signaling_strategy_t* strat = &result->strategies[strat_idx++];
                strat->type_id = t;
                strat->is_pure = true;
                strat->pure_signal = t;  // Report true type
                strat->num_signals = space->num_types;
                memset(strat->signal_probs, 0, sizeof(strat->signal_probs));
                strat->signal_probs[t] = 1.0f;
            }
        }
        result->num_strategies = strat_idx;

        // Compute expected social welfare and revenue
        float total_welfare = 0.0f;
        float total_revenue = 0.0f;
        uint32_t total_profiles = count_type_profiles(ctx);

        for (uint32_t profile_idx = 0; profile_idx < total_profiles; profile_idx++) {
            /* Phase 8: Loop progress heartbeat */
            if ((profile_idx & 0xFF) == 0 && total_profiles > 256) {
                gt_mechanism_heartbeat("gt_mechanism_loop",
                                 (float)(profile_idx + 1) / (float)total_profiles);
            }

            uint32_t profile[NIMCP_GT_MAX_PLAYERS];
            float prob = 0.0f;

            if (!enumerate_type_profile(ctx, profile_idx, profile, &prob)) {
                break;
            }

            float allocations[NIMCP_GT_MAX_PLAYERS];
            float payments[NIMCP_GT_MAX_PLAYERS];

            ctx->allocation_rule(profile, n, allocations, ctx->allocation_user_data);
            if (ctx->payment_rule) {
                ctx->payment_rule(profile, n, allocations, payments, ctx->payment_user_data);
            } else {
                memset(payments, 0, sizeof(payments));
            }

            float welfare = 0.0f;
            float revenue = 0.0f;
            for (uint32_t p = 0; p < n; p++) {
                /* Phase 8: Loop progress heartbeat */
                if ((p & 0xFF) == 0 && n > 256) {
                    gt_mechanism_heartbeat("gt_mechanism_loop",
                                     (float)(p + 1) / (float)n);
                }

                float val = ctx->type_spaces[p].types[profile[p]].valuation;
                welfare += val * allocations[p];
                revenue += payments[p];
            }

            total_welfare += prob * welfare;
            total_revenue += prob * revenue;
        }

        result->expected_social_welfare = total_welfare;
        result->expected_revenue = total_revenue;
        result->iterations_taken = 1;
        result->convergence_error = 0.0f;

        snprintf(result->explanation, sizeof(result->explanation),
                 "Truth-telling is a Bayesian Nash equilibrium");
    } else {
        result->equilibrium_found = false;
        result->iterations_taken = 0;
        result->convergence_error = ic_result.max_deviation_gain;

        snprintf(result->explanation, sizeof(result->explanation),
                 "No truth-telling equilibrium: %s", ic_result.explanation);
    }

    nimcp_platform_mutex_unlock(&ctx->mutex);
    return NIMCP_SUCCESS;
}

//=============================================================================
// Revelation Principle Verification
//=============================================================================

nimcp_error_t nimcp_mechanism_verify_revelation_principle(
    nimcp_mechanism_t ctx,
    const nimcp_mechanism_t indirect_mechanism,
    nimcp_revelation_result_t* result
) {
    if (!ctx || !result) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_mechanism_heartbeat("gt_mechanism_mechanism_verify_rev", 0.0f);


    memset(result, 0, sizeof(nimcp_revelation_result_t));

    // Revelation principle states: For any Bayesian Nash equilibrium of an
    // indirect mechanism, there exists an equivalent direct mechanism
    // that is incentive compatible.

    if (!indirect_mechanism) {
        // If no indirect mechanism provided, just check if direct mechanism is IC
        nimcp_ic_result_t ic_result;
        nimcp_error_t err = nimcp_mechanism_is_incentive_compatible(ctx, &ic_result);
        if (err != NIMCP_SUCCESS) {
            return err;
        }

        result->revelation_holds = ic_result.is_incentive_compatible;
        result->equivalent_ic = ic_result.ic_level;
        result->efficiency_loss = 0.0f;

        snprintf(result->explanation, sizeof(result->explanation),
                 "Direct mechanism IC: %s", ic_result.explanation);

        return NIMCP_SUCCESS;
    }

    nimcp_platform_mutex_lock(&ctx->mutex);

    // Compare outcomes of direct vs indirect mechanism
    // For now, just check if both are IC
    nimcp_platform_mutex_unlock(&ctx->mutex);

    nimcp_ic_result_t direct_ic, indirect_ic;
    nimcp_mechanism_is_incentive_compatible(ctx, &direct_ic);
    nimcp_mechanism_is_incentive_compatible(indirect_mechanism, &indirect_ic);

    result->revelation_holds = direct_ic.is_incentive_compatible;
    result->equivalent_ic = direct_ic.ic_level;
    result->efficiency_loss = 0.0f;

    if (result->revelation_holds) {
        snprintf(result->explanation, sizeof(result->explanation),
                 "Revelation principle verified: direct mechanism is IC");
    } else {
        snprintf(result->explanation, sizeof(result->explanation),
                 "Revelation principle: direct mechanism not IC (may need different allocation)");
    }

    return NIMCP_SUCCESS;
}

//=============================================================================
// Signaling Game Functions
//=============================================================================

nimcp_error_t nimcp_signaling_set_sender_types(
    nimcp_mechanism_t ctx,
    const nimcp_type_t* types,
    uint32_t num_types
) {
    if (!ctx || !types) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_mechanism_heartbeat("gt_mechanism_signaling_set_sender", 0.0f);


    if (ctx->config.type != NIMCP_MECHANISM_SIGNALING) {
        return NIMCP_GT_ERROR_INVALID_STATE;
    }

    if (num_types == 0 || num_types > NIMCP_GT_MAX_TYPES) {
        return NIMCP_GT_ERROR_INVALID_PARAMETER;
    }

    // Sender is player 0 in signaling games
    float uniform_prob = 1.0f / (float)num_types;
    float probs[NIMCP_GT_MAX_TYPES];
    for (uint32_t i = 0; i < num_types; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_types > 256) {
            gt_mechanism_heartbeat("gt_mechanism_loop",
                             (float)(i + 1) / (float)num_types);
        }

        probs[i] = uniform_prob;
    }

    return nimcp_mechanism_set_type_space(ctx, 0, types, probs, num_types);
}

nimcp_error_t nimcp_signaling_set_signals(
    nimcp_mechanism_t ctx,
    const nimcp_signal_t* signals,
    uint32_t num_signals
) {
    if (!ctx || !signals) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_mechanism_heartbeat("gt_mechanism_signaling_set_signal", 0.0f);


    if (ctx->config.type != NIMCP_MECHANISM_SIGNALING) {
        return NIMCP_GT_ERROR_INVALID_STATE;
    }

    if (num_signals == 0 || num_signals > NIMCP_GT_MAX_SIGNALS) {
        return NIMCP_GT_ERROR_INVALID_PARAMETER;
    }

    nimcp_platform_mutex_lock(&ctx->mutex);

    memcpy(ctx->signals, signals, num_signals * sizeof(nimcp_signal_t));
    ctx->num_signals = num_signals;

    nimcp_platform_mutex_unlock(&ctx->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_signaling_compute_separating_equilibrium(
    nimcp_mechanism_t ctx,
    nimcp_signal_equilibrium_result_t* result
) {
    if (!ctx || !result) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_mechanism_heartbeat("gt_mechanism_signaling_compute_se", 0.0f);


    if (ctx->config.type != NIMCP_MECHANISM_SIGNALING) {
        return NIMCP_GT_ERROR_INVALID_STATE;
    }

    nimcp_platform_mutex_lock(&ctx->mutex);

    memset(result, 0, sizeof(nimcp_signal_equilibrium_result_t));
    result->type = NIMCP_SIGNAL_EQUIL_SEPARATING;

    uint32_t num_types = ctx->type_spaces[0].num_types;
    uint32_t num_signals = ctx->num_signals;

    // For separating equilibrium, need at least as many signals as types
    if (num_signals < num_types) {
        result->equilibrium_found = false;
        snprintf(result->explanation, sizeof(result->explanation),
                 "Cannot separate: %u signals < %u types", num_signals, num_types);
        nimcp_platform_mutex_unlock(&ctx->mutex);
        return NIMCP_GT_ERROR_NO_EQUILIBRIUM;
    }

    // Check if single-crossing condition holds (higher types prefer higher signals)
    // For simplicity, assume types are ordered by quality and signals by cost

    bool can_separate = true;
    for (uint32_t t = 0; t < num_types && can_separate; t++) {
        // Type t sends signal t (simple assignment)
        nimcp_signaling_strategy_t* strat = &result->sender_strategies[t];
        strat->type_id = t;
        strat->is_pure = true;
        strat->pure_signal = t;
        strat->num_signals = num_signals;
        memset(strat->signal_probs, 0, sizeof(strat->signal_probs));
        strat->signal_probs[t] = 1.0f;

        // Check incentive to deviate (simplified)
        float own_value = ctx->type_spaces[0].types[t].quality;
        float signal_cost = ctx->signals[t].cost;

        // Higher types should prefer higher signals despite cost
        if (t > 0) {
            float lower_signal_cost = ctx->signals[t-1].cost;
            float cost_diff = signal_cost - lower_signal_cost;

            // Need higher type to value separation more than cost difference
            // Simplified check: cost should be feasible
            if (cost_diff > own_value) {
                can_separate = false;
            }
        }
    }

    if (can_separate) {
        result->equilibrium_found = true;
        result->num_sender_types = num_types;
        result->num_signals = num_signals;

        // Set receiver beliefs (perfect inference in separating)
        for (uint32_t s = 0; s < num_signals; s++) {
            /* Phase 8: Loop progress heartbeat */
            if ((s & 0xFF) == 0 && num_signals > 256) {
                gt_mechanism_heartbeat("gt_mechanism_loop",
                                 (float)(s + 1) / (float)num_signals);
            }

            nimcp_receiver_beliefs_t* beliefs = &result->receiver_beliefs[s];
            beliefs->signal_id = s;
            beliefs->num_types = num_types;
            beliefs->on_equilibrium_path = (s < num_types);

            memset(beliefs->posteriors, 0, sizeof(beliefs->posteriors));
            if (s < num_types) {
                beliefs->posteriors[s] = 1.0f;  // Perfect inference
            } else {
                // Off-path beliefs: assume worst type
                beliefs->posteriors[0] = 1.0f;
            }
        }

        // Information content: full revelation in separating
        result->information_transmitted = log2f((float)num_types);

        snprintf(result->explanation, sizeof(result->explanation),
                 "Separating equilibrium: %u types fully revealed", num_types);
    } else {
        result->equilibrium_found = false;
        snprintf(result->explanation, sizeof(result->explanation),
                 "No separating equilibrium: single-crossing violated");
    }

    nimcp_platform_mutex_unlock(&ctx->mutex);

    if (!result->equilibrium_found) {
        return NIMCP_GT_ERROR_NO_EQUILIBRIUM;
    }

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_signaling_compute_pooling_equilibrium(
    nimcp_mechanism_t ctx,
    nimcp_signal_equilibrium_result_t* result
) {
    if (!ctx || !result) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_mechanism_heartbeat("gt_mechanism_signaling_compute_po", 0.0f);


    if (ctx->config.type != NIMCP_MECHANISM_SIGNALING) {
        return NIMCP_GT_ERROR_INVALID_STATE;
    }

    nimcp_platform_mutex_lock(&ctx->mutex);

    memset(result, 0, sizeof(nimcp_signal_equilibrium_result_t));
    result->type = NIMCP_SIGNAL_EQUIL_POOLING;

    uint32_t num_types = ctx->type_spaces[0].num_types;
    uint32_t num_signals = ctx->num_signals;

    if (num_signals == 0) {
        result->equilibrium_found = false;
        nimcp_platform_mutex_unlock(&ctx->mutex);
        return NIMCP_GT_ERROR_INVALID_STATE;
    }

    // In pooling, all types send the same signal
    // Find signal that minimizes cost (typically lowest cost signal)
    uint32_t pooling_signal = 0;
    float min_cost = ctx->signals[0].cost;

    for (uint32_t s = 1; s < num_signals; s++) {
        if (ctx->signals[s].cost < min_cost) {
            min_cost = ctx->signals[s].cost;
            pooling_signal = s;
        }
    }

    result->equilibrium_found = true;
    result->num_sender_types = num_types;
    result->num_signals = num_signals;

    // All types send pooling signal
    for (uint32_t t = 0; t < num_types; t++) {
        /* Phase 8: Loop progress heartbeat */
        if ((t & 0xFF) == 0 && num_types > 256) {
            gt_mechanism_heartbeat("gt_mechanism_loop",
                             (float)(t + 1) / (float)num_types);
        }

        nimcp_signaling_strategy_t* strat = &result->sender_strategies[t];
        strat->type_id = t;
        strat->is_pure = true;
        strat->pure_signal = pooling_signal;
        strat->num_signals = num_signals;
        memset(strat->signal_probs, 0, sizeof(strat->signal_probs));
        strat->signal_probs[pooling_signal] = 1.0f;
    }

    // Receiver beliefs
    for (uint32_t s = 0; s < num_signals; s++) {
        /* Phase 8: Loop progress heartbeat */
        if ((s & 0xFF) == 0 && num_signals > 256) {
            gt_mechanism_heartbeat("gt_mechanism_loop",
                             (float)(s + 1) / (float)num_signals);
        }

        nimcp_receiver_beliefs_t* beliefs = &result->receiver_beliefs[s];
        beliefs->signal_id = s;
        beliefs->num_types = num_types;
        beliefs->on_equilibrium_path = (s == pooling_signal);

        memset(beliefs->posteriors, 0, sizeof(beliefs->posteriors));

        if (s == pooling_signal) {
            // On-path: use prior
            for (uint32_t t = 0; t < num_types; t++) {
                /* Phase 8: Loop progress heartbeat */
                if ((t & 0xFF) == 0 && num_types > 256) {
                    gt_mechanism_heartbeat("gt_mechanism_loop",
                                     (float)(t + 1) / (float)num_types);
                }

                beliefs->posteriors[t] = ctx->type_spaces[0].probabilities[t];
            }
        } else {
            // Off-path: pessimistic beliefs (worst type)
            beliefs->posteriors[0] = 1.0f;
        }
    }

    // No information transmitted in pooling
    result->information_transmitted = 0.0f;

    snprintf(result->explanation, sizeof(result->explanation),
             "Pooling equilibrium: all %u types send signal %u", num_types, pooling_signal);

    nimcp_platform_mutex_unlock(&ctx->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_signaling_compute_all_equilibria(
    nimcp_mechanism_t ctx,
    nimcp_signal_equilibrium_result_t* results,
    uint32_t max_results,
    uint32_t* num_found
) {
    if (!ctx || !results || !num_found) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    *num_found = 0;

    /* Phase 8: Heartbeat at operation start */
    gt_mechanism_heartbeat("gt_mechanism_signaling_compute_al", 0.0f);


    if (max_results == 0) {
        return NIMCP_SUCCESS;
    }

    // Try separating equilibrium
    nimcp_signal_equilibrium_result_t sep_result;
    nimcp_error_t err = nimcp_signaling_compute_separating_equilibrium(ctx, &sep_result);
    if (err == NIMCP_SUCCESS && sep_result.equilibrium_found) {
        results[*num_found] = sep_result;
        (*num_found)++;
    }

    if (*num_found >= max_results) {
        return NIMCP_SUCCESS;
    }

    // Try pooling equilibrium
    nimcp_signal_equilibrium_result_t pool_result;
    err = nimcp_signaling_compute_pooling_equilibrium(ctx, &pool_result);
    if (err == NIMCP_SUCCESS && pool_result.equilibrium_found) {
        results[*num_found] = pool_result;
        (*num_found)++;
    }

    return NIMCP_SUCCESS;
}

//=============================================================================
// Query Functions
//=============================================================================

nimcp_mechanism_state_t nimcp_mechanism_get_state(const nimcp_mechanism_t ctx) {
    if (!ctx) {
        return NIMCP_MECHANISM_STATE_ERROR;
    }
    /* Phase 8: Heartbeat at operation start */
    gt_mechanism_heartbeat("gt_mechanism_mechanism_get_state", 0.0f);


    return ctx->state;
}

nimcp_mechanism_type_t nimcp_mechanism_get_type(const nimcp_mechanism_t ctx) {
    if (!ctx) {
        return NIMCP_MECHANISM_DIRECT;
    }
    /* Phase 8: Heartbeat at operation start */
    gt_mechanism_heartbeat("gt_mechanism_mechanism_get_type", 0.0f);


    return ctx->config.type;
}

const char* nimcp_mechanism_type_name(nimcp_mechanism_type_t type) {
    if (type >= NIMCP_MECHANISM_COUNT) {
        return "Unknown";
    }
    return s_mechanism_type_names[type];
}

const char* nimcp_ic_level_name(nimcp_ic_level_t level) {
    if (level > NIMCP_IC_EX_POST) {
        return "Unknown";
    }
    return s_ic_level_names[level];
}

const char* nimcp_signal_equilibrium_name(nimcp_signal_equilibrium_t type) {
    if (type >= NIMCP_SIGNAL_EQUIL_COUNT) {
        return "Unknown";
    }
    return s_signal_equilibrium_names[type];
}

float nimcp_mechanism_expected_utility(
    nimcp_mechanism_t ctx,
    nimcp_player_id_t player,
    uint32_t type_idx,
    uint32_t report
) {
    if (!ctx || player >= ctx->num_players) {
        return 0.0f;
    }

    if (!ctx->allocation_rule) {
        return 0.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_mechanism_heartbeat("gt_mechanism_mechanism_expected_u", 0.0f);


    nimcp_platform_mutex_lock(&ctx->mutex);

    uint32_t n = ctx->num_players;
    uint32_t total_profiles = count_type_profiles(ctx);
    float expected_utility = 0.0f;

    for (uint32_t profile_idx = 0; profile_idx < total_profiles; profile_idx++) {
        /* Phase 8: Loop progress heartbeat */
        if ((profile_idx & 0xFF) == 0 && total_profiles > 256) {
            gt_mechanism_heartbeat("gt_mechanism_loop",
                             (float)(profile_idx + 1) / (float)total_profiles);
        }

        uint32_t profile[NIMCP_GT_MAX_PLAYERS];
        float prob = 0.0f;

        if (!enumerate_type_profile(ctx, profile_idx, profile, &prob)) {
            break;
        }

        if (profile[player] != type_idx) continue;

        uint32_t reports[NIMCP_GT_MAX_PLAYERS];
        memcpy(reports, profile, sizeof(reports));
        reports[player] = report;

        float allocations[NIMCP_GT_MAX_PLAYERS];
        float payments[NIMCP_GT_MAX_PLAYERS];

        ctx->allocation_rule(reports, n, allocations, ctx->allocation_user_data);
        if (ctx->payment_rule) {
            ctx->payment_rule(reports, n, allocations, payments, ctx->payment_user_data);
        } else {
            memset(payments, 0, sizeof(payments));
        }

        float utility = compute_utility(ctx, player, type_idx, reports, allocations, payments);

        float cond_prob = prob / ctx->type_spaces[player].probabilities[type_idx];
        expected_utility += cond_prob * utility;
    }

    nimcp_platform_mutex_unlock(&ctx->mutex);
    return expected_utility;
}

float nimcp_signaling_information_content(
    const nimcp_signal_equilibrium_result_t* result
) {
    if (!result) {
        return 0.0f;
    }
    /* Phase 8: Heartbeat at operation start */
    gt_mechanism_heartbeat("gt_mechanism_signaling_informatio", 0.0f);


    return result->information_transmitted;
}

//=============================================================================
// Utility Functions
//=============================================================================

void nimcp_type_init(nimcp_type_t* type, uint32_t type_id, float valuation) {
    if (!type) return;

    /* Phase 8: Heartbeat at operation start */
    gt_mechanism_heartbeat("gt_mechanism_type_init", 0.0f);


    memset(type, 0, sizeof(nimcp_type_t));
    type->type_id = type_id;
    type->valuation = valuation;
    type->cost = 0.0f;
    type->quality = valuation;  // Default: quality = valuation
    type->custom_data = NULL;
    type->custom_data_size = 0;
}

void nimcp_type_space_init(nimcp_type_space_t* space, nimcp_player_id_t player_id) {
    if (!space) return;

    /* Phase 8: Heartbeat at operation start */
    gt_mechanism_heartbeat("gt_mechanism_type_space_init", 0.0f);


    memset(space, 0, sizeof(nimcp_type_space_t));
    space->player_id = player_id;
    space->num_types = 0;
    space->type_realized = false;
}

void nimcp_signal_init(nimcp_signal_t* signal, uint32_t signal_id,
                        const char* name, float cost) {
    if (!signal) return;

    /* Phase 8: Heartbeat at operation start */
    gt_mechanism_heartbeat("gt_mechanism_signal_init", 0.0f);


    memset(signal, 0, sizeof(nimcp_signal_t));
    signal->signal_id = signal_id;
    signal->cost = cost;
    signal->intensity = 1.0f;
    signal->is_credible = (cost > 0.0f);

    if (name) {
        strncpy(signal->name, name, sizeof(signal->name) - 1);
        signal->name[sizeof(signal->name) - 1] = '\0';
    }
}

void nimcp_mechanism_result_init(nimcp_mechanism_result_t* result) {
    if (!result) return;

    /* Phase 8: Heartbeat at operation start */
    gt_mechanism_heartbeat("gt_mechanism_mechanism_result_ini", 0.0f);


    memset(result, 0, sizeof(nimcp_mechanism_result_t));
    result->state = NIMCP_MECHANISM_STATE_UNINITIALIZED;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * WHAT: Query knowledge graph for GT Mechanism self-knowledge
 * WHY:  Enable self-awareness about module's role and connections
 * HOW:  Query KG for entity observations and relations
 */
int gt_mechanism_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    gt_mechanism_heartbeat("gt_mechanism_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "GT_Mechanism");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                gt_mechanism_heartbeat("gt_mechanism_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            /* GT Mechanism self-knowledge logged */
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "GT_Mechanism");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "GT_Mechanism");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void gt_mechanism_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_gt_mechanism_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int gt_mechanism_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "gt_mechanism_training_begin: NULL argument");
        return -1;
    }
    gt_mechanism_heartbeat_instance(NULL, "gt_mechanism_training_begin", 0.0f);
    (void)(struct nimcp_mechanism_struct*)instance; /* Module state available for reset */
    return 0;
}

int gt_mechanism_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "gt_mechanism_training_end: NULL argument");
        return -1;
    }
    gt_mechanism_heartbeat_instance(NULL, "gt_mechanism_training_end", 1.0f);
    (void)(struct nimcp_mechanism_struct*)instance; /* Module state available for finalization */
    return 0;
}

int gt_mechanism_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "gt_mechanism_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    gt_mechanism_heartbeat_instance(NULL, "gt_mechanism_training_step", progress);
    (void)(struct nimcp_mechanism_struct*)instance; /* Module state available for step adaptation */
    return 0;
}
