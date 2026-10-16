/**
 * @file nimcp_hypothalamus_adapter.c
 * @brief Implementation of hypothalamus brain adapter
 *
 * WHAT: Unified adapter connecting hypothalamus functions to the brain system
 * WHY:  Enable homeostatic regulation, circadian rhythms, stress response
 * HOW:  Orchestrates SCN, HPA axis, and autonomic nuclei
 *
 * @version Phase H1: Hypothalamus Brain Integration
 * @date 2025-12-30
 */

#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_adapter.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_wiring_helpers.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_threshold_constants.h"
#include "constants/nimcp_math_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(hypothalamus_adapter, MESH_ADAPTER_CATEGORY_COGNITIVE)


/*=============================================================================
 * LOGGING MODULE IDENTIFIER
 *===========================================================================*/

#define HYPOTHALAMUS_LOG_MODULE "HYPOTHALAMUS"

/*=============================================================================
 * MATHEMATICAL CONSTANTS
 *===========================================================================*/


#define TWO_PI (2.0f * (float)M_PI)

/* Time conversion */
#define US_PER_HOUR (3600000000ULL)
#define US_PER_MINUTE (60000000ULL)
#define US_PER_SECOND (1000000ULL)

/*=============================================================================
 * INTERNAL STRUCTURE
 *===========================================================================*/

struct hypothalamus_adapter {
    /* Configuration */
    hypothalamus_config_t config;

    /* Complete state */
    hypothalamus_state_t state;

    /* Callbacks */
    hypothalamus_alert_callback_t alert_callback;
    void* alert_user_data;
    hypothalamus_hormone_callback_t hormone_callback;
    void* hormone_user_data;
    hypothalamus_autonomic_callback_t autonomic_callback;
    void* autonomic_user_data;

    /* Bio-async communication context */
    bio_module_context_t bio_ctx;

    /* Statistics */
    hypothalamus_stats_t stats;

    /* Internal timing */
    uint64_t last_update_time_us;
    hypo_circadian_phase_t previous_phase;
};

/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

/**
 * @brief Simple exponential decay
 */
static float exponential_decay(float current, float target, float rate, float dt) {
    return current + (target - current) * (1.0f - expf(-rate * dt));
}

/**
 * @brief Calculate circadian phase from time
 */
static hypo_circadian_phase_t phase_to_period(float phase) {
    /* Normalize phase to [0, 2*PI] */
    while (phase < 0.0f) phase += NIMCP_TWO_PI_F;
    while (phase >= NIMCP_TWO_PI_F) phase -= NIMCP_TWO_PI_F;

    /* Map phase to 24-hour periods (phase 0 = midnight) */
    float hour = (phase / NIMCP_TWO_PI_F) * 24.0f;

    if (hour < 3.0f) return HYPO_CIRCADIAN_PHASE_LATE_NIGHT;
    if (hour < 6.0f) return HYPO_CIRCADIAN_PHASE_LATE_NIGHT;
    if (hour < 9.0f) return HYPO_CIRCADIAN_PHASE_EARLY_MORNING;
    if (hour < 12.0f) return HYPO_CIRCADIAN_PHASE_LATE_MORNING;
    if (hour < 15.0f) return HYPO_CIRCADIAN_PHASE_EARLY_AFTERNOON;
    if (hour < 18.0f) return HYPO_CIRCADIAN_PHASE_LATE_AFTERNOON;
    if (hour < 21.0f) return HYPO_CIRCADIAN_PHASE_EVENING;
    return HYPO_CIRCADIAN_PHASE_EARLY_NIGHT;
}

/**
 * @brief Calculate melatonin level from circadian phase
 * High at night, low during day
 */
static float calculate_melatonin(float phase) {
    /* Melatonin peaks around midnight (phase = 0) */
    /* Use cosine that peaks at 0 and is low at PI (noon) */
    float melatonin = 0.5f * (1.0f + cosf(phase));

    /* Sharpen the transition with power function */
    melatonin = powf(melatonin, 1.5f);

    return nimcp_clamp01(melatonin);
}

/**
 * @brief Calculate cortisol level from circadian phase
 * Peaks in early morning, lowest at midnight
 */
static float calculate_circadian_cortisol(float phase) {
    /* Cortisol peaks around 6-8 AM (phase ~= PI/2) */
    /* Use cosine shifted to peak in morning */
    float phase_shifted = phase - (float)M_PI / 4.0f;  /* Shift peak to morning */
    float cortisol = 0.5f * (1.0f + cosf(phase_shifted));

    /* Add some asymmetry - faster rise, slower decline */
    if (cortisol > 0.5f) {
        cortisol = 0.5f + 0.5f * powf(2.0f * (cortisol - 0.5f), 0.8f);
    }

    return nimcp_clamp01(cortisol);
}

/**
 * @brief Calculate alertness from circadian and sleep pressure
 */
static float calculate_alertness(float circadian_cortisol, float melatonin,
                                  float sleep_pressure) {
    /* Alertness positively correlates with cortisol */
    /* Negatively correlates with melatonin and sleep pressure */
    float alertness = circadian_cortisol * 0.6f -
                      melatonin * 0.3f -
                      sleep_pressure * 0.4f + 0.4f;

    return nimcp_clamp01(alertness);
}

/**
 * @brief Update homeostatic parameter with PID-like control
 */
static void update_homeostatic_param(homeostatic_parameter_t* param,
                                      float current_value,
                                      float kp, float ki, float dt) {
    param->current_value = current_value;
    param->error = param->setpoint - current_value;

    /* Integrate error with decay */
    param->integral_error = exponential_decay(param->integral_error,
                                               param->error,
                                               0.1f, dt);
    param->integral_error = fmaxf(-1.0f, fminf(1.0f, param->integral_error));

    /* Calculate correction signal */
    param->correction_signal = kp * param->error + ki * param->integral_error;
    param->correction_signal = nimcp_clamp01(fabsf(param->correction_signal)) *
                               (param->correction_signal > 0 ? 1.0f : -1.0f);
}

/**
 * @brief Set error state
 */
static void set_error(hypothalamus_adapter_t* adapter, hypothalamus_error_t error) {
    if (!adapter) return;
    adapter->state.last_error = error;
    if (error != HYPOTHALAMUS_ERROR_NONE) {
        adapter->state.status = HYPOTHALAMUS_STATUS_ERROR;
        LOG_ERROR(HYPOTHALAMUS_LOG_MODULE, "Error set: %d", error);
    }
}

/**
 * @brief Emit alert callback
 */
static void emit_alert(hypothalamus_adapter_t* adapter,
                        hypothalamus_status_t status,
                        const void* data) {
    if (adapter->config.enable_events && adapter->alert_callback) {
        adapter->alert_callback(status, data, adapter->alert_user_data);
    }
}

/*=============================================================================
 * BIO-ASYNC MESSAGE HANDLERS (Forward declarations)
 *===========================================================================*/

