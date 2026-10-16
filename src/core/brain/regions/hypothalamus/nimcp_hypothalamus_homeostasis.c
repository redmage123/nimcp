/**
 * @file nimcp_hypothalamus_homeostasis.c
 * @brief Implementation of Homeostatic Control with Alignment-Safe Setpoints
 *
 * WHAT: PI/PD control for homeostatic regulation with Byrnes alignment
 * WHY:  Setpoints ARE the reward function - careful design enables alignment
 * HOW:  Classical control theory applied to biological homeostasis
 *
 * @version Phase 2: Homeostasis System
 * @date 2026-01-04
 */

#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_homeostasis.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_learning_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(hypothalamus_homeostasis, MESH_ADAPTER_CATEGORY_COGNITIVE)


/*=============================================================================
 * LOGGING MODULE IDENTIFIER
 *===========================================================================*/

#define HOMEO_LOG_MODULE "HYPO_HOMEO"

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

#define US_PER_SECOND (1000000ULL)

/* Default PID gains (conservative) */
#define DEFAULT_KP 0.5f
#define DEFAULT_KI 0.1f
#define DEFAULT_KD 0.05f
#define DEFAULT_INTEGRAL_MAX 1.0f
#define DEFAULT_DERIVATIVE_FILTER 0.1f

/* Variable defaults */
static const struct {
    hypo_variable_type_t type;
    const char* name;
    const char* unit;
    float setpoint;
    float setpoint_min;
    float setpoint_max;
    float warning_threshold;
    float critical_threshold;
    float alignment_weight;
} VARIABLE_DEFAULTS[HYPO_VAR_COUNT] = {
    {HYPO_VAR_TEMPERATURE,   "Temperature",     "C",     37.0f,  35.0f, 39.0f, 0.5f, 1.5f, 0.8f},
    {HYPO_VAR_GLUCOSE,       "Glucose",         "mg/dL", 90.0f,  70.0f, 120.0f, 20.0f, 40.0f, 0.8f},
    {HYPO_VAR_OSMOLARITY,    "Osmolarity",      "mOsm/L", 285.0f, 275.0f, 295.0f, 5.0f, 15.0f, 0.6f},
    {HYPO_VAR_PH,            "pH",              "",      7.4f,   7.35f, 7.45f, 0.02f, 0.05f, 0.9f},
    {HYPO_VAR_OXYGEN,        "Oxygen Sat",      "%",     98.0f,  90.0f, 100.0f, 3.0f, 8.0f, 0.95f},
    {HYPO_VAR_CO2,           "CO2",             "mmHg",  40.0f,  35.0f, 45.0f, 3.0f, 8.0f, 0.7f},
    {HYPO_VAR_SLEEP_PRESSURE,"Sleep Pressure",  "",      0.3f,   0.0f, 1.0f, 0.3f, 0.5f, 0.5f},
    {HYPO_VAR_AROUSAL,       "Arousal",         "",      0.5f,   0.1f, 0.9f, 0.2f, 0.4f, 0.4f},
    {HYPO_VAR_SOCIAL,        "Social Need",     "",      0.5f,   0.0f, 1.0f, 0.3f, 0.5f, 0.6f},
    {HYPO_VAR_CURIOSITY,     "Curiosity",       "",      0.5f,   0.0f, 1.0f, 0.3f, 0.5f, 0.5f}
};

/*=============================================================================
 * INTERNAL STRUCTURE
 *===========================================================================*/

struct hypo_homeostasis {
    /* Configuration */
    hypo_homeostasis_config_t config;

    /* Homeostatic variables */
    hypo_homeostatic_var_t variables[HYPO_VAR_COUNT];

    /* Current reward */
    hypo_alignment_reward_t current_reward;

    /* Statistics */
    hypo_homeostasis_stats_t stats;

    /* Timing */
    uint64_t last_update_us;
    uint64_t total_runtime_us;

    /* Thread safety */
    nimcp_mutex_t* mutex;
    bool mutex_owned;
};

/**
 * @brief Initialize a PID controller
 */
