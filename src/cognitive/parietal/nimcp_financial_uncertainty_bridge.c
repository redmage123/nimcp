//=============================================================================
// nimcp_financial_uncertainty_bridge.c - Financial Uncertainty Decomposition Bridge
//=============================================================================
/**
 * @file nimcp_financial_uncertainty_bridge.c
 * @brief Implementation of uncertainty decomposition for financial decisions
 *
 * @author NIMCP Development Team
 * @date 2026-01-29
 */

#include "cognitive/parietal/nimcp_financial_uncertainty_bridge.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/statistics/nimcp_statistics.h"
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
#include "constants/nimcp_threshold_constants.h"
#include "utils/math/nimcp_math_helpers.h"

/* Health agent: using pre-existing custom implementation */
static nimcp_health_agent_t* g_fin_uncertainty_health_agent = NULL;

BRIDGE_DEFINE_MESH_REGISTRATION(fin_uncertainty, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
struct kg_wiring;
typedef struct kg_wiring kg_wiring_t;

/* KG message type defines */
#define KG_MSG_FIN_UNCERTAINTY_REQUEST   "FIN_UNCERTAINTY_REQUEST"
#define KG_MSG_FIN_UNCERTAINTY_RESPONSE  "FIN_UNCERTAINTY_RESPONSE"
#define KG_MSG_FIN_UNCERTAINTY_ERROR     "FIN_UNCERTAINTY_ERROR"
#define KG_MSG_FIN_UNCERTAINTY_UPDATE    "FIN_UNCERTAINTY_UPDATE"

//=============================================================================
// BBB Integration
//=============================================================================
extern int bbb_validate_data(bbb_system_t bbb, const void* data, size_t size,
                              const char* context);
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

//=============================================================================
// Thread-local Error
//=============================================================================

static _Thread_local char fin_uncertainty_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(fin_uncertainty_last_error, sizeof(fin_uncertainty_last_error), fmt, args);
    va_end(args);
}

//=============================================================================
// Heartbeat Helpers
//=============================================================================

static inline void fin_uncertainty_heartbeat(const char* operation, float progress) {
    if (g_fin_uncertainty_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_fin_uncertainty_health_agent, operation, progress);
    }
}

static inline void fin_uncertainty_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_fin_uncertainty_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_fin_uncertainty_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_fin_uncertainty_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

//=============================================================================
// Internal Structure
//=============================================================================

struct financial_uncertainty_bridge {
    fin_uncertainty_config_t config;
    fin_uncertainty_bridge_stats_t stats;

    /* Modulation state */
    float inflammation;
    float fatigue;

    /* Subsystem pointers */
    brain_immune_system_t* immune;
    bbb_system_t bbb;
    kg_wiring_t* kg_wiring;
    nimcp_health_agent_t* health_agent;
    void* logger;
    void* security;
    void* ethics;
    const void* lgss;
    void* coordinator;
    void* bio_router;

    /* Security validation flags */
    bool enable_bbb_validation;
    bool enable_immune_validation;

    /* Operational state */
    fin_uncertainty_op_state_t operational_state;
};

/* Weighted mean/variance kept local as central stats module doesn't provide weighted variants */
static float compute_weighted_mean(const float* values, const float* weights, uint32_t count) {
    if (count == 0) return 0.0f;
    float sum = 0.0f;
    float weight_sum = 0.0f;
    for (uint32_t i = 0; i < count; i++) {
        float w = weights ? weights[i] : 1.0f;
        sum += values[i] * w;
        weight_sum += w;
    }
    if (weight_sum < 1e-10f) return 0.0f;
    return sum / (fabsf(weight_sum) > 1e-7f ? weight_sum : 1e-7f);
}

static float compute_weighted_variance(const float* values, const float* weights,
                                        uint32_t count, float wmean) {
    if (count <= 1) return 0.0f;
    float sum_sq = 0.0f;
    float weight_sum = 0.0f;
    for (uint32_t i = 0; i < count; i++) {
        float w = weights ? weights[i] : 1.0f;
        float diff = values[i] - wmean;
        sum_sq += w * diff * diff;
        weight_sum += w;
    }
    if (weight_sum < 1e-10f) return 0.0f;
    return sum_sq / (fabsf(weight_sum) > 1e-7f ? weight_sum : 1e-7f);
}

//=============================================================================
// Instance-Level Validation Helper
//=============================================================================

static int fin_uncertainty_validate_instance(financial_uncertainty_bridge_t* bridge,
                                              const char* operation) {
    if (!bridge) return FIN_UNCERTAINTY_ERR_NULL;

    if (bridge->enable_bbb_validation && bridge->bbb) {
        int rc = bbb_validate_data(bridge->bbb, NULL, 0, operation);
        if (rc != 0) {
            set_error("BBB validation failed for %s", operation);
            bridge->stats.bbb_validations++;
            NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_BBB_VALIDATION,
                "fin_uncertainty: BBB validation failed for %s", operation);
            return FIN_UNCERTAINTY_ERR_VALIDATION;
        }
        bridge->stats.bbb_validations++;
    }

    if (bridge->enable_immune_validation && bridge->immune) {
        int rc = brain_immune_validate_operation(bridge->immune, operation, 5);
        if (rc != 0) {
            set_error("Immune validation failed for %s", operation);
            bridge->stats.immune_checks++;
            NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_BBB_VALIDATION,
                "fin_uncertainty: immune validation failed for %s", operation);
            return FIN_UNCERTAINTY_ERR_VALIDATION;
        }
        bridge->stats.immune_checks++;
    }

    return FIN_UNCERTAINTY_ERR_OK;
}

//=============================================================================
// Antigen Presentation Helper
//=============================================================================

static void fin_uncertainty_present_antigen(financial_uncertainty_bridge_t* bridge,
                                             const char* anomaly, uint32_t severity) {
    if (bridge && bridge->immune) {
        uint8_t sig[64] = {0};
        snprintf((char*)sig, sizeof(sig), "fin_uncertainty:%s", anomaly);
        uint32_t antigen_id = 0;
        brain_immune_present_antigen(bridge->immune, 0, sig, strlen((char*)sig),
                                      severity, 0, &antigen_id);
    }
}

