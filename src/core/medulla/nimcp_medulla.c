/**
 * @file nimcp_medulla.c
 * @brief Medulla Oblongata Main Orchestrator Implementation
 *
 * WHAT: Implementation of medulla orchestrator coordinating vital functions
 * WHY:  Provides centralized coordination of arousal, protection, circadian, and coupling
 * HOW:  Manages subsystems, integrates with external systems, coordinates state changes
 *
 * @author NIMCP Development Team
 * @date 2025-12-17
 * @version 1.0.0
 */

#include "core/medulla/nimcp_medulla.h"
#include "utils/fault_tolerance/nimcp_health_monitor.h"
#include "utils/fault_tolerance/nimcp_recovery.h"
#include "cognitive/nimcp_sleep_wake.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/time/nimcp_time.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include <stddef.h>  /* for NULL */

/* BBB Integration (Security) */
#include "security/nimcp_blood_brain_barrier.h"
#include "core/medulla/nimcp_medulla_bbb.h"

//=============================================================================
// Magic Number for Validation
//=============================================================================
#define MEDULLA_MAGIC 0x4D454455
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(medulla)

//=============================================================================
// BBB Integration (Blood-Brain Barrier)
//=============================================================================

/** Global BBB system for medulla input validation */
static bbb_system_t g_medulla_bbb = NULL;

/** Global BBB configuration for medulla-specific validation */
static medulla_bbb_config_t g_medulla_bbb_config;
static bool g_medulla_bbb_config_initialized = false;

void medulla_set_bbb(bbb_system_t bbb) {
    g_medulla_bbb = bbb;
    NIMCP_LOGGING_INFO("Medulla BBB %s", bbb ? "enabled" : "disabled");
}

void medulla_bbb_set_system(bbb_system_t bbb) {
    medulla_set_bbb(bbb);
}

medulla_bbb_config_t medulla_bbb_default_config(void) {
    medulla_bbb_config_t config;
    config.max_arousal_delta = 0.5f;
    config.min_health_score = 0.0f;
    config.max_health_score = 100.0f;
    config.max_neuromodulator_level = 2.0f;
    config.strict_mode = true;
    config.enable_logging = true;
    return config;
}

void medulla_bbb_set_config(const medulla_bbb_config_t* config) {
    if (config) {
        g_medulla_bbb_config = *config;
    } else {
        g_medulla_bbb_config = medulla_bbb_default_config();
    }
    g_medulla_bbb_config_initialized = true;
}

static medulla_bbb_config_t* get_medulla_bbb_config(void) {
    if (!g_medulla_bbb_config_initialized) {
        g_medulla_bbb_config = medulla_bbb_default_config();
        g_medulla_bbb_config_initialized = true;
    }
    return &g_medulla_bbb_config;
}

bool medulla_bbb_validate_arousal_input(float delta, bool is_boost,
                                         medulla_bbb_validation_result_t* result) {
    medulla_bbb_validation_result_t local_result;
    if (!result) result = &local_result;
    result->valid = true;
    result->reason[0] = '\0';
    result->has_safe_value = false;
    if (!g_medulla_bbb) return true;
    medulla_bbb_config_t* config = get_medulla_bbb_config();
    if (isnan(delta) || isinf(delta)) {
        result->valid = false;
        snprintf(result->reason, sizeof(result->reason), "Arousal delta is NaN or Inf");
        if (config->enable_logging) NIMCP_LOGGING_WARN("BBB rejected arousal: %s", result->reason);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_SECURITY_THREAT, result->reason);
        return false;
    }
    if (is_boost && delta < 0.0f) {
        result->valid = false;
        snprintf(result->reason, sizeof(result->reason), "Boost delta must be non-negative (got %.3f)", delta);
        if (config->enable_logging) NIMCP_LOGGING_WARN("BBB rejected arousal: %s", result->reason);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_SECURITY_THREAT, result->reason);
        return false;
    }
    float abs_delta = delta < 0 ? -delta : delta;
    if (abs_delta > config->max_arousal_delta) {
        result->valid = false;
        snprintf(result->reason, sizeof(result->reason), "Arousal delta %.3f exceeds max %.3f", abs_delta, config->max_arousal_delta);
        result->safe_value = is_boost ? config->max_arousal_delta : -config->max_arousal_delta;
        result->has_safe_value = true;
        if (config->enable_logging) NIMCP_LOGGING_WARN("BBB rejected arousal: %s", result->reason);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_SECURITY_THREAT, result->reason);
        return false;
    }
    bbb_validation_result_t bbb_result;
    bool valid = bbb_validate_input(g_medulla_bbb, &delta, sizeof(float), &bbb_result);
    if (!valid) {
        result->valid = false;
        snprintf(result->reason, sizeof(result->reason), "%s", bbb_result.reason);
        if (config->enable_logging) NIMCP_LOGGING_WARN("BBB rejected arousal: %s", result->reason);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_SECURITY_THREAT, result->reason);
        return false;
    }
    return true;
}

bool medulla_bbb_validate_health_alert(float health_score, medulla_bbb_validation_result_t* result) {
    medulla_bbb_validation_result_t local_result;
    if (!result) result = &local_result;
    result->valid = true;
    result->reason[0] = '\0';
    result->has_safe_value = false;
    if (!g_medulla_bbb) return true;
    medulla_bbb_config_t* config = get_medulla_bbb_config();
    if (isnan(health_score) || isinf(health_score)) {
        result->valid = false;
        snprintf(result->reason, sizeof(result->reason), "Health score is NaN or Inf");
        if (config->enable_logging) NIMCP_LOGGING_WARN("BBB rejected health: %s", result->reason);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_SECURITY_THREAT, result->reason);
        return false;
    }
    if (health_score < config->min_health_score || health_score > config->max_health_score) {
        result->valid = false;
        snprintf(result->reason, sizeof(result->reason), "Health score %.2f out of range [%.2f, %.2f]",
                 health_score, config->min_health_score, config->max_health_score);
        result->safe_value = (health_score < config->min_health_score) ? config->min_health_score : config->max_health_score;
        result->has_safe_value = true;
        if (config->enable_logging) NIMCP_LOGGING_WARN("BBB rejected health: %s", result->reason);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_SECURITY_THREAT, result->reason);
        return false;
    }
    bbb_validation_result_t bbb_result;
    bool valid = bbb_validate_input(g_medulla_bbb, &health_score, sizeof(float), &bbb_result);
    if (!valid) {
        result->valid = false;
        snprintf(result->reason, sizeof(result->reason), "%s", bbb_result.reason);
        if (config->enable_logging) NIMCP_LOGGING_WARN("BBB rejected health: %s", result->reason);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_SECURITY_THREAT, result->reason);
        return false;
    }
    return true;
}

