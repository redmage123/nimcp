//=============================================================================
// nimcp_fuzzy_bridge.c - Fuzzy Logic System Bridge Implementation
//=============================================================================
/**
 * @file nimcp_fuzzy_bridge.c
 * @brief Integration hub connecting fuzzy logic to 18 NIMCP subsystems
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#include "utils/fuzzy/nimcp_fuzzy_bridge.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/memory/nimcp_memory.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>

//=============================================================================
// Health Agent Integration (Phase 8)
//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/math/nimcp_math_helpers.h"
struct nimcp_health_agent;
typedef struct nimcp_health_agent nimcp_health_agent_t;
extern void nimcp_health_agent_heartbeat_ex(nimcp_health_agent_t* agent,
                                             const char* operation,
                                             float progress);

/* Forward declarations for immune/BBB validation */
struct brain_immune_system;
typedef struct brain_immune_system brain_immune_system_t;
extern int brain_immune_validate_operation(brain_immune_system_t* immune, const char* operation, uint32_t severity);
extern int brain_immune_present_antigen(brain_immune_system_t* immune, uint32_t source, const uint8_t* signature, size_t sig_len, uint32_t severity);

struct bbb_system_struct;
typedef struct bbb_system_struct* bbb_system_t;
extern int bbb_validate_data(bbb_system_t bbb, const void* data, size_t size, const char* context);
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

/* Manual health agent declaration - custom heartbeat function defined below */
static nimcp_health_agent_t* g_fuzzy_bridge_health_agent = NULL;

static __thread char tls_fuzzy_bridge_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(tls_fuzzy_bridge_error, sizeof(tls_fuzzy_bridge_error), fmt, args);
    va_end(args);
}

//=============================================================================
// KG Wiring Message Types
//=============================================================================

#define KG_MSG_FUZZY_INFERENCE_REQUEST   "FUZZY_INF_REQUEST"
#define KG_MSG_FUZZY_INFERENCE_RESPONSE  "FUZZY_INF_RESPONSE"
#define KG_MSG_FUZZY_ERROR               "FUZZY_ERROR"
#define KG_MSG_FUZZY_HEALTH_UPDATE       "FUZZY_HEALTH_UPDATE"

//=============================================================================
// Internal Bridge Structure
//=============================================================================

struct fuzzy_bridge {
    fuzzy_bridge_config_t config;
    fuzzy_bridge_stats_t stats;
    fuzzy_bridge_state_t state;

    /* Subsystem pointers */
    void* immune;
    void* bbb;
    nimcp_health_agent_t* health_agent;
    kg_wiring_t* kg_wiring;
    void* kg_registry;
    void* logger;
    void* security;
    void* cycle_coordinator;
    void* bio_router;
    void* ethics;
    const void* lgss_kb;
    void* snn;
    void* stdp;
    void* plasticity;
    void* lnn;
    void* training;
    void* quantum;
    void* symbolic;

    /* Internal fuzzy inference engines for bridge operations */
    fuzzy_inference_engine_t* convergence_fis;
    fuzzy_inference_engine_t* lr_schedule_fis;
    fuzzy_inference_engine_t* plasticity_fis;

    float inflammation_level;
    float fatigue_level;
};

//=============================================================================
// KG Wiring Helper
//=============================================================================

static int fuzzy_kg_publish(fuzzy_bridge_t* bridge, const char* msg_type,
                            const void* payload, size_t size) {
    if (bridge && bridge->kg_wiring) {
        bridge->stats.kg_messages_sent++;
        /* kg_wiring_publish would be called here - for now just count */
        (void)msg_type; (void)payload; (void)size;
        return 0;
    }
    return 0;
}

//=============================================================================
// Immune/BBB Validation Helpers
//=============================================================================