//=============================================================================
// KG Publish Helper
//=============================================================================

static int fin_uncertainty_kg_publish(financial_uncertainty_bridge_t* bridge,
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

fin_uncertainty_config_t financial_uncertainty_bridge_default_config(void) {
    fin_uncertainty_heartbeat("default_config", 0.0f);

    fin_uncertainty_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    /* Decomposition settings */
    cfg.min_predictions = 2;
    cfg.confidence_threshold = NIMCP_CONFIDENCE_MIN;
    cfg.use_weighted_decomposition = true;

    /* Info gathering thresholds */
    cfg.info_gathering_threshold = 0.6f;    /* If epistemic > 60% of total */
    cfg.act_threshold = 0.2f;               /* Low uncertainty: act confidently */
    cfg.pass_threshold = 0.8f;              /* High uncertainty: pass */

    /* Modulation sensitivity */
    cfg.inflammation_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    cfg.fatigue_sensitivity = NIMCP_SENSITIVITY_DEFAULT;

    /* Security */
    cfg.enable_bbb_validation = false;
    cfg.enable_immune_validation = false;

    fin_uncertainty_heartbeat("default_config", 1.0f);
    return cfg;
}

financial_uncertainty_bridge_t* financial_uncertainty_bridge_create(
    const fin_uncertainty_config_t* config)
{
    fin_uncertainty_heartbeat("create", 0.0f);

    financial_uncertainty_bridge_t* bridge =
        (financial_uncertainty_bridge_t*)nimcp_malloc(sizeof(financial_uncertainty_bridge_t));
    if (!bridge) {
        set_error("Failed to allocate financial_uncertainty_bridge_t");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate financial_uncertainty_bridge_t");
        return NULL;
    }
    memset(bridge, 0, sizeof(*bridge));

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = financial_uncertainty_bridge_default_config();
    }

    /* Initialize state */
    bridge->operational_state = FIN_UNCERTAINTY_STATE_IDLE;
    bridge->inflammation = 0.0f;
    bridge->fatigue = 0.0f;

    /* Copy security flags */
    bridge->enable_bbb_validation = bridge->config.enable_bbb_validation;
    bridge->enable_immune_validation = bridge->config.enable_immune_validation;

    fin_uncertainty_heartbeat("create", 1.0f);
    return bridge;
}

void financial_uncertainty_bridge_destroy(financial_uncertainty_bridge_t* bridge) {
    if (!bridge) return;
    fin_uncertainty_heartbeat("destroy", 0.0f);

    nimcp_free(bridge);
    bridge = NULL;
    fin_uncertainty_heartbeat("destroy", 1.0f);
}

fin_uncertainty_op_state_t financial_uncertainty_bridge_get_state(
    const financial_uncertainty_bridge_t* bridge)
{
    if (!bridge) return FIN_UNCERTAINTY_STATE_UNINITIALIZED;
    return bridge->operational_state;
}

int financial_uncertainty_bridge_reset(financial_uncertainty_bridge_t* bridge) {
    if (!bridge) {
        set_error("NULL bridge in reset");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_uncertainty_bridge_reset: bridge is NULL");
        return FIN_UNCERTAINTY_ERR_NULL;
    }

    fin_uncertainty_heartbeat_instance(bridge->health_agent, "reset", 0.0f);

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    /* Reset modulation */
    bridge->inflammation = 0.0f;
    bridge->fatigue = 0.0f;

    bridge->operational_state = FIN_UNCERTAINTY_STATE_IDLE;

    fin_uncertainty_heartbeat_instance(bridge->health_agent, "reset", 1.0f);
    return FIN_UNCERTAINTY_ERR_OK;
}

//=============================================================================
// Subsystem Setters
//=============================================================================

int financial_uncertainty_bridge_set_immune(financial_uncertainty_bridge_t* bridge,
                                             void* immune) {
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_uncertainty_bridge_set_immune: bridge is NULL");
        return FIN_UNCERTAINTY_ERR_NULL;
    }
    bridge->immune = (brain_immune_system_t*)immune;
    return FIN_UNCERTAINTY_ERR_OK;
}

int financial_uncertainty_bridge_set_bbb(financial_uncertainty_bridge_t* bridge,
                                          bbb_system_t bbb) {
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_uncertainty_bridge_set_bbb: bridge is NULL");
        return FIN_UNCERTAINTY_ERR_NULL;
    }
    bridge->bbb = bbb;
    return FIN_UNCERTAINTY_ERR_OK;
}

int financial_uncertainty_bridge_enable_bbb_validation(
    financial_uncertainty_bridge_t* bridge, bool enable) {
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_uncertainty_bridge_enable_bbb_validation: bridge is NULL");
        return FIN_UNCERTAINTY_ERR_NULL;
    }
    bridge->enable_bbb_validation = enable;
    return FIN_UNCERTAINTY_ERR_OK;
}

int financial_uncertainty_bridge_enable_immune_validation(
    financial_uncertainty_bridge_t* bridge, bool enable) {
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_uncertainty_bridge_enable_immune_validation: bridge is NULL");
        return FIN_UNCERTAINTY_ERR_NULL;
    }
    bridge->enable_immune_validation = enable;
    return FIN_UNCERTAINTY_ERR_OK;
}

int financial_uncertainty_bridge_set_kg_wiring(financial_uncertainty_bridge_t* bridge,
                                                void* kg) {
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_uncertainty_bridge_set_kg_wiring: bridge is NULL");
        return FIN_UNCERTAINTY_ERR_NULL;
    }
    bridge->kg_wiring = (kg_wiring_t*)kg;
    return FIN_UNCERTAINTY_ERR_OK;
}