bool medulla_bbb_validate_neuromod_input(float level, uint32_t neuromod_type,
                                          medulla_bbb_validation_result_t* result) {
    medulla_bbb_validation_result_t local_result;
    if (!result) result = &local_result;
    result->valid = true;
    result->reason[0] = '\0';
    result->has_safe_value = false;
    if (!g_medulla_bbb) return true;
    medulla_bbb_config_t* config = get_medulla_bbb_config();
    if (isnan(level) || isinf(level)) {
        result->valid = false;
        snprintf(result->reason, sizeof(result->reason), "Neuromodulator level (type %u) is NaN or Inf", neuromod_type);
        if (config->enable_logging) NIMCP_LOGGING_WARN("BBB rejected neuromod: %s", result->reason);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_SECURITY_THREAT, result->reason);
        return false;
    }
    if (level < 0.0f || level > config->max_neuromodulator_level) {
        result->valid = false;
        snprintf(result->reason, sizeof(result->reason), "Neuromodulator level %.3f (type %u) out of range [0, %.3f]",
                 level, neuromod_type, config->max_neuromodulator_level);
        result->safe_value = (level < 0.0f) ? 0.0f : config->max_neuromodulator_level;
        result->has_safe_value = true;
        if (config->enable_logging) NIMCP_LOGGING_WARN("BBB rejected neuromod: %s", result->reason);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_SECURITY_THREAT, result->reason);
        return false;
    }
    bbb_validation_result_t bbb_result;
    bool valid = bbb_validate_input(g_medulla_bbb, &level, sizeof(float), &bbb_result);
    if (!valid) {
        result->valid = false;
        snprintf(result->reason, sizeof(result->reason), "%s", bbb_result.reason);
        if (config->enable_logging) NIMCP_LOGGING_WARN("BBB rejected neuromod: %s", result->reason);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_SECURITY_THREAT, result->reason);
        return false;
    }
    return true;
}


//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Internal medulla structure
 */
struct medulla_struct {
    // Magic number for validation (must be first field)
    uint32_t magic;

    // Configuration
    medulla_config_t config;

    // Subsystems (Phase 1.2-1.5 - placeholders for now)
    struct arousal_state_struct* arousal;
    struct protective_cutoff_struct* protection;
    struct brainstem_coupling_struct* coupling;
    struct circadian_rhythm_struct* circadian;

    // External system connections
    health_monitor_t health_monitor;
    nimcp_recovery_t recovery_system;
    sleep_system_t sleep_wake;
    neuromodulator_system_t neuromodulators;

    // State
    medulla_state_t state;
    arousal_level_t arousal_level;
    protection_level_t protection_level;
    circadian_phase_t circadian_phase;

    // Current values
    float current_arousal;          // 0-1
    float current_circadian_time;   // 0-24 hours

    // Statistics
    medulla_stats_t stats;

    // Bio-async
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;

    // Thread safety
    nimcp_platform_mutex_t* mutex;

    // Timing
    uint64_t last_update_us;
    uint64_t start_time_us;
};

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * WHAT: Validate medulla pointer using magic number
 * WHY:  Detect use-after-free and invalid pointers
 * HOW:  Check non-null and magic number match
 */
static inline bool is_valid_medulla(const medulla_t medulla) {
    return medulla != NULL && medulla->magic == MEDULLA_MAGIC;
}

/**
 * WHAT: Get current timestamp in microseconds
 * WHY:  Timing for updates and statistics
 * HOW:  Use platform time API
 */
static uint64_t get_timestamp_us(void) {
    return nimcp_time_get_us();
}

/**
 * WHAT: Classify arousal level from continuous value
 * WHY:  Convert 0-1 arousal to categorical level
 * HOW:  Threshold-based categorization
 */
static arousal_level_t classify_arousal_level(float arousal) {
    if (arousal < 0.1f) return AROUSAL_LEVEL_COMA;
    if (arousal < 0.3f) return AROUSAL_LEVEL_DEEP_SLEEP;
    if (arousal < 0.4f) return AROUSAL_LEVEL_LIGHT_SLEEP;
    if (arousal < 0.5f) return AROUSAL_LEVEL_DROWSY;
    if (arousal < 0.7f) return AROUSAL_LEVEL_AWAKE;
    if (arousal < 0.9f) return AROUSAL_LEVEL_ALERT;
    return AROUSAL_LEVEL_HYPERAROUSAL;
}

/**
 * WHAT: Determine circadian phase from time
 * WHY:  Map circadian time to phase category
 * HOW:  Time-based categorization (24-hour cycle)
 */
static circadian_phase_t classify_circadian_phase(float hours) {
    // Normalize to 0-24 range
    while (hours >= 24.0f) hours -= 24.0f;
    while (hours < 0.0f) hours += 24.0f;

    if (hours >= 6.0f && hours < 9.0f) return CIRCADIAN_PHASE_EARLY_MORNING;
    if (hours >= 9.0f && hours < 12.0f) return CIRCADIAN_PHASE_MORNING;
    if (hours >= 12.0f && hours < 15.0f) return CIRCADIAN_PHASE_AFTERNOON;
    if (hours >= 15.0f && hours < 18.0f) return CIRCADIAN_PHASE_EVENING;
    if (hours >= 18.0f && hours < 21.0f) return CIRCADIAN_PHASE_LATE_EVENING;
    if (hours >= 21.0f && hours < 24.0f) return CIRCADIAN_PHASE_NIGHT;
    if (hours >= 0.0f && hours < 3.0f) return CIRCADIAN_PHASE_DEEP_NIGHT;
    return CIRCADIAN_PHASE_PRE_DAWN;
}

/**
 * WHAT: Get target arousal for circadian phase
 * WHY:  Different phases have different arousal targets
 * HOW:  Phase-dependent target values
 */
static float get_circadian_arousal_target(circadian_phase_t phase) {
    switch (phase) {
        case CIRCADIAN_PHASE_EARLY_MORNING: return 0.4f;  // Rising
        case CIRCADIAN_PHASE_MORNING: return 0.7f;        // Peak
        case CIRCADIAN_PHASE_AFTERNOON: return 0.6f;      // Dip
        case CIRCADIAN_PHASE_EVENING: return 0.7f;        // Second peak
        case CIRCADIAN_PHASE_LATE_EVENING: return 0.5f;   // Declining
        case CIRCADIAN_PHASE_NIGHT: return 0.3f;          // Sleep prep
        case CIRCADIAN_PHASE_DEEP_NIGHT: return 0.2f;     // Minimum
        case CIRCADIAN_PHASE_PRE_DAWN: return 0.3f;       // Pre-wake
        default: return 0.5f;
    }
}