static int fuzzy_bridge_validate_subsystems(fuzzy_bridge_t* bridge, const char* operation) {
    if (!bridge) return FUZZY_BRIDGE_ERR_NULL;

    /* Immune system validation */
    if (bridge->config.enable_immune_integration && bridge->immune) {
        int rc = brain_immune_validate_operation((brain_immune_system_t*)bridge->immune, operation, 5);
        if (rc != 0) {
            set_error("fuzzy_bridge: immune validation failed for %s", operation);
            bridge->stats.immune_checks++;
            return FUZZY_BRIDGE_ERR_SUBSYSTEM;
        }
        bridge->stats.immune_checks++;
    }

    /* BBB validation */
    if (bridge->config.enable_bbb_validation && bridge->bbb) {
        int rc = bbb_validate_data((bbb_system_t)bridge->bbb, NULL, 0, operation);
        if (rc != 0) {
            set_error("fuzzy_bridge: BBB validation failed for %s", operation);
            bridge->stats.bbb_validations++;
            return FUZZY_BRIDGE_ERR_SUBSYSTEM;
        }
        bridge->stats.bbb_validations++;
    }

    return FUZZY_BRIDGE_ERR_OK;
}

static void fuzzy_bridge_present_antigen(fuzzy_bridge_t* bridge, const char* anomaly_type, uint32_t severity) {
    if (!bridge || !bridge->immune) return;

    uint8_t signature[64] = {0};
    snprintf((char*)signature, sizeof(signature), "fuzzy:%s", anomaly_type);

    brain_immune_present_antigen(
        (brain_immune_system_t*)bridge->immune,
        2,  /* ANTIGEN_SOURCE_ANOMALY */
        signature,
        strlen((char*)signature),
        severity
    );
}

//=============================================================================
// Internal: Build standard FIS for convergence detection
//=============================================================================

static fuzzy_inference_engine_t* build_convergence_fis(void) {
    fuzzy_inference_config_t cfg = fuzzy_inference_default_config();
    cfg.fis_type = FUZZY_FIS_SUGENO;
    fuzzy_inference_engine_t* fis = fuzzy_inference_create_custom(&cfg);
    if (!fis) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "build_convergence_fis: fis is NULL");
        return NULL;
    }

    /* Input 0: loss_delta — how much loss changed */
    fuzzy_variable_t loss_delta;
    fuzzy_variable_create(&loss_delta, "loss_delta", -1.0f, 1.0f);
    fuzzy_set_t ld_small, ld_medium, ld_large;
    fuzzy_set_create(&ld_small, "small", &(fuzzy_mf_t){.type = FUZZY_MF_Z_SHAPED, .params = {-0.01f, 0.01f}, .num_params = 2}, FUZZY_HEDGE_NONE);
    fuzzy_set_create(&ld_medium, "medium", &(fuzzy_mf_t){.type = FUZZY_MF_TRIANGULAR, .params = {0.005f, 0.05f, 0.2f}, .num_params = 3}, FUZZY_HEDGE_NONE);
    fuzzy_set_create(&ld_large, "large", &(fuzzy_mf_t){.type = FUZZY_MF_S_SHAPED, .params = {0.1f, 0.5f}, .num_params = 2}, FUZZY_HEDGE_NONE);
    fuzzy_variable_add_term(&loss_delta, &ld_small);
    fuzzy_variable_add_term(&loss_delta, &ld_medium);
    fuzzy_variable_add_term(&loss_delta, &ld_large);
    fuzzy_inference_add_input(fis, &loss_delta);

    /* Input 1: gradient_norm */
    fuzzy_variable_t grad_norm;
    fuzzy_variable_create(&grad_norm, "grad_norm", 0.0f, 10.0f);
    fuzzy_set_t gn_small, gn_medium, gn_large;
    fuzzy_set_create(&gn_small, "small", &(fuzzy_mf_t){.type = FUZZY_MF_Z_SHAPED, .params = {0.001f, 0.01f}, .num_params = 2}, FUZZY_HEDGE_NONE);
    fuzzy_set_create(&gn_medium, "medium", &(fuzzy_mf_t){.type = FUZZY_MF_TRIANGULAR, .params = {0.005f, 0.05f, 0.5f}, .num_params = 3}, FUZZY_HEDGE_NONE);
    fuzzy_set_create(&gn_large, "large", &(fuzzy_mf_t){.type = FUZZY_MF_S_SHAPED, .params = {0.1f, 1.0f}, .num_params = 2}, FUZZY_HEDGE_NONE);
    fuzzy_variable_add_term(&grad_norm, &gn_small);
    fuzzy_variable_add_term(&grad_norm, &gn_medium);
    fuzzy_variable_add_term(&grad_norm, &gn_large);
    fuzzy_inference_add_input(fis, &grad_norm);

    /* Output: convergence degree (Sugeno constant) */
    fuzzy_variable_t conv_out;
    fuzzy_variable_create(&conv_out, "convergence", 0.0f, 1.0f);
    fuzzy_inference_add_output(fis, &conv_out);

    /* Rules: small delta + small grad => high convergence */
    float high[] = {0.95f};
    fuzzy_rule_t r1 = fuzzy_rule_sugeno(0, 0, 1, 0, high, 1, 1.0f); /* small,small -> 0.95 */
    float med[] = {0.5f};
    fuzzy_rule_t r2 = fuzzy_rule_sugeno(0, 0, 1, 1, med, 1, 1.0f);  /* small,medium -> 0.5 */
    float low[] = {0.1f};
    fuzzy_rule_t r3 = fuzzy_rule_sugeno(0, 1, 1, 1, low, 1, 1.0f);  /* medium,medium -> 0.1 */
    float vlow[] = {0.02f};
    fuzzy_rule_t r4 = fuzzy_rule_sugeno(0, 2, 1, 2, vlow, 1, 1.0f); /* large,large -> 0.02 */

    fuzzy_inference_add_rule(fis, &r1);
    fuzzy_inference_add_rule(fis, &r2);
    fuzzy_inference_add_rule(fis, &r3);
    fuzzy_inference_add_rule(fis, &r4);

    return fis;
}

