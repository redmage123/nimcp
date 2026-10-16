/**
 * @file nimcp_financial_stdp_bridge.c
 * @brief Financial STDP Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-29
 *
 * WHAT: Bridge for learning timing correlations between market signals and
 *       trade outcomes using STDP (Spike-Timing Dependent Plasticity).
 *
 * WHY:  Financial markets exhibit temporal patterns where signals precede
 *       profitable/unprofitable outcomes. STDP naturally learns these
 *       signal-to-outcome timing correlations.
 *
 * HOW:  STDP rules update synaptic weights based on signal-outcome temporal
 *       differences. Reward modulation scales plasticity by trade P&L.
 *
 * @author NIMCP Development Team
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>

#include "cognitive/parietal/nimcp_financial_stdp_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(fin_stdp)

/* Stub heartbeat for migration compatibility */
static inline void fin_stdp_heartbeat_global(const char* op, float progress) {
    (void)op; (void)progress;
}

BRIDGE_DEFINE_MESH_REGISTRATION(fin_stdp, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * Immune/BBB Integration (Phase 9: Security Integration)
 * ============================================================================ */

struct brain_immune_system;
typedef struct brain_immune_system brain_immune_system_t;
extern int brain_immune_validate_operation(brain_immune_system_t* immune,
                                            const char* operation,
                                            uint32_t severity);

struct bbb_system_struct;
extern int bbb_validate_data(bbb_system_t bbb, const void* data,
                              size_t size, const char* context);

/* ============================================================================
 * Thread-Local Error Handling
 * ============================================================================ */

static _Thread_local char fin_stdp_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(fin_stdp_last_error, sizeof(fin_stdp_last_error), fmt, args);
    va_end(args);
}

const char* financial_stdp_bridge_get_last_error(void) {
    return fin_stdp_last_error;
}

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Signal buffer entry
 */
typedef struct {
    fin_signal_extended_t signal;
    bool in_use;
} signal_buffer_entry_t;

/**
 * @brief Outcome buffer entry
 */
typedef struct {
    fin_outcome_extended_t outcome;
    bool in_use;
} outcome_buffer_entry_t;

/**
 * @brief Correlation entry
 */
typedef struct {
    fin_stdp_correlation_t correlation;
    bool in_use;
} correlation_entry_t;

struct financial_stdp_bridge {
    uint32_t magic;
    fin_stdp_config_t config;
    fin_stdp_op_state_t op_state;
    fin_stdp_bridge_stats_t stats;

    /* Signal buffer (circular) */
    signal_buffer_entry_t* signal_buffer;
    uint32_t signal_buffer_size;
    uint32_t signal_write_idx;
    uint32_t signal_count;

    /* Outcome buffer (circular) */
    outcome_buffer_entry_t* outcome_buffer;
    uint32_t outcome_buffer_size;
    uint32_t outcome_write_idx;
    uint32_t outcome_count;

    /* Learned correlations */
    correlation_entry_t* correlations;
    uint32_t max_correlations;
    uint32_t correlation_count;

    /* BCM state */
    float bcm_threshold;

    /* Homeostatic state */
    float mean_weight;
    float weight_scale;

    /* Current time tracking */
    uint64_t current_time_ms;

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
    fin_stdp_learn_callback_t learn_callback;
    void* learn_callback_data;
    fin_stdp_signal_callback_t signal_callback;
    void* signal_callback_data;
};

static inline float expf_safe(float x) {
    /* Clamp to avoid overflow/underflow */
    if (x > 88.0f) return 1e38f;
    if (x < -88.0f) return 0.0f;
    return expf(x);
}

/* ============================================================================
 * KG Wiring Message Types
 * ============================================================================ */

#define KG_MSG_FIN_STDP_SIGNAL         "FIN_STDP_SIGNAL"
#define KG_MSG_FIN_STDP_LEARN          "FIN_STDP_LEARN"
#define KG_MSG_FIN_STDP_WEIGHT_UPDATE  "FIN_STDP_WEIGHT_UPDATE"

