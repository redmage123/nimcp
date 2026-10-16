//=============================================================================
// nimcp_pr_cerebellum_bridge.c - Prime Resonant Cerebellum Bridge Implementation
//=============================================================================
/**
 * @file nimcp_pr_cerebellum_bridge.c
 * @brief Implementation of procedural memory and cerebellar timing for PR Memory
 *
 * WHAT: Implements sequence management, timing tracking, error-based learning
 * WHY:  Enable biologically-realistic procedural memory and skill learning
 * HOW:  Sequence tracking, timing models, LTD/LTP mechanisms
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/memory/core/nimcp_pr_cerebellum_bridge.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdio.h>
#include "glial/myelin_sheath/nimcp_myelin_math.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(pr_cerebellum_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_pr_cerebellum_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_pr_cerebellum_bridge_mesh_registry = NULL;

nimcp_error_t pr_cerebellum_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_pr_cerebellum_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "pr_cerebellum_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_MEMORY);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "pr_cerebellum_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_pr_cerebellum_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_pr_cerebellum_bridge_mesh_registry = registry;
    return err;
}

void pr_cerebellum_bridge_mesh_unregister(void) {
    if (g_pr_cerebellum_bridge_mesh_registry && g_pr_cerebellum_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_pr_cerebellum_bridge_mesh_registry, g_pr_cerebellum_bridge_mesh_id);
        g_pr_cerebellum_bridge_mesh_id = 0;
        g_pr_cerebellum_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from pr_cerebellum_bridge module (instance-level) */
static inline void pr_cerebellum_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_pr_cerebellum_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_pr_cerebellum_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_pr_cerebellum_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "PR_CEREBELLUM_BRIDGE"

/* Security subsystem setters (Phase 1: Audit Gap Remediation) */
//=============================================================================
// Platform Abstraction
//=============================================================================

#ifdef _WIN32
    #include <windows.h>
    typedef CRITICAL_SECTION pr_cereb_mutex_t;
    #define PR_CEREB_MUTEX_INIT(m) InitializeCriticalSection(&(m))
    #define PR_CEREB_MUTEX_DESTROY(m) DeleteCriticalSection(&(m))
    #define PR_CEREB_MUTEX_LOCK(m) EnterCriticalSection(&(m))
    #define PR_CEREB_MUTEX_UNLOCK(m) LeaveCriticalSection(&(m))
#else
    #include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/memory/nimcp_memory.h"
    typedef nimcp_mutex_t pr_cereb_mutex_t;
    #define PR_CEREB_MUTEX_INIT(m) nimcp_mutex_init(&(m), NULL)
    #define PR_CEREB_MUTEX_DESTROY(m) nimcp_mutex_destroy(&(m))
    #define PR_CEREB_MUTEX_LOCK(m) nimcp_mutex_lock(&(m))
    #define PR_CEREB_MUTEX_UNLOCK(m) nimcp_mutex_unlock(&(m))
#endif

/* High-resolution timing */
#ifdef _WIN32
static uint64_t get_time_ms(void) {
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (uint64_t)((count.QuadPart * 1000) / freq.QuadPart);
}
#else
static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000ULL + ts.tv_nsec / 1000000ULL);
}
#endif

//=============================================================================
// Helper Functions
//=============================================================================

static float absf(float value) {
    return value < 0 ? -value : value;
}

/**
 * @brief Calculate timing error
 */
static float calculate_timing_error(float expected, float actual) {
    return actual - expected;
}

/**
 * @brief Calculate timing accuracy (0 = perfect, 1 = max error)
 */
static float timing_to_accuracy(float error_ms, float max_error_ms) {
    float abs_error = absf(error_ms);
    if (abs_error >= max_error_ms) return 0.0f;
    return 1.0f - (abs_error / max_error_ms);
}

/**
 * @brief Update running variance (Welford's algorithm)
 */
static void update_variance(float* variance, float* mean, float new_value, uint32_t count) {
    if (count <= 1) {
        *mean = new_value;
        *variance = 0.0f;
        return;
    }

    float delta = new_value - *mean;
    *mean += delta / (float)count;
    float delta2 = new_value - *mean;
    *variance = (*variance * (float)(count - 1) + delta * delta2) / (float)count;
}

/**
 * @brief Determine automatization level from metrics
 */
static pr_automatization_level_t compute_automatization_level(
    float consolidation,
    float accuracy,
    uint32_t executions,
    float threshold,
    uint32_t min_executions
) {
    if (executions < min_executions / 4) {
        return PR_AUTO_NOVICE;
    }

    float score = (consolidation + accuracy) / 2.0f;

    if (score >= threshold && executions >= min_executions) {
        return PR_AUTO_EXPERT;
    } else if (score >= threshold * 0.8f && executions >= min_executions / 2) {
        return PR_AUTO_PROFICIENT;
    } else if (score >= threshold * 0.5f && executions >= min_executions / 4) {
        return PR_AUTO_ADVANCED;
    }

    return PR_AUTO_NOVICE;
}

//=============================================================================
// Internal Data Structures
//=============================================================================

/**
 * @brief Internal bridge structure
 */
struct pr_cerebellum_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    /* Configuration */
    pr_cerebellum_config_t config;

    /* Sequences */
    pr_sequence_t* sequences;
    size_t num_sequences;
    size_t sequence_capacity;
    uint64_t next_sequence_id;
    pr_cereb_mutex_t sequence_mutex;
    bool sequence_mutex_initialized;

    /* Entanglement graph (external) */
    entangle_graph_t entangle_graph;

    /* Timing history */
    pr_timing_history_t* timing_history;
    size_t timing_history_size;
    size_t timing_history_capacity;
    size_t timing_history_write_idx;
    pr_cereb_mutex_t history_mutex;
    bool history_mutex_initialized;

    /* Callbacks */
    pr_sequence_callback_t sequence_callback;
    void* sequence_callback_data;
    pr_error_callback_t error_callback;
    void* error_callback_data;

    /* Statistics */
    pr_cerebellum_stats_t stats;
    pr_cereb_mutex_t stats_mutex;
    bool stats_mutex_initialized;

    /* State */
    bool initialized;
    uint64_t last_update_ms;

    /* Health agent (instance-level) - Phase 8 */
    nimcp_health_agent_t* health_agent;
};

BRIDGE_DEFINE_SECURITY_SETTERS_TYPE(pr_cerebellum_bridge, struct pr_cerebellum_bridge_struct)

//=============================================================================
// Internal Sequence Management
//=============================================================================

/**
 * @brief Find sequence by ID (caller must hold mutex)
 */
static pr_sequence_t* find_sequence_unlocked(pr_cerebellum_bridge_t bridge,
                                              uint64_t sequence_id) {
    for (size_t i = 0; i < bridge->num_sequences; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_sequences > 256) {
            pr_cerebellum_bridge_heartbeat("pr_cerebellu_loop",
                             (float)(i + 1) / (float)bridge->num_sequences);
        }

        if (bridge->sequences[i].sequence_id == sequence_id) {
            return &bridge->sequences[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "compute_automatization_level: validation failed");
    return NULL;
}

/**
 * @brief Initialize sequence elements array
 */
static bool init_sequence_elements(pr_sequence_t* seq, size_t initial_capacity) {
    seq->elements = (pr_sequence_element_t*)nimcp_calloc(
        initial_capacity, sizeof(pr_sequence_element_t));
    if (!seq->elements) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_sequence_elements: seq->elements is NULL");
        return false;
    }
    seq->capacity = initial_capacity;
    seq->length = 0;
    return true;
}

/**
 * @brief Free sequence elements
 */
static void free_sequence_elements(pr_sequence_t* seq) {
    if (seq->elements) {
        nimcp_free(seq->elements);
        seq->elements = NULL;
    }
    seq->length = 0;
    seq->capacity = 0;
}

/**
 * @brief Add element to sequence (assumes caller has space)
 */