static nimcp_error_t handle_stress_input_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data);

static nimcp_error_t handle_temperature_input(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data);

static nimcp_error_t handle_state_query(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data);

/*=============================================================================
 * KG-DRIVEN WIRING CALLBACK
 *===========================================================================*/

/**
 * @brief Wiring callback for KG-driven handler registration
 *
 * Called by the wiring orchestrator with message types discovered from KG
 * HANDLES_MESSAGE relations. Registers the appropriate handlers dynamically.
 */
static int hypothalamus_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
) {
    if (!ctx || !message_types || message_count == 0) {
        return 0;
    }

    hypothalamus_adapter_t* adapter = (hypothalamus_adapter_t*)user_data;
    if (!adapter) {
        LOG_WARNING(HYPOTHALAMUS_LOG_MODULE, "Wiring callback: adapter is NULL");
        return 0;
    }

    LOG_DEBUG(HYPOTHALAMUS_LOG_MODULE,
              "KG wiring callback: registering %u message handlers", message_count);

    for (uint32_t i = 0; i < message_count; i++) {
        switch (message_types[i]) {
            case BIO_MSG_STRESS_INPUT:
                bio_router_register_handler(ctx, message_types[i],
                    handle_stress_input_request);
                LOG_DEBUG(HYPOTHALAMUS_LOG_MODULE,
                          "Registered handler: BIO_MSG_STRESS_INPUT");
                break;

            case BIO_MSG_TEMPERATURE_INPUT:
                bio_router_register_handler(ctx, message_types[i],
                    handle_temperature_input);
                LOG_DEBUG(HYPOTHALAMUS_LOG_MODULE,
                          "Registered handler: BIO_MSG_TEMPERATURE_INPUT");
                break;

            case BIO_MSG_STATE_QUERY:
                bio_router_register_handler(ctx, message_types[i],
                    handle_state_query);
                LOG_DEBUG(HYPOTHALAMUS_LOG_MODULE,
                          "Registered handler: BIO_MSG_STATE_QUERY");
                break;

            default:
                LOG_DEBUG(HYPOTHALAMUS_LOG_MODULE,
                          "Unknown message type 0x%04X in wiring callback",
                          message_types[i]);
                break;
        }
    }

    return 0;
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

hypothalamus_config_t hypothalamus_default_config(void) {
    hypothalamus_config_t config;
    memset(&config, 0, sizeof(config));

    /* Circadian settings */
    config.circadian_period_hours = HYPOTHALAMUS_DEFAULT_CIRCADIAN_PERIOD_HOURS;
    config.initial_phase = 0.0f;  /* Start at midnight */
    config.enable_circadian = true;

    /* Homeostatic setpoints */
    config.temperature_setpoint_c = HYPOTHALAMUS_DEFAULT_TEMP_SETPOINT_C;
    config.osmolality_setpoint = 290.0f;  /* mOsm/kg */
    config.glucose_setpoint = 90.0f;       /* mg/dL */

    /* Stress response */
    config.enable_hpa_axis = true;
    config.cortisol_baseline = HYPOTHALAMUS_DEFAULT_CORTISOL_BASELINE;
    config.crh_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config.cortisol_feedback_gain = 0.5f;

    /* Autonomic control */
    config.enable_autonomic = true;
    config.sympathetic_bias = HYPOTHALAMUS_DEFAULT_AUTONOMIC_BALANCE;

    /* Appetite and metabolism */
    config.enable_appetite = true;
    config.hunger_threshold = HYPOTHALAMUS_DEFAULT_HUNGER_THRESHOLD;
    config.thirst_threshold = HYPOTHALAMUS_DEFAULT_THIRST_THRESHOLD;
    config.leptin_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config.ghrelin_sensitivity = NIMCP_SENSITIVITY_DEFAULT;

    /* Integration */
    config.enable_limbic_input = true;
    config.enable_brainstem_output = true;
    config.enable_pituitary_link = true;

    /* Bio-async */
    config.enable_bio_async = true;
    config.default_channel = BIO_CHANNEL_NOREPINEPHRINE;

    /* Events */
    config.enable_events = true;

    return config;
}

hypothalamus_adapter_t* hypothalamus_create(const hypothalamus_config_t* config) {
    LOG_INFO(HYPOTHALAMUS_LOG_MODULE, "Creating hypothalamus adapter");

    hypothalamus_adapter_t* adapter = (hypothalamus_adapter_t*)nimcp_calloc(
        1, sizeof(hypothalamus_adapter_t));
    if (!adapter) {
        LOG_ERROR(HYPOTHALAMUS_LOG_MODULE, "Failed to allocate adapter memory");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hypothalamus_create: adapter is NULL");
        return NULL;
    }

    /* Set configuration */
    if (config) {
        adapter->config = *config;
        LOG_DEBUG(HYPOTHALAMUS_LOG_MODULE, "Using provided configuration");
    } else {
        adapter->config = hypothalamus_default_config();
        LOG_DEBUG(HYPOTHALAMUS_LOG_MODULE, "Using default configuration");
    }

    /* Initialize circadian state */
    adapter->state.circadian.phase = adapter->config.initial_phase;
    adapter->state.circadian.amplitude = 1.0f;
    adapter->state.circadian.period = phase_to_period(adapter->config.initial_phase);
    adapter->state.circadian.melatonin_level = calculate_melatonin(
        adapter->config.initial_phase);
    adapter->state.circadian.cortisol_level = calculate_circadian_cortisol(
        adapter->config.initial_phase);
    adapter->state.circadian.alertness = 0.5f;
    adapter->state.circadian.sleep_pressure = 0.0f;

    /* Initialize thermoregulation */
    adapter->state.thermoregulation.core_temp.setpoint =
        adapter->config.temperature_setpoint_c;
    adapter->state.thermoregulation.core_temp.current_value =
        adapter->config.temperature_setpoint_c;
    adapter->state.thermoregulation.skin_temp = 33.0f;
    adapter->state.thermoregulation.heat_production = 0.5f;
    adapter->state.thermoregulation.heat_loss = 0.5f;

    /* Initialize appetite */
    adapter->state.appetite.blood_glucose.setpoint = adapter->config.glucose_setpoint;
    adapter->state.appetite.blood_glucose.current_value = adapter->config.glucose_setpoint;
    adapter->state.appetite.ghrelin_level = 0.3f;
    adapter->state.appetite.leptin_level = 0.5f;
    adapter->state.appetite.npy_level = 0.2f;
    adapter->state.appetite.pomc_level = 0.5f;

    /* Initialize hydration */
    adapter->state.hydration.osmolality.setpoint = adapter->config.osmolality_setpoint;
    adapter->state.hydration.osmolality.current_value = adapter->config.osmolality_setpoint;
    adapter->state.hydration.blood_volume.setpoint = 1.0f;
    adapter->state.hydration.blood_volume.current_value = 1.0f;
    adapter->state.hydration.vasopressin_level = 0.3f;

    /* Initialize HPA axis */
    adapter->state.hpa_axis.cortisol_level = adapter->config.cortisol_baseline;
    adapter->state.hpa_axis.crh_level = 0.2f;
    adapter->state.hpa_axis.acth_level = 0.2f;
    adapter->state.hpa_axis.hpa_sensitivity = NIMCP_SENSITIVITY_DEFAULT;

    /* Initialize autonomic */
    adapter->state.autonomic.sympathetic_tone = 0.3f;
    adapter->state.autonomic.parasympathetic_tone = 0.5f;
    adapter->state.autonomic.heart_rate_mod = 1.0f;
    adapter->state.autonomic.blood_pressure_mod = 1.0f;
    adapter->state.autonomic.respiratory_rate_mod = 1.0f;
    adapter->state.autonomic.pupil_dilation = 0.3f;
    adapter->state.autonomic.digestive_activity = 0.5f;

    /* Initialize status */
    adapter->state.status = HYPOTHALAMUS_STATUS_IDLE;
    adapter->state.last_error = HYPOTHALAMUS_ERROR_NONE;

    /* Initialize bio-async communication */
    adapter->bio_ctx = NULL;

    if (adapter->config.enable_bio_async && bio_router_is_initialized()) {
        LOG_DEBUG(HYPOTHALAMUS_LOG_MODULE, "Registering with bio-async router");

        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_HYPOTHALAMUS,
            .module_name = "hypothalamus",
            .inbox_capacity = 32,
            .user_data = adapter
        };

        adapter->bio_ctx = bio_router_register_module(&bio_info);
        if (adapter->bio_ctx) {
            /* Try KG-driven wiring callback first */
            nimcp_error_t cb_result = bio_router_register_wiring_callback(
                BIO_MODULE_HYPOTHALAMUS,
                (void*)hypothalamus_wiring_handler_callback,
                adapter
            );

            if (cb_result != NIMCP_SUCCESS) {
                /* Fall back to legacy direct registration */
                LOG_DEBUG(HYPOTHALAMUS_LOG_MODULE,
                          "KG wiring not available, using legacy registration");

                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(adapter->bio_ctx,
                        BIO_MSG_STRESS_INPUT, handle_stress_input_request)
                );
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(adapter->bio_ctx,
                        BIO_MSG_TEMPERATURE_INPUT, handle_temperature_input)
                );
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(adapter->bio_ctx,
                        BIO_MSG_STATE_QUERY, handle_state_query)
                );
            }

            LOG_INFO(HYPOTHALAMUS_LOG_MODULE, "Bio-async handlers registered");
        } else {
            LOG_WARNING(HYPOTHALAMUS_LOG_MODULE,
                        "Failed to register with bio-async router");
        }
    }

    /* Initialize statistics */
    memset(&adapter->stats, 0, sizeof(adapter->stats));

    adapter->previous_phase = adapter->state.circadian.period;

    LOG_INFO(HYPOTHALAMUS_LOG_MODULE, "Hypothalamus adapter created successfully");
    return adapter;
}

