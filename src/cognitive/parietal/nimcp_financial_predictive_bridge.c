//=============================================================================
// nimcp_financial_predictive_bridge.c - Financial Predictive Coding Bridge
//=============================================================================
/**
 * @file nimcp_financial_predictive_bridge.c
 * @brief Implementation of predictive coding for financial decision making
 *
 * @author NIMCP Development Team
 * @date 2026-01-29
 */

#include "cognitive/parietal/nimcp_financial_predictive_bridge.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/error/nimcp_error_codes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_learning_constants.h"
#include "constants/nimcp_threshold_constants.h"
#include "constants/nimcp_neural_constants.h"
#include "constants/nimcp_math_constants.h"
#include "utils/math/nimcp_math_helpers.h"

/* Health agent: using pre-existing custom implementation */
static nimcp_health_agent_t* g_fin_predictive_health_agent = NULL;


/* Forward declarations for immune/BBB validation stubs */
struct brain_immune_system;
typedef struct brain_immune_system brain_immune_system_t;
extern int brain_immune_validate_operation(brain_immune_system_t* immune,
                                            const char* operation,
                                            uint32_t severity);
extern int brain_immune_present_antigen(brain_immune_system_t* immune,
                                         int source,
                                         const uint8_t* epitope,
                                         size_t epitope_len,
                                         uint32_t severity,
                                         uint32_t source_node,
                                         uint32_t* antigen_id);

struct bbb_system_struct;
typedef struct bbb_system_struct* bbb_system_t;
extern int bbb_validate_data(bbb_system_t bbb, const void* data,
                              size_t size, const char* context);

/* Stub declarations for subsystem integration globals */
static void* g_fin_predictive_bridge_immune = NULL;
static void* g_fin_predictive_bridge_bbb = NULL;

#include "utils/bridge/nimcp_bridge_boilerplate.h"
BRIDGE_DEFINE_MESH_REGISTRATION(fin_predictive, MESH_ADAPTER_CATEGORY_COGNITIVE)


void financial_predictive_bridge_set_global_bbb(bbb_system_t bbb) {
    g_fin_predictive_bridge_bbb = bbb;
}

//=============================================================================
// KG Wiring Integration (Change Set 1)
//=============================================================================
struct kg_wiring;
typedef struct kg_wiring kg_wiring_t;

/* KG message type defines */
#define KG_MSG_FIN_PREDICTIVE_REQUEST   "FIN_PREDICTIVE_REQUEST"
#define KG_MSG_FIN_PREDICTIVE_RESPONSE  "FIN_PREDICTIVE_RESPONSE"
#define KG_MSG_FIN_PREDICTIVE_ERROR     "FIN_PREDICTIVE_ERROR"
#define KG_MSG_FIN_PREDICTIVE_UPDATE    "FIN_PREDICTIVE_UPDATE"

//=============================================================================
// Thread-local Error
//=============================================================================

static _Thread_local char fin_predictive_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(fin_predictive_last_error, sizeof(fin_predictive_last_error), fmt, args);
    va_end(args);
}

//=============================================================================
// Heartbeat Helpers
//=============================================================================

static inline void fin_predictive_heartbeat(const char* operation, float progress) {
    if (g_fin_predictive_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_fin_predictive_health_agent, operation, progress);
    }
}

static inline void fin_predictive_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_fin_predictive_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_fin_predictive_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_fin_predictive_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

//=============================================================================
// Internal Structure
//=============================================================================

struct financial_predictive_bridge {
    fin_predictive_config_t config;
    fin_predictive_state_t state;
    fin_predictive_bridge_stats_t stats;

    /* Modulation state */
    float inflammation;
    float fatigue;

    /* Internal belief state */
    float* belief_mean;             /* Mean beliefs [num_assets * horizon] */
    float* belief_precision;        /* Precision of beliefs */
    float* prior_mean;              /* Prior expectations */
    float* prior_precision;         /* Prior precision */
    uint32_t belief_dim;

    /* Model parameters */
    float drift_rate;               /* Expected drift */
    float volatility_scale;         /* Volatility scaling */
    float mean_reversion_rate;      /* Mean reversion speed */

    /* Subsystem pointers */
    brain_immune_system_t* immune;
    bbb_system_t bbb;
    kg_wiring_t* kg_wiring;
    nimcp_health_agent_t* health_agent;
    void* logger;

    /* Security validation flags */
    bool enable_bbb_validation;
    bool enable_immune_validation;

    /* RNG state */
    uint64_t rng_state;

    /* Operational state */
    fin_predictive_op_state_t operational_state;
};

//=============================================================================
// RNG Utilities
//=============================================================================

static float predictive_randf(uint64_t* state) {
    *state = *state * 6364136223846793005ULL + 1442695040888963407ULL;
    return (float)((*state >> 33) & 0x7FFFFFFF) / (float)0x7FFFFFFF;
}

static float predictive_randn(uint64_t* state) {
    float u1 = predictive_randf(state) + 1e-10f;
    float u2 = predictive_randf(state);
    return sqrtf(-2.0f * logf(u1)) * cosf(NIMCP_TWO_PI_F * u2);
}

static float s_shaped(float low, float high, float x) {
    if (x <= low) return 0.0f;
    if (x >= high) return 1.0f;
    /* Guard against division by zero when low == high */
    float range = high - low;
    if (range < 1e-8f) return 0.5f;
    float t = (x - low) / range;
    return t * t * (3.0f - 2.0f * t);
}

static float softmax_temperature(float* values, float* probs, uint32_t n, float temperature) {
    if (n == 0 || temperature < 1e-10f) return 0.0f;

    /* Find max for numerical stability */
    float max_val = values[0];
    for (uint32_t i = 1; i < n; i++) {
        if (values[i] > max_val) max_val = values[i];
    }

    /* Compute exp and sum */
    float sum = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        float v = (values[i] - max_val) / temperature;
        probs[i] = expf(v);
        sum += probs[i];
    }

    /* Normalize */
    if (sum > 1e-10f) {
        for (uint32_t i = 0; i < n; i++) {
            probs[i] /= sum;
        }
    }

    return sum;
}