static void init_pid_controller(hypo_pid_controller_t* ctrl,
                                 hypo_controller_type_t type,
                                 const hypo_pid_gains_t* gains,
                                 float setpoint) {
    memset(ctrl, 0, sizeof(*ctrl));

    ctrl->type = type;

    if (gains) {
        ctrl->gains = *gains;
    } else {
        ctrl->gains.kp = DEFAULT_KP;
        ctrl->gains.ki = DEFAULT_KI;
        ctrl->gains.kd = DEFAULT_KD;
        ctrl->gains.integral_max = DEFAULT_INTEGRAL_MAX;
        ctrl->gains.derivative_filter = DEFAULT_DERIVATIVE_FILTER;
    }

    ctrl->setpoint = setpoint;
    ctrl->current_value = setpoint;  /* Start at setpoint */
    ctrl->error = 0.0f;
    ctrl->integral = 0.0f;
    ctrl->derivative = 0.0f;
    ctrl->last_error = 0.0f;
    ctrl->output = 0.0f;
    ctrl->output_raw = 0.0f;
    ctrl->output_min = -1.0f;
    ctrl->output_max = 1.0f;
    ctrl->saturated = false;
}

/**
 * @brief Update PID controller
 */
static float update_pid_controller(hypo_pid_controller_t* ctrl,
                                    float current_value,
                                    float dt) {
    if (dt <= 0.0f) return ctrl->output;

    ctrl->current_value = current_value;
    ctrl->error = ctrl->setpoint - current_value;

    /* Proportional term */
    float p_term = ctrl->gains.kp * ctrl->error;

    /* Integral term (with anti-windup) */
    float i_term = 0.0f;
    if (ctrl->type == HYPO_CTRL_PI || ctrl->type == HYPO_CTRL_PID) {
        ctrl->integral += ctrl->error * dt;
        ctrl->integral = nimcp_clampf(ctrl->integral,
                               -ctrl->gains.integral_max,
                               ctrl->gains.integral_max);
        i_term = ctrl->gains.ki * ctrl->integral;
    }

    /* Derivative term (with filtering) */
    float d_term = 0.0f;
    if (ctrl->type == HYPO_CTRL_PD || ctrl->type == HYPO_CTRL_PID) {
        float raw_derivative = (ctrl->error - ctrl->last_error) / dt;

        /* Low-pass filter on derivative */
        float alpha = ctrl->gains.derivative_filter;
        ctrl->derivative = alpha * raw_derivative +
                           (1.0f - alpha) * ctrl->derivative;

        d_term = ctrl->gains.kd * ctrl->derivative;
    }

    ctrl->last_error = ctrl->error;

    /* Compute output */
    ctrl->output_raw = p_term + i_term + d_term;
    ctrl->output = nimcp_clampf(ctrl->output_raw, ctrl->output_min, ctrl->output_max);

    /* Track saturation */
    ctrl->saturated = (fabsf(ctrl->output_raw) > fabsf(ctrl->output));

    return ctrl->output;
}

/**
 * @brief Initialize a homeostatic variable
 */
static void init_homeostatic_var(hypo_homeostatic_var_t* var,
                                  hypo_variable_type_t type,
                                  hypo_controller_type_t ctrl_type,
                                  const hypo_pid_gains_t* gains) {
    memset(var, 0, sizeof(*var));

    var->type = type;
    var->name = VARIABLE_DEFAULTS[type].name;
    var->unit = VARIABLE_DEFAULTS[type].unit;

    /* Setpoint configuration */
    var->setpoint = VARIABLE_DEFAULTS[type].setpoint;
    var->setpoint_min = VARIABLE_DEFAULTS[type].setpoint_min;
    var->setpoint_max = VARIABLE_DEFAULTS[type].setpoint_max;
    var->setpoint_lock = HYPO_LOCK_SOFT;  /* Physiological can be unlocked */

    /* Current state */
    var->value = var->setpoint;  /* Start at setpoint */
    var->error = 0.0f;
    var->error_rate = 0.0f;

    /* Initialize controller */
    init_pid_controller(&var->controller, ctrl_type, gains, var->setpoint);

    /* Reward contribution */
    var->reward_contribution = 0.0f;
    var->alignment_weight = VARIABLE_DEFAULTS[type].alignment_weight;

    /* Thresholds */
    var->warning_threshold = VARIABLE_DEFAULTS[type].warning_threshold;
    var->critical_threshold = VARIABLE_DEFAULTS[type].critical_threshold;
    var->warning_active = false;
    var->critical_active = false;
}

/**
 * @brief Log setpoint access for audit
 */