/**
 * WHAT: Determine protection level from health score
 * WHY:  Map health status to protection response
 * HOW:  Threshold-based escalation
 */
static protection_level_t determine_protection_level(float health_score) {
    if (health_score < 10.0f) return PROTECTION_LEVEL_SHUTDOWN;
    if (health_score < 30.0f) return PROTECTION_LEVEL_CRITICAL;
    if (health_score < 50.0f) return PROTECTION_LEVEL_DEFENSIVE;
    if (health_score < 70.0f) return PROTECTION_LEVEL_GUARDED;
    if (health_score < 90.0f) return PROTECTION_LEVEL_CAUTIOUS;
    return PROTECTION_LEVEL_NORMAL;
}

/**
 * WHAT: Modulate arousal based on protection level
 * WHY:  Reduce arousal during protective states
 * HOW:  Apply protection-based suppression factor
 */
static float apply_protection_modulation(float arousal, protection_level_t protection) {
    float factor = 1.0f;
    switch (protection) {
        case PROTECTION_LEVEL_CAUTIOUS: factor = 0.9f; break;
        case PROTECTION_LEVEL_GUARDED: factor = 0.7f; break;
        case PROTECTION_LEVEL_DEFENSIVE: factor = 0.5f; break;
        case PROTECTION_LEVEL_CRITICAL: factor = 0.3f; break;
        case PROTECTION_LEVEL_SHUTDOWN: factor = 0.1f; break;
        default: factor = 1.0f; break;
    }
    return arousal * factor;
}

/* Forward declaration for broadcast helper */
static void medulla_broadcast_state_change(medulla_t medulla, bio_message_type_t type);

/**
 * WHAT: Update circadian rhythm
 * WHY:  Progress circadian time
 * HOW:  Increment time, wrap at period
 */
static void update_circadian(medulla_t medulla, float dt) {
    if (!medulla || !medulla->config.circadian.enable_synchronization) {
        return;
    }

    /* Store previous phase for change detection */
    circadian_phase_t prev_phase = medulla->circadian_phase;

    // Progress time (hours)
    float dt_hours = dt / 3600.0f;
    medulla->current_circadian_time += dt_hours;

    // Wrap at period
    float period = medulla->config.circadian.period_hours;
    while (medulla->current_circadian_time >= period) {
        medulla->current_circadian_time -= period;
        medulla->stats.circadian_cycles++;
    }

    // Update phase
    medulla->circadian_phase = classify_circadian_phase(medulla->current_circadian_time);
    medulla->stats.circadian_time_hours = medulla->current_circadian_time;

    /* Broadcast if phase changed */
    if (medulla->circadian_phase != prev_phase) {
        medulla_broadcast_state_change(medulla, BIO_MSG_MEDULLA_CIRCADIAN_CHANGED);
    }
}

/**
 * WHAT: Update arousal state
 * WHY:  Manage consciousness level
 * HOW:  Apply circadian target, protection modulation, decay
 */
static void update_arousal(medulla_t medulla, float dt) {
    if (!medulla) return;

    /* Store previous level for change detection */
    arousal_level_t prev_level = medulla->arousal_level;
    float prev_arousal = medulla->current_arousal;

    // Get circadian target
    float target = get_circadian_arousal_target(medulla->circadian_phase);

    // Move towards target
    float rate = medulla->config.arousal.arousal_decay_rate;
    float delta = target - medulla->current_arousal;
    medulla->current_arousal += delta * rate * dt;

    // Apply protection modulation
    medulla->current_arousal = apply_protection_modulation(
        medulla->current_arousal,
        medulla->protection_level
    );

    // Clamp
    if (medulla->current_arousal < medulla->config.arousal.min_arousal) {
        medulla->current_arousal = medulla->config.arousal.min_arousal;
    }
    if (medulla->current_arousal > medulla->config.arousal.max_arousal) {
        medulla->current_arousal = medulla->config.arousal.max_arousal;
    }

    // Update classification
    medulla->arousal_level = classify_arousal_level(medulla->current_arousal);

    // Update stats
    medulla->stats.arousal_updates++;
    medulla->stats.avg_arousal = (medulla->stats.avg_arousal * (medulla->stats.arousal_updates - 1) +
                                   medulla->current_arousal) / medulla->stats.arousal_updates;

    /* Broadcast if arousal level category changed or significant value change (>0.1) */
    if (medulla->arousal_level != prev_level ||
        fabsf(medulla->current_arousal - prev_arousal) > 0.1f) {
        medulla_broadcast_state_change(medulla, BIO_MSG_MEDULLA_AROUSAL_CHANGED);
    }
}

/**
 * WHAT: Update protection level
 * WHY:  Respond to health status
 * HOW:  Query health monitor, adjust protection
 */
static void update_protection(medulla_t medulla) {
    if (!medulla || !medulla->health_monitor) {
        return;
    }

    // Get health score
    float health_score = health_monitor_get_score(medulla->health_monitor);

    // BBB validation of health score before using it for protection decisions
    if (!medulla_bbb_validate_health_alert(health_score, NULL)) {
        NIMCP_LOGGING_WARN("BBB rejected health score %.2f, skipping protection update", health_score);
        return;
    }

    // Determine new protection level
    protection_level_t new_level = determine_protection_level(health_score);

    // Track state changes and broadcast
    if (new_level != medulla->protection_level) {
        if (new_level > PROTECTION_LEVEL_NORMAL) {
            medulla->stats.protection_activations++;
        }
        medulla->protection_level = new_level;

        /* Broadcast protection level change */
        medulla_broadcast_state_change(medulla, BIO_MSG_MEDULLA_PROTECTION_CHANGED);
    }

    // Emergency shutdown if critical
    if (new_level == PROTECTION_LEVEL_SHUTDOWN &&
        medulla->config.protection.enable_auto_shutdown) {
        medulla_emergency_shutdown(medulla, "Health score critical");
    }
}

/**
 * WHAT: Synchronize with sleep-wake system
 * WHY:  Bidirectional arousal-sleep state coupling
 * HOW:  Query sleep state, adjust arousal accordingly
 */
static void synchronize_sleep_wake(medulla_t medulla) {
    if (!medulla || !medulla->sleep_wake) {
        return;
    }

    sleep_state_t sleep_state = sleep_get_current_state(medulla->sleep_wake);

    // Map sleep state to arousal
    float target_arousal = 0.5f;
    switch (sleep_state) {
        case SLEEP_STATE_AWAKE:
            target_arousal = 0.7f;
            break;
        case SLEEP_STATE_DROWSY:
            target_arousal = 0.4f;
            break;
        case SLEEP_STATE_LIGHT_NREM:
            target_arousal = 0.3f;
            break;
        case SLEEP_STATE_DEEP_NREM:
            target_arousal = 0.2f;
            break;
        case SLEEP_STATE_REM:
            target_arousal = 0.5f;  // REM has higher arousal
            break;
    }

    // Blend with current arousal
    medulla->current_arousal = medulla->current_arousal * 0.7f + target_arousal * 0.3f;
    medulla->stats.sleep_transitions++;
}

