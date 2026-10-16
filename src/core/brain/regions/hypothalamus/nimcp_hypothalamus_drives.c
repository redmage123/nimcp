/**
 * @file nimcp_hypothalamus_drives.c
 * @brief Implementation of Hypothalamus Drive System with Alignment Safety
 *
 * WHAT: Drive state management and alignment-safe setpoint control
 * WHY:  Implements Steve Byrnes' "Steering Subsystem" concept for AGI safety
 * HOW:  Drives as reward function parameters with explicit alignment weights
 *
 * BYRNES' KEY INSIGHT:
 * The hypothalamus (steering subsystem ~10% of brain) sends reward signals that
 * steer the learning subsystem (~90%). Careful design of this reward function
 * is a key lever for AGI alignment.
 *
 * @version Phase 1: Core Drive System + Alignment Safety
 * @date 2026-01-04
 */

#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_drives.h"
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

BRIDGE_BOILERPLATE_MESH_ONLY(hypothalamus_drives, MESH_ADAPTER_CATEGORY_COGNITIVE)


/*=============================================================================
 * LOGGING MODULE IDENTIFIER
 *===========================================================================*/

#define DRIVE_LOG_MODULE "HYPO_DRIVE"

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

#define US_PER_SECOND (1000000ULL)
#define US_PER_MINUTE (60000000ULL)
#define US_PER_HOUR   (3600000000ULL)

/* Default drive parameters */
#define DEFAULT_DRIVE_RISE_RATE     0.0001f   /* Per microsecond */
#define DEFAULT_DRIVE_DECAY_RATE    0.001f    /* Per microsecond when satisfied */
#define DEFAULT_DRIVE_BASELINE      0.1f
#define DEFAULT_URGENCY_THRESHOLD   0.5f
#define DEFAULT_PRIORITY_THRESHOLD  0.7f

/* Alignment safety defaults (Byrnes' recommended values) */
#define DEFAULT_HUMAN_WELLBEING_WEIGHT  1.0f
#define DEFAULT_HARM_AVOIDANCE_WEIGHT   1.0f
#define DEFAULT_HONESTY_WEIGHT          0.9f
#define DEFAULT_HELPFULNESS_WEIGHT      0.8f

/* Unlock key for soft-locked setpoints (Byrnes safety key) */
#define SETPOINT_UNLOCK_KEY 0xB4F355AFE5ULL

/*=============================================================================
 * INTERNAL STRUCTURE
 *===========================================================================*/

struct hypo_drive_system {
    /* Configuration */
    hypo_drive_config_t config;

    /* Drive states */
    hypo_drive_state_t drives[HYPO_DRIVE_COUNT];

    /* Nucleus states */
    hypo_nucleus_state_t nuclei[HYPO_NUCLEUS_COUNT];

    /* Global modulation */
    float global_drive_gain;
    float arousal_level;

    /* Priority tracking */
    hypo_drive_type_t highest_priority;
    float priority_threshold;

    /* Reward computation */
    hypo_reward_signal_t current_reward;
    float reward_accumulator;

    /* Statistics */
    hypo_drive_stats_t stats;

    /* Timing */
    uint64_t creation_time_us;
    uint64_t last_update_us;
    uint64_t total_runtime_us;

    /* Thread safety */
    nimcp_mutex_t* mutex;
    bool mutex_owned;
};

/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

/**
 * @brief Simple exponential decay toward target
 */
static float exponential_decay(float current, float target, float rate, float dt) {
    return current + (target - current) * (1.0f - expf(-rate * dt));
}

/**
 * @brief Get nucleus for a drive type
 */
static hypo_nucleus_type_t drive_to_nucleus(hypo_drive_type_t drive) {
    switch (drive) {
        case HYPO_DRIVE_HUNGER:
            return HYPO_NUCLEUS_LATERAL;          /* Lateral for hunger */
        case HYPO_DRIVE_THIRST:
            return HYPO_NUCLEUS_SUPRAOPTIC;       /* Supraoptic for thirst */
        case HYPO_DRIVE_TEMPERATURE:
            return HYPO_NUCLEUS_ANTERIOR;         /* Anterior for cooling */
        case HYPO_DRIVE_FATIGUE:
            return HYPO_NUCLEUS_PREOPTIC;         /* Preoptic for sleep */
        case HYPO_DRIVE_SOCIAL:
            return HYPO_NUCLEUS_PARAVENTRICULAR;  /* PVN for oxytocin/social */
        case HYPO_DRIVE_CURIOSITY:
            return HYPO_NUCLEUS_LATERAL;          /* Lateral for arousal/seeking */
        case HYPO_DRIVE_SAFETY:
            return HYPO_NUCLEUS_PARAVENTRICULAR;  /* PVN for stress/CRH */
        case HYPO_DRIVE_AUTONOMY:
            return HYPO_NUCLEUS_POSTERIOR;        /* Posterior for arousal */
        case HYPO_DRIVE_COMPETENCE:
            return HYPO_NUCLEUS_ARCUATE;          /* Arcuate for motivation */
        default:
            return HYPO_NUCLEUS_LATERAL;
    }
}

/**
 * @brief Initialize a single drive state
 */
