/**
 * @file nimcp_circadian.c
 * @brief Circadian Rhythm Modulation System Implementation
 * @version 1.0.0
 * @date 2025-12-17
 *
 * WHAT: Implementation of circadian rhythm control system
 * WHY:  Models SCN master clock for realistic brain simulation
 * HOW:  Sinusoidal modulation, phase tracking, zeitgeber entrainment
 */

#include "core/medulla/nimcp_circadian.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_math_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(circadian)

/* ============================================================================
 * Constants - Phase Timing
 * ============================================================================ */

/** @brief Phase duration (3 hours in microseconds) */
#define PHASE_DURATION_US (3ULL * 60ULL * 60ULL * 1000000ULL)

/** @brief Free-running period in hours (natural human circadian period) */
#define FREE_RUNNING_PERIOD_HOURS 24.2f

/** @brief PI constant for sinusoidal calculations */

/* ============================================================================
 * Modulation Curve Definitions
 * ============================================================================ */

/**
 * @brief Modulation factors for each phase
 *
 * WHAT: Pre-computed modulation values for 8 circadian phases
 * WHY:  Fast lookup, biologically-grounded profiles
 * HOW:  Values derived from neuroscience literature on circadian effects
 *
 * Format: [PHASE][MODULATION_TYPE]
 * Phases: NIGHT_DEEP, NIGHT_LATE, DAWN, MORNING, MIDDAY, AFTERNOON, EVENING, DUSK
 * Types: AROUSAL, LEARNING_RATE, CONSOLIDATION, METABOLISM
 */
static const float MODULATION_CURVES[CIRCADIAN_NUM_PHASES][CIRCADIAN_MODULATION_COUNT] = {
    /* NIGHT_DEEP (00:00-03:00): Deep sleep, high consolidation */
    {0.2f, 0.3f, 0.9f, 0.6f},

    /* NIGHT_LATE (03:00-06:00): REM sleep, temperature minimum */
    {0.15f, 0.25f, 0.85f, 0.55f},

    /* DAWN (06:00-09:00): Cortisol rise, awakening */
    {0.5f, 0.7f, 0.5f, 0.8f},

    /* MORNING (09:00-12:00): Peak alertness and learning */
    {1.0f, 0.9f, 0.3f, 1.0f},

    /* MIDDAY (12:00-15:00): Post-lunch dip */
    {0.7f, 0.7f, 0.3f, 0.9f},

    /* AFTERNOON (15:00-18:00): Recovery, motor performance peak */
    {0.8f, 0.75f, 0.4f, 0.95f},

    /* EVENING (18:00-21:00): Wind down, melatonin rise */
    {0.5f, 0.6f, 0.7f, 0.75f},

    /* DUSK (21:00-24:00): Sleep preparation */
    {0.3f, 0.4f, 0.8f, 0.65f}
};

/* ============================================================================
 * Main Structure Definition
 * ============================================================================ */

/**
 * @brief Circadian rhythm system state
 */
struct circadian_rhythm {
    /* Configuration */
    circadian_config_t config;

    /* Current state */
    circadian_phase_t current_phase;
    uint64_t phase_start_time_us;
    uint64_t last_update_time_us;

    /* Modulation factors (current values) */
    float modulation_factors[CIRCADIAN_MODULATION_COUNT];

    /* Sleep pressure (Process S) */
    float sleep_pressure;          /* 0.0-1.0 */
    bool is_sleeping;              /* Current sleep state */

    /* Entrainment */
    float cumulative_phase_shift;  /* Accumulated phase shift in hours */

    /* Bio-async integration */
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;

    /* Thread safety */
    nimcp_platform_mutex_t* mutex;
};

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static void compute_modulation_factors(circadian_rhythm_t* rhythm);
static void update_sleep_pressure(circadian_rhythm_t* rhythm, float elapsed_hours);
static float compute_phase_shift(circadian_rhythm_t* rhythm,
                                  circadian_zeitgeber_t zeitgeber,
                                  float strength);

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

