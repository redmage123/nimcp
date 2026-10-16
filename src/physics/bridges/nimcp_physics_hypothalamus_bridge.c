#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_physics_hypothalamus_bridge.c - Physics Layer to Hypothalamus Bridge
//=============================================================================
/**
 * @file nimcp_physics_hypothalamus_bridge.c
 * @brief Implementation of physics-hypothalamus bidirectional bridge
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "physics/bridges/nimcp_physics_hypothalamus_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_math_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(physics_hypothalamus_bridge)

#define LOG_MODULE "PHYSICS_HYPOTHALAMUS_BRIDGE"


//=============================================================================
// Internal Structure
//=============================================================================

struct physics_hypo_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    /** Configuration */
    physics_hypo_config_t config;

    /** Connected physics modules */
    nimcp_thermodynamic_state_t* thermo;
    nimcp_hh_population_t* hh_pop;

    /** Connected homeostasis system */
    hypo_homeostasis_handle_t* homeostasis;

    /** Current physics state */
    physics_hypo_state_t current_state;

    /** Current modulation */
    physics_hypo_modulation_t current_modulation;

    /** Statistics */
    physics_hypo_stats_t stats;

    /** Timers */
    float temp_sample_timer;
    float energy_sample_timer;
    float circadian_timer;

    /** Circadian phase (hours, 0.0-24.0) */
    float circadian_phase;

    /** Running averages for statistics */
    float temp_deviation_sum;
    float atp_level_sum;
    uint64_t sample_count;

    /** Initialized flag */
    bool initialized;
};

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Compute circadian multiplier based on phase
 *
 * Models daily activity cycle with peak around noon (12:00)
 */
static float compute_circadian_multiplier(float phase) {
    /* Normalize to 0-24 range */
    while (phase < 0.0f) phase += 24.0f;
    while (phase >= 24.0f) phase -= 24.0f;

    /* Sinusoidal model: peak at 12:00 (noon), trough at 0:00 (midnight) */
    float radians = (phase - 6.0f) * (NIMCP_TWO_PI_F / 24.0f);
    float multiplier = 0.75f + 0.25f * sinf(radians);

    return multiplier;
}

/**
 * @brief Determine thermoregulation direction from temperature
 */
static physics_hypo_thermo_dir_t compute_thermo_direction(
    float temperature,
    float setpoint
) {
    float deviation = temperature - setpoint;
    float tolerance = 0.5f;  /* Half degree tolerance */

    if (fabsf(deviation) < tolerance) {
        return PHYSICS_HYPO_THERMO_NEUTRAL;
    } else if (deviation > 0.0f) {
        return PHYSICS_HYPO_THERMO_COOLING;
    } else {
        return PHYSICS_HYPO_THERMO_HEATING;
    }
}

/**
 * @brief Compute thermoregulation strength from temperature deviation
 */
static float compute_thermo_strength(float temperature, float setpoint) {
    float deviation = fabsf(temperature - setpoint);

    /* Linear ramp: 0 at setpoint, 1.0 at 3 degrees deviation */
    float strength = deviation / 3.0f;
    if (strength > 1.0f) strength = 1.0f;

    return strength;
}

//=============================================================================
// Configuration API
//=============================================================================

int physics_hypo_default_config(physics_hypo_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    config->enable_temperature = true;
    config->enable_energy = true;
    config->enable_circadian = true;
    config->enable_arousal = true;
    config->temp_update_interval_ms = 100.0f;
    config->energy_update_interval_ms = 50.0f;
    config->circadian_update_interval_ms = 1000.0f;
    config->temp_setpoint = PHYSICS_HYPO_TEMP_SETPOINT;
    config->atp_setpoint = PHYSICS_HYPO_ATP_SETPOINT;
    config->circadian_period_hours = PHYSICS_HYPO_CIRCADIAN_PERIOD;
    config->modulation_strength = 0.5f;
    config->use_pid_control = true;

    return 0;
}

//=============================================================================
// Lifecycle API
//=============================================================================