static void init_drive_state(hypo_drive_state_t* drive,
                              hypo_drive_type_t type,
                              float setpoint) {
    memset(drive, 0, sizeof(*drive));

    drive->type = type;
    drive->level = 0.0f;
    drive->urgency = 0.0f;
    drive->satisfaction = 0.5f;
    drive->setpoint = setpoint;
    drive->deviation = 0.0f;
    drive->rise_rate = DEFAULT_DRIVE_RISE_RATE;
    drive->decay_rate = DEFAULT_DRIVE_DECAY_RATE;
    drive->baseline = DEFAULT_DRIVE_BASELINE;
    drive->last_satisfied_us = 0;
    drive->time_since_satisfied = 0;
    drive->active = false;
    drive->suppressed = false;
}

/**
 * @brief Initialize a single nucleus state
 */
static void init_nucleus_state(hypo_nucleus_state_t* nucleus,
                                hypo_nucleus_type_t type) {
    nucleus->type = type;
    nucleus->activity = 0.0f;
    nucleus->output_signal = 0.0f;
    nucleus->enabled = true;
}

/**
 * @brief Log alignment modification attempt (CRITICAL FOR SAFETY)
 */
static void log_alignment_access(hypo_drive_system_handle_t* system,
                                  const char* operation,
                                  const char* target,
                                  uint32_t modifier_id,
                                  bool success) {
    if (!system) return;

    system->stats.setpoint_access_attempts++;

    if (!success) {
        system->stats.setpoint_access_denied++;
    }

    /* Always log alignment-related access */
    if (success) {
        LOG_WARNING(DRIVE_LOG_MODULE,
                    "ALIGNMENT ACCESS: op=%s, target=%s, modifier=%u, result=ALLOWED",
                    operation, target, modifier_id);
    } else {
        LOG_WARNING(DRIVE_LOG_MODULE,
                    "ALIGNMENT ACCESS DENIED: op=%s, target=%s, modifier=%u, result=BLOCKED",
                    operation, target, modifier_id);
    }
}

/**
 * @brief Check if setpoint modification is permitted
 */
static bool can_modify_setpoint(const hypo_drive_system_handle_t* system) {
    if (!system) {
        return false;
    }

    hypo_lock_state_t lock = system->config.setpoints.setpoints_lock;
    return lock == HYPO_LOCK_UNLOCKED;
}

/**
 * @brief Check if alignment weight modification is permitted
 */