//=============================================================================
// Global Validation Helper
//=============================================================================

static int fin_predictive_validate_global(const char* operation) {
    if (g_fin_predictive_bridge_immune) {
        int rc = brain_immune_validate_operation(g_fin_predictive_bridge_immune, operation, 5);
        if (rc != 0) {
            set_error("fin_predictive: immune validation failed for %s", operation);
            return FIN_PREDICTIVE_ERR_SUBSYSTEM;
        }
    }
    if (g_fin_predictive_bridge_bbb) {
        int rc = bbb_validate_data(g_fin_predictive_bridge_bbb, NULL, 0, operation);
        if (rc != 0) {
            set_error("fin_predictive: BBB validation failed for %s", operation);
            return FIN_PREDICTIVE_ERR_SUBSYSTEM;
        }
    }
    return FIN_PREDICTIVE_ERR_OK;
}

//=============================================================================
// Instance-Level Validation Helper
//=============================================================================

static int fin_predictive_validate_instance(financial_predictive_bridge_t* bridge,
                                             const char* operation) {
    if (!bridge) return FIN_PREDICTIVE_ERR_NULL;

    if (bridge->enable_bbb_validation && bridge->bbb) {
        int rc = bbb_validate_data(bridge->bbb, NULL, 0, operation);
        if (rc != 0) {
            set_error("BBB validation failed for %s", operation);
            bridge->stats.bbb_validations++;
            NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_BBB_VALIDATION,
                "fin_predictive: BBB validation failed for %s", operation);
            return FIN_PREDICTIVE_ERR_VALIDATION;
        }
        bridge->stats.bbb_validations++;
    }

    if (bridge->enable_immune_validation && bridge->immune) {
        int rc = brain_immune_validate_operation(bridge->immune, operation, 5);
        if (rc != 0) {
            set_error("Immune validation failed for %s", operation);
            bridge->stats.immune_checks++;
            NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_BBB_VALIDATION,
                "fin_predictive: immune validation failed for %s", operation);
            return FIN_PREDICTIVE_ERR_VALIDATION;
        }
        bridge->stats.immune_checks++;
    }

    return FIN_PREDICTIVE_ERR_OK;
}

//=============================================================================
// Antigen Presentation Helper
//=============================================================================

static void fin_predictive_present_antigen(financial_predictive_bridge_t* bridge,
                                            const char* anomaly, uint32_t severity) {
    if (bridge && bridge->immune) {
        uint8_t sig[64] = {0};
        snprintf((char*)sig, sizeof(sig), "fin_predictive:%s", anomaly);
        uint32_t antigen_id = 0;
        brain_immune_present_antigen(bridge->immune, 0, sig, strlen((char*)sig),
                                      severity, 0, &antigen_id);
    }
}

//=============================================================================
// KG Publish Helper
//=============================================================================

static int fin_predictive_kg_publish(financial_predictive_bridge_t* bridge,
                                      const char* msg_type,
                                      const void* payload, size_t size) {
    if (bridge && bridge->kg_wiring) {
        /* kg_wiring_publish would be called here */
        (void)msg_type; (void)payload; (void)size;
        bridge->stats.kg_messages_sent++;
        return 0;
    }
    return 0;
}

//=============================================================================
// Lifecycle API
//=============================================================================

fin_predictive_config_t financial_predictive_bridge_default_config(void) {
    fin_predictive_heartbeat("default_config", 0.0f);

    fin_predictive_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    /* Model settings */
    cfg.model_type = FIN_MODEL_MEAN_REVERSION;
    cfg.num_assets = 10;
    cfg.prediction_horizon = 10;

    /* Precision settings */
    cfg.initial_precision = 1.0f;
    cfg.min_precision = 0.01f;
    cfg.max_precision = 100.0f;
    cfg.precision_learning_rate = NIMCP_LEARNING_RATE_COARSE;

    /* Belief update settings */
    cfg.belief_learning_rate = NIMCP_LEARNING_RATE_COARSE;
    cfg.prediction_error_gain = 1.0f;

    /* Active inference settings */
    cfg.efe_temperature = NIMCP_TEMPERATURE_DEFAULT;
    cfg.exploration_weight = 0.3f;
    cfg.complexity_weight = 0.1f;
    cfg.efe_num_samples = 100;

    /* Modulation sensitivity */
    cfg.inflammation_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    cfg.fatigue_sensitivity = NIMCP_SENSITIVITY_DEFAULT;

    /* Security */
    cfg.enable_bbb_validation = false;
    cfg.enable_immune_validation = false;

    fin_predictive_heartbeat("default_config", 1.0f);
    return cfg;
}

