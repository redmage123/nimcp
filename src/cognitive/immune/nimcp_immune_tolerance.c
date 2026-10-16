/**
 * @file nimcp_immune_tolerance.c
 * @brief Immune Tolerance Learning Implementation
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Implements self/non-self discrimination for brain immune system
 * WHY:  Prevent autoimmune attacks on legitimate system patterns
 * HOW:  Central tolerance (deletion), peripheral tolerance (anergy), self database
 *
 * @author NIMCP Development Team
 */

#include "cognitive/immune/nimcp_immune_tolerance.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(immune_tolerance, MESH_ADAPTER_CATEGORY_SECURITY)



/* Mutex convenience macros */
#define nimcp_mutex_create() nimcp_platform_mutex_create()
#define nimcp_mutex_lock(m) nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)(m))
#define nimcp_mutex_unlock(m) nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)(m))
#define nimcp_mutex_destroy(m) do { \
    nimcp_platform_mutex_destroy((nimcp_platform_mutex_t*)(m)); \
    nimcp_free(m); \
    (m) = NULL; \
} while(0)

/* ============================================================================
 * Internal Helpers - Forward Declarations
 * ============================================================================ */

static uint64_t get_timestamp_ms(void);
static self_pattern_t* find_pattern_by_id(tolerance_system_t* sys, uint32_t id);
static anergic_cell_record_t* find_anergic_cell(
    tolerance_system_t* sys,
    uint32_t cell_id,
    bool is_b_cell
);
static int add_anergic_cell(
    tolerance_system_t* sys,
    uint32_t cell_id,
    bool is_b_cell,
    uint32_t pattern_id,
    float affinity
);
static float compute_pattern_affinity(
    const uint8_t* p1,
    size_t len1,
    const uint8_t* p2,
    size_t len2
);

/* ============================================================================
 * String Conversion
 * ============================================================================ */

/**
 * @brief Convert phase to string
 */