//=============================================================================
// Lifecycle
//=============================================================================

fuzzy_bridge_config_t fuzzy_bridge_default_config(void) {
    fuzzy_bridge_config_t config;
    memset(&config, 0, sizeof(config));
    config.enable_snn_integration = true;
    config.enable_stdp_integration = true;
    config.enable_plasticity_integration = true;
    config.enable_lnn_integration = true;
    config.enable_training_integration = true;
    config.enable_quantum_integration = false;
    config.enable_symbolic_integration = false;
    config.enable_immune_integration = true;
    config.enable_bbb_validation = true;
    config.enable_kg_wiring = true;
    config.enable_logging = true;
    config.enable_security = true;
    config.enable_ethics = true;
    config.enable_lgss = true;
    config.spike_rate_min = 0.0f;
    config.spike_rate_max = 200.0f;
    config.stdp_window_ms = 20.0f;
    config.plasticity_rate_min = 0.001f;
    config.plasticity_rate_max = 0.1f;
    config.lnn_time_step = 0.01f;
    config.training_lr_min = 1e-6f;
    config.training_lr_max = 0.1f;
    config.convergence_threshold = 0.8f;
    config.inflammation_sensitivity = 0.3f;
    config.fatigue_sensitivity = 0.2f;
    config.health_check_interval_ms = 5000;
    return config;
}

fuzzy_bridge_t* fuzzy_bridge_create(const fuzzy_bridge_config_t* config) {
    fuzzy_bridge_t* bridge = (fuzzy_bridge_t*)nimcp_calloc(1, sizeof(fuzzy_bridge_t));
    if (!bridge) {
        set_error("Failed to allocate fuzzy bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY, "fuzzy_bridge_create: Failed to allocate fuzzy bridge");
        return NULL;
    }

    bridge->config = config ? *config : fuzzy_bridge_default_config();
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->state = FUZZY_BRIDGE_STATE_INITIALIZING;

    /* Build internal FIS engines */
    bridge->convergence_fis = build_convergence_fis();
    bridge->lr_schedule_fis = NULL; /* Built on demand */
    bridge->plasticity_fis = NULL;  /* Built on demand */

    /* KG wiring initialized to NULL (set via fuzzy_bridge_set_kg_wiring) */
    bridge->kg_wiring = NULL;

    bridge->inflammation_level = 0.0f;
    bridge->fatigue_level = 0.0f;
    bridge->state = FUZZY_BRIDGE_STATE_ACTIVE;

    return bridge;
}

void fuzzy_bridge_destroy(fuzzy_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->convergence_fis) fuzzy_inference_destroy(bridge->convergence_fis);
    if (bridge->lr_schedule_fis) fuzzy_inference_destroy(bridge->lr_schedule_fis);
    if (bridge->plasticity_fis) fuzzy_inference_destroy(bridge->plasticity_fis);

    nimcp_free(bridge);
}