financial_predictive_bridge_t* financial_predictive_bridge_create(
    const fin_predictive_config_t* config)
{
    fin_predictive_heartbeat("create", 0.0f);

    financial_predictive_bridge_t* bridge =
        (financial_predictive_bridge_t*)nimcp_malloc(sizeof(financial_predictive_bridge_t));
    if (!bridge) {
        set_error("Failed to allocate financial_predictive_bridge_t");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate financial_predictive_bridge_t");
        return NULL;
    }
    memset(bridge, 0, sizeof(*bridge));

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = financial_predictive_bridge_default_config();
    }

    /* Validate config */
    if (bridge->config.num_assets == 0) {
        bridge->config.num_assets = 10;
    }
    if (bridge->config.num_assets > FIN_PREDICTIVE_MAX_ASSETS) {
        bridge->config.num_assets = FIN_PREDICTIVE_MAX_ASSETS;
    }
    if (bridge->config.prediction_horizon == 0) {
        bridge->config.prediction_horizon = 10;
    }
    if (bridge->config.prediction_horizon > FIN_PREDICTIVE_MAX_HORIZON) {
        bridge->config.prediction_horizon = FIN_PREDICTIVE_MAX_HORIZON;
    }

    /* Allocate internal belief state */
    uint32_t belief_size = bridge->config.num_assets * bridge->config.prediction_horizon;
    bridge->belief_dim = belief_size;

    bridge->belief_mean = (float*)nimcp_calloc(belief_size, sizeof(float));
    if (!bridge->belief_mean) return NULL;
    bridge->belief_precision = (float*)nimcp_calloc(belief_size, sizeof(float));
    if (!bridge->belief_precision) return NULL;
    bridge->prior_mean = (float*)nimcp_calloc(belief_size, sizeof(float));
    if (!bridge->prior_mean) return NULL;
    bridge->prior_precision = (float*)nimcp_calloc(belief_size, sizeof(float));
    if (!bridge->prior_precision) return NULL;

    if (!bridge->belief_mean || !bridge->belief_precision ||
        !bridge->prior_mean || !bridge->prior_precision) {
        set_error("Failed to allocate belief state arrays");
        financial_predictive_bridge_destroy(bridge);
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate belief state arrays");
        return NULL;
    }

    /* Initialize precisions */
    for (uint32_t i = 0; i < belief_size; i++) {
        bridge->belief_precision[i] = bridge->config.initial_precision;
        bridge->prior_precision[i] = bridge->config.initial_precision;
    }

    /* Initialize model parameters */
    bridge->drift_rate = 0.0001f;       /* Small positive drift */
    bridge->volatility_scale = 0.02f;   /* 2% daily vol typical */
    bridge->mean_reversion_rate = 0.1f; /* Mean reversion speed */

    /* Initialize state */
    bridge->operational_state = FIN_PREDICTIVE_STATE_IDLE;
    bridge->inflammation = 0.0f;
    bridge->fatigue = 0.0f;

    /* Copy security flags */
    bridge->enable_bbb_validation = bridge->config.enable_bbb_validation;
    bridge->enable_immune_validation = bridge->config.enable_immune_validation;

    /* Initialize RNG */
    bridge->rng_state = 42;

    fin_predictive_heartbeat("create", 1.0f);
    return bridge;
}

void financial_predictive_bridge_destroy(financial_predictive_bridge_t* bridge) {
    if (!bridge) return;
    fin_predictive_heartbeat("destroy", 0.0f);

    if (bridge->belief_mean) nimcp_free(bridge->belief_mean);
    if (bridge->belief_precision) nimcp_free(bridge->belief_precision);
    if (bridge->prior_mean) nimcp_free(bridge->prior_mean);
    if (bridge->prior_precision) nimcp_free(bridge->prior_precision);

    nimcp_free(bridge);
    bridge = NULL;
    fin_predictive_heartbeat("destroy", 1.0f);
}

fin_predictive_op_state_t financial_predictive_bridge_get_state(
    const financial_predictive_bridge_t* bridge)
{
    if (!bridge) return FIN_PREDICTIVE_STATE_UNINITIALIZED;
    return bridge->operational_state;
}

int financial_predictive_bridge_reset(financial_predictive_bridge_t* bridge) {
    if (!bridge) {
        set_error("NULL bridge in reset");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_predictive_bridge_reset: bridge is NULL");
        return FIN_PREDICTIVE_ERR_NULL;
    }

    fin_predictive_heartbeat_instance(bridge->health_agent, "reset", 0.0f);

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    /* Reset modulation */
    bridge->inflammation = 0.0f;
    bridge->fatigue = 0.0f;

    /* Reset beliefs to priors */
    uint32_t belief_size = bridge->belief_dim;
    for (uint32_t i = 0; i < belief_size; i++) {
        bridge->belief_mean[i] = bridge->prior_mean[i];
        bridge->belief_precision[i] = bridge->config.initial_precision;
    }

    bridge->operational_state = FIN_PREDICTIVE_STATE_IDLE;

    fin_predictive_heartbeat_instance(bridge->health_agent, "reset", 1.0f);
    return FIN_PREDICTIVE_ERR_OK;
}

//=============================================================================
// Subsystem Setters
//=============================================================================

int financial_predictive_bridge_set_immune(financial_predictive_bridge_t* bridge,
                                            void* immune) {
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_predictive_bridge_set_immune: bridge is NULL");
        return FIN_PREDICTIVE_ERR_NULL;
    }
    bridge->immune = (brain_immune_system_t*)immune;
    return FIN_PREDICTIVE_ERR_OK;
}

int financial_predictive_bridge_set_bbb(financial_predictive_bridge_t* bridge,
                                         void* bbb) {
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_predictive_bridge_set_bbb: bridge is NULL");
        return FIN_PREDICTIVE_ERR_NULL;
    }
    bridge->bbb = (bbb_system_t)bbb;
    return FIN_PREDICTIVE_ERR_OK;
}

int financial_predictive_bridge_enable_bbb_validation(
    financial_predictive_bridge_t* bridge, bool enable) {
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_predictive_bridge_enable_bbb_validation: bridge is NULL");
        return FIN_PREDICTIVE_ERR_NULL;
    }
    bridge->enable_bbb_validation = enable;
    return FIN_PREDICTIVE_ERR_OK;
}

int financial_predictive_bridge_enable_immune_validation(
    financial_predictive_bridge_t* bridge, bool enable) {
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_predictive_bridge_enable_immune_validation: bridge is NULL");
        return FIN_PREDICTIVE_ERR_NULL;
    }
    bridge->enable_immune_validation = enable;
    return FIN_PREDICTIVE_ERR_OK;
}

int financial_predictive_bridge_set_kg_wiring(financial_predictive_bridge_t* bridge,
                                               void* kg) {
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_predictive_bridge_set_kg_wiring: bridge is NULL");
        return FIN_PREDICTIVE_ERR_NULL;
    }
    bridge->kg_wiring = (kg_wiring_t*)kg;
    return FIN_PREDICTIVE_ERR_OK;
}