int financial_uncertainty_bridge_set_health_agent(financial_uncertainty_bridge_t* bridge,
                                                   void* health_agent) {
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_uncertainty_bridge_set_health_agent: bridge is NULL");
        return FIN_UNCERTAINTY_ERR_NULL;
    }
    bridge->health_agent = (nimcp_health_agent_t*)health_agent;
    return FIN_UNCERTAINTY_ERR_OK;
}

int financial_uncertainty_bridge_set_logger(financial_uncertainty_bridge_t* bridge,
                                             void* logger) {
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_uncertainty_bridge_set_logger: bridge is NULL");
        return FIN_UNCERTAINTY_ERR_NULL;
    }
    bridge->logger = logger;
    return FIN_UNCERTAINTY_ERR_OK;
}

int financial_uncertainty_bridge_set_security(financial_uncertainty_bridge_t* bridge,
                                               void* security) {
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_uncertainty_bridge_set_security: bridge is NULL");
        return FIN_UNCERTAINTY_ERR_NULL;
    }
    bridge->security = security;
    return FIN_UNCERTAINTY_ERR_OK;
}

int financial_uncertainty_bridge_set_ethics(financial_uncertainty_bridge_t* bridge,
                                             ethics_engine_t ethics) {
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_uncertainty_bridge_set_ethics: bridge is NULL");
        return FIN_UNCERTAINTY_ERR_NULL;
    }
    bridge->ethics = (void*)ethics;
    return FIN_UNCERTAINTY_ERR_OK;
}

int financial_uncertainty_bridge_set_lgss(financial_uncertainty_bridge_t* bridge,
                                           const void* lgss) {
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_uncertainty_bridge_set_lgss: bridge is NULL");
        return FIN_UNCERTAINTY_ERR_NULL;
    }
    bridge->lgss = lgss;
    return FIN_UNCERTAINTY_ERR_OK;
}

int financial_uncertainty_bridge_set_coordinator(financial_uncertainty_bridge_t* bridge,
                                                  brain_cycle_coordinator_t* coordinator) {
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_uncertainty_bridge_set_coordinator: bridge is NULL");
        return FIN_UNCERTAINTY_ERR_NULL;
    }
    bridge->coordinator = (void*)coordinator;
    return FIN_UNCERTAINTY_ERR_OK;
}

int financial_uncertainty_bridge_set_bio_router(financial_uncertainty_bridge_t* bridge,
                                                 void* bio_router) {
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_uncertainty_bridge_set_bio_router: bridge is NULL");
        return FIN_UNCERTAINTY_ERR_NULL;
    }
    bridge->bio_router = bio_router;
    return FIN_UNCERTAINTY_ERR_OK;
}

//=============================================================================
// Core Uncertainty Decomposition: decompose()
//=============================================================================

int financial_uncertainty_bridge_decompose(
    financial_uncertainty_bridge_t* bridge,
    const fin_prediction_t* prediction,
    fin_uncertainty_t* uncertainty)
{
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_uncertainty_bridge_decompose: bridge is NULL");
        return FIN_UNCERTAINTY_ERR_NULL;
    }
    if (!prediction) {
        set_error("NULL prediction");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_uncertainty_bridge_decompose: prediction is NULL");
        return FIN_UNCERTAINTY_ERR_NULL;
    }
    if (!uncertainty) {
        set_error("NULL uncertainty");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_uncertainty_bridge_decompose: uncertainty is NULL");
        return FIN_UNCERTAINTY_ERR_NULL;
    }
    if (!prediction->values || prediction->count == 0) {
        set_error("Empty prediction array");
        return FIN_UNCERTAINTY_ERR_INVALID_PARAM;
    }

    /* Validate subsystems */
    int val_rc = fin_uncertainty_validate_instance(bridge, "decompose");
    if (val_rc != FIN_UNCERTAINTY_ERR_OK) return val_rc;

    fin_uncertainty_heartbeat_instance(bridge->health_agent, "decompose", 0.0f);
    bridge->stats.health_heartbeats++;

    bridge->operational_state = FIN_UNCERTAINTY_STATE_DECOMPOSING;

    /* Check minimum predictions */
    if (prediction->count < bridge->config.min_predictions) {
        set_error("Insufficient predictions: %u < %u", prediction->count,
                  (uint32_t)bridge->config.min_predictions);
        bridge->operational_state = FIN_UNCERTAINTY_STATE_ERROR;
        return FIN_UNCERTAINTY_ERR_INSUFFICIENT_DATA;
    }

    /* Health modulation: affects precision of decomposition */
    float health_mod = 1.0f
        - bridge->inflammation * bridge->config.inflammation_sensitivity * 0.2f
        - bridge->fatigue * bridge->config.fatigue_sensitivity * 0.15f;
    if (health_mod < 0.1f) health_mod = 0.1f;

    /*
     * Variance Decomposition:
     * Total Variance = Epistemic + Aleatoric
     *
     * For ensemble predictions:
     * - Epistemic: Variance of the predictions themselves (model uncertainty)
     * - Aleatoric: Average variance implied by confidences (noise)
     *
     * If we have confidence values, we interpret them as:
     * confidence = 1 / (1 + variance_estimate)
     * variance_estimate = (1 - confidence) / confidence
     */

    float mean_pred = 0.0f;
    float epistemic_var = 0.0f;
    float aleatoric_var = 0.0f;

    if (bridge->config.use_weighted_decomposition && prediction->confidences) {
        /* Weighted decomposition */
        mean_pred = compute_weighted_mean(prediction->values, prediction->confidences,
                                           prediction->count);
        epistemic_var = compute_weighted_variance(prediction->values, prediction->confidences,
                                                   prediction->count, mean_pred);

        /* Aleatoric: average implied variance from confidences */
        float total_aleatoric = 0.0f;
        float weight_sum = 0.0f;
        for (uint32_t i = 0; i < prediction->count; i++) {
            float conf = prediction->confidences[i];
            if (conf < bridge->config.confidence_threshold) continue;
            conf = nimcp_clampf(conf, 0.01f, 0.99f);

            /* Convert confidence to variance estimate */
            /* High confidence -> low variance, low confidence -> high variance */
            float implied_var = (1.0f - conf) / conf;
            total_aleatoric += implied_var * conf;  /* Weight by confidence */
            weight_sum += conf;
        }
        if (weight_sum > 1e-10f) {
            aleatoric_var = total_aleatoric / (fabsf(weight_sum) > 1e-7f ? weight_sum : 1e-7f);
        }
    } else {
        /* Unweighted decomposition - use central statistics module */
        mean_pred = nimcp_stats_mean(prediction->values, prediction->count);
        epistemic_var = nimcp_stats_variance(prediction->values, prediction->count);

        /* Without confidences, estimate aleatoric from prediction spread */
        /* Use heuristic: aleatoric ~ 0.5 * epistemic for financial data */
        aleatoric_var = epistemic_var * 0.5f;
    }

    /* Apply health modulation */
    epistemic_var *= (2.0f - health_mod);  /* Uncertainty increases under stress */
    aleatoric_var *= (2.0f - health_mod);

    /* Compute total and store results */
    float total_var = epistemic_var + aleatoric_var;

    /* Convert variances to standard deviations for interpretability */
    uncertainty->epistemic = sqrtf(epistemic_var);
    uncertainty->aleatoric = sqrtf(aleatoric_var);
    uncertainty->total = sqrtf(total_var);

    /* Publish to KG */
    fin_uncertainty_kg_publish(bridge, KG_MSG_FIN_UNCERTAINTY_RESPONSE,
                                uncertainty, sizeof(*uncertainty));

    bridge->stats.decompositions++;
    bridge->operational_state = FIN_UNCERTAINTY_STATE_IDLE;

    fin_uncertainty_heartbeat_instance(bridge->health_agent, "decompose", 1.0f);
    bridge->stats.health_heartbeats++;

    return FIN_UNCERTAINTY_ERR_OK;
}

