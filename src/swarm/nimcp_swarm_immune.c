/**
 * @file nimcp_swarm_immune.c
 * @brief Swarm Immune System Implementation
 *
 * Biologically-inspired adaptive immune response for swarm robotics.
 * Based on principles of:
 * - T-cell mediated immunity
 * - B-cell antibody production
 * - Clonal selection theory
 * - Immunological memory
 * - Self/non-self discrimination
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include "swarm/nimcp_swarm_immune.h"
#include "core/neuron_types/nimcp_neural_logic.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdlib.h>
#include <sys/random.h>
#include <fcntl.h>
#include <unistd.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_learning_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(swarm_immune)

/* ============================================================================
 * Constants and Macros
 * ============================================================================ */

#define NIMCP_SWARM_IMMUNE_VERSION "1.0.0"
#define NIMCP_SWARM_IMMUNE_DEFAULT_MAX_MEMORY_CELLS 1000
#define NIMCP_SWARM_IMMUNE_DEFAULT_MAX_THREATS 100
#define NIMCP_SWARM_IMMUNE_DEFAULT_MAX_RESPONSES 50
#define NIMCP_SWARM_IMMUNE_DEFAULT_RECOGNITION_THRESHOLD 0.75f
#define NIMCP_SWARM_IMMUNE_DEFAULT_SELF_TOLERANCE 0.9f
#define NIMCP_SWARM_IMMUNE_DEFAULT_MEMORY_DECAY NIMCP_EMA_DECAY_DEFAULT
#define NIMCP_SWARM_IMMUNE_DEFAULT_CLONAL_EXPANSION 1.5f
#define NIMCP_SWARM_IMMUNE_DEFAULT_CONFIRMATION_THRESHOLD 3

/* Threat type names */
static const char* THREAT_TYPE_NAMES[] = {
    "MALICIOUS_DRONE",
    "JAMMING",
    "SPOOFING",
    "INJECTION",
    "REPLAY",
    "PHYSICAL",
    "DOS",
    "SYBIL",
    "BYZANTINE"
};

/* Response type names */
static const char* RESPONSE_TYPE_NAMES[] = {
    "NONE",
    "ISOLATION",
    "EVASION",
    "COUNTER_ATTACK",
    "ALERT",
    "COORDINATION",
    "RECONFIGURATION",
    "AUTHENTICATION"
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Calculate fuzzy match score between two patterns
 */
static float calculate_pattern_match(
    const uint8_t* pattern1,
    size_t len1,
    const uint8_t* pattern2,
    size_t len2
) {
    if (!pattern1 || !pattern2 || len1 == 0 || len2 == 0) {
        return 0.0F;
    }

    /* Use Hamming distance for same-length patterns */
    if (len1 == len2) {
        size_t matches = 0;
        for (size_t i = 0; i < len1; i++) {
            if (pattern1[i] == pattern2[i]) {
                matches++;
            }
        }
        return (float)matches / (float)len1;
    }

    /* Use subsequence matching for different lengths */
    size_t min_len = (len1 < len2) ? len1 : len2;
    size_t matches = 0;

    for (size_t i = 0; i < min_len; i++) {
        if (pattern1[i] == pattern2[i]) {
            matches++;
        }
    }

    return (float)matches / (float)((len1 + len2) / 2);
}

/**
 * @brief Calculate behavioral anomaly score
 */
static float calculate_anomaly_score(
    const NimcpSwarmBehaviorProfile* baseline,
    const NimcpSwarmBehaviorProfile* current
) {
    if (!baseline || !current) {
        return 0.0F;
    }

    float score = 0.0F;
    float weight = 0.25F; /* Equal weight for each metric */

    /* Message rate anomaly */
    float msg_rate_diff = fabsf(current->msg_rate - baseline->msg_rate);
    score += weight * (msg_rate_diff / (baseline->msg_rate + 1.0F));

    /* Movement pattern anomaly */
    float movement_diff = 0.0F;
    for (int i = 0; i < 3; i++) {
        float diff = fabsf(current->movement_pattern[i] - baseline->movement_pattern[i]);
        movement_diff += diff * diff;
    }
    movement_diff = sqrtf(movement_diff);
    score += weight * movement_diff;

    /* Energy usage anomaly */
    float energy_diff = fabsf(current->energy_usage - baseline->energy_usage);
    score += weight * (energy_diff / (baseline->energy_usage + 1.0F));

    /* Connection changes anomaly */
    float conn_diff = fabsf((float)current->connection_changes - (float)baseline->connection_changes);
    score += weight * (conn_diff / (float)(baseline->connection_changes + 1));

    return fminf(score, 1.0F);
}

/**
 * @brief Get current timestamp in milliseconds
 */
static uint64_t get_current_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

/**
 * @brief Generate cryptographically secure unique ID
 *
 * WHAT: Generate random ID using getrandom() syscall
 * WHY:  Prevent ID prediction attacks and race conditions
 * HOW:  Use getrandom() (thread-safe syscall), fall back to secure /dev/urandom
 *
 * THREAD SAFETY: getrandom() is thread-safe, no file descriptors needed
 * SECURITY FIX: Eliminates race condition from file-based /dev/urandom approach
 */
static uint32_t generate_unique_id(void) {
    static uint64_t counter = 0;  /* 64-bit to prevent wraparound, accessed atomically */
    uint32_t id;

    /* Try getrandom() syscall (thread-safe, no file descriptor race) */
    ssize_t n = getrandom(&id, sizeof(id), GRND_NONBLOCK);
    if (n == sizeof(id)) {
        return id;
    }

    /* Fallback: try /dev/urandom with proper error handling */
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        n = read(fd, &id, sizeof(id));
        close(fd);
        if (n == sizeof(id)) {
            return id;
        }
    }

    /* Last resort fallback: combine time and atomic counter */
    LOG_WARN("Using weak RNG for ID generation - secure RNG unavailable");
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    id = (uint32_t)(ts.tv_nsec ^ ts.tv_sec);

    /* Use 64-bit counter with atomic increment to prevent wraparound */
    uint64_t count = __sync_fetch_and_add(&counter, 1);
    id ^= (uint32_t)(count ^ (count >> 32));  /* Mix upper and lower 32 bits */
    return id;
}

/* ============================================================================
 * Core API Implementation
 * ============================================================================ */

nimcp_result_t nimcp_swarm_immune_default_config(NimcpSwarmImmuneConfig* config) {
    NIMCP_CHECK_THROW(config, NIMCP_INVALID_PARAM, "config pointer is NULL");

    config->max_memory_cells = NIMCP_SWARM_IMMUNE_DEFAULT_MAX_MEMORY_CELLS;
    config->max_active_threats = NIMCP_SWARM_IMMUNE_DEFAULT_MAX_THREATS;
    config->max_active_responses = NIMCP_SWARM_IMMUNE_DEFAULT_MAX_RESPONSES;
    config->recognition_threshold = NIMCP_SWARM_IMMUNE_DEFAULT_RECOGNITION_THRESHOLD;
    config->self_tolerance = NIMCP_SWARM_IMMUNE_DEFAULT_SELF_TOLERANCE;
    config->memory_decay_rate = NIMCP_SWARM_IMMUNE_DEFAULT_MEMORY_DECAY;
    config->clonal_expansion_rate = NIMCP_SWARM_IMMUNE_DEFAULT_CLONAL_EXPANSION;
    config->enable_sharing = true;
    config->enable_coordination = true;
    config->confirmation_threshold = NIMCP_SWARM_IMMUNE_DEFAULT_CONFIRMATION_THRESHOLD;

    return NIMCP_SUCCESS;
}

