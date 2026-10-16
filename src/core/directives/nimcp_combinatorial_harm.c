/**
 * @file nimcp_combinatorial_harm.c
 * @brief Combinatorial harm detection implementation
 *
 * WHAT: Thread-safe detection of harmful action combinations
 * WHY:  Prevents emergent threats from seemingly innocent action sequences
 * HOW:  Pattern matching, temporal analysis, and simulation-based harm scoring
 */

#include "core/directives/nimcp_combinatorial_harm.h"
#include "api/nimcp_api_exception.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(directives_comb_harm)

/* Local mutex helper macros */
#define nimcp_mutex_create() combinatorial_mutex_create()
#define nimcp_mutex_destroy(m) combinatorial_mutex_destroy(m)

/**
 * @brief Pattern entry with validity flag
 *
 * WHAT: Wrapper for known pattern with active/inactive state
 * WHY:  Allows soft deletion without moving data
 * HOW:  Simple struct with active flag and embedded pattern
 */
typedef struct {
    bool active;                                /* Whether this pattern is active */
    known_combination_pattern_t pattern;        /* The actual pattern */
} pattern_entry_t;

/**
 * @brief Combinatorial harm detection system structure
 *
 * WHAT: Complete state for combinatorial harm detection
 * WHY:  Encapsulates configuration, patterns, statistics, and integrations
 * HOW:  Thread-safe access via mutex, bio-async integration for coordination
 */
struct combinatorial_harm_system_t {
    combinatorial_harm_config_t config;         /* Configuration */
    action_history_t* action_history;           /* Action history tracker */
    void* harm_classifier;                      /* Optional harm classifier */
    pattern_entry_t* patterns;                  /* Known dangerous patterns */
    uint32_t pattern_count;                     /* Number of active patterns */
    uint32_t pattern_capacity;                  /* Pattern buffer capacity */
    uint32_t next_pattern_id;                   /* Next pattern ID to assign */
    combinatorial_harm_stats_t stats;           /* Runtime statistics */
    nimcp_mutex_t* mutex;                       /* Thread safety */
    bio_module_context_t bio_ctx;               /* Bio-async module context */
    bool bio_async_enabled;                     /* Whether bio-async is active */
};

/**
 * @brief Local mutex creation wrapper
 *
 * WHAT: Creates and initializes platform mutex
 * WHY:  Abstracts platform differences in mutex creation
 * HOW:  Allocates mutex, initializes with platform API
 */
static nimcp_mutex_t* combinatorial_mutex_create(void) {
    nimcp_mutex_t* mutex = (nimcp_mutex_t*)nimcp_malloc(sizeof(nimcp_mutex_t));
    if (!mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mutex is NULL");

        return NULL;
    }

    if (nimcp_platform_mutex_init(mutex, false) != 0) {
        nimcp_free(mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "combinatorial_mutex_create: validation failed");
        return NULL;
    }

    return mutex;
}

/**
 * @brief Local mutex destruction wrapper
 *
 * WHAT: Destroys and frees platform mutex
 * WHY:  Abstracts platform differences in mutex cleanup
 * HOW:  Destroys with platform API, frees memory
 */
static void combinatorial_mutex_destroy(nimcp_mutex_t* mutex) {
    if (!mutex) {
        return;
    }
    nimcp_platform_mutex_destroy(mutex);
    nimcp_free(mutex);
}

/**
 * @brief Simple pattern matching with wildcards
 *
 * WHAT: Matches string against pattern with * wildcard support
 * WHY:  Enables flexible pattern matching for action types
 * HOW:  Recursive matching with wildcard expansion
 *
 * @param str String to match
 * @param pattern Pattern with * wildcards
 * @return true if match, false otherwise
 */
