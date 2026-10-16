//=============================================================================
// nimcp_financial_neural_bridge.c - Financial Neural Integration Bridge
//=============================================================================
/**
 * @file nimcp_financial_neural_bridge.c
 * @brief SNN spike encoding, STDP reward learning, LNN prediction,
 *        plasticity adaptation, quantum optimization for financial data
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#include "cognitive/parietal/nimcp_financial_neural_bridge.h"
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
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_learning_constants.h"
#include "constants/nimcp_threshold_constants.h"
#include "constants/nimcp_math_constants.h"
#include "utils/math/nimcp_math_helpers.h"

/* Health agent: using pre-existing custom implementation */
static nimcp_health_agent_t* g_fin_neural_health_agent = NULL;


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
static void* g_fin_neural_bridge_immune = NULL;
static void* g_fin_neural_bridge_bbb = NULL;

BRIDGE_DEFINE_MESH_REGISTRATION(fin_neural, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define KG_MSG_FIN_NEURAL_REQUEST    "FIN_NEURAL_REQUEST"
#define KG_MSG_FIN_NEURAL_RESPONSE   "FIN_NEURAL_RESPONSE"
#define KG_MSG_FIN_NEURAL_ERROR      "FIN_NEURAL_ERROR"
#define KG_MSG_FIN_NEURAL_UPDATE     "FIN_NEURAL_UPDATE"

static inline void fin_neural_heartbeat(const char* operation, float progress) {
    if (g_fin_neural_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_fin_neural_health_agent, operation, progress);
    }
}

static inline void fin_neural_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_fin_neural_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_fin_neural_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_fin_neural_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

//=============================================================================
// Thread-local Error
//=============================================================================

static _Thread_local char fin_neural_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(fin_neural_last_error, sizeof(fin_neural_last_error), fmt, args);
    va_end(args);
}

//=============================================================================
// Immune/BBB Validation Helper (Global)
//=============================================================================
static int fin_neural_bridge_validate_subsystems(const char* operation) {
    if (g_fin_neural_bridge_immune) {
        int rc = brain_immune_validate_operation(g_fin_neural_bridge_immune, operation, 5);
        if (rc != 0) {
            set_error("financial_neural_bridge: immune validation failed for %s", operation);
            return FIN_NEURAL_ERR_SUBSYSTEM;
        }
    }
    if (g_fin_neural_bridge_bbb) {
        int rc = bbb_validate_data(g_fin_neural_bridge_bbb, NULL, 0, operation);
        if (rc != 0) {
            set_error("financial_neural_bridge: BBB validation failed for %s", operation);
            return FIN_NEURAL_ERR_SUBSYSTEM;
        }
    }
    return FIN_NEURAL_ERR_OK;
}

//=============================================================================
// RNG Utilities
//=============================================================================

static float neural_randf(uint64_t* state) {
    *state = *state * 6364136223846793005ULL + 1442695040888963407ULL;
    return (float)((*state >> 33) & 0x7FFFFFFF) / (float)0x7FFFFFFF;
}

static float neural_randn(uint64_t* state) {
    float u1 = neural_randf(state) + 1e-10f;
    float u2 = neural_randf(state);
    return sqrtf(-2.0f * logf(u1)) * cosf(NIMCP_TWO_PI_F * u2);
}

//=============================================================================
// Internal Structure
//=============================================================================

struct financial_neural_bridge {
    fin_neural_config_t config;
    fin_neural_state_t state;
    fin_neural_stats_t stats;
    float inflammation, fatigue;
    /* Subsystem pointers */
    void* snn;
    void* stdp;
    void* lnn;
    void* plasticity;
    void* quantum;
    brain_immune_system_t* immune;
    void* health_agent;
    void* logger;  /* Phase 8: Change Set 2/3 */
    void* fuzzy_bridge;
    bbb_system_t bbb;
    kg_wiring_t* kg_wiring;
    /* Security validation flags */
    bool enable_bbb_validation;
    bool enable_immune_validation;
    /* LNN internal state */
    float lnn_state[FIN_NEURAL_MAX_LNN_STATE_DIM];
    uint32_t lnn_state_dim;
    float lnn_time;
    /* Training state */
    float current_loss;
    float convergence_degree;
    uint32_t training_epoch;
    /* Memory store */
    fin_memory_pattern_t memory[FIN_NEURAL_MAX_MEMORY_PATTERNS];
    uint32_t memory_count;
    /* Plasticity state */
    fin_plasticity_params_t current_plasticity;
    /* RNG state for MC/noise */
    uint64_t rng_state;
};

//=============================================================================
// Instance-Level Heartbeat Helper (Phase 8: Change Set 2/3)
//=============================================================================

static inline void neural_heartbeat_instance(financial_neural_bridge_t* bridge,
                                              const char* op, float progress) {
    if (bridge && bridge->health_agent) {
        /* nimcp_health_agent_heartbeat_ex would be called here */
        (void)op; (void)progress;
    }
}

//=============================================================================
// Logging Macros (Phase 8: Change Set 2/3)
//=============================================================================

#define FIN_NEURAL_LOG_DEBUG(bridge, fmt, ...) /* placeholder */
#define FIN_NEURAL_LOG_INFO(bridge, fmt, ...)  /* placeholder */
#define FIN_NEURAL_LOG_WARN(bridge, fmt, ...)  /* placeholder */
#define FIN_NEURAL_LOG_ERROR(bridge, fmt, ...) /* placeholder */

/**
 * @brief Publish a message through KG wiring
 * @param bridge Neural bridge instance
 * @param msg_type Message type string
 * @param payload Payload data
 * @param size Payload size in bytes
 * @return 0 on success
 */
static int neural_kg_publish(financial_neural_bridge_t* bridge, const char* msg_type,
                              const void* payload, size_t size) {
    if (bridge && bridge->kg_wiring) {
        /* kg_wiring_publish would be called here */
        (void)msg_type; (void)payload; (void)size;
        return 0;
    }
    return 0;
}

//=============================================================================
// Instance-Level Validation Helper
//=============================================================================

#ifndef FIN_NEURAL_ERR_VALIDATION
#define FIN_NEURAL_ERR_VALIDATION (FIN_NEURAL_ERROR_BASE + 12)
#endif

static int neural_validate_subsystems(financial_neural_bridge_t* bridge, const char* operation) {
    if (!bridge) return FIN_NEURAL_ERR_NULL;

    if (bridge->enable_bbb_validation && bridge->bbb) {
        int rc = bbb_validate_data(bridge->bbb, NULL, 0, operation);
        if (rc != 0) {
            set_error("BBB validation failed for %s", operation);
            NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_BBB_VALIDATION,
                "financial_neural: BBB validation failed for %s", operation);
            return FIN_NEURAL_ERR_VALIDATION;
        }
    }

    if (bridge->enable_immune_validation && bridge->immune) {
        int rc = brain_immune_validate_operation(bridge->immune, operation, 5);
        if (rc != 0) {
            set_error("Immune validation failed for %s", operation);
            NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_BBB_VALIDATION,
                "financial_neural: immune validation failed for %s", operation);
            return FIN_NEURAL_ERR_VALIDATION;
        }
    }

    return FIN_NEURAL_ERR_OK;
}

//=============================================================================
// Antigen Presentation Helper
//=============================================================================
static void neural_present_antigen(financial_neural_bridge_t* bridge,
                                    const char* anomaly, uint32_t severity) {
    if (bridge && bridge->immune) {
        uint8_t sig[64] = {0};
        snprintf((char*)sig, sizeof(sig), "fin_neural:%s", anomaly);
        uint32_t antigen_id = 0;
        brain_immune_present_antigen(bridge->immune, 0, sig, strlen((char*)sig),
                                      severity, 0, &antigen_id);
    }
}

//=============================================================================
// Helper: Fuzzy membership functions
//=============================================================================

/** S-shaped membership function: returns 0 below low, 1 above high, smooth in between */
static float s_shaped(float low, float high, float x) {
    if (x <= low) return 0.0f;
    if (x >= high) return 1.0f;
    /* Guard against division by zero when low == high */
    float range = high - low;
    if (range < 1e-8f) return 0.5f;
    float t = (x - low) / range;
    return t * t * (3.0f - 2.0f * t);
}

//=============================================================================
// Lifecycle
//=============================================================================

