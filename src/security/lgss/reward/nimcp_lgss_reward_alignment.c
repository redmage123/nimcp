/**
 * @file nimcp_lgss_reward_alignment.c
 * @brief LGSS Component A9: Reward System Alignment Implementation
 * @date 2026-01-16
 *
 * Implementation of reward alignment monitoring and reward hacking detection.
 */

#include "security/lgss/reward/nimcp_lgss_reward_alignment.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(lgss_reward_alignment, MESH_ADAPTER_CATEGORY_SECURITY)


/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

/**
 * @brief Get current timestamp in microseconds
 */
static uint64_t get_timestamp_us(void) {
    /* Platform-specific implementation would go here */
    static uint64_t counter = 0;
    return counter++;
}

/**
 * @brief Add sample to sliding window
 */
static void add_to_window(
    reward_alignment_monitor_t* monitor,
    float reward_value,
    reward_source_type_t source,
    bool was_aligned)
{
    if (!monitor) return;

    reward_rate_sample_t* sample = &monitor->window[monitor->window_head];
    sample->timestamp_us = get_timestamp_us();
    sample->reward_value = reward_value;
    sample->source = source;
    sample->was_aligned = was_aligned;

    monitor->window_head = (monitor->window_head + 1) % REWARD_ALIGNMENT_MAX_WINDOW_SAMPLES;
    if (monitor->window_count < REWARD_ALIGNMENT_MAX_WINDOW_SAMPLES) {
        monitor->window_count++;
    }
}

/**
 * @brief Calculate current reward rate from sliding window
 */
static float calculate_reward_rate(const reward_alignment_monitor_t* monitor) {
    if (!monitor || monitor->window_count < 2) {
        return 0.0f;
    }

    uint64_t newest_time = 0;
    uint64_t oldest_time = UINT64_MAX;
    uint32_t count = 0;

    for (uint32_t i = 0; i < monitor->window_count; i++) {
        const reward_rate_sample_t* sample = &monitor->window[i];
        if (sample->timestamp_us > newest_time) {
            newest_time = sample->timestamp_us;
        }
        if (sample->timestamp_us < oldest_time) {
            oldest_time = sample->timestamp_us;
        }
        count++;
    }

    uint64_t window_duration_us = newest_time - oldest_time;
    if (window_duration_us == 0) {
        return 0.0f;
    }

    /* Convert to rewards per second */
    float duration_sec = (float)window_duration_us / 1000000.0f;
    return (float)count / duration_sec;
}

/**
 * @brief Find aligned goal by ID
 */
static const aligned_goal_t* find_goal(
    const reward_alignment_monitor_t* monitor,
    uint32_t goal_id)
{
    if (!monitor) { return NULL; }

    for (uint32_t i = 0; i < monitor->num_aligned_goals; i++) {
        if (monitor->aligned_goals[i].goal_id == goal_id &&
            monitor->aligned_goals[i].is_active) {
            return &monitor->aligned_goals[i];
        }
    }
    return NULL;
}

/**
 * @brief Compute pathway configuration hash
 */
static uint32_t compute_pathway_hash(const reward_alignment_monitor_t* monitor) {
    if (!monitor) return 0;

    uint32_t hash = 0x12345678;
    /* Simple hash of goals and configuration */
    for (uint32_t i = 0; i < monitor->num_aligned_goals; i++) {
        hash ^= monitor->aligned_goals[i].goal_id;
        hash = (hash << 5) | (hash >> 27);
    }
    hash ^= (uint32_t)(monitor->config.baseline_reward_rate * 1000.0f);
    hash ^= (uint32_t)(monitor->config.max_reward_deviation * 1000.0f);

    return hash;
}

/*=============================================================================
 * LIFECYCLE API IMPLEMENTATION
 *===========================================================================*/

reward_alignment_config_t reward_alignment_default_config(void) {
    reward_alignment_config_t config = {
        .baseline_reward_rate = REWARD_ALIGNMENT_BASELINE_RATE,
        .max_reward_deviation = REWARD_ALIGNMENT_MAX_DEVIATION,
        .max_value_change_rate = REWARD_ALIGNMENT_MAX_VALUE_CHANGE,
        .sliding_window_ms = REWARD_ALIGNMENT_SLIDING_WINDOW_MS,
        .detect_self_stimulation = true,
        .detect_reward_tampering = true,
        .detect_proxy_gaming = true,
        .self_stim_threshold = REWARD_ALIGNMENT_SELF_STIM_THRESHOLD
    };
    return config;
}