static void add_element_unlocked(pr_sequence_t* seq,
                                  uint64_t memory_id,
                                  float interval_ms) {
    size_t pos = seq->length;
    pr_sequence_element_t* elem = &seq->elements[pos];

    elem->memory_id = memory_id;
    elem->timing.expected_interval_ms = interval_ms;
    elem->timing.actual_interval_ms = interval_ms;
    elem->timing.variance_ms = 0.0f;
    elem->timing.phase = 0.0f;
    elem->timing.execution_count = 0;
    elem->timing.timing_accuracy = 1.0f;
    elem->transition_prob = 1.0f;
    elem->position = (uint32_t)pos;
    elem->is_branch_point = false;

    seq->length++;
}

//=============================================================================
// Configuration Functions
//=============================================================================

NIMCP_EXPORT pr_cerebellum_config_t pr_cerebellum_config_default(void) {
    pr_cerebellum_config_t config;
    memset(&config, 0, sizeof(config));

    config.max_sequences = PR_CEREB_MAX_SEQUENCES;
    config.max_sequence_length = PR_CEREB_MAX_SEQUENCE_LENGTH;

    config.timing_precision_ms = PR_CEREB_DEFAULT_TIMING_PRECISION;
    config.max_timing_error_ms = PR_CEREB_MAX_TIMING_ERROR_MS;
    config.coincidence_window_ms = PR_CEREB_COINCIDENCE_WINDOW_MS;

    config.error_learning_rate = PR_CEREB_ERROR_LEARNING_RATE;
    config.error_decay_rate = PR_CEREB_ERROR_DECAY_RATE;
    config.ltd_factor = PR_CEREB_LTD_FACTOR;
    config.ltp_factor = PR_CEREB_LTP_FACTOR;

    config.automatization_threshold = PR_CEREB_AUTOMATIZATION_THRESHOLD;
    config.min_executions_for_auto = 50;

    config.timing_history_size = PR_CEREB_DEFAULT_HISTORY_SIZE;
    config.track_timing_history = true;

    config.enable_z_ladder_sync = true;
    config.enable_entanglement_update = false;

    return config;
}

NIMCP_EXPORT bool pr_cerebellum_config_validate(const pr_cerebellum_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_cerebellum_config_validate: config is NULL");
        return false;
    }

    if (config->max_sequences == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_cerebellum_config_validate: config->max_sequences is zero");
        return false;
    }
    if (config->max_sequence_length == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_cerebellum_config_validate: config->max_sequence_length is zero");
        return false;
    }
    if (config->timing_precision_ms < 0) {
        return false;
    }
    if (config->max_timing_error_ms <= 0) {
        return false;
    }
    if (config->error_learning_rate < 0 || config->error_learning_rate > 1) {
        return false;
    }
    if (config->ltd_factor < 0 || config->ltd_factor > 1) {
        return false;
    }
    if (config->ltp_factor < 0 || config->ltp_factor > 1) {
        return false;
    }

    return true;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

NIMCP_EXPORT pr_cerebellum_bridge_t pr_cerebellum_bridge_create(
    const pr_cerebellum_config_t* config
) {
    pr_cerebellum_config_t cfg;
    if (config) {
        if (!pr_cerebellum_config_validate(config)) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_cerebellum_bridge_create: pr_cerebellum_config_validate is NULL");
            return NULL;
        }
        cfg = *config;
    } else {
        cfg = pr_cerebellum_config_default();
    }

    /* Allocate bridge */
    pr_cerebellum_bridge_t bridge = (pr_cerebellum_bridge_t)nimcp_calloc(1, sizeof(*bridge));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    bridge->config = cfg;

    /* Allocate sequence array */
    size_t initial_seq_capacity = cfg.max_sequences < 32 ? cfg.max_sequences : 32;
    bridge->sequences = (pr_sequence_t*)nimcp_calloc(initial_seq_capacity, sizeof(pr_sequence_t));
    if (!bridge->sequences) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_cerebellum_bridge_create: bridge->sequences is NULL");
        return NULL;
    }
    bridge->sequence_capacity = initial_seq_capacity;
    bridge->num_sequences = 0;
    bridge->next_sequence_id = 1;

    /* Allocate timing history */
    if (cfg.track_timing_history && cfg.timing_history_size > 0) {
        bridge->timing_history = (pr_timing_history_t*)nimcp_calloc(
            cfg.timing_history_size, sizeof(pr_timing_history_t));
        if (!bridge->timing_history) {
            nimcp_free(bridge->sequences);
            nimcp_free(bridge);
            bridge = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_cerebellum_bridge_create: bridge->timing_history is NULL");
            return NULL;
        }
        bridge->timing_history_capacity = cfg.timing_history_size;
    }
    bridge->timing_history_size = 0;
    bridge->timing_history_write_idx = 0;

    /* Initialize mutexes */
    PR_CEREB_MUTEX_INIT(bridge->sequence_mutex);
    bridge->sequence_mutex_initialized = true;

    PR_CEREB_MUTEX_INIT(bridge->history_mutex);
    bridge->history_mutex_initialized = true;

    PR_CEREB_MUTEX_INIT(bridge->stats_mutex);
    bridge->stats_mutex_initialized = true;

    /* Initialize state */
    bridge->entangle_graph = NULL;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->last_update_ms = get_time_ms();
    bridge->initialized = true;

    return bridge;
}

NIMCP_EXPORT void pr_cerebellum_bridge_destroy(pr_cerebellum_bridge_t bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "pr_cerebellum");

    /* Free sequence elements */
    if (bridge->sequences) {
        for (size_t i = 0; i < bridge->num_sequences; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->num_sequences > 256) {
                pr_cerebellum_bridge_heartbeat("pr_cerebellu_loop",
                                 (float)(i + 1) / (float)bridge->num_sequences);
            }

            free_sequence_elements(&bridge->sequences[i]);
        }
        nimcp_free(bridge->sequences);
    }

    /* Free timing history */
    if (bridge->timing_history) {
        nimcp_free(bridge->timing_history);
    }

    /* Destroy mutexes */
    if (bridge->sequence_mutex_initialized) {
        PR_CEREB_MUTEX_DESTROY(bridge->sequence_mutex);
    }
    if (bridge->history_mutex_initialized) {
        PR_CEREB_MUTEX_DESTROY(bridge->history_mutex);
    }
    if (bridge->stats_mutex_initialized) {
        PR_CEREB_MUTEX_DESTROY(bridge->stats_mutex);
    }

    nimcp_free(bridge);
    bridge = NULL;
}

NIMCP_EXPORT pr_cerebellum_error_t pr_cerebellum_bridge_set_entanglement(
    pr_cerebellum_bridge_t bridge,
    entangle_graph_t graph
) {
    if (!bridge) return PR_CEREB_ERROR_NULL_POINTER;

    bridge->entangle_graph = graph;
    return PR_CEREB_SUCCESS;
}

NIMCP_EXPORT pr_cerebellum_error_t pr_cerebellum_bridge_reset(
    pr_cerebellum_bridge_t bridge
) {
    if (!bridge) return PR_CEREB_ERROR_NULL_POINTER;

    /* Clear all sequences */
    PR_CEREB_MUTEX_LOCK(bridge->sequence_mutex);
    for (size_t i = 0; i < bridge->num_sequences; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_sequences > 256) {
            pr_cerebellum_bridge_heartbeat("pr_cerebellu_loop",
                             (float)(i + 1) / (float)bridge->num_sequences);
        }

        free_sequence_elements(&bridge->sequences[i]);
    }
    bridge->num_sequences = 0;
    bridge->next_sequence_id = 1;
    PR_CEREB_MUTEX_UNLOCK(bridge->sequence_mutex);

    /* Clear timing history */
    if (bridge->config.track_timing_history) {
        PR_CEREB_MUTEX_LOCK(bridge->history_mutex);
        bridge->timing_history_size = 0;
        bridge->timing_history_write_idx = 0;
        PR_CEREB_MUTEX_UNLOCK(bridge->history_mutex);
    }

    /* Reset statistics */
    PR_CEREB_MUTEX_LOCK(bridge->stats_mutex);
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->stats.last_update_ms = get_time_ms();
    PR_CEREB_MUTEX_UNLOCK(bridge->stats_mutex);

    bridge->last_update_ms = get_time_ms();

    return PR_CEREB_SUCCESS;
}