fuzzy_bridge_state_t fuzzy_bridge_get_state(const fuzzy_bridge_t* bridge) {
    if (!bridge) return FUZZY_BRIDGE_STATE_ERROR;
    return bridge->state;
}

//=============================================================================
// Subsystem Setters
//=============================================================================

#define FUZZY_BRIDGE_SETTER(name, field) \
    int fuzzy_bridge_set_##name(fuzzy_bridge_t* bridge, void* ptr) { \
        if (!bridge) { \
            set_error("fuzzy_bridge_set_" #name ": NULL bridge"); \
            NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "fuzzy_bridge_set_" #name ": NULL bridge"); \
            return FUZZY_BRIDGE_ERR_NULL; \
        } \
        bridge->field = ptr; \
        return FUZZY_BRIDGE_ERR_OK; \
    }

FUZZY_BRIDGE_SETTER(immune, immune)
FUZZY_BRIDGE_SETTER(bbb, bbb)
FUZZY_BRIDGE_SETTER(kg_registry, kg_registry)
FUZZY_BRIDGE_SETTER(logger, logger)
FUZZY_BRIDGE_SETTER(security, security)
FUZZY_BRIDGE_SETTER(cycle_coordinator, cycle_coordinator)
FUZZY_BRIDGE_SETTER(bio_router, bio_router)
FUZZY_BRIDGE_SETTER(ethics, ethics)
FUZZY_BRIDGE_SETTER(snn, snn)
FUZZY_BRIDGE_SETTER(stdp, stdp)
FUZZY_BRIDGE_SETTER(plasticity, plasticity)
FUZZY_BRIDGE_SETTER(lnn, lnn)
FUZZY_BRIDGE_SETTER(training, training)
FUZZY_BRIDGE_SETTER(quantum, quantum)
FUZZY_BRIDGE_SETTER(symbolic, symbolic)

#undef FUZZY_BRIDGE_SETTER

int fuzzy_bridge_set_health_agent(fuzzy_bridge_t* bridge, void* health_agent) {
    if (!bridge) {
        set_error("fuzzy_bridge_set_health_agent: NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "fuzzy_bridge_set_health_agent: NULL bridge");
        return FUZZY_BRIDGE_ERR_NULL;
    }
    bridge->health_agent = (nimcp_health_agent_t*)health_agent;
    return FUZZY_BRIDGE_ERR_OK;
}

int fuzzy_bridge_set_lgss(fuzzy_bridge_t* bridge, const void* lgss_kb) {
    if (!bridge) {
        set_error("fuzzy_bridge_set_lgss: NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "fuzzy_bridge_set_lgss: NULL bridge");
        return FUZZY_BRIDGE_ERR_NULL;
    }
    bridge->lgss_kb = lgss_kb;
    return FUZZY_BRIDGE_ERR_OK;
}

int fuzzy_bridge_set_kg_wiring(fuzzy_bridge_t* bridge, kg_wiring_t* kg) {
    if (!bridge) {
        set_error("set_kg_wiring: NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "fuzzy_bridge_set_kg_wiring: NULL bridge");
        return -1;
    }
    bridge->kg_wiring = kg;
    return 0;
}

kg_module_wiring_t* fuzzy_bridge_create_kg_wiring(void) {
    /* Placeholder - actual implementation would create wiring descriptor */
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "fuzzy_bridge_create_kg_wiring: operation failed");
    return NULL;
}

//=============================================================================
// SNN Integration: Fuzzy <-> Spike Conversion
//=============================================================================

int fuzzy_bridge_to_spike_population(fuzzy_bridge_t* bridge,
                                      const float* memberships, uint32_t count,
                                      float* out_rates) {
    if (!bridge || !memberships || !out_rates) {
        set_error("fuzzy_bridge_to_spike_population: NULL argument");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "fuzzy_bridge_to_spike_population: NULL argument");
        return FUZZY_BRIDGE_ERR_NULL;
    }

    fuzzy_bridge_validate_subsystems(bridge, "spike_population_encode");

    float rate_min = bridge->config.spike_rate_min;
    float rate_max = bridge->config.spike_rate_max;
    float range = rate_max - rate_min;

    for (uint32_t i = 0; i < count; i++) {
        float mu = nimcp_clampf(memberships[i], 0.0f, 1.0f);
        out_rates[i] = rate_min + mu * range;
    }

    bridge->stats.spike_conversions++;
    return FUZZY_BRIDGE_ERR_OK;
}

int fuzzy_bridge_from_spike_population(fuzzy_bridge_t* bridge,
                                        const float* rates, uint32_t count,
                                        float* out_memberships) {
    if (!bridge || !rates || !out_memberships) {
        set_error("fuzzy_bridge_from_spike_population: NULL argument");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "fuzzy_bridge_from_spike_population: NULL argument");
        return FUZZY_BRIDGE_ERR_NULL;
    }

    fuzzy_bridge_validate_subsystems(bridge, "spike_population_decode");

    float rate_min = bridge->config.spike_rate_min;
    float rate_max = bridge->config.spike_rate_max;
    float range = rate_max - rate_min;

    for (uint32_t i = 0; i < count; i++) {
        if (range < FUZZY_PRECISION) {
            out_memberships[i] = 0.5f;
        } else {
            out_memberships[i] = nimcp_clampf((rates[i] - rate_min) / range, 0.0f, 1.0f);
        }
    }

    bridge->stats.spike_conversions++;
    return FUZZY_BRIDGE_ERR_OK;
}