NimcpSwarmImmuneSystem* nimcp_swarm_immune_create(
    const NimcpSwarmImmuneConfig* config,
    bio_module_context_t* bio_ctx,
    uint32_t self_drone_id
) {
    if (!config) {
        LOG_ERROR("Invalid configuration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL config for swarm immune create");
        return NULL;
    }

    NimcpSwarmImmuneSystem* system = (NimcpSwarmImmuneSystem*)nimcp_malloc(
        sizeof(NimcpSwarmImmuneSystem)
    );
    if (!system) {
        LOG_ERROR("Failed to allocate immune system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "failed to allocate swarm immune system");
        return NULL;
    }

    memset(system, 0, sizeof(NimcpSwarmImmuneSystem));
    system->config = *config;
    system->bio_ctx = bio_ctx;
    system->self_drone_id = self_drone_id;

    /* Generate self signature using cryptographically secure RNG */
    ssize_t n = getrandom(system->self_signature, 32, 0);
    if (n != 32) {
        /* Fallback: read from /dev/urandom */
        int fd = open("/dev/urandom", O_RDONLY);
        if (fd >= 0) {
            n = read(fd, system->self_signature, 32);
            close(fd);
        }
        if (n != 32) {
            /* Last resort: use time-based weak fallback */
            LOG_WARN("Using weak RNG for self signature - secure RNG unavailable");
            struct timespec ts;
            clock_gettime(CLOCK_MONOTONIC, &ts);
            for (size_t i = 0; i < 32; i++) {
                system->self_signature[i] = (uint8_t)((ts.tv_nsec ^ ts.tv_sec ^ i) % 256);
            }
        }
    }

    /* Allocate memory cells */
    system->memory_cell_capacity = config->max_memory_cells;
    system->memory_cells = (NimcpSwarmMemoryCell*)nimcp_malloc(
        system->memory_cell_capacity * sizeof(NimcpSwarmMemoryCell)
    );
    if (!system->memory_cells) {
        LOG_ERROR("Failed to allocate memory cells");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "failed to allocate immune memory cells");
        nimcp_free(system);
        return NULL;
    }

    /* Allocate active threats */
    system->active_threat_capacity = config->max_active_threats;
    system->active_threats = (NimcpSwarmThreat*)nimcp_malloc(
        system->active_threat_capacity * sizeof(NimcpSwarmThreat)
    );
    if (!system->active_threats) {
        LOG_ERROR("Failed to allocate threat storage");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "failed to allocate immune threat storage");
        nimcp_free(system->memory_cells);
        nimcp_free(system);
        return NULL;
    }

    /* Allocate active responses */
    system->active_response_capacity = config->max_active_responses;
    system->active_responses = (NimcpSwarmResponse*)nimcp_malloc(
        system->active_response_capacity * sizeof(NimcpSwarmResponse)
    );
    if (!system->active_responses) {
        LOG_ERROR("Failed to allocate response storage");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "failed to allocate immune response storage");
        nimcp_free(system->active_threats);
        nimcp_free(system->memory_cells);
        nimcp_free(system);
        return NULL;
    }

    /* Allocate behavior profiles */
    system->behavior_profile_capacity = 100; /* Initial capacity */
    system->behavior_profiles = (NimcpSwarmBehaviorProfile*)nimcp_malloc(
        system->behavior_profile_capacity * sizeof(NimcpSwarmBehaviorProfile)
    );
    if (!system->behavior_profiles) {
        LOG_ERROR("Failed to allocate behavior profiles");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "failed to allocate immune behavior profiles");
        nimcp_free(system->active_responses);
        nimcp_free(system->active_threats);
        nimcp_free(system->memory_cells);
        nimcp_free(system);
        return NULL;
    }

    /* Allocate threat rules */
    system->threat_rule_capacity = 50; /* Initial capacity */
    system->threat_rules = (immune_threat_rule_t*)nimcp_malloc(
        system->threat_rule_capacity * sizeof(immune_threat_rule_t)
    );
    if (!system->threat_rules) {
        LOG_ERROR("Failed to allocate threat rules");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "failed to allocate immune threat rules");
        nimcp_free(system->behavior_profiles);
        nimcp_free(system->active_responses);
        nimcp_free(system->active_threats);
        nimcp_free(system->memory_cells);
        nimcp_free(system);
        return NULL;
    }

    /* Create mutex for thread safety */
    system->mutex = nimcp_platform_mutex_create();
    if (!system->mutex) {
        LOG_ERROR("Failed to create mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "failed to create immune mutex");
        nimcp_free(system->threat_rules);
        nimcp_free(system->behavior_profiles);
        nimcp_free(system->active_responses);
        nimcp_free(system->active_threats);
        nimcp_free(system->memory_cells);
        nimcp_free(system);
        return NULL;
    }

    /* Initialize maturation timestamp */
    system->last_maturation_time = 0;

    LOG_INFO("Swarm immune system created (drone_id=%u, version=%s)",
                   self_drone_id, NIMCP_SWARM_IMMUNE_VERSION);

    return system;
}

void nimcp_swarm_immune_destroy(NimcpSwarmImmuneSystem* system) {
    if (!system) {
        return;
    }

    LOG_INFO("Destroying swarm immune system (drone_id=%u)",
                   system->self_drone_id);

    if (system->mutex) {
        nimcp_platform_mutex_destroy(system->mutex);
        nimcp_free(system->mutex);
        system->mutex = NULL;
    }

    /* Free threat rules and their signal_sources arrays */
    for (size_t i = 0; i < system->threat_rule_count; i++) {
        if (system->threat_rules[i].signal_sources) {
            nimcp_free(system->threat_rules[i].signal_sources);
        }
    }
    nimcp_free(system->threat_rules);

    nimcp_free(system->behavior_profiles);
    nimcp_free(system->active_responses);
    nimcp_free(system->active_threats);
    nimcp_free(system->memory_cells);
    nimcp_free(system);
}

/* ============================================================================
 * Antigen Recognition (Threat Detection)
 * ============================================================================ */