static int bridge_kg_publish(financial_stdp_bridge_t* bridge,
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

const char* fin_stdp_op_state_name(fin_stdp_op_state_t state) {
    switch (state) {
        case FIN_STDP_OP_STATE_UNINITIALIZED: return "UNINITIALIZED";
        case FIN_STDP_OP_STATE_INITIALIZED:   return "INITIALIZED";
        case FIN_STDP_OP_STATE_LEARNING:      return "LEARNING";
        case FIN_STDP_OP_STATE_ACTIVE:        return "ACTIVE";
        case FIN_STDP_OP_STATE_DEGRADED:      return "DEGRADED";
        case FIN_STDP_OP_STATE_ERROR:         return "ERROR";
        default: return "UNKNOWN";
    }
}

const char* fin_stdp_signal_category_name(fin_stdp_signal_category_t category) {
    switch (category) {
        case FIN_STDP_SIGNAL_TECHNICAL:    return "TECHNICAL";
        case FIN_STDP_SIGNAL_SENTIMENT:    return "SENTIMENT";
        case FIN_STDP_SIGNAL_PRICE_ACTION: return "PRICE_ACTION";
        case FIN_STDP_SIGNAL_FUNDAMENTAL:  return "FUNDAMENTAL";
        case FIN_STDP_SIGNAL_CROSS_ASSET:  return "CROSS_ASSET";
        case FIN_STDP_SIGNAL_VOLUME:       return "VOLUME";
        case FIN_STDP_SIGNAL_VOLATILITY:   return "VOLATILITY";
        case FIN_STDP_SIGNAL_CUSTOM:       return "CUSTOM";
        default: return "UNKNOWN";
    }
}

const char* fin_stdp_mode_name(fin_stdp_mode_t mode) {
    switch (mode) {
        case FIN_STDP_MODE_STANDARD:         return "STANDARD";
        case FIN_STDP_MODE_REWARD_MODULATED: return "REWARD_MODULATED";
        case FIN_STDP_MODE_TRIPLET:          return "TRIPLET";
        case FIN_STDP_MODE_BCM:              return "BCM";
        default: return "UNKNOWN";
    }
}

const char* financial_stdp_bridge_version(void) {
    return FINANCIAL_STDP_BRIDGE_VERSION;
}

/* ============================================================================
 * Configuration
 * ============================================================================ */

int financial_stdp_bridge_default_config(fin_stdp_config_t* config) {
    if (!config) {
        set_error("config is NULL");
        return FIN_STDP_ERR_NULL;
    }

    memset(config, 0, sizeof(fin_stdp_config_t));

    /* STDP parameters */
    config->tau_plus_ms = 20.0f;        /* 20ms LTP window */
    config->tau_minus_ms = 20.0f;       /* 20ms LTD window */
    config->a_plus = 0.01f;             /* LTP magnitude */
    config->a_minus = 0.012f;           /* LTD magnitude (slightly larger) */
    config->learning_rate = 0.001f;     /* Base learning rate */
    config->learning_window_ms = FIN_STDP_DEFAULT_WINDOW_MS;

    /* Weight bounds */
    config->weight_min = 0.0f;
    config->weight_max = 1.0f;
    config->initial_weight = 0.5f;

    /* Reward modulation */
    config->enable_reward_modulation = true;
    config->reward_scale = 1.5f;
    config->punishment_scale = 1.0f;

    /* BCM parameters */
    config->enable_bcm = false;
    config->bcm_tau_ms = 1000.0f;
    config->bcm_target_rate = 0.5f;

    /* Homeostatic parameters */
    config->enable_homeostasis = true;
    config->homeostatic_tau_ms = 10000.0f;
    config->target_mean_weight = 0.5f;

    /* Integration settings */
    config->enable_immune_integration = true;
    config->enable_bbb_validation = true;
    config->enable_kg_messaging = true;
    config->enable_health_monitoring = true;

    /* Plasticity mode */
    config->mode = FIN_STDP_MODE_REWARD_MODULATED;

    /* Logging */
    config->verbose_logging = false;

    return FIN_STDP_ERR_OK;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

financial_stdp_bridge_t* financial_stdp_bridge_create(
    const fin_stdp_config_t* config)
{
    fin_stdp_heartbeat_global("fin_stdp_create", 0.0f);

    financial_stdp_bridge_t* bridge = (financial_stdp_bridge_t*)
        nimcp_malloc(sizeof(financial_stdp_bridge_t));
    if (!bridge) {
        set_error("Failed to allocate financial_stdp_bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate financial_stdp_bridge");
        return NULL;
    }
    memset(bridge, 0, sizeof(financial_stdp_bridge_t));

    bridge->magic = FINANCIAL_STDP_BRIDGE_MAGIC;

    /* Copy configuration or use defaults */
    if (config) {
        bridge->config = *config;
    } else {
        financial_stdp_bridge_default_config(&bridge->config);
    }

    /* Allocate signal buffer */
    bridge->signal_buffer_size = FIN_STDP_MAX_SIGNALS;
    bridge->signal_buffer = (signal_buffer_entry_t*)nimcp_malloc(
        bridge->signal_buffer_size * sizeof(signal_buffer_entry_t));
    if (!bridge->signal_buffer) {
        set_error("Failed to allocate signal buffer");
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate signal buffer");
        return NULL;
    }
    memset(bridge->signal_buffer, 0,
           bridge->signal_buffer_size * sizeof(signal_buffer_entry_t));

    /* Allocate outcome buffer */
    bridge->outcome_buffer_size = FIN_STDP_MAX_OUTCOMES;
    bridge->outcome_buffer = (outcome_buffer_entry_t*)nimcp_malloc(
        bridge->outcome_buffer_size * sizeof(outcome_buffer_entry_t));
    if (!bridge->outcome_buffer) {
        set_error("Failed to allocate outcome buffer");
        nimcp_free(bridge->signal_buffer);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate outcome buffer");
        return NULL;
    }
    memset(bridge->outcome_buffer, 0,
           bridge->outcome_buffer_size * sizeof(outcome_buffer_entry_t));

    /* Allocate correlation storage */
    bridge->max_correlations = FIN_STDP_MAX_SIGNAL_TYPES;
    bridge->correlations = (correlation_entry_t*)nimcp_malloc(
        bridge->max_correlations * sizeof(correlation_entry_t));
    if (!bridge->correlations) {
        set_error("Failed to allocate correlations");
        nimcp_free(bridge->outcome_buffer);
        nimcp_free(bridge->signal_buffer);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate correlations");
        return NULL;
    }
    memset(bridge->correlations, 0,
           bridge->max_correlations * sizeof(correlation_entry_t));

    /* Initialize state */
    bridge->op_state = FIN_STDP_OP_STATE_INITIALIZED;
    bridge->bcm_threshold = bridge->config.bcm_target_rate;
    bridge->mean_weight = bridge->config.initial_weight;
    bridge->weight_scale = 1.0f;
    bridge->current_time_ms = 0;

    fin_stdp_heartbeat_global("fin_stdp_create", 1.0f);
    return bridge;
}

void financial_stdp_bridge_destroy(financial_stdp_bridge_t* bridge) {
    fin_stdp_heartbeat_global("fin_stdp_destroy", 0.0f);

    if (bridge) {
        if (bridge->signal_buffer) {
            nimcp_free(bridge->signal_buffer);
        }
        if (bridge->outcome_buffer) {
            nimcp_free(bridge->outcome_buffer);
        }
        if (bridge->correlations) {
            nimcp_free(bridge->correlations);
        }
        bridge->magic = 0;
        bridge->op_state = FIN_STDP_OP_STATE_UNINITIALIZED;
        nimcp_free(bridge);
        bridge = NULL;
    }
}

int financial_stdp_bridge_reset(financial_stdp_bridge_t* bridge) {
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_stdp_bridge_reset: bridge is NULL");
        return FIN_STDP_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_STDP_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_STDP_ERR_STATE;
    }

    fin_stdp_heartbeat_global("fin_stdp_reset", 0.0f);

    /* Clear signal buffer */
    memset(bridge->signal_buffer, 0,
           bridge->signal_buffer_size * sizeof(signal_buffer_entry_t));
    bridge->signal_write_idx = 0;
    bridge->signal_count = 0;

    /* Clear outcome buffer */
    memset(bridge->outcome_buffer, 0,
           bridge->outcome_buffer_size * sizeof(outcome_buffer_entry_t));
    bridge->outcome_write_idx = 0;
    bridge->outcome_count = 0;

    /* Keep learned weights, just reset state */
    bridge->op_state = FIN_STDP_OP_STATE_INITIALIZED;
    bridge->current_time_ms = 0;

    fin_stdp_heartbeat_global("fin_stdp_reset", 1.0f);
    return FIN_STDP_ERR_OK;
}

int financial_stdp_bridge_reset_weights(financial_stdp_bridge_t* bridge) {
    if (!bridge) {
        set_error("bridge is NULL");
        return FIN_STDP_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_STDP_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_STDP_ERR_STATE;
    }

    fin_stdp_heartbeat_global("fin_stdp_reset_weights", 0.0f);

    /* Reset all correlations to initial state */
    for (uint32_t i = 0; i < bridge->max_correlations; i++) {
        if (bridge->correlations[i].in_use) {
            fin_stdp_correlation_t* c = &bridge->correlations[i].correlation;
            c->weight = c->initial_weight;
            c->eligibility_trace = 0.0f;
            c->update_count = 0;
            c->last_update_ms = 0;
            c->avg_dt_ms = 0.0f;
            c->predictive_accuracy = 0.5f;
        }
    }

    /* Reset BCM and homeostatic state */
    bridge->bcm_threshold = bridge->config.bcm_target_rate;
    bridge->mean_weight = bridge->config.initial_weight;
    bridge->weight_scale = 1.0f;

    fin_stdp_heartbeat_global("fin_stdp_reset_weights", 1.0f);
    return FIN_STDP_ERR_OK;
}

/* ============================================================================
 * Subsystem Setters
 * ============================================================================ */

#define FIN_STDP_SETTER(name, field) \
    int financial_stdp_bridge_set_##name( \
        financial_stdp_bridge_t* bridge, void* ptr) { \
        if (!bridge) { \
            set_error("bridge is NULL in set_" #name); \
            NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, \
                "financial_stdp_bridge_set_" #name ": bridge is NULL"); \
            return FIN_STDP_ERR_NULL; \
        } \
        if (bridge->magic != FINANCIAL_STDP_BRIDGE_MAGIC) { \
            set_error("Invalid bridge magic in set_" #name); \
            return FIN_STDP_ERR_STATE; \
        } \
        bridge->field = ptr; \
        return FIN_STDP_ERR_OK; \
    }

FIN_STDP_SETTER(immune, immune)
FIN_STDP_SETTER(health_agent, health_agent)
FIN_STDP_SETTER(kg_wiring, kg_wiring)
FIN_STDP_SETTER(logger, logger)
FIN_STDP_SETTER(security, security)
FIN_STDP_SETTER(bio_router, bio_router)

int financial_stdp_bridge_set_coordinator(
    financial_stdp_bridge_t* bridge,
    brain_cycle_coordinator_t* coordinator)
{
    if (!bridge) {
        set_error("bridge is NULL in set_coordinator");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_stdp_bridge_set_coordinator: bridge is NULL");
        return FIN_STDP_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_STDP_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic in set_coordinator");
        return FIN_STDP_ERR_STATE;
    }
    bridge->coordinator = (void*)coordinator;
    return FIN_STDP_ERR_OK;
}

int financial_stdp_bridge_set_bbb(
    financial_stdp_bridge_t* bridge,
    bbb_system_t bbb)
{
    if (!bridge) {
        set_error("bridge is NULL in set_bbb");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_stdp_bridge_set_bbb: bridge is NULL");
        return FIN_STDP_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_STDP_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic in set_bbb");
        return FIN_STDP_ERR_STATE;
    }
    bridge->bbb = (void*)bbb;
    return FIN_STDP_ERR_OK;
}

int financial_stdp_bridge_set_ethics(
    financial_stdp_bridge_t* bridge,
    ethics_engine_t ethics)
{
    if (!bridge) {
        set_error("bridge is NULL in set_ethics");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_stdp_bridge_set_ethics: bridge is NULL");
        return FIN_STDP_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_STDP_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic in set_ethics");
        return FIN_STDP_ERR_STATE;
    }
    bridge->ethics = (void*)ethics;
    return FIN_STDP_ERR_OK;
}

int financial_stdp_bridge_set_lgss(
    financial_stdp_bridge_t* bridge,
    const void* lgss)
{
    if (!bridge) {
        set_error("bridge is NULL in set_lgss");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_stdp_bridge_set_lgss: bridge is NULL");
        return FIN_STDP_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_STDP_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic in set_lgss");
        return FIN_STDP_ERR_STATE;
    }
    bridge->lgss = lgss;
    return FIN_STDP_ERR_OK;
}

/* ============================================================================
 * Callback Registration
 * ============================================================================ */

int financial_stdp_bridge_set_learn_callback(
    financial_stdp_bridge_t* bridge,
    fin_stdp_learn_callback_t callback,
    void* user_data)
{
    if (!bridge) {
        set_error("bridge is NULL in set_learn_callback");
        return FIN_STDP_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_STDP_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_STDP_ERR_STATE;
    }
    bridge->learn_callback = callback;
    bridge->learn_callback_data = user_data;
    return FIN_STDP_ERR_OK;
}

int financial_stdp_bridge_set_signal_callback(
    financial_stdp_bridge_t* bridge,
    fin_stdp_signal_callback_t callback,
    void* user_data)
{
    if (!bridge) {
        set_error("bridge is NULL in set_signal_callback");
        return FIN_STDP_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_STDP_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_STDP_ERR_STATE;
    }
    bridge->signal_callback = callback;
    bridge->signal_callback_data = user_data;
    return FIN_STDP_ERR_OK;
}

/* ============================================================================
 * Correlation Management - Internal
 * ============================================================================ */

/**
 * @brief Find or create correlation entry for signal type
 */
static correlation_entry_t* get_or_create_correlation(
    financial_stdp_bridge_t* bridge,
    int signal_type)
{
    /* Search for existing */
    for (uint32_t i = 0; i < bridge->max_correlations; i++) {
        if (bridge->correlations[i].in_use &&
            bridge->correlations[i].correlation.signal_type == signal_type) {
            return &bridge->correlations[i];
        }
    }

    /* Create new */
    for (uint32_t i = 0; i < bridge->max_correlations; i++) {
        if (!bridge->correlations[i].in_use) {
            correlation_entry_t* entry = &bridge->correlations[i];
            memset(entry, 0, sizeof(correlation_entry_t));
            entry->in_use = true;
            entry->correlation.signal_type = signal_type;
            entry->correlation.weight = bridge->config.initial_weight;
            entry->correlation.initial_weight = bridge->config.initial_weight;
            entry->correlation.eligibility_trace = 0.0f;
            entry->correlation.predictive_accuracy = 0.5f;
            bridge->correlation_count++;
            return entry;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "get_or_create_correlation: operation failed");
    return NULL;  /* Full */
}

/* ============================================================================
 * Signal Recording API
 * ============================================================================ */

int financial_stdp_bridge_record_signal(
    financial_stdp_bridge_t* bridge,
    const fin_signal_t* signal)
{
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_stdp_bridge_record_signal: bridge is NULL");
        return FIN_STDP_ERR_NULL;
    }
    if (!signal) {
        set_error("signal is NULL");
        return FIN_STDP_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_STDP_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_STDP_ERR_STATE;
    }

    /* Convert to extended format */
    fin_signal_extended_t ext;
    memset(&ext, 0, sizeof(ext));
    ext.signal = *signal;
    ext.category = FIN_STDP_SIGNAL_CUSTOM;
    ext.confidence = 1.0f;
    snprintf(ext.name, sizeof(ext.name), "signal_%d", signal->signal_type);

    return financial_stdp_bridge_record_signal_ex(bridge, &ext);
}

int financial_stdp_bridge_record_signal_ex(
    financial_stdp_bridge_t* bridge,
    const fin_signal_extended_t* signal)
{
    if (!bridge) {
        set_error("bridge is NULL");
        return FIN_STDP_ERR_NULL;
    }
    if (!signal) {
        set_error("signal is NULL");
        return FIN_STDP_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_STDP_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_STDP_ERR_STATE;
    }

    fin_stdp_heartbeat_global("fin_stdp_record_signal", 0.0f);

    /* BBB validation if enabled */
    if (bridge->config.enable_bbb_validation && bridge->bbb) {
        bridge->stats.bbb_validations++;
        int rc = bbb_validate_data((bbb_system_t)bridge->bbb, signal,
                                    sizeof(*signal), "stdp_record_signal");
        if (rc != 0) {
            set_error("BBB validation failed for record_signal");
            return FIN_STDP_ERR_BBB;
        }
    }

    /* Add to circular buffer */
    signal_buffer_entry_t* entry = &bridge->signal_buffer[bridge->signal_write_idx];
    entry->signal = *signal;
    entry->in_use = true;

    bridge->signal_write_idx = (bridge->signal_write_idx + 1) % bridge->signal_buffer_size;
    if (bridge->signal_count < bridge->signal_buffer_size) {
        bridge->signal_count++;
    }

    /* Update current time */
    if (signal->signal.timestamp_ms > bridge->current_time_ms) {
        bridge->current_time_ms = signal->signal.timestamp_ms;
    }

    /* Ensure correlation entry exists for this signal type */
    get_or_create_correlation(bridge, signal->signal.signal_type);

    /* Fire callback */
    if (bridge->signal_callback) {
        bridge->signal_callback(signal, bridge->signal_callback_data);
    }

    /* KG messaging */
    bridge_kg_publish(bridge, KG_MSG_FIN_STDP_SIGNAL, signal, sizeof(*signal));

    bridge->op_state = FIN_STDP_OP_STATE_ACTIVE;

    fin_stdp_heartbeat_global("fin_stdp_record_signal", 1.0f);
    return FIN_STDP_ERR_OK;
}

int financial_stdp_bridge_clear_signals(financial_stdp_bridge_t* bridge) {
    if (!bridge) {
        set_error("bridge is NULL");
        return FIN_STDP_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_STDP_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_STDP_ERR_STATE;
    }

    memset(bridge->signal_buffer, 0,
           bridge->signal_buffer_size * sizeof(signal_buffer_entry_t));
    bridge->signal_write_idx = 0;
    bridge->signal_count = 0;

    return FIN_STDP_ERR_OK;
}

/* ============================================================================
 * STDP Learning Core
 * ============================================================================ */

/**
 * @brief Compute STDP weight change
 *
 * Standard STDP rule:
 *   dt = t_post - t_pre (post = outcome, pre = signal)
 *   if dt > 0: dw = A+ * exp(-dt/tau+)  [signal predicted outcome -> LTP]
 *   if dt < 0: dw = -A- * exp(dt/tau-)  [signal after outcome -> LTD]
 */
static float compute_stdp_dw(
    const fin_stdp_config_t* config,
    float dt_ms,
    float signal_strength,
    float reward)
{
    float dw = 0.0f;

    if (dt_ms > 0.0f) {
        /* Signal before outcome -> potentiation (LTP) */
        dw = config->a_plus * expf_safe(-dt_ms / config->tau_plus_ms);
    } else if (dt_ms < 0.0f) {
        /* Signal after outcome -> depression (LTD) */
        dw = -config->a_minus * expf_safe(dt_ms / config->tau_minus_ms);
    }

    /* Scale by signal strength */
    dw *= signal_strength;

    /* Apply learning rate */
    dw *= config->learning_rate;

    /* Reward modulation */
    if (config->enable_reward_modulation) {
        if (reward > 0.0f) {
            dw *= (1.0f + reward * config->reward_scale);
        } else if (reward < 0.0f) {
            /* Reverse sign for negative outcome */
            dw *= (-1.0f + reward * config->punishment_scale);
        }
    }

    return dw;
}

int financial_stdp_bridge_learn_correlation(
    financial_stdp_bridge_t* bridge,
    const fin_signal_t* signal,
    const fin_outcome_t* outcome)
{
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_stdp_bridge_learn_correlation: bridge is NULL");
        return FIN_STDP_ERR_NULL;
    }
    if (!signal || !outcome) {
        set_error("signal or outcome is NULL");
        return FIN_STDP_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_STDP_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_STDP_ERR_STATE;
    }

    fin_stdp_heartbeat_global("fin_stdp_learn_corr", 0.0f);

    /* Immune check if enabled */
    if (bridge->config.enable_immune_integration && bridge->immune) {
        bridge->stats.immune_checks++;
        int rc = brain_immune_validate_operation(
            (brain_immune_system_t*)bridge->immune,
            "stdp_learn_correlation", 5);
        if (rc != 0) {
            set_error("Immune validation failed for learn_correlation");
            return FIN_STDP_ERR_IMMUNE;
        }
    }

    /* BBB validation if enabled */
    if (bridge->config.enable_bbb_validation && bridge->bbb) {
        bridge->stats.bbb_validations++;
        int rc = bbb_validate_data((bbb_system_t)bridge->bbb, outcome,
                                    sizeof(*outcome), "stdp_learn");
        if (rc != 0) {
            set_error("BBB validation failed for learn_correlation");
            return FIN_STDP_ERR_BBB;
        }
    }

    /* Get or create correlation entry */
    correlation_entry_t* entry = get_or_create_correlation(bridge, signal->signal_type);
    if (!entry) {
        set_error("Correlation storage full");
        return FIN_STDP_ERR_FULL;
    }

    fin_stdp_correlation_t* corr = &entry->correlation;

    /* Compute temporal difference */
    float dt_ms = (float)((int64_t)outcome->timestamp_ms - (int64_t)signal->timestamp_ms);

    /* Check if within learning window */
    if (fabsf(dt_ms) > (float)bridge->config.learning_window_ms) {
        /* Outside window - no learning */
        return FIN_STDP_ERR_OK;
    }

    /* Compute weight change */
    float weight_before = corr->weight;
    float dw = compute_stdp_dw(&bridge->config, dt_ms, signal->strength, outcome->outcome);

    /* Apply weight change with bounds */
    corr->weight = nimcp_clampf(corr->weight + dw, bridge->config.weight_min, bridge->config.weight_max);

    /* Update correlation statistics */
    corr->update_count++;
    corr->last_update_ms = outcome->timestamp_ms;

    /* Update running average of dt */
    float alpha = 0.1f;
    if (isfinite(dt_ms)) {
        corr->avg_dt_ms = (1.0f - alpha) * corr->avg_dt_ms + alpha * dt_ms;
    }

    /* Update predictive accuracy estimate */
    if (dw > 0.0f) {
        corr->predictive_accuracy = nimcp_clampf(
            corr->predictive_accuracy + 0.01f, 0.0f, 1.0f);
    } else if (dw < 0.0f) {
        corr->predictive_accuracy = nimcp_clampf(
            corr->predictive_accuracy - 0.01f, 0.0f, 1.0f);
    }

    bridge->stats.correlations_learned++;
    bridge->op_state = FIN_STDP_OP_STATE_LEARNING;

    /* Prepare learn result for callback */
    fin_stdp_learn_result_t result;
    result.signal_type = signal->signal_type;
    result.weight_before = weight_before;
    result.weight_after = corr->weight;
    result.delta_weight = corr->weight - weight_before;
    result.dt_ms = dt_ms;
    result.is_ltp = (dw > 0.0f);

    /* Fire callback */
    if (bridge->learn_callback) {
        bridge->learn_callback(&result, bridge->learn_callback_data);
    }

    /* KG messaging */
    bridge_kg_publish(bridge, KG_MSG_FIN_STDP_LEARN, &result, sizeof(result));

    fin_stdp_heartbeat_global("fin_stdp_learn_corr", 1.0f);
    return FIN_STDP_ERR_OK;
}

int financial_stdp_bridge_update_from_trade(
    financial_stdp_bridge_t* bridge,
    const fin_outcome_t* outcome)
{
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_stdp_bridge_update_from_trade: bridge is NULL");
        return FIN_STDP_ERR_NULL;
    }
    if (!outcome) {
        set_error("outcome is NULL");
        return FIN_STDP_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_STDP_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return -FIN_STDP_ERR_STATE;
    }

    fin_stdp_heartbeat_global("fin_stdp_update_trade", 0.0f);

    /* Immune check if enabled */
    if (bridge->config.enable_immune_integration && bridge->immune) {
        bridge->stats.immune_checks++;
        int rc = brain_immune_validate_operation(
            (brain_immune_system_t*)bridge->immune,
            "stdp_update_from_trade", 5);
        if (rc != 0) {
            set_error("Immune validation failed for update_from_trade");
            return -FIN_STDP_ERR_IMMUNE;
        }
    }

    /* BBB validation if enabled */
    if (bridge->config.enable_bbb_validation && bridge->bbb) {
        bridge->stats.bbb_validations++;
        int rc = bbb_validate_data((bbb_system_t)bridge->bbb, outcome,
                                    sizeof(*outcome), "stdp_trade_update");
        if (rc != 0) {
            set_error("BBB validation failed for update_from_trade");
            return -FIN_STDP_ERR_BBB;
        }
    }

    int signals_updated = 0;
    uint64_t window_start = 0;
    if (outcome->timestamp_ms > bridge->config.learning_window_ms) {
        window_start = outcome->timestamp_ms - bridge->config.learning_window_ms;
    }

    /* Iterate through signal buffer and update correlations */
    for (uint32_t i = 0; i < bridge->signal_buffer_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->signal_buffer_size > 256) {
            fin_stdp_heartbeat_global("fin_stdp_loop",
                (float)(i + 1) / (float)bridge->signal_buffer_size);
        }

        signal_buffer_entry_t* sig_entry = &bridge->signal_buffer[i];
        if (!sig_entry->in_use) {
            continue;
        }

        fin_signal_t* sig = &sig_entry->signal.signal;

        /* Check if signal is within learning window */
        if (sig->timestamp_ms < window_start) {
            continue;
        }
        if (sig->timestamp_ms > outcome->timestamp_ms) {
            continue;  /* Signal after outcome */
        }

        /* Learn correlation */
        int rc = financial_stdp_bridge_learn_correlation(bridge, sig, outcome);
        if (rc == FIN_STDP_ERR_OK) {
            signals_updated++;
        }
    }

    bridge->stats.updates_from_trades++;
    bridge->op_state = FIN_STDP_OP_STATE_LEARNING;

    fin_stdp_heartbeat_global("fin_stdp_update_trade", 1.0f);
    return signals_updated;
}

