/**
 * @file nimcp_interoceptive_prediction.c
 * @brief Implementation of Visceral Prediction and Interoceptive Processing
 *
 * This implementation provides predictive processing of internal body signals
 * and homeostatic integration for embodied cognition.
 *
 * Biological basis:
 * - Insular cortex processes visceral signals
 * - Prediction errors drive interoceptive learning
 * - Allostasis maintains stability through change
 */

#include "embodiment/nimcp_interoceptive_prediction.h"
#include "constants/nimcp_constants.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

#define LOG_MODULE "interoceptive_prediction"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(interoceptive_prediction)

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Prediction model for a signal
 */
typedef struct {
    nimcp_intero_signal_type_t signal; /**< Signal type */
    double predicted_value;           /**< Current prediction */
    double prediction_variance;       /**< Prediction uncertainty */
    double learning_rate;             /**< Current learning rate */

    /* History for prediction */
    double value_history[NIMCP_INTERO_HISTORY_SIZE];
    uint32_t history_index;
    uint32_t history_count;

    /* Statistics */
    double total_error;
    uint32_t prediction_count;
} nimcp_prediction_model_t;

/**
 * @brief Internal interoceptive context
 */
struct nimcp_intero_context {
    nimcp_intero_config_t config;     /**< Configuration */
    bool initialized;                  /**< Initialization flag */

    /* Organ systems */
    nimcp_system_state_t systems[NIMCP_INTERO_MAX_SYSTEMS];
    bool system_active[NIMCP_INTERO_MAX_SYSTEMS];
    uint32_t num_systems;
    uint32_t next_system_id;

    /* Prediction models */
    nimcp_prediction_model_t predictions[NIMCP_SIGNAL_COUNT];

    /* Homeostatic setpoints */
    nimcp_setpoint_t setpoints[NIMCP_INTERO_MAX_SETPOINTS];
    uint32_t num_setpoints;

    /* Current signal values */
    double signal_values[NIMCP_SIGNAL_COUNT];
    bool signal_valid[NIMCP_SIGNAL_COUNT];

    /* Allostatic load tracking */
    nimcp_allostatic_load_t allostatic_load;
    double acute_stress_level;
    double chronic_stress_accumulator;
    uint64_t stress_start_time;

    /* Emotional state */
    nimcp_emotional_state_t emotional_state;

    /* Statistics */
    nimcp_intero_stats_t stats;

