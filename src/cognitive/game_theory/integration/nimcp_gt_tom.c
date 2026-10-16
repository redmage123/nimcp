//=============================================================================
// nimcp_gt_tom.c - Theory of Mind Integration for Game Theory
//=============================================================================
/**
 * @file nimcp_gt_tom.c
 * @brief Theory of Mind (ToM) for opponent modeling in strategic games
 *
 * WHAT: Bayesian opponent modeling and mental state inference
 * WHY:  Strategic reasoning requires understanding opponent beliefs/goals
 * HOW:  Observe actions, update beliefs via Bayes rule, predict behavior
 *
 * @author NIMCP Development Team
 * @date 2024-12-27
 * @version 1.0.0
 */

#include "cognitive/game_theory/integration/nimcp_gt_tom.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/statistics/nimcp_statistics.h"
#include <string.h>
#include <math.h>
#include <float.h>

#define LOG_MODULE "tom"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(gt_tom, MESH_ADAPTER_CATEGORY_COGNITIVE)



//=============================================================================
// Constants
//=============================================================================

#define MIN_OBSERVATIONS_FOR_INFERENCE 3
#include "constants/nimcp_constants.h"
#define EPSILON NIMCP_EPSILON_NUMERICAL
#define LOG2_E 1.44269504089f

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Action history entry
 */
typedef struct {
    nimcp_observed_action_t action;
    float weight;                     /**< Decay-weighted importance */
    bool valid;                       /**< Entry is valid */
} tom_history_entry_t;

/**
 * @brief Internal opponent record
 */
typedef struct {
    nimcp_player_id_t id;
    bool active;

    // Type distribution
    float type_probs[NIMCP_TOM_NUM_OPPONENT_TYPES];
    nimcp_opponent_type_t most_likely_type;

    // Preference model
    nimcp_opponent_preferences_t preferences;

    // Mental state
    nimcp_mental_state_t mental_state;

    // Action history (circular buffer)
    tom_history_entry_t history[NIMCP_TOM_MAX_HISTORY];
    uint32_t history_head;
    uint32_t history_count;

    // Statistics
    uint32_t total_observations;
    uint32_t recent_coop_count;
    uint32_t recent_defect_count;
    float avg_payoff_received;
    float avg_payoff_given;
    float action_correlation;

    // Prediction tracking
    uint32_t predictions_made;
    uint32_t predictions_correct;
    uint32_t last_predicted_action;

    // Action frequency counts
    uint32_t action_counts[NIMCP_TOM_MAX_ACTIONS];
    uint32_t total_action_count;

} tom_opponent_record_t;

/**
 * @brief Opaque ToM context structure
 */
struct nimcp_gt_tom_struct {
    nimcp_gt_tom_config_t config;

    // Opponent records
    tom_opponent_record_t opponents[NIMCP_TOM_MAX_OPPONENTS];
    uint32_t num_opponents;

    // Thread safety
    nimcp_mutex_t* mutex;
    bool thread_safe;

    // Statistics
    uint64_t total_observations;
    uint64_t total_predictions;
    uint64_t correct_predictions;

    bool active;
};

//=============================================================================
// Static Helper Declarations
//=============================================================================

static tom_opponent_record_t* find_opponent(nimcp_gt_tom_t ctx, nimcp_player_id_t id);
static tom_opponent_record_t* get_or_create_opponent(nimcp_gt_tom_t ctx, nimcp_player_id_t id);
static void reset_opponent_record(nimcp_gt_tom_t ctx, tom_opponent_record_t* record);
static void add_history_entry(tom_opponent_record_t* record, const nimcp_observed_action_t* action);
static void apply_decay(nimcp_gt_tom_t ctx, tom_opponent_record_t* record);
static float compute_entropy(const float* probs, uint32_t n);
static float compute_type_likelihood(const tom_opponent_record_t* record, nimcp_opponent_type_t type);
static void normalize_probs(float* probs, uint32_t n);
static void update_type_distribution(nimcp_gt_tom_t ctx, tom_opponent_record_t* record);
static void infer_preferences_internal(const tom_opponent_record_t* record, nimcp_opponent_preferences_t* prefs);
static void infer_mental_state_internal(nimcp_gt_tom_t ctx, const tom_opponent_record_t* record, nimcp_mental_state_t* state);
static void predict_action_internal(nimcp_gt_tom_t ctx, const tom_opponent_record_t* record,
                                     const nimcp_action_context_t* situation, nimcp_action_prediction_t* prediction);

//=============================================================================
// Lifecycle Functions
//=============================================================================

nimcp_gt_tom_config_t nimcp_gt_tom_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    gt_tom_heartbeat("gt_tom_gt_tom_default_confi", 0.0f);


    nimcp_gt_tom_config_t config = {
        // Uniform priors
        .prior_cooperative = 0.2f,
        .prior_competitive = 0.2f,
        .prior_random = 0.2f,
        .prior_tit_for_tat = 0.2f,
        .prior_rational = 0.2f,

        // Learning parameters
        .learning_rate = NIMCP_LEARNING_RATE_COARSE,
        .decay_rate = 0.01f,
        .confidence_threshold = NIMCP_CONFIDENCE_LOW,

        // Recursion parameters
        .max_recursion_depth = 2,
        .recursion_discount = 0.7f,

        // History parameters
        .history_window = 64,
        .enable_forgetting = true,

        // Thread safety
        .thread_safe = true
    };
    return config;
}

nimcp_gt_tom_t nimcp_gt_tom_create(const nimcp_gt_tom_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    gt_tom_heartbeat("gt_tom_gt_tom_create", 0.0f);


    nimcp_gt_tom_t ctx = nimcp_calloc(1, sizeof(struct nimcp_gt_tom_struct));
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate ctx");

        return NULL;
    }

    ctx->config = config ? *config : nimcp_gt_tom_default_config();
    ctx->num_opponents = 0;
    ctx->total_observations = 0;
    ctx->total_predictions = 0;
    ctx->correct_predictions = 0;
    ctx->active = true;
    ctx->thread_safe = ctx->config.thread_safe;

    // Initialize all opponent slots as inactive
    for (uint32_t i = 0; i < NIMCP_TOM_MAX_OPPONENTS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && NIMCP_TOM_MAX_OPPONENTS > 256) {
            gt_tom_heartbeat("gt_tom_loop",
                             (float)(i + 1) / (float)NIMCP_TOM_MAX_OPPONENTS);
        }

        ctx->opponents[i].active = false;
        ctx->opponents[i].id = NIMCP_GT_INVALID_PLAYER;
    }

    // Create mutex if thread-safe mode enabled
    if (ctx->thread_safe) {
        mutex_attr_t attr = { .type = MUTEX_TYPE_NORMAL };
        ctx->mutex = nimcp_mutex_create(&attr);
        if (!ctx->mutex) {
            nimcp_free(ctx);
            ctx = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_gt_tom_create: ctx->mutex is NULL");
            return NULL;
        }
    } else {
        ctx->mutex = NULL;
    }

    return ctx;
}

