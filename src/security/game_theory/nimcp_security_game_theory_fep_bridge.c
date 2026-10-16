/**
 * @file nimcp_security_game_theory_fep_bridge.c
 * @brief Implementation of Security Game Theory - FEP bridge
 * @version 1.0.0
 * @date 2025-01-10
 *
 * WHAT: FEP integration for game-theoretic security detection
 * WHY:  Enable surprise-based adversarial strategy detection
 * HOW:  Map strategy deviations to free energy, detect manipulation
 */

#include "security/game_theory/nimcp_security_game_theory_fep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/platform/nimcp_platform.h"
#include "utils/platform/nimcp_platform_time.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(security_game_theory_fep_bridge, MESH_ADAPTER_CATEGORY_SECURITY)


/* ============================================================================
 * Internal Helper Declarations
 * ============================================================================ */

static float compute_strategy_deviation(
    sgt_fep_bridge_t* bridge,
    const float* strategy,
    uint32_t len
);

static float compute_payoff_error(
    sgt_fep_bridge_t* bridge,
    const float* payoffs,
    uint32_t rows,
    uint32_t cols
);

static float compute_coalition_surprise(
    sgt_fep_bridge_t* bridge,
    uint32_t coalition,
    uint32_t num_players
);

static sgt_fep_severity_t classify_severity(float free_energy);

static sgt_fep_manipulation_t classify_manipulation(
    float strategy_score,
    float payoff_score,
    float coalition_score
);

static void update_running_averages(
    sgt_fep_bridge_t* bridge,
    float free_energy,
    float surprise
);

/* ============================================================================
 * Default Configuration
 * ============================================================================ */

/**
 * WHAT: Provide sensible default configuration
 * WHY:  Simplify initialization with biologically-plausible defaults
 * HOW:  Set tested default values for all parameters
 */
int sgt_fep_default_config(sgt_fep_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    /* FEP parameters */
    config->strategy_fe_threshold = SGT_FEP_ADVERSARIAL_THRESHOLD;
    config->payoff_fe_threshold = SGT_FEP_ADVERSARIAL_THRESHOLD;
    config->coalition_fe_threshold = SGT_FEP_SUSPICIOUS_THRESHOLD;
    config->surprise_threshold = 10.0f;
    config->precision_learning_rate = SGT_FEP_PRECISION_LEARNING_RATE;

    /* Detection parameters */
    config->use_fep_scoring = true;
    config->enable_precision_modulation = true;
    config->normal_fe_threshold = SGT_FEP_NORMAL_THRESHOLD;
    config->critical_fe_threshold = SGT_FEP_CRITICAL_THRESHOLD;

    /* Learning parameters */
    config->enable_online_learning = true;
    config->learning_rate = SGT_FEP_DEFAULT_LEARNING_RATE;
    config->learn_from_false_positives = true;

    /* Feature extraction */
    config->strategy_feature_dim = SGT_FEP_STRATEGY_DIM;
    config->payoff_feature_dim = SGT_FEP_PAYOFF_DIM;
    config->coalition_feature_dim = SGT_FEP_COALITION_DIM;

    /* Active inference */
    config->enable_active_inference = true;
    config->action_temperature = 1.0f;

    /* Sensitivity factors */
    config->fep_sensitivity = 1.0f;
    config->security_sensitivity = 1.0f;

    return 0;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

/**
 * WHAT: Create security game theory FEP bridge
 * WHY:  Enable FEP-based adversarial detection
 * HOW:  Allocate memory, initialize state, connect systems
 */
sgt_fep_bridge_t* sgt_fep_create(
    const sgt_fep_config_t* config,
    security_game_theory_bridge_t* sgt_bridge,
    fep_system_t* fep_system
) {
    /* Validate required parameters */
    if (!fep_system) {
        NIMCP_LOGGING_ERROR("SGT FEP bridge: FEP system is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sgt_fep_create: fep_system is NULL");
        return NULL;
    }

    /* Allocate bridge */
    sgt_fep_bridge_t* bridge = (sgt_fep_bridge_t*)nimcp_malloc(sizeof(sgt_fep_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("SGT FEP bridge: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sgt_fep_create: bridge is NULL");
        return NULL;
    }

    memset(bridge, 0, sizeof(sgt_fep_bridge_t));

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        sgt_fep_default_config(&bridge->config);
    }

    /* Store system references */
    bridge->fep_system = fep_system;
    bridge->sgt_bridge = sgt_bridge;

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "security_game_theory_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("SGT FEP bridge: mutex creation failed");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sgt_fep_create: bridge->base is NULL");
        return NULL;
    }

    /* Allocate feature buffers */
    bridge->strategy_features = (float*)nimcp_malloc(
        bridge->config.strategy_feature_dim * sizeof(float));
    bridge->payoff_features = (float*)nimcp_malloc(
        bridge->config.payoff_feature_dim * sizeof(float));
    bridge->coalition_features = (float*)nimcp_malloc(
        bridge->config.coalition_feature_dim * sizeof(float));

    if (!bridge->strategy_features || !bridge->payoff_features ||
        !bridge->coalition_features) {
        NIMCP_LOGGING_ERROR("SGT FEP bridge: feature buffer allocation failed");
        sgt_fep_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sgt_fep_create: operation failed");
        return NULL;
    }

    /* Initialize state */
    bridge->state.active = true;
    bridge->state.current_precision = SGT_FEP_DEFAULT_PRECISION;
    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_INFO("SGT FEP bridge created successfully");
    return bridge;
}