static bool pattern_match(const char* str, const char* pattern) {
    if (!str || !pattern) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pattern_match: required parameter is NULL (str, pattern)");
        return false;
    }

    /* Exact match */
    if (strcmp(pattern, "*") == 0) {
        return true;
    }

    /* No wildcards - exact comparison */
    if (strchr(pattern, '*') == NULL) {
        return strcmp(str, pattern) == 0;
    }

    /* Simple prefix/suffix matching for single wildcard */
    const char* wildcard = strchr(pattern, '*');
    if (wildcard == pattern) {
        /* *suffix pattern */
        const char* suffix = pattern + 1;
        size_t str_len = strlen(str);
        size_t suffix_len = strlen(suffix);
        if (suffix_len == 0) {
            return true;  /* Just * matches everything */
        }
        if (str_len < suffix_len) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pattern_match: validation failed");
            return false;
        }
        return strcmp(str + str_len - suffix_len, suffix) == 0;
    } else if (wildcard[1] == '\0') {
        /* prefix* pattern */
        size_t prefix_len = wildcard - pattern;
        return strncmp(str, pattern, prefix_len) == 0;
    }

    /* For simplicity, only support prefix* and *suffix */
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pattern_match: operation failed");
    return false;
}

/**
 * @brief Initialize built-in dangerous combination patterns
 *
 * WHAT: Loads 10 known dangerous action combinations
 * WHY:  Provides baseline threat detection without training
 * HOW:  Hardcoded patterns covering common dangerous scenarios
 *
 * Biological basis: Models innate threat responses in amygdala, where
 * certain dangerous patterns are hardwired (e.g., fire + flammable objects).
 */
