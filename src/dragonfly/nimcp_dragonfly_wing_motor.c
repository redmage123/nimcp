/**
 * @file nimcp_dragonfly_wing_motor.c
 * @brief Four-Wing Motor Pattern Generator Implementation
 *
 * WHAT: Generates wing motor patterns for flight control
 * WHY:  Enables biologically-realistic flight dynamics
 * HOW:  Central Pattern Generator (CPG) with phase coordination
 *
 * @author NIMCP Team
 * @date 2024-12-29
 */

#include "dragonfly/nimcp_dragonfly_wing_motor.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_math_constants.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(dragonfly_wing_motor)

//=============================================================================
// Constants
//=============================================================================


#define TWO_PI (2.0f * (float)M_PI)

//=============================================================================
// Local Helpers
//=============================================================================

static inline uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static inline float wrap_phase(float phase) {
    while (phase >= NIMCP_TWO_PI_F) phase -= NIMCP_TWO_PI_F;
    while (phase < 0.0f) phase += NIMCP_TWO_PI_F;
    return phase;
}

//=============================================================================
// Internal Structure
//=============================================================================

struct dragonfly_wing_motor_s {
    /* Configuration */
    wing_motor_config_t config;

    /* Flight state */
    flight_mode_t mode;
    flight_dynamics_t desired_dynamics;

    /* Wing states */
    wing_state_t wings[WING_COUNT];

    /* CPG state */
    float phase[WING_COUNT];
    float frequency[WING_COUNT];
    float amplitude[WING_COUNT];

    /* Output */
    float net_thrust;
    float net_torque[3];

    /* Energy tracking */
    float current_power_w;
    float total_energy_j;

    /* Statistics */
    wing_motor_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Timing */
    uint64_t creation_time_us;
    uint64_t last_step_us;
};

//=============================================================================
// Configuration Functions
//=============================================================================

wing_motor_config_t wing_motor_default_config(void) {
    wing_motor_config_t config = {
        /* Wing geometry */
        .wing_length_m = 0.035f,
        .wing_chord_m = 0.008f,
        .wing_mass_kg = 0.00001f,

        /* CPG parameters */
        .base_frequency_hz = 30.0f,
        .frequency_range_hz = 10.0f,
        .phase_coupling_strength = 0.5f,

        /* Control gains */
        .thrust_gain = 1.0f,
        .roll_gain = 1.0f,
        .pitch_gain = 1.0f,
        .yaw_gain = 1.0f,

        /* Limits */
        .max_amplitude = 1.0f,
        .max_frequency_hz = WING_MAX_FREQUENCY_HZ,
        .max_asymmetry = 0.3f,

        /* Energy model */
        .enable_energy_model = true,
        .energy_per_stroke_j = 0.0001f
    };
    return config;
}