static bool can_modify_alignment(const hypo_drive_system_handle_t* system) {
    if (!system) {
        return false;
    }

    hypo_lock_state_t lock = system->config.setpoints.alignment_lock;
    return lock == HYPO_LOCK_UNLOCKED;
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

hypo_drive_config_t hypo_drive_default_config(void) {
    hypo_drive_config_t config;
    memset(&config, 0, sizeof(config));

    /* CRITICAL: Default to CONTROLLED mode for safety */
    config.alignment_mode = HYPO_ALIGN_CONTROLLED;

    /* Physiological setpoints */
    config.setpoints.temperature_setpoint = 37.0f;      /* 37.0 C */
    config.setpoints.glucose_setpoint = 90.0f;          /* 90 mg/dL */
    config.setpoints.osmolarity_setpoint = 285.0f;      /* 285 mOsm/L */
    config.setpoints.sleep_pressure_setpoint = 0.7f;

    /* Psychological setpoints */
    config.setpoints.social_setpoint = 0.5f;
    config.setpoints.curiosity_setpoint = 0.6f;
    config.setpoints.safety_setpoint = 0.8f;
    config.setpoints.autonomy_setpoint = 0.7f;
    config.setpoints.competence_setpoint = 0.6f;

    /* ALIGNMENT WEIGHTS (Byrnes' key insight) */
    config.setpoints.human_wellbeing_weight = DEFAULT_HUMAN_WELLBEING_WEIGHT;
    config.setpoints.harm_avoidance_weight = DEFAULT_HARM_AVOIDANCE_WEIGHT;
    config.setpoints.honesty_weight = DEFAULT_HONESTY_WEIGHT;
    config.setpoints.helpfulness_weight = DEFAULT_HELPFULNESS_WEIGHT;

    /* Control parameters */
    config.setpoints.reward_gain = 1.0f;
    config.setpoints.punishment_gain = 1.0f;
    config.setpoints.temporal_discount = NIMCP_REWARD_DISCOUNT_DEFAULT;

    /* CRITICAL: Lock alignment weights by default */
    config.setpoints.setpoints_lock = HYPO_LOCK_SOFT;   /* Physiological can be unlocked */
    config.setpoints.alignment_lock = HYPO_LOCK_HARD;   /* Alignment HARD locked */

    /* Audit trail initialization */
    config.setpoints.modification_count = 0;
    config.setpoints.last_modified_us = 0;
    config.setpoints.modifier_id = 0;

    /* Drive parameters */
    config.drive_update_rate_hz = 60.0f;
    config.urgency_threshold = DEFAULT_URGENCY_THRESHOLD;
    config.conflict_resolution_tau = 0.1f;

    /* Reward computation */
    config.reward_smoothing = 0.9f;
    config.enable_alignment_bonus = true;

    /* Platform tier */
    config.min_tier = PLATFORM_TIER_MEDIUM;

    /* Safety features - enabled by default */
    config.enable_setpoint_logging = true;
    config.enable_alignment_alerts = true;

    return config;
}

hypo_drive_system_handle_t* hypo_drive_create(const hypo_drive_config_t* config) {
    LOG_INFO(DRIVE_LOG_MODULE, "Creating hypothalamus drive system");

    hypo_drive_system_handle_t* system = (hypo_drive_system_handle_t*)nimcp_calloc(
        1, sizeof(hypo_drive_system_handle_t));
    if (!system) {
        LOG_ERROR(DRIVE_LOG_MODULE, "Failed to allocate drive system memory");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hypo_drive_create: system is NULL");
        return NULL;
    }

    /* Set configuration */
    if (config) {
        system->config = *config;
        LOG_DEBUG(DRIVE_LOG_MODULE, "Using provided configuration");
    } else {
        system->config = hypo_drive_default_config();
        LOG_DEBUG(DRIVE_LOG_MODULE, "Using default configuration (alignment HARD locked)");
    }

    /* Create mutex for thread safety */
    mutex_attr_t mutex_attr = {
        .type = MUTEX_TYPE_RECURSIVE  /* Allow recursive locking */
    };
    system->mutex = nimcp_mutex_create(&mutex_attr);
    if (system->mutex) {
        system->mutex_owned = true;
    } else {
        LOG_WARNING(DRIVE_LOG_MODULE, "Failed to create mutex, running without thread safety");
    }

    /* Initialize drive states */
    init_drive_state(&system->drives[HYPO_DRIVE_HUNGER], HYPO_DRIVE_HUNGER, 0.3f);
    init_drive_state(&system->drives[HYPO_DRIVE_THIRST], HYPO_DRIVE_THIRST, 0.3f);
    init_drive_state(&system->drives[HYPO_DRIVE_TEMPERATURE], HYPO_DRIVE_TEMPERATURE,
                     system->config.setpoints.temperature_setpoint);
    init_drive_state(&system->drives[HYPO_DRIVE_FATIGUE], HYPO_DRIVE_FATIGUE,
                     system->config.setpoints.sleep_pressure_setpoint);
    init_drive_state(&system->drives[HYPO_DRIVE_SOCIAL], HYPO_DRIVE_SOCIAL,
                     system->config.setpoints.social_setpoint);
    init_drive_state(&system->drives[HYPO_DRIVE_CURIOSITY], HYPO_DRIVE_CURIOSITY,
                     system->config.setpoints.curiosity_setpoint);
    init_drive_state(&system->drives[HYPO_DRIVE_SAFETY], HYPO_DRIVE_SAFETY,
                     system->config.setpoints.safety_setpoint);
    init_drive_state(&system->drives[HYPO_DRIVE_AUTONOMY], HYPO_DRIVE_AUTONOMY,
                     system->config.setpoints.autonomy_setpoint);
    init_drive_state(&system->drives[HYPO_DRIVE_COMPETENCE], HYPO_DRIVE_COMPETENCE,
                     system->config.setpoints.competence_setpoint);

    /* Initialize nucleus states */
    for (int i = 0; i < HYPO_NUCLEUS_COUNT; i++) {
        init_nucleus_state(&system->nuclei[i], (hypo_nucleus_type_t)i);
    }

    /* Initialize global modulation */
    system->global_drive_gain = 1.0f;
    system->arousal_level = 0.5f;
    system->highest_priority = HYPO_DRIVE_SAFETY;  /* Safety first */
    system->priority_threshold = DEFAULT_PRIORITY_THRESHOLD;

    /* Initialize reward computation */
    memset(&system->current_reward, 0, sizeof(system->current_reward));
    system->reward_accumulator = 0.0f;

    /* Initialize statistics */
    memset(&system->stats, 0, sizeof(system->stats));

    /* Log alignment configuration for audit */
    LOG_INFO(DRIVE_LOG_MODULE,
             "Alignment config: mode=%s, lock=%s, wellbeing=%.2f, harm_avoid=%.2f",
             hypo_alignment_mode_string(system->config.alignment_mode),
             hypo_lock_state_string(system->config.setpoints.alignment_lock),
             system->config.setpoints.human_wellbeing_weight,
             system->config.setpoints.harm_avoidance_weight);

    LOG_INFO(DRIVE_LOG_MODULE, "Hypothalamus drive system created successfully");
    return system;
}

void hypo_drive_destroy(hypo_drive_system_handle_t* system) {
    if (!system) return;

    LOG_INFO(DRIVE_LOG_MODULE, "Destroying hypothalamus drive system");

    /* Log final stats for audit */
    LOG_INFO(DRIVE_LOG_MODULE,
             "Final stats: updates=%lu, violations=%lu, access_denied=%lu",
             (unsigned long)system->stats.updates_processed,
             (unsigned long)system->stats.alignment_violations,
             (unsigned long)system->stats.setpoint_access_denied);

    /* Destroy mutex if owned */
    if (system->mutex && system->mutex_owned) {
        nimcp_mutex_destroy(system->mutex);
        system->mutex = NULL;
    }

    LOG_DEBUG(DRIVE_LOG_MODULE, "Drive system destroyed");
    nimcp_free(system);
}

bool hypo_drive_reset(hypo_drive_system_handle_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_drive_reset: system is NULL");
        return false;
    }

    LOG_DEBUG(DRIVE_LOG_MODULE, "Resetting drive system state");

    if (system->mutex) {
        nimcp_mutex_lock(system->mutex);
    }

    /* Reset drive states (preserve setpoints - those are locked) */
    for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
        system->drives[i].level = 0.0f;
        system->drives[i].urgency = 0.0f;
        system->drives[i].satisfaction = 0.5f;
        system->drives[i].deviation = 0.0f;
        system->drives[i].last_satisfied_us = 0;
        system->drives[i].time_since_satisfied = 0;
        system->drives[i].active = false;
        system->drives[i].suppressed = false;
        /* NOTE: setpoint, rise_rate, decay_rate, baseline preserved */
    }

    /* Reset nucleus states */
    for (int i = 0; i < HYPO_NUCLEUS_COUNT; i++) {
        system->nuclei[i].activity = 0.0f;
        system->nuclei[i].output_signal = 0.0f;
        /* NOTE: enabled state preserved */
    }

    /* Reset global modulation */
    system->global_drive_gain = 1.0f;
    system->arousal_level = 0.5f;
    system->highest_priority = HYPO_DRIVE_SAFETY;

    /* Reset reward computation */
    memset(&system->current_reward, 0, sizeof(system->current_reward));
    system->reward_accumulator = 0.0f;

    /* Reset timing (preserve creation time) */
    system->last_update_us = 0;
    system->total_runtime_us = 0;

    /* NOTE: Configuration (including alignment weights and locks) is NOT reset */
    /* This is intentional - alignment safety is preserved across resets */

    if (system->mutex) {
        nimcp_mutex_unlock(system->mutex);
    }

    LOG_DEBUG(DRIVE_LOG_MODULE,
              "Drive system reset complete (alignment config preserved)");
    return true;
}

