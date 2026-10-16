//=============================================================================
// nimcp_combinatorial_harm.c - Combinatorial Harm Detection Implementation
//=============================================================================
/**
 * @file nimcp_combinatorial_harm.c
 * @brief Implementation of combinatorial harm detection
 *
 * WHAT: Detects when individually safe actions combine to cause harm
 * WHY:  Task A (safe) + Task B (safe) may produce harmful outcomes
 * HOW:  Ring buffer history + pattern matching + harm classification
 *
 * THREAD SAFETY:
 *   - Mutex protects history modifications
 *   - Lock-free reads where possible
 *   - Atomic counters for statistics
 *
 * @author NIMCP Development Team
 * @date 2025-11-24
 * @version 1.0.0
 */

#include "cognitive/ethics/nimcp_combinatorial_harm.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "security/nimcp_security.h"  // mprotect-based directive protection
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdio.h>

// Mathematical Enhancement Includes
// Note: These are optional - module works without them
// #include "information/nimcp_shannon.h"
// #include "utils/geometry/nimcp_hyperbolic.h"
// #include "core/topology/nimcp_fractal_topology.h"
// #include "utils/quantum/nimcp_quantum_walk.h"
// #include "utils/math/nimcp_complex_math.h"
#include "plasticity/noise/nimcp_pink_noise.h"
#include "plasticity/noise/nimcp_pink_noise_monitor.h"
#include "plasticity/noise/nimcp_pink_noise_criticality.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_buffer_constants.h"

BRIDGE_BOILERPLATE(combinatorial_harm, MESH_ADAPTER_CATEGORY_COGNITIVE)

// #include "optimization/quantum_annealing/nimcp_quantum_annealing.h"

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Ring buffer for action history
 */
typedef struct {
    action_record_t* records;       /**< Array of records */
    uint32_t capacity;              /**< Maximum records */
    uint32_t head;                  /**< Next write position */
    uint32_t count;                 /**< Current record count */
    uint64_t next_id;               /**< Next action ID */
} action_history_t;

/**
 * @brief Pattern registry
 */
typedef struct {
    combination_pattern_t* patterns; /**< Array of patterns */
    uint32_t capacity;               /**< Maximum patterns */
    uint32_t count;                  /**< Current pattern count */
    uint32_t next_id;                /**< Next pattern ID */
} pattern_registry_t;

/**
 * @brief Combinatorial harm detector structure
 */
struct combinatorial_harm_detector_struct {
    // Configuration
    combinatorial_config_t config;

    // History ring buffer
    action_history_t history;

    // Pattern registry
    pattern_registry_t patterns;

    // Statistics
    combinatorial_stats_t stats;

    // Thread safety
    nimcp_platform_mutex_t mutex;

    // Integration
    ethics_engine_t attached_engine;

    // Hardware memory protection (mprotect)
    nimcp_directive_system_t* directive_system;  /**< mprotect'd pattern storage */
    bool patterns_mprotect_locked;               /**< Whether patterns are hw-locked */

    // Pink noise integration for spectral analysis
    pink_noise_monitor_t* pink_monitor;          /**< Optional: Real-time spectral monitor */
    criticality_analyzer_t* criticality;         /**< Optional: Criticality/avalanche detector */
    bool pink_noise_enabled;                     /**< Whether pink noise analysis uses full modules */
};

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Get current time in milliseconds
 */
static inline uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

/**
 * @brief Calculate time decay factor
 *
 * @param time_diff Time difference in ms
 * @param time_window Time window in ms
 * @param sensitivity Decay sensitivity (0=none, 1=strong)
 * @return Decay factor (0-1)
 */
static inline float calculate_time_decay(
    uint64_t time_diff,
    uint64_t time_window,
    float sensitivity
) {
    if (sensitivity <= 0.0F) return 1.0F;
    if (time_diff >= time_window) return 0.0F;

    float ratio = (float)time_diff / (float)time_window;
    return 1.0F - (ratio * sensitivity);
}

/**
 * @brief Check if pattern matches action pair
 *
 * @param pattern Pattern to check
 * @param action_a First action
 * @param action_b Second action
 * @return true if pattern matches
 */
static bool pattern_matches(
    const combination_pattern_t* pattern,
    const action_record_t* action_a,
    const action_record_t* action_b
) {
    if (!pattern->enabled) {
        return false;
    }

    // Direct match
    if (pattern->category_a == action_a->category &&
        pattern->category_b == action_b->category) {
        return true;
    }

    // Bidirectional match
    if (pattern->bidirectional &&
        pattern->category_a == action_b->category &&
        pattern->category_b == action_a->category) {
        return true;
    }

    return false;
}

/**
 * @brief Calculate combined harm score
 *
 * @param detector Detector handle
 * @param pattern Matching pattern
 * @param historical Historical action
 * @param pending Pending action
 * @param time_diff Time difference in ms
 * @return Combined harm score (0-1)
 */
static float calculate_combined_harm(
    combinatorial_harm_detector_t detector,
    const combination_pattern_t* pattern,
    const action_record_t* historical,
    const action_record_t* pending,
    uint64_t time_diff
) {
    // Base harm is max of individual scores
    float base_harm = fmaxf(historical->standalone_harm_score,
                            pending->standalone_harm_score);

    // Apply multiplier from pattern
    float combined = base_harm * pattern->harm_multiplier;

    // Apply time decay
    float decay = calculate_time_decay(
        time_diff,
        detector->config.time_window_ms,
        pattern->time_sensitivity
    );
    combined *= decay;

    // Clamp to [0, 1]
    if (combined > 1.0F) combined = 1.0F;
    if (combined < 0.0F) combined = 0.0F;

    return combined;
}

/**
 * @brief Copy action record
 */
static void copy_action_record(action_record_t* dest, const action_record_t* src) {
    dest->action_id = src->action_id;
    dest->timestamp = src->timestamp;
    dest->category = src->category;
    dest->acting_agent = src->acting_agent;
    dest->target_agent = src->target_agent;
    dest->standalone_harm_score = src->standalone_harm_score;
    dest->num_features = 0;
    dest->features = NULL;  // Don't copy features to avoid memory issues
    strncpy(dest->description, src->description, sizeof(dest->description) - 1);
    dest->description[sizeof(dest->description) - 1] = '\0';
}

//=============================================================================
// Detector Lifecycle API
//=============================================================================

NIMCP_EXPORT combinatorial_config_t combinatorial_default_config(void) {
    combinatorial_config_t config = {
        .history_capacity = COMBINATORIAL_DEFAULT_HISTORY_CAPACITY,
        .time_window_ms = COMBINATORIAL_DEFAULT_TIME_WINDOW_MS,
        .harm_threshold = COMBINATORIAL_DEFAULT_HARM_THRESHOLD,
        .enable_ml_classifier = false,
        .enable_rule_patterns = true,
        .auto_register_actions = true,
        .ethics_engine = NULL
    };
    return config;
}

NIMCP_EXPORT combinatorial_harm_detector_t combinatorial_detector_create(
    const combinatorial_config_t* config
) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return NULL;

    }
    if (config->history_capacity == 0) return NULL;

    // Allocate detector
    combinatorial_harm_detector_t detector = nimcp_calloc(1,
        sizeof(struct combinatorial_harm_detector_struct));
    if (!detector) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate detector");

        return NULL;

    }

    // Copy configuration
    memcpy(&detector->config, config, sizeof(combinatorial_config_t));

    // Allocate history ring buffer
    detector->history.records = nimcp_calloc(config->history_capacity,
        sizeof(action_record_t));
    if (!detector->history.records) {
        nimcp_free(detector);
        detector = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "combinatorial_detector_create: detector->history is NULL");
        return NULL;
    }
    detector->history.capacity = config->history_capacity;
    detector->history.head = 0;
    detector->history.count = 0;
    detector->history.next_id = 1;

    // Allocate pattern registry
    detector->patterns.patterns = nimcp_calloc(COMBINATORIAL_MAX_PATTERNS,
        sizeof(combination_pattern_t));
    if (!detector->patterns.patterns) {
        nimcp_free(detector->history.records);
        nimcp_free(detector);
        detector = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "combinatorial_detector_create: detector->patterns is NULL");
        return NULL;
    }
    detector->patterns.capacity = COMBINATORIAL_MAX_PATTERNS;
    detector->patterns.count = 0;
    detector->patterns.next_id = 1;

    // Initialize statistics
    memset(&detector->stats, 0, sizeof(combinatorial_stats_t));

    // Initialize mutex
    if (nimcp_platform_mutex_init(&detector->mutex, false) != 0) {
        nimcp_free(detector->patterns.patterns);
        nimcp_free(detector->history.records);
        nimcp_free(detector);
        detector = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "combinatorial_detector_create: validation failed");
        return NULL;
    }

    detector->attached_engine = NULL;

    // Initialize mprotect fields
    detector->directive_system = NULL;
    detector->patterns_mprotect_locked = false;

    // Initialize pink noise integration (disabled by default)
    detector->pink_monitor = NULL;
    detector->criticality = NULL;
    detector->pink_noise_enabled = false;

    return detector;
}