void hypothalamus_destroy(hypothalamus_adapter_t* adapter) {
    if (!adapter) return;

    LOG_INFO(HYPOTHALAMUS_LOG_MODULE, "Destroying hypothalamus adapter");

    /* Unregister from bio-async router */
    if (adapter->bio_ctx) {
        LOG_DEBUG(HYPOTHALAMUS_LOG_MODULE, "Unregistering from bio-async router");
        bio_router_unregister_module(adapter->bio_ctx);
        adapter->bio_ctx = NULL;
    }

    LOG_DEBUG(HYPOTHALAMUS_LOG_MODULE, "Hypothalamus adapter destroyed");
    nimcp_free(adapter);
}

bool hypothalamus_reset(hypothalamus_adapter_t* adapter) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypothalamus_reset: adapter is NULL");
        return false;
    }

    LOG_DEBUG(HYPOTHALAMUS_LOG_MODULE, "Resetting adapter state");

    /* Reset to initial configuration values */
    adapter->state.circadian.phase = adapter->config.initial_phase;
    adapter->state.circadian.amplitude = 1.0f;
    adapter->state.circadian.period = phase_to_period(adapter->config.initial_phase);
    adapter->state.circadian.sleep_pressure = 0.0f;

    /* Reset homeostatic parameters to setpoints */
    adapter->state.thermoregulation.core_temp.current_value =
        adapter->state.thermoregulation.core_temp.setpoint;
    adapter->state.thermoregulation.core_temp.error = 0.0f;
    adapter->state.thermoregulation.core_temp.integral_error = 0.0f;

    adapter->state.appetite.blood_glucose.current_value =
        adapter->state.appetite.blood_glucose.setpoint;
    adapter->state.appetite.hunger_drive = 0.0f;

    adapter->state.hydration.osmolality.current_value =
        adapter->state.hydration.osmolality.setpoint;
    adapter->state.hydration.thirst_drive = 0.0f;

    /* Reset HPA axis */
    adapter->state.hpa_axis.cortisol_level = adapter->config.cortisol_baseline;
    adapter->state.hpa_axis.stress_input = 0.0f;
    adapter->state.hpa_axis.chronic_stress = false;
    adapter->state.hpa_axis.activation_count = 0;

    /* Reset autonomic */
    adapter->state.autonomic.sympathetic_tone = 0.3f;
    adapter->state.autonomic.parasympathetic_tone = 0.5f;
    adapter->state.autonomic.fight_or_flight = false;
    adapter->state.autonomic.rest_and_digest = false;

    /* Reset status */
    adapter->state.status = HYPOTHALAMUS_STATUS_IDLE;
    adapter->state.last_error = HYPOTHALAMUS_ERROR_NONE;

    LOG_DEBUG(HYPOTHALAMUS_LOG_MODULE, "Adapter reset complete");
    return true;
}

/*=============================================================================
 * CIRCADIAN RHYTHM FUNCTIONS
 *===========================================================================*/