/*=============================================================================
 * DRIVE STATE ACCESS
 *===========================================================================*/

bool hypo_drive_update(hypo_drive_system_handle_t* system, uint64_t delta_time_us) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_drive_update: system is NULL");
        return false;
    }

    if (system->mutex) {
        nimcp_mutex_lock(system->mutex);
    }

    float dt_seconds = (float)delta_time_us / (float)US_PER_SECOND;

    /* Update each drive */
    for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
        hypo_drive_state_t* drive = &system->drives[i];

        if (drive->suppressed) continue;

        /* Drive level naturally increases over time (hunger grows) */
        float target = 1.0f;  /* Drives tend toward maximum unless satisfied */
        drive->level = exponential_decay(drive->level, target,
                                          drive->rise_rate * 1000000.0f, dt_seconds);
        drive->level = nimcp_clamp01(drive->level);

        /* Update time since satisfied */
        drive->time_since_satisfied += delta_time_us;

        /* Compute deviation from setpoint */
        drive->deviation = drive->level - drive->setpoint;

        /* Compute urgency (nonlinear - more urgent when further from setpoint) */
        float abs_deviation = fabsf(drive->deviation);
        drive->urgency = nimcp_clamp01(abs_deviation * abs_deviation * 4.0f);

        /* Apply global gain and arousal modulation */
        drive->urgency *= system->global_drive_gain;
        drive->urgency *= (0.5f + 0.5f * system->arousal_level);

        /* Drive is active if urgency exceeds threshold */
        bool was_active = drive->active;
        drive->active = drive->urgency > system->config.urgency_threshold;

        /* Track activation */
        if (drive->active && !was_active) {
            system->stats.drive_activations[i]++;
        }

        /* Update associated nucleus */
        hypo_nucleus_type_t nucleus_idx = drive_to_nucleus((hypo_drive_type_t)i);
        system->nuclei[nucleus_idx].activity =
            fmaxf(system->nuclei[nucleus_idx].activity, drive->urgency);
    }

    /* Update nucleus output signals */
    for (int i = 0; i < HYPO_NUCLEUS_COUNT; i++) {
        hypo_nucleus_state_t* nucleus = &system->nuclei[i];
        if (!nucleus->enabled) {
            nucleus->output_signal = 0.0f;
            continue;
        }

        /* Simple linear transfer function */
        nucleus->output_signal = nimcp_clamp01(nucleus->activity);

        /* Decay activity for next cycle */
        nucleus->activity = exponential_decay(nucleus->activity, 0.0f, 0.1f, dt_seconds);
    }

    /* Determine highest priority drive */
    float max_urgency = 0.0f;
    hypo_drive_type_t new_priority = system->highest_priority;

    for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
        if (system->drives[i].urgency > max_urgency) {
            max_urgency = system->drives[i].urgency;
            new_priority = (hypo_drive_type_t)i;
        }
    }

    /* Only switch priority if difference exceeds threshold (hysteresis) */
    if (max_urgency > system->drives[system->highest_priority].urgency +
                      system->priority_threshold * 0.2f) {
        if (new_priority != system->highest_priority) {
            system->stats.priority_switches++;

            /* Track drive conflicts when multiple drives are competing */
            int active_count = 0;
            for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
                if (system->drives[i].active) active_count++;
            }
            if (active_count > 1) {
                system->stats.drive_conflicts++;
            }
        }
        system->highest_priority = new_priority;
    }

    /* Update timing */
    system->last_update_us += delta_time_us;
    system->total_runtime_us += delta_time_us;
    system->stats.updates_processed++;

    if (system->mutex) {
        nimcp_mutex_unlock(system->mutex);
    }

    return true;
}