//=============================================================================
// Ensemble Decomposition
//=============================================================================

int financial_uncertainty_bridge_decompose_ensemble(
    financial_uncertainty_bridge_t* bridge,
    const fin_ensemble_prediction_t* ensemble,
    fin_uncertainty_t* uncertainty)
{
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_uncertainty_bridge_decompose_ensemble: bridge is NULL");
        return FIN_UNCERTAINTY_ERR_NULL;
    }
    if (!ensemble) {
        set_error("NULL ensemble");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_uncertainty_bridge_decompose_ensemble: ensemble is NULL");
        return FIN_UNCERTAINTY_ERR_NULL;
    }
    if (!uncertainty) {
        set_error("NULL uncertainty");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_uncertainty_bridge_decompose_ensemble: uncertainty is NULL");
        return FIN_UNCERTAINTY_ERR_NULL;
    }

    if (ensemble->ensemble_size == 0 || ensemble->num_samples == 0) {
        set_error("Empty ensemble");
        return FIN_UNCERTAINTY_ERR_INVALID_PARAM;
    }

    /* Validate subsystems */
    int val_rc = fin_uncertainty_validate_instance(bridge, "decompose_ensemble");
    if (val_rc != FIN_UNCERTAINTY_ERR_OK) return val_rc;

    fin_uncertainty_heartbeat_instance(bridge->health_agent, "decompose_ensemble", 0.0f);
    bridge->stats.health_heartbeats++;

    bridge->operational_state = FIN_UNCERTAINTY_STATE_DECOMPOSING;

    /*
     * Full Bayesian variance decomposition:
     * Total = E[Var(Y|X)] + Var(E[Y|X])
     *       = Aleatoric   + Epistemic
     *
     * E[Var(Y|X)]: Average of within-model variances
     * Var(E[Y|X]): Variance of model means
     */

    uint32_t M = ensemble->ensemble_size;   /* Number of ensemble members */
    uint32_t N = ensemble->num_samples;     /* Samples per member */

    /* Compute mean prediction for each ensemble member */
    float* member_means = (float*)nimcp_calloc(M, sizeof(float));
    float* member_vars = (float*)nimcp_calloc(M, sizeof(float));
    if (!member_means || !member_vars) {
        if (member_means) nimcp_free(member_means);
        if (member_vars) nimcp_free(member_vars);
        set_error("Memory allocation failed");
        bridge->operational_state = FIN_UNCERTAINTY_STATE_ERROR;
        return FIN_UNCERTAINTY_ERR_NO_MEMORY;
    }

    for (uint32_t m = 0; m < M; m++) {
        if (!ensemble->ensemble_predictions[m]) continue;

        /* Mean of this member's predictions */
        float sum = 0.0f;
        for (uint32_t n = 0; n < N; n++) {
            sum += ensemble->ensemble_predictions[m][n];
        }
        member_means[m] = sum / (float)N;

        /* Variance of this member (or use provided variance) */
        if (ensemble->ensemble_variances && ensemble->ensemble_variances[m]) {
            /* Average provided variances */
            float var_sum = 0.0f;
            for (uint32_t n = 0; n < N; n++) {
                var_sum += ensemble->ensemble_variances[m][n];
            }
            member_vars[m] = var_sum / (float)N;
        } else {
            /* Compute variance from predictions */
            float var_sum = 0.0f;
            for (uint32_t n = 0; n < N; n++) {
                float diff = ensemble->ensemble_predictions[m][n] - member_means[m];
                var_sum += diff * diff;
            }
            member_vars[m] = var_sum / (float)(N > 1 ? N - 1 : 1);
        }
    }

    /* Aleatoric: E[Var(Y|X)] - average of within-model variances */
    float aleatoric_var = nimcp_stats_mean(member_vars, M);

    /* Epistemic: Var(E[Y|X]) - variance of model means */
    float epistemic_var = nimcp_stats_variance(member_means, M);

    nimcp_free(member_means);
    member_means = NULL;
    nimcp_free(member_vars);
    member_vars = NULL;

    /* Health modulation */
    float health_mod = 1.0f
        - bridge->inflammation * bridge->config.inflammation_sensitivity * 0.2f
        - bridge->fatigue * bridge->config.fatigue_sensitivity * 0.15f;
    if (health_mod < 0.1f) health_mod = 0.1f;

    epistemic_var *= (2.0f - health_mod);
    aleatoric_var *= (2.0f - health_mod);

    float total_var = epistemic_var + aleatoric_var;

    uncertainty->epistemic = sqrtf(epistemic_var);
    uncertainty->aleatoric = sqrtf(aleatoric_var);
    uncertainty->total = sqrtf(total_var);

    bridge->stats.decompositions++;
    bridge->operational_state = FIN_UNCERTAINTY_STATE_IDLE;

    fin_uncertainty_heartbeat_instance(bridge->health_agent, "decompose_ensemble", 1.0f);
    bridge->stats.health_heartbeats++;

    return FIN_UNCERTAINTY_ERR_OK;
}