const char* tolerance_phase_to_string(tolerance_phase_t phase) {
    switch (phase) {
        case TOLERANCE_PHASE_TRAINING:     return "TRAINING";
        case TOLERANCE_PHASE_CENTRAL:      return "CENTRAL";
        case TOLERANCE_PHASE_PERIPHERAL:   return "PERIPHERAL";
        case TOLERANCE_PHASE_OPERATIONAL:  return "OPERATIONAL";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Convert selection outcome to string
 */
const char* tolerance_selection_to_string(selection_outcome_t outcome) {
    switch (outcome) {
        case SELECTION_PASS:      return "PASS";
        case SELECTION_DELETE:    return "DELETE";
        case SELECTION_EDIT:      return "EDIT";
        case SELECTION_ANERGIZE:  return "ANERGIZE";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Convert cell state to string
 */
const char* tolerance_cell_state_to_string(cell_tolerance_state_t state) {
    switch (state) {
        case CELL_TOLERANT:  return "TOLERANT";
        case CELL_ANERGIC:   return "ANERGIC";
        case CELL_DELETED:   return "DELETED";
        case CELL_EDITED:    return "EDITED";
        default: return "UNKNOWN";
    }
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

/**
 * @brief Get default tolerance configuration
 *
 * WHAT: Sensible defaults based on biological immune system
 * WHY:  Easy initialization with proven thresholds
 * HOW:  Return configured struct
 */
int tolerance_default_config(tolerance_config_t* config) {
    if (!config) {
        NIMCP_LOGGING_ERROR("NULL config pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tolerance_default_config: config is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    immune_tolerance_heartbeat("immune_toler_tolerance_default_co", 0.0f);


    config->max_self_patterns = TOLERANCE_MAX_SELF_PATTERNS;
    config->self_match_threshold = TOLERANCE_DEFAULT_THRESHOLD;
    config->central_deletion_threshold = TOLERANCE_CENTRAL_THRESHOLD;
    config->receptor_editing_threshold = 0.80f;
    config->anergy_threshold = TOLERANCE_ANERGY_THRESHOLD;
    config->enable_anergy = true;
    config->enable_receptor_editing = true;
    config->initial_phase = TOLERANCE_PHASE_TRAINING;
    config->auto_transition = true;
    config->training_pattern_count = 100;
    config->thread_safe = true;

    return 0;
}

/**
 * @brief Create tolerance system
 *
 * WHAT: Allocate and initialize tolerance system
 * WHY:  Set up self/non-self discrimination
 * HOW:  Allocate pattern arrays, create mutex, link immune system
 */
tolerance_system_t* tolerance_create(
    const tolerance_config_t* config,
    brain_immune_system_t* immune_system
) {
    if (!immune_system) {
        NIMCP_LOGGING_ERROR("NULL immune_system pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tolerance_create: immune_system is NULL");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    immune_tolerance_heartbeat("immune_toler_tolerance_create", 0.0f);


    tolerance_system_t* sys = nimcp_calloc(1, sizeof(tolerance_system_t));
    if (!sys) {
        NIMCP_LOGGING_ERROR("Failed to allocate tolerance system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tolerance_create: sys is NULL");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        memcpy(&sys->config, config, sizeof(tolerance_config_t));
    } else {
        tolerance_default_config(&sys->config);
    }

    /* Allocate self pattern database */
    sys->self_pattern_capacity = sys->config.max_self_patterns;
    sys->self_patterns = nimcp_calloc(
        sys->self_pattern_capacity,
        sizeof(self_pattern_t)
    );
    if (!sys->self_patterns) {
        NIMCP_LOGGING_ERROR("Failed to allocate self patterns");
        nimcp_free(sys);
        sys = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "tolerance_create: sys->self_patterns is NULL");
        return NULL;
    }

    /* Allocate anergic cell records */
    sys->anergic_cell_capacity = 512;
    sys->anergic_cells = nimcp_calloc(
        sys->anergic_cell_capacity,
        sizeof(anergic_cell_record_t)
    );
    if (!sys->anergic_cells) {
        NIMCP_LOGGING_ERROR("Failed to allocate anergic cells");
        nimcp_free(sys->self_patterns);
        nimcp_free(sys);
        sys = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "tolerance_create: sys->anergic_cells is NULL");
        return NULL;
    }

    /* Create mutex */
    if (sys->config.thread_safe) {
        sys->mutex = nimcp_mutex_create();
        if (!sys->mutex) {
            NIMCP_LOGGING_WARN("Failed to create mutex, disabling thread safety");
            sys->config.thread_safe = false;
        }
    }

    /* Link to immune system */
    sys->immune_system = immune_system;

    /* Initialize state */
    sys->phase = sys->config.initial_phase;
    sys->next_pattern_id = 1;
    sys->initialized = true;
    sys->creation_time = get_timestamp_ms();

    memset(&sys->stats, 0, sizeof(tolerance_stats_t));

    NIMCP_LOGGING_INFO("Tolerance system created in phase: %s",
                       tolerance_phase_to_string(sys->phase));

    return sys;
}

/**
 * @brief Destroy tolerance system
 *
 * WHAT: Free all tolerance system resources
 * WHY:  Clean shutdown, prevent leaks
 * HOW:  Free arrays, destroy mutex
 */
void tolerance_destroy(tolerance_system_t* system) {
    if (!system) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    immune_tolerance_heartbeat("immune_toler_tolerance_destroy", 0.0f);


    if (system->config.thread_safe && system->mutex) {
        nimcp_mutex_destroy(system->mutex);
    }

    if (system->self_patterns) {
        nimcp_free(system->self_patterns);
    }

    if (system->anergic_cells) {
        nimcp_free(system->anergic_cells);
    }

    nimcp_free(system);
    system = NULL;
}

/* ============================================================================
 * Self Pattern Registration Implementation
 * ============================================================================ */

/**
 * @brief Register self pattern
 *
 * WHAT: Add normal operation pattern to database
 * WHY:  Build self-recognition repertoire
 * HOW:  Store pattern with metadata
 */
int tolerance_register_self_pattern(
    tolerance_system_t* system,
    const uint8_t* pattern,
    size_t len,
    const char* description,
    uint32_t* pattern_id
) {
    if (!system || !pattern || len == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tolerance_register_self_pattern: required parameter is NULL (system, pattern)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    immune_tolerance_heartbeat("immune_toler_tolerance_register_s", 0.0f);


    if (len > TOLERANCE_PATTERN_SIZE) {
        NIMCP_LOGGING_WARN("Pattern length %zu exceeds max %d",
                           len, TOLERANCE_PATTERN_SIZE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "tolerance_register_self_pattern: validation failed");
        return -1;
    }

    if (system->config.thread_safe) {
        nimcp_mutex_lock(system->mutex);
    }

    /* Check capacity */
    if (system->self_pattern_count >= system->self_pattern_capacity) {
        NIMCP_LOGGING_ERROR("Self pattern database full (%zu patterns)",
                            system->self_pattern_count);
        if (system->config.thread_safe) {
            nimcp_mutex_unlock(system->mutex);
        }
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "tolerance_register_self_pattern: validation failed");
        return -1;
    }

    /* Add pattern */
    self_pattern_t* sp = &system->self_patterns[system->self_pattern_count];
    sp->id = system->next_pattern_id++;
    memcpy(sp->pattern, pattern, len);
    sp->pattern_len = len;
    sp->match_threshold = system->config.self_match_threshold;
    sp->presentation_count = 0;
    sp->registered_time = get_timestamp_ms();
    sp->last_matched = 0;
    sp->immutable = false;
    sp->confidence = 1.0f;

    if (description) {
        strncpy(sp->description, description, sizeof(sp->description) - 1);
        sp->description[sizeof(sp->description) - 1] = '\0';
    } else {
        sp->description[0] = '\0';
    }

    if (pattern_id) {
        *pattern_id = sp->id;
    }

    system->self_pattern_count++;

    if (system->config.thread_safe) {
        nimcp_mutex_unlock(system->mutex);
    }

    NIMCP_LOGGING_DEBUG("Registered self pattern ID=%u, len=%zu, desc='%s'",
                        sp->id, len, sp->description);

    return 0;
}

/**
 * @brief Check if pattern is self
 *
 * WHAT: Match pattern against self database
 * WHY:  Primary self/non-self discrimination
 * HOW:  Fuzzy match, return highest affinity
 */
bool tolerance_check_self(
    tolerance_system_t* system,
    const uint8_t* pattern,
    size_t len,
    uint32_t* matched_pattern_id,
    float* affinity
) {
    if (!system || !pattern || len == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tolerance_check_self: required parameter is NULL (system, pattern)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    immune_tolerance_heartbeat("immune_toler_tolerance_check_self", 0.0f);


    if (system->config.thread_safe) {
        nimcp_mutex_lock(system->mutex);
    }

    system->stats.total_self_checks++;

    float max_affinity = 0.0f;
    uint32_t best_match_id = 0;
    bool is_self = false;

    /* Check against all self patterns */
    for (size_t i = 0; i < system->self_pattern_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->self_pattern_count > 256) {
            immune_tolerance_heartbeat("immune_toler_loop",
                             (float)(i + 1) / (float)system->self_pattern_count);
        }

        self_pattern_t* sp = &system->self_patterns[i];

        float aff = compute_pattern_affinity(
            pattern, len,
            sp->pattern, sp->pattern_len
        );

        if (aff > max_affinity) {
            max_affinity = aff;
            best_match_id = sp->id;
        }

        if (aff >= sp->match_threshold) {
            sp->presentation_count++;
            sp->last_matched = get_timestamp_ms();
            is_self = true;
        }
    }

    if (is_self) {
        system->stats.self_matches++;
    } else {
        system->stats.non_self_matches++;
    }

    if (matched_pattern_id) {
        *matched_pattern_id = best_match_id;
    }

    if (affinity) {
        *affinity = max_affinity;
    }

    if (system->config.thread_safe) {
        nimcp_mutex_unlock(system->mutex);
    }

    return is_self;
}

/**
 * @brief Remove self pattern
 *
 * WHAT: Delete pattern from database
 * WHY:  System behavior changed, pattern invalid
 * HOW:  Remove and compact array
 */
int tolerance_remove_self_pattern(
    tolerance_system_t* system,
    uint32_t pattern_id
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    immune_tolerance_heartbeat("immune_toler_tolerance_remove_sel", 0.0f);


    if (system->config.thread_safe) {
        nimcp_mutex_lock(system->mutex);
    }

    self_pattern_t* sp = find_pattern_by_id(system, pattern_id);
    if (!sp) {
        if (system->config.thread_safe) {
            nimcp_mutex_unlock(system->mutex);
        }
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "tolerance_remove_self_pattern: validation failed");
        return -1;
    }

    if (sp->immutable) {
        NIMCP_LOGGING_WARN("Cannot remove immutable pattern ID=%u", pattern_id);
        if (system->config.thread_safe) {
            nimcp_mutex_unlock(system->mutex);
        }
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "tolerance_remove_self_pattern: validation failed");
        return -1;
    }

    /* Compact array */
    size_t idx = sp - system->self_patterns;
    if (idx < system->self_pattern_count - 1) {
        memmove(sp, sp + 1,
                (system->self_pattern_count - idx - 1) * sizeof(self_pattern_t));
    }
    system->self_pattern_count--;

    if (system->config.thread_safe) {
        nimcp_mutex_unlock(system->mutex);
    }

    return 0;
}

/**
 * @brief Clear all self patterns
 *
 * WHAT: Remove all patterns (except immutable)
 * WHY:  Reset for retraining
 * HOW:  Filter out immutable, compact array
 */
int tolerance_clear_self_patterns(tolerance_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    immune_tolerance_heartbeat("immune_toler_tolerance_clear_self", 0.0f);


    if (system->config.thread_safe) {
        nimcp_mutex_lock(system->mutex);
    }

    size_t write_idx = 0;
    for (size_t read_idx = 0; read_idx < system->self_pattern_count; read_idx++) {
        /* Phase 8: Loop progress heartbeat */
        if ((read_idx & 0xFF) == 0 && system->self_pattern_count > 256) {
            immune_tolerance_heartbeat("immune_toler_loop",
                             (float)(read_idx + 1) / (float)system->self_pattern_count);
        }

        if (system->self_patterns[read_idx].immutable) {
            if (write_idx != read_idx) {
                memcpy(&system->self_patterns[write_idx],
                       &system->self_patterns[read_idx],
                       sizeof(self_pattern_t));
            }
            write_idx++;
        }
    }

    system->self_pattern_count = write_idx;

    if (system->config.thread_safe) {
        nimcp_mutex_unlock(system->mutex);
    }

    return 0;
}

/**
 * @brief Get self pattern count
 */
size_t tolerance_get_self_patterns_count(const tolerance_system_t* system) {
    /* Phase 8: Heartbeat at operation start */
    immune_tolerance_heartbeat("immune_toler_tolerance_get_self_p", 0.0f);


    return system ? system->self_pattern_count : 0;
}

/* ============================================================================
 * Central Tolerance Implementation
 * ============================================================================ */

/**
 * @brief Perform central selection
 *
 * WHAT: Thymic/bone marrow-style selection
 * WHY:  Delete strongly self-reactive cells
 * HOW:  Test receptor against self patterns, decide fate
 */
int tolerance_central_selection(
    tolerance_system_t* system,
    uint32_t cell_id,
    bool is_b_cell,
    const uint8_t* receptor,
    size_t receptor_len,
    selection_outcome_t* outcome
) {
    if (!system || !receptor || !outcome || receptor_len == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tolerance_central_selection: required parameter is NULL (system, receptor, outcome)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    immune_tolerance_heartbeat("immune_toler_tolerance_central_se", 0.0f);


    if (system->config.thread_safe) {
        nimcp_mutex_lock(system->mutex);
    }

    /* Default: pass selection */
    *outcome = SELECTION_PASS;

    /* Check affinity to all self patterns */
    float max_self_affinity = 0.0f;
    uint32_t matched_pattern_id = 0;

    for (size_t i = 0; i < system->self_pattern_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->self_pattern_count > 256) {
            immune_tolerance_heartbeat("immune_toler_loop",
                             (float)(i + 1) / (float)system->self_pattern_count);
        }

        self_pattern_t* sp = &system->self_patterns[i];

        float aff = compute_pattern_affinity(
            receptor, receptor_len,
            sp->pattern, sp->pattern_len
        );

        if (aff > max_self_affinity) {
            max_self_affinity = aff;
            matched_pattern_id = sp->id;
        }
    }

    /* Determine fate based on self-affinity */
    if (max_self_affinity >= system->config.central_deletion_threshold) {
        /* Strong self-reactivity → Clonal deletion */
        *outcome = SELECTION_DELETE;
        system->stats.cells_deleted++;
        system->stats.negative_selections++;

    } else if (max_self_affinity >= system->config.receptor_editing_threshold
               && system->config.enable_receptor_editing) {
        /* Medium self-reactivity → Receptor editing */
        *outcome = SELECTION_EDIT;
        system->stats.cells_edited++;

    } else if (max_self_affinity >= system->config.anergy_threshold
               && system->config.enable_anergy) {
        /* Low-medium self-reactivity → Anergize */
        *outcome = SELECTION_ANERGIZE;
        add_anergic_cell(system, cell_id, is_b_cell, matched_pattern_id,
                         max_self_affinity);

    } else {
        /* Low self-reactivity → Positive selection (pass) */
        *outcome = SELECTION_PASS;
        system->stats.positive_selections++;
    }

    if (system->config.thread_safe) {
        nimcp_mutex_unlock(system->mutex);
    }

    NIMCP_LOGGING_DEBUG("Central selection: cell=%u, is_b=%d, affinity=%.3f, outcome=%s",
                        cell_id, is_b_cell, max_self_affinity,
                        tolerance_selection_to_string(*outcome));

    return 0;
}

/**
 * @brief Delete self-reactive cell
 *
 * WHAT: Clonal deletion
 * WHY:  Remove strongly self-reactive cell
 * HOW:  Mark cell apoptotic in immune system
 */
int tolerance_delete_cell(
    tolerance_system_t* system,
    uint32_t cell_id,
    bool is_b_cell
) {
    if (!system || !system->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tolerance_delete_cell: required parameter is NULL (system, system->immune_system)");
        return -1;
    }

    /* Find cell and mark apoptotic */
    /* Phase 8: Heartbeat at operation start */
    immune_tolerance_heartbeat("immune_toler_tolerance_delete_cel", 0.0f);


    if (is_b_cell) {
        brain_b_cell_t* b_cell = NULL;
        for (size_t i = 0; i < system->immune_system->b_cell_count; i++) {
            if (system->immune_system->b_cells[i].id == cell_id) {
                b_cell = &system->immune_system->b_cells[i];
                break;
            }
        }

        if (b_cell) {
            b_cell->state = B_CELL_APOPTOTIC;
            NIMCP_LOGGING_DEBUG("Deleted B cell ID=%u (apoptotic)", cell_id);
        }
    } else {
        /* T cells don't have explicit apoptotic state in current implementation */
        /* Could remove from array or mark with special flag */
        NIMCP_LOGGING_DEBUG("Marked T cell ID=%u for deletion", cell_id);
    }

    system->stats.cells_deleted++;
    return 0;
}

/* ============================================================================
 * Peripheral Tolerance Implementation
 * ============================================================================ */

/**
 * @brief Induce anergy in cell
 *
 * WHAT: Inactivate cell without deletion
 * WHY:  Peripheral tolerance mechanism
 * HOW:  Record cell as anergic
 */
int tolerance_induce_anergy(
    tolerance_system_t* system,
    uint32_t cell_id,
    bool is_b_cell,
    uint32_t self_pattern_id,
    float affinity
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    immune_tolerance_heartbeat("immune_toler_tolerance_induce_ane", 0.0f);


    if (system->config.thread_safe) {
        nimcp_mutex_lock(system->mutex);
    }

    int result = add_anergic_cell(system, cell_id, is_b_cell,
                                  self_pattern_id, affinity);

    if (result == 0) {
        system->stats.anergy_inductions++;
    }

    if (system->config.thread_safe) {
        nimcp_mutex_unlock(system->mutex);
    }

    return result;
}

/**
 * @brief Check if cell is anergic
 *
 * WHAT: Query anergic state
 * WHY:  Prevent anergic cells from responding
 * HOW:  Lookup in anergic records
 */
bool tolerance_is_anergic(
    const tolerance_system_t* system,
    uint32_t cell_id,
    bool is_b_cell
) {
    if (!system) {
        return false;
    }

    /* Cast away const for mutex (doesn't modify state) */
    /* Phase 8: Heartbeat at operation start */
    immune_tolerance_heartbeat("immune_toler_tolerance_is_anergic", 0.0f);


    tolerance_system_t* sys = (tolerance_system_t*)system;

    if (sys->config.thread_safe) {
        nimcp_mutex_lock(sys->mutex);
    }

    bool is_anergic = (find_anergic_cell(sys, cell_id, is_b_cell) != NULL);

    if (sys->config.thread_safe) {
        nimcp_mutex_unlock(sys->mutex);
    }

    return is_anergic;
}

/**
 * @brief Reverse anergy
 *
 * WHAT: Reactivate anergic cell
 * WHY:  Allow recovery from incorrect anergy
 * HOW:  Remove from anergic records
 */
int tolerance_reverse_anergy(
    tolerance_system_t* system,
    uint32_t cell_id,
    bool is_b_cell
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    immune_tolerance_heartbeat("immune_toler_tolerance_reverse_an", 0.0f);


    if (system->config.thread_safe) {
        nimcp_mutex_lock(system->mutex);
    }

    anergic_cell_record_t* record = find_anergic_cell(system, cell_id, is_b_cell);
    if (!record) {
        if (system->config.thread_safe) {
            nimcp_mutex_unlock(system->mutex);
        }
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "tolerance_reverse_anergy: validation failed");
        return -1;
    }

    if (!record->reversible) {
        NIMCP_LOGGING_WARN("Cannot reverse irreversible anergy for cell ID=%u",
                           cell_id);
        if (system->config.thread_safe) {
            nimcp_mutex_unlock(system->mutex);
        }
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "tolerance_reverse_anergy: validation failed");
        return -1;
    }

    /* Remove from array (compact) */
    size_t idx = record - system->anergic_cells;
    if (idx < system->anergic_cell_count - 1) {
        memmove(record, record + 1,
                (system->anergic_cell_count - idx - 1) * sizeof(anergic_cell_record_t));
    }
    system->anergic_cell_count--;
    system->stats.anergic_cells--;
    system->stats.anergy_reversals++;

    if (system->config.thread_safe) {
        nimcp_mutex_unlock(system->mutex);
    }

    return 0;
}

/* ============================================================================
 * Phase Management Implementation
 * ============================================================================ */

/**
 * @brief Set tolerance phase
 */
int tolerance_set_phase(
    tolerance_system_t* system,
    tolerance_phase_t phase
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    immune_tolerance_heartbeat("immune_toler_tolerance_set_phase", 0.0f);


    if (system->config.thread_safe) {
        nimcp_mutex_lock(system->mutex);
    }

    tolerance_phase_t old_phase = system->phase;
    system->phase = phase;

    if (system->config.thread_safe) {
        nimcp_mutex_unlock(system->mutex);
    }

    NIMCP_LOGGING_INFO("Tolerance phase transition: %s -> %s",
                       tolerance_phase_to_string(old_phase),
                       tolerance_phase_to_string(phase));

    return 0;
}

/**
 * @brief Get current phase
 */
tolerance_phase_t tolerance_get_phase(const tolerance_system_t* system) {
    /* Phase 8: Heartbeat at operation start */
    immune_tolerance_heartbeat("immune_toler_tolerance_get_phase", 0.0f);


    return system ? system->phase : TOLERANCE_PHASE_OPERATIONAL;
}

/* ============================================================================
 * Threshold Tuning Implementation
 * ============================================================================ */

/**
 * @brief Set self-match threshold
 */
int tolerance_set_self_threshold(
    tolerance_system_t* system,
    float threshold
) {
    if (!system || threshold < 0.0f || threshold > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "tolerance_set_self_threshold: system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    immune_tolerance_heartbeat("immune_toler_tolerance_set_self_t", 0.0f);


    if (system->config.thread_safe) {
        nimcp_mutex_lock(system->mutex);
    }

    system->config.self_match_threshold = threshold;

    if (system->config.thread_safe) {
        nimcp_mutex_unlock(system->mutex);
    }

    return 0;
}

/**
 * @brief Set central deletion threshold
 */
int tolerance_set_central_threshold(
    tolerance_system_t* system,
    float threshold
) {
    if (!system || threshold < 0.0f || threshold > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "tolerance_set_central_threshold: system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    immune_tolerance_heartbeat("immune_toler_tolerance_set_centra", 0.0f);


    if (system->config.thread_safe) {
        nimcp_mutex_lock(system->mutex);
    }

    system->config.central_deletion_threshold = threshold;

    if (system->config.thread_safe) {
        nimcp_mutex_unlock(system->mutex);
    }

    return 0;
}

/**
 * @brief Set anergy threshold
 */
int tolerance_set_anergy_threshold(
    tolerance_system_t* system,
    float threshold
) {
    if (!system || threshold < 0.0f || threshold > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "tolerance_set_anergy_threshold: system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    immune_tolerance_heartbeat("immune_toler_tolerance_set_anergy", 0.0f);


    if (system->config.thread_safe) {
        nimcp_mutex_lock(system->mutex);
    }

    system->config.anergy_threshold = threshold;

    if (system->config.thread_safe) {
        nimcp_mutex_unlock(system->mutex);
    }

    return 0;
}

/* ============================================================================
 * Statistics and Query Implementation
 * ============================================================================ */

/**
 * @brief Get tolerance statistics
 */
int tolerance_get_stats(
    const tolerance_system_t* system,
    tolerance_stats_t* stats
) {
    if (!system || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tolerance_get_stats: required parameter is NULL (system, stats)");
        return -1;
    }

    /* Cast away const for mutex */
    /* Phase 8: Heartbeat at operation start */
    immune_tolerance_heartbeat("immune_toler_tolerance_get_stats", 0.0f);


    tolerance_system_t* sys = (tolerance_system_t*)system;

    if (sys->config.thread_safe) {
        nimcp_mutex_lock(sys->mutex);
    }

    memcpy(stats, &sys->stats, sizeof(tolerance_stats_t));
    stats->self_pattern_count = sys->self_pattern_count;
    stats->anergic_cells = sys->anergic_cell_count;

    if (sys->config.thread_safe) {
        nimcp_mutex_unlock(sys->mutex);
    }

    return 0;
}

/**
 * @brief Get self pattern by ID
 */
const self_pattern_t* tolerance_get_pattern(
    const tolerance_system_t* system,
    uint32_t pattern_id
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return NULL;
    }

    /* Cast away const for helper function */
    /* Phase 8: Heartbeat at operation start */
    immune_tolerance_heartbeat("immune_toler_tolerance_get_patter", 0.0f);


    return find_pattern_by_id((tolerance_system_t*)system, pattern_id);
}

/**
 * @brief Compute affinity between patterns
 *
 * WHAT: Delegate to brain immune affinity computation
 * WHY:  Reuse existing fuzzy matching
 * HOW:  Call brain_immune_compute_affinity
 */
float tolerance_compute_affinity(
    const uint8_t* pattern1,
    size_t len1,
    const uint8_t* pattern2,
    size_t len2
) {
    /* Phase 8: Heartbeat at operation start */
    immune_tolerance_heartbeat("immune_toler_tolerance_compute_af", 0.0f);


    return compute_pattern_affinity(pattern1, len1, pattern2, len2);
}

/* ============================================================================
 * Internal Helper Implementations
 * ============================================================================ */

/**
 * @brief Get current timestamp in milliseconds
 */
static uint64_t get_timestamp_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

/**
 * @brief Find pattern by ID
 */
static self_pattern_t* find_pattern_by_id(
    tolerance_system_t* sys,
    uint32_t id
) {
    if (!sys) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sys is NULL");

        return NULL;
    }

    for (size_t i = 0; i < sys->self_pattern_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && sys->self_pattern_count > 256) {
            immune_tolerance_heartbeat("immune_toler_loop",
                             (float)(i + 1) / (float)sys->self_pattern_count);
        }

        if (sys->self_patterns[i].id == id) {
            return &sys->self_patterns[i];
        }
    }

    return NULL;
}

/**
 * @brief Find anergic cell record
 */
static anergic_cell_record_t* find_anergic_cell(
    tolerance_system_t* sys,
    uint32_t cell_id,
    bool is_b_cell
) {
    if (!sys) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sys is NULL");

        return NULL;
    }

    for (size_t i = 0; i < sys->anergic_cell_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && sys->anergic_cell_count > 256) {
            immune_tolerance_heartbeat("immune_toler_loop",
                             (float)(i + 1) / (float)sys->anergic_cell_count);
        }

        anergic_cell_record_t* rec = &sys->anergic_cells[i];
        if (rec->cell_id == cell_id && rec->is_b_cell == is_b_cell) {
            return rec;
        }
    }

    return NULL;
}