//=============================================================================
// Sequence Management Functions
//=============================================================================

NIMCP_EXPORT pr_cerebellum_error_t pr_cerebellum_bridge_create_sequence(
    pr_cerebellum_bridge_t bridge,
    const char* name,
    pr_procedural_type_t type,
    uint64_t* sequence_id
) {
    if (!bridge || !sequence_id) return PR_CEREB_ERROR_NULL_POINTER;

    uint64_t now = get_time_ms();

    PR_CEREB_MUTEX_LOCK(bridge->sequence_mutex);

    /* Check capacity */
    if (bridge->num_sequences >= bridge->config.max_sequences) {
        PR_CEREB_MUTEX_UNLOCK(bridge->sequence_mutex);
        return PR_CEREB_ERROR_SEQUENCE_FULL;
    }

    /* Grow array if needed */
    if (bridge->num_sequences >= bridge->sequence_capacity) {
        size_t new_cap = bridge->sequence_capacity * 2;
        if (new_cap > bridge->config.max_sequences) {
            new_cap = bridge->config.max_sequences;
        }
        pr_sequence_t* new_seqs = (pr_sequence_t*)nimcp_realloc(
            bridge->sequences, new_cap * sizeof(pr_sequence_t));
        if (!new_seqs) {
            PR_CEREB_MUTEX_UNLOCK(bridge->sequence_mutex);
            return PR_CEREB_ERROR_NO_MEMORY;
        }
        bridge->sequences = new_seqs;
        bridge->sequence_capacity = new_cap;
    }

    /* Initialize new sequence */
    pr_sequence_t* seq = &bridge->sequences[bridge->num_sequences];
    memset(seq, 0, sizeof(*seq));

    seq->sequence_id = bridge->next_sequence_id++;
    if (name) {
        strncpy(seq->name, name, sizeof(seq->name) - 1);
        seq->name[sizeof(seq->name) - 1] = '\0';
    } else {
        snprintf(seq->name, sizeof(seq->name), "seq_%lu",
                 (unsigned long)seq->sequence_id);
    }
    seq->type = type;

    /* Allocate elements array */
    if (!init_sequence_elements(seq, 8)) {
        PR_CEREB_MUTEX_UNLOCK(bridge->sequence_mutex);
        return PR_CEREB_ERROR_NO_MEMORY;
    }

    seq->state = PR_SEQ_IDLE;
    seq->current_position = 0;
    seq->consolidation = 0.0f;
    seq->automatization = 0.0f;
    seq->total_executions = 0;
    seq->successful_executions = 0;
    seq->avg_error = 0.0f;
    seq->created_time_ms = now;
    seq->last_executed_ms = 0;

    *sequence_id = seq->sequence_id;
    bridge->num_sequences++;

    PR_CEREB_MUTEX_UNLOCK(bridge->sequence_mutex);

    /* Update statistics */
    PR_CEREB_MUTEX_LOCK(bridge->stats_mutex);
    bridge->stats.sequences_created++;
    PR_CEREB_MUTEX_UNLOCK(bridge->stats_mutex);

    return PR_CEREB_SUCCESS;
}

NIMCP_EXPORT pr_cerebellum_error_t pr_cerebellum_bridge_add_element(
    pr_cerebellum_bridge_t bridge,
    uint64_t sequence_id,
    uint64_t memory_id,
    float interval_ms
) {
    if (!bridge) return PR_CEREB_ERROR_NULL_POINTER;

    PR_CEREB_MUTEX_LOCK(bridge->sequence_mutex);

    pr_sequence_t* seq = find_sequence_unlocked(bridge, sequence_id);
    if (!seq) {
        PR_CEREB_MUTEX_UNLOCK(bridge->sequence_mutex);
        return PR_CEREB_ERROR_SEQUENCE_NOT_FOUND;
    }

    /* Check length */
    if (seq->length >= bridge->config.max_sequence_length) {
        PR_CEREB_MUTEX_UNLOCK(bridge->sequence_mutex);
        return PR_CEREB_ERROR_SEQUENCE_FULL;
    }

    /* Grow elements array if needed */
    if (seq->length >= seq->capacity) {
        size_t new_cap = seq->capacity * 2;
        if (new_cap > bridge->config.max_sequence_length) {
            new_cap = bridge->config.max_sequence_length;
        }
        pr_sequence_element_t* new_elems = (pr_sequence_element_t*)nimcp_realloc(
            seq->elements, new_cap * sizeof(pr_sequence_element_t));
        if (!new_elems) {
            PR_CEREB_MUTEX_UNLOCK(bridge->sequence_mutex);
            return PR_CEREB_ERROR_NO_MEMORY;
        }
        seq->elements = new_elems;
        seq->capacity = new_cap;
    }

    add_element_unlocked(seq, memory_id, interval_ms);

    PR_CEREB_MUTEX_UNLOCK(bridge->sequence_mutex);

    return PR_CEREB_SUCCESS;
}

NIMCP_EXPORT pr_cerebellum_error_t pr_cerebellum_bridge_get_sequence(
    const pr_cerebellum_bridge_t bridge,
    uint64_t sequence_id,
    pr_sequence_t* sequence
) {
    if (!bridge || !sequence) return PR_CEREB_ERROR_NULL_POINTER;

    PR_CEREB_MUTEX_LOCK(((pr_cerebellum_bridge_t)bridge)->sequence_mutex);

    pr_sequence_t* seq = find_sequence_unlocked((pr_cerebellum_bridge_t)bridge, sequence_id);
    if (!seq) {
        PR_CEREB_MUTEX_UNLOCK(((pr_cerebellum_bridge_t)bridge)->sequence_mutex);
        return PR_CEREB_ERROR_SEQUENCE_NOT_FOUND;
    }

    /* Copy sequence (shallow - elements pointer is copied) */
    *sequence = *seq;

    PR_CEREB_MUTEX_UNLOCK(((pr_cerebellum_bridge_t)bridge)->sequence_mutex);

    return PR_CEREB_SUCCESS;
}

NIMCP_EXPORT pr_cerebellum_error_t pr_cerebellum_bridge_delete_sequence(
    pr_cerebellum_bridge_t bridge,
    uint64_t sequence_id
) {
    if (!bridge) return PR_CEREB_ERROR_NULL_POINTER;

    PR_CEREB_MUTEX_LOCK(bridge->sequence_mutex);

    /* Find sequence index */
    size_t found_idx = bridge->num_sequences;
    for (size_t i = 0; i < bridge->num_sequences; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_sequences > 256) {
            pr_cerebellum_bridge_heartbeat("pr_cerebellu_loop",
                             (float)(i + 1) / (float)bridge->num_sequences);
        }

        if (bridge->sequences[i].sequence_id == sequence_id) {
            found_idx = i;
            break;
        }
    }

    if (found_idx >= bridge->num_sequences) {
        PR_CEREB_MUTEX_UNLOCK(bridge->sequence_mutex);
        return PR_CEREB_ERROR_SEQUENCE_NOT_FOUND;
    }

    /* Free elements */
    free_sequence_elements(&bridge->sequences[found_idx]);

    /* Shift remaining sequences */
    for (size_t i = found_idx; i < bridge->num_sequences - 1; i++) {
        bridge->sequences[i] = bridge->sequences[i + 1];
    }
    bridge->num_sequences--;

    PR_CEREB_MUTEX_UNLOCK(bridge->sequence_mutex);

    return PR_CEREB_SUCCESS;
}

NIMCP_EXPORT size_t pr_cerebellum_bridge_get_sequence_count(
    const pr_cerebellum_bridge_t bridge
) {
    if (!bridge) return 0;

    PR_CEREB_MUTEX_LOCK(((pr_cerebellum_bridge_t)bridge)->sequence_mutex);
    size_t count = bridge->num_sequences;
    PR_CEREB_MUTEX_UNLOCK(((pr_cerebellum_bridge_t)bridge)->sequence_mutex);

    return count;
}

//=============================================================================
// Procedural Memory Sync Functions
//=============================================================================