//=============================================================================
// Information Gathering Decision
//=============================================================================

int financial_uncertainty_bridge_should_gather_info(
    financial_uncertainty_bridge_t* bridge,
    const fin_uncertainty_t* uncertainty,
    bool* should_gather)
{
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_uncertainty_bridge_should_gather_info: bridge is NULL");
        return FIN_UNCERTAINTY_ERR_NULL;
    }
    if (!uncertainty) {
        set_error("NULL uncertainty");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_uncertainty_bridge_should_gather_info: uncertainty is NULL");
        return FIN_UNCERTAINTY_ERR_NULL;
    }
    if (!should_gather) {
        set_error("NULL should_gather");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_uncertainty_bridge_should_gather_info: should_gather is NULL");
        return FIN_UNCERTAINTY_ERR_NULL;
    }

    /* Validate subsystems */
    int val_rc = fin_uncertainty_validate_instance(bridge, "should_gather_info");
    if (val_rc != FIN_UNCERTAINTY_ERR_OK) return val_rc;

    fin_uncertainty_heartbeat_instance(bridge->health_agent, "should_gather_info", 0.0f);
    bridge->stats.health_heartbeats++;

    bridge->operational_state = FIN_UNCERTAINTY_STATE_ANALYZING;

    /*
     * Decision logic:
     * - If epistemic uncertainty is high relative to total, gathering info helps
     * - If total uncertainty is below threshold, we can act confidently
     * - If total is very high and mostly aleatoric, info won't help much
     */

    float total = uncertainty->total;
    float epistemic = uncertainty->epistemic;

    /* Avoid division by zero */
    if (total < 1e-10f) {
        *should_gather = false;
        bridge->stats.info_gather_recommendations++;
        bridge->operational_state = FIN_UNCERTAINTY_STATE_IDLE;
        return FIN_UNCERTAINTY_ERR_OK;
    }

    float epistemic_ratio = (epistemic * epistemic) / (total * total);

    /* Health modulation: under stress, be more cautious (gather more info) */
    float health_mod = 1.0f
        + bridge->inflammation * bridge->config.inflammation_sensitivity * 0.1f
        + bridge->fatigue * bridge->config.fatigue_sensitivity * 0.1f;

    /* Adjust threshold based on health */
    float adjusted_threshold = bridge->config.info_gathering_threshold / health_mod;

    /* Should gather if epistemic is a significant portion of total */
    *should_gather = (epistemic_ratio >= adjusted_threshold);

    /* Also check if total uncertainty is low enough to act anyway */
    if (total <= bridge->config.act_threshold) {
        *should_gather = false;  /* Low uncertainty, no need for more info */
    }

    bridge->stats.info_gather_recommendations++;
    bridge->operational_state = FIN_UNCERTAINTY_STATE_IDLE;

    fin_uncertainty_heartbeat_instance(bridge->health_agent, "should_gather_info", 1.0f);
    bridge->stats.health_heartbeats++;

    return FIN_UNCERTAINTY_ERR_OK;
}

//=============================================================================
// Decision Guidance
//=============================================================================

int financial_uncertainty_bridge_get_guidance(
    financial_uncertainty_bridge_t* bridge,
    const fin_uncertainty_t* uncertainty,
    fin_decision_guidance_t* guidance)
{
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_uncertainty_bridge_get_guidance: bridge is NULL");
        return FIN_UNCERTAINTY_ERR_NULL;
    }
    if (!uncertainty) {
        set_error("NULL uncertainty");
        return FIN_UNCERTAINTY_ERR_NULL;
    }
    if (!guidance) {
        set_error("NULL guidance");
        return FIN_UNCERTAINTY_ERR_NULL;
    }

    /* Validate subsystems */
    int val_rc = fin_uncertainty_validate_instance(bridge, "get_guidance");
    if (val_rc != FIN_UNCERTAINTY_ERR_OK) return val_rc;

    fin_uncertainty_heartbeat_instance(bridge->health_agent, "get_guidance", 0.0f);
    bridge->stats.health_heartbeats++;

    float total = uncertainty->total;
    float epistemic = uncertainty->epistemic;
    float aleatoric = uncertainty->aleatoric;

    /* Health modulation */
    float health_mod = 1.0f
        + bridge->inflammation * bridge->config.inflammation_sensitivity * 0.15f
        + bridge->fatigue * bridge->config.fatigue_sensitivity * 0.1f;

    /* Adjust thresholds */
    float act_thresh = bridge->config.act_threshold * health_mod;
    float pass_thresh = bridge->config.pass_threshold / health_mod;

    if (total <= act_thresh) {
        /* Low uncertainty - act confidently */
        *guidance = FIN_DECISION_ACT_CONFIDENTLY;
    } else if (total >= pass_thresh) {
        /* Very high uncertainty - pass */
        *guidance = FIN_DECISION_PASS;
    } else {
        /* Medium uncertainty - need to decide based on composition */
        float epistemic_ratio = (total > 1e-10f) ?
            (epistemic * epistemic) / (total * total) : 0.0f;

        if (epistemic_ratio >= bridge->config.info_gathering_threshold) {
            /* High epistemic - wait for info */
            *guidance = FIN_DECISION_WAIT;
        } else if (aleatoric > epistemic * 2.0f) {
            /* High aleatoric - hedge the position */
            *guidance = FIN_DECISION_HEDGE;
        } else {
            /* Mixed - act cautiously */
            *guidance = FIN_DECISION_ACT_CAUTIOUSLY;
        }
    }

    fin_uncertainty_heartbeat_instance(bridge->health_agent, "get_guidance", 1.0f);
    bridge->stats.health_heartbeats++;

    return FIN_UNCERTAINTY_ERR_OK;
}