circadian_rhythm_t* circadian_create(const circadian_config_t* config)
{
    /* WHAT: Create and initialize circadian rhythm system
     * WHY:  Set up clock, modulation curves, sleep pressure
     * HOW:  Allocate, populate config, initialize state, create mutex
     */

    circadian_rhythm_t* rhythm = nimcp_calloc(1, sizeof(circadian_rhythm_t));
    if (!rhythm) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "circadian_create: failed to allocate circadian rhythm system");
        NIMCP_LOGGING_ERROR("Failed to allocate circadian rhythm system");
        return NULL;
    }

    /* Use provided config or defaults */
    if (config) {
        rhythm->config = *config;
    } else {
        circadian_default_config(&rhythm->config);
    }

    /* Initialize state */
    rhythm->current_phase = CIRCADIAN_PHASE_NIGHT_DEEP;
    rhythm->phase_start_time_us = nimcp_time_monotonic_us();
    rhythm->last_update_time_us = rhythm->phase_start_time_us;
    rhythm->sleep_pressure = 0.0f;
    rhythm->is_sleeping = false;
    rhythm->cumulative_phase_shift = 0.0f;
    rhythm->bio_async_enabled = false;

    /* Compute initial modulation factors */
    compute_modulation_factors(rhythm);

    /* Create mutex */
    rhythm->mutex = nimcp_platform_mutex_create();
    if (!rhythm->mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "circadian_create: failed to create mutex");
        NIMCP_LOGGING_ERROR("Failed to create circadian mutex");
        nimcp_free(rhythm);
        return NULL;
    }

    NIMCP_LOGGING_INFO("Circadian rhythm system created (cycle: %.1fh, scale: %.1fx)",
                       rhythm->config.cycle_duration_us / 3600000000.0,
                       rhythm->config.time_scale);

    return rhythm;
}

void circadian_destroy(circadian_rhythm_t* rhythm)
{
    /* WHAT: Clean up circadian rhythm system
     * WHY:  Free resources, prevent leaks
     * HOW:  Disconnect bio-async, destroy mutex, free structure
     */

    if (!rhythm) {
        return;
    }

    /* Disconnect from bio-async if connected */
    if (rhythm->bio_async_enabled) {
        circadian_disconnect_bio_async(rhythm);
    }

    /* Destroy mutex */
    if (rhythm->mutex) {
        nimcp_platform_mutex_destroy(rhythm->mutex);
        nimcp_free(rhythm->mutex);
        rhythm->mutex = NULL;
    }

    nimcp_free(rhythm);
    NIMCP_LOGGING_INFO("Circadian rhythm system destroyed");
}

int circadian_default_config(circadian_config_t* config)
{
    /* WHAT: Populate configuration with defaults
     * WHY:  Provide sensible starting values
     * HOW:  Set 24h cycle, real-time, entrained mode
     */

    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "circadian_default_config: config is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    config->cycle_duration_us = CIRCADIAN_DEFAULT_CYCLE_DURATION_US;
    config->time_scale = 1.0f;
    config->mode = CIRCADIAN_MODE_ENTRAINED;
    config->free_running_period_hours = FREE_RUNNING_PERIOD_HOURS;
    config->entrainment_strength_light = 0.8f;
    config->entrainment_strength_activity = 0.3f;
    config->entrainment_strength_social = 0.1f;
    config->enable_sleep_pressure = true;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Time and Phase Functions
 * ============================================================================ */

int circadian_update(circadian_rhythm_t* rhythm)
{
    /* WHAT: Update circadian clock and modulation factors
     * WHY:  Advance time, transition phases, update sleep pressure
     * HOW:  Compute elapsed time, update phase, recompute factors
     */

    if (!rhythm) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "circadian_update: rhythm is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(rhythm->mutex);

    /* Get current time */
    uint64_t current_time_us = nimcp_time_monotonic_us();
    uint64_t elapsed_us = current_time_us - rhythm->last_update_time_us;

    /* Apply time scale */
    elapsed_us = (uint64_t)((float)elapsed_us * rhythm->config.time_scale);

    /* Update sleep pressure if enabled */
    if (rhythm->config.enable_sleep_pressure) {
        float elapsed_hours = (float)elapsed_us / 3600000000.0f;
        update_sleep_pressure(rhythm, elapsed_hours);
    }

    /* Compute time within current phase */
    uint64_t time_in_phase_us = current_time_us - rhythm->phase_start_time_us;
    time_in_phase_us = (uint64_t)((float)time_in_phase_us * rhythm->config.time_scale);

    /* Check if phase transition needed */
    if (time_in_phase_us >= PHASE_DURATION_US) {
        /* Advance to next phase */
        rhythm->current_phase = (circadian_phase_t)((rhythm->current_phase + 1) % CIRCADIAN_NUM_PHASES);
        rhythm->phase_start_time_us = current_time_us;

        /* Recompute modulation factors for new phase */
        compute_modulation_factors(rhythm);

        NIMCP_LOGGING_DEBUG("Circadian phase transition: %s",
                           circadian_phase_name(rhythm->current_phase));
    }

    rhythm->last_update_time_us = current_time_us;

    nimcp_platform_mutex_unlock(rhythm->mutex);
    return NIMCP_SUCCESS;
}

circadian_phase_t circadian_get_phase(const circadian_rhythm_t* rhythm)
{
    /* WHAT: Get current circadian phase
     * WHY:  Query current position in cycle
     * HOW:  Return current phase enum
     */

    if (!rhythm) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "circadian_get_phase: rhythm is NULL");
        return CIRCADIAN_PHASE_NIGHT_DEEP;
    }

    return rhythm->current_phase;
}