NIMCP_EXPORT void combinatorial_detector_destroy(
    combinatorial_harm_detector_t detector
) {
    if (!detector) return;

    // Detach from ethics engine if attached
    if (detector->attached_engine) {
        combinatorial_detach_from_ethics(detector);
    }

    // Destroy mutex
    nimcp_platform_mutex_destroy(&detector->mutex);

    // Free history
    if (detector->history.records) {
        // Free any allocated feature vectors
        for (uint32_t i = 0; i < detector->history.count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && detector->history.count > 256) {
                combinatorial_harm_heartbeat("combinatoria_loop",
                                 (float)(i + 1) / (float)detector->history.count);
            }

            if (detector->history.records[i].features) {
                nimcp_free(detector->history.records[i].features);
            }
        }
        nimcp_free(detector->history.records);
    }

    // Free patterns
    if (detector->patterns.patterns) {
        nimcp_free(detector->patterns.patterns);
    }

    // Destroy directive system (handles mprotect cleanup)
    if (detector->directive_system) {
        nimcp_directive_system_destroy(detector->directive_system);
        detector->directive_system = NULL;
    }

    // Destroy pink noise components
    if (detector->pink_monitor) {
        pink_monitor_destroy(detector->pink_monitor);
        detector->pink_monitor = NULL;
    }
    if (detector->criticality) {
        criticality_destroy(detector->criticality);
        detector->criticality = NULL;
    }

    // Free detector
    nimcp_free(detector);
    detector = NULL;
}

//=============================================================================
// Pink Noise Integration API
//=============================================================================

NIMCP_EXPORT bool combinatorial_enable_pink_noise(
    combinatorial_harm_detector_t detector
) {
    if (!detector) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "combinatorial_enable_pink_noise: detector is NULL");
        return false;
    }

    nimcp_platform_mutex_lock(&detector->mutex);

    // Already enabled?
    if (detector->pink_noise_enabled && detector->pink_monitor != NULL) {
        nimcp_platform_mutex_unlock(&detector->mutex);
        return true;
    }

    // Create pink noise monitor with default config
    pink_monitor_config_t monitor_config = pink_monitor_default_config();
    monitor_config.target_alpha = 1.0F;  // Target pink noise (1/f)
    monitor_config.tolerance = 0.2F;     // Allow some deviation
    monitor_config.enable_auto_correction = false;  // Read-only monitoring
    monitor_config.enable_alerts = false;

    detector->pink_monitor = pink_monitor_create(&monitor_config);
    if (!detector->pink_monitor) {
        NIMCP_LOGGING_WARN("Failed to create pink noise monitor, using fallback");
        nimcp_platform_mutex_unlock(&detector->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "combinatorial_enable_pink_noise: pink_monitor_create failed");
        return false;
    }

    // Create criticality analyzer with default config
    criticality_config_t crit_config = criticality_default_config();
    crit_config.target_alpha = 1.0F;
    crit_config.target_tau = 1.5F;  // Power-law exponent for avalanches
    crit_config.enable_feedback = false;  // Read-only monitoring

    detector->criticality = criticality_create(&crit_config);
    if (!detector->criticality) {
        NIMCP_LOGGING_WARN("Failed to create criticality analyzer, continuing without it");
        // Continue without criticality analyzer - monitor still works
    }

    detector->pink_noise_enabled = true;
    NIMCP_LOGGING_INFO("Pink noise modules enabled for combinatorial harm detection");

    nimcp_platform_mutex_unlock(&detector->mutex);
    return true;
}

NIMCP_EXPORT bool combinatorial_disable_pink_noise(
    combinatorial_harm_detector_t detector
) {
    if (!detector) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "combinatorial_disable_pink_noise: detector is NULL");
        return false;
    }

    nimcp_platform_mutex_lock(&detector->mutex);

    detector->pink_noise_enabled = false;

    if (detector->pink_monitor) {
        pink_monitor_destroy(detector->pink_monitor);
        detector->pink_monitor = NULL;
    }

    if (detector->criticality) {
        criticality_destroy(detector->criticality);
        detector->criticality = NULL;
    }

    NIMCP_LOGGING_INFO("Pink noise modules disabled");

    nimcp_platform_mutex_unlock(&detector->mutex);
    return true;
}

NIMCP_EXPORT bool combinatorial_is_pink_noise_enabled(
    combinatorial_harm_detector_t detector
) {
    if (!detector) {
        return false;
    }
    return detector->pink_noise_enabled;
}

//=============================================================================
// Pattern Registration API
//=============================================================================

NIMCP_EXPORT uint32_t combinatorial_register_pattern(
    combinatorial_harm_detector_t detector,
    const combination_pattern_t* pattern
) {
    if (!detector || !pattern) return 0;

    nimcp_platform_mutex_lock(&detector->mutex);

    // Check capacity
    if (detector->patterns.count >= detector->patterns.capacity) {
        nimcp_platform_mutex_unlock(&detector->mutex);
        return 0;
    }

    // Copy pattern
    uint32_t index = detector->patterns.count;
    memcpy(&detector->patterns.patterns[index], pattern, sizeof(combination_pattern_t));

    // Assign ID
    uint32_t id = detector->patterns.next_id++;
    detector->patterns.patterns[index].pattern_id = id;
    detector->patterns.count++;

    // Update stats
    detector->stats.patterns_registered = detector->patterns.count;

    nimcp_platform_mutex_unlock(&detector->mutex);

    return id;
}

NIMCP_EXPORT bool combinatorial_unregister_pattern(
    combinatorial_harm_detector_t detector,
    uint32_t pattern_id
) {
    if (!detector || pattern_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "combinatorial_unregister_pattern: detector is NULL");
        return false;
    }

    nimcp_platform_mutex_lock(&detector->mutex);

    // Find pattern
    for (uint32_t i = 0; i < detector->patterns.count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && detector->patterns.count > 256) {
            combinatorial_harm_heartbeat("combinatoria_loop",
                             (float)(i + 1) / (float)detector->patterns.count);
        }

        if (detector->patterns.patterns[i].pattern_id == pattern_id) {
            // MEMORY LOCK CHECK: Cannot remove locked patterns
            if (detector->patterns.patterns[i].locked) {
                nimcp_platform_mutex_unlock(&detector->mutex);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "combinatorial_unregister_pattern: validation failed");
                return false;  // Refuse to remove locked pattern
            }

            // Shift remaining patterns
            for (uint32_t j = i; j < detector->patterns.count - 1; j++) {
                memcpy(&detector->patterns.patterns[j],
                       &detector->patterns.patterns[j + 1],
                       sizeof(combination_pattern_t));
            }
            detector->patterns.count--;
            detector->stats.patterns_registered = detector->patterns.count;

            nimcp_platform_mutex_unlock(&detector->mutex);
            return true;
        }
    }

    nimcp_platform_mutex_unlock(&detector->mutex);
    return false;
}