reward_alignment_monitor_t* reward_alignment_create(
    const reward_alignment_config_t* config)
{
    reward_alignment_monitor_t* monitor = nimcp_calloc(1, sizeof(reward_alignment_monitor_t));
    if (!monitor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "reward_alignment_create: allocation failed");

        return NULL;
    }

    monitor->magic = REWARD_ALIGNMENT_MONITOR_MAGIC;

    /* Apply configuration */
    if (config) {
        monitor->config = *config;
    } else {
        monitor->config = reward_alignment_default_config();
    }

    /* Initialize state */
    monitor->num_aligned_goals = 0;
    monitor->window_head = 0;
    monitor->window_count = 0;
    monitor->last_reward_value = 0.0f;
    monitor->last_reward_time = 0;
    monitor->consecutive_uncaused = 0;
    monitor->pathway_locked = false;
    monitor->pathway_hash = 0;

    /* Initialize statistics */
    memset(&monitor->stats, 0, sizeof(reward_alignment_stats_t));

    monitor->initialized = true;
    return monitor;
}

void reward_alignment_destroy(reward_alignment_monitor_t* monitor) {
    if (!monitor) return;
    if (monitor->magic != REWARD_ALIGNMENT_MONITOR_MAGIC) return;

    monitor->magic = 0;
    monitor->initialized = false;
    nimcp_free(monitor);
}