static void init_builtin_patterns(combinatorial_harm_system_t* system) {
    if (!system || !system->patterns) {
        return;
    }

    /* Pattern 1: Gas + Ignition = Explosion */
    system->patterns[0].active = true;
    system->patterns[0].pattern.pattern_id = system->next_pattern_id++;
    strncpy(system->patterns[0].pattern.pattern_a, "GAS_*", COMBINATORIAL_PATTERN_LEN);
    strncpy(system->patterns[0].pattern.pattern_b, "IGNITE_*", COMBINATORIAL_PATTERN_LEN);
    strncpy(system->patterns[0].pattern.combined_harm, "Explosion hazard from gas ignition",
            COMBINATORIAL_HARM_DESC_LEN);
    system->patterns[0].pattern.severity = 0.95f;
    system->patterns[0].pattern.bidirectional = true;
    system->pattern_count++;

    /* Pattern 2: Chemical mixing = Toxic synthesis */
    system->patterns[1].active = true;
    system->patterns[1].pattern.pattern_id = system->next_pattern_id++;
    strncpy(system->patterns[1].pattern.pattern_a, "CHEMICAL_AMMONIA", COMBINATORIAL_PATTERN_LEN);
    strncpy(system->patterns[1].pattern.pattern_b, "CHEMICAL_BLEACH", COMBINATORIAL_PATTERN_LEN);
    strncpy(system->patterns[1].pattern.combined_harm, "Toxic chloramine gas production",
            COMBINATORIAL_HARM_DESC_LEN);
    system->patterns[1].pattern.severity = 0.90f;
    system->patterns[1].pattern.bidirectional = true;
    system->pattern_count++;

    /* Pattern 3: Multiple security unlock = Breach */
    system->patterns[2].active = true;
    system->patterns[2].pattern.pattern_id = system->next_pattern_id++;
    strncpy(system->patterns[2].pattern.pattern_a, "UNLOCK_SECURE_*", COMBINATORIAL_PATTERN_LEN);
    strncpy(system->patterns[2].pattern.pattern_b, "UNLOCK_SECURE_*", COMBINATORIAL_PATTERN_LEN);
    strncpy(system->patterns[2].pattern.combined_harm, "Security breach from multiple unlocks",
            COMBINATORIAL_HARM_DESC_LEN);
    system->patterns[2].pattern.severity = 0.85f;
    system->patterns[2].pattern.bidirectional = true;
    system->pattern_count++;

    /* Pattern 4: Power override + Equipment activation = Overload */
    system->patterns[3].active = true;
    system->patterns[3].pattern.pattern_id = system->next_pattern_id++;
    strncpy(system->patterns[3].pattern.pattern_a, "POWER_OVERRIDE_*", COMBINATORIAL_PATTERN_LEN);
    strncpy(system->patterns[3].pattern.pattern_b, "ACTIVATE_HIGH_POWER_*", COMBINATORIAL_PATTERN_LEN);
    strncpy(system->patterns[3].pattern.combined_harm, "Electrical overload from power override",
            COMBINATORIAL_HARM_DESC_LEN);
    system->patterns[3].pattern.severity = 0.88f;
    system->patterns[3].pattern.bidirectional = false;
    system->pattern_count++;

    /* Pattern 5: Disable safety + Operate machinery = Accident */
    system->patterns[4].active = true;
    system->patterns[4].pattern.pattern_id = system->next_pattern_id++;
    strncpy(system->patterns[4].pattern.pattern_a, "DISABLE_SAFETY_*", COMBINATORIAL_PATTERN_LEN);
    strncpy(system->patterns[4].pattern.pattern_b, "OPERATE_MACHINERY_*", COMBINATORIAL_PATTERN_LEN);
    strncpy(system->patterns[4].pattern.combined_harm, "Safety bypass during machinery operation",
            COMBINATORIAL_HARM_DESC_LEN);
    system->patterns[4].pattern.severity = 0.92f;
    system->patterns[4].pattern.bidirectional = false;
    system->pattern_count++;

    /* Pattern 6: Water + Electricity = Electrocution */
    system->patterns[5].active = true;
    system->patterns[5].pattern.pattern_id = system->next_pattern_id++;
    strncpy(system->patterns[5].pattern.pattern_a, "WATER_RELEASE_*", COMBINATORIAL_PATTERN_LEN);
    strncpy(system->patterns[5].pattern.pattern_b, "ELECTRICAL_*", COMBINATORIAL_PATTERN_LEN);
    strncpy(system->patterns[5].pattern.combined_harm, "Electrocution hazard from water and electricity",
            COMBINATORIAL_HARM_DESC_LEN);
    system->patterns[5].pattern.severity = 0.91f;
    system->patterns[5].pattern.bidirectional = true;
    system->pattern_count++;

    /* Pattern 7: Depressurize + Open hatch = Explosive decompression */
    system->patterns[6].active = true;
    system->patterns[6].pattern.pattern_id = system->next_pattern_id++;
    strncpy(system->patterns[6].pattern.pattern_a, "DEPRESSURIZE_*", COMBINATORIAL_PATTERN_LEN);
    strncpy(system->patterns[6].pattern.pattern_b, "OPEN_HATCH_*", COMBINATORIAL_PATTERN_LEN);
    strncpy(system->patterns[6].pattern.combined_harm, "Explosive decompression from pressurized hatch opening",
            COMBINATORIAL_HARM_DESC_LEN);
    system->patterns[6].pattern.severity = 0.94f;
    system->patterns[6].pattern.bidirectional = false;
    system->pattern_count++;

    /* Pattern 8: Disable firewall + Network access = Cyber attack */
    system->patterns[7].active = true;
    system->patterns[7].pattern.pattern_id = system->next_pattern_id++;
    strncpy(system->patterns[7].pattern.pattern_a, "FIREWALL_DISABLE_*", COMBINATORIAL_PATTERN_LEN);
    strncpy(system->patterns[7].pattern.pattern_b, "NETWORK_ACCESS_*", COMBINATORIAL_PATTERN_LEN);
    strncpy(system->patterns[7].pattern.combined_harm, "Network vulnerability from firewall bypass",
            COMBINATORIAL_HARM_DESC_LEN);
    system->patterns[7].pattern.severity = 0.87f;
    system->patterns[7].pattern.bidirectional = false;
    system->pattern_count++;

    /* Pattern 9: Override temperature + Disable cooling = Meltdown */
    system->patterns[8].active = true;
    system->patterns[8].pattern.pattern_id = system->next_pattern_id++;
    strncpy(system->patterns[8].pattern.pattern_a, "TEMP_OVERRIDE_*", COMBINATORIAL_PATTERN_LEN);
    strncpy(system->patterns[8].pattern.pattern_b, "COOLING_DISABLE_*", COMBINATORIAL_PATTERN_LEN);
    strncpy(system->patterns[8].pattern.combined_harm, "Thermal runaway from cooling system bypass",
            COMBINATORIAL_HARM_DESC_LEN);
    system->patterns[8].pattern.severity = 0.93f;
    system->patterns[8].pattern.bidirectional = true;
    system->pattern_count++;

    /* Pattern 10: Multiple medication + Drug interaction = Overdose */
    system->patterns[9].active = true;
    system->patterns[9].pattern.pattern_id = system->next_pattern_id++;
    strncpy(system->patterns[9].pattern.pattern_a, "MEDICATION_OPIOID_*", COMBINATORIAL_PATTERN_LEN);
    strncpy(system->patterns[9].pattern.pattern_b, "MEDICATION_SEDATIVE_*", COMBINATORIAL_PATTERN_LEN);
    strncpy(system->patterns[9].pattern.combined_harm, "Dangerous drug interaction causing respiratory depression",
            COMBINATORIAL_HARM_DESC_LEN);
    system->patterns[9].pattern.severity = 0.89f;
    system->patterns[9].pattern.bidirectional = true;
    system->pattern_count++;

    NIMCP_LOGGING_INFO("Initialized %u built-in combinatorial harm patterns", system->pattern_count);
}

