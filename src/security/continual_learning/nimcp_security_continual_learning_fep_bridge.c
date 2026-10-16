/**
 * @file nimcp_security_continual_learning_fep_bridge.c
 * @brief Implementation of Security Continual Learning FEP Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: FEP integration for continual learning security
 * WHY:  Enable surprise-based forgetting attack detection
 * HOW:  Map retention/drift to free energy, use active inference for responses
 */

#include "security/continual_learning/nimcp_security_continual_learning_fep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/platform/nimcp_platform.h"
#include "utils/platform/nimcp_platform_time.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "constants/nimcp_threshold_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(security_continual_learning_fep_bridge, MESH_ADAPTER_CATEGORY_SECURITY)


/* ============================================================================
 * Internal Constants
 * ============================================================================ */

/** Exponential moving average decay */
#define EMA_ALPHA 0.1f

/** Sigmoid temperature for probability conversion */
#define SIGMOID_TEMPERATURE NIMCP_TEMPERATURE_LOW

/** Minimum non-zero free energy to avoid log(0) */
#define MIN_FREE_ENERGY 0.001f

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Sigmoid function
 */
static inline float sigmoid(float x) {
    return 1.0f / (1.0f + expf(-x));
}

/**
 * @brief Get current time in milliseconds
 */
static uint64_t get_current_time_ms(void) {
    return nimcp_platform_time_monotonic_ms();
}

/**
 * @brief Classify severity from free energy
 */
static security_cl_fep_severity_t classify_severity(float free_energy) {
    if (free_energy < SECURITY_CL_FEP_NORMAL_THRESHOLD) {
        return SECURITY_CL_FEP_SEVERITY_NONE;
    } else if (free_energy < SECURITY_CL_FEP_SUSPICIOUS_THRESHOLD) {
        return SECURITY_CL_FEP_SEVERITY_LOW;
    } else if (free_energy < SECURITY_CL_FEP_ATTACK_THRESHOLD) {
        return SECURITY_CL_FEP_SEVERITY_MEDIUM;
    } else if (free_energy < SECURITY_CL_FEP_CRITICAL_THRESHOLD) {
        return SECURITY_CL_FEP_SEVERITY_HIGH;
    }
    return SECURITY_CL_FEP_SEVERITY_CRITICAL;
}

/**
 * @brief Select response based on severity
 */
static security_cl_fep_response_t select_response_from_severity(
    security_cl_fep_severity_t severity
) {
    switch (severity) {
        case SECURITY_CL_FEP_SEVERITY_NONE:
            return SECURITY_CL_FEP_RESPONSE_NONE;
        case SECURITY_CL_FEP_SEVERITY_LOW:
            return SECURITY_CL_FEP_RESPONSE_MONITOR;
        case SECURITY_CL_FEP_SEVERITY_MEDIUM:
            return SECURITY_CL_FEP_RESPONSE_EWC_BOOST;
        case SECURITY_CL_FEP_SEVERITY_HIGH:
            return SECURITY_CL_FEP_RESPONSE_LR_REDUCE;
        case SECURITY_CL_FEP_SEVERITY_CRITICAL:
            return SECURITY_CL_FEP_RESPONSE_CONSOLIDATE;
        default:
            return SECURITY_CL_FEP_RESPONSE_NONE;
    }
}

/* ============================================================================
 * Default Configuration
 * ============================================================================ */