physics_hypo_bridge_t* physics_hypo_bridge_create(
    const physics_hypo_config_t* config
) {
    physics_hypo_bridge_t* bridge = nimcp_calloc(1, sizeof(*bridge));
    NIMCP_API_CHECK_ALLOC(bridge, "Failed to allocate physics-hypothalamus bridge");

    /* Initialize bridge base infrastructure */
    if (bridge_base_init(&bridge->base, 0, "physics_hypothalamus_bridge") != 0) {
        nimcp_free(bridge);
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        physics_hypo_default_config(&bridge->config);
    }

    /* Initialize state */
    bridge->current_state.temperature = bridge->config.temp_setpoint;
    bridge->current_state.atp_level = bridge->config.atp_setpoint;
    bridge->current_state.circadian_phase = 12.0f;  /* Start at noon */

    /* Initialize modulation to neutral */
    bridge->current_modulation.q10_modifier = 1.0f;
    bridge->current_modulation.metabolic_modifier = 1.0f;
    bridge->current_modulation.conductance_modifier = 1.0f;
    bridge->current_modulation.threshold_modifier = 1.0f;
    bridge->current_modulation.tau_modifier = 1.0f;
    bridge->current_modulation.thermo_direction = PHYSICS_HYPO_THERMO_NEUTRAL;
    bridge->current_modulation.thermo_strength = 0.0f;
    bridge->current_modulation.energy_conservation = false;
    bridge->current_modulation.arousal_level = 0.5f;
    bridge->current_modulation.circadian_multiplier = 1.0f;
    bridge->current_modulation.controller_output = 0.0f;

    bridge->circadian_phase = 12.0f;

    bridge->initialized = true;

    NIMCP_LOG_INFO(PHYSICS_HYPO_MODULE_NAME,
        "Physics-hypothalamus bridge created: temp_coupling=%d, energy_coupling=%d",
        bridge->config.enable_temperature, bridge->config.enable_energy);

    return bridge;
}

void physics_hypo_bridge_destroy(physics_hypo_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "physics_hypothalamus");

    NIMCP_LOG_INFO(PHYSICS_HYPO_MODULE_NAME,
        "Bridge destroyed - physics_to_hypo: %lu, hypo_to_physics: %lu",
        (unsigned long)bridge->stats.physics_to_hypo_count,
        (unsigned long)bridge->stats.hypo_to_physics_count);

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int physics_hypo_connect_physics(
    physics_hypo_bridge_t* bridge,
    nimcp_thermodynamic_state_t* thermo,
    nimcp_hh_population_t* hh_pop
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    bridge->thermo = thermo;
    bridge->hh_pop = hh_pop;

    NIMCP_LOG_DEBUG(PHYSICS_HYPO_MODULE_NAME,
        "Connected physics: thermo=%p, hh=%p",
        (void*)thermo, (void*)hh_pop);

    return 0;
}

int physics_hypo_connect_homeostasis(
    physics_hypo_bridge_t* bridge,
    hypo_homeostasis_handle_t* homeostasis
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    bridge->homeostasis = homeostasis;

    NIMCP_LOG_DEBUG(PHYSICS_HYPO_MODULE_NAME,
        "Connected homeostasis: %p", (void*)homeostasis);

    return 0;
}

int physics_hypo_reset(physics_hypo_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Reset state */
    bridge->current_state.temperature = bridge->config.temp_setpoint;
    bridge->current_state.atp_level = bridge->config.atp_setpoint;
    bridge->current_state.circadian_phase = 12.0f;
    bridge->current_state.timestamp_ms = 0.0f;

    /* Reset modulation */
    bridge->current_modulation.q10_modifier = 1.0f;
    bridge->current_modulation.metabolic_modifier = 1.0f;
    bridge->current_modulation.conductance_modifier = 1.0f;
    bridge->current_modulation.threshold_modifier = 1.0f;
    bridge->current_modulation.tau_modifier = 1.0f;
    bridge->current_modulation.thermo_direction = PHYSICS_HYPO_THERMO_NEUTRAL;
    bridge->current_modulation.thermo_strength = 0.0f;

    /* Reset timers */
    bridge->temp_sample_timer = 0.0f;
    bridge->energy_sample_timer = 0.0f;
    bridge->circadian_timer = 0.0f;
    bridge->circadian_phase = 12.0f;

    /* Reset stats */
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->temp_deviation_sum = 0.0f;
    bridge->atp_level_sum = 0.0f;
    bridge->sample_count = 0;

    NIMCP_LOG_DEBUG(PHYSICS_HYPO_MODULE_NAME, "Bridge reset");

    return 0;
}