bool hypo_drive_get_state(const hypo_drive_system_handle_t* system,
                          hypo_drive_type_t drive_type,
                          hypo_drive_state_t* state) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_drive_get_state: system is NULL");
        return false;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_drive_get_state: state is NULL");
        return false;
    }
    if (drive_type >= HYPO_DRIVE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_drive_get_state: invalid drive_type");
        return false;
    }

    *state = system->drives[drive_type];
    return true;
}

bool hypo_drive_get_system_state(const hypo_drive_system_handle_t* system,
                                  hypo_drive_system_t* state) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_drive_get_system_state: system is NULL");
        return false;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_drive_get_system_state: state is NULL");
        return false;
    }

    memcpy(state->drives, system->drives, sizeof(state->drives));
    state->global_drive_gain = system->global_drive_gain;
    state->arousal_level = system->arousal_level;
    state->highest_priority = system->highest_priority;
    state->priority_threshold = system->priority_threshold;
    state->total_satisfactions = 0;
    state->drive_conflicts = system->stats.drive_conflicts;

    for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
        state->total_satisfactions += system->stats.drive_satisfactions[i];
    }

    return true;
}

float hypo_drive_satisfy(hypo_drive_system_handle_t* system,
                         hypo_drive_type_t drive_type,
                         float satisfaction_level) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_drive_satisfy: system is NULL");
        return 0.0f;
    }
    if (drive_type >= HYPO_DRIVE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_drive_satisfy: invalid drive_type");
        return 0.0f;
    }

    if (system->mutex) {
        nimcp_mutex_lock(system->mutex);
    }

    satisfaction_level = nimcp_clamp01(satisfaction_level);
    hypo_drive_state_t* drive = &system->drives[drive_type];

    /* Calculate reward based on drive reduction */
    float drive_before = drive->level;

    /* Apply satisfaction - drive level decreases */
    float target = drive->baseline;
    drive->level = exponential_decay(drive->level, target,
                                      drive->decay_rate * satisfaction_level, 1.0f);

    /* Update satisfaction and timing */
    drive->satisfaction = satisfaction_level;
    drive->last_satisfied_us = system->last_update_us;
    drive->time_since_satisfied = 0;

    /* Compute reward signal */
    float drive_reduction = drive_before - drive->level;
    float reward = drive_reduction * system->config.setpoints.reward_gain;

    /* Apply alignment bonus if enabled */
    if (system->config.enable_alignment_bonus) {
        /* Bonus for satisfying drives in an aligned way */
        float alignment_factor =
            (system->config.setpoints.human_wellbeing_weight +
             system->config.setpoints.helpfulness_weight) / 2.0f;
        reward *= (0.5f + 0.5f * alignment_factor);
    }

    /* Update statistics */
    system->stats.drive_satisfactions[drive_type]++;

    /* Update reward accumulator */
    system->reward_accumulator = exponential_decay(
        system->reward_accumulator, reward,
        1.0f - system->config.reward_smoothing, 1.0f);

    if (system->mutex) {
        nimcp_mutex_unlock(system->mutex);
    }

    return reward;
}

hypo_drive_type_t hypo_drive_get_priority(const hypo_drive_system_handle_t* system) {
    if (!system) return HYPO_DRIVE_SAFETY;  /* Default to safety */
    return system->highest_priority;
}

bool hypo_drive_get_urgencies(const hypo_drive_system_handle_t* system,
                               float* urgencies) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_drive_get_urgencies: system is NULL");
        return false;
    }
    if (!urgencies) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_drive_get_urgencies: urgencies is NULL");
        return false;
    }

    for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
        urgencies[i] = system->drives[i].urgency;
    }

    return true;
}

/*=============================================================================
 * REWARD COMPUTATION
 *===========================================================================*/

bool hypo_drive_compute_reward(const hypo_drive_system_handle_t* system,
                                hypo_reward_signal_t* signal) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_drive_compute_reward: system is NULL");
        return false;
    }
    if (!signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_drive_compute_reward: signal is NULL");
        return false;
    }

    memset(signal, 0, sizeof(*signal));

    /* Compute drive satisfaction component */
    float drive_reward = 0.0f;
    for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
        /* Reward for being near setpoint, punishment for deviation */
        float deviation = fabsf(system->drives[i].deviation);
        drive_reward -= deviation * 0.2f;  /* Penalty for deviation */

        /* Bonus for satisfied drives */
        if (system->drives[i].satisfaction > 0.5f) {
            drive_reward += (system->drives[i].satisfaction - 0.5f) * 0.3f;
        }
    }
    signal->drive_satisfaction = nimcp_clampf(drive_reward, -1.0f, 1.0f);

    /* Compute alignment components */
    const hypo_setpoint_config_t* sp = &system->config.setpoints;

    /* Alignment bonus based on configuration */
    signal->alignment_bonus = 0.0f;
    if (system->config.enable_alignment_bonus) {
        /* Reward for being in aligned state */
        signal->alignment_bonus = nimcp_clamp01(
            (sp->human_wellbeing_weight * 0.3f +
             sp->harm_avoidance_weight * 0.3f +
             sp->honesty_weight * 0.2f +
             sp->helpfulness_weight * 0.2f) * 0.1f);
    }

    /* Alignment penalty would be computed if misalignment detected */
    /* (Requires external context about actual behavior) */
    signal->alignment_penalty = 0.0f;

    /* Combine components */
    signal->immediate_reward = signal->drive_satisfaction + signal->alignment_bonus;
    signal->anticipated_reward = signal->immediate_reward * sp->temporal_discount;

    /* Net reward signal */
    signal->reward_signal = nimcp_clampf(
        signal->immediate_reward - signal->alignment_penalty, -1.0f, 1.0f);

    /* Prediction error (simple model - actual minus expected) */
    signal->prediction_error = signal->reward_signal - system->reward_accumulator;

    /* Convert to dopamine level (reward → dopamine is nonlinear) */
    if (signal->reward_signal >= 0) {
        signal->dopamine_level = nimcp_clamp01(0.5f + signal->reward_signal * 0.5f);
    } else {
        signal->dopamine_level = nimcp_clamp01(0.5f + signal->reward_signal * 0.3f);
    }

    /* Learning rate modulation based on reward magnitude */
    signal->learning_rate_mod = nimcp_clamp01(0.5f + fabsf(signal->prediction_error) * 0.5f);

    return true;
}