bool wing_motor_validate_config(const wing_motor_config_t* config) {
    if (!config) {
        return false;
    }

    if (config->wing_length_m <= 0.0f) {
        return false;
    }
    if (config->wing_chord_m <= 0.0f) {
        return false;
    }
    if (config->wing_mass_kg <= 0.0f) {
        return false;
    }

    if (config->base_frequency_hz < WING_MIN_FREQUENCY_HZ ||
        config->base_frequency_hz > WING_MAX_FREQUENCY_HZ) return false;
    if (config->frequency_range_hz < 0.0f) {
        return false;
    }
    if (config->phase_coupling_strength < 0.0f ||
        config->phase_coupling_strength > 1.0f) return false;

    if (config->thrust_gain < 0.0f) {
        return false;
    }
    if (config->roll_gain < 0.0f) {
        return false;
    }
    if (config->pitch_gain < 0.0f) {
        return false;
    }
    if (config->yaw_gain < 0.0f) {
        return false;
    }

    if (config->max_amplitude <= 0.0f || config->max_amplitude > 1.0f) {
        return false;
    }
    if (config->max_frequency_hz < WING_MIN_FREQUENCY_HZ) {
        return false;
    }
    if (config->max_asymmetry < 0.0f || config->max_asymmetry > 1.0f) {
        return false;
    }

    if (config->energy_per_stroke_j < 0.0f) {
        return false;
    }

    return true;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

dragonfly_wing_motor_t dragonfly_wing_motor_create(const wing_motor_config_t* config) {
    wing_motor_config_t cfg = config ? *config : wing_motor_default_config();

    if (!wing_motor_validate_config(&cfg)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "dragonfly_wing_motor_create: invalid configuration");
        return NULL;
    }

    dragonfly_wing_motor_t motor = nimcp_calloc(1, sizeof(struct dragonfly_wing_motor_s));
    if (!motor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "dragonfly_wing_motor_create: failed to allocate motor");
        return NULL;
    }

    motor->config = cfg;
    motor->mode = FLIGHT_MODE_HOVER;
    motor->creation_time_us = get_time_us();

    /* Initialize CPG with default hover pattern */
    /* Dragonfly wing pattern: fore wings lead hind wings by ~90 degrees */
    motor->phase[WING_LEFT_FORE] = 0.0f;
    motor->phase[WING_RIGHT_FORE] = 0.0f;
    motor->phase[WING_LEFT_HIND] = (float)(M_PI / 2.0);
    motor->phase[WING_RIGHT_HIND] = (float)(M_PI / 2.0);

    for (int i = 0; i < WING_COUNT; i++) {
        motor->frequency[i] = cfg.base_frequency_hz;
        motor->amplitude[i] = 0.7f;
    }

    motor->mutex = nimcp_mutex_create(NULL);
    if (!motor->mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED,
            "dragonfly_wing_motor_create: failed to create mutex");
        nimcp_free(motor);
        return NULL;
    }

    return motor;
}

void dragonfly_wing_motor_destroy(dragonfly_wing_motor_t motor) {
    if (!motor) return;

    if (motor->mutex) {
        nimcp_mutex_free(motor->mutex);
    }

    nimcp_free(motor);
}

int dragonfly_wing_motor_reset(dragonfly_wing_motor_t motor) {
    if (!motor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "dragonfly_wing_motor_reset: motor is NULL");
        return -1;
    }

    nimcp_mutex_lock(motor->mutex);

    motor->mode = FLIGHT_MODE_HOVER;
    memset(&motor->desired_dynamics, 0, sizeof(motor->desired_dynamics));
    memset(motor->wings, 0, sizeof(motor->wings));

    /* Reset CPG to hover pattern */
    motor->phase[WING_LEFT_FORE] = 0.0f;
    motor->phase[WING_RIGHT_FORE] = 0.0f;
    motor->phase[WING_LEFT_HIND] = (float)(M_PI / 2.0);
    motor->phase[WING_RIGHT_HIND] = (float)(M_PI / 2.0);

    for (int i = 0; i < WING_COUNT; i++) {
        motor->frequency[i] = motor->config.base_frequency_hz;
        motor->amplitude[i] = 0.7f;
    }

    motor->net_thrust = 0.0f;
    memset(motor->net_torque, 0, sizeof(motor->net_torque));
    motor->current_power_w = 0.0f;

    nimcp_mutex_unlock(motor->mutex);

    return 0;
}

//=============================================================================
// Control Functions
//=============================================================================