bool hypothalamus_update_circadian(hypothalamus_adapter_t* adapter,
                                    uint64_t delta_time_us) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypothalamus_update_circadian: adapter is NULL");
        return false;
    }
    if (!adapter->config.enable_circadian) {
        return false;  /* Feature disabled — not an error */
    }

    /* Convert delta time to hours for circadian calculation */
    float delta_hours = (float)delta_time_us / (float)US_PER_HOUR;

    /* Calculate phase increment */
    float phase_increment = (NIMCP_TWO_PI_F / adapter->config.circadian_period_hours) *
                            delta_hours;

    /* Update phase */
    adapter->state.circadian.phase += phase_increment;

    /* Normalize phase to [0, 2*PI] */
    while (adapter->state.circadian.phase >= NIMCP_TWO_PI_F) {
        adapter->state.circadian.phase -= NIMCP_TWO_PI_F;
    }
    while (adapter->state.circadian.phase < 0.0f) {
        adapter->state.circadian.phase += NIMCP_TWO_PI_F;
    }

    /* Update period */
    hypo_circadian_phase_t new_period = phase_to_period(adapter->state.circadian.phase);

    /* Detect phase transition */
    if (new_period != adapter->previous_phase) {
        adapter->state.status = HYPOTHALAMUS_STATUS_CIRCADIAN_SHIFT;

        /* Broadcast phase change */
        if (adapter->bio_ctx) {
            hypothalamus_broadcast_circadian_phase(adapter, new_period);
        }

        adapter->previous_phase = new_period;
    }

    adapter->state.circadian.period = new_period;

    /* Update melatonin level */
    adapter->state.circadian.melatonin_level = calculate_melatonin(
        adapter->state.circadian.phase);

    /* Update circadian cortisol component */
    float circadian_cortisol = calculate_circadian_cortisol(
        adapter->state.circadian.phase);

    /* Combine circadian and stress cortisol */
    adapter->state.circadian.cortisol_level =
        circadian_cortisol * 0.6f +
        adapter->state.hpa_axis.cortisol_level * 0.4f;

    /* Update sleep pressure (accumulates during wake, decreases during sleep) */
    float delta_seconds = (float)delta_time_us / (float)US_PER_SECOND;
    bool is_night = (adapter->state.circadian.period == HYPO_CIRCADIAN_PHASE_EARLY_NIGHT ||
                     adapter->state.circadian.period == HYPO_CIRCADIAN_PHASE_MID_NIGHT ||
                     adapter->state.circadian.period == HYPO_CIRCADIAN_PHASE_LATE_NIGHT);

    if (is_night) {
        /* Sleep pressure decreases during night */
        adapter->state.circadian.sleep_pressure = exponential_decay(
            adapter->state.circadian.sleep_pressure, 0.0f, 0.0001f, delta_seconds);
    } else {
        /* Sleep pressure increases during day */
        adapter->state.circadian.sleep_pressure = exponential_decay(
            adapter->state.circadian.sleep_pressure, 1.0f, 0.00002f, delta_seconds);
    }

    /* Calculate alertness */
    adapter->state.circadian.alertness = calculate_alertness(
        circadian_cortisol,
        adapter->state.circadian.melatonin_level,
        adapter->state.circadian.sleep_pressure);

    adapter->state.circadian.current_time_us += delta_time_us;
    adapter->stats.circadian_ticks++;

    return true;
}

hypo_circadian_phase_t hypothalamus_get_circadian_phase(
    const hypothalamus_adapter_t* adapter) {
    if (!adapter) return HYPO_CIRCADIAN_PHASE_EARLY_MORNING;
    return adapter->state.circadian.period;
}

bool hypothalamus_get_circadian_state(const hypothalamus_adapter_t* adapter,
                                       hypo_circadian_state_t* state) {
    if (!adapter || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypothalamus_get_circadian_state: required parameter is NULL (adapter, state)");
        return false;
    }
    *state = adapter->state.circadian;
    return true;
}

float hypothalamus_apply_light(hypothalamus_adapter_t* adapter,
                                float intensity,
                                float duration_ms) {
    if (!adapter || !adapter->config.enable_circadian) return 0.0f;

    /* Light causes phase-dependent shifts in the circadian rhythm */
    /* Light in early night delays phase, light in late night advances phase */

    intensity = nimcp_clamp01(intensity);
    float duration_hours = duration_ms / 3600000.0f;

    /* Calculate phase response curve (PRC) */
    float prc_sensitivity = 0.0f;

    hypo_circadian_phase_t phase = adapter->state.circadian.period;
    switch (phase) {
        case HYPO_CIRCADIAN_PHASE_EVENING:
        case HYPO_CIRCADIAN_PHASE_EARLY_NIGHT:
            /* Light delays phase (negative shift) */
            prc_sensitivity = -0.3f;
            break;
        case HYPO_CIRCADIAN_PHASE_MID_NIGHT:
            /* Minimal effect */
            prc_sensitivity = 0.0f;
            break;
        case HYPO_CIRCADIAN_PHASE_LATE_NIGHT:
        case HYPO_CIRCADIAN_PHASE_EARLY_MORNING:
            /* Light advances phase (positive shift) */
            prc_sensitivity = 0.2f;
            break;
        default:
            /* Daytime light has minimal effect */
            prc_sensitivity = 0.05f;
            break;
    }

    float phase_shift = prc_sensitivity * intensity * duration_hours;

    /* Apply shift */
    adapter->state.circadian.phase += phase_shift;

    /* Normalize */
    while (adapter->state.circadian.phase >= NIMCP_TWO_PI_F) {
        adapter->state.circadian.phase -= NIMCP_TWO_PI_F;
    }
    while (adapter->state.circadian.phase < 0.0f) {
        adapter->state.circadian.phase += NIMCP_TWO_PI_F;
    }

    /* Also suppress melatonin with bright light */
    if (intensity > 0.5f) {
        adapter->state.circadian.melatonin_level *= (1.0f - intensity * 0.5f);
        adapter->state.circadian.melatonin_level = nimcp_clamp01(
            adapter->state.circadian.melatonin_level);
    }

    LOG_DEBUG(HYPOTHALAMUS_LOG_MODULE,
              "Light exposure: intensity=%.2f, duration=%.1fms, phase_shift=%.4f",
              intensity, duration_ms, phase_shift);

    return phase_shift;
}

/*=============================================================================
 * HOMEOSTATIC REGULATION FUNCTIONS
 *===========================================================================*/