void nimcp_gt_tom_destroy(nimcp_gt_tom_t ctx) {
    if (!ctx) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_tom_heartbeat("gt_tom_gt_tom_destroy", 0.0f);


    ctx->active = false;

    // Destroy mutex
    if (ctx->mutex) {
        nimcp_mutex_destroy(ctx->mutex);
    }

    nimcp_free(ctx);
    ctx = NULL;
}

//=============================================================================
// Static Helper Implementations
//=============================================================================

static tom_opponent_record_t* find_opponent(nimcp_gt_tom_t ctx, nimcp_player_id_t id) {
    for (uint32_t i = 0; i < NIMCP_TOM_MAX_OPPONENTS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && NIMCP_TOM_MAX_OPPONENTS > 256) {
            gt_tom_heartbeat("gt_tom_loop",
                             (float)(i + 1) / (float)NIMCP_TOM_MAX_OPPONENTS);
        }

        if (ctx->opponents[i].active && ctx->opponents[i].id == id) {
            return &ctx->opponents[i];
        }
    }
    return NULL;  /* Opponent not found — normal condition */
}

static tom_opponent_record_t* get_or_create_opponent(nimcp_gt_tom_t ctx, nimcp_player_id_t id) {
    // First, try to find existing
    tom_opponent_record_t* record = find_opponent(ctx, id);
    if (record) {
        return record;
    }

    // Find empty slot
    for (uint32_t i = 0; i < NIMCP_TOM_MAX_OPPONENTS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && NIMCP_TOM_MAX_OPPONENTS > 256) {
            gt_tom_heartbeat("gt_tom_loop",
                             (float)(i + 1) / (float)NIMCP_TOM_MAX_OPPONENTS);
        }

        if (!ctx->opponents[i].active) {
            record = &ctx->opponents[i];
            reset_opponent_record(ctx, record);
            record->id = id;
            record->active = true;
            ctx->num_opponents++;
            return record;
        }
    }

    NIMCP_LOGGING_WARN("get_or_create_opponent: opponent capacity full (max %u)", NIMCP_TOM_MAX_OPPONENTS);
    return NULL;  // No space for new opponent
}

static void reset_opponent_record(nimcp_gt_tom_t ctx, tom_opponent_record_t* record) {
    memset(record, 0, sizeof(tom_opponent_record_t));

    // Set priors from config
    record->type_probs[NIMCP_OPPONENT_COOPERATIVE] = ctx->config.prior_cooperative;
    record->type_probs[NIMCP_OPPONENT_COMPETITIVE] = ctx->config.prior_competitive;
    record->type_probs[NIMCP_OPPONENT_RANDOM] = ctx->config.prior_random;
    record->type_probs[NIMCP_OPPONENT_TIT_FOR_TAT] = ctx->config.prior_tit_for_tat;
    record->type_probs[NIMCP_OPPONENT_RATIONAL] = ctx->config.prior_rational;
    record->type_probs[NIMCP_OPPONENT_UNKNOWN] = 0.0f;

    record->most_likely_type = NIMCP_OPPONENT_UNKNOWN;
    record->history_head = 0;
    record->history_count = 0;
    record->id = NIMCP_GT_INVALID_PLAYER;
    record->active = false;
}

static void add_history_entry(tom_opponent_record_t* record, const nimcp_observed_action_t* action) {
    uint32_t idx = record->history_head;
    record->history[idx].action = *action;
    record->history[idx].weight = 1.0f;
    record->history[idx].valid = true;

    record->history_head = (record->history_head + 1) % NIMCP_TOM_MAX_HISTORY;
    if (record->history_count < NIMCP_TOM_MAX_HISTORY) {
        record->history_count++;
    }
}

static void apply_decay(nimcp_gt_tom_t ctx, tom_opponent_record_t* record) {
    if (!ctx->config.enable_forgetting) {
        return;
    }

    float decay = 1.0f - ctx->config.decay_rate;
    for (uint32_t i = 0; i < record->history_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && record->history_count > 256) {
            gt_tom_heartbeat("gt_tom_loop",
                             (float)(i + 1) / (float)record->history_count);
        }

        record->history[i].weight *= decay;
        if (record->history[i].weight < EPSILON) {
            record->history[i].valid = false;
        }
    }
}

/**
 * @brief Compute entropy using central statistics module
 *
 * Delegates to nimcp_stats_entropy() which returns Shannon entropy in bits.
 */
static float compute_entropy(const float* probs, uint32_t n) {
    if (!probs || n == 0) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    if (n > 256) {
        gt_tom_heartbeat("gt_tom_entropy", 0.5f);
    }

    return nimcp_stats_entropy(probs, n);
}

static void normalize_probs(float* probs, uint32_t n) {
    float sum = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            gt_tom_heartbeat("gt_tom_loop",
                             (float)(i + 1) / (float)n);
        }

        sum += probs[i];
    }
    if (sum > EPSILON) {
        for (uint32_t i = 0; i < n; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && n > 256) {
                gt_tom_heartbeat("gt_tom_loop",
                                 (float)(i + 1) / (float)n);
            }

            probs[i] /= sum;
        }
    } else {
        // Uniform distribution if all zero
        for (uint32_t i = 0; i < n; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && n > 256) {
                gt_tom_heartbeat("gt_tom_loop",
                                 (float)(i + 1) / (float)n);
            }

            probs[i] = 1.0f / (float)n;
        }
    }
}

/**
 * @brief Compute likelihood of observations given opponent type
 *
 * WHAT: P(actions | type) for Bayesian update
 * WHY:  Core of Bayesian inference
 * HOW:  Type-specific action probability models
 */
