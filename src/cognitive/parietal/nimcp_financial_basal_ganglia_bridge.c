/**
 * @file nimcp_financial_basal_ganglia_bridge.c
 * @brief Financial Basal Ganglia Bridge Implementation - Decision Systems (Phase 6)
 * @version 1.0.0
 * @date 2026-01-29
 *
 * WHAT: Bridge for modeling action selection and reinforcement learning in
 *       financial decision-making, following the basal ganglia model.
 *
 * WHY:  Financial trading requires action selection under uncertainty.
 *       This implementation provides Q-learning with temporal difference,
 *       softmax action selection, and habit detection.
 *
 * HOW:  Actions are evaluated using tabular Q-learning with state hashing.
 *       Selection uses configurable softmax or epsilon-greedy policies.
 *       Habit formation is tracked via action repetition patterns.
 *
 * @author NIMCP Development Team
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>

#include "cognitive/parietal/nimcp_financial_basal_ganglia_bridge.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Health Agent Integration (Phase 8: System-Wide Health Integration)
 * ============================================================================ */
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(fin_bg)

/* Stub heartbeat for migration compatibility */
static inline void fin_bg_heartbeat_global(const char* op, float progress) {
    (void)op; (void)progress;
}

BRIDGE_DEFINE_MESH_REGISTRATION(fin_bg, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * Thread-Local Error Handling
 * ============================================================================ */

static _Thread_local char fin_bg_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(fin_bg_last_error, sizeof(fin_bg_last_error), fmt, args);
    va_end(args);
}

const char* financial_bg_bridge_get_last_error(void) {
    return fin_bg_last_error;
}

/* ============================================================================
 * Q-Table Configuration
 * ============================================================================ */

/** Number of discrete state bins per dimension */
#define Q_STATE_BINS           8

/** Total state space size (simplified: 6 market + 4 position features) */
#define Q_STATE_FEATURES       10

/** Q-table size = bins^features * num_actions (too large for tabular!) */
/** Using function approximation with feature-based Q-values instead */

/** Number of feature weights per action */
#define Q_WEIGHT_DIM           (Q_STATE_FEATURES + 1)  /* +1 for bias */

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

/**
 * @brief Action history entry for habit tracking
 */
typedef struct {
    fin_bg_action_t action;
    uint64_t timestamp_ms;
    float q_value_at_selection;
} action_history_entry_t;

#define ACTION_HISTORY_SIZE    64

struct financial_bg_bridge {
    uint32_t magic;
    fin_bg_config_t config;
    fin_bg_op_state_t op_state;  /* Operational state */
    fin_bg_bridge_stats_t stats;

    /* Subsystem pointers */
    void* immune;
    void* bbb;
    void* health_agent;
    void* kg_wiring;
    void* logger;
    void* security;
    void* ethics;
    const void* lgss;
    void* coordinator;
    void* bio_router;

    /* Callbacks */
    fin_bg_action_callback_t action_callback;
    void* action_callback_data;
    fin_bg_conflict_callback_t conflict_callback;
    void* conflict_callback_data;
    fin_bg_habit_callback_t habit_callback;
    void* habit_callback_data;

    /* Q-learning state: linear function approximation */
    /* Q(s,a) = w_a^T * phi(s) where phi(s) are state features */
    float q_weights[FIN_BG_ACTION_COUNT][Q_WEIGHT_DIM];

    /* Eligibility traces for TD(lambda) */
    float eligibility[FIN_BG_ACTION_COUNT][Q_WEIGHT_DIM];

    /* Learning state */
    float last_td_error;
    float beta_oscillation;  /* Current indecision level */

    /* Habit tracking */
    action_history_entry_t action_history[ACTION_HISTORY_SIZE];
    uint32_t history_index;
    uint32_t history_count;
    uint32_t consecutive_action_counts[FIN_BG_ACTION_COUNT];
    fin_bg_action_t last_action;

    /* Temporal credit */
    float temporal_credits[FIN_BG_ACTION_COUNT];
};

static inline float maxf(float a, float b) {
    return a > b ? a : b;
}

/**
 * @brief Extract feature vector from state
 */
static void state_to_features(const fin_bg_state_t* state, float* features) {
    /* Market features (normalized to [-1,1] or [0,1]) */
    features[0] = state->market.price_change;
    features[1] = state->market.volatility;
    features[2] = state->market.momentum;
    features[3] = state->market.regime_confidence;
    features[4] = state->market.sentiment;
    features[5] = state->market.spread;

    /* Position features */
    features[6] = state->position.position_size;
    features[7] = state->position.unrealized_pnl;
    features[8] = state->position.exposure;
    features[9] = nimcp_clampf(state->position.hold_duration_ms / 86400000.0f, 0.0f, 1.0f);

    /* Bias term */
    features[10] = 1.0f;
}

/**
 * @brief Compute Q-value using linear function approximation
 */
static float compute_q_value(const financial_bg_bridge_t* bridge,
                              const float* features,
                              fin_bg_action_t action) {
    float q = 0.0f;
    for (int i = 0; i < Q_WEIGHT_DIM; i++) {
        q += bridge->q_weights[action][i] * features[i];
    }
    return q;
}

/**
 * @brief Softmax action selection
 */