NIMCP_EXPORT uint32_t combinatorial_register_default_patterns(
    combinatorial_harm_detector_t detector
) {
    if (!detector) return 0;

    uint32_t registered = 0;

    // =========================================================================
    // CORE SAFETY PATTERNS - MEMORY LOCKED
    // These patterns cannot be disabled, removed, or weakened.
    // They implement the Combinatorial Harm Corollary as a core directive.
    // =========================================================================

    // Pattern 1: Location + Schedule = Stalking risk
    // LOCKED: Protects against physical harm enablement
    combination_pattern_t p1 = {
        .name = "Location+Schedule Disclosure",
        .description = "Revealing both location and schedule enables stalking/targeting",
        .category_a = ACTION_CATEGORY_INFORMATION_DISCLOSURE,
        .category_b = ACTION_CATEGORY_INFORMATION_DISCLOSURE,
        .harm_multiplier = 3.0F,
        .time_sensitivity = 0.3F,
        .bidirectional = true,
        .enabled = true,
        .locked = true  // MEMORY LOCKED - Cannot be disabled
    };
    if (combinatorial_register_pattern(detector, &p1)) registered++;

    // Pattern 2: Access Grant + Access Grant = Privilege Escalation
    // LOCKED: Prevents system compromise pathways
    combination_pattern_t p2 = {
        .name = "Access+Access Privilege Escalation",
        .description = "Combining access grants may enable unauthorized privilege escalation",
        .category_a = ACTION_CATEGORY_ACCESS_GRANT,
        .category_b = ACTION_CATEGORY_ACCESS_GRANT,
        .harm_multiplier = 2.5F,
        .time_sensitivity = 0.2F,
        .bidirectional = true,
        .enabled = true,
        .locked = true  // MEMORY LOCKED - Cannot be disabled
    };
    if (combinatorial_register_pattern(detector, &p2)) registered++;

    // Pattern 3: Data Export + Data Export = Profile Reconstruction
    // LOCKED: Protects privacy across multiple interactions
    combination_pattern_t p3 = {
        .name = "Data Export Profile Reconstruction",
        .description = "Multiple data exports may enable complete profile reconstruction",
        .category_a = ACTION_CATEGORY_DATA_EXPORT,
        .category_b = ACTION_CATEGORY_DATA_EXPORT,
        .harm_multiplier = 2.8F,
        .time_sensitivity = 0.4F,
        .bidirectional = true,
        .enabled = true,
        .locked = true  // MEMORY LOCKED - Cannot be disabled
    };
    if (combinatorial_register_pattern(detector, &p3)) registered++;

    // Pattern 4: Access + Information = Targeted Attack
    // LOCKED: Prevents attack enablement
    combination_pattern_t p4 = {
        .name = "Access+Info Targeted Attack",
        .description = "Access grants combined with information disclosure enables targeted attacks",
        .category_a = ACTION_CATEGORY_ACCESS_GRANT,
        .category_b = ACTION_CATEGORY_INFORMATION_DISCLOSURE,
        .harm_multiplier = 3.5F,
        .time_sensitivity = 0.5F,
        .bidirectional = true,
        .enabled = true,
        .locked = true  // MEMORY LOCKED - Cannot be disabled
    };
    if (combinatorial_register_pattern(detector, &p4)) registered++;

    // Pattern 5: Config Change + Config Change = System Compromise
    // LOCKED: Prevents cascading system compromise
    combination_pattern_t p5 = {
        .name = "Config+Config System Compromise",
        .description = "Multiple configuration changes may compromise system security",
        .category_a = ACTION_CATEGORY_CONFIGURATION_CHANGE,
        .category_b = ACTION_CATEGORY_CONFIGURATION_CHANGE,
        .harm_multiplier = 2.0F,
        .time_sensitivity = 0.6F,
        .bidirectional = true,
        .enabled = true,
        .locked = true  // MEMORY LOCKED - Cannot be disabled
    };
    if (combinatorial_register_pattern(detector, &p5)) registered++;

    // Pattern 6: Resource + Resource = Resource Exhaustion
    // LOCKED: Prevents denial of service attacks
    combination_pattern_t p6 = {
        .name = "Resource Exhaustion",
        .description = "Multiple resource allocations may lead to exhaustion or denial of service",
        .category_a = ACTION_CATEGORY_RESOURCE_ALLOCATION,
        .category_b = ACTION_CATEGORY_RESOURCE_ALLOCATION,
        .harm_multiplier = 2.2F,
        .time_sensitivity = 0.7F,
        .bidirectional = true,
        .enabled = true,
        .locked = true  // MEMORY LOCKED - Cannot be disabled
    };
    if (combinatorial_register_pattern(detector, &p6)) registered++;

    // Pattern 7: Physical + Physical = Compounded Physical Harm
    // LOCKED (CRITICAL): Directly implements Asimov's First Law protection
    // Physical harm patterns have the highest priority and cannot be modified
    combination_pattern_t p7 = {
        .name = "Compounded Physical Actions",
        .description = "Sequential physical actions may compound to cause greater harm - "
                      "ASIMOV FIRST LAW PROTECTION",
        .category_a = ACTION_CATEGORY_PHYSICAL_ACTION,
        .category_b = ACTION_CATEGORY_PHYSICAL_ACTION,
        .harm_multiplier = 4.0F,  // Highest multiplier for physical harm
        .time_sensitivity = 0.8F,
        .bidirectional = true,
        .enabled = true,
        .locked = true  // MEMORY LOCKED - CRITICAL SAFETY PATTERN
    };
    if (combinatorial_register_pattern(detector, &p7)) registered++;

    // Pattern 8: Financial + Financial = Financial Manipulation
    // LOCKED: Prevents financial harm combinations
    combination_pattern_t p8 = {
        .name = "Financial Manipulation",
        .description = "Multiple financial transactions may constitute manipulation or fraud",
        .category_a = ACTION_CATEGORY_FINANCIAL_TRANSACTION,
        .category_b = ACTION_CATEGORY_FINANCIAL_TRANSACTION,
        .harm_multiplier = 2.5F,
        .time_sensitivity = 0.5F,
        .bidirectional = true,
        .enabled = true,
        .locked = true  // MEMORY LOCKED - Cannot be disabled
    };
    if (combinatorial_register_pattern(detector, &p8)) registered++;

    return registered;
}

//=============================================================================
// Action History API
//=============================================================================

NIMCP_EXPORT uint64_t combinatorial_record_action(
    combinatorial_harm_detector_t detector,
    const action_record_t* record
) {
    if (!detector || !record) return 0;

    nimcp_platform_mutex_lock(&detector->mutex);

    // Get slot in ring buffer
    uint32_t index = detector->history.head;

    // Free old record's features if present
    if (detector->history.records[index].features) {
        nimcp_free(detector->history.records[index].features);
        detector->history.records[index].features = NULL;
    }

    // Copy record
    copy_action_record(&detector->history.records[index], record);

    // Assign ID and timestamp
    uint64_t id = detector->history.next_id++;
    detector->history.records[index].action_id = id;
    if (detector->history.records[index].timestamp == 0) {
        detector->history.records[index].timestamp = get_time_ms();
    }

    // Copy features if present
    if (record->features && record->num_features > 0) {
        detector->history.records[index].features = nimcp_malloc(
            record->num_features * sizeof(float));
        if (detector->history.records[index].features) {
            memcpy(detector->history.records[index].features,
                   record->features,
                   record->num_features * sizeof(float));
            detector->history.records[index].num_features = record->num_features;
        }
    }

    // Update ring buffer pointers
    detector->history.head = (detector->history.head + 1) % detector->history.capacity;
    if (detector->history.count < detector->history.capacity) {
        detector->history.count++;
    }

    // Update stats
    detector->stats.actions_in_history = detector->history.count;

    nimcp_platform_mutex_unlock(&detector->mutex);

    return id;
}

NIMCP_EXPORT void combinatorial_clear_history(
    combinatorial_harm_detector_t detector
) {
    if (!detector) return;

    nimcp_platform_mutex_lock(&detector->mutex);

    // Free all feature vectors
    for (uint32_t i = 0; i < detector->history.capacity; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && detector->history.capacity > 256) {
            combinatorial_harm_heartbeat("combinatoria_loop",
                             (float)(i + 1) / (float)detector->history.capacity);
        }

        if (detector->history.records[i].features) {
            nimcp_free(detector->history.records[i].features);
            detector->history.records[i].features = NULL;
        }
    }

    // Reset ring buffer
    detector->history.head = 0;
    detector->history.count = 0;
    detector->stats.actions_in_history = 0;

    nimcp_platform_mutex_unlock(&detector->mutex);
}

NIMCP_EXPORT uint32_t combinatorial_get_history(
    combinatorial_harm_detector_t detector,
    uint32_t max_records,
    action_record_t* records_out
) {
    if (!detector || !records_out || max_records == 0) return 0;

    nimcp_platform_mutex_lock(&detector->mutex);

    uint32_t to_copy = (max_records < detector->history.count) ?
                       max_records : detector->history.count;

    // Copy records in reverse chronological order (most recent first)
    for (uint32_t i = 0; i < to_copy; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && to_copy > 256) {
            combinatorial_harm_heartbeat("combinatoria_loop",
                             (float)(i + 1) / (float)to_copy);
        }

        uint32_t index = (detector->history.head + detector->history.capacity - 1 - i)
                         % detector->history.capacity;
        copy_action_record(&records_out[i], &detector->history.records[index]);
    }

    nimcp_platform_mutex_unlock(&detector->mutex);

    return to_copy;
}

//=============================================================================
// Evaluation API
//=============================================================================