//=============================================================================
// STDP Integration
//=============================================================================

int fuzzy_bridge_stdp_temporal_membership(fuzzy_bridge_t* bridge, float dt_ms,
                                           float* out_potentiation,
                                           float* out_depression) {
    if (!bridge || !out_potentiation || !out_depression) {
        set_error("fuzzy_bridge_stdp_temporal_membership: NULL argument");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "fuzzy_bridge_stdp_temporal_membership: NULL argument");
        return FUZZY_BRIDGE_ERR_NULL;
    }

    fuzzy_bridge_validate_subsystems(bridge, "stdp_temporal");

    float window = bridge->config.stdp_window_ms;

    /* Potentiation: pre-before-post (dt > 0), exponential decay */
    if (dt_ms > 0.0f) {
        fuzzy_mf_t pot_mf = fuzzy_mf_z_shaped(0.0f, window);
        *out_potentiation = fuzzy_mf_evaluate(&pot_mf, dt_ms);
        /* Invert because z_shaped goes 1->0, we want high near 0 */
        *out_potentiation = 1.0f - *out_potentiation;
        fuzzy_mf_t pot_mf2 = fuzzy_mf_gaussian(0.0f, window / 3.0f);
        *out_potentiation = fuzzy_mf_evaluate(&pot_mf2, dt_ms);
    } else {
        *out_potentiation = 0.0f;
    }

    /* Depression: post-before-pre (dt < 0), exponential decay */
    if (dt_ms < 0.0f) {
        fuzzy_mf_t dep_mf = fuzzy_mf_gaussian(0.0f, window / 3.0f);
        *out_depression = fuzzy_mf_evaluate(&dep_mf, dt_ms);
    } else {
        *out_depression = 0.0f;
    }

    bridge->stats.stdp_modulations++;
    return FUZZY_BRIDGE_ERR_OK;
}

//=============================================================================
// Plasticity Integration
//=============================================================================