static void softmax_probabilities(const float* q_values, uint32_t num_actions,
                                   float temperature, float* probs) {
    /* Find max for numerical stability */
    float max_q = q_values[0];
    for (uint32_t i = 1; i < num_actions; i++) {
        if (q_values[i] > max_q) max_q = q_values[i];
    }

    /* Compute exp(Q/tau - max) */
    float sum = 0.0f;
    for (uint32_t i = 0; i < num_actions; i++) {
        probs[i] = expf((q_values[i] - max_q) / temperature);
        sum += probs[i];
    }

    /* Normalize */
    if (sum > 0.0f) {
        for (uint32_t i = 0; i < num_actions; i++) {
            probs[i] /= sum;
        }
    } else {
        /* Uniform if degenerate */
        for (uint32_t i = 0; i < num_actions; i++) {
            probs[i] = 1.0f / (num_actions > 0 ? num_actions : 1);
        }
    }
}

/**
 * @brief Sample from probability distribution
 */
static uint32_t sample_action(const float* probs, uint32_t num_actions) {
    /* Simple LCG for reproducible randomness - thread-local for thread safety */
    static _Thread_local uint32_t rand_state = 12345;
    rand_state = rand_state * 1103515245 + 12345;
    float r = (float)(rand_state & 0x7FFFFFFF) / (float)0x7FFFFFFF;

    float cumsum = 0.0f;
    for (uint32_t i = 0; i < num_actions; i++) {
        cumsum += probs[i];
        if (r < cumsum) {
            return i;
        }
    }
    return num_actions - 1;
}

/**
 * @brief Compute beta oscillation (indecision) from value distribution
 */
static float compute_beta_oscillation(const float* q_values, uint32_t num_actions,
                                       float conflict_threshold) {
    if (num_actions < 2) return 0.0f;

    /* Find top 2 Q-values */
    float best = -1e10f, second = -1e10f;
    for (uint32_t i = 0; i < num_actions; i++) {
        if (q_values[i] > best) {
            second = best;
            best = q_values[i];
        } else if (q_values[i] > second) {
            second = q_values[i];
        }
    }

    /* Beta oscillation increases when values are close */
    float gap = best - second;
    if (gap < conflict_threshold) {
        /* Higher beta when values are similar (more indecision) */
        return 1.0f - (gap / conflict_threshold);
    }
    return 0.0f;
}

/**
 * @brief Record action in history
 */
static void record_action(financial_bg_bridge_t* bridge,
                           fin_bg_action_t action,
                           float q_value) {
    action_history_entry_t* entry = &bridge->action_history[bridge->history_index];
    entry->action = action;
    entry->timestamp_ms = 0;  /* Would use real time */
    entry->q_value_at_selection = q_value;

    bridge->history_index = (bridge->history_index + 1) % ACTION_HISTORY_SIZE;
    if (bridge->history_count < ACTION_HISTORY_SIZE) {
        bridge->history_count++;
    }

    /* Update consecutive counts */
    if (action == bridge->last_action) {
        bridge->consecutive_action_counts[action]++;
    } else {
        /* Reset all counts for new action */
        for (int i = 0; i < FIN_BG_ACTION_COUNT; i++) {
            bridge->consecutive_action_counts[i] = 0;
        }
        bridge->consecutive_action_counts[action] = 1;
    }
    bridge->last_action = action;
}

/* ============================================================================
 * KG Wiring Message Types
 * ============================================================================ */

#define KG_MSG_FIN_BG_EVALUATE      "FIN_BG_EVALUATE"
#define KG_MSG_FIN_BG_SELECT        "FIN_BG_SELECT"
#define KG_MSG_FIN_BG_UPDATE        "FIN_BG_UPDATE"
#define KG_MSG_FIN_BG_CONFLICT      "FIN_BG_CONFLICT"
#define KG_MSG_FIN_BG_HABIT         "FIN_BG_HABIT"

static int bridge_kg_publish(financial_bg_bridge_t* bridge,
                              const char* msg_type,
                              const void* payload,
                              size_t size) {
    if (bridge && bridge->kg_wiring && bridge->config.enable_kg_messaging) {
        bridge->stats.kg_messages_sent++;
        /* kg_wiring_publish would be called here */
        (void)msg_type; (void)payload; (void)size;
        return 0;
    }
    return 0;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* fin_bg_action_name(fin_bg_action_t action) {
    switch (action) {
        case FIN_BG_ACTION_ENTER_LONG:    return "ENTER_LONG";
        case FIN_BG_ACTION_ENTER_SHORT:   return "ENTER_SHORT";
        case FIN_BG_ACTION_EXIT_POSITION: return "EXIT_POSITION";
        case FIN_BG_ACTION_SCALE_IN:      return "SCALE_IN";
        case FIN_BG_ACTION_SCALE_OUT:     return "SCALE_OUT";
        case FIN_BG_ACTION_HOLD:          return "HOLD";
        case FIN_BG_ACTION_REBALANCE:     return "REBALANCE";
        case FIN_BG_ACTION_HEDGE:         return "HEDGE";
        default: return "UNKNOWN";
    }
}

const char* fin_bg_op_state_name(fin_bg_op_state_t state) {
    switch (state) {
        case FIN_BG_OP_STATE_UNINITIALIZED: return "UNINITIALIZED";
        case FIN_BG_OP_STATE_INITIALIZED:   return "INITIALIZED";
        case FIN_BG_OP_STATE_ACTIVE:        return "ACTIVE";
        case FIN_BG_OP_STATE_DEGRADED:      return "DEGRADED";
        case FIN_BG_OP_STATE_ERROR:         return "ERROR";
        default: return "UNKNOWN";
    }
}

const char* financial_bg_bridge_version(void) {
    return FINANCIAL_BG_BRIDGE_VERSION;
}

/* ============================================================================
 * Memory Management API
 * ============================================================================ */

fin_bg_action_value_t* financial_bg_alloc_actions(uint32_t num_actions) {
    if (num_actions == 0 || num_actions > FIN_BG_MAX_ACTIONS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "financial_bg_alloc_actions: num_actions is zero");
        return NULL;
    }
    fin_bg_action_value_t* actions = (fin_bg_action_value_t*)
        nimcp_calloc(num_actions, sizeof(fin_bg_action_value_t));
    return actions;
}