/**
 * WHAT: Destroy FEP bridge
 * WHY:  Clean up all resources
 * HOW:  Free buffers, mutex, disconnect bio-async
 */
void sgt_fep_destroy(sgt_fep_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        sgt_fep_disconnect_bio_async(bridge);
    }

    /* Free feature buffers */
    if (bridge->strategy_features) {
        nimcp_free(bridge->strategy_features);
    }
    if (bridge->payoff_features) {
        nimcp_free(bridge->payoff_features);
    }
    if (bridge->coalition_features) {
        nimcp_free(bridge->coalition_features);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("SGT FEP bridge destroyed");
}

/**
 * WHAT: Reset bridge to initial state
 * WHY:  Allow clean restart
 * HOW:  Zero state/stats, reset precision
 */
int sgt_fep_reset(sgt_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Reset state */
    memset(&bridge->state, 0, sizeof(sgt_fep_state_t));
    bridge->state.active = true;
    bridge->state.current_precision = SGT_FEP_DEFAULT_PRECISION;

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(sgt_fep_stats_t));
    bridge->stats.min_precision = SGT_FEP_DEFAULT_PRECISION;
    bridge->stats.max_precision = SGT_FEP_DEFAULT_PRECISION;
    bridge->stats.current_precision = SGT_FEP_DEFAULT_PRECISION;

    /* Reset effects */
    memset(&bridge->fep_effects, 0, sizeof(fep_to_sgt_effects_t));
    memset(&bridge->sgt_effects, 0, sizeof(sgt_to_fep_effects_t));

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("SGT FEP bridge reset");
    return 0;
}

/* ============================================================================
 * Configuration Implementation
 * ============================================================================ */

int sgt_fep_get_config(
    const sgt_fep_bridge_t* bridge,
    sgt_fep_config_t* config
) {
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sgt_fep_get_config: required parameter is NULL (bridge, config)");
        return -1;
    }

    *config = bridge->config;
    return 0;
}