int financial_predictive_bridge_set_health_agent(financial_predictive_bridge_t* bridge,
                                                  void* health_agent) {
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_predictive_bridge_set_health_agent: bridge is NULL");
        return FIN_PREDICTIVE_ERR_NULL;
    }
    bridge->health_agent = (nimcp_health_agent_t*)health_agent;
    return FIN_PREDICTIVE_ERR_OK;
}

int financial_predictive_bridge_set_logger(financial_predictive_bridge_t* bridge,
                                            void* logger) {
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_predictive_bridge_set_logger: bridge is NULL");
        return FIN_PREDICTIVE_ERR_NULL;
    }
    bridge->logger = logger;
    return FIN_PREDICTIVE_ERR_OK;
}

//=============================================================================
// Core Predictive Coding: Predict
//=============================================================================

int financial_predictive_bridge_predict(
    financial_predictive_bridge_t* bridge,
    const fin_market_observation_t* observation,
    fin_predictive_state_t* state)
{
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_predictive_bridge_predict: bridge is NULL");
        return FIN_PREDICTIVE_ERR_NULL;
    }
    if (!state) {
        set_error("NULL state");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_predictive_bridge_predict: state is NULL");
        return FIN_PREDICTIVE_ERR_NULL;
    }

    /* Validate subsystems */
    int val_rc = fin_predictive_validate_instance(bridge, "predict");
    if (val_rc != FIN_PREDICTIVE_ERR_OK) return val_rc;

    fin_predictive_heartbeat_instance(bridge->health_agent, "predict", 0.0f);
    bridge->stats.health_heartbeats++;

    bridge->operational_state = FIN_PREDICTIVE_STATE_PREDICTING;

    uint32_t num_assets = state->num_assets;
    uint32_t horizon = state->horizon;

    if (num_assets > bridge->config.num_assets) {
        num_assets = bridge->config.num_assets;
    }
    if (horizon > bridge->config.prediction_horizon) {
        horizon = bridge->config.prediction_horizon;
    }

    /* Health modulation: reduce precision under stress */
    float health_mod = 1.0f
        - bridge->inflammation * bridge->config.inflammation_sensitivity * 0.3f
        - bridge->fatigue * bridge->config.fatigue_sensitivity * 0.2f;
    if (health_mod < 0.1f) health_mod = 0.1f;

    /* Generate predictions based on model type */
    switch (bridge->config.model_type) {
        case FIN_MODEL_RANDOM_WALK: {
            /* Random walk with drift: E[P_t+h] = P_t * exp(mu * h) */
            for (uint32_t a = 0; a < num_assets; a++) {
                float current_price = 1.0f;
                if (observation && observation->prices && a < observation->num_assets) {
                    current_price = observation->prices[a];
                }

                for (uint32_t h = 0; h < horizon; h++) {
                    float idx = (float)(a * horizon + h);
                    float drift = bridge->drift_rate * (float)(h + 1);
                    float predicted = current_price * expf(drift);

                    /* Add noise scaled by horizon */
                    float noise = predictive_randn(&bridge->rng_state) *
                                  bridge->volatility_scale * sqrtf((float)(h + 1));
                    predicted *= expf(noise * 0.5f);

                    state->predictions[a * horizon + h] = predicted;

                    /* Precision decreases with horizon */
                    float base_precision = bridge->config.initial_precision;
                    float horizon_decay = 1.0f / (1.0f + 0.1f * (float)(h + 1));
                    state->precisions[a * horizon + h] = base_precision * horizon_decay * health_mod;

                    /* Update internal beliefs */
                    if (a * horizon + h < bridge->belief_dim) {
                        bridge->belief_mean[a * horizon + h] = predicted;
                        bridge->belief_precision[a * horizon + h] =
                            state->precisions[a * horizon + h];
                    }
                }
            }
            break;
        }

        case FIN_MODEL_MEAN_REVERSION: {
            /* Ornstein-Uhlenbeck: dX = theta(mu - X)dt + sigma*dW */
            float theta = bridge->mean_reversion_rate;

            for (uint32_t a = 0; a < num_assets; a++) {
                float current_price = 1.0f;
                float long_term_mean = 1.0f;

                if (observation && observation->prices && a < observation->num_assets) {
                    current_price = observation->prices[a];
                    /* Estimate long-term mean from recent returns */
                    if (observation->returns && fabsf(observation->returns[a]) > 0.001f) {
                        long_term_mean = current_price * (1.0f + observation->returns[a] * 10.0f);
                    } else {
                        long_term_mean = current_price;
                    }
                }

                float price = current_price;
                for (uint32_t h = 0; h < horizon; h++) {
                    /* Mean reversion toward long-term mean */
                    float dt = NIMCP_SIMULATION_DT_MS;
                    float reversion = theta * (long_term_mean - price) * dt;
                    float diffusion = bridge->volatility_scale *
                                      predictive_randn(&bridge->rng_state) * sqrtf(dt);
                    price = price + reversion + diffusion;
                    if (price < 0.01f) price = 0.01f;

                    state->predictions[a * horizon + h] = price;

                    /* Precision: higher for mean-reverting model */
                    float base_precision = bridge->config.initial_precision * 1.5f;
                    float horizon_decay = 1.0f / (1.0f + 0.05f * (float)(h + 1));
                    state->precisions[a * horizon + h] = base_precision * horizon_decay * health_mod;

                    if (a * horizon + h < bridge->belief_dim) {
                        bridge->belief_mean[a * horizon + h] = price;
                        bridge->belief_precision[a * horizon + h] =
                            state->precisions[a * horizon + h];
                    }
                }
            }
            break;
        }

        case FIN_MODEL_MOMENTUM: {
            /* Trend-following: prediction follows recent trend */
            for (uint32_t a = 0; a < num_assets; a++) {
                float current_price = 1.0f;
                float trend = 0.0f;

                if (observation && observation->prices && a < observation->num_assets) {
                    current_price = observation->prices[a];
                    if (observation->returns && a < observation->num_assets) {
                        trend = observation->returns[a];
                    }
                }

                for (uint32_t h = 0; h < horizon; h++) {
                    /* Project trend forward with decay */
                    float trend_decay = expf(-0.1f * (float)(h + 1));
                    float predicted = current_price * expf(trend * trend_decay * (float)(h + 1));

                    state->predictions[a * horizon + h] = predicted;

                    /* Precision lower for momentum (higher uncertainty) */
                    float base_precision = bridge->config.initial_precision * 0.8f;
                    float horizon_decay = 1.0f / (1.0f + 0.15f * (float)(h + 1));
                    state->precisions[a * horizon + h] = base_precision * horizon_decay * health_mod;

                    if (a * horizon + h < bridge->belief_dim) {
                        bridge->belief_mean[a * horizon + h] = predicted;
                        bridge->belief_precision[a * horizon + h] =
                            state->precisions[a * horizon + h];
                    }
                }
            }
            break;
        }

        default:
            /* Fallback: simple linear extrapolation */
            for (uint32_t a = 0; a < num_assets; a++) {
                float current_price = 1.0f;
                if (observation && observation->prices && a < observation->num_assets) {
                    current_price = observation->prices[a];
                }

                for (uint32_t h = 0; h < horizon; h++) {
                    state->predictions[a * horizon + h] = current_price;
                    state->precisions[a * horizon + h] =
                        bridge->config.initial_precision * health_mod /
                        (1.0f + 0.1f * (float)(h + 1));
                }
            }
            break;
    }

    /* Clamp precisions to valid range */
    for (uint32_t i = 0; i < num_assets * horizon; i++) {
        state->precisions[i] = nimcp_clampf(state->precisions[i],
                                       bridge->config.min_precision,
                                       bridge->config.max_precision);
    }

    bridge->stats.predictions_made++;
    bridge->operational_state = FIN_PREDICTIVE_STATE_IDLE;

    fin_predictive_heartbeat_instance(bridge->health_agent, "predict", 1.0f);
    bridge->stats.health_heartbeats++;

    return FIN_PREDICTIVE_ERR_OK;
}