bool hypothalamus_update_homeostasis(hypothalamus_adapter_t* adapter,
                                      uint64_t delta_time_us) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypothalamus_update_homeostasis: adapter is NULL");
        return false;
    }

    float dt = (float)delta_time_us / (float)US_PER_SECOND;

    /* Update thermoregulation */
    thermoregulation_state_t* thermo = &adapter->state.thermoregulation;

    update_homeostatic_param(&thermo->core_temp, thermo->core_temp.current_value,
                             0.5f, 0.1f, dt);

    /* Determine thermoregulatory responses */
    /* temp_error = setpoint - current: negative when too hot, positive when too cold */
    float temp_error = thermo->core_temp.error;

    if (temp_error > 0.5f) {
        /* Too cold (setpoint > current) - activate heating */
        thermo->shivering_active = temp_error > 1.0f;
        thermo->vasoconstriction = true;
        thermo->vasodilation = false;
        thermo->sweating_active = false;
        thermo->heat_production = nimcp_clamp01(0.5f + temp_error * 0.3f);
        thermo->heat_loss = nimcp_clamp01(0.3f);

        if (temp_error > 2.0f) {
            adapter->state.status = HYPOTHALAMUS_STATUS_THERMAL_ALERT;
            adapter->stats.thermal_alerts++;
        }
    } else if (temp_error < -0.5f) {
        /* Too hot (current > setpoint) - activate cooling */
        thermo->sweating_active = temp_error < -1.0f;
        thermo->vasodilation = true;
        thermo->vasoconstriction = false;
        thermo->shivering_active = false;
        thermo->heat_production = nimcp_clamp01(0.3f);
        thermo->heat_loss = nimcp_clamp01(0.5f - temp_error * 0.3f);

        if (temp_error < -2.0f) {
            adapter->state.status = HYPOTHALAMUS_STATUS_THERMAL_ALERT;
            adapter->stats.thermal_alerts++;
        }
    } else {
        /* Normal range */
        thermo->shivering_active = false;
        thermo->sweating_active = false;
        thermo->vasoconstriction = false;
        thermo->vasodilation = false;
        thermo->heat_production = 0.5f;
        thermo->heat_loss = 0.5f;
    }

    /* Update appetite */
    if (adapter->config.enable_appetite) {
        appetite_state_t* appetite = &adapter->state.appetite;

        update_homeostatic_param(&appetite->blood_glucose,
                                  appetite->blood_glucose.current_value,
                                  0.3f, 0.05f, dt);

        /* Update hunger hormones based on glucose */
        /* glucose_error = setpoint - current: positive when glucose is low */
        float glucose_error = appetite->blood_glucose.error;

        if (glucose_error > 0) {
            /* Low glucose - increase ghrelin (hunger) */
            appetite->ghrelin_level = exponential_decay(
                appetite->ghrelin_level,
                nimcp_clamp01(0.5f + glucose_error * 0.02f),
                3.0f, dt);
            appetite->leptin_level = exponential_decay(
                appetite->leptin_level, 0.3f, 3.0f, dt);
        } else {
            /* Normal/high glucose - increase leptin (satiety) */
            appetite->ghrelin_level = exponential_decay(
                appetite->ghrelin_level, 0.2f, 3.0f, dt);
            appetite->leptin_level = exponential_decay(
                appetite->leptin_level,
                nimcp_clamp01(0.5f - glucose_error * 0.01f),
                3.0f, dt);
        }

        /* Calculate hunger drive */
        appetite->npy_level = appetite->ghrelin_level *
                              adapter->config.ghrelin_sensitivity;
        appetite->pomc_level = appetite->leptin_level *
                               adapter->config.leptin_sensitivity;

        appetite->hunger_drive = nimcp_clamp01(appetite->npy_level - appetite->pomc_level);
        appetite->satiety_signal = nimcp_clamp01(appetite->pomc_level - appetite->npy_level);

        appetite->feeding_motivated = appetite->hunger_drive >
                                       adapter->config.hunger_threshold;

        if (appetite->feeding_motivated) {
            adapter->state.status = HYPOTHALAMUS_STATUS_HUNGER_DRIVE;
            adapter->stats.hunger_episodes++;
        }
    }

    /* Update hydration */
    hydration_state_t* hydration = &adapter->state.hydration;

    update_homeostatic_param(&hydration->osmolality,
                              hydration->osmolality.current_value,
                              0.3f, 0.05f, dt);

    /* Update vasopressin based on osmolality */
    /* osmo_error = setpoint - current: negative when osmolality is high */
    float osmo_error = hydration->osmolality.error;

    if (osmo_error < 0) {
        /* High osmolality (current > setpoint) - increase vasopressin */
        hydration->vasopressin_level = exponential_decay(
            hydration->vasopressin_level,
            nimcp_clamp01(0.5f - osmo_error * 0.01f),
            3.0f, dt);
    } else {
        /* Low osmolality - decrease vasopressin */
        hydration->vasopressin_level = exponential_decay(
            hydration->vasopressin_level, 0.3f, 3.0f, dt);
    }

    /* Calculate thirst drive */
    /* Negative osmo_error means high osmolality, so negate to get positive drive */
    hydration->thirst_drive = nimcp_clamp01(-osmo_error * 0.01f +
                                       (1.0f - hydration->blood_volume.current_value));

    hydration->drinking_motivated = hydration->thirst_drive >
                                     adapter->config.thirst_threshold;

    if (hydration->drinking_motivated) {
        adapter->state.status = HYPOTHALAMUS_STATUS_THIRST_DRIVE;
        adapter->stats.thirst_episodes++;
    }

    adapter->stats.homeostatic_corrections++;
    return true;
}

bool hypothalamus_set_temperature(hypothalamus_adapter_t* adapter,
                                   float temperature_c) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypothalamus_set_temperature: adapter is NULL");
        return false;
    }
    adapter->state.thermoregulation.core_temp.current_value = temperature_c;
    return true;
}

bool hypothalamus_set_blood_glucose(hypothalamus_adapter_t* adapter,
                                     float glucose_mg_dl) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypothalamus_set_blood_glucose: adapter is NULL");
        return false;
    }
    adapter->state.appetite.blood_glucose.current_value = glucose_mg_dl;
    return true;
}

bool hypothalamus_set_osmolality(hypothalamus_adapter_t* adapter,
                                  float osmolality_mosm) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypothalamus_set_osmolality: adapter is NULL");
        return false;
    }
    adapter->state.hydration.osmolality.current_value = osmolality_mosm;
    return true;
}

bool hypothalamus_get_thermoregulation(const hypothalamus_adapter_t* adapter,
                                        thermoregulation_state_t* state) {
    if (!adapter || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypothalamus_get_thermoregulation: adapter or state is NULL");
        return false;
    }
    *state = adapter->state.thermoregulation;
    return true;
}

bool hypothalamus_get_appetite(const hypothalamus_adapter_t* adapter,
                                appetite_state_t* state) {
    if (!adapter || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypothalamus_get_appetite: adapter or state is NULL");
        return false;
    }
    *state = adapter->state.appetite;
    return true;
}

bool hypothalamus_get_hydration(const hypothalamus_adapter_t* adapter,
                                 hydration_state_t* state) {
    if (!adapter || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypothalamus_get_hydration: adapter or state is NULL");
        return false;
    }
    *state = adapter->state.hydration;
    return true;
}

/*=============================================================================
 * HPA AXIS (STRESS RESPONSE) FUNCTIONS
 *===========================================================================*/