int sgt_fep_set_config(
    sgt_fep_bridge_t* bridge,
    const sgt_fep_config_t* config
) {
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sgt_fep_set_config: required parameter is NULL (bridge, config)");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->config = *config;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Core Update Implementation
 * ============================================================================ */

/**
 * WHAT: Update FEP effects on security game theory (unlocked version)
 * WHY:  Called from within locked context (e.g., sgt_fep_compute_effects)
 * HOW:  Process current FEP state, update effects - caller must hold mutex
 */
static int sgt_fep_update_unlocked(sgt_fep_bridge_t* bridge) {
    if (!bridge->state.active) {
        return -1;
    }

    if (!bridge->fep_system) {
        return -1;
    }

    /* Get current FEP state */
    float current_fe = fep_get_free_energy(bridge->fep_system);
    float surprise = fep_compute_surprise(bridge->fep_system);

    /* Update running averages */
    update_running_averages(bridge, current_fe, surprise);

    /* Compute FEP-based manipulation score */
    float threshold = bridge->config.strategy_fe_threshold;
    bridge->fep_effects.fep_manipulation_score = current_fe / threshold;
    if (bridge->fep_effects.fep_manipulation_score > 1.0f) {
        bridge->fep_effects.fep_manipulation_score = 1.0f;
    }

    /* Store current FEP metrics */
    bridge->fep_effects.current_free_energy = current_fe;
    bridge->fep_effects.current_surprise = surprise;
    bridge->fep_effects.detection_sensitivity = bridge->state.current_precision;

    /* Classify severity */
    bridge->fep_effects.severity = classify_severity(current_fe);
    bridge->state.last_severity = bridge->fep_effects.severity;

    /* Compute confidence based on prediction stability */
    float pred_error = fep_get_prediction_error(bridge->fep_system, 0);
    bridge->fep_effects.confidence = 1.0f - (pred_error / 10.0f);
    if (bridge->fep_effects.confidence < 0.0f) {
        bridge->fep_effects.confidence = 0.0f;
    }
    if (bridge->fep_effects.confidence > 1.0f) {
        bridge->fep_effects.confidence = 1.0f;
    }

    /* Active inference: compute recommended action if enabled */
    if (bridge->config.enable_active_inference) {
        fep_efe_t efe;
        if (bridge->fep_system->policies && bridge->fep_system->num_policies > 0) {
            fep_compute_efe(bridge->fep_system,
                          &bridge->fep_system->policies[0], &efe);
            bridge->fep_effects.action_recommended = (efe.total > 0.5f);
        }
    }

    /* Update state */
    bridge->state.update_count++;
    bridge->state.last_update_time = (uint64_t)(nimcp_platform_time_monotonic_us() * 1000ULL);

    /* Update statistics */
    bridge->stats.bridge_updates++;
    bridge->stats.avg_free_energy = bridge->state.avg_free_energy;
    bridge->stats.avg_surprise = bridge->state.avg_surprise;
    if (current_fe > bridge->stats.max_free_energy) {
        bridge->stats.max_free_energy = current_fe;
    }
    if (surprise > bridge->stats.max_surprise) {
        bridge->stats.max_surprise = surprise;
    }

    return 0;
}

/**
 * WHAT: Update FEP effects on security game theory
 * WHY:  Compute FEP-derived manipulation scores
 * HOW:  Process current FEP state, update effects
 */
int sgt_fep_update(sgt_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    int result = sgt_fep_update_unlocked(bridge);
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return result;
}

/**
 * WHAT: Compute all FEP effects
 * WHY:  Provide comprehensive FEP analysis
 * HOW:  Calculate free energy components and map to effects
 */
int sgt_fep_compute_effects(
    sgt_fep_bridge_t* bridge,
    fep_to_sgt_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sgt_fep_compute_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    if (!bridge->fep_system) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return -1;
    }

    /* Update internal effects first (already locked) */
    sgt_fep_update_unlocked(bridge);

    /* Copy to output */
    *effects = bridge->fep_effects;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Update FEP from security detection
 * WHY:  Enable online learning from detections
 * HOW:  Convert detection to observation, update beliefs
 */
int sgt_fep_update_from_detection(
    sgt_fep_bridge_t* bridge,
    bool is_manipulation,
    sgt_fep_manipulation_t type,
    float confidence
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Update effects based on detection */
    if (is_manipulation) {
        bridge->sgt_effects.manipulations_detected++;
        bridge->fep_effects.manipulation_type = type;
    } else {
        bridge->sgt_effects.normal_strategies++;
    }

    /* Update running average */
    float score = is_manipulation ? confidence : 0.0f;
    bridge->sgt_effects.avg_strategy_deviation =
        0.9f * bridge->sgt_effects.avg_strategy_deviation + 0.1f * score;

    /* Update FEP if online learning enabled */
    if (bridge->config.enable_online_learning) {
        if (is_manipulation) {
            /* High-surprise observation: increase precision */
            fep_update_precision(bridge->fep_system);
        } else {
            /* Normal observation: update generative model */
            fep_update_beliefs(bridge->fep_system);
        }
    }

    /* Update statistics */
    bridge->state.detection_count++;
    bridge->stats.total_detections++;

    if (is_manipulation) {
        bridge->stats.manipulations_found++;
        bridge->stats.fep_based_detections++;

        /* Update severity distribution */
        sgt_fep_severity_t sev = bridge->fep_effects.severity;
        switch (sev) {
            case SGT_FEP_SEVERITY_NONE:
                bridge->stats.severity_none++;
                break;
            case SGT_FEP_SEVERITY_LOW:
                bridge->stats.severity_low++;
                break;
            case SGT_FEP_SEVERITY_MEDIUM:
                bridge->stats.severity_medium++;
                break;
            case SGT_FEP_SEVERITY_HIGH:
                bridge->stats.severity_high++;
                break;
            case SGT_FEP_SEVERITY_CRITICAL:
                bridge->stats.severity_critical++;
                break;
        }
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Detection Implementation
 * ============================================================================ */

/**
 * WHAT: Analyze strategy for adversarial behavior
 * WHY:  Detect manipulative strategy deviations
 * HOW:  Compute free energy of strategy observation
 */
int sgt_fep_analyze_strategy(
    sgt_fep_bridge_t* bridge,
    const float* strategy,
    uint32_t strategy_len,
    uint32_t player_id,
    sgt_fep_detection_result_t* result
) {
    if (!bridge || !strategy || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sgt_fep_analyze_strategy: required parameter is NULL (bridge, strategy, result)");
        return -1;
    }

    uint64_t start_time = (uint64_t)(nimcp_platform_time_monotonic_us() * 1000ULL);

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Initialize result */
    memset(result, 0, sizeof(sgt_fep_detection_result_t));
    result->affected_player = player_id;

    /* Compute strategy deviation */
    float deviation = compute_strategy_deviation(bridge, strategy, strategy_len);
    result->strategy_score = deviation;

    /* Process through FEP */
    uint32_t dim = strategy_len < bridge->config.strategy_feature_dim ?
                   strategy_len : bridge->config.strategy_feature_dim;

    /* Copy strategy to features with normalization */
    for (uint32_t i = 0; i < dim; i++) {
        bridge->strategy_features[i] = strategy[i];
    }

    fep_process_observation(bridge->fep_system, bridge->strategy_features, dim);

    /* Compute free energy */
    fep_free_energy_t fe;
    fep_compute_free_energy(bridge->fep_system, &fe);

    result->free_energy = fe.total;
    result->surprise = fe.surprise;
    result->prediction_error = fep_get_prediction_error(bridge->fep_system, 0);
    result->complexity = fe.complexity;
    result->inaccuracy = fe.inaccuracy;

    /* Classify result */
    result->severity = classify_severity(fe.total);
    result->manipulation_detected = (result->severity >= SGT_FEP_SEVERITY_HIGH);
    result->type = result->manipulation_detected ?
                   SGT_FEP_MANIP_STRATEGY_SHIFT : SGT_FEP_MANIP_NONE;

    /* Compute confidence */
    float normalized_fe = fe.total / bridge->config.strategy_fe_threshold;
    result->confidence = normalized_fe > 1.0f ? 1.0f : normalized_fe;

    /* Generate explanation */
    snprintf(result->explanation, sizeof(result->explanation),
             "Strategy analysis: FE=%.2f, deviation=%.2f, severity=%s",
             fe.total, deviation, sgt_fep_severity_to_string(result->severity));

    /* Update effects */
    bridge->fep_effects.strategy_deviation_score = result->strategy_score;

    /* Timing */
    uint64_t end_time = (uint64_t)(nimcp_platform_time_monotonic_us() * 1000ULL);
    result->detection_time_ns = end_time - start_time;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Analyze payoff matrix for manipulation
 * WHY:  Detect tampered payoff values
 * HOW:  Compute prediction error against expected payoffs
 */
int sgt_fep_analyze_payoff(
    sgt_fep_bridge_t* bridge,
    const float* payoffs,
    uint32_t rows,
    uint32_t cols,
    sgt_fep_detection_result_t* result
) {
    if (!bridge || !payoffs || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sgt_fep_analyze_payoff: required parameter is NULL (bridge, payoffs, result)");
        return -1;
    }

    uint64_t start_time = (uint64_t)(nimcp_platform_time_monotonic_us() * 1000ULL);

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Initialize result */
    memset(result, 0, sizeof(sgt_fep_detection_result_t));

    /* Compute payoff error */
    float payoff_error = compute_payoff_error(bridge, payoffs, rows, cols);
    result->payoff_score = payoff_error;

    /* Extract features from payoff matrix */
    uint32_t total = rows * cols;
    uint32_t dim = total < bridge->config.payoff_feature_dim ?
                   total : bridge->config.payoff_feature_dim;

    /* Normalize payoffs to feature range */
    float max_val = 0.0f;
    for (uint32_t i = 0; i < total; i++) {
        float abs_val = fabsf(payoffs[i]);
        if (abs_val > max_val) {
            max_val = abs_val;
        }
    }
    float scale = max_val > 0.0f ? 1.0f / max_val : 1.0f;

    for (uint32_t i = 0; i < dim; i++) {
        bridge->payoff_features[i] = payoffs[i] * scale;
    }

    fep_process_observation(bridge->fep_system, bridge->payoff_features, dim);

    /* Compute free energy */
    fep_free_energy_t fe;
    fep_compute_free_energy(bridge->fep_system, &fe);

    result->free_energy = fe.total;
    result->surprise = fe.surprise;
    result->prediction_error = fep_get_prediction_error(bridge->fep_system, 0);
    result->complexity = fe.complexity;
    result->inaccuracy = fe.inaccuracy;

    /* Classify result */
    result->severity = classify_severity(fe.total);
    result->manipulation_detected = (result->severity >= SGT_FEP_SEVERITY_HIGH);
    result->type = result->manipulation_detected ?
                   SGT_FEP_MANIP_PAYOFF_TAMPERING : SGT_FEP_MANIP_NONE;

    /* Compute confidence */
    float normalized_fe = fe.total / bridge->config.payoff_fe_threshold;
    result->confidence = normalized_fe > 1.0f ? 1.0f : normalized_fe;

    /* Generate explanation */
    snprintf(result->explanation, sizeof(result->explanation),
             "Payoff analysis: FE=%.2f, error=%.2f, %dx%d matrix, severity=%s",
             fe.total, payoff_error, rows, cols,
             sgt_fep_severity_to_string(result->severity));

    /* Update effects */
    bridge->fep_effects.payoff_manipulation_score = result->payoff_score;
    bridge->sgt_effects.payoffs_validated++;
    if (result->manipulation_detected) {
        bridge->sgt_effects.payoffs_rejected++;
    }

    /* Timing */
    uint64_t end_time = (uint64_t)(nimcp_platform_time_monotonic_us() * 1000ULL);
    result->detection_time_ns = end_time - start_time;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Analyze coalition for attack patterns
 * WHY:  Detect coordinated adversarial coalitions
 * HOW:  Compute surprise from coalition formation patterns
 */
int sgt_fep_analyze_coalition(
    sgt_fep_bridge_t* bridge,
    uint32_t coalition,
    const uint32_t* player_ids,
    uint32_t num_players,
    sgt_fep_detection_result_t* result
) {
    if (!bridge || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sgt_fep_analyze_coalition: required parameter is NULL (bridge, result)");
        return -1;
    }

    uint64_t start_time = (uint64_t)(nimcp_platform_time_monotonic_us() * 1000ULL);

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Initialize result */
    memset(result, 0, sizeof(sgt_fep_detection_result_t));
    result->num_suspicious_players = num_players;

    /* Compute coalition surprise */
    float coalition_surprise = compute_coalition_surprise(bridge, coalition, num_players);
    result->coalition_score = coalition_surprise;

    /* Extract coalition features */
    uint32_t dim = bridge->config.coalition_feature_dim;
    memset(bridge->coalition_features, 0, dim * sizeof(float));

    /* Encode coalition information */
    bridge->coalition_features[0] = (float)num_players / SECURITY_GT_MAX_PLAYERS;
    bridge->coalition_features[1] = (float)coalition / (float)0xFFFFFFFF;
    bridge->coalition_features[2] = coalition_surprise;

    /* Encode player distribution if provided */
    if (player_ids && num_players > 0) {
        uint32_t encode_count = (num_players < dim - 3) ? num_players : (dim - 3);
        for (uint32_t i = 0; i < encode_count; i++) {
            bridge->coalition_features[3 + i] =
                (float)player_ids[i] / SECURITY_GT_MAX_PLAYERS;
        }
    }

    fep_process_observation(bridge->fep_system, bridge->coalition_features, dim);

    /* Compute free energy */
    fep_free_energy_t fe;
    fep_compute_free_energy(bridge->fep_system, &fe);

    result->free_energy = fe.total;
    result->surprise = fe.surprise;
    result->prediction_error = fep_get_prediction_error(bridge->fep_system, 0);
    result->complexity = fe.complexity;
    result->inaccuracy = fe.inaccuracy;

    /* Classify result */
    result->severity = classify_severity(fe.total);
    result->manipulation_detected = (result->severity >= SGT_FEP_SEVERITY_MEDIUM);

    /* Determine manipulation type based on patterns */
    if (result->manipulation_detected) {
        if (coalition_surprise > 0.8f) {
            result->type = SGT_FEP_MANIP_COALITION_ATTACK;
        } else if (num_players > SECURITY_GT_MAX_PLAYERS / 2) {
            result->type = SGT_FEP_MANIP_SYBIL_DETECTED;
        } else {
            result->type = SGT_FEP_MANIP_COALITION_ATTACK;
        }
    } else {
        result->type = SGT_FEP_MANIP_NONE;
    }

    /* Compute confidence */
    float normalized_fe = fe.total / bridge->config.coalition_fe_threshold;
    result->confidence = normalized_fe > 1.0f ? 1.0f : normalized_fe;

    /* Generate explanation */
    snprintf(result->explanation, sizeof(result->explanation),
             "Coalition analysis: FE=%.2f, surprise=%.2f, %u players, severity=%s",
             fe.total, coalition_surprise, num_players,
             sgt_fep_severity_to_string(result->severity));

    /* Update effects */
    bridge->fep_effects.coalition_attack_score = result->coalition_score;
    if (result->manipulation_detected) {
        bridge->sgt_effects.coalition_attacks++;
    }

    /* Timing */
    uint64_t end_time = (uint64_t)(nimcp_platform_time_monotonic_us() * 1000ULL);
    result->detection_time_ns = end_time - start_time;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Comprehensive adversarial analysis
 * WHY:  Combine all analysis types
 * HOW:  Fuse strategy, payoff, and coalition analysis
 */
int sgt_fep_full_analysis(
    sgt_fep_bridge_t* bridge,
    const float* strategy,
    uint32_t strategy_len,
    const float* payoffs,
    uint32_t payoff_rows,
    uint32_t payoff_cols,
    uint32_t coalition,
    sgt_fep_detection_result_t* result
) {
    if (!bridge || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sgt_fep_full_analysis: required parameter is NULL (bridge, result)");
        return -1;
    }

    uint64_t start_time = (uint64_t)(nimcp_platform_time_monotonic_us() * 1000ULL);

    /* Initialize result */
    memset(result, 0, sizeof(sgt_fep_detection_result_t));

    float total_score = 0.0f;
    float max_fe = 0.0f;
    int analysis_count = 0;

    sgt_fep_detection_result_t sub_result;

    /* Analyze strategy if provided */
    if (strategy && strategy_len > 0) {
        if (sgt_fep_analyze_strategy(bridge, strategy, strategy_len, 0, &sub_result) == 0) {
            result->strategy_score = sub_result.strategy_score;
            total_score += sub_result.confidence;
            if (sub_result.free_energy > max_fe) {
                max_fe = sub_result.free_energy;
            }
            analysis_count++;
        }
    }

    /* Analyze payoffs if provided */
    if (payoffs && payoff_rows > 0 && payoff_cols > 0) {
        if (sgt_fep_analyze_payoff(bridge, payoffs, payoff_rows, payoff_cols, &sub_result) == 0) {
            result->payoff_score = sub_result.payoff_score;
            total_score += sub_result.confidence;
            if (sub_result.free_energy > max_fe) {
                max_fe = sub_result.free_energy;
            }
            analysis_count++;
        }
    }

    /* Analyze coalition if provided */
    if (coalition != 0) {
        uint32_t num_players = 0;
        for (int i = 0; i < 32; i++) {
            if (coalition & (1u << i)) {
                num_players++;
            }
        }

        if (sgt_fep_analyze_coalition(bridge, coalition, NULL, num_players, &sub_result) == 0) {
            result->coalition_score = sub_result.coalition_score;
            total_score += sub_result.confidence;
            if (sub_result.free_energy > max_fe) {
                max_fe = sub_result.free_energy;
            }
            analysis_count++;
        }
    }

    /* Compute aggregate results */
    if (analysis_count > 0) {
        result->confidence = total_score / analysis_count;
        result->free_energy = max_fe;
        result->surprise = fep_compute_surprise(bridge->fep_system);
        result->prediction_error = fep_get_prediction_error(bridge->fep_system, 0);

        /* Classify overall severity */
        result->severity = classify_severity(max_fe);
        result->manipulation_detected = (result->severity >= SGT_FEP_SEVERITY_HIGH);

        /* Classify manipulation type */
        result->type = classify_manipulation(
            result->strategy_score,
            result->payoff_score,
            result->coalition_score
        );

        /* Generate explanation */
        snprintf(result->explanation, sizeof(result->explanation),
                 "Full analysis: FE=%.2f, conf=%.2f, severity=%s, type=%s",
                 max_fe, result->confidence,
                 sgt_fep_severity_to_string(result->severity),
                 sgt_fep_manipulation_to_string(result->type));
    } else {
        snprintf(result->explanation, sizeof(result->explanation),
                 "No data provided for analysis");
    }

    /* Timing */
    uint64_t end_time = (uint64_t)(nimcp_platform_time_monotonic_us() * 1000ULL);
    result->detection_time_ns = end_time - start_time;

    return 0;
}

/* ============================================================================
 * Precision Modulation Implementation
 * ============================================================================ */

/**
 * WHAT: Apply precision modulation
 * WHY:  Adapt sensitivity to detection performance
 * HOW:  Adjust precision based on detection rates
 */
int sgt_fep_apply_precision_modulation(sgt_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (!bridge->config.enable_precision_modulation) {
        return 0;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Compute manipulation rate */
    float manip_rate = 0.0f;
    if (bridge->state.detection_count > 0) {
        manip_rate = (float)bridge->sgt_effects.manipulations_detected /
                     (float)bridge->state.detection_count;
    }

    /* Determine target precision */
    float target_precision = SGT_FEP_DEFAULT_PRECISION;
    if (manip_rate > 0.2f) {
        /* High manipulation rate: increase precision (more sensitive) */
        target_precision = SGT_FEP_MAX_PRECISION;
    } else if (manip_rate < 0.05f) {
        /* Low manipulation rate: decrease precision */
        target_precision = SGT_FEP_MIN_PRECISION;
    }

    /* Smooth adaptation */
    float alpha = bridge->config.precision_learning_rate;
    bridge->state.current_precision =
        (1.0f - alpha) * bridge->state.current_precision + alpha * target_precision;

    /* Clamp to valid range */
    if (bridge->state.current_precision < SGT_FEP_MIN_PRECISION) {
        bridge->state.current_precision = SGT_FEP_MIN_PRECISION;
    }
    if (bridge->state.current_precision > SGT_FEP_MAX_PRECISION) {
        bridge->state.current_precision = SGT_FEP_MAX_PRECISION;
    }

    /* Update statistics */
    bridge->stats.precision_adaptations++;
    bridge->stats.current_precision = bridge->state.current_precision;
    if (bridge->state.current_precision < bridge->stats.min_precision) {
        bridge->stats.min_precision = bridge->state.current_precision;
    }
    if (bridge->state.current_precision > bridge->stats.max_precision) {
        bridge->stats.max_precision = bridge->state.current_precision;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Report false positive
 * WHY:  Reduce precision to prevent similar FPs
 * HOW:  Decrease precision by fixed factor
 */
int sgt_fep_report_false_positive(sgt_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    bridge->sgt_effects.false_positives++;
    bridge->stats.false_positives++;

    /* Reduce precision if learning enabled */
    if (bridge->config.learn_from_false_positives) {
        float reduction = 0.9f;
        bridge->state.current_precision *= reduction;

        if (bridge->state.current_precision < SGT_FEP_MIN_PRECISION) {
            bridge->state.current_precision = SGT_FEP_MIN_PRECISION;
        }

        bridge->stats.current_precision = bridge->state.current_precision;
        if (bridge->state.current_precision < bridge->stats.min_precision) {
            bridge->stats.min_precision = bridge->state.current_precision;
        }
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int sgt_fep_set_precision(sgt_fep_bridge_t* bridge, float precision) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Clamp to valid range */
    if (precision < SGT_FEP_MIN_PRECISION) {
        precision = SGT_FEP_MIN_PRECISION;
    }
    if (precision > SGT_FEP_MAX_PRECISION) {
        precision = SGT_FEP_MAX_PRECISION;
    }

    bridge->state.current_precision = precision;
    bridge->stats.current_precision = precision;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

float sgt_fep_get_precision(const sgt_fep_bridge_t* bridge) {
    if (!bridge) {
        return -1.0f;
    }
    return bridge->state.current_precision;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int sgt_fep_get_fep_effects(
    const sgt_fep_bridge_t* bridge,
    fep_to_sgt_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sgt_fep_get_fep_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    *effects = bridge->fep_effects;
    return 0;
}

int sgt_fep_get_sgt_effects(
    const sgt_fep_bridge_t* bridge,
    sgt_to_fep_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sgt_fep_get_sgt_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    *effects = bridge->sgt_effects;
    return 0;
}

int sgt_fep_get_state(
    const sgt_fep_bridge_t* bridge,
    sgt_fep_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sgt_fep_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    *state = bridge->state;
    return 0;
}

int sgt_fep_get_stats(
    const sgt_fep_bridge_t* bridge,
    sgt_fep_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sgt_fep_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}

float sgt_fep_get_manipulation_score(const sgt_fep_bridge_t* bridge) {
    if (!bridge) {
        return -1.0f;
    }
    return bridge->fep_effects.fep_manipulation_score;
}

float sgt_fep_get_free_energy(const sgt_fep_bridge_t* bridge) {
    if (!bridge) {
        return -1.0f;
    }
    return bridge->fep_effects.current_free_energy;
}

float sgt_fep_get_surprise(const sgt_fep_bridge_t* bridge) {
    if (!bridge) {
        return -1.0f;
    }
    return bridge->fep_effects.current_surprise;
}

/* ============================================================================
 * Bio-Async Implementation
 * ============================================================================ */

/**
 * WHAT: Connect to bio-async router
 * WHY:  Enable inter-module security notifications
 * HOW:  Register module, setup inbox
 */
int sgt_fep_connect_bio_async(sgt_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (bridge->base.bio_async_enabled) {
        return 0;  /* Already connected */
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SECURITY_GT_FEP,
        .module_name = "security_gt_fep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("SGT FEP bridge connected to bio-async");
    }

    return 0;
}

/**
 * WHAT: Disconnect from bio-async router
 * WHY:  Clean shutdown of bio-async connection
 * HOW:  Unregister module
 */
int sgt_fep_disconnect_bio_async(sgt_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) {
        return 0;
    }

    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    bridge->base.bio_ctx = NULL;

    NIMCP_LOGGING_INFO("SGT FEP bridge disconnected from bio-async");
    return 0;
}

bool sgt_fep_is_bio_async_connected(const sgt_fep_bridge_t* bridge) {
    return bridge ? bridge->base.bio_async_enabled : false;
}

/* ============================================================================
 * Utility Implementation
 * ============================================================================ */

/**
 * WHAT: Print bridge summary
 * WHY:  Debugging and monitoring
 * HOW:  Format and output key metrics
 */
void sgt_fep_print_summary(const sgt_fep_bridge_t* bridge) {
    if (!bridge) {
        printf("SGT FEP Bridge: NULL\n");
        return;
    }

    printf("\n========== SGT FEP Bridge Summary ==========\n");
    printf("State:\n");
    printf("  Active:           %s\n", bridge->state.active ? "yes" : "no");
    printf("  Update Count:     %lu\n", (unsigned long)bridge->state.update_count);
    printf("  Detection Count:  %lu\n", (unsigned long)bridge->state.detection_count);
    printf("  Current Precision: %.3f\n", bridge->state.current_precision);
    printf("  Avg Free Energy:  %.3f\n", bridge->state.avg_free_energy);
    printf("  Avg Surprise:     %.3f\n", bridge->state.avg_surprise);
    printf("  Last Severity:    %s\n",
           sgt_fep_severity_to_string(bridge->state.last_severity));

    printf("\nFEP Effects (FEP -> Security):\n");
    printf("  Manipulation Score:  %.3f\n",
           bridge->fep_effects.fep_manipulation_score);
    printf("  Strategy Deviation:  %.3f\n",
           bridge->fep_effects.strategy_deviation_score);
    printf("  Payoff Manipulation: %.3f\n",
           bridge->fep_effects.payoff_manipulation_score);
    printf("  Coalition Attack:    %.3f\n",
           bridge->fep_effects.coalition_attack_score);
    printf("  Current Free Energy: %.3f\n",
           bridge->fep_effects.current_free_energy);
    printf("  Current Surprise:    %.3f\n",
           bridge->fep_effects.current_surprise);
    printf("  Confidence:          %.3f\n", bridge->fep_effects.confidence);

    printf("\nSecurity Effects (Security -> FEP):\n");
    printf("  Manipulations:   %lu\n",
           (unsigned long)bridge->sgt_effects.manipulations_detected);
    printf("  Normal:          %lu\n",
           (unsigned long)bridge->sgt_effects.normal_strategies);
    printf("  False Positives: %lu\n",
           (unsigned long)bridge->sgt_effects.false_positives);
    printf("  Coalition Attacks: %lu\n",
           (unsigned long)bridge->sgt_effects.coalition_attacks);

    printf("\nStatistics:\n");
    printf("  Total Detections:    %lu\n",
           (unsigned long)bridge->stats.total_detections);
    printf("  FEP-based Detections: %lu\n",
           (unsigned long)bridge->stats.fep_based_detections);
    printf("  Manipulations Found: %lu\n",
           (unsigned long)bridge->stats.manipulations_found);
    printf("  Max Free Energy:     %.3f\n", bridge->stats.max_free_energy);
    printf("  Max Surprise:        %.3f\n", bridge->stats.max_surprise);
    printf("  Precision Range:     [%.3f, %.3f]\n",
           bridge->stats.min_precision, bridge->stats.max_precision);

    printf("\nBio-Async: %s\n",
           bridge->base.bio_async_enabled ? "connected" : "disconnected");
    printf("=============================================\n\n");
}

const char* sgt_fep_severity_to_string(sgt_fep_severity_t severity) {
    switch (severity) {
        case SGT_FEP_SEVERITY_NONE:
            return "none";
        case SGT_FEP_SEVERITY_LOW:
            return "low";
        case SGT_FEP_SEVERITY_MEDIUM:
            return "medium";
        case SGT_FEP_SEVERITY_HIGH:
            return "high";
        case SGT_FEP_SEVERITY_CRITICAL:
            return "critical";
        default:
            return "unknown";
    }
}

const char* sgt_fep_manipulation_to_string(sgt_fep_manipulation_t type) {
    switch (type) {
        case SGT_FEP_MANIP_NONE:
            return "none";
        case SGT_FEP_MANIP_STRATEGY_SHIFT:
            return "strategy_shift";
        case SGT_FEP_MANIP_PAYOFF_TAMPERING:
            return "payoff_tampering";
        case SGT_FEP_MANIP_COALITION_ATTACK:
            return "coalition_attack";
        case SGT_FEP_MANIP_EQUILIBRIUM_POISON:
            return "equilibrium_poison";
        case SGT_FEP_MANIP_TIMING_ATTACK:
            return "timing_attack";
        case SGT_FEP_MANIP_SYBIL_DETECTED:
            return "sybil_detected";
        case SGT_FEP_MANIP_UNKNOWN:
        default:
            return "unknown";
    }
}

/* ============================================================================
 * Internal Helper Implementation
 * ============================================================================ */

/**
 * WHAT: Compute strategy deviation from expected
 * WHY:  Quantify how much strategy differs from Nash
 * HOW:  L2 norm of difference from expected
 */
static float compute_strategy_deviation(
    sgt_fep_bridge_t* bridge,
    const float* strategy,
    uint32_t len
) {
    if (!bridge || !strategy || len == 0) {
        return 0.0f;
    }

    /* Compute deviation as variance from uniform distribution */
    float expected = 1.0f / len;
    float deviation = 0.0f;

    for (uint32_t i = 0; i < len; i++) {
        float diff = strategy[i] - expected;
        deviation += diff * diff;
    }

    return sqrtf(deviation);
}

/**
 * WHAT: Compute payoff prediction error
 * WHY:  Detect payoff manipulation
 * HOW:  Compute variance and check for anomalies
 */
static float compute_payoff_error(
    sgt_fep_bridge_t* bridge,
    const float* payoffs,
    uint32_t rows,
    uint32_t cols
) {
    if (!bridge || !payoffs || rows == 0 || cols == 0) {
        return 0.0f;
    }

    uint32_t total = rows * cols;

    /* Compute mean */
    float mean = 0.0f;
    for (uint32_t i = 0; i < total; i++) {
        mean += payoffs[i];
    }
    mean /= total;

    /* Compute variance */
    float variance = 0.0f;
    for (uint32_t i = 0; i < total; i++) {
        float diff = payoffs[i] - mean;
        variance += diff * diff;
    }
    variance /= total;

    /* Check for NaN/Inf */
    for (uint32_t i = 0; i < total; i++) {
        if (isnan(payoffs[i]) || isinf(payoffs[i])) {
            return 1.0f;  /* Maximum error */
        }
    }

    /* Normalize variance to [0,1] */
    float normalized = sqrtf(variance) / (fabsf(mean) + 1.0f);
    return normalized > 1.0f ? 1.0f : normalized;
}

/**
 * WHAT: Compute coalition surprise
 * WHY:  Detect unexpected coalition formations
 * HOW:  Based on coalition size and formation rate
 */
static float compute_coalition_surprise(
    sgt_fep_bridge_t* bridge,
    uint32_t coalition,
    uint32_t num_players
) {
    if (!bridge) {
        return 0.0f;
    }

    /* Large coalitions are more surprising */
    float size_surprise = (float)num_players / SECURITY_GT_MAX_PLAYERS;

    /* Coalitions with many members are suspicious */
    if (num_players > SECURITY_GT_MAX_PLAYERS / 2) {
        size_surprise *= 1.5f;
    }

    /* Cap at 1.0 */
    return size_surprise > 1.0f ? 1.0f : size_surprise;
}

/**
 * WHAT: Classify severity from free energy
 * WHY:  Map continuous FE to discrete levels
 * HOW:  Threshold-based classification
 */
static sgt_fep_severity_t classify_severity(float free_energy) {
    if (free_energy >= SGT_FEP_CRITICAL_THRESHOLD) {
        return SGT_FEP_SEVERITY_CRITICAL;
    }
    if (free_energy >= SGT_FEP_ADVERSARIAL_THRESHOLD) {
        return SGT_FEP_SEVERITY_HIGH;
    }
    if (free_energy >= SGT_FEP_SUSPICIOUS_THRESHOLD) {
        return SGT_FEP_SEVERITY_MEDIUM;
    }
    if (free_energy >= SGT_FEP_NORMAL_THRESHOLD) {
        return SGT_FEP_SEVERITY_LOW;
    }
    return SGT_FEP_SEVERITY_NONE;
}

/**
 * WHAT: Classify manipulation type from component scores
 * WHY:  Determine primary manipulation vector
 * HOW:  Find highest scoring component
 */
static sgt_fep_manipulation_t classify_manipulation(
    float strategy_score,
    float payoff_score,
    float coalition_score
) {
    /* Find maximum */
    if (strategy_score >= payoff_score && strategy_score >= coalition_score) {
        return strategy_score > 0.5f ?
               SGT_FEP_MANIP_STRATEGY_SHIFT : SGT_FEP_MANIP_NONE;
    }
    if (payoff_score >= strategy_score && payoff_score >= coalition_score) {
        return payoff_score > 0.5f ?
               SGT_FEP_MANIP_PAYOFF_TAMPERING : SGT_FEP_MANIP_NONE;
    }
    if (coalition_score > 0.5f) {
        return SGT_FEP_MANIP_COALITION_ATTACK;
    }

    return SGT_FEP_MANIP_NONE;
}

/**
 * WHAT: Update running averages
 * WHY:  Track trends in FEP metrics
 * HOW:  Exponential moving average
 */
static void update_running_averages(
    sgt_fep_bridge_t* bridge,
    float free_energy,
    float surprise
) {
    const float alpha = 0.1f;

    bridge->state.avg_free_energy =
        (1.0f - alpha) * bridge->state.avg_free_energy + alpha * free_energy;
    bridge->state.avg_surprise =
        (1.0f - alpha) * bridge->state.avg_surprise + alpha * surprise;
}