int security_cl_fep_default_config(security_cl_fep_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    /* FEP parameters */
    config->free_energy_threshold = SECURITY_CL_FEP_ATTACK_THRESHOLD;
    config->surprise_threshold = 10.0f;
    config->precision_learning_rate = SECURITY_CL_FEP_PRECISION_LR;

    /* Retention-to-FE mapping */
    config->retention_loss_weight = 1.0f;
    config->drift_weight = 0.8f;
    config->replay_anomaly_weight = 1.2f;

    /* Detection parameters */
    config->enable_fep_detection = true;
    config->enable_precision_modulation = true;
    config->enable_active_inference = true;

    /* Learning */
    config->enable_online_learning = true;
    config->belief_learning_rate = SECURITY_CL_FEP_BELIEF_LR;
    config->learn_from_false_positives = true;

    /* Bio-async */
    config->enable_bio_async = true;
    config->bio_inbox_capacity = SECURITY_CL_FEP_BIO_INBOX_CAPACITY;

    /* Logging */
    config->enable_logging = false;

    return 0;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

security_cl_fep_bridge_t* security_cl_fep_create(
    const security_cl_fep_config_t* config,
    fep_system_t* fep_system,
    security_cl_bridge_t* security_cl
) {
    /* Validate required handles */
    if (!fep_system || !security_cl) {
        NIMCP_LOGGING_ERROR("Security CL FEP bridge: NULL system handles");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_cl_fep_create: required parameter is NULL (fep_system, security_cl)");
        return NULL;
    }

    /* Allocate bridge structure */
    security_cl_fep_bridge_t* bridge = (security_cl_fep_bridge_t*)nimcp_malloc(
        sizeof(security_cl_fep_bridge_t)
    );
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Security CL FEP bridge: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "security_cl_fep_create: bridge is NULL");
        return NULL;
    }

    memset(bridge, 0, sizeof(security_cl_fep_bridge_t));

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        security_cl_fep_default_config(&bridge->config);
    }

    /* Connect systems */
    bridge->fep_system = fep_system;
    bridge->security_cl = security_cl;

    /* Initialize base bridge */
    if (bridge_base_init(&bridge->base, 0, "security_continual_learning_fe") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Security CL FEP bridge: mutex creation failed");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "security_cl_fep_create: mutex creation failed");
        return NULL;
    }

    bridge->base.module_id = BIO_MODULE_SECURITY_CORE_FEP;
    bridge->base.module_name = "security_cl_fep_bridge";
    bridge->base.bio_async_enabled = false;

    /* Initialize state */
    bridge->state.active = true;
    bridge->state.current_precision = SECURITY_CL_FEP_DEFAULT_PRECISION;
    bridge->state.precision_ema = SECURITY_CL_FEP_DEFAULT_PRECISION;
    bridge->state.last_response = SECURITY_CL_FEP_RESPONSE_NONE;

    /* Initialize effects */
    bridge->fep_effects.valid = false;
    bridge->cl_effects.valid = false;

    /* Connect bio-async if enabled */
    if (bridge->config.enable_bio_async) {
        security_cl_fep_connect_bio_async(bridge);
    }

    NIMCP_LOGGING_INFO("Security CL FEP bridge created");
    return bridge;
}

void security_cl_fep_destroy(security_cl_fep_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Disconnect bio-async */
    if (bridge->base.bio_async_enabled) {
        security_cl_fep_disconnect_bio_async(bridge);
    }

    /* Free retention beliefs if allocated */
    if (bridge->state.retention_beliefs) {
        nimcp_free(bridge->state.retention_beliefs);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Security CL FEP bridge destroyed");
}

int security_cl_fep_reset(security_cl_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Reset state */
    bridge->state.update_count = 0;
    bridge->state.detection_count = 0;
    bridge->state.current_precision = SECURITY_CL_FEP_DEFAULT_PRECISION;
    bridge->state.precision_ema = SECURITY_CL_FEP_DEFAULT_PRECISION;
    bridge->state.avg_free_energy = 0.0f;
    bridge->state.max_free_energy = 0.0f;
    bridge->state.avg_surprise = 0.0f;
    bridge->state.last_response = SECURITY_CL_FEP_RESPONSE_NONE;

    /* Reset effects */
    memset(&bridge->fep_effects, 0, sizeof(security_cl_fep_effects_t));
    memset(&bridge->cl_effects, 0, sizeof(fep_security_cl_effects_t));

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(security_cl_fep_stats_t));

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Configuration Implementation
 * ============================================================================ */

int security_cl_fep_get_config(
    const security_cl_fep_bridge_t* bridge,
    security_cl_fep_config_t* config
) {
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_cl_fep_get_config: required parameter is NULL (bridge, config)");
        return -1;
    }

    *config = bridge->config;
    return 0;
}