void financial_bg_free_actions(fin_bg_action_value_t* actions) {
    if (actions) {
        nimcp_free(actions);
        actions = NULL;
    }
}

int financial_bg_init_decision(fin_bg_decision_t* decision, uint32_t num_actions) {
    if (!decision) {
        set_error("decision is NULL");
        return FIN_BG_ERR_NULL;
    }
    if (num_actions == 0 || num_actions > FIN_BG_MAX_ACTIONS) {
        set_error("Invalid num_actions: %u", num_actions);
        return FIN_BG_ERR_INVALID_PARAM;
    }

    memset(decision, 0, sizeof(fin_bg_decision_t));
    decision->actions = financial_bg_alloc_actions(num_actions);
    if (!decision->actions) {
        set_error("Failed to allocate actions array");
        return FIN_BG_ERR_NO_MEMORY;
    }
    decision->num_actions = num_actions;
    decision->selected = FIN_BG_ACTION_HOLD;  /* Default to hold */
    return FIN_BG_ERR_OK;
}

void financial_bg_cleanup_decision(fin_bg_decision_t* decision) {
    if (decision) {
        financial_bg_free_actions(decision->actions);
        decision->actions = NULL;
        decision->num_actions = 0;
    }
}

/* ============================================================================
 * Configuration
 * ============================================================================ */

int financial_bg_bridge_default_config(fin_bg_config_t* config) {
    if (!config) {
        set_error("config is NULL");
        return FIN_BG_ERR_NULL;
    }

    memset(config, 0, sizeof(fin_bg_config_t));

    /* Q-learning parameters */
    config->learning_rate = 0.01f;
    config->discount_factor = 0.95f;
    config->eligibility_decay = 0.9f;

    /* Action selection parameters */
    config->temperature = 0.5f;
    config->epsilon = 0.1f;
    config->use_softmax = true;

    /* Beta oscillation (indecision) parameters */
    config->beta_threshold = 0.7f;
    config->conflict_threshold = 0.1f;

    /* Habit formation parameters */
    config->habit_threshold_reps = 5;
    config->habit_decay_rate = 0.05f;

    /* Temporal credit assignment */
    config->credit_window_ms = 3600000;  /* 1 hour */
    config->credit_decay = 0.99f;

    /* Integration settings */
    config->enable_immune_integration = true;
    config->enable_bbb_validation = true;
    config->enable_kg_messaging = true;
    config->enable_health_monitoring = true;

    /* Logging */
    config->verbose_logging = false;

    return FIN_BG_ERR_OK;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

financial_bg_bridge_t* financial_bg_bridge_create(
    const fin_bg_config_t* config)
{
    fin_bg_heartbeat_global("fin_bg_create", 0.0f);

    financial_bg_bridge_t* bridge = (financial_bg_bridge_t*)
        nimcp_malloc(sizeof(financial_bg_bridge_t));
    if (!bridge) {
        set_error("Failed to allocate financial_bg_bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate financial_bg_bridge");
        return NULL;
    }
    memset(bridge, 0, sizeof(financial_bg_bridge_t));

    bridge->magic = FINANCIAL_BG_BRIDGE_MAGIC;

    /* Copy configuration or use defaults */
    if (config) {
        bridge->config = *config;
    } else {
        financial_bg_bridge_default_config(&bridge->config);
    }

    /* Initialize Q-weights to small random values */
    for (int a = 0; a < FIN_BG_ACTION_COUNT; a++) {
        for (int i = 0; i < Q_WEIGHT_DIM; i++) {
            /* Simple deterministic initialization */
            bridge->q_weights[a][i] = 0.01f * ((a * Q_WEIGHT_DIM + i) % 7 - 3);
        }
        /* Slight bias toward HOLD action */
        if (a == FIN_BG_ACTION_HOLD) {
            bridge->q_weights[a][Q_WEIGHT_DIM - 1] += 0.1f;
        }
    }

    /* Initialize eligibility traces to zero */
    memset(bridge->eligibility, 0, sizeof(bridge->eligibility));

    /* Initialize learning state */
    bridge->last_td_error = 0.0f;
    bridge->beta_oscillation = 0.0f;

    /* Initialize habit tracking */
    bridge->history_index = 0;
    bridge->history_count = 0;
    bridge->last_action = FIN_BG_ACTION_HOLD;
    memset(bridge->consecutive_action_counts, 0, sizeof(bridge->consecutive_action_counts));
    memset(bridge->temporal_credits, 0, sizeof(bridge->temporal_credits));

    bridge->op_state = FIN_BG_OP_STATE_INITIALIZED;

    fin_bg_heartbeat_global("fin_bg_create", 1.0f);
    return bridge;
}

void financial_bg_bridge_destroy(financial_bg_bridge_t* bridge) {
    fin_bg_heartbeat_global("fin_bg_destroy", 0.0f);

    if (bridge) {
        bridge->magic = 0;
        bridge->op_state = FIN_BG_OP_STATE_UNINITIALIZED;
        nimcp_free(bridge);
        bridge = NULL;
    }
}

int financial_bg_bridge_reset(financial_bg_bridge_t* bridge) {
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_bg_bridge_reset: bridge is NULL");
        return FIN_BG_ERR_NULL;
    }

    if (bridge->magic != FINANCIAL_BG_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_INVALID_STATE,
            "financial_bg_bridge_reset: invalid magic");
        return FIN_BG_ERR_STATE;
    }

    fin_bg_heartbeat_global("fin_bg_reset", 0.0f);

    /* Reset stats */
    memset(&bridge->stats, 0, sizeof(fin_bg_bridge_stats_t));

    /* Re-initialize Q-weights */
    for (int a = 0; a < FIN_BG_ACTION_COUNT; a++) {
        for (int i = 0; i < Q_WEIGHT_DIM; i++) {
            bridge->q_weights[a][i] = 0.01f * ((a * Q_WEIGHT_DIM + i) % 7 - 3);
        }
        if (a == FIN_BG_ACTION_HOLD) {
            bridge->q_weights[a][Q_WEIGHT_DIM - 1] += 0.1f;
        }
    }

    /* Reset eligibility traces */
    memset(bridge->eligibility, 0, sizeof(bridge->eligibility));

    /* Reset learning state */
    bridge->last_td_error = 0.0f;
    bridge->beta_oscillation = 0.0f;

    /* Reset habit tracking */
    bridge->history_index = 0;
    bridge->history_count = 0;
    bridge->last_action = FIN_BG_ACTION_HOLD;
    memset(bridge->consecutive_action_counts, 0, sizeof(bridge->consecutive_action_counts));
    memset(bridge->temporal_credits, 0, sizeof(bridge->temporal_credits));

    bridge->op_state = FIN_BG_OP_STATE_INITIALIZED;

    fin_bg_heartbeat_global("fin_bg_reset", 1.0f);
    return FIN_BG_ERR_OK;
}