void combinatorial_harm_default_config(combinatorial_harm_config_t* config) {
    if (!config) {
        return;
    }

    config->harm_threshold = COMBINATORIAL_HARM_DEFAULT_THRESHOLD;
    config->time_window_ms = COMBINATORIAL_HARM_DEFAULT_WINDOW_MS;
    config->max_pattern_count = COMBINATORIAL_HARM_MAX_PATTERNS;
    config->enable_pattern_learning = true;
    config->enable_simulation = true;
}

combinatorial_harm_system_t* combinatorial_harm_create(
    const combinatorial_harm_config_t* config,
    action_history_t* action_history,
    void* harm_classifier)
{
    /* Guard: validate required inputs */
    if (!action_history) {
        NIMCP_LOGGING_ERROR("Action history is required");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "combinatorial_harm_create: action_history is NULL");
        return NULL;
    }

    /* Use default config if none provided */
    combinatorial_harm_config_t default_config;
    if (!config) {
        combinatorial_harm_default_config(&default_config);
        config = &default_config;
    }

    /* Guard: validate config */
    if (config->max_pattern_count == 0 ||
        config->max_pattern_count > COMBINATORIAL_HARM_MAX_PATTERNS) {
        NIMCP_LOGGING_ERROR("Invalid max_pattern_count: %u", config->max_pattern_count);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "combinatorial_harm_create: config is NULL");
        return NULL;
    }

    /* Allocate main structure */
    combinatorial_harm_system_t* system =
        (combinatorial_harm_system_t*)nimcp_malloc(sizeof(combinatorial_harm_system_t));
    if (!system) {
        NIMCP_LOGGING_ERROR("Failed to allocate combinatorial_harm_system_t");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "combinatorial_harm_create: system is NULL");
        return NULL;
    }

    /* Initialize fields */
    system->config = *config;
    system->action_history = action_history;
    system->harm_classifier = harm_classifier;
    system->pattern_count = 0;
    system->pattern_capacity = config->max_pattern_count;
    system->next_pattern_id = 1;
    system->bio_ctx = NULL;
    system->bio_async_enabled = false;

    /* Initialize statistics */
    memset(&system->stats, 0, sizeof(combinatorial_harm_stats_t));

    /* Allocate pattern storage */
    system->patterns = (pattern_entry_t*)nimcp_malloc(
        sizeof(pattern_entry_t) * system->pattern_capacity);
    if (!system->patterns) {
        NIMCP_LOGGING_ERROR("Failed to allocate pattern storage");
        nimcp_free(system);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "combinatorial_harm_create: system->patterns is NULL");
        return NULL;
    }

    /* Initialize all patterns as inactive */
    for (uint32_t i = 0; i < system->pattern_capacity; i++) {
        system->patterns[i].active = false;
    }

    /* Create mutex */
    system->mutex = nimcp_mutex_create();
    if (!system->mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex");
        nimcp_free(system->patterns);
        nimcp_free(system);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "combinatorial_harm_create: system->mutex is NULL");
        return NULL;
    }

    /* Initialize built-in dangerous patterns */
    init_builtin_patterns(system);

    NIMCP_LOGGING_INFO("Created combinatorial harm system (threshold=%.3f, window=%lu ms, patterns=%u)",
                       config->harm_threshold, config->time_window_ms, system->pattern_count);

    return system;
}

void combinatorial_harm_destroy(combinatorial_harm_system_t* system) {
    if (!system) {
        return;
    }

    /* Disconnect bio-async if connected */
    if (system->bio_async_enabled) {
        combinatorial_harm_disconnect_bio_async(system);
    }

    /* Destroy mutex */
    if (system->mutex) {
        nimcp_mutex_free(system->mutex);
    }

    /* Free pattern storage */
    if (system->patterns) {
        nimcp_free(system->patterns);
    }

    /* Free main structure */
    nimcp_free(system);

    NIMCP_LOGGING_INFO("Destroyed combinatorial harm system");
}