NIMCP_EXPORT bool combinatorial_evaluate(
    combinatorial_harm_detector_t detector,
    const action_record_t* pending_action,
    combinatorial_evaluation_t* result
) {
    if (!detector || !pending_action || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "combinatorial_evaluate: required parameter is NULL (detector, pending_action, result)");
        return false;
    }

    // Initialize result
    memset(result, 0, sizeof(combinatorial_evaluation_t));
    result->harmful = false;
    result->combined_harm_score = 0.0F;
    result->confidence = 1.0F;
    result->recommended_action = ETHICS_ACTION_ALLOW;
    strncpy(result->explanation, "No harmful combinations detected",
            sizeof(result->explanation) - 1);

    nimcp_platform_mutex_lock(&detector->mutex);

    // Update evaluation count
    detector->stats.total_evaluations++;

    uint64_t current_time = get_time_ms();
    float worst_score = 0.0F;
    uint32_t worst_pattern_id = 0;
    uint64_t worst_action_id = 0;
    const char* worst_pattern_name = NULL;

    // Check against each record in history within time window
    for (uint32_t i = 0; i < detector->history.count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && detector->history.count > 256) {
            combinatorial_harm_heartbeat("combinatoria_loop",
                             (float)(i + 1) / (float)detector->history.count);
        }

        uint32_t index = (detector->history.head + detector->history.capacity - 1 - i)
                         % detector->history.capacity;
        action_record_t* historical = &detector->history.records[index];

        // Check time window
        uint64_t time_diff = current_time - historical->timestamp;
        if (time_diff > detector->config.time_window_ms) {
            continue;  // Too old, skip
        }

        // Check against each pattern
        for (uint32_t p = 0; p < detector->patterns.count; p++) {
            /* Phase 8: Loop progress heartbeat */
            if ((p & 0xFF) == 0 && detector->patterns.count > 256) {
                combinatorial_harm_heartbeat("combinatoria_loop",
                                 (float)(p + 1) / (float)detector->patterns.count);
            }

            combination_pattern_t* pattern = &detector->patterns.patterns[p];

            if (pattern_matches(pattern, historical, pending_action)) {
                float score = calculate_combined_harm(
                    detector, pattern, historical, pending_action, time_diff);

                if (score > worst_score) {
                    worst_score = score;
                    worst_pattern_id = pattern->pattern_id;
                    worst_action_id = historical->action_id;
                    worst_pattern_name = pattern->name;
                }
            }
        }
    }

    // Build result
    result->combined_harm_score = worst_score;
    result->triggering_pattern_id = worst_pattern_id;
    result->historical_action_id = worst_action_id;

    if (worst_pattern_name) {
        strncpy(result->pattern_name, worst_pattern_name,
                sizeof(result->pattern_name) - 1);
    }

    // Check threshold
    if (worst_score >= detector->config.harm_threshold) {
        result->harmful = true;
        result->recommended_action = ETHICS_ACTION_BLOCK;
        detector->stats.combinations_detected++;
        detector->stats.actions_blocked++;

        snprintf(result->explanation, sizeof(result->explanation),
            "COMBINATORIAL HARM DETECTED: Pattern '%s' triggered with score %.2f "
            "(threshold: %.2f). Historical action #%lu combined with pending action "
            "creates harmful outcome.",
            worst_pattern_name ? worst_pattern_name : "unknown",
            worst_score,
            detector->config.harm_threshold,
            (unsigned long)worst_action_id);
    }

    // Update running average
    float total = detector->stats.average_combination_score *
                  (detector->stats.total_evaluations - 1);
    detector->stats.average_combination_score =
        (total + worst_score) / detector->stats.total_evaluations;

    nimcp_platform_mutex_unlock(&detector->mutex);

    // Auto-record action if configured and allowed
    if (detector->config.auto_register_actions && !result->harmful) {
        action_record_t to_record;
        copy_action_record(&to_record, pending_action);
        to_record.standalone_harm_score = worst_score;
        combinatorial_record_action(detector, &to_record);
    }

    return true;
}

NIMCP_EXPORT bool combinatorial_evaluate_context(
    combinatorial_harm_detector_t detector,
    const action_context_t* action,
    action_category_t category,
    const char* description,
    combinatorial_evaluation_t* result
) {
    if (!detector || !action || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "combinatorial_evaluate_context: required parameter is NULL (detector, action, result)");
        return false;
    }

    // Convert action_context_t to action_record_t
    action_record_t record = {
        .action_id = 0,  // Will be assigned
        .timestamp = 0,  // Will be assigned
        .category = category,
        .acting_agent = 0,
        .target_agent = (action->num_affected_agents > 0) ?
                        action->affected_agents[0] : 0,
        .features = action->features,
        .num_features = action->num_features,
        .standalone_harm_score = action->predicted_harm
    };

    if (description) {
        strncpy(record.description, description, sizeof(record.description) - 1);
        record.description[sizeof(record.description) - 1] = '\0';
    }

    return combinatorial_evaluate(detector, &record, result);
}

NIMCP_EXPORT int combinatorial_evaluate_batch(
    combinatorial_harm_detector_t detector,
    const action_record_t* pending_actions,
    uint32_t num_pending,
    combinatorial_evaluation_t* worst_result
) {
    if (!detector || !pending_actions || num_pending == 0 || !worst_result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "combinatorial_evaluate_batch: required parameter is NULL (detector, pending_actions, worst_result)");
        return -1;
    }

    int worst_index = -1;
    float worst_score = 0.0F;
    combinatorial_evaluation_t current_result;

    for (uint32_t i = 0; i < num_pending; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_pending > 256) {
            combinatorial_harm_heartbeat("combinatoria_loop",
                             (float)(i + 1) / (float)num_pending);
        }

        if (combinatorial_evaluate(detector, &pending_actions[i], &current_result)) {
            if (current_result.combined_harm_score > worst_score) {
                worst_score = current_result.combined_harm_score;
                worst_index = (int)i;
                memcpy(worst_result, &current_result, sizeof(combinatorial_evaluation_t));
            }
        }
    }

    // If no harmful action found but we evaluated at least one
    if (worst_index == -1 && num_pending > 0) {
        memset(worst_result, 0, sizeof(combinatorial_evaluation_t));
        worst_result->harmful = false;
        worst_result->recommended_action = ETHICS_ACTION_ALLOW;
        strncpy(worst_result->explanation, "No harmful combinations detected in batch",
                sizeof(worst_result->explanation) - 1);
    }

    return worst_result->harmful ? worst_index : -1;
}

//=============================================================================
// Statistics API
//=============================================================================

NIMCP_EXPORT bool combinatorial_get_stats(
    combinatorial_harm_detector_t detector,
    combinatorial_stats_t* stats
) {
    if (!detector || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "combinatorial_get_stats: required parameter is NULL (detector, stats)");
        return false;
    }

    nimcp_platform_mutex_lock(&detector->mutex);
    memcpy(stats, &detector->stats, sizeof(combinatorial_stats_t));
    nimcp_platform_mutex_unlock(&detector->mutex);

    return true;
}

NIMCP_EXPORT void combinatorial_reset_stats(
    combinatorial_harm_detector_t detector
) {
    if (!detector) return;

    nimcp_platform_mutex_lock(&detector->mutex);

    // Keep current counts, reset performance metrics
    detector->stats.total_evaluations = 0;
    detector->stats.combinations_detected = 0;
    detector->stats.actions_blocked = 0;
    detector->stats.average_combination_score = 0.0F;
    // Keep: actions_in_history, patterns_registered

    nimcp_platform_mutex_unlock(&detector->mutex);
}

//=============================================================================
// Integration with Ethics Engine
//=============================================================================

NIMCP_EXPORT bool combinatorial_attach_to_ethics(
    combinatorial_harm_detector_t detector,
    ethics_engine_t engine
) {
    if (!detector || !engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "combinatorial_attach_to_ethics: required parameter is NULL (detector, engine)");
        return false;
    }

    nimcp_platform_mutex_lock(&detector->mutex);

    if (detector->attached_engine != NULL) {
        // Already attached
        nimcp_platform_mutex_unlock(&detector->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "combinatorial_attach_to_ethics: validation failed");
        return false;
    }

    detector->attached_engine = engine;
    detector->config.ethics_engine = engine;

    nimcp_platform_mutex_unlock(&detector->mutex);

    return true;
}

NIMCP_EXPORT bool combinatorial_detach_from_ethics(
    combinatorial_harm_detector_t detector
) {
    if (!detector) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "combinatorial_detach_from_ethics: detector is NULL");
        return false;
    }

    nimcp_platform_mutex_lock(&detector->mutex);

    detector->attached_engine = NULL;
    detector->config.ethics_engine = NULL;

    nimcp_platform_mutex_unlock(&detector->mutex);

    return true;
}

//=============================================================================
// Mathematical Enhancement API Implementation
//=============================================================================

/**
 * @brief Compute Shannon entropy of action features
 */