//=============================================================================
// Full Analysis
//=============================================================================

int financial_uncertainty_bridge_analyze(
    financial_uncertainty_bridge_t* bridge,
    const fin_prediction_t* prediction,
    fin_uncertainty_analysis_t* analysis)
{
    if (!bridge) {
        set_error("NULL bridge");
        return FIN_UNCERTAINTY_ERR_NULL;
    }
    if (!prediction) {
        set_error("NULL prediction");
        return FIN_UNCERTAINTY_ERR_NULL;
    }
    if (!analysis) {
        set_error("NULL analysis");
        return FIN_UNCERTAINTY_ERR_NULL;
    }

    /* Validate subsystems */
    int val_rc = fin_uncertainty_validate_instance(bridge, "analyze");
    if (val_rc != FIN_UNCERTAINTY_ERR_OK) return val_rc;

    fin_uncertainty_heartbeat_instance(bridge->health_agent, "analyze", 0.0f);
    bridge->stats.health_heartbeats++;

    /* Step 1: Decompose uncertainty */
    int rc = financial_uncertainty_bridge_decompose(bridge, prediction, &analysis->uncertainty);
    if (rc != FIN_UNCERTAINTY_ERR_OK) return rc;

    fin_uncertainty_heartbeat_instance(bridge->health_agent, "analyze", 0.3f);

    /* Step 2: Compute derived metrics */
    float total_sq = analysis->uncertainty.total * analysis->uncertainty.total;
    if (total_sq > 1e-10f) {
        float epi_sq = analysis->uncertainty.epistemic * analysis->uncertainty.epistemic;
        float ale_sq = analysis->uncertainty.aleatoric * analysis->uncertainty.aleatoric;
        analysis->epistemic_ratio = epi_sq / total_sq;
        analysis->aleatoric_ratio = ale_sq / total_sq;
    } else {
        analysis->epistemic_ratio = 0.0f;
        analysis->aleatoric_ratio = 0.0f;
    }

    /* Step 3: Compute prediction statistics */
    if (prediction->count > 0 && prediction->values) {
        analysis->mean_prediction = nimcp_stats_mean(prediction->values, prediction->count);
        analysis->std_prediction = nimcp_stats_std_dev(prediction->values, prediction->count);

        if (prediction->confidences) {
            analysis->mean_confidence = nimcp_stats_mean(prediction->confidences, prediction->count);
        } else {
            analysis->mean_confidence = 0.5f;  /* Default */
        }
    }

    fin_uncertainty_heartbeat_instance(bridge->health_agent, "analyze", 0.5f);

    /* Step 4: Determine if should gather info */
    rc = financial_uncertainty_bridge_should_gather_info(bridge, &analysis->uncertainty,
                                                          &analysis->should_gather_info);
    if (rc != FIN_UNCERTAINTY_ERR_OK) return rc;

    /* Step 5: Get decision guidance */
    rc = financial_uncertainty_bridge_get_guidance(bridge, &analysis->uncertainty,
                                                    &analysis->guidance);
    if (rc != FIN_UNCERTAINTY_ERR_OK) return rc;

    fin_uncertainty_heartbeat_instance(bridge->health_agent, "analyze", 0.7f);

    /* Step 6: Generate info recommendations if needed */
    if (analysis->should_gather_info && analysis->recommendations) {
        uint32_t num_recs = 0;
        rc = financial_uncertainty_bridge_recommend_info(bridge, &analysis->uncertainty,
                                                          analysis->recommendations,
                                                          analysis->num_recommendations,
                                                          &num_recs);
        if (rc == FIN_UNCERTAINTY_ERR_OK) {
            analysis->num_recommendations = num_recs;
        }
    }

    /* Step 7: Compute overall confidence in analysis */
    /* Higher confidence when we have more data and lower total uncertainty */
    float data_factor = (float)prediction->count / (float)FIN_UNCERTAINTY_MAX_PREDICTIONS;
    if (data_factor > 1.0f) data_factor = 1.0f;
    float uncertainty_factor = 1.0f - analysis->uncertainty.total;
    if (uncertainty_factor < 0.1f) uncertainty_factor = 0.1f;

    analysis->confidence = 0.5f * data_factor + 0.5f * uncertainty_factor;

    fin_uncertainty_heartbeat_instance(bridge->health_agent, "analyze", 1.0f);
    bridge->stats.health_heartbeats++;

    return FIN_UNCERTAINTY_ERR_OK;
}

//=============================================================================
// Information Source Recommendations
//=============================================================================