float circadian_get_cycle_position(const circadian_rhythm_t* rhythm)
{
    /* WHAT: Get normalized position in cycle (0.0-1.0)
     * WHY:  Provides smooth position for continuous calculations
     * HOW:  Compute phase fraction and combine with phase index
     */

    if (!rhythm) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "circadian_get_cycle_position: rhythm is NULL");
        return 0.0f;
    }

    /* Compute time within current phase */
    uint64_t current_time_us = nimcp_time_monotonic_us();
    uint64_t time_in_phase_us = current_time_us - rhythm->phase_start_time_us;
    time_in_phase_us = (uint64_t)((float)time_in_phase_us * rhythm->config.time_scale);

    /* Phase fraction (0.0-1.0 within current phase) */
    float phase_fraction = (float)time_in_phase_us / (float)PHASE_DURATION_US;
    if (phase_fraction > 1.0f) phase_fraction = 1.0f;

    /* Total position = (phase_index + phase_fraction) / num_phases */
    float position = ((float)rhythm->current_phase + phase_fraction) / (float)CIRCADIAN_NUM_PHASES;

    return position;
}

int circadian_reset_phase(circadian_rhythm_t* rhythm, circadian_phase_t phase)
{
    /* WHAT: Reset clock to specific phase
     * WHY:  Simulate jet lag, shift work
     * HOW:  Update current phase, reset timers
     */

    if (!rhythm) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "circadian_reset_phase: rhythm is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (phase >= CIRCADIAN_PHASE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "circadian_reset_phase: phase out of range");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(rhythm->mutex);

    rhythm->current_phase = phase;
    rhythm->phase_start_time_us = nimcp_time_monotonic_us();
    rhythm->last_update_time_us = rhythm->phase_start_time_us;

    /* Recompute modulation factors */
    compute_modulation_factors(rhythm);

    nimcp_platform_mutex_unlock(rhythm->mutex);

    NIMCP_LOGGING_INFO("Circadian phase reset to %s", circadian_phase_name(phase));
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Modulation Functions
 * ============================================================================ */

float circadian_get_modulation(const circadian_rhythm_t* rhythm,
                                circadian_modulation_type_t type)
{
    /* WHAT: Get current modulation factor for parameter
     * WHY:  Apply circadian effects to brain systems
     * HOW:  Return pre-computed factor for current phase
     */

    if (!rhythm) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "circadian_get_modulation: rhythm is NULL");
        return 0.5f;  /* Neutral value on error */
    }

    if (type >= CIRCADIAN_MODULATION_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "circadian_get_modulation: type out of range");
        return 0.5f;  /* Neutral value on error */
    }

    return rhythm->modulation_factors[type];
}

/* ============================================================================
 * Entrainment Functions
 * ============================================================================ */

int circadian_apply_zeitgeber(circadian_rhythm_t* rhythm,
                               circadian_zeitgeber_t zeitgeber,
                               float strength)
{
    /* WHAT: Apply time cue to entrain circadian rhythm
     * WHY:  Simulate light, activity, social cues
     * HOW:  Compute phase shift based on type, strength, current phase
     */

    if (!rhythm) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "circadian_apply_zeitgeber: rhythm is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (zeitgeber >= CIRCADIAN_ZEITGEBER_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "circadian_apply_zeitgeber: zeitgeber out of range");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Clamp strength */
    if (strength < 0.0f) strength = 0.0f;
    if (strength > 1.0f) strength = 1.0f;

    /* Only entrain if in entrained mode */
    if (rhythm->config.mode != CIRCADIAN_MODE_ENTRAINED) {
        return NIMCP_SUCCESS;
    }

    nimcp_platform_mutex_lock(rhythm->mutex);

    /* Compute phase shift */
    float phase_shift_hours = compute_phase_shift(rhythm, zeitgeber, strength);
    rhythm->cumulative_phase_shift += phase_shift_hours;

    /* Apply phase shift if significant (>= 15 minutes) */
    if (fabsf(rhythm->cumulative_phase_shift) >= 0.25f) {
        /* Convert hours to microseconds */
        int64_t shift_us = (int64_t)(rhythm->cumulative_phase_shift * 3600000000.0f);

        /* Adjust phase start time (positive shift = earlier wake) */
        rhythm->phase_start_time_us -= shift_us;

        NIMCP_LOGGING_DEBUG("Circadian entrainment: %.2fh shift", rhythm->cumulative_phase_shift);

        rhythm->cumulative_phase_shift = 0.0f;
    }

    nimcp_platform_mutex_unlock(rhythm->mutex);
    return NIMCP_SUCCESS;
}