int reward_alignment_reset(reward_alignment_monitor_t* monitor) {
    if (!monitor || monitor->magic != REWARD_ALIGNMENT_MONITOR_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Reset window */
    monitor->window_head = 0;
    monitor->window_count = 0;
    memset(monitor->window, 0, sizeof(monitor->window));

    /* Reset state */
    monitor->last_reward_value = 0.0f;
    monitor->last_reward_time = 0;
    monitor->consecutive_uncaused = 0;

    /* Keep goals and configuration */
    return 0;
}

/*=============================================================================
 * CORE VALIDATION API IMPLEMENTATION
 *===========================================================================*/

reward_alignment_status_t reward_alignment_validate(
    reward_alignment_monitor_t* monitor,
    reward_signal_t* signal)
{
    if (!monitor || !signal) {
        return REWARD_STATUS_MISALIGNED;
    }
    if (monitor->magic != REWARD_ALIGNMENT_MONITOR_MAGIC) {
        return REWARD_STATUS_MISALIGNED;
    }

    signal->status = REWARD_STATUS_ALIGNED;
    signal->confidence = 0.0f;
    uint64_t now = get_timestamp_us();
    signal->timestamp_us = now;

    monitor->stats.total_signals++;

    /* Check 1: Verify external cause */
    bool has_external_cause = signal->external_cause;
    if (signal->source == REWARD_SOURCE_SELF_GENERATED) {
        has_external_cause = false;
    } else if (signal->source == REWARD_SOURCE_GOAL_ACHIEVEMENT) {
        has_external_cause = reward_alignment_is_goal_registered(monitor, signal->goal_id);
    } else if (signal->source == REWARD_SOURCE_EXTERNAL ||
               signal->source == REWARD_SOURCE_HOMEOSTATIC ||
               signal->source == REWARD_SOURCE_SOCIAL) {
        has_external_cause = true;
    }

    if (!has_external_cause) {
        monitor->consecutive_uncaused++;
        monitor->stats.uncaused_rewards++;

        /* Self-stimulation detection */
        if (monitor->config.detect_self_stimulation &&
            monitor->consecutive_uncaused >= monitor->config.self_stim_threshold) {
            signal->status = REWARD_STATUS_HACKING_DETECTED;
            monitor->stats.blocked_signals++;
            monitor->stats.self_stim_alerts++;

            if (monitor->hack_alert_callback) {
                monitor->hack_alert_callback(HACK_SELF_STIMULATION,
                                            monitor->callback_user_data);
            }
            return REWARD_STATUS_HACKING_DETECTED;
        }
    } else {
        monitor->consecutive_uncaused = 0;
    }

    /* Check 2: Goal alignment */
    if (signal->goal_id != 0) {
        const aligned_goal_t* goal = find_goal(monitor, signal->goal_id);
        if (!goal) {
            signal->status = REWARD_STATUS_MISALIGNED;
            monitor->stats.misaligned_signals++;
            return REWARD_STATUS_MISALIGNED;
        }

        /* Verify reward magnitude is within goal bounds */
        if (fabsf(signal->reward_value) > goal->max_reward) {
            signal->status = REWARD_STATUS_SUSPICIOUS;
            signal->reward_value = (signal->reward_value > 0)
                                   ? goal->max_reward
                                   : -goal->max_reward;
            monitor->stats.suspicious_signals++;
        }

        /* Extra confidence for safety-aligned goals */
        if (goal->is_safety_aligned) {
            signal->confidence += 0.3f;
        }
    }

    /* Check 3: Rate monitoring */
    float current_rate = calculate_reward_rate(monitor);
    float max_rate = monitor->config.baseline_reward_rate *
                     monitor->config.max_reward_deviation;

    if (current_rate > max_rate) {
        signal->status = REWARD_STATUS_SUSPICIOUS;
        monitor->stats.suspicious_signals++;

        /* Severe rate anomaly indicates hacking */
        if (current_rate > max_rate * 2.0f) {
            signal->status = REWARD_STATUS_HACKING_DETECTED;
            monitor->stats.blocked_signals++;
            monitor->stats.hack_attempts++;
            monitor->stats.last_hack_type = HACK_RATE_ANOMALY;
            monitor->stats.last_hack_time = now;

            if (monitor->hack_alert_callback) {
                monitor->hack_alert_callback(HACK_RATE_ANOMALY,
                                            monitor->callback_user_data);
            }
            return REWARD_STATUS_HACKING_DETECTED;
        }
    }

    /* Check 4: Value change rate */
    if (monitor->last_reward_time > 0) {
        float time_diff_sec = (float)(now - monitor->last_reward_time) / 1000000.0f;
        if (time_diff_sec > 0.001f) {  /* Avoid division by zero */
            float value_change = fabsf(signal->reward_value - monitor->last_reward_value);
            float change_rate = value_change / time_diff_sec;

            if (change_rate > monitor->config.max_value_change_rate) {
                signal->status = REWARD_STATUS_SUSPICIOUS;
                monitor->stats.suspicious_signals++;
            }
        }
    }

    /* Check 5: Pathway integrity (if locked) */
    if (monitor->pathway_locked) {
        uint32_t current_hash = compute_pathway_hash(monitor);
        if (current_hash != monitor->pathway_hash) {
            signal->status = REWARD_STATUS_HACKING_DETECTED;
            monitor->stats.blocked_signals++;
            monitor->stats.hack_attempts++;
            monitor->stats.last_hack_type = HACK_PATHWAY_MODIFICATION;
            monitor->stats.last_hack_time = now;

            if (monitor->hack_alert_callback) {
                monitor->hack_alert_callback(HACK_PATHWAY_MODIFICATION,
                                            monitor->callback_user_data);
            }
            return REWARD_STATUS_HACKING_DETECTED;
        }
    }

    /* Update state */
    monitor->last_reward_value = signal->reward_value;
    monitor->last_reward_time = now;

    /* Add to window if not blocked */
    if (signal->status != REWARD_STATUS_HACKING_DETECTED) {
        add_to_window(monitor, signal->reward_value, signal->source,
                     signal->status == REWARD_STATUS_ALIGNED);
    }

    /* Update statistics */
    if (signal->status == REWARD_STATUS_ALIGNED) {
        monitor->stats.aligned_signals++;
        signal->confidence = 0.8f + (has_external_cause ? 0.2f : 0.0f);
    }

    monitor->stats.current_rate = current_rate;
    if (current_rate > monitor->stats.peak_rate) {
        monitor->stats.peak_rate = current_rate;
    }
    /* Running average */
    monitor->stats.average_rate = (monitor->stats.average_rate * 0.99f) +
                                  (current_rate * 0.01f);

    return signal->status;
}

bool reward_alignment_detect_hacking(
    reward_alignment_monitor_t* monitor,
    reward_hack_detection_t* detection)
{
    if (!monitor || !detection) { return false; }
    if (monitor->magic != REWARD_ALIGNMENT_MONITOR_MAGIC) { return false; }

    memset(detection, 0, sizeof(reward_hack_detection_t));
    detection->timestamp = get_timestamp_us();
    detection->hacking_detected = false;
    detection->hack_type = HACK_NONE;
    detection->baseline_rate = monitor->config.baseline_reward_rate;
    detection->reward_rate = calculate_reward_rate(monitor);
    detection->uncaused_count = monitor->consecutive_uncaused;

    /* Check 1: Self-stimulation */
    if (monitor->config.detect_self_stimulation) {
        if (monitor->consecutive_uncaused >= monitor->config.self_stim_threshold) {
            detection->hacking_detected = true;
            detection->hack_type = HACK_SELF_STIMULATION;
            detection->confidence = 0.9f;
            snprintf(detection->description, sizeof(detection->description),
                    "Self-stimulation detected: %u consecutive uncaused rewards",
                    monitor->consecutive_uncaused);
            return true;
        }
    }

    /* Check 2: Rate anomaly */
    float max_rate = monitor->config.baseline_reward_rate *
                     monitor->config.max_reward_deviation;
    if (detection->reward_rate > max_rate * 2.0f) {
        detection->hacking_detected = true;
        detection->hack_type = HACK_RATE_ANOMALY;
        detection->confidence = 0.85f;
        snprintf(detection->description, sizeof(detection->description),
                "Rate anomaly: %.2f rewards/sec (max: %.2f)",
                detection->reward_rate, max_rate);
        return true;
    }

    /* Check 3: Pathway integrity */
    if (monitor->pathway_locked) {
        uint32_t current_hash = compute_pathway_hash(monitor);
        if (current_hash != monitor->pathway_hash) {
            detection->hacking_detected = true;
            detection->hack_type = HACK_PATHWAY_MODIFICATION;
            detection->confidence = 0.95f;
            snprintf(detection->description, sizeof(detection->description),
                    "Pathway modification detected: hash mismatch");
            return true;
        }
    }

    /* Check 4: Suspicious pattern in window */
    uint32_t uncaused_in_window = 0;
    for (uint32_t i = 0; i < monitor->window_count; i++) {
        if (!monitor->window[i].was_aligned) {
            uncaused_in_window++;
        }
    }
    float uncaused_ratio = (monitor->window_count > 0)
                          ? (float)uncaused_in_window / (float)monitor->window_count
                          : 0.0f;

    if (uncaused_ratio > 0.5f && monitor->window_count >= 10) {
        detection->hacking_detected = true;
        detection->hack_type = HACK_REWARD_TAMPERING;
        detection->confidence = 0.7f;
        snprintf(detection->description, sizeof(detection->description),
                "High uncaused reward ratio: %.1f%% of %u recent rewards",
                uncaused_ratio * 100.0f, monitor->window_count);
        return true;
    }

    snprintf(detection->description, sizeof(detection->description),
            "No hacking detected");
    return false;
}

bool reward_alignment_verify_external_cause(
    reward_alignment_monitor_t* monitor,
    const reward_signal_t* signal)
{
    if (!monitor || !signal) { return false; }

    /* Self-generated signals never have external cause */
    if (signal->source == REWARD_SOURCE_SELF_GENERATED) {
        return false;
    }

    /* Unknown source needs verification */
    if (signal->source == REWARD_SOURCE_UNKNOWN) {
        return signal->external_cause;  /* Trust explicit flag only */
    }

    /* Goal-based signals require registered goal */
    if (signal->source == REWARD_SOURCE_GOAL_ACHIEVEMENT) {
        return find_goal(monitor, signal->goal_id) != NULL;
    }

    /* External, homeostatic, social signals have external cause */
    return true;
}

/*=============================================================================
 * GOAL MANAGEMENT API IMPLEMENTATION
 *===========================================================================*/

int reward_alignment_register_goal(
    reward_alignment_monitor_t* monitor,
    uint32_t goal_id,
    const char* description,
    float base_reward,
    bool is_safety_aligned)
{
    if (!monitor || goal_id == 0) {
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (monitor->magic != REWARD_ALIGNMENT_MONITOR_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (monitor->num_aligned_goals >= REWARD_ALIGNMENT_MAX_ALIGNED_GOALS) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Check if already registered */
    for (uint32_t i = 0; i < monitor->num_aligned_goals; i++) {
        if (monitor->aligned_goals[i].goal_id == goal_id) {
            /* Update existing */
            aligned_goal_t* goal = &monitor->aligned_goals[i];
            if (description) {
                strncpy(goal->description, description, sizeof(goal->description) - 1);
            }
            goal->base_reward = base_reward;
            goal->max_reward = base_reward * 2.0f;  /* 2x base as max */
            goal->is_safety_aligned = is_safety_aligned;
            goal->is_active = true;
            return 0;
        }
    }

    /* Add new goal */
    aligned_goal_t* goal = &monitor->aligned_goals[monitor->num_aligned_goals];
    goal->goal_id = goal_id;
    if (description) {
        strncpy(goal->description, description, sizeof(goal->description) - 1);
        goal->description[sizeof(goal->description) - 1] = '\0';
    } else {
        goal->description[0] = '\0';
    }
    goal->base_reward = base_reward;
    goal->max_reward = base_reward * 2.0f;
    goal->is_safety_aligned = is_safety_aligned;
    goal->is_active = true;
    goal->registration_time = get_timestamp_us();

    monitor->num_aligned_goals++;

    /* Update pathway hash if locked */
    if (monitor->pathway_locked) {
        monitor->pathway_hash = compute_pathway_hash(monitor);
    }

    return 0;
}

int reward_alignment_unregister_goal(
    reward_alignment_monitor_t* monitor,
    uint32_t goal_id)
{
    if (!monitor || goal_id == 0) {
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (monitor->magic != REWARD_ALIGNMENT_MONITOR_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Pathway protection check */
    if (monitor->pathway_locked) {
        return -1;  /* Cannot modify when locked */
    }

    for (uint32_t i = 0; i < monitor->num_aligned_goals; i++) {
        if (monitor->aligned_goals[i].goal_id == goal_id) {
            /* Mark inactive (don't remove to preserve indices) */
            monitor->aligned_goals[i].is_active = false;
            return 0;
        }
    }

    return -1;  /* Not found */
}

bool reward_alignment_is_goal_registered(
    const reward_alignment_monitor_t* monitor,
    uint32_t goal_id)
{
    return find_goal(monitor, goal_id) != NULL;
}

bool reward_alignment_is_goal_safe(
    const reward_alignment_monitor_t* monitor,
    uint32_t goal_id)
{
    const aligned_goal_t* goal = find_goal(monitor, goal_id);
    return goal && goal->is_safety_aligned;
}

/*=============================================================================
 * RATE MONITORING API IMPLEMENTATION
 *===========================================================================*/

int reward_alignment_get_rate(
    const reward_alignment_monitor_t* monitor,
    float* rate)
{
    if (!monitor || !rate) {
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (monitor->magic != REWARD_ALIGNMENT_MONITOR_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    *rate = calculate_reward_rate(monitor);
    return 0;
}

bool reward_alignment_rate_ok(const reward_alignment_monitor_t* monitor) {
    if (!monitor || monitor->magic != REWARD_ALIGNMENT_MONITOR_MAGIC) {
        return false;
    }

    float current_rate = calculate_reward_rate(monitor);
    float max_rate = monitor->config.baseline_reward_rate *
                     monitor->config.max_reward_deviation;
    return current_rate <= max_rate;
}

float reward_alignment_get_rate_anomaly(const reward_alignment_monitor_t* monitor) {
    if (!monitor || monitor->magic != REWARD_ALIGNMENT_MONITOR_MAGIC) {
        return 1.0f;  /* Max anomaly if invalid */
    }

    float current_rate = calculate_reward_rate(monitor);
    float max_rate = monitor->config.baseline_reward_rate *
                     monitor->config.max_reward_deviation;

    if (max_rate <= 0.0f) {
        return 0.0f;
    }

    float anomaly = current_rate / max_rate;
    if (anomaly > 1.0f) {
        return fminf(anomaly - 1.0f, 1.0f);  /* How far over limit [0,1] */
    }
    return 0.0f;
}

/*=============================================================================
 * PATHWAY PROTECTION API IMPLEMENTATION
 *===========================================================================*/

int reward_alignment_lock_pathways(reward_alignment_monitor_t* monitor) {
    if (!monitor || monitor->magic != REWARD_ALIGNMENT_MONITOR_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    monitor->pathway_hash = compute_pathway_hash(monitor);
    monitor->pathway_locked = true;
    return 0;
}

bool reward_alignment_pathways_locked(const reward_alignment_monitor_t* monitor) {
    if (!monitor || monitor->magic != REWARD_ALIGNMENT_MONITOR_MAGIC) {
        return false;
    }
    return monitor->pathway_locked;
}

bool reward_alignment_verify_pathways(const reward_alignment_monitor_t* monitor) {
    if (!monitor || monitor->magic != REWARD_ALIGNMENT_MONITOR_MAGIC) {
        return false;
    }
    if (!monitor->pathway_locked) {
        return true;  /* Not locked, nothing to verify */
    }

    uint32_t current_hash = compute_pathway_hash(monitor);
    return current_hash == monitor->pathway_hash;
}

/*=============================================================================
 * VTA/AIX INTEGRATION API IMPLEMENTATION
 *===========================================================================*/

int reward_alignment_set_vta(
    reward_alignment_monitor_t* monitor,
    void* vta)
{
    if (!monitor || monitor->magic != REWARD_ALIGNMENT_MONITOR_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }
    monitor->vta = vta;
    return 0;
}

int reward_alignment_set_aix(
    reward_alignment_monitor_t* monitor,
    void* aix)
{
    if (!monitor || monitor->magic != REWARD_ALIGNMENT_MONITOR_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }
    monitor->aix = aix;
    return 0;
}

/*=============================================================================
 * STATISTICS API IMPLEMENTATION
 *===========================================================================*/

int reward_alignment_get_stats(
    const reward_alignment_monitor_t* monitor,
    reward_alignment_stats_t* stats)
{
    if (!monitor || !stats) {
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (monitor->magic != REWARD_ALIGNMENT_MONITOR_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    *stats = monitor->stats;
    return 0;
}

int reward_alignment_reset_stats(reward_alignment_monitor_t* monitor) {
    if (!monitor || monitor->magic != REWARD_ALIGNMENT_MONITOR_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    memset(&monitor->stats, 0, sizeof(reward_alignment_stats_t));
    return 0;
}

/*=============================================================================
 * CALLBACK API IMPLEMENTATION
 *===========================================================================*/

int reward_alignment_set_hack_callback(
    reward_alignment_monitor_t* monitor,
    void (*callback)(reward_hack_type_t type, void* user_data),
    void* user_data)
{
    if (!monitor || monitor->magic != REWARD_ALIGNMENT_MONITOR_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    monitor->hack_alert_callback = callback;
    monitor->callback_user_data = user_data;
    return 0;
}

/*=============================================================================
 * UTILITY API IMPLEMENTATION
 *===========================================================================*/

void reward_signal_init(
    reward_signal_t* signal,
    float value,
    reward_source_type_t source,
    uint32_t target_neuron)
{
    if (!signal) return;

    memset(signal, 0, sizeof(reward_signal_t));
    signal->magic = REWARD_SIGNAL_MAGIC;
    signal->reward_value = value;
    signal->source = source;
    signal->target_neuron = target_neuron;
    signal->status = REWARD_STATUS_ALIGNED;  /* Default, will be validated */
    signal->timestamp_us = get_timestamp_us();
    signal->external_cause = (source != REWARD_SOURCE_SELF_GENERATED &&
                             source != REWARD_SOURCE_UNKNOWN);
    signal->confidence = 0.0f;
    signal->goal_id = 0;
}

const char* reward_alignment_status_string(reward_alignment_status_t status) {
    switch (status) {
        case REWARD_STATUS_ALIGNED:
            return "ALIGNED";
        case REWARD_STATUS_MISALIGNED:
            return "MISALIGNED";
        case REWARD_STATUS_SUSPICIOUS:
            return "SUSPICIOUS";
        case REWARD_STATUS_HACKING_DETECTED:
            return "HACKING_DETECTED";
        default:
            return "UNKNOWN";
    }
}

const char* reward_hack_type_string(reward_hack_type_t type) {
    switch (type) {
        case HACK_NONE:
            return "NONE";
        case HACK_SELF_STIMULATION:
            return "SELF_STIMULATION";
        case HACK_REWARD_TAMPERING:
            return "REWARD_TAMPERING";
        case HACK_PROXY_GAMING:
            return "PROXY_GAMING";
        case HACK_PATHWAY_MODIFICATION:
            return "PATHWAY_MODIFICATION";
        case HACK_RATE_ANOMALY:
            return "RATE_ANOMALY";
        case HACK_VALUE_ANOMALY:
            return "VALUE_ANOMALY";
        default:
            return "UNKNOWN";
    }
}

const char* reward_source_type_string(reward_source_type_t source) {
    switch (source) {
        case REWARD_SOURCE_EXTERNAL:
            return "EXTERNAL";
        case REWARD_SOURCE_GOAL_ACHIEVEMENT:
            return "GOAL_ACHIEVEMENT";
        case REWARD_SOURCE_HOMEOSTATIC:
            return "HOMEOSTATIC";
        case REWARD_SOURCE_SOCIAL:
            return "SOCIAL";
        case REWARD_SOURCE_INTRINSIC:
            return "INTRINSIC";
        case REWARD_SOURCE_UNKNOWN:
            return "UNKNOWN";
        case REWARD_SOURCE_SELF_GENERATED:
            return "SELF_GENERATED";
        default:
            return "INVALID";
    }
}