int dragonfly_wing_motor_set_mode(
    dragonfly_wing_motor_t motor,
    flight_mode_t mode
) {
    if (!motor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "dragonfly_wing_motor_set_mode: motor is NULL");
        return -1;
    }

    nimcp_mutex_lock(motor->mutex);

    motor->mode = mode;

    /* Adjust CPG parameters for flight mode */
    float base_freq = motor->config.base_frequency_hz;
    float base_amp = 0.7f;

    switch (mode) {
        case FLIGHT_MODE_HOVER:
            /* Symmetric, moderate frequency */
            for (int i = 0; i < WING_COUNT; i++) {
                motor->frequency[i] = base_freq;
                motor->amplitude[i] = base_amp;
            }
            break;

        case FLIGHT_MODE_FORWARD:
            /* Higher frequency, tilted stroke plane */
            for (int i = 0; i < WING_COUNT; i++) {
                motor->frequency[i] = base_freq * 1.2f;
                motor->amplitude[i] = base_amp * 1.1f;
            }
            break;

        case FLIGHT_MODE_PURSUIT:
            /* Maximum frequency and amplitude */
            for (int i = 0; i < WING_COUNT; i++) {
                motor->frequency[i] = motor->config.max_frequency_hz;
                motor->amplitude[i] = motor->config.max_amplitude;
            }
            break;

        case FLIGHT_MODE_EVASIVE:
            /* Asymmetric for rapid turns */
            motor->frequency[WING_LEFT_FORE] = base_freq * 1.3f;
            motor->frequency[WING_LEFT_HIND] = base_freq * 1.3f;
            motor->frequency[WING_RIGHT_FORE] = base_freq * 0.9f;
            motor->frequency[WING_RIGHT_HIND] = base_freq * 0.9f;
            for (int i = 0; i < WING_COUNT; i++) {
                motor->amplitude[i] = motor->config.max_amplitude;
            }
            break;

        case FLIGHT_MODE_TURN_LEFT:
            /* Increase right wing frequency */
            motor->frequency[WING_LEFT_FORE] = base_freq * 0.9f;
            motor->frequency[WING_LEFT_HIND] = base_freq * 0.9f;
            motor->frequency[WING_RIGHT_FORE] = base_freq * 1.2f;
            motor->frequency[WING_RIGHT_HIND] = base_freq * 1.2f;
            break;

        case FLIGHT_MODE_TURN_RIGHT:
            /* Increase left wing frequency */
            motor->frequency[WING_LEFT_FORE] = base_freq * 1.2f;
            motor->frequency[WING_LEFT_HIND] = base_freq * 1.2f;
            motor->frequency[WING_RIGHT_FORE] = base_freq * 0.9f;
            motor->frequency[WING_RIGHT_HIND] = base_freq * 0.9f;
            break;

        case FLIGHT_MODE_ASCEND:
            /* Higher frequency, vertical stroke */
            for (int i = 0; i < WING_COUNT; i++) {
                motor->frequency[i] = base_freq * 1.3f;
                motor->amplitude[i] = motor->config.max_amplitude;
            }
            break;

        case FLIGHT_MODE_DESCEND:
            /* Lower frequency for controlled descent */
            for (int i = 0; i < WING_COUNT; i++) {
                motor->frequency[i] = base_freq * 0.7f;
                motor->amplitude[i] = base_amp * 0.8f;
            }
            break;

        case FLIGHT_MODE_BACKWARD:
            /* Reverse stroke plane */
            for (int i = 0; i < WING_COUNT; i++) {
                motor->frequency[i] = base_freq * 0.9f;
                motor->amplitude[i] = base_amp;
            }
            break;
    }

    nimcp_mutex_unlock(motor->mutex);

    return 0;
}

int dragonfly_wing_motor_set_dynamics(
    dragonfly_wing_motor_t motor,
    const flight_dynamics_t* dynamics
) {
    if (!motor || !dynamics) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "dragonfly_wing_motor_set_dynamics: motor=%p, dynamics=%p",
            (void*)motor, (void*)dynamics);
        return -1;
    }

    nimcp_mutex_lock(motor->mutex);
    motor->desired_dynamics = *dynamics;
    nimcp_mutex_unlock(motor->mutex);

    return 0;
}