int financial_stdp_bridge_batch_learn(
    financial_stdp_bridge_t* bridge,
    const fin_signal_t* signals,
    const fin_outcome_t* outcomes,
    uint32_t count)
{
    if (!bridge) {
        set_error("bridge is NULL");
        return FIN_STDP_ERR_NULL;
    }
    if (!signals || !outcomes) {
        set_error("signals or outcomes is NULL");
        return FIN_STDP_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_STDP_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_STDP_ERR_STATE;
    }

    fin_stdp_heartbeat_global("fin_stdp_batch", 0.0f);

    for (uint32_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            fin_stdp_heartbeat_global("fin_stdp_batch_loop",
                (float)(i + 1) / (float)count);
        }

        int rc = financial_stdp_bridge_learn_correlation(bridge,
                                                          &signals[i],
                                                          &outcomes[i]);
        if (rc != FIN_STDP_ERR_OK) {
            /* Continue with remaining pairs */
        }
    }

    fin_stdp_heartbeat_global("fin_stdp_batch", 1.0f);
    return FIN_STDP_ERR_OK;
}

/* ============================================================================
 * Homeostatic and BCM Updates
 * ============================================================================ */

int financial_stdp_bridge_apply_homeostasis(financial_stdp_bridge_t* bridge) {
    if (!bridge) {
        set_error("bridge is NULL");
        return FIN_STDP_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_STDP_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_STDP_ERR_STATE;
    }

    if (!bridge->config.enable_homeostasis) {
        return FIN_STDP_ERR_OK;
    }

    fin_stdp_heartbeat_global("fin_stdp_homeostasis", 0.0f);

    /* Compute current mean weight */
    float sum = 0.0f;
    uint32_t active_count = 0;

    for (uint32_t i = 0; i < bridge->max_correlations; i++) {
        if (bridge->correlations[i].in_use) {
            sum += bridge->correlations[i].correlation.weight;
            active_count++;
        }
    }

    if (active_count == 0) {
        return FIN_STDP_ERR_OK;
    }

    float current_mean = sum / (float)active_count;

    /* Compute scaling factor to bring mean toward target */
    float target = bridge->config.target_mean_weight;
    float tau = bridge->config.homeostatic_tau_ms;
    float scale = 1.0f + (target - current_mean) / tau;
    scale = nimcp_clampf(scale, 0.9f, 1.1f);

    /* Apply scaling */
    for (uint32_t i = 0; i < bridge->max_correlations; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_correlations > 256) {
            fin_stdp_heartbeat_global("fin_stdp_home_loop",
                (float)(i + 1) / (float)bridge->max_correlations);
        }

        if (bridge->correlations[i].in_use) {
            fin_stdp_correlation_t* c = &bridge->correlations[i].correlation;
            c->weight = nimcp_clampf(c->weight * scale,
                               bridge->config.weight_min,
                               bridge->config.weight_max);
        }
    }

    bridge->mean_weight = current_mean;
    bridge->weight_scale = scale;

    fin_stdp_heartbeat_global("fin_stdp_homeostasis", 1.0f);
    return FIN_STDP_ERR_OK;
}