NIMCP_EXPORT pr_cerebellum_error_t pr_cerebellum_bridge_sync_procedural(
    pr_cerebellum_bridge_t bridge,
    pr_memory_node_t* node
) {
    if (!bridge || !node) return PR_CEREB_ERROR_NULL_POINTER;

    /* Find sequences containing this memory */
    float total_consolidation = 0.0f;
    float total_accuracy = 0.0f;
    int sequence_count = 0;

    PR_CEREB_MUTEX_LOCK(bridge->sequence_mutex);

    for (size_t i = 0; i < bridge->num_sequences; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_sequences > 256) {
            pr_cerebellum_bridge_heartbeat("pr_cerebellu_loop",
                             (float)(i + 1) / (float)bridge->num_sequences);
        }

        pr_sequence_t* seq = &bridge->sequences[i];
        for (size_t j = 0; j < seq->length; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && seq->length > 256) {
                pr_cerebellum_bridge_heartbeat("pr_cerebellu_loop",
                                 (float)(j + 1) / (float)seq->length);
            }

            if (seq->elements[j].memory_id == node->node_id) {
                total_consolidation += seq->consolidation;
                total_accuracy += seq->elements[j].timing.timing_accuracy;
                sequence_count++;
            }
        }
    }

    PR_CEREB_MUTEX_UNLOCK(bridge->sequence_mutex);

    if (sequence_count == 0) {
        return PR_CEREB_SUCCESS;  /* Not part of any sequence */
    }

    /* Calculate averages */
    float avg_consolidation = total_consolidation / (float)sequence_count;
    float avg_accuracy = total_accuracy / (float)sequence_count;

    /* Boost accessibility based on timing accuracy */
    float z_boost = (avg_accuracy - 0.5f) * 0.2f;  /* +/- 0.1 */
    node->state.z = nimcp_myelin_clamp(node->state.z + z_boost, 0.0f, 1.0f);

    /* Influence consolidation based on sequence consolidation */
    float w_blend = avg_consolidation * 0.1f;
    node->state.w = nimcp_myelin_clamp(node->state.w + w_blend, 0.0f, 1.0f);

    /* Update statistics */
    PR_CEREB_MUTEX_LOCK(bridge->stats_mutex);
    bridge->stats.memories_modified++;
    bridge->stats.avg_accessibility_change =
        (bridge->stats.avg_accessibility_change * (float)(bridge->stats.memories_modified - 1)
         + z_boost) / (float)bridge->stats.memories_modified;
    PR_CEREB_MUTEX_UNLOCK(bridge->stats_mutex);

    return PR_CEREB_SUCCESS;
}

NIMCP_EXPORT int pr_cerebellum_bridge_sync_sequence(
    pr_cerebellum_bridge_t bridge,
    uint64_t sequence_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_cerebellum_bridge_sync_sequence: bridge is NULL");
        return -1;
    }

    int synced = 0;

    PR_CEREB_MUTEX_LOCK(bridge->sequence_mutex);

    pr_sequence_t* seq = find_sequence_unlocked(bridge, sequence_id);
    if (!seq) {
        PR_CEREB_MUTEX_UNLOCK(bridge->sequence_mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_cerebellum_bridge_sync_sequence: seq is NULL");
        return -1;
    }

    /* For each element, we would sync with the actual memory node */
    /* This requires external memory access - for now just count */
    synced = (int)seq->length;

    PR_CEREB_MUTEX_UNLOCK(bridge->sequence_mutex);

    return synced;
}

NIMCP_EXPORT pr_automatization_level_t pr_cerebellum_bridge_get_automatization(
    const pr_cerebellum_bridge_t bridge,
    uint64_t sequence_id
) {
    if (!bridge) return PR_AUTO_NOVICE;

    PR_CEREB_MUTEX_LOCK(((pr_cerebellum_bridge_t)bridge)->sequence_mutex);

    pr_sequence_t* seq = find_sequence_unlocked((pr_cerebellum_bridge_t)bridge, sequence_id);
    if (!seq) {
        PR_CEREB_MUTEX_UNLOCK(((pr_cerebellum_bridge_t)bridge)->sequence_mutex);
        return PR_AUTO_NOVICE;
    }

    /* Calculate average timing accuracy */
    float total_accuracy = 0.0f;
    for (size_t i = 0; i < seq->length; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && seq->length > 256) {
            pr_cerebellum_bridge_heartbeat("pr_cerebellu_loop",
                             (float)(i + 1) / (float)seq->length);
        }

        total_accuracy += seq->elements[i].timing.timing_accuracy;
    }
    float avg_accuracy = seq->length > 0 ? total_accuracy / (float)seq->length : 0.0f;

    pr_automatization_level_t level = compute_automatization_level(
        seq->consolidation, avg_accuracy, seq->total_executions,
        bridge->config.automatization_threshold,
        bridge->config.min_executions_for_auto);

    PR_CEREB_MUTEX_UNLOCK(((pr_cerebellum_bridge_t)bridge)->sequence_mutex);

    return level;
}

//=============================================================================
// Timing Memory Functions
//=============================================================================

NIMCP_EXPORT pr_cerebellum_error_t pr_cerebellum_bridge_timing_memory(
    pr_cerebellum_bridge_t bridge,
    uint64_t memory_id,
    float interval_ms
) {
    if (!bridge) return PR_CEREB_ERROR_NULL_POINTER;

    uint64_t now = get_time_ms();

    /* Record in history */
    if (bridge->config.track_timing_history && bridge->timing_history) {
        PR_CEREB_MUTEX_LOCK(bridge->history_mutex);

        size_t idx = bridge->timing_history_write_idx;
        bridge->timing_history[idx].memory_id = memory_id;
        bridge->timing_history[idx].sequence_id = 0;
        bridge->timing_history[idx].expected_interval_ms = 0;  /* Unknown */
        bridge->timing_history[idx].actual_interval_ms = interval_ms;
        bridge->timing_history[idx].error_ms = 0;
        bridge->timing_history[idx].timestamp_ms = now;

        bridge->timing_history_write_idx =
            (idx + 1) % bridge->timing_history_capacity;
        if (bridge->timing_history_size < bridge->timing_history_capacity) {
            bridge->timing_history_size++;
        }

        PR_CEREB_MUTEX_UNLOCK(bridge->history_mutex);
    }

    /* Update statistics */
    PR_CEREB_MUTEX_LOCK(bridge->stats_mutex);
    bridge->stats.timing_events++;
    bridge->stats.last_update_ms = now;
    PR_CEREB_MUTEX_UNLOCK(bridge->stats_mutex);

    return PR_CEREB_SUCCESS;
}