fin_neural_config_t financial_neural_bridge_default_config(void) {
    fin_neural_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    /* Encoding */
    cfg.default_encoding = FIN_ENCODING_RATE;
    cfg.spike_channels = 32;
    cfg.encoding_gain = 1.0f;
    cfg.encoding_threshold = 0.01f;
    cfg.enable_fuzzy_encoding = false;

    /* STDP */
    cfg.stdp_learning_rate = NIMCP_LEARNING_RATE_DEFAULT;
    cfg.stdp_temporal_window_ms = 20.0f;
    cfg.reward_decay_rate = 0.95f;

    /* LNN */
    cfg.lnn_state_dim = 8;
    cfg.lnn_time_constant = 0.1f;
    cfg.prediction_horizon = 10;
    cfg.enable_fuzzy_prediction = true;

    /* Plasticity */
    cfg.plasticity_base_rate = 0.01f;
    cfg.performance_window_days = 30.0f;
    cfg.stability_window_days = 30.0f;

    /* Quantum */
    cfg.enable_quantum = false;
    cfg.quantum_max_qubits = 0;

    /* Memory */
    cfg.max_memory_patterns = FIN_NEURAL_MAX_MEMORY_PATTERNS;
    cfg.consolidation_threshold = 0.5f;

    /* Modulation */
    cfg.inflammation_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    cfg.fatigue_sensitivity = NIMCP_SENSITIVITY_DEFAULT;

    return cfg;
}

financial_neural_bridge_t* financial_neural_bridge_create(
    const fin_neural_config_t* config)
{
    fin_neural_heartbeat("financial_neural_bridge_create", 0.0f);

    financial_neural_bridge_t* bridge =
        (financial_neural_bridge_t*)nimcp_malloc(sizeof(financial_neural_bridge_t));
    if (!bridge) {
        set_error("Failed to allocate financial_neural_bridge_t");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY, "Failed to allocate financial_neural_bridge_t");
        return NULL;
    }
    memset(bridge, 0, sizeof(*bridge));

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = financial_neural_bridge_default_config();
    }

    bridge->state = FIN_NEURAL_STATE_IDLE;
    bridge->inflammation = 0.0f;
    bridge->fatigue = 0.0f;

    /* Initialize LNN state */
    bridge->lnn_state_dim = bridge->config.lnn_state_dim;
    if (bridge->lnn_state_dim > FIN_NEURAL_MAX_LNN_STATE_DIM) {
        bridge->lnn_state_dim = FIN_NEURAL_MAX_LNN_STATE_DIM;
    }
    bridge->lnn_time = 0.0f;
    for (uint32_t i = 0; i < bridge->lnn_state_dim; i++) {
        bridge->lnn_state[i] = 0.0f;
    }

    /* Initialize training */
    bridge->current_loss = 1.0f;
    bridge->convergence_degree = 0.0f;
    bridge->training_epoch = 0;

    /* Initialize memory */
    bridge->memory_count = 0;

    /* Default plasticity */
    memset(&bridge->current_plasticity, 0, sizeof(bridge->current_plasticity));
    bridge->current_plasticity.current_plasticity_rate = bridge->config.plasticity_base_rate;
    bridge->current_plasticity.adapted_risk_tolerance = 1.0f;
    bridge->current_plasticity.adapted_position_size_scale = 1.0f;
    bridge->current_plasticity.adapted_stop_loss_distance = 1.0f;

    /* RNG seed */
    bridge->rng_state = 42;

    /* KG wiring (Change Set 1) */
    bridge->kg_wiring = NULL;

    fin_neural_heartbeat("financial_neural_bridge_create", 1.0f);
    return bridge;
}

void financial_neural_bridge_destroy(financial_neural_bridge_t* bridge) {
    if (!bridge) return;
    fin_neural_heartbeat("financial_neural_bridge_destroy", 0.0f);
    nimcp_free(bridge);
    bridge = NULL;
    fin_neural_heartbeat("financial_neural_bridge_destroy", 1.0f);
}

fin_neural_state_t financial_neural_bridge_get_state(
    const financial_neural_bridge_t* bridge)
{
    if (!bridge) return FIN_NEURAL_STATE_UNINITIALIZED;
    return bridge->state;
}

int financial_neural_bridge_reset(financial_neural_bridge_t* bridge) {
    if (!bridge) {
        set_error("NULL bridge in reset");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_neural_bridge_reset: bridge is NULL");
        return FIN_NEURAL_ERR_NULL;
    }

    fin_neural_heartbeat_instance(
        (nimcp_health_agent_t*)bridge->health_agent, "reset", 0.0f);

    bridge->state = FIN_NEURAL_STATE_IDLE;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->inflammation = 0.0f;
    bridge->fatigue = 0.0f;

    /* Reset LNN state */
    for (uint32_t i = 0; i < bridge->lnn_state_dim; i++) {
        bridge->lnn_state[i] = 0.0f;
    }
    bridge->lnn_time = 0.0f;

    /* Reset training */
    bridge->current_loss = 1.0f;
    bridge->convergence_degree = 0.0f;
    bridge->training_epoch = 0;

    /* Reset memory */
    bridge->memory_count = 0;

    /* Reset plasticity */
    bridge->current_plasticity.current_plasticity_rate = bridge->config.plasticity_base_rate;
    bridge->current_plasticity.adapted_risk_tolerance = 1.0f;
    bridge->current_plasticity.adapted_position_size_scale = 1.0f;
    bridge->current_plasticity.adapted_stop_loss_distance = 1.0f;
    bridge->current_plasticity.performance_score = 0.0f;
    bridge->current_plasticity.stability_score = 0.0f;
    bridge->current_plasticity.adaptation_epoch = 0;
    bridge->current_plasticity.fuzzy_adaptation_degree = 0.0f;

    fin_neural_heartbeat_instance(
        (nimcp_health_agent_t*)bridge->health_agent, "reset", 1.0f);
    return FIN_NEURAL_ERR_OK;
}

//=============================================================================
// Subsystem Setters
//=============================================================================