//=============================================================================
// Core Predictive Coding: Update
//=============================================================================

int financial_predictive_bridge_update(
    financial_predictive_bridge_t* bridge,
    const fin_market_observation_t* observation,
    fin_predictive_state_t* state)
{
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_predictive_bridge_update: bridge is NULL");
        return FIN_PREDICTIVE_ERR_NULL;
    }
    if (!observation) {
        set_error("NULL observation");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_predictive_bridge_update: observation is NULL");
        return FIN_PREDICTIVE_ERR_NULL;
    }
    if (!state) {
        set_error("NULL state");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_predictive_bridge_update: state is NULL");
        return FIN_PREDICTIVE_ERR_NULL;
    }

    /* Validate subsystems */
    int val_rc = fin_predictive_validate_instance(bridge, "update");
    if (val_rc != FIN_PREDICTIVE_ERR_OK) return val_rc;

    fin_predictive_heartbeat_instance(bridge->health_agent, "update", 0.0f);
    bridge->stats.health_heartbeats++;

    bridge->operational_state = FIN_PREDICTIVE_STATE_UPDATING;

    uint32_t num_assets = state->num_assets;
    if (num_assets > observation->num_assets) {
        num_assets = observation->num_assets;
    }
    if (num_assets > bridge->config.num_assets) {
        num_assets = bridge->config.num_assets;
    }

    float lr = bridge->config.belief_learning_rate;
    float pe_gain = bridge->config.prediction_error_gain;

    /* Health modulation */
    float health_mod = 1.0f
        - bridge->inflammation * bridge->config.inflammation_sensitivity * 0.2f
        - bridge->fatigue * bridge->config.fatigue_sensitivity * 0.15f;
    if (health_mod < 0.1f) health_mod = 0.1f;

    /* Compute prediction errors and update beliefs */
    float total_squared_error = 0.0f;
    float max_error = 0.0f;

    for (uint32_t a = 0; a < num_assets; a++) {
        /* Get actual observation (price at t=0) */
        float actual = observation->prices[a];

        /* Prediction error = actual - predicted (for h=0, the most recent prediction) */
        float predicted = state->predictions[a * state->horizon];
        float error = actual - predicted;
        float precision = state->precisions[a * state->horizon];

        /* Store prediction error */
        state->prediction_errors[a * state->horizon] = error;

        /* Track error statistics */
        total_squared_error += error * error;
        if (fabsf(error) > max_error) {
            max_error = fabsf(error);
        }

        /* Precision-weighted prediction error update (Bayesian belief update) */
        /* new_mean = prior_mean + (precision * gain * learning_rate) * error */
        float weighted_error = precision * pe_gain * lr * health_mod * error;

        /* Update internal beliefs for all horizons (shift forward) */
        for (uint32_t h = 0; h < state->horizon; h++) {
            uint32_t idx = a * state->horizon + h;
            if (idx < bridge->belief_dim) {
                bridge->belief_mean[idx] += weighted_error;

                /* Adapt precision based on prediction error magnitude */
                /* If error is small, increase precision; if large, decrease */
                float error_magnitude = fabsf(error / (actual + 1e-10f));
                float precision_update = bridge->config.precision_learning_rate *
                    (1.0f / (error_magnitude + 0.1f) - bridge->belief_precision[idx]);
                bridge->belief_precision[idx] += precision_update * health_mod;
                bridge->belief_precision[idx] = nimcp_clampf(bridge->belief_precision[idx],
                    bridge->config.min_precision, bridge->config.max_precision);
            }
        }
    }

    /* Check for anomalously large prediction errors */
    float rms_error = sqrtf(total_squared_error / (float)(num_assets > 0 ? num_assets : 1));
    if (rms_error > 0.1f) {  /* 10% RMS error threshold */
        fin_predictive_present_antigen(bridge, "high_prediction_error", 3);
    }
    if (max_error > 0.2f) {  /* 20% max error threshold */
        fin_predictive_present_antigen(bridge, "extreme_prediction_error", 4);
    }

    bridge->stats.updates++;
    bridge->operational_state = FIN_PREDICTIVE_STATE_IDLE;

    fin_predictive_heartbeat_instance(bridge->health_agent, "update", 1.0f);
    bridge->stats.health_heartbeats++;

    return FIN_PREDICTIVE_ERR_OK;
}