int circadian_set_time_scale(circadian_rhythm_t* rhythm, float scale)
{
    /* WHAT: Set simulation time scale
     * WHY:  Speed up or slow down for testing
     * HOW:  Update config time scale
     */

    if (!rhythm) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "circadian_set_time_scale: rhythm is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (scale <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "circadian_set_time_scale: scale must be positive");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(rhythm->mutex);
    rhythm->config.time_scale = scale;
    nimcp_platform_mutex_unlock(rhythm->mutex);

    NIMCP_LOGGING_INFO("Circadian time scale set to %.1fx", scale);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Sleep Pressure Functions
 * ============================================================================ */

float circadian_get_sleep_pressure(const circadian_rhythm_t* rhythm)
{
    /* WHAT: Get homeostatic sleep drive (Process S)
     * WHY:  Combine with circadian for sleep propensity
     * HOW:  Return accumulated sleep pressure
     */

    if (!rhythm) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "circadian_get_sleep_pressure: rhythm is NULL");
        return 0.0f;
    }

    return rhythm->sleep_pressure;
}

/* ============================================================================
 * Bio-async Integration
 * ============================================================================ */

int circadian_connect_bio_async(circadian_rhythm_t* rhythm)
{
    /* WHAT: Connect to bio-async router
     * WHY:  Enable inter-module messaging
     * HOW:  Register module, allocate inbox
     */

    if (!rhythm) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "circadian_connect_bio_async: rhythm is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (rhythm->bio_async_enabled) {
        return NIMCP_SUCCESS;  /* Already connected */
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_CIRCADIAN,
        .module_name = "circadian_rhythm",
        .inbox_capacity = 32,
        .user_data = rhythm
    };

    rhythm->bio_ctx = bio_router_register_module(&info);
    if (rhythm->bio_ctx) {
        rhythm->bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Circadian system connected to bio-async router");
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
    }

    return NIMCP_SUCCESS;
}

int circadian_disconnect_bio_async(circadian_rhythm_t* rhythm)
{
    /* WHAT: Disconnect from bio-async router
     * WHY:  Clean shutdown
     * HOW:  Unregister module
     */

    if (!rhythm) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "circadian_disconnect_bio_async: rhythm is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (!rhythm->bio_async_enabled) {
        return NIMCP_SUCCESS;
    }

    if (rhythm->bio_ctx) {
        bio_router_unregister_module(rhythm->bio_ctx);
        rhythm->bio_ctx = NULL;
    }

    rhythm->bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Circadian system disconnected from bio-async router");

    return NIMCP_SUCCESS;
}