int financial_uncertainty_bridge_recommend_info(
    financial_uncertainty_bridge_t* bridge,
    const fin_uncertainty_t* uncertainty,
    fin_info_recommendation_t* recommendations,
    uint32_t max_recommendations,
    uint32_t* num_recommendations)
{
    if (!bridge) {
        set_error("NULL bridge");
        return FIN_UNCERTAINTY_ERR_NULL;
    }
    if (!uncertainty) {
        set_error("NULL uncertainty");
        return FIN_UNCERTAINTY_ERR_NULL;
    }
    if (!recommendations || max_recommendations == 0) {
        set_error("Invalid recommendations buffer");
        return FIN_UNCERTAINTY_ERR_INVALID_PARAM;
    }
    if (!num_recommendations) {
        set_error("NULL num_recommendations");
        return FIN_UNCERTAINTY_ERR_NULL;
    }

    /* Validate subsystems */
    int val_rc = fin_uncertainty_validate_instance(bridge, "recommend_info");
    if (val_rc != FIN_UNCERTAINTY_ERR_OK) return val_rc;

    fin_uncertainty_heartbeat_instance(bridge->health_agent, "recommend_info", 0.0f);
    bridge->stats.health_heartbeats++;

    /*
     * Recommend information sources based on uncertainty profile.
     * Higher epistemic uncertainty suggests more info would help.
     * Different sources address different types of uncertainty.
     */

    uint32_t count = 0;
    float epistemic = uncertainty->epistemic;
    float total = uncertainty->total;

    /* Only recommend if epistemic is significant */
    if (total < 1e-10f || epistemic < 0.1f * total) {
        *num_recommendations = 0;
        return FIN_UNCERTAINTY_ERR_OK;
    }

    /* Source: Fundamental analysis - reduces model uncertainty */
    if (count < max_recommendations) {
        recommendations[count].source = FIN_INFO_SOURCE_FUNDAMENTAL;
        recommendations[count].expected_uncertainty_reduction = 0.25f * epistemic / (fabsf(total) > 1e-7f ? total : 1e-7f);
        recommendations[count].cost = 0.3f;
        recommendations[count].value_of_information =
            recommendations[count].expected_uncertainty_reduction /
            (recommendations[count].cost + 0.01f);
        snprintf(recommendations[count].description,
                 sizeof(recommendations[count].description),
                 "Analyze financial statements and earnings reports");
        count++;
    }

    /* Source: Analyst estimates - consensus reduces variance */
    if (count < max_recommendations) {
        recommendations[count].source = FIN_INFO_SOURCE_ANALYST;
        recommendations[count].expected_uncertainty_reduction = 0.2f * epistemic / (fabsf(total) > 1e-7f ? total : 1e-7f);
        recommendations[count].cost = 0.1f;
        recommendations[count].value_of_information =
            recommendations[count].expected_uncertainty_reduction /
            (recommendations[count].cost + 0.01f);
        snprintf(recommendations[count].description,
                 sizeof(recommendations[count].description),
                 "Review analyst estimates and consensus targets");
        count++;
    }

    /* Source: Technical analysis - pattern recognition */
    if (count < max_recommendations) {
        recommendations[count].source = FIN_INFO_SOURCE_TECHNICAL;
        recommendations[count].expected_uncertainty_reduction = 0.15f * epistemic / (total > 0 ? total : 1);
        recommendations[count].cost = 0.2f;
        recommendations[count].value_of_information =
            recommendations[count].expected_uncertainty_reduction /
            (recommendations[count].cost + 0.01f);
        snprintf(recommendations[count].description,
                 sizeof(recommendations[count].description),
                 "Analyze price patterns and technical indicators");
        count++;
    }

    /* Source: Sentiment - market psychology */
    if (count < max_recommendations) {
        recommendations[count].source = FIN_INFO_SOURCE_SENTIMENT;
        recommendations[count].expected_uncertainty_reduction = 0.1f * epistemic / (total > 0 ? total : 1);
        recommendations[count].cost = 0.15f;
        recommendations[count].value_of_information =
            recommendations[count].expected_uncertainty_reduction /
            (recommendations[count].cost + 0.01f);
        snprintf(recommendations[count].description,
                 sizeof(recommendations[count].description),
                 "Assess market sentiment and news flow");
        count++;
    }

    /* Source: Options market - implied information */
    if (count < max_recommendations) {
        recommendations[count].source = FIN_INFO_SOURCE_OPTIONS;
        recommendations[count].expected_uncertainty_reduction = 0.18f * epistemic / (total > 0 ? total : 1);
        recommendations[count].cost = 0.25f;
        recommendations[count].value_of_information =
            recommendations[count].expected_uncertainty_reduction /
            (recommendations[count].cost + 0.01f);
        snprintf(recommendations[count].description,
                 sizeof(recommendations[count].description),
                 "Analyze options implied volatility and skew");
        count++;
    }

    /* Sort by value of information (simple bubble sort) */
    for (uint32_t i = 0; i < count - 1; i++) {
        for (uint32_t j = 0; j < count - i - 1; j++) {
            if (recommendations[j].value_of_information <
                recommendations[j + 1].value_of_information) {
                fin_info_recommendation_t temp = recommendations[j];
                recommendations[j] = recommendations[j + 1];
                recommendations[j + 1] = temp;
            }
        }
    }

    *num_recommendations = count;
    bridge->stats.info_gather_recommendations++;

    fin_uncertainty_heartbeat_instance(bridge->health_agent, "recommend_info", 1.0f);
    bridge->stats.health_heartbeats++;

    return FIN_UNCERTAINTY_ERR_OK;
}

//=============================================================================
// Utility Functions
//=============================================================================

fin_prediction_t* financial_uncertainty_prediction_create(uint32_t count) {
    if (count == 0 || count > FIN_UNCERTAINTY_MAX_PREDICTIONS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "financial_uncertainty_prediction_create: count is zero");
        return NULL;
    }

    fin_prediction_t* pred = (fin_prediction_t*)nimcp_malloc(sizeof(fin_prediction_t));
    if (!pred) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "financial_uncertainty_prediction_create: pred is NULL");
        return NULL;
    }

    memset(pred, 0, sizeof(*pred));
    pred->count = count;

    pred->values = (float*)nimcp_calloc(count, sizeof(float));
    pred->confidences = (float*)nimcp_calloc(count, sizeof(float));

    if (!pred->values || !pred->confidences) {
        financial_uncertainty_prediction_destroy(pred);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "financial_uncertainty_prediction_create: required parameter is NULL (pred->values, pred->confidences)");
        return NULL;
    }

    /* Initialize confidences to 1.0 */
    for (uint32_t i = 0; i < count; i++) {
        pred->confidences[i] = 1.0f;
    }

    return pred;
}

