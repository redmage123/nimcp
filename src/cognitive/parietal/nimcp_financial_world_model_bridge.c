/**
 * @file nimcp_financial_world_model_bridge.c
 * @brief Financial World Model Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-29
 *
 * WHAT: Bridge for maintaining and predicting financial market world states
 *       including counterfactual analysis and policy rollout simulation
 *
 * WHY:  Effective trading requires mental models of market dynamics that can
 *       predict forward, evaluate counterfactuals, and simulate policies.
 *
 * HOW:  Maintains world state in memory, uses prediction models (random walk,
 *       mean reversion, momentum, regime switching) for forward simulation,
 *       and integrates with NIMCP's security and monitoring infrastructure.
 *
 * @author NIMCP Development Team
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>

#include "cognitive/parietal/nimcp_financial_world_model_bridge.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

#define LOG_MODULE "financial_world_model"

/* ============================================================================
 * Health Agent Integration (Phase 8: System-Wide Health Integration)
 * ============================================================================ */
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_math_constants.h"
#include "utils/math/nimcp_math_helpers.h"

/* Health agent: using pre-existing custom implementation */
static nimcp_health_agent_t* g_financial_world_model_bridge_health_agent = NULL;

BRIDGE_DEFINE_MESH_REGISTRATION(financial_world_model_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


/** @brief Send heartbeat from financial_world_model_bridge module */
static inline void fin_world_heartbeat(const char* operation, float progress) {
    if (g_financial_world_model_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_financial_world_model_bridge_health_agent, operation, progress);
    }
}

/** @brief Send heartbeat from financial_world_model_bridge module (instance-level) */
static inline void fin_world_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_financial_world_model_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_financial_world_model_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_financial_world_model_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

/* ============================================================================
 * Thread-Local Error
 * ============================================================================ */

static _Thread_local char fin_world_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(fin_world_last_error, sizeof(fin_world_last_error), fmt, args);
    va_end(args);
}

/* ============================================================================
 * KG Wiring Integration
 * ============================================================================ */

#define KG_MSG_FIN_WORLD_STATE_UPDATE   "FIN_WORLD_STATE_UPDATE"
#define KG_MSG_FIN_WORLD_PREDICTION     "FIN_WORLD_PREDICTION"
#define KG_MSG_FIN_WORLD_COUNTERFACTUAL "FIN_WORLD_COUNTERFACTUAL"
#define KG_MSG_FIN_WORLD_ROLLOUT        "FIN_WORLD_ROLLOUT"
#define KG_MSG_FIN_WORLD_ERROR          "FIN_WORLD_ERROR"

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

/**
 * @brief Financial world model bridge structure
 */
struct financial_world_model_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    uint32_t magic;
    fin_world_bridge_state_t state;

    /* Configuration */
    fin_world_model_config_t config;

    /* Current world state */
    fin_world_state_t current_state;
    bool state_initialized;

    /* Subsystem pointers */
    void* immune;
    void* bbb;
    void* health_agent;
    void* kg_wiring;
    void* logger;
    void* security;
    void* ethics;
    void* lgss;
    void* cycle;
    void* bio_router;

    /* Callbacks */
    fin_world_state_callback_t state_cb;
    void* state_cb_data;
    fin_world_prediction_callback_t prediction_cb;
    void* prediction_cb_data;

    /* Statistics */
    fin_world_model_bridge_stats_t stats;

    /* Random state for Monte Carlo (simple LCG) */
    uint64_t rng_state;
};

/* Security integration via bridge_base */
BRIDGE_DEFINE_SECURITY_SETTERS(financial_world_model_bridge)

/* ============================================================================
 * Static Name Tables
 * ============================================================================ */

static const char* regime_names[] = {
    "bull",
    "bear",
    "sideways",
    "crisis"
};

static const char* bridge_state_names[] = {
    "uninitialized",
    "initialized",
    "active",
    "degraded",
    "error"
};

static const char* model_names[] = {
    "random_walk",
    "mean_revert",
    "momentum",
    "regime_switch"
};

/** Simple LCG random number generator */
static inline float random_uniform(financial_world_model_bridge_t* bridge) {
    bridge->rng_state = bridge->rng_state * 6364136223846793005ULL + 1442695040888963407ULL;
    return (float)(bridge->rng_state >> 33) / (float)(1ULL << 31);
}

/** Box-Muller transform for Gaussian random */
static inline float random_normal(financial_world_model_bridge_t* bridge) {
    float u1 = random_uniform(bridge);
    float u2 = random_uniform(bridge);
    if (u1 < 1e-10f) u1 = 1e-10f;  /* Avoid log(0) */
    return sqrtf(-2.0f * logf(u1)) * cosf(NIMCP_TWO_PI_F * u2);
}

/**
 * @brief Publish message through KG wiring
 */
static int bridge_kg_publish(financial_world_model_bridge_t* bridge, const char* msg_type,
                              const void* payload, size_t size) {
    if (bridge && bridge->kg_wiring && bridge->config.enable_kg_messaging) {
        bridge->stats.kg_messages_sent++;
        /* kg_wiring_publish would be called here */
        (void)msg_type; (void)payload; (void)size;
        return 0;
    }
    return 0;
}

/* ============================================================================
 * World State Helpers
 * ============================================================================ */