/**
 * WHAT: Apply neuromodulator adjustments
 * WHY:  Neuromodulators influence arousal and circadian
 * HOW:  Query neuromodulator levels, apply modulation
 */
static void apply_neuromodulator_modulation(medulla_t medulla) {
    if (!medulla || !medulla->neuromodulators) {
        return;
    }

    // Get norepinephrine (arousal)
    float norepinephrine = neuromodulator_get_level(
        medulla->neuromodulators,
        NEUROMOD_NOREPINEPHRINE
    );

    // BBB validation of norepinephrine level
    if (!medulla_bbb_validate_neuromod_input(norepinephrine, NEUROMOD_NOREPINEPHRINE, NULL)) {
        NIMCP_LOGGING_WARN("BBB rejected norepinephrine level %.3f", norepinephrine);
        norepinephrine = 0.0f;  /* Use safe default */
    }

    // Get acetylcholine (alertness)
    float acetylcholine = neuromodulator_get_level(
        medulla->neuromodulators,
        NEUROMOD_ACETYLCHOLINE
    );

    // BBB validation of acetylcholine level
    if (!medulla_bbb_validate_neuromod_input(acetylcholine, NEUROMOD_ACETYLCHOLINE, NULL)) {
        NIMCP_LOGGING_WARN("BBB rejected acetylcholine level %.3f", acetylcholine);
        acetylcholine = 0.0f;  /* Use safe default */
    }

    // Modulate arousal
    float boost = (norepinephrine + acetylcholine) * 0.1f;
    medulla->current_arousal += boost;

    // Clamp
    if (medulla->current_arousal > medulla->config.arousal.max_arousal) {
        medulla->current_arousal = medulla->config.arousal.max_arousal;
    }
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

medulla_config_t medulla_default_config(void) {
    medulla_config_t config;
    memset(&config, 0, sizeof(config));

    // Arousal defaults
    config.arousal.baseline_arousal = 0.5f;
    config.arousal.arousal_decay_rate = 0.1f;
    config.arousal.min_arousal = 0.0f;
    config.arousal.max_arousal = 1.0f;
    config.arousal.enable_auto_regulation = true;

    // Protection defaults
    config.protection.health_threshold_critical = 30.0f;
    config.protection.health_threshold_defensive = 50.0f;
    config.protection.recovery_time_ms = 5000;
    config.protection.enable_auto_shutdown = true;

    // Coupling defaults
    config.coupling.coupling_strength = 0.8f;
    config.coupling.latency_ms = 10.0f;
    config.coupling.enable_bidirectional = true;

    // Circadian defaults
    config.circadian.period_hours = 24.0f;
    config.circadian.phase_offset_hours = 0.0f;
    config.circadian.amplitude = 0.5f;
    config.circadian.enable_synchronization = true;

    // Integration defaults
    config.enable_health_integration = true;
    config.enable_recovery_integration = true;
    config.enable_sleep_integration = true;
    config.enable_neuromod_integration = true;

    // Update timing
    config.update_interval_ms = 100;

    // Bio-async
    config.enable_bio_async = true;
    config.inbox_capacity = 64;

    return config;
}

medulla_t medulla_create(const medulla_config_t* config) {
    // Allocate structure
    medulla_t medulla = nimcp_calloc(1, sizeof(struct medulla_struct));
    if (!medulla) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "medulla_create: failed to allocate medulla structure");
        NIMCP_LOGGING_ERROR("Failed to allocate medulla structure");
        return NULL;
    }

    // Set magic number for validation
    medulla->magic = MEDULLA_MAGIC;

    // Copy config
    if (config) {
        medulla->config = *config;
    } else {
        medulla->config = medulla_default_config();
    }

    // Initialize state
    medulla->state = MEDULLA_STATE_STOPPED;
    medulla->arousal_level = AROUSAL_LEVEL_AWAKE;
    medulla->protection_level = PROTECTION_LEVEL_NORMAL;
    medulla->circadian_phase = CIRCADIAN_PHASE_MORNING;

    // Initialize values
    medulla->current_arousal = medulla->config.arousal.baseline_arousal;
    medulla->current_circadian_time = medulla->config.circadian.phase_offset_hours;

    // Initialize stats
    memset(&medulla->stats, 0, sizeof(medulla_stats_t));
    medulla->stats.state = medulla->state;
    medulla->stats.arousal_level = medulla->arousal_level;
    medulla->stats.protection_level = medulla->protection_level;
    medulla->stats.circadian_phase = medulla->circadian_phase;
    medulla->stats.current_arousal = medulla->current_arousal;

    // Create mutex
    medulla->mutex = nimcp_platform_mutex_create();
    if (!medulla->mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "medulla_create: failed to create mutex");
        NIMCP_LOGGING_ERROR("Failed to create medulla mutex");
        nimcp_free(medulla);
        return NULL;
    }

    // Subsystems will be created in Phase 1.2-1.5
    medulla->arousal = NULL;
    medulla->protection = NULL;
    medulla->coupling = NULL;
    medulla->circadian = NULL;

    // External connections initialized to NULL
    medulla->health_monitor = NULL;
    medulla->recovery_system = NULL;
    medulla->sleep_wake = NULL;
    medulla->neuromodulators = NULL;

    // Bio-async
    medulla->bio_async_enabled = false;
    medulla->bio_ctx = NULL;

    // Timing
    medulla->last_update_us = get_timestamp_us();
    medulla->start_time_us = medulla->last_update_us;

    NIMCP_LOGGING_INFO("Medulla orchestrator created");
    return medulla;
}

void medulla_destroy(medulla_t medulla) {
    if (!is_valid_medulla(medulla)) {
        return;
    }

    // Stop if running
    if (medulla->state == MEDULLA_STATE_RUNNING) {
        medulla_stop(medulla);
    }

    // Disconnect bio-async
    if (medulla->bio_async_enabled) {
        medulla_disconnect_bio_async(medulla);
    }

    // Destroy subsystems (Phase 1.2-1.5 will implement)
    // arousal_state_destroy(medulla->arousal);
    // protective_cutoff_destroy(medulla->protection);
    // brainstem_coupling_destroy(medulla->coupling);
    // circadian_rhythm_destroy(medulla->circadian);

    // Clear magic number before destruction
    medulla->magic = 0;

    // Destroy mutex
    if (medulla->mutex) {
        nimcp_platform_mutex_destroy(medulla->mutex);
        nimcp_free(medulla->mutex);
        medulla->mutex = NULL;
    }

    // Free structure
    nimcp_free(medulla);

    NIMCP_LOGGING_INFO("Medulla orchestrator destroyed");
}