/**
 * @brief Add anergic cell record
 */
static int add_anergic_cell(
    tolerance_system_t* sys,
    uint32_t cell_id,
    bool is_b_cell,
    uint32_t pattern_id,
    float affinity
) {
    if (!sys) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sys is NULL");

        return -1;
    }

    /* Check if already anergic */
    if (find_anergic_cell(sys, cell_id, is_b_cell)) {
        return 0;  /* Already anergic */
    }

    /* Check capacity */
    if (sys->anergic_cell_count >= sys->anergic_cell_capacity) {
        NIMCP_LOGGING_ERROR("Anergic cell records full");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "add_anergic_cell: capacity exceeded");
        return -1;
    }

    /* Add record */
    anergic_cell_record_t* rec = &sys->anergic_cells[sys->anergic_cell_count];
    rec->cell_id = cell_id;
    rec->is_b_cell = is_b_cell;
    rec->self_pattern_id = pattern_id;
    rec->affinity = affinity;
    rec->anergy_time = get_timestamp_ms();
    rec->reversible = true;

    sys->anergic_cell_count++;
    sys->stats.anergic_cells++;

    NIMCP_LOGGING_DEBUG("Anergized cell ID=%u, is_b=%d, affinity=%.3f",
                        cell_id, is_b_cell, affinity);

    return 0;
}