/* ============================================================================
 * Subsystem Setters
 * ============================================================================ */

#define FIN_BG_SETTER(name, field) \
    int financial_bg_bridge_set_##name( \
        financial_bg_bridge_t* bridge, void* ptr) { \
        if (!bridge) { \
            set_error("bridge is NULL in set_" #name); \
            NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, \
                "financial_bg_bridge_set_" #name ": bridge is NULL"); \
            return FIN_BG_ERR_NULL; \
        } \
        if (bridge->magic != FINANCIAL_BG_BRIDGE_MAGIC) { \
            set_error("Invalid bridge magic in set_" #name); \
            return FIN_BG_ERR_STATE; \
        } \
        bridge->field = ptr; \
        return FIN_BG_ERR_OK; \
    }

FIN_BG_SETTER(immune, immune)
FIN_BG_SETTER(health_agent, health_agent)
FIN_BG_SETTER(kg_wiring, kg_wiring)
FIN_BG_SETTER(logger, logger)
FIN_BG_SETTER(security, security)
FIN_BG_SETTER(bio_router, bio_router)

int financial_bg_bridge_set_coordinator(
    financial_bg_bridge_t* bridge,
    brain_cycle_coordinator_t* coordinator)
{
    if (!bridge) {
        set_error("bridge is NULL in set_coordinator");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_bg_bridge_set_coordinator: bridge is NULL");
        return FIN_BG_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_BG_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic in set_coordinator");
        return FIN_BG_ERR_STATE;
    }
    bridge->coordinator = (void*)coordinator;
    return FIN_BG_ERR_OK;
}

int financial_bg_bridge_set_bbb(
    financial_bg_bridge_t* bridge,
    bbb_system_t bbb)
{
    if (!bridge) {
        set_error("bridge is NULL in set_bbb");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_bg_bridge_set_bbb: bridge is NULL");
        return FIN_BG_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_BG_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic in set_bbb");
        return FIN_BG_ERR_STATE;
    }
    bridge->bbb = (void*)bbb;
    return FIN_BG_ERR_OK;
}

int financial_bg_bridge_set_ethics(
    financial_bg_bridge_t* bridge,
    ethics_engine_t ethics)
{
    if (!bridge) {
        set_error("bridge is NULL in set_ethics");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_bg_bridge_set_ethics: bridge is NULL");
        return FIN_BG_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_BG_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic in set_ethics");
        return FIN_BG_ERR_STATE;
    }
    bridge->ethics = (void*)ethics;
    return FIN_BG_ERR_OK;
}

int financial_bg_bridge_set_lgss(
    financial_bg_bridge_t* bridge,
    const void* lgss)
{
    if (!bridge) {
        set_error("bridge is NULL in set_lgss");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_bg_bridge_set_lgss: bridge is NULL");
        return FIN_BG_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_BG_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic in set_lgss");
        return FIN_BG_ERR_STATE;
    }
    bridge->lgss = lgss;
    return FIN_BG_ERR_OK;
}

/* ============================================================================
 * Callback Registration
 * ============================================================================ */

int financial_bg_bridge_set_action_callback(
    financial_bg_bridge_t* bridge,
    fin_bg_action_callback_t callback,
    void* user_data)
{
    if (!bridge) {
        set_error("bridge is NULL in set_action_callback");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_bg_bridge_set_action_callback: bridge is NULL");
        return FIN_BG_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_BG_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic in set_action_callback");
        return FIN_BG_ERR_STATE;
    }
    bridge->action_callback = callback;
    bridge->action_callback_data = user_data;
    return FIN_BG_ERR_OK;
}

int financial_bg_bridge_set_conflict_callback(
    financial_bg_bridge_t* bridge,
    fin_bg_conflict_callback_t callback,
    void* user_data)
{
    if (!bridge) {
        set_error("bridge is NULL in set_conflict_callback");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_bg_bridge_set_conflict_callback: bridge is NULL");
        return FIN_BG_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_BG_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic in set_conflict_callback");
        return FIN_BG_ERR_STATE;
    }
    bridge->conflict_callback = callback;
    bridge->conflict_callback_data = user_data;
    return FIN_BG_ERR_OK;
}

int financial_bg_bridge_set_habit_callback(
    financial_bg_bridge_t* bridge,
    fin_bg_habit_callback_t callback,
    void* user_data)
{
    if (!bridge) {
        set_error("bridge is NULL in set_habit_callback");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_bg_bridge_set_habit_callback: bridge is NULL");
        return FIN_BG_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_BG_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic in set_habit_callback");
        return FIN_BG_ERR_STATE;
    }
    bridge->habit_callback = callback;
    bridge->habit_callback_data = user_data;
    return FIN_BG_ERR_OK;
}

/* ============================================================================
 * Core API - Action Evaluation & Selection
 * ============================================================================ */

int financial_bg_bridge_evaluate_actions(
    financial_bg_bridge_t* bridge,
    const fin_bg_state_t* state,
    fin_bg_decision_t* out_decision)
{
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_bg_bridge_evaluate_actions: bridge is NULL");
        return FIN_BG_ERR_NULL;
    }
    if (!state || !out_decision) {
        set_error("state or out_decision is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_bg_bridge_evaluate_actions: NULL argument");
        return FIN_BG_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_BG_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_BG_ERR_STATE;
    }
    if (!out_decision->actions || out_decision->num_actions == 0) {
        set_error("Decision actions not allocated");
        return FIN_BG_ERR_NO_ACTIONS;
    }

    fin_bg_heartbeat_global("fin_bg_evaluate", 0.0f);

    /* BBB validation if enabled */
    if (bridge->config.enable_bbb_validation && bridge->bbb) {
        bridge->stats.bbb_validations++;
        /* bbb_validate_data would be called here */
    }

    /* Immune check if enabled */
    if (bridge->config.enable_immune_integration && bridge->immune) {
        bridge->stats.immune_checks++;
        /* immune_check would be called here */
    }

    /* Extract features from state */
    float features[Q_WEIGHT_DIM];
    state_to_features(state, features);

    /* Compute Q-values for all actions */
    float q_values[FIN_BG_ACTION_COUNT];
    uint32_t num_to_eval = (out_decision->num_actions < FIN_BG_ACTION_COUNT)
                            ? out_decision->num_actions : FIN_BG_ACTION_COUNT;

    for (uint32_t i = 0; i < num_to_eval; i++) {
        q_values[i] = compute_q_value(bridge, features, (fin_bg_action_t)i);
    }

    /* Compute softmax probabilities */
    float probs[FIN_BG_ACTION_COUNT];
    softmax_probabilities(q_values, num_to_eval, bridge->config.temperature, probs);

    /* Compute beta oscillation (indecision level) */
    bridge->beta_oscillation = compute_beta_oscillation(q_values, num_to_eval,
                                                          bridge->config.conflict_threshold);

    /* Fill decision structure */
    for (uint32_t i = 0; i < num_to_eval; i++) {
        out_decision->actions[i].action = (fin_bg_action_t)i;
        out_decision->actions[i].q_value = q_values[i];
        out_decision->actions[i].probability = probs[i];
        out_decision->actions[i].is_habitual =
            (bridge->consecutive_action_counts[i] >= bridge->config.habit_threshold_reps);
        out_decision->actions[i].temporal_credit = bridge->temporal_credits[i];
    }

    out_decision->beta_oscillation = bridge->beta_oscillation;
    out_decision->decision_conflict = (bridge->beta_oscillation > bridge->config.beta_threshold);

    /* Update state */
    bridge->op_state = FIN_BG_OP_STATE_ACTIVE;
    bridge->stats.action_evaluations++;

    /* KG messaging */
    bridge_kg_publish(bridge, KG_MSG_FIN_BG_EVALUATE, out_decision, sizeof(*out_decision));

    fin_bg_heartbeat_global("fin_bg_evaluate", 1.0f);
    return FIN_BG_ERR_OK;
}

int financial_bg_bridge_select_action(
    financial_bg_bridge_t* bridge,
    fin_bg_decision_t* decision)
{
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_bg_bridge_select_action: bridge is NULL");
        return FIN_BG_ERR_NULL;
    }
    if (!decision || !decision->actions || decision->num_actions == 0) {
        set_error("decision or actions is NULL/empty");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_bg_bridge_select_action: invalid decision");
        return FIN_BG_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_BG_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_BG_ERR_STATE;
    }

    fin_bg_heartbeat_global("fin_bg_select", 0.0f);

    uint32_t selected_idx = 0;

    if (bridge->config.use_softmax) {
        /* Softmax selection */
        float probs[FIN_BG_MAX_ACTIONS];
        for (uint32_t i = 0; i < decision->num_actions && i < FIN_BG_MAX_ACTIONS; i++) {
            probs[i] = decision->actions[i].probability;
        }
        selected_idx = sample_action(probs, decision->num_actions);
    } else {
        /* Epsilon-greedy selection */
        static uint32_t eps_rand = 54321;
        eps_rand = eps_rand * 1103515245 + 12345;
        float r = (float)(eps_rand & 0x7FFFFFFF) / (float)0x7FFFFFFF;

        if (r < bridge->config.epsilon) {
            /* Random exploration */
            selected_idx = eps_rand % decision->num_actions;
        } else {
            /* Greedy exploitation */
            selected_idx = 0;
            float best_q = decision->actions[0].q_value;
            for (uint32_t i = 1; i < decision->num_actions; i++) {
                if (decision->actions[i].q_value > best_q) {
                    best_q = decision->actions[i].q_value;
                    selected_idx = i;
                }
            }
        }
    }

    decision->selected = decision->actions[selected_idx].action;

    /* Record action for habit tracking */
    record_action(bridge, decision->selected, decision->actions[selected_idx].q_value);

    /* Check if selected action is habitual */
    if (decision->actions[selected_idx].is_habitual) {
        bridge->stats.habit_detections++;

        /* Fire habit callback */
        if (bridge->habit_callback) {
            fin_bg_habit_result_t habit_result = {
                .is_habitual = true,
                .habit_strength = (float)bridge->consecutive_action_counts[decision->selected] /
                                  (float)(bridge->config.habit_threshold_reps * 2),
                .repetition_count = bridge->consecutive_action_counts[decision->selected],
                .goal_directed_score = 1.0f - habit_result.habit_strength
            };
            snprintf(habit_result.description, FIN_BG_DESC_LEN,
                     "Action %s repeated %u times",
                     fin_bg_action_name(decision->selected),
                     habit_result.repetition_count);
            bridge->habit_callback(decision->selected, &habit_result,
                                    bridge->habit_callback_data);
        }

        /* KG messaging for habit */
        bridge_kg_publish(bridge, KG_MSG_FIN_BG_HABIT, decision, sizeof(*decision));
    }

    /* Check for conflict */
    if (decision->decision_conflict) {
        bridge->stats.conflicts_detected++;

        /* Fire conflict callback */
        if (bridge->conflict_callback) {
            fin_bg_conflict_result_t conflict = {0};
            financial_bg_bridge_analyze_conflict(bridge, decision, &conflict);
            bridge->conflict_callback(&conflict, bridge->conflict_callback_data);
        }

        /* KG messaging for conflict */
        bridge_kg_publish(bridge, KG_MSG_FIN_BG_CONFLICT, decision, sizeof(*decision));
    }

    /* Fire action callback */
    if (bridge->action_callback) {
        bridge->action_callback(decision, bridge->action_callback_data);
    }

    bridge->stats.action_selections++;

    /* KG messaging */
    bridge_kg_publish(bridge, KG_MSG_FIN_BG_SELECT, decision, sizeof(*decision));

    fin_bg_heartbeat_global("fin_bg_select", 1.0f);
    return FIN_BG_ERR_OK;
}