int dragonfly_wing_motor_step(
    dragonfly_wing_motor_t motor,
    float dt_s,
    wing_motor_output_t* output
) {
    if (!motor || !output || dt_s <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "dragonfly_wing_motor_step: motor=%p, output=%p, dt_s=%f",
            (void*)motor, (void*)output, dt_s);
        return -1;
    }

    nimcp_mutex_lock(motor->mutex);

    uint64_t now = get_time_us();

    /* Step CPG for each wing */
    for (int i = 0; i < WING_COUNT; i++) {
        /* Advance phase */
        motor->phase[i] += NIMCP_TWO_PI_F * motor->frequency[i] * dt_s;
        motor->phase[i] = wrap_phase(motor->phase[i]);

        /* Generate wing state from CPG */
        wing_state_t* wing = &motor->wings[i];
        wing->stroke_frequency = motor->frequency[i];
        wing->stroke_amplitude = motor->amplitude[i];
        wing->stroke_phase = motor->phase[i];

        /* Sinusoidal stroke pattern */
        wing->feather_angle = motor->amplitude[i] * sinf(motor->phase[i]) *
                              (float)(M_PI / 3.0);  /* ±60 degrees */
        wing->deviation_angle = motor->amplitude[i] * 0.3f *
                                sinf(motor->phase[i] * 2.0f) *
                                (float)(M_PI / 6.0);  /* ±30 degrees */

        /* Stroke plane angle varies with flight mode */
        wing->stroke_plane_angle = 0.0f;
        if (motor->mode == FLIGHT_MODE_FORWARD ||
            motor->mode == FLIGHT_MODE_PURSUIT) {
            wing->stroke_plane_angle = (float)(M_PI / 6.0);  /* Tilted forward */
        } else if (motor->mode == FLIGHT_MODE_BACKWARD) {
            wing->stroke_plane_angle = -(float)(M_PI / 6.0);  /* Tilted back */
        }
    }

    /* Compute net thrust (simplified model) */
    float total_thrust = 0.0f;
    for (int i = 0; i < WING_COUNT; i++) {
        /* Thrust proportional to frequency * amplitude^2 */
        float wing_thrust = motor->frequency[i] * motor->amplitude[i] *
                            motor->amplitude[i] * motor->config.thrust_gain;
        total_thrust += wing_thrust;
    }
    motor->net_thrust = total_thrust / WING_COUNT;

    /* Compute net torques from asymmetries */
    /* Roll: difference between left and right */
    float left_power = (motor->frequency[WING_LEFT_FORE] * motor->amplitude[WING_LEFT_FORE] +
                        motor->frequency[WING_LEFT_HIND] * motor->amplitude[WING_LEFT_HIND]) / 2.0f;
    float right_power = (motor->frequency[WING_RIGHT_FORE] * motor->amplitude[WING_RIGHT_FORE] +
                         motor->frequency[WING_RIGHT_HIND] * motor->amplitude[WING_RIGHT_HIND]) / 2.0f;
    motor->net_torque[0] = (right_power - left_power) * motor->config.roll_gain;

    /* Pitch: difference between fore and hind */
    float fore_power = (motor->frequency[WING_LEFT_FORE] * motor->amplitude[WING_LEFT_FORE] +
                        motor->frequency[WING_RIGHT_FORE] * motor->amplitude[WING_RIGHT_FORE]) / 2.0f;
    float hind_power = (motor->frequency[WING_LEFT_HIND] * motor->amplitude[WING_LEFT_HIND] +
                        motor->frequency[WING_RIGHT_HIND] * motor->amplitude[WING_RIGHT_HIND]) / 2.0f;
    motor->net_torque[1] = (hind_power - fore_power) * motor->config.pitch_gain;

    /* Yaw: phase difference between left and right fore wings */
    float phase_diff = motor->phase[WING_RIGHT_FORE] - motor->phase[WING_LEFT_FORE];
    motor->net_torque[2] = sinf(phase_diff) * motor->config.yaw_gain;

    /* Update energy model */
    if (motor->config.enable_energy_model) {
        float strokes_this_step = 0.0f;
        for (int i = 0; i < WING_COUNT; i++) {
            strokes_this_step += motor->frequency[i] * dt_s;
        }
        float energy = strokes_this_step * motor->config.energy_per_stroke_j;
        motor->total_energy_j += energy;
        motor->current_power_w = energy / dt_s;

        motor->stats.total_energy_j = motor->total_energy_j;
    }

    /* Update statistics */
    motor->stats.cycles_generated++;
    motor->stats.strokes_total += (uint64_t)(motor->frequency[0] * dt_s * WING_COUNT);

    float avg_freq = 0.0f;
    float avg_amp = 0.0f;
    for (int i = 0; i < WING_COUNT; i++) {
        avg_freq += motor->frequency[i];
        avg_amp += motor->amplitude[i];
    }
    motor->stats.avg_frequency_hz = avg_freq / WING_COUNT;
    motor->stats.avg_amplitude = avg_amp / WING_COUNT;

    if (motor->net_thrust > motor->stats.max_thrust_achieved) {
        motor->stats.max_thrust_achieved = motor->net_thrust;
    }

    /* Fill output */
    memcpy(output->wings, motor->wings, sizeof(output->wings));
    output->net_thrust = motor->net_thrust;
    memcpy(output->net_torque, motor->net_torque, sizeof(output->net_torque));
    output->timestamp_us = now;

    motor->last_step_us = now;

    nimcp_mutex_unlock(motor->mutex);

    return 0;
}