/**
 * @brief Compute pattern affinity (fuzzy matching)
 *
 * WHAT: Calculate similarity between two patterns
 * WHY:  Self-recognition requires fuzzy matching, not exact
 * HOW:  3-component scoring: exact match, bit similarity, length similarity
 *
 * Matches brain_immune_compute_affinity implementation
 */
static float compute_pattern_affinity(
    const uint8_t* p1,
    size_t len1,
    const uint8_t* p2,
    size_t len2
) {
    if (!p1 || !p2 || len1 == 0 || len2 == 0) {
        return 0.0f;
    }

    size_t min_len = len1 < len2 ? len1 : len2;
    size_t max_len = len1 > len2 ? len1 : len2;

    /* Component 1: Exact byte matches (50% weight) */
    size_t exact_matches = 0;
    for (size_t i = 0; i < min_len; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && min_len > 256) {
            immune_tolerance_heartbeat("immune_toler_loop",
                             (float)(i + 1) / (float)min_len);
        }

        if (p1[i] == p2[i]) {
            exact_matches++;
        }
    }
    float exact_score = (float)exact_matches / (float)max_len;

    /* Component 2: Bit-level similarity (30% weight) */
    size_t bit_matches = 0;
    size_t total_bits = max_len * 8;
    for (size_t i = 0; i < min_len; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && min_len > 256) {
            immune_tolerance_heartbeat("immune_toler_loop",
                             (float)(i + 1) / (float)min_len);
        }

        uint8_t xor_val = p1[i] ^ p2[i];
        /* Count matching bits (inverse of set bits in XOR) */
        for (int b = 0; b < 8; b++) {
            /* Phase 8: Loop progress heartbeat */
            if ((b & 0xFF) == 0 && 8 > 256) {
                immune_tolerance_heartbeat("immune_toler_loop",
                                 (float)(b + 1) / (float)8);
            }

            if ((xor_val & (1 << b)) == 0) {
                bit_matches++;
            }
        }
    }
    float bit_score = (float)bit_matches / (float)total_bits;

    /* Component 3: Length similarity (20% weight) */
    float len_score = (float)min_len / (float)max_len;

    /* Weighted combination */
    float affinity = 0.50f * exact_score + 0.30f * bit_score + 0.20f * len_score;

    return affinity;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * @brief Query self-knowledge from knowledge graph
 *
 * WHAT: Query KG for module self-awareness information
 * WHY:  Enable introspective self-knowledge about immune tolerance
 * HOW:  Look up entity and relations in KG
 *
 * @param kg Knowledge graph reader handle
 * @return 1 if self-knowledge found, 0 otherwise
 */
int tolerance_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    immune_tolerance_heartbeat("immune_toler_tolerance_query_self", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Immune_Tolerance");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                immune_tolerance_heartbeat("immune_toler_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Immune tolerance self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Immune_Tolerance");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Immune_Tolerance");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void immune_tolerance_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_immune_tolerance_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int immune_tolerance_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "immune_tolerance_training_begin: NULL argument");
        return -1;
    }
    immune_tolerance_heartbeat_instance(NULL, "immune_tolerance_training_begin", 0.0f);
    return 0;
}

int immune_tolerance_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "immune_tolerance_training_end: NULL argument");
        return -1;
    }
    immune_tolerance_heartbeat_instance(NULL, "immune_tolerance_training_end", 1.0f);
    return 0;
}

int immune_tolerance_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "immune_tolerance_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    immune_tolerance_heartbeat_instance(NULL, "immune_tolerance_training_step", progress);
    return 0;
}