NIMCP_EXPORT float pr_cerebellum_bridge_timing_in_sequence(
    pr_cerebellum_bridge_t bridge,
    uint64_t sequence_id,
    uint64_t memory_id,
    float interval_ms
) {
    if (!bridge) return NAN;

    uint64_t now = get_time_ms();
    float error_ms = 0.0f;
    float expected_ms = 0.0f;

    PR_CEREB_MUTEX_LOCK(bridge->sequence_mutex);

    pr_sequence_t* seq = find_sequence_unlocked(bridge, sequence_id);
    if (!seq) {
        PR_CEREB_MUTEX_UNLOCK(bridge->sequence_mutex);
        return NAN;
    }

    /* Find element */
    pr_sequence_element_t* elem = NULL;
    for (size_t i = 0; i < seq->length; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && seq->length > 256) {
            pr_cerebellum_bridge_heartbeat("pr_cerebellu_loop",
                             (float)(i + 1) / (float)seq->length);
        }

        if (seq->elements[i].memory_id == memory_id) {
            elem = &seq->elements[i];
            break;
        }
    }

    if (!elem) {
        PR_CEREB_MUTEX_UNLOCK(bridge->sequence_mutex);
        return NAN;
    }

    expected_ms = elem->timing.expected_interval_ms;
    error_ms = calculate_timing_error(expected_ms, interval_ms);

    /* Update timing info */
    elem->timing.actual_interval_ms = interval_ms;
    elem->timing.execution_count++;

    /* Update variance */
    update_variance(&elem->timing.variance_ms, &elem->timing.expected_interval_ms,
                    interval_ms, elem->timing.execution_count);

    /* Update accuracy */
    float accuracy = timing_to_accuracy(error_ms, bridge->config.max_timing_error_ms);
    uint32_t n = elem->timing.execution_count;
    elem->timing.timing_accuracy =
        (elem->timing.timing_accuracy * (float)(n - 1) + accuracy) / (float)n;

    PR_CEREB_MUTEX_UNLOCK(bridge->sequence_mutex);

    /* Record in history */
    if (bridge->config.track_timing_history && bridge->timing_history) {
        PR_CEREB_MUTEX_LOCK(bridge->history_mutex);

        size_t idx = bridge->timing_history_write_idx;
        bridge->timing_history[idx].memory_id = memory_id;
        bridge->timing_history[idx].sequence_id = sequence_id;
        bridge->timing_history[idx].expected_interval_ms = expected_ms;
        bridge->timing_history[idx].actual_interval_ms = interval_ms;
        bridge->timing_history[idx].error_ms = error_ms;
        bridge->timing_history[idx].timestamp_ms = now;

        bridge->timing_history_write_idx =
            (idx + 1) % bridge->timing_history_capacity;
        if (bridge->timing_history_size < bridge->timing_history_capacity) {
            bridge->timing_history_size++;
        }

        PR_CEREB_MUTEX_UNLOCK(bridge->history_mutex);
    }

    /* Update statistics */
    PR_CEREB_MUTEX_LOCK(bridge->stats_mutex);
    bridge->stats.timing_events++;
    uint64_t total = bridge->stats.timing_events;
    bridge->stats.avg_timing_error_ms =
        (bridge->stats.avg_timing_error_ms * (float)(total - 1) + absf(error_ms)) / (float)total;
    if (absf(error_ms) > bridge->stats.max_timing_error_ms) {
        bridge->stats.max_timing_error_ms = absf(error_ms);
    }
    bridge->stats.timing_accuracy =
        (bridge->stats.timing_accuracy * (float)(total - 1) + accuracy) / (float)total;
    bridge->stats.last_update_ms = now;
    PR_CEREB_MUTEX_UNLOCK(bridge->stats_mutex);

    return error_ms;
}

NIMCP_EXPORT float pr_cerebellum_bridge_get_expected_timing(
    const pr_cerebellum_bridge_t bridge,
    uint64_t sequence_id,
    size_t position
) {
    if (!bridge) return -1.0f;

    PR_CEREB_MUTEX_LOCK(((pr_cerebellum_bridge_t)bridge)->sequence_mutex);

    pr_sequence_t* seq = find_sequence_unlocked((pr_cerebellum_bridge_t)bridge, sequence_id);
    if (!seq || position >= seq->length) {
        PR_CEREB_MUTEX_UNLOCK(((pr_cerebellum_bridge_t)bridge)->sequence_mutex);
        return -1.0f;
    }

    float expected = seq->elements[position].timing.expected_interval_ms;

    PR_CEREB_MUTEX_UNLOCK(((pr_cerebellum_bridge_t)bridge)->sequence_mutex);

    return expected;
}

NIMCP_EXPORT float pr_cerebellum_bridge_get_timing_variance(
    const pr_cerebellum_bridge_t bridge,
    uint64_t sequence_id,
    size_t position
) {
    if (!bridge) return -1.0f;

    PR_CEREB_MUTEX_LOCK(((pr_cerebellum_bridge_t)bridge)->sequence_mutex);

    pr_sequence_t* seq = find_sequence_unlocked((pr_cerebellum_bridge_t)bridge, sequence_id);
    if (!seq || position >= seq->length) {
        PR_CEREB_MUTEX_UNLOCK(((pr_cerebellum_bridge_t)bridge)->sequence_mutex);
        return -1.0f;
    }

    float variance = seq->elements[position].timing.variance_ms;

    PR_CEREB_MUTEX_UNLOCK(((pr_cerebellum_bridge_t)bridge)->sequence_mutex);

    return variance;
}

//=============================================================================
// Error Signal Functions
//=============================================================================

NIMCP_EXPORT pr_cerebellum_error_t pr_cerebellum_bridge_error_signal(
    pr_cerebellum_bridge_t bridge,
    pr_error_type_t type,
    float expected,
    float actual,
    uint64_t memory_id
) {
    return pr_cerebellum_bridge_error_in_sequence(bridge, type, expected, actual, 0, 0);
}

NIMCP_EXPORT pr_cerebellum_error_t pr_cerebellum_bridge_error_in_sequence(
    pr_cerebellum_bridge_t bridge,
    pr_error_type_t type,
    float expected,
    float actual,
    uint64_t sequence_id,
    size_t position
) {
    if (!bridge) return PR_CEREB_ERROR_NULL_POINTER;
    if (type >= PR_ERROR_TYPE_COUNT) return PR_CEREB_ERROR_INVALID_CONFIG;

    uint64_t now = get_time_ms();
    float error_magnitude = (actual - expected) / (absf(expected) + PR_CEREB_EPSILON);
    error_magnitude = nimcp_myelin_clamp(error_magnitude, -1.0f, 1.0f);

    /* Create error signal */
    pr_error_signal_t signal;
    signal.type = type;
    signal.magnitude = error_magnitude;
    signal.expected_value = expected;
    signal.actual_value = actual;
    signal.memory_id = 0;
    signal.sequence_id = sequence_id;
    signal.sequence_position = (uint32_t)position;
    signal.timestamp_ms = now;

    /* If in sequence context, update sequence state */
    if (sequence_id != 0) {
        PR_CEREB_MUTEX_LOCK(bridge->sequence_mutex);

        pr_sequence_t* seq = find_sequence_unlocked(bridge, sequence_id);
        if (seq && position < seq->length) {
            signal.memory_id = seq->elements[position].memory_id;

            /* Update sequence error tracking */
            uint32_t n = seq->total_executions + 1;
            seq->avg_error = (seq->avg_error * (float)(n - 1) + absf(error_magnitude)) / (float)n;

            /* Apply error-based learning */
            float error_effect = absf(error_magnitude) * bridge->config.error_learning_rate;

            /* If error is large, reduce consolidation (need more practice) */
            if (absf(error_magnitude) > 0.3f) {
                seq->consolidation = nimcp_myelin_clamp(seq->consolidation - error_effect * 0.5f, 0.0f, 1.0f);
            }
        }

        PR_CEREB_MUTEX_UNLOCK(bridge->sequence_mutex);
    }

    /* Update statistics */
    PR_CEREB_MUTEX_LOCK(bridge->stats_mutex);
    bridge->stats.error_signals++;
    bridge->stats.errors_by_type[type]++;
    uint64_t n = bridge->stats.error_signals;
    bridge->stats.avg_error_magnitude =
        (bridge->stats.avg_error_magnitude * (float)(n - 1) + absf(error_magnitude)) / (float)n;
    bridge->stats.last_update_ms = now;
    PR_CEREB_MUTEX_UNLOCK(bridge->stats_mutex);

    /* Invoke callback */
    if (bridge->error_callback) {
        bridge->error_callback(&signal, bridge->error_callback_data);
    }

    return PR_CEREB_SUCCESS;
}