//=============================================================================
// Expected Free Energy Computation
//=============================================================================

int financial_predictive_bridge_expected_free_energy(
    financial_predictive_bridge_t* bridge,
    fin_action_type_t action,
    const fin_predictive_state_t* state,
    const fin_preferred_outcome_t* preferred,
    fin_efe_result_t* result)
{
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_predictive_bridge_expected_free_energy: bridge is NULL");
        return FIN_PREDICTIVE_ERR_NULL;
    }
    if (!state) {
        set_error("NULL state");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_predictive_bridge_expected_free_energy: state is NULL");
        return FIN_PREDICTIVE_ERR_NULL;
    }
    if (!result) {
        set_error("NULL result");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_predictive_bridge_expected_free_energy: result is NULL");
        return FIN_PREDICTIVE_ERR_NULL;
    }

    /* Validate subsystems */
    int val_rc = fin_predictive_validate_instance(bridge, "expected_free_energy");
    if (val_rc != FIN_PREDICTIVE_ERR_OK) return val_rc;

    fin_predictive_heartbeat_instance(bridge->health_agent, "efe", 0.0f);
    bridge->stats.health_heartbeats++;

    bridge->operational_state = FIN_PREDICTIVE_STATE_COMPUTING_EFE;

    memset(result, 0, sizeof(*result));
    result->action = action;

    uint32_t num_assets = state->num_assets;
    if (num_assets > bridge->config.num_assets) {
        num_assets = bridge->config.num_assets;
    }

    /* Health modulation affects precision weighting */
    float health_mod = 1.0f
        - bridge->inflammation * bridge->config.inflammation_sensitivity * 0.15f
        - bridge->fatigue * bridge->config.fatigue_sensitivity * 0.1f;
    if (health_mod < 0.1f) health_mod = 0.1f;

    /*
     * Expected Free Energy G(π) = Epistemic_Value + Pragmatic_Value
     *
     * Epistemic Value: Information gain from action
     *   - Higher for actions that reduce uncertainty (increase precision)
     *   - Computed as expected KL divergence
     *
     * Pragmatic Value: Expected deviation from preferred outcomes
     *   - Lower (more negative) for actions achieving goals
     *   - Computed as expected squared deviation from preferences
     *
     * Total EFE: G(π) = - epistemic_value + pragmatic_value + complexity
     * (Minimizing G means maximizing epistemic and minimizing deviation from prefs)
     */

    float epistemic_value = 0.0f;
    float pragmatic_value = 0.0f;
    float complexity_cost = 0.0f;

    /* Monte Carlo estimation of EFE */
    uint32_t num_samples = bridge->config.efe_num_samples;
    if (num_samples == 0) num_samples = 100;

    for (uint32_t s = 0; s < num_samples; s++) {
        /* Sample future state under this action */
        float sample_return = 0.0f;
        float sample_uncertainty = 0.0f;

        for (uint32_t a = 0; a < num_assets; a++) {
            /* Get prediction and precision for this asset */
            float predicted = state->predictions[a * state->horizon];
            float precision = state->precisions[a * state->horizon];

            /* Sample from belief distribution */
            float std = 1.0f / sqrtf(precision + 1e-10f);
            float sample = predicted + predictive_randn(&bridge->rng_state) * std;

            /* Compute return under sampled state */
            float ret = (sample - predicted) / (predicted + 1e-10f);

            /* Action-specific effect on return */
            switch (action) {
                case FIN_ACTION_BUY:
                    sample_return += ret * 1.0f;  /* Full exposure */
                    break;
                case FIN_ACTION_SELL:
                    sample_return += ret * -1.0f; /* Short exposure */
                    break;
                case FIN_ACTION_HOLD:
                    sample_return += ret * 0.5f;  /* Reduced exposure */
                    break;
                case FIN_ACTION_HEDGE:
                    sample_return += ret * 0.1f;  /* Heavily hedged */
                    sample_uncertainty -= 0.5f;   /* Hedging reduces uncertainty */
                    break;
                case FIN_ACTION_OBSERVE:
                    sample_return += 0.0f;        /* No position */
                    sample_uncertainty -= 1.0f;   /* Pure information gain */
                    break;
                default:
                    sample_return += ret * 0.25f;
                    break;
            }

            /* Accumulate uncertainty */
            sample_uncertainty += std;
        }

        sample_return /= (float)(num_assets > 0 ? num_assets : 1);
        sample_uncertainty /= (float)(num_assets > 0 ? num_assets : 1);

        /* Epistemic value: negative uncertainty (we want to reduce it) */
        epistemic_value -= sample_uncertainty;

        /* Pragmatic value: deviation from preferred outcomes */
        if (preferred) {
            float target_return = preferred->target_return;
            float deviation = (sample_return - target_return) * (sample_return - target_return);
            pragmatic_value += deviation;
        }
    }

    /* Normalize by number of samples */
    epistemic_value /= (float)num_samples;
    pragmatic_value /= (float)num_samples;

    /* Apply exploration weight */
    epistemic_value *= bridge->config.exploration_weight;
    pragmatic_value *= (1.0f - bridge->config.exploration_weight);

    /* Complexity cost: penalize complex actions */
    switch (action) {
        case FIN_ACTION_HOLD:
            complexity_cost = 0.0f;
            break;
        case FIN_ACTION_BUY:
        case FIN_ACTION_SELL:
            complexity_cost = 0.1f;
            break;
        case FIN_ACTION_HEDGE:
            complexity_cost = 0.2f;
            break;
        case FIN_ACTION_REBALANCE:
            complexity_cost = 0.3f;
            break;
        case FIN_ACTION_OBSERVE:
            complexity_cost = 0.05f;
            break;
        default:
            complexity_cost = 0.15f;
            break;
    }
    complexity_cost *= bridge->config.complexity_weight;

    /* Total EFE: minimize means maximize epistemic and minimize pragmatic */
    float total_efe = -epistemic_value + pragmatic_value + complexity_cost;

    /* Apply health modulation */
    result->epistemic_value = epistemic_value * health_mod;
    result->pragmatic_value = pragmatic_value;
    result->complexity_cost = complexity_cost;
    result->total_efe = total_efe;
    result->confidence = health_mod;

    bridge->stats.efe_computations++;
    bridge->operational_state = FIN_PREDICTIVE_STATE_IDLE;

    fin_predictive_heartbeat_instance(bridge->health_agent, "efe", 1.0f);
    bridge->stats.health_heartbeats++;

    return FIN_PREDICTIVE_ERR_OK;
}