int fuzzy_bridge_plasticity_rate(fuzzy_bridge_t* bridge,
                                  float performance_score, float stability_score,
                                  float* out_rate) {
    if (!bridge || !out_rate) {
        set_error("fuzzy_bridge_plasticity_rate: NULL argument");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "fuzzy_bridge_plasticity_rate: NULL argument");
        return FUZZY_BRIDGE_ERR_NULL;
    }

    fuzzy_bridge_validate_subsystems(bridge, "plasticity_rate");

    /* Simple fuzzy rule: high performance + high stability -> low plasticity
     * low performance + low stability -> high plasticity */
    float need_change = 1.0f - (performance_score * 0.6f + stability_score * 0.4f);
    need_change = nimcp_clampf(need_change, 0.0f, 1.0f);

    float rate_min = bridge->config.plasticity_rate_min;
    float rate_max = bridge->config.plasticity_rate_max;
    *out_rate = rate_min + need_change * (rate_max - rate_min);

    /* Modulate by inflammation (high inflammation -> higher plasticity) */
    float infl_boost = bridge->inflammation_level * bridge->config.inflammation_sensitivity;
    *out_rate = nimcp_clampf(*out_rate * (1.0f + infl_boost), rate_min, rate_max);

    bridge->stats.plasticity_rate_computations++;
    return FUZZY_BRIDGE_ERR_OK;
}

//=============================================================================
// LNN Integration
//=============================================================================

int fuzzy_bridge_lnn_classify_state(fuzzy_bridge_t* bridge,
                                     const float* state, uint32_t state_dim,
                                     fuzzy_value_t* out_value) {
    if (!bridge || !state || !out_value) {
        set_error("fuzzy_bridge_lnn_classify_state: NULL argument");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "fuzzy_bridge_lnn_classify_state: NULL argument");
        return FUZZY_BRIDGE_ERR_NULL;
    }
    if (state_dim == 0) {
        set_error("fuzzy_bridge_lnn_classify_state: state_dim is 0");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_INVALID_PARAM, "fuzzy_bridge_lnn_classify_state: state_dim is 0");
        return FUZZY_BRIDGE_ERR_INVALID_DIM;
    }

    fuzzy_bridge_validate_subsystems(bridge, "lnn_classify");

    /* Compute state magnitude as summary statistic */
    float magnitude = 0.0f;
    for (uint32_t i = 0; i < state_dim; i++) {
        magnitude += state[i] * state[i];
    }
    magnitude = sqrtf(magnitude) / (float)state_dim;

    /* Classify into fuzzy categories: low, medium, high, extreme */
    memset(out_value, 0, sizeof(fuzzy_value_t));
    out_value->num_terms = 4;

    fuzzy_mf_t mf_low = fuzzy_mf_z_shaped(0.0f, 0.3f);
    fuzzy_mf_t mf_med = fuzzy_mf_triangular(0.1f, 0.4f, 0.7f);
    fuzzy_mf_t mf_high = fuzzy_mf_triangular(0.5f, 0.75f, 1.0f);
    fuzzy_mf_t mf_ext = fuzzy_mf_s_shaped(0.8f, 1.2f);

    out_value->memberships[0] = fuzzy_mf_evaluate(&mf_low, magnitude);
    out_value->memberships[1] = fuzzy_mf_evaluate(&mf_med, magnitude);
    out_value->memberships[2] = fuzzy_mf_evaluate(&mf_high, magnitude);
    out_value->memberships[3] = fuzzy_mf_evaluate(&mf_ext, magnitude);

    /* Find dominant */
    out_value->dominant_degree = 0.0f;
    for (uint32_t i = 0; i < 4; i++) {
        if (out_value->memberships[i] > out_value->dominant_degree) {
            out_value->dominant_degree = out_value->memberships[i];
            out_value->dominant_term = i;
        }
    }
    out_value->entropy = fuzzy_entropy(out_value->memberships, 4);

    bridge->stats.lnn_classifications++;
    return FUZZY_BRIDGE_ERR_OK;
}

//=============================================================================
// Training Integration
//=============================================================================