int financial_neural_bridge_set_snn(financial_neural_bridge_t* bridge, void* snn) {
    if (!bridge) { set_error("NULL bridge"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_neural_bridge_set_snn: bridge is NULL"); return FIN_NEURAL_ERR_NULL; }
    bridge->snn = snn;
    return FIN_NEURAL_ERR_OK;
}

int financial_neural_bridge_set_stdp(financial_neural_bridge_t* bridge, void* stdp) {
    if (!bridge) { set_error("NULL bridge"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_neural_bridge_set_stdp: bridge is NULL"); return FIN_NEURAL_ERR_NULL; }
    bridge->stdp = stdp;
    return FIN_NEURAL_ERR_OK;
}

int financial_neural_bridge_set_lnn(financial_neural_bridge_t* bridge, void* lnn) {
    if (!bridge) { set_error("NULL bridge"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_neural_bridge_set_lnn: bridge is NULL"); return FIN_NEURAL_ERR_NULL; }
    bridge->lnn = lnn;
    return FIN_NEURAL_ERR_OK;
}

int financial_neural_bridge_set_plasticity(financial_neural_bridge_t* bridge,
                                            void* plasticity)
{
    if (!bridge) { set_error("NULL bridge"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_neural_bridge_set_plasticity: bridge is NULL"); return FIN_NEURAL_ERR_NULL; }
    bridge->plasticity = plasticity;
    return FIN_NEURAL_ERR_OK;
}

int financial_neural_bridge_set_quantum(financial_neural_bridge_t* bridge,
                                         void* quantum)
{
    if (!bridge) { set_error("NULL bridge"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_neural_bridge_set_quantum: bridge is NULL"); return FIN_NEURAL_ERR_NULL; }
    bridge->quantum = quantum;
    return FIN_NEURAL_ERR_OK;
}

int financial_neural_bridge_set_immune(financial_neural_bridge_t* bridge,
                                        void* immune)
{
    if (!bridge) { set_error("NULL bridge"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_neural_bridge_set_immune: bridge is NULL"); return FIN_NEURAL_ERR_NULL; }
    bridge->immune = (brain_immune_system_t*)immune;
    return FIN_NEURAL_ERR_OK;
}

int financial_neural_bridge_set_health_agent(financial_neural_bridge_t* bridge,
                                              void* health_agent)
{
    if (!bridge) { set_error("NULL bridge"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_neural_bridge_set_health_agent: bridge is NULL"); return FIN_NEURAL_ERR_NULL; }
    bridge->health_agent = health_agent;
    return FIN_NEURAL_ERR_OK;
}

int financial_neural_bridge_set_fuzzy_bridge(financial_neural_bridge_t* bridge,
                                              void* fuzzy_bridge)
{
    if (!bridge) { set_error("NULL bridge"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_neural_bridge_set_fuzzy_bridge: bridge is NULL"); return FIN_NEURAL_ERR_NULL; }
    bridge->fuzzy_bridge = fuzzy_bridge;
    return FIN_NEURAL_ERR_OK;
}

int financial_neural_bridge_set_instance_bbb(financial_neural_bridge_t* bridge, void* bbb)
{
    if (!bridge) { set_error("NULL bridge"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_neural_bridge_set_instance_bbb: bridge is NULL"); return FIN_NEURAL_ERR_NULL; }
    bridge->bbb = (bbb_system_t)bbb;
    return FIN_NEURAL_ERR_OK;
}

int financial_neural_bridge_enable_bbb_validation(financial_neural_bridge_t* bridge,
                                                   bool enable)
{
    if (!bridge) { set_error("NULL bridge"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_neural_bridge_enable_bbb_validation: bridge is NULL"); return FIN_NEURAL_ERR_NULL; }
    bridge->enable_bbb_validation = enable;
    return FIN_NEURAL_ERR_OK;
}

int financial_neural_bridge_enable_immune_validation(financial_neural_bridge_t* bridge,
                                                      bool enable)
{
    if (!bridge) { set_error("NULL bridge"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_neural_bridge_enable_immune_validation: bridge is NULL"); return FIN_NEURAL_ERR_NULL; }
    bridge->enable_immune_validation = enable;
    return FIN_NEURAL_ERR_OK;
}

//=============================================================================
// KG Wiring Setter (Change Set 1)
//=============================================================================

int financial_neural_bridge_set_kg_wiring(financial_neural_bridge_t* bridge, kg_wiring_t* kg) {
    if (!bridge) {
        set_error("set_kg_wiring: NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_neural_bridge_set_kg_wiring: NULL bridge");
        return FIN_NEURAL_ERR_NULL;
    }
    bridge->kg_wiring = kg;
    return FIN_NEURAL_ERR_OK;
}

//=============================================================================
// Spike Encoding
//=============================================================================

/** Event type modulation factor: how strongly each event type drives spikes */
static float event_type_modulation(fin_market_event_type_t type) {
    switch (type) {
        case FIN_EVENT_PRICE_CHANGE:       return 1.0f;
        case FIN_EVENT_VOLUME_SPIKE:       return 0.8f;
        case FIN_EVENT_VOLATILITY_SHIFT:   return 1.2f;
        case FIN_EVENT_REGIME_CHANGE:      return 1.5f;
        case FIN_EVENT_SENTIMENT_SHIFT:    return 0.9f;
        case FIN_EVENT_INDICATOR_SIGNAL:   return 0.7f;
        case FIN_EVENT_EARNINGS:           return 1.3f;
        case FIN_EVENT_MACRO_RELEASE:      return 1.1f;
        default:                           return 1.0f;
    }
}

int financial_neural_bridge_encode_market_event(
    financial_neural_bridge_t* bridge,
    const fin_market_event_t* event,
    fin_spike_train_t* out_spikes)
{
    if (!bridge) { set_error("NULL bridge"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_neural_bridge_encode_market_event: bridge is NULL"); return FIN_NEURAL_ERR_NULL; }
    if (!event || !out_spikes) {
        set_error("NULL event or output");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_neural_bridge_encode_market_event: NULL event or output");
        return FIN_NEURAL_ERR_NULL;
    }

    /* Instance-level security validation */
    int val_rc = neural_validate_subsystems(bridge, "encode_market_event");
    if (val_rc != FIN_NEURAL_ERR_OK) return val_rc;

    fin_neural_heartbeat_instance(
        (nimcp_health_agent_t*)bridge->health_agent, "encode_market_event", 0.0f);

    bridge->state = FIN_NEURAL_STATE_ENCODING;
    memset(out_spikes, 0, sizeof(*out_spikes));

    uint32_t channels = bridge->config.spike_channels;
    if (channels > FIN_NEURAL_MAX_SPIKE_CHANNELS) {
        channels = FIN_NEURAL_MAX_SPIKE_CHANNELS;
    }

    float gain = bridge->config.encoding_gain;
    float mod = event_type_modulation(event->type);
    float abs_mag = fabsf(event->magnitude);

    /* Apply inflammation/fatigue modulation */
    float health_mod = 1.0f
        - bridge->inflammation * bridge->config.inflammation_sensitivity * 0.3f
        - bridge->fatigue * bridge->config.fatigue_sensitivity * 0.2f;
    if (health_mod < 0.1f) health_mod = 0.1f;

    fin_spike_encoding_t enc = bridge->config.default_encoding;
    out_spikes->encoding = enc;

    float total_activity = 0.0f;
    uint32_t active = 0;

    switch (enc) {
        case FIN_ENCODING_RATE: {
            /* Rate encoding: spike_rate = gain * |magnitude| * mod for each channel */
            float base_rate = gain * abs_mag * mod * health_mod;
            for (uint32_t i = 0; i < channels; i++) {
                /* Distribute across channels with slight variation based on event type */
                float channel_mod = 1.0f;
                if ((uint32_t)event->type < channels) {
                    /* Boost the channel corresponding to event type */
                    if (i == (uint32_t)event->type) {
                        channel_mod = 2.0f;
                    }
                }
                float rate = base_rate * channel_mod;
                if (rate > bridge->config.encoding_threshold) {
                    out_spikes->spike_rates[i] = rate;
                    out_spikes->spike_times[i] = event->timestamp_us;
                    active++;
                }
                total_activity += out_spikes->spike_rates[i];
            }
            break;
        }

        case FIN_ENCODING_TEMPORAL: {
            /* Temporal encoding: spike time encodes magnitude (earlier = stronger) */
            float base_rate = gain * abs_mag * mod * health_mod;
            for (uint32_t i = 0; i < channels; i++) {
                float rate = base_rate;
                if (i == (uint32_t)(event->type % channels)) {
                    rate *= 1.5f;
                }
                if (rate > bridge->config.encoding_threshold) {
                    out_spikes->spike_rates[i] = rate;
                    /* Temporal: stronger magnitude => earlier spike time */
                    uint64_t delay_us = (uint64_t)((1.0f / (rate + 0.001f)) * 1000.0f);
                    out_spikes->spike_times[i] = event->timestamp_us + delay_us;
                    active++;
                }
                total_activity += out_spikes->spike_rates[i];
            }
            break;
        }

        case FIN_ENCODING_POPULATION: {
            /* Population encoding: Gaussian distribution centered on magnitude */
            float center = abs_mag * (float)channels;
            float sigma = (float)channels / 6.0f;
            if (sigma < 1.0f) sigma = 1.0f;
            for (uint32_t i = 0; i < channels; i++) {
                float dist = (float)i - center;
                float rate = gain * mod * health_mod *
                             expf(-0.5f * (dist * dist) / (sigma * sigma));
                if (rate > bridge->config.encoding_threshold) {
                    out_spikes->spike_rates[i] = rate;
                    out_spikes->spike_times[i] = event->timestamp_us;
                    active++;
                }
                total_activity += out_spikes->spike_rates[i];
            }
            break;
        }

        case FIN_ENCODING_FUZZY_POPULATION: {
            /* Fuzzy population: use context as regime degrees if available */
            if (bridge->config.enable_fuzzy_encoding && event->context_count >= 6) {
                /* context[0..5] interpreted as bull/bear/sideways/high_vol/crisis/recovery */
                for (uint32_t i = 0; i < channels; i++) {
                    float rate = 0.0f;
                    if (i < 6) {
                        rate = event->context[i] * gain * health_mod;
                    } else {
                        /* Remaining channels: base rate from magnitude */
                        rate = gain * abs_mag * mod * health_mod *
                               expf(-(float)(i - 6) / (float)channels);
                    }
                    if (rate > bridge->config.encoding_threshold) {
                        out_spikes->spike_rates[i] = rate;
                        out_spikes->spike_times[i] = event->timestamp_us;
                        active++;
                    }
                    total_activity += out_spikes->spike_rates[i];
                }
            } else {
                /* Fallback: population encoding if fuzzy not enabled */
                float center = abs_mag * (float)channels;
                float sigma = (float)channels / 6.0f;
                if (sigma < 1.0f) sigma = 1.0f;
                for (uint32_t i = 0; i < channels; i++) {
                    float dist = (float)i - center;
                    float rate = gain * mod * health_mod *
                                 expf(-0.5f * (dist * dist) / (sigma * sigma));
                    if (rate > bridge->config.encoding_threshold) {
                        out_spikes->spike_rates[i] = rate;
                        out_spikes->spike_times[i] = event->timestamp_us;
                        active++;
                    }
                    total_activity += out_spikes->spike_rates[i];
                }
            }
            break;
        }

        default:
            set_error("Unknown encoding type %d", (int)enc);
            NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_INVALID_PARAM, "financial_neural_bridge_encode_market_event: Unknown encoding type %d", (int)enc);
            bridge->state = FIN_NEURAL_STATE_ERROR;
            return FIN_NEURAL_ERR_ENCODING;
    }

    out_spikes->active_channels = active;
    out_spikes->total_activity = total_activity;

    /* Check for spike rate anomaly (biological max ~500Hz) */
    for (uint32_t i = 0; i < channels; i++) {
        if (out_spikes->spike_rates[i] > 500.0f) {
            neural_present_antigen(bridge, "spike_rate_anomaly", 3);
            break; /* Only present once per encoding */
        }
    }

    bridge->stats.events_encoded++;
    bridge->state = FIN_NEURAL_STATE_IDLE;

    fin_neural_heartbeat_instance(
        (nimcp_health_agent_t*)bridge->health_agent, "encode_market_event", 1.0f);

    return FIN_NEURAL_ERR_OK;
}

//=============================================================================
// Fuzzy Regime Encoding
//=============================================================================

int financial_neural_bridge_encode_fuzzy_regime(
    financial_neural_bridge_t* bridge,
    const fin_fuzzy_market_condition_t* condition,
    fin_spike_train_t* out_spikes)
{
    if (!bridge) { set_error("NULL bridge"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_neural_bridge_encode_fuzzy_regime: bridge is NULL"); return FIN_NEURAL_ERR_NULL; }
    if (!condition || !out_spikes) {
        set_error("NULL condition or output");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_neural_bridge_encode_fuzzy_regime: NULL condition or output");
        return FIN_NEURAL_ERR_NULL;
    }

    fin_neural_heartbeat_instance(
        (nimcp_health_agent_t*)bridge->health_agent, "encode_fuzzy_regime", 0.0f);

    memset(out_spikes, 0, sizeof(*out_spikes));
    out_spikes->encoding = FIN_ENCODING_FUZZY_POPULATION;

    float gain = bridge->config.encoding_gain;
    float total = 0.0f;
    uint32_t active = 0;

    /* Map 6 fuzzy regime degrees to channels 0-5:
     *   0 = bull, 1 = bear, 2 = sideways, 3 = high_vol, 4 = crisis, 5 = recovery */
    float degrees[6];
    degrees[0] = condition->bull_degree;
    degrees[1] = condition->bear_degree;
    degrees[2] = condition->sideways_degree;
    degrees[3] = condition->high_vol_degree;
    degrees[4] = condition->crisis_degree;
    degrees[5] = condition->recovery_degree;

    uint32_t channels = bridge->config.spike_channels;
    if (channels > FIN_NEURAL_MAX_SPIKE_CHANNELS) {
        channels = FIN_NEURAL_MAX_SPIKE_CHANNELS;
    }

    for (uint32_t i = 0; i < channels; i++) {
        if (i < 6) {
            out_spikes->spike_rates[i] = degrees[i] * gain;
        } else {
            out_spikes->spike_rates[i] = 0.0f;
        }
        if (out_spikes->spike_rates[i] > bridge->config.encoding_threshold) {
            active++;
        }
        total += out_spikes->spike_rates[i];
    }

    out_spikes->active_channels = active;
    out_spikes->total_activity = total;

    bridge->stats.events_encoded++;

    fin_neural_heartbeat_instance(
        (nimcp_health_agent_t*)bridge->health_agent, "encode_fuzzy_regime", 1.0f);

    return FIN_NEURAL_ERR_OK;
}

//=============================================================================
// Decode Spikes
//=============================================================================

int financial_neural_bridge_decode_spikes(
    financial_neural_bridge_t* bridge,
    const fin_spike_train_t* spikes,
    float* out_signal, float* out_confidence)
{
    if (!bridge) { set_error("NULL bridge"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_neural_bridge_decode_spikes: bridge is NULL"); return FIN_NEURAL_ERR_NULL; }
    if (!spikes || !out_signal || !out_confidence) {
        set_error("NULL parameter in decode_spikes");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_neural_bridge_decode_spikes: NULL parameter");
        return FIN_NEURAL_ERR_NULL;
    }

    uint32_t channels = bridge->config.spike_channels;
    if (channels > FIN_NEURAL_MAX_SPIKE_CHANNELS) {
        channels = FIN_NEURAL_MAX_SPIKE_CHANNELS;
    }

    /* Weighted average of spike_rates to reconstruct signal */
    float weighted_sum = 0.0f;
    float weight_total = 0.0f;

    for (uint32_t i = 0; i < channels; i++) {
        float w = (float)(i + 1) / (float)channels;
        weighted_sum += spikes->spike_rates[i] * w;
        weight_total += w;
    }

    if (weight_total > 0.0f) {
        *out_signal = weighted_sum / weight_total;
    } else {
        *out_signal = 0.0f;
    }

    /* Confidence = total_activity / channels */
    if (channels > 0) {
        *out_confidence = spikes->total_activity / (float)channels;
    } else {
        *out_confidence = 0.0f;
    }
    *out_confidence = nimcp_clampf(*out_confidence, 0.0f, 1.0f);

    return FIN_NEURAL_ERR_OK;
}

//=============================================================================
// STDP Reward Learning
//=============================================================================

int financial_neural_bridge_compute_fuzzy_reward(
    financial_neural_bridge_t* bridge,
    float trade_return,
    fin_stdp_reward_t* out_reward)
{
    if (!bridge) { set_error("NULL bridge"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_neural_bridge_compute_fuzzy_reward: bridge is NULL"); return FIN_NEURAL_ERR_NULL; }
    if (!out_reward) { set_error("NULL output"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_neural_bridge_compute_fuzzy_reward: out_reward is NULL"); return FIN_NEURAL_ERR_NULL; }

    memset(out_reward, 0, sizeof(*out_reward));

    /* Fuzzy reward membership functions:
     *   profitable: s_shaped(0.05, 0.20, trade_return)
     *   neutral:    1 - |trade_return|/0.1, clamped [0,1]
     *   loss:       s_shaped(0.05, 0.20, -trade_return) */
    out_reward->fuzzy_profitable_degree = s_shaped(0.05f, 0.20f, trade_return);
    out_reward->fuzzy_neutral_degree =
        nimcp_clampf(1.0f - fabsf(trade_return) / 0.1f, 0.0f, 1.0f);
    out_reward->fuzzy_loss_degree = s_shaped(0.05f, 0.20f, -trade_return);

    /* reward_magnitude = profitable - loss, range [-1, 1] */
    out_reward->reward_magnitude =
        out_reward->fuzzy_profitable_degree - out_reward->fuzzy_loss_degree;

    return FIN_NEURAL_ERR_OK;
}

int financial_neural_bridge_stdp_reward(
    financial_neural_bridge_t* bridge,
    float trade_return,
    uint64_t trade_duration_us,
    fin_stdp_reward_t* out_reward)
{
    if (!bridge) { set_error("NULL bridge"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_neural_bridge_stdp_reward: bridge is NULL"); return FIN_NEURAL_ERR_NULL; }
    if (!out_reward) { set_error("NULL output"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_neural_bridge_stdp_reward: out_reward is NULL"); return FIN_NEURAL_ERR_NULL; }

    /* Instance-level security validation */
    int val_rc = neural_validate_subsystems(bridge, "stdp_reward");
    if (val_rc != FIN_NEURAL_ERR_OK) return val_rc;

    fin_neural_heartbeat_instance(
        (nimcp_health_agent_t*)bridge->health_agent, "stdp_reward", 0.0f);

    bridge->state = FIN_NEURAL_STATE_LEARNING;

    /* Compute fuzzy reward components */
    int rc = financial_neural_bridge_compute_fuzzy_reward(bridge, trade_return, out_reward);
    if (rc != FIN_NEURAL_ERR_OK) {
        bridge->state = FIN_NEURAL_STATE_ERROR;
        return rc;
    }

    /* Temporal discount: exp(-duration / (1e6 * 86400 * 30)) => 30-day half-life in microseconds */
    double duration_sec = (double)trade_duration_us / 1e6;
    double half_life_sec = 86400.0 * 30.0;
    out_reward->temporal_discount = (float)exp(-duration_sec / half_life_sec);

    out_reward->reward_timestamp_us = trade_duration_us;

    /* Apply temporal discount to reward magnitude */
    out_reward->reward_magnitude *= out_reward->temporal_discount;

    /* Apply inflammation/fatigue modulation to learning signal */
    float health_mod = 1.0f
        - bridge->inflammation * bridge->config.inflammation_sensitivity * 0.2f
        - bridge->fatigue * bridge->config.fatigue_sensitivity * 0.15f;
    if (health_mod < 0.1f) health_mod = 0.1f;
    out_reward->reward_magnitude *= health_mod;

    /* Update stats */
    bridge->stats.stdp_updates++;
    bridge->stats.cumulative_reward += out_reward->reward_magnitude;

    bridge->state = FIN_NEURAL_STATE_IDLE;

    fin_neural_heartbeat_instance(
        (nimcp_health_agent_t*)bridge->health_agent, "stdp_reward", 1.0f);

    return FIN_NEURAL_ERR_OK;
}

//=============================================================================
// LNN Prediction
//=============================================================================

int financial_neural_bridge_lnn_predict(
    financial_neural_bridge_t* bridge,
    const fin_time_series_t* recent_data,
    uint32_t horizon_steps,
    fin_neural_prediction_t* out_prediction)
{
    if (!bridge) { set_error("NULL bridge"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_neural_bridge_lnn_predict: bridge is NULL"); return FIN_NEURAL_ERR_NULL; }
    if (!out_prediction) { set_error("NULL output"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_neural_bridge_lnn_predict: out_prediction is NULL"); return FIN_NEURAL_ERR_NULL; }

    /* Global validation (backward compatibility) */
    int val_rc = fin_neural_bridge_validate_subsystems("lnn_predict");
    if (val_rc != FIN_NEURAL_ERR_OK) return val_rc;

    /* Instance-level security validation */
    val_rc = neural_validate_subsystems(bridge, "lnn_predict");
    if (val_rc != FIN_NEURAL_ERR_OK) return val_rc;

    fin_neural_heartbeat_instance(
        (nimcp_health_agent_t*)bridge->health_agent, "lnn_predict", 0.0f);

    bridge->state = FIN_NEURAL_STATE_PREDICTING;
    memset(out_prediction, 0, sizeof(*out_prediction));

    uint32_t dim = bridge->lnn_state_dim;
    float tc = bridge->config.lnn_time_constant;
    if (tc < 1e-6f) tc = 0.1f;

    float dt = 0.01f; /* Integration time step */

    /* Use recent data to drive the liquid state machine */
    uint32_t data_len = 0;
    if (recent_data && recent_data->length > 0) {
        data_len = recent_data->length;

        /* Feed recent data into state update */
        uint32_t steps = data_len;
        if (steps > 100) steps = 100; /* Limit computation */

        for (uint32_t s = 0; s < steps; s++) {
            uint32_t idx = data_len - steps + s;
            float price = recent_data->prices[idx];
            /* Normalize price to a reasonable input range */
            float input = 0.0f;
            if (idx > 0) {
                float prev = recent_data->prices[idx - 1];
                if (prev > 0.0f) {
                    input = (price - prev) / prev; /* Return */
                }
            }

            /* LNN state update: state[i] += dt * (-state[i]/tc + input_i) */
            for (uint32_t i = 0; i < dim; i++) {
                float inp_i = input;
                /* Different dimensions capture different timescales */
                if (i > 0) {
                    inp_i *= expf(-(float)i * 0.3f);
                }
                bridge->lnn_state[i] += dt * (
                    -bridge->lnn_state[i] / tc + inp_i
                );
            }
            bridge->lnn_time += dt;
        }
    }

    /* Predicted return = weighted sum of state vector */
    float predicted_return = 0.0f;
    float state_norm = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        float w = 1.0f / (float)(i + 1); /* Decreasing weights for higher dims */
        predicted_return += bridge->lnn_state[i] * w;
        state_norm += bridge->lnn_state[i] * bridge->lnn_state[i];
    }
    state_norm = sqrtf(state_norm);

    /* Predicted volatility = stddev of recent state changes */
    float vol_sum = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        vol_sum += bridge->lnn_state[i] * bridge->lnn_state[i];
    }
    /* Guard against division by zero when dim == 0 */
    float predicted_vol = (dim > 0) ? sqrtf(vol_sum / (float)dim) : 0.0f;

    /* Confidence = 1 / (1 + |state_norm_change|) */
    float confidence = 1.0f / (1.0f + state_norm);

    /* Apply health modulation */
    float health_mod = 1.0f
        - bridge->inflammation * bridge->config.inflammation_sensitivity * 0.15f
        - bridge->fatigue * bridge->config.fatigue_sensitivity * 0.1f;
    if (health_mod < 0.1f) health_mod = 0.1f;
    confidence *= health_mod;

    out_prediction->predicted_return = predicted_return;
    out_prediction->predicted_volatility = predicted_vol;
    out_prediction->predicted_direction = (predicted_return > 0.0f) ? 1.0f :
                                          (predicted_return < 0.0f) ? -1.0f : 0.0f;
    out_prediction->confidence = nimcp_clampf(confidence, 0.0f, 1.0f);

    /* Copy state vector */
    out_prediction->state_dim = dim;
    for (uint32_t i = 0; i < dim; i++) {
        out_prediction->state_vector[i] = bridge->lnn_state[i];
    }

    out_prediction->horizon_steps = horizon_steps;
    out_prediction->prediction_quality = confidence * health_mod;

    /* Fuzzy post-processing: classify state into fuzzy regime */
    if (bridge->config.enable_fuzzy_prediction) {
        fin_fuzzy_market_condition_t* regime = &out_prediction->fuzzy_regime;
        memset(regime, 0, sizeof(*regime));

        /* Use state[0] (trend component) for bull/bear/sideways */
        float trend = (dim > 0) ? bridge->lnn_state[0] : 0.0f;
        regime->bull_degree = s_shaped(0.0f, 0.05f, trend);
        regime->bear_degree = s_shaped(0.0f, 0.05f, -trend);
        regime->sideways_degree = nimcp_clampf(1.0f - fabsf(trend) / 0.02f, 0.0f, 1.0f);

        /* Use state[1] (vol component) for high_vol */
        float vol_state = (dim > 1) ? fabsf(bridge->lnn_state[1]) : 0.0f;
        regime->high_vol_degree = s_shaped(0.02f, 0.08f, vol_state);

        /* Composite for crisis/recovery */
        float composite = (dim > 2) ? bridge->lnn_state[2] : 0.0f;
        regime->crisis_degree = s_shaped(0.0f, 0.1f, -composite) * regime->high_vol_degree;
        regime->recovery_degree = s_shaped(0.0f, 0.1f, composite) *
                                  (1.0f - regime->crisis_degree);

        /* Determine dominant condition */
        float max_deg = regime->bull_degree;
        regime->dominant = FIN_MKT_BULL;

        if (regime->bear_degree > max_deg) {
            max_deg = regime->bear_degree;
            regime->dominant = FIN_MKT_BEAR;
        }
        if (regime->sideways_degree > max_deg) {
            max_deg = regime->sideways_degree;
            regime->dominant = FIN_MKT_SIDEWAYS;
        }
        if (regime->high_vol_degree > max_deg) {
            max_deg = regime->high_vol_degree;
            regime->dominant = FIN_MKT_HIGH_VOLATILITY;
        }
        if (regime->crisis_degree > max_deg) {
            max_deg = regime->crisis_degree;
            regime->dominant = FIN_MKT_CRISIS;
        }
        if (regime->recovery_degree > max_deg) {
            regime->dominant = FIN_MKT_RECOVERY;
        }
    }

    bridge->stats.predictions_made++;
    bridge->state = FIN_NEURAL_STATE_IDLE;

    fin_neural_heartbeat_instance(
        (nimcp_health_agent_t*)bridge->health_agent, "lnn_predict", 1.0f);

    return FIN_NEURAL_ERR_OK;
}

//=============================================================================
// LNN State Update
//=============================================================================

int financial_neural_bridge_lnn_update(
    financial_neural_bridge_t* bridge,
    const float* observation, uint32_t obs_dim)
{
    if (!bridge) { set_error("NULL bridge"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_neural_bridge_lnn_update: bridge is NULL"); return FIN_NEURAL_ERR_NULL; }
    if (!observation) { set_error("NULL observation"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_neural_bridge_lnn_update: observation is NULL"); return FIN_NEURAL_ERR_NULL; }

    fin_neural_heartbeat_instance(
        (nimcp_health_agent_t*)bridge->health_agent, "lnn_update", 0.0f);

    uint32_t dim = bridge->lnn_state_dim;
    float tc = bridge->config.lnn_time_constant;
    if (tc < 1e-6f) tc = 0.1f;
    float dt = 0.01f;

    uint32_t update_dim = (obs_dim < dim) ? obs_dim : dim;

    /* Apply new observation to evolve LNN state */
    for (uint32_t i = 0; i < dim; i++) {
        float input = (i < update_dim) ? observation[i] : 0.0f;
        bridge->lnn_state[i] += dt * (
            -bridge->lnn_state[i] / tc + input
        );
    }
    bridge->lnn_time += dt;

    fin_neural_heartbeat_instance(
        (nimcp_health_agent_t*)bridge->health_agent, "lnn_update", 1.0f);

    return FIN_NEURAL_ERR_OK;
}

//=============================================================================
// Plasticity & Adaptation
//=============================================================================

int financial_neural_bridge_adapt_risk_params(
    financial_neural_bridge_t* bridge,
    float sharpe_ratio,
    float volatility_trend,
    fin_plasticity_params_t* out_params)
{
    if (!bridge) { set_error("NULL bridge"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_neural_bridge_adapt_risk_params: bridge is NULL"); return FIN_NEURAL_ERR_NULL; }
    if (!out_params) { set_error("NULL output"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_neural_bridge_adapt_risk_params: out_params is NULL"); return FIN_NEURAL_ERR_NULL; }

    fin_neural_heartbeat_instance(
        (nimcp_health_agent_t*)bridge->health_agent, "adapt_risk_params", 0.0f);

    bridge->state = FIN_NEURAL_STATE_ADAPTING;

    /* Performance score: normalized Sharpe ratio */
    float performance_score = nimcp_clampf(sharpe_ratio / 3.0f, -1.0f, 1.0f);
    /* Map to [0, 1] for plasticity computation */
    float perf_01 = (performance_score + 1.0f) / 2.0f;

    /* Stability score: 1 - volatility_trend (stable = high) */
    float stability_score = nimcp_clampf(1.0f - volatility_trend, 0.0f, 1.0f);

    /* Plasticity rate = base_rate * (1 + performance * 0.5) * (stability * 0.8 + 0.2) */
    float base_rate = bridge->config.plasticity_base_rate;
    float plasticity_rate = base_rate
        * (1.0f + perf_01 * 0.5f)
        * (stability_score * 0.8f + 0.2f);

    /* Apply health modulation */
    float health_mod = 1.0f
        - bridge->inflammation * bridge->config.inflammation_sensitivity * 0.25f
        - bridge->fatigue * bridge->config.fatigue_sensitivity * 0.2f;
    if (health_mod < 0.1f) health_mod = 0.1f;
    plasticity_rate *= health_mod;

    /* Adapted risk tolerance = base * (1 + plasticity_rate * (performance - 0.5)) */
    float adapted_risk_tolerance = 1.0f * (1.0f + plasticity_rate * (perf_01 - 0.5f));
    adapted_risk_tolerance = nimcp_clampf(adapted_risk_tolerance, 0.2f, 3.0f);

    /* Adapted position size scale = clamp(1.0 + 0.2 * (sharpe - 1.0), 0.5, 2.0) */
    float adapted_position_size = nimcp_clampf(1.0f + 0.2f * (sharpe_ratio - 1.0f), 0.5f, 2.0f);

    /* Adapted stop loss distance = base * (1 + 0.1 * volatility_trend) */
    float adapted_stop_loss = 1.0f * (1.0f + 0.1f * volatility_trend);

    /* Fuzzy adaptation degree: how strongly the system is adapting */
    float fuzzy_adapt = s_shaped(0.005f, 0.05f, plasticity_rate);

    /* Fill output */
    out_params->current_plasticity_rate = plasticity_rate;
    out_params->performance_score = performance_score;
    out_params->stability_score = stability_score;
    out_params->adapted_risk_tolerance = adapted_risk_tolerance;
    out_params->adapted_position_size_scale = adapted_position_size;
    out_params->adapted_stop_loss_distance = adapted_stop_loss;
    out_params->fuzzy_adaptation_degree = fuzzy_adapt;
    out_params->adaptation_epoch = bridge->current_plasticity.adaptation_epoch + 1;

    /* Store in bridge */
    bridge->current_plasticity = *out_params;

    bridge->stats.plasticity_adaptations++;
    bridge->stats.current_plasticity_rate = plasticity_rate;
    bridge->state = FIN_NEURAL_STATE_IDLE;

    fin_neural_heartbeat_instance(
        (nimcp_health_agent_t*)bridge->health_agent, "adapt_risk_params", 1.0f);

    return FIN_NEURAL_ERR_OK;
}

int financial_neural_bridge_get_plasticity(
    const financial_neural_bridge_t* bridge,
    fin_plasticity_params_t* out_params)
{
    if (!bridge) { set_error("NULL bridge"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_neural_bridge_get_plasticity: bridge is NULL"); return FIN_NEURAL_ERR_NULL; }
    if (!out_params) { set_error("NULL output"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_neural_bridge_get_plasticity: out_params is NULL"); return FIN_NEURAL_ERR_NULL; }
    *out_params = bridge->current_plasticity;
    return FIN_NEURAL_ERR_OK;
}

//=============================================================================
// Quantum Optimization (Classical Fallback)
//=============================================================================

int financial_neural_bridge_quantum_optimize(
    financial_neural_bridge_t* bridge,
    const float* expected_returns, uint32_t asset_count,
    const float* covariance_matrix,
    const float* constraints, uint32_t num_constraints,
    fin_quantum_result_t* out_result)
{
    if (!bridge) { set_error("NULL bridge"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_neural_bridge_quantum_optimize: bridge is NULL"); return FIN_NEURAL_ERR_NULL; }
    if (!expected_returns || !out_result) {
        set_error("NULL parameter in quantum_optimize");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_neural_bridge_quantum_optimize: NULL parameter");
        return FIN_NEURAL_ERR_NULL;
    }
    if (asset_count == 0 || asset_count > 256) {
        set_error("Invalid asset count %u", asset_count);
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_INVALID_PARAM, "financial_neural_bridge_quantum_optimize: Invalid asset count %u", asset_count);
        return FIN_NEURAL_ERR_QUANTUM;
    }
    int val_rc = fin_neural_bridge_validate_subsystems("quantum_optimize");
    if (val_rc != FIN_NEURAL_ERR_OK) return val_rc;

    fin_neural_heartbeat_instance(
        (nimcp_health_agent_t*)bridge->health_agent, "quantum_optimize", 0.0f);

    memset(out_result, 0, sizeof(*out_result));
    out_result->asset_count = asset_count;
    out_result->classical_fallback_used = true;
    out_result->qubits_used = 0;
    out_result->quantum_advantage_estimate = 0.0f;

    /* Classical fallback: mean-variance optimization or equal-weight */
    if (covariance_matrix) {
        /* Simple mean-variance: w_i proportional to expected_return / variance */
        float total_weight = 0.0f;
        for (uint32_t i = 0; i < asset_count; i++) {
            /* Diagonal of covariance = variance of asset i */
            float var_i = covariance_matrix[i * asset_count + i];
            if (var_i < 1e-10f) var_i = 1e-10f;
            float w = expected_returns[i] / var_i;
            if (w < 0.0f) w = 0.0f; /* Long-only constraint */
            out_result->optimal_weights[i] = w;
            total_weight += w;
        }
        /* Normalize to sum to 1 */
        if (total_weight > 0.0f) {
            for (uint32_t i = 0; i < asset_count; i++) {
                out_result->optimal_weights[i] /= total_weight;
            }
        } else {
            /* All negative expected returns: equal weight */
            for (uint32_t i = 0; i < asset_count; i++) {
                out_result->optimal_weights[i] = 1.0f / (float)asset_count;
            }
        }
    } else {
        /* Equal-weight portfolio */
        for (uint32_t i = 0; i < asset_count; i++) {
            out_result->optimal_weights[i] = 1.0f / (float)asset_count;
        }
    }

    /* Compute objective value (expected portfolio return) */
    float obj = 0.0f;
    for (uint32_t i = 0; i < asset_count; i++) {
        obj += out_result->optimal_weights[i] * expected_returns[i];
    }
    out_result->objective_value = obj;

    /* Fuzzy constraint satisfaction: simple check */
    if (constraints && num_constraints > 0) {
        /* Placeholder: assume constraints mostly satisfied in classical mode */
        out_result->fuzzy_constraint_satisfaction = 0.85f;
    } else {
        out_result->fuzzy_constraint_satisfaction = 1.0f;
    }

    /* If quantum subsystem is connected, placeholder for future quantum integration */
    if (bridge->quantum && bridge->config.enable_quantum) {
        /* Future: dispatch to quantum hardware/simulator */
        /* For now, still use classical result but note quantum was requested */
        out_result->qubits_used = bridge->config.quantum_max_qubits;
        out_result->quantum_advantage_estimate = 0.0f;
        /* Still classical fallback */
    }

    bridge->stats.quantum_optimizations++;

    fin_neural_heartbeat_instance(
        (nimcp_health_agent_t*)bridge->health_agent, "quantum_optimize", 1.0f);

    return FIN_NEURAL_ERR_OK;
}

//=============================================================================
// Memory Consolidation
//=============================================================================

int financial_neural_bridge_store_pattern(
    financial_neural_bridge_t* bridge,
    const float* pattern, uint32_t dim,
    float outcome, float importance)
{
    if (!bridge) { set_error("NULL bridge"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_neural_bridge_store_pattern: bridge is NULL"); return FIN_NEURAL_ERR_NULL; }
    if (!pattern) { set_error("NULL pattern"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_neural_bridge_store_pattern: pattern is NULL"); return FIN_NEURAL_ERR_NULL; }
    int val_rc = fin_neural_bridge_validate_subsystems("store_pattern");
    if (val_rc != FIN_NEURAL_ERR_OK) return val_rc;

    fin_neural_heartbeat_instance(
        (nimcp_health_agent_t*)bridge->health_agent, "store_pattern", 0.0f);

    uint32_t max_patterns = bridge->config.max_memory_patterns;
    if (max_patterns > FIN_NEURAL_MAX_MEMORY_PATTERNS) {
        max_patterns = FIN_NEURAL_MAX_MEMORY_PATTERNS;
    }

    uint32_t store_dim = (dim < FIN_NEURAL_MAX_LNN_STATE_DIM)
                         ? dim : FIN_NEURAL_MAX_LNN_STATE_DIM;

    /* Find insertion point: if memory full, replace least important pattern */
    uint32_t insert_idx = bridge->memory_count;

    if (bridge->memory_count >= max_patterns) {
        /* Find pattern with lowest importance */
        float min_importance = bridge->memory[0].importance;
        uint32_t min_idx = 0;
        for (uint32_t i = 1; i < bridge->memory_count; i++) {
            if (bridge->memory[i].importance < min_importance) {
                min_importance = bridge->memory[i].importance;
                min_idx = i;
            }
        }
        /* Only replace if new pattern is more important */
        if (importance > min_importance) {
            insert_idx = min_idx;
        } else {
            /* No room and not important enough */
            fin_neural_heartbeat_instance(
                (nimcp_health_agent_t*)bridge->health_agent, "store_pattern", 1.0f);
            return FIN_NEURAL_ERR_OK;
        }
    }

    /* Store the pattern */
    fin_memory_pattern_t* mem = &bridge->memory[insert_idx];
    memset(mem, 0, sizeof(*mem));
    for (uint32_t i = 0; i < store_dim; i++) {
        mem->pattern_vector[i] = pattern[i];
    }
    mem->outcome = outcome;
    mem->importance = importance;
    mem->creation_time_us = bridge->lnn_time > 0.0f
        ? (uint64_t)(bridge->lnn_time * 1e6f) : 0;
    mem->retrieval_count = 0;
    mem->retrieval_strength = 1.0f;

    if (insert_idx == bridge->memory_count && bridge->memory_count < max_patterns) {
        bridge->memory_count++;
    }

    fin_neural_heartbeat_instance(
        (nimcp_health_agent_t*)bridge->health_agent, "store_pattern", 1.0f);

    return FIN_NEURAL_ERR_OK;
}

int financial_neural_bridge_retrieve_patterns(
    financial_neural_bridge_t* bridge,
    const float* query, uint32_t dim,
    fin_memory_pattern_t* out_patterns,
    uint32_t max_patterns, uint32_t* out_count)
{
    if (!bridge) { set_error("NULL bridge"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_neural_bridge_retrieve_patterns: bridge is NULL"); return FIN_NEURAL_ERR_NULL; }
    if (!query || !out_patterns || !out_count) {
        set_error("NULL parameter in retrieve_patterns");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_neural_bridge_retrieve_patterns: NULL parameter");
        return FIN_NEURAL_ERR_NULL;
    }

    fin_neural_heartbeat_instance(
        (nimcp_health_agent_t*)bridge->health_agent, "retrieve_patterns", 0.0f);

    uint32_t retrieve_dim = (dim < FIN_NEURAL_MAX_LNN_STATE_DIM)
                            ? dim : FIN_NEURAL_MAX_LNN_STATE_DIM;

    /* Compute query norm */
    float query_norm = 0.0f;
    for (uint32_t i = 0; i < retrieve_dim; i++) {
        query_norm += query[i] * query[i];
    }
    query_norm = sqrtf(query_norm);
    if (query_norm < 1e-10f) query_norm = 1e-10f;

    /* Compute cosine similarity for each stored pattern */
    typedef struct {
        uint32_t idx;
        float similarity;
    } sim_entry_t;

    /* Use stack allocation for similarity scores */
    sim_entry_t sims[FIN_NEURAL_MAX_MEMORY_PATTERNS];
    uint32_t n = bridge->memory_count;

    for (uint32_t m = 0; m < n; m++) {
        float dot = 0.0f;
        float pat_norm = 0.0f;
        for (uint32_t i = 0; i < retrieve_dim; i++) {
            dot += query[i] * bridge->memory[m].pattern_vector[i];
            pat_norm += bridge->memory[m].pattern_vector[i] *
                        bridge->memory[m].pattern_vector[i];
        }
        pat_norm = sqrtf(pat_norm);
        if (pat_norm < 1e-10f) pat_norm = 1e-10f;

        sims[m].idx = m;
        sims[m].similarity = dot / (query_norm * pat_norm);
    }

    /* Simple selection sort to find top-k by similarity (descending) */
    for (uint32_t i = 0; i < n && i < max_patterns; i++) {
        uint32_t best = i;
        for (uint32_t j = i + 1; j < n; j++) {
            if (sims[j].similarity > sims[best].similarity) {
                best = j;
            }
        }
        if (best != i) {
            sim_entry_t tmp = sims[i];
            sims[i] = sims[best];
            sims[best] = tmp;
        }
    }

    /* Copy top-k patterns to output */
    uint32_t count = (n < max_patterns) ? n : max_patterns;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t idx = sims[i].idx;
        out_patterns[i] = bridge->memory[idx];
        /* Update retrieval count */
        bridge->memory[idx].retrieval_count++;
    }
    *out_count = count;

    bridge->stats.memory_retrievals++;

    fin_neural_heartbeat_instance(
        (nimcp_health_agent_t*)bridge->health_agent, "retrieve_patterns", 1.0f);

    return FIN_NEURAL_ERR_OK;
}

int financial_neural_bridge_consolidate(
    financial_neural_bridge_t* bridge)
{
    if (!bridge) { set_error("NULL bridge"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_neural_bridge_consolidate: bridge is NULL"); return FIN_NEURAL_ERR_NULL; }

    fin_neural_heartbeat_instance(
        (nimcp_health_agent_t*)bridge->health_agent, "consolidate", 0.0f);

    float threshold = bridge->config.consolidation_threshold;
    uint32_t n = bridge->memory_count;

    /* Remove low-importance patterns below threshold */
    uint32_t write_idx = 0;
    for (uint32_t i = 0; i < n; i++) {
        float effective_importance = bridge->memory[i].importance *
                                    bridge->memory[i].retrieval_strength;
        if (effective_importance >= threshold || bridge->memory[i].retrieval_count > 0) {
            /* Keep this pattern */
            if (write_idx != i) {
                bridge->memory[write_idx] = bridge->memory[i];
            }

            /* Boost retrieval strength of frequently-retrieved patterns */
            if (bridge->memory[write_idx].retrieval_count > 0) {
                bridge->memory[write_idx].retrieval_strength +=
                    0.1f * (float)bridge->memory[write_idx].retrieval_count;
                if (bridge->memory[write_idx].retrieval_strength > 5.0f) {
                    bridge->memory[write_idx].retrieval_strength = 5.0f;
                }
                /* Reset retrieval count after consolidation */
                bridge->memory[write_idx].retrieval_count = 0;
            } else {
                /* Decay retrieval strength for unretrieved patterns */
                bridge->memory[write_idx].retrieval_strength *= 0.95f;
            }

            write_idx++;
        }
    }

    bridge->memory_count = write_idx;
    bridge->stats.memory_consolidations++;

    fin_neural_heartbeat_instance(
        (nimcp_health_agent_t*)bridge->health_agent, "consolidate", 1.0f);

    return FIN_NEURAL_ERR_OK;
}

//=============================================================================
// Training
//=============================================================================

int financial_neural_bridge_train_step(
    financial_neural_bridge_t* bridge,
    const float* input, const float* target,
    uint32_t dim, float learning_rate)
{
    if (!bridge) { set_error("NULL bridge"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_neural_bridge_train_step: bridge is NULL"); return FIN_NEURAL_ERR_NULL; }
    if (!input || !target) {
        set_error("NULL input or target");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_neural_bridge_train_step: NULL input or target");
        return FIN_NEURAL_ERR_NULL;
    }

    /* Instance-level security validation */
    int val_rc = neural_validate_subsystems(bridge, "train_step");
    if (val_rc != FIN_NEURAL_ERR_OK) return val_rc;

    fin_neural_heartbeat_instance(
        (nimcp_health_agent_t*)bridge->health_agent, "train_step", 0.0f);

    bridge->state = FIN_NEURAL_STATE_LEARNING;

    uint32_t train_dim = (dim < bridge->lnn_state_dim) ? dim : bridge->lnn_state_dim;
    float lr = learning_rate;
    if (lr <= 0.0f) lr = bridge->config.stdp_learning_rate;

    /* Apply health modulation to learning rate */
    float health_mod = 1.0f
        - bridge->inflammation * bridge->config.inflammation_sensitivity * 0.2f
        - bridge->fatigue * bridge->config.fatigue_sensitivity * 0.15f;
    if (health_mod < 0.1f) health_mod = 0.1f;
    lr *= health_mod;

    /* Save previous loss for divergence detection */
    float prev_loss = bridge->current_loss;

    /* Simple gradient descent on LNN state:
     * For each dim: predicted = state[i], target = target[i]
     * loss_i = (target[i] - state[i])^2
     * grad_i = -2 * (target[i] - state[i])
     * state[i] -= lr * grad_i = state[i] + lr * 2 * (target[i] - state[i]) */

    float total_loss = 0.0f;
    float max_gradient = 0.0f;
    for (uint32_t i = 0; i < train_dim; i++) {
        /* Feed input to state first */
        float tc = bridge->config.lnn_time_constant;
        if (tc < 1e-6f) tc = 0.1f;
        float dt = 0.01f;
        bridge->lnn_state[i] += dt * (-bridge->lnn_state[i] / tc + input[i]);

        /* Compute loss */
        float err = target[i] - bridge->lnn_state[i];
        total_loss += err * err;

        /* Track gradient magnitude for explosion detection */
        float grad = 2.0f * fabsf(err);
        if (grad > max_gradient) max_gradient = grad;

        /* Gradient update */
        bridge->lnn_state[i] += lr * 2.0f * err;
    }

    /* Check for gradient explosion (threshold: gradient > 1000) */
    if (max_gradient > 1000.0f) {
        neural_present_antigen(bridge, "gradient_explosion", 4);
    }

    /* Update training state */
    bridge->training_epoch++;
    bridge->current_loss = total_loss / (float)(train_dim > 0 ? train_dim : 1);

    /* Check for training divergence (loss increasing by > 50%) */
    if (bridge->training_epoch > 1 && prev_loss > 0.0f) {
        float loss_ratio = bridge->current_loss / prev_loss;
        if (loss_ratio > 1.5f) {
            neural_present_antigen(bridge, "training_divergence", 3);
        }
    }

    /* Update convergence degree: how close to converged
     * convergence = 1 - loss (clamped, with smoothing) */
    float target_conv = nimcp_clampf(1.0f - bridge->current_loss, 0.0f, 1.0f);
    /* Exponential moving average for smoothing */
    float alpha = 0.1f;
    bridge->convergence_degree =
        bridge->convergence_degree * (1.0f - alpha) + target_conv * alpha;

    /* Update avg prediction error */
    float n_total = (float)bridge->stats.predictions_made +
                    (float)bridge->training_epoch;
    if (n_total > 0.0f) {
        bridge->stats.avg_prediction_error =
            bridge->stats.avg_prediction_error * ((n_total - 1.0f) / n_total) +
            bridge->current_loss * (1.0f / n_total);
    }

    bridge->state = FIN_NEURAL_STATE_IDLE;

    fin_neural_heartbeat_instance(
        (nimcp_health_agent_t*)bridge->health_agent, "train_step", 1.0f);

    return FIN_NEURAL_ERR_OK;
}

int financial_neural_bridge_get_convergence(
    const financial_neural_bridge_t* bridge,
    float* out_loss, float* out_convergence_degree)
{
    if (!bridge) { set_error("NULL bridge"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_neural_bridge_get_convergence: bridge is NULL"); return FIN_NEURAL_ERR_NULL; }
    if (!out_loss || !out_convergence_degree) {
        set_error("NULL output in get_convergence");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_neural_bridge_get_convergence: NULL output");
        return FIN_NEURAL_ERR_NULL;
    }

    *out_loss = bridge->current_loss;
    *out_convergence_degree = bridge->convergence_degree;
    return FIN_NEURAL_ERR_OK;
}

//=============================================================================
// Health & Modulation
//=============================================================================

int financial_neural_bridge_set_inflammation(financial_neural_bridge_t* bridge,
                                              float level)
{
    if (!bridge) { set_error("NULL bridge"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_neural_bridge_set_inflammation: bridge is NULL"); return FIN_NEURAL_ERR_NULL; }
    bridge->inflammation = nimcp_clampf(level, 0.0f, 1.0f);
    return FIN_NEURAL_ERR_OK;
}

int financial_neural_bridge_set_fatigue(financial_neural_bridge_t* bridge,
                                         float level)
{
    if (!bridge) { set_error("NULL bridge"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_neural_bridge_set_fatigue: bridge is NULL"); return FIN_NEURAL_ERR_NULL; }
    bridge->fatigue = nimcp_clampf(level, 0.0f, 1.0f);
    return FIN_NEURAL_ERR_OK;
}

int financial_neural_bridge_get_stats(const financial_neural_bridge_t* bridge,
                                       fin_neural_stats_t* stats)
{
    if (!bridge) { set_error("NULL bridge"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_neural_bridge_get_stats: bridge is NULL"); return FIN_NEURAL_ERR_NULL; }
    if (!stats) { set_error("NULL stats output"); NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "financial_neural_bridge_get_stats: stats is NULL"); return FIN_NEURAL_ERR_NULL; }
    *stats = bridge->stats;
    return FIN_NEURAL_ERR_OK;
}

void financial_neural_bridge_reset_stats(financial_neural_bridge_t* bridge) {
    if (!bridge) return;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
}

const char* financial_neural_bridge_get_last_error(void) {
    return fin_neural_last_error;
}

//=============================================================================
// Logger Setter (Phase 8: Change Set 2/3)
//=============================================================================

int financial_neural_bridge_set_logger(financial_neural_bridge_t* bridge, void* logger) {
    if (!bridge) {
        set_error("set_logger: NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_neural_bridge_set_logger: NULL bridge");
        return FIN_NEURAL_ERR_NULL;
    }
    bridge->logger = logger;
    return FIN_NEURAL_ERR_OK;
}