nimcp_result_t nimcp_swarm_immune_detect_threat(
    NimcpSwarmImmuneSystem* system,
    const uint8_t* data,
    size_t data_len,
    uint32_t source_drone_id,
    uint32_t* threat_id
) {
    NIMCP_CHECK_THROW(system, NIMCP_INVALID_PARAM, "immune system is NULL");
    NIMCP_CHECK_THROW(data, NIMCP_INVALID_PARAM, "data pointer is NULL");
    NIMCP_CHECK_THROW(data_len > 0, NIMCP_INVALID_PARAM, "data_len must be greater than 0");

    nimcp_platform_mutex_lock(system->mutex);

    /* Check against known threat signatures in memory cells */
    float best_match = 0.0F;
    NimcpSwarmMemoryCell* matched_cell = NULL;

    for (size_t i = 0; i < system->memory_cell_count; i++) {
        NimcpSwarmMemoryCell* cell = &system->memory_cells[i];

        float match_score = calculate_pattern_match(
            cell->signature.pattern,
            cell->signature.pattern_len,
            data,
            data_len
        );

        if (match_score > best_match &&
            match_score >= cell->signature.match_threshold) {
            best_match = match_score;
            matched_cell = cell;
        }
    }

    /* If threat detected */
    if (matched_cell && best_match >= system->config.recognition_threshold) {
        /* Check if we have space for new threat */
        if (system->active_threat_count >= system->active_threat_capacity) {
            LOG_WARN("Active threat capacity reached");
            nimcp_platform_mutex_unlock(system->mutex);
            return NIMCP_NO_MEMORY;
        }

        /* Create new threat entry */
        NimcpSwarmThreat* threat = &system->active_threats[system->active_threat_count];
        threat->id = generate_unique_id();
        threat->type = matched_cell->signature.type;
        threat->source_drone_id = source_drone_id;
        threat->confidence = best_match;
        threat->detection_time = get_current_time_ms();
        threat->confirmed = false;
        threat->confirming_drones = 1;

        /* Validate threat type before use */
        if (!NIMCP_VALIDATE_ENUM(threat->type, THREAT_COUNT)) {
            LOG_ERROR("Invalid threat type from memory cell: %d", threat->type);
            nimcp_platform_mutex_unlock(system->mutex);
            return NIMCP_INVALID_PARAM;
        }

        /* Determine severity based on confidence and threat type */
        if (best_match > 0.9F || threat->type == THREAT_MALICIOUS_DRONE) {
            threat->severity = SWARM_SEVERITY_CRITICAL;
        } else if (best_match > 0.8F) {
            threat->severity = SWARM_SEVERITY_HIGH;
        } else if (best_match > 0.7F) {
            threat->severity = SWARM_SEVERITY_MEDIUM;
        } else {
            threat->severity = SWARM_SEVERITY_LOW;
        }

        /* Copy threat data */
        threat->data_len = (data_len < 256) ? data_len : 256;
        memcpy(threat->data, data, threat->data_len);

        if (threat_id) {
            *threat_id = threat->id;
        }

        system->active_threat_count++;
        system->total_threats_detected++;

        /* Update memory cell activation */
        matched_cell->activation_count++;
        matched_cell->last_activation = get_current_time_ms();

        LOG_WARN("Threat detected: type=%s, confidence=%.2f, source=%u",
                       THREAT_TYPE_NAMES[threat->type], best_match, source_drone_id);

        /* Send BIO_MSG_SWARM_IMMUNE_ALERT on new threat detection */
        if (system->bio_ctx && *system->bio_ctx) {
            struct {
                bio_message_header_t hdr;
                uint32_t threat_id;
                uint32_t threat_type;
                uint32_t severity;
                uint32_t source_drone_id;
                float    confidence;
            } alert_msg = {0};
            alert_msg.hdr.type          = BIO_MSG_SWARM_IMMUNE_ALERT;
            alert_msg.hdr.source_module = BIO_MODULE_IMMUNE_BRAIN;
            alert_msg.hdr.target_module = 0;
            alert_msg.hdr.payload_size  = sizeof(alert_msg) - sizeof(bio_message_header_t);
            alert_msg.hdr.flags         = BIO_MSG_FLAG_URGENT | BIO_MSG_FLAG_BROADCAST;
            alert_msg.threat_id         = threat->id;
            alert_msg.threat_type       = (uint32_t)threat->type;
            alert_msg.severity          = (uint32_t)threat->severity;
            alert_msg.source_drone_id   = source_drone_id;
            alert_msg.confidence        = best_match;
            (void)bio_router_broadcast(
                *system->bio_ctx, &alert_msg, sizeof(alert_msg));
        }

        nimcp_platform_mutex_unlock(system->mutex);
        return NIMCP_SUCCESS;
    }

    nimcp_platform_mutex_unlock(system->mutex);
    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_swarm_immune_check_behavior(
    NimcpSwarmImmuneSystem* system,
    uint32_t drone_id,
    const NimcpSwarmBehaviorProfile* behavior,
    float* anomaly_score
) {
    NIMCP_CHECK_THROW(system, NIMCP_INVALID_PARAM, "immune system is NULL");
    NIMCP_CHECK_THROW(behavior, NIMCP_INVALID_PARAM, "behavior profile is NULL");
    NIMCP_CHECK_THROW(anomaly_score, NIMCP_INVALID_PARAM, "anomaly_score output pointer is NULL");

    nimcp_platform_mutex_lock(system->mutex);

    /* Find baseline profile for this drone */
    NimcpSwarmBehaviorProfile* baseline = NULL;
    for (size_t i = 0; i < system->behavior_profile_count; i++) {
        if (system->behavior_profiles[i].drone_id == drone_id) {
            baseline = &system->behavior_profiles[i];
            break;
        }
    }

    /* If no baseline, create one */
    if (!baseline) {
        if (system->behavior_profile_count >= system->behavior_profile_capacity) {
            LOG_WARN("Behavior profile capacity reached");
            nimcp_platform_mutex_unlock(system->mutex);
            return NIMCP_NO_MEMORY;
        }

        baseline = &system->behavior_profiles[system->behavior_profile_count];
        memcpy(baseline, behavior, sizeof(NimcpSwarmBehaviorProfile));
        /* Preserve input anomaly_score for threat detection logic gates */
        /* Set last_update to current time so NOT-gate checks work correctly */
        baseline->last_update = get_current_time_ms();
        system->behavior_profile_count++;

        *anomaly_score = behavior->anomaly_score;
        nimcp_platform_mutex_unlock(system->mutex);
        return NIMCP_SUCCESS;
    }

    /* Calculate anomaly score */
    float score = calculate_anomaly_score(baseline, behavior);
    *anomaly_score = score;

    /* Update baseline with exponential moving average */
    float alpha = 0.1F; /* Smoothing factor */
    baseline->msg_rate = alpha * behavior->msg_rate + (1.0F - alpha) * baseline->msg_rate;
    baseline->energy_usage = alpha * behavior->energy_usage + (1.0F - alpha) * baseline->energy_usage;
    for (int i = 0; i < 3; i++) {
        baseline->movement_pattern[i] = alpha * behavior->movement_pattern[i] +
                                       (1.0F - alpha) * baseline->movement_pattern[i];
    }
    baseline->anomaly_score = alpha * score + (1.0F - alpha) * baseline->anomaly_score;
    baseline->last_update = get_current_time_ms();

    if (score > 0.7F) {
        LOG_WARN("High anomaly score detected: drone=%u, score=%.2f",
                       drone_id, score);
    }

    nimcp_platform_mutex_unlock(system->mutex);
    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_swarm_immune_verify_self(
    NimcpSwarmImmuneSystem* system,
    uint32_t drone_id,
    const uint8_t* signature,
    size_t signature_len,
    bool* is_self
) {
    NIMCP_CHECK_THROW(system, NIMCP_INVALID_PARAM, "immune system is NULL");
    NIMCP_CHECK_THROW(signature, NIMCP_INVALID_PARAM, "signature is NULL");
    NIMCP_CHECK_THROW(is_self, NIMCP_INVALID_PARAM, "is_self output pointer is NULL");

    nimcp_platform_mutex_lock(system->mutex);

    /* Check if this is our own drone ID */
    if (drone_id == system->self_drone_id) {
        *is_self = true;
        nimcp_platform_mutex_unlock(system->mutex);
        return NIMCP_SUCCESS;
    }

    /* Verify signature against known good signatures */
    /* In a real implementation, this would check against a whitelist */
    /* For now, use a simple tolerance check */

    float match_score = calculate_pattern_match(
        system->self_signature,
        32,
        signature,
        signature_len
    );

    *is_self = (match_score >= system->config.self_tolerance);

    if (!(*is_self)) {
        LOG_WARN("Non-self drone detected: id=%u, match=%.2f",
                       drone_id, match_score);
    }

    nimcp_platform_mutex_unlock(system->mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Memory Cells (Learned Patterns)
 * ============================================================================ */

nimcp_result_t nimcp_swarm_immune_add_memory_cell(
    NimcpSwarmImmuneSystem* system,
    const NimcpSwarmThreatSignature* signature,
    NimcpSwarmResponseType response,
    float effectiveness,
    uint32_t* cell_id
) {
    NIMCP_CHECK_THROW(system, NIMCP_INVALID_PARAM, "immune system is NULL");
    NIMCP_CHECK_THROW(signature, NIMCP_INVALID_PARAM, "threat signature is NULL");

    /* Validate enum parameters */
    if (!NIMCP_VALIDATE_ENUM(signature->type, THREAT_COUNT)) {
        LOG_ERROR("Invalid threat type: %d", signature->type);
        return NIMCP_INVALID_PARAM;
    }
    if (!NIMCP_VALIDATE_ENUM(response, RESPONSE_COUNT)) {
        LOG_ERROR("Invalid response type: %d", response);
        return NIMCP_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(system->mutex);

    if (system->memory_cell_count >= system->memory_cell_capacity) {
        LOG_WARN("Memory cell capacity reached");
        nimcp_platform_mutex_unlock(system->mutex);
        return NIMCP_NO_MEMORY;
    }

    NimcpSwarmMemoryCell* cell = &system->memory_cells[system->memory_cell_count];
    cell->id = generate_unique_id();
    cell->signature = *signature;
    cell->response = response;
    cell->effectiveness = effectiveness;
    cell->activation_count = 0;
    cell->created_time = get_current_time_ms();
    cell->last_activation = 0;
    cell->decay_factor = system->config.memory_decay_rate;
    cell->shared = false;

    if (cell_id) {
        *cell_id = cell->id;
    }

    system->memory_cell_count++;

    LOG_INFO("Memory cell added: id=%u, type=%s, response=%s",
                   cell->id,
                   THREAT_TYPE_NAMES[signature->type],
                   RESPONSE_TYPE_NAMES[response]);

    nimcp_platform_mutex_unlock(system->mutex);
    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_swarm_immune_find_memory_cell(
    NimcpSwarmImmuneSystem* system,
    const uint8_t* data,
    size_t data_len,
    NimcpSwarmMemoryCell** cell
) {
    NIMCP_CHECK_THROW(system, NIMCP_INVALID_PARAM, "immune system is NULL");
    NIMCP_CHECK_THROW(data, NIMCP_INVALID_PARAM, "data pointer is NULL");
    NIMCP_CHECK_THROW(cell, NIMCP_INVALID_PARAM, "cell output pointer is NULL");

    nimcp_platform_mutex_lock(system->mutex);

    float best_match = 0.0F;
    NimcpSwarmMemoryCell* best_cell = NULL;

    for (size_t i = 0; i < system->memory_cell_count; i++) {
        NimcpSwarmMemoryCell* current = &system->memory_cells[i];

        float match_score = calculate_pattern_match(
            current->signature.pattern,
            current->signature.pattern_len,
            data,
            data_len
        );

        if (match_score > best_match &&
            match_score >= current->signature.match_threshold) {
            best_match = match_score;
            best_cell = current;
        }
    }

    if (best_cell && best_match >= system->config.recognition_threshold) {
        *cell = best_cell;
        nimcp_platform_mutex_unlock(system->mutex);
        return NIMCP_SUCCESS;
    }

    nimcp_platform_mutex_unlock(system->mutex);
    return NIMCP_NOT_FOUND;
}

nimcp_result_t nimcp_swarm_immune_update_effectiveness(
    NimcpSwarmImmuneSystem* system,
    uint32_t cell_id,
    float new_effectiveness
) {
    NIMCP_CHECK_THROW(system, NIMCP_INVALID_PARAM, "immune system is NULL");

    nimcp_platform_mutex_lock(system->mutex);

    for (size_t i = 0; i < system->memory_cell_count; i++) {
        if (system->memory_cells[i].id == cell_id) {
            /* Update with exponential moving average */
            float alpha = 0.3F;
            system->memory_cells[i].effectiveness =
                alpha * new_effectiveness +
                (1.0F - alpha) * system->memory_cells[i].effectiveness;

            nimcp_platform_mutex_unlock(system->mutex);
            return NIMCP_SUCCESS;
        }
    }

    nimcp_platform_mutex_unlock(system->mutex);
    return NIMCP_NOT_FOUND;
}

/**
 * @brief Internal helper for memory decay (caller must hold mutex)
 *
 * WHAT: Apply decay to memory cells without acquiring mutex
 * WHY:  Allow calling from other functions that already hold mutex
 * HOW:  Same logic as public function but no mutex operations
 */
static nimcp_result_t nimcp_swarm_immune_decay_memory_unlocked(
    NimcpSwarmImmuneSystem* system,
    uint64_t current_time
) {
    size_t removed = 0;

    /* Apply decay and remove weak memory cells */
    for (size_t i = 0; i < system->memory_cell_count; ) {
        NimcpSwarmMemoryCell* cell = &system->memory_cells[i];

        /* Calculate time-based decay */
        uint64_t time_diff = current_time - cell->last_activation;
        if (time_diff > 0) {
            float time_factor = expf(-(float)time_diff / 3600000.0F); /* 1 hour decay */
            cell->effectiveness *= cell->decay_factor * time_factor;
        }

        /* Remove if effectiveness too low */
        if (cell->effectiveness < 0.1F && cell->activation_count < 5) {
            /* Swap with last element and reduce count */
            system->memory_cells[i] = system->memory_cells[system->memory_cell_count - 1];
            system->memory_cell_count--;
            removed++;
        } else {
            i++;
        }
    }

    if (removed > 0) {
        LOG_INFO("Memory decay removed %zu weak cells", removed);
    }

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_swarm_immune_decay_memory(
    NimcpSwarmImmuneSystem* system,
    uint64_t current_time
) {
    NIMCP_CHECK_THROW(system, NIMCP_INVALID_PARAM, "immune system is NULL");

    nimcp_platform_mutex_lock(system->mutex);
    nimcp_result_t result = nimcp_swarm_immune_decay_memory_unlocked(system, current_time);
    nimcp_platform_mutex_unlock(system->mutex);
    return result;
}

/* ============================================================================
 * Antibody Production (Response Generation)
 * ============================================================================ */

NimcpSwarmResponseType nimcp_swarm_immune_get_response(
    NimcpSwarmImmuneSystem* system,
    NimcpSwarmThreatType threat_type,
    NimcpSwarmSeverity severity
) {
    /* Response strategy matrix */
    static const NimcpSwarmResponseType response_matrix[THREAT_COUNT][4] = {
        /* LOW             MEDIUM              HIGH                CRITICAL */
        {RESPONSE_ALERT,  RESPONSE_ALERT,     RESPONSE_ISOLATION, RESPONSE_ISOLATION},        /* MALICIOUS_DRONE */
        {RESPONSE_EVASION, RESPONSE_EVASION,  RESPONSE_EVASION,   RESPONSE_RECONFIGURATION}, /* JAMMING */
        {RESPONSE_ALERT,  RESPONSE_AUTHENTICATION, RESPONSE_AUTHENTICATION, RESPONSE_ISOLATION}, /* SPOOFING */
        {RESPONSE_ALERT,  RESPONSE_ALERT,     RESPONSE_ISOLATION, RESPONSE_COUNTER_ATTACK},  /* INJECTION */
        {RESPONSE_ALERT,  RESPONSE_AUTHENTICATION, RESPONSE_AUTHENTICATION, RESPONSE_ISOLATION}, /* REPLAY */
        {RESPONSE_EVASION, RESPONSE_EVASION,  RESPONSE_COORDINATION, RESPONSE_COUNTER_ATTACK}, /* PHYSICAL */
        {RESPONSE_ALERT,  RESPONSE_EVASION,   RESPONSE_RECONFIGURATION, RESPONSE_RECONFIGURATION}, /* DOS */
        {RESPONSE_ALERT,  RESPONSE_AUTHENTICATION, RESPONSE_ISOLATION, RESPONSE_ISOLATION},     /* SYBIL */
        {RESPONSE_ALERT,  RESPONSE_ISOLATION, RESPONSE_ISOLATION, RESPONSE_COORDINATION}       /* BYZANTINE */
    };

    if (threat_type >= THREAT_COUNT || severity > SWARM_SEVERITY_CRITICAL) {
        return RESPONSE_ALERT;
    }

    return response_matrix[threat_type][severity];
}

nimcp_result_t nimcp_swarm_immune_generate_response(
    NimcpSwarmImmuneSystem* system,
    uint32_t threat_id,
    uint32_t* response_id
) {
    NIMCP_CHECK_THROW(system, NIMCP_INVALID_PARAM, "immune system is NULL");
    NIMCP_CHECK_THROW(response_id, NIMCP_INVALID_PARAM, "response_id output pointer is NULL");

    nimcp_platform_mutex_lock(system->mutex);

    /* Find the threat */
    NimcpSwarmThreat* threat = NULL;
    for (size_t i = 0; i < system->active_threat_count; i++) {
        if (system->active_threats[i].id == threat_id) {
            threat = &system->active_threats[i];
            break;
        }
    }

    if (!threat) {
        nimcp_platform_mutex_unlock(system->mutex);
        return NIMCP_NOT_FOUND;
    }

    /* Check capacity */
    if (system->active_response_count >= system->active_response_capacity) {
        LOG_WARN("Active response capacity reached");
        nimcp_platform_mutex_unlock(system->mutex);
        return NIMCP_NO_MEMORY;
    }

    /* Generate response */
    NimcpSwarmResponse* response = &system->active_responses[system->active_response_count];
    response->id = generate_unique_id();
    response->threat_id = threat_id;
    response->target_drone_id = threat->source_drone_id;
    response->start_time = get_current_time_ms();

    /* Determine response type based on threat */
    response->type = nimcp_swarm_immune_get_response(
        system,
        threat->type,
        threat->severity
    );

    /* Set response parameters based on severity */
    response->intensity = (float)threat->severity / (float)SWARM_SEVERITY_CRITICAL;
    response->duration = (uint64_t)(5000 * (1 + threat->severity)); /* 5-20 seconds */

    /* Determine if coordination needed */
    response->coordinated = (threat->severity >= SWARM_SEVERITY_HIGH) ||
                           (response->type == RESPONSE_COORDINATION);
    response->participating_drones = response->coordinated ? 3 : 1;

    *response_id = response->id;
    system->active_response_count++;

    LOG_INFO("Response generated: id=%u, type=%s, threat=%u, intensity=%.2f",
                   response->id,
                   RESPONSE_TYPE_NAMES[response->type],
                   threat_id,
                   response->intensity);

    nimcp_platform_mutex_unlock(system->mutex);
    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_swarm_immune_execute_response(
    NimcpSwarmImmuneSystem* system,
    uint32_t response_id
) {
    NIMCP_CHECK_THROW(system, NIMCP_INVALID_PARAM, "immune system is NULL");

    nimcp_platform_mutex_lock(system->mutex);

    /* Find the response */
    NimcpSwarmResponse* response = NULL;
    for (size_t i = 0; i < system->active_response_count; i++) {
        if (system->active_responses[i].id == response_id) {
            response = &system->active_responses[i];
            break;
        }
    }

    if (!response) {
        nimcp_platform_mutex_unlock(system->mutex);
        return NIMCP_NOT_FOUND;
    }

    /* Execute response based on type */
    switch (response->type) {
        case RESPONSE_ISOLATION:
            LOG_WARN("EXECUTING: Isolating drone %u", response->target_drone_id);
            /* In real implementation: remove drone from routing tables */
            break;

        case RESPONSE_EVASION:
            LOG_INFO("EXECUTING: Evading threat from drone %u", response->target_drone_id);
            /* In real implementation: adjust position/trajectory */
            break;

        case RESPONSE_COUNTER_ATTACK:
            LOG_WARN("EXECUTING: Counter-attack against drone %u", response->target_drone_id);
            /* In real implementation: active defense measures */
            break;

        case RESPONSE_ALERT:
            LOG_INFO("EXECUTING: Broadcasting alert for threat %u", response->threat_id);
            if (system->config.enable_sharing && system->bio_ctx) {
                nimcp_swarm_immune_broadcast_alert(system, response->threat_id, SWARM_SEVERITY_MEDIUM);
            }
            break;

        case RESPONSE_COORDINATION:
            LOG_INFO("EXECUTING: Coordinating defensive response");
            /* In real implementation: multi-drone coordinated action */
            break;

        case RESPONSE_RECONFIGURATION:
            LOG_INFO("EXECUTING: Reconfiguring network topology");
            /* In real implementation: adjust communication topology */
            break;

        case RESPONSE_AUTHENTICATION:
            LOG_INFO("EXECUTING: Re-authenticating swarm members");
            /* In real implementation: challenge-response protocol */
            break;

        default:
            break;
    }

    nimcp_platform_mutex_unlock(system->mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Clonal Selection (Adaptive Evolution)
 * ============================================================================ */

nimcp_result_t nimcp_swarm_immune_amplify_response(
    NimcpSwarmImmuneSystem* system,
    uint32_t response_id,
    float success_rate
) {
    NIMCP_CHECK_THROW(system, NIMCP_INVALID_PARAM, "immune system is NULL");
    NIMCP_CHECK_THROW(success_rate >= 0.0F && success_rate <= 1.0F, NIMCP_INVALID_PARAM, "success_rate must be in range [0.0, 1.0]");

    nimcp_platform_mutex_lock(system->mutex);

    /* Find the response */
    NimcpSwarmResponse* response = NULL;
    for (size_t i = 0; i < system->active_response_count; i++) {
        if (system->active_responses[i].id == response_id) {
            response = &system->active_responses[i];
            break;
        }
    }

    if (!response) {
        nimcp_platform_mutex_unlock(system->mutex);
        return NIMCP_NOT_FOUND;
    }

    /* Find associated threat and memory cell */
    NimcpSwarmThreat* threat = NULL;
    for (size_t i = 0; i < system->active_threat_count; i++) {
        if (system->active_threats[i].id == response->threat_id) {
            threat = &system->active_threats[i];
            break;
        }
    }

    if (!threat) {
        nimcp_platform_mutex_unlock(system->mutex);
        return NIMCP_NOT_FOUND;
    }

    /* Update memory cell effectiveness (clonal selection) */
    uint32_t cell_to_share = 0;
    bool should_share = false;

    for (size_t i = 0; i < system->memory_cell_count; i++) {
        NimcpSwarmMemoryCell* cell = &system->memory_cells[i];

        if (cell->signature.type == threat->type && cell->response == response->type) {
            /* Amplify effective responses */
            float amplification = system->config.clonal_expansion_rate * success_rate;
            cell->effectiveness = fminf(1.0F, cell->effectiveness * amplification);
            cell->activation_count++;

            LOG_INFO("Amplifying memory cell %u: effectiveness %.2f -> %.2f",
                          cell->id,
                          cell->effectiveness / amplification,
                          cell->effectiveness);

            /* Defer share until after mutex release to avoid deadlock */
            if (success_rate > 0.8F && system->config.enable_sharing && !cell->shared) {
                cell_to_share = cell->id;
                should_share = true;
            }

            break;
        }
    }

    nimcp_platform_mutex_unlock(system->mutex);

    /* Share outside of lock to avoid deadlock */
    if (should_share) {
        nimcp_swarm_immune_share_memory_cell(system, cell_to_share);
    }

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_swarm_immune_adapt_signature(
    NimcpSwarmImmuneSystem* system,
    uint32_t cell_id,
    const uint8_t* new_data,
    size_t new_data_len
) {
    NIMCP_CHECK_THROW(system, NIMCP_INVALID_PARAM, "immune system is NULL");
    NIMCP_CHECK_THROW(new_data, NIMCP_INVALID_PARAM, "new_data pointer is NULL");
    NIMCP_CHECK_THROW(new_data_len > 0, NIMCP_INVALID_PARAM, "new_data_len must be greater than 0");

    nimcp_platform_mutex_lock(system->mutex);

    for (size_t i = 0; i < system->memory_cell_count; i++) {
        if (system->memory_cells[i].id == cell_id) {
            NimcpSwarmMemoryCell* cell = &system->memory_cells[i];

            /* Blend existing pattern with new observation */
            size_t min_len = (cell->signature.pattern_len < new_data_len) ?
                            cell->signature.pattern_len : new_data_len;

            for (size_t j = 0; j < min_len; j++) {
                /* Weighted average */
                cell->signature.pattern[j] = (uint8_t)(
                    0.7F * cell->signature.pattern[j] + 0.3F * new_data[j]
                );
            }

            /* Slightly relax threshold for mutated threats */
            cell->signature.match_threshold *= 0.95F;
            cell->signature.match_threshold = fmaxf(0.5F, cell->signature.match_threshold);

            LOG_INFO("Adapted signature for memory cell %u", cell_id);

            nimcp_platform_mutex_unlock(system->mutex);
            return NIMCP_SUCCESS;
        }
    }

    nimcp_platform_mutex_unlock(system->mutex);
    return NIMCP_NOT_FOUND;
}

static nimcp_result_t nimcp_swarm_immune_affinity_maturation_unlocked(
    NimcpSwarmImmuneSystem* system
) {
    /* Sort memory cells by effectiveness and activation count */
    /* Keep most effective, remove least effective */
    size_t removed = 0;

    for (size_t i = 0; i < system->memory_cell_count; ) {
        NimcpSwarmMemoryCell* cell = &system->memory_cells[i];

        /* Remove low-performing cells */
        if (cell->activation_count > 10 && cell->effectiveness < 0.3F) {
            system->memory_cells[i] = system->memory_cells[system->memory_cell_count - 1];
            system->memory_cell_count--;
            removed++;
        } else {
            i++;
        }
    }

    if (removed > 0) {
        LOG_INFO("Affinity maturation: removed %zu low-affinity cells", removed);
    }

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_swarm_immune_affinity_maturation(
    NimcpSwarmImmuneSystem* system
) {
    NIMCP_CHECK_THROW(system, NIMCP_INVALID_PARAM, "immune system is NULL");

    nimcp_platform_mutex_lock(system->mutex);
    nimcp_result_t result = nimcp_swarm_immune_affinity_maturation_unlocked(system);
    nimcp_platform_mutex_unlock(system->mutex);
    return result;
}

/* ============================================================================
 * Swarm Coordination (via Bio-Async)
 * ============================================================================ */

nimcp_result_t nimcp_swarm_immune_share_threat(
    NimcpSwarmImmuneSystem* system,
    uint32_t threat_id
) {
    NIMCP_CHECK_THROW(system, NIMCP_INVALID_PARAM, "immune system is NULL");

    if (!system->config.enable_sharing || !system->bio_ctx) {
        return NIMCP_SUCCESS; /* Sharing disabled */
    }

    nimcp_platform_mutex_lock(system->mutex);

    /* Find threat */
    NimcpSwarmThreat* threat = NULL;
    for (size_t i = 0; i < system->active_threat_count; i++) {
        if (system->active_threats[i].id == threat_id) {
            threat = &system->active_threats[i];
            break;
        }
    }

    if (!threat) {
        nimcp_platform_mutex_unlock(system->mutex);
        return NIMCP_NOT_FOUND;
    }

    /* Bio-async threat sharing stubbed - requires brain integration */
    (void)threat;  /* Suppress unused warning */
    LOG_INFO("Shared threat %u with swarm (stubbed)", threat_id);

    nimcp_platform_mutex_unlock(system->mutex);
    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_swarm_immune_share_memory_cell(
    NimcpSwarmImmuneSystem* system,
    uint32_t cell_id
) {
    NIMCP_CHECK_THROW(system, NIMCP_INVALID_PARAM, "immune system is NULL");

    if (!system->config.enable_sharing || !system->bio_ctx) {
        return NIMCP_SUCCESS;
    }

    nimcp_platform_mutex_lock(system->mutex);

    for (size_t i = 0; i < system->memory_cell_count; i++) {
        if (system->memory_cells[i].id == cell_id) {
            NimcpSwarmMemoryCell* cell = &system->memory_cells[i];

            /* Bio-async memory cell sharing stubbed - requires brain integration */
            cell->shared = true;
            LOG_INFO("Shared memory cell %u with swarm (stubbed)", cell_id);

            nimcp_platform_mutex_unlock(system->mutex);
            return NIMCP_SUCCESS;
        }
    }

    nimcp_platform_mutex_unlock(system->mutex);
    return NIMCP_NOT_FOUND;
}

nimcp_result_t nimcp_swarm_immune_coordinate_response(
    NimcpSwarmImmuneSystem* system,
    uint32_t response_id,
    const uint32_t* participating_drones,
    size_t num_drones
) {
    NIMCP_CHECK_THROW(system, NIMCP_INVALID_PARAM, "immune system is NULL");
    NIMCP_CHECK_THROW(participating_drones, NIMCP_INVALID_PARAM, "participating_drones is NULL");

    if (!system->config.enable_coordination || !system->bio_ctx) {
        return NIMCP_SUCCESS;
    }

    nimcp_platform_mutex_lock(system->mutex);

    /* Find response */
    NimcpSwarmResponse* response = NULL;
    for (size_t i = 0; i < system->active_response_count; i++) {
        if (system->active_responses[i].id == response_id) {
            response = &system->active_responses[i];
            break;
        }
    }

    if (!response) {
        nimcp_platform_mutex_unlock(system->mutex);
        return NIMCP_NOT_FOUND;
    }

    /* Bio-async coordination stubbed - requires brain integration */
    response->participating_drones = (uint32_t)num_drones;
    LOG_INFO("Coordinated response %u with %zu drones (stubbed)", response_id, num_drones);

    nimcp_platform_mutex_unlock(system->mutex);
    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_swarm_immune_broadcast_alert(
    NimcpSwarmImmuneSystem* system,
    uint32_t threat_id,
    NimcpSwarmSeverity priority
) {
    NIMCP_CHECK_THROW(system, NIMCP_INVALID_PARAM, "immune system is NULL");

    if (!system->bio_ctx) {
        return NIMCP_SUCCESS;
    }

    nimcp_platform_mutex_lock(system->mutex);

    /* Find threat */
    NimcpSwarmThreat* threat = NULL;
    for (size_t i = 0; i < system->active_threat_count; i++) {
        if (system->active_threats[i].id == threat_id) {
            threat = &system->active_threats[i];
            break;
        }
    }

    if (!threat) {
        nimcp_platform_mutex_unlock(system->mutex);
        return NIMCP_NOT_FOUND;
    }

    /* Bio-async alert broadcast stubbed - requires brain integration */
    (void)priority;  /* Suppress unused warning */
    (void)threat;    /* Suppress unused warning */
    LOG_WARN("Broadcast alert for threat %u (stubbed)", threat_id);

    nimcp_platform_mutex_unlock(system->mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Threat Management
 * ============================================================================ */

nimcp_result_t nimcp_swarm_immune_confirm_threat(
    NimcpSwarmImmuneSystem* system,
    uint32_t threat_id,
    uint32_t confirming_drone_id
) {
    NIMCP_CHECK_THROW(system, NIMCP_INVALID_PARAM, "immune system is NULL");

    nimcp_platform_mutex_lock(system->mutex);

    for (size_t i = 0; i < system->active_threat_count; i++) {
        if (system->active_threats[i].id == threat_id) {
            NimcpSwarmThreat* threat = &system->active_threats[i];

            threat->confirming_drones++;

            if (threat->confirming_drones >= system->config.confirmation_threshold) {
                threat->confirmed = true;
                LOG_WARN("Threat %u confirmed by %u drones",
                              threat_id, threat->confirming_drones);
            }

            nimcp_platform_mutex_unlock(system->mutex);
            return NIMCP_SUCCESS;
        }
    }

    nimcp_platform_mutex_unlock(system->mutex);
    return NIMCP_NOT_FOUND;
}

nimcp_result_t nimcp_swarm_immune_neutralize_threat(
    NimcpSwarmImmuneSystem* system,
    uint32_t threat_id
) {
    NIMCP_CHECK_THROW(system, NIMCP_INVALID_PARAM, "immune system is NULL");

    nimcp_platform_mutex_lock(system->mutex);

    for (size_t i = 0; i < system->active_threat_count; i++) {
        if (system->active_threats[i].id == threat_id) {
            /* Remove threat by swapping with last */
            system->active_threats[i] = system->active_threats[system->active_threat_count - 1];
            system->active_threat_count--;
            system->total_threats_neutralized++;

            LOG_INFO("Threat %u neutralized", threat_id);

            nimcp_platform_mutex_unlock(system->mutex);
            return NIMCP_SUCCESS;
        }
    }

    nimcp_platform_mutex_unlock(system->mutex);
    return NIMCP_NOT_FOUND;
}

nimcp_result_t nimcp_swarm_immune_get_threat(
    NimcpSwarmImmuneSystem* system,
    uint32_t threat_id,
    const NimcpSwarmThreat** threat
) {
    NIMCP_CHECK_THROW(system, NIMCP_INVALID_PARAM, "immune system is NULL");
    NIMCP_CHECK_THROW(threat, NIMCP_INVALID_PARAM, "threat output pointer is NULL");

    nimcp_platform_mutex_lock(system->mutex);

    for (size_t i = 0; i < system->active_threat_count; i++) {
        if (system->active_threats[i].id == threat_id) {
            *threat = &system->active_threats[i];
            nimcp_platform_mutex_unlock(system->mutex);
            return NIMCP_SUCCESS;
        }
    }

    nimcp_platform_mutex_unlock(system->mutex);
    return NIMCP_NOT_FOUND;
}

/* ============================================================================
 * Statistics and Monitoring
 * ============================================================================ */

nimcp_result_t nimcp_swarm_immune_get_stats(
    NimcpSwarmImmuneSystem* system,
    uint64_t* total_threats,
    uint64_t* neutralized,
    uint64_t* false_positives
) {
    NIMCP_CHECK_THROW(system, NIMCP_INVALID_PARAM, "immune system is NULL");
    NIMCP_CHECK_THROW(total_threats, NIMCP_INVALID_PARAM, "total_threats output pointer is NULL");
    NIMCP_CHECK_THROW(neutralized, NIMCP_INVALID_PARAM, "neutralized output pointer is NULL");
    NIMCP_CHECK_THROW(false_positives, NIMCP_INVALID_PARAM, "false_positives output pointer is NULL");

    nimcp_platform_mutex_lock(system->mutex);

    *total_threats = system->total_threats_detected;
    *neutralized = system->total_threats_neutralized;
    *false_positives = system->false_positive_count;

    nimcp_platform_mutex_unlock(system->mutex);
    return NIMCP_SUCCESS;
}

const char* nimcp_swarm_threat_type_name(NimcpSwarmThreatType type) {
    if (type < THREAT_COUNT) {
        return THREAT_TYPE_NAMES[type];
    }
    return "UNKNOWN";
}

const char* nimcp_swarm_response_type_name(NimcpSwarmResponseType type) {
    if (type < RESPONSE_COUNT) {
        return RESPONSE_TYPE_NAMES[type];
    }
    return "UNKNOWN";
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

nimcp_result_t nimcp_swarm_immune_update(
    NimcpSwarmImmuneSystem* system,
    uint64_t current_time
) {
    NIMCP_CHECK_THROW(system, NIMCP_INVALID_PARAM, "immune system is NULL");

    nimcp_platform_mutex_lock(system->mutex);

    /* Process expired responses */
    for (size_t i = 0; i < system->active_response_count; ) {
        NimcpSwarmResponse* response = &system->active_responses[i];

        if (current_time - response->start_time >= response->duration) {
            /* Remove expired response */
            system->active_responses[i] = system->active_responses[system->active_response_count - 1];
            system->active_response_count--;
        } else {
            i++;
        }
    }

    /* Apply memory decay - use unlocked version since we already hold mutex */
    nimcp_swarm_immune_decay_memory_unlocked(system, current_time);

    /* Perform affinity maturation periodically - move static to system state */
    if (current_time - system->last_maturation_time > 60000) { /* Every minute */
        nimcp_swarm_immune_affinity_maturation_unlocked(system);
        system->last_maturation_time = current_time;
    }

    nimcp_platform_mutex_unlock(system->mutex);
    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_swarm_immune_reset(
    NimcpSwarmImmuneSystem* system,
    bool preserve_memory
) {
    NIMCP_CHECK_THROW(system, NIMCP_INVALID_PARAM, "immune system is NULL");

    nimcp_platform_mutex_lock(system->mutex);

    /* Clear active threats and responses */
    system->active_threat_count = 0;
    system->active_response_count = 0;

    /* Optionally clear memory cells */
    if (!preserve_memory) {
        system->memory_cell_count = 0;
    }

    /* Clear behavior profiles */
    system->behavior_profile_count = 0;

    /* Reset statistics */
    system->total_threats_detected = 0;
    system->total_threats_neutralized = 0;
    system->false_positive_count = 0;

    LOG_INFO("Immune system reset (preserve_memory=%s)",
                   preserve_memory ? "true" : "false");

    nimcp_platform_mutex_unlock(system->mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Logic-Based Threat Detection Implementation
 * ============================================================================ */

/**
 * @brief Add threat detection rule
 *
 * WHAT: Adds logic-based rule for threat detection
 * WHY:  Enable compositional threat detection from multiple signals
 * HOW:  Uses logic gates to combine detector outputs
 */
nimcp_result_t immune_add_threat_rule(
    NimcpSwarmImmuneSystem* immune,
    const immune_threat_rule_t* rule
) {
    NIMCP_CHECK_THROW(immune, NIMCP_INVALID_PARAM, "immune system is NULL");
    NIMCP_CHECK_THROW(rule, NIMCP_INVALID_PARAM, "threat rule is NULL");

    /* Validate enum parameter */
    if (!NIMCP_VALIDATE_ENUM(rule->threat_type, THREAT_COUNT)) {
        LOG_ERROR("Invalid threat type in rule: %d", rule->threat_type);
        return NIMCP_INVALID_PARAM;
    }

    /* Guard clause: Check capacity */
    if (immune->threat_rule_count >= immune->threat_rule_capacity) {
        LOG_WARN("Threat rule capacity reached");
        return NIMCP_NO_MEMORY;
    }

    nimcp_platform_mutex_lock(immune->mutex);

    /* Copy rule */
    immune_threat_rule_t* new_rule = &immune->threat_rules[immune->threat_rule_count];
    new_rule->threat_id = rule->threat_id;
    new_rule->detection_logic = rule->detection_logic;
    new_rule->num_sources = rule->num_sources;
    new_rule->confidence_threshold = rule->confidence_threshold;
    new_rule->threat_type = rule->threat_type;

    /* Allocate and copy signal sources */
    if (rule->num_sources > 0 && rule->signal_sources) {
        new_rule->signal_sources = (uint32_t*)nimcp_malloc(
            rule->num_sources * sizeof(uint32_t)
        );
        if (!new_rule->signal_sources) {
            nimcp_platform_mutex_unlock(immune->mutex);
            LOG_ERROR("Failed to allocate signal sources");
            return NIMCP_NO_MEMORY;
        }
        memcpy(new_rule->signal_sources, rule->signal_sources,
               rule->num_sources * sizeof(uint32_t));
    } else {
        new_rule->signal_sources = NULL;
    }

    immune->threat_rule_count++;

    LOG_INFO("Threat rule added: id=%u, logic=%d, sources=%u, type=%s",
             new_rule->threat_id,
             new_rule->detection_logic,
             new_rule->num_sources,
             THREAT_TYPE_NAMES[new_rule->threat_type]);

    nimcp_platform_mutex_unlock(immune->mutex);
    return NIMCP_SUCCESS;
}

/**
 * @brief Evaluate threats using logic gates
 *
 * WHAT: Evaluates all threat rules using neural logic
 * WHY:  Detect complex multi-signal threat patterns
 * HOW:  Evaluates logic gates over detector signals
 */
nimcp_result_t immune_evaluate_threats(
    NimcpSwarmImmuneSystem* immune,
    float* threat_scores,
    uint32_t* num_threats
) {
    NIMCP_CHECK_THROW(immune, NIMCP_INVALID_PARAM, "immune system is NULL");
    NIMCP_CHECK_THROW(threat_scores, NIMCP_INVALID_PARAM, "threat_scores output array is NULL");
    NIMCP_CHECK_THROW(num_threats, NIMCP_INVALID_PARAM, "num_threats output pointer is NULL");

    nimcp_platform_mutex_lock(immune->mutex);

    *num_threats = 0;

    /* Evaluate each threat rule */
    for (size_t i = 0; i < immune->threat_rule_count; i++) {
        immune_threat_rule_t* rule = &immune->threat_rules[i];

        /* Validate threat type before array access */
        if (!NIMCP_VALIDATE_ENUM(rule->threat_type, THREAT_COUNT)) {
            LOG_ERROR("Invalid threat type in rule %zu: %d", i, rule->threat_type);
            continue;
        }

        /* Guard clause: Skip rules with no sources */
        if (rule->num_sources == 0 || !rule->signal_sources) {
            continue;
        }

        /* Gather signals from sources (using anomaly scores from behavior profiles) */
        float signal_values[256] = {0};
        uint32_t valid_signals = 0;

        for (uint32_t s = 0; s < rule->num_sources && s < 256; s++) {
            uint32_t source_id = rule->signal_sources[s];

            /* Find behavior profile for this source */
            for (size_t p = 0; p < immune->behavior_profile_count; p++) {
                if (immune->behavior_profiles[p].drone_id == source_id) {
                    signal_values[s] = immune->behavior_profiles[p].anomaly_score;
                    valid_signals++;
                    break;
                }
            }
        }

        /* Guard clause: Need at least one valid signal */
        if (valid_signals == 0) {
            threat_scores[i] = 0.0F;
            continue;
        }

        /* Evaluate detection logic */
        float threat_score = 0.0F;

        switch (rule->detection_logic) {
            case LOGIC_GATE_AND:
                /* All sources must detect threat */
                threat_score = 1.0F;
                for (uint32_t s = 0; s < rule->num_sources && s < 256; s++) {
                    if (signal_values[s] < threat_score) {
                        threat_score = signal_values[s];
                    }
                }
                break;

            case LOGIC_GATE_OR:
                /* Any source detecting triggers threat */
                threat_score = 0.0F;
                for (uint32_t s = 0; s < rule->num_sources && s < 256; s++) {
                    if (signal_values[s] > threat_score) {
                        threat_score = signal_values[s];
                    }
                }
                break;

            case LOGIC_GATE_NOT:
                /* Absence of expected signal = threat */
                /* Average signal should be low for NOT to be high */
                {
                    float avg_signal = 0.0F;
                    for (uint32_t s = 0; s < rule->num_sources && s < 256; s++) {
                        avg_signal += signal_values[s];
                    }
                    avg_signal /= (float)rule->num_sources;
                    threat_score = 1.0F - avg_signal; /* Invert */
                }
                break;

            case LOGIC_GATE_IMPLIES:
                /* If first signal, then others must follow */
                if (rule->num_sources >= 2) {
                    float antecedent = signal_values[0];
                    float consequent = signal_values[1];

                    /* IMPLIES: A → B = ¬A ∨ B */
                    if (antecedent >= 0.5F && consequent < 0.5F) {
                        /* Implication violated = threat */
                        threat_score = 1.0F;
                    } else {
                        threat_score = 0.0F;
                    }
                }
                break;

            default:
                /* Average of signals */
                for (uint32_t s = 0; s < rule->num_sources && s < 256; s++) {
                    threat_score += signal_values[s];
                }
                threat_score /= (float)rule->num_sources;
                break;
        }

        threat_scores[i] = threat_score;

        /* Check if threat score exceeds threshold */
        if (threat_score >= rule->confidence_threshold) {
            (*num_threats)++;
            LOG_WARN("Threat detected: rule_id=%u, type=%s, score=%.2f",
                     rule->threat_id,
                     THREAT_TYPE_NAMES[rule->threat_type],
                     threat_score);
        }
    }

    nimcp_platform_mutex_unlock(immune->mutex);

    if (*num_threats > 0) {
        LOG_INFO("Threat evaluation complete: %u threats detected", *num_threats);
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Generate logic-based immune response
 *
 * WHAT: Determines response strategy using logic evaluation
 * WHY:  Choose appropriate response based on threat logic
 * HOW:  Evaluates response rules with IMPLIES/OR/NOT gates
 */
nimcp_result_t immune_logic_response(
    NimcpSwarmImmuneSystem* immune,
    uint32_t threat_id,
    immune_response_t* response
) {
    NIMCP_CHECK_THROW(immune, NIMCP_INVALID_PARAM, "immune system is NULL");
    NIMCP_CHECK_THROW(response, NIMCP_INVALID_PARAM, "response output pointer is NULL");

    nimcp_platform_mutex_lock(immune->mutex);

    /* Find the threat */
    NimcpSwarmThreat* threat = NULL;
    for (size_t i = 0; i < immune->active_threat_count; i++) {
        if (immune->active_threats[i].id == threat_id) {
            threat = &immune->active_threats[i];
            break;
        }
    }

    /* Guard clause: Threat not found */
    if (!threat) {
        nimcp_platform_mutex_unlock(immune->mutex);
        LOG_WARN("Threat %u not found for logic response", threat_id);
        return NIMCP_NOT_FOUND;
    }

    /* Determine response using logic evaluation */
    response->response_id = generate_unique_id();
    response->response_logic = LOGIC_GATE_IMPLIES;
    response->response_type = nimcp_swarm_immune_get_response(
        immune,
        threat->type,
        threat->severity
    );
    response->intensity = threat->confidence;
    response->requires_coordination = (threat->severity >= SWARM_SEVERITY_HIGH);

    /* Use IMPLIES logic: If threat_severe THEN coordinated_response */
    if (threat->severity >= SWARM_SEVERITY_HIGH) {
        /* High severity implies coordination required */
        response->requires_coordination = true;
        LOG_INFO("Logic response: IMPLIES (high_severity → coordination)");
    } else {
        /* Low severity: individual response sufficient */
        response->requires_coordination = false;
        LOG_INFO("Logic response: Individual response sufficient");
    }

    LOG_INFO("Generated logic response: id=%u, type=%s, intensity=%.2f, coordinated=%s",
             response->response_id,
             RESPONSE_TYPE_NAMES[response->response_type],
             response->intensity,
             response->requires_coordination ? "YES" : "NO");

    nimcp_platform_mutex_unlock(immune->mutex);
    return NIMCP_SUCCESS;
}

/**
 * @brief Integrate with Blood-Brain-Barrier for threat messaging
 *
 * WHAT: Sends threat alerts via BBB bio-async messaging
 * WHY:  Coordinate immune response across swarm
 * HOW:  Uses BBB_MSG_THREAT message type
 */
nimcp_result_t immune_send_bbb_threat_alert(
    NimcpSwarmImmuneSystem* immune,
    uint32_t threat_id,
    NimcpSwarmSeverity priority
) {
    NIMCP_CHECK_THROW(immune, NIMCP_INVALID_PARAM, "immune system is NULL");

    nimcp_platform_mutex_lock(immune->mutex);

    /* Find threat first - must exist regardless of bio_ctx availability */
    NimcpSwarmThreat* threat = NULL;
    for (size_t i = 0; i < immune->active_threat_count; i++) {
        if (immune->active_threats[i].id == threat_id) {
            threat = &immune->active_threats[i];
            break;
        }
    }

    if (!threat) {
        nimcp_platform_mutex_unlock(immune->mutex);
        return NIMCP_NOT_FOUND;
    }

    /* Guard clause: Bio-async not available - threat exists but can't send alert */
    if (!immune->bio_ctx) {
        LOG_DEBUG("Bio-async not available for BBB threat alert");
        nimcp_platform_mutex_unlock(immune->mutex);
        return NIMCP_SUCCESS;
    }

    /* Validate threat type before array access */
    if (!NIMCP_VALIDATE_ENUM(threat->type, THREAT_COUNT)) {
        LOG_ERROR("Invalid threat type for BBB alert: %d", threat->type);
        nimcp_platform_mutex_unlock(immune->mutex);
        return NIMCP_INVALID_PARAM;
    }

    LOG_INFO("BBB Threat Alert: id=%u, type=%s, priority=%d",
             threat_id,
             THREAT_TYPE_NAMES[threat->type],
             priority);

    /* Send BIO_MSG_SWARM_IMMUNE_ALERT via bio-async router so that the
     * brain immune system and security modules receive the swarm threat.
     * bio_ctx is bio_module_context_t* (pointer-to-pointer since
     * bio_module_context_t is already a pointer typedef). */
    if (immune->bio_ctx && *immune->bio_ctx) {
        struct {
            bio_message_header_t hdr;
            uint32_t threat_id;
            uint32_t threat_type;
            uint32_t severity;
            uint32_t source_drone_id;
            float    confidence;
        } alert_msg = {0};
        alert_msg.hdr.type          = BIO_MSG_SWARM_IMMUNE_ALERT;
        alert_msg.hdr.source_module = BIO_MODULE_IMMUNE_BRAIN;
        alert_msg.hdr.target_module = 0; /* broadcast */
        alert_msg.hdr.payload_size  = sizeof(alert_msg) - sizeof(bio_message_header_t);
        alert_msg.hdr.flags         = BIO_MSG_FLAG_URGENT | BIO_MSG_FLAG_BROADCAST;
        alert_msg.threat_id         = threat->id;
        alert_msg.threat_type       = (uint32_t)threat->type;
        alert_msg.severity          = (uint32_t)priority;
        alert_msg.source_drone_id   = threat->source_drone_id;
        alert_msg.confidence        = threat->confidence;

        nimcp_error_t send_err = bio_router_broadcast(
            *immune->bio_ctx, &alert_msg, sizeof(alert_msg));
        if (send_err != NIMCP_SUCCESS) {
            LOG_WARN("Failed to broadcast swarm immune alert: %d", send_err);
        }
    }

    nimcp_platform_mutex_unlock(immune->mutex);
    return NIMCP_SUCCESS;
}

/**
 * @brief Evaluate NOT gate threat logic
 *
 * WHAT: Detects threats by absence of expected signals
 * WHY:  Identify silent failures or missing heartbeats
 * HOW:  Uses NOT gate to invert signal presence
 */
nimcp_result_t immune_evaluate_not_threat(
    NimcpSwarmImmuneSystem* immune,
    uint32_t expected_signal,
    uint64_t time_window,
    bool* threat_detected
) {
    NIMCP_CHECK_THROW(immune, NIMCP_INVALID_PARAM, "immune system is NULL");
    NIMCP_CHECK_THROW(threat_detected, NIMCP_INVALID_PARAM, "threat_detected output pointer is NULL");

    nimcp_platform_mutex_lock(immune->mutex);

    uint64_t current_time = get_current_time_ms();
    *threat_detected = false;

    /* Check if expected signal is present in recent behavior profiles */
    bool signal_present = false;

    for (size_t i = 0; i < immune->behavior_profile_count; i++) {
        NimcpSwarmBehaviorProfile* profile = &immune->behavior_profiles[i];

        /* Check if this drone matches expected signal source */
        if (profile->drone_id == expected_signal) {
            uint64_t time_diff = current_time - profile->last_update;

            /* Signal present if updated within time window */
            if (time_diff < time_window) {
                signal_present = true;
                break;
            }
        }
    }

    /* NOT gate: threat detected if signal NOT present */
    *threat_detected = !signal_present;

    if (*threat_detected) {
        LOG_WARN("NOT threat detected: expected signal %u absent for %lu ms",
                 expected_signal, time_window);
    } else {
        LOG_DEBUG("NOT threat evaluation: signal %u present", expected_signal);
    }

    nimcp_platform_mutex_unlock(immune->mutex);
    return NIMCP_SUCCESS;
}