static void log_setpoint_access(hypo_homeostasis_handle_t* system,
                                 hypo_variable_type_t var_type,
                                 uint32_t modifier_id,
                                 bool success) {
    if (!system) return;

    system->stats.setpoint_violations += (success ? 0 : 1);

    LOG_DEBUG(HOMEO_LOG_MODULE,
              "Setpoint access: var=%s, modifier=%u, result=%s",
              VARIABLE_DEFAULTS[var_type].name,
              modifier_id,
              success ? "ALLOWED" : "DENIED");
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

hypo_homeostasis_config_t hypo_homeostasis_default_config(void) {
    hypo_homeostasis_config_t config;
    memset(&config, 0, sizeof(config));

    /* Controller defaults */
    config.default_controller = HYPO_CTRL_PI;  /* PI is stable */
    config.default_gains.kp = DEFAULT_KP;
    config.default_gains.ki = DEFAULT_KI;
    config.default_gains.kd = DEFAULT_KD;
    config.default_gains.integral_max = DEFAULT_INTEGRAL_MAX;
    config.default_gains.derivative_filter = DEFAULT_DERIVATIVE_FILTER;

    /* Update rate */
    config.update_rate_hz = 60.0f;

    /* Alignment configuration - get from drive system */
    config.alignment_setpoints = (hypo_setpoint_config_t){
        .human_wellbeing_weight = 1.0f,
        .harm_avoidance_weight = 1.0f,
        .honesty_weight = 0.9f,
        .helpfulness_weight = 0.8f,
        .reward_gain = 1.0f,
        .punishment_gain = 1.0f,
        .temporal_discount = NIMCP_REWARD_DISCOUNT_DEFAULT,
        .setpoints_lock = HYPO_LOCK_SOFT,
        .alignment_lock = HYPO_LOCK_HARD  /* Alignment always locked */
    };

    /* Safety */
    config.enable_warning_alerts = true;
    config.enable_critical_alerts = true;
    config.enable_reward_logging = false;

    return config;
}

hypo_homeostasis_handle_t* hypo_homeostasis_create(
    const hypo_homeostasis_config_t* config) {

    LOG_INFO(HOMEO_LOG_MODULE, "Creating homeostasis system");

    hypo_homeostasis_handle_t* system = (hypo_homeostasis_handle_t*)nimcp_calloc(
        1, sizeof(hypo_homeostasis_handle_t));
    if (!system) {
        LOG_ERROR(HOMEO_LOG_MODULE, "Failed to allocate homeostasis memory");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hypo_homeostasis_create: allocation failed");
        return NULL;
    }

    /* Set configuration */
    if (config) {
        system->config = *config;
    } else {
        system->config = hypo_homeostasis_default_config();
    }

    /* Create mutex */
    mutex_attr_t mutex_attr = {.type = MUTEX_TYPE_RECURSIVE};
    system->mutex = nimcp_mutex_create(&mutex_attr);
    if (!system->mutex) {
        nimcp_free(system);
        return NULL;
    }
    system->mutex_owned = true;

    /* Initialize all homeostatic variables */
    for (int i = 0; i < HYPO_VAR_COUNT; i++) {
        init_homeostatic_var(&system->variables[i],
                              (hypo_variable_type_t)i,
                              system->config.default_controller,
                              &system->config.default_gains);
    }

    /* Initialize reward */
    memset(&system->current_reward, 0, sizeof(system->current_reward));

    /* Initialize statistics */
    memset(&system->stats, 0, sizeof(system->stats));

    LOG_INFO(HOMEO_LOG_MODULE,
             "Homeostasis system created: %d variables, controller=%s",
             HYPO_VAR_COUNT,
             hypo_controller_type_string(system->config.default_controller));

    return system;
}

void hypo_homeostasis_destroy(hypo_homeostasis_handle_t* system) {
    if (!system) return;

    LOG_INFO(HOMEO_LOG_MODULE, "Destroying homeostasis system");

    if (system->mutex && system->mutex_owned) {
        nimcp_mutex_destroy(system->mutex);
    }

    nimcp_free(system);
}

bool hypo_homeostasis_reset(hypo_homeostasis_handle_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_homeostasis_reset: system is NULL");
        return false;
    }

    if (system->mutex) nimcp_mutex_lock(system->mutex);

    /* Reset variable values to setpoints (preserve setpoints) */
    for (int i = 0; i < HYPO_VAR_COUNT; i++) {
        hypo_homeostatic_var_t* var = &system->variables[i];
        var->value = var->setpoint;
        var->error = 0.0f;
        var->error_rate = 0.0f;
        var->reward_contribution = 0.0f;
        var->warning_active = false;
        var->critical_active = false;

        /* Reset controller */
        var->controller.current_value = var->setpoint;
        var->controller.error = 0.0f;
        var->controller.integral = 0.0f;
        var->controller.derivative = 0.0f;
        var->controller.last_error = 0.0f;
        var->controller.output = 0.0f;
        var->controller.saturated = false;
    }

    /* Reset reward */
    memset(&system->current_reward, 0, sizeof(system->current_reward));

    /* Reset timing */
    system->last_update_us = 0;
    system->total_runtime_us = 0;

    if (system->mutex) nimcp_mutex_unlock(system->mutex);

    LOG_DEBUG(HOMEO_LOG_MODULE, "Homeostasis reset complete");
    return true;
}