bool hypothalamus_update_hpa_axis(hypothalamus_adapter_t* adapter,
                                   uint64_t delta_time_us) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypothalamus_update_hpa_axis: adapter is NULL");
        return false;
    }
    if (!adapter->config.enable_hpa_axis) {
        return false;  /* Feature disabled — not an error */
    }

    float dt = (float)delta_time_us / (float)US_PER_SECOND;
    hpa_axis_state_t* hpa = &adapter->state.hpa_axis;

    /* Calculate negative feedback from cortisol */
    hpa->negative_feedback = hpa->cortisol_level *
                             adapter->config.cortisol_feedback_gain;

    /* Calculate effective stress input (stress minus feedback) */
    float effective_stress = nimcp_clamp01(hpa->stress_input - hpa->negative_feedback);

    /* Update CRH from PVN */
    float crh_target = effective_stress * adapter->config.crh_sensitivity *
                       hpa->hpa_sensitivity;
    hpa->crh_level = exponential_decay(hpa->crh_level, crh_target, 50.0f, dt);

    /* Update ACTH from pituitary (delayed response to CRH) */
    float acth_target = hpa->crh_level * 0.8f;
    hpa->acth_level = exponential_decay(hpa->acth_level, acth_target, 30.0f, dt);

    /* Update cortisol from adrenal (further delayed) */
    float cortisol_target = hpa->acth_level * 0.7f + adapter->config.cortisol_baseline;
    float cortisol_baseline_component = calculate_circadian_cortisol(
        adapter->state.circadian.phase) * adapter->config.cortisol_baseline;
    cortisol_target = nimcp_clamp01(cortisol_target + cortisol_baseline_component);

    hpa->cortisol_level = exponential_decay(hpa->cortisol_level, cortisol_target,
                                             20.0f, dt);

    /* Detect chronic stress (sustained high cortisol) */
    if (hpa->cortisol_level > 0.7f) {
        hpa->activation_count++;
        if (hpa->activation_count > 100) {
            if (!hpa->chronic_stress) {
                hpa->chronic_stress = true;
                adapter->stats.chronic_stress_episodes++;
                LOG_WARNING(HYPOTHALAMUS_LOG_MODULE, "Chronic stress detected");

                /* HPA axis becomes less sensitive with chronic stress */
                hpa->hpa_sensitivity *= 0.99f;
            }
        }
    } else {
        hpa->activation_count = 0;
        if (hpa->chronic_stress && hpa->cortisol_level < 0.4f) {
            hpa->chronic_stress = false;
            /* Slow recovery of HPA sensitivity */
            hpa->hpa_sensitivity = exponential_decay(
                hpa->hpa_sensitivity, 1.0f, 0.0001f, dt);
        }
    }

    /* Decay stress input over time (slow decay - stress lingers) */
    hpa->stress_input = exponential_decay(hpa->stress_input, 0.0f, 0.005f, dt);

    /* Update status */
    if (hpa->cortisol_level > 0.6f) {
        adapter->state.status = HYPOTHALAMUS_STATUS_STRESS_RESPONSE;
    }

    return true;
}

float hypothalamus_apply_stress(hypothalamus_adapter_t* adapter,
                                 float stress_level) {
    if (!adapter || !adapter->config.enable_hpa_axis) return 0.0f;

    stress_level = nimcp_clamp01(stress_level);
    hpa_axis_state_t* hpa = &adapter->state.hpa_axis;

    /* Add stress to cumulative input */
    hpa->stress_input = nimcp_clamp01(hpa->stress_input + stress_level);

    hpa->last_activation_us = adapter->state.current_time_us;
    adapter->stats.stress_activations++;

    /* Broadcast stress response */
    if (adapter->bio_ctx && stress_level > 0.3f) {
        hypothalamus_broadcast_stress_response(adapter, hpa->cortisol_level);
    }

    LOG_DEBUG(HYPOTHALAMUS_LOG_MODULE, "Stress applied: level=%.2f, cortisol=%.2f",
              stress_level, hpa->cortisol_level);

    return hpa->cortisol_level;
}

bool hypothalamus_get_hpa_state(const hypothalamus_adapter_t* adapter,
                                 hpa_axis_state_t* state) {
    if (!adapter || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypothalamus_get_hpa_state: adapter or state is NULL");
        return false;
    }
    *state = adapter->state.hpa_axis;
    return true;
}

float hypothalamus_get_cortisol(const hypothalamus_adapter_t* adapter) {
    if (!adapter) return 0.0f;
    return adapter->state.hpa_axis.cortisol_level;
}

/*=============================================================================
 * AUTONOMIC CONTROL FUNCTIONS
 *===========================================================================*/

bool hypothalamus_update_autonomic(hypothalamus_adapter_t* adapter,
                                    uint64_t delta_time_us) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypothalamus_update_autonomic: adapter is NULL");
        return false;
    }
    if (!adapter->config.enable_autonomic) {
        return false;  /* Feature disabled — not an error */
    }

    float dt = (float)delta_time_us / (float)US_PER_SECOND;
    autonomic_state_t* ans = &adapter->state.autonomic;

    /* Integrate inputs from various systems */
    float stress_input = adapter->state.hpa_axis.cortisol_level;
    float circadian_input = adapter->state.circadian.alertness;
    float thermal_input = fabsf(adapter->state.thermoregulation.core_temp.error) / 2.0f;

    /* Calculate sympathetic drive */
    float sympathetic_target = nimcp_clamp01(
        stress_input * 0.8f +
        circadian_input * 0.1f +
        thermal_input * 0.05f +
        adapter->config.sympathetic_bias * 0.05f);

    /* Calculate parasympathetic drive (generally reciprocal to stress) */
    float parasympathetic_target = nimcp_clamp01(
        (1.0f - stress_input) * 0.8f +
        (1.0f - circadian_input) * 0.1f +
        0.1f);

    /* Update with dynamics */
    ans->sympathetic_tone = exponential_decay(
        ans->sympathetic_tone, sympathetic_target, 20.0f, dt);
    ans->parasympathetic_tone = exponential_decay(
        ans->parasympathetic_tone, parasympathetic_target, 20.0f, dt);

    /* Calculate autonomic outputs */
    float balance = ans->sympathetic_tone - ans->parasympathetic_tone;

    /* Heart rate: sympathetic increases, parasympathetic decreases */
    ans->heart_rate_mod = 1.0f + balance * 0.5f;

    /* Blood pressure: sympathetic increases */
    ans->blood_pressure_mod = 1.0f + ans->sympathetic_tone * 0.3f;

    /* Respiratory rate: sympathetic increases */
    ans->respiratory_rate_mod = 1.0f + ans->sympathetic_tone * 0.2f;

    /* Pupil: sympathetic dilates */
    ans->pupil_dilation = nimcp_clamp01(0.3f + ans->sympathetic_tone * 0.5f);

    /* Digestion: parasympathetic increases */
    ans->digestive_activity = nimcp_clamp01(ans->parasympathetic_tone * 0.8f);

    /* Detect discrete states */
    bool was_fight_or_flight = ans->fight_or_flight;
    bool was_rest_and_digest = ans->rest_and_digest;

    ans->fight_or_flight = ans->sympathetic_tone > 0.7f &&
                           ans->parasympathetic_tone < 0.3f;
    ans->rest_and_digest = ans->parasympathetic_tone > 0.6f &&
                           ans->sympathetic_tone < 0.4f;

    /* Track state transitions */
    if (ans->fight_or_flight && !was_fight_or_flight) {
        adapter->state.status = HYPOTHALAMUS_STATUS_AUTONOMIC_ALERT;
        adapter->stats.sympathetic_bursts++;
        emit_alert(adapter, HYPOTHALAMUS_STATUS_AUTONOMIC_ALERT, ans);
    }

    if (ans->rest_and_digest && !was_rest_and_digest) {
        adapter->stats.parasympathetic_switches++;
    }

    /* Call autonomic callback if registered */
    if (adapter->autonomic_callback) {
        adapter->autonomic_callback(ans, adapter->autonomic_user_data);
    }

    return true;
}