int fin_world_state_alloc(fin_world_state_t* state, uint32_t num_assets) {
    if (!state || num_assets == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "fin_world_state_alloc: state is NULL");
        return -1;
    }

    state->asset_prices = nimcp_calloc(num_assets, sizeof(float));
    if (!state->asset_prices) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "fin_world_state_alloc: state->asset_prices is NULL");
        return -1;
    }

    state->volatilities = nimcp_calloc(num_assets, sizeof(float));
    if (!state->volatilities) {
        nimcp_free(state->asset_prices);
        state->asset_prices = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "fin_world_state_alloc: state->volatilities is NULL");
        return -1;
    }

    state->num_assets = num_assets;
    state->regime = FIN_REGIME_SIDEWAYS;
    state->timestamp_ms = 0;
    return 0;
}

void fin_world_state_free(fin_world_state_t* state) {
    if (state) {
        if (state->asset_prices) {
            nimcp_free(state->asset_prices);
            state->asset_prices = NULL;
        }
        if (state->volatilities) {
            nimcp_free(state->volatilities);
            state->volatilities = NULL;
        }
        state->num_assets = 0;
    }
}

int fin_world_state_copy(fin_world_state_t* dst, const fin_world_state_t* src) {
    if (!dst || !src || !dst->asset_prices || !dst->volatilities) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fin_world_state_copy: required parameter is NULL (dst, src, dst->asset_prices, dst->volatilities)");
        return -1;
    }
    if (dst->num_assets < src->num_assets) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "fin_world_state_copy: validation failed");
        return -1;
    }

    memcpy(dst->asset_prices, src->asset_prices, src->num_assets * sizeof(float));
    memcpy(dst->volatilities, src->volatilities, src->num_assets * sizeof(float));
    dst->regime = src->regime;
    dst->num_assets = src->num_assets;
    dst->timestamp_ms = src->timestamp_ms;
    return 0;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int financial_world_model_bridge_default_config(fin_world_model_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");
        return -1;
    }

    fin_world_heartbeat("fin_world_default_config", 0.0f);

    memset(config, 0, sizeof(*config));

    /* Model settings */
    config->max_assets = FIN_WORLD_MAX_ASSETS;
    config->default_trajectory_len = 100;
    config->default_model = FIN_PRED_MODEL_RANDOM_WALK;

    /* Simulation parameters */
    config->volatility_scaling = 1.0f;
    config->mean_reversion_rate = 0.1f;
    config->monte_carlo_samples = 1000;

    /* Integration settings */
    config->enable_immune_integration = true;
    config->enable_bbb_validation = true;
    config->enable_kg_messaging = true;
    config->enable_health_monitoring = true;

    /* Logging */
    config->verbose_logging = false;

    fin_world_heartbeat("fin_world_default_config", 1.0f);
    return 0;
}

financial_world_model_bridge_t* financial_world_model_bridge_create(
    const fin_world_model_config_t* config
) {
    fin_world_heartbeat("fin_world_create", 0.0f);

    financial_world_model_bridge_t* bridge = nimcp_calloc(1, sizeof(financial_world_model_bridge_t));
    if (!bridge) {
        set_error("Failed to allocate financial_world_model_bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY, "Failed to allocate financial_world_model_bridge");
        return NULL;
    }

    bridge->magic = FINANCIAL_WORLD_MODEL_BRIDGE_MAGIC;
    bridge->state = FIN_WORLD_STATE_UNINITIALIZED;

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        financial_world_model_bridge_default_config(&bridge->config);
    }

    /* Initialize bridge base (creates mutex) */
    if (bridge_base_init(&bridge->base, BIO_MODULE_FINANCIAL_WORLD_MODEL, "financial_world_model") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "financial_world_model_bridge_create: validation failed");
        return NULL;
    }

    /* Allocate world state */
    if (fin_world_state_alloc(&bridge->current_state, bridge->config.max_assets) != 0) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        set_error("Failed to allocate world state");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "financial_world_model_bridge_create: validation failed");
        return NULL;
    }

    /* Initialize RNG */
    bridge->rng_state = nimcp_time_get_ms() ^ 0xDEADBEEF;

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    bridge->state_initialized = false;
    bridge->state = FIN_WORLD_STATE_INITIALIZED;

    fin_world_heartbeat("fin_world_create", 1.0f);
    return bridge;
}