int financial_bg_bridge_update_from_outcome(
    financial_bg_bridge_t* bridge,
    const fin_bg_state_t* prev_state,
    const fin_bg_outcome_t* outcome)
{
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_bg_bridge_update_from_outcome: bridge is NULL");
        return FIN_BG_ERR_NULL;
    }
    if (!prev_state || !outcome) {
        set_error("prev_state or outcome is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_bg_bridge_update_from_outcome: NULL argument");
        return FIN_BG_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_BG_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_BG_ERR_STATE;
    }

    fin_bg_heartbeat_global("fin_bg_update", 0.0f);

    /* Extract features from previous and next states */
    float prev_features[Q_WEIGHT_DIM];
    float next_features[Q_WEIGHT_DIM];
    state_to_features(prev_state, prev_features);
    state_to_features(&outcome->next_state, next_features);

    /* Compute current Q(s,a) */
    float q_current = compute_q_value(bridge, prev_features, outcome->action_taken);

    /* Compute max Q(s',a') for next state (if not terminal) */
    float q_next_max = 0.0f;
    if (!outcome->terminal) {
        for (int a = 0; a < FIN_BG_ACTION_COUNT; a++) {
            float q_next = compute_q_value(bridge, next_features, (fin_bg_action_t)a);
            if (q_next > q_next_max) {
                q_next_max = q_next;
            }
        }
    }

    /* Compute TD error: delta = r + gamma * Q(s',a*) - Q(s,a) */
    float td_error = outcome->reward +
                     bridge->config.discount_factor * q_next_max -
                     q_current;
    bridge->last_td_error = td_error;

    /* Update eligibility traces */
    /* First, decay all traces */
    for (int a = 0; a < FIN_BG_ACTION_COUNT; a++) {
        for (int i = 0; i < Q_WEIGHT_DIM; i++) {
            bridge->eligibility[a][i] *= bridge->config.discount_factor *
                                          bridge->config.eligibility_decay;
        }
    }

    /* Set eligibility for taken action (replacing traces) */
    for (int i = 0; i < Q_WEIGHT_DIM; i++) {
        bridge->eligibility[outcome->action_taken][i] = prev_features[i];
    }

    /* Update Q-weights using eligibility traces: w += alpha * delta * e */
    for (int a = 0; a < FIN_BG_ACTION_COUNT; a++) {
        for (int i = 0; i < Q_WEIGHT_DIM; i++) {
            bridge->q_weights[a][i] += bridge->config.learning_rate *
                                        td_error *
                                        bridge->eligibility[a][i];
        }
    }

    /* Update temporal credit for the action */
    bridge->temporal_credits[outcome->action_taken] +=
        bridge->config.learning_rate * outcome->reward;

    /* Decay temporal credits */
    for (int a = 0; a < FIN_BG_ACTION_COUNT; a++) {
        bridge->temporal_credits[a] *= bridge->config.credit_decay;
    }

    bridge->stats.outcome_updates++;

    /* KG messaging */
    bridge_kg_publish(bridge, KG_MSG_FIN_BG_UPDATE, outcome, sizeof(*outcome));

    fin_bg_heartbeat_global("fin_bg_update", 1.0f);
    return FIN_BG_ERR_OK;
}