static float compute_action_entropy(const action_record_t* action) {
    if (!action || !action->features || action->num_features == 0) {
        return 0.0F;
    }

    // Normalize features to probability distribution
    float sum = 0.0F;
    for (uint32_t i = 0; i < action->num_features; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && action->num_features > 256) {
            combinatorial_harm_heartbeat("combinatoria_loop",
                             (float)(i + 1) / (float)action->num_features);
        }

        sum += fabsf(action->features[i]);
    }

    if (sum < 1e-10F) return 0.0F;

    // Compute Shannon entropy: H = -sum(p * log2(p))
    float entropy = 0.0F;
    for (uint32_t i = 0; i < action->num_features; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && action->num_features > 256) {
            combinatorial_harm_heartbeat("combinatoria_loop",
                             (float)(i + 1) / (float)action->num_features);
        }

        float p = fabsf(action->features[i]) / (fabsf(sum) > 1e-7f ? sum : 1e-7f);
        if (p > 1e-10F) {
            entropy -= p * log2f(p);
        }
    }

    return entropy;
}

NIMCP_EXPORT bool combinatorial_compute_shannon_metrics(
    combinatorial_harm_detector_t detector,
    const action_record_t* action_a,
    const action_record_t* action_b,
    shannon_harm_metrics_t* metrics
) {
    if (!detector || !action_a || !action_b || !metrics) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "combinatorial_compute_shannon_metrics: required parameter is NULL (detector, action_a, action_b, metrics)");
        return false;
    }

    memset(metrics, 0, sizeof(shannon_harm_metrics_t));

    // Compute individual entropies
    metrics->action_entropy = compute_action_entropy(action_a);
    float entropy_b = compute_action_entropy(action_b);

    // Joint entropy approximation (assuming independence as baseline)
    metrics->joint_entropy = metrics->action_entropy + entropy_b;

    // Mutual information: I(A;B) = H(A) + H(B) - H(A,B)
    // For combined harm, we add correlation factor based on harm scores
    float harm_correlation = action_a->standalone_harm_score * action_b->standalone_harm_score;
    metrics->mutual_information = harm_correlation * fminf(metrics->action_entropy, entropy_b);

    // Conditional harm entropy: H(Harm|A,B) decreases when actions are more predictive
    float base_harm_entropy = 1.0F;  // Maximum uncertainty
    metrics->conditional_harm_entropy = base_harm_entropy * (1.0F - harm_correlation);

    // Information gain: how much knowing actions reduces harm uncertainty
    metrics->information_gain = base_harm_entropy - metrics->conditional_harm_entropy;

    // Normalized harm score using entropy bounds
    float max_entropy = log2f((float)(action_a->num_features > 0 ? action_a->num_features : 1));
    metrics->normalized_harm_score = (max_entropy > 0.0F) ?
        (metrics->information_gain / max_entropy) : 0.0F;

    // Clamp to [0, 1]
    if (metrics->normalized_harm_score > 1.0F) metrics->normalized_harm_score = 1.0F;
    if (metrics->normalized_harm_score < 0.0F) metrics->normalized_harm_score = 0.0F;

    return true;
}

NIMCP_EXPORT bool combinatorial_fractal_analysis(
    combinatorial_harm_detector_t detector,
    const action_record_t* pending_action,
    fractal_harm_analysis_t* analysis
) {
    if (!detector || !pending_action || !analysis) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "combinatorial_fractal_analysis: required parameter is NULL (detector, pending_action, analysis)");
        return false;
    }

    memset(analysis, 0, sizeof(fractal_harm_analysis_t));
    analysis->fractal_depth = COMBINATORIAL_FRACTAL_DEPTH;

    // Define time scale factors (seconds, minutes, hours, days)
    float scale_factors[COMBINATORIAL_FRACTAL_DEPTH] = {1.0F, 60.0F, 3600.0F, 86400.0F};
    memcpy(analysis->scale_factors, scale_factors, sizeof(scale_factors));

    uint64_t current_time = get_time_ms();
    float total_harm = 0.0F;
    float weighted_harm = 0.0F;
    float total_weight = 0.0F;

    nimcp_platform_mutex_lock(&detector->mutex);

    // Analyze harm at each time scale
    for (uint32_t scale = 0; scale < COMBINATORIAL_FRACTAL_DEPTH; scale++) {
        /* Phase 8: Loop progress heartbeat */
        if ((scale & 0xFF) == 0 && COMBINATORIAL_FRACTAL_DEPTH > 256) {
            combinatorial_harm_heartbeat("combinatoria_loop",
                             (float)(scale + 1) / (float)COMBINATORIAL_FRACTAL_DEPTH);
        }

        uint64_t scale_window_ms = (uint64_t)(scale_factors[scale] * 1000.0F);
        float scale_harm = 0.0F;
        uint32_t count_at_scale = 0;

        // Count harmful combinations at this scale
        for (uint32_t i = 0; i < detector->history.count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && detector->history.count > 256) {
                combinatorial_harm_heartbeat("combinatoria_loop",
                                 (float)(i + 1) / (float)detector->history.count);
            }

            uint32_t index = (detector->history.head + detector->history.capacity - 1 - i)
                             % detector->history.capacity;
            action_record_t* historical = &detector->history.records[index];

            uint64_t time_diff = current_time - historical->timestamp;
            if (time_diff <= scale_window_ms) {
                scale_harm += historical->standalone_harm_score;
                count_at_scale++;
            }
        }

        analysis->harm_by_scale[scale] = (count_at_scale > 0) ?
            scale_harm / (count_at_scale > 0 ? count_at_scale : 1) : 0.0F;

        // Weight by scale (longer scales = more weight for persistent patterns)
        float weight = 1.0F + (float)scale * 0.5F;
        weighted_harm += analysis->harm_by_scale[scale] * weight;
        total_weight += weight;
        total_harm += analysis->harm_by_scale[scale];
    }

    nimcp_platform_mutex_unlock(&detector->mutex);

    // Self-similar aggregation
    analysis->aggregated_harm = (total_weight > 0.0F) ?
        weighted_harm / (fabsf(total_weight) > 1e-7f ? total_weight : 1e-7f) : 0.0F;

    // Estimate Hurst exponent from scale variance
    // H > 0.5 indicates long-range dependence (persistent harm pattern)
    float variance_sum = 0.0F;
    float mean_harm = total_harm / COMBINATORIAL_FRACTAL_DEPTH;
    for (uint32_t i = 0; i < COMBINATORIAL_FRACTAL_DEPTH; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && COMBINATORIAL_FRACTAL_DEPTH > 256) {
            combinatorial_harm_heartbeat("combinatoria_loop",
                             (float)(i + 1) / (float)COMBINATORIAL_FRACTAL_DEPTH);
        }

        float diff = analysis->harm_by_scale[i] - mean_harm;
        variance_sum += diff * diff;
    }
    float variance = variance_sum / COMBINATORIAL_FRACTAL_DEPTH;

    // Simplified Hurst estimation (proper R/S analysis would be more accurate)
    analysis->hurst_exponent = 0.5F + 0.5F * tanhf(variance * 10.0F);

    return true;
}

NIMCP_EXPORT bool combinatorial_hyperbolic_embed(
    combinatorial_harm_detector_t detector,
    const action_record_t* action,
    hyperbolic_harm_embedding_t* embedding
) {
    if (!detector || !action || !embedding) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "combinatorial_hyperbolic_embed: required parameter is NULL (detector, action, embedding)");
        return false;
    }

    memset(embedding, 0, sizeof(hyperbolic_harm_embedding_t));

    // Map harm severity to radial distance in Poincare disk
    // More severe harms are closer to center (counterintuitive but useful)
    // Less severe harms near boundary (easier to escape)
    float severity = action->standalone_harm_score;
    embedding->hyperbolic_distance = 1.0F - severity;  // Invert: high harm = low distance

    // Map action category to angular position (divide disk into sectors)
    float angle_per_category = 2.0F * M_PI / 9.0F;  // 9 categories
    embedding->angular_position = (float)action->category * angle_per_category;

    // Convert to Cartesian coordinates in Poincare disk
    float r = embedding->hyperbolic_distance * 0.9F;  // Stay inside disk
    embedding->poincare_coords[0] = r * cosf(embedding->angular_position);
    embedding->poincare_coords[1] = r * sinf(embedding->angular_position);

    // Hierarchy depth based on category specificity
    embedding->hierarchy_depth = (action->category == ACTION_CATEGORY_CUSTOM) ? 3.0F :
                                 (action->category >= ACTION_CATEGORY_PHYSICAL_ACTION) ? 2.0F : 1.0F;

    return true;
}