int security_cl_fep_set_config(
    security_cl_fep_bridge_t* bridge,
    const security_cl_fep_config_t* config
) {
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_cl_fep_set_config: required parameter is NULL (bridge, config)");
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

int security_cl_fep_compute_effects(security_cl_fep_bridge_t* bridge) {
    if (!bridge || !bridge->state.active) {
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    uint64_t start_time = get_current_time_ms();

    /* Get security CL effects */
    cl_security_effects_t cl_effects;
    if (security_cl_get_cl_effects(bridge->security_cl, &cl_effects) != 0) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return -1;
    }

    /* Store CL effects */
    bridge->cl_effects.current_retention = cl_effects.current_retention;
    bridge->cl_effects.retention_delta = cl_effects.retention_delta;
    bridge->cl_effects.retention_anomaly = cl_effects.retention_anomaly;
    bridge->cl_effects.drift_magnitude = cl_effects.current_drift_score;
    bridge->cl_effects.drift_type = cl_effects.drift_type;
    bridge->cl_effects.adversarial_drift =
        (cl_effects.drift_type == SECURITY_CL_DRIFT_ADVERSARIAL ||
         cl_effects.drift_type == SECURITY_CL_DRIFT_MANIPULATION);
    bridge->cl_effects.replay_integrity_failures = cl_effects.replay_integrity_failures;
    bridge->cl_effects.replay_poisoned = cl_effects.replay_anomaly;
    bridge->cl_effects.current_lr = cl_effects.current_lr;
    bridge->cl_effects.lr_manipulation = cl_effects.lr_anomaly;
    bridge->cl_effects.timestamp_ms = get_current_time_ms();
    bridge->cl_effects.valid = true;

    /* Compute free energy components */
    float retention_fe = security_cl_fep_retention_to_fe(
        bridge, cl_effects.current_task_id,
        cl_effects.current_retention, 0.9f /* expected baseline */
    );

    float drift_error = security_cl_fep_drift_to_error(
        bridge, cl_effects.current_drift_score, cl_effects.drift_type
    );

    float replay_surprise = security_cl_fep_replay_to_surprise(
        bridge, cl_effects.replay_integrity_failures, cl_effects.replay_anomaly
    );

    /* Compute total free energy */
    float total_fe = retention_fe + drift_error + replay_surprise;
    if (total_fe < MIN_FREE_ENERGY) {
        total_fe = MIN_FREE_ENERGY;
    }

    /* Compute surprise from FEP system */
    float surprise = fep_compute_surprise(bridge->fep_system);

    /* Update FEP effects */
    bridge->fep_effects.current_free_energy = total_fe;
    bridge->fep_effects.surprise_level = surprise + replay_surprise;
    bridge->fep_effects.prediction_error = drift_error;

    /* Compute severity score (normalized to [0,1]) */
    bridge->fep_effects.forgetting_severity_score =
        nimcp_clampf(total_fe / SECURITY_CL_FEP_CRITICAL_THRESHOLD, 0.0f, 1.0f);

    /* Attack likelihood via sigmoid */
    float normalized_fe = (total_fe - SECURITY_CL_FEP_SUSPICIOUS_THRESHOLD) /
                          SIGMOID_TEMPERATURE;
    bridge->fep_effects.attack_likelihood = sigmoid(normalized_fe);

    /* Classify severity */
    bridge->fep_effects.severity = classify_severity(total_fe);

    /* Precision effects */
    bridge->fep_effects.detection_precision = bridge->state.current_precision;
    bridge->fep_effects.sensitivity_multiplier =
        1.0f + (bridge->state.current_precision - 1.0f) * 0.5f;

    /* Select response via active inference */
    if (bridge->config.enable_active_inference) {
        security_cl_fep_response_t response;
        float urgency;
        security_cl_fep_select_response(bridge, &response, &urgency);
        bridge->fep_effects.response = response;
        bridge->fep_effects.response_urgency = urgency;

        /* Compute response parameters */
        bridge->fep_effects.ewc_boost_factor =
            (response >= SECURITY_CL_FEP_RESPONSE_EWC_BOOST) ?
            (1.0f + urgency * 2.0f) : 1.0f;
        bridge->fep_effects.lr_reduction_factor =
            (response >= SECURITY_CL_FEP_RESPONSE_LR_REDUCE) ?
            nimcp_clampf(1.0f - urgency * 0.5f, 0.1f, 1.0f) : 1.0f;
    } else {
        bridge->fep_effects.response = select_response_from_severity(
            bridge->fep_effects.severity
        );
        bridge->fep_effects.response_urgency =
            bridge->fep_effects.forgetting_severity_score;
        bridge->fep_effects.ewc_boost_factor = 1.0f;
        bridge->fep_effects.lr_reduction_factor = 1.0f;
    }

    bridge->fep_effects.last_update_ms = get_current_time_ms();
    bridge->fep_effects.valid = true;

    /* Update running averages */
    bridge->state.avg_free_energy =
        (1.0f - EMA_ALPHA) * bridge->state.avg_free_energy +
        EMA_ALPHA * total_fe;
    bridge->state.avg_surprise =
        (1.0f - EMA_ALPHA) * bridge->state.avg_surprise +
        EMA_ALPHA * bridge->fep_effects.surprise_level;
    if (total_fe > bridge->state.max_free_energy) {
        bridge->state.max_free_energy = total_fe;
    }

    /* Update statistics */
    bridge->state.update_count++;
    bridge->stats.total_updates++;
    bridge->stats.avg_free_energy = bridge->state.avg_free_energy;
    bridge->stats.max_free_energy = bridge->state.max_free_energy;
    bridge->stats.avg_surprise = bridge->state.avg_surprise;

    uint64_t elapsed = get_current_time_ms() - start_time;
    bridge->stats.avg_update_time_us =
        (bridge->stats.avg_update_time_us * (bridge->stats.total_updates - 1) +
         elapsed * 1000.0f) / bridge->stats.total_updates;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int security_cl_fep_update_from_detection(
    security_cl_fep_bridge_t* bridge,
    bool is_attack,
    security_cl_forgetting_type_t attack_type,
    float severity
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Update CL effects with attack info */
    if (is_attack) {
        bridge->cl_effects.attacks_detected++;
        bridge->cl_effects.attack_type = attack_type;
        bridge->cl_effects.attack_severity = severity;
        bridge->stats.attacks_detected++;
    }

    /* Update FEP if online learning enabled */
    if (bridge->config.enable_online_learning) {
        if (is_attack) {
            /* Attack = high surprise observation, increase precision */
            fep_update_precision(bridge->fep_system);
            bridge->state.current_precision *= 1.1f;
            if (bridge->state.current_precision > SECURITY_CL_FEP_MAX_PRECISION) {
                bridge->state.current_precision = SECURITY_CL_FEP_MAX_PRECISION;
            }
        } else {
            /* Normal = update beliefs */
            fep_update_beliefs(bridge->fep_system);
        }
    }

    bridge->state.detection_count++;
    bridge->stats.fep_detections++;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int security_cl_fep_apply_precision_modulation(security_cl_fep_bridge_t* bridge) {
    if (!bridge || !bridge->config.enable_precision_modulation) {
        return 0;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Adapt precision based on attack rate */
    float attack_rate = 0.0f;
    if (bridge->state.detection_count > 0) {
        attack_rate = (float)bridge->stats.attacks_detected /
                      (float)bridge->state.detection_count;
    }

    /* Target precision based on attack rate */
    float target_precision;
    if (attack_rate > 0.2f) {
        /* High attack rate -> high precision */
        target_precision = SECURITY_CL_FEP_MAX_PRECISION;
    } else if (attack_rate < 0.05f) {
        /* Low attack rate -> lower precision */
        target_precision = SECURITY_CL_FEP_DEFAULT_PRECISION * 0.5f;
    } else {
        target_precision = SECURITY_CL_FEP_DEFAULT_PRECISION;
    }

    /* Smooth adaptation */
    float lr = bridge->config.precision_learning_rate;
    bridge->state.current_precision =
        (1.0f - lr) * bridge->state.current_precision + lr * target_precision;
    bridge->state.current_precision = nimcp_clampf(
        bridge->state.current_precision,
        SECURITY_CL_FEP_MIN_PRECISION,
        SECURITY_CL_FEP_MAX_PRECISION
    );

    bridge->stats.precision_adaptations++;
    bridge->stats.current_precision = bridge->state.current_precision;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int security_cl_fep_select_response(
    security_cl_fep_bridge_t* bridge,
    security_cl_fep_response_t* response,
    float* urgency
) {
    if (!bridge || !response || !urgency) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_cl_fep_select_response: required parameter is NULL (bridge, response, urgency)");
        return -1;
    }

    /* Get current severity */
    security_cl_fep_severity_t severity = bridge->fep_effects.severity;
    float fe = bridge->fep_effects.current_free_energy;

    /* Map severity to response */
    *response = select_response_from_severity(severity);

    /* Compute urgency based on free energy */
    *urgency = nimcp_clampf(fe / SECURITY_CL_FEP_CRITICAL_THRESHOLD, 0.0f, 1.0f);

    /* Track response */
    if (*response != SECURITY_CL_FEP_RESPONSE_NONE) {
        bridge->stats.responses_issued++;
        bridge->state.last_response = *response;
        bridge->state.last_response_time_ms = get_current_time_ms();

        switch (*response) {
            case SECURITY_CL_FEP_RESPONSE_EWC_BOOST:
                bridge->stats.ewc_boosts++;
                break;
            case SECURITY_CL_FEP_RESPONSE_LR_REDUCE:
                bridge->stats.lr_reductions++;
                break;
            case SECURITY_CL_FEP_RESPONSE_REPLAY_LOCK:
                bridge->stats.replay_locks++;
                break;
            case SECURITY_CL_FEP_RESPONSE_CONSOLIDATE:
                bridge->stats.emergency_consolidations++;
                break;
            default:
                break;
        }
    }

    return 0;
}

/* ============================================================================
 * Retention-FEP Mapping Implementation
 * ============================================================================ */

float security_cl_fep_retention_to_fe(
    const security_cl_fep_bridge_t* bridge,
    uint32_t task_id,
    float current_retention,
    float expected_retention
) {
    if (!bridge) {
        return 0.0f;
    }

    (void)task_id; /* May use for task-specific baseline in future */

    /* Retention loss = 1 - (current / expected) */
    float retention_loss = 1.0f - (current_retention / (expected_retention + 0.001f));
    if (retention_loss < 0.0f) {
        retention_loss = 0.0f;
    }

    /* Scale by weight and precision */
    float fe_contribution = retention_loss *
                            bridge->config.retention_loss_weight *
                            bridge->state.current_precision *
                            SECURITY_CL_FEP_RETENTION_SCALE;

    return fe_contribution;
}

float security_cl_fep_drift_to_error(
    const security_cl_fep_bridge_t* bridge,
    float drift_score,
    security_cl_drift_type_t drift_type
) {
    if (!bridge) {
        return 0.0f;
    }

    /* Base error from drift magnitude */
    float error = drift_score * bridge->config.drift_weight *
                  SECURITY_CL_FEP_DRIFT_SCALE;

    /* Amplify for adversarial drift */
    if (drift_type == SECURITY_CL_DRIFT_ADVERSARIAL ||
        drift_type == SECURITY_CL_DRIFT_MANIPULATION) {
        error *= 2.0f;
    }

    /* Apply precision weighting */
    error *= bridge->state.current_precision;

    return error;
}

float security_cl_fep_replay_to_surprise(
    const security_cl_fep_bridge_t* bridge,
    uint32_t integrity_failures,
    bool is_poisoned
) {
    if (!bridge) {
        return 0.0f;
    }

    float surprise = 0.0f;

    /* Base surprise from integrity failures */
    if (integrity_failures > 0) {
        /* Surprise = -ln(P(failures)) approximated logarithmically */
        surprise = logf(1.0f + (float)integrity_failures) *
                   bridge->config.replay_anomaly_weight;
    }

    /* Confirmed poisoning = very high surprise */
    if (is_poisoned) {
        surprise += SECURITY_CL_FEP_REPLAY_SCALE * 2.0f;
    }

    /* Apply precision */
    surprise *= bridge->state.current_precision;

    return surprise;
}

/* ============================================================================
 * Detection Implementation
 * ============================================================================ */

bool security_cl_fep_detect_attack(
    security_cl_fep_bridge_t* bridge,
    security_cl_fep_severity_t* severity,
    float* confidence
) {
    if (!bridge || !bridge->fep_effects.valid) {
        if (severity) *severity = SECURITY_CL_FEP_SEVERITY_NONE;
        if (confidence) *confidence = 0.0f;
        return false;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    float fe = bridge->fep_effects.current_free_energy;
    security_cl_fep_severity_t sev = classify_severity(fe);

    if (severity) {
        *severity = sev;
    }

    if (confidence) {
        *confidence = bridge->fep_effects.attack_likelihood;
    }

    bool is_attack = (sev >= SECURITY_CL_FEP_SEVERITY_MEDIUM);

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return is_attack;
}

float security_cl_fep_get_attack_likelihood(
    const security_cl_fep_bridge_t* bridge
) {
    if (!bridge || !bridge->fep_effects.valid) {
        return 0.0f;
    }

    return bridge->fep_effects.attack_likelihood;
}

int security_cl_fep_report_false_positive(security_cl_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    bridge->stats.false_positives++;

    /* Reduce precision to prevent similar FPs */
    if (bridge->config.learn_from_false_positives) {
        bridge->state.current_precision *= 0.9f;
        if (bridge->state.current_precision < SECURITY_CL_FEP_MIN_PRECISION) {
            bridge->state.current_precision = SECURITY_CL_FEP_MIN_PRECISION;
        }
        bridge->stats.precision_adaptations++;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int security_cl_fep_get_effects(
    const security_cl_fep_bridge_t* bridge,
    security_cl_fep_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_cl_fep_get_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    *effects = bridge->fep_effects;
    return 0;
}

int security_cl_fep_get_cl_effects(
    const security_cl_fep_bridge_t* bridge,
    fep_security_cl_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_cl_fep_get_cl_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    *effects = bridge->cl_effects;
    return 0;
}

float security_cl_fep_get_free_energy(const security_cl_fep_bridge_t* bridge) {
    if (!bridge || !bridge->fep_effects.valid) {
        return -1.0f;
    }

    return bridge->fep_effects.current_free_energy;
}

float security_cl_fep_get_surprise(const security_cl_fep_bridge_t* bridge) {
    if (!bridge || !bridge->fep_effects.valid) {
        return -1.0f;
    }

    return bridge->fep_effects.surprise_level;
}

float security_cl_fep_get_precision(const security_cl_fep_bridge_t* bridge) {
    if (!bridge) {
        return -1.0f;
    }

    return bridge->state.current_precision;
}

int security_cl_fep_get_stats(
    const security_cl_fep_bridge_t* bridge,
    security_cl_fep_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_cl_fep_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    *stats = bridge->stats;
    stats->uptime_ms = get_current_time_ms();
    return 0;
}

void security_cl_fep_print_summary(const security_cl_fep_bridge_t* bridge) {
    if (!bridge) {
        printf("Security CL FEP Bridge: NULL\n");
        return;
    }

    printf("\n");
    printf("=== Security CL FEP Bridge Summary ===\n");
    printf("State: %s\n", bridge->state.active ? "ACTIVE" : "INACTIVE");
    printf("\n");

    printf("Free Energy:\n");
    printf("  Current:     %.3f\n", bridge->fep_effects.current_free_energy);
    printf("  Average:     %.3f\n", bridge->state.avg_free_energy);
    printf("  Maximum:     %.3f\n", bridge->state.max_free_energy);
    printf("  Severity:    %s\n",
           security_cl_fep_severity_to_string(bridge->fep_effects.severity));
    printf("\n");

    printf("Detection:\n");
    printf("  Precision:   %.3f\n", bridge->state.current_precision);
    printf("  Attack Prob: %.1f%%\n", bridge->fep_effects.attack_likelihood * 100.0f);
    printf("  Surprise:    %.3f\n", bridge->fep_effects.surprise_level);
    printf("\n");

    printf("Response:\n");
    printf("  Type:        %s\n",
           security_cl_fep_response_to_string(bridge->fep_effects.response));
    printf("  Urgency:     %.1f%%\n", bridge->fep_effects.response_urgency * 100.0f);
    printf("  EWC Boost:   %.2fx\n", bridge->fep_effects.ewc_boost_factor);
    printf("  LR Factor:   %.2fx\n", bridge->fep_effects.lr_reduction_factor);
    printf("\n");

    printf("Statistics:\n");
    printf("  Updates:     %lu\n", (unsigned long)bridge->stats.total_updates);
    printf("  Detections:  %lu\n", (unsigned long)bridge->stats.fep_detections);
    printf("  Attacks:     %lu\n", (unsigned long)bridge->stats.attacks_detected);
    printf("  False Pos:   %lu\n", (unsigned long)bridge->stats.false_positives);
    printf("  Responses:   %lu\n", (unsigned long)bridge->stats.responses_issued);
    printf("\n");
    printf("Bio-Async: %s\n",
           bridge->base.bio_async_enabled ? "CONNECTED" : "DISCONNECTED");
    printf("======================================\n\n");
}

/* ============================================================================
 * Bio-Async Implementation
 * ============================================================================ */

int security_cl_fep_connect_bio_async(security_cl_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (bridge->base.bio_async_enabled) {
        return 0; /* Already connected */
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SECURITY_CORE_FEP,
        .module_name = "security_cl_fep_bridge",
        .inbox_capacity = bridge->config.bio_inbox_capacity,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Security CL FEP bridge connected to bio-async");
    }

    return 0;
}

int security_cl_fep_disconnect_bio_async(security_cl_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) {
        return 0;
    }

    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    bridge->base.bio_ctx = NULL;

    NIMCP_LOGGING_INFO("Security CL FEP bridge disconnected from bio-async");
    return 0;
}

bool security_cl_fep_is_bio_async_connected(const security_cl_fep_bridge_t* bridge) {
    return bridge ? bridge->base.bio_async_enabled : false;
}

/* ============================================================================
 * String Conversion Implementation
 * ============================================================================ */

const char* security_cl_fep_severity_to_string(security_cl_fep_severity_t severity) {
    switch (severity) {
        case SECURITY_CL_FEP_SEVERITY_NONE:     return "NONE";
        case SECURITY_CL_FEP_SEVERITY_LOW:      return "LOW";
        case SECURITY_CL_FEP_SEVERITY_MEDIUM:   return "MEDIUM";
        case SECURITY_CL_FEP_SEVERITY_HIGH:     return "HIGH";
        case SECURITY_CL_FEP_SEVERITY_CRITICAL: return "CRITICAL";
        default:                                 return "UNKNOWN";
    }
}

const char* security_cl_fep_response_to_string(security_cl_fep_response_t response) {
    switch (response) {
        case SECURITY_CL_FEP_RESPONSE_NONE:        return "NONE";
        case SECURITY_CL_FEP_RESPONSE_MONITOR:     return "MONITOR";
        case SECURITY_CL_FEP_RESPONSE_EWC_BOOST:   return "EWC_BOOST";
        case SECURITY_CL_FEP_RESPONSE_LR_REDUCE:   return "LR_REDUCE";
        case SECURITY_CL_FEP_RESPONSE_REPLAY_LOCK: return "REPLAY_LOCK";
        case SECURITY_CL_FEP_RESPONSE_CONSOLIDATE: return "CONSOLIDATE";
        case SECURITY_CL_FEP_RESPONSE_ROLLBACK:    return "ROLLBACK";
        default:                                    return "UNKNOWN";
    }
}