int financial_stdp_bridge_consolidate(financial_stdp_bridge_t* bridge) {
    if (!bridge) {
        set_error("bridge is NULL");
        return FIN_STDP_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_STDP_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_STDP_ERR_STATE;
    }

    fin_stdp_heartbeat_global("fin_stdp_consolidate", 0.0f);

    /* Apply homeostasis */
    financial_stdp_bridge_apply_homeostasis(bridge);

    /* Decay eligibility traces */
    for (uint32_t i = 0; i < bridge->max_correlations; i++) {
        if (bridge->correlations[i].in_use) {
            bridge->correlations[i].correlation.eligibility_trace *= 0.9f;
        }
    }

    /* Prune very weak correlations (optional cleanup) */
    float prune_threshold = bridge->config.weight_min + 0.01f;
    for (uint32_t i = 0; i < bridge->max_correlations; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_correlations > 256) {
            fin_stdp_heartbeat_global("fin_stdp_cons_loop",
                (float)(i + 1) / (float)bridge->max_correlations);
        }

        if (bridge->correlations[i].in_use) {
            fin_stdp_correlation_t* c = &bridge->correlations[i].correlation;
            if (c->weight < prune_threshold && c->update_count < 10) {
                /* Very weak with few updates - remove */
                bridge->correlations[i].in_use = false;
                bridge->correlation_count--;
            }
        }
    }

    bridge->op_state = FIN_STDP_OP_STATE_ACTIVE;

    fin_stdp_heartbeat_global("fin_stdp_consolidate", 1.0f);
    return FIN_STDP_ERR_OK;
}