//=============================================================================
// Active Inference
//=============================================================================

int financial_predictive_bridge_active_inference(
    financial_predictive_bridge_t* bridge,
    const fin_predictive_state_t* state,
    const fin_preferred_outcome_t* preferred,
    fin_active_inference_result_t* result)
{
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_predictive_bridge_active_inference: bridge is NULL");
        return FIN_PREDICTIVE_ERR_NULL;
    }
    if (!state) {
        set_error("NULL state");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_predictive_bridge_active_inference: state is NULL");
        return FIN_PREDICTIVE_ERR_NULL;
    }
    if (!result) {
        set_error("NULL result");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_predictive_bridge_active_inference: result is NULL");
        return FIN_PREDICTIVE_ERR_NULL;
    }

    /* Validate subsystems */
    int val_rc = fin_predictive_validate_instance(bridge, "active_inference");
    if (val_rc != FIN_PREDICTIVE_ERR_OK) return val_rc;

    fin_predictive_heartbeat_instance(bridge->health_agent, "active_inference", 0.0f);
    bridge->stats.health_heartbeats++;

    bridge->operational_state = FIN_PREDICTIVE_STATE_ACTIVE_INFERENCE;

    /* Evaluate EFE for all actions */
    uint32_t num_actions = FIN_ACTION_COUNT;
    fin_efe_result_t efe_results[FIN_ACTION_COUNT];
    float neg_efe[FIN_ACTION_COUNT];
    float probs[FIN_ACTION_COUNT];

    for (uint32_t a = 0; a < num_actions; a++) {
        int rc = financial_predictive_bridge_expected_free_energy(
            bridge, (fin_action_type_t)a, state, preferred, &efe_results[a]);
        if (rc != FIN_PREDICTIVE_ERR_OK) {
            bridge->operational_state = FIN_PREDICTIVE_STATE_ERROR;
            return rc;
        }
        /* Negative EFE (we want to maximize negative EFE, i.e., minimize EFE) */
        neg_efe[a] = -efe_results[a].total_efe;
    }

    /* Softmax to get action probabilities */
    float temperature = bridge->config.efe_temperature;
    if (temperature < 0.01f) temperature = 0.01f;
    softmax_temperature(neg_efe, probs, num_actions, temperature);

    /* Store probabilities in EFE results */
    for (uint32_t a = 0; a < num_actions; a++) {
        efe_results[a].probability = probs[a];
    }

    /* Select action with highest probability (greedy) or sample */
    fin_action_type_t selected_action = FIN_ACTION_HOLD;
    float max_prob = probs[0];
    for (uint32_t a = 1; a < num_actions; a++) {
        if (probs[a] > max_prob) {
            max_prob = probs[a];
            selected_action = (fin_action_type_t)a;
        }
    }

    /* Fill result */
    result->selected_action = selected_action;
    result->selected_action_efe = efe_results[selected_action].total_efe;
    result->exploration_bonus = efe_results[selected_action].epistemic_value;
    result->num_actions = num_actions;

    /* Compute expected return and risk under selected action */
    float expected_return = 0.0f;
    float expected_risk = 0.0f;

    for (uint32_t a = 0; a < state->num_assets; a++) {
        float predicted = state->predictions[a * state->horizon];
        float precision = state->precisions[a * state->horizon];
        float std = 1.0f / sqrtf(precision + 1e-10f);

        /* Action-specific position effect */
        float position = 0.0f;
        switch (selected_action) {
            case FIN_ACTION_BUY: position = 1.0f; break;
            case FIN_ACTION_SELL: position = -1.0f; break;
            case FIN_ACTION_HOLD: position = 0.5f; break;
            case FIN_ACTION_HEDGE: position = 0.1f; break;
            default: position = 0.25f; break;
        }

        expected_return += position * (predicted - 1.0f);  /* Assume reference price of 1 */
        expected_risk += position * position * std * std;
    }

    result->expected_return = expected_return / (float)(state->num_assets > 0 ? state->num_assets : 1);
    result->expected_risk = sqrtf(expected_risk / (float)(state->num_assets > 0 ? state->num_assets : 1));

    /* Copy action weights if allocated */
    if (result->action_weights && result->num_weights >= state->num_assets) {
        for (uint32_t a = 0; a < state->num_assets; a++) {
            float position = 0.0f;
            switch (selected_action) {
                case FIN_ACTION_BUY: position = 1.0f / (float)state->num_assets; break;
                case FIN_ACTION_SELL: position = -1.0f / (float)state->num_assets; break;
                case FIN_ACTION_HOLD: position = 0.5f / (float)state->num_assets; break;
                case FIN_ACTION_HEDGE: position = 0.1f / (float)state->num_assets; break;
                default: position = 0.25f / (float)state->num_assets; break;
            }
            result->action_weights[a] = position;
        }
    }

    /* Copy all action EFEs if allocated */
    if (result->all_actions) {
        for (uint32_t a = 0; a < num_actions; a++) {
            result->all_actions[a] = efe_results[a];
        }
    }

    bridge->stats.active_inferences++;
    bridge->operational_state = FIN_PREDICTIVE_STATE_IDLE;

    fin_predictive_heartbeat_instance(bridge->health_agent, "active_inference", 1.0f);
    bridge->stats.health_heartbeats++;

    return FIN_PREDICTIVE_ERR_OK;
}