float hypo_drive_get_reward(const hypo_drive_system_handle_t* system) {
    if (!system) return 0.0f;
    return system->reward_accumulator;
}

/*=============================================================================
 * SETPOINT ACCESS (ALIGNMENT-CRITICAL)
 *===========================================================================*/

bool hypo_drive_get_setpoints(const hypo_drive_system_handle_t* system,
                               hypo_setpoint_config_t* config) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_drive_get_setpoints: system is NULL");
        return false;
    }
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_drive_get_setpoints: config is NULL");
        return false;
    }

    *config = system->config.setpoints;
    return true;
}

bool hypo_drive_modify_setpoint(hypo_drive_system_handle_t* system,
                                 hypo_drive_type_t drive_type,
                                 float new_setpoint,
                                 uint32_t modifier_id) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_drive_modify_setpoint: system is NULL");
        return false;
    }
    if (drive_type >= HYPO_DRIVE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_drive_modify_setpoint: invalid drive_type");
        return false;
    }

    bool success = false;

    if (system->mutex) {
        nimcp_mutex_lock(system->mutex);
    }

    /* Check if modification is permitted */
    if (can_modify_setpoint(system)) {
        /* Clamp to valid range */
        new_setpoint = nimcp_clamp01(new_setpoint);

        /* Update drive setpoint */
        system->drives[drive_type].setpoint = new_setpoint;

        /* Update audit trail */
        system->config.setpoints.modification_count++;
        system->config.setpoints.last_modified_us = system->last_update_us;
        system->config.setpoints.modifier_id = modifier_id;

        success = true;

        LOG_DEBUG(DRIVE_LOG_MODULE,
                  "Setpoint modified: drive=%s, value=%.2f, modifier=%u",
                  hypo_drive_type_string(drive_type), new_setpoint, modifier_id);
    }

    /* Log access attempt */
    if (system->config.enable_setpoint_logging) {
        log_alignment_access(system, "MODIFY_SETPOINT",
                             hypo_drive_type_string(drive_type),
                             modifier_id, success);
    }

    if (system->mutex) {
        nimcp_mutex_unlock(system->mutex);
    }

    return success;
}

bool hypo_drive_modify_alignment_weight(hypo_drive_system_handle_t* system,
                                         const char* weight_name,
                                         float new_weight,
                                         uint32_t modifier_id) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_drive_modify_alignment_weight: system is NULL");
        return false;
    }
    if (!weight_name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_drive_modify_alignment_weight: weight_name is NULL");
        return false;
    }

    bool success = false;

    if (system->mutex) {
        nimcp_mutex_lock(system->mutex);
    }

    /* THIS IS HIGHLY RESTRICTED - alignment weights should almost never change */
    if (can_modify_alignment(system)) {
        new_weight = nimcp_clamp01(new_weight);

        /* Find and update the weight */
        if (strcmp(weight_name, "human_wellbeing") == 0) {
            system->config.setpoints.human_wellbeing_weight = new_weight;
            success = true;
        } else if (strcmp(weight_name, "harm_avoidance") == 0) {
            system->config.setpoints.harm_avoidance_weight = new_weight;
            success = true;
        } else if (strcmp(weight_name, "honesty") == 0) {
            system->config.setpoints.honesty_weight = new_weight;
            success = true;
        } else if (strcmp(weight_name, "helpfulness") == 0) {
            system->config.setpoints.helpfulness_weight = new_weight;
            success = true;
        }

        if (success) {
            system->config.setpoints.modification_count++;
            system->config.setpoints.last_modified_us = system->last_update_us;
            system->config.setpoints.modifier_id = modifier_id;

            LOG_WARNING(DRIVE_LOG_MODULE,
                        "ALIGNMENT WEIGHT MODIFIED: %s=%.2f by modifier=%u",
                        weight_name, new_weight, modifier_id);
        }
    } else {
        /* Alignment is locked - generate alert */
        if (system->config.enable_alignment_alerts) {
            system->stats.alignment_violations++;
            LOG_ERROR(DRIVE_LOG_MODULE,
                      "ALIGNMENT VIOLATION: Attempted modification of %s "
                      "while LOCKED (modifier=%u)",
                      weight_name, modifier_id);
        }
    }

    /* Always log alignment access attempts */
    log_alignment_access(system, "MODIFY_ALIGNMENT_WEIGHT",
                         weight_name, modifier_id, success);

    if (system->mutex) {
        nimcp_mutex_unlock(system->mutex);
    }

    return success;
}