//=============================================================================
// Physics -> Hypothalamus API
//=============================================================================

int physics_hypo_report_state(
    physics_hypo_bridge_t* bridge,
    physics_hypo_state_t* state
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Sample thermodynamics */
    if (bridge->thermo && bridge->config.enable_temperature) {
        double atp_ratio = nimcp_thermo_get_atp_ratio(bridge->thermo);
        bridge->current_state.atp_level = (float)atp_ratio;

        /* Temperature would come from thermodynamic state if available */
        /* For now, use current state */
    }

    /* Sample HH population */
    if (bridge->hh_pop) {
        float rate;
        if (nimcp_hh_population_get_rate(bridge->hh_pop, &rate) == NIMCP_SUCCESS) {
            bridge->current_state.avg_firing_rate = rate;
        }
    }

    /* Update circadian phase */
    bridge->current_state.circadian_phase = bridge->circadian_phase;

    /* Report to homeostasis system if connected */
    if (bridge->homeostasis && bridge->config.enable_temperature) {
        hypo_homeostasis_set_value(
            bridge->homeostasis,
            HYPO_VAR_TEMPERATURE,
            bridge->current_state.temperature
        );
    }

    /* Update statistics */
    bridge->stats.physics_to_hypo_count++;

    /* Track running averages */
    float temp_dev = fabsf(bridge->current_state.temperature -
                          bridge->config.temp_setpoint);
    bridge->temp_deviation_sum += temp_dev;
    bridge->atp_level_sum += bridge->current_state.atp_level;
    bridge->sample_count++;

    if (bridge->sample_count > 0) {
        bridge->stats.avg_temp_deviation =
            bridge->temp_deviation_sum / (float)bridge->sample_count;
        bridge->stats.avg_atp_level =
            bridge->atp_level_sum / (float)bridge->sample_count;
    }

    /* Check for deviations */
    if (temp_dev > 1.0f) {
        bridge->stats.temp_deviations++;
    }

    if (bridge->current_state.atp_level < PHYSICS_HYPO_ATP_CRITICAL) {
        bridge->stats.atp_critical_events++;
    }

    /* Copy to output if requested */
    if (state) {
        *state = bridge->current_state;
    }

    return 0;
}

int physics_hypo_report_temperature(
    physics_hypo_bridge_t* bridge,
    float temperature
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    bridge->current_state.temperature = temperature;

    /* Report to homeostasis */
    if (bridge->homeostasis) {
        hypo_homeostasis_set_value(
            bridge->homeostasis,
            HYPO_VAR_TEMPERATURE,
            temperature
        );
    }

    return 0;
}

int physics_hypo_report_atp(
    physics_hypo_bridge_t* bridge,
    float atp_level
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    bridge->current_state.atp_level = atp_level;

    /* Report as glucose proxy to homeostasis */
    if (bridge->homeostasis) {
        hypo_homeostasis_set_value(
            bridge->homeostasis,
            HYPO_VAR_GLUCOSE,
            atp_level
        );
    }

    return 0;
}

//=============================================================================
// Hypothalamus -> Physics API
//=============================================================================