bool hypothalamus_get_autonomic(const hypothalamus_adapter_t* adapter,
                                 autonomic_state_t* state) {
    if (!adapter || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypothalamus_get_autonomic: adapter or state is NULL");
        return false;
    }
    *state = adapter->state.autonomic;
    return true;
}

float hypothalamus_get_autonomic_balance(const hypothalamus_adapter_t* adapter) {
    if (!adapter) return 0.5f;

    const autonomic_state_t* ans = &adapter->state.autonomic;
    float total = ans->sympathetic_tone + ans->parasympathetic_tone;

    if (total < 0.01f) return 0.5f;

    return ans->sympathetic_tone / total;
}

/*=============================================================================
 * INTEGRATED UPDATE FUNCTION
 *===========================================================================*/

bool hypothalamus_update(hypothalamus_adapter_t* adapter,
                          uint64_t delta_time_us) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypothalamus_update: adapter is NULL");
        return false;
    }

    uint64_t start_time = adapter->state.current_time_us;

    /* Reset status for this cycle */
    adapter->state.status = HYPOTHALAMUS_STATUS_IDLE;

    /* Update all subsystems in order */
    hypothalamus_update_circadian(adapter, delta_time_us);
    hypothalamus_update_homeostasis(adapter, delta_time_us);
    hypothalamus_update_hpa_axis(adapter, delta_time_us);
    hypothalamus_update_autonomic(adapter, delta_time_us);

    /* Update timing */
    adapter->state.current_time_us += delta_time_us;
    adapter->stats.updates_processed++;

    /* Calculate update latency */
    uint64_t latency = adapter->state.current_time_us - start_time - delta_time_us;
    adapter->stats.avg_update_latency_us =
        (adapter->stats.avg_update_latency_us *
         (adapter->stats.updates_processed - 1) + (float)latency) /
        adapter->stats.updates_processed;

    if ((float)latency > adapter->stats.max_update_latency_us) {
        adapter->stats.max_update_latency_us = (float)latency;
    }

    return true;
}

bool hypothalamus_get_state(const hypothalamus_adapter_t* adapter,
                             hypothalamus_state_t* state) {
    if (!adapter || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypothalamus_get_state: adapter or state is NULL");
        return false;
    }
    *state = adapter->state;
    return true;
}

/*=============================================================================
 * STATUS AND DIAGNOSTICS
 *===========================================================================*/

hypothalamus_status_t hypothalamus_get_status(
    const hypothalamus_adapter_t* adapter) {
    if (!adapter) return HYPOTHALAMUS_STATUS_ERROR;
    return adapter->state.status;
}

hypothalamus_error_t hypothalamus_get_last_error(
    const hypothalamus_adapter_t* adapter) {
    if (!adapter) return HYPOTHALAMUS_ERROR_INTERNAL;
    return adapter->state.last_error;
}

const char* hypothalamus_error_string(hypothalamus_error_t error) {
    switch (error) {
        case HYPOTHALAMUS_ERROR_NONE: return "No error";
        case HYPOTHALAMUS_ERROR_INVALID_CONFIG: return "Invalid configuration";
        case HYPOTHALAMUS_ERROR_HOMEOSTATIC_FAILURE: return "Homeostatic regulation failure";
        case HYPOTHALAMUS_ERROR_CIRCADIAN_DISRUPTION: return "Circadian rhythm disruption";
        case HYPOTHALAMUS_ERROR_HPA_DYSFUNCTION: return "HPA axis dysfunction";
        case HYPOTHALAMUS_ERROR_AUTONOMIC_IMBALANCE: return "Autonomic imbalance";
        case HYPOTHALAMUS_ERROR_LIMBIC_DISCONNECT: return "Limbic system disconnection";
        case HYPOTHALAMUS_ERROR_PITUITARY_FAILURE: return "Pituitary link failure";
        case HYPOTHALAMUS_ERROR_INTERNAL: return "Internal error";
        default: return "Unknown error";
    }
}

const char* hypothalamus_status_string(hypothalamus_status_t status) {
    switch (status) {
        case HYPOTHALAMUS_STATUS_IDLE: return "Idle";
        case HYPOTHALAMUS_STATUS_STRESS_RESPONSE: return "Stress response active";
        case HYPOTHALAMUS_STATUS_THERMAL_ALERT: return "Thermal alert";
        case HYPOTHALAMUS_STATUS_HUNGER_DRIVE: return "Hunger drive active";
        case HYPOTHALAMUS_STATUS_THIRST_DRIVE: return "Thirst drive active";
        case HYPOTHALAMUS_STATUS_CIRCADIAN_SHIFT: return "Circadian phase shift";
        case HYPOTHALAMUS_STATUS_AUTONOMIC_ALERT: return "Autonomic alert";
        case HYPOTHALAMUS_STATUS_ERROR: return "Error";
        default: return "Unknown";
    }
}

bool hypothalamus_get_stats(const hypothalamus_adapter_t* adapter,
                             hypothalamus_stats_t* stats) {
    if (!adapter || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypothalamus_get_stats: adapter or stats is NULL");
        return false;
    }
    *stats = adapter->stats;
    return true;
}

bool hypothalamus_get_config(const hypothalamus_adapter_t* adapter,
                              hypothalamus_config_t* config) {
    if (!adapter || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypothalamus_get_config: adapter or config is NULL");
        return false;
    }
    *config = adapter->config;
    return true;
}

/*=============================================================================
 * CALLBACK REGISTRATION
 *===========================================================================*/

bool hypothalamus_set_alert_callback(hypothalamus_adapter_t* adapter,
                                      hypothalamus_alert_callback_t callback,
                                      void* user_data) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypothalamus_set_alert_callback: adapter is NULL");
        return false;
    }
    adapter->alert_callback = callback;
    adapter->alert_user_data = user_data;
    return true;
}

bool hypothalamus_set_hormone_callback(hypothalamus_adapter_t* adapter,
                                        hypothalamus_hormone_callback_t callback,
                                        void* user_data) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypothalamus_set_hormone_callback: adapter is NULL");
        return false;
    }
    adapter->hormone_callback = callback;
    adapter->hormone_user_data = user_data;
    return true;
}