int dragonfly_wing_motor_pursuit_pattern(
    dragonfly_wing_motor_t motor,
    float heading_error,
    float pitch_error,
    float speed_demand
) {
    if (!motor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "dragonfly_wing_motor_pursuit_pattern: motor is NULL");
        return -1;
    }

    nimcp_mutex_lock(motor->mutex);

    motor->mode = FLIGHT_MODE_PURSUIT;

    /* Maximum frequency scaled by speed demand */
    float target_freq = motor->config.base_frequency_hz +
                        motor->config.frequency_range_hz * speed_demand;
    target_freq = nimcp_clampf(target_freq, WING_MIN_FREQUENCY_HZ,
                          motor->config.max_frequency_hz);

    /* Apply heading correction via asymmetric frequency */
    float heading_mod = nimcp_clampf(heading_error * 0.5f,
                                -motor->config.max_asymmetry,
                                motor->config.max_asymmetry);

    motor->frequency[WING_LEFT_FORE] = target_freq * (1.0f - heading_mod);
    motor->frequency[WING_LEFT_HIND] = target_freq * (1.0f - heading_mod);
    motor->frequency[WING_RIGHT_FORE] = target_freq * (1.0f + heading_mod);
    motor->frequency[WING_RIGHT_HIND] = target_freq * (1.0f + heading_mod);

    /* Maximum amplitude for pursuit */
    for (int i = 0; i < WING_COUNT; i++) {
        motor->amplitude[i] = motor->config.max_amplitude;
    }

    nimcp_mutex_unlock(motor->mutex);

    return 0;
}

int dragonfly_wing_motor_hover_pattern(
    dragonfly_wing_motor_t motor,
    float roll_error,
    float pitch_error,
    float altitude_error
) {
    if (!motor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "dragonfly_wing_motor_hover_pattern: motor is NULL");
        return -1;
    }

    nimcp_mutex_lock(motor->mutex);

    motor->mode = FLIGHT_MODE_HOVER;

    float base_freq = motor->config.base_frequency_hz;

    /* Altitude correction via overall frequency */
    float freq_mod = nimcp_clampf(altitude_error * 0.1f, -0.3f, 0.3f);
    float target_freq = base_freq * (1.0f + freq_mod);

    /* Roll correction via left/right asymmetry */
    float roll_mod = nimcp_clampf(roll_error * 0.3f,
                             -motor->config.max_asymmetry,
                             motor->config.max_asymmetry);

    motor->frequency[WING_LEFT_FORE] = target_freq * (1.0f - roll_mod);
    motor->frequency[WING_LEFT_HIND] = target_freq * (1.0f - roll_mod);
    motor->frequency[WING_RIGHT_FORE] = target_freq * (1.0f + roll_mod);
    motor->frequency[WING_RIGHT_HIND] = target_freq * (1.0f + roll_mod);

    /* Pitch correction via fore/hind asymmetry */
    float pitch_mod = nimcp_clampf(pitch_error * 0.3f,
                              -motor->config.max_asymmetry,
                              motor->config.max_asymmetry);

    motor->frequency[WING_LEFT_FORE] *= (1.0f + pitch_mod);
    motor->frequency[WING_RIGHT_FORE] *= (1.0f + pitch_mod);
    motor->frequency[WING_LEFT_HIND] *= (1.0f - pitch_mod);
    motor->frequency[WING_RIGHT_HIND] *= (1.0f - pitch_mod);

    /* Moderate amplitude for hover */
    for (int i = 0; i < WING_COUNT; i++) {
        motor->amplitude[i] = 0.7f;
    }

    nimcp_mutex_unlock(motor->mutex);

    return 0;
}

//=============================================================================
// Query Functions
//=============================================================================

