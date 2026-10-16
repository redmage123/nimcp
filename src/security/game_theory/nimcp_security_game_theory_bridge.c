/**
 * @file nimcp_security_game_theory_bridge.c
 * @brief Security-Game Theory Bridge Implementation
 * @version 1.0.0
 * @date 2025-01-10
 *
 * WHAT: Bidirectional integration between security subsystem and game theory
 * WHY:  Game-theoretic reasoning is vulnerable to adversarial exploitation,
 *       payoff manipulation, coalition attacks, and equilibrium tampering.
 * HOW:  Security validates payoff matrices, monitors coalitions, verifies
 *       mechanism design, detects manipulation; game theory provides strategic
 *       analysis of security threat models.
 *
 * @author NIMCP Development Team
 */

#include "security/game_theory/nimcp_security_game_theory_bridge.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(security_game_theory_bridge, MESH_ADAPTER_CATEGORY_SECURITY)


/* ============================================================================
 * Internal Constants
 * ============================================================================ */

/** Default coalition formation window */
#define DEFAULT_COALITION_WINDOW_MS        5000

/** Default history depth for manipulation detection */
#define DEFAULT_HISTORY_DEPTH              32

/** Sybil detection threshold default */
#define DEFAULT_SYBIL_THRESHOLD            0.7f

/** Collusion detection threshold default */
#define DEFAULT_COLLUSION_THRESHOLD        0.6f

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get current timestamp in milliseconds
 */
static uint64_t get_timestamp_ms(void) {
    return nimcp_time_get_us() / 1000;
}

/**
 * @brief Get current timestamp in nanoseconds for performance tracking
 */
static uint64_t get_timestamp_ns(void) {
    return nimcp_time_get_us() * 1000;
}

/**
 * @brief Update running average
 */
static float update_running_avg(float current_avg, uint64_t count, float new_value) {
    if (count == 0) {
        return new_value;
    }
    return (current_avg * (float)(count - 1) + new_value) / (float)count;
}

/**
 * @brief Check if float is NaN
 */
static bool is_nan(float value) {
    return value != value;
}

/**
 * @brief Check if float is infinite
 */
static bool is_inf(float value) {
    return value == HUGE_VALF || value == -HUGE_VALF ||
           (value > 0 && value * 2 == value) ||
           (value < 0 && value * 2 == value);
}

/**
 * @brief Count bits set in bitmask (population count)
 */
static uint32_t popcount(uint32_t x) {
    uint32_t count = 0;
    while (x) {
        count += x & 1;
        x >>= 1;
    }
    return count;
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

int security_gt_default_config(security_game_theory_config_t* config) {
    if (!config) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    memset(config, 0, sizeof(security_game_theory_config_t));

    /* Payoff Validation */
    config->enable_payoff_validation = true;
    config->payoff_lower_bound = -SECURITY_GT_DEFAULT_PAYOFF_BOUND;
    config->payoff_upper_bound = SECURITY_GT_DEFAULT_PAYOFF_BOUND;
    config->enable_tampering_detection = true;
    config->check_nan_inf = true;

    /* Coalition Monitoring */
    config->enable_coalition_monitoring = true;
    config->max_coalition_size = SECURITY_GT_MAX_PLAYERS;
    config->sybil_detection_threshold = DEFAULT_SYBIL_THRESHOLD;
    config->collusion_threshold = DEFAULT_COLLUSION_THRESHOLD;
    config->coalition_formation_window_ms = DEFAULT_COALITION_WINDOW_MS;

    /* Mechanism Integrity */
    config->enable_mechanism_verification = true;
    config->verify_incentive_compatibility = true;
    config->verify_individual_rationality = true;
    config->payment_bound = SECURITY_GT_DEFAULT_PAYOFF_BOUND;

    /* Equilibrium Verification */
    config->enable_equilibrium_verification = true;
    config->nash_epsilon = SECURITY_GT_DEFAULT_EPSILON;
    config->regret_threshold = 0.01f;
    config->max_verification_iterations = 1000;

    /* Manipulation Detection */
    config->enable_manipulation_detection = true;
    config->manipulation_sensitivity = 0.5f;
    config->history_depth = DEFAULT_HISTORY_DEPTH;

    /* Sensitivity Factors */
    config->security_sensitivity = 1.0f;
    config->game_theory_sensitivity = 1.0f;

    return 0;
}

security_game_theory_bridge_t* security_gt_bridge_create(
    const security_game_theory_config_t* config
) {
    /* Allocate bridge structure */
    security_game_theory_bridge_t* bridge = nimcp_malloc(sizeof(security_game_theory_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate security_game_theory_bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "security_gt_bridge_create: allocation failed");

        return NULL;
    }
    memset(bridge, 0, sizeof(security_game_theory_bridge_t));

    /* Initialize config */
    if (config) {
        bridge->config = *config;
    } else {
        security_gt_default_config(&bridge->config);
    }

    /* Initialize base bridge */
    if (bridge_base_init(&bridge->base, SECURITY_GT_MODULE_ID, "security_gt_bridge") != 0) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "security_gt_bridge_create: validation failed");
        return NULL;
    }

    /* Initialize state */
    bridge->state.last_update_time = get_timestamp_ms();

    return bridge;
}