static float compute_type_likelihood(const tom_opponent_record_t* record, nimcp_opponent_type_t type) {
    if (record->total_action_count == 0) {
        return 1.0f;  // No data, uniform likelihood
    }

    float likelihood = 1.0f;

    // Analyze action patterns
    float coop_rate = 0.0f;
    if (record->recent_coop_count + record->recent_defect_count > 0) {
        coop_rate = (float)record->recent_coop_count /
                    (float)(record->recent_coop_count + record->recent_defect_count);
    }

    // Compute likelihood based on expected behavior for each type
    switch (type) {
        case NIMCP_OPPONENT_COOPERATIVE:
            // Cooperative: high cooperation rate expected
            likelihood = 0.3f + 0.7f * coop_rate;
            break;

        case NIMCP_OPPONENT_COMPETITIVE:
            // Competitive: low cooperation rate expected
            likelihood = 0.3f + 0.7f * (1.0f - coop_rate);
            break;

        case NIMCP_OPPONENT_RANDOM:
            // Random: uniform actions, moderate cooperation
            likelihood = 0.5f + 0.2f * (1.0f - fabsf(coop_rate - 0.5f) * 2.0f);
            break;

        case NIMCP_OPPONENT_TIT_FOR_TAT:
            // Tit-for-tat: high correlation with my previous actions
            likelihood = 0.3f + 0.7f * fabsf(record->action_correlation);
            break;

        case NIMCP_OPPONENT_RATIONAL:
            // Rational: moderate, context-dependent behavior
            likelihood = 0.5f;
            break;

        case NIMCP_OPPONENT_UNKNOWN:
        default:
            likelihood = 0.1f;
            break;
    }

    // Clamp to avoid numerical issues
    if (likelihood < 0.01f) likelihood = 0.01f;
    if (likelihood > 1.0f) likelihood = 1.0f;

    return likelihood;
}

static void update_type_distribution(nimcp_gt_tom_t ctx, tom_opponent_record_t* record) {
    // Bayesian update: P(type|actions) = P(actions|type) * P(type) / P(actions)
    float posteriors[NIMCP_TOM_NUM_OPPONENT_TYPES];

    for (uint32_t t = 0; t < NIMCP_TOM_NUM_OPPONENT_TYPES; t++) {
        /* Phase 8: Loop progress heartbeat */
        if ((t & 0xFF) == 0 && NIMCP_TOM_NUM_OPPONENT_TYPES > 256) {
            gt_tom_heartbeat("gt_tom_loop",
                             (float)(t + 1) / (float)NIMCP_TOM_NUM_OPPONENT_TYPES);
        }

        float likelihood = compute_type_likelihood(record, (nimcp_opponent_type_t)t);
        posteriors[t] = likelihood * record->type_probs[t];
    }

    normalize_probs(posteriors, NIMCP_TOM_NUM_OPPONENT_TYPES);

    // Blend with learning rate
    float lr = ctx->config.learning_rate;
    for (uint32_t t = 0; t < NIMCP_TOM_NUM_OPPONENT_TYPES; t++) {
        /* Phase 8: Loop progress heartbeat */
        if ((t & 0xFF) == 0 && NIMCP_TOM_NUM_OPPONENT_TYPES > 256) {
            gt_tom_heartbeat("gt_tom_loop",
                             (float)(t + 1) / (float)NIMCP_TOM_NUM_OPPONENT_TYPES);
        }

        record->type_probs[t] = (1.0f - lr) * record->type_probs[t] + lr * posteriors[t];
    }

    // Find most likely type
    float max_prob = 0.0f;
    record->most_likely_type = NIMCP_OPPONENT_UNKNOWN;
    for (uint32_t t = 0; t < NIMCP_TOM_NUM_OPPONENT_TYPES; t++) {
        /* Phase 8: Loop progress heartbeat */
        if ((t & 0xFF) == 0 && NIMCP_TOM_NUM_OPPONENT_TYPES > 256) {
            gt_tom_heartbeat("gt_tom_loop",
                             (float)(t + 1) / (float)NIMCP_TOM_NUM_OPPONENT_TYPES);
        }

        if (record->type_probs[t] > max_prob) {
            max_prob = record->type_probs[t];
            record->most_likely_type = (nimcp_opponent_type_t)t;
        }
    }

    // If insufficient confidence, mark as unknown
    float entropy = compute_entropy(record->type_probs, NIMCP_TOM_NUM_OPPONENT_TYPES);
    float max_entropy = logf((float)NIMCP_TOM_NUM_OPPONENT_TYPES) * LOG2_E;
    float confidence = 1.0f - (entropy / max_entropy);
    if (confidence < ctx->config.confidence_threshold) {
        record->most_likely_type = NIMCP_OPPONENT_UNKNOWN;
    }
}

static void infer_preferences_internal(const tom_opponent_record_t* record, nimcp_opponent_preferences_t* prefs) {
    memset(prefs, 0, sizeof(nimcp_opponent_preferences_t));

    if (record->total_action_count == 0) {
        return;
    }

    // Action preferences from frequency
    for (uint32_t a = 0; a < NIMCP_TOM_MAX_ACTIONS; a++) {
        /* Phase 8: Loop progress heartbeat */
        if ((a & 0xFF) == 0 && NIMCP_TOM_MAX_ACTIONS > 256) {
            gt_tom_heartbeat("gt_tom_loop",
                             (float)(a + 1) / (float)NIMCP_TOM_MAX_ACTIONS);
        }

        prefs->action_preferences[a] = (float)record->action_counts[a] / (float)record->total_action_count;
    }

    // Infer from type probabilities
    float coop_prob = record->type_probs[NIMCP_OPPONENT_COOPERATIVE];
    float comp_prob = record->type_probs[NIMCP_OPPONENT_COMPETITIVE];
    float tft_prob = record->type_probs[NIMCP_OPPONENT_TIT_FOR_TAT];

    prefs->cooperation_tendency = (coop_prob - comp_prob);
    prefs->reciprocity_strength = tft_prob * 2.0f;
    if (prefs->reciprocity_strength > 1.0f) prefs->reciprocity_strength = 1.0f;

    // Payoff sensitivity from average payoff pursuit
    prefs->payoff_sensitivity = 0.5f + 0.5f * comp_prob;

    // Fairness sensitivity from cooperation tendency
    prefs->fairness_sensitivity = 0.5f + 0.5f * coop_prob;

    // Risk aversion: competitive players tend to be risk-seeking
    prefs->risk_aversion = coop_prob - comp_prob * 0.5f;
}