/* ============================================================================
 * Weight Access API
 * ============================================================================ */

int financial_stdp_bridge_get_weight(
    financial_stdp_bridge_t* bridge,
    int signal_type,
    float* out_weight)
{
    if (!bridge || !out_weight) {
        set_error("bridge or out_weight is NULL");
        return FIN_STDP_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_STDP_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_STDP_ERR_STATE;
    }

    for (uint32_t i = 0; i < bridge->max_correlations; i++) {
        if (bridge->correlations[i].in_use &&
            bridge->correlations[i].correlation.signal_type == signal_type) {
            *out_weight = bridge->correlations[i].correlation.weight;
            return FIN_STDP_ERR_OK;
        }
    }

    set_error("Signal type %d not found", signal_type);
    return FIN_STDP_ERR_NOT_FOUND;
}

int financial_stdp_bridge_get_correlation(
    financial_stdp_bridge_t* bridge,
    int signal_type,
    fin_stdp_correlation_t* out_correlation)
{
    if (!bridge || !out_correlation) {
        set_error("bridge or out_correlation is NULL");
        return FIN_STDP_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_STDP_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_STDP_ERR_STATE;
    }

    for (uint32_t i = 0; i < bridge->max_correlations; i++) {
        if (bridge->correlations[i].in_use &&
            bridge->correlations[i].correlation.signal_type == signal_type) {
            *out_correlation = bridge->correlations[i].correlation;
            return FIN_STDP_ERR_OK;
        }
    }

    set_error("Signal type %d not found", signal_type);
    return FIN_STDP_ERR_NOT_FOUND;
}