void security_gt_bridge_destroy(security_game_theory_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Cleanup base bridge */
    bridge_base_cleanup(&bridge->base);

    /* Free bridge structure */
    nimcp_free(bridge);
}

/* ============================================================================
 * Connection API Implementation
 * ============================================================================ */

int security_gt_bridge_connect_gt_system(
    security_game_theory_bridge_t* bridge,
    nimcp_gt_system_t gt_system
) {
    BRIDGE_NULL_CHECK(bridge);
    if (!gt_system) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    BRIDGE_LOCK(bridge);
    bridge->gt_system = gt_system;
    bridge->base.system_a = gt_system;
    bridge->base.system_a_connected = true;
    bridge->base.bridge_active = bridge->base.system_a_connected ||
                                  bridge->base.system_b_connected;
    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_gt_bridge_connect_coalition_game(
    security_game_theory_bridge_t* bridge,
    nimcp_coalition_game_t coalition_game
) {
    BRIDGE_NULL_CHECK(bridge);
    if (!coalition_game) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    BRIDGE_LOCK(bridge);
    bridge->coalition_game = coalition_game;
    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_gt_bridge_connect_mechanism(
    security_game_theory_bridge_t* bridge,
    nimcp_mechanism_t mechanism
) {
    BRIDGE_NULL_CHECK(bridge);
    if (!mechanism) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    BRIDGE_LOCK(bridge);
    bridge->mechanism = mechanism;
    bridge->base.system_b = mechanism;
    bridge->base.system_b_connected = true;
    bridge->base.bridge_active = bridge->base.system_a_connected ||
                                  bridge->base.system_b_connected;
    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_gt_bridge_connect_equilibrium(
    security_game_theory_bridge_t* bridge,
    nimcp_equilibrium_t equilibrium
) {
    BRIDGE_NULL_CHECK(bridge);
    if (!equilibrium) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    BRIDGE_LOCK(bridge);
    bridge->equilibrium = equilibrium;
    bridge->base.system_a = equilibrium;
    bridge->base.system_a_connected = true;
    bridge->base.bridge_active = true;
    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_gt_bridge_disconnect(security_game_theory_bridge_t* bridge) {
    BRIDGE_NULL_CHECK(bridge);

    BRIDGE_LOCK(bridge);

    bridge->gt_system = NULL;
    bridge->coalition_game = NULL;
    bridge->mechanism = NULL;
    bridge->equilibrium = NULL;

    bridge->base.system_a = NULL;
    bridge->base.system_b = NULL;
    bridge->base.system_a_connected = false;
    bridge->base.system_b_connected = false;
    bridge->base.bridge_active = false;

    BRIDGE_UNLOCK(bridge);

    return 0;
}

bool security_gt_bridge_is_connected(
    const security_game_theory_bridge_t* bridge
) {
    BRIDGE_NULL_CHECK_BOOL(bridge);
    return bridge->base.bridge_active;
}

/* ============================================================================
 * Security -> Game Theory Direction
 * ============================================================================ */

int security_gt_validate_payoff_matrix(
    security_game_theory_bridge_t* bridge,
    const float* payoffs,
    uint32_t rows,
    uint32_t cols,
    security_payoff_result_t* result
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(result);

    uint64_t start_time = get_timestamp_ns();

    memset(result, 0, sizeof(security_payoff_result_t));
    result->rows = rows;
    result->cols = cols;

    /* Check null pointer */
    if (!payoffs) {
        result->is_valid = false;
        result->status = SECURITY_PAYOFF_INVALID_NULL;
        snprintf(result->reason, SECURITY_GT_MAX_REASON_LEN, "Payoff matrix is NULL");
        return 0;
    }

    /* Check dimensions */
    if (rows == 0 || cols == 0 || rows > SECURITY_GT_MAX_MATRIX_DIM ||
        cols > SECURITY_GT_MAX_MATRIX_DIM) {
        result->is_valid = false;
        result->status = SECURITY_PAYOFF_INVALID_DIMENSION;
        snprintf(result->reason, SECURITY_GT_MAX_REASON_LEN,
                 "Invalid dimensions: %u x %u", rows, cols);
        return 0;
    }

    BRIDGE_LOCK(bridge);

    result->total_elements = rows * cols;
    result->min_value = payoffs[0];
    result->max_value = payoffs[0];

    /* Scan matrix for issues */
    for (uint32_t i = 0; i < result->total_elements; i++) {
        float val = payoffs[i];

        /* Check NaN */
        if (bridge->config.check_nan_inf && is_nan(val)) {
            result->nan_count++;
        }

        /* Check Inf */
        if (bridge->config.check_nan_inf && is_inf(val)) {
            result->inf_count++;
        }

        /* Check bounds */
        if (val < bridge->config.payoff_lower_bound ||
            val > bridge->config.payoff_upper_bound) {
            result->out_of_bounds_count++;
        }

        /* Track min/max */
        if (!is_nan(val) && !is_inf(val)) {
            if (val < result->min_value) result->min_value = val;
            if (val > result->max_value) result->max_value = val;
        }
    }

    /* Determine status */
    if (result->nan_count > 0) {
        result->is_valid = false;
        result->status = SECURITY_PAYOFF_INVALID_NAN;
        snprintf(result->reason, SECURITY_GT_MAX_REASON_LEN,
                 "Matrix contains %u NaN values", result->nan_count);
        bridge->stats.payoff_invalid_nan_count++;
    } else if (result->inf_count > 0) {
        result->is_valid = false;
        result->status = SECURITY_PAYOFF_INVALID_INF;
        snprintf(result->reason, SECURITY_GT_MAX_REASON_LEN,
                 "Matrix contains %u Inf values", result->inf_count);
        bridge->stats.payoff_invalid_inf_count++;
    } else if (result->out_of_bounds_count > 0) {
        result->is_valid = false;
        result->status = SECURITY_PAYOFF_INVALID_BOUNDS;
        snprintf(result->reason, SECURITY_GT_MAX_REASON_LEN,
                 "Matrix has %u out-of-bounds values", result->out_of_bounds_count);
        bridge->stats.payoff_invalid_bounds_count++;
    } else {
        result->is_valid = true;
        result->status = SECURITY_PAYOFF_VALID;
        snprintf(result->reason, SECURITY_GT_MAX_REASON_LEN, "Payoff matrix is valid");
        bridge->stats.payoff_valid_count++;
    }

    /* Update statistics */
    bridge->stats.total_payoff_validations++;
    result->validation_time_ns = get_timestamp_ns() - start_time;
    bridge->stats.avg_validation_time_ns = update_running_avg(
        bridge->stats.avg_validation_time_ns,
        bridge->stats.total_payoff_validations,
        (float)result->validation_time_ns
    );

    /* Update state */
    bridge->state.last_payoff_valid = result->is_valid;
    bridge->state.last_validation_time = get_timestamp_ms();

    /* Update effects */
    bridge->security_effects.payoff_validations++;
    if (!result->is_valid) {
        bridge->security_effects.payoff_rejections++;
    }

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_gt_monitor_coalition(
    security_game_theory_bridge_t* bridge,
    uint32_t coalition,
    const uint32_t* player_ids,
    uint32_t num_players,
    security_coalition_result_t* result
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(result);

    uint64_t start_time = get_timestamp_ns();

    memset(result, 0, sizeof(security_coalition_result_t));
    result->formation_timestamp = get_timestamp_ms();

    /* Validate inputs */
    if (num_players == 0) {
        result->alert = SECURITY_COALITION_NORMAL;
        result->is_suspicious = false;
        snprintf(result->reason, SECURITY_GT_MAX_REASON_LEN, "Empty coalition");
        return 0;
    }

    BRIDGE_LOCK(bridge);

    result->coalition_size = num_players;
    result->num_members = num_players < SECURITY_GT_MAX_PLAYERS ?
                          num_players : SECURITY_GT_MAX_PLAYERS;

    /* Copy member IDs */
    if (player_ids) {
        for (uint32_t i = 0; i < result->num_members; i++) {
            result->members[i] = player_ids[i];
        }
    }

    /* Check coalition size limit */
    if (num_players > bridge->config.max_coalition_size) {
        result->alert = SECURITY_COALITION_SIZE_EXCEEDED;
        result->is_suspicious = true;
        snprintf(result->reason, SECURITY_GT_MAX_REASON_LEN,
                 "Coalition size %u exceeds limit %u",
                 num_players, bridge->config.max_coalition_size);
        bridge->security_effects.coalitions_blocked++;
        goto finish;
    }

    /* Compute Sybil score based on coalition density */
    uint32_t coalition_bits = popcount(coalition);
    if (coalition_bits > 0 && num_players > coalition_bits) {
        result->sybil_score = (float)(num_players - coalition_bits) / (float)num_players;
    } else {
        result->sybil_score = 0.0f;
    }

    /* Check Sybil threshold */
    if (result->sybil_score > bridge->config.sybil_detection_threshold) {
        result->alert = SECURITY_COALITION_SYBIL_DETECTED;
        result->is_suspicious = true;
        snprintf(result->reason, SECURITY_GT_MAX_REASON_LEN,
                 "Sybil attack suspected (score: %.2f)", result->sybil_score);
        bridge->stats.sybil_detections++;
        bridge->security_effects.sybil_detections++;
        goto finish;
    }

    /* Compute collusion score (simplified: based on coalition size vs total) */
    result->collusion_score = (float)num_players / (float)SECURITY_GT_MAX_PLAYERS;
    if (result->collusion_score > bridge->config.collusion_threshold) {
        result->alert = SECURITY_COALITION_COLLUSION;
        result->is_suspicious = true;
        snprintf(result->reason, SECURITY_GT_MAX_REASON_LEN,
                 "Collusion pattern detected (score: %.2f)", result->collusion_score);
        bridge->stats.collusion_detections++;
        goto finish;
    }

    /* Normal coalition */
    result->alert = SECURITY_COALITION_NORMAL;
    result->is_suspicious = false;
    snprintf(result->reason, SECURITY_GT_MAX_REASON_LEN, "Coalition activity normal");

finish:
    /* Update statistics */
    bridge->stats.total_coalition_checks++;
    result->detection_time_ns = get_timestamp_ns() - start_time;
    bridge->stats.avg_coalition_check_time_ns = update_running_avg(
        bridge->stats.avg_coalition_check_time_ns,
        bridge->stats.total_coalition_checks,
        (float)result->detection_time_ns
    );

    if (result->is_suspicious) {
        bridge->state.suspicious_coalitions++;
        bridge->stats.coalitions_blocked++;
    }

    bridge->state.last_coalition_check_time = get_timestamp_ms();

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_gt_verify_mechanism(
    security_game_theory_bridge_t* bridge,
    nimcp_mechanism_t mechanism,
    security_mechanism_result_t* result
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(result);

    uint64_t start_time = get_timestamp_ns();

    memset(result, 0, sizeof(security_mechanism_result_t));

    /* Check null mechanism */
    if (!mechanism) {
        result->is_valid = false;
        result->status = SECURITY_MECHANISM_INVALID_STATE;
        snprintf(result->reason, SECURITY_GT_MAX_REASON_LEN, "Mechanism is NULL");
        return 0;
    }

    BRIDGE_LOCK(bridge);

    /* Verify IC if enabled */
    if (bridge->config.verify_incentive_compatibility) {
        nimcp_ic_result_t ic_result;
        memset(&ic_result, 0, sizeof(ic_result));

        nimcp_error_t ic_err = nimcp_mechanism_is_incentive_compatible(mechanism, &ic_result);
        if (ic_err == NIMCP_SUCCESS) {
            result->incentive_compatible = ic_result.is_incentive_compatible;
            result->max_ic_violation = ic_result.max_deviation_gain;

            if (!ic_result.is_incentive_compatible) {
                result->violating_player = ic_result.violator_id;
                result->violating_type = ic_result.true_type;
                result->profitable_deviation = ic_result.profitable_lie;
            }
        } else {
            result->incentive_compatible = false;
        }
    } else {
        result->incentive_compatible = true;
    }

    /* Verify IR if enabled */
    if (bridge->config.verify_individual_rationality) {
        nimcp_ir_result_t ir_result;
        memset(&ir_result, 0, sizeof(ir_result));

        nimcp_error_t ir_err = nimcp_mechanism_is_individually_rational(mechanism, &ir_result);
        if (ir_err == NIMCP_SUCCESS) {
            result->individually_rational = ir_result.is_individually_rational;
            result->max_ir_violation = ir_result.utility_shortfall;

            if (!ir_result.is_individually_rational && result->violating_player == 0) {
                result->violating_player = ir_result.violator_id;
                result->violating_type = ir_result.violating_type;
            }
        } else {
            result->individually_rational = false;
        }
    } else {
        result->individually_rational = true;
    }

    /* Determine overall status */
    if (!result->incentive_compatible) {
        result->is_valid = false;
        result->status = SECURITY_MECHANISM_IC_VIOLATION;
        snprintf(result->reason, SECURITY_GT_MAX_REASON_LEN,
                 "IC violation: player %u gains %.4f from misreporting",
                 result->violating_player, result->max_ic_violation);
        bridge->stats.ic_violations++;
    } else if (!result->individually_rational) {
        result->is_valid = false;
        result->status = SECURITY_MECHANISM_IR_VIOLATION;
        snprintf(result->reason, SECURITY_GT_MAX_REASON_LEN,
                 "IR violation: player %u type %u has negative utility",
                 result->violating_player, result->violating_type);
        bridge->stats.ir_violations++;
    } else {
        result->is_valid = true;
        result->status = SECURITY_MECHANISM_VALID;
        snprintf(result->reason, SECURITY_GT_MAX_REASON_LEN, "Mechanism is valid");
    }

    /* Update statistics */
    bridge->stats.total_mechanism_checks++;
    result->verification_time_ns = get_timestamp_ns() - start_time;
    bridge->stats.avg_mechanism_check_time_ns = update_running_avg(
        bridge->stats.avg_mechanism_check_time_ns,
        bridge->stats.total_mechanism_checks,
        (float)result->verification_time_ns
    );

    if (!result->is_valid) {
        bridge->stats.mechanisms_invalidated++;
        bridge->security_effects.mechanisms_invalidated++;
    }

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_gt_check_equilibrium(
    security_game_theory_bridge_t* bridge,
    nimcp_equilibrium_t equilibrium,
    const nimcp_strategy_profile_t* strategies,
    security_equilibrium_result_t* result
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(result);

    uint64_t start_time = get_timestamp_ns();

    memset(result, 0, sizeof(security_equilibrium_result_t));

    /* Check null inputs */
    if (!equilibrium || !strategies) {
        result->is_valid = false;
        result->status = SECURITY_EQUILIBRIUM_INVALID_BR;
        snprintf(result->reason, SECURITY_GT_MAX_REASON_LEN,
                 "Equilibrium or strategies is NULL");
        return 0;
    }

    BRIDGE_LOCK(bridge);

    result->num_players = strategies->num_players;
    result->epsilon = bridge->config.nash_epsilon;

    /* Verify Nash equilibrium using the equilibrium solver */
    bool is_nash = nimcp_equilibrium_is_nash(equilibrium, strategies, result->epsilon);
    result->is_nash = is_nash;

    /* Compute regrets for each player */
    float regrets[SECURITY_GT_MAX_PLAYERS];
    memset(regrets, 0, sizeof(regrets));

    nimcp_error_t regret_err = nimcp_equilibrium_compute_regret(
        equilibrium, strategies, regrets);

    if (regret_err == NIMCP_SUCCESS) {
        float total_regret = 0.0f;
        result->max_regret = 0.0f;

        for (uint32_t i = 0; i < result->num_players && i < SECURITY_GT_MAX_PLAYERS; i++) {
            result->player_regrets[i] = regrets[i];
            total_regret += regrets[i];

            if (regrets[i] > result->max_regret) {
                result->max_regret = regrets[i];
            }
        }

        if (result->num_players > 0) {
            result->avg_regret = total_regret / (float)result->num_players;
        }
    }

    /* Determine overall status */
    if (!is_nash) {
        result->is_valid = false;
        result->status = SECURITY_EQUILIBRIUM_INVALID_BR;
        snprintf(result->reason, SECURITY_GT_MAX_REASON_LEN,
                 "Not a Nash equilibrium (max regret: %.4f)", result->max_regret);
        bridge->stats.equilibria_rejected++;
        bridge->security_effects.equilibria_rejected++;
    } else if (result->max_regret > bridge->config.regret_threshold) {
        result->is_valid = false;
        result->status = SECURITY_EQUILIBRIUM_HIGH_REGRET;
        snprintf(result->reason, SECURITY_GT_MAX_REASON_LEN,
                 "Regret %.4f exceeds threshold %.4f",
                 result->max_regret, bridge->config.regret_threshold);
        bridge->stats.equilibria_rejected++;
        bridge->security_effects.equilibria_rejected++;
    } else {
        result->is_valid = true;
        result->is_stable = true;
        result->status = SECURITY_EQUILIBRIUM_VALID;
        result->stability_margin = bridge->config.regret_threshold - result->max_regret;
        snprintf(result->reason, SECURITY_GT_MAX_REASON_LEN,
                 "Valid Nash equilibrium (max regret: %.4f)", result->max_regret);
        bridge->stats.equilibria_verified++;
    }

    /* Update statistics */
    bridge->stats.total_equilibrium_checks++;
    result->verification_time_ns = get_timestamp_ns() - start_time;
    bridge->stats.avg_equilibrium_check_time_ns = update_running_avg(
        bridge->stats.avg_equilibrium_check_time_ns,
        bridge->stats.total_equilibrium_checks,
        (float)result->verification_time_ns
    );
    bridge->stats.avg_max_regret = update_running_avg(
        bridge->stats.avg_max_regret,
        bridge->stats.total_equilibrium_checks,
        result->max_regret
    );

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_gt_detect_manipulation(
    security_game_theory_bridge_t* bridge,
    uint32_t player_id,
    const uint32_t* recent_actions,
    uint32_t num_actions,
    security_manipulation_result_t* result
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(result);

    uint64_t start_time = get_timestamp_ns();

    memset(result, 0, sizeof(security_manipulation_result_t));
    result->affected_player = player_id;

    /* Check null actions */
    if (!recent_actions || num_actions == 0) {
        result->manipulation_detected = false;
        result->type = SECURITY_MANIPULATION_NONE;
        snprintf(result->reason, SECURITY_GT_MAX_REASON_LEN, "No actions to analyze");
        return 0;
    }

    BRIDGE_LOCK(bridge);

    /* Analyze action patterns for timing anomalies */
    float timing_score = 0.0f;
    float pattern_score = 0.0f;

    /* Simple pattern detection: check for repetitive patterns */
    if (num_actions >= 4) {
        uint32_t repeats = 0;
        for (uint32_t i = 2; i < num_actions; i++) {
            if (recent_actions[i] == recent_actions[i - 2]) {
                repeats++;
            }
        }
        pattern_score = (float)repeats / (float)(num_actions - 2);
    }

    /* Compute overall manipulation score */
    float sensitivity = nimcp_clampf(bridge->config.manipulation_sensitivity, 0.0f, 1.0f);
    float threshold = 1.0f - sensitivity;

    result->timing_anomaly_score = timing_score;
    result->pattern_match_score = pattern_score;
    result->confidence = (pattern_score + timing_score) / 2.0f;

    /* Determine if manipulation detected */
    if (result->confidence > threshold) {
        result->manipulation_detected = true;

        if (pattern_score > timing_score) {
            result->type = SECURITY_MANIPULATION_STRATEGY_INJECT;
            snprintf(result->reason, SECURITY_GT_MAX_REASON_LEN,
                     "Strategy injection pattern detected (confidence: %.2f)",
                     result->confidence);
        } else {
            result->type = SECURITY_MANIPULATION_TIMING;
            snprintf(result->reason, SECURITY_GT_MAX_REASON_LEN,
                     "Timing attack pattern detected (confidence: %.2f)",
                     result->confidence);
        }

        bridge->stats.manipulations_detected++;
        bridge->state.manipulation_events++;
        bridge->state.current_manipulation_risk = result->confidence;
    } else {
        result->manipulation_detected = false;
        result->type = SECURITY_MANIPULATION_NONE;
        snprintf(result->reason, SECURITY_GT_MAX_REASON_LEN,
                 "No manipulation detected (confidence: %.2f)", result->confidence);
    }

    /* Update statistics */
    bridge->stats.total_manipulation_checks++;
    result->detection_time_ns = get_timestamp_ns() - start_time;
    bridge->stats.avg_detection_confidence = update_running_avg(
        bridge->stats.avg_detection_confidence,
        bridge->stats.total_manipulation_checks,
        result->confidence
    );

    result->event_count = num_actions;
    result->first_event_timestamp = get_timestamp_ms();

    BRIDGE_UNLOCK(bridge);

    return 0;
}

/* ============================================================================
 * Game Theory -> Security Direction
 * ============================================================================ */

int security_gt_analyze_threat_game(
    security_game_theory_bridge_t* bridge,
    const float* attacker_payoffs,
    const float* defender_payoffs,
    uint32_t num_attacker_actions,
    uint32_t num_defender_actions,
    float* optimal_defense,
    float* expected_payoff
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(attacker_payoffs);
    BRIDGE_NULL_CHECK(defender_payoffs);
    BRIDGE_NULL_CHECK(optimal_defense);
    BRIDGE_NULL_CHECK(expected_payoff);

    if (num_attacker_actions == 0 || num_defender_actions == 0) {
        return NIMCP_ERROR_INVALID_PARAMETER;
    }

    BRIDGE_LOCK(bridge);

    /* Compute minimax strategy for defender (simplified) */
    float best_defender_value = -HUGE_VALF;
    uint32_t best_defender_action = 0;

    for (uint32_t d = 0; d < num_defender_actions; d++) {
        /* Find worst-case attacker response */
        float worst_case = HUGE_VALF;

        for (uint32_t a = 0; a < num_attacker_actions; a++) {
            float defender_payoff = defender_payoffs[a * num_defender_actions + d];
            if (defender_payoff < worst_case) {
                worst_case = defender_payoff;
            }
        }

        /* Track best worst-case */
        if (worst_case > best_defender_value) {
            best_defender_value = worst_case;
            best_defender_action = d;
        }
    }

    /* Build optimal defense strategy (pure strategy in this simplified version) */
    for (uint32_t d = 0; d < num_defender_actions; d++) {
        optimal_defense[d] = (d == best_defender_action) ? 1.0f : 0.0f;
    }

    *expected_payoff = best_defender_value;

    /* Update effects */
    bridge->gt_effects.threat_games_analyzed++;
    bridge->gt_effects.defender_optimal_payoff = best_defender_value;
    bridge->gt_effects.defense_strategies_computed++;
    bridge->gt_effects.recommended_action = best_defender_action;

    /* Compute attacker predicted payoff */
    float attacker_predicted = 0.0f;
    for (uint32_t a = 0; a < num_attacker_actions; a++) {
        float payoff = attacker_payoffs[a * num_defender_actions + best_defender_action];
        if (payoff > attacker_predicted) {
            attacker_predicted = payoff;
        }
    }
    bridge->gt_effects.attacker_predicted_payoff = attacker_predicted;

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_gt_form_defensive_coalition(
    security_game_theory_bridge_t* bridge,
    const uint32_t* defender_ids,
    uint32_t num_defenders,
    uint32_t* coalition_out,
    float* strength_out
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(coalition_out);
    BRIDGE_NULL_CHECK(strength_out);

    if (num_defenders == 0) {
        *coalition_out = 0;
        *strength_out = 0.0f;
        return 0;
    }

    BRIDGE_LOCK(bridge);

    /* Form coalition bitmask from defender IDs */
    uint32_t coalition = 0;
    for (uint32_t i = 0; i < num_defenders && i < 32; i++) {
        if (defender_ids && defender_ids[i] < 32) {
            coalition |= (1u << defender_ids[i]);
        } else {
            coalition |= (1u << i);
        }
    }

    *coalition_out = coalition;

    /* Compute coalition strength (simplified: based on size) */
    float strength = (float)popcount(coalition) / (float)SECURITY_GT_MAX_PLAYERS;
    *strength_out = nimcp_clampf(strength, 0.0f, 1.0f);

    /* Update effects */
    bridge->gt_effects.defense_coalitions_formed++;
    bridge->gt_effects.coalition_strength = *strength_out;

    BRIDGE_UNLOCK(bridge);

    return 0;
}

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

int security_gt_bridge_update(
    security_game_theory_bridge_t* bridge,
    uint64_t delta_ms
) {
    BRIDGE_NULL_CHECK(bridge);

    uint64_t start_time = get_timestamp_ns();

    BRIDGE_LOCK(bridge);

    bridge->stats.bridge_updates++;
    bridge->state.last_update_time = get_timestamp_ms();

    /* Reset per-cycle counters */
    bridge->security_effects.payoff_validations = 0;
    bridge->security_effects.payoff_rejections = 0;
    bridge->security_effects.coalitions_blocked = 0;
    bridge->security_effects.sybil_detections = 0;

    /* Update threat level based on recent manipulation */
    if (bridge->state.manipulation_events > 0) {
        bridge->security_effects.threat_level = nimcp_clampf(
            bridge->state.current_manipulation_risk, 0.0f, 1.0f);
    } else {
        /* Decay threat level over time */
        bridge->security_effects.threat_level *= 0.95f;
    }

    /* High security mode if threat level is high */
    bridge->security_effects.high_security_mode =
        (bridge->security_effects.threat_level > 0.7f);

    /* Record update time */
    uint64_t update_time = get_timestamp_ns() - start_time;
    bridge->stats.avg_update_time_ns = update_running_avg(
        bridge->stats.avg_update_time_ns,
        bridge->stats.bridge_updates,
        (float)update_time
    );

    /* Record in base bridge */
    bridge_base_record_update(&bridge->base);

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_gt_apply_security_effects(
    security_game_theory_bridge_t* bridge
) {
    BRIDGE_NULL_CHECK(bridge);

    BRIDGE_LOCK(bridge);

    /* Apply security sensitivity scaling */
    float sensitivity = nimcp_clampf(bridge->config.security_sensitivity, 0.5f, 2.0f);

    /* Scale threat level by sensitivity */
    bridge->security_effects.threat_level *= sensitivity;
    bridge->security_effects.threat_level = nimcp_clampf(
        bridge->security_effects.threat_level, 0.0f, 1.0f);

    /* Update validation active state */
    bridge->security_effects.payoff_validation_active =
        bridge->config.enable_payoff_validation;
    bridge->security_effects.coalition_monitoring_active =
        bridge->config.enable_coalition_monitoring;
    bridge->security_effects.mechanism_verification_active =
        bridge->config.enable_mechanism_verification;
    bridge->security_effects.equilibrium_audit_active =
        bridge->config.enable_equilibrium_verification;

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_gt_apply_gt_effects(
    security_game_theory_bridge_t* bridge
) {
    BRIDGE_NULL_CHECK(bridge);

    BRIDGE_LOCK(bridge);

    /* Apply game theory sensitivity scaling */
    float sensitivity = nimcp_clampf(bridge->config.game_theory_sensitivity, 0.5f, 2.0f);

    /* Scale defense effectiveness by sensitivity */
    bridge->gt_effects.defense_effectiveness *= sensitivity;
    bridge->gt_effects.defense_effectiveness = nimcp_clampf(
        bridge->gt_effects.defense_effectiveness, 0.0f, 1.0f);

    /* Compute expected loss reduction based on defense strategies */
    if (bridge->gt_effects.defense_strategies_computed > 0) {
        bridge->gt_effects.expected_loss_reduction =
            bridge->gt_effects.defense_effectiveness * 0.5f;
    }

    /* Compute security resource allocation */
    float threat = bridge->security_effects.threat_level;
    bridge->gt_effects.security_resource_allocation = nimcp_clampf(threat, 0.1f, 0.9f);

    BRIDGE_UNLOCK(bridge);

    return 0;
}

/* ============================================================================
 * Query API Implementation
 * ============================================================================ */

int security_gt_bridge_get_state(
    const security_game_theory_bridge_t* bridge,
    security_game_theory_state_t* state
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(state);

    security_game_theory_bridge_t* mutable_bridge = (security_game_theory_bridge_t*)bridge;

    BRIDGE_LOCK(mutable_bridge);
    *state = bridge->state;
    BRIDGE_UNLOCK(mutable_bridge);

    return 0;
}

int security_gt_bridge_get_stats(
    const security_game_theory_bridge_t* bridge,
    security_game_theory_stats_t* stats
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(stats);

    security_game_theory_bridge_t* mutable_bridge = (security_game_theory_bridge_t*)bridge;

    BRIDGE_LOCK(mutable_bridge);
    *stats = bridge->stats;
    BRIDGE_UNLOCK(mutable_bridge);

    return 0;
}

int security_gt_get_security_effects(
    const security_game_theory_bridge_t* bridge,
    security_to_game_theory_effects_t* effects
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(effects);

    security_game_theory_bridge_t* mutable_bridge = (security_game_theory_bridge_t*)bridge;

    BRIDGE_LOCK(mutable_bridge);
    *effects = bridge->security_effects;
    BRIDGE_UNLOCK(mutable_bridge);

    return 0;
}

int security_gt_get_gt_effects(
    const security_game_theory_bridge_t* bridge,
    game_theory_to_security_effects_t* effects
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(effects);

    security_game_theory_bridge_t* mutable_bridge = (security_game_theory_bridge_t*)bridge;

    BRIDGE_LOCK(mutable_bridge);
    *effects = bridge->gt_effects;
    BRIDGE_UNLOCK(mutable_bridge);

    return 0;
}

int security_gt_bridge_reset_stats(security_game_theory_bridge_t* bridge) {
    BRIDGE_NULL_CHECK(bridge);

    BRIDGE_LOCK(bridge);
    memset(&bridge->stats, 0, sizeof(security_game_theory_stats_t));
    BRIDGE_UNLOCK(bridge);

    return 0;
}

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

int security_gt_bridge_connect_bio_async(
    security_game_theory_bridge_t* bridge
) {
    BRIDGE_NULL_CHECK(bridge);
    return bridge_base_connect_bio_async(&bridge->base);
}

int security_gt_bridge_disconnect_bio_async(
    security_game_theory_bridge_t* bridge
) {
    BRIDGE_NULL_CHECK(bridge);
    return bridge_base_disconnect_bio_async(&bridge->base);
}

bool security_gt_bridge_is_bio_async_connected(
    const security_game_theory_bridge_t* bridge
) {
    BRIDGE_NULL_CHECK_BOOL(bridge);
    return bridge_base_is_bio_async_connected(&bridge->base);
}