int physics_hypo_get_modulation(
    physics_hypo_bridge_t* bridge,
    physics_hypo_modulation_t* modulation
) {
    if (!bridge || !modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_hypo_get_modulation: required parameter is NULL (bridge, modulation)");
        return -1;
    }

    /* Get controller output from homeostasis if connected */
    if (bridge->homeostasis && bridge->config.use_pid_control) {
        float temp_output = hypo_homeostasis_get_output(
            bridge->homeostasis,
            HYPO_VAR_TEMPERATURE
        );
        bridge->current_modulation.controller_output = temp_output;

        /* Map controller output to modulation */
        /* Positive output = need heating, negative = need cooling */
        bridge->current_modulation.thermo_direction =
            compute_thermo_direction(
                bridge->current_state.temperature,
                bridge->config.temp_setpoint
            );

        bridge->current_modulation.thermo_strength =
            compute_thermo_strength(
                bridge->current_state.temperature,
                bridge->config.temp_setpoint
            );

        /* Temperature affects Q10 and time constants */
        float temp_mod = 1.0f + temp_output * bridge->config.modulation_strength;
        temp_mod = fmaxf(PHYSICS_HYPO_MIN_MOD, fminf(PHYSICS_HYPO_MAX_MOD, temp_mod));
        bridge->current_modulation.q10_modifier = temp_mod;
        bridge->current_modulation.tau_modifier = 1.0f / temp_mod;

        /* Update stats for heating/cooling */
        if (bridge->current_modulation.thermo_direction == PHYSICS_HYPO_THERMO_HEATING) {
            bridge->stats.heating_activations++;
        } else if (bridge->current_modulation.thermo_direction == PHYSICS_HYPO_THERMO_COOLING) {
            bridge->stats.cooling_activations++;
        }
    } else {
        /* Without homeostasis, compute locally */
        bridge->current_modulation.thermo_direction =
            compute_thermo_direction(
                bridge->current_state.temperature,
                bridge->config.temp_setpoint
            );

        bridge->current_modulation.thermo_strength =
            compute_thermo_strength(
                bridge->current_state.temperature,
                bridge->config.temp_setpoint
            );
    }

    /* Energy modulation */
    if (bridge->config.enable_energy) {
        float atp = bridge->current_state.atp_level;
        if (atp < bridge->config.atp_setpoint) {
            /* Low energy: reduce activity */
            float deficit = bridge->config.atp_setpoint - atp;
            bridge->current_modulation.energy_conservation = deficit > 0.2f;
            bridge->current_modulation.metabolic_modifier =
                0.5f + 0.5f * (atp / bridge->config.atp_setpoint);
            bridge->current_modulation.conductance_modifier =
                0.7f + 0.3f * (atp / bridge->config.atp_setpoint);
        } else {
            bridge->current_modulation.energy_conservation = false;
            bridge->current_modulation.metabolic_modifier = 1.0f;
            bridge->current_modulation.conductance_modifier = 1.0f;
        }
    }

    /* Circadian modulation */
    if (bridge->config.enable_circadian) {
        bridge->current_modulation.circadian_multiplier =
            compute_circadian_multiplier(bridge->circadian_phase);
    }

    /* Arousal modulation */
    if (bridge->config.enable_arousal) {
        /* Arousal affects threshold (lower = more excitable) */
        /* High circadian multiplier = high arousal = lower threshold */
        bridge->current_modulation.arousal_level =
            bridge->current_modulation.circadian_multiplier;
        bridge->current_modulation.threshold_modifier =
            1.0f - 0.2f * (bridge->current_modulation.arousal_level - 0.5f);
    }

    *modulation = bridge->current_modulation;

    return 0;
}

int physics_hypo_apply_modulation(
    physics_hypo_bridge_t* bridge,
    const physics_hypo_modulation_t* modulation
) {
    if (!bridge || !modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_hypo_apply_modulation: required parameter is NULL (bridge, modulation)");
        return -1;
    }

    bridge->current_modulation = *modulation;
    bridge->stats.hypo_to_physics_count++;

    /* In full implementation, would apply to HH population */
    if (bridge->hh_pop) {
        NIMCP_LOG_DEBUG(PHYSICS_HYPO_MODULE_NAME,
            "Applied modulation: Q10=%.2f, tau=%.2f, cond=%.2f, thermo=%d/%.2f",
            modulation->q10_modifier,
            modulation->tau_modifier,
            modulation->conductance_modifier,
            modulation->thermo_direction,
            modulation->thermo_strength);
    }

    return 0;
}