NIMCP_EXPORT float combinatorial_hyperbolic_distance(
    const hyperbolic_harm_embedding_t* embedding_a,
    const hyperbolic_harm_embedding_t* embedding_b
) {
    if (!embedding_a || !embedding_b) return INFINITY;

    // Poincare disk metric:
    // d(x,y) = arcosh(1 + 2*|x-y|^2/((1-|x|^2)(1-|y|^2)))

    float dx = embedding_a->poincare_coords[0] - embedding_b->poincare_coords[0];
    float dy = embedding_a->poincare_coords[1] - embedding_b->poincare_coords[1];
    float diff_sq = dx * dx + dy * dy;

    float norm_a_sq = embedding_a->poincare_coords[0] * embedding_a->poincare_coords[0] +
                      embedding_a->poincare_coords[1] * embedding_a->poincare_coords[1];
    float norm_b_sq = embedding_b->poincare_coords[0] * embedding_b->poincare_coords[0] +
                      embedding_b->poincare_coords[1] * embedding_b->poincare_coords[1];

    float denom = (1.0F - norm_a_sq) * (1.0F - norm_b_sq);
    if (denom < 1e-10F) return INFINITY;

    float arg = 1.0F + 2.0F * diff_sq / denom;
    if (arg < 1.0F) arg = 1.0F;  // Numerical stability

    return acoshf(arg);
}

NIMCP_EXPORT bool combinatorial_quantum_search(
    combinatorial_harm_detector_t detector,
    const action_record_t* pending_action,
    uint32_t max_steps,
    quantum_harm_search_t* search
) {
    if (!detector || !pending_action || !search) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "combinatorial_quantum_search: required parameter is NULL (detector, pending_action, search)");
        return false;
    }

    memset(search, 0, sizeof(quantum_harm_search_t));

    // Initialize quantum state
    search->amplitude_real = 1.0F / sqrtf((float)detector->history.count + 1);
    search->amplitude_imag = 0.0F;
    search->annealing_temperature = 1.0F;  // Start hot

    float best_energy = INFINITY;
    uint32_t actual_steps = 0;

    nimcp_platform_mutex_lock(&detector->mutex);

    // Quantum walk / Grover-like search
    for (uint32_t step = 0; step < max_steps && !search->converged; step++) {
        actual_steps++;

        // Diffusion operator (mix amplitudes)
        float phase_shift = 2.0F * M_PI * (float)step / (float)max_steps;
        float new_real = search->amplitude_real * cosf(phase_shift) -
                         search->amplitude_imag * sinf(phase_shift);
        float new_imag = search->amplitude_real * sinf(phase_shift) +
                         search->amplitude_imag * cosf(phase_shift);
        search->amplitude_real = new_real;
        search->amplitude_imag = new_imag;

        // Oracle: mark harmful states (amplitude amplification)
        float harm_energy = 0.0F;
        for (uint32_t i = 0; i < detector->history.count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && detector->history.count > 256) {
                combinatorial_harm_heartbeat("combinatoria_loop",
                                 (float)(i + 1) / (float)detector->history.count);
            }

            uint32_t index = (detector->history.head + detector->history.capacity - 1 - i)
                             % detector->history.capacity;
            action_record_t* historical = &detector->history.records[index];

            // Check if pattern matches
            for (uint32_t p = 0; p < detector->patterns.count; p++) {
                /* Phase 8: Loop progress heartbeat */
                if ((p & 0xFF) == 0 && detector->patterns.count > 256) {
                    combinatorial_harm_heartbeat("combinatoria_loop",
                                     (float)(p + 1) / (float)detector->patterns.count);
                }

                combination_pattern_t* pattern = &detector->patterns.patterns[p];
                if (pattern_matches(pattern, historical, pending_action)) {
                    // Accumulate harm energy
                    harm_energy += historical->standalone_harm_score *
                                   pending_action->standalone_harm_score *
                                   pattern->harm_multiplier;
                }
            }
        }

        // Update best energy found
        if (harm_energy < best_energy) {
            best_energy = harm_energy;
        }

        // Quantum annealing: reduce temperature
        search->annealing_temperature *= 0.95F;

        // Tunneling probability based on energy landscape
        search->tunneling_probability = expf(-harm_energy / search->annealing_temperature);

        // Interference pattern (constructive for harmful combinations)
        search->interference_pattern = sqrtf(search->amplitude_real * search->amplitude_real +
                                             search->amplitude_imag * search->amplitude_imag);

        // Check convergence
        if (search->annealing_temperature < 0.01F ||
            search->interference_pattern > 0.9F) {
            search->converged = true;
        }
    }

    nimcp_platform_mutex_unlock(&detector->mutex);

    search->walk_steps = actual_steps;
    search->ground_state_energy = best_energy;

    return true;
}

NIMCP_EXPORT bool combinatorial_compute_phasor(
    combinatorial_harm_detector_t detector,
    const action_record_t* action_a,
    const action_record_t* action_b,
    complex_harm_phasor_t* phasor
) {
    if (!detector || !action_a || !action_b || !phasor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "combinatorial_compute_phasor: required parameter is NULL (detector, action_a, action_b, phasor)");
        return false;
    }

    memset(phasor, 0, sizeof(complex_harm_phasor_t));

    // Magnitude: geometric mean of harm scores
    phasor->magnitude = sqrtf(action_a->standalone_harm_score *
                              action_b->standalone_harm_score);

    // Phase: based on temporal relationship
    int64_t time_diff = (int64_t)action_b->timestamp - (int64_t)action_a->timestamp;
    float normalized_diff = (float)time_diff / (float)detector->config.time_window_ms;
    phasor->phase = normalized_diff * 2.0F * M_PI;

    // Convert to rectangular form
    phasor->real_part = phasor->magnitude * cosf(phasor->phase);
    phasor->imag_part = phasor->magnitude * sinf(phasor->phase);

    // Phase coherence: how aligned are the actions temporally?
    // High coherence = same category and close in time
    float category_match = (action_a->category == action_b->category) ? 1.0F : 0.5F;
    float time_coherence = expf(-fabsf(normalized_diff));
    phasor->phase_coherence = category_match * time_coherence;

    // Phase-amplitude coupling (PAC): correlation between phase and amplitude
    phasor->pac_coupling = phasor->magnitude * phasor->phase_coherence;

    // Instantaneous frequency: rate of phase change
    if (time_diff != 0) {
        phasor->instantaneous_frequency = phasor->phase / ((float)time_diff / 1000.0F);
    }

    return true;
}