//=============================================================================
// Control Functions
//=============================================================================

int medulla_start(medulla_t medulla) {
    if (!is_valid_medulla(medulla)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "medulla_start: medulla is NULL or invalid");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(medulla->mutex);

    if (medulla->state == MEDULLA_STATE_RUNNING) {
        nimcp_platform_mutex_unlock(medulla->mutex);
        return 0;  // Already running
    }

    medulla->state = MEDULLA_STATE_STARTING;

    // Start subsystems (Phase 1.2-1.5 will implement)
    // arousal_state_start(medulla->arousal);
    // protective_cutoff_start(medulla->protection);
    // brainstem_coupling_start(medulla->coupling);
    // circadian_rhythm_start(medulla->circadian);

    // Reset timing
    medulla->last_update_us = get_timestamp_us();
    medulla->start_time_us = medulla->last_update_us;

    medulla->state = MEDULLA_STATE_RUNNING;
    medulla->stats.state = medulla->state;

    nimcp_platform_mutex_unlock(medulla->mutex);

    /* Heartbeat when started */
    medulla_heartbeat("medulla_started", 1.0f);

    NIMCP_LOGGING_INFO("Medulla orchestrator started");
    return 0;
}

int medulla_stop(medulla_t medulla) {
    if (!is_valid_medulla(medulla)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "medulla_stop: medulla is NULL or invalid");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(medulla->mutex);

    if (medulla->state == MEDULLA_STATE_STOPPED) {
        nimcp_platform_mutex_unlock(medulla->mutex);
        return 0;  // Already stopped
    }

    medulla->state = MEDULLA_STATE_STOPPING;

    // Stop subsystems (Phase 1.2-1.5 will implement)
    // arousal_state_stop(medulla->arousal);
    // protective_cutoff_stop(medulla->protection);
    // brainstem_coupling_stop(medulla->coupling);
    // circadian_rhythm_stop(medulla->circadian);

    medulla->state = MEDULLA_STATE_STOPPED;
    medulla->stats.state = medulla->state;

    nimcp_platform_mutex_unlock(medulla->mutex);

    /* Heartbeat when stopped */
    medulla_heartbeat("medulla_stopped", 1.0f);

    NIMCP_LOGGING_INFO("Medulla orchestrator stopped");
    return 0;
}

int medulla_update(medulla_t medulla, float dt) {
    if (!is_valid_medulla(medulla)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "medulla_update: medulla is NULL or invalid");
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Heartbeat at start of update */
    medulla_heartbeat("medulla_update_start", 0.0f);

    nimcp_platform_mutex_lock(medulla->mutex);

    if (medulla->state != MEDULLA_STATE_RUNNING) {
        nimcp_platform_mutex_unlock(medulla->mutex);
        return -1;
    }

    uint64_t start_us = get_timestamp_us();

    // Coordination pipeline
    // 1. Update circadian rhythm
    update_circadian(medulla, dt);

    // 2. Check health monitor and update protection
    if (medulla->config.enable_health_integration) {
        update_protection(medulla);
    }

    // 3. Update arousal state (modulated by protection/circadian)
    update_arousal(medulla, dt);

    // 4. Apply neuromodulator adjustments
    if (medulla->config.enable_neuromod_integration) {
        apply_neuromodulator_modulation(medulla);
    }

    // 5. Synchronize with sleep-wake
    if (medulla->config.enable_sleep_integration) {
        synchronize_sleep_wake(medulla);
    }

    // 6. Update brainstem coupling (Phase 1.4 will implement)
    // brainstem_coupling_update(medulla->coupling, dt);

    // Update stats
    medulla->stats.total_updates++;
    medulla->stats.current_arousal = medulla->current_arousal;
    medulla->stats.arousal_level = medulla->arousal_level;
    medulla->stats.protection_level = medulla->protection_level;
    medulla->stats.circadian_phase = medulla->circadian_phase;

    uint64_t end_us = get_timestamp_us();
    float update_time_us = (float)(end_us - start_us);
    medulla->stats.avg_update_time_us =
        (medulla->stats.avg_update_time_us * (medulla->stats.total_updates - 1) +
         update_time_us) / medulla->stats.total_updates;

    medulla->stats.uptime_ms = (end_us - medulla->start_time_us) / 1000;
    medulla->last_update_us = end_us;

    nimcp_platform_mutex_unlock(medulla->mutex);

    /* Heartbeat at end of update */
    medulla_heartbeat("medulla_update_complete", 1.0f);

    return 0;
}

int medulla_emergency_shutdown(medulla_t medulla, const char* reason) {
    if (!is_valid_medulla(medulla)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "medulla_emergency_shutdown: medulla is NULL or invalid");
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Heartbeat for emergency */
    medulla_heartbeat("medulla_emergency", 0.0f);

    nimcp_platform_mutex_lock(medulla->mutex);

    NIMCP_LOGGING_ERROR("Medulla emergency shutdown: %s", reason ? reason : "unknown");

    // Set emergency state
    medulla->state = MEDULLA_STATE_EMERGENCY;
    medulla->protection_level = PROTECTION_LEVEL_SHUTDOWN;
    medulla->current_arousal = medulla->config.arousal.min_arousal;

    // Update stats
    medulla->stats.emergency_shutdowns++;
    medulla->stats.state = medulla->state;
    medulla->stats.protection_level = medulla->protection_level;

    // Trigger recovery if connected
    if (medulla->recovery_system && medulla->config.enable_recovery_integration) {
        medulla->stats.recovery_triggers++;
        // nimcp_recovery_trigger(medulla->recovery_system, reason);
    }

    nimcp_platform_mutex_unlock(medulla->mutex);

    /* Broadcast emergency shutdown to all modules */
    medulla_broadcast_state_change(medulla, BIO_MSG_MEDULLA_EMERGENCY_SHUTDOWN);

    return 0;
}

int medulla_request_state_change(medulla_t medulla, medulla_state_t new_state) {
    if (!is_valid_medulla(medulla)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "medulla_request_state_change: medulla is NULL or invalid");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(medulla->mutex);

    // Validate transition
    if (medulla->state == MEDULLA_STATE_EMERGENCY &&
        new_state != MEDULLA_STATE_STOPPED) {
        NIMCP_LOGGING_WARN("Cannot transition from EMERGENCY except to STOPPED");
        nimcp_platform_mutex_unlock(medulla->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "medulla_request_state_change: operation failed");
        return -1;
    }

    medulla->state = new_state;
    medulla->stats.state = new_state;

    nimcp_platform_mutex_unlock(medulla->mutex);

    NIMCP_LOGGING_INFO("Medulla state changed to %s",
                       medulla_state_to_string(new_state));
    return 0;
}