void financial_world_model_bridge_destroy(financial_world_model_bridge_t* bridge) {
    if (!bridge || bridge->magic != FINANCIAL_WORLD_MODEL_BRIDGE_MAGIC) {
        return;
    }

    fin_world_heartbeat("fin_world_destroy", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    /* Free world state */
    fin_world_state_free(&bridge->current_state);

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Cleanup base */
    bridge_base_cleanup(&bridge->base);

    bridge->magic = 0;
    nimcp_free(bridge);
    bridge = NULL;
}

int financial_world_model_bridge_reset(financial_world_model_bridge_t* bridge) {
    if (!bridge || bridge->magic != FINANCIAL_WORLD_MODEL_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_world_model_bridge_reset: invalid bridge");
        return FIN_WORLD_ERR_NULL;
    }

    fin_world_heartbeat("fin_world_reset", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset world state to zeros */
    memset(bridge->current_state.asset_prices, 0,
           bridge->current_state.num_assets * sizeof(float));
    memset(bridge->current_state.volatilities, 0,
           bridge->current_state.num_assets * sizeof(float));
    bridge->current_state.regime = FIN_REGIME_SIDEWAYS;
    bridge->current_state.timestamp_ms = 0;
    bridge->state_initialized = false;

    /* Reset stats */
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    /* Reset RNG */
    bridge->rng_state = nimcp_time_get_ms() ^ 0xDEADBEEF;

    bridge->state = FIN_WORLD_STATE_INITIALIZED;

    nimcp_mutex_unlock(bridge->base.mutex);

    fin_world_heartbeat("fin_world_reset", 1.0f);
    return FIN_WORLD_ERR_OK;
}

/* ============================================================================
 * Subsystem Setters
 * ============================================================================ */

#define FIN_WORLD_SETTER(name, field) \
    int financial_world_model_bridge_set_##name(financial_world_model_bridge_t* bridge, void* ptr) { \
        if (!bridge || bridge->magic != FINANCIAL_WORLD_MODEL_BRIDGE_MAGIC) { \
            set_error("bridge is NULL in set_" #name); \
            NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_world_model_bridge_set_" #name ": bridge is NULL"); \
            return FIN_WORLD_ERR_NULL; \
        } \
        nimcp_mutex_lock(bridge->base.mutex); \
        bridge->field = ptr; \
        nimcp_mutex_unlock(bridge->base.mutex); \
        return FIN_WORLD_ERR_OK; \
    }

FIN_WORLD_SETTER(immune,        immune)
FIN_WORLD_SETTER(health_agent,  health_agent)
FIN_WORLD_SETTER(kg_wiring,     kg_wiring)
FIN_WORLD_SETTER(logger,        logger)
FIN_WORLD_SETTER(security,      security)
FIN_WORLD_SETTER(bio_router,    bio_router)

/* Security setters for bbb, ethics, lgss, coordinator handled by bridge_base */

/* ============================================================================
 * World State API Implementation
 * ============================================================================ */

int financial_world_model_bridge_set_state(
    financial_world_model_bridge_t* bridge,
    const fin_world_state_t* state
) {
    if (!bridge || bridge->magic != FINANCIAL_WORLD_MODEL_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "set_state: invalid bridge");
        return FIN_WORLD_ERR_NULL;
    }
    if (!state || !state->asset_prices || !state->volatilities) {
        set_error("state is NULL or incomplete");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "set_state: state is NULL");
        return FIN_WORLD_ERR_NULL;
    }

    fin_world_heartbeat("fin_world_set_state", 0.0f);

    /* BBB validation */
    if (bridge->config.enable_bbb_validation && bridge->bbb) {
        BRIDGE_BBB_VALIDATE(bridge, state->asset_prices, state->num_assets * sizeof(float));
        bridge->stats.bbb_validations++;
    }

    /* Immune check */
    if (bridge->config.enable_immune_integration && bridge->immune) {
        bridge->stats.immune_checks++;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Check capacity */
    if (state->num_assets > bridge->config.max_assets) {
        nimcp_mutex_unlock(bridge->base.mutex);
        set_error("Too many assets: %u > %u", state->num_assets, bridge->config.max_assets);
        return FIN_WORLD_ERR_CAPACITY;
    }

    /* Copy state */
    memcpy(bridge->current_state.asset_prices, state->asset_prices,
           state->num_assets * sizeof(float));
    memcpy(bridge->current_state.volatilities, state->volatilities,
           state->num_assets * sizeof(float));
    bridge->current_state.regime = state->regime;
    bridge->current_state.num_assets = state->num_assets;
    bridge->current_state.timestamp_ms = state->timestamp_ms ? state->timestamp_ms : nimcp_time_get_ms();

    bridge->state_initialized = true;
    bridge->stats.state_updates++;
    bridge->state = FIN_WORLD_STATE_ACTIVE;

    /* Fire callback if registered */
    fin_world_state_callback_t cb = bridge->state_cb;
    void* cb_data = bridge->state_cb_data;

    nimcp_mutex_unlock(bridge->base.mutex);

    if (cb) {
        cb(&bridge->current_state, cb_data);
    }

    /* KG notification */
    bridge_kg_publish(bridge, KG_MSG_FIN_WORLD_STATE_UPDATE, state, sizeof(*state));

    fin_world_heartbeat("fin_world_set_state", 1.0f);
    bridge->stats.health_heartbeats++;

    return FIN_WORLD_ERR_OK;
}

int financial_world_model_bridge_get_state(
    const financial_world_model_bridge_t* bridge,
    fin_world_state_t* state
) {
    if (!bridge || bridge->magic != FINANCIAL_WORLD_MODEL_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "get_state: invalid bridge");
        return FIN_WORLD_ERR_NULL;
    }
    if (!state || !state->asset_prices || !state->volatilities) {
        set_error("state is NULL or incomplete");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "get_state: state is NULL");
        return FIN_WORLD_ERR_NULL;
    }

    fin_world_heartbeat("fin_world_get_state", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->state_initialized) {
        nimcp_mutex_unlock(bridge->base.mutex);
        set_error("World state not initialized");
        return FIN_WORLD_ERR_STATE;
    }

    if (fin_world_state_copy(state, &bridge->current_state) != 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        set_error("Failed to copy state");
        return FIN_WORLD_ERR_NO_MEMORY;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return FIN_WORLD_ERR_OK;
}

int financial_world_model_bridge_update_asset(
    financial_world_model_bridge_t* bridge,
    uint32_t asset_index,
    float price,
    float volatility
) {
    if (!bridge || bridge->magic != FINANCIAL_WORLD_MODEL_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        return FIN_WORLD_ERR_NULL;
    }

    fin_world_heartbeat("fin_world_update_asset", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    if (asset_index >= bridge->current_state.num_assets) {
        nimcp_mutex_unlock(bridge->base.mutex);
        set_error("Asset index out of range: %u >= %u", asset_index, bridge->current_state.num_assets);
        return FIN_WORLD_ERR_INVALID_PARAM;
    }

    bridge->current_state.asset_prices[asset_index] = price;
    bridge->current_state.volatilities[asset_index] = volatility;
    bridge->current_state.timestamp_ms = nimcp_time_get_ms();
    bridge->state_initialized = true;
    bridge->stats.state_updates++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return FIN_WORLD_ERR_OK;
}

int financial_world_model_bridge_set_regime(
    financial_world_model_bridge_t* bridge,
    fin_market_regime_t regime
) {
    if (!bridge || bridge->magic != FINANCIAL_WORLD_MODEL_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        return FIN_WORLD_ERR_NULL;
    }

    if (regime >= FIN_REGIME_COUNT) {
        set_error("Invalid regime: %d", regime);
        return FIN_WORLD_ERR_INVALID_PARAM;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->current_state.regime = regime;
    bridge->current_state.timestamp_ms = nimcp_time_get_ms();
    nimcp_mutex_unlock(bridge->base.mutex);

    return FIN_WORLD_ERR_OK;
}

/* ============================================================================
 * Prediction Implementation
 * ============================================================================ */

/**
 * @brief Apply one prediction step using random walk
 */
static void predict_step_random_walk(financial_world_model_bridge_t* bridge,
                                      fin_world_state_t* state) {
    for (uint32_t i = 0; i < state->num_assets; i++) {
        float vol = state->volatilities[i] * bridge->config.volatility_scaling;
        float shock = random_normal(bridge) * vol / sqrtf(252.0f);  /* Daily vol to step */
        state->asset_prices[i] *= (1.0f + shock);
        if (state->asset_prices[i] < 0.01f) state->asset_prices[i] = 0.01f;
    }
}

/**
 * @brief Apply one prediction step using mean reversion (OU process)
 */
static void predict_step_mean_revert(financial_world_model_bridge_t* bridge,
                                      fin_world_state_t* state,
                                      const float* mean_prices) {
    float kappa = bridge->config.mean_reversion_rate;
    for (uint32_t i = 0; i < state->num_assets; i++) {
        float vol = state->volatilities[i] * bridge->config.volatility_scaling;
        float shock = random_normal(bridge) * vol / sqrtf(252.0f);
        float mean = mean_prices ? mean_prices[i] : state->asset_prices[i];
        float drift = kappa * (mean - state->asset_prices[i]) / 252.0f;
        state->asset_prices[i] += drift + state->asset_prices[i] * shock;
        if (state->asset_prices[i] < 0.01f) state->asset_prices[i] = 0.01f;
    }
}

/**
 * @brief Apply one prediction step using momentum
 */
static void predict_step_momentum(financial_world_model_bridge_t* bridge,
                                   fin_world_state_t* state,
                                   const float* prev_prices) {
    for (uint32_t i = 0; i < state->num_assets; i++) {
        float vol = state->volatilities[i] * bridge->config.volatility_scaling;
        float shock = random_normal(bridge) * vol / sqrtf(252.0f);
        float momentum = 0.0f;
        if (prev_prices && prev_prices[i] > 0.01f) {
            momentum = (state->asset_prices[i] - prev_prices[i]) / prev_prices[i];
            momentum = nimcp_clampf(momentum, -0.1f, 0.1f);  /* Cap momentum */
        }
        float drift = momentum * 0.1f;  /* Momentum contribution */
        state->asset_prices[i] *= (1.0f + drift + shock);
        if (state->asset_prices[i] < 0.01f) state->asset_prices[i] = 0.01f;
    }
}

int financial_world_model_bridge_predict_forward(
    financial_world_model_bridge_t* bridge,
    uint32_t horizon,
    fin_prediction_model_t model,
    fin_world_state_t* trajectory
) {
    if (!bridge || bridge->magic != FINANCIAL_WORLD_MODEL_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "predict_forward: invalid bridge");
        return FIN_WORLD_ERR_NULL;
    }
    if (!trajectory) {
        set_error("trajectory is NULL");
        return FIN_WORLD_ERR_NULL;
    }
    if (horizon == 0 || horizon > FIN_WORLD_MAX_TRAJECTORY) {
        set_error("Invalid horizon: %u", horizon);
        return FIN_WORLD_ERR_INVALID_PARAM;
    }
    if (model >= FIN_PRED_MODEL_COUNT) {
        set_error("Invalid model: %d", model);
        return FIN_WORLD_ERR_INVALID_PARAM;
    }

    fin_world_heartbeat("fin_world_predict", 0.0f);

    /* BBB validation */
    if (bridge->config.enable_bbb_validation && bridge->bbb) {
        bridge->stats.bbb_validations++;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->state_initialized) {
        nimcp_mutex_unlock(bridge->base.mutex);
        set_error("World state not initialized");
        return FIN_WORLD_ERR_STATE;
    }

    uint32_t num_assets = bridge->current_state.num_assets;

    /* Allocate working state */
    fin_world_state_t work_state;
    if (fin_world_state_alloc(&work_state, num_assets) != 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        set_error("Failed to allocate working state");
        return FIN_WORLD_ERR_NO_MEMORY;
    }
    fin_world_state_copy(&work_state, &bridge->current_state);

    /* Previous prices for momentum */
    float* prev_prices = nimcp_calloc(num_assets, sizeof(float));
    if (!prev_prices) {
        fin_world_state_free(&work_state);
        nimcp_mutex_unlock(bridge->base.mutex);
        return FIN_WORLD_ERR_NO_MEMORY;
    }
    memcpy(prev_prices, bridge->current_state.asset_prices, num_assets * sizeof(float));

    /* Generate trajectory */
    for (uint32_t t = 0; t < horizon; t++) {
        switch (model) {
            case FIN_PRED_MODEL_RANDOM_WALK:
                predict_step_random_walk(bridge, &work_state);
                break;
            case FIN_PRED_MODEL_MEAN_REVERT:
                predict_step_mean_revert(bridge, &work_state, bridge->current_state.asset_prices);
                break;
            case FIN_PRED_MODEL_MOMENTUM:
                predict_step_momentum(bridge, &work_state, prev_prices);
                /* Update prev for next step */
                if (t > 0) {
                    memcpy(prev_prices, trajectory[t-1].asset_prices, num_assets * sizeof(float));
                }
                break;
            case FIN_PRED_MODEL_REGIME_SWITCH:
                /* Simplified: vary vol based on regime */
                {
                    float vol_mult = 1.0f;
                    if (work_state.regime == FIN_REGIME_CRISIS) vol_mult = 2.0f;
                    else if (work_state.regime == FIN_REGIME_BULL) vol_mult = 0.8f;
                    float orig_scaling = bridge->config.volatility_scaling;
                    bridge->config.volatility_scaling *= vol_mult;
                    predict_step_random_walk(bridge, &work_state);
                    bridge->config.volatility_scaling = orig_scaling;
                }
                break;
            default:
                predict_step_random_walk(bridge, &work_state);
                break;
        }

        work_state.timestamp_ms = bridge->current_state.timestamp_ms + (t + 1) * 86400000ULL; /* +1 day */

        /* Copy to output trajectory */
        if (trajectory[t].asset_prices && trajectory[t].volatilities) {
            fin_world_state_copy(&trajectory[t], &work_state);
        }

        /* Progress heartbeat */
        if (t % 10 == 0) {
            fin_world_heartbeat("fin_world_predict", (float)(t + 1) / (float)horizon);
        }
    }

    nimcp_free(prev_prices);
    prev_prices = NULL;
    fin_world_state_free(&work_state);

    bridge->stats.predictions++;

    /* Fire callback if registered */
    fin_world_prediction_callback_t cb = bridge->prediction_cb;
    void* cb_data = bridge->prediction_cb_data;

    nimcp_mutex_unlock(bridge->base.mutex);

    if (cb) {
        cb(trajectory, horizon, cb_data);
    }

    /* KG notification */
    bridge_kg_publish(bridge, KG_MSG_FIN_WORLD_PREDICTION, NULL, 0);

    fin_world_heartbeat("fin_world_predict", 1.0f);
    bridge->stats.health_heartbeats++;

    return FIN_WORLD_ERR_OK;
}

int financial_world_model_bridge_predict_monte_carlo(
    financial_world_model_bridge_t* bridge,
    uint32_t horizon,
    uint32_t num_samples,
    fin_world_state_t** trajectories,
    float* probabilities
) {
    if (!bridge || bridge->magic != FINANCIAL_WORLD_MODEL_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        return FIN_WORLD_ERR_NULL;
    }
    if (!trajectories || !probabilities) {
        set_error("Output arrays are NULL");
        return FIN_WORLD_ERR_NULL;
    }

    fin_world_heartbeat("fin_world_monte_carlo", 0.0f);

    /* Generate each sample trajectory */
    for (uint32_t s = 0; s < num_samples; s++) {
        if (trajectories[s]) {
            int rc = financial_world_model_bridge_predict_forward(
                bridge, horizon, bridge->config.default_model, trajectories[s]);
            if (rc != FIN_WORLD_ERR_OK) {
                return rc;
            }
        }
        probabilities[s] = 1.0f / (float)num_samples;  /* Equal weights */

        if (s % 100 == 0) {
            fin_world_heartbeat("fin_world_monte_carlo", (float)(s + 1) / (float)num_samples);
        }
    }

    fin_world_heartbeat("fin_world_monte_carlo", 1.0f);
    return FIN_WORLD_ERR_OK;
}

/* ============================================================================
 * Counterfactual Implementation
 * ============================================================================ */

int financial_world_model_bridge_counterfactual(
    financial_world_model_bridge_t* bridge,
    const fin_world_state_t* initial_perturbation,
    uint32_t horizon,
    fin_counterfactual_result_t* result
) {
    if (!bridge || bridge->magic != FINANCIAL_WORLD_MODEL_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "counterfactual: invalid bridge");
        return FIN_WORLD_ERR_NULL;
    }
    if (!result) {
        set_error("result is NULL");
        return FIN_WORLD_ERR_NULL;
    }

    fin_world_heartbeat("fin_world_counterfactual", 0.0f);

    memset(result, 0, sizeof(*result));

    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->state_initialized) {
        nimcp_mutex_unlock(bridge->base.mutex);
        set_error("World state not initialized");
        return FIN_WORLD_ERR_STATE;
    }

    uint32_t num_assets = bridge->current_state.num_assets;

    /* Allocate trajectory */
    result->trajectory = nimcp_calloc(horizon, sizeof(fin_world_state_t));
    if (!result->trajectory) {
        nimcp_mutex_unlock(bridge->base.mutex);
        set_error("Failed to allocate trajectory");
        return FIN_WORLD_ERR_NO_MEMORY;
    }

    /* Allocate each state in trajectory */
    for (uint32_t t = 0; t < horizon; t++) {
        if (fin_world_state_alloc(&result->trajectory[t], num_assets) != 0) {
            for (uint32_t i = 0; i < t; i++) {
                fin_world_state_free(&result->trajectory[i]);
            }
            nimcp_free(result->trajectory);
            result->trajectory = NULL;
            nimcp_mutex_unlock(bridge->base.mutex);
            return FIN_WORLD_ERR_NO_MEMORY;
        }
    }

    result->cumulative_returns = nimcp_calloc(horizon, sizeof(float));
    if (!result->cumulative_returns) {
        for (uint32_t t = 0; t < horizon; t++) {
            fin_world_state_free(&result->trajectory[t]);
        }
        nimcp_free(result->trajectory);
        result->trajectory = NULL;
        nimcp_mutex_unlock(bridge->base.mutex);
        return FIN_WORLD_ERR_NO_MEMORY;
    }

    /* Apply perturbation to initial state */
    fin_world_state_t perturbed;
    if (fin_world_state_alloc(&perturbed, num_assets) != 0) {
        nimcp_free(result->cumulative_returns);
        for (uint32_t t = 0; t < horizon; t++) {
            fin_world_state_free(&result->trajectory[t]);
        }
        nimcp_free(result->trajectory);
        nimcp_mutex_unlock(bridge->base.mutex);
        return FIN_WORLD_ERR_NO_MEMORY;
    }
    fin_world_state_copy(&perturbed, &bridge->current_state);

    /* Apply perturbation: add price deltas */
    if (initial_perturbation && initial_perturbation->asset_prices) {
        uint32_t perturb_count = initial_perturbation->num_assets < num_assets ?
                                  initial_perturbation->num_assets : num_assets;
        for (uint32_t i = 0; i < perturb_count; i++) {
            perturbed.asset_prices[i] += initial_perturbation->asset_prices[i];
            if (perturbed.asset_prices[i] < 0.01f) {
                perturbed.asset_prices[i] = 0.01f;
            }
        }
        if (initial_perturbation->regime != 0 || initial_perturbation == NULL) {
            /* Optionally override regime if specified */
            if (initial_perturbation->regime < FIN_REGIME_COUNT) {
                perturbed.regime = initial_perturbation->regime;
            }
        }
    }

    /* Save original state temporarily */
    fin_world_state_t original;
    fin_world_state_alloc(&original, num_assets);
    fin_world_state_copy(&original, &bridge->current_state);

    /* Set perturbed state */
    fin_world_state_copy(&bridge->current_state, &perturbed);

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Run prediction from perturbed state */
    int rc = financial_world_model_bridge_predict_forward(
        bridge, horizon, bridge->config.default_model, result->trajectory);

    /* Restore original state */
    nimcp_mutex_lock(bridge->base.mutex);
    fin_world_state_copy(&bridge->current_state, &original);
    nimcp_mutex_unlock(bridge->base.mutex);

    fin_world_state_free(&original);
    fin_world_state_free(&perturbed);

    if (rc != FIN_WORLD_ERR_OK) {
        financial_world_model_bridge_free_counterfactual(result);
        return rc;
    }

    result->trajectory_len = horizon;

    /* Calculate cumulative returns */
    float initial_value = 0.0f;
    for (uint32_t i = 0; i < num_assets; i++) {
        initial_value += bridge->current_state.asset_prices[i];
    }

    for (uint32_t t = 0; t < horizon; t++) {
        float current_value = 0.0f;
        for (uint32_t i = 0; i < num_assets; i++) {
            current_value += result->trajectory[t].asset_prices[i];
        }
        result->cumulative_returns[t] = (current_value - initial_value) / initial_value;
    }

    /* Calculate expected Sharpe (simplified) */
    float mean_return = result->cumulative_returns[horizon - 1] / (float)horizon;
    float var_sum = 0.0f;
    for (uint32_t t = 1; t < horizon; t++) {
        float daily_ret = result->cumulative_returns[t] - result->cumulative_returns[t - 1];
        var_sum += (daily_ret - mean_return) * (daily_ret - mean_return);
    }
    float std_dev = sqrtf(var_sum / (float)(horizon - 1));
    result->expected_sharpe = std_dev > 0.0001f ? (mean_return / std_dev) * sqrtf(252.0f) : 0.0f;
    result->probability = 1.0f;  /* Single scenario */

    bridge->stats.counterfactuals++;

    /* KG notification */
    bridge_kg_publish(bridge, KG_MSG_FIN_WORLD_COUNTERFACTUAL, NULL, 0);

    fin_world_heartbeat("fin_world_counterfactual", 1.0f);
    bridge->stats.health_heartbeats++;

    return FIN_WORLD_ERR_OK;
}

void financial_world_model_bridge_free_counterfactual(fin_counterfactual_result_t* result) {
    if (!result) return;

    if (result->trajectory) {
        for (uint32_t t = 0; t < result->trajectory_len; t++) {
            fin_world_state_free(&result->trajectory[t]);
        }
        nimcp_free(result->trajectory);
        result->trajectory = NULL;
    }

    if (result->cumulative_returns) {
        nimcp_free(result->cumulative_returns);
        result->cumulative_returns = NULL;
    }

    result->trajectory_len = 0;
}

/* ============================================================================
 * Policy Rollout Implementation
 * ============================================================================ */

int financial_world_model_bridge_rollout_policy(
    financial_world_model_bridge_t* bridge,
    const fin_policy_action_t* actions,
    uint32_t num_actions,
    float initial_capital,
    fin_rollout_result_t* result
) {
    if (!bridge || bridge->magic != FINANCIAL_WORLD_MODEL_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "rollout_policy: invalid bridge");
        return FIN_WORLD_ERR_NULL;
    }
    if (!actions || num_actions == 0 || !result) {
        set_error("Invalid parameters");
        return FIN_WORLD_ERR_INVALID_PARAM;
    }

    fin_world_heartbeat("fin_world_rollout", 0.0f);

    memset(result, 0, sizeof(*result));

    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->state_initialized) {
        nimcp_mutex_unlock(bridge->base.mutex);
        set_error("World state not initialized");
        return FIN_WORLD_ERR_STATE;
    }

    uint32_t num_assets = bridge->current_state.num_assets;
    uint32_t horizon = num_actions;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Allocate trajectory */
    result->trajectory = nimcp_calloc(horizon, sizeof(fin_world_state_t));
    if (!result->trajectory) {
        set_error("Failed to allocate trajectory");
        return FIN_WORLD_ERR_NO_MEMORY;
    }

    for (uint32_t t = 0; t < horizon; t++) {
        if (fin_world_state_alloc(&result->trajectory[t], num_assets) != 0) {
            for (uint32_t i = 0; i < t; i++) {
                fin_world_state_free(&result->trajectory[i]);
            }
            nimcp_free(result->trajectory);
            result->trajectory = NULL;
            return FIN_WORLD_ERR_NO_MEMORY;
        }
    }

    result->portfolio_values = nimcp_calloc(horizon, sizeof(float));
    if (!result->portfolio_values) {
        for (uint32_t t = 0; t < horizon; t++) {
            fin_world_state_free(&result->trajectory[t]);
        }
        nimcp_free(result->trajectory);
        result->trajectory = NULL;
        return FIN_WORLD_ERR_NO_MEMORY;
    }

    /* First predict market trajectory */
    int rc = financial_world_model_bridge_predict_forward(
        bridge, horizon, bridge->config.default_model, result->trajectory);
    if (rc != FIN_WORLD_ERR_OK) {
        financial_world_model_bridge_free_rollout(result);
        return rc;
    }

    result->trajectory_len = horizon;

    /* Simulate policy execution */
    float* positions = nimcp_calloc(num_assets, sizeof(float));  /* Position in each asset */
    if (!positions) {
        financial_world_model_bridge_free_rollout(result);
        return FIN_WORLD_ERR_NO_MEMORY;
    }

    float cash = initial_capital;
    float peak_value = initial_capital;
    float max_drawdown = 0.0f;
    uint32_t num_trades = 0;

    for (uint32_t t = 0; t < horizon; t++) {
        const fin_policy_action_t* action = &actions[t];

        /* Execute action */
        if (action->asset_index < num_assets) {
            float price = result->trajectory[t].asset_prices[action->asset_index];
            float position_change = action->position_delta * (cash +
                positions[action->asset_index] * price);

            if (position_change > 0 && cash >= position_change) {
                /* Buy */
                float shares = position_change / price;
                positions[action->asset_index] += shares;
                cash -= position_change;
                num_trades++;
            } else if (position_change < 0 && positions[action->asset_index] > 0) {
                /* Sell */
                float shares = -position_change / price;
                shares = shares < positions[action->asset_index] ? shares : positions[action->asset_index];
                positions[action->asset_index] -= shares;
                cash += shares * price;
                num_trades++;
            }

            /* Check stop loss / take profit */
            if (action->stop_loss > 0 && price <= action->stop_loss) {
                cash += positions[action->asset_index] * price;
                positions[action->asset_index] = 0;
                num_trades++;
            }
            if (action->take_profit > 0 && price >= action->take_profit) {
                cash += positions[action->asset_index] * price;
                positions[action->asset_index] = 0;
                num_trades++;
            }
        }

        /* Calculate portfolio value */
        float portfolio_value = cash;
        for (uint32_t i = 0; i < num_assets; i++) {
            portfolio_value += positions[i] * result->trajectory[t].asset_prices[i];
        }
        result->portfolio_values[t] = portfolio_value;

        /* Track drawdown */
        if (portfolio_value > peak_value) {
            peak_value = portfolio_value;
        }
        float drawdown = (peak_value - portfolio_value) / peak_value;
        if (drawdown > max_drawdown) {
            max_drawdown = drawdown;
        }

        fin_world_heartbeat("fin_world_rollout", (float)(t + 1) / (float)horizon);
    }

    nimcp_free(positions);
    positions = NULL;

    result->final_pnl = result->portfolio_values[horizon - 1] - initial_capital;
    result->max_drawdown = max_drawdown;
    result->num_trades = num_trades;

    /* Calculate Sharpe */
    float mean_return = result->final_pnl / initial_capital / (float)horizon;
    float var_sum = 0.0f;
    for (uint32_t t = 1; t < horizon; t++) {
        float daily_ret = (result->portfolio_values[t] - result->portfolio_values[t - 1]) /
                          result->portfolio_values[t - 1];
        var_sum += (daily_ret - mean_return) * (daily_ret - mean_return);
    }
    float std_dev = sqrtf(var_sum / (float)(horizon - 1));
    result->expected_sharpe = std_dev > 0.0001f ? (mean_return / std_dev) * sqrtf(252.0f) : 0.0f;

    bridge->stats.rollouts++;

    /* KG notification */
    bridge_kg_publish(bridge, KG_MSG_FIN_WORLD_ROLLOUT, NULL, 0);

    fin_world_heartbeat("fin_world_rollout", 1.0f);
    bridge->stats.health_heartbeats++;

    return FIN_WORLD_ERR_OK;
}