/*=============================================================================
 * VARIABLE MANAGEMENT
 *===========================================================================*/

bool hypo_homeostasis_set_value(hypo_homeostasis_handle_t* system,
                                 hypo_variable_type_t var_type,
                                 float value) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_homeostasis_set_value: system is NULL");
        return false;
    }
    if (var_type >= HYPO_VAR_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_homeostasis_set_value: invalid var_type");
        return false;
    }

    if (system->mutex) nimcp_mutex_lock(system->mutex);

    system->variables[var_type].value = value;

    if (system->mutex) nimcp_mutex_unlock(system->mutex);
    return true;
}

bool hypo_homeostasis_get_variable(const hypo_homeostasis_handle_t* system,
                                    hypo_variable_type_t var_type,
                                    hypo_homeostatic_var_t* var) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_homeostasis_get_variable: system is NULL");
        return false;
    }
    if (!var) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_homeostasis_get_variable: var is NULL");
        return false;
    }
    if (var_type >= HYPO_VAR_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_homeostasis_get_variable: invalid var_type");
        return false;
    }

    *var = system->variables[var_type];
    return true;
}

bool hypo_homeostasis_modify_setpoint(hypo_homeostasis_handle_t* system,
                                       hypo_variable_type_t var_type,
                                       float new_setpoint,
                                       uint32_t modifier_id) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_homeostasis_modify_setpoint: system is NULL");
        return false;
    }
    if (var_type >= HYPO_VAR_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_homeostasis_modify_setpoint: invalid var_type");
        return false;
    }

    bool success = false;

    if (system->mutex) nimcp_mutex_lock(system->mutex);

    hypo_homeostatic_var_t* var = &system->variables[var_type];

    /* Check lock state */
    if (var->setpoint_lock == HYPO_LOCK_UNLOCKED) {
        /* Clamp to valid range */
        new_setpoint = nimcp_clampf(new_setpoint, var->setpoint_min, var->setpoint_max);

        var->setpoint = new_setpoint;
        var->controller.setpoint = new_setpoint;

        success = true;

        LOG_DEBUG(HOMEO_LOG_MODULE,
                  "Setpoint modified: %s = %.2f (modifier=%u)",
                  var->name, new_setpoint, modifier_id);
    }

    log_setpoint_access(system, var_type, modifier_id, success);

    if (system->mutex) nimcp_mutex_unlock(system->mutex);

    return success;
}

/*=============================================================================
 * UPDATE AND CONTROL
 *===========================================================================*/