bool hypothalamus_set_autonomic_callback(hypothalamus_adapter_t* adapter,
                                          hypothalamus_autonomic_callback_t callback,
                                          void* user_data) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypothalamus_set_autonomic_callback: adapter is NULL");
        return false;
    }
    adapter->autonomic_callback = callback;
    adapter->autonomic_user_data = user_data;
    return true;
}

/*=============================================================================
 * BIO-ASYNC COMMUNICATION
 *===========================================================================*/

bio_module_context_t hypothalamus_get_bio_context(
    hypothalamus_adapter_t* adapter) {
    if (!adapter) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adapter is NULL");

        return NULL;

    }
    return adapter->bio_ctx;
}

uint32_t hypothalamus_process_bio_messages(hypothalamus_adapter_t* adapter,
                                            uint32_t max_messages) {
    if (!adapter || !adapter->bio_ctx) return 0;

    uint32_t processed = bio_router_process_inbox(adapter->bio_ctx, max_messages);
    if (processed > 0) {
        LOG_DEBUG(HYPOTHALAMUS_LOG_MODULE, "Processed %u bio-async messages",
                  processed);
    }
    return processed;
}

nimcp_error_t hypothalamus_broadcast_circadian_phase(
    hypothalamus_adapter_t* adapter,
    hypo_circadian_phase_t new_phase) {

    if (!adapter) return NIMCP_BIO_ERROR_NOT_INITIALIZED;
    if (!adapter->bio_ctx) return NIMCP_SUCCESS;  /* Not an error if disabled */

    LOG_DEBUG(HYPOTHALAMUS_LOG_MODULE,
              "Broadcasting circadian phase: %d", new_phase);

    /* Create circadian phase message */
    bio_message_header_t msg;
    memset(&msg, 0, sizeof(msg));

    msg.type = BIO_MSG_CIRCADIAN_PHASE_CHANGE;
    msg.source_module = BIO_MODULE_HYPOTHALAMUS;
    msg.target_module = 0;  /* Broadcast */
    msg.payload_size = sizeof(hypo_circadian_phase_t);
    msg.channel = adapter->config.default_channel;
    msg.flags = BIO_MSG_FLAG_BROADCAST;

    /* Note: Full implementation would include phase data in payload */
    return bio_router_broadcast(adapter->bio_ctx, &msg, sizeof(msg));
}

nimcp_error_t hypothalamus_broadcast_stress_response(
    hypothalamus_adapter_t* adapter,
    float cortisol_level) {

    if (!adapter) return NIMCP_BIO_ERROR_NOT_INITIALIZED;
    if (!adapter->bio_ctx) return NIMCP_SUCCESS;

    LOG_DEBUG(HYPOTHALAMUS_LOG_MODULE,
              "Broadcasting stress response: cortisol=%.2f", cortisol_level);

    bio_message_header_t msg;
    memset(&msg, 0, sizeof(msg));

    msg.type = BIO_MSG_STRESS_RESPONSE;
    msg.source_module = BIO_MODULE_HYPOTHALAMUS;
    msg.target_module = 0;
    msg.payload_size = sizeof(float);
    msg.channel = BIO_CHANNEL_NOREPINEPHRINE;  /* Stress uses NE */
    msg.flags = BIO_MSG_FLAG_BROADCAST;

    return bio_router_broadcast(adapter->bio_ctx, &msg, sizeof(msg));
}

nimcp_error_t hypothalamus_broadcast_homeostatic_alert(
    hypothalamus_adapter_t* adapter,
    uint32_t alert_type,
    float urgency) {

    if (!adapter) return NIMCP_BIO_ERROR_NOT_INITIALIZED;
    if (!adapter->bio_ctx) return NIMCP_SUCCESS;

    LOG_DEBUG(HYPOTHALAMUS_LOG_MODULE,
              "Broadcasting homeostatic alert: type=%u, urgency=%.2f",
              alert_type, urgency);

    bio_message_header_t msg;
    memset(&msg, 0, sizeof(msg));

    msg.type = BIO_MSG_HOMEOSTATIC_ALERT;
    msg.source_module = BIO_MODULE_HYPOTHALAMUS;
    msg.target_module = 0;
    msg.payload_size = sizeof(uint32_t) + sizeof(float);
    msg.channel = adapter->config.default_channel;
    msg.flags = BIO_MSG_FLAG_BROADCAST;

    return bio_router_broadcast(adapter->bio_ctx, &msg, sizeof(msg));
}

/*=============================================================================
 * BIO-ASYNC MESSAGE HANDLERS (Implementation)
 *===========================================================================*/

static nimcp_error_t handle_stress_input_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data) {

    hypothalamus_adapter_t* adapter = (hypothalamus_adapter_t*)user_data;
    if (!adapter || !msg) {
        LOG_ERROR(HYPOTHALAMUS_LOG_MODULE, "Invalid stress input request");
        return NIMCP_BIO_ERROR_NOT_INITIALIZED;
    }

    /* Extract stress level from message (simplified) */
    float stress_level = 0.5f;  /* Default if not provided */

    /* Apply stress */
    float cortisol = hypothalamus_apply_stress(adapter, stress_level);

    LOG_DEBUG(HYPOTHALAMUS_LOG_MODULE,
              "Handled stress input: level=%.2f, cortisol=%.2f",
              stress_level, cortisol);

    /* Complete promise with cortisol level */
    if (response_promise) {
        nimcp_bio_promise_complete(response_promise, &cortisol);
    }

    return NIMCP_SUCCESS;
}

static nimcp_error_t handle_temperature_input(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data) {

    hypothalamus_adapter_t* adapter = (hypothalamus_adapter_t*)user_data;
    if (!adapter || !msg) {
        LOG_ERROR(HYPOTHALAMUS_LOG_MODULE, "Invalid temperature input");
        return NIMCP_BIO_ERROR_NOT_INITIALIZED;
    }

    /* Extract temperature from message (simplified) */
    float temperature = 37.0f;  /* Default */

    hypothalamus_set_temperature(adapter, temperature);

    LOG_DEBUG(HYPOTHALAMUS_LOG_MODULE, "Handled temperature input: %.1f C",
              temperature);

    (void)response_promise;  /* No response needed */
    return NIMCP_SUCCESS;
}

static nimcp_error_t handle_state_query(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data) {

    hypothalamus_adapter_t* adapter = (hypothalamus_adapter_t*)user_data;
    if (!adapter || !msg) {
        LOG_ERROR(HYPOTHALAMUS_LOG_MODULE, "Invalid state query");
        return NIMCP_BIO_ERROR_NOT_INITIALIZED;
    }

    LOG_DEBUG(HYPOTHALAMUS_LOG_MODULE, "Handling state query");

    /* Build response with current state */
    if (response_promise) {
        nimcp_bio_promise_complete(response_promise, &adapter->state);
    }

    return NIMCP_SUCCESS;
}