void financial_world_model_bridge_free_rollout(fin_rollout_result_t* result) {
    if (!result) return;

    if (result->trajectory) {
        for (uint32_t t = 0; t < result->trajectory_len; t++) {
            fin_world_state_free(&result->trajectory[t]);
        }
        nimcp_free(result->trajectory);
        result->trajectory = NULL;
    }

    if (result->portfolio_values) {
        nimcp_free(result->portfolio_values);
        result->portfolio_values = NULL;
    }

    result->trajectory_len = 0;
}

/* ============================================================================
 * Callback Registration
 * ============================================================================ */

int financial_world_model_bridge_set_state_callback(
    financial_world_model_bridge_t* bridge,
    fin_world_state_callback_t callback,
    void* user_data
) {
    if (!bridge || bridge->magic != FINANCIAL_WORLD_MODEL_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        return FIN_WORLD_ERR_NULL;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state_cb = callback;
    bridge->state_cb_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return FIN_WORLD_ERR_OK;
}

int financial_world_model_bridge_set_prediction_callback(
    financial_world_model_bridge_t* bridge,
    fin_world_prediction_callback_t callback,
    void* user_data
) {
    if (!bridge || bridge->magic != FINANCIAL_WORLD_MODEL_BRIDGE_MAGIC) {
        set_error("bridge is NULL or invalid");
        return FIN_WORLD_ERR_NULL;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->prediction_cb = callback;
    bridge->prediction_cb_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return FIN_WORLD_ERR_OK;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

fin_world_bridge_state_t financial_world_model_bridge_get_bridge_state(
    const financial_world_model_bridge_t* bridge
) {
    if (!bridge || bridge->magic != FINANCIAL_WORLD_MODEL_BRIDGE_MAGIC) {
        return FIN_WORLD_STATE_ERROR;
    }
    return bridge->state;
}

int financial_world_model_bridge_get_stats(
    const financial_world_model_bridge_t* bridge,
    fin_world_model_bridge_stats_t* stats
) {
    if (!bridge || bridge->magic != FINANCIAL_WORLD_MODEL_BRIDGE_MAGIC || !stats) {
        set_error("NULL argument in get_stats");
        return FIN_WORLD_ERR_NULL;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return FIN_WORLD_ERR_OK;
}

void financial_world_model_bridge_reset_stats(financial_world_model_bridge_t* bridge) {
    if (!bridge || bridge->magic != FINANCIAL_WORLD_MODEL_BRIDGE_MAGIC) {
        return;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->base.mutex);
}

uint32_t financial_world_model_bridge_get_num_assets(
    const financial_world_model_bridge_t* bridge
) {
    if (!bridge || bridge->magic != FINANCIAL_WORLD_MODEL_BRIDGE_MAGIC) {
        return 0;
    }
    return bridge->current_state.num_assets;
}

const char* financial_world_model_bridge_get_last_error(void) {
    return fin_world_last_error;
}

/* ============================================================================
 * Health Integration
 * ============================================================================ */

int financial_world_model_bridge_heartbeat(
    financial_world_model_bridge_t* bridge,
    const char* operation,
    float progress
) {
    if (!bridge || bridge->magic != FINANCIAL_WORLD_MODEL_BRIDGE_MAGIC) {
        return FIN_WORLD_ERR_NULL;
    }

    fin_world_heartbeat(operation ? operation : "fin_world_heartbeat", progress);

    if (bridge->health_agent) {
        nimcp_health_agent_heartbeat_ex(
            (nimcp_health_agent_t*)bridge->health_agent, operation, progress);
    }

    bridge->stats.health_heartbeats++;
    return FIN_WORLD_ERR_OK;
}

/* ============================================================================
 * Training Hooks (B23 Upgrade Compatibility)
 * ============================================================================ */

int financial_world_model_bridge_training_begin(financial_world_model_bridge_t* bridge) {
    if (!bridge || bridge->magic != FINANCIAL_WORLD_MODEL_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "financial_world_model_bridge_training_begin: NULL argument");
        return -1;
    }
    fin_world_heartbeat_instance((nimcp_health_agent_t*)bridge->health_agent,
                                  "financial_world_model_bridge_training_begin", 0.0f);
    return 0;
}

int financial_world_model_bridge_training_end(financial_world_model_bridge_t* bridge) {
    if (!bridge || bridge->magic != FINANCIAL_WORLD_MODEL_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "financial_world_model_bridge_training_end: NULL argument");
        return -1;
    }
    fin_world_heartbeat_instance((nimcp_health_agent_t*)bridge->health_agent,
                                  "financial_world_model_bridge_training_end", 1.0f);
    return 0;
}

int financial_world_model_bridge_training_step(financial_world_model_bridge_t* bridge, float progress) {
    if (!bridge || bridge->magic != FINANCIAL_WORLD_MODEL_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "financial_world_model_bridge_training_step: NULL argument");
        return -1;
    }

    /* Safety gates: ethics + LGSS pre-check */
    BRIDGE_ETHICS_GATE(bridge, "financial_world_model_bridge_training_step");
    BRIDGE_LGSS_GATE(bridge, "financial_world_model_bridge_training_step");

    fin_world_heartbeat_instance((nimcp_health_agent_t*)bridge->health_agent,
                                  "financial_world_model_bridge_training_step", progress);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* fin_world_regime_name(fin_market_regime_t regime) {
    if (regime >= FIN_REGIME_COUNT) {
        return "unknown";
    }
    return regime_names[regime];
}

const char* fin_world_bridge_state_name(fin_world_bridge_state_t state) {
    if (state > FIN_WORLD_STATE_ERROR) {
        return "unknown";
    }
    return bridge_state_names[state];
}

const char* fin_world_model_name(fin_prediction_model_t model) {
    if (model >= FIN_PRED_MODEL_COUNT) {
        return "unknown";
    }
    return model_names[model];
}

const char* financial_world_model_bridge_version(void) {
    return FINANCIAL_WORLD_MODEL_BRIDGE_VERSION;
}