int financial_stdp_bridge_get_all_correlations(
    financial_stdp_bridge_t* bridge,
    fin_stdp_correlation_t* correlations,
    uint32_t max_count)
{
    if (!bridge || !correlations) {
        set_error("bridge or correlations is NULL");
        return -FIN_STDP_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_STDP_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return -FIN_STDP_ERR_STATE;
    }

    uint32_t count = 0;
    for (uint32_t i = 0; i < bridge->max_correlations && count < max_count; i++) {
        if (bridge->correlations[i].in_use) {
            correlations[count++] = bridge->correlations[i].correlation;
        }
    }

    return (int)count;
}

/**
 * @brief Compare correlations by weight (descending)
 */
static int compare_correlations_by_weight(const void* a, const void* b) {
    const fin_stdp_correlation_t* ca = (const fin_stdp_correlation_t*)a;
    const fin_stdp_correlation_t* cb = (const fin_stdp_correlation_t*)b;
    if (cb->weight > ca->weight) return 1;
    if (cb->weight < ca->weight) return -1;
    return 0;
}

int financial_stdp_bridge_get_top_predictive(
    financial_stdp_bridge_t* bridge,
    fin_stdp_correlation_t* correlations,
    uint32_t count)
{
    if (!bridge || !correlations) {
        set_error("bridge or correlations is NULL");
        return -FIN_STDP_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_STDP_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return -FIN_STDP_ERR_STATE;
    }

    /* First get all correlations */
    fin_stdp_correlation_t* all = (fin_stdp_correlation_t*)nimcp_malloc(
        bridge->correlation_count * sizeof(fin_stdp_correlation_t));
    if (!all) {
        return -FIN_STDP_ERR_NO_MEMORY;
    }

    int total = financial_stdp_bridge_get_all_correlations(bridge, all, bridge->correlation_count);
    if (total <= 0) {
        nimcp_free(all);
        all = NULL;
        return total;
    }

    /* Sort by weight (descending) */
    qsort(all, total, sizeof(fin_stdp_correlation_t), compare_correlations_by_weight);

    /* Copy top N */
    uint32_t to_copy = (uint32_t)total < count ? (uint32_t)total : count;
    memcpy(correlations, all, to_copy * sizeof(fin_stdp_correlation_t));

    nimcp_free(all);
    all = NULL;
    return (int)to_copy;
}