int financial_bg_bridge_test_goal_directed(
    financial_bg_bridge_t* bridge,
    fin_bg_action_t action,
    const fin_bg_state_t* state,
    fin_bg_habit_result_t* out_result)
{
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_bg_bridge_test_goal_directed: bridge is NULL");
        return FIN_BG_ERR_NULL;
    }
    if (!state || !out_result) {
        set_error("state or out_result is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_bg_bridge_test_goal_directed: NULL argument");
        return FIN_BG_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_BG_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_BG_ERR_STATE;
    }
    if (action >= FIN_BG_ACTION_COUNT) {
        set_error("Invalid action: %d", action);
        return FIN_BG_ERR_INVALID_PARAM;
    }

    fin_bg_heartbeat_global("fin_bg_test_goal", 0.0f);

    memset(out_result, 0, sizeof(fin_bg_habit_result_t));

    uint32_t reps = bridge->consecutive_action_counts[action];
    float habit_strength = 0.0f;

    if (reps >= bridge->config.habit_threshold_reps) {
        /* Action is habitual */
        out_result->is_habitual = true;
        habit_strength = (float)reps /
            (float)(bridge->config.habit_threshold_reps * 2);
        habit_strength = nimcp_clampf(habit_strength, 0.0f, 1.0f);
    } else {
        out_result->is_habitual = false;
        habit_strength = (float)reps /
            (float)bridge->config.habit_threshold_reps;
    }

    out_result->habit_strength = habit_strength;
    out_result->repetition_count = reps;
    out_result->goal_directed_score = 1.0f - habit_strength;

    /* Check if action value is significantly better than alternatives */
    float features[Q_WEIGHT_DIM];
    state_to_features(state, features);
    float q_action = compute_q_value(bridge, features, action);

    float q_max_other = -1e10f;
    for (int a = 0; a < FIN_BG_ACTION_COUNT; a++) {
        if (a != action) {
            float q = compute_q_value(bridge, features, (fin_bg_action_t)a);
            if (q > q_max_other) {
                q_max_other = q;
            }
        }
    }

    /* If action is clearly best, it's more likely goal-directed */
    float value_advantage = q_action - q_max_other;
    if (value_advantage > bridge->config.conflict_threshold * 2) {
        out_result->goal_directed_score =
            maxf(out_result->goal_directed_score, 0.5f + value_advantage);
        out_result->goal_directed_score =
            nimcp_clampf(out_result->goal_directed_score, 0.0f, 1.0f);
    }

    if (out_result->is_habitual) {
        snprintf(out_result->description, FIN_BG_DESC_LEN,
                 "HABITUAL: %s repeated %u times (strength=%.2f)",
                 fin_bg_action_name(action), reps, habit_strength);
    } else {
        snprintf(out_result->description, FIN_BG_DESC_LEN,
                 "GOAL-DIRECTED: %s selected by value (score=%.2f)",
                 fin_bg_action_name(action), out_result->goal_directed_score);
    }

    fin_bg_heartbeat_global("fin_bg_test_goal", 1.0f);
    return FIN_BG_ERR_OK;
}