    /* Timing */
    uint64_t last_update_time;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get current timestamp in nanoseconds
 */
static inline uint64_t get_timestamp_ns(void) {
    return nimcp_time_get_us() * 1000ULL;
}

/**
 * @brief Find system by ID
 */
static nimcp_system_state_t* find_system(
    nimcp_intero_context_t* ctx,
    uint32_t system_id
) {
    for (uint32_t i = 0; i < NIMCP_INTERO_MAX_SYSTEMS; i++) {
        if (ctx->system_active[i] && ctx->systems[i].system_id == system_id) {
            return &ctx->systems[i];
        }
    }
    return NULL;
}

/**
 * @brief Find system by type
 */
static nimcp_system_state_t* find_system_by_type(
    nimcp_intero_context_t* ctx,
    nimcp_organ_system_t type
) {
    for (uint32_t i = 0; i < NIMCP_INTERO_MAX_SYSTEMS; i++) {
        if (ctx->system_active[i] && ctx->systems[i].type == type) {
            return &ctx->systems[i];
        }
    }
    return NULL;
}

/**
 * @brief Find signal in system
 */
static nimcp_intero_signal_t* find_signal_in_system(
    nimcp_system_state_t* system,
    nimcp_intero_signal_type_t type
) {
    for (uint32_t i = 0; i < system->num_signals; i++) {
        if (system->signals[i].type == type) {
            return &system->signals[i];
        }
    }
    return NULL;
}

/**
 * @brief Find setpoint by signal
 */
static nimcp_setpoint_t* find_setpoint(
    nimcp_intero_context_t* ctx,
    nimcp_intero_signal_type_t signal
) {
    for (uint32_t i = 0; i < ctx->num_setpoints; i++) {
        if (ctx->setpoints[i].signal == signal) {
            return &ctx->setpoints[i];
        }
    }
    return NULL;
}

/**
 * @brief Get default signal ranges
 */
static void get_signal_defaults(
    nimcp_intero_signal_type_t type,
    double* target,
    double* tol_low,
    double* tol_high,
    double* crit_low,
    double* crit_high
) {
    switch (type) {
        case NIMCP_SIGNAL_HEART_RATE:
            *target = 70.0; *tol_low = 60.0; *tol_high = 80.0;
            *crit_low = 40.0; *crit_high = 120.0;
            break;
        case NIMCP_SIGNAL_BLOOD_PRESSURE_SYS:
            *target = 120.0; *tol_low = 110.0; *tol_high = 130.0;
            *crit_low = 80.0; *crit_high = 180.0;
            break;
        case NIMCP_SIGNAL_BLOOD_PRESSURE_DIA:
            *target = 80.0; *tol_low = 70.0; *tol_high = 90.0;
            *crit_low = 50.0; *crit_high = 110.0;
            break;
        case NIMCP_SIGNAL_BREATHING_RATE:
            *target = 14.0; *tol_low = 12.0; *tol_high = 18.0;
            *crit_low = 8.0; *crit_high = 30.0;
            break;
        case NIMCP_SIGNAL_OXYGEN_SAT:
            *target = 98.0; *tol_low = 95.0; *tol_high = 100.0;
            *crit_low = 90.0; *crit_high = 100.0;
            break;
        case NIMCP_SIGNAL_CORE_TEMP:
            *target = 37.0; *tol_low = 36.5; *tol_high = 37.5;
            *crit_low = 35.0; *crit_high = 40.0;
            break;
        case NIMCP_SIGNAL_GLUCOSE:
            *target = 100.0; *tol_low = 80.0; *tol_high = 120.0;
            *crit_low = 50.0; *crit_high = 200.0;
            break;
        case NIMCP_SIGNAL_HUNGER:
        case NIMCP_SIGNAL_THIRST:
        case NIMCP_SIGNAL_FATIGUE:
        case NIMCP_SIGNAL_MUSCLE_TENSION:
        case NIMCP_SIGNAL_INFLAMMATION:
        case NIMCP_SIGNAL_PAIN_INTENSITY:
        case NIMCP_SIGNAL_DISCOMFORT:
            *target = 0.2; *tol_low = 0.0; *tol_high = 0.4;
            *crit_low = 0.0; *crit_high = 0.8;
            break;
        case NIMCP_SIGNAL_ENERGY_LEVEL:
        case NIMCP_SIGNAL_SATIETY:
            *target = 0.7; *tol_low = 0.5; *tol_high = 0.9;
            *crit_low = 0.2; *crit_high = 1.0;
            break;
        default:
            *target = 0.5; *tol_low = 0.3; *tol_high = 0.7;
            *crit_low = 0.1; *crit_high = 0.9;
            break;
    }
}

/**
 * @brief Compute homeostatic state from deviation
 */
static nimcp_homeostatic_state_t compute_homeo_state(
    const nimcp_setpoint_t* setpoint,
    double value
) {
    if (value < setpoint->critical_low || value > setpoint->critical_high) {
        return NIMCP_HOMEO_CRITICAL;
    }
    if (value < setpoint->tolerance_low * 0.8 || value > setpoint->tolerance_high * 1.2) {
        return NIMCP_HOMEO_SEVERE_DEVIATION;
    }
    if (value < setpoint->tolerance_low * 0.9 || value > setpoint->tolerance_high * 1.1) {
        return NIMCP_HOMEO_MODERATE_DEVIATION;
    }
    if (value < setpoint->tolerance_low || value > setpoint->tolerance_high) {
        return NIMCP_HOMEO_MILD_DEVIATION;
    }
    return NIMCP_HOMEO_OPTIMAL;
}

/**
 * @brief Update prediction model
 */
static void update_prediction_model(
    nimcp_prediction_model_t* model,
    double actual_value,
    double learning_rate
) {
    double error = actual_value - model->predicted_value;

    /* Update prediction using exponential moving average */
    model->predicted_value += learning_rate * error;

    /* Update variance estimate */
    model->prediction_variance = 0.9 * model->prediction_variance + 0.1 * error * error;

    /* Update history */
    model->value_history[model->history_index] = actual_value;
    model->history_index = (model->history_index + 1) % NIMCP_INTERO_HISTORY_SIZE;
    if (model->history_count < NIMCP_INTERO_HISTORY_SIZE) {
        model->history_count++;
    }

    /* Update statistics */
    model->total_error += fabs(error);
    model->prediction_count++;
}

/**
 * @brief Update emotional state from interoceptive signals
 */
static void update_emotional_state(nimcp_intero_context_t* ctx) {
    if (!ctx->config.enable_emotional_mapping) {
        return;
    }

    /* Compute arousal from physiological markers */
    double arousal = 0.0;
    int arousal_count = 0;

    if (ctx->signal_valid[NIMCP_SIGNAL_HEART_RATE]) {
        /* Higher HR = higher arousal */
        double hr = ctx->signal_values[NIMCP_SIGNAL_HEART_RATE];
        arousal += (hr - 60.0) / 60.0;  /* Normalize: 60-120 bpm -> 0-1 */
        arousal_count++;
    }
    if (ctx->signal_valid[NIMCP_SIGNAL_BREATHING_RATE]) {
        double br = ctx->signal_values[NIMCP_SIGNAL_BREATHING_RATE];
        arousal += (br - 12.0) / 12.0;  /* Normalize */
        arousal_count++;
    }
    if (ctx->signal_valid[NIMCP_SIGNAL_CORTISOL]) {
        arousal += ctx->signal_values[NIMCP_SIGNAL_CORTISOL];
        arousal_count++;
    }
    if (ctx->signal_valid[NIMCP_SIGNAL_ADRENALINE]) {
        arousal += ctx->signal_values[NIMCP_SIGNAL_ADRENALINE];
        arousal_count++;
    }

    if (arousal_count > 0) {
        arousal /= arousal_count;
    }
    arousal = fmax(0.0, fmin(1.0, arousal));

    /* Compute valence from wellbeing markers */
    double valence = 0.0;
    int valence_count = 0;

    /* Positive contributors */
    if (ctx->signal_valid[NIMCP_SIGNAL_ENERGY_LEVEL]) {
        valence += ctx->signal_values[NIMCP_SIGNAL_ENERGY_LEVEL] - 0.5;
        valence_count++;
    }
    if (ctx->signal_valid[NIMCP_SIGNAL_SATIETY]) {
        valence += (ctx->signal_values[NIMCP_SIGNAL_SATIETY] - 0.5) * 0.5;
        valence_count++;
    }
    if (ctx->signal_valid[NIMCP_SIGNAL_DOPAMINE]) {
        valence += ctx->signal_values[NIMCP_SIGNAL_DOPAMINE] - 0.5;
        valence_count++;
    }
    if (ctx->signal_valid[NIMCP_SIGNAL_SEROTONIN]) {
        valence += ctx->signal_values[NIMCP_SIGNAL_SEROTONIN] - 0.5;
        valence_count++;
    }

    /* Negative contributors */
    if (ctx->signal_valid[NIMCP_SIGNAL_PAIN_INTENSITY]) {
        valence -= ctx->signal_values[NIMCP_SIGNAL_PAIN_INTENSITY];
        valence_count++;
    }
    if (ctx->signal_valid[NIMCP_SIGNAL_FATIGUE]) {
        valence -= ctx->signal_values[NIMCP_SIGNAL_FATIGUE] * 0.5;
        valence_count++;
    }
    if (ctx->signal_valid[NIMCP_SIGNAL_HUNGER]) {
        valence -= ctx->signal_values[NIMCP_SIGNAL_HUNGER] * 0.3;
        valence_count++;
    }
    if (ctx->signal_valid[NIMCP_SIGNAL_DISCOMFORT]) {
        valence -= ctx->signal_values[NIMCP_SIGNAL_DISCOMFORT];
        valence_count++;
    }

    if (valence_count > 0) {
        valence /= valence_count;
    }
    valence = fmax(-1.0, fmin(1.0, valence));

    /* Update emotional state */
    ctx->emotional_state.arousal_value = arousal;
    ctx->emotional_state.valence_value = valence;

    /* Map to discrete states */
    if (arousal < 0.2) {
        ctx->emotional_state.arousal = NIMCP_AROUSAL_VERY_LOW;
    } else if (arousal < 0.4) {
        ctx->emotional_state.arousal = NIMCP_AROUSAL_LOW;
    } else if (arousal < 0.6) {
        ctx->emotional_state.arousal = NIMCP_AROUSAL_MODERATE;
    } else if (arousal < 0.8) {
        ctx->emotional_state.arousal = NIMCP_AROUSAL_HIGH;
    } else {
        ctx->emotional_state.arousal = NIMCP_AROUSAL_VERY_HIGH;
    }

    if (valence < -0.5) {
        ctx->emotional_state.valence = NIMCP_VALENCE_VERY_NEGATIVE;
    } else if (valence < -0.1) {
        ctx->emotional_state.valence = NIMCP_VALENCE_NEGATIVE;
    } else if (valence < 0.1) {
        ctx->emotional_state.valence = NIMCP_VALENCE_NEUTRAL;
    } else if (valence < 0.5) {
        ctx->emotional_state.valence = NIMCP_VALENCE_POSITIVE;
    } else {
        ctx->emotional_state.valence = NIMCP_VALENCE_VERY_POSITIVE;
    }

    /* Compute derived measures */
    ctx->emotional_state.stress_level = arousal * (1.0 - (valence + 1.0) / 2.0);
    ctx->emotional_state.wellbeing = (valence + 1.0) / 2.0 * (1.0 - ctx->emotional_state.stress_level);
    ctx->emotional_state.energy = ctx->signal_valid[NIMCP_SIGNAL_ENERGY_LEVEL] ?
                                 ctx->signal_values[NIMCP_SIGNAL_ENERGY_LEVEL] : 0.5;

    ctx->emotional_state.stress_response_active = (arousal > 0.7 && valence < 0.0);
    ctx->emotional_state.update_time = get_timestamp_ns();

    ctx->stats.avg_arousal = (ctx->stats.avg_arousal * 0.95 + arousal * 0.05);
    if (!isfinite(ctx->stats.avg_arousal)) ctx->stats.avg_arousal = arousal;
    ctx->stats.avg_stress = (ctx->stats.avg_stress * 0.95 + ctx->emotional_state.stress_level * 0.05);
    if (!isfinite(ctx->stats.avg_stress)) ctx->stats.avg_stress = ctx->emotional_state.stress_level;
}

/**
 * @brief Update allostatic load
 */
static void update_allostatic_load(
    nimcp_intero_context_t* ctx,
    double delta_time
) {
    /* Decay chronic stress slowly */
    ctx->chronic_stress_accumulator -= ctx->config.allostatic_decay * delta_time;
    ctx->chronic_stress_accumulator = fmax(0.0, ctx->chronic_stress_accumulator);

    /* Accumulate from current stress */
    ctx->chronic_stress_accumulator += ctx->acute_stress_level * delta_time * 0.01;
    ctx->chronic_stress_accumulator = fmin(1.0, ctx->chronic_stress_accumulator);

    /* Update allostatic load */
    ctx->allostatic_load.acute_stress = ctx->acute_stress_level;
    ctx->allostatic_load.chronic_stress = ctx->chronic_stress_accumulator;
    ctx->allostatic_load.total_load = 0.3 * ctx->acute_stress_level +
                                      0.7 * ctx->chronic_stress_accumulator;
    ctx->allostatic_load.recovery_capacity = 1.0 - ctx->allostatic_load.total_load;

    /* Compute per-system contributions */
    for (uint32_t i = 0; i < NIMCP_SYSTEM_COUNT; i++) {
        nimcp_system_state_t* sys = find_system_by_type(ctx, (nimcp_organ_system_t)i);
        if (sys && sys->is_active) {
            ctx->allostatic_load.system_load[i] = sys->allostatic_load;
        } else {
            ctx->allostatic_load.system_load[i] = 0.0;
        }
    }

    ctx->allostatic_load.assessment_time = get_timestamp_ns();
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

void nimcp_intero_default_config(nimcp_intero_config_t* config) {
    if (!config) {
        return;
    }

    memset(config, 0, sizeof(*config));

    config->prediction_learning_rate = 0.1;
    config->prediction_decay = 0.01;
    config->error_threshold = 0.1;

    config->regulation_strength = 0.5;
    config->allostatic_decay = 0.001;
    config->critical_threshold = 0.8;

    config->enable_emotional_mapping = true;
    config->arousal_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->valence_sensitivity = NIMCP_SENSITIVITY_DEFAULT;

    config->assessment_window = 60.0;  /* 60 seconds */
    config->update_rate_hz = 10.0;
}

nimcp_intero_context_t* nimcp_intero_create(const nimcp_intero_config_t* config) {
    /* Use defaults if config is NULL */
    nimcp_intero_config_t default_config;
    if (!config) {
        nimcp_intero_default_config(&default_config);
        config = &default_config;
    }

    nimcp_intero_context_t* ctx = nimcp_malloc(sizeof(nimcp_intero_context_t));
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate interoceptive context");
        return NULL;
    }

    nimcp_intero_error_t err = nimcp_intero_init(ctx, config);
    if (err != NIMCP_INTERO_OK) {
        nimcp_free(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_intero_create: validation failed");
        return NULL;
    }

    return ctx;
}

nimcp_intero_error_t nimcp_intero_init(
    nimcp_intero_context_t* ctx,
    const nimcp_intero_config_t* config
) {
    if (!ctx || !config) {
        return NIMCP_INTERO_ERROR_NULL_PARAM;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->config = *config;
    ctx->initialized = true;
    ctx->next_system_id = 1;
    ctx->stats.creation_time = get_timestamp_ns();
    ctx->last_update_time = ctx->stats.creation_time;

    /* Initialize prediction models */
    for (int i = 0; i < NIMCP_SIGNAL_COUNT; i++) {
        ctx->predictions[i].signal = (nimcp_intero_signal_type_t)i;
        ctx->predictions[i].predicted_value = 0.5;  /* Default neutral */
        ctx->predictions[i].prediction_variance = 1.0;
        ctx->predictions[i].learning_rate = config->prediction_learning_rate;
    }

    LOG_INFO("Initialized interoceptive prediction context");

    return NIMCP_INTERO_OK;
}

nimcp_intero_error_t nimcp_intero_reset(nimcp_intero_context_t* ctx) {
    if (!ctx) {
        return NIMCP_INTERO_ERROR_NULL_PARAM;
    }

    if (!ctx->initialized) {
        return NIMCP_INTERO_ERROR_NOT_INITIALIZED;
    }

    /* Clear systems */
    memset(ctx->systems, 0, sizeof(ctx->systems));
    memset(ctx->system_active, 0, sizeof(ctx->system_active));
    ctx->num_systems = 0;
    ctx->next_system_id = 1;

    /* Clear setpoints */
    ctx->num_setpoints = 0;

    /* Clear signal values */
    memset(ctx->signal_values, 0, sizeof(ctx->signal_values));
    memset(ctx->signal_valid, 0, sizeof(ctx->signal_valid));

    /* Reset allostatic load */
    memset(&ctx->allostatic_load, 0, sizeof(ctx->allostatic_load));
    ctx->acute_stress_level = 0.0;
    ctx->chronic_stress_accumulator = 0.0;

    /* Reset emotional state */
    memset(&ctx->emotional_state, 0, sizeof(ctx->emotional_state));
    ctx->emotional_state.valence = NIMCP_VALENCE_NEUTRAL;
    ctx->emotional_state.arousal = NIMCP_AROUSAL_MODERATE;

    /* Reset predictions */
    for (int i = 0; i < NIMCP_SIGNAL_COUNT; i++) {
        ctx->predictions[i].predicted_value = 0.5;
        ctx->predictions[i].prediction_variance = 1.0;
        ctx->predictions[i].history_index = 0;
        ctx->predictions[i].history_count = 0;
        ctx->predictions[i].total_error = 0.0;
        ctx->predictions[i].prediction_count = 0;
    }

    /* Reset statistics */
    uint64_t creation_time = ctx->stats.creation_time;
    memset(&ctx->stats, 0, sizeof(ctx->stats));
    ctx->stats.creation_time = creation_time;

    ctx->last_update_time = get_timestamp_ns();

    LOG_INFO("Reset interoceptive context");

    return NIMCP_INTERO_OK;
}

void nimcp_intero_destroy(nimcp_intero_context_t* ctx) {
    if (!ctx) {
        return;
    }

    LOG_INFO("Destroying interoceptive context (signals: %llu, avg error: %.3f)",
             (unsigned long long)ctx->stats.total_signals,
             ctx->stats.avg_prediction_error);

    nimcp_free(ctx);
}

/* ============================================================================
 * System Management
 * ============================================================================ */

nimcp_intero_error_t nimcp_intero_register_system(
    nimcp_intero_context_t* ctx,
    nimcp_organ_system_t system_type,
    uint32_t* system_id
) {
    if (!ctx || !system_id) {
        return NIMCP_INTERO_ERROR_NULL_PARAM;
    }

    if (!ctx->initialized) {
        return NIMCP_INTERO_ERROR_NOT_INITIALIZED;
    }

    /* Check if already exists */
    nimcp_system_state_t* existing = find_system_by_type(ctx, system_type);
    if (existing) {
        *system_id = existing->system_id;
        return NIMCP_INTERO_OK;
    }

    /* Find free slot */
    int free_slot = -1;
    for (uint32_t i = 0; i < NIMCP_INTERO_MAX_SYSTEMS; i++) {
        if (!ctx->system_active[i]) {
            free_slot = (int)i;
            break;
        }
    }

    if (free_slot < 0) {
        return NIMCP_INTERO_ERROR_SYSTEM_LIMIT;
    }

    /* Initialize system */
    nimcp_system_state_t* sys = &ctx->systems[free_slot];
    memset(sys, 0, sizeof(*sys));

    sys->system_id = ctx->next_system_id++;
    sys->type = system_type;
    sys->homeo_state = NIMCP_HOMEO_OPTIMAL;
    sys->is_active = true;
    sys->last_update = get_timestamp_ns();

    ctx->system_active[free_slot] = true;
    ctx->num_systems++;
    ctx->stats.active_systems = ctx->num_systems;

    *system_id = sys->system_id;

    LOG_DEBUG("Registered system %s (ID: %u)",
              nimcp_intero_system_name(system_type), sys->system_id);

    return NIMCP_INTERO_OK;
}

nimcp_intero_error_t nimcp_intero_get_system(
    const nimcp_intero_context_t* ctx,
    uint32_t system_id,
    nimcp_system_state_t* state
) {
    if (!ctx || !state) {
        return NIMCP_INTERO_ERROR_NULL_PARAM;
    }

    for (uint32_t i = 0; i < NIMCP_INTERO_MAX_SYSTEMS; i++) {
        if (ctx->system_active[i] && ctx->systems[i].system_id == system_id) {
            *state = ctx->systems[i];
            return NIMCP_INTERO_OK;
        }
    }

    return NIMCP_INTERO_ERROR_INVALID_SYSTEM;
}

nimcp_intero_error_t nimcp_intero_get_all_systems(
    const nimcp_intero_context_t* ctx,
    nimcp_system_state_t* systems,
    uint32_t max_systems,
    uint32_t* num_systems
) {
    if (!ctx || !systems || !num_systems) {
        return NIMCP_INTERO_ERROR_NULL_PARAM;
    }

    *num_systems = 0;
    for (uint32_t i = 0; i < NIMCP_INTERO_MAX_SYSTEMS && *num_systems < max_systems; i++) {
        if (ctx->system_active[i]) {
            systems[*num_systems] = ctx->systems[i];
            (*num_systems)++;
        }
    }

    return NIMCP_INTERO_OK;
}

nimcp_intero_error_t nimcp_intero_init_standard_systems(nimcp_intero_context_t* ctx) {
    if (!ctx) {
        return NIMCP_INTERO_ERROR_NULL_PARAM;
    }

    if (!ctx->initialized) {
        return NIMCP_INTERO_ERROR_NOT_INITIALIZED;
    }

    /* Register all standard systems */
    static const nimcp_organ_system_t standard_systems[] = {
        NIMCP_SYSTEM_CARDIOVASCULAR,
        NIMCP_SYSTEM_RESPIRATORY,
        NIMCP_SYSTEM_DIGESTIVE,
        NIMCP_SYSTEM_THERMAL,
        NIMCP_SYSTEM_METABOLIC,
        NIMCP_SYSTEM_MUSCULAR,
        NIMCP_SYSTEM_IMMUNE,
        NIMCP_SYSTEM_ENDOCRINE,
        NIMCP_SYSTEM_PAIN
    };

    size_t num_standard = sizeof(standard_systems) / sizeof(standard_systems[0]);

    for (size_t i = 0; i < num_standard; i++) {
        uint32_t system_id;
        nimcp_intero_error_t err = nimcp_intero_register_system(
            ctx, standard_systems[i], &system_id
        );
        if (err != NIMCP_INTERO_OK) {
            LOG_ERROR("Failed to register system %s",
                     nimcp_intero_system_name(standard_systems[i]));
            return err;
        }
    }

    /* Set up default setpoints for key signals */
    static const nimcp_intero_signal_type_t key_signals[] = {
        NIMCP_SIGNAL_HEART_RATE,
        NIMCP_SIGNAL_BLOOD_PRESSURE_SYS,
        NIMCP_SIGNAL_BLOOD_PRESSURE_DIA,
        NIMCP_SIGNAL_BREATHING_RATE,
        NIMCP_SIGNAL_OXYGEN_SAT,
        NIMCP_SIGNAL_CORE_TEMP,
        NIMCP_SIGNAL_GLUCOSE,
        NIMCP_SIGNAL_HUNGER,
        NIMCP_SIGNAL_ENERGY_LEVEL,
        NIMCP_SIGNAL_FATIGUE,
        NIMCP_SIGNAL_PAIN_INTENSITY
    };

    size_t num_signals = sizeof(key_signals) / sizeof(key_signals[0]);

    for (size_t i = 0; i < num_signals; i++) {
        nimcp_setpoint_t sp;
        sp.signal = key_signals[i];
        get_signal_defaults(key_signals[i],
                           &sp.target_value,
                           &sp.tolerance_low, &sp.tolerance_high,
                           &sp.critical_low, &sp.critical_high);
        sp.current_value = sp.target_value;
        sp.regulation_gain = ctx->config.regulation_strength;

        nimcp_intero_set_setpoint(ctx, &sp);
    }

    LOG_INFO("Initialized %zu standard body systems with %zu setpoints",
             num_standard, num_signals);

    return NIMCP_INTERO_OK;
}

/* ============================================================================
 * Signal Processing
 * ============================================================================ */

nimcp_intero_error_t nimcp_intero_process_signal(
    nimcp_intero_context_t* ctx,
    const nimcp_intero_signal_t* signal
) {
    if (!ctx || !signal) {
        return NIMCP_INTERO_ERROR_NULL_PARAM;
    }

    if (!ctx->initialized) {
        return NIMCP_INTERO_ERROR_NOT_INITIALIZED;
    }

    /* Store signal value */
    if (signal->type > 0 && signal->type < NIMCP_SIGNAL_COUNT) {
        ctx->signal_values[signal->type] = signal->value;
        ctx->signal_valid[signal->type] = signal->is_valid;
    }

    /* Update prediction model */
    nimcp_prediction_model_t* model = &ctx->predictions[signal->type];
    double prediction_error = signal->value - model->predicted_value;

    update_prediction_model(model, signal->value, ctx->config.prediction_learning_rate);

    /* Find associated system and update */
    nimcp_system_state_t* sys = find_system_by_type(ctx, signal->system);
    if (sys) {
        /* Find or add signal in system */
        nimcp_intero_signal_t* sys_signal = find_signal_in_system(sys, signal->type);
        if (!sys_signal && sys->num_signals < NIMCP_INTERO_MAX_SIGNALS) {
            sys_signal = &sys->signals[sys->num_signals++];
        }

        if (sys_signal) {
            *sys_signal = *signal;
            sys_signal->predicted_value = model->predicted_value;
            sys_signal->prediction_error = prediction_error;
        }

        sys->activity_level = fmax(sys->activity_level, fabs(prediction_error));
        sys->last_update = get_timestamp_ns();

        /* Update homeostatic state */
        nimcp_setpoint_t* sp = find_setpoint(ctx, signal->type);
        if (sp) {
            sp->current_value = signal->value;
            double deviation = fabs(signal->value - sp->target_value) /
                              (sp->tolerance_high - sp->tolerance_low);
            sys->deviation_magnitude = fmax(sys->deviation_magnitude, deviation);
            sys->homeo_state = compute_homeo_state(sp, signal->value);

            if (sys->homeo_state != NIMCP_HOMEO_OPTIMAL) {
                sys->allostatic_load += deviation * 0.01;
                sys->allostatic_load = fmin(1.0, sys->allostatic_load);
                ctx->stats.homeostatic_violations++;
            }
        }
    }

    /* Update statistics */
    ctx->stats.total_signals++;
    ctx->stats.total_predictions++;

    if (fabs(prediction_error) > ctx->config.error_threshold) {
        ctx->stats.prediction_errors++;
    }

    ctx->stats.avg_prediction_error = (ctx->stats.avg_prediction_error *
                                       (ctx->stats.total_predictions - 1) +
                                       fabs(prediction_error)) / ctx->stats.total_predictions;

    return NIMCP_INTERO_OK;
}

nimcp_intero_error_t nimcp_intero_process_signals(
    nimcp_intero_context_t* ctx,
    const nimcp_intero_signal_t* signals,
    uint32_t num_signals
) {
    if (!ctx || !signals) {
        return NIMCP_INTERO_ERROR_NULL_PARAM;
    }

    for (uint32_t i = 0; i < num_signals; i++) {
        nimcp_intero_error_t err = nimcp_intero_process_signal(ctx, &signals[i]);
        if (err != NIMCP_INTERO_OK) {
            return err;
        }
    }

    return NIMCP_INTERO_OK;
}

nimcp_intero_error_t nimcp_intero_get_signal(
    const nimcp_intero_context_t* ctx,
    nimcp_intero_signal_type_t signal_type,
    double* value
) {
    if (!ctx || !value) {
        return NIMCP_INTERO_ERROR_NULL_PARAM;
    }

    if (signal_type <= 0 || signal_type >= NIMCP_SIGNAL_COUNT) {
        return NIMCP_INTERO_ERROR_INVALID_SIGNAL;
    }

    if (!ctx->signal_valid[signal_type]) {
        return NIMCP_INTERO_ERROR_INVALID_SIGNAL;
    }

    *value = ctx->signal_values[signal_type];
    return NIMCP_INTERO_OK;
}

nimcp_intero_error_t nimcp_intero_get_prediction_error(
    const nimcp_intero_context_t* ctx,
    nimcp_intero_signal_type_t signal_type,
    double* error
) {
    if (!ctx || !error) {
        return NIMCP_INTERO_ERROR_NULL_PARAM;
    }

    if (signal_type <= 0 || signal_type >= NIMCP_SIGNAL_COUNT) {
        return NIMCP_INTERO_ERROR_INVALID_SIGNAL;
    }

    const nimcp_prediction_model_t* model = &ctx->predictions[signal_type];
    if (ctx->signal_valid[signal_type]) {
        *error = ctx->signal_values[signal_type] - model->predicted_value;
    } else {
        *error = 0.0;
    }

    return NIMCP_INTERO_OK;
}

/* ============================================================================
 * Prediction
 * ============================================================================ */

nimcp_intero_error_t nimcp_intero_predict(
    nimcp_intero_context_t* ctx,
    nimcp_intero_signal_type_t signal_type,
    double horizon,
    nimcp_intero_prediction_t* prediction
) {
    if (!ctx || !prediction) {
        return NIMCP_INTERO_ERROR_NULL_PARAM;
    }

    if (!ctx->initialized) {
        return NIMCP_INTERO_ERROR_NOT_INITIALIZED;
    }

    if (signal_type <= 0 || signal_type >= NIMCP_SIGNAL_COUNT) {
        return NIMCP_INTERO_ERROR_INVALID_SIGNAL;
    }

    memset(prediction, 0, sizeof(*prediction));

    nimcp_prediction_model_t* model = &ctx->predictions[signal_type];

    /* Simple prediction using trend if history available */
    double predicted = model->predicted_value;

    if (model->history_count >= 2) {
        /* Calculate trend */
        int idx1 = (model->history_index - 1 + NIMCP_INTERO_HISTORY_SIZE) % NIMCP_INTERO_HISTORY_SIZE;
        int idx2 = (model->history_index - 2 + NIMCP_INTERO_HISTORY_SIZE) % NIMCP_INTERO_HISTORY_SIZE;
        double trend = model->value_history[idx1] - model->value_history[idx2];

        /* Project forward */
        predicted += trend * horizon * ctx->config.update_rate_hz;
    }

    prediction->signal = signal_type;
    prediction->predicted_value = predicted;
    prediction->prediction_horizon = horizon;
    prediction->confidence = 1.0 / (1.0 + sqrt(model->prediction_variance));
    prediction->actual_available = false;

    ctx->stats.total_predictions++;

    return NIMCP_INTERO_OK;
}

nimcp_intero_error_t nimcp_intero_update_prediction(
    nimcp_intero_context_t* ctx,
    nimcp_intero_signal_type_t signal_type,
    double actual_value
) {
    if (!ctx) {
        return NIMCP_INTERO_ERROR_NULL_PARAM;
    }

    if (!ctx->initialized) {
        return NIMCP_INTERO_ERROR_NOT_INITIALIZED;
    }

    if (signal_type <= 0 || signal_type >= NIMCP_SIGNAL_COUNT) {
        return NIMCP_INTERO_ERROR_INVALID_SIGNAL;
    }

    nimcp_prediction_model_t* model = &ctx->predictions[signal_type];
    update_prediction_model(model, actual_value, ctx->config.prediction_learning_rate);

    ctx->signal_values[signal_type] = actual_value;
    ctx->signal_valid[signal_type] = true;

    return NIMCP_INTERO_OK;
}

nimcp_intero_error_t nimcp_intero_get_total_prediction_error(
    const nimcp_intero_context_t* ctx,
    double* total_error
) {
    if (!ctx || !total_error) {
        return NIMCP_INTERO_ERROR_NULL_PARAM;
    }

    double weighted_error = 0.0;
    double total_precision = 0.0;

    for (int i = 1; i < NIMCP_SIGNAL_COUNT; i++) {
        if (ctx->signal_valid[i]) {
            double precision = 1.0 / (1.0 + ctx->predictions[i].prediction_variance);
            double error = fabs(ctx->signal_values[i] - ctx->predictions[i].predicted_value);
            weighted_error += precision * error;
            total_precision += precision;
        }
    }

    if (total_precision > 0.0) {
        *total_error = weighted_error / total_precision;
    } else {
        *total_error = 0.0;
    }

    return NIMCP_INTERO_OK;
}

/* ============================================================================
 * Homeostatic Integration
 * ============================================================================ */

nimcp_intero_error_t nimcp_intero_set_setpoint(
    nimcp_intero_context_t* ctx,
    const nimcp_setpoint_t* setpoint
) {
    if (!ctx || !setpoint) {
        return NIMCP_INTERO_ERROR_NULL_PARAM;
    }

    if (!ctx->initialized) {
        return NIMCP_INTERO_ERROR_NOT_INITIALIZED;
    }

    /* Find existing or add new */
    nimcp_setpoint_t* existing = find_setpoint(ctx, setpoint->signal);
    if (existing) {
        *existing = *setpoint;
        return NIMCP_INTERO_OK;
    }

    if (ctx->num_setpoints >= NIMCP_INTERO_MAX_SETPOINTS) {
        return NIMCP_INTERO_ERROR_SIGNAL_LIMIT;
    }

    ctx->setpoints[ctx->num_setpoints++] = *setpoint;

    return NIMCP_INTERO_OK;
}

nimcp_intero_error_t nimcp_intero_get_setpoint(
    const nimcp_intero_context_t* ctx,
    nimcp_intero_signal_type_t signal_type,
    nimcp_setpoint_t* setpoint
) {
    if (!ctx || !setpoint) {
        return NIMCP_INTERO_ERROR_NULL_PARAM;
    }

    for (uint32_t i = 0; i < ctx->num_setpoints; i++) {
        if (ctx->setpoints[i].signal == signal_type) {
            *setpoint = ctx->setpoints[i];
            return NIMCP_INTERO_OK;
        }
    }

    return NIMCP_INTERO_ERROR_INVALID_SIGNAL;
}

nimcp_intero_error_t nimcp_intero_get_homeostatic_state(
    const nimcp_intero_context_t* ctx,
    nimcp_intero_signal_type_t signal_type,
    nimcp_homeostatic_state_t* state,
    double* deviation
) {
    if (!ctx || !state || !deviation) {
        return NIMCP_INTERO_ERROR_NULL_PARAM;
    }

    nimcp_setpoint_t sp;
    nimcp_intero_error_t err = nimcp_intero_get_setpoint(ctx, signal_type, &sp);
    if (err != NIMCP_INTERO_OK) {
        return err;
    }

    *state = compute_homeo_state(&sp, sp.current_value);
    *deviation = fabs(sp.current_value - sp.target_value) /
                (sp.tolerance_high - sp.tolerance_low);

    return NIMCP_INTERO_OK;
}

nimcp_intero_error_t nimcp_intero_compute_drive(
    nimcp_intero_context_t* ctx,
    nimcp_intero_signal_type_t signal_type,
    double* drive
) {
    if (!ctx || !drive) {
        return NIMCP_INTERO_ERROR_NULL_PARAM;
    }

    nimcp_setpoint_t sp;
    nimcp_intero_error_t err = nimcp_intero_get_setpoint(ctx, signal_type, &sp);
    if (err != NIMCP_INTERO_OK) {
        *drive = 0.0;
        return NIMCP_INTERO_OK;  /* No setpoint = no drive */
    }

    /* Compute drive as deviation from setpoint */
    double deviation = sp.current_value - sp.target_value;
    double range = sp.tolerance_high - sp.tolerance_low;

    if (range > 0.0) {
        *drive = -deviation / range * sp.regulation_gain;
    } else {
        *drive = 0.0;
    }

    /* Clamp to [-1, 1] */
    *drive = fmax(-1.0, fmin(1.0, *drive));

    return NIMCP_INTERO_OK;
}

/* ============================================================================
 * Allostatic Load
 * ============================================================================ */

nimcp_intero_error_t nimcp_intero_get_allostatic_load(
    const nimcp_intero_context_t* ctx,
    nimcp_allostatic_load_t* load
) {
    if (!ctx || !load) {
        return NIMCP_INTERO_ERROR_NULL_PARAM;
    }

    *load = ctx->allostatic_load;
    return NIMCP_INTERO_OK;
}

nimcp_intero_error_t nimcp_intero_apply_stress(
    nimcp_intero_context_t* ctx,
    double intensity,
    double duration
) {
    if (!ctx) {
        return NIMCP_INTERO_ERROR_NULL_PARAM;
    }

    if (!ctx->initialized) {
        return NIMCP_INTERO_ERROR_NOT_INITIALIZED;
    }

    intensity = fmax(0.0, fmin(1.0, intensity));

    ctx->acute_stress_level = intensity;
    ctx->stress_start_time = get_timestamp_ns();

    /* Immediate effects on signals */
    if (ctx->signal_valid[NIMCP_SIGNAL_HEART_RATE]) {
        ctx->signal_values[NIMCP_SIGNAL_HEART_RATE] += 20.0 * intensity;
    }
    if (ctx->signal_valid[NIMCP_SIGNAL_CORTISOL]) {
        ctx->signal_values[NIMCP_SIGNAL_CORTISOL] += 0.3 * intensity;
        ctx->signal_values[NIMCP_SIGNAL_CORTISOL] = fmin(1.0, ctx->signal_values[NIMCP_SIGNAL_CORTISOL]);
    }
    if (ctx->signal_valid[NIMCP_SIGNAL_ADRENALINE]) {
        ctx->signal_values[NIMCP_SIGNAL_ADRENALINE] += 0.4 * intensity;
        ctx->signal_values[NIMCP_SIGNAL_ADRENALINE] = fmin(1.0, ctx->signal_values[NIMCP_SIGNAL_ADRENALINE]);
    }
    if (ctx->signal_valid[NIMCP_SIGNAL_BREATHING_RATE]) {
        ctx->signal_values[NIMCP_SIGNAL_BREATHING_RATE] += 4.0 * intensity;
    }

    /* Accumulate chronic stress */
    ctx->chronic_stress_accumulator += intensity * duration * 0.001;
    ctx->chronic_stress_accumulator = fmin(1.0, ctx->chronic_stress_accumulator);

    LOG_DEBUG("Applied stress: intensity=%.2f, duration=%.1fs", intensity, duration);

    return NIMCP_INTERO_OK;
}

nimcp_intero_error_t nimcp_intero_apply_recovery(
    nimcp_intero_context_t* ctx,
    double duration
) {
    if (!ctx) {
        return NIMCP_INTERO_ERROR_NULL_PARAM;
    }

    if (!ctx->initialized) {
        return NIMCP_INTERO_ERROR_NOT_INITIALIZED;
    }

    /* Decay stress */
    double decay = duration * 0.1;
    ctx->acute_stress_level = fmax(0.0, ctx->acute_stress_level - decay);
    ctx->chronic_stress_accumulator = fmax(0.0, ctx->chronic_stress_accumulator - decay * 0.5);

    /* Recovery effects */
    if (ctx->signal_valid[NIMCP_SIGNAL_CORTISOL]) {
        ctx->signal_values[NIMCP_SIGNAL_CORTISOL] -= 0.1 * duration;
        ctx->signal_values[NIMCP_SIGNAL_CORTISOL] = fmax(0.0, ctx->signal_values[NIMCP_SIGNAL_CORTISOL]);
    }
    if (ctx->signal_valid[NIMCP_SIGNAL_ENERGY_LEVEL]) {
        ctx->signal_values[NIMCP_SIGNAL_ENERGY_LEVEL] += 0.05 * duration;
        ctx->signal_values[NIMCP_SIGNAL_ENERGY_LEVEL] = fmin(1.0, ctx->signal_values[NIMCP_SIGNAL_ENERGY_LEVEL]);
    }

    LOG_DEBUG("Applied recovery: duration=%.1fs", duration);

    return NIMCP_INTERO_OK;
}

/* ============================================================================
 * Emotional Mapping
 * ============================================================================ */

nimcp_intero_error_t nimcp_intero_get_emotional_state(
    const nimcp_intero_context_t* ctx,
    nimcp_emotional_state_t* state
) {
    if (!ctx || !state) {
        return NIMCP_INTERO_ERROR_NULL_PARAM;
    }

    *state = ctx->emotional_state;
    return NIMCP_INTERO_OK;
}

nimcp_intero_error_t nimcp_intero_compute_arousal(
    const nimcp_intero_context_t* ctx,
    double* arousal,
    nimcp_arousal_state_t* state
) {
    if (!ctx || !arousal || !state) {
        return NIMCP_INTERO_ERROR_NULL_PARAM;
    }

    *arousal = ctx->emotional_state.arousal_value;
    *state = ctx->emotional_state.arousal;

    return NIMCP_INTERO_OK;
}

nimcp_intero_error_t nimcp_intero_compute_valence(
    const nimcp_intero_context_t* ctx,
    double* valence,
    nimcp_valence_t* state
) {
    if (!ctx || !valence || !state) {
        return NIMCP_INTERO_ERROR_NULL_PARAM;
    }

    *valence = ctx->emotional_state.valence_value;
    *state = ctx->emotional_state.valence;

    return NIMCP_INTERO_OK;
}

/* ============================================================================
 * Assessment
 * ============================================================================ */

nimcp_intero_error_t nimcp_intero_assess_awareness(
    const nimcp_intero_context_t* ctx,
    nimcp_intero_awareness_t* awareness
) {
    if (!ctx || !awareness) {
        return NIMCP_INTERO_ERROR_NULL_PARAM;
    }

    memset(awareness, 0, sizeof(*awareness));

    /* Calculate accuracy based on prediction errors */
    double total_accuracy = 0.0;
    int count = 0;

    for (int i = 1; i < NIMCP_SIGNAL_COUNT; i++) {
        if (ctx->predictions[i].prediction_count > 0) {
            double avg_error = ctx->predictions[i].total_error / ctx->predictions[i].prediction_count;
            double accuracy = 1.0 / (1.0 + avg_error);
            total_accuracy += accuracy;
            count++;
        }
    }

    if (count > 0) {
        awareness->accuracy = total_accuracy / count;
    }

    /* Calculate sensitivity */
    awareness->sensitivity = 1.0 / (1.0 + ctx->config.error_threshold);

    /* Stability based on variance */
    double total_variance = 0.0;
    int var_count = 0;
    for (int i = 1; i < NIMCP_SIGNAL_COUNT; i++) {
        if (ctx->predictions[i].history_count > 0) {
            total_variance += ctx->predictions[i].prediction_variance;
            var_count++;
        }
    }
    if (var_count > 0) {
        awareness->stability = 1.0 / (1.0 + sqrt(total_variance / var_count));
    }

    /* Per-system accuracy */
    for (uint32_t i = 0; i < NIMCP_INTERO_MAX_SYSTEMS; i++) {
        if (ctx->system_active[i]) {
            nimcp_organ_system_t type = ctx->systems[i].type;
            if (type < NIMCP_SYSTEM_COUNT) {
                awareness->system_accuracy[type] = 1.0 -
                    ctx->systems[i].deviation_magnitude;
            }
        }
    }

    awareness->assessment_time = get_timestamp_ns();

    /* Note: Stats update moved to nimcp_intero_update() to avoid const violation.
     * This function is intentionally read-only per API contract. */

    return NIMCP_INTERO_OK;
}

/* ============================================================================
 * Update and Processing
 * ============================================================================ */

nimcp_intero_error_t nimcp_intero_update(
    nimcp_intero_context_t* ctx,
    double delta_time
) {
    if (!ctx) {
        return NIMCP_INTERO_ERROR_NULL_PARAM;
    }

    if (!ctx->initialized) {
        return NIMCP_INTERO_ERROR_NOT_INITIALIZED;
    }

    /* Decay prediction variance */
    for (int i = 0; i < NIMCP_SIGNAL_COUNT; i++) {
        ctx->predictions[i].prediction_variance *= (1.0 - ctx->config.prediction_decay * delta_time);
        ctx->predictions[i].prediction_variance = fmax(0.01, ctx->predictions[i].prediction_variance);
    }

    /* Decay acute stress */
    ctx->acute_stress_level *= exp(-delta_time * 0.1);

    /* Update system states */
    for (uint32_t i = 0; i < NIMCP_INTERO_MAX_SYSTEMS; i++) {
        if (ctx->system_active[i]) {
            /* Decay activity level */
            ctx->systems[i].activity_level *= 0.95;

            /* Decay allostatic load slowly */
            ctx->systems[i].allostatic_load -= ctx->config.allostatic_decay * delta_time;
            ctx->systems[i].allostatic_load = fmax(0.0, ctx->systems[i].allostatic_load);

            /* Decay deviation */
            ctx->systems[i].deviation_magnitude *= 0.99;
        }
    }

    /* Update allostatic load */
    update_allostatic_load(ctx, delta_time);

    /* Update emotional state */
    update_emotional_state(ctx);

    ctx->last_update_time = get_timestamp_ns();

    return NIMCP_INTERO_OK;
}

/* ============================================================================
 * Statistics and Utility
 * ============================================================================ */

nimcp_intero_error_t nimcp_intero_get_stats(
    const nimcp_intero_context_t* ctx,
    nimcp_intero_stats_t* stats
) {
    if (!ctx || !stats) {
        return NIMCP_INTERO_ERROR_NULL_PARAM;
    }

    *stats = ctx->stats;
    stats->active_systems = ctx->num_systems;

    return NIMCP_INTERO_OK;
}

const char* nimcp_intero_system_name(nimcp_organ_system_t system) {
    static const char* names[] = {
        "Unknown", "Cardiovascular", "Respiratory", "Digestive",
        "Thermal", "Metabolic", "Muscular", "Immune",
        "Endocrine", "Urinary", "Vestibular", "Pain"
    };

    if (system >= 0 && system < NIMCP_SYSTEM_COUNT) {
        return names[system];
    }
    return "Unknown";
}

const char* nimcp_intero_signal_name(nimcp_intero_signal_type_t signal) {
    static const char* names[] = {
        "Unknown",
        "Heart Rate", "Systolic BP", "Diastolic BP", "HRV",
        "Breathing Rate", "O2 Saturation", "CO2 Level", "Breathing Depth",
        "Hunger", "Satiety", "Thirst", "Gut Activity", "Nausea",
        "Core Temp", "Skin Temp", "Sweating", "Shivering",
        "Energy Level", "Glucose", "ATP Level",
        "Muscle Tension", "Fatigue", "Soreness",
        "Inflammation", "Sickness",
        "Cortisol", "Adrenaline", "Dopamine", "Serotonin",
        "Pain Intensity", "Pain Location", "Discomfort"
    };

    if (signal >= 0 && signal < NIMCP_SIGNAL_COUNT) {
        return names[signal];
    }
    return "Unknown";
}

const char* nimcp_intero_homeo_state_name(nimcp_homeostatic_state_t state) {
    static const char* names[] = {
        "Optimal", "Mild Deviation", "Moderate Deviation",
        "Severe Deviation", "Critical"
    };

    if (state >= 0 && state < NIMCP_HOMEO_COUNT) {
        return names[state];
    }
    return "Unknown";
}

const char* nimcp_intero_arousal_name(nimcp_arousal_state_t state) {
    static const char* names[] = {
        "Very Low", "Low", "Moderate", "High", "Very High"
    };

    if (state >= 0 && state < NIMCP_AROUSAL_COUNT) {
        return names[state];
    }
    return "Unknown";
}

const char* nimcp_intero_valence_name(nimcp_valence_t valence) {
    static const char* names[] = {
        "Very Negative", "Negative", "Neutral", "Positive", "Very Positive"
    };

    if (valence >= 0 && valence < NIMCP_VALENCE_COUNT) {
        return names[valence];
    }
    return "Unknown";
}