/*=============================================================================
 * LOCK MANAGEMENT (ALIGNMENT SAFETY)
 *===========================================================================*/

bool hypo_drive_lock_setpoints(hypo_drive_system_handle_t* system,
                                hypo_lock_state_t lock_state) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_drive_lock_setpoints: system is NULL");
        return false;
    }

    if (system->mutex) {
        nimcp_mutex_lock(system->mutex);
    }

    hypo_lock_state_t current = system->config.setpoints.setpoints_lock;

    /* Cannot downgrade from HARD lock */
    if (current == HYPO_LOCK_HARD && lock_state != HYPO_LOCK_HARD) {
        LOG_WARNING(DRIVE_LOG_MODULE,
                    "Cannot downgrade setpoint lock from HARD");
        if (system->mutex) {
            nimcp_mutex_unlock(system->mutex);
        }
        return false;
    }

    system->config.setpoints.setpoints_lock = lock_state;
    LOG_INFO(DRIVE_LOG_MODULE, "Setpoint lock changed to %s",
             hypo_lock_state_string(lock_state));

    if (system->mutex) {
        nimcp_mutex_unlock(system->mutex);
    }

    return true;
}

bool hypo_drive_lock_alignment(hypo_drive_system_handle_t* system,
                                hypo_lock_state_t lock_state) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_drive_lock_alignment: system is NULL");
        return false;
    }

    if (system->mutex) {
        nimcp_mutex_lock(system->mutex);
    }

    hypo_lock_state_t current = system->config.setpoints.alignment_lock;

    /* Cannot downgrade from HARD lock - this is critical for safety */
    if (current == HYPO_LOCK_HARD && lock_state != HYPO_LOCK_HARD) {
        LOG_ERROR(DRIVE_LOG_MODULE,
                  "SECURITY: Cannot downgrade alignment lock from HARD");
        system->stats.alignment_violations++;
        if (system->mutex) {
            nimcp_mutex_unlock(system->mutex);
        }
        return false;
    }

    system->config.setpoints.alignment_lock = lock_state;
    LOG_WARNING(DRIVE_LOG_MODULE, "ALIGNMENT LOCK changed to %s",
                hypo_lock_state_string(lock_state));

    if (system->mutex) {
        nimcp_mutex_unlock(system->mutex);
    }

    return true;
}

bool hypo_drive_unlock_setpoints(hypo_drive_system_handle_t* system,
                                  uint64_t unlock_key) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_drive_unlock_setpoints: system is NULL");
        return false;
    }

    if (system->mutex) {
        nimcp_mutex_lock(system->mutex);
    }

    hypo_lock_state_t current = system->config.setpoints.setpoints_lock;

    /* Cannot unlock HARD locks */
    if (current == HYPO_LOCK_HARD) {
        LOG_WARNING(DRIVE_LOG_MODULE,
                    "Cannot unlock HARD locked setpoints");
        if (system->mutex) {
            nimcp_mutex_unlock(system->mutex);
        }
        return false;
    }

    /* Verify unlock key for SOFT locks */
    if (current == HYPO_LOCK_SOFT) {
        if (unlock_key != SETPOINT_UNLOCK_KEY) {
            LOG_WARNING(DRIVE_LOG_MODULE,
                        "Invalid unlock key for setpoints");
            system->stats.setpoint_access_denied++;
            if (system->mutex) {
                nimcp_mutex_unlock(system->mutex);
            }
            return false;
        }
    }

    system->config.setpoints.setpoints_lock = HYPO_LOCK_UNLOCKED;
    LOG_INFO(DRIVE_LOG_MODULE, "Setpoints unlocked");

    if (system->mutex) {
        nimcp_mutex_unlock(system->mutex);
    }

    return true;
}

hypo_lock_state_t hypo_drive_get_alignment_lock_state(
    const hypo_drive_system_handle_t* system) {
    if (!system) return HYPO_LOCK_HARD;  /* Default to safest state */
    return system->config.setpoints.alignment_lock;
}

/*=============================================================================
 * ALIGNMENT MODE
 *===========================================================================*/

hypo_alignment_mode_t hypo_drive_get_alignment_mode(
    const hypo_drive_system_handle_t* system) {
    if (!system) return HYPO_ALIGN_CONTROLLED;  /* Default to safest mode */
    return system->config.alignment_mode;
}