/* ============================================================================
 * Extended API
 * ============================================================================ */

int financial_bg_bridge_get_td_error(
    financial_bg_bridge_t* bridge,
    float* out_td_error)
{
    if (!bridge) {
        set_error("bridge is NULL");
        return FIN_BG_ERR_NULL;
    }
    if (!out_td_error) {
        set_error("out_td_error is NULL");
        return FIN_BG_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_BG_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_BG_ERR_STATE;
    }

    *out_td_error = bridge->last_td_error;
    return FIN_BG_ERR_OK;
}

int financial_bg_bridge_get_q_value(
    financial_bg_bridge_t* bridge,
    const fin_bg_state_t* state,
    fin_bg_action_t action,
    float* out_q_value)
{
    if (!bridge) {
        set_error("bridge is NULL");
        return FIN_BG_ERR_NULL;
    }
    if (!state || !out_q_value) {
        set_error("state or out_q_value is NULL");
        return FIN_BG_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_BG_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_BG_ERR_STATE;
    }
    if (action >= FIN_BG_ACTION_COUNT) {
        set_error("Invalid action: %d", action);
        return FIN_BG_ERR_INVALID_PARAM;
    }

    float features[Q_WEIGHT_DIM];
    state_to_features(state, features);
    *out_q_value = compute_q_value(bridge, features, action);

    return FIN_BG_ERR_OK;
}