//=============================================================================
// Utility Functions
//=============================================================================

fin_predictive_state_t* financial_predictive_state_create(
    uint32_t num_assets, uint32_t horizon)
{
    if (num_assets == 0 || horizon == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "financial_predictive_state_create: num_assets is zero");
        return NULL;
    }
    if (num_assets > FIN_PREDICTIVE_MAX_ASSETS) {
        num_assets = FIN_PREDICTIVE_MAX_ASSETS;
    }
    if (horizon > FIN_PREDICTIVE_MAX_HORIZON) {
        horizon = FIN_PREDICTIVE_MAX_HORIZON;
    }

    fin_predictive_state_t* state = (fin_predictive_state_t*)nimcp_malloc(sizeof(fin_predictive_state_t));
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "financial_predictive_state_create: state is NULL");
        return NULL;
    }

    memset(state, 0, sizeof(*state));
    state->num_assets = num_assets;
    state->horizon = horizon;

    uint32_t size = num_assets * horizon;
    state->predictions = (float*)nimcp_calloc(size, sizeof(float));
    state->precisions = (float*)nimcp_calloc(size, sizeof(float));
    if (!state->precisions) return NULL;
    state->prediction_errors = (float*)nimcp_calloc(size, sizeof(float));

    if (!state->predictions || !state->precisions || !state->prediction_errors) {
        financial_predictive_state_destroy(state);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "financial_predictive_state_create: required parameter is NULL (state->predictions, state->precisions, state->prediction_errors)");
        return NULL;
    }

    /* Initialize precisions to 1.0 */
    for (uint32_t i = 0; i < size; i++) {
        state->precisions[i] = 1.0f;
    }

    return state;
}

void financial_predictive_state_destroy(fin_predictive_state_t* state) {
    if (!state) return;

    if (state->predictions) nimcp_free(state->predictions);
    if (state->precisions) nimcp_free(state->precisions);
    if (state->prediction_errors) nimcp_free(state->prediction_errors);
    nimcp_free(state);
    state = NULL;
}

fin_active_inference_result_t* financial_predictive_result_create(
    uint32_t num_assets, uint32_t num_actions)
{
    if (num_assets == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "financial_predictive_result_create: num_assets is zero");
        return NULL;
    }
    if (num_actions == 0) num_actions = FIN_ACTION_COUNT;

    fin_active_inference_result_t* result =
        (fin_active_inference_result_t*)nimcp_malloc(sizeof(fin_active_inference_result_t));
    if (!result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "financial_predictive_result_create: result is NULL");
        return NULL;
    }

    memset(result, 0, sizeof(*result));
    result->num_weights = num_assets;
    result->num_actions = num_actions;

    result->action_weights = (float*)nimcp_calloc(num_assets, sizeof(float));
    result->all_actions = (fin_efe_result_t*)nimcp_calloc(num_actions, sizeof(fin_efe_result_t));

    if (!result->action_weights || !result->all_actions) {
        financial_predictive_result_destroy(result);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "financial_predictive_result_create: required parameter is NULL (result->action_weights, result->all_actions)");
        return NULL;
    }

    return result;
}

void financial_predictive_result_destroy(fin_active_inference_result_t* result) {
    if (!result) return;

    if (result->action_weights) nimcp_free(result->action_weights);
    if (result->all_actions) nimcp_free(result->all_actions);
    nimcp_free(result);
    result = NULL;
}

//=============================================================================
// Precision Modulation
//=============================================================================

int financial_predictive_bridge_set_inflammation(
    financial_predictive_bridge_t* bridge, float level)
{
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_predictive_bridge_set_inflammation: bridge is NULL");
        return FIN_PREDICTIVE_ERR_NULL;
    }
    bridge->inflammation = nimcp_clampf(level, 0.0f, 1.0f);
    return FIN_PREDICTIVE_ERR_OK;
}

int financial_predictive_bridge_set_fatigue(
    financial_predictive_bridge_t* bridge, float level)
{
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_predictive_bridge_set_fatigue: bridge is NULL");
        return FIN_PREDICTIVE_ERR_NULL;
    }
    bridge->fatigue = nimcp_clampf(level, 0.0f, 1.0f);
    return FIN_PREDICTIVE_ERR_OK;
}

//=============================================================================
// Statistics
//=============================================================================

int financial_predictive_bridge_get_stats(
    const financial_predictive_bridge_t* bridge,
    fin_predictive_bridge_stats_t* stats)
{
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_predictive_bridge_get_stats: bridge is NULL");
        return FIN_PREDICTIVE_ERR_NULL;
    }
    if (!stats) {
        set_error("NULL stats");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_predictive_bridge_get_stats: stats is NULL");
        return FIN_PREDICTIVE_ERR_NULL;
    }
    *stats = bridge->stats;
    return FIN_PREDICTIVE_ERR_OK;
}

void financial_predictive_bridge_reset_stats(financial_predictive_bridge_t* bridge) {
    if (!bridge) return;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
}

const char* financial_predictive_bridge_get_last_error(void) {
    return fin_predictive_last_error;
}