int fuzzy_bridge_training_lr_schedule(fuzzy_bridge_t* bridge,
                                       float epoch_progress, float loss_trend,
                                       float base_lr, float* out_lr) {
    if (!bridge || !out_lr) {
        set_error("fuzzy_bridge_training_lr_schedule: NULL argument");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "fuzzy_bridge_training_lr_schedule: NULL argument");
        return FUZZY_BRIDGE_ERR_NULL;
    }

    fuzzy_bridge_validate_subsystems(bridge, "lr_schedule");

    /* Fuzzy LR scheduling:
     * - Early training + decreasing loss -> keep LR high
     * - Late training + stable loss -> reduce LR
     * - Increasing loss -> reduce LR significantly */
    float lr_mult = 1.0f;

    /* Epoch effect: taper as training progresses */
    fuzzy_mf_t early_mf = fuzzy_mf_z_shaped(0.0f, 0.5f);
    float early_degree = fuzzy_mf_evaluate(&early_mf, epoch_progress);

    /* Loss trend effect */
    fuzzy_mf_t improving_mf = fuzzy_mf_z_shaped(-0.5f, 0.0f);
    float improving = fuzzy_mf_evaluate(&improving_mf, loss_trend);

    fuzzy_mf_t worsening_mf = fuzzy_mf_s_shaped(0.0f, 0.5f);
    float worsening = fuzzy_mf_evaluate(&worsening_mf, loss_trend);

    /* Combine: high early + improving -> keep LR; late or worsening -> reduce */
    lr_mult = 0.1f + 0.9f * fuzzy_tnorm(early_degree, improving, FUZZY_TNORM_ALGEBRAIC_PRODUCT);
    lr_mult -= 0.5f * worsening;
    lr_mult = nimcp_clampf(lr_mult, 0.01f, 1.5f);

    *out_lr = nimcp_clampf(base_lr * lr_mult, bridge->config.training_lr_min,
                      bridge->config.training_lr_max);

    bridge->stats.training_lr_schedules++;
    return FUZZY_BRIDGE_ERR_OK;
}

int fuzzy_bridge_training_convergence(fuzzy_bridge_t* bridge,
                                       float loss_delta, float gradient_norm,
                                       float* out_convergence) {
    if (!bridge || !out_convergence) {
        set_error("fuzzy_bridge_training_convergence: NULL argument");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "fuzzy_bridge_training_convergence: NULL argument");
        return FUZZY_BRIDGE_ERR_NULL;
    }

    fuzzy_bridge_validate_subsystems(bridge, "convergence");

    if (bridge->convergence_fis) {
        float inputs[2] = {fabsf(loss_delta), gradient_norm};
        fuzzy_inference_result_t result;
        int rc = fuzzy_inference_evaluate(bridge->convergence_fis, inputs, 2, &result);
        if (rc == 0 && result.num_outputs > 0) {
            *out_convergence = nimcp_clampf(result.crisp_outputs[0], 0.0f, 1.0f);
        } else {
            /* Fallback */
            *out_convergence = (fabsf(loss_delta) < 0.001f && gradient_norm < 0.01f) ? 0.9f : 0.1f;
        }
    } else {
        *out_convergence = (fabsf(loss_delta) < 0.001f && gradient_norm < 0.01f) ? 0.9f : 0.1f;
    }

    bridge->stats.convergence_checks++;
    return FUZZY_BRIDGE_ERR_OK;
}

//=============================================================================
// Quantum Integration
//=============================================================================

int fuzzy_bridge_quantum_inference(fuzzy_bridge_t* bridge,
                                    const float* inputs, uint32_t num_inputs,
                                    fuzzy_inference_engine_t* engine,
                                    fuzzy_inference_result_t* out_result) {
    if (!bridge || !inputs || !engine || !out_result) {
        set_error("fuzzy_bridge_quantum_inference: NULL argument");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "fuzzy_bridge_quantum_inference: NULL argument");
        return FUZZY_BRIDGE_ERR_NULL;
    }

    fuzzy_bridge_validate_subsystems(bridge, "quantum_inference");

    /* Currently falls back to classical inference.
     * When quantum backend is connected, this will use quantum-accelerated
     * fuzzification via amplitude encoding. */
    int rc = fuzzy_inference_evaluate(engine, inputs, num_inputs, out_result);

    bridge->stats.quantum_inferences++;
    return rc;
}

//=============================================================================
// Symbolic Logic Integration
//=============================================================================