int dragonfly_wing_motor_get_state(
    const dragonfly_wing_motor_t motor,
    wing_id_t wing,
    wing_state_t* state
) {
    if (!motor || !state || wing >= WING_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "dragonfly_wing_motor_get_state: motor=%p, state=%p, wing=%d",
            (void*)motor, (void*)state, wing);
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)motor->mutex);
    *state = motor->wings[wing];
    nimcp_mutex_unlock((nimcp_mutex_t*)motor->mutex);

    return 0;
}

int dragonfly_wing_motor_get_all_states(
    const dragonfly_wing_motor_t motor,
    wing_state_t states[WING_COUNT]
) {
    if (!motor || !states) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "dragonfly_wing_motor_get_all_states: motor=%p, states=%p",
            (void*)motor, (void*)states);
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)motor->mutex);
    memcpy(states, motor->wings, sizeof(motor->wings));
    nimcp_mutex_unlock((nimcp_mutex_t*)motor->mutex);

    return 0;
}

flight_mode_t dragonfly_wing_motor_get_mode(const dragonfly_wing_motor_t motor) {
    if (!motor) return FLIGHT_MODE_HOVER;
    return motor->mode;
}

float dragonfly_wing_motor_get_power_w(const dragonfly_wing_motor_t motor) {
    if (!motor) return 0.0f;
    return motor->current_power_w;
}

//=============================================================================
// Statistics and Configuration Functions
//=============================================================================

int dragonfly_wing_motor_get_stats(
    const dragonfly_wing_motor_t motor,
    wing_motor_stats_t* stats
) {
    if (!motor || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "dragonfly_wing_motor_get_stats: motor=%p, stats=%p",
            (void*)motor, (void*)stats);
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)motor->mutex);
    *stats = motor->stats;
    nimcp_mutex_unlock((nimcp_mutex_t*)motor->mutex);

    return 0;
}

int dragonfly_wing_motor_reset_stats(dragonfly_wing_motor_t motor) {
    if (!motor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "dragonfly_wing_motor_reset_stats: motor is NULL");
        return -1;
    }

    nimcp_mutex_lock(motor->mutex);
    memset(&motor->stats, 0, sizeof(motor->stats));
    motor->total_energy_j = 0.0f;
    nimcp_mutex_unlock(motor->mutex);

    return 0;
}

int dragonfly_wing_motor_set_config(
    dragonfly_wing_motor_t motor,
    const wing_motor_config_t* config
) {
    if (!motor || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "dragonfly_wing_motor_set_config: motor=%p, config=%p",
            (void*)motor, (void*)config);
        return -1;
    }
    if (!wing_motor_validate_config(config)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "dragonfly_wing_motor_set_config: invalid configuration");
        return -1;
    }

    nimcp_mutex_lock(motor->mutex);
    motor->config = *config;
    nimcp_mutex_unlock(motor->mutex);

    return 0;
}

const char* dragonfly_flight_mode_name(flight_mode_t mode) {
    switch (mode) {
        case FLIGHT_MODE_HOVER:      return "HOVER";
        case FLIGHT_MODE_FORWARD:    return "FORWARD";
        case FLIGHT_MODE_BACKWARD:   return "BACKWARD";
        case FLIGHT_MODE_ASCEND:     return "ASCEND";
        case FLIGHT_MODE_DESCEND:    return "DESCEND";
        case FLIGHT_MODE_TURN_LEFT:  return "TURN_LEFT";
        case FLIGHT_MODE_TURN_RIGHT: return "TURN_RIGHT";
        case FLIGHT_MODE_PURSUIT:    return "PURSUIT";
        case FLIGHT_MODE_EVASIVE:    return "EVASIVE";
        default:                     return "UNKNOWN";
    }
}

const char* dragonfly_wing_name(wing_id_t wing) {
    switch (wing) {
        case WING_LEFT_FORE:  return "LEFT_FORE";
        case WING_LEFT_HIND:  return "LEFT_HIND";
        case WING_RIGHT_FORE: return "RIGHT_FORE";
        case WING_RIGHT_HIND: return "RIGHT_HIND";
        default:              return "UNKNOWN";
    }
}