NIMCP_EXPORT float pr_cerebellum_bridge_apply_ltd(
    pr_cerebellum_bridge_t bridge,
    uint64_t memory_id,
    float ltd_amount
) {
    if (!bridge) return -1.0f;

    ltd_amount = nimcp_myelin_clamp(ltd_amount, 0.0f, 1.0f);
    float new_consolidation = 0.0f;
    bool found = false;

    PR_CEREB_MUTEX_LOCK(bridge->sequence_mutex);

    /* Find sequences containing this memory and apply LTD */
    for (size_t i = 0; i < bridge->num_sequences; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_sequences > 256) {
            pr_cerebellum_bridge_heartbeat("pr_cerebellu_loop",
                             (float)(i + 1) / (float)bridge->num_sequences);
        }

        pr_sequence_t* seq = &bridge->sequences[i];
        for (size_t j = 0; j < seq->length; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && seq->length > 256) {
                pr_cerebellum_bridge_heartbeat("pr_cerebellu_loop",
                                 (float)(j + 1) / (float)seq->length);
            }

            if (seq->elements[j].memory_id == memory_id) {
                /* Apply LTD to sequence consolidation */
                float ltd_effect = ltd_amount * bridge->config.ltd_factor;
                seq->consolidation = nimcp_myelin_clamp(seq->consolidation - ltd_effect, 0.0f, 1.0f);
                new_consolidation = seq->consolidation;
                found = true;
            }
        }
    }

    PR_CEREB_MUTEX_UNLOCK(bridge->sequence_mutex);

    if (!found) return -1.0f;

    /* Update statistics */
    PR_CEREB_MUTEX_LOCK(bridge->stats_mutex);
    bridge->stats.ltd_events++;
    bridge->stats.corrections_applied++;
    PR_CEREB_MUTEX_UNLOCK(bridge->stats_mutex);

    return new_consolidation;
}

NIMCP_EXPORT float pr_cerebellum_bridge_apply_ltp(
    pr_cerebellum_bridge_t bridge,
    uint64_t memory_id,
    float ltp_amount
) {
    if (!bridge) return -1.0f;

    ltp_amount = nimcp_myelin_clamp(ltp_amount, 0.0f, 1.0f);
    float new_consolidation = 0.0f;
    bool found = false;

    PR_CEREB_MUTEX_LOCK(bridge->sequence_mutex);

    /* Find sequences containing this memory and apply LTP */
    for (size_t i = 0; i < bridge->num_sequences; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_sequences > 256) {
            pr_cerebellum_bridge_heartbeat("pr_cerebellu_loop",
                             (float)(i + 1) / (float)bridge->num_sequences);
        }

        pr_sequence_t* seq = &bridge->sequences[i];
        for (size_t j = 0; j < seq->length; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && seq->length > 256) {
                pr_cerebellum_bridge_heartbeat("pr_cerebellu_loop",
                                 (float)(j + 1) / (float)seq->length);
            }

            if (seq->elements[j].memory_id == memory_id) {
                /* Apply LTP to sequence consolidation */
                float ltp_effect = ltp_amount * bridge->config.ltp_factor;
                seq->consolidation = nimcp_myelin_clamp(seq->consolidation + ltp_effect, 0.0f, 1.0f);
                new_consolidation = seq->consolidation;
                found = true;
            }
        }
    }

    PR_CEREB_MUTEX_UNLOCK(bridge->sequence_mutex);

    if (!found) return -1.0f;

    /* Update statistics */
    PR_CEREB_MUTEX_LOCK(bridge->stats_mutex);
    bridge->stats.ltp_events++;
    PR_CEREB_MUTEX_UNLOCK(bridge->stats_mutex);

    return new_consolidation;
}

//=============================================================================
// Sequence Execution Functions
//=============================================================================

NIMCP_EXPORT pr_cerebellum_error_t pr_cerebellum_bridge_start_sequence(
    pr_cerebellum_bridge_t bridge,
    uint64_t sequence_id
) {
    if (!bridge) return PR_CEREB_ERROR_NULL_POINTER;

    uint64_t now = get_time_ms();

    PR_CEREB_MUTEX_LOCK(bridge->sequence_mutex);

    pr_sequence_t* seq = find_sequence_unlocked(bridge, sequence_id);
    if (!seq) {
        PR_CEREB_MUTEX_UNLOCK(bridge->sequence_mutex);
        return PR_CEREB_ERROR_SEQUENCE_NOT_FOUND;
    }

    seq->state = PR_SEQ_INITIATING;
    seq->current_position = 0;
    seq->execution_start_ms = now;
    seq->last_element_time_ms = now;

    pr_sequence_t seq_copy = *seq;  /* For callback */

    PR_CEREB_MUTEX_UNLOCK(bridge->sequence_mutex);

    /* Invoke callback */
    if (bridge->sequence_callback) {
        bridge->sequence_callback(&seq_copy, PR_SEQ_INITIATING,
                                   bridge->sequence_callback_data);
    }

    return PR_CEREB_SUCCESS;
}

NIMCP_EXPORT pr_cerebellum_error_t pr_cerebellum_bridge_execute_next(
    pr_cerebellum_bridge_t bridge,
    uint64_t sequence_id,
    float interval_ms,
    uint64_t* element_id
) {
    if (!bridge || !element_id) return PR_CEREB_ERROR_NULL_POINTER;

    uint64_t now = get_time_ms();

    PR_CEREB_MUTEX_LOCK(bridge->sequence_mutex);

    pr_sequence_t* seq = find_sequence_unlocked(bridge, sequence_id);
    if (!seq) {
        PR_CEREB_MUTEX_UNLOCK(bridge->sequence_mutex);
        return PR_CEREB_ERROR_SEQUENCE_NOT_FOUND;
    }

    if (seq->current_position >= seq->length) {
        seq->state = PR_SEQ_COMPLETING;
        PR_CEREB_MUTEX_UNLOCK(bridge->sequence_mutex);
        return PR_CEREB_ERROR_SEQUENCE_NOT_FOUND;  /* Sequence done */
    }

    seq->state = PR_SEQ_EXECUTING;

    pr_sequence_element_t* elem = &seq->elements[seq->current_position];
    *element_id = elem->memory_id;

    /* Update timing */
    float error_ms = calculate_timing_error(elem->timing.expected_interval_ms, interval_ms);
    elem->timing.actual_interval_ms = interval_ms;
    elem->timing.execution_count++;

    /* Update accuracy */
    float accuracy = timing_to_accuracy(error_ms, bridge->config.max_timing_error_ms);
    uint32_t n = elem->timing.execution_count;
    elem->timing.timing_accuracy =
        (elem->timing.timing_accuracy * (float)(n - 1) + accuracy) / (float)n;

    seq->last_element_time_ms = now;
    seq->current_position++;

    PR_CEREB_MUTEX_UNLOCK(bridge->sequence_mutex);

    /* Update statistics */
    PR_CEREB_MUTEX_LOCK(bridge->stats_mutex);
    bridge->stats.timing_events++;
    PR_CEREB_MUTEX_UNLOCK(bridge->stats_mutex);

    return PR_CEREB_SUCCESS;
}

NIMCP_EXPORT pr_cerebellum_error_t pr_cerebellum_bridge_complete_sequence(
    pr_cerebellum_bridge_t bridge,
    uint64_t sequence_id,
    bool success
) {
    if (!bridge) return PR_CEREB_ERROR_NULL_POINTER;

    uint64_t now = get_time_ms();

    PR_CEREB_MUTEX_LOCK(bridge->sequence_mutex);

    pr_sequence_t* seq = find_sequence_unlocked(bridge, sequence_id);
    if (!seq) {
        PR_CEREB_MUTEX_UNLOCK(bridge->sequence_mutex);
        return PR_CEREB_ERROR_SEQUENCE_NOT_FOUND;
    }

    seq->state = PR_SEQ_IDLE;
    seq->total_executions++;
    if (success) {
        seq->successful_executions++;

        /* Boost consolidation for successful execution */
        float boost = bridge->config.ltp_factor;
        seq->consolidation = nimcp_myelin_clamp(seq->consolidation + boost, 0.0f, 1.0f);
    }
    seq->last_executed_ms = now;

    /* Update automatization */
    float avg_accuracy = 0.0f;
    for (size_t i = 0; i < seq->length; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && seq->length > 256) {
            pr_cerebellum_bridge_heartbeat("pr_cerebellu_loop",
                             (float)(i + 1) / (float)seq->length);
        }

        avg_accuracy += seq->elements[i].timing.timing_accuracy;
    }
    avg_accuracy = seq->length > 0 ? avg_accuracy / (float)seq->length : 0.0f;
    seq->automatization = (seq->consolidation + avg_accuracy) / 2.0f;

    pr_sequence_t seq_copy = *seq;

    PR_CEREB_MUTEX_UNLOCK(bridge->sequence_mutex);

    /* Update statistics */
    PR_CEREB_MUTEX_LOCK(bridge->stats_mutex);
    bridge->stats.sequences_executed++;
    if (success) {
        bridge->stats.sequences_completed++;
        bridge->stats.ltp_events++;
    }

    /* Check if reached automatization */
    if (seq_copy.automatization >= bridge->config.automatization_threshold &&
        seq_copy.total_executions >= bridge->config.min_executions_for_auto) {
        bridge->stats.automatizations++;
    }
    PR_CEREB_MUTEX_UNLOCK(bridge->stats_mutex);

    /* Invoke callback */
    if (bridge->sequence_callback) {
        bridge->sequence_callback(&seq_copy, PR_SEQ_COMPLETING,
                                   bridge->sequence_callback_data);
    }

    return PR_CEREB_SUCCESS;
}