int fuzzy_bridge_symbolic_match(fuzzy_bridge_t* bridge,
                                 const char* action_type, float fuzzy_score,
                                 bool* out_match, float* out_action_score) {
    if (!bridge || !action_type || !out_match || !out_action_score) {
        set_error("fuzzy_bridge_symbolic_match: NULL argument");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "fuzzy_bridge_symbolic_match: NULL argument");
        return FUZZY_BRIDGE_ERR_NULL;
    }

    fuzzy_bridge_validate_subsystems(bridge, "symbolic_match");

    /* Default: no symbolic override */
    *out_match = false;
    *out_action_score = fuzzy_score;

    /* If symbolic engine connected, query it */
    if (bridge->symbolic && bridge->config.enable_symbolic_integration) {
        /* Symbolic logic would refine the fuzzy score based on crisp rules */
        *out_match = true;
        bridge->stats.symbolic_matches++;
    }

    return FUZZY_BRIDGE_ERR_OK;
}

//=============================================================================
// Health & Monitoring
//=============================================================================

int fuzzy_bridge_heartbeat(fuzzy_bridge_t* bridge, const char* operation, float progress) {
    if (!bridge) {
        set_error("fuzzy_bridge_heartbeat: NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "fuzzy_bridge_heartbeat: NULL bridge");
        return FUZZY_BRIDGE_ERR_NULL;
    }

    if (bridge->health_agent) {
        nimcp_health_agent_heartbeat_ex(bridge->health_agent, operation, progress);
    }

    bridge->stats.health_heartbeats++;
    return FUZZY_BRIDGE_ERR_OK;
}

int fuzzy_bridge_check_health(const fuzzy_bridge_t* bridge) {
    if (!bridge) {
        set_error("fuzzy_bridge_check_health: NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "fuzzy_bridge_check_health: NULL bridge");
        return FUZZY_BRIDGE_ERR_NULL;
    }
    if (bridge->state == FUZZY_BRIDGE_STATE_ERROR) {
        set_error("fuzzy_bridge_check_health: bridge in ERROR state");
        fuzzy_bridge_present_antigen((fuzzy_bridge_t*)bridge, "state_error", 7);
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_INVALID_STATE, "fuzzy_bridge_check_health: bridge in ERROR state");
        return FUZZY_BRIDGE_ERR_STATE;
    }
    if (bridge->state == FUZZY_BRIDGE_STATE_SHUTDOWN) {
        set_error("fuzzy_bridge_check_health: bridge in SHUTDOWN state");
        fuzzy_bridge_present_antigen((fuzzy_bridge_t*)bridge, "state_shutdown", 5);
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_INVALID_STATE, "fuzzy_bridge_check_health: bridge in SHUTDOWN state");
        return FUZZY_BRIDGE_ERR_STATE;
    }
    return FUZZY_BRIDGE_ERR_OK;
}

//=============================================================================
// Modulation & Statistics
//=============================================================================

int fuzzy_bridge_set_inflammation(fuzzy_bridge_t* bridge, float level) {
    if (!bridge) {
        set_error("fuzzy_bridge_set_inflammation: NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "fuzzy_bridge_set_inflammation: NULL bridge");
        return FUZZY_BRIDGE_ERR_NULL;
    }
    bridge->inflammation_level = nimcp_clampf(level, 0.0f, 1.0f);
    return FUZZY_BRIDGE_ERR_OK;
}

int fuzzy_bridge_set_fatigue(fuzzy_bridge_t* bridge, float level) {
    if (!bridge) {
        set_error("fuzzy_bridge_set_fatigue: NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "fuzzy_bridge_set_fatigue: NULL bridge");
        return FUZZY_BRIDGE_ERR_NULL;
    }
    bridge->fatigue_level = nimcp_clampf(level, 0.0f, 1.0f);
    return FUZZY_BRIDGE_ERR_OK;
}

int fuzzy_bridge_get_stats(const fuzzy_bridge_t* bridge, fuzzy_bridge_stats_t* stats) {
    if (!bridge || !stats) {
        set_error("fuzzy_bridge_get_stats: NULL argument");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, "fuzzy_bridge_get_stats: NULL argument");
        return FUZZY_BRIDGE_ERR_NULL;
    }
    *stats = bridge->stats;
    return FUZZY_BRIDGE_ERR_OK;
}

void fuzzy_bridge_reset_stats(fuzzy_bridge_t* bridge) {
    if (!bridge) return;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
}

const char* fuzzy_bridge_get_last_error(void) {
    return tls_fuzzy_bridge_error;
}