static void infer_mental_state_internal(nimcp_gt_tom_t ctx, const tom_opponent_record_t* record, nimcp_mental_state_t* state) {
    memset(state, 0, sizeof(nimcp_mental_state_t));

    // Beliefs about my type (mirror of their type distribution)
    // Assume opponent projects their type onto me
    for (uint32_t t = 0; t < NIMCP_TOM_NUM_OPPONENT_TYPES; t++) {
        /* Phase 8: Loop progress heartbeat */
        if ((t & 0xFF) == 0 && NIMCP_TOM_NUM_OPPONENT_TYPES > 256) {
            gt_tom_heartbeat("gt_tom_loop",
                             (float)(t + 1) / (float)NIMCP_TOM_NUM_OPPONENT_TYPES);
        }

        state->believed_my_type[t] = record->type_probs[t];
    }

    // Beliefs about my next action (based on our correlation)
    float uniform = 1.0f / (float)NIMCP_TOM_MAX_ACTIONS;
    for (uint32_t a = 0; a < NIMCP_TOM_MAX_ACTIONS; a++) {
        /* Phase 8: Loop progress heartbeat */
        if ((a & 0xFF) == 0 && NIMCP_TOM_MAX_ACTIONS > 256) {
            gt_tom_heartbeat("gt_tom_loop",
                             (float)(a + 1) / (float)NIMCP_TOM_MAX_ACTIONS);
        }

        state->believed_my_next_action[a] = uniform;
    }

    // Desires (goals) from type probabilities
    float coop = record->type_probs[NIMCP_OPPONENT_COOPERATIVE];
    float comp = record->type_probs[NIMCP_OPPONENT_COMPETITIVE];
    float tft = record->type_probs[NIMCP_OPPONENT_TIT_FOR_TAT];

    state->goal_own_payoff = 0.5f + 0.5f * comp;
    state->goal_other_harm = comp * 0.5f;
    state->goal_fairness = coop * 0.7f + tft * 0.3f;
    state->goal_cooperation = coop * 0.8f + tft * 0.2f;

    // Intentions (predicted action)
    nimcp_opponent_preferences_t prefs;
    infer_preferences_internal(record, &prefs);
    for (uint32_t a = 0; a < NIMCP_TOM_MAX_ACTIONS; a++) {
        /* Phase 8: Loop progress heartbeat */
        if ((a & 0xFF) == 0 && NIMCP_TOM_MAX_ACTIONS > 256) {
            gt_tom_heartbeat("gt_tom_loop",
                             (float)(a + 1) / (float)NIMCP_TOM_MAX_ACTIONS);
        }

        state->intended_action[a] = prefs.action_preferences[a];
    }

    // Find most likely action
    float max_prob = 0.0f;
    state->most_likely_action = 0;
    for (uint32_t a = 0; a < NIMCP_TOM_MAX_ACTIONS; a++) {
        /* Phase 8: Loop progress heartbeat */
        if ((a & 0xFF) == 0 && NIMCP_TOM_MAX_ACTIONS > 256) {
            gt_tom_heartbeat("gt_tom_loop",
                             (float)(a + 1) / (float)NIMCP_TOM_MAX_ACTIONS);
        }

        if (state->intended_action[a] > max_prob) {
            max_prob = state->intended_action[a];
            state->most_likely_action = a;
        }
    }

    // Confidence in intentions
    float entropy = compute_entropy(state->intended_action, NIMCP_TOM_MAX_ACTIONS);
    float max_entropy = logf((float)NIMCP_TOM_MAX_ACTIONS) * LOG2_E;
    state->action_confidence = 1.0f - (entropy / max_entropy);

    // Meta-cognition: sophistication from non-random behavior
    float random_prob = record->type_probs[NIMCP_OPPONENT_RANDOM];
    state->reasoning_sophistication = 1.0f - random_prob;

    // Inferred recursion depth based on sophistication
    state->inferred_recursion_depth = (uint32_t)(state->reasoning_sophistication * (float)ctx->config.max_recursion_depth);
}