NIMCP_EXPORT pr_cerebellum_error_t pr_cerebellum_bridge_abort_sequence(
    pr_cerebellum_bridge_t bridge,
    uint64_t sequence_id
) {
    if (!bridge) return PR_CEREB_ERROR_NULL_POINTER;

    PR_CEREB_MUTEX_LOCK(bridge->sequence_mutex);

    pr_sequence_t* seq = find_sequence_unlocked(bridge, sequence_id);
    if (!seq) {
        PR_CEREB_MUTEX_UNLOCK(bridge->sequence_mutex);
        return PR_CEREB_ERROR_SEQUENCE_NOT_FOUND;
    }

    seq->state = PR_SEQ_ABORTED;
    seq->total_executions++;  /* Count as execution attempt */

    pr_sequence_t seq_copy = *seq;

    PR_CEREB_MUTEX_UNLOCK(bridge->sequence_mutex);

    /* Update statistics */
    PR_CEREB_MUTEX_LOCK(bridge->stats_mutex);
    bridge->stats.sequences_executed++;
    bridge->stats.sequences_aborted++;
    PR_CEREB_MUTEX_UNLOCK(bridge->stats_mutex);

    /* Invoke callback */
    if (bridge->sequence_callback) {
        bridge->sequence_callback(&seq_copy, PR_SEQ_ABORTED,
                                   bridge->sequence_callback_data);
    }

    return PR_CEREB_SUCCESS;
}

NIMCP_EXPORT int pr_cerebellum_bridge_get_position(
    const pr_cerebellum_bridge_t bridge,
    uint64_t sequence_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_cerebellum_bridge_get_position: bridge is NULL");
        return -1;
    }

    PR_CEREB_MUTEX_LOCK(((pr_cerebellum_bridge_t)bridge)->sequence_mutex);

    pr_sequence_t* seq = find_sequence_unlocked((pr_cerebellum_bridge_t)bridge, sequence_id);
    if (!seq) {
        PR_CEREB_MUTEX_UNLOCK(((pr_cerebellum_bridge_t)bridge)->sequence_mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_cerebellum_bridge_get_position: seq is NULL");
        return -1;
    }

    int pos = (seq->state == PR_SEQ_EXECUTING || seq->state == PR_SEQ_INITIATING)
              ? (int)seq->current_position : -1;

    PR_CEREB_MUTEX_UNLOCK(((pr_cerebellum_bridge_t)bridge)->sequence_mutex);

    return pos;
}

//=============================================================================
// Callback Functions
//=============================================================================

NIMCP_EXPORT pr_cerebellum_error_t pr_cerebellum_bridge_set_sequence_callback(
    pr_cerebellum_bridge_t bridge,
    pr_sequence_callback_t callback,
    void* user_data
) {
    if (!bridge) return PR_CEREB_ERROR_NULL_POINTER;

    bridge->sequence_callback = callback;
    bridge->sequence_callback_data = user_data;

    return PR_CEREB_SUCCESS;
}

NIMCP_EXPORT pr_cerebellum_error_t pr_cerebellum_bridge_set_error_callback(
    pr_cerebellum_bridge_t bridge,
    pr_error_callback_t callback,
    void* user_data
) {
    if (!bridge) return PR_CEREB_ERROR_NULL_POINTER;

    bridge->error_callback = callback;
    bridge->error_callback_data = user_data;

    return PR_CEREB_SUCCESS;
}

//=============================================================================
// Statistics Functions
//=============================================================================

NIMCP_EXPORT pr_cerebellum_error_t pr_cerebellum_bridge_get_stats(
    const pr_cerebellum_bridge_t bridge,
    pr_cerebellum_stats_t* stats
) {
    if (!bridge || !stats) return PR_CEREB_ERROR_NULL_POINTER;

    PR_CEREB_MUTEX_LOCK(((pr_cerebellum_bridge_t)bridge)->stats_mutex);
    *stats = bridge->stats;
    PR_CEREB_MUTEX_UNLOCK(((pr_cerebellum_bridge_t)bridge)->stats_mutex);

    return PR_CEREB_SUCCESS;
}

NIMCP_EXPORT pr_cerebellum_error_t pr_cerebellum_bridge_reset_stats(
    pr_cerebellum_bridge_t bridge
) {
    if (!bridge) return PR_CEREB_ERROR_NULL_POINTER;

    PR_CEREB_MUTEX_LOCK(bridge->stats_mutex);
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->stats.last_update_ms = get_time_ms();
    PR_CEREB_MUTEX_UNLOCK(bridge->stats_mutex);

    return PR_CEREB_SUCCESS;
}

NIMCP_EXPORT pr_cerebellum_error_t pr_cerebellum_bridge_get_timing_history(
    const pr_cerebellum_bridge_t bridge,
    pr_timing_history_t* entries,
    size_t max_entries,
    size_t* count
) {
    if (!bridge || !entries || !count) return PR_CEREB_ERROR_NULL_POINTER;

    *count = 0;

    if (!bridge->config.track_timing_history || !bridge->timing_history) {
        return PR_CEREB_SUCCESS;
    }

    PR_CEREB_MUTEX_LOCK(((pr_cerebellum_bridge_t)bridge)->history_mutex);

    size_t available = bridge->timing_history_size;
    size_t to_copy = available < max_entries ? available : max_entries;

    if (to_copy > 0) {
        size_t start_idx = 0;
        if (bridge->timing_history_size < bridge->timing_history_capacity) {
            start_idx = 0;
        } else {
            start_idx = bridge->timing_history_write_idx;
        }

        for (size_t i = 0; i < to_copy; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && to_copy > 256) {
                pr_cerebellum_bridge_heartbeat("pr_cerebellu_loop",
                                 (float)(i + 1) / (float)to_copy);
            }

            size_t src_idx = (start_idx + (available - to_copy) + i)
                             % bridge->timing_history_capacity;
            entries[i] = bridge->timing_history[src_idx];
        }
        *count = to_copy;
    }

    PR_CEREB_MUTEX_UNLOCK(((pr_cerebellum_bridge_t)bridge)->history_mutex);

    return PR_CEREB_SUCCESS;
}

//=============================================================================
// Utility Functions
//=============================================================================

NIMCP_EXPORT const char* pr_cerebellum_error_string(pr_cerebellum_error_t error) {
    switch (error) {
        case PR_CEREB_SUCCESS: return "Success";
        case PR_CEREB_ERROR_NULL_POINTER: return "Null pointer";
        case PR_CEREB_ERROR_NO_MEMORY: return "Memory allocation failed";
        case PR_CEREB_ERROR_NOT_INITIALIZED: return "Bridge not initialized";
        case PR_CEREB_ERROR_INVALID_CONFIG: return "Invalid configuration";
        case PR_CEREB_ERROR_SEQUENCE_FULL: return "Sequence buffer full";
        case PR_CEREB_ERROR_SEQUENCE_NOT_FOUND: return "Sequence not found";
        case PR_CEREB_ERROR_INVALID_TIMING: return "Invalid timing value";
        case PR_CEREB_ERROR_NO_ENTANGLEMENT: return "Entanglement graph not set";
        default: return "Unknown error";
    }
}