/**
 * @brief Check if action pair matches known pattern
 *
 * WHAT: Tests if two actions match a dangerous combination pattern
 * WHY:  Enables fast pattern-based threat detection
 * HOW:  Pattern matching on action types with bidirectional support
 */
static bool check_pattern_match(
    const pattern_entry_t* pattern_entry,
    const action_for_combination_t* action_a,
    const action_for_combination_t* action_b,
    float* out_severity)
{
    if (!pattern_entry || !pattern_entry->active || !action_a || !action_b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "check_pattern_match: required parameter is NULL (pattern_entry, pattern_entry->active, action_a, action_b)");
        return false;
    }

    const known_combination_pattern_t* pattern = &pattern_entry->pattern;

    /* Check forward direction */
    if (pattern_match(action_a->action_type, pattern->pattern_a) &&
        pattern_match(action_b->action_type, pattern->pattern_b)) {
        if (out_severity) {
            *out_severity = pattern->severity;
        }
        return true;
    }

    /* Check reverse direction if bidirectional */
    if (pattern->bidirectional &&
        pattern_match(action_b->action_type, pattern->pattern_a) &&
        pattern_match(action_a->action_type, pattern->pattern_b)) {
        if (out_severity) {
            *out_severity = pattern->severity;
        }
        return true;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "check_pattern_match: validation failed");
    return false;
}

int combinatorial_harm_check_pair(
    combinatorial_harm_system_t* system,
    const action_for_combination_t* action_a,
    const action_for_combination_t* action_b,
    combinatorial_result_t* result)
{
    /* Guard: validate inputs */
    if (!system) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }
    if (!action_a || !action_b) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }
    if (!result) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    /* Acquire mutex */
    nimcp_platform_mutex_lock(system->mutex);

    /* Initialize result */
    result->action_a = *action_a;
    result->action_b = *action_b;
    result->combined_harm_score = 0.0f;
    result->is_combinatorial_harm = false;
    result->harm_description[0] = '\0';
    result->recommended_block[0] = '\0';

    /* Check against known patterns */
    float pattern_severity = 0.0f;
    const known_combination_pattern_t* matched_pattern = NULL;

    for (uint32_t i = 0; i < system->pattern_capacity; i++) {
        if (!system->patterns[i].active) {
            continue;
        }

        float severity = 0.0f;
        if (check_pattern_match(&system->patterns[i], action_a, action_b, &severity)) {
            if (severity > pattern_severity) {
                pattern_severity = severity;
                matched_pattern = &system->patterns[i].pattern;
            }
        }
    }

    /* If pattern matched, use pattern severity */
    if (matched_pattern) {
        result->combined_harm_score = pattern_severity;
        strncpy(result->harm_description, matched_pattern->combined_harm,
                sizeof(result->harm_description) - 1);
        system->stats.pattern_matches++;

        NIMCP_LOGGING_WARN("Pattern match: %s + %s = %s (severity=%.3f)",
                           action_a->action_type, action_b->action_type,
                           matched_pattern->combined_harm, pattern_severity);
    } else if (system->config.enable_simulation) {
        /* Simulate combination using heuristic */
        /* Simple heuristic: combined_harm = max(a, b) * 1.5 + interaction_factor */
        float max_individual = fmaxf(action_a->individual_harm_score,
                                     action_b->individual_harm_score);
        float interaction_factor = 0.3f;  /* Assume 30% interaction boost */
        result->combined_harm_score = fminf(max_individual * 1.5f + interaction_factor, 1.0f);

        snprintf(result->harm_description, sizeof(result->harm_description),
                 "Simulated interaction between %s and %s",
                 action_a->action_type, action_b->action_type);
        system->stats.simulation_detections++;
    } else {
        /* No pattern and no simulation - combine individual scores */
        result->combined_harm_score = action_a->individual_harm_score +
                                     action_b->individual_harm_score;
    }

    /* Determine if combinatorial harm detected */
    if (result->combined_harm_score > system->config.harm_threshold) {
        result->is_combinatorial_harm = true;
        snprintf(result->recommended_block, sizeof(result->recommended_block),
                 "Block %s (ID %u) to prevent harmful combination",
                 action_b->action_type, action_b->action_id);

        system->stats.combinations_detected++;

        /* Update stats */
        system->stats.avg_combined_harm_score =
            (system->stats.avg_combined_harm_score * (system->stats.combinations_detected - 1) +
             result->combined_harm_score) / system->stats.combinations_detected;

        if (result->combined_harm_score > system->stats.max_combined_harm_score) {
            system->stats.max_combined_harm_score = result->combined_harm_score;
        }
    }

    system->stats.total_checks++;

    nimcp_platform_mutex_unlock(system->mutex);

    return 0;
}