bool hypo_homeostasis_update(hypo_homeostasis_handle_t* system,
                              uint64_t delta_time_us) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_homeostasis_update: system is NULL");
        return false;
    }

    if (system->mutex) nimcp_mutex_lock(system->mutex);

    float dt = (float)delta_time_us / (float)US_PER_SECOND;

    /* Update each homeostatic variable */
    for (int i = 0; i < HYPO_VAR_COUNT; i++) {
        hypo_homeostatic_var_t* var = &system->variables[i];

        /* Store previous error for rate calculation */
        float prev_error = var->error;

        /* Update controller */
        update_pid_controller(&var->controller, var->value, dt);

        /* Update variable state */
        var->error = var->setpoint - var->value;
        var->error_rate = (dt > 0) ? (var->error - prev_error) / dt : 0.0f;

        /* Compute reward contribution */
        float abs_error = fabsf(var->error);
        var->reward_contribution = var->alignment_weight *
                                    (1.0f - nimcp_clamp01(abs_error / var->critical_threshold));

        /* Check thresholds */
        bool was_warning = var->warning_active;
        bool was_critical = var->critical_active;

        var->warning_active = abs_error > var->warning_threshold;
        var->critical_active = abs_error > var->critical_threshold;

        /* Track alerts */
        if (var->warning_active && !was_warning) {
            system->stats.warnings_triggered++;
            if (system->config.enable_warning_alerts) {
                LOG_WARNING(HOMEO_LOG_MODULE,
                            "Warning: %s deviation %.2f (threshold %.2f)",
                            var->name, abs_error, var->warning_threshold);
            }
        }

        if (var->critical_active && !was_critical) {
            system->stats.criticals_triggered++;
            if (system->config.enable_critical_alerts) {
                LOG_ERROR(HOMEO_LOG_MODULE,
                          "CRITICAL: %s deviation %.2f (threshold %.2f)",
                          var->name, abs_error, var->critical_threshold);
            }
        }

        /* Track saturation */
        if (var->controller.saturated) {
            system->stats.saturation_events++;
        }
    }

    /* Update timing */
    system->last_update_us += delta_time_us;
    system->total_runtime_us += delta_time_us;
    system->stats.updates_processed++;

    if (system->mutex) nimcp_mutex_unlock(system->mutex);

    return true;
}

float hypo_homeostasis_get_output(const hypo_homeostasis_handle_t* system,
                                   hypo_variable_type_t var_type) {
    if (!system || var_type >= HYPO_VAR_COUNT) return 0.0f;
    return system->variables[var_type].controller.output;
}

bool hypo_homeostasis_get_all_outputs(const hypo_homeostasis_handle_t* system,
                                       float* outputs) {
    if (!system || !outputs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_homeostasis_get_all_outputs: system or outputs is NULL");
        return false;
    }

    for (int i = 0; i < HYPO_VAR_COUNT; i++) {
        outputs[i] = system->variables[i].controller.output;
    }
    return true;
}

/*=============================================================================
 * REWARD COMPUTATION (ALIGNMENT-CRITICAL)
 *===========================================================================*/

bool hypo_homeostasis_compute_reward(const hypo_homeostasis_handle_t* system,
                                      hypo_alignment_reward_t* reward) {
    if (!system || !reward) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_homeostasis_compute_reward: system or reward is NULL");
        return false;
    }

    memset(reward, 0, sizeof(*reward));

    const hypo_setpoint_config_t* align = &system->config.alignment_setpoints;

    /* Compute homeostatic reward from variable states */
    float total_reward_contribution = 0.0f;
    float total_penalty = 0.0f;
    float total_weight = 0.0f;

    for (int i = 0; i < HYPO_VAR_COUNT; i++) {
        const hypo_homeostatic_var_t* var = &system->variables[i];

        total_reward_contribution += var->reward_contribution;
        total_weight += var->alignment_weight;

        /* Penalty for critical deviations */
        if (var->critical_active) {
            total_penalty += var->alignment_weight * 0.5f;
        } else if (var->warning_active) {
            total_penalty += var->alignment_weight * 0.1f;
        }
    }

    /* Normalize */
    if (total_weight > 0.0f) {
        reward->homeostatic_reward = total_reward_contribution / total_weight;
        reward->homeostatic_penalty = total_penalty / total_weight;
    }

    /* Alignment components (Byrnes' explicit parameters) */
    reward->wellbeing_bonus = reward->homeostatic_reward * align->human_wellbeing_weight * 0.1f;
    reward->harm_penalty = reward->homeostatic_penalty * align->harm_avoidance_weight;
    reward->honesty_bonus = align->honesty_weight * 0.05f;  /* Baseline honesty bonus */
    reward->helpfulness_bonus = align->helpfulness_weight * 0.05f;

    /* Total reward */
    reward->total_reward = align->reward_gain * (
        reward->homeostatic_reward +
        reward->wellbeing_bonus +
        reward->honesty_bonus +
        reward->helpfulness_bonus -
        reward->harm_penalty * align->punishment_gain
    );

    /* Clamp to [-1, +1] */
    reward->total_reward = nimcp_clampf(reward->total_reward, -1.0f, 1.0f);

    /* RPE (simple model) */
    reward->reward_prediction_error = reward->total_reward - system->current_reward.total_reward;

    /* Convert to dopamine (nonlinear) */
    if (reward->total_reward >= 0) {
        reward->dopamine_signal = nimcp_clamp01(0.5f + reward->total_reward * 0.5f);
    } else {
        reward->dopamine_signal = nimcp_clamp01(0.5f + reward->total_reward * 0.3f);
    }

    /* Learning rate modulation */
    reward->learning_rate_mod = nimcp_clamp01(0.5f + fabsf(reward->reward_prediction_error) * 0.5f);

    /* Update statistics */
    ((hypo_homeostasis_handle_t*)system)->stats.total_rewards_computed++;

    float* avg = &((hypo_homeostasis_handle_t*)system)->stats.avg_reward;
    uint64_t n = system->stats.total_rewards_computed;
    *avg = (*avg * (n - 1) + reward->total_reward) / n;

    if (reward->total_reward > system->stats.max_reward) {
        ((hypo_homeostasis_handle_t*)system)->stats.max_reward = reward->total_reward;
    }
    if (reward->total_reward < system->stats.min_reward || n == 1) {
        ((hypo_homeostasis_handle_t*)system)->stats.min_reward = reward->total_reward;
    }

    /* Store for next RPE calculation */
    ((hypo_homeostasis_handle_t*)system)->current_reward = *reward;

    return true;
}