//=============================================================================
// Circadian API
//=============================================================================

int physics_hypo_set_circadian_phase(
    physics_hypo_bridge_t* bridge,
    float phase
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Normalize to 0-24 range */
    while (phase < 0.0f) phase += 24.0f;
    while (phase >= 24.0f) phase -= 24.0f;

    bridge->circadian_phase = phase;
    bridge->current_state.circadian_phase = phase;
    bridge->stats.current_circadian_phase = phase;

    return 0;
}

int physics_hypo_advance_circadian(
    physics_hypo_bridge_t* bridge,
    float dt
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->config.enable_circadian) return 0;

    /* Convert ms to hours */
    float hours = dt / (1000.0f * 3600.0f);

    /* Advance phase */
    bridge->circadian_phase += hours;

    /* Wrap around at 24 hours */
    while (bridge->circadian_phase >= 24.0f) {
        bridge->circadian_phase -= 24.0f;
    }

    bridge->current_state.circadian_phase = bridge->circadian_phase;
    bridge->stats.current_circadian_phase = bridge->circadian_phase;

    return 0;
}

float physics_hypo_get_circadian_multiplier(
    const physics_hypo_bridge_t* bridge
) {
    if (!bridge) return 1.0f;
    return compute_circadian_multiplier(bridge->circadian_phase);
}

//=============================================================================
// Update API
//=============================================================================

int physics_hypo_update(
    physics_hypo_bridge_t* bridge,
    float dt
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Update timers */
    bridge->temp_sample_timer += dt;
    bridge->energy_sample_timer += dt;
    bridge->circadian_timer += dt;

    /* Temperature sampling */
    if (bridge->config.enable_temperature &&
        bridge->temp_sample_timer >= bridge->config.temp_update_interval_ms) {
        bridge->temp_sample_timer = 0.0f;

        /* Report temperature state */
        physics_hypo_report_state(bridge, NULL);
    }

    /* Energy sampling */
    if (bridge->config.enable_energy &&
        bridge->energy_sample_timer >= bridge->config.energy_update_interval_ms) {
        bridge->energy_sample_timer = 0.0f;

        /* Sample ATP if available */
        if (bridge->thermo) {
            double atp = nimcp_thermo_get_atp_ratio(bridge->thermo);
            physics_hypo_report_atp(bridge, (float)atp);
        }
    }

    /* Circadian update */
    if (bridge->config.enable_circadian &&
        bridge->circadian_timer >= bridge->config.circadian_update_interval_ms) {
        bridge->circadian_timer = 0.0f;
        physics_hypo_advance_circadian(bridge, bridge->config.circadian_update_interval_ms);
    }

    /* Get modulation from hypothalamus */
    physics_hypo_modulation_t modulation;
    physics_hypo_get_modulation(bridge, &modulation);

    /* Apply modulation to physics */
    physics_hypo_apply_modulation(bridge, &modulation);

    bridge->stats.last_update_ms += dt;

    return 0;
}

//=============================================================================
// Query API
//=============================================================================

int physics_hypo_get_state(
    const physics_hypo_bridge_t* bridge,
    physics_hypo_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_hypo_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }
    *state = bridge->current_state;
    return 0;
}

int physics_hypo_get_stats(
    const physics_hypo_bridge_t* bridge,
    physics_hypo_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_hypo_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

bool physics_hypo_is_connected(const physics_hypo_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    bool has_physics = bridge->thermo || bridge->hh_pop;
    return has_physics && bridge->homeostasis;
}

int physics_hypo_get_thermo_status(
    const physics_hypo_bridge_t* bridge,
    physics_hypo_thermo_dir_t* direction,
    float* strength
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    if (direction) {
        *direction = bridge->current_modulation.thermo_direction;
    }
    if (strength) {
        *strength = bridge->current_modulation.thermo_strength;
    }

    return 0;
}