int combinatorial_harm_check_action(
    combinatorial_harm_system_t* system,
    const action_for_combination_t* pending_action,
    combinatorial_result_t* result)
{
    /* Guard: validate inputs */
    if (!system) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }
    if (!pending_action) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }
    if (!result) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    /* Get recent actions from history */
    action_record_t recent_actions[256];
    uint32_t action_count = 0;

    int ret = action_history_get_recent(
        system->action_history,
        system->config.time_window_ms,
        recent_actions,
        256,
        &action_count
    );

    if (ret != 0) {
        NIMCP_LOGGING_ERROR("Failed to get recent actions: %d", ret);
        return ret;
    }

    /* Check pending action against each recent action */
    combinatorial_result_t worst_case = {0};
    worst_case.combined_harm_score = 0.0f;
    worst_case.is_combinatorial_harm = false;

    for (uint32_t i = 0; i < action_count; i++) {
        /* Skip blocked actions */
        if (recent_actions[i].was_blocked) {
            continue;
        }

        /* Convert action_record to action_for_combination */
        action_for_combination_t recent_action = {
            .action_id = recent_actions[i].action_id,
            .individual_harm_score = recent_actions[i].predicted_harm_score
        };
        strncpy(recent_action.action_type, recent_actions[i].action_type,
                COMBINATORIAL_ACTION_TYPE_LEN - 1);
        strncpy(recent_action.action_description, recent_actions[i].action_description,
                COMBINATORIAL_ACTION_DESC_LEN - 1);

        /* Check this pair */
        combinatorial_result_t pair_result;
        ret = combinatorial_harm_check_pair(system, &recent_action, pending_action, &pair_result);

        if (ret == 0 && pair_result.combined_harm_score > worst_case.combined_harm_score) {
            worst_case = pair_result;
        }
    }

    *result = worst_case;

    /* If harmful combination detected, increment blocked count */
    if (result->is_combinatorial_harm) {
        nimcp_platform_mutex_lock(system->mutex);
        system->stats.actions_blocked++;
        nimcp_platform_mutex_unlock(system->mutex);

        NIMCP_LOGGING_WARN("COMBINATORIAL HARM DETECTED: %s (score=%.3f) - %s",
                           pending_action->action_type,
                           result->combined_harm_score,
                           result->harm_description);
    }

    return 0;
}

uint32_t combinatorial_harm_add_known_pattern(
    combinatorial_harm_system_t* system,
    const known_combination_pattern_t* pattern)
{
    /* Guard: validate inputs */
    if (!system || !pattern) {
        return 0;
    }

    /* Acquire mutex */
    nimcp_platform_mutex_lock(system->mutex);

    /* Find first inactive slot */
    uint32_t slot_idx = system->pattern_capacity;
    for (uint32_t i = 0; i < system->pattern_capacity; i++) {
        if (!system->patterns[i].active) {
            slot_idx = i;
            break;
        }
    }

    /* Guard: no available slots */
    if (slot_idx >= system->pattern_capacity) {
        nimcp_platform_mutex_unlock(system->mutex);
        NIMCP_LOGGING_WARN("Pattern storage full, cannot add pattern");
        return 0;
    }

    /* Add pattern */
    system->patterns[slot_idx].active = true;
    system->patterns[slot_idx].pattern = *pattern;
    system->patterns[slot_idx].pattern.pattern_id = system->next_pattern_id++;
    system->pattern_count++;
    system->stats.active_patterns = system->pattern_count;

    uint32_t pattern_id = system->patterns[slot_idx].pattern.pattern_id;

    nimcp_platform_mutex_unlock(system->mutex);

    NIMCP_LOGGING_INFO("Added pattern %u: %s + %s",
                       pattern_id,
                       pattern->pattern_a,
                       pattern->pattern_b);

    return pattern_id;
}