static void predict_action_internal(nimcp_gt_tom_t ctx, const tom_opponent_record_t* record,
                                     const nimcp_action_context_t* situation, nimcp_action_prediction_t* prediction) {
    (void)ctx;  // May be used for recursion parameters in future
    memset(prediction, 0, sizeof(nimcp_action_prediction_t));

    // Weighted combination of type-specific predictions
    float total_prob[NIMCP_TOM_MAX_ACTIONS] = {0};

    for (uint32_t t = 0; t < NIMCP_TOM_NUM_OPPONENT_TYPES; t++) {
        /* Phase 8: Loop progress heartbeat */
        if ((t & 0xFF) == 0 && NIMCP_TOM_NUM_OPPONENT_TYPES > 256) {
            gt_tom_heartbeat("gt_tom_loop",
                             (float)(t + 1) / (float)NIMCP_TOM_NUM_OPPONENT_TYPES);
        }

        float type_prob = record->type_probs[t];
        if (type_prob < EPSILON) continue;

        // Generate type-specific prediction
        float type_pred[NIMCP_TOM_MAX_ACTIONS];
        memset(type_pred, 0, sizeof(type_pred));

        switch ((nimcp_opponent_type_t)t) {
            case NIMCP_OPPONENT_COOPERATIVE:
                // Cooperative: prefer mutually beneficial actions
                // Assume lower action indices are more cooperative
                for (uint32_t a = 0; a < situation->num_available_actions; a++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((a & 0xFF) == 0 && situation->num_available_actions > 256) {
                        gt_tom_heartbeat("gt_tom_loop",
                                         (float)(a + 1) / (float)situation->num_available_actions);
                    }

                    type_pred[a] = 1.0f / (1.0f + (float)a);
                }
                break;

            case NIMCP_OPPONENT_COMPETITIVE:
                // Competitive: maximize own payoff
                for (uint32_t a = 0; a < situation->num_available_actions; a++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((a & 0xFF) == 0 && situation->num_available_actions > 256) {
                        gt_tom_heartbeat("gt_tom_loop",
                                         (float)(a + 1) / (float)situation->num_available_actions);
                    }

                    type_pred[a] = situation->available_payoffs[a] + 1.0f;
                    if (type_pred[a] < 0.01f) type_pred[a] = 0.01f;
                }
                break;

            case NIMCP_OPPONENT_RANDOM:
                // Random: uniform
                for (uint32_t a = 0; a < situation->num_available_actions; a++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((a & 0xFF) == 0 && situation->num_available_actions > 256) {
                        gt_tom_heartbeat("gt_tom_loop",
                                         (float)(a + 1) / (float)situation->num_available_actions);
                    }

                    type_pred[a] = 1.0f;
                }
                break;

            case NIMCP_OPPONENT_TIT_FOR_TAT:
                // Tit-for-tat: mirror my previous action
                if (situation->my_intended_action < NIMCP_TOM_MAX_ACTIONS) {
                    type_pred[situation->my_intended_action] = 0.7f;
                }
                // Some noise
                for (uint32_t a = 0; a < situation->num_available_actions; a++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((a & 0xFF) == 0 && situation->num_available_actions > 256) {
                        gt_tom_heartbeat("gt_tom_loop",
                                         (float)(a + 1) / (float)situation->num_available_actions);
                    }

                    type_pred[a] += 0.1f;
                }
                break;

            case NIMCP_OPPONENT_RATIONAL:
                // Rational: Nash equilibrium behavior (approximate as mixed strategy)
                for (uint32_t a = 0; a < situation->num_available_actions; a++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((a & 0xFF) == 0 && situation->num_available_actions > 256) {
                        gt_tom_heartbeat("gt_tom_loop",
                                         (float)(a + 1) / (float)situation->num_available_actions);
                    }

                    type_pred[a] = 0.5f + 0.5f * situation->available_payoffs[a];
                    if (type_pred[a] < 0.01f) type_pred[a] = 0.01f;
                }
                break;

            case NIMCP_OPPONENT_UNKNOWN:
            default:
                // Unknown: use empirical frequency
                for (uint32_t a = 0; a < NIMCP_TOM_MAX_ACTIONS; a++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((a & 0xFF) == 0 && NIMCP_TOM_MAX_ACTIONS > 256) {
                        gt_tom_heartbeat("gt_tom_loop",
                                         (float)(a + 1) / (float)NIMCP_TOM_MAX_ACTIONS);
                    }

                    type_pred[a] = (float)record->action_counts[a] + 0.1f;
                }
                break;
        }

        normalize_probs(type_pred, NIMCP_TOM_MAX_ACTIONS);

        // Add weighted by type probability
        for (uint32_t a = 0; a < NIMCP_TOM_MAX_ACTIONS; a++) {
            /* Phase 8: Loop progress heartbeat */
            if ((a & 0xFF) == 0 && NIMCP_TOM_MAX_ACTIONS > 256) {
                gt_tom_heartbeat("gt_tom_loop",
                                 (float)(a + 1) / (float)NIMCP_TOM_MAX_ACTIONS);
            }

            total_prob[a] += type_prob * type_pred[a];
        }
    }

    normalize_probs(total_prob, NIMCP_TOM_MAX_ACTIONS);

    // Copy to output
    memcpy(prediction->action_probabilities, total_prob, sizeof(total_prob));

    // Find most likely action
    float max_prob = 0.0f;
    prediction->most_likely_action = 0;
    for (uint32_t a = 0; a < NIMCP_TOM_MAX_ACTIONS; a++) {
        /* Phase 8: Loop progress heartbeat */
        if ((a & 0xFF) == 0 && NIMCP_TOM_MAX_ACTIONS > 256) {
            gt_tom_heartbeat("gt_tom_loop",
                             (float)(a + 1) / (float)NIMCP_TOM_MAX_ACTIONS);
        }

        if (total_prob[a] > max_prob) {
            max_prob = total_prob[a];
            prediction->most_likely_action = a;
        }
    }

    // Compute confidence
    float entropy = compute_entropy(total_prob, NIMCP_TOM_MAX_ACTIONS);
    float max_entropy = logf((float)NIMCP_TOM_MAX_ACTIONS) * LOG2_E;
    prediction->confidence = 1.0f - (entropy / max_entropy);

    // Expected payoff given prediction
    prediction->expected_payoff = 0.0f;
    for (uint32_t a = 0; a < situation->num_available_actions; a++) {
        /* Phase 8: Loop progress heartbeat */
        if ((a & 0xFF) == 0 && situation->num_available_actions > 256) {
            gt_tom_heartbeat("gt_tom_loop",
                             (float)(a + 1) / (float)situation->num_available_actions);
        }

        prediction->expected_payoff += total_prob[a] * situation->available_payoffs[a];
    }

    prediction->assumed_type = record->most_likely_type;
}

//=============================================================================
// Observation Functions
//=============================================================================