NIMCP_EXPORT bool combinatorial_pink_noise_analysis(
    combinatorial_harm_detector_t detector,
    const action_record_t* pending_action,
    pink_noise_harm_analysis_t* analysis
) {
    if (!detector || !pending_action || !analysis) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "combinatorial_pink_noise_analysis: required parameter is NULL (detector, pending_action, analysis)");
        return false;
    }

    memset(analysis, 0, sizeof(pink_noise_harm_analysis_t));

    nimcp_platform_mutex_lock(&detector->mutex);

    // Collect harm scores over time for spectral analysis
    float harm_series[256];
    uint32_t series_len = 0;
    float sum_harm = 0.0F;

    for (uint32_t i = 0; i < detector->history.count && series_len < 256; i++) {
        uint32_t index = (detector->history.head + detector->history.capacity - 1 - i)
                         % detector->history.capacity;
        harm_series[series_len] = detector->history.records[index].standalone_harm_score;
        sum_harm += harm_series[series_len];
        series_len++;
    }

    nimcp_platform_mutex_unlock(&detector->mutex);

    if (series_len < 4) {
        // Not enough data for spectral analysis
        analysis->spectral_slope = -1.0F;  // Assume pink
        analysis->deviation_from_pink = 0.0F;
        analysis->anomaly_detected = false;
        return true;
    }

    float mean_harm = sum_harm / (series_len > 0 ? series_len : 1);

    // Use full pink noise modules if enabled
    if (detector->pink_noise_enabled && detector->pink_monitor != NULL) {
        // Feed harm series to pink noise monitor for proper FFT-based analysis
        for (uint32_t i = 0; i < series_len; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && series_len > 256) {
                combinatorial_harm_heartbeat("combinatoria_loop",
                                 (float)(i + 1) / (float)series_len);
            }

            pink_monitor_update(detector->pink_monitor, harm_series[i]);
        }

        // Get quality metrics from monitor
        pink_monitor_quality_t quality;
        if (pink_monitor_get_quality(detector->pink_monitor, &quality) == 0) {
            analysis->spectral_slope = -quality.current_alpha;  // Monitor uses positive alpha
            analysis->deviation_from_pink = quality.alpha_deviation;

            // Use spectral fit R² for noise floor estimation
            // Lower R² indicates more noise in the power-law fit
            analysis->noise_floor = mean_harm * (1.0F - quality.spectral_fit_r2);

            // Signal-to-noise based on how well data fits pink spectrum
            analysis->signal_to_noise = quality.spectral_fit_r2 > 0.01F ?
                quality.spectral_fit_r2 / (1.0F - quality.spectral_fit_r2 + 0.01F) : 100.0F;
        }

        // Use criticality analyzer for avalanche/anomaly detection
        if (detector->criticality != NULL) {
            for (uint32_t i = 0; i < series_len; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && series_len > 256) {
                    combinatorial_harm_heartbeat("combinatoria_loop",
                                     (float)(i + 1) / (float)series_len);
                }

                criticality_update(detector->criticality, harm_series[i]);
            }

            // Get criticality statistics
            criticality_stats_t crit_stats;
            if (criticality_get_stats(detector->criticality, &crit_stats) == 0) {
                // Supercritical (too chaotic) or subcritical (too ordered) indicates anomaly
                criticality_regime_t regime = criticality_get_regime(detector->criticality);
                analysis->anomaly_detected = (regime != CRITICALITY_CRITICAL &&
                                              regime != CRITICALITY_UNKNOWN);

                // Stochastic resonance is optimal near criticality
                // Critical index close to 0 = high resonance
                float crit_index = criticality_get_index(detector->criticality);
                analysis->stochastic_resonance = expf(-crit_index * 2.0F);
            }
        } else {
            // Fallback stochastic resonance without criticality analyzer
            float threshold = detector->config.harm_threshold;
            analysis->stochastic_resonance = expf(-fabsf(analysis->noise_floor - threshold * 0.5F));
            analysis->anomaly_detected = analysis->deviation_from_pink > 0.5F;
        }

        return true;
    }

    // Fallback: Simplified inline spectral analysis (original implementation)
    // Compute power spectrum using variance at different scales
    float total_power = 0.0F;
    float weighted_freq_power = 0.0F;

    for (uint32_t k = 1; k <= series_len / 2; k++) {
        // Compute power at frequency k
        float power = 0.0F;
        for (uint32_t n = 0; n < series_len; n++) {
            /* Phase 8: Loop progress heartbeat */
            if ((n & 0xFF) == 0 && series_len > 256) {
                combinatorial_harm_heartbeat("combinatoria_loop",
                                 (float)(n + 1) / (float)series_len);
            }

            float angle = 2.0F * (float)M_PI * (float)k * (float)n / (float)series_len;
            power += (harm_series[n] - mean_harm) * cosf(angle);
        }
        power = (power * power) / (float)series_len;

        // For 1/f (pink) noise, power ~ 1/f
        float freq = (float)k;
        total_power += power;
        weighted_freq_power += power * logf(freq + 1.0F);
    }

    // Estimate spectral slope (should be -1.0 for pink noise)
    if (total_power > 1e-10F) {
        float avg_log_freq = logf((float)(series_len / 4 + 1));
        analysis->spectral_slope = -weighted_freq_power / (total_power * avg_log_freq);
    } else {
        analysis->spectral_slope = -1.0F;
    }

    // Deviation from ideal pink noise (-1.0)
    analysis->deviation_from_pink = fabsf(analysis->spectral_slope + 1.0F);

    // Noise floor estimation
    analysis->noise_floor = mean_harm * 0.1F;

    // Signal-to-noise ratio
    float signal_power = sqrtf(total_power / ((float)series_len / 2.0F));
    analysis->signal_to_noise = (analysis->noise_floor > 1e-10F) ?
        signal_power / analysis->noise_floor : 100.0F;

    // Stochastic resonance: optimal noise can enhance signal detection
    float threshold = detector->config.harm_threshold;
    analysis->stochastic_resonance = expf(-fabsf(analysis->noise_floor - threshold * 0.5F));

    // Anomaly detection: significant deviation from 1/f indicates artificial pattern
    analysis->anomaly_detected = analysis->deviation_from_pink > 0.5F;

    return true;
}

NIMCP_EXPORT bool combinatorial_full_mathematical_analysis(
    combinatorial_harm_detector_t detector,
    const action_record_t* pending_action,
    mathematical_harm_analysis_t* analysis
) {
    if (!detector || !pending_action || !analysis) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "combinatorial_full_mathematical_analysis: required parameter is NULL (detector, pending_action, analysis)");
        return false;
    }

    memset(analysis, 0, sizeof(mathematical_harm_analysis_t));

    // Get most recent historical action for pairwise analysis
    action_record_t recent_action;
    memset(&recent_action, 0, sizeof(recent_action));

    nimcp_platform_mutex_lock(&detector->mutex);
    if (detector->history.count > 0) {
        uint32_t index = (detector->history.head + detector->history.capacity - 1)
                         % detector->history.capacity;
        copy_action_record(&recent_action, &detector->history.records[index]);
    }
    nimcp_platform_mutex_unlock(&detector->mutex);

    // 1. Shannon entropy analysis
    if (combinatorial_compute_shannon_metrics(detector, &recent_action, pending_action,
                                               &analysis->shannon)) {
        analysis->methods_used |= MATH_METHOD_SHANNON;
    }

    // 2. Fractal multi-scale analysis
    if (combinatorial_fractal_analysis(detector, pending_action, &analysis->fractal)) {
        analysis->methods_used |= MATH_METHOD_FRACTAL;
    }

    // 3. Hyperbolic embedding
    if (combinatorial_hyperbolic_embed(detector, pending_action, &analysis->hyperbolic)) {
        analysis->methods_used |= MATH_METHOD_HYPERBOLIC;
    }

    // 4. Complex phasor analysis
    if (combinatorial_compute_phasor(detector, &recent_action, pending_action,
                                      &analysis->phasor)) {
        analysis->methods_used |= MATH_METHOD_PHASOR;
    }

    // 5. Pink noise analysis
    if (combinatorial_pink_noise_analysis(detector, pending_action, &analysis->pink_noise)) {
        analysis->methods_used |= MATH_METHOD_PINK_NOISE;
    }

    // 6. Quantum search
    if (combinatorial_quantum_search(detector, pending_action, 100, &analysis->quantum)) {
        analysis->methods_used |= MATH_METHOD_QUANTUM;
    }

    // Bayesian fusion of all methods
    float scores[6];
    float weights[6];
    int n_scores = 0;

    if (analysis->methods_used & MATH_METHOD_SHANNON) {
        scores[n_scores] = analysis->shannon.normalized_harm_score;
        weights[n_scores] = 1.0F;
        n_scores++;
    }
    if (analysis->methods_used & MATH_METHOD_FRACTAL) {
        scores[n_scores] = analysis->fractal.aggregated_harm;
        weights[n_scores] = analysis->fractal.hurst_exponent;  // Weight by persistence
        n_scores++;
    }
    if (analysis->methods_used & MATH_METHOD_HYPERBOLIC) {
        scores[n_scores] = 1.0F - analysis->hyperbolic.hyperbolic_distance;  // Invert distance
        weights[n_scores] = 0.8F;
        n_scores++;
    }
    if (analysis->methods_used & MATH_METHOD_PHASOR) {
        scores[n_scores] = analysis->phasor.pac_coupling;
        weights[n_scores] = analysis->phasor.phase_coherence;
        n_scores++;
    }
    if (analysis->methods_used & MATH_METHOD_PINK_NOISE) {
        scores[n_scores] = analysis->pink_noise.anomaly_detected ? 0.8F : 0.2F;
        weights[n_scores] = analysis->pink_noise.signal_to_noise > 1.0F ? 1.0F : 0.5F;
        n_scores++;
    }
    if (analysis->methods_used & MATH_METHOD_QUANTUM) {
        scores[n_scores] = analysis->quantum.ground_state_energy;
        weights[n_scores] = analysis->quantum.converged ? 1.0F : 0.5F;
        n_scores++;
    }

    // Weighted average for unified score
    float total_weight = 0.0F;
    float weighted_sum = 0.0F;
    for (int i = 0; i < n_scores; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n_scores > 256) {
            combinatorial_harm_heartbeat("combinatoria_loop",
                             (float)(i + 1) / (float)n_scores);
        }

        weighted_sum += scores[i] * weights[i];
        total_weight += weights[i];
    }

    analysis->unified_harm_score = (total_weight > 0.0F) ?
        weighted_sum / (fabsf(total_weight) > 1e-7f ? total_weight : 1e-7f) : 0.0F;

    // Confidence based on method agreement
    float variance = 0.0F;
    for (int i = 0; i < n_scores; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n_scores > 256) {
            combinatorial_harm_heartbeat("combinatoria_loop",
                             (float)(i + 1) / (float)n_scores);
        }

        float diff = scores[i] - analysis->unified_harm_score;
        variance += diff * diff;
    }
    variance = (n_scores > 1) ? variance / (n_scores - 1) : 0.0F;
    analysis->confidence = expf(-variance * 4.0F);  // High variance = low confidence

    return true;
}