//=============================================================================
// Integration Functions
//=============================================================================

int medulla_connect_health_monitor(medulla_t medulla, health_monitor_t health_monitor) {
    if (!is_valid_medulla(medulla)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "medulla_connect_health_monitor: medulla is NULL or invalid");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(medulla->mutex);
    medulla->health_monitor = health_monitor;
    nimcp_platform_mutex_unlock(medulla->mutex);

    NIMCP_LOGGING_INFO("Health monitor connected to medulla");
    return 0;
}

int medulla_connect_recovery_system(medulla_t medulla, nimcp_recovery_t recovery) {
    if (!is_valid_medulla(medulla)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "medulla_connect_recovery_system: medulla is NULL or invalid");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(medulla->mutex);
    medulla->recovery_system = recovery;
    nimcp_platform_mutex_unlock(medulla->mutex);

    NIMCP_LOGGING_INFO("Recovery system connected to medulla");
    return 0;
}

int medulla_connect_sleep_wake(medulla_t medulla, sleep_system_t sleep_wake) {
    if (!is_valid_medulla(medulla)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "medulla_connect_sleep_wake: medulla is NULL or invalid");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(medulla->mutex);
    medulla->sleep_wake = sleep_wake;
    nimcp_platform_mutex_unlock(medulla->mutex);

    NIMCP_LOGGING_INFO("Sleep-wake system connected to medulla");
    return 0;
}

int medulla_connect_neuromodulators(medulla_t medulla, neuromodulator_system_t neuromodulators) {
    if (!is_valid_medulla(medulla)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "medulla_connect_neuromodulators: medulla is NULL or invalid");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(medulla->mutex);
    medulla->neuromodulators = neuromodulators;
    nimcp_platform_mutex_unlock(medulla->mutex);

    NIMCP_LOGGING_INFO("Neuromodulator system connected to medulla");
    return 0;
}

//=============================================================================
// Query Functions
//=============================================================================

float medulla_get_arousal_level(const medulla_t medulla) {
    if (!is_valid_medulla(medulla)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "medulla_get_arousal_level: medulla is NULL or invalid");
        return -1.0f;
    }

    nimcp_platform_mutex_lock(medulla->mutex);
    float arousal = medulla->current_arousal;
    nimcp_platform_mutex_unlock(medulla->mutex);

    return arousal;
}

int medulla_boost_arousal(medulla_t medulla, float delta) {
    if (!is_valid_medulla(medulla)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "medulla_boost_arousal: medulla is NULL or invalid");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (delta < 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "medulla_boost_arousal: delta must be non-negative");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* BBB validation of arousal delta */
    if (!medulla_bbb_validate_arousal_input(delta, true, NULL)) {
        return NIMCP_ERROR_SECURITY_THREAT;
    }

    nimcp_platform_mutex_lock(medulla->mutex);

    float new_arousal = medulla->current_arousal + delta;

    /* Clamp to configured range */
    if (new_arousal > medulla->config.arousal.max_arousal) {
        new_arousal = medulla->config.arousal.max_arousal;
    }

    medulla->current_arousal = new_arousal;

    nimcp_platform_mutex_unlock(medulla->mutex);
    return 0;
}

int medulla_reduce_arousal(medulla_t medulla, float delta) {
    if (!is_valid_medulla(medulla)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "medulla_reduce_arousal: medulla is NULL or invalid");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (delta < 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "medulla_reduce_arousal: delta must be non-negative");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* BBB validation of arousal delta (reduce uses negative internally but API expects positive) */
    if (!medulla_bbb_validate_arousal_input(delta, false, NULL)) {
        return NIMCP_ERROR_SECURITY_THREAT;
    }

    nimcp_platform_mutex_lock(medulla->mutex);

    float new_arousal = medulla->current_arousal - delta;

    /* Clamp to configured range */
    if (new_arousal < medulla->config.arousal.min_arousal) {
        new_arousal = medulla->config.arousal.min_arousal;
    }

    medulla->current_arousal = new_arousal;

    nimcp_platform_mutex_unlock(medulla->mutex);
    return 0;
}

protection_level_t medulla_get_protection_level(const medulla_t medulla) {
    if (!is_valid_medulla(medulla)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "medulla_get_protection_level: medulla is NULL or invalid");
        return PROTECTION_LEVEL_NORMAL;
    }

    nimcp_platform_mutex_lock(medulla->mutex);
    protection_level_t level = medulla->protection_level;
    nimcp_platform_mutex_unlock(medulla->mutex);

    return level;
}

circadian_phase_t medulla_get_circadian_phase(const medulla_t medulla) {
    if (!is_valid_medulla(medulla)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "medulla_get_circadian_phase: medulla is NULL or invalid");
        return CIRCADIAN_PHASE_MORNING;
    }

    nimcp_platform_mutex_lock(medulla->mutex);
    circadian_phase_t phase = medulla->circadian_phase;
    nimcp_platform_mutex_unlock(medulla->mutex);

    return phase;
}