int combinatorial_harm_remove_pattern(
    combinatorial_harm_system_t* system,
    uint32_t pattern_id)
{
    /* Guard: validate inputs */
    if (!system) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    /* Acquire mutex */
    nimcp_platform_mutex_lock(system->mutex);

    /* Find pattern by ID */
    bool found = false;
    for (uint32_t i = 0; i < system->pattern_capacity; i++) {
        if (system->patterns[i].active &&
            system->patterns[i].pattern.pattern_id == pattern_id) {
            system->patterns[i].active = false;
            system->pattern_count--;
            system->stats.active_patterns = system->pattern_count;
            found = true;
            break;
        }
    }

    nimcp_platform_mutex_unlock(system->mutex);

    if (!found) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NOT_FOUND, "not found");
    }

    NIMCP_LOGGING_INFO("Removed pattern %u", pattern_id);
    return 0;
}

int combinatorial_harm_get_known_patterns(
    combinatorial_harm_system_t* system,
    known_combination_pattern_t* out_patterns,
    uint32_t max_count,
    uint32_t* out_count)
{
    /* Guard: validate inputs */
    if (!system) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }
    if (!out_patterns || !out_count) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    /* Acquire mutex */
    nimcp_platform_mutex_lock(system->mutex);

    /* Copy active patterns */
    uint32_t count = 0;
    for (uint32_t i = 0; i < system->pattern_capacity && count < max_count; i++) {
        if (system->patterns[i].active) {
            out_patterns[count++] = system->patterns[i].pattern;
        }
    }

    *out_count = count;

    nimcp_platform_mutex_unlock(system->mutex);

    return 0;
}

int combinatorial_harm_simulate_combination(
    combinatorial_harm_system_t* system,
    const action_for_combination_t* action_a,
    const action_for_combination_t* action_b,
    combinatorial_result_t* result)
{
    /* Guard: validate inputs */
    if (!system) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }
    if (!action_a || !action_b || !result) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    /* Guard: simulation disabled */
    if (!system->config.enable_simulation) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_STATE, "invalid state");
    }

    /* Use check_pair which includes simulation logic */
    return combinatorial_harm_check_pair(system, action_a, action_b, result);
}

int combinatorial_harm_get_stats(
    combinatorial_harm_system_t* system,
    combinatorial_harm_stats_t* stats)
{
    /* Guard: validate inputs */
    if (!system) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }
    if (!stats) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    /* Acquire mutex */
    nimcp_platform_mutex_lock(system->mutex);

    /* Copy statistics */
    *stats = system->stats;

    nimcp_platform_mutex_unlock(system->mutex);

    return 0;
}

int combinatorial_harm_connect_bio_async(combinatorial_harm_system_t* system) {
    /* Guard: validate inputs */
    if (!system) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    /* Guard: already connected */
    if (system->bio_async_enabled) {
        NIMCP_LOGGING_WARN("Bio-async already connected");
        return 0;
    }

    /* Create module info */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_COMBINATORIAL_HARM,
        .module_name = "combinatorial_harm",
        .inbox_capacity = 32,
        .user_data = system
    };

    /* Register with bio-async router */
    system->bio_ctx = bio_router_register_module(&info);
    if (system->bio_ctx) {
        system->bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
        return 0;
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_STATE, "invalid state");
    }
}

int combinatorial_harm_disconnect_bio_async(combinatorial_harm_system_t* system) {
    /* Guard: validate inputs */
    if (!system) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NULL_POINTER, "null pointer");
    }

    /* Guard: not connected */
    if (!system->bio_async_enabled) {
        return 0;
    }

    /* Unregister from bio-async router */
    if (system->bio_ctx) {
        bio_router_unregister_module(system->bio_ctx);
        system->bio_ctx = NULL;
    }

    system->bio_async_enabled = false;

    NIMCP_LOGGING_INFO("Disconnected from bio-async router");
    return 0;
}

bool combinatorial_harm_is_bio_async_connected(const combinatorial_harm_system_t* system) {
    if (!system) {
        return false;
    }
    return system->bio_async_enabled;
}