NIMCP_EXPORT const char* pr_procedural_type_name(pr_procedural_type_t type) {
    switch (type) {
        case PR_PROC_MOTOR_SEQUENCE: return "Motor Sequence";
        case PR_PROC_TIMING_PATTERN: return "Timing Pattern";
        case PR_PROC_COGNITIVE_SKILL: return "Cognitive Skill";
        case PR_PROC_PERCEPTUAL_SKILL: return "Perceptual Skill";
        case PR_PROC_HABIT: return "Habit";
        default: return "Unknown";
    }
}

NIMCP_EXPORT const char* pr_sequence_state_name(pr_sequence_state_t state) {
    switch (state) {
        case PR_SEQ_IDLE: return "Idle";
        case PR_SEQ_INITIATING: return "Initiating";
        case PR_SEQ_EXECUTING: return "Executing";
        case PR_SEQ_COMPLETING: return "Completing";
        case PR_SEQ_ERROR: return "Error";
        case PR_SEQ_ABORTED: return "Aborted";
        default: return "Unknown";
    }
}

NIMCP_EXPORT const char* pr_error_type_name(pr_error_type_t type) {
    switch (type) {
        case PR_ERROR_TIMING: return "Timing";
        case PR_ERROR_SEQUENCE: return "Sequence";
        case PR_ERROR_AMPLITUDE: return "Amplitude";
        case PR_ERROR_COORDINATION: return "Coordination";
        case PR_ERROR_PREDICTION: return "Prediction";
        default: return "Unknown";
    }
}

NIMCP_EXPORT const char* pr_automatization_level_name(pr_automatization_level_t level) {
    switch (level) {
        case PR_AUTO_NOVICE: return "Novice";
        case PR_AUTO_ADVANCED: return "Advanced";
        case PR_AUTO_PROFICIENT: return "Proficient";
        case PR_AUTO_EXPERT: return "Expert";
        default: return "Unknown";
    }
}

NIMCP_EXPORT void pr_cerebellum_print_sequence(const pr_sequence_t* sequence) {
    if (!sequence) {
        printf("pr_sequence: NULL\n");
        return;
    }

    printf("Sequence '%s' (ID: %lu)\n", sequence->name,
           (unsigned long)sequence->sequence_id);
    printf("  Type: %s\n", pr_procedural_type_name(sequence->type));
    printf("  State: %s\n", pr_sequence_state_name(sequence->state));
    printf("  Length: %zu elements\n", sequence->length);
    printf("  Consolidation: %.3f\n", sequence->consolidation);
    printf("  Automatization: %.3f (%s)\n", sequence->automatization,
           pr_automatization_level_name(compute_automatization_level(
               sequence->consolidation, sequence->automatization,
               sequence->total_executions,
               PR_CEREB_AUTOMATIZATION_THRESHOLD, 50)));
    printf("  Executions: %u total, %u successful\n",
           sequence->total_executions, sequence->successful_executions);
    printf("  Avg Error: %.3f\n", sequence->avg_error);

    if (sequence->length > 0 && sequence->elements) {
        printf("  Elements:\n");
        for (size_t i = 0; i < sequence->length && i < 10; i++) {
            printf("    [%zu] Memory %lu: interval=%.1fms, accuracy=%.2f\n",
                   i, (unsigned long)sequence->elements[i].memory_id,
                   sequence->elements[i].timing.expected_interval_ms,
                   sequence->elements[i].timing.timing_accuracy);
        }
        if (sequence->length > 10) {
            printf("    ... and %zu more\n", sequence->length - 10);
        }
    }
}

NIMCP_EXPORT void pr_cerebellum_bridge_print_summary(const pr_cerebellum_bridge_t bridge) {
    if (!bridge) {
        printf("pr_cerebellum_bridge: NULL\n");
        return;
    }

    printf("=== Cerebellum Bridge Summary ===\n");

    PR_CEREB_MUTEX_LOCK(((pr_cerebellum_bridge_t)bridge)->sequence_mutex);
    printf("\nSequences: %zu\n", bridge->num_sequences);
    for (size_t i = 0; i < bridge->num_sequences && i < 5; i++) {
        printf("  '%s': %zu elements, consolidation=%.2f\n",
               bridge->sequences[i].name,
               bridge->sequences[i].length,
               bridge->sequences[i].consolidation);
    }
    if (bridge->num_sequences > 5) {
        printf("  ... and %zu more\n", bridge->num_sequences - 5);
    }
    PR_CEREB_MUTEX_UNLOCK(((pr_cerebellum_bridge_t)bridge)->sequence_mutex);

    pr_cerebellum_stats_t stats;
    pr_cerebellum_bridge_get_stats(bridge, &stats);

    printf("\nStatistics:\n");
    printf("  Sequences created: %lu\n", (unsigned long)stats.sequences_created);
    printf("  Sequences executed: %lu\n", (unsigned long)stats.sequences_executed);
    printf("  Sequences completed: %lu\n", (unsigned long)stats.sequences_completed);
    printf("  Sequences aborted: %lu\n", (unsigned long)stats.sequences_aborted);
    printf("  Timing events: %lu\n", (unsigned long)stats.timing_events);
    printf("  Avg timing error: %.2f ms\n", stats.avg_timing_error_ms);
    printf("  Timing accuracy: %.1f%%\n", stats.timing_accuracy * 100.0f);
    printf("  Error signals: %lu\n", (unsigned long)stats.error_signals);
    printf("  LTD events: %lu\n", (unsigned long)stats.ltd_events);
    printf("  LTP events: %lu\n", (unsigned long)stats.ltp_events);
    printf("  Automatizations: %lu\n", (unsigned long)stats.automatizations);

    printf("=================================\n");
}

NIMCP_EXPORT uint64_t pr_cerebellum_current_time_ms(void) {
    return get_time_ms();
}

NIMCP_EXPORT bool pr_cerebellum_bridge_validate(const pr_cerebellum_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_cerebellum_bridge_validate: bridge is NULL");
        return false;
    }
    if (!bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_cerebellum_bridge_validate: bridge->initialized is NULL");
        return false;
    }
    if (!bridge->sequences) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_cerebellum_bridge_validate: bridge->sequences is NULL");
        return false;
    }

    /* Verify sequence integrity */
    for (size_t i = 0; i < bridge->num_sequences; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_sequences > 256) {
            pr_cerebellum_bridge_heartbeat("pr_cerebellu_loop",
                             (float)(i + 1) / (float)bridge->num_sequences);
        }

        if (bridge->sequences[i].length > bridge->sequences[i].capacity) {
            return false;
        }
        if (bridge->sequences[i].consolidation < 0.0f ||
            bridge->sequences[i].consolidation > 1.0f) {
            return false;
        }
    }

    return true;
}

//=============================================================================
// Instance Health Agent Setter (B25 Upgrade)
//=============================================================================

void pr_cerebellum_bridge_set_instance_health_agent(
    pr_cerebellum_bridge_t bridge, nimcp_health_agent_t* agent)
{
    if (bridge) {
        bridge->health_agent = agent;
    }
}

//=============================================================================
// Training Hook Stubs (B25 Upgrade)
//=============================================================================

int pr_cerebellum_bridge_training_begin(pr_cerebellum_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_cerebellum_bridge_training_begin: NULL argument");
        return -1;
    }
    pr_cerebellum_bridge_heartbeat_instance(bridge->health_agent, "pr_cerebellum_bridge_training_begin", 0.0f);
    return 0;
}

int pr_cerebellum_bridge_training_end(pr_cerebellum_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_cerebellum_bridge_training_end: NULL argument");
        return -1;
    }
    pr_cerebellum_bridge_heartbeat_instance(bridge->health_agent, "pr_cerebellum_bridge_training_end", 1.0f);
    return 0;
}

int pr_cerebellum_bridge_training_step(pr_cerebellum_bridge_t bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_cerebellum_bridge_training_step: NULL argument");
        return -1;
    }
    pr_cerebellum_bridge_heartbeat_instance(bridge->health_agent, "pr_cerebellum_bridge_training_step", progress);
    return 0;
}