int medulla_get_stats(const medulla_t medulla, medulla_stats_t* stats) {
    if (!is_valid_medulla(medulla)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "medulla_get_stats: medulla is NULL or invalid");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "medulla_get_stats: stats is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(medulla->mutex);
    *stats = medulla->stats;
    nimcp_platform_mutex_unlock(medulla->mutex);

    return 0;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * @brief Message payload for medulla state broadcasts
 */
typedef struct {
    bio_message_header_t header;
    float arousal;                      /**< Current arousal level [0-1] */
    protection_level_t protection;      /**< Current protection level */
    circadian_phase_t phase;            /**< Current circadian phase */
    arousal_level_t arousal_level;      /**< Classified arousal level */
    medulla_state_t state;              /**< Current medulla state */
} bio_msg_medulla_state_t;

/**
 * @brief Broadcast medulla state change via bio-async
 *
 * WHAT: Notify other modules of medulla state changes
 * WHY:  Enables reactive coordination without tight coupling
 * HOW:  Send typed message via bio-async router
 *
 * @param medulla Medulla instance
 * @param type Message type indicating what changed
 */
static void medulla_broadcast_state_change(medulla_t medulla, bio_message_type_t type) {
    if (!medulla || !medulla->bio_async_enabled || !medulla->bio_ctx) {
        return;
    }

    bio_msg_medulla_state_t msg;
    memset(&msg, 0, sizeof(msg));

    /* Initialize header */
    bio_msg_init_header(&msg.header, type, BIO_MODULE_MEDULLA, 0, sizeof(msg));
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;

    /* Fill payload with current state */
    msg.arousal = medulla->current_arousal;
    msg.protection = medulla->protection_level;
    msg.phase = medulla->circadian_phase;
    msg.arousal_level = medulla->arousal_level;
    msg.state = medulla->state;

    bio_router_broadcast(medulla->bio_ctx, &msg, sizeof(msg));

    NIMCP_LOGGING_DEBUG("Medulla broadcast: type=0x%04X arousal=%.2f protection=%d phase=%d",
                        type, msg.arousal, msg.protection, msg.phase);
}

/**
 * @brief Bio-async message handler for medulla
 *
 * WHAT: Handle incoming bio-async messages for medulla module
 * WHY:  Process state requests and commands from other modules
 * HOW:  Dispatch by message type
 *
 * @param msg Pointer to message (header + payload)
 * @param msg_size Total message size
 * @param response_promise Promise to complete with response (may be NULL)
 * @param user_data Medulla instance
 * @return NIMCP_SUCCESS or error code
 */
static nimcp_error_t medulla_bio_handler(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    (void)response_promise;  /* Response not needed for current message types */
    medulla_t medulla = (medulla_t)user_data;
    const bio_message_header_t* header = (const bio_message_header_t*)msg;

    if (!medulla || !header || msg_size < sizeof(bio_message_header_t)) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    switch (header->type) {
        case BIO_MSG_MEDULLA_STATE_REQUEST:
            /* Respond with current state */
            medulla_broadcast_state_change(medulla, BIO_MSG_MEDULLA_STATE);
            break;

        case BIO_MSG_MEDULLA_AROUSAL_SET:
            /* Handle arousal set command from hypothalamus */
            /* Extract arousal value from payload if present */
            if (msg_size > sizeof(bio_message_header_t)) {
                const float* arousal_ptr = (const float*)((const char*)msg + sizeof(bio_message_header_t));
                nimcp_platform_mutex_lock(medulla->mutex);
                medulla->current_arousal = *arousal_ptr;
                if (medulla->current_arousal < medulla->config.arousal.min_arousal) {
                    medulla->current_arousal = medulla->config.arousal.min_arousal;
                }
                if (medulla->current_arousal > medulla->config.arousal.max_arousal) {
                    medulla->current_arousal = medulla->config.arousal.max_arousal;
                }
                medulla->arousal_level = classify_arousal_level(medulla->current_arousal);
                nimcp_platform_mutex_unlock(medulla->mutex);
            }
            break;

        case BIO_MSG_MEDULLA_PROTECTION_SET:
            /* Handle protection set command */
            if (msg_size > sizeof(bio_message_header_t)) {
                const protection_level_t* level_ptr = (const protection_level_t*)((const char*)msg + sizeof(bio_message_header_t));
                nimcp_platform_mutex_lock(medulla->mutex);
                medulla->protection_level = *level_ptr;
                nimcp_platform_mutex_unlock(medulla->mutex);
            }
            break;

        case BIO_MSG_MEDULLA_EMERGENCY_REQUEST:
            /* Handle emergency shutdown request */
            medulla_emergency_shutdown(medulla, "Bio-async emergency request");
            break;

        default:
            /* Unknown message type - ignore */
            break;
    }

    return NIMCP_SUCCESS;
}

int medulla_connect_bio_async(medulla_t medulla) {
    if (!is_valid_medulla(medulla)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "medulla_connect_bio_async: medulla is NULL or invalid");
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (medulla->bio_async_enabled) {
        return 0;  /* Already connected */
    }

    /* Check if router is initialized */
    if (!bio_router_is_initialized()) {
        NIMCP_LOGGING_WARN("Bio-async router not initialized, skipping medulla registration");
        return NIMCP_ERROR_NOT_INITIALIZED;
    }

    /* Register module with bio-async router */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_MEDULLA,
        .module_name = "medulla",
        .inbox_capacity = medulla->config.inbox_capacity > 0 ? medulla->config.inbox_capacity : 64,
        .user_data = medulla
    };

    medulla->bio_ctx = bio_router_register_module(&info);
    if (!medulla->bio_ctx) {
        NIMCP_LOGGING_ERROR("Failed to register medulla with bio-async router");
        return NIMCP_ERROR_NO_MEMORY;
    }

    /* Register message handlers */
    bio_router_register_handler(medulla->bio_ctx, BIO_MSG_MEDULLA_STATE_REQUEST, medulla_bio_handler);
    bio_router_register_handler(medulla->bio_ctx, BIO_MSG_MEDULLA_AROUSAL_SET, medulla_bio_handler);
    bio_router_register_handler(medulla->bio_ctx, BIO_MSG_MEDULLA_PROTECTION_SET, medulla_bio_handler);
    bio_router_register_handler(medulla->bio_ctx, BIO_MSG_MEDULLA_EMERGENCY_REQUEST, medulla_bio_handler);

    medulla->bio_async_enabled = true;

    NIMCP_LOGGING_INFO("Medulla connected to bio-async router");
    return NIMCP_SUCCESS;
}

int medulla_disconnect_bio_async(medulla_t medulla) {
    if (!is_valid_medulla(medulla)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "medulla_disconnect_bio_async: medulla is NULL or invalid");
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (!medulla->bio_async_enabled) {
        return 0;  /* Not connected */
    }

    /* Unregister from bio-async router */
    if (medulla->bio_ctx) {
        bio_router_unregister_module(medulla->bio_ctx);
    }

    medulla->bio_async_enabled = false;
    medulla->bio_ctx = NULL;

    NIMCP_LOGGING_INFO("Medulla disconnected from bio-async router");
    return 0;
}

bool medulla_is_bio_async_connected(const medulla_t medulla) {
    if (!is_valid_medulla(medulla)) {
        return false;
    }

    return medulla->bio_async_enabled;
}

/**
 * @brief Notify arousal level change via bio-async
 *
 * Called when arousal level changes significantly.
 */
void medulla_notify_arousal_changed(medulla_t medulla) {
    if (is_valid_medulla(medulla)) {
        medulla_broadcast_state_change(medulla, BIO_MSG_MEDULLA_AROUSAL_CHANGED);
    }
}

/**
 * @brief Notify protection level change via bio-async
 *
 * Called when protection level changes.
 */
void medulla_notify_protection_changed(medulla_t medulla) {
    if (is_valid_medulla(medulla)) {
        medulla_broadcast_state_change(medulla, BIO_MSG_MEDULLA_PROTECTION_CHANGED);
    }
}

/**
 * @brief Notify circadian phase change via bio-async
 *
 * Called when circadian phase changes.
 */