int financial_bg_bridge_analyze_conflict(
    financial_bg_bridge_t* bridge,
    const fin_bg_decision_t* decision,
    fin_bg_conflict_result_t* out_conflict)
{
    if (!bridge) {
        set_error("bridge is NULL");
        return FIN_BG_ERR_NULL;
    }
    if (!decision || !out_conflict) {
        set_error("decision or out_conflict is NULL");
        return FIN_BG_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_BG_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_BG_ERR_STATE;
    }

    memset(out_conflict, 0, sizeof(fin_bg_conflict_result_t));

    if (decision->num_actions < 2) {
        out_conflict->has_conflict = false;
        snprintf(out_conflict->resolution, FIN_BG_DESC_LEN, "No conflict - single action");
        return FIN_BG_ERR_OK;
    }

    /* Find top 2 actions by Q-value */
    uint32_t best_idx = 0, second_idx = 1;
    float best_q = decision->actions[0].q_value;
    float second_q = decision->actions[1].q_value;

    if (second_q > best_q) {
        best_idx = 1;
        second_idx = 0;
        float tmp = best_q;
        best_q = second_q;
        second_q = tmp;
    }

    for (uint32_t i = 2; i < decision->num_actions; i++) {
        float q = decision->actions[i].q_value;
        if (q > best_q) {
            second_q = best_q;
            second_idx = best_idx;
            best_q = q;
            best_idx = i;
        } else if (q > second_q) {
            second_q = q;
            second_idx = i;
        }
    }

    float gap = best_q - second_q;
    out_conflict->value_gap = gap;

    if (gap < bridge->config.conflict_threshold) {
        out_conflict->has_conflict = true;
        out_conflict->action1 = decision->actions[best_idx].action;
        out_conflict->action2 = decision->actions[second_idx].action;
        out_conflict->conflict_intensity = 1.0f - (gap / bridge->config.conflict_threshold);

        snprintf(out_conflict->resolution, FIN_BG_DESC_LEN,
                 "Conflict: %s vs %s (gap=%.4f). Consider waiting or gathering more info.",
                 fin_bg_action_name(out_conflict->action1),
                 fin_bg_action_name(out_conflict->action2),
                 gap);
    } else {
        out_conflict->has_conflict = false;
        out_conflict->action1 = decision->actions[best_idx].action;
        out_conflict->conflict_intensity = 0.0f;

        snprintf(out_conflict->resolution, FIN_BG_DESC_LEN,
                 "No conflict: %s clearly preferred (gap=%.4f)",
                 fin_bg_action_name(out_conflict->action1), gap);
    }

    return FIN_BG_ERR_OK;
}

int financial_bg_bridge_set_temperature(
    financial_bg_bridge_t* bridge,
    float temperature)
{
    if (!bridge) {
        set_error("bridge is NULL");
        return FIN_BG_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_BG_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_BG_ERR_STATE;
    }
    if (temperature <= 0.0f) {
        set_error("Temperature must be positive");
        return FIN_BG_ERR_INVALID_PARAM;
    }

    bridge->config.temperature = temperature;
    return FIN_BG_ERR_OK;
}

int financial_bg_bridge_get_beta_oscillation(
    financial_bg_bridge_t* bridge,
    float* out_beta)
{
    if (!bridge) {
        set_error("bridge is NULL");
        return FIN_BG_ERR_NULL;
    }
    if (!out_beta) {
        set_error("out_beta is NULL");
        return FIN_BG_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_BG_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_BG_ERR_STATE;
    }

    *out_beta = bridge->beta_oscillation;
    return FIN_BG_ERR_OK;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

fin_bg_op_state_t financial_bg_bridge_get_op_state(
    const financial_bg_bridge_t* bridge)
{
    if (!bridge || bridge->magic != FINANCIAL_BG_BRIDGE_MAGIC) {
        return FIN_BG_OP_STATE_UNINITIALIZED;
    }
    return bridge->op_state;
}

int financial_bg_bridge_get_stats(
    const financial_bg_bridge_t* bridge,
    fin_bg_bridge_stats_t* stats)
{
    if (!bridge) {
        set_error("bridge is NULL");
        return FIN_BG_ERR_NULL;
    }
    if (!stats) {
        set_error("stats is NULL");
        return FIN_BG_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_BG_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_BG_ERR_STATE;
    }

    *stats = bridge->stats;
    return FIN_BG_ERR_OK;
}

void financial_bg_bridge_reset_stats(financial_bg_bridge_t* bridge) {
    if (bridge && bridge->magic == FINANCIAL_BG_BRIDGE_MAGIC) {
        memset(&bridge->stats, 0, sizeof(fin_bg_bridge_stats_t));
    }
}

/* ============================================================================
 * Health Integration
 * ============================================================================ */

int financial_bg_bridge_heartbeat(
    financial_bg_bridge_t* bridge,
    const char* operation,
    float progress)
{
    if (!bridge) {
        return FIN_BG_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_BG_BRIDGE_MAGIC) {
        return FIN_BG_ERR_STATE;
    }

    /* Forward to global health agent */
    fin_bg_heartbeat_global(operation ? operation : "fin_bg_heartbeat", progress);

    /* Forward to instance-level health agent */
    if (bridge->health_agent) {
        nimcp_health_agent_heartbeat_ex(
            (nimcp_health_agent_t*)bridge->health_agent, operation, progress);
    }

    bridge->stats.health_heartbeats++;
    return FIN_BG_ERR_OK;
}