bool circadian_is_bio_async_connected(const circadian_rhythm_t* rhythm)
{
    /* WHAT: Check bio-async connection status
     * WHY:  Verify messaging capability
     * HOW:  Check flag and context validity
     */

    if (!rhythm) {
        return false;
    }

    return rhythm->bio_async_enabled && rhythm->bio_ctx != NULL;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* circadian_phase_name(circadian_phase_t phase)
{
    /* WHAT: Get human-readable phase name
     * WHY:  Debugging, logging
     * HOW:  Static string array lookup
     */

    static const char* names[] = {
        "NIGHT_DEEP",
        "NIGHT_LATE",
        "DAWN",
        "MORNING",
        "MIDDAY",
        "AFTERNOON",
        "EVENING",
        "DUSK"
    };

    if (phase >= CIRCADIAN_PHASE_COUNT) {
        return "UNKNOWN";
    }

    return names[phase];
}

const char* circadian_modulation_name(circadian_modulation_type_t type)
{
    /* WHAT: Get human-readable modulation type name
     * WHY:  Debugging, logging
     * HOW:  Static string array lookup
     */

    static const char* names[] = {
        "AROUSAL",
        "LEARNING_RATE",
        "CONSOLIDATION",
        "METABOLISM"
    };

    if (type >= CIRCADIAN_MODULATION_COUNT) {
        return "UNKNOWN";
    }

    return names[type];
}

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

static void compute_modulation_factors(circadian_rhythm_t* rhythm)
{
    /* WHAT: Compute current modulation factors from curves
     * WHY:  Update factors for current phase
     * HOW:  Copy from pre-computed curve table
     */

    if (!rhythm) {
        return;
    }

    /* Copy modulation values for current phase */
    for (int i = 0; i < CIRCADIAN_MODULATION_COUNT; i++) {
        rhythm->modulation_factors[i] = MODULATION_CURVES[rhythm->current_phase][i];
    }
}

static void update_sleep_pressure(circadian_rhythm_t* rhythm, float elapsed_hours)
{
    /* WHAT: Update homeostatic sleep pressure
     * WHY:  Model adenosine accumulation/clearance
     * HOW:  Accumulate during wake, dissipate during sleep
     */

    if (!rhythm || elapsed_hours < 0.0f) {
        return;
    }

    /* Determine if sleeping based on arousal modulation */
    float arousal = rhythm->modulation_factors[CIRCADIAN_MODULATION_AROUSAL];
    rhythm->is_sleeping = (arousal < 0.4f);  /* Low arousal = sleep */

    if (rhythm->is_sleeping) {
        /* Dissipate sleep pressure during sleep */
        rhythm->sleep_pressure -= CIRCADIAN_SLEEP_DISSIPATION_RATE * elapsed_hours;
        if (rhythm->sleep_pressure < 0.0f) {
            rhythm->sleep_pressure = 0.0f;
        }
    } else {
        /* Accumulate sleep pressure during wake */
        rhythm->sleep_pressure += CIRCADIAN_SLEEP_ACCUMULATION_RATE * elapsed_hours;
        if (rhythm->sleep_pressure > CIRCADIAN_MAX_SLEEP_PRESSURE) {
            rhythm->sleep_pressure = CIRCADIAN_MAX_SLEEP_PRESSURE;
        }
    }
}

static float compute_phase_shift(circadian_rhythm_t* rhythm,
                                  circadian_zeitgeber_t zeitgeber,
                                  float strength)
{
    /* WHAT: Compute phase shift from zeitgeber
     * WHY:  Entrainment depends on zeitgeber type, timing, strength
     * HOW:  Phase response curve (PRC) based on current phase
     */

    if (!rhythm) {
        return 0.0f;
    }

    /* Get entrainment strength for this zeitgeber type */
    float entrainment_gain = 0.0f;
    switch (zeitgeber) {
        case CIRCADIAN_ZEITGEBER_LIGHT:
            entrainment_gain = rhythm->config.entrainment_strength_light;
            break;
        case CIRCADIAN_ZEITGEBER_ACTIVITY:
            entrainment_gain = rhythm->config.entrainment_strength_activity;
            break;
        case CIRCADIAN_ZEITGEBER_SOCIAL:
            entrainment_gain = rhythm->config.entrainment_strength_social;
            break;
        default:
            return 0.0f;
    }

    /* Phase response curve (PRC)
     * - Morning light: phase advance (negative shift = earlier)
     * - Evening light: phase delay (positive shift = later)
     * - Minimal effect at midday and deep night
     */
    float prc_value = 0.0f;
    switch (rhythm->current_phase) {
        case CIRCADIAN_PHASE_NIGHT_DEEP:
            prc_value = 0.0f;      /* Minimal response */
            break;
        case CIRCADIAN_PHASE_NIGHT_LATE:
            prc_value = -0.3f;     /* Phase advance */
            break;
        case CIRCADIAN_PHASE_DAWN:
            prc_value = -0.5f;     /* Strong phase advance */
            break;
        case CIRCADIAN_PHASE_MORNING:
            prc_value = -0.2f;     /* Weak advance */
            break;
        case CIRCADIAN_PHASE_MIDDAY:
            prc_value = 0.0f;      /* Minimal response */
            break;
        case CIRCADIAN_PHASE_AFTERNOON:
            prc_value = 0.1f;      /* Weak delay */
            break;
        case CIRCADIAN_PHASE_EVENING:
            prc_value = 0.4f;      /* Phase delay */
            break;
        case CIRCADIAN_PHASE_DUSK:
            prc_value = 0.3f;      /* Moderate delay */
            break;
        default:
            prc_value = 0.0f;
            break;
    }

    /* Compute final phase shift in hours */
    float phase_shift = prc_value * entrainment_gain * strength;

    return phase_shift;
}