void medulla_notify_circadian_changed(medulla_t medulla) {
    if (is_valid_medulla(medulla)) {
        medulla_broadcast_state_change(medulla, BIO_MSG_MEDULLA_CIRCADIAN_CHANGED);
    }
}

/**
 * @brief Notify emergency shutdown via bio-async
 *
 * Called when emergency shutdown is initiated.
 */
void medulla_notify_emergency_shutdown(medulla_t medulla) {
    if (is_valid_medulla(medulla)) {
        medulla_broadcast_state_change(medulla, BIO_MSG_MEDULLA_EMERGENCY_SHUTDOWN);
    }
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* medulla_arousal_level_to_string(arousal_level_t level) {
    switch (level) {
        case AROUSAL_LEVEL_COMA: return "COMA";
        case AROUSAL_LEVEL_DEEP_SLEEP: return "DEEP_SLEEP";
        case AROUSAL_LEVEL_LIGHT_SLEEP: return "LIGHT_SLEEP";
        case AROUSAL_LEVEL_DROWSY: return "DROWSY";
        case AROUSAL_LEVEL_AWAKE: return "AWAKE";
        case AROUSAL_LEVEL_ALERT: return "ALERT";
        case AROUSAL_LEVEL_HYPERAROUSAL: return "HYPERAROUSAL";
        default: return "UNKNOWN";
    }
}

const char* medulla_protection_level_to_string(protection_level_t level) {
    switch (level) {
        case PROTECTION_LEVEL_NORMAL: return "NORMAL";
        case PROTECTION_LEVEL_CAUTIOUS: return "CAUTIOUS";
        case PROTECTION_LEVEL_GUARDED: return "GUARDED";
        case PROTECTION_LEVEL_DEFENSIVE: return "DEFENSIVE";
        case PROTECTION_LEVEL_CRITICAL: return "CRITICAL";
        case PROTECTION_LEVEL_SHUTDOWN: return "SHUTDOWN";
        default: return "UNKNOWN";
    }
}

const char* medulla_circadian_phase_to_string(circadian_phase_t phase) {
    switch (phase) {
        case CIRCADIAN_PHASE_EARLY_MORNING: return "EARLY_MORNING";
        case CIRCADIAN_PHASE_MORNING: return "MORNING";
        case CIRCADIAN_PHASE_AFTERNOON: return "AFTERNOON";
        case CIRCADIAN_PHASE_EVENING: return "EVENING";
        case CIRCADIAN_PHASE_LATE_EVENING: return "LATE_EVENING";
        case CIRCADIAN_PHASE_NIGHT: return "NIGHT";
        case CIRCADIAN_PHASE_DEEP_NIGHT: return "DEEP_NIGHT";
        case CIRCADIAN_PHASE_PRE_DAWN: return "PRE_DAWN";
        default: return "UNKNOWN";
    }
}

const char* medulla_state_to_string(medulla_state_t state) {
    switch (state) {
        case MEDULLA_STATE_STOPPED: return "STOPPED";
        case MEDULLA_STATE_STARTING: return "STARTING";
        case MEDULLA_STATE_RUNNING: return "RUNNING";
        case MEDULLA_STATE_DEGRADED: return "DEGRADED";
        case MEDULLA_STATE_EMERGENCY: return "EMERGENCY";
        case MEDULLA_STATE_STOPPING: return "STOPPING";
        default: return "UNKNOWN";
    }
}

//=============================================================================
// Test Helper Functions
//=============================================================================

int medulla_test_set_arousal(medulla_t medulla, float level) {
    if (!is_valid_medulla(medulla)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "medulla_test_set_arousal: is_valid_medulla is NULL");
        return -1;
    }

    /* Clamp level to valid range */
    if (level < 0.0f) level = 0.0f;
    if (level > 1.0f) level = 1.0f;

    nimcp_platform_mutex_lock(medulla->mutex);

    /* Set current arousal directly */
    medulla->current_arousal = level;

    /* Update arousal level enum using the same thresholds as normal operation */
    medulla->arousal_level = classify_arousal_level(level);

    /* Also update stats for test visibility */
    medulla->stats.arousal_level = medulla->arousal_level;
    medulla->stats.current_arousal = medulla->current_arousal;

    nimcp_platform_mutex_unlock(medulla->mutex);
    return 0;
}

int medulla_test_set_protection(medulla_t medulla, protection_level_t level) {
    if (!is_valid_medulla(medulla)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "medulla_test_set_protection: is_valid_medulla is NULL");
        return -1;
    }

    if (level < PROTECTION_LEVEL_NORMAL || level > PROTECTION_LEVEL_SHUTDOWN) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "medulla_test_set_protection: validation failed");
        return -1;
    }

    nimcp_platform_mutex_lock(medulla->mutex);
    medulla->protection_level = level;
    nimcp_platform_mutex_unlock(medulla->mutex);
    return 0;
}

int medulla_test_set_circadian(medulla_t medulla, circadian_phase_t phase) {
    if (!is_valid_medulla(medulla)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "medulla_test_set_circadian: is_valid_medulla is NULL");
        return -1;
    }

    if (phase < CIRCADIAN_PHASE_EARLY_MORNING || phase > CIRCADIAN_PHASE_PRE_DAWN) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "medulla_test_set_circadian: validation failed");
        return -1;
    }

    /* Map phase to circadian time (middle of each phase's range) */
    float phase_time;
    switch (phase) {
        case CIRCADIAN_PHASE_EARLY_MORNING: phase_time = 7.5f; break;   /* [6,9) */
        case CIRCADIAN_PHASE_MORNING:       phase_time = 10.5f; break;  /* [9,12) */
        case CIRCADIAN_PHASE_AFTERNOON:     phase_time = 13.5f; break;  /* [12,15) */
        case CIRCADIAN_PHASE_EVENING:       phase_time = 16.5f; break;  /* [15,18) */
        case CIRCADIAN_PHASE_LATE_EVENING:  phase_time = 19.5f; break;  /* [18,21) */
        case CIRCADIAN_PHASE_NIGHT:         phase_time = 22.5f; break;  /* [21,24) */
        case CIRCADIAN_PHASE_DEEP_NIGHT:    phase_time = 1.5f; break;   /* [0,3) */
        case CIRCADIAN_PHASE_PRE_DAWN:      phase_time = 4.5f; break;   /* [3,6) */
        default: phase_time = 10.5f; break;
    }

    nimcp_platform_mutex_lock(medulla->mutex);
    medulla->circadian_phase = phase;
    medulla->current_circadian_time = phase_time;
    medulla->stats.circadian_phase = phase;
    nimcp_platform_mutex_unlock(medulla->mutex);
    return 0;
}