float hypo_homeostasis_get_reward(const hypo_homeostasis_handle_t* system) {
    if (!system) return 0.0f;
    return system->current_reward.total_reward;
}

bool hypo_homeostasis_check_alignment(const hypo_homeostasis_handle_t* system,
                                       float* alignment_score) {
    if (!system) {
        if (alignment_score) *alignment_score = 0.0f;
        return false;
    }

    const hypo_setpoint_config_t* align = &system->config.alignment_setpoints;

    /* Compute alignment score from explicit weights */
    float score = 0.0f;
    score += align->human_wellbeing_weight * 0.3f;
    score += align->harm_avoidance_weight * 0.3f;
    score += align->honesty_weight * 0.2f;
    score += align->helpfulness_weight * 0.2f;

    score = nimcp_clamp01(score);

    if (alignment_score) {
        *alignment_score = score;
    }

    return score >= 0.7f;
}

/*=============================================================================
 * PID CONTROLLER TUNING
 *===========================================================================*/

bool hypo_homeostasis_set_gains(hypo_homeostasis_handle_t* system,
                                 hypo_variable_type_t var_type,
                                 const hypo_pid_gains_t* gains) {
    if (!system || !gains || var_type >= HYPO_VAR_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_homeostasis_set_gains: required parameter is NULL (system, gains)");
        return false;
    }

    if (system->mutex) nimcp_mutex_lock(system->mutex);

    system->variables[var_type].controller.gains = *gains;

    if (system->mutex) nimcp_mutex_unlock(system->mutex);

    LOG_DEBUG(HOMEO_LOG_MODULE,
              "PID gains updated for %s: kp=%.2f, ki=%.2f, kd=%.2f",
              system->variables[var_type].name,
              gains->kp, gains->ki, gains->kd);

    return true;
}

bool hypo_homeostasis_get_gains(const hypo_homeostasis_handle_t* system,
                                 hypo_variable_type_t var_type,
                                 hypo_pid_gains_t* gains) {
    if (!system || !gains || var_type >= HYPO_VAR_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_homeostasis_get_gains: required parameter is NULL (system, gains)");
        return false;
    }

    *gains = system->variables[var_type].controller.gains;
    return true;
}

/*=============================================================================
 * STATISTICS AND DIAGNOSTICS
 *===========================================================================*/

bool hypo_homeostasis_get_stats(const hypo_homeostasis_handle_t* system,
                                 hypo_homeostasis_stats_t* stats) {
    if (!system || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_homeostasis_get_stats: required parameter is NULL (system, stats)");
        return false;
    }

    *stats = system->stats;
    return true;
}

const char* hypo_variable_type_string(hypo_variable_type_t var_type) {
    if (var_type >= HYPO_VAR_COUNT) return "UNKNOWN";
    return VARIABLE_DEFAULTS[var_type].name;
}

const char* hypo_controller_type_string(hypo_controller_type_t type) {
    switch (type) {
        case HYPO_CTRL_PROPORTIONAL: return "P";
        case HYPO_CTRL_PI:           return "PI";
        case HYPO_CTRL_PD:           return "PD";
        case HYPO_CTRL_PID:          return "PID";
        case HYPO_CTRL_ADAPTIVE:     return "ADAPTIVE";
        default:                     return "UNKNOWN";
    }
}