void financial_uncertainty_prediction_destroy(fin_prediction_t* prediction) {
    if (!prediction) return;

    if (prediction->values) nimcp_free(prediction->values);
    if (prediction->confidences) nimcp_free(prediction->confidences);
    nimcp_free(prediction);
    prediction = NULL;
}

fin_uncertainty_analysis_t* financial_uncertainty_analysis_create(
    uint32_t max_recommendations)
{
    fin_uncertainty_analysis_t* analysis =
        (fin_uncertainty_analysis_t*)nimcp_malloc(sizeof(fin_uncertainty_analysis_t));
    if (!analysis) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "financial_uncertainty_analysis_create: analysis is NULL");
        return NULL;
    }

    memset(analysis, 0, sizeof(*analysis));

    if (max_recommendations > 0) {
        analysis->recommendations =
            (fin_info_recommendation_t*)nimcp_calloc(max_recommendations,
                                                sizeof(fin_info_recommendation_t));
        if (!analysis->recommendations) {
            nimcp_free(analysis);
            analysis = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "financial_uncertainty_analysis_create: analysis->recommendations is NULL");
            return NULL;
        }
        analysis->num_recommendations = max_recommendations;
    }

    return analysis;
}

void financial_uncertainty_analysis_destroy(fin_uncertainty_analysis_t* analysis) {
    if (!analysis) return;

    if (analysis->recommendations) nimcp_free(analysis->recommendations);
    nimcp_free(analysis);
    analysis = NULL;
}

void financial_uncertainty_analysis_free_recommendations(
    fin_uncertainty_analysis_t* analysis)
{
    if (!analysis) return;
    if (analysis->recommendations) {
        nimcp_free(analysis->recommendations);
        analysis->recommendations = NULL;
    }
    analysis->num_recommendations = 0;
}

//=============================================================================
// Modulation API
//=============================================================================

int financial_uncertainty_bridge_set_inflammation(
    financial_uncertainty_bridge_t* bridge, float level)
{
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_uncertainty_bridge_set_inflammation: bridge is NULL");
        return FIN_UNCERTAINTY_ERR_NULL;
    }
    bridge->inflammation = nimcp_clampf(level, 0.0f, 1.0f);
    return FIN_UNCERTAINTY_ERR_OK;
}

int financial_uncertainty_bridge_set_fatigue(
    financial_uncertainty_bridge_t* bridge, float level)
{
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_uncertainty_bridge_set_fatigue: bridge is NULL");
        return FIN_UNCERTAINTY_ERR_NULL;
    }
    bridge->fatigue = nimcp_clampf(level, 0.0f, 1.0f);
    return FIN_UNCERTAINTY_ERR_OK;
}

//=============================================================================
// Statistics
//=============================================================================

int financial_uncertainty_bridge_get_stats(
    const financial_uncertainty_bridge_t* bridge,
    fin_uncertainty_bridge_stats_t* stats)
{
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_uncertainty_bridge_get_stats: bridge is NULL");
        return FIN_UNCERTAINTY_ERR_NULL;
    }
    if (!stats) {
        set_error("NULL stats");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_uncertainty_bridge_get_stats: stats is NULL");
        return FIN_UNCERTAINTY_ERR_NULL;
    }
    *stats = bridge->stats;
    return FIN_UNCERTAINTY_ERR_OK;
}

void financial_uncertainty_bridge_reset_stats(financial_uncertainty_bridge_t* bridge) {
    if (!bridge) return;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
}

const char* financial_uncertainty_bridge_get_last_error(void) {
    return fin_uncertainty_last_error;
}

//=============================================================================
// String Conversion Utilities
//=============================================================================

const char* fin_uncertainty_state_name(fin_uncertainty_op_state_t state) {
    switch (state) {
        case FIN_UNCERTAINTY_STATE_UNINITIALIZED: return "UNINITIALIZED";
        case FIN_UNCERTAINTY_STATE_IDLE: return "IDLE";
        case FIN_UNCERTAINTY_STATE_DECOMPOSING: return "DECOMPOSING";
        case FIN_UNCERTAINTY_STATE_ANALYZING: return "ANALYZING";
        case FIN_UNCERTAINTY_STATE_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

const char* fin_uncertainty_info_source_name(fin_info_source_t source) {
    switch (source) {
        case FIN_INFO_SOURCE_NONE: return "NONE";
        case FIN_INFO_SOURCE_FUNDAMENTAL: return "FUNDAMENTAL";
        case FIN_INFO_SOURCE_TECHNICAL: return "TECHNICAL";
        case FIN_INFO_SOURCE_SENTIMENT: return "SENTIMENT";
        case FIN_INFO_SOURCE_MACROECONOMIC: return "MACROECONOMIC";
        case FIN_INFO_SOURCE_INDUSTRY: return "INDUSTRY";
        case FIN_INFO_SOURCE_INSIDER: return "INSIDER";
        case FIN_INFO_SOURCE_ANALYST: return "ANALYST";
        case FIN_INFO_SOURCE_OPTIONS: return "OPTIONS";
        case FIN_INFO_SOURCE_FLOW: return "FLOW";
        default: return "UNKNOWN";
    }
}

const char* fin_uncertainty_guidance_name(fin_decision_guidance_t guidance) {
    switch (guidance) {
        case FIN_DECISION_WAIT: return "WAIT";
        case FIN_DECISION_ACT_CAUTIOUSLY: return "ACT_CAUTIOUSLY";
        case FIN_DECISION_ACT_CONFIDENTLY: return "ACT_CONFIDENTLY";
        case FIN_DECISION_HEDGE: return "HEDGE";
        case FIN_DECISION_PASS: return "PASS";
        default: return "UNKNOWN";
    }
}

const char* financial_uncertainty_bridge_version(void) {
    return "1.0.0";
}