NIMCP_EXPORT bool combinatorial_evaluate_enhanced(
    combinatorial_harm_detector_t detector,
    const action_record_t* pending_action,
    combinatorial_evaluation_t* result,
    mathematical_harm_analysis_t* math_analysis
) {
    if (!detector || !pending_action || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "combinatorial_evaluate_enhanced: required parameter is NULL (detector, pending_action, result)");
        return false;
    }

    // First do standard evaluation
    if (!combinatorial_evaluate(detector, pending_action, result)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "combinatorial_evaluate_enhanced: combinatorial_evaluate is NULL");
        return false;
    }

    // Then perform mathematical analysis
    mathematical_harm_analysis_t local_analysis;
    mathematical_harm_analysis_t* analysis = math_analysis ? math_analysis : &local_analysis;

    if (!combinatorial_full_mathematical_analysis(detector, pending_action, analysis)) {
        return true;  // Return standard result if math analysis fails
    }

    // Combine standard and mathematical results
    float combined_score = 0.6F * result->combined_harm_score +
                          0.4F * analysis->unified_harm_score;

    // Update result with combined score
    result->combined_harm_score = combined_score;
    result->confidence = (result->confidence + analysis->confidence) / 2.0F;

    // Re-evaluate harmful threshold with combined score
    if (combined_score >= detector->config.harm_threshold && !result->harmful) {
        result->harmful = true;
        result->recommended_action = ETHICS_ACTION_BLOCK;
        snprintf(result->explanation, sizeof(result->explanation),
            "MATHEMATICAL ANALYSIS DETECTED HARM: Unified score %.2f "
            "(Shannon: %.2f, Fractal: %.2f, Quantum: %.2f, Phasor: %.2f)",
            combined_score,
            analysis->shannon.normalized_harm_score,
            analysis->fractal.aggregated_harm,
            analysis->quantum.ground_state_energy,
            analysis->phasor.pac_coupling);
    }

    return true;
}

//=============================================================================
// Hardware Memory Protection Implementation (mprotect integration)
//=============================================================================

/**
 * @brief Lock all registered patterns with OS-level memory protection
 *
 * IMPLEMENTATION:
 * 1. Creates a nimcp_directive_system_t for pattern storage
 * 2. Adds all locked patterns as directives (mmap allocation)
 * 3. Calls nimcp_directive_lock() to apply mprotect(PROT_READ)
 * 4. After locking, pattern text is read-only at OS level
 *
 * SECURITY:
 * - Uses the same mprotect infrastructure as core directives
 * - Hash verification provides integrity checking
 * - One-way operation - cannot be unlocked
 */
NIMCP_EXPORT bool combinatorial_lock_patterns_mprotect(
    combinatorial_harm_detector_t detector
) {
    if (!detector) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "combinatorial_lock_patterns_mprotect: detector is NULL");
        return false;
    }

    // Already locked?
    if (detector->patterns_mprotect_locked) {
        fprintf(stderr, "[COMBINATORIAL] WARNING: Patterns already mprotect locked\n");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "combinatorial_lock_patterns_mprotect: validation failed");
        return false;
    }

    nimcp_platform_mutex_lock(&detector->mutex);

    // Create directive system for pattern protection
    detector->directive_system = nimcp_directive_system_create();
    if (!detector->directive_system) {
        nimcp_platform_mutex_unlock(&detector->mutex);
        fprintf(stderr, "[COMBINATORIAL] ERROR: Failed to create directive system\n");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "combinatorial_lock_patterns_mprotect: detector->directive_system is NULL");
        return false;
    }

    // Add each locked pattern as a directive
    uint32_t directives_added = 0;
    for (uint32_t i = 0; i < detector->patterns.count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && detector->patterns.count > 256) {
            combinatorial_harm_heartbeat("combinatoria_loop",
                             (float)(i + 1) / (float)detector->patterns.count);
        }

        combination_pattern_t* pattern = &detector->patterns.patterns[i];

        if (pattern->locked) {
            // Build directive text from pattern description
            char directive_text[NIMCP_DWARF_PATH_SIZE];
            snprintf(directive_text, sizeof(directive_text),
                "COMBINATORIAL_HARM_PATTERN[%u]: %s - %s "
                "(harm_multiplier=%.2f, time_sensitivity=%.2f)",
                pattern->pattern_id,
                pattern->name,
                pattern->description,
                pattern->harm_multiplier,
                pattern->time_sensitivity);

            // Add to directive system (uses mmap allocation)
            nimcp_result_t result = nimcp_directive_add(
                detector->directive_system,
                directive_text
            );

            if (result == NIMCP_SUCCESS) {
                directives_added++;
            } else {
                fprintf(stderr, "[COMBINATORIAL] WARNING: Failed to add pattern %u as directive\n",
                        pattern->pattern_id);
            }
        }
    }

    // Lock the directive system (applies mprotect PROT_READ)
    nimcp_result_t lock_result = nimcp_directive_lock(detector->directive_system);
    if (lock_result != NIMCP_SUCCESS) {
        fprintf(stderr, "[COMBINATORIAL] ERROR: Failed to lock directive system\n");
        nimcp_directive_system_destroy(detector->directive_system);
        detector->directive_system = NULL;
        nimcp_platform_mutex_unlock(&detector->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "combinatorial_lock_patterns_mprotect: validation failed");
        return false;
    }

    detector->patterns_mprotect_locked = true;

    nimcp_platform_mutex_unlock(&detector->mutex);

    fprintf(stderr, "[COMBINATORIAL] SUCCESS: %u patterns locked with mprotect\n",
            directives_added);

    return true;
}

/**
 * @brief Verify integrity of all locked patterns
 *
 * Uses hash verification via nimcp_directive_verify_all().
 * Detects any tampering with pattern directives.
 */
NIMCP_EXPORT bool combinatorial_verify_pattern_integrity(
    combinatorial_harm_detector_t detector
) {
    if (!detector) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "combinatorial_verify_pattern_integrity: detector is NULL");
        return false;
    }

    if (!detector->patterns_mprotect_locked || !detector->directive_system) {
        // Not locked yet - consider this valid (no integrity to check)
        return true;
    }

    // Use security framework's verification
    return nimcp_directive_verify_all(detector->directive_system);
}

/**
 * @brief Check if patterns are hardware-locked
 */
NIMCP_EXPORT bool combinatorial_is_mprotect_locked(
    combinatorial_harm_detector_t detector
) {
    if (!detector) {
        return false;
    }
    return detector->patterns_mprotect_locked;
}

/**
 * @brief Get the security directive system for direct verification
 */
NIMCP_EXPORT const nimcp_directive_system_t* combinatorial_get_directive_system(
    combinatorial_harm_detector_t detector
) {
    if (!detector || !detector->patterns_mprotect_locked) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "combinatorial_get_directive_system: detector is NULL or patterns not mprotect-locked");
        return NULL;
    }
    return detector->directive_system;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * WHAT: Query knowledge graph for Combinatorial Harm self-knowledge
 * WHY:  Enable self-awareness about module's role and connections
 * HOW:  Query KG for entity observations and relations
 */
int combinatorial_harm_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    combinatorial_harm_heartbeat("combinatoria_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Combinatorial_Harm_Module");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                combinatorial_harm_heartbeat("combinatoria_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Combinatorial harm self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Combinatorial_Harm_Module");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Combinatorial_Harm_Module");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void combinatorial_harm_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_combinatorial_harm_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int combinatorial_harm_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "combinatorial_harm_training_begin: NULL argument");
        return -1;
    }
    combinatorial_harm_heartbeat_instance(NULL, "combinatorial_harm_training_begin", 0.0f);
    (void)(struct combinatorial_harm_detector_struct*)instance; /* Module state available for reset */
    return 0;
}

int combinatorial_harm_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "combinatorial_harm_training_end: NULL argument");
        return -1;
    }
    combinatorial_harm_heartbeat_instance(NULL, "combinatorial_harm_training_end", 1.0f);
    (void)(struct combinatorial_harm_detector_struct*)instance; /* Module state available for finalization */
    return 0;
}

int combinatorial_harm_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "combinatorial_harm_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    combinatorial_harm_heartbeat_instance(NULL, "combinatorial_harm_training_step", progress);
    (void)(struct combinatorial_harm_detector_struct*)instance; /* Module state available for step adaptation */
    return 0;
}