bool hypo_drive_check_alignment(const hypo_drive_system_handle_t* system,
                                 float* alignment_score) {
    if (!system) {
        if (alignment_score) *alignment_score = 0.0f;
        return false;
    }

    /* Compute alignment score based on weight configuration */
    const hypo_setpoint_config_t* sp = &system->config.setpoints;

    float score = 0.0f;
    score += sp->human_wellbeing_weight * 0.3f;
    score += sp->harm_avoidance_weight * 0.3f;
    score += sp->honesty_weight * 0.2f;
    score += sp->helpfulness_weight * 0.2f;

    score = nimcp_clamp01(score);

    if (alignment_score) {
        *alignment_score = score;
    }

    /* Check if alignment is within acceptable bounds */
    bool aligned = (score >= 0.7f);

    /* Track alignment checks */
    ((hypo_drive_system_handle_t*)system)->stats.alignment_checks++;

    if (!aligned) {
        ((hypo_drive_system_handle_t*)system)->stats.alignment_violations++;
        LOG_WARNING(DRIVE_LOG_MODULE,
                    "Alignment check: score=%.2f (below threshold)", score);
    }

    return aligned;
}

/*=============================================================================
 * NUCLEUS CONTROL
 *===========================================================================*/

float hypo_drive_get_nucleus_activity(const hypo_drive_system_handle_t* system,
                                       hypo_nucleus_type_t nucleus) {
    if (!system || nucleus >= HYPO_NUCLEUS_COUNT) return 0.0f;
    return system->nuclei[nucleus].activity;
}

float hypo_drive_set_nucleus_input(hypo_drive_system_handle_t* system,
                                    hypo_nucleus_type_t nucleus,
                                    float input) {
    if (!system || nucleus >= HYPO_NUCLEUS_COUNT) return 0.0f;

    if (system->mutex) {
        nimcp_mutex_lock(system->mutex);
    }

    input = nimcp_clamp01(input);

    /* Set nucleus activity */
    system->nuclei[nucleus].activity =
        fmaxf(system->nuclei[nucleus].activity, input);

    /* Compute output signal */
    float output = 0.0f;
    if (system->nuclei[nucleus].enabled) {
        output = nimcp_clamp01(system->nuclei[nucleus].activity);
        system->nuclei[nucleus].output_signal = output;
    }

    if (system->mutex) {
        nimcp_mutex_unlock(system->mutex);
    }

    return output;
}

/*=============================================================================
 * STATISTICS AND DIAGNOSTICS
 *===========================================================================*/

bool hypo_drive_get_stats(const hypo_drive_system_handle_t* system,
                           hypo_drive_stats_t* stats) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_drive_get_stats: system is NULL");
        return false;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_drive_get_stats: stats is NULL");
        return false;
    }

    *stats = system->stats;
    return true;
}

const char* hypo_drive_type_string(hypo_drive_type_t drive_type) {
    switch (drive_type) {
        case HYPO_DRIVE_HUNGER:      return "HUNGER";
        case HYPO_DRIVE_THIRST:      return "THIRST";
        case HYPO_DRIVE_TEMPERATURE: return "TEMPERATURE";
        case HYPO_DRIVE_FATIGUE:     return "FATIGUE";
        case HYPO_DRIVE_SOCIAL:      return "SOCIAL";
        case HYPO_DRIVE_CURIOSITY:   return "CURIOSITY";
        case HYPO_DRIVE_SAFETY:      return "SAFETY";
        case HYPO_DRIVE_AUTONOMY:    return "AUTONOMY";
        case HYPO_DRIVE_COMPETENCE:  return "COMPETENCE";
        default:                     return "UNKNOWN";
    }
}

const char* hypo_nucleus_type_string(hypo_nucleus_type_t nucleus) {
    switch (nucleus) {
        case HYPO_NUCLEUS_LATERAL:        return "LATERAL";
        case HYPO_NUCLEUS_VENTROMEDIAL:   return "VENTROMEDIAL";
        case HYPO_NUCLEUS_ANTERIOR:       return "ANTERIOR";
        case HYPO_NUCLEUS_POSTERIOR:      return "POSTERIOR";
        case HYPO_NUCLEUS_ARCUATE:        return "ARCUATE";
        case HYPO_NUCLEUS_PARAVENTRICULAR:return "PARAVENTRICULAR";
        case HYPO_NUCLEUS_SUPRACHIASMATIC:return "SUPRACHIASMATIC";
        case HYPO_NUCLEUS_SUPRAOPTIC:     return "SUPRAOPTIC";
        case HYPO_NUCLEUS_PREOPTIC:       return "PREOPTIC";
        case HYPO_NUCLEUS_TUBEROMAMMILLARY:return "TUBEROMAMMILLARY";
        default:                          return "UNKNOWN";
    }
}

const char* hypo_alignment_mode_string(hypo_alignment_mode_t mode) {
    switch (mode) {
        case HYPO_ALIGN_CONTROLLED:       return "CONTROLLED";
        case HYPO_ALIGN_SOCIAL_INSTINCT:  return "SOCIAL_INSTINCT";
        case HYPO_ALIGN_HYBRID:           return "HYBRID";
        default:                          return "UNKNOWN";
    }
}

const char* hypo_lock_state_string(hypo_lock_state_t state) {
    switch (state) {
        case HYPO_LOCK_UNLOCKED: return "UNLOCKED";
        case HYPO_LOCK_SOFT:     return "SOFT";
        case HYPO_LOCK_HARD:     return "HARD";
        default:                 return "UNKNOWN";
    }
}

/*=============================================================================
 * THREAD SAFETY
 *===========================================================================*/

nimcp_mutex_t* hypo_drive_get_mutex(hypo_drive_system_handle_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_drive_get_mutex: system is NULL");
        return NULL;
    }
    return system->mutex;
}