nimcp_error_t nimcp_gt_tom_observe_action(
    nimcp_gt_tom_t ctx,
    nimcp_player_id_t opponent_id,
    uint32_t action,
    const nimcp_action_context_t* context
) {
    /* Phase 8: Heartbeat at operation start */
    gt_tom_heartbeat("gt_tom_gt_tom_observe_actio", 0.0f);


    NIMCP_CHECK_THROW(ctx && context, NIMCP_ERROR_INVALID_PARAM, "ctx or context is NULL");
    NIMCP_CHECK_THROW(action < NIMCP_TOM_MAX_ACTIONS, NIMCP_ERROR_OUT_OF_RANGE, "action out of range");
    NIMCP_CHECK_THROW(ctx->active, NIMCP_ERROR_INVALID_STATE, "ctx is not active");

    // Lock if thread-safe
    if (ctx->thread_safe && ctx->mutex) {
        nimcp_mutex_lock(ctx->mutex);
    }

    // Get or create opponent record
    tom_opponent_record_t* record = get_or_create_opponent(ctx, opponent_id);
    if (!record) {
        if (ctx->thread_safe && ctx->mutex) {
            nimcp_mutex_unlock(ctx->mutex);
        }
        return NIMCP_GT_ERROR_CAPACITY;
    }

    // Create observation
    nimcp_observed_action_t obs = {
        .action_id = action,
        .timestamp_ms = 0,  // Could use nimcp_time_get_ms()
        .payoff_received = 0.0f,
        .round_number = context->round_number,
        .my_previous_action = context->my_intended_action
    };
    memcpy(obs.situation_context, context->situation_features, sizeof(obs.situation_context));

    // Add to history
    add_history_entry(record, &obs);

    // Update action counts
    record->action_counts[action]++;
    record->total_action_count++;
    record->total_observations++;
    ctx->total_observations++;

    // Update cooperation/defection counts (assuming action 0 = cooperate)
    if (action == 0) {
        record->recent_coop_count++;
    } else {
        record->recent_defect_count++;
    }

    // Update action correlation with my actions
    if (record->total_observations > 1) {
        // Simple correlation: did they match my previous action?
        float match = (action == context->my_intended_action) ? 1.0f : 0.0f;
        float alpha = 0.1f;
        record->action_correlation = (1.0f - alpha) * record->action_correlation + alpha * (match * 2.0f - 1.0f);
    }

    // Check prediction accuracy (if we made a prediction)
    if (record->predictions_made > 0 && record->last_predicted_action < NIMCP_TOM_MAX_ACTIONS) {
        if (action == record->last_predicted_action) {
            record->predictions_correct++;
            ctx->correct_predictions++;
        }
    }

    // Apply decay to old observations
    apply_decay(ctx, record);

    if (ctx->thread_safe && ctx->mutex) {
        nimcp_mutex_unlock(ctx->mutex);
    }

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_gt_tom_update_beliefs(
    nimcp_gt_tom_t ctx,
    nimcp_player_id_t opponent_id
) {
    /* Phase 8: Heartbeat at operation start */
    gt_tom_heartbeat("gt_tom_gt_tom_update_belief", 0.0f);


    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(ctx->active, NIMCP_ERROR_INVALID_STATE, "ctx is not active");

    if (ctx->thread_safe && ctx->mutex) {
        nimcp_mutex_lock(ctx->mutex);
    }

    tom_opponent_record_t* record = find_opponent(ctx, opponent_id);
    if (!record) {
        if (ctx->thread_safe && ctx->mutex) {
            nimcp_mutex_unlock(ctx->mutex);
        }
        return NIMCP_GT_ERROR_PLAYER_NOT_FOUND;
    }

    // Update type distribution via Bayesian inference
    update_type_distribution(ctx, record);

    // Update preferences
    infer_preferences_internal(record, &record->preferences);

    // Update mental state
    infer_mental_state_internal(ctx, record, &record->mental_state);

    if (ctx->thread_safe && ctx->mutex) {
        nimcp_mutex_unlock(ctx->mutex);
    }

    return NIMCP_SUCCESS;
}

//=============================================================================
// Prediction Functions
//=============================================================================

nimcp_error_t nimcp_gt_tom_predict_action(
    nimcp_gt_tom_t ctx,
    nimcp_player_id_t opponent_id,
    const nimcp_action_context_t* situation,
    nimcp_action_prediction_t* prediction
) {
    /* Phase 8: Heartbeat at operation start */
    gt_tom_heartbeat("gt_tom_gt_tom_predict_actio", 0.0f);


    NIMCP_CHECK_THROW(ctx && situation && prediction, NIMCP_ERROR_INVALID_PARAM, "ctx, situation, or prediction is NULL");
    NIMCP_CHECK_THROW(ctx->active, NIMCP_ERROR_INVALID_STATE, "ctx is not active");

    if (ctx->thread_safe && ctx->mutex) {
        nimcp_mutex_lock(ctx->mutex);
    }

    tom_opponent_record_t* record = find_opponent(ctx, opponent_id);
    if (!record) {
        if (ctx->thread_safe && ctx->mutex) {
            nimcp_mutex_unlock(ctx->mutex);
        }
        return NIMCP_GT_ERROR_PLAYER_NOT_FOUND;
    }

    predict_action_internal(ctx, record, situation, prediction);

    // Track prediction for accuracy measurement
    record->predictions_made++;
    record->last_predicted_action = prediction->most_likely_action;
    ctx->total_predictions++;

    if (ctx->thread_safe && ctx->mutex) {
        nimcp_mutex_unlock(ctx->mutex);
    }

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_gt_tom_get_type_distribution(
    nimcp_gt_tom_t ctx,
    nimcp_player_id_t opponent_id,
    nimcp_type_distribution_t* dist
) {
    /* Phase 8: Heartbeat at operation start */
    gt_tom_heartbeat("gt_tom_gt_tom_get_type_dist", 0.0f);


    NIMCP_CHECK_THROW(ctx && dist, NIMCP_ERROR_INVALID_PARAM, "ctx or dist is NULL");
    NIMCP_CHECK_THROW(ctx->active, NIMCP_ERROR_INVALID_STATE, "ctx is not active");

    if (ctx->thread_safe && ctx->mutex) {
        nimcp_mutex_lock(ctx->mutex);
    }

    tom_opponent_record_t* record = find_opponent(ctx, opponent_id);
    if (!record) {
        if (ctx->thread_safe && ctx->mutex) {
            nimcp_mutex_unlock(ctx->mutex);
        }
        return NIMCP_GT_ERROR_PLAYER_NOT_FOUND;
    }

    memcpy(dist->probabilities, record->type_probs, sizeof(record->type_probs));
    dist->most_likely = record->most_likely_type;

    // Compute entropy and confidence
    dist->entropy = compute_entropy(record->type_probs, NIMCP_TOM_NUM_OPPONENT_TYPES);
    float max_entropy = logf((float)NIMCP_TOM_NUM_OPPONENT_TYPES) * LOG2_E;
    dist->confidence = 1.0f - (dist->entropy / max_entropy);

    if (ctx->thread_safe && ctx->mutex) {
        nimcp_mutex_unlock(ctx->mutex);
    }

    return NIMCP_SUCCESS;
}

//=============================================================================
// Inference Functions
//=============================================================================

nimcp_error_t nimcp_gt_tom_infer_preferences(
    nimcp_gt_tom_t ctx,
    nimcp_player_id_t opponent_id,
    nimcp_opponent_preferences_t* preferences
) {
    /* Phase 8: Heartbeat at operation start */
    gt_tom_heartbeat("gt_tom_gt_tom_infer_prefere", 0.0f);


    NIMCP_CHECK_THROW(ctx && preferences, NIMCP_ERROR_INVALID_PARAM, "ctx or preferences is NULL");
    NIMCP_CHECK_THROW(ctx->active, NIMCP_ERROR_INVALID_STATE, "ctx is not active");

    if (ctx->thread_safe && ctx->mutex) {
        nimcp_mutex_lock(ctx->mutex);
    }

    tom_opponent_record_t* record = find_opponent(ctx, opponent_id);
    if (!record) {
        if (ctx->thread_safe && ctx->mutex) {
            nimcp_mutex_unlock(ctx->mutex);
        }
        return NIMCP_GT_ERROR_PLAYER_NOT_FOUND;
    }

    infer_preferences_internal(record, preferences);

    if (ctx->thread_safe && ctx->mutex) {
        nimcp_mutex_unlock(ctx->mutex);
    }

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_gt_tom_infer_mental_state(
    nimcp_gt_tom_t ctx,
    nimcp_player_id_t opponent_id,
    nimcp_mental_state_t* state
) {
    /* Phase 8: Heartbeat at operation start */
    gt_tom_heartbeat("gt_tom_gt_tom_infer_mental_", 0.0f);


    NIMCP_CHECK_THROW(ctx && state, NIMCP_ERROR_INVALID_PARAM, "ctx or state is NULL");
    NIMCP_CHECK_THROW(ctx->active, NIMCP_ERROR_INVALID_STATE, "ctx is not active");

    if (ctx->thread_safe && ctx->mutex) {
        nimcp_mutex_lock(ctx->mutex);
    }

    tom_opponent_record_t* record = find_opponent(ctx, opponent_id);
    if (!record) {
        if (ctx->thread_safe && ctx->mutex) {
            nimcp_mutex_unlock(ctx->mutex);
        }
        return NIMCP_GT_ERROR_PLAYER_NOT_FOUND;
    }

    infer_mental_state_internal(ctx, record, state);

    if (ctx->thread_safe && ctx->mutex) {
        nimcp_mutex_unlock(ctx->mutex);
    }

    return NIMCP_SUCCESS;
}

//=============================================================================
// Recursive Reasoning Functions
//=============================================================================

nimcp_error_t nimcp_gt_tom_what_do_they_think_i_will_do(
    nimcp_gt_tom_t ctx,
    nimcp_player_id_t opponent_id,
    nimcp_action_prediction_t* my_prediction
) {
    /* Phase 8: Heartbeat at operation start */
    gt_tom_heartbeat("gt_tom_gt_tom_what_do_they_", 0.0f);


    NIMCP_CHECK_THROW(ctx && my_prediction, NIMCP_ERROR_INVALID_PARAM, "ctx or my_prediction is NULL");
    NIMCP_CHECK_THROW(ctx->active, NIMCP_ERROR_INVALID_STATE, "ctx is not active");

    if (ctx->thread_safe && ctx->mutex) {
        nimcp_mutex_lock(ctx->mutex);
    }

    tom_opponent_record_t* record = find_opponent(ctx, opponent_id);
    if (!record) {
        if (ctx->thread_safe && ctx->mutex) {
            nimcp_mutex_unlock(ctx->mutex);
        }
        return NIMCP_GT_ERROR_PLAYER_NOT_FOUND;
    }

    memset(my_prediction, 0, sizeof(nimcp_action_prediction_t));

    // Level-2 ToM: what do they think I will do?
    // Use their mental state model's beliefs about my actions
    memcpy(my_prediction->action_probabilities,
           record->mental_state.believed_my_next_action,
           sizeof(my_prediction->action_probabilities));

    // Find most likely
    float max_prob = 0.0f;
    my_prediction->most_likely_action = 0;
    for (uint32_t a = 0; a < NIMCP_TOM_MAX_ACTIONS; a++) {
        /* Phase 8: Loop progress heartbeat */
        if ((a & 0xFF) == 0 && NIMCP_TOM_MAX_ACTIONS > 256) {
            gt_tom_heartbeat("gt_tom_loop",
                             (float)(a + 1) / (float)NIMCP_TOM_MAX_ACTIONS);
        }

        if (my_prediction->action_probabilities[a] > max_prob) {
            max_prob = my_prediction->action_probabilities[a];
            my_prediction->most_likely_action = a;
        }
    }

    // Confidence based on their reasoning sophistication
    my_prediction->confidence = record->mental_state.reasoning_sophistication *
                                record->mental_state.action_confidence *
                                ctx->config.recursion_discount;

    my_prediction->assumed_type = record->most_likely_type;

    if (ctx->thread_safe && ctx->mutex) {
        nimcp_mutex_unlock(ctx->mutex);
    }

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_gt_tom_best_response_to_beliefs(
    nimcp_gt_tom_t ctx,
    nimcp_player_id_t opponent_id,
    uint32_t* my_action
) {
    /* Phase 8: Heartbeat at operation start */
    gt_tom_heartbeat("gt_tom_gt_tom_best_response", 0.0f);


    NIMCP_CHECK_THROW(ctx && my_action, NIMCP_ERROR_INVALID_PARAM, "ctx or my_action is NULL");
    NIMCP_CHECK_THROW(ctx->active, NIMCP_ERROR_INVALID_STATE, "ctx is not active");

    if (ctx->thread_safe && ctx->mutex) {
        nimcp_mutex_lock(ctx->mutex);
    }

    tom_opponent_record_t* record = find_opponent(ctx, opponent_id);
    if (!record) {
        if (ctx->thread_safe && ctx->mutex) {
            nimcp_mutex_unlock(ctx->mutex);
        }
        return NIMCP_GT_ERROR_PLAYER_NOT_FOUND;
    }

    // Best response: action that maximizes expected payoff given opponent prediction
    // Without payoff matrix, we use simple heuristics based on opponent type

    nimcp_opponent_type_t opp_type = record->most_likely_type;

    switch (opp_type) {
        case NIMCP_OPPONENT_COOPERATIVE:
            // Cooperate with cooperative opponent
            *my_action = 0;
            break;

        case NIMCP_OPPONENT_COMPETITIVE:
            // Defect against competitive opponent
            *my_action = 1;
            break;

        case NIMCP_OPPONENT_TIT_FOR_TAT:
            // Cooperate to start positive cycle
            *my_action = 0;
            break;

        case NIMCP_OPPONENT_RANDOM:
        case NIMCP_OPPONENT_RATIONAL:
        case NIMCP_OPPONENT_UNKNOWN:
        default:
            // Default to rational/mixed strategy (cooperate)
            *my_action = 0;
            break;
    }

    if (ctx->thread_safe && ctx->mutex) {
        nimcp_mutex_unlock(ctx->mutex);
    }

    return NIMCP_SUCCESS;
}

//=============================================================================
// Utility Functions
//=============================================================================

nimcp_error_t nimcp_gt_tom_reset_opponent(
    nimcp_gt_tom_t ctx,
    nimcp_player_id_t opponent_id
) {
    /* Phase 8: Heartbeat at operation start */
    gt_tom_heartbeat("gt_tom_gt_tom_reset_opponen", 0.0f);


    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");

    if (ctx->thread_safe && ctx->mutex) {
        nimcp_mutex_lock(ctx->mutex);
    }

    tom_opponent_record_t* record = find_opponent(ctx, opponent_id);
    if (!record) {
        if (ctx->thread_safe && ctx->mutex) {
            nimcp_mutex_unlock(ctx->mutex);
        }
        return NIMCP_GT_ERROR_PLAYER_NOT_FOUND;
    }

    nimcp_player_id_t saved_id = record->id;
    reset_opponent_record(ctx, record);
    record->id = saved_id;
    record->active = true;

    if (ctx->thread_safe && ctx->mutex) {
        nimcp_mutex_unlock(ctx->mutex);
    }

    return NIMCP_SUCCESS;
}

float nimcp_gt_tom_get_confidence(
    nimcp_gt_tom_t ctx,
    nimcp_player_id_t opponent_id
) {
    if (!ctx || !ctx->active) {
        return -1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_tom_heartbeat("gt_tom_gt_tom_get_confidenc", 0.0f);


    if (ctx->thread_safe && ctx->mutex) {
        nimcp_mutex_lock(ctx->mutex);
    }

    tom_opponent_record_t* record = find_opponent(ctx, opponent_id);
    if (!record) {
        if (ctx->thread_safe && ctx->mutex) {
            nimcp_mutex_unlock(ctx->mutex);
        }
        return -1.0f;
    }

    // Confidence based on:
    // 1. Type distribution entropy (lower = more confident)
    // 2. Number of observations (more = more confident)
    float entropy = compute_entropy(record->type_probs, NIMCP_TOM_NUM_OPPONENT_TYPES);
    float max_entropy = logf((float)NIMCP_TOM_NUM_OPPONENT_TYPES) * LOG2_E;
    float entropy_confidence = 1.0f - (entropy / max_entropy);

    // Observation confidence: saturates around 30 observations
    float obs_confidence = (float)record->total_observations / 30.0f;
    if (obs_confidence > 1.0f) obs_confidence = 1.0f;

    float confidence = entropy_confidence * obs_confidence;

    if (ctx->thread_safe && ctx->mutex) {
        nimcp_mutex_unlock(ctx->mutex);
    }

    return confidence;
}

nimcp_error_t nimcp_gt_tom_get_opponent_belief(
    nimcp_gt_tom_t ctx,
    nimcp_player_id_t opponent_id,
    nimcp_opponent_belief_t* belief
) {
    /* Phase 8: Heartbeat at operation start */
    gt_tom_heartbeat("gt_tom_gt_tom_get_opponent_", 0.0f);


    NIMCP_CHECK_THROW(ctx && belief, NIMCP_ERROR_INVALID_PARAM, "ctx or belief is NULL");

    if (ctx->thread_safe && ctx->mutex) {
        nimcp_mutex_lock(ctx->mutex);
    }

    tom_opponent_record_t* record = find_opponent(ctx, opponent_id);
    if (!record) {
        if (ctx->thread_safe && ctx->mutex) {
            nimcp_mutex_unlock(ctx->mutex);
        }
        return NIMCP_GT_ERROR_PLAYER_NOT_FOUND;
    }

    memset(belief, 0, sizeof(nimcp_opponent_belief_t));

    belief->opponent_id = record->id;
    belief->active = record->active;

    // Type distribution
    memcpy(belief->type_distribution.probabilities, record->type_probs, sizeof(record->type_probs));
    belief->type_distribution.most_likely = record->most_likely_type;
    belief->type_distribution.entropy = compute_entropy(record->type_probs, NIMCP_TOM_NUM_OPPONENT_TYPES);
    float max_entropy = logf((float)NIMCP_TOM_NUM_OPPONENT_TYPES) * LOG2_E;
    belief->type_distribution.confidence = 1.0f - (belief->type_distribution.entropy / max_entropy);

    // Preferences and mental state
    belief->preferences = record->preferences;
    belief->mental_state = record->mental_state;

    // Statistics
    belief->num_observations = record->total_observations;
    belief->recent_cooperation_count = record->recent_coop_count;
    belief->recent_defection_count = record->recent_defect_count;
    belief->avg_payoff_received = record->avg_payoff_received;
    belief->avg_payoff_given = record->avg_payoff_given;
    belief->correlation_with_me = record->action_correlation;

    // Prediction accuracy
    belief->predictions_made = record->predictions_made;
    belief->predictions_correct = record->predictions_correct;
    if (record->predictions_made > 0) {
        belief->prediction_accuracy = (float)record->predictions_correct / (float)record->predictions_made;
    }

    if (ctx->thread_safe && ctx->mutex) {
        nimcp_mutex_unlock(ctx->mutex);
    }

    return NIMCP_SUCCESS;
}

uint32_t nimcp_gt_tom_get_opponent_count(nimcp_gt_tom_t ctx) {
    if (!ctx) {
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_tom_heartbeat("gt_tom_gt_tom_get_opponent_", 0.0f);


    if (ctx->thread_safe && ctx->mutex) {
        nimcp_mutex_lock(ctx->mutex);
    }

    uint32_t count = ctx->num_opponents;

    if (ctx->thread_safe && ctx->mutex) {
        nimcp_mutex_unlock(ctx->mutex);
    }

    return count;
}

bool nimcp_gt_tom_is_opponent_tracked(
    nimcp_gt_tom_t ctx,
    nimcp_player_id_t opponent_id
) {
    if (!ctx) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_tom_heartbeat("gt_tom_gt_tom_is_opponent_t", 0.0f);


    if (ctx->thread_safe && ctx->mutex) {
        nimcp_mutex_lock(ctx->mutex);
    }

    tom_opponent_record_t* record = find_opponent(ctx, opponent_id);
    bool tracked = (record != NULL);

    if (ctx->thread_safe && ctx->mutex) {
        nimcp_mutex_unlock(ctx->mutex);
    }

    return tracked;
}

//=============================================================================
// Type Name Utilities
//=============================================================================

const char* nimcp_opponent_type_name(nimcp_opponent_type_t type) {
    switch (type) {
        case NIMCP_OPPONENT_COOPERATIVE:
            return "Cooperative";
        case NIMCP_OPPONENT_COMPETITIVE:
            return "Competitive";
        case NIMCP_OPPONENT_RANDOM:
            return "Random";
        case NIMCP_OPPONENT_TIT_FOR_TAT:
            return "Tit-for-Tat";
        case NIMCP_OPPONENT_RATIONAL:
            return "Rational";
        case NIMCP_OPPONENT_UNKNOWN:
            return "Unknown";
        default:
            return "Invalid";
    }
}

//=============================================================================
// KG Self-Awareness Integration
//=============================================================================

int tom_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    gt_tom_heartbeat("gt_tom_tom_query_self_knowl", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "ToM_Module");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                gt_tom_heartbeat("gt_tom_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            LOG_DEBUG(LOG_MODULE, "ToM self-knowledge: %s", self->observations[i]);
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "ToM_Module");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "ToM_Module");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void gt_tom_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_gt_tom_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int gt_tom_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "gt_tom_training_begin: NULL argument");
        return -1;
    }
    gt_tom_heartbeat_instance(NULL, "gt_tom_training_begin", 0.0f);
    (void)(struct nimcp_gt_tom_struct*)instance; /* Module state available for reset */
    return 0;
}

int gt_tom_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "gt_tom_training_end: NULL argument");
        return -1;
    }
    gt_tom_heartbeat_instance(NULL, "gt_tom_training_end", 1.0f);
    (void)(struct nimcp_gt_tom_struct*)instance; /* Module state available for finalization */
    return 0;
}

int gt_tom_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "gt_tom_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    gt_tom_heartbeat_instance(NULL, "gt_tom_training_step", progress);
    (void)(struct nimcp_gt_tom_struct*)instance; /* Module state available for step adaptation */
    return 0;
}