uint32_t financial_stdp_bridge_get_signal_count(
    const financial_stdp_bridge_t* bridge)
{
    if (!bridge || bridge->magic != FINANCIAL_STDP_BRIDGE_MAGIC) {
        return 0;
    }
    return bridge->correlation_count;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

fin_stdp_op_state_t financial_stdp_bridge_get_op_state(
    const financial_stdp_bridge_t* bridge)
{
    if (!bridge || bridge->magic != FINANCIAL_STDP_BRIDGE_MAGIC) {
        return FIN_STDP_OP_STATE_UNINITIALIZED;
    }
    return bridge->op_state;
}

int financial_stdp_bridge_get_stats(
    const financial_stdp_bridge_t* bridge,
    fin_stdp_bridge_stats_t* stats)
{
    if (!bridge) {
        set_error("bridge is NULL");
        return FIN_STDP_ERR_NULL;
    }
    if (!stats) {
        set_error("stats is NULL");
        return FIN_STDP_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_STDP_BRIDGE_MAGIC) {
        set_error("Invalid bridge magic");
        return FIN_STDP_ERR_STATE;
    }

    *stats = bridge->stats;
    return FIN_STDP_ERR_OK;
}

void financial_stdp_bridge_reset_stats(financial_stdp_bridge_t* bridge) {
    if (bridge && bridge->magic == FINANCIAL_STDP_BRIDGE_MAGIC) {
        memset(&bridge->stats, 0, sizeof(fin_stdp_bridge_stats_t));
    }
}

/* ============================================================================
 * Health Integration
 * ============================================================================ */

int financial_stdp_bridge_heartbeat(
    financial_stdp_bridge_t* bridge,
    const char* operation,
    float progress)
{
    if (!bridge) {
        return FIN_STDP_ERR_NULL;
    }
    if (bridge->magic != FINANCIAL_STDP_BRIDGE_MAGIC) {
        return FIN_STDP_ERR_STATE;
    }

    /* Forward to global health agent */
    fin_stdp_heartbeat_global(
        operation ? operation : "fin_stdp_heartbeat", progress);

    /* Forward to instance-level health agent */
    if (bridge->health_agent) {
        nimcp_health_agent_heartbeat_ex(
            (nimcp_health_agent_t*)bridge->health_agent, operation, progress);
    }

    bridge->stats.health_heartbeats++;
    return FIN_STDP_ERR_OK;
}
